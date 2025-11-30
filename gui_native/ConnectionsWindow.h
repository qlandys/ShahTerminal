#pragma once

#include <QDialog>
#include "TradeManager.h"

class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class ConnectionStore;

class ConnectionsWindow : public QDialog {
    Q_OBJECT

public:
    ConnectionsWindow(ConnectionStore *store, TradeManager *manager, QWidget *parent = nullptr);

    void refreshUi();

public slots:
    void handleManagerStateChanged(TradeManager::ConnectionState state, const QString &message);
    void appendLogMessage(const QString &message);

private slots:
    void handleConnectClicked();
    void handleDisconnectClicked();

private:
    void applyState(TradeManager::ConnectionState state, const QString &message);
    MexcCredentials collectCredentials() const;

    ConnectionStore *m_store = nullptr;
    TradeManager *m_manager = nullptr;

    QLineEdit *m_apiKeyEdit = nullptr;
    QLineEdit *m_secretEdit = nullptr;
    QCheckBox *m_saveSecretCheck = nullptr;
    QCheckBox *m_viewOnlyCheck = nullptr;
    QCheckBox *m_autoConnectCheck = nullptr;
    QLabel *m_statusBadge = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_disconnectButton = nullptr;
    QPlainTextEdit *m_logView = nullptr;
};
