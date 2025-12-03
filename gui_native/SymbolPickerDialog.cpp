#include "SymbolPickerDialog.h"

#include <QDialogButtonBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QModelIndex>
#include <QComboBox>
#include <QToolButton>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QAbstractItemView>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>
#include <QStyle>
#include <QColor>
#include <QPainter>
#include <algorithm>

namespace {
QString resolveAssetPath(const QString &relative)
{
    const QString rel = QDir::fromNativeSeparators(relative);
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList bases = {
        appDir,
        QDir(appDir).filePath(QStringLiteral("img")),
        QDir(appDir).filePath(QStringLiteral("img/icons")),
        QDir(appDir).filePath(QStringLiteral("img/icons/outline")),
        QDir(appDir).filePath(QStringLiteral("../img")),
        QDir(appDir).filePath(QStringLiteral("../img/icons")),
        QDir(appDir).filePath(QStringLiteral("../img/icons/outline"))
    };
    for (const QString &base : bases) {
        const QString candidate = QDir(base).filePath(rel);
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }
    return QString();
}
} // namespace

SymbolPickerDialog::SymbolPickerDialog(QWidget *parent)
    : QDialog(parent)
    , m_filterEdit(new QLineEdit(this))
    , m_listView(new QListView(this))
    , m_model(new QStandardItemModel(this))
    , m_proxy(new QSortFilterProxyModel(this))
{
    setWindowTitle(tr("Select symbol"));
    setModal(false);
    setWindowModality(Qt::NonModal);
    setMinimumSize(320, 360);

    auto *layout = new QVBoxLayout(this);
    auto *label = new QLabel(tr("Symbol"), this);
    layout->addWidget(label);

    m_filterEdit->setPlaceholderText(tr("Search..."));
    layout->addWidget(m_filterEdit);

    m_accountCombo = new QComboBox(this);
    layout->addWidget(m_accountCombo);

    auto *refreshButton = new QToolButton(this);
    refreshButton->setAutoRaise(true);
    refreshButton->setToolTip(tr("Refresh symbols list from exchange"));
    refreshButton->setCursor(Qt::PointingHandCursor);
    const QString refreshPath = resolveAssetPath(QStringLiteral("icons/outline/refresh.svg"));
    if (!refreshPath.isEmpty()) {
        QIcon ico(refreshPath);
        refreshButton->setIcon(ico);
        refreshButton->setIconSize(QSize(16, 16));
    } else {
        refreshButton->setText(QStringLiteral("↻"));
    }
    layout->addWidget(refreshButton);

    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterRole(Qt::DisplayRole);
    m_proxy->sort(0);

    m_listView->setModel(m_proxy);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_listView->setIconSize(QSize(16, 16));
    m_listView->setUniformItemSizes(true);
    m_listView->setSpacing(2);
    layout->addWidget(m_listView, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &SymbolPickerDialog::acceptSelection);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &SymbolPickerDialog::handleFilterChanged);
    connect(m_filterEdit, &QLineEdit::returnPressed, this, &SymbolPickerDialog::acceptSelection);
    connect(m_listView, &QListView::doubleClicked, this, &SymbolPickerDialog::handleActivated);
    connect(refreshButton, &QToolButton::clicked, this, &SymbolPickerDialog::refreshRequested);
}

