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

#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Define some helpful function attributes. */
#if defined(__GNUC__) || defined(__clang__) /* GCC/Clang */
  /* Have GCC or Clang emit a warning when an SDS mutation is either performed
   * on a null string or if the return value is ignored. */
#  define SDS_MUT_FUNC __attribute__((__warn_unused_result__, __nonnull__(1)))
  /* The same, but instead of warning on unused, it hints to the compiler
   * that this function returns a unique pointer. */
#  define SDS_INIT_FUNC __extension__ __attribute__((__warn_unused_result__, __malloc__))
  /* An SDS function that doesn't modify the string. */
#  define SDS_CONST_FUNC __extension__ __attribute__((__nonnull__(1), __pure__))
#  define SDS_PRINTF_FUNC(fmt,args) __extension__ __attribute((                            \
    __nonnull__(1), __warn_unused_result__, __format__(printf, fmt, args)))
#  define SDS_FMT_STR
  /* Flags to signal that this is a likely or unlikely condition. */
#  define SDS_LIKELY(x) __extension__ __builtin_expect(!!(x), 1)
#  define SDS_UNLIKELY(x) __extension__ __builtin_expect(!!(x), 0)
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

#ifndef SDS_32_BIT
#  if UINT32_MAX == UINTPTR_MAX
#    define SDS_32_BIT
#  endif
#endif

#ifdef SDS_32_BIT
#  define SDS_64_BIT_ONLY(...)
#else
#  define SDS_64_BIT_ONLY(...) __VA_ARGS__
#endif

/* restrict keyword */
#ifndef s_restrict
#  if defined(restrict) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#    define s_restrict restrict
#  elif defined(__cplusplus) && (defined(__GNUC__) || defined(__clang__))
#    define s_restrict __restrict__
#  else
#    define s_restrict
#  endif
#endif

/* If you define SDS_ABORT_ON_ERROR, instead of the sds functions returning
 * NULL, it will print a message and abort. */
#ifdef SDS_ABORT_ON_ERROR
#  define SDS_UNREACHABLE(...)                                          \
    do {                                                                \
        fprintf(stderr, "%s:%d: sds reached unreachable code!\n"        \
                        "Aborting because of SDS_ABORT_ON_ERROR.\n",    \
                __FILE__, __LINE__);                                    \
        abort();                                                        \
    } while (0)
#  define SDS_ERR_RETURN(...)                                           \
    do {                                                                \
        fprintf(stderr, "%s:%d: sds encountered an error!\n"            \
                        "Aborting because of SDS_ABORT_ON_ERROR.\n",    \
                __FILE__, __LINE__);                                    \
        abort();                                                        \
    } while (0)
#else
#  if defined(__GNUC__) || defined(__clang__)
#    define SDS_UNREACHABLE(...) __builtin_unreachable(); return __VA_ARGS__
#  else
#    define SDS_UNREACHABLE(...) return __VA_ARGS__
#  endif
#  define SDS_ERR_RETURN(...) return __VA_ARGS__
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

#undef SDS_HDR_STRUCT

#define SDS_TYPE_8  0
#define SDS_TYPE_16 1
#define SDS_TYPE_32 2
#define SDS_TYPE_64 3
#define SDS_TYPE_MASK 3
#define SDS_TYPE_BITS 2
#define SDS_HDR_VAR(T,s) struct sdshdr##T *sh = (struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T) + 1));
#define SDS_HDR(T,s) ((struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T) + 1)))

/* Creates a single case statement for SDS_HDR_LAMBDA and SDS_HDR_LAMBDA_2.
 * It creates the following:
 *   sh: A pointer to the sds header struct.
 *   sdshdr: A typedef of the sdshdr struct.
 *   sdshdr_uint: The unsigned version of the header's int size.
 *   sdshdr_int: Same as above, but signed. */
#define SDS_HDR_CASE(T, s, ...)                                     \
    case SDS_TYPE_##T: {                                            \
        sds _s = (s); /* prevent null arithmetic warnings */        \
        typedef struct sdshdr##T sdshdr;                            \
        typedef uint##T##_t sdshdr_uint;                            \
        typedef int##T##_t sdshdr_int;                              \
        {   /* C90 needs a block here */                            \
            sdshdr *sh = NULL;                                      \
            /* Avoid unused variable/typedef warnings which are */  \
            /* bugged and can't be silenced with pragmas in gcc. */ \
            extern sdshdr_uint _sds_uint##T;                        \
            extern sdshdr_int _sds_int##T;                          \
            (void)_sds_uint##T;                                     \
            (void)_sds_int##T;                                      \
            (void)sh;                                               \
            /* Only set sh if s is not NULL */                      \
            if (_s != NULL)                                         \
                sh = SDS_HDR(T,_s);                                 \
            { __VA_ARGS__; }                                        \
        }                                                           \
    }                                                               \
    break

