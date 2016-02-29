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

#include "qimagescale_p.h"
#include "qimage.h"
#include <private/qsimd_p.h>

#if defined(QT_COMPILER_SUPPORTS_SSE4_1)

QT_BEGIN_NAMESPACE

using namespace QImageScale;

inline static __m128i qt_qimageScaleAARGBA_helper(const unsigned int *pix, int xyap, int Cxy, int step, const __m128i vxyap, const __m128i vCxy)
{
    auto vpix = _mm_cvtepu8_epi32(_mm_cvtsi32_si128(*pix));
    auto vx = _mm_mullo_epi32(vpix, vxyap);
    int i;
    for (i = (1 << 14) - xyap; i > Cxy; i -= Cxy) {
        pix += step;
        vpix = _mm_cvtepu8_epi32(_mm_cvtsi32_si128(*pix));
        vx = _mm_add_epi32(vx, _mm_mullo_epi32(vpix, vCxy));
    }
    pix += step;
    vpix = _mm_cvtepu8_epi32(_mm_cvtsi32_si128(*pix));
    vx = _mm_add_epi32(vx, _mm_mullo_epi32(vpix, _mm_set1_epi32(i)));
    return vx;
}

template<bool RGB>
void qt_qimageScaleAARGBA_up_x_down_y_sse4(QImageScaleInfo *isi, unsigned int *dest,
                                           int dw, int dh, int dow, int sow)
{
    auto ypoints = isi->ypoints;
    auto xpoints = isi->xpoints;
    auto xapoints = isi->xapoints;
    auto yapoints = isi->yapoints;

    const auto v256 = _mm_set1_epi32(256);

    /* go through every scanline in the output buffer */
    for (auto y = 0; y < dh; y++) {
        auto Cy = yapoints[y] >> 16;
        auto yap = yapoints[y] & 0xffff;
        const auto vCy = _mm_set1_epi32(Cy);
        const auto vyap = _mm_set1_epi32(yap);

        auto dptr = dest + (y * dow);
        for (auto x = 0; x < dw; x++) {
            auto sptr = ypoints[y] + xpoints[x];
            auto vx = qt_qimageScaleAARGBA_helper(sptr, yap, Cy, sow, vyap, vCy);

            auto xap = xapoints[x];
            if (xap > 0) {
                const auto vxap = _mm_set1_epi32(xap);
                const auto vinvxap = _mm_sub_epi32(v256, vxap);
                auto vr = qt_qimageScaleAARGBA_helper(sptr + 1, yap, Cy, sow, vyap, vCy);

                vx = _mm_mullo_epi32(vx, vinvxap);
                vr = _mm_mullo_epi32(vr, vxap);
                vx = _mm_add_epi32(vx, vr);
                vx = _mm_srli_epi32(vx, 8);
            }
            vx = _mm_srli_epi32(vx, 14);
            vx = _mm_packus_epi32(vx, _mm_setzero_si128());
            vx = _mm_packus_epi16(vx, _mm_setzero_si128());
            *dptr = _mm_cvtsi128_si32(vx);
            if (RGB)
                *dptr |= 0xff000000;
            dptr++;
        }
    }
}

template<bool RGB>
void qt_qimageScaleAARGBA_down_x_up_y_sse4(QImageScaleInfo *isi, unsigned int *dest,
                                           int dw, int dh, int dow, int sow)
{
    auto ypoints = isi->ypoints;
    auto xpoints = isi->xpoints;
    auto xapoints = isi->xapoints;
    auto yapoints = isi->yapoints;

    const auto v256 = _mm_set1_epi32(256);

    /* go through every scanline in the output buffer */
    for (auto y = 0; y < dh; y++) {
        auto dptr = dest + (y * dow);
        for (auto x = 0; x < dw; x++) {
            auto Cx = xapoints[x] >> 16;
            auto xap = xapoints[x] & 0xffff;
            const auto vCx = _mm_set1_epi32(Cx);
            const auto vxap = _mm_set1_epi32(xap);

            auto sptr = ypoints[y] + xpoints[x];
            auto vx = qt_qimageScaleAARGBA_helper(sptr, xap, Cx, 1, vxap, vCx);

            auto yap = yapoints[y];
            if (yap > 0) {
                const auto vyap = _mm_set1_epi32(yap);
                const auto vinvyap = _mm_sub_epi32(v256, vyap);
                auto vr = qt_qimageScaleAARGBA_helper(sptr + sow, xap, Cx, 1, vxap, vCx);

                vx = _mm_mullo_epi32(vx, vinvyap);
                vr = _mm_mullo_epi32(vr, vyap);
                vx = _mm_add_epi32(vx, vr);
                vx = _mm_srli_epi32(vx, 8);
            }
            vx = _mm_srli_epi32(vx, 14);
            vx = _mm_packus_epi32(vx, _mm_setzero_si128());
            vx = _mm_packus_epi16(vx, _mm_setzero_si128());
            *dptr = _mm_cvtsi128_si32(vx);
            if (RGB)
                *dptr |= 0xff000000;
            dptr++;
        }
    }
}

