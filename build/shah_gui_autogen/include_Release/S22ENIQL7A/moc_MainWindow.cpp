/****************************************************************************
** Meta object code from reading C++ file 'MainWindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../gui_native/MainWindow.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MainWindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.10.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN10MainWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto MainWindow::qt_create_metaobjectdata<qt_meta_tag_ZN10MainWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "MainWindow",
        "handleTabChanged",
        "",
        "index",
        "handleTabCloseRequested",
        "handleNewTabRequested",
        "handleNewLadderRequested",
        "handleLadderStatusMessage",
        "msg",
        "handleLadderPingUpdated",
        "ms",
        "handleDomRowClicked",
        "Qt::MouseButton",
        "button",
        "row",
        "price",
        "bidQty",
        "askQty",
        "logLadderStatus",
        "logMarkerEvent",
        "updateTimeLabel",
        "openConnectionsWindow",
        "openPluginsWindow",
        "openSettingsWindow",
        "handleConnectionStateChanged",
        "ConnectionStore::Profile",
        "profile",
        "TradeManager::ConnectionState",
        "state",
        "message",
        "handlePositionChanged",
        "accountName",
        "symbol",
        "TradePosition",
        "position",
        "handleLocalOrdersUpdated",
        "QList<DomWidget::LocalOrderMarker>",
        "markers",
        "applyNotionalPreset",
        "presetIndex",
        "startNotionalEdit",
        "QWidget*",
        "columnContainer",
        "commitNotionalEdit",
        "apply",
        "toggleAlertsPanel"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'handleTabChanged'
        QtMocHelpers::SlotData<void(int)>(1, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Slot 'handleTabCloseRequested'
        QtMocHelpers::SlotData<void(int)>(4, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Slot 'handleNewTabRequested'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleNewLadderRequested'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleLadderStatusMessage'
        QtMocHelpers::SlotData<void(const QString &)>(7, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Slot 'handleLadderPingUpdated'
        QtMocHelpers::SlotData<void(int)>(9, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 10 },
        }}),
        // Slot 'handleDomRowClicked'
        QtMocHelpers::SlotData<void(Qt::MouseButton, int, double, double, double)>(11, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 12, 13 }, { QMetaType::Int, 14 }, { QMetaType::Double, 15 }, { QMetaType::Double, 16 },
            { QMetaType::Double, 17 },
        }}),
        // Slot 'logLadderStatus'
        QtMocHelpers::SlotData<void(const QString &)>(18, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Slot 'logMarkerEvent'
        QtMocHelpers::SlotData<void(const QString &)>(19, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Slot 'updateTimeLabel'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'openConnectionsWindow'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'openPluginsWindow'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'openSettingsWindow'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleConnectionStateChanged'
        QtMocHelpers::SlotData<void(ConnectionStore::Profile, TradeManager::ConnectionState, const QString &)>(24, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 25, 26 }, { 0x80000000 | 27, 28 }, { QMetaType::QString, 29 },
        }}),
        // Slot 'handlePositionChanged'
        QtMocHelpers::SlotData<void(const QString &, const QString &, const TradePosition &)>(30, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 31 }, { QMetaType::QString, 32 }, { 0x80000000 | 33, 34 },
        }}),
        // Slot 'handleLocalOrdersUpdated'
        QtMocHelpers::SlotData<void(const QString &, const QString &, const QVector<DomWidget::LocalOrderMarker> &)>(35, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 31 }, { QMetaType::QString, 32 }, { 0x80000000 | 36, 37 },
        }}),
        // Slot 'applyNotionalPreset'
        QtMocHelpers::SlotData<void(int)>(38, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 39 },
        }}),
        // Slot 'startNotionalEdit'
        QtMocHelpers::SlotData<void(QWidget *, int)>(40, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 41, 42 }, { QMetaType::Int, 39 },
        }}),
        // Slot 'commitNotionalEdit'
        QtMocHelpers::SlotData<void(QWidget *, bool)>(43, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 41, 42 }, { QMetaType::Bool, 44 },
        }}),
        // Slot 'toggleAlertsPanel'
        QtMocHelpers::SlotData<void()>(45, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MainWindow, qt_meta_tag_ZN10MainWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10MainWindowE_t>.metaTypes,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->handleTabChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->handleTabCloseRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->handleNewTabRequested(); break;
        case 3: _t->handleNewLadderRequested(); break;
        case 4: _t->handleLadderStatusMessage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->handleLadderPingUpdated((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->handleDomRowClicked((*reinterpret_cast<std::add_pointer_t<Qt::MouseButton>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[5]))); break;
        case 7: _t->logLadderStatus((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->logMarkerEvent((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->updateTimeLabel(); break;
        case 10: _t->openConnectionsWindow(); break;
        case 11: _t->openPluginsWindow(); break;
        case 12: _t->openSettingsWindow(); break;
        case 13: _t->handleConnectionStateChanged((*reinterpret_cast<std::add_pointer_t<ConnectionStore::Profile>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<TradeManager::ConnectionState>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 14: _t->handlePositionChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<TradePosition>>(_a[3]))); break;
        case 15: _t->handleLocalOrdersUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QList<DomWidget::LocalOrderMarker>>>(_a[3]))); break;
        case 16: _t->applyNotionalPreset((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 17: _t->startNotionalEdit((*reinterpret_cast<std::add_pointer_t<QWidget*>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 18: _t->commitNotionalEdit((*reinterpret_cast<std::add_pointer_t<QWidget*>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 19: _t->toggleAlertsPanel(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 14:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 2:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< TradePosition >(); break;
            }
            break;
        case 17:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QWidget* >(); break;
            }
            break;
        case 18:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QWidget* >(); break;
            }
            break;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 20)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 20;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 20)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 20;
    }
    return _id;
}
QT_WARNING_POP
