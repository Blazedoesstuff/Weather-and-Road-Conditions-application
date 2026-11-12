/****************************************************************************
** Meta object code from reading C++ file 'RoadConditionsService.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../include/RoadConditionsService.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'RoadConditionsService.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.10.3. It"
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
struct qt_meta_tag_ZN21RoadConditionsServiceE_t {};
} // unnamed namespace

template <> constexpr inline auto RoadConditionsService::qt_create_metaobjectdata<qt_meta_tag_ZN21RoadConditionsServiceE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "RoadConditionsService",
        "roadConditionsReady",
        "",
        "RoadConditions",
        "conditions",
        "roadConditionsFailed",
        "message"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'roadConditionsReady'
        QtMocHelpers::SignalData<void(const RoadConditions &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'roadConditionsFailed'
        QtMocHelpers::SignalData<void(const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<RoadConditionsService, qt_meta_tag_ZN21RoadConditionsServiceE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject RoadConditionsService::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN21RoadConditionsServiceE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN21RoadConditionsServiceE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN21RoadConditionsServiceE_t>.metaTypes,
    nullptr
} };

void RoadConditionsService::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<RoadConditionsService *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->roadConditionsReady((*reinterpret_cast<std::add_pointer_t<RoadConditions>>(_a[1]))); break;
        case 1: _t->roadConditionsFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (RoadConditionsService::*)(const RoadConditions & )>(_a, &RoadConditionsService::roadConditionsReady, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (RoadConditionsService::*)(const QString & )>(_a, &RoadConditionsService::roadConditionsFailed, 1))
            return;
    }
}

const QMetaObject *RoadConditionsService::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *RoadConditionsService::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN21RoadConditionsServiceE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int RoadConditionsService::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void RoadConditionsService::roadConditionsReady(const RoadConditions & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void RoadConditionsService::roadConditionsFailed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}
QT_WARNING_POP
