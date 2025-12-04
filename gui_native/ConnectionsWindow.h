#pragma once

#include <QDialog>
#include "TradeManager.h"
#include <QVector>
#include <QColor>

class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class ConnectionStore;
class QVBoxLayout;
class QToolButton;

class ConnectionsWindow : public QDialog {
    Q_OBJECT

public:
    ConnectionsWindow(ConnectionStore *store, TradeManager *manager, QWidget *parent = nullptr);

    void refreshUi();

public slots:
    void handleManagerStateChanged(TradeManager::ConnectionState state, const QString &message);
    void appendLogMessage(const QString &message);

private slots:
    void handleConnectClicked(const QString &id);
    void handleDisconnectClicked();

private:
    struct CardWidgets {
        QString id;
        QLabel *statusBadge = nullptr;
        QLineEdit *apiKeyEdit = nullptr;
        QLineEdit *secretEdit = nullptr;
        QLineEdit *passphraseEdit = nullptr;
        QLineEdit *uidEdit = nullptr;
        QLineEdit *proxyEdit = nullptr;
        QCheckBox *saveSecretCheck = nullptr;
        QCheckBox *viewOnlyCheck = nullptr;
        QCheckBox *autoConnectCheck = nullptr;
        QToolButton *colorButton = nullptr;
        QToolButton *expandButton = nullptr;
        QPushButton *connectButton = nullptr;
        QPushButton *disconnectButton = nullptr;
        QColor color;
        QWidget *body = nullptr;
        QWidget *frame = nullptr;
        bool expanded = true;
    };

    CardWidgets *createCard(const QString &id);
    CardWidgets *ensureCard(const QString &id);
    void applyState(TradeManager::ConnectionState state, const QString &message);
    MexcCredentials collectCredentials(const CardWidgets &card) const;
    void persistCard(const CardWidgets &card);
    void setCardExpanded(CardWidgets *card, bool expanded);
    void moveCard(CardWidgets *card, int delta);
    void clearCard(CardWidgets *card);
    void rebuildLayout();

    ConnectionStore *m_store = nullptr;
    TradeManager *m_manager = nullptr;

    QVector<CardWidgets *> m_cards;
    QVBoxLayout *m_cardsLayout = nullptr;
    QWidget *m_cardsContainer = nullptr;
    QString m_activeId;
    QPlainTextEdit *m_logView = nullptr;
};
