/* SDSLib 2.0 -- A C dynamic strings library
 *
 * Copyright (c) 2006-2015, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2015, Oran Agra
 * Copyright (c) 2015, Redis Labs, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#define SDS_C
#include "sds.h"
#include "sdsalloc.h"

#define sds char *

const char *SDS_NOINIT = "SDS_NOINIT";
#if defined(SDS_DEBUG) && SDS_CLANG_VER
#  define sdsBoundsCheck(x, len)                                        \
    do {                                                                \
        const size_t _sz = __builtin_object_size((x), 0);               \
        if (unlikely(len < 0 || (_sz != (size_t)-1 && _sz < len))) {    \
            fprintf(stderr, "%s:%d: sds bounds check failed: "          \
                            "detected object size %zu but length %zd\n",\
                            __FILE__, __LINE__, _sz, (ssize_t)len);     \
            abort();                                                    \
        }                                                               \
    } while (0)
#else
#  define sdsBoundsCheck(x, len) do {} while (0)
#endif

#if !(SDS_GCC_VER || SDS_CLANG_VER)
  /* Flags to signal that this is a likely or unlikely condition. */
#  define likely(x) __builtin_expect(!!(x), 1)
#  define unlikely(x) __builtin_expect(!!(x), 0)
#else
#  define likely(x) (x)
#  define unlikely(x) (x)
#endif

/* If you define SDS_ABORT_ON_ERROR, instead of the sds functions returning
 * NULL, it will print a message and abort. This also guarantees non null
 * return values, which allows potential optimizations. */
#ifdef SDS_ABORT_ON_ERROR
#  define sdsUnreachable(...)                                           \
    do {                                                                \
        fprintf(stderr, "%s:%d: sds reached unreachable code!\n"        \
                        "Aborting because of SDS_ABORT_ON_ERROR.\n",    \
                __FILE__, __LINE__);                                    \
        abort();                                                        \
    } while (0)
#  define sdsErrReturn(...)                                             \
    do {                                                                \
        fprintf(stderr, "%s:%d: sds encountered an error!\n"            \
                        "Aborting because of SDS_ABORT_ON_ERROR.\n",    \
                __FILE__, __LINE__);                                    \
        abort();                                                        \
    } while (0)
#else
   /* Clang 3.0 (or earlier?) and GCC 4.5.0 added __builtin_unreachable */
#  if (SDS_CLANG_VER >= 300 || SDS_GCC_VER >= 405)
#    define sdsUnreachable(...)                                         \
        __builtin_unreachable();                                        \
        return __VA_ARGS__
#  else
#    define sdsUnreachable(...) return __VA_ARGS__
#  endif
#  define sdsErrReturn(...) return __VA_ARGS__
#endif

/* Creates a single case statement for sdsHdrLambda and sdsHdrLambda2.
 * It creates the following:
 *   sh: A pointer to the sds header struct.
 *   sdshdr: A typedef of the sdshdr struct.
 *   sdshdr_uint: The unsigned version of the header's int size.
 *   sdshdr_int: Same as above, but signed. */
#define sdsHdrCase(T, s, ...)                                           \
     {                                                                  \
        typedef struct sdshdr##T sdshdr;                                \
        typedef uint##T##_t sdshdr_uint;                                \
        typedef int##T##_t sdshdr_int;                                  \
        if (0) { /* Prevent unused warnings */                          \
            sdshdr_int _sds_int##T=0;                                   \
            sdshdr_uint _sds_uint##T=0;                                 \
            (void)_sds_int##T; (void)_sds_uint##T;                      \
        }                                                               \
        {   /* C90 needs a block here */                                \
            sdshdr *sh = SDS_HDR(T, s);                                 \
            (void)sh;                                                   \
            { __VA_ARGS__; }                                            \
        }                                                               \
    }                                                                   \


/* Automatically generates the code block for each sds type. */
#define sdsHdrLambda(s, ...) {                                          \
    const unsigned char _flags = (s)[-1];                               \
    sdsHdrLambda2(s, (_flags), __VA_ARGS__);                            \
}

/* Same as above, but takes a precalculated type option. */
#ifdef SDS_32_BIT
/* 32-bit version. */
#define sdsHdrLambda2(s, _type, ...) {                                  \
    enum sdshdrtype _type_ = (enum sdshdrtype)(_type & SDS_TYPE_MASK);  \
    if (_type_ == SDS_TYPE_8)                                           \
        sdsHdrCase(8, (s), __VA_ARGS__)                                 \
    else if (_type_ == SDS_TYPE_16)                                     \
        sdsHdrCase(16, (s), __VA_ARGS__)                                \
    else                                                                \
        sdsHdrCase(32, (s), __VA_ARGS__)                                \
}
#else
/* 64-bit version. The compiler decides whether to make this a switch. */
#define sdsHdrLambda2(s, _type, ...) {                                  \
    enum sdshdrtype _type_ = (enum sdshdrtype)(_type & SDS_TYPE_MASK);  \
    if (_type_ == SDS_TYPE_8)                                           \
        sdsHdrCase(8, (s), __VA_ARGS__)                                 \
    else if (_type_ == SDS_TYPE_16)                                     \
        sdsHdrCase(16, (s), __VA_ARGS__)                                \
    else if (_type_ == SDS_TYPE_32)                                     \
        sdsHdrCase(32, (s), __VA_ARGS__)                                \
    else                                                                \
        sdsHdrCase(64, (s), __VA_ARGS__)                                \
}
#endif

static const size_t sdsHdrSizes[] = {
    sizeof(struct sdshdr8) + 1,
    sizeof(struct sdshdr16) + 1,
    sizeof(struct sdshdr32) + 1,
#ifdef SDS_32_BIT
    sizeof(struct sdshdr32) + 1
#else
    sizeof(struct sdshdr64) + 1
#endif
};

static inline size_t sdsHdrSize(char type) {
    enum sdshdrtype hdrtype = (enum sdshdrtype) (type & SDS_TYPE_MASK);
    return sdsHdrSizes[hdrtype];
}

static inline char sdsReqType(size_t string_size) {
    if (string_size < 1 << 8)
        return SDS_TYPE_8;
    if (string_size < 1 << 16)
        return SDS_TYPE_16;
#ifdef SDS_32_BIT
    return SDS_TYPE_32;
#else
    if (string_size < 1ll << 32)
        return SDS_TYPE_32;
    return SDS_TYPE_64;
#endif
}


