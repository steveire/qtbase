/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qimage.h"
#include <private/qimage_p.h>
#include <private/qsimd_p.h>
#include <private/qdrawhelper_p.h>
#include <private/qdrawingprimitive_sse2_p.h>

#ifdef QT_COMPILER_SUPPORTS_SSE2

QT_BEGIN_NAMESPACE

bool convert_ARGB_to_ARGB_PM_inplace_sse2(QImageData *data, Qt::ImageConversionFlags)
{
    Q_ASSERT(data->format == QImage::Format_ARGB32 || data->format == QImage::Format_RGBA8888);

    // extra pixels on each line
    const auto spare = data->width & 3;
    // width in pixels of the pad at the end of each line
    const auto pad = (data->bytes_per_line >> 2) - data->width;
    const auto iter = data->width >> 2;
    auto height = data->height;

    const auto alphaMask = _mm_set1_epi32(0xff000000);
    const auto nullVector = _mm_setzero_si128();
    const auto half = _mm_set1_epi16(0x80);
    const auto colorMask = _mm_set1_epi32(0x00ff00ff);

    auto d = reinterpret_cast<__m128i*>(data->data);
    while (height--) {
        auto end = d + iter;

        for (; d != end; ++d) {
            const auto srcVector = _mm_loadu_si128(d);
            const auto srcVectorAlpha = _mm_and_si128(srcVector, alphaMask);
            if (_mm_movemask_epi8(_mm_cmpeq_epi32(srcVectorAlpha, alphaMask)) == 0xffff) {
                // opaque, data is unchanged
            } else if (_mm_movemask_epi8(_mm_cmpeq_epi32(srcVectorAlpha, nullVector)) == 0xffff) {
                // fully transparent
                _mm_storeu_si128(d, nullVector);
            } else {
                auto alphaChannel = _mm_srli_epi32(srcVector, 24);
                alphaChannel = _mm_or_si128(alphaChannel, _mm_slli_epi32(alphaChannel, 16));

                __m128i result;
                BYTE_MUL_SSE2(result, srcVector, alphaChannel, colorMask, half);
                result = _mm_or_si128(_mm_andnot_si128(alphaMask, result), srcVectorAlpha);
                _mm_storeu_si128(d, result);
            }
        }

        auto p = reinterpret_cast<QRgb*>(d);
        auto pe = p+spare;
        for (; p != pe; ++p) {
            if (*p < 0x00ffffff)
                *p = 0;
            else if (*p < 0xff000000)
                *p = qPremultiply(*p);
        }

        d = reinterpret_cast<__m128i*>(p+pad);
    }

    if (data->format == QImage::Format_ARGB32)
        data->format = QImage::Format_ARGB32_Premultiplied;
    else
        data->format = QImage::Format_RGBA8888_Premultiplied;
    return true;
}

QT_END_NAMESPACE

#endif // QT_COMPILER_SUPPORTS_SSE2
