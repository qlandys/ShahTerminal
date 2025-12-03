/****************************************************************************
** Meta object code from reading C++ file 'TradeManager.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../gui_native/TradeManager.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'TradeManager.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN12TradeManagerE_t {};
} // unnamed namespace

template <> constexpr inline auto TradeManager::qt_create_metaobjectdata<qt_meta_tag_ZN12TradeManagerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "TradeManager",
        "connectionStateChanged",
        "",
        "TradeManager::ConnectionState",
        "state",
        "message",
        "orderPlaced",
        "symbol",
        "OrderSide",
        "side",
        "price",
        "quantity",
        "orderCanceled",
        "orderFailed",
        "positionChanged",
        "TradePosition",
        "position",
        "logMessage",
        "handleSocketConnected",
        "handleSocketDisconnected",
        "handleSocketError",
        "QAbstractSocket::SocketError",
        "error",
        "handleSocketTextMessage",
        "handleSocketBinaryMessage",
        "payload",
        "refreshListenKey",
        "ConnectionState",
        "Disconnected",
        "Connecting",
        "Connected",
        "Error"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'connectionStateChanged'
        QtMocHelpers::SignalData<void(TradeManager::ConnectionState, const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::QString, 5 },
        }}),
        // Signal 'orderPlaced'
        QtMocHelpers::SignalData<void(const QString &, OrderSide, double, double)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { 0x80000000 | 8, 9 }, { QMetaType::Double, 10 }, { QMetaType::Double, 11 },
        }}),
        // Signal 'orderCanceled'
        QtMocHelpers::SignalData<void(const QString &, OrderSide, double)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { 0x80000000 | 8, 9 }, { QMetaType::Double, 10 },
        }}),
        // Signal 'orderFailed'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { QMetaType::QString, 5 },
        }}),
        // Signal 'positionChanged'
        QtMocHelpers::SignalData<void(const QString &, const TradePosition &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { 0x80000000 | 15, 16 },
        }}),
        // Signal 'logMessage'
        QtMocHelpers::SignalData<void(const QString &)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 },
        }}),
        // Slot 'handleSocketConnected'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleSocketDisconnected'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleSocketError'
        QtMocHelpers::SlotData<void(QAbstractSocket::SocketError)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 21, 22 },
        }}),
        // Slot 'handleSocketTextMessage'
        QtMocHelpers::SlotData<void(const QString &)>(23, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 5 },
        }}),
        // Slot 'handleSocketBinaryMessage'
        QtMocHelpers::SlotData<void(const QByteArray &)>(24, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QByteArray, 25 },
        }}),
        // Slot 'refreshListenKey'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'ConnectionState'
        QtMocHelpers::EnumData<enum ConnectionState>(27, 27, QMC::EnumIsScoped).add({
            {   28, ConnectionState::Disconnected },
            {   29, ConnectionState::Connecting },
            {   30, ConnectionState::Connected },
            {   31, ConnectionState::Error },
        }),
    };
    return QtMocHelpers::metaObjectData<TradeManager, qt_meta_tag_ZN12TradeManagerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject TradeManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12TradeManagerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12TradeManagerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN12TradeManagerE_t>.metaTypes,
    nullptr
} };

void TradeManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<TradeManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->connectionStateChanged((*reinterpret_cast<std::add_pointer_t<TradeManager::ConnectionState>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 1: _t->orderPlaced((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<OrderSide>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4]))); break;
        case 2: _t->orderCanceled((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<OrderSide>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3]))); break;
        case 3: _t->orderFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->positionChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<TradePosition>>(_a[2]))); break;
        case 5: _t->logMessage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->handleSocketConnected(); break;
        case 7: _t->handleSocketDisconnected(); break;
        case 8: _t->handleSocketError((*reinterpret_cast<std::add_pointer_t<QAbstractSocket::SocketError>>(_a[1]))); break;
        case 9: _t->handleSocketTextMessage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->handleSocketBinaryMessage((*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 11: _t->refreshListenKey(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 4:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< TradePosition >(); break;
            }
            break;
        case 8:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAbstractSocket::SocketError >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (TradeManager::*)(TradeManager::ConnectionState , const QString & )>(_a, &TradeManager::connectionStateChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (TradeManager::*)(const QString & , OrderSide , double , double )>(_a, &TradeManager::orderPlaced, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (TradeManager::*)(const QString & , OrderSide , double )>(_a, &TradeManager::orderCanceled, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (TradeManager::*)(const QString & , const QString & )>(_a, &TradeManager::orderFailed, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (TradeManager::*)(const QString & , const TradePosition & )>(_a, &TradeManager::positionChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (TradeManager::*)(const QString & )>(_a, &TradeManager::logMessage, 5))
            return;
    }
}

const QMetaObject *TradeManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TradeManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12TradeManagerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int TradeManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void TradeManager::connectionStateChanged(TradeManager::ConnectionState _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void TradeManager::orderPlaced(const QString & _t1, OrderSide _t2, double _t3, double _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 2
void TradeManager::orderCanceled(const QString & _t1, OrderSide _t2, double _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3);
}

// SIGNAL 3
void TradeManager::orderFailed(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void TradeManager::positionChanged(const QString & _t1, const TradePosition & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void TradeManager::logMessage(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}
QT_WARNING_POP
