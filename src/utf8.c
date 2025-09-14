/*
 * jsonpg - a JSON parser/generator
 * © 2025 Bob Davison (see also: LICENSE)
 *
 * utf8.c
 *   validating UTF-8 sequences in JSON strings
 *   encoding Unicode codepoints into valid UTF-8 byte sequences
 *   identifying UTF-16 surrogare pairs and converting to codepoints
 */
#include <assert.h>
#include <stdint.h>
#include <string.h>

#define BYTE_ORDER_MARK "\xEF\xBB\xBF"

#define SURROGATE_MIN           0xD800
#define SURROGATE_MAX           0xDFFF
#define SURROGATE_OFFSET        0x10000
#define SURROGATE_HI_BITS(x)    ((x) & 0xFC00)
#define SURROGATE_LO_BITS(x)    ((x) & 0x03FF)
#define IS_1ST_SURROGATE(x)     (0xD800 == SURROGATE_HI_BITS(x))
#define IS_2ND_SURROGATE(x)     (0xDC00 == SURROGATE_HI_BITS(x))
#define IS_SURROGATE_PAIR(x, y) (IS_1ST_SURROGATE(x) && IS_2ND_SURROGATE(y))

#define CODEPOINT_MAX 0x10FFFF

// codepoint breakpoints for encoding
#define _1_BYTE_MAX 0x7F
#define _2_BYTE_MAX 0x7FF
#define _3_BYTE_MAX 0xFFFF

// utf lead byte structure
#define CONTINUATION_BYTE 0x80
#define _2_BYTE_LEADER     0xC0
#define _3_BYTE_LEADER     0xE0
#define _4_BYTE_LEADER     0xF0

// bits masks
#define HI_2_BITS(x)  ((x) & 0xC0)
#define HI_3_BITS(x)  ((x) & 0xE0)
#define HI_4_BITS(x)  ((x) & 0xF0)
#define HI_5_BITS(x)  ((x) & 0xF8)

#define LO_3_BITS(x)  ((x) & 0x07)
#define LO_4_BITS(x)  ((x) & 0x0F)
#define LO_5_BITS(x)  ((x) & 0x1F)
#define LO_6_BITS(x)  ((x) & 0x3F)

// byte identification for decoding
#define IS_2_BYTE_LEADER(x) (_2_BYTE_LEADER == HI_3_BITS(x))
#define IS_3_BYTE_LEADER(x) (_3_BYTE_LEADER == HI_4_BITS(x))
#define IS_4_BYTE_LEADER(x) (_4_BYTE_LEADER == HI_5_BITS(x))
#define IS_CONTINUATION(x)  (CONTINUATION_BYTE == HI_2_BITS(x))

static inline bool is_surrogate(unsigned cp) 
{
        return cp >= SURROGATE_MIN && cp <= SURROGATE_MAX;
}

static inline bool is_valid_codepoint(unsigned cp) 
{
        return cp <= CODEPOINT_MAX && !is_surrogate(cp);
}

/*
 * Writes a Unicode codepoint as utf-8 bytes to the provided 
 * byte array point
 * The provided byte array must have space for up to 4 characters
 *
 * The codepoint should be valid before calling this function
 *
 * Will fail silently on non-debug build if an invalid codepoint is supplied
 * In a debug build it will fail an assertion
 */      
static void utf8_encode(unsigned cp, Bytes *bytes) 
{
        int shift = 0;
        Byte lead_byte;
        if(cp <= _1_BYTE_MAX) {
                // Ascii, just one byte
                lead_byte = (Byte)cp;
        } else if(cp <= _2_BYTE_MAX) {
                // 2 byte UTF8, byte 1 is 110 and highest 5 bits
                shift = 6;
                lead_byte = (Byte)(_2_BYTE_LEADER 
                                        | LO_5_BITS(cp >> shift));
        } else if(is_surrogate(cp)) {
                // UTF-16 surrogates are not legal Unicode
                assert(0 && "Codepoint invalid: in surrogate range");
                return;
        } else if(cp <= _3_BYTE_MAX) {
                // 3 byte UTF8, byte 1 is 1110 and highest 4 bits
                shift = 12;
                lead_byte = (Byte)(_3_BYTE_LEADER 
                                        | LO_4_BITS(cp >> shift));
        } else if(cp <= CODEPOINT_MAX) {
                // 4 byte UTF8, byte 1 is 11110 and highest 3 bytes
                shift = 18;
                lead_byte = (Byte)(_4_BYTE_LEADER 
                                        | LO_3_BITS(cp >> shift));
        } else {
                // value to large to be legal Unicode
                assert(0 && "Codepoint invalid: above maximum value");
                return; // wont get here but ...
        }
        *((*bytes)++) = lead_byte;
        // Now any continuation bytes
        // high two bits '10' and next highest 6 bits from codepoint 
        while(shift > 0) {
                shift -= 6;
                *((*bytes)++) = CONTINUATION_BYTE | LO_6_BITS(cp >> shift);
        }
}

