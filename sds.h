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

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

/* Define some helpful function attributes. */
#if defined(__GNUC__) || defined(__clang__) /* GCC/Clang */
  /* Have GCC or Clang emit a warning when an SDS mutation is either performed
   * on a null string or if the return value is ignored. */
#  define SDS_MUT_FUNC __attribute__((__warn_unused_result__, __nonnull__(1)))
  /* The same, but instead of warning on unused, it hints to the compiler
   * that this function returns a unique pointer. */
#  define SDS_INIT_FUNC __attribute__((__warn_unused_result__, __malloc__))
  /* An SDS function that doesn't modify the string. */
#  define SDS_CONST_FUNC __attribute__((__nonnull__(1), __pure__))
#  define SDS_PRINTF_FUNC(fmt,args) __attribute((                            \
    __nonnull__(1), __warn_unused_result__, __format__(printf, fmt, args)))
#  define SDS_FMT_STR
  /* Flags to signal that this is a likely or unlikely condition. */
#  define SDS_LIKELY(x) __builtin_expect(!!(x), 1)
#  define SDS_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else /* MSVC */
#  if defined(_MSVC_VER) && (_MSVC_VER >= 1700)
#    include <sal.h>
#    define SDS_MUT_FUNC _Check_return_
#    define SDS_INIT_FUNC _Check_return_ __declspec(restrict)
#    define SDS_FMT_STR _Printf_format_str_
#    define SDS_PRINTF_FUNC(fmt, args) _Check_return_
#  else
#    define SDS_MUT_FUNC
#    define SDS_INIT_FUNC
#    define SDS_FMT_STR
#    define SDS_PRINTF_FUNC(fmt, args)
#  endif
#  define SDS_CONST_FUNC
#  define SDS_LIKELY(x) (x)
#  define SDS_UNLIKELY(x) (x)
#endif

#define SDS_MAX_PREALLOC (1024*1024)
extern const char *SDS_NOINIT;

typedef char *sds;

/* Declare the SDS structs. They all look the same. flags is commented out
 * to avoid the non-portable #pragma pack or __attribute__((__packed__)). */
#define SDS_HDR_STRUCT(T)                                               \
struct sdshdr##T {                                                      \
    uint##T##_t len; /* used */                                         \
    uint##T##_t alloc; /* excluding the header and null terminator */   \
    /* unsigned char flags; */ /* 2 lsb of type, 6 unused bits */       \
    /* char buf[]; */ /* String data here */                            \
};

SDS_HDR_STRUCT(8)
SDS_HDR_STRUCT(16)
SDS_HDR_STRUCT(32)
SDS_HDR_STRUCT(64)

#define SDS_TYPE_8  0
#define SDS_TYPE_16 1
#define SDS_TYPE_32 2
#define SDS_TYPE_64 3
#define SDS_TYPE_MASK 3
#define SDS_TYPE_BITS 2
#define SDS_HDR_VAR(T,s) struct sdshdr##T *sh = (struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T) + 1));
#define SDS_HDR(T,s) ((struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T) + 1)))

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
 *   sds_hdr: A typedef of the sdshdr struct.
 *   sds_hdr_uint: The unsigned version of the header's int size.
 *   sds_hdr_int: Same as above, but signed. */
#define SDS_HDR_CASE(T, s, ...)                                     \
    case SDS_TYPE_##T: {                                            \
        typedef struct sdshdr##T sdshdr;                            \
        {   /* C90 needs a block here */                            \
            sdshdr *sh = NULL;                                      \
            if ((s) != NULL)                                        \
                sh = SDS_HDR(T,s);                                  \
            SDS_NO_TYPEDEF_WARNINGS                                 \
            typedef uint##T##_t sdshdr_uint;                        \
            typedef int##T##_t sdshdr_int;                          \
            do { __VA_ARGS__; } while (0);                          \
            SDS_RESET_WARNINGS                                      \
        }                                                           \
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


/* Low level functions exposed to the user API */
SDS_MUT_FUNC sds sdsMakeRoomFor(sds s, size_t addlen);
/* Does not reallocate. It will abort on an unexpected size. */
void sdsIncrLen(sds s, ptrdiff_t incr);
SDS_MUT_FUNC sds sdsRemoveFreeSpace(sds s);
SDS_CONST_FUNC size_t sdsAllocSize(sds s);
SDS_CONST_FUNC void *sdsAllocPtr(sds s);


