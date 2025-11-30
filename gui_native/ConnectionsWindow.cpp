#include "ConnectionsWindow.h"

#include "ConnectionStore.h"

#include <QCheckBox>
#include <QDateTime>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QTime>
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
    setMinimumWidth(460);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *headerLayout = new QHBoxLayout();
    auto *titleLabel = new QLabel(tr("MEXC Spot"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 1.0);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch(1);

    m_statusBadge = new QLabel(tr("Disconnected"), this);
    m_statusBadge->setObjectName(QStringLiteral("ConnectionStatusBadge"));
    m_statusBadge->setAlignment(Qt::AlignCenter);
    m_statusBadge->setMinimumWidth(120);
    m_statusBadge->setStyleSheet(QStringLiteral(
        "QLabel#ConnectionStatusBadge {"
        "  border-radius: 10px;"
        "  padding: 4px 12px;"
        "  color: #ffffff;"
        "  background: #616161;"
        "}"));
    headerLayout->addWidget(m_statusBadge);
    layout->addLayout(headerLayout);

    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setPlaceholderText(tr("Paste your API key"));
    m_secretEdit = new QLineEdit(this);
    m_secretEdit->setPlaceholderText(tr("Paste your API secret"));
    m_secretEdit->setEchoMode(QLineEdit::Password);

    layout->addWidget(new QLabel(tr("API key"), this));
    layout->addWidget(m_apiKeyEdit);
    layout->addWidget(new QLabel(tr("API secret"), this));
    layout->addWidget(m_secretEdit);

    auto *optionsLayout = new QHBoxLayout();
    optionsLayout->setSpacing(18);
    m_saveSecretCheck = new QCheckBox(tr("Save secret"), this);
    m_viewOnlyCheck = new QCheckBox(tr("View only"), this);
    m_autoConnectCheck = new QCheckBox(tr("Auto connect"), this);
    optionsLayout->addWidget(m_saveSecretCheck);
    optionsLayout->addWidget(m_viewOnlyCheck);
    optionsLayout->addWidget(m_autoConnectCheck);
    optionsLayout->addStretch(1);
    layout->addLayout(optionsLayout);

    auto *buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(12);
    m_connectButton = new QPushButton(tr("Connect"), this);
    m_disconnectButton = new QPushButton(tr("Disconnect"), this);
    buttonsLayout->addStretch(1);
    buttonsLayout->addWidget(m_disconnectButton);
    buttonsLayout->addWidget(m_connectButton);
    layout->addLayout(buttonsLayout);

    auto *logLabel = new QLabel(tr("Information"), this);
    layout->addWidget(logLabel);
    m_logView = new QPlainTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(500);
    layout->addWidget(m_logView, 1);

    connect(m_connectButton, &QPushButton::clicked, this, &ConnectionsWindow::handleConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &ConnectionsWindow::handleDisconnectClicked);

    if (m_manager) {
        connect(m_manager,
                &TradeManager::connectionStateChanged,
                this,
                &ConnectionsWindow::handleManagerStateChanged);
        connect(m_manager, &TradeManager::logMessage, this, &ConnectionsWindow::appendLogMessage);
    }

    refreshUi();
}

void ConnectionsWindow::refreshUi()
{
    MexcCredentials creds;
    if (m_manager) {
        creds = m_manager->credentials();
    }
    if (creds.apiKey.isEmpty() && m_store) {
        creds = m_store->loadMexcCredentials();
    }

    if (m_apiKeyEdit) {
        m_apiKeyEdit->setText(creds.apiKey);
    }
    if (m_secretEdit) {
        if (creds.saveSecret) {
            m_secretEdit->setText(creds.secretKey);
        } else {
            m_secretEdit->clear();
        }
    }
    if (m_saveSecretCheck) {
        m_saveSecretCheck->setChecked(creds.saveSecret);
    }
    if (m_viewOnlyCheck) {
        m_viewOnlyCheck->setChecked(creds.viewOnly);
    }
    if (m_autoConnectCheck) {
        m_autoConnectCheck->setChecked(creds.autoConnect);
    }
    applyState(m_manager ? m_manager->state() : TradeManager::ConnectionState::Disconnected, QString());
}

void ConnectionsWindow::handleManagerStateChanged(TradeManager::ConnectionState state,
                                                  const QString &message)
{
    applyState(state, message);
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

void ConnectionsWindow::handleConnectClicked()
{
    if (!m_manager) {
        return;
    }
    MexcCredentials creds = collectCredentials();
    if (m_store) {
        MexcCredentials persisted = creds;
        if (!persisted.saveSecret) {
            persisted.secretKey.clear();
        }
        m_store->saveMexcCredentials(persisted);
    }
    m_manager->setCredentials(creds);
    m_manager->connectToExchange();
}

void ConnectionsWindow::handleDisconnectClicked()
{
    if (m_manager) {
        m_manager->disconnect();
    }
}

void ConnectionsWindow::applyState(TradeManager::ConnectionState state, const QString &message)
{
    if (!m_statusBadge) {
        return;
    }
    QString text;
    switch (state) {
    case TradeManager::ConnectionState::Connected:
        text = tr("Connected");
        break;
    case TradeManager::ConnectionState::Connecting:
        text = tr("Connecting...");
        break;
    case TradeManager::ConnectionState::Error:
        text = tr("Error");
        break;
    case TradeManager::ConnectionState::Disconnected:
    default:
        text = tr("Disconnected");
        break;
    }
    m_statusBadge->setText(text);
    const QString style = QStringLiteral(
        "QLabel#ConnectionStatusBadge {"
        "  border-radius: 10px;"
        "  padding: 4px 12px;"
        "  color: #ffffff;"
        "  background: %1;"
        "}").arg(statusColor(state));
    m_statusBadge->setStyleSheet(style);

    const bool connecting = state == TradeManager::ConnectionState::Connecting;
    const bool connected = state == TradeManager::ConnectionState::Connected;
    if (m_connectButton) {
        m_connectButton->setEnabled(!connecting);
    }
    if (m_disconnectButton) {
        m_disconnectButton->setEnabled(connecting || connected);
    }
    Q_UNUSED(message);
}

MexcCredentials ConnectionsWindow::collectCredentials() const
{
    MexcCredentials creds;
    if (m_apiKeyEdit) {
        creds.apiKey = m_apiKeyEdit->text().trimmed();
    }
    if (m_secretEdit) {
        creds.secretKey = m_secretEdit->text().trimmed();
    }
    if (m_saveSecretCheck) {
        creds.saveSecret = m_saveSecretCheck->isChecked();
    }
    if (m_viewOnlyCheck) {
        creds.viewOnly = m_viewOnlyCheck->isChecked();
    }
    if (m_autoConnectCheck) {
        creds.autoConnect = m_autoConnectCheck->isChecked();
    }
    return creds;
}
