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

#ifndef SDS_H
#define SDS_H

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


#if defined(__clang__)
#  define SDS_CLANG_VER 1
#  define SDS_GCC_VER 0
#elif defined(__GNUC__)
#  define SDS_CLANG_VER 0
#  define SDS_GCC_VER (__GNUC__ * 100 + __GNUC_MINOR__)
#else
#  define SDS_CLANG_VER 0
#  define SDS_GCC_VER 0
#endif

#ifndef __has_attribute
#  define __has_attribute(x) 0
#endif
#ifndef __has_feature
#  define __has_feature(x) 0
#endif
#ifndef __has_builtin
#  define __has_builtin(x) 0
#endif

#ifdef _WIN32
#  define SDS_EXPORT __declspec(dllexport)
#else
#  define SDS_EXPORT
#endif

#ifdef SDS_DEBUG
#  define SDS_ABORT_ON_ERROR
#  if __has_attribute(__pass_object_size__) /* compile with -O2 */
#    define SDS_BOUNDED __attribute__((__pass_object_size__(0)))
#  else
#    define SDS_BOUNDED
#  endif
#else
#  define SDS_BOUNDED
#  define SDS_BOUNDS_CHECK(x, len) do {} while (0)
#endif

/* Define some helpful function attributes. */
#if (SDS_GCC_VER >= 300 || SDS_CLANG_VER) /* GCC/Clang */
  /* SDS_ABORT_ON_ERROR allows us to guarantee that sds functions
   * do not return null. */
#  if defined(SDS_ABORT_ON_ERROR) && (__has_attribute(__returns_nonnull__) || SDS_GCC_VER >= 409)
#    define SDS_RET_NONNULL __attribute__((__returns_nonnull__))
#  else
#    define SDS_RET_NONNULL
#  endif

#  if (SDS_CLANG_VER || SDS_GCC_VER >= 304)
#    define SDS_WARN_UNUSED __attribute__((__warn_unused_result__))
#  else
#    define SDS_WARN_UNUSED
#  endif

#  if (SDS_CLANG_VER || SDS_GCC_VER >= 303)
#    define SDS_NONNULL(...) __attribute__((__nonnull__(__VA_ARGS__)))
#  else
#    define _SDS_NONNULL(...)
#  endif

  /* Have GCC or Clang emit a warning when an SDS mutation is either performed
   * on a null string or if the return value is ignored. */
#  define SDS_MUT_FUNC SDS_WARN_UNUSED SDS_NONNULL(1) SDS_RET_NONNULL
  /* The same, but instead of warning on unused, it hints to the compiler
   * that this function returns a unique pointer. */
#  define SDS_ALLOC_FUNC SDS_WARN_UNUSED __attribute__((__malloc__)) \
    SDS_RET_NONNULL
  /* An SDS function that doesn't modify the string. */
#  define SDS_CONST_FUNC  __attribute__((__pure__)) SDS_NONNULL(1)
#  define SDS_PRINTF_FUNC(fmt,args) \
    __attribute__((__format__(printf, fmt, args))) SDS_MUT_FUNC
#else /* MSVC */
#  if defined(_MSC_VER) && (_MSC_VER >= 1700)
#    include <sal.h>
#    define SDS_MUT_FUNC _Check_return_
#    define SDS_ALLOC_FUNC _Check_return_ __declspec(restrict)
#    define SDS_PRINTF_FUNC(fmt, args) _Check_return_
#  else
#    define SDS_MUT_FUNC
#    define SDS_ALLOC_FUNC
#    define SDS_PRINTF_FUNC(fmt, args)
#  endif
#  define SDS_CONST_FUNC
#endif

/* restrict keyword */
#ifndef s_restrict
#  if defined(restrict) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#    define s_restrict restrict
#  elif defined(__cplusplus) && (SDS_GCC_VER || SDS_CLANG_VER)
#    define s_restrict __restrict__
#  else
#    define s_restrict
#  endif
#endif

#if defined(__STDC__) && (!defined(__STDC_VERSION__) || \
        (defined(__STDC_VERSION__) && __STDC_VERSION__ < 199901L))