/* Length of an sds string. Use it like strlen. */
SDS_CONST_FUNC size_t sdslen(const sds s) {
    sdsHdrLambda(s, { return sh->len; });
    sdsUnreachable(0);
}

SDS_CONST_FUNC size_t sdsAvailImpl(const sds s) {
    sdsHdrLambda(s, { return sh->alloc - sh->len; });
    sdsUnreachable(0);
}
/* sdsalloc() = sdsavail() + sdslen() */
SDS_CONST_FUNC size_t sdsalloc(const sds s) {
    sdsHdrLambda(s, { return sh->alloc; });
    sdsUnreachable(0);
}

/* Breaking changes: These now may reallocate and return themselves.*/
SDS_MUT_FUNC sds sdssetlen(sds s, size_t newlen) {
    if (unlikely(sdsalloc(s) < newlen + 1))
        s = sdsMakeRoomFor(s, sdslen(s) - newlen + 1);
    sdsHdrLambda(s, {
        sh->len = newlen;
        SDS_SET_SCRATCH_SIZE(s, sh->alloc - newlen);
        return s;
    });
    sdsUnreachable(NULL);
}
SDS_MUT_FUNC sds sdsinclen(sds s, size_t inc) {
    const size_t newlen = sdslen(s) + inc;
    if (unlikely(sdsalloc(s) < inc))
        s = sdsMakeRoomFor(s, inc);
    sdsHdrLambda(s, {
        sh->len = newlen;
        return s;
    });
    sdsUnreachable(NULL);
}
SDS_MUT_FUNC sds sdssetalloc(sds s, size_t newlen) {
#ifdef SDS_DEBUG
    if (unlikely(sdsReqType(newlen) > (s[-1] & SDS_TYPE_MASK))) {
        fprintf(stderr, "sdssetalloc: Truncating value!");
    }
#endif
    sdsHdrLambda(s, {
        sh->alloc = newlen;
        SDS_SET_SCRATCH_SIZE(s, sh->alloc - sh->len);
        return s;
    });
    sdsUnreachable(NULL);
}

/* Create a new sds string with the content specified by the 'init' pointer
 * and 'initlen'.
 * If NULL is used for 'init' the string is initialized with zero bytes.
 * If SDS_NOINIT is used, the buffer is left uninitialized;
 *
 * The string is always null-termined (all the sds strings are, always) so
 * even if you create an sds string with:
 *
 * mystring = sdsnewlen("abc", 3);
 *
 * You can print the string with printf() as there is an implicit \0 at the
 * end of the string. However the string is binary safe and can contain
 * \0 characters in the middle, as the length is stored in the sds header. */
SDS_ALLOC_FUNC sds sdsnewlen(const void *const s_restrict init SDS_BOUNDED,
                             size_t initlen) {
    void *sh2;
    sds s;
    char type = sdsReqType(initlen);
    int noinit = 0;
    int hdrlen = sdsHdrSize(type);
    unsigned char *fp; /* flags pointer. */

    sdsBoundsCheck(init, initlen);

    sh2 = s_malloc(hdrlen + initlen + 1);
    if (unlikely(sh2 == NULL)) sdsErrReturn(NULL);
    if (init == SDS_NOINIT)
        noinit = 1;
    else if (!init)
        memset(sh2, 0, hdrlen + initlen + 1);
    s = (char *)sh2 + hdrlen;
    fp = (unsigned char *)&s[-1];

    /* Set the new length. */
    sdsHdrLambda2(s, type, {
        sh->len = initlen;
        sh->alloc = initlen;
        *fp = type;
    });
    SDS_SET_SCRATCH_SIZE(s, 0);

    if (initlen && init && !noinit)
        memcpy(s, init, initlen);
    s[initlen] = '\0';
    return s;
}

/* Free an sds string. No operation is performed if 's' is NULL. */
void sdsfree(sds s) {
    if (s == NULL) return;
    s_free(s - sdsHdrSize(s[-1]));
}

/* Enlarge the free space at the end of the sds string so that the caller
 * is sure that after calling this function can overwrite up to addlen
 * bytes after the end of the string, plus one more byte for nul term.
 *
 * Note: this does not change the *length* of the sds string as returned
 * by sdslen(), but only the free buffer space we have. */
SDS_MUT_FUNC sds sdsMakeRoomFor(sds s, size_t addlen) {
    char *shptr, *newsh;
    size_t len, newlen;
    char type, oldtype = s[-1] & SDS_TYPE_MASK;
    size_t hdrlen;

    if (unlikely(addlen == 0)) return s;

    if (likely(SDS_SCRATCH_SIZE(s) >= (ptrdiff_t)addlen)) return s;

    sdsHdrLambda2(s, oldtype, {
        len = sh->len;

        /* Return ASAP if there is enough space left. */
        if (likely(sh->alloc - len >= addlen)) return s;
        shptr = (char *)sh;
    });

    newlen = (len + addlen);
    if (newlen < SDS_MAX_PREALLOC)
        newlen *= 2;
    else
        newlen += SDS_MAX_PREALLOC;

    type = sdsReqType(newlen);

    hdrlen = sdsHdrSize(type);

    if (likely(oldtype == type)) {
        newsh = (char *)s_realloc(shptr, hdrlen + newlen + 1);
        if (unlikely(newsh == NULL)) sdsErrReturn(NULL);
        s = newsh+hdrlen;
    } else {
        /* Since the header size changes, need to move the string forward,
         * and can't use realloc */
        newsh = (char *)s_malloc(hdrlen+newlen+1);
        if (unlikely(newsh == NULL)) sdsErrReturn(NULL);
        memcpy(newsh + hdrlen, s, len + 1);
        s_free(shptr);
        s = newsh + hdrlen;
        s[-1] = type;
    }
    /* Set the new length manually */
    sdsHdrLambda(s, {
        sh->len = len;
        sh->alloc = newlen;
    });
    SDS_SET_SCRATCH_SIZE(s, newlen - len);
    return s;
}

/* Reallocate the sds string so that it has no free space at the end. The
 * contained string remains not altered, but next concatenation operations
 * will require a reallocation.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the
 * call. */
