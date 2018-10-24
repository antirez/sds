
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
#include "sds.h"
#include "sdsalloc.h"

const char *SDS_NOINIT = "SDS_NOINIT";

/* Note: We add 1 because of the flags byte. */
static inline int sdsHdrSize(char type) {
    SDS_HDR_LAMBDA_2(NULL, type & SDS_TYPE_MASK, {
        return sizeof(sdshdr) + 1;
    });
    SDS_UNREACHABLE(0);
}

static inline char sdsReqType(size_t string_size) {
    if (string_size < 1<<8)
        return SDS_TYPE_8;
    if (string_size < 1<<16)
        return SDS_TYPE_16;
#ifndef SDS_32_BIT
    if (string_size < 1ll<<32)
        return SDS_TYPE_32;
    return SDS_TYPE_64;
#else
    return SDS_TYPE_32;
#endif
}

/* Create a new sds string with the content specified by the 'init' pointer
 * and 'initlen'.
 * If NULL is used for 'init' the string is initialized with zero bytes.
 * If SDS_NOINIT is used, the buffer is left uninitialized;
 *
 * The string is always null-termined (all the sds strings are, always) so
 * even if you create an sds string with:
 *
 * mystring = sdsnewlen("abc",3);
 *
 * You can print the string with printf() as there is an implicit \0 at the
 * end of the string. However the string is binary safe and can contain
 * \0 characters in the middle, as the length is stored in the sds header. */
SDS_INIT_FUNC sds sdsnewlen(const void *s_restrict init, size_t initlen) {
    void *sh;
    sds s;
    char type = sdsReqType(initlen);

    int hdrlen = sdsHdrSize(type);
    unsigned char *fp; /* flags pointer. */

    sh = s_malloc(hdrlen+initlen+1);
    if (sh == NULL) SDS_ERR_RETURN(NULL);
    if (init==SDS_NOINIT)
        init = NULL;
    else if (!init)
        memset(sh, 0, hdrlen+initlen+1);
    s = (char*)sh+hdrlen;
    fp = ((unsigned char*)s)-1;

    /* Set the new length. */
    SDS_HDR_LAMBDA_2(s, type, {
        sh->len = initlen;
        sh->alloc = initlen;
        *fp = type;
    });

    if (initlen && init)
        memcpy(s, init, initlen);
    s[initlen] = '\0';
    return s;
}

/* Create an empty (zero length) sds string. Even in this case the string
 * always has an implicit null term. */
SDS_INIT_FUNC sds sdsempty(void) {
    return sdsnewlen("",0);
}

/* Create a new sds string starting from a null terminated C string. */
SDS_INIT_FUNC sds sdsnew(const char *s_restrict init) {
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return sdsnewlen(init, initlen);
}

/* Duplicate an sds string. */
SDS_INIT_FUNC sds sdsdup(const sds s_restrict s) {
    if (SDS_UNLIKELY(s == NULL))
        return sdsnewlen("",0);
    return sdsnewlen(s, sdslen(s));
}

/* Free an sds string. No operation is performed if 's' is NULL. */
void sdsfree(sds s) {
    if (SDS_UNLIKELY(s == NULL)) return;
    s_free((char*)s-sdsHdrSize(s[-1]));
}

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
SDS_MUT_FUNC sds sdsupdatelen(sds s) {
    size_t reallen = strlen(s);
    return sdssetlen(s, reallen);
}

/* Modify an sds string in-place to make it empty (zero length).
 * However all the existing buffer is not discarded but set as free space
 * so that next append operations will not require allocations up to the
 * number of bytes previously available. */
