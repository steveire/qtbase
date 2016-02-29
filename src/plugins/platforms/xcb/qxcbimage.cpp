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

#include "qxcbimage.h"
#include <QtGui/QColor>
#include <QtGui/private/qimage_p.h>
#include <QtGui/private/qdrawhelper_p.h>
#ifdef XCB_USE_RENDER
#include <xcb/render.h>
// 'template' is used as a function argument name in xcb_renderutil.h
#define template template_param
// extern "C" is missing too
extern "C" {
#include <xcb/xcb_renderutil.h>
}
#undef template
#endif

QT_BEGIN_NAMESPACE

QImage::Format qt_xcb_imageFormatForVisual(QXcbConnection *connection, uint8_t depth,
                                           const xcb_visualtype_t *visual)
{
    auto format = connection->formatForDepth(depth);

    if (!visual || !format)
        return QImage::Format_Invalid;

    if (depth == 32 && format->bits_per_pixel == 32 && visual->red_mask == 0xff0000
        && visual->green_mask == 0xff00 && visual->blue_mask == 0xff)
        return QImage::Format_ARGB32_Premultiplied;

    if (depth == 30 && format->bits_per_pixel == 32 && visual->red_mask == 0x3ff
        && visual->green_mask == 0x0ffc00 && visual->blue_mask == 0x3ff00000)
        return QImage::Format_BGR30;

    if (depth == 30 && format->bits_per_pixel == 32 && visual->blue_mask == 0x3ff
        && visual->green_mask == 0x0ffc00 && visual->red_mask == 0x3ff00000)
        return QImage::Format_RGB30;

    if (depth == 24 && format->bits_per_pixel == 32 && visual->red_mask == 0xff0000
        && visual->green_mask == 0xff00 && visual->blue_mask == 0xff)
        return QImage::Format_RGB32;

    if (depth == 16 && format->bits_per_pixel == 16 && visual->red_mask == 0xf800
        && visual->green_mask == 0x7e0 && visual->blue_mask == 0x1f)
        return QImage::Format_RGB16;

    return QImage::Format_Invalid;
}

QPixmap qt_xcb_pixmapFromXPixmap(QXcbConnection *connection, xcb_pixmap_t pixmap,
                                 int width, int height, int depth,
                                 const xcb_visualtype_t *visual)
{
    auto conn = connection->xcb_connection();

    auto get_image_cookie =
        xcb_get_image_unchecked(conn, XCB_IMAGE_FORMAT_Z_PIXMAP, pixmap,
                      0, 0, width, height, 0xffffffff);

    auto image_reply =
        xcb_get_image_reply(conn, get_image_cookie, NULL);

    if (!image_reply) {
        return QPixmap();
    }

    auto data = xcb_get_image_data(image_reply);
    uint32_t length = xcb_get_image_data_length(image_reply);

    QPixmap result;

    auto format = qt_xcb_imageFormatForVisual(connection, depth, visual);
    if (format != QImage::Format_Invalid) {
        auto bytes_per_line = length / height;
        QImage image(const_cast<uint8_t *>(data), width, height, bytes_per_line, format);
        auto image_byte_order = connection->setup()->image_byte_order;

        // we may have to swap the byte order
        if ((QSysInfo::ByteOrder == QSysInfo::LittleEndian && image_byte_order == XCB_IMAGE_ORDER_MSB_FIRST)
            || (QSysInfo::ByteOrder == QSysInfo::BigEndian && image_byte_order == XCB_IMAGE_ORDER_LSB_FIRST))
        {
            for (auto i=0; i < image.height(); i++) {
                switch (format) {
                case QImage::Format_RGB16: {
                    auto p = (ushort*)image.scanLine(i);
                    auto end = p + image.width();
                    while (p < end) {
                        *p = ((*p << 8) & 0xff00) | ((*p >> 8) & 0x00ff);
                        p++;
                    }
                    break;
                }
                case QImage::Format_RGB32: // fall-through
                case QImage::Format_ARGB32_Premultiplied: {
                    auto p = (uint*)image.scanLine(i);
                    auto end = p + image.width();
                    while (p < end) {
                        *p = ((*p << 24) & 0xff000000) | ((*p << 8) & 0x00ff0000)
                            | ((*p >> 8) & 0x0000ff00) | ((*p >> 24) & 0x000000ff);
                        p++;
                    }
                    break;
                }
                default:
                    Q_ASSERT(false);
                }
            }
        }

        // fix-up alpha channel
        if (format == QImage::Format_RGB32) {
            auto p = (QRgb *)image.bits();
            for (auto y = 0; y < height; ++y) {
                for (auto x = 0; x < width; ++x)
                    p[x] |= 0xff000000;
                p += bytes_per_line / 4;
            }
        } else if (format == QImage::Format_BGR30 || format == QImage::Format_RGB30) {
            auto p = (QRgb *)image.bits();
            for (auto y = 0; y < height; ++y) {
                for (auto x = 0; x < width; ++x)
                    p[x] |= 0xc0000000;
                p += bytes_per_line / 4;
            }
        }

        result = QPixmap::fromImage(image.copy());
    }

    free(image_reply);
    return result;
}