SDS_MUT_FUNC sds sdsRemoveFreeSpace(sds s) {
    char *sh2, *newsh;
    char type, oldtype = s[-1] & SDS_TYPE_MASK;
    int hdrlen, oldhdrlen = sdsHdrSize(oldtype);
    size_t len = sdslen(s);
    sh2 = (char*)s - oldhdrlen;

    /* Check what would be the minimum SDS header that is just good enough
     * to fit this string. */
    type = sdsReqType(len);
    hdrlen = sdsHdrSize(type);

    /* If the type is the same, or at least a large enough type is still
     * required, we just realloc(), letting the allocator to do the copy
     * only if really needed. Otherwise if the change is huge, we manually
     * reallocate the string to use the different header type. */
    if (oldtype==type || type > SDS_TYPE_8) {
        newsh = (char *)s_realloc(sh2, oldhdrlen + len + 1);
        if (unlikely(newsh == NULL)) sdsErrReturn(NULL);
        s = newsh+oldhdrlen;
    } else {
        newsh = (char *)s_malloc(hdrlen + len + 1);
        if (unlikely(newsh == NULL)) sdsErrReturn(NULL);
        memcpy(newsh+hdrlen, s, len + 1);
        s_free(sh2);
        s = newsh + hdrlen;
        s[-1] = type;
    }
    /* Set the new length manually */
    sdsHdrLambda(s, {
        sh->alloc = len;
    });
    SDS_SET_SCRATCH_SIZE(s, 0);
    return s;
}

/* Return the total size of the allocation of the specified sds string,
 * including:
 * 1) The sds header before the pointer.
 * 2) The string.
 * 3) The free buffer at the end if any.
 * 4) The implicit null term.
 */
SDS_CONST_FUNC size_t sdsAllocSize(sds s) {
    size_t alloc = sdsalloc(s);
    return sdsHdrSize(s[-1]) + alloc + 1;
}

/* Return the pointer of the actual SDS allocation (normally SDS strings
 * are referenced by the start of the string buffer). */
SDS_CONST_FUNC void *sdsAllocPtr(sds s) {
    return (void*) (s - sdsHdrSize(s[-1]));
}

/* Increment the sds length and decrements the left free space at the
 * end of the string according to 'incr'. Also set the null term
 * in the new end of the string.
 *
 * This function is used in order to fix the string length after the
 * user calls sdsMakeRoomFor(), writes something after the end of
 * the current string, and finally needs to set the new length.
 *
 * Note: it is possible to use a negative increment in order to
 * right-trim the string.
 *
 * Usage example:
 *
 * Using sdsIncrLen() and sdsMakeRoomFor() it is possible to mount the
 * following schema, to cat bytes coming from the kernel to the end of an
 * sds string without copying into an intermediate buffer:
 *
 * oldlen = sdslen(s);
 * s = sdsMakeRoomFor(s, BUFFER_SIZE);
 * nread = read(fd, s+oldlen, BUFFER_SIZE);
 * ... check for nread <= 0 and handle it ...
 * sdsIncrLen(s, nread);
 */
void sdsIncrLen(sds s, ptrdiff_t incr) {
    size_t len, alloc;
    sdsHdrLambda(s, {
        assert((incr >= 0 && sh->alloc - sh->len >= (sdshdr_uint)incr) ||
               (incr < 0 && sh->len >= (sdshdr_uint)-incr));
        len = (sh->len += incr);
        alloc = sh->alloc;
    });
    SDS_SET_SCRATCH_SIZE(s, alloc - len);
    s[len] = '\0';
}

/* Grow the sds to have the specified length. Bytes that were not part of
 * the original length of the sds will be set to zero.
 *
 * if the specified length is smaller than the current length, no operation
 * is performed. */
SDS_MUT_FUNC sds sdsgrowzero(sds s, size_t len) {
    size_t curlen = sdslen(s);

    if (len <= curlen) return s;
    s = sdsMakeRoomFor(s, len - curlen);
    if (unlikely(s == NULL)) sdsErrReturn(NULL);

    /* Make sure added region doesn't contain garbage */
    memset(s + curlen, 0, (len - curlen + 1)); /* also set trailing \0 byte */
    s = sdssetlen(s, len);
    return s;
}

/* Append the specified binary-safe string pointed by 't' of 'len' bytes
 * to the end of the specified sds string 's'.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the
 * call. */
SDS_MUT_FUNC sds sdscatlen(sds s_restrict s, const void *const s_restrict t SDS_BOUNDED,
        size_t len) {
    size_t curlen, alloclen;

    sdsBoundsCheck(t, len);
    s = sdsMakeRoomFor(s, len);
    if (unlikely(s == NULL)) sdsErrReturn(NULL);

    sdsHdrLambda(s, {
        curlen = sh->len;
        alloclen = sh->alloc;
        sh->len += len;
    });
    memcpy(s + curlen, t, len);
    SDS_SET_SCRATCH_SIZE(s, alloclen - (curlen + len));
    s[curlen+len] = '\0';
    return s;
}

/* Destructively modify the sds string 's' to hold the specified binary
 * safe string pointed by 't' of length 'len' bytes. */
SDS_MUT_FUNC sds sdscpylen(sds s_restrict s,
                           const char *const s_restrict t SDS_BOUNDED,
                           size_t len) {
    sdsBoundsCheck(t, len);

    if (sdsalloc(s) < len)
        s = sdsMakeRoomFor(s, len - sdsalloc(s));

    sdsHdrLambda(s, {
        sh->len = len;
    });
    memcpy(s, t, len);
    s[len] = '\0';
    return s;
}

#define SDS_LLSTR_SIZE 21
#define SDS_INTSTR_SIZE 12
#define SDS_HEX_LLSTR_SIZE 17
#define SDS_HEX_INTSTR_SIZE 9

/* A modified version of the algorithm from Facebook's 'Three
 * optimization tips for C++':
 *   https://www.facebook.com/notes/facebook-engineering/three-optimization-tips-for-c/10151361643253920
 *
 * I changed it to prefer 32-bit math whenever possible, and to
 * be compatible with C.
 *
 * 64-bit math, even with the exact same code, can be drastically
 * slower on 32-bit devices. For example, on a 32-bit ARMv7 LG G3,
 * using 64-bit math is ten times faster than 32-bit math with the
 * same numbers. */
