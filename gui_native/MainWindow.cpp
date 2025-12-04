#include "MainWindow.h"
#include "ConnectionStore.h"
#include "ConnectionsWindow.h"
#include "DomWidget.h"
#include "LadderClient.h"
#include "PluginsWindow.h"
#include "PrintsWidget.h"
#include "SettingsWindow.h"
#include "TradeManager.h"
#include "SymbolPickerDialog.h"
#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QScreen>
#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QButtonGroup>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPixmap>
#include <QPainter>
#include <QPaintEvent>
#include <QCursor>
#include <QPushButton>
#include <QSplitter>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVariant>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QStyleOptionSlider>
#include <QEnterEvent>
#include <QWidget>
#include <QWindow>
#include <QDebug>
#include <QFile>
#include <QFile>
#include <QTextStream>
#include <QProcessEnvironment>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QTransform>
#include <QUrl>
#include <QCloseEvent>
#include <QCompleter>
#include <QKeyEvent>
#include <QStandardPaths>
#include <QSettings>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardItemModel>
#include <QComboBox>
#include <QPointer>
#include <cmath>
#include <algorithm>
#include <QListWidget>
#include <QAbstractItemView>

#if __has_include(<QMediaPlayer>)
#include <QMediaPlayer>
#define HAS_QMEDIAPLAYER 1
#else
#define HAS_QMEDIAPLAYER 0
#endif

#if __has_include(<QAudioOutput>)
#include <QAudioOutput>
#define HAS_QAUDIOOUTPUT 1
#else
#define HAS_QAUDIOOUTPUT 0
#endif

namespace {
constexpr int kDomColumnMinWidth = 140;
}

class DomScrollBar : public QScrollBar {
public:
    explicit DomScrollBar(Qt::Orientation orientation, QWidget *parent = nullptr)
        : QScrollBar(orientation, parent)
        , m_hoverAnim(new QVariantAnimation(this))
    {
        setMouseTracking(true);
        setAttribute(Qt::WA_Hover, true);
        setFixedWidth(10);
        m_hoverAnim->setDuration(140);
        m_hoverAnim->setEasingCurve(QEasingCurve::InOutCubic);
        connect(m_hoverAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_hoverProgress = value.toReal();
            update();
        });
    }

protected:
    void enterEvent(QEnterEvent *event) override
    {
        startHoverAnimation(1.0);
        QScrollBar::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        startHoverAnimation(0.0);
        QScrollBar::leaveEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        startHoverAnimation(1.0);
        QScrollBar::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        startHoverAnimation(underMouse() ? 1.0 : 0.0);
        QScrollBar::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);

        QStyleOptionSlider opt;
        initStyleOption(&opt);

        const QRect groove =
            style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarGroove, this);
        painter.fillRect(groove, QColor(0x1b, 0x1b, 0x1b));

        const QRect slider =
            style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, this);
        if (!slider.isEmpty()) {
            const QColor baseColor(0x55, 0x55, 0x55);
            const QColor hoverColor(0xa0, 0xa0, 0xa0);
            auto mix = [](const QColor &from, const QColor &to, qreal t) {
                return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * t,
                                        from.greenF() + (to.greenF() - from.greenF()) * t,
                                        from.blueF() + (to.blueF() - from.blueF()) * t,
                                        from.alphaF() + (to.alphaF() - from.alphaF()) * t);
            };
            QColor handleColor = mix(baseColor, hoverColor, m_hoverProgress);
            painter.fillRect(slider.adjusted(0, 1, 0, -1), handleColor);
        }
    }

private:
    void startHoverAnimation(qreal target)
    {
        if (qFuzzyCompare(m_hoverProgress, target)) {
            return;
        }
        m_hoverAnim->stop();
        m_hoverAnim->setStartValue(m_hoverProgress);
        m_hoverAnim->setEndValue(target);
        m_hoverAnim->start();
    }

    qreal m_hoverProgress = 0.0;
    QVariantAnimation *m_hoverAnim;
};

static QIcon mirrorIconHorizontally(const QIcon &icon, const QSize &size);
#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <windowsx.h>
#endif

// Simple append-only file logger to capture geometry/state traces when
// DebugView is not available. Helpful for reproducing issues on user's
// machine where reading OutputDebugString may be inconvenient.
static void logToFile(const QString &msg)
{
    const QString tmp = QProcessEnvironment::systemEnvironment().value("TEMP", QStringLiteral("."));
    const QString path = tmp + QDir::separator() + QStringLiteral("shah_gui_debug.log");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
    QTextStream ts(&f);
    ts << QDateTime::currentDateTime().toString(Qt::ISODate) << " " << msg << "\n";
}

