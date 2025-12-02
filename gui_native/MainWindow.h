#pragma once

#include <QMainWindow>
#include "DomTypes.h"
#include "SettingsWindow.h"
#include "TradeManager.h"

#include <QList>
#include <QVector>
#include <QStringList>
#include <QRect>
#include <array>

class QLabel;
class QTabBar;
class QFrame;
class QPropertyAnimation;
class QStackedWidget;
class QToolButton;
class QSplitter;
class QTimer;
class QWidget;
class QLineEdit;
class QCompleter;
class QPushButton;
class QIcon;
class QEvent;
class QScrollArea;
class QScrollBar;
class QSpinBox;
class QSpinBox;

class DomWidget;
class LadderClient;
class PrintsWidget;
class PluginsWindow;
class ConnectionStore;
class ConnectionsWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const QString &backendPath,
                        const QString &symbol,
                        int levels,
                        QWidget *parent = nullptr);

    ~MainWindow() override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void updateMaximizeIcon();
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private slots:
    void handleTabChanged(int index);
    void handleTabCloseRequested(int index);
    void handleNewTabRequested();
    void handleNewLadderRequested();
    void handleLadderStatusMessage(const QString &msg);
    void handleLadderPingUpdated(int ms);
    void handleDomRowClicked(Qt::MouseButton button,
                             int row,
                             double price,
                             double bidQty,
                             double askQty);
    void updateTimeLabel();
    void openConnectionsWindow();
    void openPluginsWindow();
    void openSettingsWindow();
    void handleConnectionStateChanged(TradeManager::ConnectionState state, const QString &message);
    void handlePositionChanged(const QString &symbol, const TradePosition &position);
    void applyNotionalPreset(int presetIndex);
    void startNotionalEdit(QWidget *columnContainer, int presetIndex);
    void commitNotionalEdit(QWidget *columnContainer, bool apply);