void SymbolPickerDialog::setSymbols(const QStringList &symbols, const QSet<QString> &apiOff)
{
    m_apiOff = apiOff;
    QStringList cleaned;
    cleaned.reserve(symbols.size());
    for (const QString &sym : symbols) {
        const QString s = sym.trimmed().toUpper();
        if (s.isEmpty() || cleaned.contains(s, Qt::CaseInsensitive)) {
            continue;
        }
        cleaned.push_back(s);
    }
    std::sort(cleaned.begin(), cleaned.end(), [](const QString &a, const QString &b) {
        return a.toUpper() < b.toUpper();
    });
    m_model->clear();
    for (const QString &sym : cleaned) {
        auto *item = new QStandardItem(sym);
        item->setEditable(false);
        if (m_apiOff.contains(sym)) {
            const QString apiOffPath = resolveAssetPath(QStringLiteral("icons/outline/api-off.svg"));
            if (!apiOffPath.isEmpty()) {
                item->setIcon(QIcon(apiOffPath));
            }
            item->setToolTip(tr("Symbol not supported for API trading"));
            item->setForeground(QColor(QStringLiteral("#f26b6b")));
        }
        m_model->appendRow(item);
    }
    m_proxy->invalidate();
    m_proxy->sort(0);
    selectFirstVisible();
}

void SymbolPickerDialog::setAccounts(const QVector<QPair<QString, QColor>> &accounts)
{
    m_accountCombo->clear();
    if (accounts.isEmpty()) {
        m_accountCombo->addItem(QStringLiteral("MEXC Spot"));
        return;
    }
    for (const auto &pair : accounts) {
        const QString name = pair.first;
        const QColor color = pair.second;
        QPixmap px(14, 14);
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(color.isValid() ? color : QColor("#4c9fff"));
        p.setPen(Qt::NoPen);
        p.drawEllipse(px.rect().adjusted(1, 1, -1, -1));
        p.end();
        m_accountCombo->addItem(QIcon(px), name);
    }
}

void SymbolPickerDialog::setCurrentSymbol(const QString &symbol)
{
    const QString target = symbol.trimmed().toUpper();
    if (target.isEmpty()) {
        selectFirstVisible();
        return;
    }
    int sourceRow = -1;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        const QString val = m_model->item(i)->text();
        if (val.compare(target, Qt::CaseInsensitive) == 0) {
            sourceRow = i;
            break;
        }
    }
    if (sourceRow < 0) {
        selectFirstVisible();
        return;
    }
    const QModelIndex srcIdx = m_model->index(sourceRow, 0);
    const QModelIndex proxyIdx = m_proxy->mapFromSource(srcIdx);
    if (proxyIdx.isValid()) {
        m_listView->setCurrentIndex(proxyIdx);
        m_listView->scrollTo(proxyIdx, QAbstractItemView::PositionAtCenter);
    } else {
        selectFirstVisible();
    }
}

void SymbolPickerDialog::setCurrentAccount(const QString &account)
{
    const int idx = m_accountCombo->findText(account, Qt::MatchFixedString);
    if (idx >= 0) {
        m_accountCombo->setCurrentIndex(idx);
    }
}

QString SymbolPickerDialog::selectedSymbol() const
{
    return m_selected;
}

QString SymbolPickerDialog::selectedAccount() const
{
    return m_selectedAccount;
}

void SymbolPickerDialog::handleFilterChanged(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        m_proxy->setFilterRegularExpression(QRegularExpression());
    } else {
        const QString pattern = QRegularExpression::escape(trimmed);
        m_proxy->setFilterRegularExpression(
            QRegularExpression(QStringLiteral(".*%1.*").arg(pattern), QRegularExpression::CaseInsensitiveOption));
    }
    selectFirstVisible();
}

void SymbolPickerDialog::handleActivated(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    m_selected = index.data().toString().trimmed().toUpper();
    accept();
}

void SymbolPickerDialog::acceptSelection()
{
    QModelIndex idx = m_listView->currentIndex();
    if (!idx.isValid() && m_proxy->rowCount() > 0) {
        idx = m_proxy->index(0, 0);
    }
    if (idx.isValid()) {
        m_selected = idx.data().toString().trimmed().toUpper();
    } else {
        m_selected.clear();
    }
    m_selectedAccount = m_accountCombo->currentText();
    accept();
}

void SymbolPickerDialog::selectFirstVisible()
{
    const QModelIndex idx = m_proxy->index(0, 0);
    if (idx.isValid()) {
        m_listView->setCurrentIndex(idx);
    }
}