static QString rectToString(const QRect &r)
{
    return QString("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
}

#ifdef Q_OS_WIN
static void applyNativeSnapStyleForHwnd(HWND hwnd)
{
    if (!hwnd) return;
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style &= ~WS_CAPTION;
    style |= WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}
#endif

// Smoothly animate window opacity from slightly transparent to fully opaque.
// Used to mask small visual jumps when we programmatically change geometry
// / window flags during restore.
static void animateWindowFadeIn(QWidget *w)
{
    if (!w) return;
    // Ensure starting opacity is slightly less than 1 so animation is visible
    w->setWindowOpacity(0.96);
    auto *anim = new QPropertyAnimation(w, "windowOpacity");
    anim->setDuration(180);
    anim->setStartValue(0.96);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::InOutCubic);
    QObject::connect(anim, &QPropertyAnimation::finished, anim, &QObject::deleteLater);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

// Ensure native window bounds match widget geometry; on some broken restore
// sequences the native HWND rect can diverge from QWidget geometry causing
// the visible area to be clipped. This helper forces the native bounds to
// the provided geometry when on Windows.
static void ensureNativeBounds(QWidget *w, const QRect &desired)
{
#ifdef Q_OS_WIN
    if (!w) return;
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    if (!hwnd) return;
    RECT r;
    if (GetWindowRect(hwnd, &r)) {
        QRect nativeRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
        // If native rect and desired geometry differ significantly, force it.
        const int dx = std::abs(nativeRect.x() - desired.x());
        const int dy = std::abs(nativeRect.y() - desired.y());
        const int dw = std::abs(nativeRect.width() - desired.width());
        const int dh = std::abs(nativeRect.height() - desired.height());
        if (dx > 4 || dy > 4 || dw > 8 || dh > 8) {
            // Log and set native bounds using SetWindowPos to avoid weird clipping.
            QString s = QStringLiteral("[MainWindow] Forcing native bounds to %1 (native was %2)").arg(rectToString(desired)).arg(rectToString(nativeRect));
            qDebug() << s;
            logToFile(s);
            SetWindowPos(hwnd, NULL, desired.x(), desired.y(), desired.width(), desired.height(), SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
#endif
}

MainWindow::MainWindow(const QString &backendPath,
                       const QString &symbol,
                       int levels,
                       QWidget *parent)
    : QMainWindow(parent)
    , m_backendPath(backendPath)
    , m_symbols(QStringList{symbol})
    , m_levels(levels)
    , m_workspaceTabs(nullptr)
    , m_tabUnderline(nullptr)
    , m_tabUnderlineAnim(nullptr)
    , m_tabUnderlineHiddenForDrag(false)
    , m_workspaceStack(nullptr)
    , m_addTabButton(nullptr)
    , m_addMenuButton(nullptr)
    , m_connectionIndicator(nullptr)
    , m_connectionButton(nullptr)
    , m_timeLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_pingLabel(nullptr)
    , m_orderPanel(nullptr)
    , m_orderSymbolLabel(nullptr)
    , m_orderPriceEdit(nullptr)
    , m_orderQuantityEdit(nullptr)
    , m_buyButton(nullptr)
    , m_sellButton(nullptr)
    , m_pluginsWindow(nullptr)
    , m_settingsWindow(nullptr)
    , m_connectionStore(new ConnectionStore(this))
    , m_tradeManager(new TradeManager(this))
    , m_connectionsWindow(nullptr)
    , m_tabs()
    , m_nextTabId(1)
    , m_recycledTabIds()
    , m_tabCloseIconNormal()
    , m_tabCloseIconHover()
    , m_lastAddAction(AddAction::WorkspaceTab)
    , m_draggingDomContainer(nullptr)
    , m_domDragStartGlobal()
    , m_domDragStartWindowOffset()
    , m_domDragActive(false)
    , m_timeTimer(nullptr)
        , m_topBar(nullptr)
        , m_minButton(nullptr)
    , m_maxButton(nullptr)
    , m_closeButton(nullptr)
{
    setWindowTitle(QStringLiteral("Shah Terminal"));
    resize(1920, 1080);
    setMinimumSize(800, 400);

    // Use frameless window but keep system menu and min/max buttons so
    // native snap & window controls behave as expected.
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint
                   | Qt::WindowMinMaxButtonsHint);
    // Use custom TitleBar instead of native OS strip.
    setWindowFlag(Qt::FramelessWindowHint);

    loadUserSettings();
    if (m_volumeRules.isEmpty()) {
        m_volumeRules = defaultVolumeHighlightRules();
    }

    if (m_connectionStore) {
        connect(m_connectionStore,
                &ConnectionStore::credentialsChanged,
                this,
                [this](const QString &, const MexcCredentials &) { refreshAccountColors(); });
    }
    refreshAccountColors();

    m_symbolLibrary = m_symbols;
    const QStringList defaults = {
        QStringLiteral("BTCUSDT"), QStringLiteral("ETHUSDT"), QStringLiteral("SOLUSDT"),
        QStringLiteral("BNBUSDT"), QStringLiteral("XRPUSDT"), QStringLiteral("LTCUSDT"),
        QStringLiteral("BIOUSDT")
    };
    for (const QString &sym : defaults) {
        if (!m_symbolLibrary.contains(sym, Qt::CaseInsensitive)) {
            m_symbolLibrary.push_back(sym);
        }
    }
    fetchSymbolLibrary();

    // ?????????? ???????? ?????? (Shift ? ?.?.).
    qApp->installEventFilter(this);

    buildUi();
    const QString appLogo = resolveAssetPath(QStringLiteral("logo.png"));
    if (!appLogo.isEmpty()) {
        setWindowIcon(QIcon(appLogo));
    }

#if HAS_QMEDIAPLAYER && HAS_QAUDIOOUTPUT
    // Prefer ffmpeg backend only when its runtime bits are actually deployed.
#ifdef Q_OS_WIN
    const QDir appDir(QCoreApplication::applicationDirPath());
    const bool hasFfmpegPlugin =
        QFile::exists(appDir.filePath(QStringLiteral("multimedia/ffmpegmediaplugin.dll")));
    auto hasDll = [&](const QString &pattern) {
        return !appDir.entryList(QStringList{pattern}, QDir::Files).isEmpty();
    };
    const bool hasFfmpegRuntime = hasDll(QStringLiteral("avcodec-*.dll"))
                                  && hasDll(QStringLiteral("avformat-*.dll"))
                                  && hasDll(QStringLiteral("avutil-*.dll"))
                                  && hasDll(QStringLiteral("swresample-*.dll"));
    if (hasFfmpegPlugin && hasFfmpegRuntime) {
        qputenv("QT_MEDIA_BACKEND", "ffmpeg");
    } else {
        qWarning() << "[audio] ffmpeg backend not deployed, falling back to default media backend";
    }
#else
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");
#endif

    // Notification sound (mp3), relies on Qt Multimedia backend (ffmpeg/wmf)
    const QString notifSoundPath = resolveAssetPath(QStringLiteral("sounds/notification.mp3"));
    if (!notifSoundPath.isEmpty()) {
        m_notificationOutput = new QAudioOutput(this);
        m_notificationOutput->setVolume(0.85);
        m_notificationPlayer = new QMediaPlayer(this);
        m_notificationPlayer->setAudioOutput(m_notificationOutput);
        if (QFile::exists(notifSoundPath)) {
            m_notificationPlayer->setSource(QUrl::fromLocalFile(notifSoundPath));
        } else {
            qWarning() << "[audio] notification.mp3 not found at" << notifSoundPath;
        }
        connect(m_notificationPlayer,
                &QMediaPlayer::errorOccurred,
                this,
                [this](QMediaPlayer::Error err, const QString &errStr) {
                    qWarning() << "[audio] player error" << err << errStr;
                });
    }

    const QString successSoundPath = resolveAssetPath(QStringLiteral("sounds/success.mp3"));
    if (!successSoundPath.isEmpty()) {
        m_successOutput = new QAudioOutput(this);
        m_successOutput->setVolume(0.9);
        m_successPlayer = new QMediaPlayer(this);
        m_successPlayer->setAudioOutput(m_successOutput);
        if (QFile::exists(successSoundPath)) {
            m_successPlayer->setSource(QUrl::fromLocalFile(successSoundPath));
        } else {
            qWarning() << "[audio] success.mp3 not found at" << successSoundPath;
        }
        connect(m_successPlayer,
                &QMediaPlayer::errorOccurred,
                this,
                [this](QMediaPlayer::Error err, const QString &errStr) {
                    qWarning() << "[audio] success player error" << err << errStr;
                });
    }
#endif

    if (m_tradeManager) {
        connect(m_tradeManager,
                &TradeManager::connectionStateChanged,
                this,
                &MainWindow::handleConnectionStateChanged);
        connect(m_tradeManager,
                &TradeManager::positionChanged,
                this,
                &MainWindow::handlePositionChanged);
        connect(m_tradeManager,
                &TradeManager::orderPlaced,
                this,
                [this](const QString &account, const QString &sym, OrderSide side, double price, double qty) {
                    addLocalOrderMarker(account,
                                        sym,
                                        side,
                                        price,
                                        qty,
                                        QDateTime::currentMSecsSinceEpoch());
                    const QString msg = tr("Order placed: %1 %2 @ %3")
                                            .arg(side == OrderSide::Buy ? QStringLiteral("BUY")
                                                                        : QStringLiteral("SELL"))
                                            .arg(qty, 0, 'g', 6)
                                            .arg(price, 0, 'f', 5);
                    statusBar()->showMessage(msg, 3000);
                });
        connect(m_tradeManager,
                &TradeManager::orderFailed,
                this,
                [this](const QString &account, const QString &sym, const QString &message) {
                    Q_UNUSED(account);
                    Q_UNUSED(sym);
                    const QString msg = tr("Order failed: %1").arg(message);
                    statusBar()->showMessage(msg, 4000);
                    addNotification(msg, true);
                });
        connect(m_tradeManager,
                &TradeManager::orderCanceled,
                this,
                [this](const QString &account, const QString &sym, OrderSide side, double price) {
                    removeLocalOrderMarker(account, sym, side, price);
                });
        connect(m_tradeManager,
                &TradeManager::localOrdersUpdated,
                this,
                &MainWindow::handleLocalOrdersUpdated);
    }

    createInitialWorkspace();

    if (m_connectionStore && m_tradeManager) {
        auto initProfile = [&](ConnectionStore::Profile profile) {
            MexcCredentials creds = m_connectionStore->loadMexcCredentials(profile);
            m_tradeManager->setCredentials(profile, creds);
            const bool hasKey = !creds.apiKey.isEmpty();
            const bool hasSecret = !creds.secretKey.isEmpty();
            const bool uzxProfile = (profile == ConnectionStore::Profile::UzxSpot
                                     || profile == ConnectionStore::Profile::UzxSwap);
            bool canAuto = creds.autoConnect && hasKey && hasSecret;
            if (uzxProfile) {
                canAuto = canAuto && !creds.passphrase.isEmpty();
            }
            if (canAuto) {
                QTimer::singleShot(0, this, [this, profile]() {
                    if (m_tradeManager) {
                        m_tradeManager->connectToExchange(profile);
                    }
                });
            }
        };
        initProfile(ConnectionStore::Profile::MexcSpot);
        initProfile(ConnectionStore::Profile::MexcFutures);
        initProfile(ConnectionStore::Profile::UzxSwap);
        initProfile(ConnectionStore::Profile::UzxSpot);
        handleConnectionStateChanged(ConnectionStore::Profile::MexcSpot,
                                     m_tradeManager->overallState(),
                                     QString());
    } else {
        handleConnectionStateChanged(ConnectionStore::Profile::MexcSpot,
                                     TradeManager::ConnectionState::Disconnected,
                                     QString());
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QWidget *topBar = buildTopBar(central);
    QWidget *mainArea = buildMainArea(central);

    rootLayout->addWidget(topBar);
    rootLayout->addWidget(mainArea, 1);

    // Status bar removed (ping not needed)
    statusBar()->hide();

    // Time label update timer (UTC clock + offset).
    m_timeTimer = new QTimer(this);
    m_timeTimer->setInterval(1000);
    connect(m_timeTimer, &QTimer::timeout, this, &MainWindow::updateTimeLabel);
    m_timeTimer->start();
    updateTimeLabel();

    // Window buttons style + TitleBar bottom border, ?????? ? VSCode.
    // ????? ?????? ???? ??????? ? ????????? ????? ?? ?????? ? ??????? ??????.
    setStyleSheet(
        "QFrame#TitleBar {"
        "  background-color: #252526;"
        "  border-bottom: none;" /* ??????? ?????? ??????? ? TitleBar */
        "}"
        "QFrame#SideToolbar {"
        "  background-color: transparent;"
        "  border-right: 1px solid #444444;"
        "}"
        "QTabBar::tab:first:selected {"
        "  border-left: none;"
        "}"
        "QFrame#MainAreaFrame {"
        "  border-top: 1px solid #444444;"
        "}"
        "QToolButton#WindowButtonMin, QToolButton#WindowButtonMax, QToolButton#WindowButtonClose {"
        "  background: transparent;"
        "  border: none;"
        "  color: #ffffff;"
        "  padding: 0px;"
        "}"
        "QToolButton#WindowButtonMin:hover, QToolButton#WindowButtonMax:hover {"
        "  background-color: #2a2a2a;"
        "}"
        "QToolButton#WindowButtonMin:pressed, QToolButton#WindowButtonMax:pressed {"
        "  background-color: #1f1f1f;"
        "}"
        "QToolButton#WindowButtonClose:hover {"
        "  background-color: #e81123;"
        "  color: #ffffff;"
        "}"
        "QToolButton#WindowButtonClose:pressed {"
        "  background-color: #c50f1f;"
        "  color: #ffffff;"
        "}"
        // ??????? hover-????????? ? ??????? ?????? ? ?????? ???? ?????? ????? ??? ?????????.
                "QToolButton#SideNavButton {"
        "  background: transparent;"
        "  border: none;"
        "  padding: 0px;"
        "  margin: 0px;"
        "}"
        // ????? ??? ??????? ? ????? VSCode
        "QFrame#TabsContainer {"
        "  background-color: #252526;"          /* ??? ?? ???, ??? ? ? TitleBar */
        "  border-bottom: none;"   /* ??????? ?????? ??????? ? TabsContainer */
        "}"
        "QTabBar {"
        "  border: none;" /* ??????? ??? ??????? ? ?????? QTabBar */
        "  background-color: #252526;" /* ????????????? ??? QTabBar ????? ??, ??? ? TitleBar */
        "}"
        "QTabBar::tab {"
        "  background-color: #252526;" /* ??? ?????????? ???????, ??? ? ?????-???? */
        "  color: #cccccc;" /* ???? ?????? ?????????? ??????? */
        "  padding: 0px 12px;" /* ???????, ????? ??????? ???????? ??? ?????? */
        "  border: none;" /* ??????? ??? ??????? ? ??????? */
        "  margin-left: 0px;" /* ??????? ?????? ????? ????????? */
        "  height: 100%;" /* ??????? ?? ??? ?????? ?????-???? */
        "}"
                "QTabBar::tab:selected {"
        "  background-color: #1e1e1e;"
        "  color: #ffffff;"
        "  border-top: none;"
        "  border-bottom: none;"
        "  border-left: 1px solid #444444;"
        "  border-right: 1px solid #444444;"
        "}"
        "QTabBar::tab:first:selected {"
        "  border-left: none;"
        "}"
        "QTabBar::tab:!selected:hover {"
        "  background-color: #2d2d2d;" /* ??? ??? ????????? ?? ?????????? ??????? */
        "}"
        "QTabBar::close-button {"
        "  border: none;"
        "  background: transparent;"
        "  margin: 2px;"
        "  padding: 0;"
        "}"
        "QTabBar::close-button:hover {"
        "  background: #555555;"
        "}"
        ""
        "QFrame#DomColumnFrame {"
        "  background-color: #1e1e1e;"
        "  border: 1px solid #444444;"
        "  border-radius: 0px;"
        "}"
        "QWidget#DomResizeHandle {"
        "  background-color: #2b2b2b;"
        "}"
        "QWidget#DomResizeHandle:hover {"
        "  background-color: #3a3a3a;"
        "}"
        "QWidget#DomColumnResizeHandle {"
        "  background-color: #2b2b2b;"
        "}"
        "QWidget#DomColumnResizeHandle:hover {"
        "  background-color: #3a3a3a;"
        "}"
        "QSplitter#DomPrintsSplitter::handle:horizontal {"
        "  background: #323232;"
        "  width: 2px;"
        "}"
        "QSplitter#DomPrintsSplitter::handle:horizontal:hover {"
        "  background: #4f4f4f;"
        "}"
        "QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 6px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #555555;"
        "  border-radius: 3px;"
        "  min-height: 24px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "  subcontrol-origin: margin;"
        "}"
        "QScrollBar::sub-page:vertical, QScrollBar::add-page:vertical {"
        "  background: transparent;"
        "}"
        );

    // ?????????????? ?????: ??????? ??????????? ????? QTabBar::pane,
    // ????? ?? ???? ?????? ??????? ?????? ??? ?????????.
    const QString extraTabPaneStyle = QStringLiteral(
        "QFrame#TabsContainer {"
        "  background-color: #252526;"
        "  border: none;"
        "}"
        "QTabBar {"
        "  border: none;"
        "  background: transparent;"
        "  qproperty-drawBase: 0;"
        "}"
        "QTabBar::pane {"
        "  border: none;"
        "  margin: 0px;"
        "  padding: 0px;"
        "}"
        "QTabBar::tab {"
        "  background: #252526;"
        "  color: #cccccc;"
        "  padding: 3px 10px;"
        "  margin: 0px;"
        "  border-left: 1px solid #444444;"
        "  border-right: none;"
        "  border-radius: 0px;"
        "}"
        "QTabBar::tab:last {"
        "  border-right: 1px solid #444444;"
        "}"
        "QTabBar::tab:only-one {"
        "  border-right: 1px solid #444444;"
        "}"
                "QTabBar::tab:selected {"
        "  background-color: #1e1e1e;"
        "  color: #ffffff;"
        "}"
        "QTabBar::tab:!selected:hover {"
        "  background: #2a2a2a;"
        "}"
        "QTabBar::close-button, QTabBar::close-button:hover {"
        "  background: transparent;"
        "  border: none;"
        "  margin: 0px;"
        "  padding: 0px;"
        "}");

    setStyleSheet(styleSheet() + extraTabPaneStyle);

    // ??????? ??????????? ?????????? ?????????.
    const QString vscodeTabsStyle = QStringLiteral(
        "QTabBar {"
        "  border: none;"
        "  background-color: #252526;"
        "  qproperty-drawBase: 0;"
        "  margin: 0px;"
        "  padding: 0px;"
        "}"
        "QTabBar::tab {"
        "  background-color: #252526;"
        "  color: #cccccc;"
        "  padding: 0px 12px;"
        "  border-left: 1px solid #444444;"
        "  border-right: none;"
        "  border-radius: 0px;"
        "  margin-left: 0px;"
        "  margin-right: 0px;"
        "  height: 100%;"
        "}"
        "QTabBar::tab:last {"
        "  border-right: 1px solid #444444;"
        "}"
        "QTabBar::tab:only-one {"
        "  border-right: 1px solid #444444;"
        "}"
        "QTabBar::tab:selected {"
        "  background-color: #1e1e1e;"
        "  color: #ffffff;"
        "}"
        "QTabBar::tab:!selected:hover {"
        "  background-color: #2d2d2d;"
        "}"
        "QTabBar::close-button, QTabBar::close-button:hover {"
        "  background: transparent;"
        "  border: none;"
        "  margin: 0px;"
        "  padding: 0px;"
        "}");

    setStyleSheet(styleSheet() + vscodeTabsStyle);

    }

    QWidget *MainWindow::buildTopBar(QWidget *parent)
{
    auto *top = new QFrame(parent);
    top->setObjectName(QStringLiteral("TitleBar"));
    m_topBar = top;
    auto *mainTopLayout = new QHBoxLayout(top);
    mainTopLayout->setContentsMargins(0, 0, 0, 0);
    mainTopLayout->setSpacing(0);
    // ????????????? ????????? ?????? ?????????,
    // ????? ?? ???? ?????? ???????? ?????? ??????.
    top->setFixedHeight(32);

    // top->setStyleSheet("border-bottom: 1px solid #444444;"); /* ????????? ?????? ??????? ??? ???? ?????????? */

    // ????? ?????? ??? ???????? (????????? ? ???????? ????)
    auto *logoContainer = new QFrame(top);
    logoContainer->setObjectName(QStringLiteral("SideToolbar"));
    logoContainer->setFixedWidth(42); // ?? ?? ??????, ??? ? ? SideToolbar
    auto *logoLayout = new QHBoxLayout(logoContainer);
    logoLayout->setContentsMargins(0, 0, 0, 0);
    logoLayout->setSpacing(0);
    logoLayout->setAlignment(Qt::AlignCenter); // ?????????? ??????? ??????????? ? ?????????????

    auto *logoLabel = new QLabel(logoContainer);
    logoLabel->setFixedSize(28, 28);
    const QString logoPath = resolveAssetPath(QStringLiteral("img/logo.png"));
    if (!logoPath.isEmpty()) {
        QPixmap pix(logoPath);
        logoLabel->setPixmap(pix.scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        logoLabel->setText(tr("Shah"));
    }
    logoLayout->addWidget(logoLabel);
    mainTopLayout->addWidget(logoContainer);

    // ??????? ?????? ??? ???????
    // ??????? ?????? ??? ???????
    auto *tabsContainer = new QFrame(top);
    tabsContainer->setObjectName(QStringLiteral("TabsContainer")); // << ???????? ???
    auto *tabsLayout = new QHBoxLayout(tabsContainer);
    tabsLayout->setContentsMargins(0, 0, 0, 0);
    tabsLayout->setSpacing(0);


    m_workspaceTabs = new QTabBar(tabsContainer);
    m_workspaceTabs->setExpanding(false);
    m_workspaceTabs->setTabsClosable(true);
    m_workspaceTabs->setMovable(true);
    m_workspaceTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_workspaceTabs->setFocusPolicy(Qt::NoFocus);
    m_workspaceTabs->setStyleSheet(
        "QTabBar { background-color: #252526; } "
        "QTabBar::tab { "
        "  color: #ffffff; min-width: 80px; padding: 4px 12px; height: 28px; margin: 0px; "
        "  border-left: 1px solid #444444; border-right: none; alignment: center; } "
        "QTabBar::tab:last { border-right: 1px solid #444444; } "
        "QTabBar::tab:only-one { border-right: 1px solid #444444; } "
        "QTabBar::tab:selected { "
        "  background-color: #1e1e1e; color: #ffffff; "
        "  border-left: 1px solid #444444; } ");
    QObject::connect(m_workspaceTabs, &QTabBar::tabMoved, this, [this](int from, int to) {
        Q_UNUSED(from);
        Q_UNUSED(to);
        if (m_tabUnderlineAnim) {
            m_tabUnderlineAnim->stop();
        }
        if (m_tabUnderline) {
            m_tabUnderline->hide();
            if (QApplication::mouseButtons() & Qt::LeftButton) {
                m_tabUnderlineHiddenForDrag = true;
            } else {
                QTimer::singleShot(0, this, [this]() {
                    if (m_workspaceTabs) {
                        updateTabUnderline(m_workspaceTabs->currentIndex());
                    }
                });
            }
        }
        QTimer::singleShot(0, this, [this]() { refreshTabCloseButtons(); });
    });
    connect(m_workspaceTabs, &QTabBar::currentChanged, this, &MainWindow::handleTabChanged);
    connect(m_workspaceTabs,
            &QTabBar::tabCloseRequested,
            this,
            &MainWindow::handleTabCloseRequested);
    tabsLayout->addWidget(m_workspaceTabs);

    m_addTabButton = new QToolButton(tabsContainer);
    m_addTabButton->setAutoRaise(true);
    const QSize addIconSize(16, 16);
    m_addTabButton->setIcon(loadIconTinted(QStringLiteral("plus"), QColor("#cfcfcf"), addIconSize));
    m_addTabButton->setIconSize(addIconSize);
    m_addTabButton->setCursor(Qt::PointingHandCursor);
    m_addTabButton->setFixedSize(28, 28);
    connect(m_addTabButton, &QToolButton::clicked, this, [this]() {
        triggerAddAction(m_lastAddAction);
    });

    m_addMenuButton = new QToolButton(tabsContainer);
    m_addMenuButton->setAutoRaise(true);
    const QSize chevronSize(10, 10);
    m_addMenuButton->setIcon(loadIconTinted(QStringLiteral("chevron-down"), QColor("#cfcfcf"), chevronSize));
    m_addMenuButton->setIconSize(chevronSize);
    m_addMenuButton->setFixedSize(24, 28);
    m_addMenuButton->setPopupMode(QToolButton::InstantPopup);
    m_addMenuButton->setCursor(Qt::PointingHandCursor);

    auto *menu = new QMenu(m_addMenuButton);
    QAction *newTabAction = menu->addAction(tr("New workspace tab"));
    QAction *newLadderAction = menu->addAction(tr("Add ladder column"));
    connect(newTabAction, &QAction::triggered, this, [this]() { triggerAddAction(AddAction::WorkspaceTab); });
    connect(newLadderAction, &QAction::triggered, this, [this]() { triggerAddAction(AddAction::LadderColumn); });
    m_addMenuButton->setMenu(menu);

    tabsLayout->addWidget(m_addTabButton, 0, Qt::AlignVCenter);
    tabsLayout->addWidget(m_addMenuButton, 0, Qt::AlignVCenter);
    updateAddButtonsToolTip();
    mainTopLayout->addWidget(tabsContainer, 1);

    auto *right = new QHBoxLayout();
    right->setSpacing(8);

    m_connectionIndicator = new QLabel(tr("Disconnected"), top);
    m_connectionIndicator->setObjectName(QStringLiteral("ConnectionIndicator"));
    m_connectionIndicator->setAlignment(Qt::AlignCenter);
    m_connectionIndicator->setMinimumWidth(110);
    m_connectionIndicator->setFixedHeight(22);
    m_connectionIndicator->setVisible(true);
    m_connectionIndicator->setCursor(Qt::PointingHandCursor);
    m_connectionIndicator->setStyleSheet(
        "QLabel#ConnectionIndicator {"
        "  border-radius: 11px;"
        "  padding: 2px 12px;"
        "  color: #ffffff;"
        "  background-color: #616161;"
        "  font-weight: 500;"
        "}");

    m_settingsSearchEdit = new QLineEdit(top);
    m_settingsSearchEdit->setPlaceholderText(tr("????? ????????..."));
    m_settingsSearchEdit->setClearButtonEnabled(true);
    m_settingsSearchEdit->setFixedWidth(260);
    m_settingsSearchEdit->setMaxLength(128);
    m_settingsSearchEdit->setObjectName(QStringLiteral("SettingsSearchEdit"));
    m_settingsSearchEdit->setStyleSheet(
        "QLineEdit#SettingsSearchEdit {"
        "  background-color: #252526;"
        "  color: #f0f0f0;"
        "  border-radius: 10px;"
        "  border: 1px solid #3c3c3c;"
        "  padding: 2px 10px;"
        "}"
        "QLineEdit#SettingsSearchEdit:focus {"
        "  border: 1px solid #007acc;"
        "}");
    connect(m_settingsSearchEdit, &QLineEdit::returnPressed, this, &MainWindow::handleSettingsSearch);
    right->addWidget(m_settingsSearchEdit);
    right->addWidget(m_connectionIndicator);

    // ????????? ?????? ? ???? Settings.
    m_settingEntries.append(
        {QStringLiteral("centerHotkey"),
         tr("???????????? ?????? ?? ??????"),
         {QStringLiteral("?????"), QStringLiteral("???????"), QStringLiteral("center"), QStringLiteral("spread")}});
    m_settingEntries.append(
        {QStringLiteral("volumeHighlight"),
         tr("????????? ??????? DOM"),
         {QStringLiteral("?????"), QStringLiteral("????"), QStringLiteral("volume"), QStringLiteral("highlight")}});
    QStringList completionNames;
    for (const auto &entry : m_settingEntries) {
        completionNames << entry.name;
    }
    m_settingsCompleter = new QCompleter(completionNames, this);
    m_settingsCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_settingsCompleter->setFilterMode(Qt::MatchContains);
    m_settingsCompleter->setCompletionMode(QCompleter::PopupCompletion);
    connect(m_settingsCompleter,
            QOverload<const QString &>::of(&QCompleter::activated),
            this,
            &MainWindow::handleSettingsSearchFromCompleter);
    m_settingsSearchEdit->setCompleter(m_settingsCompleter);

    m_timeLabel = new QLabel(top);
    m_timeLabel->setCursor(Qt::PointingHandCursor);
    right->addWidget(m_timeLabel);

    auto makeWinButton = [top](const QString &text, const char *objectName) {
        auto *btn = new QToolButton(top);
        btn->setText(text);
        btn->setObjectName(QLatin1String(objectName));
        btn->setAutoRaise(true);
        btn->setFixedSize(42, 32);
        return btn;
    };

    // Helper to fix window geometry if it ends up off-screen or clipped.
    auto fixWindowGeometry = [this]() {
        QRect geom = geometry();
        QScreen *scr = QGuiApplication::screenAt(geom.center());
        if (!scr) scr = QGuiApplication::primaryScreen();
        if (!scr) return;
        const QRect work = scr->availableGeometry();

        const QRect inter = work.intersected(geom);
        // If intersection is too small or window is outside work area, recenter/resize
        if (inter.width() < geom.width() / 2 || inter.height() < geom.height() / 2 || !work.contains(geom)) {
            int w = std::min(geom.width(), work.width());
            int h = std::min(geom.height(), work.height());
            int x = work.x() + (work.width() - w) / 2;
            int y = work.y() + (work.height() - h) / 2;
            qDebug() << "[MainWindow] correcting geometry to" << QRect(x, y, w, h) << " work=" << work << " geom=" << geom;
            setGeometry(x, y, w, h);
            raise();
            activateWindow();
        }
    };

    auto *minButton = makeWinButton(QString(), "WindowButtonMin");
    auto *maxButton = makeWinButton(QString(), "WindowButtonMax");
    auto *closeButton = makeWinButton(QString(), "WindowButtonClose");

    const QSize winIconSize(16, 16);
    const QSize maxIconSize(14, 14);
    const QColor winIconColor("#ffffff");
    minButton->setIcon(loadIconTinted(QStringLiteral("minus"), winIconColor, winIconSize));
    minButton->setIconSize(winIconSize);
    maxButton->setIcon(loadIconTinted(QStringLiteral("square"), winIconColor, maxIconSize));
    maxButton->setIconSize(maxIconSize);
    closeButton->setIcon(loadIconTinted(QStringLiteral("x"), winIconColor, winIconSize));
    closeButton->setIconSize(winIconSize);

    // store pointers to buttons so we can update icons on state changes
    m_minButton = minButton;
    m_maxButton = maxButton;
    m_closeButton = closeButton;

    QObject::connect(minButton, &QToolButton::clicked, this, [this]() {
        qDebug() << "[MainWindow] Min button clicked. windowState=" << windowState();
        logToFile(QStringLiteral("Min button clicked. state=%1 geometry=%2").arg(QString::number((int)windowState())).arg(rectToString(geometry())));
        showMinimized();
        // schedule a delayed check when window is restored later
        QTimer::singleShot(200, this, [this]() {
            // If we have a saved normal geometry, ensure it's valid after restore
            if (m_haveLastNormalGeometry && !isMaximized()) {
                setGeometry(m_lastNormalGeometry);
                ensureNativeBounds(this, m_lastNormalGeometry);
                logToFile(QStringLiteral("Applied saved normal geometry after minimize-restore: %1").arg(rectToString(m_lastNormalGeometry)));
            } else if (!isMaximized()) {
                // fallback correction
                QRect geom = geometry();
                QScreen *scr = QGuiApplication::screenAt(geom.center());
                if (!scr) scr = QGuiApplication::primaryScreen();
                if (scr) {
                    const QRect work = scr->availableGeometry();
                    const QRect inter = work.intersected(geom);
                    if (inter.width() < geom.width() / 2 || inter.height() < geom.height() / 2 || !work.contains(geom)) {
                        int w = std::min(geom.width(), work.width());
                        int h = std::min(geom.height(), work.height());
                        int x = work.x() + (work.width() - w) / 2;
                        int y = work.y() + (work.height() - h) / 2;
                        QRect desired(x, y, w, h);
                        setGeometry(desired);
                        ensureNativeBounds(this, desired);
                        logToFile(QStringLiteral("Fallback geometry correction after minimize-restore: %1").arg(rectToString(desired)));
                        raise();
                        activateWindow();
                    }
                }
            }
        });
    });

    QObject::connect(maxButton, &QToolButton::clicked, this, [this]() {
        const bool maximized = isMaximized();
        qDebug() << "[MainWindow] Max button clicked. before isMaximized=" << maximized
                 << " geometry=" << geometry() << " windowState=" << windowState();
        const QString s = QStringLiteral("[MainWindow] Max clicked before: isMaximized=%1").arg(maximized ? QStringLiteral("1") : QStringLiteral("0"));
        {
            const std::wstring ws = s.toStdWString();
            OutputDebugStringW(ws.c_str());
        }

        if (!maximized) {
            // About to maximize: save current normal geometry so we can restore it later
            m_lastNormalGeometry = geometry();
            m_haveLastNormalGeometry = true;
                logToFile(QStringLiteral("Saving normal geometry before maximize: %1").arg(rectToString(m_lastNormalGeometry)));
            showMaximized();
        } else {
            // Restore from maximized: use saved normal geometry if available
            // To avoid native geometry/desync issues with frameless windows,
            // temporarily disable the frameless flag so the system applies
            // normal window decoration and bounds, then re-enable our
            // frameless UI and apply the saved geometry.
            if (m_haveLastNormalGeometry) {
                // Hide heavy content to mask visual jumps during restore
                QWidget *central = centralWidget();
                if (central) central->setVisible(false);

                logToFile(QStringLiteral("Restoring from maximized: temporarily disabling FramelessWindowHint"));
                // disable frameless so Windows will restore native bounds
                setWindowFlag(Qt::FramelessWindowHint, false);
                showNormal();
                // allow the windowing system to settle, then reapply our geometry
                QTimer::singleShot(180, this, [this, central]() {
                    setGeometry(m_lastNormalGeometry);
                    ensureNativeBounds(this, m_lastNormalGeometry);
                    // re-enable frameless and show again to apply our custom titlebar
                    setWindowFlag(Qt::FramelessWindowHint, true);
                    // Calling show() will update flags; ensure the window is active
                    show();
                    // Reapply native snap style bits in case they were changed
#ifdef Q_OS_WIN
                    applyNativeSnapStyleForHwnd(reinterpret_cast<HWND>(winId()));
#endif
                    raise();
                    activateWindow();
                    logToFile(QStringLiteral("Restored saved normal geometry after un-maximize (frameless-toggle): %1").arg(rectToString(m_lastNormalGeometry)));

                    // Reveal content with a quick fade to mask remaining jumps
                    if (central) {
                        animateWindowFadeIn(this);
                        central->setVisible(true);
                    } else {
                        animateWindowFadeIn(this);
                    }
                });
            } else {
                // fallback small correction after restore
                QTimer::singleShot(50, this, [this]() {
                    QRect geom = geometry();
                    QScreen *scr = QGuiApplication::screenAt(geom.center());
                    if (!scr) scr = QGuiApplication::primaryScreen();
                    if (!scr) return;
                    const QRect work = scr->availableGeometry();
                    const QRect inter = work.intersected(geom);
                    if (inter.width() < geom.width() / 2 || inter.height() < geom.height() / 2 || !work.contains(geom)) {
                        int w = std::min(geom.width(), work.width());
                        int h = std::min(geom.height(), work.height());
                        int x = work.x() + (work.width() - w) / 2;
                        int y = work.y() + (work.height() - h) / 2;
                        QRect correctedDesired(x, y, w, h); // ????????? desired ?????
                        setGeometry(correctedDesired);
                        ensureNativeBounds(this, correctedDesired);
                        logToFile(QStringLiteral("Fallback geometry correction after minimize-restore: %1").arg(rectToString(correctedDesired)));
                        raise();
                        activateWindow();
                    }
                });
            }
        }

        updateMaximizeIcon();
        QTimer::singleShot(100, this, [this]() {
            qDebug() << "[MainWindow] geometry after action:" << geometry() << " state=" << windowState();
            logToFile(QStringLiteral("Post-max action geometry: %1 state=%2").arg(rectToString(geometry())).arg((int)windowState()));
        });
    });
    QObject::connect(closeButton, &QToolButton::clicked, this, &QWidget::close);

    // set initial maximize/restore icon
    updateMaximizeIcon();

    right->addWidget(minButton);
    right->addWidget(maxButton);
    right->addWidget(closeButton);

    mainTopLayout->addLayout(right);

    // Caption area for dragging/maximizing.
    top->installEventFilter(this);
    m_workspaceTabs->installEventFilter(this);
    m_addTabButton->installEventFilter(this);
    m_connectionIndicator->installEventFilter(this);
    m_timeLabel->installEventFilter(this);

    return top;
}
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();
    const Qt::KeyboardModifiers mods = event->modifiers();

    if (m_notionalEditActive) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    if (matchesHotkey(key, mods, m_newTabKey, m_newTabMods)) {
        handleNewTabRequested();
        event->accept();
        return;
    }
    if (matchesHotkey(key, mods, m_addLadderKey, m_addLadderMods)) {
        handleNewLadderRequested();
        event->accept();
        return;
    }
    if (matchesHotkey(key, mods, m_refreshLadderKey, m_refreshLadderMods)) {
        refreshActiveLadder();
        event->accept();
        return;
    }
    if (matchesHotkey(key, mods, m_volumeAdjustKey, m_volumeAdjustMods)) {
        m_capsAdjustMode = true;
        event->accept();
        return;
    }
    for (int i = 0; i < static_cast<int>(m_notionalPresetKeys.size()); ++i)
    {
        if (matchesHotkey(key, mods, m_notionalPresetKeys[i], m_notionalPresetMods[i]))
        {
            applyNotionalPreset(i);
            event->accept();
            return;
        }
    }

    bool match = false;
    if (key == Qt::Key_Space && mods == Qt::NoModifier) {
        if (m_tradeManager) {
            DomColumn *col = focusedDomColumn();
            if (col) {
                m_tradeManager->cancelAllOrders(col->symbol, col->accountName);
            }
        }
        event->accept();
        return;
    }
    if (key == m_centerKey) {
        if (m_centerMods == Qt::NoModifier) {
            match = true;
        } else {
            Qt::KeyboardModifiers cleaned = mods & ~Qt::KeypadModifier;
            match = (cleaned == m_centerMods);
        }
    }

    if (match) {
        centerActiveLaddersToSpread();
        event->accept();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (matchesHotkey(event->key(), event->modifiers(), m_volumeAdjustKey, m_volumeAdjustMods)) {
        m_capsAdjustMode = false;
    }
    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event && event->type() == QEvent::WindowStateChange) {
        // Handle transitions between Minimized / Maximized / Normal reliably.
        const Qt::WindowStates cur = windowState();
        // If we just became maximized (possibly via system/Aero Snap), save
        // the normal geometry reported by the window system so we can restore
        // it later.
        if (cur.testFlag(Qt::WindowMaximized)) {
            // QWidget::normalGeometry() is available on the widget and is
            // preferred here (some Qt versions don't expose normalGeometry()
            // on QWindow).
            const QRect normal = normalGeometry();
            if (normal.isValid()) {
                m_lastNormalGeometry = normal;
                m_haveLastNormalGeometry = true;
                qDebug() << "[MainWindow] saved normalGeometry from widget() :" << normal;
                logToFile(QStringLiteral("changeEvent: saved normalGeometry: %1").arg(rectToString(normal)));
            }
        }

        // If we transitioned from Minimized -> Normal, restore previous
        // maximized/normal behaviour depending on what we had before.
        if (m_prevWindowState.testFlag(Qt::WindowMinimized) && !cur.testFlag(Qt::WindowMinimized)) {
            // We are restoring from minimize.
            if (m_prevWindowState.testFlag(Qt::WindowMaximized)) {
                // If previously maximized, restore maximized but hide content briefly
                QWidget *central = centralWidget();
                if (central) central->setVisible(false);
                QTimer::singleShot(0, this, [this, central]() {
                    showMaximized();
                    QTimer::singleShot(160, this, [this, central]() {
                        if (central) {
                            animateWindowFadeIn(this);
                            central->setVisible(true);
                        } else {
                            animateWindowFadeIn(this);
                        }
                    });
                });
            } else if (m_haveLastNormalGeometry) {
                // If previously normal, restore saved normal geometry with content hidden
                QWidget *central = centralWidget();
                if (central) central->setVisible(false);
                QTimer::singleShot(120, this, [this, central]() {
                    setGeometry(m_lastNormalGeometry);
                    ensureNativeBounds(this, m_lastNormalGeometry);
                    logToFile(QStringLiteral("changeEvent: restored saved geometry after un-minimize: %1").arg(rectToString(m_lastNormalGeometry)));
                    raise();
                    activateWindow();
                    if (central) {
                        animateWindowFadeIn(this);
                        central->setVisible(true);
                    } else {
                        animateWindowFadeIn(this);
                    }
                });
            }
        }

        updateMaximizeIcon();
        m_prevWindowState = cur;
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::updateMaximizeIcon()
{
    if (!m_maxButton) return;
    const bool maximized = windowState().testFlag(Qt::WindowMaximized);
    const QSize maxIconSize(14, 14);
    if (maximized) {
        QIcon icon = loadIconTinted(QStringLiteral("squares"), QColor("#ffffff"), maxIconSize);
        m_maxButton->setIcon(mirrorIconHorizontally(icon, maxIconSize));
    } else {
        m_maxButton->setIcon(loadIconTinted(QStringLiteral("square"), QColor("#ffffff"), maxIconSize));
    }
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType);
    MSG *msg = static_cast<MSG *>(message);
    if (!msg) return QMainWindow::nativeEvent(eventType, message, result);

    if (msg->message == WM_NCCALCSIZE) {
        // Remove standard title bar and non-client area ? we draw our own TitleBar.
        // For maximized/snapped windows Windows may leave space for the standard
        // non-client frame. When that happens we must explicitly set the client
        // rect to the monitor work area so there are no stray gaps on the right/bottom
        // (classic frameless-window vs Aero Snap issue).
        if (msg->wParam == TRUE && msg->lParam) {
            NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
            RECT &r = params->rgrc[0];

            HWND hwnd = msg->hwnd;
            if (hwnd && IsZoomed(hwnd)) {
                // Window is maximized (this also covers Aero Snap to top):
                // align client rect to monitor work area (respects taskbar).
                HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi;
                mi.cbSize = sizeof(mi);
                if (GetMonitorInfoW(hMonitor, &mi)) {
                    // Use rcWork to avoid covering the taskbar
                    r = mi.rcWork;
                    logToFile(QStringLiteral("WM_NCCALCSIZE applied rcWork: %1").arg(rectToString(QRect(r.left, r.top, r.right - r.left, r.bottom - r.top))));
                }
            }

            *result = 0;
            return true;
        }
        *result = 0;
        return true;
    }

        if (msg->message == WM_NCHITTEST) {
        HWND hwndHit = msg->hwnd;
        // When maximized/snapped, treat everything as client to avoid inner gaps.
        if (hwndHit && IsZoomed(hwndHit)) {
            *result = HTCLIENT;
            return true;
        }

        const LONG x = GET_X_LPARAM(msg->lParam);
        const LONG y = GET_Y_LPARAM(msg->lParam);
        const QPoint globalPt(x, y);

        const QRect g = frameGeometry();
        const QPoint localPt = mapFromGlobal(globalPt);

        const int w = g.width();
        const int h = g.height();
        const int bw = m_resizeBorderWidth;

        // Corners first
        if (localPt.x() < bw && localPt.y() < bw) { *result = HTTOPLEFT; return true; }
        if (localPt.x() >= w - bw && localPt.y() < bw) { *result = HTTOPRIGHT; return true; }
        if (localPt.x() < bw && localPt.y() >= h - bw) { *result = HTBOTTOMLEFT; return true; }
        if (localPt.x() >= w - bw && localPt.y() >= h - bw) { *result = HTBOTTOMRIGHT; return true; }

        // Edges
        if (localPt.x() < bw) { *result = HTLEFT; return true; }
        if (localPt.x() >= w - bw) { *result = HTRIGHT; return true; }
        if (localPt.y() < bw) { *result = HTTOP; return true; }
        if (localPt.y() >= h - bw) { *result = HTBOTTOM; return true; }

        // Title bar: empty area => caption drag, interactive widgets => client.
        if (m_topBar) {
            const QPoint localInTop = m_topBar->mapFromGlobal(globalPt);
            if (m_topBar->rect().contains(localInTop)) {
                QWidget *child = m_topBar->childAt(localInTop);
                bool interactive = false;

                if (!child) {
                    interactive = false;
                } else if (child == m_minButton || child == m_maxButton || child == m_closeButton ||
                           child == m_addTabButton || child == m_timeLabel || child == m_connectionIndicator) {
                    interactive = true;
                } else if (child == m_workspaceTabs) {
                    QPoint inTabs = m_workspaceTabs->mapFrom(m_topBar, localInTop);
                    int tabIndex = m_workspaceTabs->tabAt(inTabs);
                    interactive = (tabIndex != -1);
                } else {
                    interactive = true;
                }

                if (!interactive) {
                    *result = HTCAPTION;
                    return true;
                }
            }
        }

        *result = HTCLIENT;
        return true;
    }
if (msg->message == WM_GETMINMAXINFO) {
        // Ensure maximized size/position match the monitor work area so
        // Windows maximization (and Aero Snap) doesn't leave gaps or shift
        // the window. Classic fix for frameless windows.
        if (msg->lParam) {
            MINMAXINFO *mmi = reinterpret_cast<MINMAXINFO *>(msg->lParam);
            HMONITOR hMonitor = MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi;
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfoW(hMonitor, &mi)) {
                const RECT &rcWork = mi.rcWork;
                const RECT &rcMonitor = mi.rcMonitor;

                mmi->ptMaxPosition.x = rcWork.left - rcMonitor.left;
                mmi->ptMaxPosition.y = rcWork.top - rcMonitor.top;
                mmi->ptMaxSize.x = rcWork.right - rcWork.left;
                mmi->ptMaxSize.y = rcWork.bottom - rcWork.top;
                mmi->ptMaxTrackSize = mmi->ptMaxSize;
                logToFile(QStringLiteral("WM_GETMINMAXINFO set ptMaxSize=%1 pos=%2").arg(QString::number(mmi->ptMaxSize.x) + 'x' + QString::number(mmi->ptMaxSize.y), QString::number(mmi->ptMaxPosition.x) + "," + QString::number(mmi->ptMaxPosition.y)));
            }
            *result = 0;
            return true;
        }
    }

    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

QWidget *MainWindow::buildMainArea(QWidget *parent)
{
    auto *main = new QFrame(parent);
    main->setObjectName(QStringLiteral("MainAreaFrame"));
    auto *layout = new QHBoxLayout(main);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *sidebar = new QFrame(main);
    sidebar->setObjectName(QStringLiteral("SideToolbar"));
    // ???? ????, ??? ? VSCode (????????? ?? 2px ?? ???????)
        sidebar->setFixedWidth(42);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 12, 0, 12);
    sideLayout->setSpacing(12);

    const QSize navIconSize(28, 28);
    const QColor navIconColor("#c0c0c0");

    auto makeSideButton = [this, sidebar, navIconSize, navIconColor](const QString &iconName,
                                                                     const QString &tooltip) {
        auto *btn = new QToolButton(sidebar);
        btn->setObjectName(QStringLiteral("SideNavButton"));
        btn->setAutoRaise(true);
        btn->setFixedSize(42, 32);
        // Default icon: light gray
        btn->setIcon(loadIconTinted(iconName, navIconColor, navIconSize));
        btn->setIconSize(navIconSize);
        btn->setToolTip(tooltip);
        // Make cursor indicate clickable and allow handling hover in eventFilter
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("iconName", iconName);
        btn->installEventFilter(this);
        return btn;
    };

    // ??????????? / ??????
    {
        m_connectionButton = makeSideButton(QStringLiteral("plug-connected"), tr("Connection"));
        sideLayout->addWidget(m_connectionButton, 0, Qt::AlignHCenter);
        connect(m_connectionButton, &QToolButton::clicked, this, &MainWindow::openConnectionsWindow);
    }

    // ?????????? ????????? / PnL
    {
        QToolButton *b = makeSideButton(QStringLiteral("report-money"), tr("P&L / Results"));
        sideLayout->addWidget(b, 0, Qt::AlignHCenter);
    }

    // ?????? / ??????
    {
        QToolButton *b = makeSideButton(QStringLiteral("arrows-exchange"), tr("Trades"));
        sideLayout->addWidget(b, 0, Qt::AlignHCenter);
    }

    // ???? (?????? cube-plus)
    auto *modsButton = makeSideButton(QStringLiteral("cube-plus"), tr("Mods"));
    sideLayout->addWidget(modsButton, 0, Qt::AlignHCenter);
    connect(modsButton, &QToolButton::clicked, this, &MainWindow::openPluginsWindow);

    // ??????
    {
        m_alertsButton = makeSideButton(QStringLiteral("bell"), tr("Alerts"));
        sideLayout->addWidget(m_alertsButton, 0, Qt::AlignHCenter);
        connect(m_alertsButton, &QToolButton::clicked, this, &MainWindow::toggleAlertsPanel);

        m_alertsBadge = new QLabel(m_alertsButton);
        m_alertsBadge->setObjectName(QStringLiteral("AlertsBadge"));
        m_alertsBadge->setAlignment(Qt::AlignCenter);
        m_alertsBadge->setMinimumWidth(16);
        m_alertsBadge->setFixedHeight(16);
        m_alertsBadge->setStyleSheet(
            "QLabel#AlertsBadge {"
            "  background-color: #2e8bdc;"
            "  color: #ffffff;"
            "  font-weight: 700;"
            "  border-radius: 8px;"
            "  border: 1px solid #0f1f30;"
            "  font-size: 9px;"
            "  padding: 0 3px;"
            "}");
        m_alertsBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_alertsBadge->hide();
        updateAlertsBadge();
    }

    // ?????? / ??????????
    {
        QToolButton *b = makeSideButton(QStringLiteral("alarm"), tr("Timer"));
        sideLayout->addWidget(b, 0, Qt::AlignHCenter);
    }

    sideLayout->addStretch(1);

    // ????????? ? ????? ???? (????????? ?????)
    auto *settingsNav = makeSideButton(QStringLiteral("settings"), tr("Settings"));
    sideLayout->addWidget(settingsNav, 0, Qt::AlignHCenter | Qt::AlignBottom);
    connect(settingsNav, &QToolButton::clicked, this, &MainWindow::openSettingsWindow);

    layout->addWidget(sidebar);

    m_workspaceStack = new QStackedWidget(main);
    layout->addWidget(m_workspaceStack, 1);
    m_workspaceStack->installEventFilter(this);

    // Alerts panel overlay (hidden by default)
    m_alertsPanel = new QFrame(m_workspaceStack);
    m_alertsPanel->setObjectName(QStringLiteral("AlertsPanel"));
    m_alertsPanel->setVisible(false);
    m_alertsPanel->setStyleSheet(
        "QFrame#AlertsPanel {"
        "  background-color: #15181f;"
        "  border: 1px solid #28313d;"
        "  border-radius: 8px;"
        "}"
        "QListWidget#AlertsList {"
        "  background: transparent;"
        "  border: none;"
        "  outline: none;"
        "  color: #e0e0e0;"
        "}"
        "QPushButton#AlertsMarkReadButton {"
        "  background-color: #263445;"
        "  color: #e0e0e0;"
        "  padding: 4px 8px;"
        "  border: 1px solid #2f4054;"
        "  border-radius: 4px;"
        "}"
        "QPushButton#AlertsMarkReadButton:hover {"
        "  background-color: #2f4054;"
        "}");

    auto *alertsLayout = new QVBoxLayout(m_alertsPanel);
    alertsLayout->setContentsMargins(10, 8, 10, 10);
    alertsLayout->setSpacing(6);

    auto *alertsHeader = new QHBoxLayout();
    alertsHeader->setContentsMargins(0, 0, 0, 0);
    alertsHeader->setSpacing(6);
    auto *alertsTitle = new QLabel(tr("Notifications"), m_alertsPanel);
    alertsTitle->setStyleSheet("color: #ffffff; font-weight: 600;");
    auto *markRead = new QPushButton(tr("Mark all read"), m_alertsPanel);
    markRead->setObjectName(QStringLiteral("AlertsMarkReadButton"));
    alertsHeader->addWidget(alertsTitle);
    alertsHeader->addStretch(1);
    alertsHeader->addWidget(markRead);
    alertsLayout->addLayout(alertsHeader);

    m_alertsList = new QListWidget(m_alertsPanel);
    m_alertsList->setObjectName(QStringLiteral("AlertsList"));
    m_alertsList->setSelectionMode(QAbstractItemView::NoSelection);
    m_alertsList->setFocusPolicy(Qt::NoFocus);
    m_alertsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_alertsList->setTextElideMode(Qt::ElideNone);
    m_alertsList->setWordWrap(true);
    alertsLayout->addWidget(m_alertsList, 1);

    connect(markRead, &QPushButton::clicked, this, &MainWindow::markAllNotificationsRead);

    repositionAlertsPanel();

    return main;
}

void MainWindow::createInitialWorkspace()
{
    if (!m_savedLayout.isEmpty()) {
        for (const auto &cols : m_savedLayout) {
            createWorkspaceTab(cols);
        }
    } else {
        QVector<SavedColumn> cols;
        cols.reserve(m_symbols.size());
        for (const QString &s : m_symbols) {
            const QString trimmed = s.trimmed();
            if (trimmed.isEmpty()) {
                continue;
            }
            SavedColumn sc;
            sc.symbol = trimmed;
            sc.compression = 1;
            sc.account = QStringLiteral("MEXC Spot");
            cols.push_back(sc);
        }
        createWorkspaceTab(cols);
    }
}

void MainWindow::updateTabUnderline(int index)
{
    if (!m_workspaceTabs || index < 0) {
        return;
    }

    if (!m_tabUnderline) {
        m_tabUnderline = new QFrame(m_workspaceTabs);
        m_tabUnderline->setObjectName(QStringLiteral("TabUnderline"));
        m_tabUnderline->setStyleSheet(QStringLiteral("background-color: #007acc; border: none;"));
        m_tabUnderline->setFixedHeight(2);
    }
    if (!m_tabUnderlineAnim) {
        m_tabUnderlineAnim = new QPropertyAnimation(m_tabUnderline, "geometry", this);
        m_tabUnderlineAnim->setDuration(350);

        m_tabUnderlineAnim->setEasingCurve(QEasingCurve::InOutCubic);
    }

    const QRect tabRect = m_workspaceTabs->tabRect(index);
    if (!tabRect.isValid()) {
        return;
    }

    QRect startRect(tabRect.left(), 0, 0, m_tabUnderline->height());
    QRect endRect(tabRect.left(), 0, tabRect.width(), m_tabUnderline->height());

    m_tabUnderline->setGeometry(startRect);
    m_tabUnderline->show();
    m_tabUnderline->raise();

    m_tabUnderlineAnim->stop();
    m_tabUnderlineAnim->setStartValue(startRect);
    m_tabUnderlineAnim->setEndValue(endRect);
    m_tabUnderlineAnim->start();
}

MainWindow::WorkspaceTab MainWindow::createWorkspaceTab(const QVector<SavedColumn> &columnsSpec)
{
    int tabId = 0;
    if (!m_recycledTabIds.isEmpty()) {
        std::sort(m_recycledTabIds.begin(), m_recycledTabIds.end());
        tabId = m_recycledTabIds.takeFirst();
    } else {
        tabId = m_nextTabId++;
    }

    auto *workspace = new QFrame(m_workspaceStack);
    auto *wsLayout = new QVBoxLayout(workspace);
    wsLayout->setContentsMargins(0, 0, 0, 0);
    wsLayout->setSpacing(0);

    auto *columnsContainer = new QFrame(workspace);
    auto *columnsRow = new QHBoxLayout(columnsContainer);
    columnsRow->setContentsMargins(0, 0, 0, 0);
    columnsRow->setSpacing(0);

    auto *columnsSplitter = new QSplitter(Qt::Horizontal, columnsContainer);
    columnsSplitter->setObjectName(QStringLiteral("WorkspaceColumnsSplitter"));
    columnsSplitter->setChildrenCollapsible(false);
    columnsSplitter->setHandleWidth(8);
    columnsSplitter->setStyleSheet(QStringLiteral(
        "QSplitter#WorkspaceColumnsSplitter::handle { margin: 0px -4px 0px -4px; padding: 0px; background-color: #444444; }"
        "QSplitter#WorkspaceColumnsSplitter::handle:horizontal:hover { background-color: #5a5a5a; }"
        "QSplitter#WorkspaceColumnsSplitter::handle:horizontal:pressed { background-color: #777777; }"));
    columnsSplitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    columnsRow->addWidget(columnsSplitter, 1);

    WorkspaceTab tab;
    tab.id = tabId;
    tab.workspace = workspace;
    tab.columns = columnsSplitter;

    QVector<SavedColumn> specs = columnsSpec;
    if (specs.isEmpty()) {
        specs.reserve(m_symbols.size());
        for (const QString &s : m_symbols) {
            const QString trimmed = s.trimmed();
            if (trimmed.isEmpty()) {
                continue;
            }
            SavedColumn sc;
            sc.symbol = trimmed;
            sc.compression = 1;
            sc.account = QStringLiteral("MEXC Spot");
            specs.push_back(sc);
        }
    }

    for (const auto &spec : specs) {
        DomColumn col = createDomColumn(spec.symbol, spec.account.isEmpty() ? QStringLiteral("MEXC Spot") : spec.account, tab);
        col.tickCompression = std::max(1, spec.compression);
        col.accountName = spec.account.isEmpty() ? QStringLiteral("MEXC Spot") : spec.account;
        col.accountColor = accountColorFor(col.accountName);
        if (col.tickerLabel) {
            col.tickerLabel->setProperty("accountColor", col.accountColor);
            applyTickerLabelStyle(col.tickerLabel, col.accountColor, false);
        }
        applyHeaderAccent(col);
        if (col.compressionButton) {
            col.compressionButton->setText(QStringLiteral("%1x").arg(col.tickCompression));
        }
        if (col.client) {
            col.client->setCompression(col.tickCompression);
        }
        tab.columnsData.push_back(col);
        columnsSplitter->addWidget(col.container);
        columnsSplitter->setStretchFactor(columnsSplitter->indexOf(col.container), 0);
    }

    auto *splitterSpacer = new QWidget(columnsSplitter);
    splitterSpacer->setObjectName(QStringLiteral("DomSplitterSpacer"));
    splitterSpacer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    splitterSpacer->setFocusPolicy(Qt::NoFocus);
    splitterSpacer->setMinimumWidth(1);
    splitterSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    splitterSpacer->setStyleSheet(QStringLiteral("background: transparent;"));
    columnsSplitter->addWidget(splitterSpacer);
    columnsSplitter->setStretchFactor(columnsSplitter->indexOf(splitterSpacer), 1);
    tab.columnsSpacer = splitterSpacer;

    wsLayout->addWidget(columnsContainer, 1);

    const int stackIndex = m_workspaceStack->addWidget(workspace);

    const int tabIndex = m_workspaceTabs->addTab(QStringLiteral("Tab %1").arg(tabId));
    m_workspaceTabs->setTabData(tabIndex, tabId);
    m_workspaceTabs->setTabText(tabIndex, QStringLiteral("Tab %1").arg(tabId));

    m_tabs.push_back(tab);

    m_workspaceTabs->setCurrentIndex(tabIndex);
    m_workspaceStack->setCurrentIndex(stackIndex);
    refreshTabCloseButtons();
    return tab;
}

void MainWindow::triggerAddAction(AddAction action)
{
    setLastAddAction(action);
    switch (action) {
    case AddAction::WorkspaceTab:
        handleNewTabRequested();
        break;
    case AddAction::LadderColumn:
        handleNewLadderRequested();
        break;
    }
}

void MainWindow::setLastAddAction(AddAction action)
{
    m_lastAddAction = action;
    updateAddButtonsToolTip();
}

void MainWindow::updateAddButtonsToolTip()
{
    if (!m_addTabButton) {
        return;
    }
    QString text;
    switch (m_lastAddAction) {
    case AddAction::WorkspaceTab:
        text = tr("Add workspace tab");
        break;
    case AddAction::LadderColumn:
        text = tr("Add ladder column");
        break;
    }
    m_addTabButton->setToolTip(text);
}

void MainWindow::refreshTabCloseButtons()
{
    if (!m_workspaceTabs) {
        return;
    }
    const QSize iconSize(14, 14);
    if (m_tabCloseIconNormal.isNull()) {
        m_tabCloseIconNormal = loadIconTinted(QStringLiteral("x"), QColor("#bfbfbf"), iconSize);
    }
    if (m_tabCloseIconHover.isNull()) {
        m_tabCloseIconHover = loadIconTinted(QStringLiteral("x"), QColor("#ffffff"), iconSize);
    }

    for (int i = 0; i < m_workspaceTabs->count(); ++i) {
        QToolButton *button = qobject_cast<QToolButton *>(m_workspaceTabs->tabButton(i, QTabBar::RightSide));
        if (!button || !button->property("WorkspaceTabCloseButton").toBool()) {
            button = new QToolButton(m_workspaceTabs);
            button->setAutoRaise(true);
            button->setObjectName(QStringLiteral("WorkspaceTabCloseButton"));
            button->setProperty("WorkspaceTabCloseButton", true);
            button->setCursor(Qt::PointingHandCursor);
            button->setFocusPolicy(Qt::NoFocus);
            button->setStyleSheet(QStringLiteral(
                "QToolButton#WorkspaceTabCloseButton {"
                "  border: none;"
                "  background: transparent;"
                "  margin: 0px;"
                "  padding: 0px;"
                "}"
                "QToolButton#WorkspaceTabCloseButton:hover {"
                "  background-color: #3a3a3a;"
                "}"));
            connect(button, &QToolButton::clicked, this, [this, button]() {
                if (!m_workspaceTabs) {
                    return;
                }
                for (int idx = 0; idx < m_workspaceTabs->count(); ++idx) {
                    if (m_workspaceTabs->tabButton(idx, QTabBar::RightSide) == button) {
                        handleTabCloseRequested(idx);
                        break;
                    }
                }
            });
            button->installEventFilter(this);
            m_workspaceTabs->setTabButton(i, QTabBar::RightSide, button);
        }

        if (!m_tabCloseIconNormal.isNull()) {
            button->setIcon(m_tabCloseIconNormal);
        }
        button->setIconSize(QSize(12, 12));
        button->setFixedSize(18, 18);
        button->setContentsMargins(0, 0, 0, 0);
        button->setToolTip(tr("Close tab"));
    }
}

MainWindow::DomColumn MainWindow::createDomColumn(const QString &symbol,
                                                  const QString &accountName,
                                                  WorkspaceTab &tab)
{
    DomColumn result;
    result.symbol = symbol.toUpper();
    result.accountName = accountName.isEmpty() ? QStringLiteral("MEXC Spot") : accountName;

    auto *column = new QFrame(tab.workspace);
    column->setObjectName(QStringLiteral("DomColumnFrame"));
    column->setMouseTracking(true);
    column->installEventFilter(this);

    auto *columnRowLayout = new QHBoxLayout(column);
    columnRowLayout->setContentsMargins(0, 0, 0, 0);
    columnRowLayout->setSpacing(0);

    auto *columnSplitter = new QSplitter(Qt::Horizontal, column);
    columnSplitter->setObjectName(QStringLiteral("DomColumnInnerSplitter"));
    columnSplitter->setChildrenCollapsible(false);
    columnSplitter->setHandleWidth(3);
    columnSplitter->setStyleSheet(QStringLiteral(
        "QSplitter#DomColumnInnerSplitter::handle {"
        "  background-color: #2b2b2b;"
        "}"
        "QSplitter#DomColumnInnerSplitter::handle:hover {"
        "  background-color: #3a3a3a;"
        "}"));
    columnRowLayout->addWidget(columnSplitter);

    auto *columnContent = new QWidget(columnSplitter);
    auto *layout = new QVBoxLayout(columnContent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    columnSplitter->addWidget(columnContent);

    auto *header = new QFrame(column);
    header->setObjectName(QStringLiteral("DomTitleBar"));
    header->setProperty("domContainerPtr", QVariant::fromValue<void *>(column));
    auto *hLayout = new QHBoxLayout(header);
    hLayout->setContentsMargins(8, 4, 8, 4);
    hLayout->setSpacing(6);

    result.accountName = QStringLiteral("MEXC Spot");
    result.accountColor = accountColorFor(result.accountName);
    result.header = header;
    applyHeaderAccent(result);

    auto *tickerLabel = new QLabel(result.symbol, header);
    tickerLabel->setProperty("accountColor", result.accountColor);
    applyTickerLabelStyle(tickerLabel, result.accountColor, false);
    tickerLabel->setCursor(Qt::PointingHandCursor);
    tickerLabel->setMouseTracking(true);
    tickerLabel->installEventFilter(this);
    hLayout->addWidget(tickerLabel);
    result.tickerLabel = tickerLabel;

    auto *levelsSpin = new QSpinBox(header);
    levelsSpin->setRange(50, 4000);
    levelsSpin->setValue(m_levels);
    levelsSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    levelsSpin->setFixedWidth(52);
    levelsSpin->setAlignment(Qt::AlignRight);
    levelsSpin->setToolTip(tr("Levels per side"));
    hLayout->addWidget(levelsSpin);

    auto *compressionButton = new QToolButton(header);
    compressionButton->setAutoRaise(true);
    compressionButton->setText(QStringLiteral("1x"));
    compressionButton->setCursor(Qt::PointingHandCursor);
    compressionButton->setFixedSize(32, 20);
    compressionButton->setToolTip(tr("Ticks per row (compression)"));
    compressionButton->setObjectName(QStringLiteral("CompressionButton"));
    hLayout->addWidget(compressionButton);

    hLayout->addStretch(1);
    auto *zoomOutButton = new QToolButton(header);
    zoomOutButton->setAutoRaise(true);
    zoomOutButton->setText(QStringLiteral("-"));
    zoomOutButton->setCursor(Qt::PointingHandCursor);
    zoomOutButton->setFixedSize(14, 18);
    hLayout->addWidget(zoomOutButton);
    auto *zoomInButton = new QToolButton(header);
    zoomInButton->setAutoRaise(true);
    zoomInButton->setText(QStringLiteral("+"));
    zoomInButton->setCursor(Qt::PointingHandCursor);
    zoomInButton->setFixedSize(14, 18);
    hLayout->addWidget(zoomInButton);
    auto *floatButton = new QToolButton(header);
    floatButton->setAutoRaise(true);
    floatButton->setIcon(loadIconTinted(QStringLiteral("square"), QColor("#bfbfbf"), QSize(12, 12)));
    floatButton->setIconSize(QSize(12, 12));
    floatButton->setCursor(Qt::PointingHandCursor);
    hLayout->addWidget(floatButton);
    auto *closeButton = new QToolButton(header);
    closeButton->setAutoRaise(true);
    closeButton->setIcon(loadIconTinted(QStringLiteral("x"), QColor("#bfbfbf"), QSize(12, 12)));
    closeButton->setIconSize(QSize(12, 12));
    closeButton->setCursor(Qt::PointingHandCursor);
    hLayout->addWidget(closeButton);
    layout->addWidget(header);

    auto *statusLabel = new QLabel(column);
    statusLabel->setText(tr("Starting backend..."));
    statusLabel->setContentsMargins(8, 2, 8, 2);
    layout->addWidget(statusLabel);

    auto *prints = new PrintsWidget(column);
    prints->setMinimumWidth(90);
    prints->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto *dom = new DomWidget(column);
    dom->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    dom->setVolumeHighlightRules(m_volumeRules);
    prints->setRowHeightOnly(dom->rowHeight());

    auto *printsDomSplitter = new QSplitter(Qt::Horizontal, column);
    printsDomSplitter->setObjectName(QStringLiteral("DomPrintsSplitter"));
    printsDomSplitter->setChildrenCollapsible(false);
    printsDomSplitter->setHandleWidth(2);
    printsDomSplitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *printsContainer = new QWidget(printsDomSplitter);
    auto *printsLayout = new QVBoxLayout(printsContainer);
    printsLayout->setContentsMargins(0, 0, 0, 0);
    printsLayout->setSpacing(0);
    printsLayout->addWidget(prints);

    auto *domContainer = new QWidget(printsDomSplitter);
    auto *domLayout = new QVBoxLayout(domContainer);
    domLayout->setContentsMargins(0, 0, 0, 0);
    domLayout->setSpacing(0);
    domLayout->addWidget(dom);

    printsDomSplitter->addWidget(printsContainer);
    printsDomSplitter->addWidget(domContainer);
    printsDomSplitter->setStretchFactor(0, 1);
    printsDomSplitter->setStretchFactor(1, 3);
    printsDomSplitter->setSizes({200, 600});

    auto *contentWidget = new QWidget(column);
    auto *contentRow = new QHBoxLayout(contentWidget);
    contentRow->setContentsMargins(0, 0, 0, 0);
    contentRow->setSpacing(0);
    contentRow->addWidget(printsDomSplitter);
    if (auto *handle = printsDomSplitter->handle(1)) {
        handle->setObjectName(QStringLiteral("DomResizeHandle"));
        handle->setCursor(Qt::SizeHorCursor);
    }

    auto *scroll = new QScrollArea(column);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto *domScrollBar = new DomScrollBar(Qt::Vertical, scroll);
    domScrollBar->setObjectName(QStringLiteral("DomScrollBar"));
    scroll->setVerticalScrollBar(domScrollBar);
    scroll->setWidget(contentWidget);

    layout->addWidget(scroll, 1);

    // Notional presets pinned to viewport (always visible regardless of scroll position).
    for (std::size_t i = 0; i < result.notionalValues.size(); ++i)
    {
        result.notionalValues[i] = m_defaultNotionalPresets[i];
    }
    result.orderNotional = result.notionalValues.at(std::min<std::size_t>(3, result.notionalValues.size() - 1));
    const int presetCount = static_cast<int>(result.notionalValues.size());
    auto *notionalOverlay = new QWidget(scroll->viewport());
    notionalOverlay->setAttribute(Qt::WA_TranslucentBackground, true);
    notionalOverlay->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *notionalLayout = new QVBoxLayout(notionalOverlay);
    notionalLayout->setContentsMargins(1, 0, 1, 6);
    notionalLayout->setSpacing(6);
    notionalLayout->addStretch();

    auto *notionalGroup = new QButtonGroup(notionalOverlay);
    notionalGroup->setExclusive(true);
    for (int i = 0; i < presetCount; ++i)
    {
        const double preset = result.notionalValues[i];
        auto *btn = new QToolButton(notionalOverlay);
        btn->setCheckable(true);
        btn->setText(QString::number(preset, 'g', 6));
        btn->setFixedSize(52, 21);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QStringLiteral(
            "QToolButton {"
            "  border: 1px solid #4a90e2;"
            "  border-radius: 0px;"
            "  padding: 4px 6px;"
            "  background: rgba(0,0,0,0.18);"
            "  color: #cfd8dc;"
            "}"
            "QToolButton:checked {"
            "  border-color: #4aa3ff;"
            "  background: rgba(74,163,255,0.28);"
            "  color: #e3f2fd;"
            "}"));
        notionalGroup->addButton(btn, i);
        if (preset == 10.0)
        {
            btn->setChecked(true);
            result.orderNotional = preset;
        }
        connect(btn, &QToolButton::clicked, this, [this, column, i]() {
            WorkspaceTab *tab = nullptr;
            DomColumn *col = nullptr;
            int idx = -1;
            if (locateColumn(column, tab, col, idx) && col)
            {
                if (i >= 0 && i < static_cast<int>(col->notionalValues.size()))
                {
                    col->orderNotional = col->notionalValues[i];
                }
            }
        });
        btn->setProperty("domContainerPtr", QVariant::fromValue<void *>(column));
        btn->setProperty("notionalIndex", i);
        btn->installEventFilter(this);
        notionalLayout->addWidget(btn);
    }
    notionalOverlay->adjustSize();

    auto repositionOverlay = [scroll, notionalOverlay]() {
        if (!notionalOverlay || !scroll) return;
        notionalOverlay->adjustSize();
        const int x = 2;
        const int bottomMargin = 24;
        int y = scroll->viewport()->height() - notionalOverlay->height() - bottomMargin;
        if (y < 8) y = 8;
        const int maxY = std::max(0, scroll->viewport()->height() - notionalOverlay->height() - 6);
        if (y > maxY) y = maxY;
        notionalOverlay->move(x, y);
        notionalOverlay->raise();
        notionalOverlay->show();
    };
    scroll->viewport()->installEventFilter(this);
    scroll->viewport()->setProperty("notionalOverlayPtr",
                                    QVariant::fromValue<quintptr>(reinterpret_cast<quintptr>(notionalOverlay)));
    repositionOverlay();
    QTimer::singleShot(0, this, repositionOverlay);

    // Position overlay over prints (fixed relative to column)
    repositionOverlay();
    QTimer::singleShot(0, this, repositionOverlay);

    auto *resizeStub = new QWidget(columnSplitter);
    resizeStub->setMinimumWidth(0);
    resizeStub->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    resizeStub->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    columnSplitter->addWidget(resizeStub);
    columnSplitter->setStretchFactor(columnSplitter->indexOf(columnContent), 1);
    columnSplitter->setStretchFactor(columnSplitter->indexOf(resizeStub), 0);
    columnSplitter->setSizes({columnContent->sizeHint().width(), 0});

    if (auto *handle = columnSplitter->handle(1)) {
        handle->setObjectName(QStringLiteral("DomColumnResizeHandle"));
        handle->setProperty("domContainerPtr", QVariant::fromValue<void *>(column));
        handle->setCursor(Qt::SizeHorCursor);
        handle->installEventFilter(this);
    }

    const QString symbolUpper = result.symbol;
    const auto source = symbolSourceForAccount(result.accountName.isEmpty() ? QStringLiteral("MEXC Spot")
                                                                            : result.accountName);
    const QString exchangeArg = (source == SymbolSource::UzxSwap)
                                    ? QStringLiteral("uzxswap")
                                    : (source == SymbolSource::UzxSpot) ? QStringLiteral("uzxspot")
                                                                        : QStringLiteral("mexc");
    auto *client = new LadderClient(m_backendPath, symbolUpper, m_levels, exchangeArg, dom, column, prints);
    client->setCompression(result.tickCompression);

    connect(client,
            &LadderClient::statusMessage,
            this,
            &MainWindow::handleLadderStatusMessage);
    connect(client, &LadderClient::pingUpdated, this, &MainWindow::handleLadderPingUpdated);

    connect(dom,
            &DomWidget::rowClicked,
            this,
            &MainWindow::handleDomRowClicked);
    connect(dom, &DomWidget::hoverInfoChanged, prints, &PrintsWidget::setHoverInfo);

    connect(client, &LadderClient::statusMessage, statusLabel, &QLabel::setText);

    // ????????? ??? ???????: ??????????? ???????? ??????, ???? ??? ????????? ?????? trades
    connect(zoomOutButton, &QToolButton::clicked, this, [dom, prints]() {
        dom->setRowHeight(dom->rowHeight() - 2);
        prints->setRowHeightOnly(dom->rowHeight());
    });
    connect(zoomInButton, &QToolButton::clicked, this, [dom, prints]() {
        dom->setRowHeight(dom->rowHeight() + 2);
        prints->setRowHeightOnly(dom->rowHeight());
    });
    connect(levelsSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this, client, &column = tab.columnsData.back()](int value) {
                m_levels = value;
                if (client) {
                    const auto src = symbolSourceForAccount(column.accountName);
                    const QString exch = (src == SymbolSource::UzxSwap)
                                             ? QStringLiteral("uzxswap")
                                             : (src == SymbolSource::UzxSpot) ? QStringLiteral("uzxspot")
                                                                              : QStringLiteral("mexc");
                    client->restart(column.symbol, value, exch);
                }
            });
    connect(compressionButton, &QToolButton::clicked, this, [this, column, compressionButton]() {
        WorkspaceTab *tab = nullptr;
        DomColumn *col = nullptr;
        int idx = -1;
        if (!locateColumn(column, tab, col, idx) || !col) {
            return;
        }
        bool ok = false;
        const int current = std::max(1, col->tickCompression);
        const int factor = QInputDialog::getInt(this,
                                                tr("Compression"),
                                                tr("Ticks per row:"),
                                                current,
                                                1,
                                                10000,
                                                1,
                                                &ok);
        if (!ok) {
            return;
        }
        const int clamped = std::clamp(factor, 1, 10000);
        col->tickCompression = clamped;
        compressionButton->setText(QStringLiteral("%1x").arg(clamped));
        if (col->client) {
            col->client->setCompression(clamped);
        }
    });
    connect(floatButton, &QToolButton::clicked, this, [this, column]() {
        toggleDomColumnFloating(column);
    });
    connect(closeButton, &QToolButton::clicked, this, [this, column]() {
        removeDomColumn(column);
    });
    connect(tickerLabel, &QLabel::linkActivated, this, [this, &result](const QString &) {
        retargetDomColumn(result, QString());
    });
    header->installEventFilter(this);

    result.container = column;
    result.dom = dom;
    result.prints = prints;
    result.scrollArea = scroll;
    result.scrollBar = domScrollBar;
    result.client = client;
    result.levelsSpin = levelsSpin;
    result.tickCompression = 1;
    result.compressionButton = compressionButton;
    result.accountName = QStringLiteral("MEXC Spot");
    result.accountColor = accountColorFor(result.accountName);
    result.notionalOverlay = notionalOverlay;
    result.notionalGroup = notionalGroup;
    result.localOrders.clear();
    result.dom->setLocalOrders(result.localOrders);
    if (result.prints) {
        QVector<LocalOrderMarker> empty;
        result.prints->setLocalOrders(empty);
    }
    return result;
}

MainWindow::WorkspaceTab *MainWindow::currentWorkspaceTab()
{
    if (!m_workspaceTabs) {
        return nullptr;
    }
    const int index = m_workspaceTabs->currentIndex();
    if (index < 0) {
        return nullptr;
    }

    const QVariant data = m_workspaceTabs->tabData(index);
    const int id = data.isValid() ? data.toInt() : 0;
    for (auto &tab : m_tabs) {
        if (tab.id == id) {
            return &tab;
        }
    }
    return nullptr;
}

int MainWindow::findTabIndexById(int id) const
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].id == id) {
            return i;
        }
    }
    return -1;
}

