/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qxcbxsettings.h"

#include <QtCore/QByteArray>
#include <QtCore/QtEndian>

#include <vector>
#include <algorithm>

#ifdef XCB_USE_XLIB
#include <X11/extensions/XIproto.h>
#endif //XCB_USE_XLIB

QT_BEGIN_NAMESPACE
/* Implementation of http://standards.freedesktop.org/xsettings-spec/xsettings-0.5.html */

enum XSettingsType {
    XSettingsTypeInteger = 0,
    XSettingsTypeString = 1,
    XSettingsTypeColor = 2
};

struct QXcbXSettingsCallback
{
    QXcbXSettings::PropertyChangeFunc func;
    void *handle;
};

class QXcbXSettingsPropertyValue
{
public:
    QXcbXSettingsPropertyValue()
        : last_change_serial(-1)
    {}

    void updateValue(QXcbVirtualDesktop *screen, const QByteArray &name, const QVariant &value, int last_change_serial)
    {
        if (last_change_serial <= this->last_change_serial)
            return;
        this->value = value;
        this->last_change_serial = last_change_serial;
        for (const auto &callback : callback_links)
            callback.func(screen, name, value, callback.handle);
    }

    void addCallback(QXcbXSettings::PropertyChangeFunc func, void *handle)
    {
        QXcbXSettingsCallback callback = { func, handle };
        callback_links.push_back(callback);
    }

    QVariant value;
    int last_change_serial;
    std::vector<QXcbXSettingsCallback> callback_links;

};

class QXcbXSettingsPrivate
{
public:
    QXcbXSettingsPrivate(QXcbVirtualDesktop *screen)
        : screen(screen)
        , initialized(false)
    {
    }

    QByteArray getSettings()
    {
        QXcbConnectionGrabber connectionGrabber(screen->connection());

        auto offset = 0;
        QByteArray settings;
        auto _xsettings_atom = screen->connection()->atom(QXcbAtom::_XSETTINGS_SETTINGS);
        while (1) {
            auto get_prop_cookie =
                    xcb_get_property_unchecked(screen->xcb_connection(),
                                               false,
                                               x_settings_window,
                                               _xsettings_atom,
                                               _xsettings_atom,
                                               offset/4,
                                               8192);
            auto reply = xcb_get_property_reply(screen->xcb_connection(), get_prop_cookie, NULL);
            auto more = false;
            if (!reply)
                return settings;

            const auto property_value_length = xcb_get_property_value_length(reply);
            settings.append(static_cast<const char *>(xcb_get_property_value(reply)), property_value_length);
            offset += property_value_length;
            more = reply->bytes_after != 0;

            free(reply);

            if (!more)
                break;
        }

        return settings;
    }

    static int round_to_nearest_multiple_of_4(int value)
    {
        auto remainder = value % 4;
        if (!remainder)
            return value;
        return value + 4 - remainder;
    }

#ifdef XCB_USE_XLIB
    void populateSettings(const QByteArray &xSettings)
    {
        if (xSettings.length() < 12)
            return;
        auto byteOrder = xSettings.at(0);
        if (byteOrder != LSBFirst && byteOrder != MSBFirst) {
            qWarning("ByteOrder byte %d not 0 or 1", byteOrder);
            return;
        }

#define ADJUST_BO(b, t, x) \
        ((b == LSBFirst) ?                          \
         qFromLittleEndian<t>((const uchar *)(x)) : \
         qFromBigEndian<t>((const uchar *)(x)))
#define VALIDATE_LENGTH(x)    \
        if ((size_t)xSettings.length() < (offset + local_offset + 12 + x)) { \
            qWarning("Length %d runs past end of data", x); \
            return;                                                     \
        }

        auto number_of_settings = ADJUST_BO(byteOrder, quint32, xSettings.mid(8,4).constData());
        auto data = xSettings.constData() + 12;
        size_t offset = 0;
        for (uint i = 0; i < number_of_settings; i++) {
            auto local_offset = 0;
            VALIDATE_LENGTH(2);
            auto type = static_cast<XSettingsType>(*reinterpret_cast<const quint8 *>(data + offset));
            local_offset += 2;

            VALIDATE_LENGTH(2);
            auto name_len = ADJUST_BO(byteOrder, quint16, data + offset + local_offset);
            local_offset += 2;

            VALIDATE_LENGTH(name_len);
            QByteArray name(data + offset + local_offset, name_len);
            local_offset += round_to_nearest_multiple_of_4(name_len);

            VALIDATE_LENGTH(4);
            auto last_change_serial = ADJUST_BO(byteOrder, qint32, data + offset + local_offset);
            Q_UNUSED(last_change_serial);
            local_offset += 4;

            QVariant value;
            if (type == XSettingsTypeString) {
                VALIDATE_LENGTH(4);
                auto value_length = ADJUST_BO(byteOrder, qint32, data + offset + local_offset);
                local_offset+=4;
                VALIDATE_LENGTH(value_length);
                QByteArray value_string(data + offset + local_offset, value_length);
                value.setValue(value_string);
                local_offset += round_to_nearest_multiple_of_4(value_length);
            } else if (type == XSettingsTypeInteger) {
                VALIDATE_LENGTH(4);
                auto value_length = ADJUST_BO(byteOrder, qint32, data + offset + local_offset);
                local_offset += 4;
                value.setValue(value_length);
            } else if (type == XSettingsTypeColor) {
                VALIDATE_LENGTH(2*4);
                auto red = ADJUST_BO(byteOrder, quint16, data + offset + local_offset);
                local_offset += 2;
                auto green = ADJUST_BO(byteOrder, quint16, data + offset + local_offset);
                local_offset += 2;
                auto blue = ADJUST_BO(byteOrder, quint16, data + offset + local_offset);
                local_offset += 2;
                auto alpha= ADJUST_BO(byteOrder, quint16, data + offset + local_offset);
                local_offset += 2;
                QColor color_value(red,green,blue,alpha);
                value.setValue(color_value);
            }
            offset += local_offset;
            settings[name].updateValue(screen,name,value,last_change_serial);
        }

    }
#endif //XCB_USE_XLIB

