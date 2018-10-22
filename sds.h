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

#ifndef __SDS_H
#define __SDS_H

#define SDS_MAX_PREALLOC (1024*1024)
extern const char *SDS_NOINIT;

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

typedef char *sds;

struct __attribute__ ((__packed__)) sdshdr8 {
    uint8_t len; /* used */
    uint8_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 2 lsb of type, 5 unused bits */
    /* char buf[]; */
};
struct __attribute__ ((__packed__)) sdshdr16 {
    uint16_t len; /* used */
    uint16_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 2 lsb of type, 5 unused bits */
    /* char buf[]; */
};
struct __attribute__ ((__packed__)) sdshdr32 {
    uint32_t len; /* used */
    uint32_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 2 lsb of type, 5 unused bits */
    /* char buf[]; */
};
struct __attribute__ ((__packed__)) sdshdr64 {
    uint64_t len; /* used */
    uint64_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 2 lsb of type, 5 unused bits */
    /* char buf[]; */
};

#define SDS_TYPE_8  0
#define SDS_TYPE_16 1
#define SDS_TYPE_32 2
#define SDS_TYPE_64 3
#define SDS_TYPE_MASK 3
#define SDS_TYPE_BITS 2
#define SDS_HDR_VAR(T,s) struct sdshdr##T *sh = (struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T)));
#define SDS_HDR(T,s) ((struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T))))

/* This is used to silence "unused typedef" warnings for SDS_HDR_LAMBDA. */
#if defined(__clang__)
#define SDS_NO_TYPEDEF_WARNINGS _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wunused-local-typedef\"")
#define SDS_RESET_WARNINGS _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define SDS_NO_TYPEDEF_WARNINGS _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wunused-local-typedefs\"")
#define SDS_RESET_WARNINGS _Pragma("GCC diagnostic pop")
#else
#define SDS_NO_TYPEDEF_WARNINGS
#define SDS_RESET_WARNINGS
#endif

/* Creates a single case statement for SDS_HDR_LAMBDA and SDS_HDR_LAMBDA_2.
 * It creates the following:
 *   sh: A pointer to the sds header struct.
 *   sds_hdr_uint: The unsigned version of the header's int size.
 *   sds_hdr_int: Same as above, but signed. */
#define SDS_HDR_CASE(T, s, ...)                                     \
    case SDS_TYPE_##T: {                                            \
        SDS_NO_TYPEDEF_WARNINGS                                     \
        SDS_HDR_VAR(T, s)                                           \
        typedef uint##T##_t sds_hdr_uint;                           \
        typedef int##T##_t sds_hdr_int;                             \
        do { __VA_ARGS__; } while (0);                              \
        SDS_RESET_WARNINGS                                          \
    }                                                               \
    break

/* Automatically generates the code block for each sds type. */
#define SDS_HDR_LAMBDA(s, ...) do {                                 \
    const unsigned char _flags = (s)[-1];                           \
    SDS_HDR_LAMBDA_2(s, (_flags) & SDS_TYPE_MASK, __VA_ARGS__);     \
} while (0)

/* Same as above, but takes a precalculated type option. */
#define SDS_HDR_LAMBDA_2(s, _type, ...) do {                        \
    switch ((_type)) {                                              \
        SDS_HDR_CASE(8, (s), __VA_ARGS__);                          \
        SDS_HDR_CASE(16, (s), __VA_ARGS__);                         \
        SDS_HDR_CASE(32, (s), __VA_ARGS__);                         \
        SDS_HDR_CASE(64, (s), __VA_ARGS__);                         \
    }                                                               \
} while (0)

static inline size_t sdslen(const sds s) {
    SDS_HDR_LAMBDA(s, { return sh->len; });
    return 0;
}

static inline size_t sdsavail(const sds s) {
    SDS_HDR_LAMBDA(s, { return sh->alloc - sh->len; });
    return 0;
}

static inline void sdssetlen(sds s, size_t newlen) {
    SDS_HDR_LAMBDA(s, { sh->len = newlen; });
}

static inline void sdsinclen(sds s, size_t inc) {
    SDS_HDR_LAMBDA(s, { sh->len += inc; });
}

/* sdsalloc() = sdsavail() + sdslen() */
static inline size_t sdsalloc(const sds s) {
    SDS_HDR_LAMBDA(s, { return sh->alloc; });
    return 0;
}

static inline void sdssetalloc(sds s, size_t newlen) {
    SDS_HDR_LAMBDA(s, { sh->alloc = newlen; });
}

sds sdsnewlen(const void *init, size_t initlen);
sds sdsnew(const char *init);
sds sdsempty(void);
sds sdsdup(const sds s);
void sdsfree(sds s);
sds sdsgrowzero(sds s, size_t len);
sds sdscatlen(sds s, const void *t, size_t len);
sds sdscat(sds s, const char *t);
sds sdscatsds(sds s, const sds t);
sds sdscpylen(sds s, const char *t, size_t len);
sds sdscpy(sds s, const char *t);

sds sdscatvprintf(sds s, const char *fmt, va_list ap);
#ifdef __GNUC__
sds sdscatprintf(sds s, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
#else
sds sdscatprintf(sds s, const char *fmt, ...);
#endif

sds sdscatfmt(sds s, char const *fmt, ...);
sds sdstrim(sds s, const char *cset);
void sdsrange(sds s, ptrdiff_t start, ptrdiff_t end);
void sdsupdatelen(sds s);
void sdsclear(sds s);
int sdscmp(const sds s1, const sds s2);
sds *sdssplitlen(const char *s, ptrdiff_t len, const char *sep, int seplen, int *count);
void sdsfreesplitres(sds *tokens, int count);
void sdstolower(sds s);
void sdstoupper(sds s);
sds sdsfromlonglong(long long value);
sds sdscatrepr(sds s, const char *p, size_t len);
sds *sdssplitargs(const char *line, int *argc);
sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen);
sds sdsjoin(char **argv, int argc, char *sep);
sds sdsjoinsds(sds *argv, int argc, const char *sep, size_t seplen);

/* Low level functions exposed to the user API */
sds sdsMakeRoomFor(sds s, size_t addlen);
void sdsIncrLen(sds s, ptrdiff_t incr);
sds sdsRemoveFreeSpace(sds s);
size_t sdsAllocSize(sds s);
void *sdsAllocPtr(sds s);

/* Export the allocator used by SDS to the program using SDS.
 * Sometimes the program SDS is linked to, may use a different set of
 * allocators, but may want to allocate or free things that SDS will
 * respectively free or allocate. */
void *sds_malloc(size_t size);
void *sds_realloc(void *ptr, size_t size);
void sds_free(void *ptr);

#ifdef REDIS_TEST
int sdsTest(int argc, char *argv[]);
#endif

#endif