xcb_pixmap_t qt_xcb_XPixmapFromBitmap(QXcbScreen *screen, const QImage &image)
{
    auto conn = screen->xcb_connection();
    auto bitmap = image.convertToFormat(QImage::Format_MonoLSB);
    const auto c0 = QColor(Qt::black).rgb();
    const auto c1 = QColor(Qt::white).rgb();
    if (bitmap.color(0) == c0 && bitmap.color(1) == c1) {
        bitmap.invertPixels();
        bitmap.setColor(0, c1);
        bitmap.setColor(1, c0);
    }
    const auto width = bitmap.width();
    const auto height = bitmap.height();
    const auto bytesPerLine = bitmap.bytesPerLine();
    auto destLineSize = width / 8;
    if (width % 8)
        ++destLineSize;
    auto map = bitmap.bits();
    auto buf = new uint8_t[height * destLineSize];
    for (auto i = 0; i < height; i++)
        memcpy(buf + (destLineSize * i), map + (bytesPerLine * i), destLineSize);
    auto pm = xcb_create_pixmap_from_bitmap_data(conn, screen->root(), buf,
                                                         width, height, 1, 0, 0, 0);
    delete[] buf;
    return pm;
}

xcb_cursor_t qt_xcb_createCursorXRender(QXcbScreen *screen, const QImage &image,
                                        const QPoint &spot)
{
#ifdef XCB_USE_RENDER
    auto conn = screen->xcb_connection();
    const auto w = image.width();
    const auto h = image.height();
    xcb_generic_error_t *error = 0;
    auto formatsCookie = xcb_render_query_pict_formats(conn);
    auto formatsReply = xcb_render_query_pict_formats_reply(conn,
                                                                                              formatsCookie,
                                                                                              &error);
    if (!formatsReply || error) {
        qWarning("qt_xcb_createCursorXRender: query_pict_formats failed");
        free(formatsReply);
        free(error);
        return XCB_NONE;
    }
    auto fmt = xcb_render_util_find_standard_format(formatsReply,
                                                                          XCB_PICT_STANDARD_ARGB_32);
    if (!fmt) {
        qWarning("qt_xcb_createCursorXRender: Failed to find format PICT_STANDARD_ARGB_32");
        free(formatsReply);
        return XCB_NONE;
    }

    auto img = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    auto xi = xcb_image_create(w, h, XCB_IMAGE_FORMAT_Z_PIXMAP,
                                       32, 32, 32, 32,
                                       QSysInfo::ByteOrder == QSysInfo::BigEndian ? XCB_IMAGE_ORDER_MSB_FIRST : XCB_IMAGE_ORDER_LSB_FIRST,
                                       XCB_IMAGE_ORDER_MSB_FIRST,
                                       0, 0, 0);
    if (!xi) {
        qWarning("qt_xcb_createCursorXRender: xcb_image_create failed");
        free(formatsReply);
        return XCB_NONE;
    }
    xi->data = (uint8_t *) malloc(xi->stride * h);
    if (!xi->data) {
        qWarning("qt_xcb_createCursorXRender: Failed to malloc() image data");
        xcb_image_destroy(xi);
        free(formatsReply);
        return XCB_NONE;
    }
    memcpy(xi->data, img.constBits(), img.byteCount());

    auto pix = xcb_generate_id(conn);
    xcb_create_pixmap(conn, 32, pix, screen->root(), w, h);

    auto pic = xcb_generate_id(conn);
    xcb_render_create_picture(conn, pic, pix, fmt->id, 0, 0);

    auto gc = xcb_generate_id(conn);
    xcb_create_gc(conn, gc, pix, 0, 0);
    xcb_image_put(conn, pix, gc, xi, 0, 0, 0);
    xcb_free_gc(conn, gc);

    auto cursor = xcb_generate_id(conn);
    xcb_render_create_cursor(conn, cursor, pic, spot.x(), spot.y());

    free(xi->data);
    xcb_image_destroy(xi);
    xcb_render_free_picture(conn, pic);
    xcb_free_pixmap(conn, pix);
    free(formatsReply);
    return cursor;

#else
    Q_UNUSED(screen);
    Q_UNUSED(image);
    Q_UNUSED(spot);
    return XCB_NONE;
#endif
}

QT_END_NAMESPACE
