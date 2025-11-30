#pragma once

#include <QDialog>
#include <QVector>
#include "DomTypes.h"

class QListWidget;
class QStackedWidget;
class QTableWidget;
class QCheckBox;

class SettingsWindow : public QDialog {
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget *parent = nullptr);

    struct HotkeyEntry {
        QString id;
        QString label;
        int key = 0;
        Qt::KeyboardModifiers mods = Qt::NoModifier;
    };

    void setCenterHotkey(int key, Qt::KeyboardModifiers mods, bool allLadders);
    void setCustomHotkeys(const QVector<HotkeyEntry> &entries);
    void setVolumeHighlightRules(const QVector<VolumeHighlightRule> &rules);
    void focusCenterHotkey();
    void focusVolumeHighlightRules();

signals:
    void centerHotkeyChanged(int key, Qt::KeyboardModifiers mods, bool allLadders);
    void volumeHighlightRulesChanged(const QVector<VolumeHighlightRule> &rules);
    void customHotkeyChanged(const QString &id, int key, Qt::KeyboardModifiers mods);

private:
    int addCategory(const QString &title, QWidget *page);
    void refreshVolumeRulesTable();
    void emitVolumeRulesChanged();
    void sortVolumeRules();
    void refreshHotkeysTable();

    QListWidget *m_categoryList;
    QStackedWidget *m_pages;

    QTableWidget *m_pluginsTable;
    QTableWidget *m_volumeRulesTable;
    int m_hotkeysCategoryIndex = -1;
    int m_tradingCategoryIndex = -1;

    int m_centerKey = Qt::Key_Shift;
    Qt::KeyboardModifiers m_centerMods = Qt::NoModifier;
    bool m_centerAllLadders = true;

    QVector<VolumeHighlightRule> m_volumeRules;
    bool m_updatingVolumeTable = false;
    QVector<HotkeyEntry> m_hotkeyEntries;
};
