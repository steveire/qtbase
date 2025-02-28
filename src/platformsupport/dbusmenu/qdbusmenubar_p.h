/****************************************************************************
**
** Copyright (C) 2016 Dmitry Shachnev <mitya57@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QDBUSMENUBAR_P_H
#define QDBUSMENUBAR_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qdbusplatformmenu_p.h>
#include <private/qdbusmenuadaptor_p.h>
#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtGui/QWindow>

QT_BEGIN_NAMESPACE

class QDBusMenuBar : public QPlatformMenuBar
{
    Q_OBJECT

public:
    QDBusMenuBar();
    virtual ~QDBusMenuBar();

    void insertMenu(QPlatformMenu *menu, QPlatformMenu *before) Q_DECL_OVERRIDE;
    void removeMenu(QPlatformMenu *menu) Q_DECL_OVERRIDE;
    void syncMenu(QPlatformMenu *menu) Q_DECL_OVERRIDE;
    void handleReparent(QWindow *newParentWindow) Q_DECL_OVERRIDE;
    QPlatformMenu *menuForTag(quintptr tag) const Q_DECL_OVERRIDE;

private:
    QDBusPlatformMenu *m_menu;
    QDBusMenuAdaptor *m_menuAdaptor;
    QHash<quintptr, QDBusPlatformMenuItem *> m_menuItems;
    uint m_windowId;
    QString m_objectPath;

    QDBusPlatformMenuItem *menuItemForMenu(QPlatformMenu *menu);
    static void updateMenuItem(QDBusPlatformMenuItem *item, QPlatformMenu *menu);
    void registerMenuBar();
    void unregisterMenuBar();
};

QT_END_NAMESPACE

#endif // QDBUSMENUBAR_P_H