#  if (SDS_CLANG_VER || SDS_GCC_VER)
#    define s_inline __inline__
#  else
#    define s_inline
#  endif
#else
#  define s_inline inline
#endif

#define SDS_MAX_PREALLOC (1024*1024)
extern const char *SDS_NOINIT;

#ifndef sds_cxx
typedef char *sds;
#endif
/* We use sds as a macro here to document that the functions are for a const
 * sds string, but if we use 'const sds', instead of const char *, we have
 * char *const, which isn't very useful. */
#ifndef sds
#define sds char *
#endif

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

/* New in 3.0: Quick and easy way to check available space. */
#define SDS_SET_SCRATCH_SIZE(s, x) (s[-1] = (s[-1] & SDS_TYPE_MASK) | (unsigned char)((((ptrdiff_t)x < SDS_SCRATCH_MAX) ? (((ptrdiff_t)x >= 0) ? x : 0) : SDS_SCRATCH_MAX) << SDS_TYPE_BITS))
#define SDS_SCRATCH_SIZE(s) (s[-1] >> SDS_TYPE_BITS)
#define SDS_SCRATCH_MAX ((0xFF) >> 2)
#define SDS_HDR_VAR(T,s) struct sdshdr##T *sh = (struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T) + 1));
#define SDS_HDR(T,s) ((struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T) + 1)))

/* Low level functions exposed to the user API */
SDS_MUT_FUNC SDS_EXPORT sds sdsMakeRoomFor(sds s, size_t addlen);

/* Does not reallocate. It will abort on an unexpected size. */
SDS_EXPORT void sdsIncrLen(sds s, ptrdiff_t incr);
SDS_MUT_FUNC SDS_EXPORT sds sdsRemoveFreeSpace(sds s);
SDS_CONST_FUNC SDS_EXPORT size_t sdsAllocSize(sds s);
SDS_CONST_FUNC SDS_EXPORT void *sdsAllocPtr(sds s);
SDS_CONST_FUNC SDS_EXPORT size_t sdsAvailImpl(const sds s);

SDS_CONST_FUNC SDS_EXPORT size_t sdslen(const sds s);
SDS_CONST_FUNC SDS_EXPORT size_t sdsalloc(const sds s);

/* Available space on an sds string */
SDS_CONST_FUNC static inline size_t sdsavail(const sds s) {
    size_t avail = SDS_SCRATCH_SIZE(s);
    if (avail < SDS_SCRATCH_MAX && avail > 0)
        return avail;
	else
        return sdsAvailImpl(s);
}
SDS_MUT_FUNC SDS_EXPORT sds sdssetlen(sds s, size_t newlen);
SDS_MUT_FUNC SDS_EXPORT sds sdsinclen(sds s, size_t inc);
SDS_MUT_FUNC SDS_EXPORT sds sdssetalloc(sds s, size_t newlen);

/* Modify an sds string in-place to make it empty (zero length).
 * However all the existing buffer is not discarded but set as free space
 * so that next append operations will not require allocations up to the
 * number of bytes previously available. */
SDS_MUT_FUNC static s_inline sds sdsclear(sds s) {
    s = sdssetlen(s, 0);
    s[0] = '\0';
    return s;
}

/* The sds version of strcmp */
SDS_CONST_FUNC SDS_EXPORT int sdscmp(const sds s1, const sds s2);

SDS_ALLOC_FUNC SDS_EXPORT sds sdsnewlen(const void *const s_restrict init SDS_BOUNDED,
        size_t initlen);

SDS_ALLOC_FUNC static s_inline sds sdsnew(const char *const s_restrict init) {
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return sdsnewlen(init, initlen);
}

/* Create an empty (zero length) sds string. Even in this case the string
 * always has an implicit null term. */
SDS_ALLOC_FUNC static s_inline sds sdsempty(void) {
    return sdsnewlen("",0);
}

/* Duplicate an sds string. */
SDS_ALLOC_FUNC static s_inline sds sdsdup(const sds s_restrict s) {
    if (s == NULL)
        return sdsnewlen("",0);
    return sdsnewlen(s, sdslen(s));
}

SDS_EXPORT void sdsfree(sds s);

