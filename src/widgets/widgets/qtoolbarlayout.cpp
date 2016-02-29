/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
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

#include <qapplication.h>
#include <qaction.h>
#include <qwidgetaction.h>
#include <qtoolbar.h>
#include <qstyleoption.h>
#include <qtoolbutton.h>
#include <qmenu.h>
#include <qdebug.h>
#include <qmath.h>
#ifdef Q_OS_OSX
#include <qpa/qplatformnativeinterface.h>
#endif

#include "qmainwindowlayout_p.h"
#include "qtoolbarextension_p.h"
#include "qtoolbarlayout_p.h"
#include "qtoolbarseparator_p.h"

#ifndef QT_NO_TOOLBAR

QT_BEGIN_NAMESPACE

// qmainwindow.cpp
extern QMainWindowLayout *qt_mainwindow_layout(const QMainWindow *window);

/******************************************************************************
** QToolBarItem
*/

QToolBarItem::QToolBarItem(QWidget *widget)
    : QWidgetItem(widget), action(0), customWidget(false)
{
}

bool QToolBarItem::isEmpty() const
{
    return action == 0 || !action->isVisible();
}

/******************************************************************************
** QToolBarLayout
*/

QToolBarLayout::QToolBarLayout(QWidget *parent)
    : QLayout(parent), expanded(false), animating(false), dirty(true),
        expanding(false), empty(true), expandFlag(false), popupMenu(0)
{
    auto tb = qobject_cast<QToolBar*>(parent);
    if (!tb)
        return;

    extension = new QToolBarExtension(tb);
    extension->setFocusPolicy(Qt::NoFocus);
    extension->hide();
    QObject::connect(tb, SIGNAL(orientationChanged(Qt::Orientation)),
                     extension, SLOT(setOrientation(Qt::Orientation)));

    setUsePopupMenu(qobject_cast<QMainWindow*>(tb->parentWidget()) == 0);
}

QToolBarLayout::~QToolBarLayout()
{
    while (!items.isEmpty()) {
        auto item = items.takeFirst();
        if (auto widgetAction = qobject_cast<QWidgetAction*>(item->action)) {
            if (item->customWidget)
                widgetAction->releaseWidget(item->widget());
        }
        delete item;
    }
}

void QToolBarLayout::updateMarginAndSpacing()
{
    auto tb = qobject_cast<QToolBar*>(parentWidget());
    if (!tb)
        return;
    auto style = tb->style();
    QStyleOptionToolBar opt;
    tb->initStyleOption(&opt);
    setMargin(style->pixelMetric(QStyle::PM_ToolBarItemMargin, &opt, tb)
                + style->pixelMetric(QStyle::PM_ToolBarFrameWidth, &opt, tb));
    setSpacing(style->pixelMetric(QStyle::PM_ToolBarItemSpacing, &opt, tb));
}

bool QToolBarLayout::hasExpandFlag() const
{
    return expandFlag;
}

void QToolBarLayout::setUsePopupMenu(bool set)
{
    if (!dirty && ((popupMenu == 0) == set))
        invalidate();
    if (!set) {
        QObject::connect(extension, SIGNAL(clicked(bool)),
                        this, SLOT(setExpanded(bool)), Qt::UniqueConnection);
        extension->setPopupMode(QToolButton::DelayedPopup);
        extension->setMenu(0);
        delete popupMenu;
        popupMenu = 0;
    } else {
        QObject::disconnect(extension, SIGNAL(clicked(bool)),
                            this, SLOT(setExpanded(bool)));
        extension->setPopupMode(QToolButton::InstantPopup);
        if (!popupMenu) {
            popupMenu = new QMenu(extension);
        }
        extension->setMenu(popupMenu);
    }
}

void QToolBarLayout::checkUsePopupMenu()
{
    auto tb = static_cast<QToolBar *>(parent());
    auto mw = qobject_cast<QMainWindow *>(tb->parent());
    auto o = tb->orientation();
    setUsePopupMenu(!mw || tb->isFloating() || perp(o, expandedSize(mw->size())) >= perp(o, mw->size()));
}