/* Powers of 10 */
#define P01 10
#define P02 100
#define P03 1000
#define P04 10000
#define P05 100000
#define P06 1000000
#define P07 10000000
#define P08 100000000
#define P09 1000000000
/* 64-bit range */
#define P10 10000000000ULL
#define P11 100000000000ULL
#define P12 1000000000000ULL

#define X(n) (1<<(n * 4))
#define MAX_HEX_DIGITS(T) (sizeof(T) * CHAR_BIT / 4)
/* Handles the smaller numbers. */
static int digits16_32(uint32_t v) {
/* I don't trust big endian. */
#if defined(__LITTLE_ENDIAN__) \
    && (__has_builtin(__builtin_clz) || SDS_GCC_VER >= 304)
    /* count the leading bits. Assuming int is 16-bit for shortness.
     *   0x123 = 0x0123
     *         = 0b0000000100100011
     *  zero bits: 1234567
     *  __builtin_clz will count 7 leading zeroes.
     *  (sizeof(unsigned) * CHAR_BIT / 4) will give us how many
     *  digits this will have maximum, or for our 16-bit int, 4.
     *  Now, we shift divide the __builtin_clz output by 4, to get 1.
     *  Subtract it from the maximum digits, that is how many you have.
     */
    return (v != 0) ? MAX_HEX_DIGITS(uint32_t) - (__builtin_clz(v) / 4) : 1;
#else
    if (v < X(1)) return 1;
    if (v < X(2)) return 2;
    if (v < X(3)) return 3;
    if (v < X(6)) {
        if (v < X(4)) return 4;
        return 5 + (v >= X(5));
    }
    return 7 + (v >= X(7));
#endif
}

/* Handles the bigger numbers. */
static int digits16_64(unsigned long long v) {
#if defined(__LITTLE_ENDIAN__) \
    && (__has_builtin(__builtin_clzll) || SDS_GCC_VER >= 304)
    /* see above. __builtin_clzll is the same as __builtin_clz but for unsigned long long */
    return (v != 0) ? MAX_HEX_DIGITS(unsigned long long)
                      - (__builtin_clzll(v) / 4) : 1;
#else
    if (v <= UINT32_MAX)
        return digits16_32(v & UINT32_MAX);
    else
        return 8 + digits16_32((uint32_t)(v >> 32));
#endif
}

/* Handles the smaller numbers. */
static int digits10_32(uint32_t v) {
    if (v < P01) return 1;
    if (v < P02) return 2;
    if (v < P03) return 3;
    if (v < P08) {
        if (v < P06) {
            if (v < P04) return 4;
            return 5 + (v >= P05);
        }
        return 7 + (v >= P07);
    }
    return 9 + (v >= P09);
}

/* Handles the bigger numbers. */
static int digits10_64(unsigned long long v) {
    if (v <= UINT32_MAX)
        return digits10_32((uint32_t)v);
    if (v < P12) {
        if (v < P10)
            return 9;
        return 11 + (v >= P11);
    }
    return 12 + digits10_64(v / P12);
}

static const char digits[201] =
  "0001020304050607080910111213141516171819"
  "2021222324252627282930313233343536373839"
  "4041424344454647484950515253545556575859"
  "6061626364656667686970717273747576777879"
  "8081828384858687888990919293949596979899";

/* Modulo for when you already have the divided value.
 * For dumb compilers that optimize line-by-line. */
#define MODULO_DIV(x, y, divided) ((x) - (divided) * (y))

/* The first part of the loop. Divides until var is less than
 * limit. */
#define INT2STR_DIVIDE_UNTIL(T, limit, var) do {        \
    while ((var) >= (limit)) {                          \
        /* i = (var % 100) * 2; var /= 100 */           \
        const T tmp = (var) / 100;                      \
        const T i = MODULO_DIV((var), 100, tmp) * 2;    \
        (var) = tmp;                                    \
                                                        \
        s[next] = digits[i + 1];                        \
        s[next - 1] = digits[i];                        \
        next -= 2;                                      \
    }                                                   \
} while (0)

/* Handles the last 1-2 digits */
#define INT2STR_END(value) do {                         \
    if ((value) < 10) {                                 \
        s[next] = '0' + (unsigned char)(value);         \
    } else {                                            \
        int i = (value) * 2;                            \
        s[next] = digits[i + 1];                        \
        s[next - 1] = digits[i];                        \
    }                                                   \
    s[length] = '\0';                                   \
} while (0)

int sdsuint2str(char *s, unsigned value)
{
    const int length = digits10_32(value);
    int next = length - 1;
    int v;

    /* Do signed division. Oddly, it is faster on some
     * CPUs. */
    INT2STR_DIVIDE_UNTIL(unsigned, INT_MAX, value);
    v = (int)value;

    INT2STR_DIVIDE_UNTIL(int, 100, v);
    INT2STR_END(v);
    return length;
}

int sdsulonglong2str(char *s, unsigned long long value)
{
    const int length = digits10_64(value);
    int next = length - 1;
    int v;
    /* Do 64-bit division until we get something in the signed 32-bit range. */
    INT2STR_DIVIDE_UNTIL(unsigned long long, INT_MAX, value);
    /* Do the rest in 32-bit */
    v = (int)value;
    INT2STR_DIVIDE_UNTIL(int, 100, v);
    INT2STR_END(v);
    return length;
}

int sdsint2str(char *s, int value) {
    /* Negative */
    if (value < 0) {
        unsigned v = -value;
        *s++ = '-';
        return sdsuint2str(s, v) + 1;
    }
    return sdsuint2str(s, (unsigned)value);
}

int sdslonglong2str(char *s, long long value) {
    /* Negative */
    if (value < 0) {
        unsigned long long v = -value;
        *s++ = '-';
        return sdsulonglong2str(s, v) + 1;
    }
    return sdsulonglong2str(s, (unsigned long long)value);
}
static const char digits_hex_small[] = "0123456789ABCDEF";
static const char digits_hex[513] =
  "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"
  "202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F"
  "404142434445464748494A4B4C4D4E4F505152535455565758595A5B5C5D5E5F"
  "606162636465666768696A6B6C6D6E6F707172737475767778797A7B7C7D7E7F"
  "808182838485868788898A8B8C8D8E8F909192939495969798999A9B9C9D9E9F"
  "A0A1A2A3A4A5A6A7A8A9AAABACADAEAFB0B1B2B3B4B5B6B7B8B9BABBBCBDBEBF"
  "C0C1C2C3C4C5C6C7C8C9CACBCCCDCECFD0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF"
  "E0E1E2E3E4E5E6E7E8E9EAEBECEDEEEFF0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF";