SDS_MUT_FUNC sds sdsclear(sds s) {
    s = sdssetlen(s, 0);
    s[0] = '\0';
    return s;
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
    int hdrlen;

    SDS_HDR_LAMBDA_2(s, oldtype, {
        len = sh->len;

        /* Return ASAP if there is enough space left. */
        if (SDS_LIKELY(sh->alloc - len >= addlen)) return s;
        shptr = (char *)sh;
    });

    newlen = (len+addlen);
    if (newlen < SDS_MAX_PREALLOC)
        newlen *= 2;
    else
        newlen += SDS_MAX_PREALLOC;

    type = sdsReqType(newlen);

    hdrlen = sdsHdrSize(type);
    if (oldtype==type) {
        newsh = (char *)s_realloc(shptr, hdrlen+newlen+1);
        if (SDS_UNLIKELY(newsh == NULL)) SDS_ERR_RETURN(NULL);
        s = newsh+hdrlen;
    } else {
        /* Since the header size changes, need to move the string forward,
         * and can't use realloc */
        newsh = (char *)s_malloc(hdrlen+newlen+1);
        if (SDS_UNLIKELY(newsh == NULL)) SDS_ERR_RETURN(NULL);
        memcpy(newsh+hdrlen, s, len+1);
        s_free(shptr);
        s = newsh+hdrlen;
        s[-1] = type;
    }
    /* Set the new length manually */
    SDS_HDR_LAMBDA(s, {
        sh->len = len;
        sh->alloc = newlen;
    });
    return s;
}

/* Reallocate the sds string so that it has no free space at the end. The
 * contained string remains not altered, but next concatenation operations
 * will require a reallocation.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
SDS_MUT_FUNC sds sdsRemoveFreeSpace(sds s) {
    char *sh, *newsh;
    char type, oldtype = s[-1] & SDS_TYPE_MASK;
    int hdrlen, oldhdrlen = sdsHdrSize(oldtype);
    size_t len = sdslen(s);
    sh = (char*)s-oldhdrlen;

    /* Check what would be the minimum SDS header that is just good enough to
     * fit this string. */
    type = sdsReqType(len);
    hdrlen = sdsHdrSize(type);

    /* If the type is the same, or at least a large enough type is still
     * required, we just realloc(), letting the allocator to do the copy
     * only if really needed. Otherwise if the change is huge, we manually
     * reallocate the string to use the different header type. */
    if (oldtype==type || type > SDS_TYPE_8) {
        newsh = (char *)s_realloc(sh, oldhdrlen+len+1);
        if (SDS_UNLIKELY(newsh == NULL)) SDS_ERR_RETURN(NULL);
        s = newsh+oldhdrlen;
    } else {
        newsh = (char *)s_malloc(hdrlen+len+1);
        if (SDS_UNLIKELY(newsh == NULL)) SDS_ERR_RETURN(NULL);
        memcpy(newsh+hdrlen, s, len+1);
        s_free(sh);
        s = newsh + hdrlen;
        s[-1] = type;
    }
    /* Set the new length manually */
    SDS_HDR_LAMBDA(s, {
        sh->alloc = len;
    });
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
    return sdsHdrSize(s[-1])+alloc+1;
}

/* Return the pointer of the actual SDS allocation (normally SDS strings
 * are referenced by the start of the string buffer). */
SDS_CONST_FUNC void *sdsAllocPtr(sds s) {
    return (void*) (s-sdsHdrSize(s[-1]));
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
    size_t len;
    SDS_HDR_LAMBDA(s, {
        assert((incr >= 0 && sh->alloc - sh->len >= (sdshdr_uint)incr) ||
               (incr < 0 && sh->len >= (sdshdr_uint)(-incr)));
        len = (sh->len += incr);
    });
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
    s = sdsMakeRoomFor(s,len-curlen);
    if (SDS_UNLIKELY(s == NULL)) SDS_ERR_RETURN(NULL);

    /* Make sure added region doesn't contain garbage */
    memset(s+curlen,0,(len-curlen+1)); /* also set trailing \0 byte */
    return sdssetlen(s, len);
}

