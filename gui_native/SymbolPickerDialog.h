#pragma once

#include <QDialog>
#include <QStringList>
#include <QSet>
#include <QVector>
#include <QPair>
#include <QColor>

class QLineEdit;
class QListView;
class QStandardItemModel;
class QSortFilterProxyModel;
class QModelIndex;
class QComboBox;

class SymbolPickerDialog : public QDialog {
    Q_OBJECT

public:
    explicit SymbolPickerDialog(QWidget *parent = nullptr);

    void setSymbols(const QStringList &symbols, const QSet<QString> &apiOff);
    void setAccounts(const QVector<QPair<QString, QColor>> &accounts);
    void setCurrentSymbol(const QString &symbol);
    void setCurrentAccount(const QString &account);
    QString selectedSymbol() const;
    QString selectedAccount() const;

signals:
    void refreshRequested();

private slots:
    void handleFilterChanged(const QString &text);
    void handleActivated(const QModelIndex &index);

private:
    void acceptSelection();
    void selectFirstVisible();

    QLineEdit *m_filterEdit;
    QListView *m_listView;
    QStandardItemModel *m_model;
    QSortFilterProxyModel *m_proxy;
    QComboBox *m_accountCombo;
    QString m_selected;
    QString m_selectedAccount;
    QSet<QString> m_apiOff;
};