void MainWindow::handleTabChanged(int index)
{
    if (!m_workspaceTabs || !m_workspaceStack) {
        return;
    }
    if (index < 0) {
        return;
    }
    const QVariant data = m_workspaceTabs->tabData(index);
    const int id = data.isValid() ? data.toInt() : 0;
    const int stackIndex = findTabIndexById(id);
    if (stackIndex >= 0) {
        m_workspaceStack->setCurrentIndex(stackIndex);
    }
    if (!m_tabUnderlineHiddenForDrag) {
        updateTabUnderline(index);
    }
}

void MainWindow::handleTabCloseRequested(int index)
{
    if (!m_workspaceTabs || !m_workspaceStack) {
        return;
    }
    if (m_workspaceTabs->count() <= 1 || index < 0) {
        return;
    }

    const QVariant data = m_workspaceTabs->tabData(index);
    const int id = data.isValid() ? data.toInt() : 0;
    const int tabIdx = findTabIndexById(id);
    if (tabIdx < 0) {
        return;
    }

    WorkspaceTab tab = m_tabs.takeAt(tabIdx);
    m_recycledTabIds.push_back(tab.id);

    QWidget *wsWidget = tab.workspace;
    const int stackIndex = m_workspaceStack->indexOf(wsWidget);
    if (stackIndex >= 0) {
        QWidget *widget = m_workspaceStack->widget(stackIndex);
        m_workspaceStack->removeWidget(widget);
        widget->deleteLater();
    }

    m_workspaceTabs->removeTab(index);
    QTimer::singleShot(0, this, [this]() { refreshTabCloseButtons(); });

    if (m_workspaceTabs->count() > 0) {
        const int newIndex = std::min(index, m_workspaceTabs->count() - 1);
        m_workspaceTabs->setCurrentIndex(newIndex);
    }
}

