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

/* Some diagnostics flags */
#define _SDS_PRAGMA(x) _Pragma(#x)
#define _SDS_IGNORE(x) _SDS_DIAG(ignored x)
#define _SDS_ERROR(x) _SDS_DIAG(error x)

#if defined(__clang__)
#  define _SDS_DIAG(x) _SDS_PRAGMA(clang diagnostic x)
#elif defined(__GNUC__)
#  define _SDS_DIAG(x) _SDS_PRAGMA(GCC diagnostic x)
#else
#  define _SDS_DIAG(x)
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
   /* Clang 3.0 (or earlier?) and GCC 4.5.0 added __builtin_unreachable */
#  if (defined(__clang__) && __clang_version_major__ >= 3) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)))
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

#ifndef SDS_32_BIT
#  if UINT32_MAX == UINTPTR_MAX
#    define SDS_32_BIT
#  endif
#endif

#ifdef SDS_32_BIT
#  define SDS_64_BIT_ONLY(...)
#  define SDS_TYPE_MASK 2
#else
#  define SDS_64_BIT_ONLY(...) __VA_ARGS__
#  define SDS_TYPE_MASK 3
#endif

enum sdshdrtype {
    SDS_TYPE_8,
    SDS_TYPE_16,
    SDS_TYPE_32,
    SDS_TYPE_64
};

#define SDS_TYPE_BITS 2
#define SDS_HDR_VAR(T,s) struct sdshdr##T *sh = (struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T) + 1));
#define SDS_HDR(T,s) ((struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T) + 1)))


/* Low level functions exposed to the user API */
SDS_MUT_FUNC sds sdsMakeRoomFor(sds s, size_t addlen);
enum sdsstatus {
    SDS_STATUS_NOT_CHANGED,
    SDS_STATUS_CHANGED,
    SDS_STATUS_CHANGED_TYPE
};
SDS_MUT_FUNC sds sdsMakeRoomForStatus(sds s_restrict s, size_t addlen, enum sdsstatus *s_restrict status /* out */);
/* Does not reallocate. It will abort on an unexpected size. */
void sdsIncrLen(sds s, ptrdiff_t incr);
SDS_MUT_FUNC sds sdsRemoveFreeSpace(sds s);
SDS_CONST_FUNC size_t sdsAllocSize(sds s);
SDS_CONST_FUNC void *sdsAllocPtr(sds s);

SDS_CONST_FUNC size_t sdslen(const sds s);
SDS_CONST_FUNC size_t sdsalloc(const sds s);
SDS_CONST_FUNC size_t sdsavail(const sds s);
SDS_MUT_FUNC sds sdssetlen(sds s, size_t newlen);
SDS_MUT_FUNC sds sdsinclen(sds s, size_t inc);
SDS_MUT_FUNC sds sdssetalloc(sds s, size_t newlen);

/* The sds version of strcmp */
SDS_CONST_FUNC int sdscmp(const sds s1, const sds s2);

SDS_INIT_FUNC sds sdsnewlen(const void *s_restrict init, size_t initlen);

/* Create a new sds string starting from a null terminated C string. */
SDS_INIT_FUNC static inline sds sdsnew(const char *s_restrict init) {
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return sdsnewlen(init, initlen);
}

/* Create an empty (zero length) sds string. Even in this case the string
 * always has an implicit null term. */
SDS_INIT_FUNC static inline sds sdsempty(void) {
    return sdsnewlen("",0);
}

/* Duplicate an sds string. */
SDS_INIT_FUNC static inline sds sdsdup(const sds s_restrict s) {
    if (s == NULL)
        return sdsnewlen("",0);
    return sdsnewlen(s, sdslen(s));
}

void sdsfree(sds s);

SDS_MUT_FUNC sds sdsgrowzero(sds s_restrict s, size_t len);
SDS_MUT_FUNC sds sdscatlen(sds s_restrict s, const void *s_restrict t, size_t len);

/* Append the specified null termianted C string to the sds string 's'.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
SDS_MUT_FUNC static inline sds sdscat(sds s_restrict s, const char * s_restrict t) {
    return sdscatlen(s, t, strlen(t));
}

/* Append the specified sds 't' to the existing sds 's'.
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
SDS_MUT_FUNC static inline sds sdscatsds(sds s_restrict s, const sds s_restrict t) {
    return sdscatlen(s, t, sdslen(t));
}

SDS_MUT_FUNC sds sdscpylen(sds s_restrict s, const char *s_restrict t, size_t len);

/* Like sdscpylen() but 't' must be a null-termined string so that the length
 * of the string is obtained with strlen(). */
SDS_MUT_FUNC static inline sds sdscpy(sds s_restrict s, const char *s_restrict t) {
    return sdscpylen(s, t, strlen(t));
}

SDS_PRINTF_FUNC(2,0) sds sdscatvprintf(sds s_restrict s, SDS_FMT_STR const char *s_restrict fmt,
                                       va_list ap);