void QToolBarLayout::addItem(QLayoutItem*)
{
    qWarning("QToolBarLayout::addItem(): please use addAction() instead");
    return;
}

QLayoutItem *QToolBarLayout::itemAt(int index) const
{
    if (index < 0 || index >= items.count())
        return 0;
    return items.at(index);
}

QLayoutItem *QToolBarLayout::takeAt(int index)
{
    if (index < 0 || index >= items.count())
        return 0;
    auto item = items.takeAt(index);

    if (popupMenu)
        popupMenu->removeAction(item->action);

    auto widgetAction = qobject_cast<QWidgetAction*>(item->action);
    if (widgetAction != 0 && item->customWidget) {
        widgetAction->releaseWidget(item->widget());
    } else {
        // destroy the QToolButton/QToolBarSeparator
        item->widget()->hide();
        item->widget()->deleteLater();
    }

    invalidate();
    return item;
}

void QToolBarLayout::insertAction(int index, QAction *action)
{
    index = qMax(0, index);
    index = qMin(items.count(), index);

    auto item = createItem(action);
    if (item) {
        items.insert(index, item);
        invalidate();
    }
}

int QToolBarLayout::indexOf(QAction *action) const
{
    for (auto i = 0; i < items.count(); ++i) {
        if (items.at(i)->action == action)
            return i;
    }
    return -1;
}

int QToolBarLayout::count() const
{
    return items.count();
}

bool QToolBarLayout::isEmpty() const
{
    if (dirty)
        updateGeomArray();
    return empty;
}

void QToolBarLayout::invalidate()
{
    dirty = true;
    QLayout::invalidate();
}

Qt::Orientations QToolBarLayout::expandingDirections() const
{
    if (dirty)
        updateGeomArray();
    auto tb = qobject_cast<QToolBar*>(parentWidget());
    if (!tb)
        return Qt::Orientations(0);
    auto o = tb->orientation();
    return expanding ? Qt::Orientations(o) : Qt::Orientations(0);
}

bool QToolBarLayout::movable() const
{
    auto tb = qobject_cast<QToolBar*>(parentWidget());
    if (!tb)
        return false;
    auto win = qobject_cast<QMainWindow*>(tb->parentWidget());
    return tb->isMovable() && win != 0;
}

