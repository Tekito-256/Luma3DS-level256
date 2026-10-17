/*
*   This file is part of Luma3DS (Level256 fork)
*   Copyright (C) 2026 Level256
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include "rp_jpeg.h"

#define MCU_MARGIN 4096

static const uint8_t zigzag[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63,
};

static const uint8_t baseQuantY[64] = {
    16, 11, 10, 16,  24,  40,  51,  61,
    12, 12, 14, 19,  26,  58,  60,  55,
    14, 13, 16, 24,  40,  57,  69,  56,
    14, 17, 22, 29,  51,  87,  80,  62,
    18, 22, 37, 56,  68, 109, 103,  77,
    24, 35, 55, 64,  81, 104, 113,  92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103,  99,
};

static const uint8_t baseQuantC[64] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
};

static const uint8_t dcYBits[16] = { 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t dcCBits[16] = { 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
static const uint8_t dcVals[12]  = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

static const uint8_t acYBits[16] = { 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7D };
static const uint8_t acYVals[162] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
    0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
    0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA,
};

static const uint8_t acCBits[16] = { 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
static const uint8_t acCVals[162] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0,
    0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26,
    0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
    0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
    0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
    0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA,
};

static const double aanScale[8] = {
    1.0, 1.387039845, 1.306562965, 1.175875602, 1.0, 0.785694958, 0.541196100, 0.275899379,
};

typedef RpJpegHuffTable HuffTable;

static void buildHuff(HuffTable *t, const uint8_t *bits, const uint8_t *vals)
{
    uint32_t code = 0, k = 0;

    memset(t, 0, sizeof(*t));
    for (uint32_t len = 1; len <= 16; len++)
    {
        for (uint32_t i = 0; i < bits[len - 1]; i++)
        {
            t->code[vals[k]] = (uint16_t)code++;
            t->size[vals[k]] = (uint8_t)len;
            k++;
        }
        code <<= 1;
    }
}

static void buildQuant(uint16_t *recip, uint8_t *dqt, const uint8_t *base, int quality)
{
    int scale = quality < 50 ? 5000 / quality : 200 - 2 * quality;

    for (int i = 0; i < 64; i++)
    {
        int q = (base[i] * scale + 50) / 100;
        if (q < 1)
            q = 1;
        else if (q > 255)
            q = 255;

        double divisor = q * aanScale[i >> 3] * aanScale[i & 7] * 8.0;
        recip[i] = (uint16_t)(32768.0 / divisor + 0.5);
    }

    for (int k = 0; k < 64; k++)
    {
        int q = (base[zigzag[k]] * scale + 50) / 100;
        dqt[k] = (uint8_t)(q < 1 ? 1 : q > 255 ? 255 : q);
    }
}

void RpJpeg_SetQuality(RpJpeg *enc, int quality)
{
    buildHuff(&enc->dcY, dcYBits, dcVals);
    buildHuff(&enc->dcC, dcCBits, dcVals);
    buildHuff(&enc->acY, acYBits, acYVals);
    buildHuff(&enc->acC, acCBits, acCVals);

    if (quality < 1)
        quality = 1;
    else if (quality > 100)
        quality = 100;

    enc->quality = quality;
    buildQuant(enc->recipY, enc->dqtY, baseQuantY, quality);
    buildQuant(enc->recipC, enc->dqtC, baseQuantC, quality);
}

uint32_t RpJpeg_BytesPerPixel(int format)
{
    switch (format)
    {
        case RPJPEG_FMT_RGBA8:  return 4;
        case RPJPEG_FMT_BGR8:   return 3;
        case RPJPEG_FMT_RGB565:
        case RPJPEG_FMT_RGB5A1:
        case RPJPEG_FMT_RGBA4:  return 2;
        default:                return 0;
    }
}

typedef struct BitWriter
{
    uint8_t *p;
    uint32_t acc;
    uint32_t n;
} BitWriter;

static inline void putBits(BitWriter *w, uint32_t bits, uint32_t len)
{
    w->acc = (w->acc << len) | bits;
    w->n += len;
    while (w->n >= 8)
    {
        w->n -= 8;
        uint8_t b = (uint8_t)(w->acc >> w->n);
        *w->p++ = b;
        if (b == 0xFF)
            *w->p++ = 0;
    }
}

static inline uint32_t bitLength(uint32_t v)
{
    return v == 0 ? 0 : 32 - (uint32_t)__builtin_clz(v);
}

#define MUL8(x, c) (((x) * (c)) >> 8)
#define C_0_382683433 98
#define C_0_541196100 139
#define C_0_707106781 181
#define C_1_306562965 334

static inline void fdct1d(int32_t *d, uint32_t s)
{
    int32_t tmp0 = d[0 * s] + d[7 * s], tmp7 = d[0 * s] - d[7 * s];
    int32_t tmp1 = d[1 * s] + d[6 * s], tmp6 = d[1 * s] - d[6 * s];
    int32_t tmp2 = d[2 * s] + d[5 * s], tmp5 = d[2 * s] - d[5 * s];
    int32_t tmp3 = d[3 * s] + d[4 * s], tmp4 = d[3 * s] - d[4 * s];

    int32_t tmp10 = tmp0 + tmp3, tmp13 = tmp0 - tmp3;
    int32_t tmp11 = tmp1 + tmp2, tmp12 = tmp1 - tmp2;

    d[0 * s] = tmp10 + tmp11;
    d[4 * s] = tmp10 - tmp11;

    int32_t z1 = MUL8(tmp12 + tmp13, C_0_707106781);
    d[2 * s] = tmp13 + z1;
    d[6 * s] = tmp13 - z1;

    tmp10 = tmp4 + tmp5;
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    int32_t z5 = MUL8(tmp10 - tmp12, C_0_382683433);
    int32_t z2 = MUL8(tmp10, C_0_541196100) + z5;
    int32_t z4 = MUL8(tmp12, C_1_306562965) + z5;
    int32_t z3 = MUL8(tmp11, C_0_707106781);

    int32_t z11 = tmp7 + z3, z13 = tmp7 - z3;

    d[5 * s] = z13 + z2;
    d[3 * s] = z13 - z2;
    d[1 * s] = z11 + z4;
    d[7 * s] = z11 - z4;
}

static int32_t encodeBlock(BitWriter *w, int32_t *blk, const uint16_t *recip, int32_t prevDc,
                           const HuffTable *dc, const HuffTable *ac)
{
    for (uint32_t i = 0; i < 8; i++)
        fdct1d(blk + 8 * i, 1);
    for (uint32_t i = 0; i < 8; i++)
        fdct1d(blk + i, 8);

    int32_t q[64];
    for (uint32_t k = 0; k < 64; k++)
    {
        uint32_t i = zigzag[k];
        int32_t v = blk[i];
        uint32_t m = ((uint32_t)(v < 0 ? -v : v) * recip[i] + 0x4000) >> 15;
        if (m > 1023)
            m = 1023;
        q[k] = v < 0 ? -(int32_t)m : (int32_t)m;
    }

    int32_t diff = q[0] - prevDc;
    uint32_t mag = (uint32_t)(diff < 0 ? -diff : diff);
    uint32_t nbits = bitLength(mag);
    putBits(w, dc->code[nbits], dc->size[nbits]);
    if (nbits != 0)
        putBits(w, (uint32_t)(diff < 0 ? diff - 1 : diff) & ((1u << nbits) - 1), nbits);

    uint32_t run = 0;
    for (uint32_t k = 1; k < 64; k++)
    {
        int32_t v = q[k];
        if (v == 0)
        {
            run++;
            continue;
        }

        while (run >= 16)
        {
            putBits(w, ac->code[0xF0], ac->size[0xF0]);
            run -= 16;
        }

        mag = (uint32_t)(v < 0 ? -v : v);
        nbits = bitLength(mag);
        uint32_t sym = (run << 4) | nbits;
        putBits(w, ac->code[sym], ac->size[sym]);
        putBits(w, (uint32_t)(v < 0 ? v - 1 : v) & ((1u << nbits) - 1), nbits);
        run = 0;
    }

    if (run != 0)
        putBits(w, ac->code[0x00], ac->size[0x00]);

    return q[0];
}

static inline void loadPixels(const uint8_t *p, int format, uint8_t *r, uint8_t *g, uint8_t *b)
{
    switch (format)
    {
        case RPJPEG_FMT_RGBA8:
            for (uint32_t i = 0; i < 16; i++, p += 4)
            {
                b[i] = p[1];
                g[i] = p[2];
                r[i] = p[3];
            }
            break;
        case RPJPEG_FMT_BGR8:
            for (uint32_t i = 0; i < 16; i++, p += 3)
            {
                b[i] = p[0];
                g[i] = p[1];
                r[i] = p[2];
            }
            break;
        case RPJPEG_FMT_RGB565:
            for (uint32_t i = 0; i < 16; i++, p += 2)
            {
                uint32_t px = p[0] | (p[1] << 8);
                uint32_t r5 = px >> 11, g6 = (px >> 5) & 0x3F, b5 = px & 0x1F;
                r[i] = (uint8_t)((r5 << 3) | (r5 >> 2));
                g[i] = (uint8_t)((g6 << 2) | (g6 >> 4));
                b[i] = (uint8_t)((b5 << 3) | (b5 >> 2));
            }
            break;
        case RPJPEG_FMT_RGB5A1:
            for (uint32_t i = 0; i < 16; i++, p += 2)
            {
                uint32_t px = p[0] | (p[1] << 8);
                uint32_t r5 = px >> 11, g5 = (px >> 6) & 0x1F, b5 = (px >> 1) & 0x1F;
                r[i] = (uint8_t)((r5 << 3) | (r5 >> 2));
                g[i] = (uint8_t)((g5 << 3) | (g5 >> 2));
                b[i] = (uint8_t)((b5 << 3) | (b5 >> 2));
            }
            break;
        default:
            for (uint32_t i = 0; i < 16; i++, p += 2)
            {
                uint32_t px = p[0] | (p[1] << 8);
                uint32_t r4 = px >> 12, g4 = (px >> 8) & 0xF, b4 = (px >> 4) & 0xF;
                r[i] = (uint8_t)((r4 << 4) | r4);
                g[i] = (uint8_t)((g4 << 4) | g4);
                b[i] = (uint8_t)((b4 << 4) | b4);
            }
            break;
    }
}

static inline int32_t lumaOf(uint32_t r, uint32_t g, uint32_t b)
{
    return (int32_t)((77 * r + 150 * g + 29 * b + 128) >> 8) - 128;
}

static void loadMcu(int32_t yb[4][64], int32_t *cb, int32_t *cr, const uint8_t *src,
                    uint32_t rowPitch, int format)
{
    uint8_t r0[16], g0[16], b0[16], r1[16], g1[16], b1[16];

    for (uint32_t y = 0; y < 16; y += 2)
    {
        loadPixels(src + y * rowPitch, format, r0, g0, b0);
        loadPixels(src + (y + 1) * rowPitch, format, r1, g1, b1);

        int32_t *yRow0 = yb[(y >> 3) * 2] + (y & 7) * 8;
        int32_t *yRow1 = yRow0 + 8;
        int32_t *cbRow = cb + (y >> 1) * 8;
        int32_t *crRow = cr + (y >> 1) * 8;

        for (uint32_t x = 0; x < 16; x += 2)
        {
            uint32_t bx = (x >> 3) * 64;
            uint32_t ix = x & 7;

            yRow0[bx + ix]     = lumaOf(r0[x], g0[x], b0[x]);
            yRow0[bx + ix + 1] = lumaOf(r0[x + 1], g0[x + 1], b0[x + 1]);
            yRow1[bx + ix]     = lumaOf(r1[x], g1[x], b1[x]);
            yRow1[bx + ix + 1] = lumaOf(r1[x + 1], g1[x + 1], b1[x + 1]);

            int32_t sr = r0[x] + r0[x + 1] + r1[x] + r1[x + 1];
            int32_t sg = g0[x] + g0[x + 1] + g1[x] + g1[x + 1];
            int32_t sb = b0[x] + b0[x + 1] + b1[x] + b1[x + 1];

            cbRow[x >> 1] = (-43 * sr - 85 * sg + 128 * sb + 512) >> 10;
            crRow[x >> 1] = (128 * sr - 107 * sg - 21 * sb + 512) >> 10;
        }
    }
}

static uint8_t *putMarkerSegment(uint8_t *p, uint8_t marker, uint32_t length)
{
    *p++ = 0xFF;
    *p++ = marker;
    *p++ = (uint8_t)(length >> 8);
    *p++ = (uint8_t)length;
    return p;
}

static uint8_t *putHuffTable(uint8_t *p, uint8_t classAndId, const uint8_t *bits, const uint8_t *vals, uint32_t count)
{
    *p++ = classAndId;
    memcpy(p, bits, 16);
    p += 16;
    memcpy(p, vals, count);
    return p + count;
}

static uint8_t *writeHeaders(const RpJpeg *enc, uint8_t *p, uint32_t width, uint32_t height)
{
    static const uint8_t jfif[14] = { 'J', 'F', 'I', 'F', 0, 1, 1, 0, 0, 1, 0, 1, 0, 0 };

    *p++ = 0xFF;
    *p++ = 0xD8;

    p = putMarkerSegment(p, 0xE0, 2 + sizeof(jfif));
    memcpy(p, jfif, sizeof(jfif));
    p += sizeof(jfif);

    p = putMarkerSegment(p, 0xDB, 2 + 2 * 65);
    *p++ = 0;
    memcpy(p, enc->dqtY, 64);
    p += 64;
    *p++ = 1;
    memcpy(p, enc->dqtC, 64);
    p += 64;

    p = putMarkerSegment(p, 0xC0, 17);
    *p++ = 8;
    *p++ = (uint8_t)(height >> 8);
    *p++ = (uint8_t)height;
    *p++ = (uint8_t)(width >> 8);
    *p++ = (uint8_t)width;
    *p++ = 3;
    *p++ = 1; *p++ = 0x22; *p++ = 0;
    *p++ = 2; *p++ = 0x11; *p++ = 1;
    *p++ = 3; *p++ = 0x11; *p++ = 1;

    p = putMarkerSegment(p, 0xC4, 2 + 4 * 17 + 2 * 12 + 2 * 162);
    p = putHuffTable(p, 0x00, dcYBits, dcVals, 12);
    p = putHuffTable(p, 0x10, acYBits, acYVals, 162);
    p = putHuffTable(p, 0x01, dcCBits, dcVals, 12);
    p = putHuffTable(p, 0x11, acCBits, acCVals, 162);

    p = putMarkerSegment(p, 0xDA, 12);
    *p++ = 3;
    *p++ = 1; *p++ = 0x00;
    *p++ = 2; *p++ = 0x11;
    *p++ = 3; *p++ = 0x11;
    *p++ = 0;
    *p++ = 63;
    *p++ = 0;

    return p;
}

uint32_t RpJpeg_Encode(const RpJpeg *enc, uint8_t *out, uint32_t outCap, const uint8_t *src,
                       uint32_t width, uint32_t rows, uint32_t stride, uint32_t rowStep, int format)
{
    uint32_t bpp = RpJpeg_BytesPerPixel(format);

    if (enc->quality == 0 || bpp == 0 || width == 0 || rows == 0 || (width & 15) != 0 || (rows & 15) != 0
        || rows > 0xFFFF || rowStep == 0 || outCap < 1024 + MCU_MARGIN)
        return 0;

    uint8_t *end = out + outCap;
    BitWriter w = { writeHeaders(enc, out, width, rows), 0, 0 };

    int32_t yb[4][64], cb[64], cr[64];
    int32_t dcY = 0, dcCb = 0, dcCr = 0;
    uint32_t rowPitch = stride * rowStep;

    for (uint32_t my = 0; my < rows; my += 16)
    {
        for (uint32_t mx = 0; mx < width; mx += 16)
        {
            if ((uint32_t)(end - w.p) < MCU_MARGIN)
                return 0;

            loadMcu(yb, cb, cr, src + my * rowPitch + mx * bpp, rowPitch, format);

            for (uint32_t i = 0; i < 4; i++)
                dcY = encodeBlock(&w, yb[i], enc->recipY, dcY, &enc->dcY, &enc->acY);
            dcCb = encodeBlock(&w, cb, enc->recipC, dcCb, &enc->dcC, &enc->acC);
            dcCr = encodeBlock(&w, cr, enc->recipC, dcCr, &enc->dcC, &enc->acC);
        }
    }

    if (w.n != 0)
        putBits(&w, (1u << (8 - w.n)) - 1, 8 - w.n);

    *w.p++ = 0xFF;
    *w.p++ = 0xD9;

    return (uint32_t)(w.p - out);
}