int sdshexuint2str(char *s, unsigned value) {
    const int length = digits16_32(value);
    int next = length - 1;
    while (value >= 0x100) {
        /* i = (value % 256) * 2; */
        const unsigned i = (value & 0xFF) * 2;
        /* value /= 256; */
        value /= 0x100;

        s[next] = digits_hex[i + 1];
        s[next - 1] = digits_hex[i];
        next -= 2;
    }
    /* Handles the last 1-2 digits */
    if (value < 0x10) {
        s[next] = digits_hex_small[value];
    } else {
        unsigned i = value * 2;
        s[next] = digits_hex[i + 1];
        s[next - 1] = digits_hex[i];
    }
    s[length] = '\0';
    return length;
}

int sdshexulonglong2str(char *s, unsigned long long value) {
    const int length = digits16_64(value);
    int next = length - 1;
    unsigned v;

    while (value >= UINT_MAX) {
        /* const unsigned i = (value % 256) * 2 */
        const unsigned i = (value & 0xFF) * 2;
        value /= 0x100;

        s[next] = digits_hex[i + 1];
        s[next - 1] = digits_hex[i];
        next -= 2;
    }

    /* Switch to 32-bit */
    v = (unsigned)(value & UINT_MAX);
    while (v >= 0x100) {
        /* const unsigned i = (value % 256) * 2 */
        const unsigned i = (v & 0xFF) * 2;
        v /= 0x100;

        s[next] = digits_hex[i + 1];
        s[next - 1] = digits_hex[i];
        next -= 2;
    }

    /* Handles the last 1-2 digits */
    if (v < 0x10) {
        s[next] = digits_hex_small[v];
    } else {
        unsigned i = v * 2;
        s[next] = digits_hex[i + 1];
        s[next - 1] = digits_hex[i];
    }
    s[length] = '\0';
    return length;
}

/* Creates functions named sdsadd<T> and sdsfrom<T> that appends a <type>
 * int value to an existing sds string, checking if <minsize> is available,
 * then calling sds<T>2str to make the conversion. */