void QToolBarLayout::updateGeomArray() const
{
    if (!dirty)
        return;

    auto that = const_cast<QToolBarLayout*>(this);

    auto tb = qobject_cast<QToolBar*>(parentWidget());
    if (!tb)
        return;
    auto style = tb->style();
    QStyleOptionToolBar opt;
    tb->initStyleOption(&opt);
    const auto handleExtent = movable()
            ? style->pixelMetric(QStyle::PM_ToolBarHandleExtent, &opt, tb) : 0;
    const auto margin = this->margin();
    const auto spacing = this->spacing();
    const auto extensionExtent = style->pixelMetric(QStyle::PM_ToolBarExtensionExtent, &opt, tb);
    auto o = tb->orientation();

    that->minSize = QSize(0, 0);
    that->hint = QSize(0, 0);
    rperp(o, that->minSize) = style->pixelMetric(QStyle::PM_ToolBarHandleExtent, &opt, tb);
    rperp(o, that->hint) = style->pixelMetric(QStyle::PM_ToolBarHandleExtent, &opt, tb);

    that->expanding = false;
    that->empty = false;

    QVector<QLayoutStruct> a(items.count() + 1); // + 1 for the stretch

    auto count = 0;
    for (auto i = 0; i < items.count(); ++i) {
        auto item = items.at(i);

        auto max = item->maximumSize();
        auto min = item->minimumSize();
        auto hint = item->sizeHint();
        auto exp = item->expandingDirections();
        auto empty = item->isEmpty();

        that->expanding = expanding || exp & o;


        if (item->widget()) {
            if ((item->widget()->sizePolicy().horizontalPolicy() & QSizePolicy::ExpandFlag)) {
                that->expandFlag = true;
            }
        }

        if (!empty) {
            if (count == 0) // the minimum size only displays one widget
                rpick(o, that->minSize) += pick(o, min);
            auto s = perp(o, minSize);
            rperp(o, that->minSize) = qMax(s, perp(o, min));

            //we only add spacing before item (ie never before the first one)
            rpick(o, that->hint) += (count == 0 ? 0 : spacing) + pick(o, hint);
            s = perp(o, that->hint);
            rperp(o, that->hint) = qMax(s, perp(o, hint));
            ++count;
        }

        a[i].sizeHint = pick(o, hint);
        a[i].maximumSize = pick(o, max);
        a[i].minimumSize = pick(o, min);
        a[i].expansive = exp & o;
        if (o == Qt::Horizontal)
            a[i].stretch = item->widget()->sizePolicy().horizontalStretch();
        else
            a[i].stretch = item->widget()->sizePolicy().verticalStretch();
        a[i].empty = empty;
    }

    that->geomArray = a;
    that->empty = count == 0;

    rpick(o, that->minSize) += handleExtent;
    that->minSize += QSize(2*margin, 2*margin);
    if (items.count() > 1)
        rpick(o, that->minSize) += spacing + extensionExtent;

    rpick(o, that->hint) += handleExtent;
    that->hint += QSize(2*margin, 2*margin);
    that->dirty = false;
}

static bool defaultWidgetAction(QToolBarItem *item)
{
    auto a = qobject_cast<QWidgetAction*>(item->action);
    return a != 0 && a->defaultWidget() == item->widget();
}

void QToolBarLayout::updateMacBorderMetrics()
{
#ifdef Q_OS_OSX
    QToolBar *tb = qobject_cast<QToolBar*>(parentWidget());
    if (!tb)
        return;

    QRect rect = geometry();

    QMainWindow *mainWindow = qobject_cast<QMainWindow*>(tb->parentWidget());
    if (!mainWindow || !mainWindow->isWindow() || !mainWindow->unifiedTitleAndToolBarOnMac())
        return;

    QPlatformNativeInterface *nativeInterface = QApplication::platformNativeInterface();
    QPlatformNativeInterface::NativeResourceForIntegrationFunction function =
        nativeInterface->nativeResourceFunctionForIntegration("registerContentBorderArea");
    if (!function)
        return; // Not Cocoa platform plugin.

    QPoint upper = tb->mapToParent(rect.topLeft());
    QPoint lower = tb->mapToParent(rect.bottomLeft() + QPoint(0, 1));

    typedef void (*RegisterContentBorderAreaFunction)(QWindow *window, void *identifier, int upper, int lower);
    if (mainWindow->toolBarArea(tb) == Qt::TopToolBarArea) {
        (reinterpret_cast<RegisterContentBorderAreaFunction>(function))(tb->window()->windowHandle(), tb, upper.y(), lower.y());
    } else {
        (reinterpret_cast<RegisterContentBorderAreaFunction>(function))(tb->window()->windowHandle(), tb, 0, 0);
    }
#endif
}