void MainWindow::handleNewTabRequested()
{
    setLastAddAction(AddAction::WorkspaceTab);
    createWorkspaceTab(QVector<SavedColumn>());
}

void MainWindow::handleNewLadderRequested()
{
    WorkspaceTab *tab = currentWorkspaceTab();
    if (!tab || !tab->columns) {
        return;
    }

    const int tabId = tab->id;
    SymbolPickerDialog *picker =
        createSymbolPicker(tr("Add ladder"),
                           !m_symbols.isEmpty() ? m_symbols.first() : QString(),
                           QStringLiteral("MEXC Spot"));
    if (!picker) {
        return;
    }
    connect(picker, &QDialog::accepted, this, [this, picker, tabId]() {
        const QString symbol = picker->selectedSymbol().trimmed().toUpper();
        if (symbol.isEmpty()) {
            picker->deleteLater();
            return;
        }
        const QString account = picker->selectedAccount().trimmed().isEmpty()
                                    ? QStringLiteral("MEXC Spot")
                                    : picker->selectedAccount();

        const int idx = findTabIndexById(tabId);
        if (idx < 0 || idx >= m_tabs.size()) {
            picker->deleteLater();
            return;
        }
        setLastAddAction(AddAction::LadderColumn);
        WorkspaceTab &targetTab = m_tabs[idx];
        DomColumn col = createDomColumn(symbol, account, targetTab);
        applySymbolToColumn(col, symbol, account);
        targetTab.columnsData.push_back(col);
        if (targetTab.columns) {
            const int spacerIndex =
                (targetTab.columnsSpacer ? targetTab.columns->indexOf(targetTab.columnsSpacer) : -1);
            const int insertIndex = spacerIndex >= 0 ? spacerIndex : targetTab.columns->count();
            targetTab.columns->insertWidget(insertIndex, col.container);
            targetTab.columns->setStretchFactor(targetTab.columns->indexOf(col.container), 0);
        }
        saveUserSettings();
        picker->deleteLater();
    });
    connect(picker, &QDialog::rejected, picker, &QObject::deleteLater);
    picker->open();
    picker->raise();
    picker->activateWindow();
}