#define SDS_INT_ADD_FUNC(T, type, minsize)                              \
SDS_MUT_FUNC sds (sdsadd##T)(sds s, type value) {                       \
    int len;                                                            \
    s = sdsMakeRoomFor(s, minsize);                                     \
    sdsHdrLambda(s, {                                                   \
        len = sds##T##2str(&s[sh->len], value);                         \
                                                                        \
        sh->len += len;                                                 \
        return s;                                                       \
    });                                                                 \
    sdsUnreachable(NULL);                                               \
}                                                                       \
SDS_ALLOC_FUNC sds (sdsfrom##T)(type value) {                           \
    return (sdsadd##T)(sdsempty(), value);                              \
}

#undef sds
/* Add the functions */
SDS_INT_ADD_FUNC(int, int, SDS_INTSTR_SIZE)
SDS_INT_ADD_FUNC(uint, unsigned, SDS_INTSTR_SIZE)
SDS_INT_ADD_FUNC(longlong, long long, SDS_LLSTR_SIZE)
SDS_INT_ADD_FUNC(ulonglong, unsigned long long, SDS_LLSTR_SIZE)
SDS_INT_ADD_FUNC(hexuint, unsigned, SDS_LLSTR_SIZE)
SDS_INT_ADD_FUNC(hexulonglong, unsigned long long, SDS_LLSTR_SIZE)

#undef SDS_INT_ADD_FUNC
#undef MODULO_DIV
#undef INT2STR_DIVIDE_UNTIL
#undef INT2STR_END

#define sds char *

SDS_MUT_FUNC sds sdsaddchar(sds s, unsigned int c) {
    size_t len = sdslen(s);
    s = sdsMakeRoomFor(s, 1);
    sdsHdrLambda(s, {
        len = sh->len;
        sh->len++;
    });
    SDS_SET_SCRATCH_SIZE(s, SDS_SCRATCH_SIZE(s) - 1);
    s[len] = (char)c;
    s[len + 1] = '\0';
    return s;
}

/* Like sdscatprintf() but gets va_list instead of being variadic. */
SDS_PRINTF_FUNC(2, 0) sds sdscatvprintf(sds s_restrict s, const char *const s_restrict fmt,
                                       va_list ap) {
    va_list cpy;
    char staticbuf[1024], *buf = staticbuf, *t;
    int buflen = strlen(fmt) * 2;
    int len;
    int malloced = 0;
    int i;

    /* We try to start using a static buffer for speed.
     * If not possible we revert to stack or allocation. */
    if (buflen > (int)sizeof(staticbuf)) {
        malloced = 1;
        buf = (char *)s_malloc(buflen);
        if (unlikely(buf == NULL)) sdsErrReturn(NULL);
    } else {
        buflen = sizeof(staticbuf);
    }

    /* First, try vsnprintf directly, then reallocate once if
     * needed. */
    for (i = 0; i < 2; i++) {
        va_copy(cpy, ap);
        len = vsnprintf(buf, buflen, fmt, cpy);
        va_end(cpy);

        /* vsnprintf will either return the number of bytes needed to write
         * the string or a negative on an error. If the return value is
         * greater or equal to buflen, that means we need to realloc and try
         * again. */
        if (unlikely(len < 0)) {
            if (unlikely(malloced))
                s_free(buf);
            sdsErrReturn(NULL);
        } else if (len >= buflen) {
            buflen = len + 1;

            if (unlikely(malloced)) free(buf);
            buf = (char *)s_malloc(buflen);
            malloced = 1;

            if (unlikely(buf == NULL)) sdsErrReturn(NULL);

            continue;
        }
        break;
    }

    /* Finally concat the obtained string to the SDS string and return it. */
    t = sdscatlen(s, buf, len);
    if (malloced) s_free(buf);
    return t;
}

/* Append to the sds string 's' a string obtained using printf-alike format
 * specifier.
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = sdsnew("Sum is: ");
 * s = sdscatprintf(s, "%d+%d = %d", a, b, a + b).
 *
 * Often you need to create a string from scratch with the printf-alike
 * format. When this is the need, just use sdsempty() as the target string:
 *
 * s = sdscatprintf(sdsempty(), "... your format ...", args);
 */
SDS_PRINTF_FUNC(2, 3) sds sdscatprintf(sds s_restrict s,
                                       const char *const s_restrict fmt,  ...) {
    va_list ap;
    char *t;

    va_start(ap, fmt);
    t = sdscatvprintf(s, fmt, ap);
    va_end(ap);
    return t;
}

/* Just a quick implementation of strchrnul for those who don't have it. */
static inline const char *sdsstrchrnul(const char *str, int c) {
    const char *ret = strchr(str, c);
    return (ret == NULL) ? str + strlen(str) : ret;
}

/* This function is similar to sdscatprintf, but much faster as it does
 * not rely on sprintf() family functions implemented by the libc that
 * are often very slow. Moreover directly handling the sds string as
 * new data is concatenated provides a performance improvement.
 *
 * However this function only handles an incompatible subset of printf-alike
 * format specifiers:
 *
 * %s - C String
 * %S - SDS string
 * %i, %d - signed int
 * %I, %D - 64 bit signed integer (long long, int64_t)
 * %u - unsigned int
 * %U - 64 bit unsigned integer (unsigned long long, uint64_t)
 * %x - unsigned int, uppercase hex
 * %X - 64-bit unsigned integer, uppercase hex
 * %% - Verbatim "%" character.
 */
SDS_MUT_FUNC sds sdscatfmt(sds s_restrict s, const char *s_restrict fmt, ...) {
    size_t initlen = sdslen(s);
    const char *f, *last;
    long i;
    va_list ap;

    va_start(ap, fmt);
    last = f = fmt;    /* Next format specifier byte to process. */
    i = initlen; /* Position of the next byte to write to dest str. */
    do {
        char next, *str;
        size_t l;

        last = f;
        /* Skip to the next percent sign */
        f = sdsstrchrnul(f, '%');

        /* Make sure there is always space for at least 1 char. */
        if ((ptrdiff_t)sdsavail(s) < (f - last)) {
            s = sdsMakeRoomFor(s, f - last);
        }
        if (last < f) {
            memcpy(s + i, last, f - last);
            sdsIncrLen(s, (f - last));
            i += (f - last);
        }
        switch (*f) {
        case '\0': break;
        case '%':
            next = *(f + 1);
            f += 2;
            switch (next) {
            case 's':
            case 'S':
                str = va_arg(ap, char *);
                l = (next == 's') ? strlen(str) : sdslen(str);
                if (sdsavail(s) < l) {
                    s = sdsMakeRoomFor(s, l);
                }
                memcpy(s + i, str, l);
                sdsIncrLen(s, l);
                i += l;
                break;
            case 'i':
            case 'd': {
                    int num = va_arg(ap, int);
                    s = sdsMakeRoomFor(s, SDS_INTSTR_SIZE);
                    l = sdsint2str(s + i, num);
                    sdsIncrLen(s, l);
                    i += l;
                }
                break;
            case 'I':
            case 'D': {
                    long long num = va_arg(ap, long long);
                    s = sdsMakeRoomFor(s, SDS_LLSTR_SIZE);
                    l = sdslonglong2str(s + i, num);
                    sdsIncrLen(s, l);
                    i += l;
                }
                break;
            case 'u': {
                    unsigned unum = va_arg(ap, unsigned);
                    s = sdsMakeRoomFor(s, SDS_INTSTR_SIZE);
                    l = sdsuint2str(s + i, unum);
                    sdsIncrLen(s, l);
                    i += l;
                }
                break;
            case 'U': {
                    unsigned long long unum = va_arg(ap, unsigned long long);
                    s = sdsMakeRoomFor(s, SDS_LLSTR_SIZE);
                    l = sdsulonglong2str(s + i, unum);
                    sdsIncrLen(s, l);
                    i += l;
                }
                break;
            case 'X': {
                    unsigned long long unum = va_arg(ap, unsigned long long);
                    s = sdsMakeRoomFor(s, SDS_HEX_LLSTR_SIZE);
                    l = sdshexulonglong2str(s + i, unum);
                    sdsIncrLen(s, l);
                    i += l;
                }
                break;
            case 'x':
                {
                    unsigned unum = va_arg(ap, unsigned);
                    s = sdsMakeRoomFor(s, SDS_HEX_INTSTR_SIZE);
                    l = sdshexuint2str(s + i, unum);
                    sdsIncrLen(s, l);
                    i += l;
                }
                break;
            default: /* Handle %% and generally %<unknown>. */
                s[i++] = next;
                sdsIncrLen(s, 1);
                break;
            }
           break;
        }
    } while(*f);
    va_end(ap);

    /* Add null-term */
    s[i] = '\0';
    return s;
}

/* Remove the part of the string from left and from right composed just of
 * contiguous characters found in 'cset', that is a null terminted C string.
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = sdsnew("AA...AA.a.aa.aHelloWorld     :::");
 * s = sdstrim(s, "Aa. :");
 * printf("%s\n", s);
 *
 * Output will be just "Hello World".
 */
SDS_MUT_FUNC sds sdstrim(sds s_restrict s, const char *s_restrict cset) {
    char *start, *end, *sp, *ep;
    size_t len;

    sp = start = s;
    ep = end = s + sdslen(s) - 1;
    while (sp <= end && strchr(cset, *sp)) sp++;
    while (ep > sp && strchr(cset, *ep)) ep--;
    len = (sp > ep) ? 0 : ((ep - sp) + 1);
    if (s != sp) memmove(s, sp, len);
    s[len] = '\0';
    s = sdssetlen(s, len);
    return s;
}

/* Turn the string into a smaller (or equal) string containing only the
 * substring specified by the 'start' and 'end' indexes.
 *
 * start and end can be negative, where -1 means the last character of the
 * string, -2 the penultimate character, and so forth.
 *
 * The interval is inclusive, so the start and end characters will be part
 * of the resulting string.
 *
 * The string is modified in-place.
 *
 * Example:
 *
 * s = sdsnew("Hello World");
 * sdsrange(s, 1, -1); => "ello World"
 */
SDS_MUT_FUNC sds sdsrange(sds s, ptrdiff_t start, ptrdiff_t end) {
    size_t newlen, len = sdslen(s);

    if (len == 0) return s;
    if (start < 0) {
        start = len + start;
        if (start < 0) start = 0;
    }
    if (end < 0) {
        end = len + end;
        if (end < 0) end = 0;
    }
    newlen = (start > end) ? 0 : (end - start) + 1;
    if (newlen != 0) {
        if (start >= (ptrdiff_t)len) {
            newlen = 0;
        } else if (end >= (ptrdiff_t)len) {
            end = len - 1;
            newlen = (start > end) ? 0 : (end - start) + 1;
        }
    } else {
        start = 0;
    }
    if (start && newlen != 0) memmove(s, s + start, newlen);
    s[newlen] = '\0';
    s = sdssetlen(s, newlen);
    return s;
}

/* Apply tolower() to every character of the sds string 's'. */
SDS_MUT_FUNC sds sdstolower(sds s) {
    size_t len = sdslen(s), j;

    for (j = 0; j < len; j++)
        s[j] = tolower(s[j]);
    return s;
}

/* Apply toupper() to every character of the sds string 's'. */
SDS_MUT_FUNC sds sdstoupper(sds s) {
    size_t len = sdslen(s), j;

    for (j = 0; j < len; j++)
        s[j] = toupper(s[j]);
    return s;
}

/* Compare two sds strings s1 and s2 with memcmp().
 *
 * Return value:
 *
 *     positive if s1 > s2.
 *     negative if s1 < s2.
 *     0 if s1 and s2 are exactly the same binary string.
 *
 * If two strings share exactly the same prefix, but one of the two has
 * additional characters, the longer string is considered to be greater than
 * the smaller one. */
SDS_CONST_FUNC int sdscmp(const sds s1, const sds s2) {
    size_t l1, l2, minlen;
    int cmp;

    /* Check if the pointers are equal. */
    if (s1 == s2) return 0;

    l1 = sdslen(s1);
    l2 = sdslen(s2);
    minlen = (l1 < l2) ? l1 : l2;
    cmp = memcmp(s1, s2, minlen);
    if (cmp == 0) return l1 > l2 ? 1 : (l1 < l2 ? -1 : 0);
    return cmp;
}

/* Split 's' with separator in 'sep'. An array
 * of sds strings is returned. *count will be set
 * by reference to the number of tokens returned.
 *
 * On out of memory, zero length string, zero length
 * separator, NULL is returned.
 *
 * Note that 'sep' is able to split a string using
 * a multi-character separator. For example
 * sdssplit("foo_-_bar", "_-_"); will return two
 * elements "foo" and "bar".
 *
 * This version of the function is binary-safe but
 * requires length arguments. sdssplit() is just the
 * same function but for zero-terminated strings.
 */
SDS_ALLOC_FUNC sds *sdssplitlen(const char *const s_restrict s SDS_BOUNDED, ptrdiff_t len,
        const char *const s_restrict sep SDS_BOUNDED, int seplen, int *const s_restrict count) {
    int elements = 0, slots = 5;
    long start = 0, j;
    sds *tokens;

    sdsBoundsCheck(s, len);
    sdsBoundsCheck(sep, seplen);

    if (unlikely(seplen < 1 || len < 0)) sdsErrReturn(NULL);

    tokens = (sds *)s_malloc(sizeof(sds) * slots);
    if (unlikely(tokens == NULL)) sdsErrReturn(NULL);

    if (len == 0) {
        *count = 0;
        return tokens;
    }
    for (j = 0; j < (len - (seplen - 1)); j++) {
        /* make sure there is room for the next element and the final one */
        if (slots < elements + 2) {
            sds *newtokens;

            slots *= 2;
            newtokens = (sds *)s_realloc(tokens, sizeof(sds) * slots);
            if (unlikely(newtokens == NULL)) goto err;
            tokens = newtokens;
        }
        /* search the separator */
        if ((seplen == 1 && s[j] == sep[0]) || (memcmp(s + j, sep, seplen) == 0)) {
            tokens[elements] = sdsnewlen(s + start, j - start);
            if (unlikely(tokens[elements] == NULL)) goto err;
            elements++;
            start = j + seplen;
            j = j + seplen - 1; /* skip the separator */
        }
    }
    /* Add the final element. We are sure there is room in the tokens array. */
    tokens[elements] = sdsnewlen(s + start, len - start);
    if (unlikely(tokens[elements] == NULL)) goto err;
    elements++;
    *count = elements;
    return tokens;

err:
    {
        int i;
        for (i = 0; i < elements; i++) sdsfree(tokens[i]);
        s_free(tokens);
        *count = 0;
        sdsErrReturn(NULL);
    }
}

/* Free the result returned by sdssplitlen(), or do nothing if 'tokens' is NULL. */
void sdsfreesplitres(sds *tokens, int count) {
    if (!tokens) return;
    while(count--)
        sdsfree(tokens[count]);
    s_free(tokens);
}

/* Append to the sds string "s" an escaped string representation where
 * all the non-printable characters (tested with isprint()) are turned into
 * escapes in the form "\n\r\a...." or "\x<hex-number>".
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
SDS_MUT_FUNC sds sdscatrepr(sds s_restrict s, const char *s_restrict p,
        size_t len) {
    s = sdsaddchar(s, '"');
    while(len--) {
        switch(*p) {
        case '\\':
        case '"':
            s = sdsaddchar(s, '\\');
            s = sdsaddchar(s, *p);
            break;
        case '\n': s = sdscatlen(s, "\\n", 2); break;
        case '\r': s = sdscatlen(s, "\\r", 2); break;
        case '\t': s = sdscatlen(s, "\\t", 2); break;
        case '\a': s = sdscatlen(s, "\\a", 2); break;
        case '\b': s = sdscatlen(s, "\\b", 2); break;
        default:
            if (isprint(*p))
                s = sdsaddchar(s, *p);
            else
                s = sdscatprintf(s, "\\x%02x", (unsigned char)*p);
            break;
        }
        p++;
    }
    s = sdsaddchar(s, '"');
    return s;
}

/* Helper function for sdssplitargs() that returns non zero if 'c'
 * is a valid hex digit. */
static int is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

/* Helper function for sdssplitargs() that converts a hex digit into an
 * integer from 0 to 15 */
static int hex_digit_to_int(char c) {
    switch(c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': case 'A': return 10;
    case 'b': case 'B': return 11;
    case 'c': case 'C': return 12;
    case 'd': case 'D': return 13;
    case 'e': case 'E': return 14;
    case 'f': case 'F': return 15;
    default: return 0;
    }
}

/* Split a line into arguments, where every argument can be in the
 * following programming-language REPL-alike form:
 *
 * foo bar "newline are supported\n" and "\xff\x00otherstuff"
 *
 * The number of arguments is stored into *argc, and an array
 * of sds is returned.
 *
 * The caller should free the resulting array of sds strings with
 * sdsfreesplitres().
 *
 * Note that sdscatrepr() is able to convert back a string into
 * a quoted string in the same format sdssplitargs() is able to parse.
 *
 * The function returns the allocated tokens on success, even when the
 * input string is empty, or NULL if the input contains unbalanced
 * quotes or closed quotes followed by non space characters
 * as in: "foo"bar or "foo'
 */
SDS_ALLOC_FUNC sds *sdssplitargs(const char *s_restrict line,
        int *s_restrict argc) {
    const char *p = line;
    char *current = NULL;
    char **vector = NULL;

    *argc = 0;
    while(1) {
        /* skip blanks */
        while (p[0] != '\0' && isspace(*p)) p++;
        if (p[0] != '\0') {
            /* get a token */
            int inq = 0;  /* set to 1 if we are in "quotes" */
            int insq = 0; /* set to 1 if we are in 'single quotes' */
            int done = 0;

            if (unlikely(current == NULL)) current = sdsempty();
            while (!done) {
                if (inq) {
                    if (p[0] == '\\' && p[1] == 'x' &&
                                             is_hex_digit(p[2]) &&
                                             is_hex_digit(p[3]))
                    {
                        unsigned char byte;

                        byte = (hex_digit_to_int(p[2]) * 16)
                              + hex_digit_to_int(p[3]);
                        current = sdsaddchar(current, byte);
                        p += 3;
                    } else if (p[0] == '\\' && p[1]) {
                        char c;

                        p++;
                        switch (p[0]) {
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                        case 't': c = '\t'; break;
                        case 'b': c = '\b'; break;
                        case 'a': c = '\a'; break;
                        default: c = p[0]; break;
                        }
                        current = sdsaddchar(current, c);
                    } else if (p[0] == '"') {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (unlikely(p[1] && !isspace(p[1]))) goto err;
                        done = 1;
                    } else if (unlikely(p[0] == '\0')) {
                        /* unterminated quotes */
                        goto err;
                    } else {
                        current = sdsaddchar(current, p[0]);
                    }
                } else if (insq) {
                    if (p[0] == '\\' && p[1] == '\'') {
                        p++;
                        current = sdsaddchar(current, '\'');
                    } else if (p[0] == '\'') {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (unlikely(p[1] && !isspace(p[1]))) goto err;
                        done = 1;
                    } else if (unlikely(p[0] == '\0')) {
                        /* unterminated quotes */
                        goto err;
                    } else {
                        current = sdsaddchar(current, p[0]);
                    }
                } else {
                    switch(*p) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                    case '\0':
                        done = 1;
                        break;
                    case '"':
                        inq = 1;
                        break;
                    case '\'':
                        insq = 1;
                        break;
                    default:
                        current = sdsaddchar(current, p[0]);
                        break;
                    }
                }
                if (p[0] != '\0') p++;
            }
            /* add the token to the vector */
            vector = (char **)s_realloc(vector, ((*argc) + 1) * sizeof(char *));
            if (vector == NULL) goto err;

            vector[*argc] = current;
            (*argc)++;
            current = NULL;
        } else {
            /* Even on empty input string return something not NULL. */
            if (vector == NULL) vector = (char **)s_malloc(sizeof(char *));
            return vector;
        }
    }

err:
    while((*argc)--)
        sdsfree(vector[*argc]);
    s_free(vector);
    if (current) sdsfree(current);
    *argc = 0;
    sdsErrReturn(NULL);
}