void QToolBarLayout::setGeometry(const QRect &rect)
{
    auto tb = qobject_cast<QToolBar*>(parentWidget());
    if (!tb)
        return;
    auto style = tb->style();
    QStyleOptionToolBar opt;
    tb->initStyleOption(&opt);
    const auto margin = this->margin();
    const auto extensionExtent = style->pixelMetric(QStyle::PM_ToolBarExtensionExtent, &opt, tb);
    auto o = tb->orientation();

    QLayout::setGeometry(rect);

    updateMacBorderMetrics();

    auto ranOutOfSpace = false;
    if (!animating)
        ranOutOfSpace = layoutActions(rect.size());

    if (expanded || animating || ranOutOfSpace) {
        auto area = Qt::TopToolBarArea;
        if (auto win = qobject_cast<QMainWindow*>(tb->parentWidget()))
            area = win->toolBarArea(tb);
        auto hint = sizeHint();

        QPoint pos;
        rpick(o, pos) = pick(o, rect.bottomRight()) - margin - extensionExtent + 2;
        if (area == Qt::LeftToolBarArea || area == Qt::TopToolBarArea)
            rperp(o, pos) = perp(o, rect.topLeft()) + margin;
        else
            rperp(o, pos) = perp(o, rect.bottomRight()) - margin - (perp(o, hint) - 2*margin) + 1;
        QSize size;
        rpick(o, size) = extensionExtent;
        rperp(o, size) = perp(o, hint) - 2*margin;
        QRect r(pos, size);

        if (o == Qt::Horizontal)
            r = QStyle::visualRect(parentWidget()->layoutDirection(), rect, r);

        extension->setGeometry(r);

        if (extension->isHidden())
            extension->show();
    } else {
        if (!extension->isHidden())
            extension->hide();
    }
}

bool QToolBarLayout::layoutActions(const QSize &size)
{
    if (dirty)
        updateGeomArray();

    QRect rect(0, 0, size.width(), size.height());

    QList<QWidget*> showWidgets, hideWidgets;

    auto tb = qobject_cast<QToolBar*>(parentWidget());
    if (!tb)
        return false;
    auto style = tb->style();
    QStyleOptionToolBar opt;
    tb->initStyleOption(&opt);
    const auto handleExtent = movable()
            ? style->pixelMetric(QStyle::PM_ToolBarHandleExtent, &opt, tb) : 0;
    const auto margin = this->margin();
    const auto spacing = this->spacing();
    const auto extensionExtent = style->pixelMetric(QStyle::PM_ToolBarExtensionExtent, &opt, tb);
    auto o = tb->orientation();
    auto extensionMenuContainsOnlyWidgetActions = true;

    auto space = pick(o, rect.size()) - 2*margin - handleExtent;
    if (space <= 0)
        return false;  // nothing to do.

    if(popupMenu)
        popupMenu->clear();

    auto ranOutOfSpace = false;
    auto rows = 0;
    auto rowPos = perp(o, rect.topLeft()) + margin;
    auto i = 0;
    while (i < items.count()) {
        auto a = geomArray;

        auto start = i;
        auto size = 0;
        auto prev = -1;
        auto rowHeight = 0;
        auto count = 0;
        auto maximumSize = 0;
        auto expansiveRow = false;
        for (; i < items.count(); ++i) {
            if (a[i].empty)
                continue;

            auto newSize = size + (count == 0 ? 0 : spacing) + a[i].minimumSize;
            if (prev != -1 && newSize > space) {
                if (rows == 0)
                    ranOutOfSpace = true;
                // do we have to move the previous item to the next line to make space for
                // the extension button?
                if (count > 1 && size + spacing + extensionExtent > space)
                    i = prev;
                break;
            }

            if (expanded)
                rowHeight = qMax(rowHeight, perp(o, items.at(i)->sizeHint()));
            expansiveRow = expansiveRow || a[i].expansive;
            size = newSize;
            maximumSize += spacing + (a[i].expansive ? a[i].maximumSize : a[i].smartSizeHint());
            prev = i;
            ++count;
        }

        // stretch at the end
        a[i].sizeHint = 0;
        a[i].maximumSize = QWIDGETSIZE_MAX;
        a[i].minimumSize = 0;
        a[i].expansive = true;
        a[i].stretch = 0;
        a[i].empty = true;

        if (expansiveRow && maximumSize < space) {
            expansiveRow = false;
            a[i].maximumSize = space - maximumSize;
        }

        qGeomCalc(a, start, i - start + (expansiveRow ? 0 : 1), 0,
                    space - (ranOutOfSpace ? (extensionExtent + spacing) : 0),
                    spacing);

        for (auto j = start; j < i; ++j) {
            auto item = items.at(j);

            if (a[j].empty) {
                if (!item->widget()->isHidden())
                    hideWidgets << item->widget();
                continue;
            }

            QPoint pos;
            rpick(o, pos) = margin + handleExtent + a[j].pos;
            rperp(o, pos) = rowPos;
            QSize size;
            rpick(o, size) = a[j].size;
            if (expanded)
                rperp(o, size) = rowHeight;
            else
                rperp(o, size) = perp(o, rect.size()) - 2*margin;
            QRect r(pos, size);

            if (o == Qt::Horizontal)
                r = QStyle::visualRect(parentWidget()->layoutDirection(), rect, r);

            item->setGeometry(r);

            if (item->widget()->isHidden())
                showWidgets << item->widget();
        }

        if (!expanded) {
            for (auto j = i; j < items.count(); ++j) {
                auto item = items.at(j);
                if (!item->widget()->isHidden())
                    hideWidgets << item->widget();
                if (popupMenu) {
                    if (!defaultWidgetAction(item)) {
                        popupMenu->addAction(item->action);
                        extensionMenuContainsOnlyWidgetActions = false;
                    }
                }
            }
            break;
        }

        rowPos += rowHeight + spacing;
        ++rows;
    }

    // if we are using a popup menu, not the expadning toolbar effect, we cannot move custom
    // widgets into the menu. If only custom widget actions are chopped off, the popup menu
    // is empty. So we show the little extension button to show something is chopped off,
    // but we make it disabled.
    extension->setEnabled(popupMenu == 0 || !extensionMenuContainsOnlyWidgetActions);

    // we have to do the show/hide here, because it triggers more calls to setGeometry :(
    for (auto i = 0; i < showWidgets.count(); ++i)
        showWidgets.at(i)->show();
    for (auto i = 0; i < hideWidgets.count(); ++i)
        hideWidgets.at(i)->hide();

    return ranOutOfSpace;
}