/*
 * Validates a sequence of utf-8 bytes (1-4 bytes) and 
 * returns the byte length if valid, else -1
 */
static int utf8_validate_sequence(MemoryInputStream mis) 
{
        unsigned codepoint;
        int bar;
        int cont;
        Byte byte = mis_peek(mis);

        if(IS_2_BYTE_LEADER(byte)) {
                codepoint = LO_5_BITS(byte);
                bar = _1_BYTE_MAX;
                cont = 1;
        } else if(IS_3_BYTE_LEADER(byte)) {
                codepoint = LO_4_BITS(byte);
                bar = _2_BYTE_MAX;
                cont = 2;
        } else if(IS_4_BYTE_LEADER(byte)) {
                codepoint = LO_3_BITS(byte);
                bar = _3_BYTE_MAX;
                cont = 3;
        } else if(byte <= _1_BYTE_MAX) {
                codepoint = byte;
                bar = -1;
                cont = 0;
        } else {
                return -1;
        }

        // Do we have enough input for leader and continuation bytes
        if(mis->count < 1 + cont)
                return -1;

        // Move past leader
        mis_take(mis);

        int length;
        for(length = 1 ; length <= cont ; length++) {
                byte = mis_peek(mis);
                if(!IS_CONTINUATION(byte)) 
                        return -1;
                mis_take(mis);
                codepoint = (codepoint << 6) | LO_6_BITS(byte);
        }

        // If we got here then either all valid or all invalid
        // Could be an overlong encoding or an encoding of an invalid codepoint
        if(codepoint <= bar || !is_valid_codepoint(codepoint)) 
                return -1;

        return length;
}


/*
 * Validates a sequence of 1-4 utf-8 bytes 
 * If a string buffer is provided then the bytes are appended to it
 *
 * Returns 0 if valid and succeeds in writing bytes, otherwise -1
 */
// static int write_utf8_sequence(Bytes bytes, size_t count, str_buf write_buf) 
// {
//         int len = valid_utf8_sequence(bytes, count);
//
//         if(len > 0 && write_buf) {
//                 if(str_buf_append(write_buf, bytes, len))
//                         return -1;
//         }
//
//         return len;
// }

/*
 * Counts the number of characters that match the byte order mark
 * Returns the length of the BOM if all bytes match, or 0
 */
static unsigned utf8_bom_bytes(Bytes bytes, size_t count)
{
        static unsigned bom_count = sizeof(BYTE_ORDER_MARK) / sizeof(BYTE_ORDER_MARK[0]);
        return (count >= bom_count 
                        && 0 == memcmp(BYTE_ORDER_MARK, bytes, bom_count))
                        ? bom_count
                        : 0;
}

/*
 * Returns non-zero if the supplied byte is valid
 * as the first byte of a surrogate pair  
 */
// static bool is_first_surrogate(Byte byte)
// {
//         return IS_1ST_SURROGATE(byte);
// }
//
/*
 * Returns non-zero if the supplied byte is valid
 * as the second item of a surrogate pair  
 */
// static bool is_second_surrogate(Byte byte)
// {
//         return IS_2ND_SURROGATE(byte);
// }
//
/*
 * Combines a valid utf16 surrogate pair into a valid Unicode codepoint
 */
// static int surrogate_pair_to_codepoint(int u1, int u2)
// {
//         // 110110yyyyyyyyyy 110111xxxxxxxxxx 
//         // => 0x10000 + yyyyyyyyyyxxxxxxxxxx
//         return SURROGATE_OFFSET 
//                 + (SURROGATE_LO_BITS(u1) << 10) 
//                 + SURROGATE_LO_BITS(u2);
// }
