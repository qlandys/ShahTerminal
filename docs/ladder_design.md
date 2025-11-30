# ShahTerminal ladder behavior (order book engine)

This document captures the current behavior of the ladder that visually matches
Meta-style DOM. If you change the backend or GUI, keep these invariants unless
you intentionally want different behavior.

## Price / tick model

- Exchange tick size is taken from REST `exchangeInfo`:
  - Use `quotePrecision` (or `quoteAssetPrecision` as a fallback).
  - `tickSize = 10 ^ (-quotePrecision)`.
- All internal prices in `OrderBook` are stored as integer ticks:
  - `Tick = int64_t`.
  - `tick = llround(price / tickSize)`.
- Order book maps:
  - `std::map<Tick, double> bids_`, `asks_`.
  - Key = tick index, value = quantity in base asset.
- Best bid / ask:
  - `bestBid = max(bids_.keys) * tickSize`.
  - `bestAsk = min(asks_.keys) * tickSize`.

## Ladder window (no jumping)

- `OrderBook` maintains a persistent ladder center in ticks:
  - Members: `centerTick_` and `hasCenter_`.
  - The *mid-tick* is computed on every update:
    - If both sides exist: `(bestBidTick + bestAskTick) / 2`.
    - Else: the best tick of the existing side.
- Ladder window in `ladder(levelsPerSide)`:
  - `padding = levelsPerSide`.
  - Current window: `[centerTick_ - padding, centerTick_ + padding]`.
  - *Inner band* to avoid jumping:
    - `inner = [windowMin + padding/4, windowMax - padding/4]`.
  - As long as mid-tick stays inside the inner band, `centerTick_` is unchanged.
  - If mid-tick leaves the band, `centerTick_` is shifted so that mid-tick
    comes back toward the middle of the window.
- The ladder always iterates from `maxTick` down to `minTick` with a fixed
  number of levels (capped at ~4000), so the price column on the screen is
  visually stable and does not jump every tick.

## JSON format to GUI

- Backend emits one JSON object per line:
  - `type: "ladder"`.
  - `symbol`: current symbol.
  - `timestamp`: ms since epoch.
  - `bestBid`, `bestAsk`: prices in quote asset.
  - `tickSize`: same value used internally in `OrderBook`.
  - `rows`: array of levels:
    - `{"price": <price>, "bid": <qty>, "ask": <qty>}`.
- `price` is always `tick * tickSize`.
- `bid` / `ask` quantities are sums in base asset for that tick.

## GUI rendering (PySide ladder)

- File: `gui/main.py`, class `LadderModel`.
- The GUI interprets the JSON as:
  - `price` column: formatted with 6 decimals (`f"{price:.6f}"`).
  - Left/right columns are **USDT notional**, not raw size:
    - Notional bid = `bid_qty * price`.
    - Notional ask = `ask_qty * price`.
  - Formatting for notional (`_format_notional`):
    - `>= 1000` — integer (no separators): `145716`.
    - `1 .. < 1000` — two decimals, trimmed: `91.8`, `1663.09`.
    - `< 1` — up to four decimals, trimmed.
- Heat map uses notional, not raw quantity:
  - Intensity is `log10(1 + notional)` clamped to `[0,1]`.
  - Asks — reddish, bids — greenish.
  - Best bid / ask levels are drawn with full intensity.

## Scroll stability

- The GUI does not re-center scroll on every ladder update:
  - Before updating the model, it saves `verticalScrollBar().value()`.
  - After `update_from_payload`, it restores the same value.
- This means:
  - If you scroll away from the inside market, your view stays where you left it.
  - The price column updates in-place without visible jumps.

## Backend depth pipeline (REST + WS)

All of this lives in `backend/src/main.cpp`.

- On startup:
  - `parseArgs` reads `--symbol`, `--ladder-levels`, etc.
  - `fetchExchangeInfo` calls `GET /api/v3/exchangeInfo?symbol=...` on Mexc and
    derives `tickSize` from `quotePrecision` (see “Price / tick model”).
  - `OrderBook::setTickSize(tickSize)` is called once.
- Snapshot (REST):
  - `fetchSnapshot` calls `GET /api/v3/depth?symbol=...&limit=N`.
  - For every `[priceStr, qtyStr]` in `bids` / `asks` arrays:
    - `price = stod(priceStr)`.
    - `qty = stod(qtyStr)`.
    - `tick = llround(price / tickSize)`.
  - These `(tick, qty)` pairs go to `OrderBook::loadSnapshot`.
- WebSocket stream:
  - `runWebSocket` connects to `wss://wbs-api.mexc.com/ws` via WinHTTP.
  - Sends subscription:
    - channel: `spot@public.aggre.depth.v3.api.pb@100ms@<symbol>`.
  - Handles text frames:
    - Replies with `{"method":"PONG"}` if `method == "PING"`.
  - Handles binary frames:
    - `parsePushWrapper(buffer, len, channelName, tickSize, asks, bids)` parses
      the protobuf wrapper and fills `asks` / `bids` as `(Tick, qty)`.
    - `OrderBook::applyDelta(bids, asks)` applies the diff.
    - With throttle `Config::throttle` it periodically calls `emitLadder`,
      which serializes current book to JSON (see “JSON format to GUI”).

## Protobuf decoding (Mexc aggre.depth)

- Protobuf schema is in `wsproto/websocket-proto-main`:
  - `PublicAggreDepthsV3Api.proto` and `PushDataV3ApiWrapper.proto`.
- C++ side uses a very small manual decoder:
  - `struct ProtoReader` wraps a `const uint8_t*` buffer and supports:
    - `readVarint`, `readLengthDelimited`, `skipField`.
- Functions in `main.cpp`:
  - `parseDepthItem(buf, tickSize, out)`:
    - Parses `PublicAggreDepthV3ApiItem`:
      - field `1`: `price` string.
      - field `2`: `quantity` string.
    - Converts to:
      - `price = stod(priceStr)`.
      - `qty = stod(qtyStr)` (or `0.0` if empty).
      - `tick = llround(price / tickSize)`.
    - Appends `(tick, qty)` into `out`.
  - `parseAggreDepth(buf, tickSize, asks, bids)`:
    - Parses `PublicAggreDepthsV3Api` message:
      - field `1`: repeated `asks` (depth items).
      - field `2`: repeated `bids`.
    - Calls `parseDepthItem` for every element.
  - `parsePushWrapper(data, len, channelOut, tickSize, asks, bids)`:
    - Parses `PushDataV3ApiWrapper`:
      - field `1`: `channel` string (stored into `channelOut`).
      - field `313`: `publicAggreDepths` body, length-delimited.
    - Calls `parseAggreDepth` for that body.

If you ever regenerate protobufs or switch to a full protobuf library,
make sure that:

- You still convert prices to the same `Tick` as above.
- Snapshot (`/depth`) and WS (`aggre.depth`) both write into the same
  `OrderBook` in ticks.
- JSON output keeps the current schema so the PySide GUI does not break.

## What to watch for when editing

- Do **not** switch back to `double` keys for prices in the backend maps.
- If you change the tick logic, make sure:
  - `tickSize` is still taken from exchange filters / precision.
  - Snapshot and WS deltas both use the *same* `Tick` conversion.
- If you change ladder windowing:
  - Preserve the idea of a persistent `centerTick_` with inertia.
  - Avoid recomputing the window purely from min/max book ticks on each call.
- If you change the GUI:
  - Keep the JSON schema compatible (`price`, `bid`, `ask`, `tickSize`).
  - Decide explicitly whether the heat map is on notional or raw size and
    update this doc accordingly.
