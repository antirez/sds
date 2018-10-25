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

#ifdef __cplusplus
#include <climits>
#else
#include <limits.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* SDS_ABORT_ON_ERROR allows us to guarantee that sds functions
 * do not return null. */
#ifdef SDS_ABORT_ON_ERROR
#  define SDS_RET_NONNULL __attribute__((__returns_nonnull__))
#else
#  define SDS_RET_NONNULL
#endif

#ifdef _WIN32
#  define SDS_EXPORT __declspec(dllexport)
#else
#  define SDS_EXPORT
#endif

/* Define some helpful function attributes. */
#if defined(__GNUC__) || defined(__clang__) /* GCC/Clang */
  /* Have GCC or Clang emit a warning when an SDS mutation is either performed
   * on a null string or if the return value is ignored. */
#  define SDS_MUT_FUNC __attribute__((__warn_unused_result__, __nonnull__(1))) SDS_RET_NONNULL
  /* The same, but instead of warning on unused, it hints to the compiler
   * that this function returns a unique pointer. */
#  define SDS_INIT_FUNC __attribute__((__warn_unused_result__, __malloc__)) SDS_RET_NONNULL
  /* An SDS function that doesn't modify the string. */
#  define SDS_CONST_FUNC  __attribute__((__nonnull__(1), __pure__))
#  define SDS_PRINTF_FUNC(fmt,args) __extension__ __attribute((                            \
    __nonnull__(1), __warn_unused_result__, __format__(printf, fmt, args))) SDS_RET_NONNULL
  /* Flags to signal that this is a likely or unlikely condition. */
#  define SDS_LIKELY(x) __builtin_expect(!!(x), 1)
#  define SDS_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else /* MSVC */
#  if defined(_MSC_VER) && (_MSC_VER >= 1700)
#    include <sal.h>
#    define SDS_MUT_FUNC _Check_return_
#    define SDS_INIT_FUNC _Check_return_ __declspec(restrict)
#    define SDS_PRINTF_FUNC(fmt, args) _Check_return_
#  else
#    define SDS_MUT_FUNC
#    define SDS_INIT_FUNC
#    define SDS_PRINTF_FUNC(fmt, args)
#  endif
#  define SDS_CONST_FUNC
#  define SDS_LIKELY(x) (x)
#  define SDS_UNLIKELY(x) (x)
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

#if defined(__STDC__) && (!defined(__STDC_VERSION__) || (defined(__STDC_VERSION__) && __STDC_VERSION__ < 199901L))
#  if defined(__GNUC__) || defined(__clang__)
#    define s_inline __inline__
#  else
#    define s_inline
#  endif
#else
#  define s_inline inline
#endif

#define SDS_MAX_PREALLOC (1024*1024)
extern const char *SDS_NOINIT;

typedef char *sds;
/* We use sds as a macro here to document that the functions are for a const sds string,
 * but if we use 'const sds', instead of const char *, we have char *const. */
#define sds char *

#ifndef SDS_32_BIT
#  if ULLONG_MAX > UINTPTR_MAX
#    define SDS_32_BIT
#  endif
#endif

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
#ifndef SDS_32_BIT
SDS_HDR_STRUCT(64)
#endif

#undef SDS_HDR_STRUCT


#define SDS_TYPE_MASK 3

enum sdshdrtype {
    SDS_TYPE_8,
    SDS_TYPE_16,
    SDS_TYPE_32
#ifndef SDS_32_BIT
    ,SDS_TYPE_64
#endif
};

#define SDS_TYPE_BITS 2
#define SDS_HDR_VAR(T,s) struct sdshdr##T *sh = (struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T) + 1));
#define SDS_HDR(T,s) ((struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T) + 1)))


/* Low level functions exposed to the user API */
SDS_MUT_FUNC SDS_EXPORT sds sdsMakeRoomFor(sds s, size_t addlen);
enum sdsstatus {
    SDS_STATUS_NOT_CHANGED,
    SDS_STATUS_CHANGED,
    SDS_STATUS_CHANGED_TYPE
};
SDS_MUT_FUNC SDS_EXPORT sds sdsMakeRoomForStatus(sds s_restrict s, size_t addlen, enum sdsstatus *s_restrict status /* out */);
/* Does not reallocate. It will abort on an unexpected size. */
SDS_EXPORT void sdsIncrLen(sds s, ptrdiff_t incr);
SDS_MUT_FUNC SDS_EXPORT sds sdsRemoveFreeSpace(sds s);
SDS_CONST_FUNC SDS_EXPORT size_t sdsAllocSize(sds s);
SDS_CONST_FUNC SDS_EXPORT void *sdsAllocPtr(sds s);

