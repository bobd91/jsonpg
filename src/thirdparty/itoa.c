// See LICENCE
//
// Converted to C from C++
//
// https://github.com/Tencent/rapidjson/blob/master/include/rapidjson/internal/itoa.h
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

static inline const char* get_digits_lut() {
    static const char c_digits_lut[200] = {
        '0','0','0','1','0','2','0','3','0','4','0','5','0','6','0','7','0','8','0','9',
        '1','0','1','1','1','2','1','3','1','4','1','5','1','6','1','7','1','8','1','9',
        '2','0','2','1','2','2','2','3','2','4','2','5','2','6','2','7','2','8','2','9',
        '3','0','3','1','3','2','3','3','3','4','3','5','3','6','3','7','3','8','3','9',
        '4','0','4','1','4','2','4','3','4','4','4','5','4','6','4','7','4','8','4','9',
        '5','0','5','1','5','2','5','3','5','4','5','5','5','6','5','7','5','8','5','9',
        '6','0','6','1','6','2','6','3','6','4','6','5','6','6','6','7','6','8','6','9',
        '7','0','7','1','7','2','7','3','7','4','7','5','7','6','7','7','7','8','7','9',
        '8','0','8','1','8','2','8','3','8','4','8','5','8','6','8','7','8','8','8','9',
        '9','0','9','1','9','2','9','3','9','4','9','5','9','6','9','7','9','8','9','9'
    };
    return c_digits_lut;
}