void MainWindow::handleLadderStatusMessage(const QString &msg)
{
    if (msg.isEmpty()) {
        return;
    }
    const QString lower = msg.toLower();
    // Ignore noisy heartbeat-style messages
    if (lower.contains(QStringLiteral("ping")) || lower.contains(QStringLiteral("receiving data"))) {
        return;
    }

    static const QStringList keywords = {
        QStringLiteral("error"),
        QStringLiteral("fail"),
        QStringLiteral("reject"),
        QStringLiteral("denied"),
        QStringLiteral("invalid"),
        QStringLiteral("timeout"),
        QStringLiteral("disconnect"),
        QStringLiteral("order")
    };
    bool important = false;
    for (const auto &k : keywords) {
        if (lower.contains(k)) {
            important = true;
            break;
        }
    }
    if (!important) {
        return;
    }

    addNotification(msg);
}

void MainWindow::handleLadderPingUpdated(int ms)
{
    Q_UNUSED(ms);
}

void MainWindow::handleDomRowClicked(Qt::MouseButton button,
                                     int row,
                                     double price,
                                     double bidQty,
                                     double askQty)
{
    Q_UNUSED(row);
    Q_UNUSED(bidQty);
    Q_UNUSED(askQty);
    if (!m_tradeManager || (button != Qt::LeftButton && button != Qt::RightButton)) {
        return;
    }
    auto *dom = qobject_cast<DomWidget *>(sender());
    if (!dom) {
        return;
    }
    DomColumn *column = nullptr;
    for (auto &tab : m_tabs) {
        for (auto &col : tab.columnsData) {
            if (col.dom == dom) {
                column = &col;
                break;
            }
        }
        if (column) {
            break;
        }
    }
    if (!column) {
        return;
    }
    const double notional = column->orderNotional > 0.0 ? column->orderNotional : 0.0;
    if (price <= 0.0 || notional <= 0.0) {
        statusBar()->showMessage(tr("Set a positive order size before trading"), 2000);
        return;
    }
    const double quantity = notional / price;
    if (quantity <= 0.0) {
        statusBar()->showMessage(tr("Calculated order quantity is zero"), 2000);
        return;
    }
    const OrderSide side = (button == Qt::LeftButton) ? OrderSide::Buy : OrderSide::Sell;
    m_tradeManager->placeLimitOrder(column->symbol, column->accountName, price, quantity, side);
    statusBar()->showMessage(
        tr("Submitting %1 %2 @ %3")
            .arg(side == OrderSide::Buy ? QStringLiteral("BUY") : QStringLiteral("SELL"))
            .arg(QString::number(quantity, 'f', 4))
            .arg(QString::number(price, 'f', 5)),
        2000);
}

void MainWindow::handlePositionChanged(const QString &accountName,
                                       const QString &symbol,
                                       const TradePosition &position)
{
    const QString symUpper = symbol.toUpper();
    const QString accountLower = accountName.trimmed().toLower();
    for (auto &tab : m_tabs) {
        for (auto &col : tab.columnsData) {
            if (col.symbol.compare(symUpper, Qt::CaseInsensitive) != 0) {
                continue;
            }
            if (!accountLower.isEmpty()
                && col.accountName.trimmed().toLower() != accountLower) {
                continue;
            }
            if (col.dom) {
                col.dom->setTradePosition(position);
            }
        }
    }
}

void MainWindow::retargetDomColumn(DomColumn &col, const QString &symbol)
{
    QString sym = symbol.trimmed().toUpper();
    if (sym.isEmpty()) {
        // Non-blocking picker to avoid freezing the main window while selecting.
        SymbolPickerDialog *picker = createSymbolPicker(tr("Select symbol"),
                                                        col.symbol,
                                                        col.accountName.isEmpty() ? QStringLiteral("MEXC Spot")
                                                                                  : col.accountName);
        if (!picker) {
            return;
        }
        QPointer<QWidget> container = col.container;
        connect(picker, &QDialog::accepted, this, [this, picker, container]() {
            if (!container) {
                picker->deleteLater();
                return;
            }
            WorkspaceTab *tab = nullptr;
            DomColumn *colPtr = nullptr;
            int splitIndex = -1;
            if (!locateColumn(container, tab, colPtr, splitIndex) || !colPtr) {
                picker->deleteLater();
                return;
            }
            applySymbolToColumn(*colPtr, picker->selectedSymbol(), picker->selectedAccount());
            picker->deleteLater();
            saveUserSettings();
        });
        connect(picker, &QDialog::rejected, picker, &QObject::deleteLater);
        picker->open();
        picker->raise();
        picker->activateWindow();
        return;
    }

    if (sym == col.symbol) {
        return;
    }

    applySymbolToColumn(col, sym, col.accountName);
    saveUserSettings();
}

void MainWindow::applyNotionalPreset(int presetIndex)
{
    if (presetIndex < 0) {
        return;
    }
    DomColumn *column = focusedDomColumn();
    if (!column) {
        if (auto *tab = currentWorkspaceTab()) {
            if (!tab->columnsData.isEmpty()) {
                column = &tab->columnsData.front();
            }
        }
    }
    if (!column) {
        return;
    }
    if (presetIndex >= static_cast<int>(column->notionalValues.size())) {
        return;
    }

    column->orderNotional = column->notionalValues[presetIndex];
    if (column->notionalGroup) {
        if (auto *btn = column->notionalGroup->button(presetIndex)) {
            btn->setChecked(true);
        }
    }

    statusBar()->showMessage(
        tr("Size preset set to %1").arg(column->orderNotional, 0, 'g', 6),
        800);
}

void MainWindow::startNotionalEdit(QWidget *columnContainer, int presetIndex)
{
    if (!columnContainer) {
        return;
    }
    WorkspaceTab *tab = nullptr;
    DomColumn *col = nullptr;
    int splitIndex = -1;
    if (!locateColumn(columnContainer, tab, col, splitIndex) || !col) {
        return;
    }
    if (presetIndex < 0 || presetIndex >= static_cast<int>(col->notionalValues.size())) {
        return;
    }

    if (!col->notionalEditOverlay) {
        auto *overlay = new QWidget(columnContainer);
        overlay->setObjectName(QStringLiteral("NotionalEditOverlay"));
        overlay->setStyleSheet(QStringLiteral("background-color: rgba(0,0,0,0.65);"));
        overlay->hide();
        overlay->setProperty("domContainerPtr",
                             QVariant::fromValue<void *>(columnContainer));

        auto *overlayLayout = new QVBoxLayout(overlay);
        overlayLayout->setContentsMargins(0, 0, 0, 0);
        overlayLayout->setSpacing(0);
        overlayLayout->addStretch();

        auto *panel = new QWidget(overlay);
        panel->setStyleSheet(QStringLiteral(
            "background-color:#1f1f1f; border:1px solid #4a90e2; border-radius:6px;"));
        panel->setFixedWidth(220);
        auto *panelLayout = new QVBoxLayout(panel);
        panelLayout->setContentsMargins(14, 14, 14, 14);
        panelLayout->setSpacing(8);

        auto *label = new QLabel(tr("Edit preset (USDT)"), panel);
        label->setAlignment(Qt::AlignCenter);
        panelLayout->addWidget(label);

        auto *line = new QLineEdit(panel);
        line->setAlignment(Qt::AlignCenter);
        line->setObjectName(QStringLiteral("NotionalEditField"));
        line->setProperty("domContainerPtr",
                          QVariant::fromValue<void *>(columnContainer));
        panelLayout->addWidget(line);

        auto *hint = new QLabel(tr("Enter a value and press Enter"), panel);
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet(QStringLiteral("color:#bbbbbb; font-size:11px;"));
        panelLayout->addWidget(hint);

        overlayLayout->addWidget(panel, 0, Qt::AlignHCenter | Qt::AlignBottom);
        overlayLayout->addSpacing(30);

        col->notionalEditOverlay = overlay;
        col->notionalEditField = line;
        overlay->installEventFilter(this);
        line->installEventFilter(this);
        connect(line, &QLineEdit::returnPressed, this, [this, columnContainer]() {
            commitNotionalEdit(columnContainer, true);
        });
    }

    col->editingNotionalIndex = presetIndex;
    if (col->notionalEditOverlay) {
        col->notionalEditOverlay->setGeometry(columnContainer->rect());
        col->notionalEditOverlay->show();
        col->notionalEditOverlay->raise();
    }
    if (col->notionalEditField) {
        col->notionalEditField->setText(
            QString::number(col->notionalValues[presetIndex], 'g', 8));
        col->notionalEditField->selectAll();
        col->notionalEditField->setFocus(Qt::OtherFocusReason);
    }
    m_notionalEditActive = true;
}

void MainWindow::commitNotionalEdit(QWidget *columnContainer, bool apply)
{
    if (!columnContainer) {
        return;
    }
    WorkspaceTab *tab = nullptr;
    DomColumn *col = nullptr;
    int splitIndex = -1;
    if (!locateColumn(columnContainer, tab, col, splitIndex) || !col) {
        return;
    }
    if (!col->notionalEditOverlay) {
        return;
    }

    const int index = col->editingNotionalIndex;
    if (apply && col->notionalEditField && index >= 0 &&
        index < static_cast<int>(col->notionalValues.size())) {
        bool ok = false;
        const double value = col->notionalEditField->text().toDouble(&ok);
        if (ok && value > 0.0) {
            col->notionalValues[index] = value;
            if (col->notionalGroup) {
                if (auto *btn = col->notionalGroup->button(index)) {
                    btn->setText(QString::number(value, 'g', 6));
                    if (btn->isChecked()) {
                        col->orderNotional = value;
                    }
                }
            }
        }
    }

    col->editingNotionalIndex = -1;
    col->notionalEditOverlay->hide();
    m_notionalEditActive = false;
}
void MainWindow::updateTimeLabel()
{
    if (!m_timeLabel) {
        return;
    }
    QDateTime now = QDateTime::currentDateTimeUtc();
    if (m_timeOffsetMinutes != 0) {
        now = now.addSecs(m_timeOffsetMinutes * 60);
    }

    const int hoursOffset = m_timeOffsetMinutes / 60;
    QString suffix;
    if (hoursOffset == 0) {
        suffix = QStringLiteral("UTC");
    } else {
        suffix = QStringLiteral("UTC%1%2")
                     .arg(hoursOffset > 0 ? QStringLiteral("+") : QString())
                     .arg(hoursOffset);
    }

    m_timeLabel->setText(
        now.toString(QStringLiteral("HH:mm:ss '") + suffix + QLatin1Char('\'')));
}

void MainWindow::openConnectionsWindow()
{
    if (!m_connectionStore || !m_tradeManager) {
        return;
    }
    if (!m_connectionsWindow) {
        m_connectionsWindow = new ConnectionsWindow(m_connectionStore, m_tradeManager, this);
    } else {
        m_connectionsWindow->refreshUi();
    }
    m_connectionsWindow->show();
    m_connectionsWindow->raise();
    m_connectionsWindow->activateWindow();
}

void MainWindow::handleConnectionStateChanged(ConnectionStore::Profile profile,
                                              TradeManager::ConnectionState state,
                                              const QString &message)
{
    auto profileLabel = [profile]() {
        switch (profile) {
        case ConnectionStore::Profile::MexcFutures:
            return QStringLiteral("MEXC Futures");
        case ConnectionStore::Profile::UzxSwap:
            return QStringLiteral("UZX Swap");
        case ConnectionStore::Profile::UzxSpot:
            return QStringLiteral("UZX Spot");
        case ConnectionStore::Profile::MexcSpot:
        default:
            return QStringLiteral("MEXC Spot");
        }
    };
    const auto overallState =
        m_tradeManager ? m_tradeManager->overallState() : state;
    if (m_connectionIndicator) {
        QString text;
        QColor color;
        switch (overallState) {
        case TradeManager::ConnectionState::Connected:
            text = tr("Connected");
            color = QColor("#2e7d32");
            break;
        case TradeManager::ConnectionState::Connecting:
            text = tr("Connecting...");
            color = QColor("#f9a825");
            break;
        case TradeManager::ConnectionState::Error:
            text = tr("Error");
            color = QColor("#c62828");
            break;
        case TradeManager::ConnectionState::Disconnected:
        default:
            text = tr("Disconnected");
            color = QColor("#616161");
            break;
        }
        m_connectionIndicator->setVisible(true);
        m_connectionIndicator->setText(text);
        const QString style = QStringLiteral(
            "QLabel#ConnectionIndicator {"
            "  border-radius: 11px;"
            "  padding: 2px 12px;"
            "  color: #ffffff;"
            "  font-weight: 500;"
            "  background-color: %1;"
            "}").arg(color.name());
        m_connectionIndicator->setStyleSheet(style);
    }
    if (!message.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("%1: %2").arg(profileLabel(), message), 2500);
    }

    if (state == TradeManager::ConnectionState::Error
        || state == TradeManager::ConnectionState::Disconnected) {
        const QString note = message.isEmpty()
                                 ? tr("%1 connection lost").arg(profileLabel())
                                 : QStringLiteral("%1: %2").arg(profileLabel(), message);
        addNotification(note);
    }
}

void MainWindow::addNotification(const QString &text, bool unread)
{
    static QString lastText;
    static QDateTime lastTime;
    const QDateTime now = QDateTime::currentDateTime();
    if (text == lastText && lastTime.isValid() && lastTime.msecsTo(now) < 3000) {
        return;
    }
    lastText = text;
    lastTime = now;

    NotificationEntry entry;
    entry.text = text;
    entry.timestamp = now;
    entry.read = !unread;
    const int maxStored = 99;
    while (m_notifications.size() >= maxStored) {
        if (!m_notifications.front().read && m_unreadNotifications > 0) {
            --m_unreadNotifications;
        }
        m_notifications.pop_front();
    }
    m_notifications.push_back(entry);
    if (unread) {
        ++m_unreadNotifications;
    }
    refreshAlertsList();
    updateAlertsBadge();
#if HAS_QMEDIAPLAYER && HAS_QAUDIOOUTPUT
    if (unread && m_notificationPlayer && m_notificationOutput) {
        m_notificationPlayer->stop();
        m_notificationPlayer->setPosition(0);
        m_notificationPlayer->play();
    }
#endif
}

void MainWindow::updateAlertsBadge()
{
    if (!m_alertsBadge || !m_alertsButton) {
        return;
    }

    if (m_unreadNotifications <= 0) {
        m_alertsBadge->hide();
        return;
    }

    const int capped = std::min(m_unreadNotifications, 99);
    const QString text = QString::number(capped);
    m_alertsBadge->setText(text);
    const int badgeWidth = std::max(16, m_alertsBadge->fontMetrics().horizontalAdvance(text) + 6);
    m_alertsBadge->setFixedWidth(badgeWidth);
    const int x = m_alertsButton->width() - badgeWidth - 4;
    m_alertsBadge->move(std::max(0, x), 2);
    m_alertsBadge->show();
}

void MainWindow::refreshAlertsList()
{
    if (!m_alertsList) {
        return;
    }

    m_alertsList->clear();
    // Newest first
    for (auto it = m_notifications.crbegin(); it != m_notifications.crend(); ++it) {
        const auto &n = *it;
        const QString text = QStringLiteral("%1\n%2")
                                 .arg(n.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                                      n.text);
        auto *item = new QListWidgetItem(text);
        QFont f = item->font();
        f.setBold(!n.read);
        item->setFont(f);
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        // Allow multiline and prevent eliding: size the row to fit text within current viewport width.
        const int viewWidth = m_alertsList->viewport() ? m_alertsList->viewport()->width() : 300;
        const int usableWidth = std::max(80, viewWidth - 12);
        QFontMetrics fm(f);
        const QRect br = fm.boundingRect(QRect(0, 0, usableWidth, 0),
                                         Qt::TextWordWrap,
                                         text);
        item->setSizeHint(QSize(usableWidth, br.height() + 6));
        m_alertsList->addItem(item);
    }
}

void MainWindow::markAllNotificationsRead()
{
    bool changed = false;
    for (auto &n : m_notifications) {
        if (!n.read) {
            n.read = true;
            changed = true;
        }
    }
    if (changed) {
        m_unreadNotifications = 0;
        refreshAlertsList();
        updateAlertsBadge();
    } else {
        m_unreadNotifications = 0;
        updateAlertsBadge();
    }
}

void MainWindow::repositionAlertsPanel()
{
    if (!m_alertsPanel || !m_workspaceStack) {
        return;
    }

    const int margin = 12;
    const int panelWidth = 320;
    const int minHeight = 160;
    const int maxHeight = 340;
    const int availableHeight = std::max(minHeight, m_workspaceStack->height() - margin * 2);
    const int panelHeight = std::min(availableHeight, maxHeight);
    const int x = std::max(margin, m_workspaceStack->width() - panelWidth - margin);
    const int y = margin;

    m_alertsPanel->setGeometry(x, y, panelWidth, panelHeight);
    m_alertsPanel->raise();
}

QIcon MainWindow::loadIcon(const QString &name) const
{
    // ???? ?????? "<name>.svg" ? ?????????? ??????? ?????? (appDir, img/, img/icons/, img/icons/outline/).
    const QString relFile = QStringLiteral("%1.svg").arg(name);
    const QString path = resolveAssetPath(relFile);
    if (!path.isEmpty()) {
        return QIcon(path);
    }
    return QIcon();
}

QIcon MainWindow::loadIconTinted(const QString &name, const QColor &color, const QSize &size) const
{
    const QIcon base = loadIcon(name);
    if (base.isNull()) {
        return base;
    }

    const QSize iconSize = size.isValid() ? size : QSize(16, 16);
    QPixmap src = base.pixmap(iconSize);
    if (src.isNull()) {
        return base;
    }

    QPixmap tinted(src.size());
    tinted.fill(Qt::transparent);

    QPainter p(&tinted);
    p.drawPixmap(0, 0, src);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(tinted.rect(), color);
    p.end();

    QIcon result;
    result.addPixmap(tinted);
    return result;
}