SDS_PRINTF_FUNC(2,3) sds sdscatprintf(sds s_restrict s, SDS_FMT_STR const char *s_restrict fmt, ...);

SDS_MUT_FUNC sds sdscatfmt(sds s_restrict s, const char *s_restrict fmt, ...);

SDS_MUT_FUNC sds sdstrim(sds s_restrict s, const char *s_restrict cset);
SDS_MUT_FUNC sds sdsrange(sds s, ptrdiff_t start, ptrdiff_t end);

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
SDS_MUT_FUNC static inline sds sdsupdatelen(sds s) {
    size_t reallen = strlen(s);
    return sdssetlen(s, reallen);
}
/* Modify an sds string in-place to make it empty (zero length).
 * However all the existing buffer is not discarded but set as free space
 * so that next append operations will not require allocations up to the
 * number of bytes previously available. */
SDS_MUT_FUNC static inline sds sdsclear(sds s) {
    s = sdssetlen(s, 0);
    s[0] = '\0';
    return s;
}

SDS_INIT_FUNC sds *sdssplitlen(const char *s_restrict s, ptrdiff_t len, const char *s_restrict sep, int seplen, int *count);

/* Like sdssplitlen, but uses strlen for len and seplen. */
SDS_INIT_FUNC static inline sds *sdssplit(const char *s_restrict s, const char *s_restrict sep, int *count) {
    return sdssplitlen(s, strlen(s), sep, strlen(sep), count);
}
void sdsfreesplitres(sds *tokens, int count);
SDS_MUT_FUNC sds sdstolower(sds s);
SDS_MUT_FUNC sds sdstoupper(sds s);

/* Appends a single character to an sds string. */
SDS_MUT_FUNC sds sdsaddchar(sds s, unsigned int c);
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

/* Generate a linker argument */
extern sds sdsadd_bad_argument(sds);

/* To make things a little nicer. We still need the mess of parentheses though. */
#define _SDS_TYPE_CMP(x,y, true_, false_)                                   \
    __builtin_choose_expr(                                                  \
        __builtin_types_compatible_p(__typeof__(x), y), true_, false_       \
    )
/* Both GCC and Clang love to complain about this macro, because they type
 * check disabled expressions. We have to stop it. */

#define _SDSADD_WARNINGS_OFF _SDS_DIAG(push) _SDS_IGNORE("-Wunknown-pragmas")\
    _SDS_ERROR("-Wincompatible-pointer-types") /* we want an error here */  \
    _SDS_IGNORE("-Wint-to-pointer-cast") _SDS_IGNORE("-Wunused-variable")   \
    _SDS_IGNORE("-Wpointer-to-int-cast") _SDS_IGNORE("-Wint-conversion")

#define _SDSADD_WARNINGS_ON _SDS_DIAG(pop)
#define _SDSADD_CHECK(s, x)                                                 \
            (_SDS_TYPE_CMP((x), char, sdsaddchar((s), (unsigned)(x)),        \
            _SDS_TYPE_CMP((x), const char[],                                \
                sdscatlen((s), (const char *)(x), sizeof(x)-1),             \
            _SDS_TYPE_CMP((x), char[],                                      \
                sdscatlen((s), (const char *)(x), sizeof(x)-1),             \
            _SDS_TYPE_CMP((x), const char *, sdscat((s), (const char *)(x)),\
            _SDS_TYPE_CMP((x), char *, sdscat((s), (const char *)(x)),      \
            _SDS_TYPE_CMP((x), int,                                         \
                 SDS_IS_CHAR(x) ? sdsaddchar((s), (unsigned)(x))            \
                                : sdsaddint((s), (int)(x)),                 \
            _SDS_TYPE_CMP((x), unsigned,                                    \
                 SDS_IS_CHAR(x) ? sdsaddchar((s), (unsigned)(x))            \
                                : sdsadduint((s), (unsigned)(x)),           \
            _SDS_TYPE_CMP((x), long long,                                   \
                sdsaddlonglong((s), (long long)(x)),                        \
            _SDS_TYPE_CMP((x), unsigned long long,                          \
                sdsaddulonglong((s), (unsigned long long)(x)),              \
            (int)1 /* to mess up the assertion below */                     \
        ))))))))))
#define sdsadd(s, x) __extension__({                                        \
    _SDSADD_WARNINGS_OFF                                                    \
        /* If an invalid option is used above, _s2 will be int and          \
         * sdsadd_invalid_type would expand to a negative array. */         \
    __attribute__((__unused__)) extern char sdsadd_invalid_type[                                    \
            (2 * !!__builtin_types_compatible_p(                            \
                __typeof__(_SDSADD_CHECK(s,x)), sds)) - 1  \
        ];                                                                  \
    _SDSADD_CHECK(s, x);                                                                     \
    _SDSADD_WARNINGS_ON                                                     \
})
#else /* Not supported */
#define sdsadd(s, x) do { char sdsadd_not_supported_in_this_compiler[-1]; } while (0)
#endif /* sdsadd macros */
#endif