/* Length of an sds string. Use it like strlen. */
SDS_CONST_FUNC static inline size_t sdslen(const sds s) {
    SDS_HDR_LAMBDA(s, { return sh->len; });
    return 0;
}
/* Available space on an sds string */
SDS_CONST_FUNC static inline size_t sdsavail(const sds s) {
    SDS_HDR_LAMBDA(s, { return sh->alloc - sh->len; });
    return 0;
}
/* sdsalloc() = sdsavail() + sdslen() */
SDS_CONST_FUNC static inline size_t sdsalloc(const sds s) {
    SDS_HDR_LAMBDA(s, { return sh->alloc; });
    return 0;
}

/* Breaking changes: These now may reallocate and return themselves.*/
SDS_MUT_FUNC static inline sds sdssetlen(sds s, size_t newlen) {
retry:
    SDS_HDR_LAMBDA(s, {
        /* Check if we need space */
        if (SDS_UNLIKELY(sh->alloc < newlen + 1)) {
            s = sdsMakeRoomFor(s, newlen + 1);
            goto retry;
        }
        sh->len = newlen;
    });
    return s;
}
SDS_MUT_FUNC static inline sds sdsinclen(sds s, size_t inc) {
retry:
    SDS_HDR_LAMBDA(s, {
        /* Check if we need space */
        if (SDS_UNLIKELY(sh->alloc < sh->len + inc + 1)) {
            s = sdsMakeRoomFor(s, sh->len + inc + 1);
            goto retry;
        }
        sh->len += inc;
    });
    return s;
}
SDS_MUT_FUNC static inline sds sdssetalloc(sds s, size_t newlen) {
retry:
    SDS_HDR_LAMBDA(s, {
        if (SDS_UNLIKELY(newlen > (sdshdr_uint)-1)) {
            s = sdsMakeRoomFor(s, newlen);
            goto retry;
        }
        sh->alloc = newlen;
    });
    return s;
}
/* The sds version of strcmp */
SDS_CONST_FUNC int sdscmp(const sds s1, const sds s2);

SDS_INIT_FUNC sds sdsnewlen(const void *init, size_t initlen);
SDS_INIT_FUNC sds sdsnew(const char *init);
SDS_INIT_FUNC sds sdsempty(void);
SDS_INIT_FUNC sds sdsdup(const sds s);
void sdsfree(sds s);

SDS_MUT_FUNC sds sdsgrowzero(sds s, size_t len);
SDS_MUT_FUNC sds sdscatlen(sds s, const void *t, size_t len);
SDS_MUT_FUNC sds sdscat(sds s, const char *t);
SDS_MUT_FUNC sds sdscatsds(sds s, const sds t);
SDS_MUT_FUNC sds sdscpylen(sds s, const char *t, size_t len);
SDS_MUT_FUNC sds sdscpy(sds s, const char *t);

SDS_PRINTF_FUNC(2,0) sds sdscatvprintf(sds s, SDS_FMT_STR const char *fmt,
                                       va_list ap);
SDS_PRINTF_FUNC(2,3) sds sdscatprintf(sds s, SDS_FMT_STR const char *fmt, ...);

SDS_MUT_FUNC sds sdscatfmt(sds s, const char *fmt, ...);

SDS_MUT_FUNC sds sdstrim(sds s, const char *cset);
SDS_MUT_FUNC sds sdsrange(sds s, ptrdiff_t start, ptrdiff_t end);
SDS_MUT_FUNC sds sdsupdatelen(sds s);
SDS_MUT_FUNC sds sdsclear(sds s);

SDS_INIT_FUNC sds *sdssplitlen(const char *s, ptrdiff_t len, const char *sep, int seplen, int *count);
void sdsfreesplitres(sds *tokens, int count);
SDS_MUT_FUNC sds sdstolower(sds s);
SDS_MUT_FUNC sds sdstoupper(sds s);
SDS_INIT_FUNC sds sdsfromlonglong(long long value);
SDS_MUT_FUNC sds sdscatrepr(sds s, const char *p, size_t len);
SDS_INIT_FUNC sds *sdssplitargs(const char *line, int *argc);
SDS_MUT_FUNC sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen);
SDS_MUT_FUNC sds sdsjoin(char **argv, int argc, char *sep);
SDS_MUT_FUNC sds sdsjoinsds(sds *argv, int argc, const char *sep, size_t seplen);

/* Export the allocator used by SDS to the program using SDS.
 * Sometimes the program SDS is linked to, may use a different set of
 * allocators, but may want to allocate or free things that SDS will
 * respectively free or allocate. */
void *sds_malloc(size_t size);
void *sds_realloc(void *ptr, size_t size);
void sds_free(void *ptr);

#endif