/* Append the specified binary-safe string pointed by 't' of 'len' bytes to the
 * end of the specified sds string 's'.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
SDS_MUT_FUNC sds sdscatlen(sds s_restrict s, const void *s_restrict t, size_t len) {
    size_t curlen;

    s = sdsMakeRoomFor(s,len);
    if (SDS_UNLIKELY(s == NULL)) SDS_ERR_RETURN(NULL);
    SDS_HDR_LAMBDA(s, {
        curlen = sh->len;
        memcpy(s+curlen, t, len);
        sh->len += len;
    });
    s[curlen+len] = '\0';
    return s;
}

/* Append the specified null termianted C string to the sds string 's'.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
SDS_MUT_FUNC sds sdscat(sds s_restrict s, const char * s_restrict t) {
    return sdscatlen(s, t, strlen(t));
}

/* Append the specified sds 't' to the existing sds 's'.
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
SDS_MUT_FUNC sds sdscatsds(sds s_restrict s, const sds s_restrict t) {
    return sdscatlen(s, t, sdslen(t));
}

/* Destructively modify the sds string 's' to hold the specified binary
 * safe string pointed by 't' of length 'len' bytes. */
SDS_MUT_FUNC sds sdscpylen(sds s_restrict s, const char *s_restrict t, size_t len) {
    int i;
    for (i = 0; i < 2; ++i) {
        SDS_HDR_LAMBDA(s, {
            if (sh->alloc < len) {
                s = sdsMakeRoomFor(s,len-sh->len);
                if (SDS_UNLIKELY(s == NULL)) SDS_ERR_RETURN(NULL);
                continue;
            }
            memcpy(s, t, len);
            s[len] = '\0';
            sh->len = len;
            return s;
        });
    }
    SDS_UNREACHABLE(NULL);
}

/* Like sdscpylen() but 't' must be a null-termined string so that the length
 * of the string is obtained with strlen(). */
SDS_MUT_FUNC sds sdscpy(sds s_restrict s, const char *s_restrict t) {
    return sdscpylen(s, t, strlen(t));
}



#define SDS_LLSTR_SIZE 21
#define SDS_INTSTR_SIZE 12

/* 64-bit math, even with the exact same code, can be drastically
 * slower on 32-bit devices. For example, on a 32-bit ARMv7 LG G3,
 * using 64-bit math is ten times faster than 32-bit math with the
 * same numbers. */

/* First stage of the conversion. Converts an integer to a reversed
 * string. */
#define WRITE_REVERSE_STRING(/* int & */ v, /* char * */ p)         \
do {                                                                \
    *p++ = '0'+(v%10);                                              \
    v /= 10;                                                        \
} while(v);

/* Reverses a string and returns the length of it. End must be the
 * end of start. */
#define RETURN_REVERSED_STRING(/* char * */ start, /* char * */ end)\
do {                                                                \
    int l = end - start;                                            \
    char tmp;                                                       \
    *end-- = '\0';                                                  \
    while (start < end) {                                           \
        tmp = *start;                                               \
        *start = *end;                                              \
        *end = tmp;                                                 \
        start++;                                                    \
        end--;                                                      \
    }                                                               \
    return l;                                                       \
} while (0)

#define UINT2STR_BODY(s, v) do {                                    \
    char *p = s;                                                    \
    WRITE_REVERSE_STRING(v, p);                                     \
    RETURN_REVERSED_STRING(s, p);                                   \
} while (0)

#define INT2STR_BODY(unsigned_type, s, value) do {                  \
    char *p = s;                                                    \
    unsigned_type v = (value < 0) ? -value : value;                 \
    WRITE_REVERSE_STRING(v, p);                                     \
    if (value < 0) *p++ = '-';                                      \
    RETURN_REVERSED_STRING(s, p);                                   \
} while (0)

static int sdsuint2str(char *s, unsigned v) {
    UINT2STR_BODY(s, v);
}

static int sdsulonglong2str(char *s, unsigned long long v) {
    /* Use (potentially) faster 32-bit math if we can. */
    if (v <= UINT_MAX)
        return sdsuint2str(s, (unsigned)v);

    UINT2STR_BODY(s, v);
}