SDS_MUT_FUNC SDS_EXPORT sds sdsgrowzero(sds s_restrict s, size_t len);
SDS_MUT_FUNC SDS_EXPORT sds sdscatlen(sds s_restrict s,
        const void *const s_restrict t SDS_BOUNDED, size_t len);

/* Append the specified null termianted C string to the sds string 's'.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
SDS_MUT_FUNC static s_inline sds sdscat(sds s_restrict s, const char *const s_restrict t) {
    const size_t len = (t == NULL) ? 0 : strlen(t);
    return (len > 0) ? sdscatlen(s, t, len) : s;
}

/* Append the specified sds 't' to the existing sds 's'.
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
SDS_MUT_FUNC static s_inline sds sdscatsds(sds s_restrict s,
        const sds s_restrict t) {
    const size_t len = (t == NULL) ? 0 : sdslen(t);
    return (len > 0) ? sdscatlen(s, t, len) : s;
}

SDS_MUT_FUNC SDS_EXPORT sds sdscpylen(sds s_restrict s,
        const char *const s_restrict t SDS_BOUNDED, size_t len);

/* Like sdscpylen() but 't' must be a null-termined string so that the length
 * of the string is obtained with strlen(). */
SDS_MUT_FUNC static s_inline sds sdscpy(sds s_restrict s, const char *const s_restrict t) {
    const size_t len = (t == NULL) ? 0 : strlen(t);
    return (len > 0) ? sdscpylen(s, t, len) : sdsclear(s);
}

SDS_PRINTF_FUNC(2,0) SDS_EXPORT sds sdscatvprintf(sds s_restrict s,
        const char *const s_restrict fmt, va_list ap);
SDS_PRINTF_FUNC(2,3) SDS_EXPORT sds sdscatprintf(sds s_restrict s,
        const char *const s_restrict fmt, ...);

SDS_MUT_FUNC SDS_EXPORT sds sdscatfmt(sds s_restrict s,
        const char *const s_restrict fmt, ...);

SDS_MUT_FUNC SDS_EXPORT sds sdstrim(sds s_restrict s,
        const char *s_restrict cset);
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
SDS_ALLOC_FUNC SDS_EXPORT sds *sdssplitlen(const char *const s_restrict s SDS_BOUNDED,
        ptrdiff_t len, const char *const s_restrict sep SDS_BOUNDED, int seplen,
        int *const s_restrict count);

/* Like sdssplitlen, but uses strlen for len and seplen. */
SDS_ALLOC_FUNC static s_inline sds *sdssplit(const char *const s_restrict s,
        const char *const s_restrict sep, int *const s_restrict count) {
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
SDS_MUT_FUNC SDS_EXPORT sds sdsaddhexuint(sds s, unsigned int value);
SDS_MUT_FUNC SDS_EXPORT sds sdsaddhexulonglong(sds s, unsigned long long value);

SDS_ALLOC_FUNC SDS_EXPORT sds sdsfromint(int value);
SDS_ALLOC_FUNC SDS_EXPORT sds sdsfromuint(unsigned int value);
SDS_ALLOC_FUNC SDS_EXPORT sds sdsfromlonglong(long long value);
SDS_ALLOC_FUNC SDS_EXPORT sds sdsfromulonglong(unsigned long long value);
SDS_ALLOC_FUNC SDS_EXPORT sds sdsfromhexuint(unsigned int value);
SDS_ALLOC_FUNC SDS_EXPORT sds sdsfromhexulonglong(unsigned long long value);

/* because sdsaddhexuint and sdshexfromulonglong are mouthfuls */
#define sdsaddhex(s, x) ((sizeof((x)) > sizeof(unsigned)) ? sdsaddhexulonglong((s), (unsigned long long)(x)) : sdsaddhexuint((s), (unsigned)(x)))
#define sdsfromhex(x) ((sizeof((x)) > sizeof(unsigned)) ? sdsfromhexulonglong((unsigned long long)(x)) : sdsfromhexuint((unsigned)(x)))

SDS_MUT_FUNC SDS_EXPORT sds sdscatrepr(sds s_restrict s,
        const char *s_restrict p, size_t len);
SDS_ALLOC_FUNC SDS_EXPORT sds *sdssplitargs(const char *s_restrict line,
        int *s_restrict argc);
SDS_MUT_FUNC SDS_EXPORT sds sdsmapchars(sds s_restrict s,
        const char *s_restrict from, const char *s_restrict to, size_t setlen);
SDS_ALLOC_FUNC SDS_EXPORT sds sdsjoin(const char **s_restrict argv,
        int argc, const char *s_restrict sep);
SDS_ALLOC_FUNC SDS_EXPORT sds sdsjoinsds(sds *s_restrict argv, int argc,
        const char *s_restrict sep, size_t seplen);

/* Export the allocator used by SDS to the program using SDS.
 * Sometimes the program SDS is linked to, may use a different set of
 * allocators, but may want to allocate or free things that SDS will
 * respectively free or allocate. */
SDS_ALLOC_FUNC SDS_EXPORT void *sds_malloc(size_t size);
SDS_ALLOC_FUNC SDS_EXPORT void *sds_realloc(void *ptr, size_t size);
SDS_EXPORT void sds_free(void *ptr);

#ifdef __cplusplus
}
#endif