QSize QToolBarLayout::expandedSize(const QSize &size) const
{
    if (dirty)
        updateGeomArray();

    auto tb = qobject_cast<QToolBar*>(parentWidget());
    if (!tb)
        return QSize(0, 0);
    auto win = qobject_cast<QMainWindow*>(tb->parentWidget());
    auto o = tb->orientation();
    auto style = tb->style();
    QStyleOptionToolBar opt;
    tb->initStyleOption(&opt);
    const auto handleExtent = movable()
            ? style->pixelMetric(QStyle::PM_ToolBarHandleExtent, &opt, tb) : 0;
    const auto margin = this->margin();
    const auto spacing = this->spacing();
    const auto extensionExtent = style->pixelMetric(QStyle::PM_ToolBarExtensionExtent, &opt, tb);

    auto total_w = 0;
    auto count = 0;
    for (auto x = 0; x < items.count(); ++x) {
        if (!geomArray[x].empty) {
            total_w += (count == 0 ? 0 : spacing) + geomArray[x].minimumSize;
            ++count;
        }
    }
    if (count == 0)
        return QSize(0, 0);

    auto min_w = pick(o, size);
    auto rows = (int)qSqrt(qreal(count));
    if (rows == 1)
        ++rows;      // we want to expand to at least two rows
    auto space = total_w/rows + spacing + extensionExtent;
    space = qMax(space, min_w - 2*margin - handleExtent);
    if (win != 0)
        space = qMin(space, pick(o, win->size()) - 2*margin - handleExtent);

    auto w = 0;
    auto h = 0;
    auto i = 0;
    while (i < items.count()) {
        auto count = 0;
        auto size = 0;
        auto prev = -1;
        auto rowHeight = 0;
        for (; i < items.count(); ++i) {
            if (geomArray[i].empty)
                continue;

            auto newSize = size + (count == 0 ? 0 : spacing) + geomArray[i].minimumSize;
            rowHeight = qMax(rowHeight, perp(o, items.at(i)->sizeHint()));
            if (prev != -1 && newSize > space) {
                if (count > 1 && size + spacing + extensionExtent > space) {
                    size -= spacing + geomArray[prev].minimumSize;
                    i = prev;
                }
                break;
            }

            size = newSize;
            prev = i;
            ++count;
        }

        w = qMax(size, w);
        h += rowHeight + spacing;
    }

    w += 2*margin + handleExtent + spacing + extensionExtent;
    w = qMax(w, min_w);
    if (win != 0)
        w = qMin(w, pick(o, win->size()));
    h += 2*margin - spacing; //there is no spacing before the first row

    QSize result;
    rpick(o, result) = w;
    rperp(o, result) = h;
    return result;
}

