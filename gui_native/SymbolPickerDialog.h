#pragma once

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QListView;
class QStringListModel;
class QSortFilterProxyModel;
class QModelIndex;

class SymbolPickerDialog : public QDialog {
    Q_OBJECT

public:
    explicit SymbolPickerDialog(QWidget *parent = nullptr);

    void setSymbols(const QStringList &symbols);
    void setCurrentSymbol(const QString &symbol);
    QString selectedSymbol() const;

private slots:
    void handleFilterChanged(const QString &text);
    void handleActivated(const QModelIndex &index);

private:
    void acceptSelection();
    void selectFirstVisible();

    QLineEdit *m_filterEdit;
    QListView *m_listView;
    QStringListModel *m_model;
    QSortFilterProxyModel *m_proxy;
    QString m_selected;
};