static QIcon mirrorIconHorizontally(const QIcon &icon, const QSize &size)
{
    if (icon.isNull()) {
        return icon;
    }
    QPixmap pix = icon.pixmap(size);
    if (pix.isNull()) {
        return icon;
    }
    QTransform t;
    t.scale(-1.0, 1.0);
    QPixmap mirrored = pix.transformed(t, Qt::SmoothTransformation);
    QIcon mirroredIcon;
    mirroredIcon.addPixmap(mirrored);
    return mirroredIcon;
}

QString MainWindow::resolveAssetPath(const QString &relative) const
{
    const QString rel = QDir::fromNativeSeparators(relative);
    const QString appDir = QCoreApplication::applicationDirPath();

    // ???? ?????? ? ?????????? ?????????? ? ?? ??????????????
    const QStringList bases = {
        appDir,
        QDir(appDir).filePath(QStringLiteral("img")),
        QDir(appDir).filePath(QStringLiteral("img/icons")),
        QDir(appDir).filePath(QStringLiteral("img/icons/outline")),
        QDir(appDir).filePath(QStringLiteral("img/outline")),
        QDir(appDir).filePath(QStringLiteral("../img")),
        QDir(appDir).filePath(QStringLiteral("../img/icons")),
        QDir(appDir).filePath(QStringLiteral("../img/icons/outline")),
        QDir(appDir).filePath(QStringLiteral("../img/outline")),
        QDir(appDir).filePath(QStringLiteral("../../img")),
        QDir(appDir).filePath(QStringLiteral("../../img/icons")),
        QDir(appDir).filePath(QStringLiteral("../../img/icons/outline")),
        QDir(appDir).filePath(QStringLiteral("../../img/outline"))
    };

    for (const QString &base : bases) {
        const QString candidate = QDir(base).filePath(rel);
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }

    return QString();
}

bool MainWindow::locateColumn(QWidget *container, WorkspaceTab *&tabOut, DomColumn *&colOut, int &splitIndex)
{
    tabOut = nullptr;
    colOut = nullptr;
    splitIndex = -1;
    for (auto &tab : m_tabs) {
        if (!tab.columns) continue;
        for (auto &col : tab.columnsData) {
            if (col.container == container) {
                tabOut = &tab;
                colOut = &col;
                splitIndex = tab.columns->indexOf(container);
                return true;
            }
        }
    }
    return false;
}

void MainWindow::updateDomColumnResize(int delta)
{
    if (!m_domResizeActive || !m_domResizeTab || !m_domResizeTab->columns || m_domResizeSplitterIndex < 0) {
        return;
    }
    const QList<int> &baseSizes = m_domResizeInitialSizes;
    if (m_domResizeSplitterIndex >= baseSizes.size()) {
        return;
    }

    const int columnIndex = m_domResizeSplitterIndex;
    const int neighborIndex = m_domResizeFromLeftEdge ? columnIndex - 1 : columnIndex + 1;
    if (neighborIndex < 0 || neighborIndex >= baseSizes.size()) {
        return;
    }

    auto *splitter = m_domResizeTab->columns;
    const bool neighborIsSpacer =
        (m_domResizeTab->columnsSpacer && splitter->widget(neighborIndex) == m_domResizeTab->columnsSpacer);

    const int columnBase = baseSizes.value(columnIndex);
    const int neighborBase = baseSizes.value(neighborIndex);
    const int columnMin = kDomColumnMinWidth;
    const int neighborMin = neighborIsSpacer ? 0 : kDomColumnMinWidth;

    int clampedDelta = delta;
    if (m_domResizeFromLeftEdge) {
        const int minDelta = neighborMin - neighborBase;
        const int maxDelta = columnBase - columnMin;
        clampedDelta = std::clamp(clampedDelta, minDelta, maxDelta);
    } else {
        const int minDelta = columnMin - columnBase;
        const int maxDelta = neighborBase - neighborMin;
        clampedDelta = std::clamp(clampedDelta, minDelta, maxDelta);
    }

    int newColumnSize = columnBase;
    int newNeighborSize = neighborBase;
    if (m_domResizeFromLeftEdge) {
        newNeighborSize = neighborBase + clampedDelta;
        newColumnSize = columnBase - clampedDelta;
    } else {
        newColumnSize = columnBase + clampedDelta;
        newNeighborSize = neighborBase - clampedDelta;
    }

    QList<int> newSizes = baseSizes;
    newSizes[columnIndex] = std::max(columnMin, newColumnSize);
    newSizes[neighborIndex] = std::max(neighborMin, newNeighborSize);
    splitter->setSizes(newSizes);
}

void MainWindow::endDomColumnResize()
{
    m_domResizeActive = false;
    m_domResizePending = false;
    m_domResizeContainer = nullptr;
    m_domResizeTab = nullptr;
    m_domResizeInitialSizes.clear();
    m_domResizeSplitterIndex = -1;
    m_domResizeFromLeftEdge = false;
    releaseDomResizeMouseGrab();
}

void MainWindow::cancelPendingDomResize()
{
    m_domResizePending = false;
    m_domResizeContainer = nullptr;
    m_domResizeTab = nullptr;
    m_domResizeInitialSizes.clear();
    m_domResizeSplitterIndex = -1;
    m_domResizeFromLeftEdge = false;
    releaseDomResizeMouseGrab();
}

void MainWindow::releaseDomResizeMouseGrab()
{
    if (m_domResizeHandle) {
        m_domResizeHandle->releaseMouse();
        m_domResizeHandle = nullptr;
    }
}

void MainWindow::removeDomColumn(QWidget *container)
{
    WorkspaceTab *tab = nullptr;
    DomColumn *col = nullptr;
    int splitIndex = -1;
    if (!locateColumn(container, tab, col, splitIndex) || !tab || !col) {
        return;
    }
    if (col->floatingWindow) {
        col->floatingWindow->removeEventFilter(this);
        col->floatingWindow->close();
        col->floatingWindow = nullptr;
        col->isFloating = false;
    }
    if (tab->columns && col->container) {
        if (m_domResizeContainer == col->container) {
            endDomColumnResize();
        } else if (m_domResizePending && m_domResizeContainer == col->container) {
            cancelPendingDomResize();
        }
        col->container->setParent(nullptr);
    }
    if (col->client) {
        col->client->deleteLater();
        col->client = nullptr;
    }
    if (col->dom) {
        col->dom->deleteLater();
        col->dom = nullptr;
    }
    if (col->container) {
        col->container->deleteLater();
        col->container = nullptr;
    }
    for (int i = 0; i < tab->columnsData.size(); ++i) {
        if (&tab->columnsData[i] == col) {
            tab->columnsData.removeAt(i);
            break;
        }
    }
}

void MainWindow::toggleDomColumnFloating(QWidget *container, const QPoint &globalPos)
{
    WorkspaceTab *tab = nullptr;
    DomColumn *col = nullptr;
    int splitIndex = -1;
    if (!locateColumn(container, tab, col, splitIndex) || !tab || !col) {
        return;
    }
    if (col->isFloating) {
        dockDomColumn(*tab, *col, splitIndex);
    } else {
        floatDomColumn(*tab, *col, splitIndex, globalPos);
    }
}

void MainWindow::floatDomColumn(WorkspaceTab &tab, DomColumn &col, int indexInSplitter, const QPoint &globalPos)
{
    if (!tab.columns || col.isFloating || !col.container) {
        return;
    }
    col.lastSplitterIndex = indexInSplitter >= 0 ? indexInSplitter : tab.columns->indexOf(col.container);
    col.lastSplitterSizes = tab.columns->sizes();
    if (m_domResizeContainer == col.container) {
        endDomColumnResize();
    } else if (m_domResizePending && m_domResizeContainer == col.container) {
        cancelPendingDomResize();
    }
    col.container->setParent(nullptr);

    QWidget *win = new QWidget(nullptr, Qt::Window);
    win->setAttribute(Qt::WA_DeleteOnClose, false);
    win->setWindowTitle(col.symbol.isEmpty() ? tr("DOM") : col.symbol);
    auto *layout = new QVBoxLayout(win);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(col.container);
    col.floatingWindow = win;
    col.isFloating = true;
    win->installEventFilter(this);
    win->resize(col.container->size());
    if (globalPos.isNull()) {
        win->move(mapToGlobal(QPoint(40, 60)));
    } else {
        win->move(globalPos - m_domDragStartWindowOffset);
    }
    win->show();
}

void MainWindow::dockDomColumn(WorkspaceTab &tab, DomColumn &col, int preferredIndex)
{
    if (!tab.columns || !col.container) {
        return;
    }
    if (col.floatingWindow) {
        col.floatingWindow->removeEventFilter(this);
        col.floatingWindow->hide();
        col.floatingWindow = nullptr;
    }

    const int spacerIndex = tab.columnsSpacer ? tab.columns->indexOf(tab.columnsSpacer) : -1;
    const int maxInsertIndex = (spacerIndex >= 0) ? spacerIndex : tab.columns->count();
    int insertIndex = preferredIndex >= 0 ? preferredIndex : col.lastSplitterIndex;
    if (insertIndex < 0) {
        insertIndex = maxInsertIndex;
    }
    insertIndex = std::min(insertIndex, maxInsertIndex);
    tab.columns->insertWidget(insertIndex, col.container);
    tab.columns->setStretchFactor(tab.columns->indexOf(col.container), 0);
    if (!col.lastSplitterSizes.isEmpty()) {
        tab.columns->setSizes(col.lastSplitterSizes);
    }
    col.isFloating = false;
}

void MainWindow::openPluginsWindow()
{
    if (!m_pluginsWindow) {
        m_pluginsWindow = new PluginsWindow(this);
    }
    m_pluginsWindow->show();
    m_pluginsWindow->raise();
    m_pluginsWindow->activateWindow();
}

void MainWindow::openSettingsWindow()
{
    if (!m_settingsWindow) {
        m_settingsWindow = new SettingsWindow(this);
        m_settingsWindow->setCenterHotkey(m_centerKey, m_centerMods, m_centerAllLadders);
        m_settingsWindow->setVolumeHighlightRules(m_volumeRules);
        m_settingsWindow->setCustomHotkeys(currentCustomHotkeys());
        connect(m_settingsWindow,
                &SettingsWindow::centerHotkeyChanged,
                this,
                [this](int key, Qt::KeyboardModifiers mods, bool allLadders) {
                    m_centerKey = key;
                    m_centerMods = mods;
                    m_centerAllLadders = allLadders;
                    saveUserSettings();
                });
        connect(m_settingsWindow,
                &SettingsWindow::volumeHighlightRulesChanged,
                this,
                [this](const QVector<VolumeHighlightRule> &rules) {
                    m_volumeRules = rules;
                    std::sort(m_volumeRules.begin(),
                              m_volumeRules.end(),
                              [](const VolumeHighlightRule &a, const VolumeHighlightRule &b) {
                                  return a.threshold < b.threshold;
                              });
                    applyVolumeRulesToAllDoms();
                    saveUserSettings();
                });
        connect(m_settingsWindow,
                &SettingsWindow::customHotkeyChanged,
                this,
                [this](const QString &id, int key, Qt::KeyboardModifiers mods) {
                    updateCustomHotkey(id, key, mods);
                });
    } else {
        m_settingsWindow->setCenterHotkey(m_centerKey, m_centerMods, m_centerAllLadders);
        m_settingsWindow->setVolumeHighlightRules(m_volumeRules);
        m_settingsWindow->setCustomHotkeys(currentCustomHotkeys());
    }
    m_settingsWindow->show();
    m_settingsWindow->raise();
    m_settingsWindow->activateWindow();
}

void MainWindow::toggleAlertsPanel()
{
    if (!m_alertsPanel) {
        return;
    }

    const bool show = !m_alertsPanel->isVisible();
    if (show) {
        repositionAlertsPanel();
        m_alertsPanel->show();
        m_alertsPanel->raise();
        markAllNotificationsRead();
    } else {
        m_alertsPanel->hide();
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Resize) {
        if (obj == m_workspaceStack) {
            repositionAlertsPanel();
        }
        if (auto *w = qobject_cast<QWidget *>(obj)) {
            QVariant ov = w->property("notionalOverlayPtr");
            if (ov.isValid()) {
                auto *overlay = reinterpret_cast<QWidget *>(ov.value<quintptr>());
                if (overlay) {
                    const int x = 2;
                    const int bottomMargin = 24;
                    int y = w->height() - overlay->sizeHint().height() - bottomMargin;
                    const int maxY = std::max(0, w->height() - overlay->sizeHint().height() - 6);
                    if (y < 8) y = 8;
                    if (y > maxY) y = maxY;
                    overlay->move(x, y);
                    overlay->raise();
                }
            }
        }
    }

    if (auto *btn = qobject_cast<QToolButton *>(obj)) {
        if (btn->property("notionalIndex").isValid()) {
            if (event->type() == QEvent::MouseButtonDblClick) {
                auto *column =
                    static_cast<QWidget *>(btn->property("domContainerPtr").value<void *>());
                const int presetIndex = btn->property("notionalIndex").toInt();
                startNotionalEdit(column, presetIndex);
                return true;
            }
        }
    }

    if (obj->objectName() == QLatin1String("NotionalEditOverlay") &&
        event->type() == QEvent::MouseButtonPress) {
        QWidget *column = static_cast<QWidget *>(obj->property("domContainerPtr").value<void *>());
        commitNotionalEdit(column, false);
        return true;
    }

    if (auto *line = qobject_cast<QLineEdit *>(obj)) {
        if (line->objectName() == QLatin1String("NotionalEditField") &&
            event->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent *>(event);
            if (ke->key() == Qt::Key_Escape) {
                QWidget *column =
                    static_cast<QWidget *>(line->property("domContainerPtr").value<void *>());
                commitNotionalEdit(column, false);
                return true;
            }
        }
    }

    if (event->type() == QEvent::Wheel && m_capsAdjustMode) {
        if (auto *wheel = static_cast<QWheelEvent *>(event)) {
            const int steps = wheel->angleDelta().y() / 120;
            if (steps != 0) {
                adjustVolumeRulesBySteps(steps);
                return true;
            }
        }
    }

    if (m_domResizeActive) {
        if (event->type() == QEvent::MouseMove) {
            if (auto *me = dynamic_cast<QMouseEvent *>(event)) {
                const int delta = me->globalPos().x() - m_domResizeStartPos.x();
                updateDomColumnResize(delta);
                return true;
            }
        }
    }
    if (event->type() == QEvent::MouseButtonRelease && (m_domResizeActive || m_domResizePending)) {
        endDomColumnResize();
    }
    if (m_domResizePending) {
        if (event->type() == QEvent::MouseMove) {
            if (auto *me = dynamic_cast<QMouseEvent *>(event)) {
                if ((me->buttons() & Qt::LeftButton)
                    && (me->globalPos() - m_domResizeStartPos).manhattanLength() > 2) {
                    m_domResizeActive = true;
                    m_domResizePending = false;
                    const int delta = me->globalPos().x() - m_domResizeStartPos.x();
                    updateDomColumnResize(delta);
                    return true;
                }
            }
        }
    }

    if (obj->objectName() == QLatin1String("DomColumnResizeHandle")) {
        auto *handle = qobject_cast<QWidget *>(obj);
        QWidget *column = nullptr;
        QVariant ptr = handle ? handle->property("domContainerPtr") : QVariant();
        if (ptr.isValid()) {
            column = static_cast<QWidget *>(ptr.value<void *>());
        }
        if (!column) {
            return QMainWindow::eventFilter(obj, event);
        }

        WorkspaceTab *tab = nullptr;
        DomColumn *col = nullptr;
        int columnIndex = -1;
        locateColumn(column, tab, col, columnIndex);

        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton && tab && tab->columns) {
                m_domResizePending = true;
                m_domResizeContainer = column;
                m_domResizeTab = tab;
                m_domResizeSplitterIndex = columnIndex;
                m_domResizeFromLeftEdge = false;
                m_domResizeInitialSizes = tab->columns->sizes();
                m_domResizeStartPos = me->globalPos();
                if (m_domResizeHandle != handle) {
                    releaseDomResizeMouseGrab();
                    handle->grabMouse(Qt::SizeHorCursor);
                    m_domResizeHandle = handle;
                }
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            if (m_domResizeActive && m_domResizeContainer == column) {
                auto *me = static_cast<QMouseEvent *>(event);
                const int delta = me->globalPos().x() - m_domResizeStartPos.x();
                updateDomColumnResize(delta);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            return true;
        }
        return QMainWindow::eventFilter(obj, event);
    }

    // Глобальные хоткеи, работают из любого виджета.
    if (event->type() == QEvent::KeyPress) {
        if (m_notionalEditActive) {
            return QMainWindow::eventFilter(obj, event);
        }
        auto *ke = static_cast<QKeyEvent *>(event);
        const int key = ke->key();
        const Qt::KeyboardModifiers mods = ke->modifiers();

        bool match = false;
        if (key == m_centerKey) {
            if (m_centerMods == Qt::NoModifier) {
                match = true;
            } else {
                Qt::KeyboardModifiers cleaned = mods & ~Qt::KeypadModifier;
                match = (cleaned == m_centerMods);
            }
        }
        if (match) {
            centerActiveLaddersToSpread();
            return true;
        }
        for (int i = 0; i < static_cast<int>(m_notionalPresetKeys.size()); ++i)
        {
            if (matchesHotkey(key, mods, m_notionalPresetKeys[i], m_notionalPresetMods[i]))
            {
                applyNotionalPreset(i);
                return true;
            }
        }
    }

    if (!m_topBar) {
        return QMainWindow::eventFilter(obj, event);
    }

    // Handle drag/float for DOM title bar
    if (obj->objectName() == QLatin1String("DomTitleBar")) {
        auto *frame = qobject_cast<QWidget *>(obj);
        QWidget *container = nullptr;
        QVariant ptr = frame ? frame->property("domContainerPtr") : QVariant();
        if (ptr.isValid()) {
            container = static_cast<QWidget *>(ptr.value<void *>());
        }

        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton && container) {
                m_draggingDomContainer = container;
                m_domDragStartGlobal = me->globalPos();
                m_domDragStartWindowOffset = me->globalPos() - container->mapToGlobal(QPoint(0, 0));
                m_domDragActive = false;
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            if (m_draggingDomContainer) {
                auto *me = static_cast<QMouseEvent *>(event);
                WorkspaceTab *tab = nullptr;
                DomColumn *col = nullptr;
                int idx = -1;
                locateColumn(m_draggingDomContainer, tab, col, idx);
                const int dist = (me->globalPos() - m_domDragStartGlobal).manhattanLength();
                if (dist > 6 && tab && col && !col->isFloating && (me->buttons() & Qt::LeftButton)) {
                    floatDomColumn(*tab, *col, idx, me->globalPos());
                    m_domDragActive = true;
                }
                if (m_domDragActive && col && col->floatingWindow) {
                    col->floatingWindow->move(me->globalPos() - m_domDragStartWindowOffset);
                }
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_draggingDomContainer = nullptr;
            m_domDragActive = false;
            return true;
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton && container) {
                toggleDomColumnFloating(container, me->globalPos());
                return true;
            }
        }
        return QMainWindow::eventFilter(obj, event);
    }

    // Quick hover handling for side nav buttons: ????????? ??????????? ??????
    if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
        auto *btn = qobject_cast<QToolButton *>(obj);
        if (btn && btn->objectName() == QLatin1String("SideNavButton")) {
            const QString iconName = btn->property("iconName").toString();
            if (!iconName.isEmpty()) {
                const QSize iconSize(28, 28);
                if (event->type() == QEvent::Enter) {
                    btn->setIcon(loadIconTinted(iconName, QColor("#ffffff"), iconSize));
                } else {
                    btn->setIcon(loadIconTinted(iconName, QColor("#c0c0c0"), iconSize));
                }
            }
            // Let other handlers (like tooltips) proceed.
            return QMainWindow::eventFilter(obj, event);
        }
    }


    auto *closeBtn = qobject_cast<QToolButton *>(obj);
    if (closeBtn && closeBtn->objectName() == QLatin1String("WorkspaceTabCloseButton")) {
        if (event->type() == QEvent::Enter) {
            if (!m_tabCloseIconHover.isNull()) {
                closeBtn->setIcon(m_tabCloseIconHover);
            }
        } else if (event->type() == QEvent::Leave) {
            if (!m_tabCloseIconNormal.isNull()) {
                closeBtn->setIcon(m_tabCloseIconNormal);
            }
        }
        return QMainWindow::eventFilter(obj, event);
    }

    // Double-click on workspace tab to rename it.
    if (obj == m_workspaceTabs && event->type() == QEvent::MouseButtonDblClick) {
        auto *me = static_cast<QMouseEvent *>(event);
        const QPoint pos = me->pos();
        const int index = m_workspaceTabs->tabAt(pos);
        if (index >= 0) {
            const QString currentTitle = m_workspaceTabs->tabText(index);
            bool ok = false;
            const QString newTitle = QInputDialog::getText(this,
                                                           tr("Rename tab"),
                                                           tr("Tab name:"),
                                                           QLineEdit::Normal,
                                                           currentTitle,
                                                           &ok).trimmed();
            if (ok && !newTitle.isEmpty()) {
                m_workspaceTabs->setTabText(index, newTitle);
            }
            return true;
        }
    }

    // Handle close of floating DOM window: dock it back.
    for (auto &tab : m_tabs) {
        for (auto &col : tab.columnsData) {
            if (col.floatingWindow == obj && event->type() == QEvent::Close) {
                dockDomColumn(tab, col);
                return true;
            }
        }
    }

    // While dragging a tab, hide the underline so it does not stay
    // visually attached to the old position. It will be restored when
    // tabMoved/currentChanged fires.
    if (obj == m_workspaceTabs && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            const int idx = m_workspaceTabs->tabAt(me->pos());
            if (idx == m_workspaceTabs->currentIndex()) {
                if (m_tabUnderlineAnim) {
                    m_tabUnderlineAnim->stop();
                }
                if (m_tabUnderline) {
                    m_tabUnderline->hide();
                }
                m_tabUnderlineHiddenForDrag = true;
            }
        }
    }

    if (obj == m_workspaceTabs && event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::MiddleButton) {
            const int idx = m_workspaceTabs->tabAt(me->pos());
            if (idx >= 0) {
                handleTabCloseRequested(idx);
            }
            return true;
        }
        if (me->button() == Qt::LeftButton && m_tabUnderlineHiddenForDrag) {
            m_tabUnderlineHiddenForDrag = false;
            updateTabUnderline(m_workspaceTabs->currentIndex());
        }
    }

    const bool isTopBarObject =
        obj == m_topBar || obj == m_workspaceTabs || obj == m_addTabButton
        || obj == m_timeLabel;

    // Hover/click on ticker label to open symbol picker.
    for (auto &tab : m_tabs) {
        for (auto &col : tab.columnsData) {
            if (obj == col.tickerLabel && event->type() == QEvent::Enter) {
                if (col.tickerLabel) {
                    applyTickerLabelStyle(col.tickerLabel, col.accountColor, true);
                }
                return true;
            }
            if (obj == col.tickerLabel && event->type() == QEvent::Leave) {
                if (col.tickerLabel) {
                    applyTickerLabelStyle(col.tickerLabel, col.accountColor, false);
                }
                return true;
            }
            if (obj == col.tickerLabel && event->type() == QEvent::MouseButtonPress) {
                auto *me = static_cast<QMouseEvent *>(event);
                if (me->button() == Qt::LeftButton) {
                    retargetDomColumn(col, QString());
                    return true;
                }
            }
        }
    }

    if (!isTopBarObject) {
        return QMainWindow::eventFilter(obj, event);
    }

    // Click on time label opens offset menu instead of dragging.
    if (obj == m_timeLabel && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            QMenu menu;
            struct OffsetEntry {
                QString label;
                int minutes;
            };
            QList<OffsetEntry> entries;
            for (int hours = -10; hours <= 10; ++hours) {
                OffsetEntry entry;
                entry.minutes = hours * 60;
                if (hours == 0) {
                    entry.label = QStringLiteral("UTC");
                } else {
                    entry.label =
                        QStringLiteral("UTC%1%2")
                            .arg(hours > 0 ? QStringLiteral("+") : QString()) //
                            .arg(hours);
                }
                entries.push_back(entry);
            }
            QAction *currentAction = nullptr;
            for (const auto &entry : entries) {
                QAction *act = menu.addAction(entry.label);
                act->setData(entry.minutes);
                if (entry.minutes == m_timeOffsetMinutes) {
                    act->setCheckable(true);
                    act->setChecked(true);
                    currentAction = act;
                }
            }
            if (!currentAction && m_timeOffsetMinutes == 0 && !menu.actions().isEmpty()) {
                menu.actions().first()->setCheckable(true);
                menu.actions().first()->setChecked(true);
            }

            const QPoint globalPos = m_timeLabel->mapToGlobal(
                QPoint(m_timeLabel->width() / 2, m_timeLabel->height()));
            QAction *chosen = menu.exec(globalPos);
            if (chosen) {
                m_timeOffsetMinutes = chosen->data().toInt();
                updateTimeLabel();
                saveUserSettings();
            }
            return true;
        }
    }

    if (obj == m_connectionIndicator && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            openConnectionsWindow();
            return true;
        }
    }

    // Double-click on title bar: ignored (keep single-click system behavior)

    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            // Only start native system move when the click is on an empty
            // area of the title bar (not on interactive child widgets such
            // as minimize/maximize/close buttons, tabs, etc.). This prevents
            // starting a move operation when the user actually clicks a button
            // which can lead to stuck/invalid move state after minimize.
            if (m_topBar) {
                const QPoint globalPt = me->globalPos();
                const QPoint localInTop = m_topBar->mapFromGlobal(globalPt);
                if (m_topBar->rect().contains(localInTop)) {
                    if (QWidget *child = m_topBar->childAt(localInTop)) {
                        // Click is over an interactive child ? let the child handle it.
                        return QMainWindow::eventFilter(obj, event);
                    }
                }
            }

            if (QWindow *w = windowHandle()) {
                w->startSystemMove();
                return false;
            }
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::centerActiveLaddersToSpread()
{
    WorkspaceTab *tab = currentWorkspaceTab();
    if (!tab || !tab->columns) {
        return;
    }

    auto centerDom = [](DomWidget *dom) {
        if (dom) {
            dom->centerToSpread();
        }
    };

    if (m_centerAllLadders) {
        for (auto &col : tab->columnsData) {
            centerDom(col.dom);
        }
        return;
    }

    QWidget *focus = QApplication::focusWidget();
    DomWidget *targetDom = nullptr;
    if (focus) {
        for (auto &col : tab->columnsData) {
            if (col.dom && (col.dom == focus || col.dom->isAncestorOf(focus))) {
                targetDom = col.dom;
                break;
            }
        }
    }
    if (!targetDom && !tab->columnsData.isEmpty()) {
        targetDom = tab->columnsData.front().dom;
    }
    centerDom(targetDom);
}

