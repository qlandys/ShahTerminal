#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#    include <winhttp.h>
#else
#    error "This backend is implemented for Windows (WinHTTP) only."
#endif

#include "OrderBook.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <json.hpp>

namespace
{
    using namespace std::chrono_literals;
    using json = nlohmann::json;

    struct Config
    {
        std::string symbol{"BIOUSDT"};
        std::string endpoint{"wss://wbs-api.mexc.com/ws"};
        std::size_t ladderLevelsPerSide{120};
        std::chrono::milliseconds throttle{50};
        std::size_t snapshotDepth{500};
    };

    Config parseArgs(int argc, char** argv)
    {
        Config cfg;

        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            auto value = [&](std::string_view name) {
                if (i + 1 >= argc)
                {
                    throw std::runtime_error("Missing value for " + std::string(name));
                }
                return std::string(argv[++i]);
            };

            if (arg == "--symbol")
            {
                cfg.symbol = value("--symbol");
            }
            else if (arg == "--endpoint")
            {
                cfg.endpoint = value("--endpoint");
            }
            else if (arg == "--ladder-levels")
            {
                cfg.ladderLevelsPerSide = std::stoul(value("--ladder-levels"));
            }
            else if (arg == "--throttle-ms")
            {
                cfg.throttle = std::chrono::milliseconds(std::stoul(value("--throttle-ms")));
            }
            else if (arg == "--snapshot-depth")
            {
                cfg.snapshotDepth = std::stoul(value("--snapshot-depth"));
            }
        }

        if (cfg.ladderLevelsPerSide == 0)
        {
            // Default depth: 500 levels per side around spread (1000 total)
            cfg.ladderLevelsPerSide = 500;
        }
        if (cfg.snapshotDepth == 0)
        {
            cfg.snapshotDepth = 50;
        }