static int sdsint2str(char *s, int value) {
    INT2STR_BODY(unsigned, s, value);
}

static int sdslonglong2str(char *s, long long value) {
    /* Use (potentially) faster 32-bit math if we can. */
    if (value <= INT_MAX && value >= INT_MIN)
        return sdsint2str(s, (int)value);

    INT2STR_BODY(unsigned long long, s, value);
}

/* Creates functions named sdsadd<T> and sdsfrom<T> that appends a <type>
 * int value to an existing sds string, checking if <minsize> is available,
 * then calling sds<T>2str to make the conversion. */
#define SDS_INT_ADD_FUNC(T, type, minsize)                      \
sds (sdsadd##T)(sds s, type value) {                            \
    int len, i;                                                 \
    for (i = 0; i < 2; i++) {                                   \
        SDS_HDR_LAMBDA(s, {                                     \
            if (sh->alloc - sh->len < (minsize)) {              \
                s = sdsMakeRoomFor(s, (minsize));               \
                continue;                                       \
            }                                                   \
            len = sds##T##2str(&s[sh->len], value);             \
                                                                \
            sh->len = len;                                      \
            return s;                                           \
        });                                                     \
    }                                                           \
    SDS_UNREACHABLE(NULL);                                      \
}                                                               \
sds (sdsfrom##T)(type value) {                                  \
    return (sdsadd##T)(sdsempty(), value);                      \
}

/* Add the functions */
SDS_INT_ADD_FUNC(int, int, SDS_INTSTR_SIZE)
SDS_INT_ADD_FUNC(uint, unsigned, SDS_INTSTR_SIZE)
SDS_INT_ADD_FUNC(longlong, long long, SDS_LLSTR_SIZE)
SDS_INT_ADD_FUNC(ulonglong, unsigned long long, SDS_LLSTR_SIZE)

#undef SDS_INT_ADD_FUNC


/* Like sdscatprintf() but gets va_list instead of being variadic. */
SDS_PRINTF_FUNC(2,0) sds sdscatvprintf(sds s_restrict s, SDS_FMT_STR const char *s_restrict fmt,
                                       va_list ap) {
    va_list cpy;
    char staticbuf[1024], *buf = staticbuf, *t;
    int buflen = strlen(fmt)*2;
    int len;
    int malloced = 0;
    int i;

    /* We try to start using a static buffer for speed.
     * If not possible we revert to stack or heap
     * allocation. */
    if (buflen > (int)sizeof(staticbuf)) {
        /* Alloca is faster, but we don't want to alloca
         * more than 32 kB, because we might overflow
         * the stack. */
        if (SDS_LIKELY(buflen <= 32 * 1024)) {
            buf = (char *)alloca(buflen);
        } else {
            malloced = 1;
            buf = (char *)s_malloc(buflen);
        }
        if (SDS_UNLIKELY(buf == NULL)) SDS_ERR_RETURN(NULL);
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
        if (SDS_UNLIKELY(len < 0)) {
            if (SDS_UNLIKELY(malloced))
                s_free(buf);
            SDS_ERR_RETURN(NULL);
        } else if (len >= buflen) {
            buflen = len + 1;

            if (SDS_LIKELY(buflen <= 32 * 1024)) {
                buf = (char *)alloca(buflen);
            } else {
                if (SDS_UNLIKELY(malloced)) free(buf);
                malloced = 1;
                buf = (char *)s_malloc(buflen);
            }
            if (SDS_UNLIKELY(buf == NULL)) SDS_ERR_RETURN(NULL);

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
 * s = sdscatprintf(s,"%d+%d = %d",a,b,a+b).
 *
 * Often you need to create a string from scratch with the printf-alike
 * format. When this is the need, just use sdsempty() as the target string:
 *
 * s = sdscatprintf(sdsempty(), "... your format ...", args);
 */
SDS_PRINTF_FUNC(2,3) sds sdscatprintf(sds s_restrict s,
                                      SDS_FMT_STR const char *s_restrict fmt,
                                      ...) {
    va_list ap;
    char *t;

    va_start(ap, fmt);
    t = sdscatvprintf(s,fmt,ap);
    va_end(ap);
    return t;
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
 * %i - signed int
 * %I - 64 bit signed integer (long long, int64_t)
 * %u - unsigned int
 * %U - 64 bit unsigned integer (unsigned long long, uint64_t)
 * %% - Verbatim "%" character.
 */
SDS_MUT_FUNC sds sdscatfmt(sds s_restrict s, const char *s_restrict fmt, ...) {
    size_t initlen = sdslen(s);
    const char *f = fmt;
    long i;
    va_list ap;

    va_start(ap,fmt);
    f = fmt;    /* Next format specifier byte to process. */
    i = initlen; /* Position of the next byte to write to dest str. */
    while(*f) {
        char next, *str;
        size_t l;
        long long num;
        unsigned long long unum;

        /* Make sure there is always space for at least 1 char. */
        if (sdsavail(s)==0) {
            s = sdsMakeRoomFor(s,1);
        }

        switch(*f) {
        case '%':
            next = *(f+1);
            f++;
            switch(next) {
            case 's':
            case 'S':
                str = va_arg(ap,char*);
                l = (next == 's') ? strlen(str) : sdslen(str);
                if (sdsavail(s) < l) {
                    s = sdsMakeRoomFor(s,l);
                }
                memcpy(s+i,str,l);
                sdsIncrLen(s,l);
                i += l;
                break;
            case 'i':
            case 'I':
                if (next == 'i')
                    num = va_arg(ap,int);
                else
                    num = va_arg(ap,long long);
                {
                    char buf[SDS_LLSTR_SIZE];
                    l = sdslonglong2str(buf,num);
                    if (sdsavail(s) < l) {
                        s = sdsMakeRoomFor(s,l);
                    }
                    memcpy(s+i,buf,l);
                    sdsIncrLen(s,l);
                    i += l;
                }
                break;
            case 'u':
            case 'U':
                if (next == 'u')
                    unum = va_arg(ap,unsigned int);
                else
                    unum = va_arg(ap,unsigned long long);
                {
                    char buf[SDS_LLSTR_SIZE];
                    l = sdsulonglong2str(buf,unum);
                    if (sdsavail(s) < l) {
                        s = sdsMakeRoomFor(s,l);
                    }
                    memcpy(s+i,buf,l);
                    sdsIncrLen(s,l);
                    i += l;
                }
                break;
            default: /* Handle %% and generally %<unknown>. */
                s[i++] = next;
                sdsIncrLen(s,1);
                break;
            }
            break;
        default:
            s[i++] = *f;
            sdsIncrLen(s,1);
            break;
        }
        f++;
    }
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
 * s = sdstrim(s,"Aa. :");
 * printf("%s\n", s);
 *
 * Output will be just "Hello World".
 */
SDS_MUT_FUNC sds sdstrim(sds s_restrict s, const char *s_restrict cset) {
    char *start, *end, *sp, *ep;
    size_t len;

    sp = start = s;
    ep = end = s+sdslen(s)-1;
    while(sp <= end && strchr(cset, *sp)) sp++;
    while(ep > sp && strchr(cset, *ep)) ep--;
    len = (sp > ep) ? 0 : ((ep-sp)+1);
    if (s != sp) memmove(s, sp, len);
    s[len] = '\0';
    return sdssetlen(s,len);
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
 * sdsrange(s,1,-1); => "ello World"
 */
SDS_MUT_FUNC sds sdsrange(sds s, ptrdiff_t start, ptrdiff_t end) {
    size_t newlen, len = sdslen(s);

    if (len == 0) return s;
    if (start < 0) {
        start = len+start;
        if (start < 0) start = 0;
    }
    if (end < 0) {
        end = len+end;
        if (end < 0) end = 0;
    }
    newlen = (start > end) ? 0 : (end-start)+1;
    if (newlen != 0) {
        if (start >= (ptrdiff_t)len) {
            newlen = 0;
        } else if (end >= (ptrdiff_t)len) {
            end = len-1;
            newlen = (start > end) ? 0 : (end-start)+1;
        }
    } else {
        start = 0;
    }
    if (start && newlen) memmove(s, s+start, newlen);
    s[newlen] = '\0';
    return sdssetlen(s,newlen);
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
    cmp = memcmp(s1,s2,minlen);
    if (cmp == 0) return l1>l2? 1: (l1<l2? -1: 0);
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
 * sdssplit("foo_-_bar","_-_"); will return two
 * elements "foo" and "bar".
 *
 * This version of the function is binary-safe but
 * requires length arguments. sdssplit() is just the
 * same function but for zero-terminated strings.
 */
SDS_INIT_FUNC sds *sdssplitlen(const char *s_restrict s, ptrdiff_t len, const char *s_restrict sep, int seplen, int *s_restrict count) {
    int elements = 0, slots = 5;
    long start = 0, j;
    sds *tokens;

    if (SDS_UNLIKELY(seplen < 1 || len < 0)) SDS_ERR_RETURN(NULL);

    tokens = (sds *)s_malloc(sizeof(sds)*slots);
    if (SDS_UNLIKELY(tokens == NULL)) SDS_ERR_RETURN(NULL);

    if (len == 0) {
        *count = 0;
        return tokens;
    }
    for (j = 0; j < (len-(seplen-1)); j++) {
        /* make sure there is room for the next element and the final one */
        if (slots < elements+2) {
            sds *newtokens;

            slots *= 2;
            newtokens = (sds *)s_realloc(tokens,sizeof(sds)*slots);
            if (SDS_UNLIKELY(newtokens == NULL)) goto err;
            tokens = newtokens;
        }
        /* search the separator */
        if ((seplen == 1 && *(s+j) == sep[0]) || (memcmp(s+j,sep,seplen) == 0)) {
            tokens[elements] = sdsnewlen(s+start,j-start);
            if (SDS_UNLIKELY(tokens[elements] == NULL)) goto err;
            elements++;
            start = j+seplen;
            j = j+seplen-1; /* skip the separator */
        }
    }
    /* Add the final element. We are sure there is room in the tokens array. */
    tokens[elements] = sdsnewlen(s+start,len-start);
    if (SDS_UNLIKELY(tokens[elements] == NULL)) goto err;
    elements++;
    *count = elements;
    return tokens;

err:
    {
        int i;
        for (i = 0; i < elements; i++) sdsfree(tokens[i]);
        s_free(tokens);
        *count = 0;
        SDS_ERR_RETURN(NULL);
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
SDS_MUT_FUNC sds sdscatrepr(sds s_restrict s, const char *s_restrict p, size_t len) {
    s = sdsaddchar(s,'"');
    while(len--) {
        switch(*p) {
        case '\\':
        case '"':
            s = sdsaddchar(sdsaddchar(s,'\\'), *p);
            break;
        case '\n': s = sdscatlen(s,"\\n",2); break;
        case '\r': s = sdscatlen(s,"\\r",2); break;
        case '\t': s = sdscatlen(s,"\\t",2); break;
        case '\a': s = sdscatlen(s,"\\a",2); break;
        case '\b': s = sdscatlen(s,"\\b",2); break;
        default:
            if (isprint(*p))
                s = sdsaddchar(s,*p);
            else
                s = sdscatprintf(s,"\\x%02x",(unsigned char)*p);
            break;
        }
        p++;
    }
    return sdsaddchar(s,'"');
}

/* Helper function for sdssplitargs() that returns non zero if 'c'
 * is a valid hex digit. */
int is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

/* Helper function for sdssplitargs() that converts a hex digit into an
 * integer from 0 to 15 */
int hex_digit_to_int(char c) {
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
SDS_INIT_FUNC sds *sdssplitargs(const char *s_restrict line, int *s_restrict argc) {
    const char *p = line;
    char *current = NULL;
    char **vector = NULL;

    *argc = 0;
    while(1) {
        /* skip blanks */
        while(*p && isspace(*p)) p++;
        if (*p) {
            /* get a token */
            int inq=0;  /* set to 1 if we are in "quotes" */
            int insq=0; /* set to 1 if we are in 'single quotes' */
            int done=0;

            if (SDS_UNLIKELY(current == NULL)) current = sdsempty();
            while(!done) {
                if (inq) {
                    if (*p == '\\' && *(p+1) == 'x' &&
                                             is_hex_digit(*(p+2)) &&
                                             is_hex_digit(*(p+3)))
                    {
                        unsigned char byte;

                        byte = (hex_digit_to_int(*(p+2))*16)+
                                hex_digit_to_int(*(p+3));
                        current = sdsaddchar(current,byte);
                        p += 3;
                    } else if (*p == '\\' && *(p+1)) {
                        char c;

                        p++;
                        switch(*p) {
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                        case 't': c = '\t'; break;
                        case 'b': c = '\b'; break;
                        case 'a': c = '\a'; break;
                        default: c = *p; break;
                        }
                        current = sdsaddchar(current,c);
                    } else if (*p == '"') {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (SDS_UNLIKELY(*(p+1) && !isspace(*(p+1)))) goto err;
                        done=1;
                    } else if (SDS_UNLIKELY(!*p)) {
                        /* unterminated quotes */
                        goto err;
                    } else {
                        current = sdsaddchar(current,*p);
                    }
                } else if (insq) {
                    if (*p == '\\' && *(p+1) == '\'') {
                        p++;
                        current = sdsaddchar(current,'\'');
                    } else if (*p == '\'') {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (SDS_UNLIKELY(*(p+1) && !isspace(*(p+1)))) goto err;
                        done=1;
                    } else if (SDS_UNLIKELY(!*p)) {
                        /* unterminated quotes */
                        goto err;
                    } else {
                        current = sdsaddchar(current,*p);
                    }
                } else {
                    switch(*p) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                    case '\0':
                        done=1;
                        break;
                    case '"':
                        inq=1;
                        break;
                    case '\'':
                        insq=1;
                        break;
                    default:
                        current = sdsaddchar(current,*p);
                        break;
                    }
                }
                if (*p) p++;
            }
            /* add the token to the vector */
            vector = (char **)s_realloc(vector,((*argc)+1)*sizeof(char *));
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
    SDS_ERR_RETURN(NULL);
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
SDS_MUT_FUNC sds sdsmapchars(sds s_restrict s, const char *s_restrict from, const char *s_restrict to, size_t setlen) {
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
SDS_MUT_FUNC sds sdsjoin(const char **s_restrict argv, int argc, const char *s_restrict sep) {
    sds join = sdsempty();
    int j;

    for (j = 0; j < argc; j++) {
        join = sdscat(join, argv[j]);
        if (j != argc-1) join = sdscat(join,sep);
    }
    return join;
}

/* Like sdsjoin, but joins an array of SDS strings. */
SDS_MUT_FUNC sds sdsjoinsds(sds *s_restrict argv, int argc, const char *s_restrict sep, size_t seplen) {
    sds join = sdsempty();
    int j;

    for (j = 0; j < argc; j++) {
        join = sdscatsds(join, argv[j]);
        if (j != argc-1) join = sdscatlen(join,sep,seplen);
    }
    return join;
}

/* Wrappers to the allocators used by SDS. Note that SDS will actually
 * just use the macros defined into sdsalloc.h in order to avoid to pay
 * the overhead of function calls. Here we define these wrappers only for
 * the programs SDS is linked to, if they want to touch the SDS internals
 * even if they use a different allocator. */
void *sds_malloc(size_t size) { return s_malloc(size); }
void *sds_realloc(void *ptr, size_t size) { return s_realloc(ptr,size); }
void sds_free(void *ptr) { s_free(ptr); }