MainWindow::DomColumn *MainWindow::focusedDomColumn()
{
    WorkspaceTab *tab = currentWorkspaceTab();
    if (!tab) {
        return nullptr;
    }
    QWidget *focus = QApplication::focusWidget();
    if (focus) {
        for (auto &col : tab->columnsData) {
            if (!col.container) continue;
            if (col.container == focus || col.container->isAncestorOf(focus)) {
                return &col;
            }
        }
    }
    if (!tab->columnsData.isEmpty()) {
        return &tab->columnsData.front();
    }
    return nullptr;
}

void MainWindow::refreshActiveLadder()
{
    WorkspaceTab *tab = currentWorkspaceTab();
    if (!tab) {
        return;
    }
    DomColumn *col = focusedDomColumn();
    if (!col || !col->client) {
        return;
    }
    const int levels = col->levelsSpin ? col->levelsSpin->value() : m_levels;
    m_levels = levels;
        const auto src = symbolSourceForAccount(col->accountName);
        const QString exch = (src == SymbolSource::UzxSwap)
                                 ? QStringLiteral("uzxswap")
                                 : (src == SymbolSource::UzxSpot) ? QStringLiteral("uzxspot")
                                                                  : QStringLiteral("mexc");
        col->client->restart(col->symbol, levels, exch);
}

void MainWindow::handleSettingsSearch()
{
    const QString query = m_settingsSearchEdit ? m_settingsSearchEdit->text().trimmed() : QString();
    const SettingEntry *entry = matchSettingEntry(query);
    if (entry) {
        openSettingEntry(entry->id);
    }
}

void MainWindow::handleSettingsSearchFromCompleter(const QString &value)
{
    const SettingEntry *entry = matchSettingEntry(value);
    if (!entry) {
        for (const auto &candidate : m_settingEntries) {
            if (candidate.name.compare(value, Qt::CaseInsensitive) == 0) {
                entry = &candidate;
                break;
            }
        }
    }
    if (entry) {
        openSettingEntry(entry->id);
    }
}

const MainWindow::SettingEntry *MainWindow::matchSettingEntry(const QString &query) const
{
    const QString q = query.trimmed().toLower();
    if (q.isEmpty()) {
        return nullptr;
    }
    for (const auto &entry : m_settingEntries) {
        if (entry.name.toLower().contains(q)) {
            return &entry;
        }
        for (const auto &keyword : entry.keywords) {
            const QString kw = keyword.toLower();
            if (!kw.isEmpty() && (q.contains(kw) || kw.contains(q))) {
                return &entry;
            }
        }
    }
    return nullptr;
}

void MainWindow::openSettingEntry(const QString &id)
{
    openSettingsWindow();
    if (!m_settingsWindow) {
        return;
    }
    if (id == QLatin1String("centerHotkey")) {
        m_settingsWindow->focusCenterHotkey();
        return;
    }
    if (id == QLatin1String("volumeHighlight")) {
        m_settingsWindow->focusVolumeHighlightRules();
        return;
    }
    Q_UNUSED(id);
}

void MainWindow::loadUserSettings()
{
    const QString baseDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (baseDir.isEmpty()) {
        return;
    }
    QDir().mkpath(baseDir);
    const QString file = baseDir + QLatin1String("/shah_terminal.ini");
    QSettings s(file, QSettings::IniFormat);
    s.beginGroup(QStringLiteral("hotkeys"));
    m_centerKey = s.value(QStringLiteral("centerKey"), int(Qt::Key_Shift)).toInt();
    m_centerMods = Qt::KeyboardModifiers(
        s.value(QStringLiteral("centerMods"), int(Qt::NoModifier)).toInt());
    m_centerAllLadders =
        s.value(QStringLiteral("centerAllLadders"), true).toBool();
    m_newTabKey = s.value(QStringLiteral("newTabKey"), int(Qt::Key_T)).toInt();
    m_newTabMods = Qt::KeyboardModifiers(
        s.value(QStringLiteral("newTabMods"), int(Qt::ControlModifier)).toInt());
    m_addLadderKey = s.value(QStringLiteral("addLadderKey"), int(Qt::Key_E)).toInt();
    m_addLadderMods = Qt::KeyboardModifiers(
        s.value(QStringLiteral("addLadderMods"), int(Qt::ControlModifier)).toInt());
    m_refreshLadderKey = s.value(QStringLiteral("refreshLadderKey"), int(Qt::Key_R)).toInt();
    m_refreshLadderMods = Qt::KeyboardModifiers(
        s.value(QStringLiteral("refreshLadderMods"), int(Qt::ControlModifier)).toInt());
    m_volumeAdjustKey = s.value(QStringLiteral("volumeAdjustKey"), int(Qt::Key_CapsLock)).toInt();
    m_volumeAdjustMods = Qt::KeyboardModifiers(
        s.value(QStringLiteral("volumeAdjustMods"), int(Qt::NoModifier)).toInt());
    for (int i = 0; i < static_cast<int>(m_notionalPresetKeys.size()); ++i)
    {
        const QString keyName = QStringLiteral("notionalPresetKey%1").arg(i + 1);
        const QString modName = QStringLiteral("notionalPresetMods%1").arg(i + 1);
        m_notionalPresetKeys[i] =
            s.value(keyName, int(Qt::Key_1) + i).toInt();
        m_notionalPresetMods[i] =
            Qt::KeyboardModifiers(s.value(modName, int(Qt::NoModifier)).toInt());
    }
    s.endGroup();

    s.beginGroup(QStringLiteral("clock"));
    m_timeOffsetMinutes = s.value(QStringLiteral("offsetMinutes"), 0).toInt();
    s.endGroup();

    s.beginGroup(QStringLiteral("ladder"));
    m_volumeRules.clear();
    int ruleCount = s.beginReadArray(QStringLiteral("volumeRules"));
    for (int i = 0; i < ruleCount; ++i) {
        s.setArrayIndex(i);
        VolumeHighlightRule rule;
        rule.threshold = s.value(QStringLiteral("threshold"), 0.0).toDouble();
        rule.color = QColor(s.value(QStringLiteral("color"), QStringLiteral("#ffd54f")).toString());
        if (rule.color.isValid()) {
            m_volumeRules.append(rule);
        }
    }
    s.endArray();
    s.endGroup();
    std::sort(m_volumeRules.begin(), m_volumeRules.end(), [](const VolumeHighlightRule &a, const VolumeHighlightRule &b) {
        return a.threshold < b.threshold;
    });

    s.beginGroup(QStringLiteral("symbols"));
    const QStringList savedSymbols = s.value(QStringLiteral("list")).toStringList();
    if (!savedSymbols.isEmpty()) {
        m_symbolLibrary = savedSymbols;
    }
    // Не тянем старый список apiOff из настроек, чтобы не красить все тикеры при ошибочных данных.
    m_apiOffSymbols.clear();
    s.endGroup();

    m_savedLayout.clear();
    s.beginGroup(QStringLiteral("workspace"));
    int tabCount = s.beginReadArray(QStringLiteral("tabs"));
    for (int t = 0; t < tabCount; ++t) {
        s.setArrayIndex(t);
        QVector<SavedColumn> cols;
        int colCount = s.beginReadArray(QStringLiteral("columns"));
        for (int c = 0; c < colCount; ++c) {
            s.setArrayIndex(c);
            SavedColumn sc;
            sc.symbol = s.value(QStringLiteral("symbol")).toString();
            sc.compression = s.value(QStringLiteral("compression"), 1).toInt();
            sc.account = s.value(QStringLiteral("account"), QStringLiteral("MEXC Spot")).toString();
            if (!sc.symbol.trimmed().isEmpty()) {
                cols.push_back(sc);
            }
        }
        s.endArray();
        if (!cols.isEmpty()) {
            m_savedLayout.push_back(cols);
        }
    }
    s.endArray();
    s.endGroup();
}

void MainWindow::saveUserSettings() const
{
    const QString baseDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (baseDir.isEmpty()) {
        return;
    }
    QDir().mkpath(baseDir);
    const QString file = baseDir + QLatin1String("/shah_terminal.ini");
    QSettings s(file, QSettings::IniFormat);
    s.beginGroup(QStringLiteral("hotkeys"));
    s.setValue(QStringLiteral("centerKey"), m_centerKey);
    s.setValue(QStringLiteral("centerMods"), int(m_centerMods));
    s.setValue(QStringLiteral("centerAllLadders"), m_centerAllLadders);
    s.setValue(QStringLiteral("newTabKey"), m_newTabKey);
    s.setValue(QStringLiteral("newTabMods"), int(m_newTabMods));
    s.setValue(QStringLiteral("addLadderKey"), m_addLadderKey);
    s.setValue(QStringLiteral("addLadderMods"), int(m_addLadderMods));
    s.setValue(QStringLiteral("refreshLadderKey"), m_refreshLadderKey);
    s.setValue(QStringLiteral("refreshLadderMods"), int(m_refreshLadderMods));
    s.setValue(QStringLiteral("volumeAdjustKey"), m_volumeAdjustKey);
    s.setValue(QStringLiteral("volumeAdjustMods"), int(m_volumeAdjustMods));
    for (int i = 0; i < static_cast<int>(m_notionalPresetKeys.size()); ++i)
    {
        const QString keyName = QStringLiteral("notionalPresetKey%1").arg(i + 1);
        const QString modName = QStringLiteral("notionalPresetMods%1").arg(i + 1);
        s.setValue(keyName, m_notionalPresetKeys[i]);
        s.setValue(modName, int(m_notionalPresetMods[i]));
    }
    s.endGroup();

    s.beginGroup(QStringLiteral("clock"));
    s.setValue(QStringLiteral("offsetMinutes"), m_timeOffsetMinutes);
    s.endGroup();

    s.beginGroup(QStringLiteral("ladder"));
    s.beginWriteArray(QStringLiteral("volumeRules"));
    for (int i = 0; i < m_volumeRules.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("threshold"), m_volumeRules[i].threshold);
        s.setValue(QStringLiteral("color"), m_volumeRules[i].color.name(QColor::HexRgb));
    }
    s.endArray();
    s.endGroup();

    s.beginGroup(QStringLiteral("symbols"));
    s.setValue(QStringLiteral("list"), m_symbolLibrary);
    s.setValue(QStringLiteral("apiOff"), QStringList(m_apiOffSymbols.begin(), m_apiOffSymbols.end()));
    s.endGroup();

    s.beginGroup(QStringLiteral("workspace"));
    s.beginWriteArray(QStringLiteral("tabs"));
    for (int t = 0; t < m_tabs.size(); ++t) {
        s.setArrayIndex(t);
        const auto &tab = m_tabs[t];
        s.beginWriteArray(QStringLiteral("columns"));
        for (int c = 0; c < tab.columnsData.size(); ++c) {
            s.setArrayIndex(c);
            const auto &col = tab.columnsData[c];
            s.setValue(QStringLiteral("symbol"), col.symbol);
            s.setValue(QStringLiteral("compression"), col.tickCompression);
            s.setValue(QStringLiteral("account"), col.accountName);
        }
        s.endArray();
    }
    s.endArray();
    s.endGroup();
}


void MainWindow::adjustVolumeRulesBySteps(int steps)
{
    if (steps == 0 || m_volumeRules.isEmpty()) {
        return;
    }
    const double stepFactor = 0.1;
    double factor = 1.0 + stepFactor * steps;
    if (factor <= 0.0) {
        factor = 0.1;
    }
    for (auto &rule : m_volumeRules) {
        rule.threshold = std::max(1.0, rule.threshold * factor);
    }
    std::sort(m_volumeRules.begin(), m_volumeRules.end(), [](const VolumeHighlightRule &a, const VolumeHighlightRule &b) {
        return a.threshold < b.threshold;
    });
    applyVolumeRulesToAllDoms();
    if (m_settingsWindow) {
        m_settingsWindow->setVolumeHighlightRules(m_volumeRules);
    }
    saveUserSettings();
    const int pct = static_cast<int>(std::round(factor * 100.0));
    statusBar()->showMessage(tr("Volume thresholds x%1%").arg(pct), 1200);
}

QVector<SettingsWindow::HotkeyEntry> MainWindow::currentCustomHotkeys() const
{
    QVector<SettingsWindow::HotkeyEntry> entries;
    entries.append({QStringLiteral("newTab"),
                    tr("Открыть новую вкладку"),
                    m_newTabKey,
                    m_newTabMods});
    entries.append({QStringLiteral("addLadder"),
                    tr("Добавить стакан в текущую вкладку"),
                    m_addLadderKey,
                    m_addLadderMods});
    entries.append({QStringLiteral("refreshLadder"),
                    tr("Перезапустить активный стакан"),
                    m_refreshLadderKey,
                    m_refreshLadderMods});
    entries.append({QStringLiteral("volumeAdjust"),
                    tr("Режим изменения порогов колесом"),
                    m_volumeAdjustKey,
                    m_volumeAdjustMods});
    for (int i = 0; i < static_cast<int>(m_notionalPresetKeys.size()); ++i)
    {
        entries.append({QStringLiteral("notionalPreset%1").arg(i + 1),
                        tr("Горячая клавиша пресета объема %1").arg(i + 1),
                        m_notionalPresetKeys[i],
                        m_notionalPresetMods[i]});
    }
    return entries;
}
void MainWindow::updateCustomHotkey(const QString &id, int key, Qt::KeyboardModifiers mods)
{
    auto assign = [&](int &targetKey, Qt::KeyboardModifiers &targetMods) {
        targetKey = key;
        targetMods = mods;
    };
    if (id == QLatin1String("newTab")) {
        assign(m_newTabKey, m_newTabMods);
    } else if (id == QLatin1String("addLadder")) {
        assign(m_addLadderKey, m_addLadderMods);
    } else if (id == QLatin1String("refreshLadder")) {
        assign(m_refreshLadderKey, m_refreshLadderMods);
    } else if (id == QLatin1String("volumeAdjust")) {
        assign(m_volumeAdjustKey, m_volumeAdjustMods);
        m_capsAdjustMode = false;
    } else if (id.startsWith(QLatin1String("notionalPreset"))) {
        bool ok = false;
        const int idx = id.mid(QStringLiteral("notionalPreset").size()).toInt(&ok) - 1;
        if (!ok || idx < 0 || idx >= static_cast<int>(m_notionalPresetKeys.size())) {
            return;
        }
        assign(m_notionalPresetKeys[idx], m_notionalPresetMods[idx]);
    } else {
        return;
    }
    saveUserSettings();
}
bool MainWindow::matchesHotkey(int eventKey,
                               Qt::KeyboardModifiers eventMods,
                               int key,
                               Qt::KeyboardModifiers mods)
{
    if (key == 0) {
        return false;
    }
    Qt::KeyboardModifiers cleaned = eventMods & ~Qt::KeypadModifier;
    return eventKey == key && cleaned == mods;
}

QVector<VolumeHighlightRule> MainWindow::defaultVolumeHighlightRules() const
{
    return {
        {1000.0, QColor("#ffd54f")},
        {2000.0, QColor("#ffb74d")},
        {10000.0, QColor("#ff8a65")},
        {50000.0, QColor("#ffb74d")},
        {100000.0, QColor("#ffd54f")}
    };
}

