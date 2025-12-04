#include "ConnectionsWindow.h"

#include "ConnectionStore.h"

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QDateTime>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QTime>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QString statusColor(TradeManager::ConnectionState state)
{
    switch (state) {
    case TradeManager::ConnectionState::Connected:
        return QStringLiteral("#2e7d32");
    case TradeManager::ConnectionState::Connecting:
        return QStringLiteral("#f9a825");
    case TradeManager::ConnectionState::Error:
        return QStringLiteral("#c62828");
    case TradeManager::ConnectionState::Disconnected:
    default:
        return QStringLiteral("#616161");
    }
}

QString profileTitle(const QString &id)
{
    if (id == QStringLiteral("mexcFutures")) {
        return QObject::tr("MEXC Futures");
    }
    if (id == QStringLiteral("uzxSwap")) {
        return QObject::tr("UZX Swap");
    }
    if (id == QStringLiteral("uzxSpot")) {
        return QObject::tr("UZX Spot");
    }
    return QObject::tr("MEXC Spot");
}

ConnectionStore::Profile profileFromId(const QString &id)
{
    if (id == QStringLiteral("mexcFutures")) {
        return ConnectionStore::Profile::MexcFutures;
    }
    if (id == QStringLiteral("uzxSwap")) {
        return ConnectionStore::Profile::UzxSwap;
    }
    if (id == QStringLiteral("uzxSpot")) {
        return ConnectionStore::Profile::UzxSpot;
    }
    return ConnectionStore::Profile::MexcSpot;
}

QString idFromProfile(ConnectionStore::Profile profile)
{
    switch (profile) {
    case ConnectionStore::Profile::MexcFutures:
        return QStringLiteral("mexcFutures");
    case ConnectionStore::Profile::UzxSwap:
        return QStringLiteral("uzxSwap");
    case ConnectionStore::Profile::UzxSpot:
        return QStringLiteral("uzxSpot");
    case ConnectionStore::Profile::MexcSpot:
    default:
        return QStringLiteral("mexcSpot");
    }
}

QString defaultColorForId(const QString &id)
{
    if (id == QStringLiteral("mexcFutures")) {
        return QStringLiteral("#f5b642");
    }
    if (id == QStringLiteral("uzxSwap")) {
        return QStringLiteral("#ff7f50");
    }
    if (id == QStringLiteral("uzxSpot")) {
        return QStringLiteral("#8bc34a");
    }
    return QStringLiteral("#4c9fff");
}

QString badgeStyle(const QString &color)
{
    return QStringLiteral(
        "QLabel#ConnectionStatusBadge {"
        "  border-radius: 10px;"
        "  padding: 4px 12px;"
        "  color: #ffffff;"
        "  background: %1;"
        "}").arg(color);
}

void styleColorButton(QToolButton *btn, const QColor &c)
{
    if (!btn) return;
    const QString color = c.isValid() ? c.name(QColor::HexRgb) : QStringLiteral("#999999");
    btn->setStyleSheet(
        QStringLiteral("QToolButton { background:%1; border:1px solid #3a3a3a; border-radius:4px; min-width:20px; min-height:20px; }")
            .arg(color));
}
} // namespace