        return cfg;
    }

    std::string winhttpError(const char* where)
    {
        DWORD error = GetLastError();
        LPWSTR buffer = nullptr;
        DWORD len =
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           nullptr,
                           error,
                           0,
                           reinterpret_cast<LPWSTR>(&buffer),
                           0,
                           nullptr);
        std::string msg = where;
        msg += ": ";
        if (len && buffer)
        {
            int outLen = WideCharToMultiByte(CP_UTF8, 0, buffer, len, nullptr, 0, nullptr, nullptr);
            std::string utf8(outLen, '\0');
            WideCharToMultiByte(CP_UTF8, 0, buffer, len, utf8.data(), outLen, nullptr, nullptr);
            msg += utf8;
            LocalFree(buffer);
        }
        else
        {
            msg += "unknown error";
        }
        return msg;
    }

    struct WinHttpHandle
    {
        WinHttpHandle() = default;
        explicit WinHttpHandle(HINTERNET h) : handle(h) {}
        ~WinHttpHandle() { reset(); }

        WinHttpHandle(const WinHttpHandle&) = delete;
        WinHttpHandle& operator=(const WinHttpHandle&) = delete;

        WinHttpHandle(WinHttpHandle&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
        WinHttpHandle& operator=(WinHttpHandle&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                handle = other.handle;
                other.handle = nullptr;
            }
            return *this;
        }

        void reset(HINTERNET h = nullptr)
        {
            if (handle)
            {
                WinHttpCloseHandle(handle);
            }
            handle = h;
        }

        [[nodiscard]] bool valid() const { return handle != nullptr; }
        [[nodiscard]] HINTERNET get() const { return handle; }

    private:
        HINTERNET handle{nullptr};
    };

    std::wstring toWide(const std::string& s)
    {
        if (s.empty())
        {
            return {};
        }
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        std::wstring out(len - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
        return out;
    }

    std::optional<std::string> httpGet(const std::string& host,
                                       const std::string& pathAndQuery,
                                       bool secure)
    {
        WinHttpHandle session(
            WinHttpOpen(L"ShahTerminal/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0));
        if (!session.valid())
        {
            std::cerr << "[backend] " << winhttpError("WinHttpOpen") << std::endl;
            return std::nullopt;
        }

        WinHttpHandle connection(
            WinHttpConnect(session.get(), toWide(host).c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
        if (!connection.valid())
        {
            std::cerr << "[backend] " << winhttpError("WinHttpConnect") << std::endl;
            return std::nullopt;
        }

        WinHttpHandle request(WinHttpOpenRequest(connection.get(),
                                                 L"GET",
                                                 toWide(pathAndQuery).c_str(),
                                                 nullptr,
                                                 WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                 secure ? WINHTTP_FLAG_SECURE : 0));
        if (!request.valid())
        {
            std::cerr << "[backend] " << winhttpError("WinHttpOpenRequest") << std::endl;
            return std::nullopt;
        }

        if (!WinHttpSendRequest(request.get(),
                                WINHTTP_NO_ADDITIONAL_HEADERS,
                                0,
                                WINHTTP_NO_REQUEST_DATA,
                                0,
                                0,
                                0))
        {
            std::cerr << "[backend] " << winhttpError("WinHttpSendRequest") << std::endl;
            return std::nullopt;
        }

        if (!WinHttpReceiveResponse(request.get(), nullptr))
        {
            std::cerr << "[backend] " << winhttpError("WinHttpReceiveResponse") << std::endl;
            return std::nullopt;
        }

        std::string buffer;
        for (;;)
        {
            DWORD bytesAvailable = 0;
            if (!WinHttpQueryDataAvailable(request.get(), &bytesAvailable))
            {
                std::cerr << "[backend] " << winhttpError("WinHttpQueryDataAvailable") << std::endl;
                return std::nullopt;
            }
            if (bytesAvailable == 0)
            {
                break;
            }

            std::string chunk(bytesAvailable, '\0');
            DWORD bytesRead = 0;
            if (!WinHttpReadData(request.get(), chunk.data(), bytesAvailable, &bytesRead))
            {
                std::cerr << "[backend] " << winhttpError("WinHttpReadData") << std::endl;
                return std::nullopt;
            }
            chunk.resize(bytesRead);
            buffer += chunk;
        }

        return buffer;
    }

    bool fetchExchangeInfo(const Config& cfg, double& tickSizeOut)
    {
        std::ostringstream path;
        path << "/api/v3/exchangeInfo?symbol=" << cfg.symbol;

        auto body = httpGet("api.mexc.com", path.str(), true);
        if (!body)
        {
            std::cerr << "[backend] failed to fetch exchangeInfo" << std::endl;
            return false;
        }

        json j;
        try
        {
            j = json::parse(*body);
        }
        catch (const std::exception& ex)
        {
            std::cerr << "[backend] exchangeInfo JSON parse error: " << ex.what() << std::endl;
            return false;
        }

        if (!j.contains("symbols") || !j["symbols"].is_array() || j["symbols"].empty())
        {
            std::cerr << "[backend] exchangeInfo: no symbols array" << std::endl;
            return false;
        }

        const auto& sym = j["symbols"].front();

        int quotePrecision = 0;
        if (sym.contains("quotePrecision") && sym["quotePrecision"].is_number_integer())
        {
            quotePrecision = sym["quotePrecision"].get<int>();
        }
        else if (sym.contains("quoteAssetPrecision") && sym["quoteAssetPrecision"].is_number_integer())
        {
            quotePrecision = sym["quoteAssetPrecision"].get<int>();
        }

        if (quotePrecision <= 0)
        {
            std::cerr << "[backend] exchangeInfo: missing quotePrecision" << std::endl;
            return false;
        }

        tickSizeOut = std::pow(10.0, -quotePrecision);
        std::cerr << "[backend] exchangeInfo: quotePrecision=" << quotePrecision
                  << " tickSize=" << tickSizeOut << std::endl;
        return tickSizeOut > 0.0;
    }

    bool fetchSnapshot(const Config& cfg, dom::OrderBook& book)
    {
        const double tickSize = book.tickSize();
        if (tickSize <= 0.0)
        {
            std::cerr << "[backend] fetchSnapshot: tickSize is not set" << std::endl;
            return false;
        }

        std::ostringstream path;
        path << "/api/v3/depth?symbol=" << cfg.symbol << "&limit=" << cfg.snapshotDepth;

        auto body = httpGet("api.mexc.com", path.str(), true);
        if (!body)
        {
            return false;
        }

        json j;
        try
        {
            j = json::parse(*body);
        }
        catch (const std::exception& ex)
        {
            std::cerr << "[backend] depth JSON parse error: " << ex.what() << std::endl;
            return false;
        }

        std::vector<std::pair<dom::OrderBook::Tick, double>> bids;
        std::vector<std::pair<dom::OrderBook::Tick, double>> asks;

        auto parseSide = [tickSize](const json& arr,
                                    std::vector<std::pair<dom::OrderBook::Tick, double>>& out) {
            out.clear();
            for (const auto& e : arr)
            {
                if (!e.is_array() || e.size() < 2) continue;
                double price = std::stod(e[0].get<std::string>());
                double qty = std::stod(e[1].get<std::string>());
                auto tick =
                    static_cast<dom::OrderBook::Tick>(std::llround(price / tickSize));
                out.emplace_back(tick, qty);
            }
        };

        parseSide(j["bids"], bids);
        parseSide(j["asks"], asks);
        book.loadSnapshot(bids, asks);

        std::cerr << "[backend] snapshot loaded: bids=" << bids.size() << " asks=" << asks.size() << std::endl;
        return true;
    }

    // --- минимальный парсер protobuf под нужные сообщения ---

    struct ProtoReader
    {
        const std::uint8_t* data{};
        std::size_t size{};
        std::size_t pos{};

        ProtoReader() = default;
        ProtoReader(const void* ptr, std::size_t len)
            : data(static_cast<const std::uint8_t*>(ptr))
            , size(len)
            , pos(0)
        {
        }

        bool eof() const { return pos >= size; }

        bool readVarint(std::uint64_t& out)
        {
            out = 0;
            int shift = 0;
            while (pos < size && shift < 64)
            {
                std::uint8_t b = data[pos++];
                out |= (std::uint64_t(b & 0x7F) << shift);
                if ((b & 0x80) == 0)
                {
                    return true;
                }
                shift += 7;
            }
            return false;
        }

        bool readBytes(std::size_t n, std::string& out)
        {
            if (pos + n > size)
            {
                return false;
            }
            out.assign(reinterpret_cast<const char*>(data + pos), n);
            pos += n;
            return true;
        }

        bool readLengthDelimited(std::string& out)
        {
            std::uint64_t len = 0;
            if (!readVarint(len))
            {
                return false;
            }
            return readBytes(static_cast<std::size_t>(len), out);
        }

        bool skipField(std::uint64_t key)
        {
            const auto wireType = key & 0x7;
            switch (wireType)
            {
            case 0: // varint
            {
                std::uint64_t dummy;
                return readVarint(dummy);
            }
            case 1: // 64-bit
                if (pos + 8 > size) return false;
                pos += 8;
                return true;
            case 2: // length-delimited
            {
                std::uint64_t len = 0;
                if (!readVarint(len) || pos + len > size)
                {
                    return false;
                }
                pos += static_cast<std::size_t>(len);
                return true;
            }
            case 5: // 32-bit
                if (pos + 4 > size) return false;
                pos += 4;
                return true;
            default:
                return false;
            }
        }
    };

    void parseDepthItem(const std::string& buf,
                        double tickSize,
                        std::vector<std::pair<dom::OrderBook::Tick, double>>& out)
    {
        ProtoReader r(buf.data(), buf.size());
        std::string priceStr;
        std::string qtyStr;
        while (!r.eof())
        {
            std::uint64_t key = 0;
            if (!r.readVarint(key)) break;
            const auto field = key >> 3;
            if ((key & 0x7) != 2)
            {
                if (!r.skipField(key)) break;
                continue;
            }

            std::string value;
            if (!r.readLengthDelimited(value)) break;

            if (field == 1)
            {
                priceStr = value;
            }
            else if (field == 2)
            {
                qtyStr = value;
            }
        }

        if (!priceStr.empty() && tickSize > 0.0)
        {
            double price = std::stod(priceStr);
            double qty = qtyStr.empty() ? 0.0 : std::stod(qtyStr);
            auto tick = static_cast<dom::OrderBook::Tick>(std::llround(price / tickSize));
            out.emplace_back(tick, qty);
        }
    }

    void parseAggreDepth(const std::string& buf,
                         double tickSize,
                         std::vector<std::pair<dom::OrderBook::Tick, double>>& asks,
                         std::vector<std::pair<dom::OrderBook::Tick, double>>& bids)
    {
        ProtoReader r(buf.data(), buf.size());
        while (!r.eof())
        {
            std::uint64_t key = 0;
            if (!r.readVarint(key)) break;
            const auto field = key >> 3;
            if ((key & 0x7) != 2)
            {
                if (!r.skipField(key)) break;
                continue;
            }

            std::string msg;
            if (!r.readLengthDelimited(msg)) break;

            if (field == 1) // asks
            {
                parseDepthItem(msg, tickSize, asks);
            }
            else if (field == 2) // bids
            {
                parseDepthItem(msg, tickSize, bids);
            }
            // fromVersion / toVersion мы игнорируем
        }
    }

    struct PublicAggreDeal
    {
        double price{};
        double quantity{};
        bool buy{};
        std::int64_t time{};
    };

    void parseAggreDealItem(const std::string& buf,
                            std::vector<PublicAggreDeal>& out)
    {
        ProtoReader r(buf.data(), buf.size());
        std::string priceStr;
        std::string qtyStr;
        int tradeType = 0;
        std::int64_t time = 0;

        while (!r.eof())
        {
            std::uint64_t key = 0;
            if (!r.readVarint(key)) break;
            const auto field = key >> 3;
            const auto wire = key & 0x7;

            if (wire == 2)
            {
                std::string value;
                if (!r.readLengthDelimited(value)) break;
                if (field == 1)
                {
                    priceStr = value;
                }
                else if (field == 2)
                {
                    qtyStr = value;
                }
            }
            else if (wire == 0)
            {
                std::uint64_t v = 0;
                if (!r.readVarint(v)) break;
                if (field == 3)
                {
                    tradeType = static_cast<int>(v);
                }
                else if (field == 4)
                {
                    time = static_cast<std::int64_t>(v);
                }
            }
            else
            {
                if (!r.skipField(key)) break;
            }
        }

        if (!priceStr.empty())
        {
            double price = std::stod(priceStr);
            double qty = qtyStr.empty() ? 0.0 : std::stod(qtyStr);
            if (qty <= 0.0) return;

            PublicAggreDeal d;
            d.price = price;
            d.quantity = qty;
            d.time = time;
            // tradeType: 1/2 — точное значение зависит от биржи; считаем 1=buy,2=sell
            d.buy = (tradeType != 2);
            out.push_back(d);
        }
    }

    void parseAggreDeals(const std::string& buf,
                         std::vector<PublicAggreDeal>& out)
    {
        ProtoReader r(buf.data(), buf.size());
        while (!r.eof())
        {
            std::uint64_t key = 0;
            if (!r.readVarint(key)) break;
            const auto field = key >> 3;
            if ((key & 0x7) != 2)
            {
                if (!r.skipField(key)) break;
                continue;
            }

            std::string msg;
            if (!r.readLengthDelimited(msg)) break;

            if (field == 1) // repeated deals
            {
                parseAggreDealItem(msg, out);
            }
            // field 2 = eventType (string) — игнорируем
        }
    }

    bool parsePushWrapper(const void* data,
                          std::size_t len,
                          std::string& channelOut,
                          double tickSize,
                          std::vector<std::pair<dom::OrderBook::Tick, double>>& asks,
                          std::vector<std::pair<dom::OrderBook::Tick, double>>& bids)
    {
        ProtoReader r(data, len);
        std::string depthBody;

        while (!r.eof())
        {
            std::uint64_t key = 0;
            if (!r.readVarint(key)) break;
            const auto field = key >> 3;

            if ((key & 0x7) != 2)
            {
                if (!r.skipField(key)) break;
                continue;
            }

            std::string value;
            if (!r.readLengthDelimited(value)) break;

            if (field == 1)
            {
                channelOut = value;
            }
            else if (field == 313)
            {
                depthBody = std::move(value);
            }
        }

        if (depthBody.empty())
        {
            return false;
        }

        asks.clear();
        bids.clear();
        parseAggreDepth(depthBody, tickSize, asks, bids);
        return true;
    }

    bool parseDealsFromWrapper(const void* data,
                               std::size_t len,
                               std::string& channelOut,
                               std::vector<PublicAggreDeal>& deals)
    {
        ProtoReader r(data, len);
        std::string dealsBody;

        while (!r.eof())
        {
            std::uint64_t key = 0;
            if (!r.readVarint(key)) break;
            const auto field = key >> 3;

            if ((key & 0x7) != 2)
            {
                if (!r.skipField(key)) break;
                continue;
            }

            std::string value;
            if (!r.readLengthDelimited(value)) break;

            if (field == 1)
            {
                channelOut = value;
            }
            else if (field == 314)
            {
                dealsBody = std::move(value);
            }
        }

        if (dealsBody.empty())
        {
            return false;
        }

        deals.clear();
        parseAggreDeals(dealsBody, deals);
        return !deals.empty();
    }

    void emitLadder(const Config& config,
                    const dom::OrderBook& book,
                    double bestBid,
                    double bestAsk,
                    std::int64_t ts)
    {
        auto levels = book.ladder(config.ladderLevelsPerSide);
        json out;
        out["type"] = "ladder";
        out["symbol"] = config.symbol;
        out["timestamp"] = ts;
        out["bestBid"] = bestBid;
        out["bestAsk"] = bestAsk;
        out["tickSize"] = book.tickSize();

        json rows = json::array();
        for (const auto& lvl : levels)
        {
            rows.push_back({{"price", lvl.price}, {"bid", lvl.bidQuantity}, {"ask", lvl.askQuantity}});
        }
        out["rows"] = std::move(rows);
        std::cout << out.dump() << std::endl;
    }

    bool runWebSocket(const Config& config, dom::OrderBook& book)
    {
        WinHttpHandle session(
            WinHttpOpen(L"ShahTerminal/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0));
        if (!session.valid())
        {
            std::cerr << "[backend] " << winhttpError("WinHttpOpen") << std::endl;
            return false;
        }

        const std::wstring host = L"wbs-api.mexc.com";
        const std::wstring path = L"/ws";

        WinHttpHandle connection(
            WinHttpConnect(session.get(), host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
        if (!connection.valid())
        {
            std::cerr << "[backend] " << winhttpError("WinHttpConnect") << std::endl;
            return false;
        }

        WinHttpHandle request(WinHttpOpenRequest(connection.get(),
                                                 L"GET",
                                                 path.c_str(),
                                                 nullptr,
                                                 WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                 WINHTTP_FLAG_SECURE));
        if (!request.valid())
        {
            std::cerr << "[backend] " << winhttpError("WinHttpOpenRequest") << std::endl;
            return false;
        }

        if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0))
        {
            std::cerr << "[backend] " << winhttpError("WinHttpSetOption") << std::endl;
            return false;
        }

        if (!WinHttpSendRequest(request.get(),
                                WINHTTP_NO_ADDITIONAL_HEADERS,
                                0,
                                WINHTTP_NO_REQUEST_DATA,
                                0,
                                0,
                                0))
        {
            std::cerr << "[backend] " << winhttpError("WinHttpSendRequest") << std::endl;
            return false;
        }

        if (!WinHttpReceiveResponse(request.get(), nullptr))
        {
            std::cerr << "[backend] " << winhttpError("WinHttpReceiveResponse") << std::endl;
            return false;
        }

        HINTERNET rawSocket = WinHttpWebSocketCompleteUpgrade(request.get(), 0);
        if (!rawSocket)
        {
            std::cerr << "[backend] " << winhttpError("WinHttpWebSocketCompleteUpgrade") << std::endl;
            return false;
        }
        WinHttpCloseHandle(request.get());

        std::cerr << "[backend] connected to Mexc ws" << std::endl;

        // Подписка на aggre.depth и aggre.deals
        std::ostringstream depthChannel;
        depthChannel << "spot@public.aggre.depth.v3.api.pb@100ms@" << config.symbol;
        // Aggre deals channel also requires an interval suffix (10ms/100ms); without it
        // the server replies with "Blocked" and sends no trades.
        std::ostringstream dealsChannel;
        dealsChannel << "spot@public.aggre.deals.v3.api.pb@100ms@" << config.symbol;
        json sub = {{"method", "SUBSCRIPTION"},
                    {"params", json::array({depthChannel.str(), dealsChannel.str()})}};
        const std::string subStr = sub.dump();

        if (WinHttpWebSocketSend(rawSocket,
                                 WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                                 (void*) subStr.data(),
                                 static_cast<DWORD>(subStr.size())) != S_OK)
        {
            std::cerr << "[backend] failed to send SUBSCRIPTION" << std::endl;
            WinHttpCloseHandle(rawSocket);
            return false;
        }

        std::cerr << "[backend] sent " << subStr << std::endl;

        std::vector<unsigned char> buffer(64 * 1024);
        auto lastEmit = std::chrono::steady_clock::now();

        for (;;)
        {
            DWORD received = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE type;
            HRESULT hr =
                WinHttpWebSocketReceive(rawSocket, buffer.data(), static_cast<DWORD>(buffer.size()), &received, &type);
            if (FAILED(hr))
            {
                std::cerr << "[backend] WebSocket receive failed: " << std::hex << hr << std::dec << std::endl;
                break;
            }

            if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)
            {
                std::cerr << "[backend] ws closed by server" << std::endl;
                break;
            }

            if (received == 0)
            {
                continue;
            }

            if (type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE ||
                type == WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE)
            {
                std::string text(reinterpret_cast<char*>(buffer.data()), received);
                // PING / служебные сообщения
                try
                {
                    auto j = json::parse(text);
                    const auto methodIt = j.find("method");
                    if (methodIt != j.end() && methodIt->is_string() && *methodIt == "PING")
                    {
                        const std::string pong = R"({"method":"PONG"})";
                        WinHttpWebSocketSend(rawSocket,
                                             WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                                             (void*) pong.data(),
                                             static_cast<DWORD>(pong.size()));
                    }
                    else
                    {
                        std::cerr << "[backend] control: " << text << std::endl;
                    }
                }
                catch (...)
                {
                    std::cerr << "[backend] text frame: " << text << std::endl;
                }
                continue;
            }

            if (type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE ||
                type == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE)
            {
                try
                {
                    const double tickSize = book.tickSize();
                    if (tickSize <= 0.0)
                    {
                        continue;
                    }

                    std::string channelName;
                    std::vector<std::pair<dom::OrderBook::Tick, double>> asks;
                    std::vector<std::pair<dom::OrderBook::Tick, double>> bids;
                    std::vector<PublicAggreDeal> deals;

                    // Try trades first
                    if (parseDealsFromWrapper(buffer.data(), received, channelName, deals))
                    {
                        for (const auto& d : deals)
                        {
                            json t;
                            t["type"] = "trade";
                            t["symbol"] = config.symbol;
                            t["price"] = d.price;
                            t["qty"] = d.quantity;
                            t["side"] = d.buy ? "buy" : "sell";
                            t["timestamp"] = d.time;
                            std::cout << t.dump() << std::endl;
                        }
                        continue;
                    }

                    // Depth updates
                    if (parsePushWrapper(buffer.data(), received, channelName, tickSize, asks, bids))
                    {
                        book.applyDelta(bids, asks, config.ladderLevelsPerSide);

                        const auto now = std::chrono::steady_clock::now();
                        if (now - lastEmit >= config.throttle)
                        {
                            lastEmit = now;
                            const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                   std::chrono::system_clock::now().time_since_epoch())
                                                   .count();
                            emitLadder(config, book, book.bestBid(), book.bestAsk(), nowMs);
                        }
                    }
                }
                catch (const std::exception& ex)
                {
                    std::cerr << "[backend] decode/apply error: " << ex.what() << std::endl;
                }
            }
        }

        WinHttpCloseHandle(rawSocket);
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const auto cfg = parseArgs(argc, argv);
        std::cerr << "[backend] starting WS depth for " << cfg.symbol << std::endl;
        dom::OrderBook book;

        double tickSize = 0.0;
        if (!fetchExchangeInfo(cfg, tickSize))
        {
            std::cerr << "[backend] failed to determine tick size, exiting" << std::endl;
            return 1;
        }
        book.setTickSize(tickSize);

        if (!fetchSnapshot(cfg, book))
        {
            std::cerr << "[backend] snapshot failed, continuing with empty book" << std::endl;
        }
        runWebSocket(cfg, book);
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "fatal: " << ex.what() << std::endl;
        return 1;
    }
}