    QXcbVirtualDesktop *screen;
    xcb_window_t x_settings_window;
    QMap<QByteArray, QXcbXSettingsPropertyValue> settings;
    bool initialized;
};


QXcbXSettings::QXcbXSettings(QXcbVirtualDesktop *screen)
    : d_ptr(new QXcbXSettingsPrivate(screen))
{
    QByteArray settings_atom_for_screen("_XSETTINGS_S");
    settings_atom_for_screen.append(QByteArray::number(screen->number()));
    auto atom_cookie = xcb_intern_atom(screen->xcb_connection(),
                                                           true,
                                                           settings_atom_for_screen.length(),
                                                           settings_atom_for_screen.constData());
    xcb_generic_error_t *error = 0;
    auto atom_reply = xcb_intern_atom_reply(screen->xcb_connection(),atom_cookie,&error);
    if (error) {
        free(error);
        return;
    }
    auto selection_owner_atom = atom_reply->atom;
    free(atom_reply);

    auto selection_cookie =
            xcb_get_selection_owner(screen->xcb_connection(), selection_owner_atom);

    auto selection_result =
            xcb_get_selection_owner_reply(screen->xcb_connection(), selection_cookie, &error);
    if (error) {
        free(error);
        return;
    }

    d_ptr->x_settings_window = selection_result->owner;
    free(selection_result);
    if (!d_ptr->x_settings_window) {
        return;
    }

    const uint32_t event = XCB_CW_EVENT_MASK;
    const uint32_t event_mask[] = { XCB_EVENT_MASK_STRUCTURE_NOTIFY|XCB_EVENT_MASK_PROPERTY_CHANGE };
    xcb_change_window_attributes(screen->xcb_connection(),d_ptr->x_settings_window,event,event_mask);

#ifdef XCB_USE_XLIB
    d_ptr->populateSettings(d_ptr->getSettings());
    d_ptr->initialized = true;
#endif //XCB_USE_XLIB
}

QXcbXSettings::~QXcbXSettings()
{
    delete d_ptr;
    d_ptr = 0;
}

bool QXcbXSettings::initialized() const
{
    Q_D(const QXcbXSettings);
    return d->initialized;
}

void QXcbXSettings::handlePropertyNotifyEvent(const xcb_property_notify_event_t *event)
{
    Q_D(QXcbXSettings);
    if (event->window != d->x_settings_window)
        return;
#ifdef XCB_USE_XLIB
    d->populateSettings(d->getSettings());
#endif //XCB_USE_XLIB
}

void QXcbXSettings::registerCallbackForProperty(const QByteArray &property, QXcbXSettings::PropertyChangeFunc func, void *handle)
{
    Q_D(QXcbXSettings);
    d->settings[property].addCallback(func,handle);
}

void QXcbXSettings::removeCallbackForHandle(const QByteArray &property, void *handle)
{
    Q_D(QXcbXSettings);
    auto &callbacks = d->settings[property].callback_links;

    auto isCallbackForHandle = [handle](const QXcbXSettingsCallback &cb) { return cb.handle == handle; };

    callbacks.erase(std::remove_if(callbacks.begin(), callbacks.end(),
                                   isCallbackForHandle),
                    callbacks.end());
}

void QXcbXSettings::removeCallbackForHandle(void *handle)
{
    Q_D(QXcbXSettings);
    for (auto it = d->settings.cbegin();
         it != d->settings.cend(); ++it) {
        removeCallbackForHandle(it.key(),handle);
    }
}

QVariant QXcbXSettings::setting(const QByteArray &property) const
{
    Q_D(const QXcbXSettings);
    return d->settings.value(property).value;
}

QT_END_NAMESPACE