ConnectionsWindow::ConnectionsWindow(ConnectionStore *store, TradeManager *manager, QWidget *parent)
    : QDialog(parent)
    , m_store(store)
    , m_manager(manager)
{
    setWindowTitle(tr("Connections"));
    setModal(false);
    setWindowFlag(Qt::Window);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMinimumWidth(520);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *header = new QHBoxLayout();
    auto *titleLabel = new QLabel(tr("Подключения"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 1.5);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    header->addWidget(titleLabel);
    header->addStretch(1);

    auto *addButton = new QToolButton(this);
    addButton->setText(QStringLiteral("+ Добавить подключение"));
    addButton->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(addButton);
    menu->addAction(tr("MEXC Spot"), this, [this]() { ensureCard(QStringLiteral("mexcSpot")); });
    menu->addAction(tr("MEXC Futures"), this, [this]() { ensureCard(QStringLiteral("mexcFutures")); });
    menu->addAction(tr("UZX Swap"), this, [this]() { ensureCard(QStringLiteral("uzxSwap")); });
    menu->addAction(tr("UZX Spot"), this, [this]() { ensureCard(QStringLiteral("uzxSpot")); });
    addButton->setMenu(menu);
    header->addWidget(addButton);
    layout->addLayout(header);

    m_cardsContainer = new QWidget(this);
    m_cardsLayout = new QVBoxLayout(m_cardsContainer);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(10);
    layout->addWidget(m_cardsContainer);
    layout->addStretch(1);

    auto *logLabel = new QLabel(tr("Информация"), this);
    layout->addWidget(logLabel);
    m_logView = new QPlainTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(500);
    layout->addWidget(m_logView, 1);

    ensureCard(QStringLiteral("mexcSpot"));
    ensureCard(QStringLiteral("mexcFutures"));
    ensureCard(QStringLiteral("uzxSwap"));
    ensureCard(QStringLiteral("uzxSpot"));

    if (m_manager) {
        connect(m_manager,
                &TradeManager::connectionStateChanged,
                this,
                &ConnectionsWindow::handleManagerStateChanged);
        connect(m_manager, &TradeManager::logMessage, this, &ConnectionsWindow::appendLogMessage);
    }

    refreshUi();
}

ConnectionsWindow::CardWidgets *ConnectionsWindow::createCard(const QString &id)
{
    auto *card = new CardWidgets();
    card->id = id;
    card->color = QColor(defaultColorForId(id));

    auto *frame = new QFrame(this);
    card->frame = frame;
    frame->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    frame->setStyleSheet(QStringLiteral(
        "QFrame { border:1px solid #2f2f2f; border-radius:8px; background:#1b1b1b; }"));

    auto *v = new QVBoxLayout(frame);
    v->setContentsMargins(10, 8, 10, 10);
    v->setSpacing(6);

    auto *top = new QHBoxLayout();
    card->expandButton = new QToolButton(frame);
    card->expandButton->setCheckable(true);
    card->expandButton->setChecked(true);
    card->expandButton->setText(QStringLiteral("▼"));
    card->expandButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    card->expandButton->setFixedWidth(22);
    top->addWidget(card->expandButton);

    card->colorButton = new QToolButton(frame);
    card->colorButton->setAutoRaise(false);
    card->colorButton->setToolTip(tr("Выбрать цвет аккаунта"));
    styleColorButton(card->colorButton, card->color);
    top->addWidget(card->colorButton);

    auto *title = new QLabel(profileTitle(id), frame);
    QFont f = title->font();
    f.setBold(true);
    title->setFont(f);
    top->addWidget(title);
    top->addStretch(1);

    card->statusBadge = new QLabel(tr("Отключено"), frame);
    card->statusBadge->setObjectName(QStringLiteral("ConnectionStatusBadge"));
    card->statusBadge->setAlignment(Qt::AlignCenter);
    card->statusBadge->setMinimumWidth(110);
    card->statusBadge->setStyleSheet(badgeStyle(statusColor(TradeManager::ConnectionState::Disconnected)));
    top->addWidget(card->statusBadge);

    auto *moveUp = new QToolButton(frame);
    moveUp->setToolTip(tr("Выше"));
    moveUp->setText(QStringLiteral("↑"));
    auto *moveDown = new QToolButton(frame);
    moveDown->setToolTip(tr("Ниже"));
    moveDown->setText(QStringLiteral("↓"));
    auto *removeBtn = new QToolButton(frame);
    removeBtn->setToolTip(tr("Удалить подключение"));
    removeBtn->setText(QStringLiteral("✕"));
    top->addWidget(moveUp);
    top->addWidget(moveDown);
    top->addWidget(removeBtn);
    v->addLayout(top);

    card->body = new QWidget(frame);
    auto *bodyLayout = new QVBoxLayout(card->body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(6);

    card->apiKeyEdit = new QLineEdit(card->body);
    card->apiKeyEdit->setPlaceholderText(tr("API key"));
    card->secretEdit = new QLineEdit(card->body);
    card->secretEdit->setPlaceholderText(tr("API secret"));
    card->secretEdit->setEchoMode(QLineEdit::Password);
    card->passphraseEdit = new QLineEdit(card->body);
    card->passphraseEdit->setPlaceholderText(tr("Passphrase (UZX)"));
    card->uidEdit = new QLineEdit(card->body);
        card->uidEdit->setPlaceholderText(tr("U_ID (optional)"));
    card->proxyEdit = new QLineEdit(card->body);
    card->proxyEdit->setPlaceholderText(tr("Прокси (http://user:pass@host:port)"));

    bodyLayout->addWidget(card->apiKeyEdit);
    bodyLayout->addWidget(card->secretEdit);
    bodyLayout->addWidget(card->passphraseEdit);
    bodyLayout->addWidget(card->uidEdit);
    bodyLayout->addWidget(card->proxyEdit);

    auto *options = new QHBoxLayout();
    options->setSpacing(12);
    card->saveSecretCheck = new QCheckBox(tr("Сохранить secret"), card->body);
    card->viewOnlyCheck = new QCheckBox(tr("Только просмотр"), card->body);
    card->autoConnectCheck = new QCheckBox(tr("Автоподключение"), card->body);
    options->addWidget(card->saveSecretCheck);
    options->addWidget(card->viewOnlyCheck);
    options->addWidget(card->autoConnectCheck);
    options->addStretch(1);
    bodyLayout->addLayout(options);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch(1);
    card->disconnectButton = new QPushButton(tr("Отключить"), card->body);
    card->connectButton = new QPushButton(tr("Подключить"), card->body);
    buttons->addWidget(card->disconnectButton);
    buttons->addWidget(card->connectButton);
    bodyLayout->addLayout(buttons);

    v->addWidget(card->body);

    m_cards.push_back(card);
    rebuildLayout();

    connect(card->connectButton, &QPushButton::clicked, this, [this, id]() {
        handleConnectClicked(id);
    });
    connect(card->disconnectButton, &QPushButton::clicked, this, [this, id]() {
        handleDisconnectClicked(id);
    });
    connect(card->expandButton, &QToolButton::clicked, this, [this, card]() {
        setCardExpanded(card, !card->expanded);
    });
    connect(card->colorButton, &QToolButton::clicked, this, [this, card]() {
        const QColor picked = QColorDialog::getColor(card->color, this, tr("Выбери цвет аккаунта"));
        if (picked.isValid()) {
            card->color = picked;
            styleColorButton(card->colorButton, picked);
            persistCard(*card);
        }
    });
    connect(moveUp, &QToolButton::clicked, this, [this, card]() { moveCard(card, -1); });
    connect(moveDown, &QToolButton::clicked, this, [this, card]() { moveCard(card, 1); });
    connect(removeBtn, &QToolButton::clicked, this, [this, card]() { clearCard(card); });

    auto registerPersist = [this, card]() { persistCard(*card); };
    connect(card->saveSecretCheck, &QCheckBox::toggled, this, registerPersist);
    connect(card->viewOnlyCheck, &QCheckBox::toggled, this, registerPersist);
    connect(card->autoConnectCheck, &QCheckBox::toggled, this, registerPersist);
    connect(card->apiKeyEdit, &QLineEdit::textChanged, this, registerPersist);
    connect(card->secretEdit, &QLineEdit::textChanged, this, registerPersist);
    connect(card->passphraseEdit, &QLineEdit::textChanged, this, registerPersist);
    connect(card->uidEdit, &QLineEdit::textChanged, this, registerPersist);
    connect(card->proxyEdit, &QLineEdit::textChanged, this, registerPersist);

    const bool isUZX = id.startsWith(QStringLiteral("uzx"));
    card->passphraseEdit->setVisible(isUZX);
    card->uidEdit->setVisible(!isUZX);

    return card;
}

ConnectionsWindow::CardWidgets *ConnectionsWindow::ensureCard(const QString &id)
{
    for (auto *c : m_cards) {
        if (c->id == id) {
            return c;
        }
    }
    return createCard(id);
}

void ConnectionsWindow::refreshUi()
{
    int order = 0;
    for (auto *card : m_cards) {
        MexcCredentials creds = m_store ? m_store->loadMexcCredentials(profileFromId(card->id))
                                        : MexcCredentials{};
        if (creds.colorHex.isEmpty()) {
            creds.colorHex = defaultColorForId(card->id);
        }
        const bool isUZX = card->id.startsWith(QStringLiteral("uzx"));
        card->passphraseEdit->setVisible(isUZX);
        card->uidEdit->setVisible(!isUZX);
        card->apiKeyEdit->setText(creds.apiKey);
        card->secretEdit->setText(creds.saveSecret ? creds.secretKey : QString());
        card->passphraseEdit->setText(isUZX ? creds.passphrase : QString());
        card->uidEdit->setText(isUZX ? QString() : creds.uid);
        card->proxyEdit->setText(creds.proxy);
        card->saveSecretCheck->setChecked(creds.saveSecret);
        card->viewOnlyCheck->setChecked(creds.viewOnly);
        card->autoConnectCheck->setChecked(creds.autoConnect);
        card->color = QColor(creds.colorHex);
        styleColorButton(card->colorButton, card->color);
        card->statusBadge->setText(tr("Отключено"));
        card->statusBadge->setStyleSheet(
            badgeStyle(statusColor(TradeManager::ConnectionState::Disconnected)));
        setCardExpanded(card, order < 2);
        order++;
    }

    auto applyProfileState = [&](ConnectionStore::Profile profile) {
        const auto state = m_manager ? m_manager->state(profile)
                                     : TradeManager::ConnectionState::Disconnected;
        applyState(profile, state, QString());
    };
    applyProfileState(ConnectionStore::Profile::MexcSpot);
    applyProfileState(ConnectionStore::Profile::MexcFutures);
    applyProfileState(ConnectionStore::Profile::UzxSwap);
    applyProfileState(ConnectionStore::Profile::UzxSpot);
    rebuildLayout();
}

void ConnectionsWindow::handleManagerStateChanged(ConnectionStore::Profile profile,
                                                  TradeManager::ConnectionState state,
                                                  const QString &message)
{
    applyState(profile, state, message);
    if (!message.isEmpty()) {
        appendLogMessage(message);
    }
}

void ConnectionsWindow::appendLogMessage(const QString &message)
{
    if (!m_logView) {
        return;
    }
    const QString line = QStringLiteral("[%1] %2")
                             .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")))
                             .arg(message);
    m_logView->appendPlainText(line);
    QTextCursor cursor = m_logView->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logView->setTextCursor(cursor);
}

void ConnectionsWindow::handleConnectClicked(const QString &id)
{
    CardWidgets *card = ensureCard(id);
    if (!card || !m_manager) {
        return;
    }
    MexcCredentials creds = collectCredentials(*card);
    const auto profile = profileFromId(id);
    if (m_store) {
        m_store->saveMexcCredentials(creds, profile);
    }
    m_manager->setCredentials(profile, creds);
    m_manager->connectToExchange(profile);
    applyState(profile, TradeManager::ConnectionState::Connecting, QString());
}

void ConnectionsWindow::handleDisconnectClicked(const QString &id)
{
    if (!m_manager) {
        return;
    }
    const auto profile = profileFromId(id);
    m_manager->disconnect(profile);
    applyState(profile, TradeManager::ConnectionState::Disconnected, QString());
}

void ConnectionsWindow::applyState(ConnectionStore::Profile profile,
                                   TradeManager::ConnectionState state,
                                   const QString &message)
{
    Q_UNUSED(message);
    CardWidgets *card = ensureCard(idFromProfile(profile));
    if (!card) {
        return;
    }
    card->currentState = state;
    const QString color = statusColor(state);
    QString text;
    switch (state) {
    case TradeManager::ConnectionState::Connected:
        text = tr("Подключено");
        break;
    case TradeManager::ConnectionState::Connecting:
        text = tr("Подключение...");
        break;
    case TradeManager::ConnectionState::Error:
        text = tr("Ошибка");
        break;
    case TradeManager::ConnectionState::Disconnected:
    default:
        text = tr("Отключено");
        break;
    }
    card->statusBadge->setText(text);
    card->statusBadge->setStyleSheet(badgeStyle(color));

    const bool connecting = state == TradeManager::ConnectionState::Connecting;
    const bool connected = state == TradeManager::ConnectionState::Connected;
    if (card->connectButton) {
        card->connectButton->setEnabled(!connecting);
    }
    if (card->disconnectButton) {
        card->disconnectButton->setEnabled(connecting || connected);
    }
}

MexcCredentials ConnectionsWindow::collectCredentials(const CardWidgets &card) const
{
    MexcCredentials creds;
    creds.apiKey = card.apiKeyEdit->text().trimmed();
    creds.secretKey = card.secretEdit->text().trimmed();
    const bool isUZX = card.id.startsWith(QStringLiteral("uzx"));
    creds.passphrase = isUZX && card.passphraseEdit ? card.passphraseEdit->text().trimmed() : QString();
    creds.uid = isUZX ? QString() : card.uidEdit->text().trimmed();
    creds.proxy = card.proxyEdit->text().trimmed();
    creds.colorHex = card.color.isValid() ? card.color.name(QColor::HexRgb) : defaultColorForId(card.id);
    creds.label = profileTitle(card.id);
    creds.saveSecret = card.saveSecretCheck->isChecked();
    creds.viewOnly = card.viewOnlyCheck->isChecked();
    creds.autoConnect = card.autoConnectCheck->isChecked();
    return creds;
}

void ConnectionsWindow::persistCard(const CardWidgets &card)
{
    if (!m_store) {
        return;
    }
    MexcCredentials creds = collectCredentials(card);
    m_store->saveMexcCredentials(creds, profileFromId(card.id));
}

void ConnectionsWindow::setCardExpanded(CardWidgets *card, bool expanded)
{
    if (!card || !card->body || !card->expandButton) {
        return;
    }
    card->expanded = expanded;
    card->body->setVisible(expanded);
    card->expandButton->setText(expanded ? QStringLiteral("▼") : QStringLiteral("►"));
}

void ConnectionsWindow::moveCard(CardWidgets *card, int delta)
{
    if (!card) return;
    const int idx = m_cards.indexOf(card);
    if (idx < 0) return;
    const int newIdx = idx + delta;
    if (newIdx < 0 || newIdx >= m_cards.size()) return;
    m_cards.move(idx, newIdx);
    rebuildLayout();
}

void ConnectionsWindow::clearCard(CardWidgets *card)
{
    if (!card) return;
    card->apiKeyEdit->clear();
    card->secretEdit->clear();
    if (card->passphraseEdit) {
        card->passphraseEdit->clear();
    }
    card->uidEdit->clear();
    card->proxyEdit->clear();
    card->saveSecretCheck->setChecked(false);
    card->viewOnlyCheck->setChecked(false);
    card->autoConnectCheck->setChecked(true);
    persistCard(*card);
    applyState(profileFromId(card->id),
               TradeManager::ConnectionState::Disconnected,
               QString());
}

void ConnectionsWindow::rebuildLayout()
{
    if (!m_cardsLayout) return;
    QLayoutItem *child;
    while ((child = m_cardsLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->setParent(nullptr);
        }
        delete child;
    }
    for (auto *card : m_cards) {
        if (card && card->frame) {
            m_cardsLayout->addWidget(card->frame);
        }
    }
    m_cardsLayout->addStretch(1);
}