template<bool RGB>
void qt_qimageScaleAARGBA_down_xy_sse4(QImageScaleInfo *isi, unsigned int *dest,
                                       int dw, int dh, int dow, int sow)
{
    auto ypoints = isi->ypoints;
    auto xpoints = isi->xpoints;
    auto xapoints = isi->xapoints;
    auto yapoints = isi->yapoints;

    for (auto y = 0; y < dh; y++) {
        auto Cy = yapoints[y] >> 16;
        auto yap = yapoints[y] & 0xffff;
        const auto vCy = _mm_set1_epi32(Cy);
        const auto vyap = _mm_set1_epi32(yap);

        auto dptr = dest + (y * dow);
        for (auto x = 0; x < dw; x++) {
            const auto Cx = xapoints[x] >> 16;
            const auto xap = xapoints[x] & 0xffff;
            const auto vCx = _mm_set1_epi32(Cx);
            const auto vxap = _mm_set1_epi32(xap);

            auto sptr = ypoints[y] + xpoints[x];
            auto vx = qt_qimageScaleAARGBA_helper(sptr, xap, Cx, 1, vxap, vCx);
            auto vr = _mm_mullo_epi32(_mm_srli_epi32(vx, 4), vyap);

            int j;
            for (j = (1 << 14) - yap; j > Cy; j -= Cy) {
                sptr += sow;
                vx = qt_qimageScaleAARGBA_helper(sptr, xap, Cx, 1, vxap, vCx);
                vr = _mm_add_epi32(vr, _mm_mullo_epi32(_mm_srli_epi32(vx, 4), vCy));
            }
            sptr += sow;
            vx = qt_qimageScaleAARGBA_helper(sptr, xap, Cx, 1, vxap, vCx);
            vr = _mm_add_epi32(vr, _mm_mullo_epi32(_mm_srli_epi32(vx, 4), _mm_set1_epi32(j)));

            vr = _mm_srli_epi32(vr, 24);
            vr = _mm_packus_epi32(vr, _mm_setzero_si128());
            vr = _mm_packus_epi16(vr, _mm_setzero_si128());
            *dptr = _mm_cvtsi128_si32(vr);
            if (RGB)
                *dptr |= 0xff000000;
            dptr++;
        }
    }
}

template void qt_qimageScaleAARGBA_up_x_down_y_sse4<false>(QImageScaleInfo *isi, unsigned int *dest,
                                                           int dw, int dh, int dow, int sow);

template void qt_qimageScaleAARGBA_up_x_down_y_sse4<true>(QImageScaleInfo *isi, unsigned int *dest,
                                                          int dw, int dh, int dow, int sow);

template void qt_qimageScaleAARGBA_down_x_up_y_sse4<false>(QImageScaleInfo *isi, unsigned int *dest,
                                                           int dw, int dh, int dow, int sow);

template void qt_qimageScaleAARGBA_down_x_up_y_sse4<true>(QImageScaleInfo *isi, unsigned int *dest,
                                                          int dw, int dh, int dow, int sow);

template void qt_qimageScaleAARGBA_down_xy_sse4<false>(QImageScaleInfo *isi, unsigned int *dest,
                                                       int dw, int dh, int dow, int sow);

template void qt_qimageScaleAARGBA_down_xy_sse4<true>(QImageScaleInfo *isi, unsigned int *dest,
                                                      int dw, int dh, int dow, int sow);

QT_END_NAMESPACE

#endif