/* Automatically generates the code block for each sds type. */
#define SDS_HDR_LAMBDA(s, ...) {                                    \
    const unsigned char _flags = (s)[-1];                           \
    SDS_HDR_LAMBDA_2(s, (_flags) & SDS_TYPE_MASK, __VA_ARGS__);     \
}

/* Same as above, but takes a precalculated type option. */
#define SDS_HDR_LAMBDA_2(s, _type, ...) {                           \
    switch ((_type)) {                                              \
        SDS_HDR_CASE(8, (s), __VA_ARGS__);                          \
        SDS_HDR_CASE(16, (s), __VA_ARGS__);                         \
        SDS_HDR_CASE(32, (s), __VA_ARGS__);                         \
        SDS_64_BIT_ONLY(SDS_HDR_CASE(64, (s), __VA_ARGS__);)        \
    }                                                               \
}


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
    SDS_UNREACHABLE(0);
}
/* Available space on an sds string */
SDS_CONST_FUNC static inline size_t sdsavail(const sds s) {
    SDS_HDR_LAMBDA(s, { return sh->alloc - sh->len; });
    SDS_UNREACHABLE(0);
}
/* sdsalloc() = sdsavail() + sdslen() */
SDS_CONST_FUNC static inline size_t sdsalloc(const sds s) {
    SDS_HDR_LAMBDA(s, { return sh->alloc; });
    SDS_UNREACHABLE(0);
}

/* Breaking changes: These now may reallocate and return themselves.*/
SDS_MUT_FUNC static inline sds sdssetlen(sds s, size_t newlen) {
    int i;
    for (i = 0; i < 2; ++i) {
        SDS_HDR_LAMBDA(s, {
            /* Check if we need space */
            if (SDS_UNLIKELY(sh->alloc < newlen + 1)) {
                s = sdsMakeRoomFor(s, newlen + 1);
                continue;
            }
            sh->len = newlen;
            return s;
        });
    }
    SDS_UNREACHABLE(NULL);
}
SDS_MUT_FUNC static inline sds sdsinclen(sds s, size_t inc) {
    int i;
    for (i = 0; i < 2; ++i) {
        SDS_HDR_LAMBDA(s, {
            /* Check if we need space */
            if (SDS_UNLIKELY(sh->alloc < sh->len + inc + 1)) {
                s = sdsMakeRoomFor(s, sh->len + inc + 1);
                continue;
            }
            sh->len += inc;
            return s;
        });
    }
    SDS_UNREACHABLE(NULL);
}
SDS_MUT_FUNC static inline sds sdssetalloc(sds s, size_t newlen) {
    int i;
    for (i = 0; i < 2; ++i) {
        SDS_HDR_LAMBDA(s, {
            if (SDS_UNLIKELY(newlen > (sdshdr_uint)-1)) {
                s = sdsMakeRoomFor(s, newlen);
                continue;
            }
            sh->alloc = newlen;
            return s;
        });
    }
    SDS_UNREACHABLE(NULL);
}
/* The sds version of strcmp */
SDS_CONST_FUNC int sdscmp(const sds s1, const sds s2);

SDS_INIT_FUNC sds sdsnewlen(const void *s_restrict init, size_t initlen);
SDS_INIT_FUNC sds sdsnew(const char *s_restrict init);
SDS_INIT_FUNC sds sdsempty(void);
SDS_INIT_FUNC sds sdsdup(const sds s_restrict s);
void sdsfree(sds s);

SDS_MUT_FUNC sds sdsgrowzero(sds s_restrict s, size_t len);
SDS_MUT_FUNC sds sdscatlen(sds s_restrict s, const void *s_restrict t, size_t len);
SDS_MUT_FUNC sds sdscat(sds s_restrict s, const char *s_restrict t);
SDS_MUT_FUNC sds sdscatsds(sds s_restrict s, const sds s_restrict t);
SDS_MUT_FUNC sds sdscpylen(sds s_restrict s, const char *s_restrict t, size_t len);
SDS_MUT_FUNC sds sdscpy(sds s_restrict s, const char *s_restrict t);

SDS_PRINTF_FUNC(2,0) sds sdscatvprintf(sds s_restrict s, SDS_FMT_STR const char *s_restrict fmt,
                                       va_list ap);
SDS_PRINTF_FUNC(2,3) sds sdscatprintf(sds s_restrict s, SDS_FMT_STR const char *s_restrict fmt, ...);

SDS_MUT_FUNC sds sdscatfmt(sds s_restrict s, const char *s_restrict fmt, ...);

SDS_MUT_FUNC sds sdstrim(sds s_restrict s, const char *s_restrict cset);
SDS_MUT_FUNC sds sdsrange(sds s, ptrdiff_t start, ptrdiff_t end);
SDS_MUT_FUNC sds sdsupdatelen(sds s);
SDS_MUT_FUNC sds sdsclear(sds s);