void QToolBarLayout::setExpanded(bool exp)
{
    auto tb = qobject_cast<QToolBar*>(parentWidget());
    if (!tb)
        return;
    if (exp == expanded && !tb->isWindow())
        return;

    expanded = exp;
    extension->setChecked(expanded);

    if (auto win = qobject_cast<QMainWindow*>(tb->parentWidget())) {
#ifdef QT_NO_DOCKWIDGET
        animating = false;
#else
        animating = !tb->isWindow() && win->isAnimated();
#endif
        auto layout = qt_mainwindow_layout(win);
        if (expanded) {
            tb->raise();
        } else {
            auto path = layout->layoutState.indexOf(tb);
            if (!path.isEmpty()) {
                auto rect = layout->layoutState.itemRect(path);
                layoutActions(rect.size());
            }
        }
        layout->layoutState.toolBarAreaLayout.apply(animating);
    }
}

QSize QToolBarLayout::minimumSize() const
{
    if (dirty)
        updateGeomArray();
    return minSize;
}

QSize QToolBarLayout::sizeHint() const
{
    if (dirty)
        updateGeomArray();
    return hint;
}

QToolBarItem *QToolBarLayout::createItem(QAction *action)
{
    auto customWidget = false;
    auto standardButtonWidget = false;
    QWidget *widget = 0;
    auto tb = qobject_cast<QToolBar*>(parentWidget());
    if (!tb)
        return (QToolBarItem *)0;

    if (auto widgetAction = qobject_cast<QWidgetAction *>(action)) {
        widget = widgetAction->requestWidget(tb);
        if (widget != 0) {
            widget->setAttribute(Qt::WA_LayoutUsesWidgetRect);
            customWidget = true;
        }
    } else if (action->isSeparator()) {
        auto sep = new QToolBarSeparator(tb);
        connect(tb, SIGNAL(orientationChanged(Qt::Orientation)),
                sep, SLOT(setOrientation(Qt::Orientation)));
        widget = sep;
    }

    if (!widget) {
        auto button = new QToolButton(tb);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setIconSize(tb->iconSize());
        button->setToolButtonStyle(tb->toolButtonStyle());
        QObject::connect(tb, SIGNAL(iconSizeChanged(QSize)),
                         button, SLOT(setIconSize(QSize)));
        QObject::connect(tb, SIGNAL(toolButtonStyleChanged(Qt::ToolButtonStyle)),
                         button, SLOT(setToolButtonStyle(Qt::ToolButtonStyle)));
        button->setDefaultAction(action);
        QObject::connect(button, SIGNAL(triggered(QAction*)), tb, SIGNAL(actionTriggered(QAction*)));
        widget = button;
        standardButtonWidget = true;
    }

    widget->hide();
    auto result = new QToolBarItem(widget);
    if (standardButtonWidget)
        result->setAlignment(Qt::AlignJustify);
    result->customWidget = customWidget;
    result->action = action;
    return result;
}

QT_END_NAMESPACE

#endif // QT_NO_TOOLBAR