SDS_CONST_FUNC SDS_EXPORT size_t sdslen(const sds s);
SDS_CONST_FUNC SDS_EXPORT size_t sdsalloc(const sds s);
SDS_CONST_FUNC SDS_EXPORT size_t sdsavail(const sds s);
SDS_MUT_FUNC SDS_EXPORT sds sdssetlen(sds s, size_t newlen);
SDS_MUT_FUNC SDS_EXPORT sds sdsinclen(sds s, size_t inc);
SDS_MUT_FUNC SDS_EXPORT sds sdssetalloc(sds s, size_t newlen);

/* The sds version of strcmp */
SDS_CONST_FUNC SDS_EXPORT int sdscmp(const sds s1, const sds s2);

SDS_INIT_FUNC SDS_EXPORT sds sdsnewlen(const void *s_restrict init, size_t initlen);

/* Create a new sds string starting from a null terminated C string. */
SDS_INIT_FUNC static s_inline sds sdsnew(const char *s_restrict init) {
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return sdsnewlen(init, initlen);
}

/* Create an empty (zero length) sds string. Even in this case the string
 * always has an implicit null term. */
SDS_INIT_FUNC static s_inline sds sdsempty(void) {
    return sdsnewlen("",0);
}

/* Duplicate an sds string. */
SDS_INIT_FUNC static s_inline sds sdsdup(const sds s_restrict s) {
    if (s == NULL)
        return sdsnewlen("",0);
    return sdsnewlen(s, sdslen(s));
}

SDS_EXPORT void sdsfree(sds s);

SDS_MUT_FUNC SDS_EXPORT sds sdsgrowzero(sds s_restrict s, size_t len);
SDS_MUT_FUNC SDS_EXPORT sds sdscatlen(sds s_restrict s, const void *s_restrict t, size_t len);

/* Append the specified null termianted C string to the sds string 's'.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
SDS_MUT_FUNC static s_inline sds sdscat(sds s_restrict s, const char *s_restrict t) {
    return sdscatlen(s, t, strlen(t));
}

/* Append the specified sds 't' to the existing sds 's'.
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
SDS_MUT_FUNC static s_inline sds sdscatsds(sds s_restrict s, const sds s_restrict t) {
    return sdscatlen(s, t, sdslen(t));
}

SDS_MUT_FUNC SDS_EXPORT sds sdscpylen(sds s_restrict s, const char *s_restrict t, size_t len);

/* Like sdscpylen() but 't' must be a null-termined string so that the length
 * of the string is obtained with strlen(). */
SDS_MUT_FUNC static s_inline sds sdscpy(sds s_restrict s, const char *s_restrict t) {
    return sdscpylen(s, t, strlen(t));
}

SDS_PRINTF_FUNC(2,0) SDS_EXPORT sds sdscatvprintf(sds s_restrict s, const char *s_restrict fmt,
                                       va_list ap);
SDS_PRINTF_FUNC(2,3) SDS_EXPORT sds sdscatprintf(sds s_restrict s, const char *s_restrict fmt, ...);

SDS_MUT_FUNC SDS_EXPORT sds sdscatfmt(sds s_restrict s, const char *s_restrict fmt, ...);

SDS_MUT_FUNC SDS_EXPORT sds sdstrim(sds s_restrict s, const char *s_restrict cset);
SDS_MUT_FUNC SDS_EXPORT sds sdsrange(sds s, ptrdiff_t start, ptrdiff_t end);

/* Set the sds string length to the length as obtained with strlen(), so
 * considering as content only up to the first null term character.
 *
 * This function is useful when the sds string is hacked manually in some
 * way, like in the following example:
 *
 * s = sdsnew("foobar");
 * s[2] = '\0';
 * sdsupdatelen(s);
 * printf("%d\n", sdslen(s));
 *
 * The output will be "2", but if we comment out the call to sdsupdatelen()
 * the output will be "6" as the string was modified but the logical length
 * remains 6 bytes. */
SDS_MUT_FUNC static s_inline sds sdsupdatelen(sds s) {
    size_t reallen = strlen(s);
    return sdssetlen(s, reallen);
}
/* Modify an sds string in-place to make it empty (zero length).
 * However all the existing buffer is not discarded but set as free space
 * so that next append operations will not require allocations up to the
 * number of bytes previously available. */
SDS_MUT_FUNC static s_inline sds sdsclear(sds s) {
    s = sdssetlen(s, 0);
    s[0] = '\0';
    return s;
}

SDS_INIT_FUNC SDS_EXPORT sds *sdssplitlen(const char *s_restrict s, ptrdiff_t len, const char *s_restrict sep, int seplen, int *s_restrict count);

/* Like sdssplitlen, but uses strlen for len and seplen. */
SDS_INIT_FUNC static s_inline sds *sdssplit(const char *s_restrict s, const char *s_restrict sep, int *s_restrict count) {
    return sdssplitlen(s, strlen(s), sep, strlen(sep), count);
}
void sdsfreesplitres(sds *tokens, int count);
SDS_MUT_FUNC SDS_EXPORT sds sdstolower(sds s);
SDS_MUT_FUNC SDS_EXPORT sds sdstoupper(sds s);