SDS_INIT_FUNC sds *sdssplitlen(const char *s_restrict s, ptrdiff_t len, const char *s_restrict sep, int seplen, int *count);
SDS_INIT_FUNC static inline sds *sdssplit(const char *s_restrict s, const char *s_restrict sep, int *count) {
    return sdssplitlen(s, strlen(s), sep, strlen(sep), count);
}
void sdsfreesplitres(sds *tokens, int count);
SDS_MUT_FUNC sds sdstolower(sds s);
SDS_MUT_FUNC sds sdstoupper(sds s);

/* Appends a single character to an sds string. */
SDS_MUT_FUNC static inline sds sdsaddchar(sds s, unsigned int c) {
    size_t len, i;

    for (i = 0; i < 2; ++i) {
        SDS_HDR_LAMBDA(s, {
            len = sh->len;
            if (sh->alloc - len < 1) {
                s = sdsMakeRoomFor(s, 2);
                continue;
            }
            sh->len++;
        });
	    s[len] = (char)c;
        s[len + 1] = '\0';
        return s;
    }
    SDS_UNREACHABLE(NULL);
}
SDS_MUT_FUNC sds sdsaddint(sds s, int value);
SDS_MUT_FUNC sds sdsadduint(sds s, unsigned int value);
SDS_MUT_FUNC sds sdsaddlonglong(sds s, long long value);
SDS_MUT_FUNC sds sdsaddulonglong(sds s, unsigned long long value);

SDS_INIT_FUNC sds sdsfromint(int value);
SDS_INIT_FUNC sds sdsfromuint(unsigned int value);
SDS_INIT_FUNC sds sdsfromlonglong(long long value);
SDS_INIT_FUNC sds sdsfromulonglong(unsigned long long value);

SDS_MUT_FUNC sds sdscatrepr(sds s_restrict s, const char *s_restrict p, size_t len);
SDS_INIT_FUNC sds *sdssplitargs(const char *s_restrict line, int *s_restrict argc);
SDS_MUT_FUNC sds sdsmapchars(sds s_restrict s, const char *s_restrict from, const char *s_restrict to, size_t setlen);
SDS_MUT_FUNC sds sdsjoin(const char **s_restrict argv, int argc, const char *s_restrict sep);
SDS_MUT_FUNC sds sdsjoinsds(sds *s_restrict argv, int argc, const char *s_restrict sep, size_t seplen);

/* Export the allocator used by SDS to the program using SDS.
 * Sometimes the program SDS is linked to, may use a different set of
 * allocators, but may want to allocate or free things that SDS will
 * respectively free or allocate. */
void *sds_malloc(size_t size);
void *sds_realloc(void *ptr, size_t size);
void sds_free(void *ptr);

#ifdef __cplusplus
}
#endif

/* basic std::string wrappers */
#ifdef __cplusplus
#include <string>
SDS_MUT_FUNC static inline sds sdsaddstdstr(sds s_restrict s, const std::string &s_restrict x) {
    return sdscatlen(s, x.c_str(), x.length());
}
static inline std::string sds2stdstr(const sds s) {
    return std::string(s, sdslen(s));
}
SDS_INIT_FUNC static inline sds sdsfromstdstr(const std::string &s_restrict x) {
    return sdsnewlen(x.c_str(), x.length());
}
#endif

/* A hacky macro that detects most character literals. Wrap in parentheses to disable.
 * What this does is stringify x and check if it starts or ends in a single quote.
 *
 * This lets us detect character literals even after promotion, which even the most
 * complicated C++ template can't detect. */
#define SDS_IS_CHAR(x) ((#x[0] == '\'') || (sizeof(#x) > 3 && #x[sizeof(#x) - 2] == '\''))

#ifndef SDSADD_TYPE
#  if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#    define SDSADD_TYPE 1 /* _Generic */
#  elif defined(__cplusplus) && __cplusplus >= 201103L
#    define SDSADD_TYPE 2 /* C++ overload/type_traits */
#  elif !defined(__cplusplus) && (defined(__GNUC__) || defined(__clang__))
#    define SDSADD_TYPE 3 /* __builtin_types_compatible_p/__builtin_choose_expr */
#  else
#    define SDSADD_TYPE 0 /* not supported */
#  endif
#endif /* SDSADD_TYPE */

/* Code for the special sdsadd macro. */
#if SDSADD_TYPE == 1 /* _Generic */
#define sdsadd(s, x)                                                        \
    _Generic((x),                                                           \
        char *: sdscat, const char *: sdscat,                               \
        char: sdsaddchar,                                                   \
        int: SDS_IS_CHAR(x) ? (sds (*)(sds, int))sdsaddchar : sdsaddint,    \
        unsigned: SDS_IS_CHAR(x) ? sdsaddchar : sdsadduint,                 \
        long long: sdsaddlonglong,                                          \
        unsigned long long: sdsaddulonglong                                 \
    )((s), (x))