/* basic std::string wrappers */
#ifdef __cplusplus
#include <string>
SDS_MUT_FUNC static inline sds sdsaddstdstr(sds s, const std::string &x) {
    return sdscatlen(s, x.c_str(), x.length());
}
static inline std::string sds2stdstr(const sds s) {
    return std::string(s, sdslen(s));
}
SDS_ALLOC_FUNC static inline sds sdsfromstdstr(const std::string &x) {
    return sdsnewlen(x.c_str(), x.length());
}
#endif

/* A hacky macro that detects most character literals. Wrap in parentheses to
 * disable. What this does is stringify x and check if it starts or ends in a
 * single quote.
 *
 * This lets us detect character literals even after promotion, which even the
 * most complicated C++ template can't detect. */
#define SDS_IS_CHAR(x) ((#x[0] == '\'') || \
        (sizeof(#x) > 3 && #x[sizeof(#x)-2] == '\''))

#ifndef SDSADD_TYPE
#  if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
        || (__has_feature(c_generic_selections))
#    define SDSADD_TYPE 1 /* _Generic */
#  elif defined(__cplusplus) && __cplusplus >= 201103L
#    define SDSADD_TYPE 2 /* C++ overload/type_traits */
   /* GCC 3.2+ and all Clang versions support type 3 in C mode. */
#  elif !defined(__cplusplus) && (SDS_CLANG_VER || SDS_GCC_VER >= 302)
#    define SDSADD_TYPE 3
#  else
#    define SDSADD_TYPE 0 /* not supported */
#  endif
#endif /* SDSADD_TYPE */

/* Code for the special sdsadd macro. */
#if SDSADD_TYPE == 1 /* _Generic */
#define sdsadd(s, x)                                                        \
    _Generic((x),                                                           \
        char: sdsaddchar,                                                   \
        char *: sdscat, const char *: sdscat,                               \
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
     * deduct its size, it can pass it as a template parameter, avoiding a
     * strlen call. However, it is impossible to have priority over
     * const char * without !std::is_array, because for some reason,
     * const char * outprioritizes const char[4]. */
    template <size_t N>
    SDS_MUT_FUNC static inline sds (sdsadd)(sds s, const char (&x)[N]) {
        return sdscatlen(s, x, N - 1);
    }
    /* C string, with an unknown compile-time size. */
    template <typename T,
              typename std::enable_if<
                  std::is_convertible<base_type<T>, const char *>::value &&
                      !std::is_array<T>::value,
                  int>::type = 0>
    SDS_MUT_FUNC static inline sds (sdsadd)(sds s, const T x) {
        return sdscat(s, x);
    }
    /* Catches all integers. */
    template <typename T,
              typename std::enable_if<std::is_integral<base_type<T>>::value,
                                      int>::type = 0>
    SDS_MUT_FUNC static inline sds (sdsadd)(sds s, const T x) {
        /* Unsigned */
        if (std::is_unsigned<base_type<T>>::value) {
            /* GCC 4.9 doesn't get the point and does sign warnings here */
            if (static_cast<typename std::make_unsigned<T>::type>(x) <= UINT_MAX)
                return sdsadduint(s, static_cast<unsigned>(x));
            else
                return sdsaddulonglong(s, x);
        /* Signed */
        } else {
            if (static_cast<typename std::make_signed<T>::type>(x) <= INT_MAX
             && static_cast<typename std::make_signed<T>::type>(x) >= INT_MIN)
                return sdsaddint(s, static_cast<int>(x));
            else
                return sdsaddlonglong(s, x);
        }
    }
    /* char. This has overload priority over the generic one. */
    SDS_MUT_FUNC static inline sds (sdsadd)(sds s, const char x) {
        return sdsaddchar(s, x);
    }
    /* std::string */
    SDS_MUT_FUNC static inline sds (sdsadd)(sds s, const std::string &x) {
        return sdsaddstdstr(s, x);
    }
};

/* If SDS_IS_CHAR(x) expands to true in the macro below, it will specialize
 * _sdsadd_t_ to use this overload. */
template <> struct _sdsadd_t_<true> {
    SDS_MUT_FUNC static inline sds (sdsadd)(sds s, const unsigned x) {
        return sdsaddchar(s, x);
    }
};

#define sdsadd(s, x) _sdsadd_t_<SDS_IS_CHAR(x)>::sdsadd((s), (x))

#elif SDSADD_TYPE == 3 /* GCC extensions */

/* Generate a compile error. */
extern int sdsadd_bad_argument(sds, void *);

/* To make things a little nicer. We still need the mess of parentheses though. */
#define SDS_TYPE_CMP(x,y, true_, false_)                                    \
    __builtin_choose_expr(                                                  \
        __builtin_types_compatible_p(__typeof__(x), y), true_, false_       \
    )
/* clang-format off */
#define SDSADD_CHECK(s, x)                                                  \
        (SDS_TYPE_CMP((x), char, sdsaddchar,                                \
         SDS_TYPE_CMP((x), const char[], sdscat,                            \
         SDS_TYPE_CMP((x), char[], sdscat,                                  \
         SDS_TYPE_CMP((x), const char *, sdscat,                            \
         SDS_TYPE_CMP((x), char *, sdscat,                                  \
         SDS_TYPE_CMP((x), int,                                             \
             SDS_IS_CHAR(x) ? (sds (*)(sds,int))sdsaddchar : sdsaddint,     \
         SDS_TYPE_CMP((x), unsigned,                                        \
             SDS_IS_CHAR(x) ? sdsaddchar : sdsadduint,                      \
         SDS_TYPE_CMP((x), long long, sdsaddlonglong,                       \
         SDS_TYPE_CMP((x), unsigned long long, sdsaddulonglong,             \
        sdsadd_bad_argument /* to mess up the assertion below */            \
    )))))))))((s), (x)))
/* clang-format on */
#if __STDC_VERSION__ >= 201112L \
       || __has_feature(c_static_assert) \
       || (SDS_GCC_VER >= 406)
#  define sdsadd(s, x) __extension__({ \
    enum { sdsadd = __builtin_types_compatible_p(__typeof__(SDSADD_CHECK(s,x)), sds) }; \
    __extension__ _Static_assert(sdsadd, "unsupported type for argument 2"); \
    SDSADD_CHECK(s,x); \
})
#else
#define sdsadd(s, x) __extension__({                                        \
    /* If an invalid option is used above, _s2 will be int and              \
     * sdsadd_invalid_type would expand to a negative array. */             \
    enum { sdsadd = __builtin_types_compatible_p(__typeof__(SDSADD_CHECK(s,x)), sds) }; \
    __attribute__((unused)) \
    extern char sdsadd_assert_unsupported_type_for_argument_2[1 - (2 * !sdsadd)];                                                                  \
    SDSADD_CHECK(s, x);                                                     \
})
#endif
#else /* Not supported */
#define sdsadd(s, x) extern char sdsadd_not_supported_in_this_compiler[-1];
#endif /* sdsadd macros */

#undef sds

#endif /* SDS_H */