/* Appends a single character to an sds string. */
SDS_MUT_FUNC SDS_EXPORT sds sdsaddchar(sds s, unsigned int c);
SDS_MUT_FUNC SDS_EXPORT sds sdsaddint(sds s, int value);
SDS_MUT_FUNC SDS_EXPORT sds sdsadduint(sds s, unsigned int value);
SDS_MUT_FUNC SDS_EXPORT sds sdsaddlonglong(sds s, long long value);
SDS_MUT_FUNC SDS_EXPORT sds sdsaddulonglong(sds s, unsigned long long value);

SDS_INIT_FUNC SDS_EXPORT sds sdsfromint(int value);
SDS_INIT_FUNC SDS_EXPORT sds sdsfromuint(unsigned int value);
SDS_INIT_FUNC SDS_EXPORT sds sdsfromlonglong(long long value);
SDS_INIT_FUNC SDS_EXPORT sds sdsfromulonglong(unsigned long long value);

SDS_MUT_FUNC SDS_EXPORT sds sdscatrepr(sds s_restrict s, const char *s_restrict p, size_t len);
SDS_INIT_FUNC SDS_EXPORT sds *sdssplitargs(const char *s_restrict line, int *s_restrict argc);
SDS_MUT_FUNC SDS_EXPORT sds sdsmapchars(sds s_restrict s, const char *s_restrict from, const char *s_restrict to, size_t setlen);
SDS_MUT_FUNC SDS_EXPORT sds sdsjoin(const char **s_restrict argv, int argc, const char *s_restrict sep);
SDS_MUT_FUNC SDS_EXPORT sds sdsjoinsds(sds *s_restrict argv, int argc, const char *s_restrict sep, size_t seplen);

/* Export the allocator used by SDS to the program using SDS.
 * Sometimes the program SDS is linked to, may use a different set of
 * allocators, but may want to allocate or free things that SDS will
 * respectively free or allocate. */
SDS_EXPORT void *sds_malloc(size_t size);
SDS_EXPORT void *sds_realloc(void *ptr, size_t size);
SDS_EXPORT void sds_free(void *ptr);

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
   /* GCC 3.2 and all Clang versions support type 3. */
#  elif !defined(__cplusplus) && (defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 2))))
#    define SDSADD_TYPE 3
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
#include <climits>

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
            /* GCC 4.9 doesn't get the point and does sign warnings here */
            if (static_cast<typename std::make_unsigned<T>::type>(x) <= UINT_MAX)
                return sdsadduint(s, static_cast<unsigned>(x));
            else
                return sdsaddulonglong(s, x);
        } else {
            if (static_cast<typename std::make_signed<T>::type>(x) <= INT_MAX
             && static_cast<typename std::make_signed<T>::type>(x) >= INT_MIN)
                return sdsaddint(s, static_cast<int>(x));
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

/* Generate a compile error. */
extern int sdsadd_bad_argument(sds, void *);

/* To make things a little nicer. We still need the mess of parentheses though. */
#define _SDS_TYPE_CMP(x,y, true_, false_)                                   \
    __builtin_choose_expr(                                                  \
        __builtin_types_compatible_p(__typeof__(x), y), true_, false_       \
    )

#define _SDSADD_CHECK(s, x)                                                 \
            (_SDS_TYPE_CMP((x), char, sdsaddchar,                           \
             _SDS_TYPE_CMP((x), const char[], sdscat,                       \
             _SDS_TYPE_CMP((x), char[], sdscat,                             \
             _SDS_TYPE_CMP((x), const char *, sdscat,                       \
             _SDS_TYPE_CMP((x), char *, sdscat,                             \
             _SDS_TYPE_CMP((x), int,                                        \
                 SDS_IS_CHAR(x) ? (sds (*)(sds,int))sdsaddchar : sdsaddint, \
            _SDS_TYPE_CMP((x), unsigned,                                    \
                 SDS_IS_CHAR(x) ? sdsaddchar : sdsadduint,                  \
            _SDS_TYPE_CMP((x), long long, sdsaddlonglong,                   \
            _SDS_TYPE_CMP((x), unsigned long long, sdsaddulonglong,         \
            sdsadd_bad_argument /* to mess up the assertion below */        \
        )))))))))((s), (x)))

#define sdsadd(s, x) __extension__({                                        \
        /* If an invalid option is used above, _s2 will be int and          \
         * sdsadd_invalid_type would expand to a negative array. */         \
    __attribute__((__unused__)) extern char sdsadd_invalid_type[            \
            (2 * !!__builtin_types_compatible_p(                            \
                __typeof__(_SDSADD_CHECK(s,x)), sds)) - 1                   \
        ];                                                                  \
    _SDSADD_CHECK(s, x);                                                    \
})
#else /* Not supported */
#define sdsadd(s, x) sdsadd_not_supported_in_this_compiler[-1];
#endif /* sdsadd macros */

#undef sds

#endif