/* Modify the string substituting all the occurrences of the set of
 * characters specified in the 'from' string to the corresponding character
 * in the 'to' array.
 *
 * For instance: sdsmapchars(mystring, "ho", "01", 2)
 * will have the effect of turning the string "hello" into "0ell1".
 *
 * The function returns the sds string pointer, that is always the same
 * as the input pointer since no resize is needed. */
SDS_MUT_FUNC sds sdsmapchars(sds s_restrict s, const char *s_restrict from,
        const char *s_restrict to, size_t setlen) {
    size_t j, i, l = sdslen(s);

    for (j = 0; j < l; j++) {
        for (i = 0; i < setlen; i++) {
            if (s[j] == from[i]) {
                s[j] = to[i];
                break;
            }
        }
    }
    return s;
}

/* Join an array of C strings using the specified separator (also a C string).
 * Returns the result as an sds string. */
SDS_ALLOC_FUNC sds sdsjoin(const char **s_restrict argv, int argc,
        const char *s_restrict sep) {
    sds join = sdsempty();
    int j;

    for (j = 0; j < argc; j++) {
        join = sdscat(join, argv[j]);
        if (j != argc - 1) join = sdscat(join, sep);
    }
    return join;
}

/* Like sdsjoin, but joins an array of SDS strings. */
SDS_ALLOC_FUNC sds sdsjoinsds(sds *s_restrict argv, int argc,
        const char *s_restrict sep, size_t seplen) {
    sds join = sdsempty();
    int j;

    for (j = 0; j < argc; j++) {
        join = sdscatsds(join, argv[j]);
        if (j != argc - 1) join = sdscatlen(join, sep, seplen);
    }
    return join;
}

/* Wrappers to the allocators used by SDS. Note that SDS will actually
 * just use the macros defined into sdsalloc.h in order to avoid to pay
 * the overhead of function calls. Here we define these wrappers only for
 * the programs SDS is linked to, if they want to touch the SDS internals
 * even if they use a different allocator. */
void *sds_malloc(size_t size) { return s_malloc(size); }
void *sds_realloc(void *ptr, size_t size) { return s_realloc(ptr, size); }
void sds_free(void *ptr) { s_free(ptr); }