static inline char* u64toa(uint64_t value, char* buffer) {
    const char* c_digits_lut = get_digits_lut();
    const uint64_t  k_ten8 = 100000000;
    const uint64_t  k_ten9 = k_ten8 * 10;
    const uint64_t k_ten10 = k_ten8 * 100;
    const uint64_t k_ten11 = k_ten8 * 1000;
    const uint64_t k_ten12 = k_ten8 * 10000;
    const uint64_t k_ten13 = k_ten8 * 100000;
    const uint64_t k_ten14 = k_ten8 * 1000000;
    const uint64_t k_ten15 = k_ten8 * 10000000;
    const uint64_t k_ten16 = k_ten8 * k_ten8;

    if (value < k_ten8) {
        uint32_t v = (uint32_t)(value);
        if (v < 10000) {
            const uint32_t d1 = (v / 100) << 1;
            const uint32_t d2 = (v % 100) << 1;

            if (v >= 1000)
                *buffer++ = c_digits_lut[d1];
            if (v >= 100)
                *buffer++ = c_digits_lut[d1 + 1];
            if (v >= 10)
                *buffer++ = c_digits_lut[d2];
            *buffer++ = c_digits_lut[d2 + 1];
        }
        else {
            // value = bbbbcccc
            const uint32_t b = v / 10000;
            const uint32_t c = v % 10000;

            const uint32_t d1 = (b / 100) << 1;
            const uint32_t d2 = (b % 100) << 1;

            const uint32_t d3 = (c / 100) << 1;
            const uint32_t d4 = (c % 100) << 1;

            if (value >= 10000000)
                *buffer++ = c_digits_lut[d1];
            if (value >= 1000000)
                *buffer++ = c_digits_lut[d1 + 1];
            if (value >= 100000)
                *buffer++ = c_digits_lut[d2];
            *buffer++ = c_digits_lut[d2 + 1];

            *buffer++ = c_digits_lut[d3];
            *buffer++ = c_digits_lut[d3 + 1];
            *buffer++ = c_digits_lut[d4];
            *buffer++ = c_digits_lut[d4 + 1];
        }
    }
    else if (value < k_ten16) {
        const uint32_t v0 = (uint32_t)(value / k_ten8);
        const uint32_t v1 = (uint32_t)(value % k_ten8);

        const uint32_t b0 = v0 / 10000;
        const uint32_t c0 = v0 % 10000;

        const uint32_t d1 = (b0 / 100) << 1;
        const uint32_t d2 = (b0 % 100) << 1;

        const uint32_t d3 = (c0 / 100) << 1;
        const uint32_t d4 = (c0 % 100) << 1;

        const uint32_t b1 = v1 / 10000;
        const uint32_t c1 = v1 % 10000;

        const uint32_t d5 = (b1 / 100) << 1;
        const uint32_t d6 = (b1 % 100) << 1;

        const uint32_t d7 = (c1 / 100) << 1;
        const uint32_t d8 = (c1 % 100) << 1;

        if (value >= k_ten15)
            *buffer++ = c_digits_lut[d1];
        if (value >= k_ten14)
            *buffer++ = c_digits_lut[d1 + 1];
        if (value >= k_ten13)
            *buffer++ = c_digits_lut[d2];
        if (value >= k_ten12)
            *buffer++ = c_digits_lut[d2 + 1];
        if (value >= k_ten11)
            *buffer++ = c_digits_lut[d3];
        if (value >= k_ten10)
            *buffer++ = c_digits_lut[d3 + 1];
        if (value >= k_ten9)
            *buffer++ = c_digits_lut[d4];

        *buffer++ = c_digits_lut[d4 + 1];
        *buffer++ = c_digits_lut[d5];
        *buffer++ = c_digits_lut[d5 + 1];
        *buffer++ = c_digits_lut[d6];
        *buffer++ = c_digits_lut[d6 + 1];
        *buffer++ = c_digits_lut[d7];
        *buffer++ = c_digits_lut[d7 + 1];
        *buffer++ = c_digits_lut[d8];
        *buffer++ = c_digits_lut[d8 + 1];
    }
    else {
        const uint32_t a = (uint32_t)(value / k_ten16); // 1 to 1844
        value %= k_ten16;

        if (a < 10)
            *buffer++ = (char)('0' + (char)(a));
        else if (a < 100) {
            const uint32_t i = a << 1;
            *buffer++ = c_digits_lut[i];
            *buffer++ = c_digits_lut[i + 1];
        }
        else if (a < 1000) {
            *buffer++ = (char)('0' + (char)(a / 100));

            const uint32_t i = (a % 100) << 1;
            *buffer++ = c_digits_lut[i];
            *buffer++ = c_digits_lut[i + 1];
        }
        else {
            const uint32_t i = (a / 100) << 1;
            const uint32_t j = (a % 100) << 1;
            *buffer++ = c_digits_lut[i];
            *buffer++ = c_digits_lut[i + 1];
            *buffer++ = c_digits_lut[j];
            *buffer++ = c_digits_lut[j + 1];
        }

        const uint32_t v0 = (uint32_t)(value / k_ten8);
        const uint32_t v1 = (uint32_t)(value % k_ten8);

        const uint32_t b0 = v0 / 10000;
        const uint32_t c0 = v0 % 10000;

        const uint32_t d1 = (b0 / 100) << 1;
        const uint32_t d2 = (b0 % 100) << 1;

        const uint32_t d3 = (c0 / 100) << 1;
        const uint32_t d4 = (c0 % 100) << 1;

        const uint32_t b1 = v1 / 10000;
        const uint32_t c1 = v1 % 10000;

        const uint32_t d5 = (b1 / 100) << 1;
        const uint32_t d6 = (b1 % 100) << 1;

        const uint32_t d7 = (c1 / 100) << 1;
        const uint32_t d8 = (c1 % 100) << 1;

        *buffer++ = c_digits_lut[d1];
        *buffer++ = c_digits_lut[d1 + 1];
        *buffer++ = c_digits_lut[d2];
        *buffer++ = c_digits_lut[d2 + 1];
        *buffer++ = c_digits_lut[d3];
        *buffer++ = c_digits_lut[d3 + 1];
        *buffer++ = c_digits_lut[d4];
        *buffer++ = c_digits_lut[d4 + 1];
        *buffer++ = c_digits_lut[d5];
        *buffer++ = c_digits_lut[d5 + 1];
        *buffer++ = c_digits_lut[d6];
        *buffer++ = c_digits_lut[d6 + 1];
        *buffer++ = c_digits_lut[d7];
        *buffer++ = c_digits_lut[d7 + 1];
        *buffer++ = c_digits_lut[d8];
        *buffer++ = c_digits_lut[d8 + 1];
    }

    return buffer;
}

static inline char* i64toa(int64_t value, char* buffer) {
    ASSERT(buffer);
    uint64_t u = (uint64_t)(value);
    if (value < 0) {
        *buffer++ = '-';
        u = ~u + 1;
    }

    return u64toa(u, buffer);
}

static const unsigned i64toa_min_buffer_length = 22;
