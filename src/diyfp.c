// See LICENCE
//
// Portions converted to C from JapidJSON C++
//
// Tencent is pleased to support the open source community by making RapidJSON available.
//
// Copyright (C) 2015 THL A29 Limited, a Tencent company, and Milo Yip.
//
// Licensed under the MIT License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

// This is a C++ header-only implementation of Grisu2 algorithm from the publication:
// Loitsch, Florian. "Printing floating-point numbers quickly and accurately with
// integers." ACM Sigplan Notices 45.6 (2010): 233-243.

#include <math.h>

#define UINT64_C2 (high32, low32) (((uint64_t)high32) << 32) | (uint64_t)low32)

typedef struct {
        uint64_t f;
        int exp;
} DiyFp;

static DiyFp diyfp_from_double(double d)
{
        uint64_t f;
        int e;

        union {
                double d;
                uint64_t u64;
        } u = { d };

        int biased_e = (int)((u.u64 & kDpExponentMask) >> kDpSignificandSize);
        uint64_t significand = (u.u64 & kDpSignificandMask);
        if (biased_e != 0) {
            f = significand + kDpHiddenBit;
            e = biased_e - kDpExponentBias;
        }
        else {
            f = significand;
            e = kDpMinExponent + 1;
        }
        return (DiyFp){ f, e };
}

static DiyFp diyfp_sub(DiyFp lhs, DiyFp rhs)
{
        return (DiyFp){ lhs.f - rhs.f, lhs.e};
}

static DiyFp diyfp_mul(DiyFp lhs, DiyFp rhs)
{
#if __STDC_VERSION__ > 202300L
        typedef unsigned _BitInt(128)  u128

        u128 p = (u128)(f) * (u128)(rhs.f);
        uint64_t h = (uint64_t)(p >> 64);
        uint64_t l = (uint64_t)(p);
        if (l & (uint64_t(1) << 63)) // rounding
            h++;
        return (DiyFp){h, e + rhs.e + 64};
#else
        const uint64_t M32 = 0xFFFFFFFF;
        const uint64_t a = f >> 32;
        const uint64_t b = f & M32;
        const uint64_t c = rhs.f >> 32;
        const uint64_t d = rhs.f & M32;
        const uint64_t ac = a * c;
        const uint64_t bc = b * c;
        const uint64_t ad = a * d;
        const uint64_t bd = b * d;
        uint64_t tmp = (bd >> 32) + (ad & M32) + (bc & M32);
        tmp += 1U << 31;  /// mult_round
        return (DiyFp){ac + (ad >> 32) + (bc >> 32) + (tmp >> 32), e + rhs.e + 64};
#endif
}

static DiyFp diyfp_normalize(DiyFp fp) {
        int s = (int)(clzll(fp.f));
        return (DiyFp){fp.f << s, fp.e - s};
}

static DiyFp diyfp_normalize_boundary(DiyFp fp) {
        DiyFp res = fp;
        while (!(res.f & (kDpHiddenBit << 1))) {
                res.f <<= 1;
                res.e--;
        }
        res.f <<= (kDiySignificandSize - kDpSignificandSize - 2);
        res.e = res.e - (kDiySignificandSize - kDpSignificandSize - 2);
        return res;
}

static void diyfp_normalized_boundaries(DiyFp fp, DiyFp* minus, DiyFp* plus) 
{
        DiyFp pl = diyfp_normalize_boundary((DiyFp){(fp.f << 1) + 1, fp.e - 1});
        DiyFp mi = (fp.f == kDpHiddenBit)
                        ? (DiyFp){(fp.f << 2) - 1, fp.e - 2) 
                        : (DiyFp){(fp.f << 1) - 1, fp.e - 1);
        mi.f <<= mi.e - pl.e;
        mi.e = pl.e;
        *plus = pl;
        *minus = mi;
}

static double diyfp_to_double(DiyFp fp) {
        union {
            double d;
            uint64_t u64;
        } u;

        JSONPG_ASSERT(dp.f <= kDpHiddenBit + kDpSignificandMask);
        if (fp.e < kDpDenormalExponent) {
            // Underflow.
            return 0.0;
        }
        if (fp.e >= kDpMaxExponent) {
            // Overflow.
            return INFINITY;
        }
        const uint64_t be = (fp.e == kDpDenormalExponent && (fp.f & kDpHiddenBit) == 0) 
                                ? 0
                                : (uint64_t)(fp.e + kDpExponentBias);
        u.u64 = (fp.f & kDpSignificandMask) | (be << kDpSignificandSize);
        return u.d;
}