void MainWindow::applyVolumeRulesToAllDoms()
{
    for (auto &tab : m_tabs) {
        for (auto &col : tab.columnsData) {
            if (col.dom) {
                col.dom->setVolumeHighlightRules(m_volumeRules);
            }
        }
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
#ifdef Q_OS_WIN
    // Enable native snapping/resizing, but keep frameless (no system caption).
    if (!m_nativeSnapEnabled) {
        WId id = winId();
        HWND hwnd = reinterpret_cast<HWND>(id);
        if (hwnd) {
            LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
            // Keep WS_CAPTION off so system doesn't draw its title bar; enable thick frame
            // so native snapping and resize work.
            style &= ~WS_CAPTION;
            style |= WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
            SetWindowLongPtr(hwnd, GWL_STYLE, style);
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
        m_nativeSnapEnabled = true;
    }
#endif
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveUserSettings();
    // Гарантированно закрываем приложение даже если остаются невидимые окна или фоновые таймеры.
    QTimer::singleShot(0, qApp, []() { QCoreApplication::exit(0); });
    QMainWindow::closeEvent(event);
}
void MainWindow::addLocalOrderMarker(const QString &accountName,
                                     const QString &symbol,
                                     OrderSide side,
                                     double price,
                                     double quantity,
                                     qint64 createdMs)
{
    const qint64 ts = createdMs > 0 ? createdMs : QDateTime::currentMSecsSinceEpoch();
    const QString symUpper = symbol.toUpper();
    const QString accountLower = accountName.trimmed().toLower();
    const int maxMarkers = 20;
    for (auto &tab : m_tabs) {
        for (auto &col : tab.columnsData) {
            if (col.symbol.toUpper() != symUpper) {
                continue;
            }
            if (!accountLower.isEmpty()
                && col.accountName.trimmed().toLower() != accountLower) {
                continue;
            }
            DomWidget::LocalOrderMarker base;
            base.price = price;
            base.quantity = std::abs(quantity * price);
            base.side = side;
            base.createdMs = ts;
            MainWindow::ManualOrder entry;
            entry.marker = base;
            entry.synced = false;
            col.manualOrders.append(entry);
            if (col.manualOrders.size() > maxMarkers) {
                col.manualOrders.remove(0, col.manualOrders.size() - maxMarkers);
            }
            refreshColumnMarkers(col);
        }
    }
}

void MainWindow::removeLocalOrderMarker(const QString &accountName,
                                        const QString &symbol,
                                        OrderSide side,
                                        double price)
{
    const QString symUpper = symbol.toUpper();
    const QString accountLower = accountName.trimmed().toLower();
    const double tol = 1e-8;
    for (auto &tab : m_tabs) {
        for (auto &col : tab.columnsData) {
            if (col.symbol.toUpper() != symUpper) continue;
            if (!accountLower.isEmpty()
                && col.accountName.trimmed().toLower() != accountLower) {
                continue;
            }
            auto matchMarker = [&](const DomWidget::LocalOrderMarker &m) {
                if (m.side != side) return false;
                return std::abs(m.price - price) <= tol;
            };
            const int beforeLocal = col.localOrders.size();
            col.localOrders.erase(std::remove_if(col.localOrders.begin(),
                                                 col.localOrders.end(),
                                                 matchMarker),
                                  col.localOrders.end());
            auto matchManual = [&](const ManualOrder &m) {
                if (m.marker.side != side) return false;
                return std::abs(m.marker.price - price) <= tol;
            };
            const int beforeManual = col.manualOrders.size();
            const auto manualIt = std::remove_if(col.manualOrders.begin(),
                                                 col.manualOrders.end(),
                                                 matchManual);
            if (manualIt != col.manualOrders.end()) {
                col.manualOrders.erase(manualIt, col.manualOrders.end());
            }
            if (col.localOrders.size() != beforeLocal || col.manualOrders.size() != beforeManual) {
                refreshColumnMarkers(col);
            }
        }
    }
}

void MainWindow::handleLocalOrdersUpdated(const QString &accountName,
                                          const QString &symbol,
                                          const QVector<DomWidget::LocalOrderMarker> &markers)
{
    const QString normalizedAccount = accountName.trimmed().isEmpty()
                                           ? QStringLiteral("MEXC Spot")
                                           : accountName.trimmed();
    const QString targetAccount = normalizedAccount.toLower();
    const QString targetSymbol = symbol.toUpper();
    for (auto &tab : m_tabs) {
        for (auto &col : tab.columnsData) {
            if (!targetSymbol.isEmpty() && col.symbol.toUpper() != targetSymbol) {
                continue;
            }
            if (!targetAccount.isEmpty()
                && !col.accountName.trimmed().isEmpty()
                && col.accountName.toLower() != targetAccount) {
                continue;
            }
            col.remoteOrders = markers;
            refreshColumnMarkers(col);
        }
    }
}

bool MainWindow::containsSimilarMarker(const QVector<DomWidget::LocalOrderMarker> &markers,
                                       const DomWidget::LocalOrderMarker &candidate) const
{
    constexpr double tol = 1e-8;
    for (const auto &existing : markers) {
        if (existing.side == candidate.side && std::abs(existing.price - candidate.price) <= tol) {
            return true;
        }
    }
    return false;
}

void MainWindow::refreshColumnMarkers(DomColumn &col)
{
    QVector<DomWidget::LocalOrderMarker> combined = col.remoteOrders;
    QVector<ManualOrder> updatedManuals;
    updatedManuals.reserve(col.manualOrders.size());
    for (auto &manual : col.manualOrders) {
        const bool remoteHas = containsSimilarMarker(col.remoteOrders, manual.marker);
        if (remoteHas) {
            manual.synced = true;
            if (!containsSimilarMarker(combined, manual.marker)) {
                combined.push_back(manual.marker);
            }
            updatedManuals.push_back(manual);
        } else if (!manual.synced) {
            // still waiting for remote ack
            if (!containsSimilarMarker(combined, manual.marker)) {
                combined.push_back(manual.marker);
            }
            updatedManuals.push_back(manual);
        } else {
            // synced but remote gone -> drop marker
        }
    }
    col.manualOrders = std::move(updatedManuals);
    col.localOrders = combined;
    if (col.dom) {
        col.dom->setLocalOrders(col.localOrders);
    }
    if (col.prints) {
        QVector<LocalOrderMarker> printMarkers;
        printMarkers.reserve(col.localOrders.size());
        for (const auto &m : col.localOrders) {
            LocalOrderMarker pm;
            pm.price = m.price;
            pm.quantity = m.quantity;
            pm.buy = (m.side == OrderSide::Buy);
            pm.createdMs = m.createdMs;
            printMarkers.push_back(pm);
        }
        col.prints->setLocalOrders(printMarkers);
    }
}


void MainWindow::fetchSymbolLibrary()
{
    if (m_symbolRequestInFlight) {
        return;
    }
    // Сбрасываем api-off перед новой загрузкой, чтобы не красить весь список старыми данными.
    m_apiOffSymbols.clear();
    const QUrl url(QStringLiteral("https://api.mexc.com/api/v3/exchangeInfo"));
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_symbolRequestInFlight = true;
    auto *reply = m_symbolFetcher.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_symbolRequestInFlight = false;
        const auto err = reply->error();
        const QByteArray raw = reply->readAll();
        reply->deleteLater();
        if (err != QNetworkReply::NoError) {
            qWarning() << "[symbols] fetch failed:" << err;
            statusBar()->showMessage(tr("Failed to load symbols"), 2500);
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (!doc.isObject()) {
            qWarning() << "[symbols] invalid payload";
            return;
        }
        const QJsonArray arr = doc.object().value(QStringLiteral("symbols")).toArray();
        QStringList fetched;
        QSet<QString> apiOff;
        fetched.reserve(arr.size());
        for (const auto &v : arr) {
            const QJsonObject obj = v.toObject();
            QString sym = obj.value(QStringLiteral("symbol")).toString().trimmed().toUpper();
            if (sym.isEmpty()) {
                continue;
            }
            const QString status = obj.value(QStringLiteral("status")).toString();
            const bool spotAllowed = obj.value(QStringLiteral("isSpotTradingAllowed")).toBool(true);
            const bool tradable = status.isEmpty()
                                  || status.compare(QStringLiteral("TRADING"), Qt::CaseInsensitive) == 0
                                  || status.compare(QStringLiteral("ENABLED"), Qt::CaseInsensitive) == 0;
            fetched.push_back(sym);
            if (!tradable || !spotAllowed) {
                apiOff.insert(sym);
            }
        }
        if (!fetched.isEmpty()) {
            mergeSymbolLibrary(fetched, apiOff);
            statusBar()->showMessage(tr("Loaded %1 symbols").arg(fetched.size()), 2000);
        }
    });
}

MainWindow::SymbolSource MainWindow::symbolSourceForAccount(const QString &accountName) const
{
    const QString lower = accountName.toLower();
    if (lower.contains(QStringLiteral("uzx"))) {
        if (lower.contains(QStringLiteral("spot"))) {
            return SymbolSource::UzxSpot;
        }
        return SymbolSource::UzxSwap;
    }
    return SymbolSource::Mexc;
}

void MainWindow::fetchSymbolLibrary(SymbolSource source, SymbolPickerDialog *dlg)
{
    if (source == SymbolSource::Mexc) {
        fetchSymbolLibrary();
        if (dlg) {
            dlg->setSymbols(m_symbolLibrary, m_apiOffSymbols);
        }
        return;
    }

    const bool isSwap = source == SymbolSource::UzxSwap;
    bool &inFlightRef = isSwap ? m_uzxSwapRequestInFlight : m_uzxSpotRequestInFlight;
    if (inFlightRef) {
        return;
    }
    if (isSwap && !m_uzxSwapSymbols.isEmpty()) {
        if (dlg) dlg->setSymbols(m_uzxSwapSymbols, m_uzxSwapApiOff);
        return;
    }
    if (!isSwap && !m_uzxSpotSymbols.isEmpty()) {
        if (dlg) dlg->setSymbols(m_uzxSpotSymbols, m_uzxSpotApiOff);
        return;
    }

    const QString urlStr = isSwap ? QStringLiteral("https://api-v2.uzx.com/notification/swap/tickers")
                                  : QStringLiteral("https://api-v2.uzx.com/notification/spot/tickers");
    inFlightRef = true;
    bool *flagPtr = &inFlightRef;
    auto *reply = m_symbolFetcher.get(QNetworkRequest(QUrl(urlStr)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, dlg, isSwap, flagPtr]() {
        if (flagPtr) {
            *flagPtr = false;
        }
        const auto err = reply->error();
        const QByteArray raw = reply->readAll();
        reply->deleteLater();
        if (err != QNetworkReply::NoError) {
            qWarning() << "[symbols] uzx fetch failed:" << err;
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (!doc.isObject()) {
            return;
        }
        const QJsonArray arr = doc.object().value(QStringLiteral("data")).toArray();
        QStringList list;
        list.reserve(arr.size());
        for (const auto &v : arr) {
            const QJsonObject obj = v.toObject();
            QString sym = obj.value(QStringLiteral("symbol")).toString().trimmed().toUpper();
            if (sym.isEmpty()) continue;
            sym = sym.replace(QStringLiteral("-"), QString());
            if (!list.contains(sym, Qt::CaseInsensitive)) {
                list.push_back(sym);
            }
        }
        std::sort(list.begin(), list.end(), [](const QString &a, const QString &b) {
            return a.toUpper() < b.toUpper();
        });
        if (isSwap) {
            m_uzxSwapSymbols = list;
            m_uzxSwapApiOff.clear();
        } else {
            m_uzxSpotSymbols = list;
            m_uzxSpotApiOff.clear();
        }
        if (dlg) {
            dlg->setSymbols(list, QSet<QString>());
        }
    });
}

void MainWindow::mergeSymbolLibrary(const QStringList &symbols, const QSet<QString> &apiOff)
{
    QStringList merged;
    merged.reserve(m_symbolLibrary.size() + symbols.size());
    auto addList = [&merged](const QStringList &src) {
        for (const QString &sym : src) {
            const QString s = sym.trimmed().toUpper();
            if (s.isEmpty() || merged.contains(s, Qt::CaseInsensitive)) {
                continue;
            }
            merged.push_back(s);
        }
    };
    addList(m_symbolLibrary);
    addList(symbols);
    std::sort(merged.begin(), merged.end(), [](const QString &a, const QString &b) {
        return a.toUpper() < b.toUpper();
    });
    m_symbolLibrary = merged;
    m_apiOffSymbols.clear();
    for (const QString &s : apiOff) {
        m_apiOffSymbols.insert(s.trimmed().toUpper());
    }
}

SymbolPickerDialog *MainWindow::createSymbolPicker(const QString &title,
                                                   const QString &currentSymbol,
                                                   const QString &currentAccount)
{
    auto *dlg = new SymbolPickerDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->setModal(false);
    dlg->setWindowModality(Qt::NonModal);
    dlg->setWindowTitle(title);
    // Если по какой-то причине api-off внезапно содержит почти все тикеры, не красим всё подряд.
    const bool apiOffLooksSuspicious =
        !m_apiOffSymbols.isEmpty() && m_apiOffSymbols.size() >= (m_symbolLibrary.size() * 8) / 10;
    const QSet<QString> apiOff = apiOffLooksSuspicious ? QSet<QString>() : m_apiOffSymbols;
    const SymbolSource src = symbolSourceForAccount(currentAccount.isEmpty() ? currentAccount : dlg->selectedAccount());
    if (src == SymbolSource::Mexc) {
        dlg->setSymbols(m_symbolLibrary, apiOff);
    } else if (src == SymbolSource::UzxSwap && !m_uzxSwapSymbols.isEmpty()) {
        dlg->setSymbols(m_uzxSwapSymbols, m_uzxSwapApiOff);
    } else if (src == SymbolSource::UzxSpot && !m_uzxSpotSymbols.isEmpty()) {
        dlg->setSymbols(m_uzxSpotSymbols, m_uzxSpotApiOff);
    } else {
        fetchSymbolLibrary(src, dlg);
    }

    QVector<QPair<QString, QColor>> accounts;
    QSet<QString> seen;
    for (auto it = m_accountColors.constBegin(); it != m_accountColors.constEnd(); ++it) {
        accounts.push_back({it.key(), it.value()});
        seen.insert(it.key().toLower());
    }
    auto ensureAccount = [&](const QString &name, const QColor &fallback) {
        if (seen.contains(name.toLower())) return;
        accounts.push_back({name, fallback});
        seen.insert(name.toLower());
    };
    ensureAccount(QStringLiteral("MEXC Spot"), QColor("#4c9fff"));
    ensureAccount(QStringLiteral("MEXC Futures"), QColor("#f5b642"));
    ensureAccount(QStringLiteral("UZX Spot"), QColor("#8bc34a"));
    ensureAccount(QStringLiteral("UZX Swap"), QColor("#ff7f50"));
    dlg->setAccounts(accounts);
    dlg->setCurrentSymbol(currentSymbol);
    dlg->setCurrentAccount(currentAccount.isEmpty() ? QStringLiteral("MEXC Spot") : currentAccount);
    connect(dlg,
            &SymbolPickerDialog::refreshRequested,
            this,
            [this, dlg]() {
                const SymbolSource src = symbolSourceForAccount(dlg->selectedAccount());
                fetchSymbolLibrary(src, dlg);
            });
    connect(dlg,
            &SymbolPickerDialog::accountChanged,
            this,
            [this, dlg](const QString &account) {
                const SymbolSource src = symbolSourceForAccount(account);
                if (src == SymbolSource::Mexc) {
                    dlg->setSymbols(m_symbolLibrary, m_apiOffSymbols);
                } else if (src == SymbolSource::UzxSwap && !m_uzxSwapSymbols.isEmpty()) {
                    dlg->setSymbols(m_uzxSwapSymbols, m_uzxSwapApiOff);
                } else if (src == SymbolSource::UzxSpot && !m_uzxSpotSymbols.isEmpty()) {
                    dlg->setSymbols(m_uzxSpotSymbols, m_uzxSpotApiOff);
                } else {
                    fetchSymbolLibrary(src, dlg);
                }
            });
    return dlg;
}

void MainWindow::applySymbolToColumn(DomColumn &col,
                                     const QString &symbol,
                                     const QString &accountName)
{
    const QString sym = symbol.trimmed().toUpper();
    if (sym.isEmpty()) {
        return;
    }
    QString account = accountName.trimmed();
    if (account.isEmpty()) {
        account = QStringLiteral("MEXC Spot");
    }

    if (!m_symbolLibrary.contains(sym, Qt::CaseInsensitive)) {
        m_symbolLibrary.push_back(sym);
    }

    col.symbol = sym;
    col.accountName = account;
    col.accountColor = accountColorFor(account);
    if (col.tickerLabel) {
        col.tickerLabel->setProperty("accountColor", col.accountColor);
        col.tickerLabel->setText(sym);
        applyTickerLabelStyle(col.tickerLabel, col.accountColor, false);
    }
    applyHeaderAccent(col);

    if (!col.localOrders.isEmpty()) {
        col.localOrders.clear();
        if (col.dom) {
            col.dom->setLocalOrders(col.localOrders);
        }
        if (col.prints) {
            QVector<LocalOrderMarker> empty;
            col.prints->setLocalOrders(empty);
        }
    }

    const int levels = col.levelsSpin ? col.levelsSpin->value() : m_levels;
    if (col.client) {
        const auto src = symbolSourceForAccount(col.accountName);
        const QString exch = (src == SymbolSource::UzxSwap)
                                 ? QStringLiteral("uzxswap")
                                 : (src == SymbolSource::UzxSpot) ? QStringLiteral("uzxspot")
                                                                  : QStringLiteral("mexc");
        col.client->restart(sym, levels, exch);
    }
}

void MainWindow::refreshAccountColors()
{
    m_accountColors.clear();
    auto insertProfile = [&](ConnectionStore::Profile profile,
                             const QString &fallbackName,
                             const QString &fallbackColor) {
        MexcCredentials creds =
            m_connectionStore ? m_connectionStore->loadMexcCredentials(profile) : MexcCredentials{};
        QString name = creds.label.trimmed();
        if (name.isEmpty()) {
            name = fallbackName;
        }
        QColor color = QColor(!creds.colorHex.isEmpty() ? creds.colorHex : fallbackColor);
        if (!color.isValid()) {
            color = QColor(fallbackColor);
        }
        m_accountColors.insert(name, color);
    };
    insertProfile(ConnectionStore::Profile::MexcSpot,
                  QStringLiteral("MEXC Spot"),
                  QStringLiteral("#4c9fff"));
    insertProfile(ConnectionStore::Profile::MexcFutures,
                  QStringLiteral("MEXC Futures"),
                  QStringLiteral("#f5b642"));
    insertProfile(ConnectionStore::Profile::UzxSwap,
                  QStringLiteral("UZX Swap"),
                  QStringLiteral("#ff7f50"));
    insertProfile(ConnectionStore::Profile::UzxSpot,
                  QStringLiteral("UZX Spot"),
                  QStringLiteral("#8bc34a"));
    applyAccountColorsToColumns();
}

QColor MainWindow::accountColorFor(const QString &accountName) const
{
    const QString nameLower = accountName.trimmed().toLower();
    for (auto it = m_accountColors.constBegin(); it != m_accountColors.constEnd(); ++it) {
        if (it.key().trimmed().toLower() == nameLower) {
            return it.value();
        }
    }
    if (nameLower.contains(QStringLiteral("uzx"))) {
        if (nameLower.contains(QStringLiteral("spot"))) {
            return QColor("#8bc34a");
        }
        return QColor("#ff7f50");
    }
    if (nameLower.contains(QStringLiteral("future"))) {
        return QColor("#f5b642");
    }
    return QColor("#4c9fff");
}

void MainWindow::applyAccountColorsToColumns()
{
    for (auto &tab : m_tabs) {
        for (auto &col : tab.columnsData) {
            col.accountColor = accountColorFor(col.accountName);
            if (col.tickerLabel) {
                col.tickerLabel->setProperty("accountColor", col.accountColor);
                applyTickerLabelStyle(col.tickerLabel, col.accountColor, false);
            }
            applyHeaderAccent(col);
        }
    }
}

void MainWindow::applyTickerLabelStyle(QLabel *label, const QColor &accent, bool hovered)
{
    if (!label) {
        return;
    }
    QColor textColor = accent.isValid() ? accent : QColor("#8ab4ff");
    if (hovered) {
        textColor = textColor.lighter(125);
    }
    label->setStyleSheet(
        QStringLiteral("QLabel { color:%1; font-weight:600; padding:0px; }")
            .arg(textColor.name(QColor::HexRgb)));
}

void MainWindow::applyHeaderAccent(DomColumn &col)
{
    if (!col.header) {
        return;
    }
    const QColor base = col.accountColor.isValid() ? col.accountColor : QColor("#3a7bd5");
    const QColor border = base.darker(135);
    const QString style = QStringLiteral(
        "QFrame#DomTitleBar {"
        "  background:%1;"
        "  border:1px solid %2;"
        "  border-radius:0px;"
        "}"
        "QToolButton {"
        "  background: transparent;"
        "  color: #f0f0f0;"
        "}"
        "QToolButton:hover {"
        "  background: rgba(255,255,255,0.07);"
        "}"
        "QSpinBox {"
        "  background: rgba(0,0,0,0.25);"
        "  color: #ffffff;"
        "  border: 1px solid %2;"
        "  border-radius: 4px;"
        "  padding-right: 6px;"
        "}"
        "QToolButton#CompressionButton { padding:0 4px; }")
                           .arg(base.name(QColor::HexRgb))
                           .arg(border.name(QColor::HexRgb));
    col.header->setStyleSheet(style);
}