private:
    struct DomColumn {
        QWidget *container = nullptr;
        QString symbol;
        DomWidget *dom = nullptr;
        LadderClient *client = nullptr;
        PrintsWidget *prints = nullptr;
        QScrollArea *scrollArea = nullptr;
        QScrollBar *scrollBar = nullptr;
        QWidget *floatingWindow = nullptr;
        bool isFloating = false;
        int lastSplitterIndex = -1;
        QList<int> lastSplitterSizes;
        QSpinBox *levelsSpin = nullptr;
        double orderNotional = 10.0;
        QWidget *notionalOverlay = nullptr;
        class QButtonGroup *notionalGroup = nullptr;
        QWidget *notionalEditOverlay = nullptr;
        class QLineEdit *notionalEditField = nullptr;
        int editingNotionalIndex = -1;
        std::array<double, 5> notionalValues{};
    };

    struct WorkspaceTab {
        int id = 0;
        QWidget *workspace = nullptr;
        QSplitter *columns = nullptr;
        QWidget *columnsSpacer = nullptr; // invisible splitter tail so the right edge stays resizable
        QVector<DomColumn> columnsData;
    };

    void buildUi();
    QWidget *buildTopBar(QWidget *parent);
    QWidget *buildMainArea(QWidget *parent);

    void createInitialWorkspace();
    void updateTabUnderline(int index);
    void createWorkspaceTab();
    void refreshTabCloseButtons();
    DomColumn createDomColumn(const QString &symbol, WorkspaceTab &tab);
    void toggleDomColumnFloating(QWidget *container, const QPoint &globalPos = QPoint());
    void floatDomColumn(WorkspaceTab &tab, DomColumn &col, int indexInSplitter, const QPoint &globalPos = QPoint());
    void dockDomColumn(WorkspaceTab &tab, DomColumn &col, int preferredIndex = -1);
    bool locateColumn(QWidget *container, WorkspaceTab *&tabOut, DomColumn *&colOut, int &splitIndex);
    void removeDomColumn(QWidget *container);
    void updateDomColumnResize(int delta);
    void endDomColumnResize();
    void cancelPendingDomResize();
    void releaseDomResizeMouseGrab();
    enum class AddAction {
        WorkspaceTab,
        LadderColumn
    };
    void triggerAddAction(AddAction action);
    void setLastAddAction(AddAction action);
    void updateAddButtonsToolTip();
    void centerActiveLaddersToSpread();
    void handleSettingsSearch();
    void handleSettingsSearchFromCompleter(const QString &value);
    struct SettingEntry {
        QString id;
        QString name;
        QStringList keywords;
    };
    const SettingEntry *matchSettingEntry(const QString &query) const;
    void openSettingEntry(const QString &id);
    void loadUserSettings();
    void saveUserSettings() const;
    QVector<VolumeHighlightRule> defaultVolumeHighlightRules() const;
    void applyVolumeRulesToAllDoms();
    void refreshActiveLadder();
    DomColumn *focusedDomColumn();
    void adjustVolumeRulesBySteps(int steps);
    QVector<SettingsWindow::HotkeyEntry> currentCustomHotkeys() const;
    void updateCustomHotkey(const QString &id, int key, Qt::KeyboardModifiers mods);
    static bool matchesHotkey(int eventKey,
                              Qt::KeyboardModifiers eventMods,
                              int key,
                              Qt::KeyboardModifiers mods);

    WorkspaceTab *currentWorkspaceTab();
    int findTabIndexById(int id) const;
    QIcon loadIcon(const QString &name) const;
    QIcon loadIconTinted(const QString &name, const QColor &color, const QSize &size) const;
    QString resolveAssetPath(const QString &relative) const;

    QString m_backendPath;
    QStringList m_symbols;
    int m_levels;

    QTabBar *m_workspaceTabs;
    QFrame *m_tabUnderline;
    QPropertyAnimation *m_tabUnderlineAnim;
    bool m_tabUnderlineHiddenForDrag;
    QStackedWidget *m_workspaceStack;
    QToolButton *m_addTabButton;
    QToolButton *m_addMenuButton;
    QLineEdit *m_settingsSearchEdit;
    QCompleter *m_settingsCompleter = nullptr;
    QLabel *m_connectionIndicator;
    QToolButton *m_connectionButton;
    QLabel *m_timeLabel;

    QLabel *m_statusLabel;
    QLabel *m_pingLabel;

    QWidget *m_orderPanel;
    QLabel *m_orderSymbolLabel;
    QLineEdit *m_orderPriceEdit;
    QLineEdit *m_orderQuantityEdit;
    QPushButton *m_buyButton;
    QPushButton *m_sellButton;

    PluginsWindow *m_pluginsWindow;
    SettingsWindow *m_settingsWindow;
    ConnectionStore *m_connectionStore;
    TradeManager *m_tradeManager;
    ConnectionsWindow *m_connectionsWindow;

    QVector<WorkspaceTab> m_tabs;
    int m_nextTabId;
    QVector<int> m_recycledTabIds;
    QIcon m_tabCloseIconNormal;
    QIcon m_tabCloseIconHover;
    AddAction m_lastAddAction;
    std::array<double, 5> m_defaultNotionalPresets{{1.0, 2.5, 5.0, 10.0, 25.0}};
    bool m_notionalEditActive = false;

    QTimer *m_timeTimer;
    QWidget *m_topBar;
    int m_timeOffsetMinutes = 0;
    // Window buttons (stored so we can update icon dynamically)
    QToolButton *m_minButton;
    QToolButton *m_maxButton;
    QToolButton *m_closeButton;

    bool m_nativeSnapEnabled = false;
    int m_resizeBorderWidth = 6; // resize area thickness in px

    // (no custom dragging state ? we use native system move/snap)
    QWidget *m_draggingDomContainer = nullptr;
    QPoint m_domDragStartGlobal;
    QPoint m_domDragStartWindowOffset;
    bool m_domDragActive = false;
    QWidget *m_domResizeContainer = nullptr;
    WorkspaceTab *m_domResizeTab = nullptr;
    QList<int> m_domResizeInitialSizes;
    QPoint m_domResizeStartPos;
    int m_domResizeSplitterIndex = -1;
    bool m_domResizeFromLeftEdge = false;
    bool m_domResizeActive = false;
    bool m_domResizePending = false;
    QWidget *m_domResizeHandle = nullptr;

    // Remember last normal (non-maximized) geometry so we can restore it
    // reliably after system-maximize (including Aero Snap) without ending up
    // with a clipped/offset window.
    QRect m_lastNormalGeometry;
    bool m_haveLastNormalGeometry = false;
    Qt::WindowStates m_prevWindowState = Qt::WindowNoState;

    // Hotkey: ????????????? ???????? ?? ??????.
    int m_centerKey = Qt::Key_Shift;
    Qt::KeyboardModifiers m_centerMods = Qt::NoModifier;
    bool m_centerAllLadders = true;
    QVector<SettingEntry> m_settingEntries;
    QVector<VolumeHighlightRule> m_volumeRules;
    bool m_capsAdjustMode = false;
    int m_newTabKey = Qt::Key_T;
    Qt::KeyboardModifiers m_newTabMods = Qt::ControlModifier;
    int m_addLadderKey = Qt::Key_E;
    Qt::KeyboardModifiers m_addLadderMods = Qt::ControlModifier;
    int m_refreshLadderKey = Qt::Key_R;
    Qt::KeyboardModifiers m_refreshLadderMods = Qt::ControlModifier;
    int m_volumeAdjustKey = Qt::Key_CapsLock;
    Qt::KeyboardModifiers m_volumeAdjustMods = Qt::NoModifier;
    std::array<int, 5> m_notionalPresetKeys{
        {Qt::Key_1, Qt::Key_2, Qt::Key_3, Qt::Key_4, Qt::Key_5}};
    std::array<Qt::KeyboardModifiers, 5> m_notionalPresetMods{
        {Qt::NoModifier, Qt::NoModifier, Qt::NoModifier, Qt::NoModifier, Qt::NoModifier}};
};