static const int kDiySignificandSize = 64;
static const int kDpSignificandSize = 52;
static const int kDpExponentBias = 0x3FF + kDpSignificandSize;
static const int kDpMaxExponent = 0x7FF - kDpExponentBias;
static const int kDpMinExponent = -kDpExponentBias;
static const int kDpDenormalExponent = -kDpExponentBias + 1;
static const uint64_t kDpExponentMask = UINT64_C2(0x7FF00000, 0x00000000);
static const uint64_t kDpSignificandMask = UINT64_C2(0x000FFFFF, 0xFFFFFFFF);
static const uint64_t kDpHiddenBit = UINT64_C2(0x00100000, 0x00000000);


static inline DiyFp diyfp_get_cached_power_by_index(size_t index) {
    // 10^-348, 10^-340, ..., 10^340
    static const uint64_t k_cached_powers_f[] = {
        UINT64_C2(0xfa8fd5a0, 0x081c0288), UINT64_C2(0xbaaee17f, 0xa23ebf76),
        UINT64_C2(0x8b16fb20, 0x3055ac76), UINT64_C2(0xcf42894a, 0x5dce35ea),
        UINT64_C2(0x9a6bb0aa, 0x55653b2d), UINT64_C2(0xe61acf03, 0x3d1a45df),
        UINT64_C2(0xab70fe17, 0xc79ac6ca), UINT64_C2(0xff77b1fc, 0xbebcdc4f),
        UINT64_C2(0xbe5691ef, 0x416bd60c), UINT64_C2(0x8dd01fad, 0x907ffc3c),
        UINT64_C2(0xd3515c28, 0x31559a83), UINT64_C2(0x9d71ac8f, 0xada6c9b5),
        UINT64_C2(0xea9c2277, 0x23ee8bcb), UINT64_C2(0xaecc4991, 0x4078536d),
        UINT64_C2(0x823c1279, 0x5db6ce57), UINT64_C2(0xc2109436, 0x4dfb5637),
        UINT64_C2(0x9096ea6f, 0x3848984f), UINT64_C2(0xd77485cb, 0x25823ac7),
        UINT64_C2(0xa086cfcd, 0x97bf97f4), UINT64_C2(0xef340a98, 0x172aace5),
        UINT64_C2(0xb23867fb, 0x2a35b28e), UINT64_C2(0x84c8d4df, 0xd2c63f3b),
        UINT64_C2(0xc5dd4427, 0x1ad3cdba), UINT64_C2(0x936b9fce, 0xbb25c996),
        UINT64_C2(0xdbac6c24, 0x7d62a584), UINT64_C2(0xa3ab6658, 0x0d5fdaf6),
        UINT64_C2(0xf3e2f893, 0xdec3f126), UINT64_C2(0xb5b5ada8, 0xaaff80b8),
        UINT64_C2(0x87625f05, 0x6c7c4a8b), UINT64_C2(0xc9bcff60, 0x34c13053),
        UINT64_C2(0x964e858c, 0x91ba2655), UINT64_C2(0xdff97724, 0x70297ebd),
        UINT64_C2(0xa6dfbd9f, 0xb8e5b88f), UINT64_C2(0xf8a95fcf, 0x88747d94),
        UINT64_C2(0xb9447093, 0x8fa89bcf), UINT64_C2(0x8a08f0f8, 0xbf0f156b),
        UINT64_C2(0xcdb02555, 0x653131b6), UINT64_C2(0x993fe2c6, 0xd07b7fac),
        UINT64_C2(0xe45c10c4, 0x2a2b3b06), UINT64_C2(0xaa242499, 0x697392d3),
        UINT64_C2(0xfd87b5f2, 0x8300ca0e), UINT64_C2(0xbce50864, 0x92111aeb),
        UINT64_C2(0x8cbccc09, 0x6f5088cc), UINT64_C2(0xd1b71758, 0xe219652c),
        UINT64_C2(0x9c400000, 0x00000000), UINT64_C2(0xe8d4a510, 0x00000000),
        UINT64_C2(0xad78ebc5, 0xac620000), UINT64_C2(0x813f3978, 0xf8940984),
        UINT64_C2(0xc097ce7b, 0xc90715b3), UINT64_C2(0x8f7e32ce, 0x7bea5c70),
        UINT64_C2(0xd5d238a4, 0xabe98068), UINT64_C2(0x9f4f2726, 0x179a2245),
        UINT64_C2(0xed63a231, 0xd4c4fb27), UINT64_C2(0xb0de6538, 0x8cc8ada8),
        UINT64_C2(0x83c7088e, 0x1aab65db), UINT64_C2(0xc45d1df9, 0x42711d9a),
        UINT64_C2(0x924d692c, 0xa61be758), UINT64_C2(0xda01ee64, 0x1a708dea),
        UINT64_C2(0xa26da399, 0x9aef774a), UINT64_C2(0xf209787b, 0xb47d6b85),
        UINT64_C2(0xb454e4a1, 0x79dd1877), UINT64_C2(0x865b8692, 0x5b9bc5c2),
        UINT64_C2(0xc83553c5, 0xc8965d3d), UINT64_C2(0x952ab45c, 0xfa97a0b3),
        UINT64_C2(0xde469fbd, 0x99a05fe3), UINT64_C2(0xa59bc234, 0xdb398c25),
        UINT64_C2(0xf6c69a72, 0xa3989f5c), UINT64_C2(0xb7dcbf53, 0x54e9bece),
        UINT64_C2(0x88fcf317, 0xf22241e2), UINT64_C2(0xcc20ce9b, 0xd35c78a5),
        UINT64_C2(0x98165af3, 0x7b2153df), UINT64_C2(0xe2a0b5dc, 0x971f303a),
        UINT64_C2(0xa8d9d153, 0x5ce3b396), UINT64_C2(0xfb9b7cd9, 0xa4a7443c),
        UINT64_C2(0xbb764c4c, 0xa7a44410), UINT64_C2(0x8bab8eef, 0xb6409c1a),
        UINT64_C2(0xd01fef10, 0xa657842c), UINT64_C2(0x9b10a4e5, 0xe9913129),
        UINT64_C2(0xe7109bfb, 0xa19c0c9d), UINT64_C2(0xac2820d9, 0x623bf429),
        UINT64_C2(0x80444b5e, 0x7aa7cf85), UINT64_C2(0xbf21e440, 0x03acdd2d),
        UINT64_C2(0x8e679c2f, 0x5e44ff8f), UINT64_C2(0xd433179d, 0x9c8cb841),
        UINT64_C2(0x9e19db92, 0xb4e31ba9), UINT64_C2(0xeb96bf6e, 0xbadf77d9),
        UINT64_C2(0xaf87023b, 0x9bf0ee6b)
    };
    static const int16_t k_cached_powers_e[] = {
        -1220, -1193, -1166, -1140, -1113, -1087, -1060, -1034, -1007,  -980,
        -954,  -927,  -901,  -874,  -847,  -821,  -794,  -768,  -741,  -715,
        -688,  -661,  -635,  -608,  -582,  -555,  -529,  -502,  -475,  -449,
        -422,  -396,  -369,  -343,  -316,  -289,  -263,  -236,  -210,  -183,
        -157,  -130,  -103,   -77,   -50,   -24,     3,    30,    56,    83,
        109,   136,   162,   189,   216,   242,   269,   295,   322,   348,
        375,   402,   428,   455,   481,   508,   534,   561,   588,   614,
        641,   667,   694,   720,   747,   774,   800,   827,   853,   880,
        907,   933,   960,   986,  1013,  1039,  1066
    };
    JSONPG_ASSERT(index < 87);
    return (DiyFp){k_cached_powers_f[index], k_cached_powers_e[index]};
}

static inline DiyFp diyfp_get_cached_power(int e, int* K) {

    //int k = static_cast<int>(ceil((-61 - e) * 0.30102999566398114)) + 374;
    double dk = (-61 - e) * 0.30102999566398114 + 347;  // dk must be positive, so can do ceiling in positive
    int k = (int)dk;
    if (dk - k > 0.0)
        k++;

    unsigned index = (unsigned)((k >> 3) + 1);
    *K = -(-348 + (int)(index << 3));    // decimal exponent no need lookup table

    return diyfp_get_cached_power_by_index(index);
}

static inline DiyFp diyfp_get_cached_power10(int exp, int *outExp) {
    JSONPG_ASSERT(exp >= -348);
    unsigned index = (unsigned)(exp + 348) / 8u;
    *outExp = -348 + (int)(index) * 8;
    return diyfp_get_cached_power_by_index(index);
}