#elif SDSADD_TYPE == 2 /* C++ */
/* We use overloads and type_traits for C++. */
#include <type_traits>
#include <limits.h>

/* This struct is needed because switching overloads in a ternary
 * isn't possible without specializations and casts, and C++ doesn't
 * allow partial function specialization. What happens is that when
 * SDS_IS_CHAR expands to 1, it will specialize to the one below
 * that forces the char overload. */
template <bool>
struct _sdsadd_t_ {
    /* just to make things cleaner */
    template <typename T>
    using base_type = typename std::decay<T>::type;

    /* C++ allows arrays to be passed as parameter references, and if it can
     * deduct its size, it can pass it as a template parameter, avoiding a strlen
     * call. However, it is impossible to have priority over const char * without
     * !std::is_array, because for some reason, const char * outprioritizes
     * const char[4]. */
    template <size_t N>
    SDS_MUT_FUNC static inline sds _sdsadd(sds s_restrict s, const char (&s_restrict x)[N]) {
        return sdscatlen(s, x, N - 1);
    }
    /* C string, with an unknown compile-time size. */
    template <typename T,
        typename std::enable_if<
            std::is_convertible<base_type<T>, const char *>::value
            && !std::is_array<T>::value /* Disables arrays to enable the above overload */
        , int>::type = 0>
    SDS_MUT_FUNC static inline sds _sdsadd(sds s_restrict s, const T x) {
        return sdscat(s, x);
    }
    /* Catches all integers. */
    template <typename T,
        typename std::enable_if<
            std::is_integral<base_type<T>>::value, int
        >::type = 0>
    SDS_MUT_FUNC static inline sds _sdsadd(sds s_restrict s, const T x) {
        if (std::is_unsigned<base_type<T>>::value) {
            if (x <= UINT_MAX)
                return sdsadduint(s, static_cast<unsigned>(x));
            else
                return sdsaddulonglong(s, x);
        } else {
            if (x <= INT_MAX && x >= INT_MIN)
                return sdsaddint(s, static_cast<unsigned>(x));
            else
                return sdsaddlonglong(s, x);
        }
    }
    /* char. This has overload priority over the generic one. */
    SDS_MUT_FUNC static inline sds _sdsadd(sds s_restrict s, const char x) {
        return sdsaddchar(s, x);
    }
    /* std::string */
    SDS_MUT_FUNC static inline sds _sdsadd(sds s_restrict s, const std::string &s_restrict x) {
        return sdsaddstdstr(s, x);
    }
};

/* If SDS_IS_CHAR(x) expands to true in the macro below, it will specialize
 * _sdsadd_t_ to use this overload. */
template <> struct _sdsadd_t_<true> {
    SDS_MUT_FUNC static inline sds _sdsadd(sds s_restrict s, const unsigned x) {
        return sdsaddchar(s, x);
    }
};

#define sdsadd(s, x) _sdsadd_t_<SDS_IS_CHAR(x)>::_sdsadd((s), (x))

#elif SDSADD_TYPE == 3 /* GCC extensions */

extern sds sdsadd_bad_argument(sds, void *);

/* To make things a little nicer. We still need the mess of parentheses though. */
#define _SDS_TYPE_CMP(x,y, true_, false_)                                   \
    __builtin_choose_expr(                                                  \
        __builtin_types_compatible_p(__typeof__(x), y), true_, false_       \
    )

#define sdsadd(s, x) __extension__                                          \
    _SDS_TYPE_CMP((x), const char[], sdscat,                                \
    _SDS_TYPE_CMP((x), char[], sdscat,                                      \
    _SDS_TYPE_CMP((x), const char *, sdscat,                                \
    _SDS_TYPE_CMP((x), char *, sdscat,                                      \
    _SDS_TYPE_CMP((x), char, sdsaddchar,                                    \
    _SDS_TYPE_CMP((x), int,                                                 \
         (SDS_IS_CHAR(x) ? (sds (*)(sds, int))sdsaddchar : sdsaddint),      \
    _SDS_TYPE_CMP((x), unsigned,                                            \
         (SDS_IS_CHAR(x) ? sdsaddchar : sdsadduint),                        \
    _SDS_TYPE_CMP((x), long long, sdsaddlonglong,                           \
    _SDS_TYPE_CMP((x), unsigned long long, sdsaddulonglong,                 \
    sdsadd_bad_argument)))))))))((s), (x))
#else /* Not supported */
#define sdsadd(s, x) do { char sdsadd_not_supported_in_this_compiler[-1]; } while (0)
#endif /* sdsadd macros */
#endif
