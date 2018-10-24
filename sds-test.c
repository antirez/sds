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
#include <limits.h>

#include "testhelp.h"
#include "sds.h"

int sdsTest(void) {
    sds x = sdsnew("foo"), y;

    test_cond("Create a string and obtain the length",
        sdslen(x) == 3 && memcmp(x,"foo\0",4) == 0)

    sdsfree(x);
    x = sdsnewlen("foo",2);
    test_cond("Create a string with specified length",
        sdslen(x) == 2 && memcmp(x,"fo\0",3) == 0)

    x = sdscat(x,"bar");
    test_cond("Strings concatenation",
        sdslen(x) == 5 && memcmp(x,"fobar\0",6) == 0);

    x = sdscpy(x,"a");
    test_cond("sdscpy() against an originally longer string",
        sdslen(x) == 1 && memcmp(x,"a\0",2) == 0)

    x = sdscpy(x,"xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk");
    test_cond("sdscpy() against an originally shorter string",
        sdslen(x) == 33 &&
        memcmp(x,"xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk\0",33) == 0)

    x = sdsclear(x);
    test_cond("sdsclear() properly clears a string", strlen(x) == 0)

    x = sdscat(x, "bar");
    test_cond("sdsclear() overwrites an sds string properly",
        sdslen(x) == 3 && memcmp(x, "bar\0", 4) == 0)

    sdsfree(x);
    x = sdscatprintf(sdsempty(),"%d",123);
    test_cond("sdscatprintf() seems working in the base case",
        sdslen(x) == 3 && memcmp(x,"123\0",4) == 0)

    sdsfree(x);
    x = sdscatprintf(sdsempty(), "%s", "0ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJ");
    test_cond("sdscatprintf() seems working with a very long string",
        sdslen(x) == 101 &&
        memcmp("0ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJ", x, 101) == 0)

    sdsfree(x);
    x = sdsnew("--");
    x = sdscatfmt(x, "Hello %s World %I,%I--", "Hi!", LLONG_MIN,LLONG_MAX);
    test_cond("sdscatfmt() seems working in the base case",
        sdslen(x) == 60 &&
        memcmp(x,"--Hello Hi! World -9223372036854775808,"
                 "9223372036854775807--",60) == 0)
    printf("[%s]\n",x);

    sdsfree(x);
    x = sdsnew("--");
    x = sdscatfmt(x, "%u,%U--", UINT_MAX, ULLONG_MAX);
    test_cond("sdscatfmt() seems working with unsigned numbers",
        sdslen(x) == 35 &&
        memcmp(x,"--4294967295,18446744073709551615--",35) == 0)


    sdsfree(x);
    x = sdscatfmt(sdsempty(), "%s", "xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk");
    test_cond("sdscatfmt() seems working with a very long string",
        sdslen(x) == 33 &&
        memcmp(x,"xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk\0",33) == 0)

    sdsfree(x);
    x = sdsnew(" x ");
    x = sdstrim(x," x");
    test_cond("sdstrim() works when all chars match",
        sdslen(x) == 0)

    sdsfree(x);
    x = sdsnew(" x ");
    x = sdstrim(x," ");
    test_cond("sdstrim() works when a single char remains",
        sdslen(x) == 1 && x[0] == 'x')

    sdsfree(x);
    x = sdsnew("xxciaoyyy");
    x = sdstrim(x,"xy");
    test_cond("sdstrim() correctly trims characters",
        sdslen(x) == 4 && memcmp(x,"ciao\0",5) == 0)

    y = sdsdup(x);
    y = sdsrange(y,1,1);
    test_cond("sdsrange(...,1,1)",
        sdslen(y) == 1 && memcmp(y,"i\0",2) == 0)

    sdsfree(y);
    y = sdsdup(x);
    y = sdsrange(y,1,-1);
    test_cond("sdsrange(...,1,-1)",
        sdslen(y) == 3 && memcmp(y,"iao\0",4) == 0)

    sdsfree(y);
    y = sdsdup(x);
    y = sdsrange(y,-2,-1);
    test_cond("sdsrange(...,-2,-1)",
        sdslen(y) == 2 && memcmp(y,"ao\0",3) == 0)

    sdsfree(y);
    y = sdsdup(x);
    y = sdsrange(y,2,1);
    test_cond("sdsrange(...,2,1)",
        sdslen(y) == 0 && memcmp(y,"\0",1) == 0)

    sdsfree(y);
    y = sdsdup(x);
    y = sdsrange(y,1,100);
    test_cond("sdsrange(...,1,100)",
        sdslen(y) == 3 && memcmp(y,"iao\0",4) == 0)

    sdsfree(y);
    y = sdsdup(x);
    y = sdsrange(y,100,100);
    test_cond("sdsrange(...,100,100)",
        sdslen(y) == 0 && memcmp(y,"\0",1) == 0)

    sdsfree(y);
    sdsfree(x);
    x = sdsnew("foo");
    y = sdsnew("foa");
    test_cond("sdscmp(foo,foa)", sdscmp(x,y) > 0)

    sdsfree(y);
    sdsfree(x);
    x = sdsnew("bar");
    y = sdsnew("bar");
    test_cond("sdscmp(bar,bar)", sdscmp(x,y) == 0)

    sdsfree(y);
    sdsfree(x);
    x = sdsnew("aar");
    y = sdsnew("bar");
    test_cond("sdscmp(bar,bar)", sdscmp(x,y) < 0)

    sdsfree(y);
    sdsfree(x);
    x = sdsnewlen("\a\n\0foo\r",7);
    y = sdscatrepr(sdsempty(),x,sdslen(x));
    test_cond("sdscatrepr(...data...)",
        memcmp(y,"\"\\a\\n\\x00foo\\r\"",15) == 0)

    {
        char *p;
        int i;
        size_t step = 10, j;

        sdsfree(x);
        sdsfree(y);
        x = sdsnew("0");
        test_cond("sdsnew() free/len buffers", sdslen(x) == 1 && sdsavail(x) == 0);

        /* Run the test a few times in order to hit the first two
         * SDS header types. */
        for (i = 0; i < 10; i++) {
            size_t oldlen = sdslen(x);
            x = sdsMakeRoomFor(x,step);

            test_cond("sdsMakeRoomFor() len", sdslen(x) == oldlen);
            test_cond("sdsMakeRoomFor() free", sdsavail(x) >= step);
            p = x+oldlen;
            for (j = 0; j < step; j++) {
                p[j] = 'A'+j;
            }
            sdsIncrLen(x,step);
        }
        test_cond("sdsMakeRoomFor() content",
            memcmp("0ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJ",x,101) == 0);
        test_cond("sdsMakeRoomFor() final length",sdslen(x)==101);

        sdsfree(x);
    }

    x = sdsnew("TeSt123");
    x = sdstolower(x);
    test_cond("whether sdstolower works properly",
        strcmp(x, "test123") == 0)

    sdsfree(x);
    x = sdsnew("TeSt123");
    x = sdstoupper(x);
    test_cond("whether sdstoupper works properly",
        strcmp(x, "TEST123") == 0)

    sdsfree(x);

    {
        const char *args[] = { "hello", "world", NULL };
        x = sdsjoin(args, 2, "|");
        test_cond("whether sdsjoin works properly",
            strcmp(x, "hello|world") == 0)
    }
    sdsfree(x);
    {
        int argcount;
        const char *tosplit = "helloSworldStest";
        sds *split = sdssplit(tosplit, "S", &argcount);

        test_cond("whether sdssplit works properly",
            argcount == 3
            && strcmp(split[0], "hello") == 0
            && strcmp(split[1], "world") == 0
            && strcmp(split[2], "test") == 0)

        sdsfreesplitres(split, argcount);
        split = sdssplitlen(tosplit, strlen(tosplit), "S", 1, &argcount);

        test_cond("whether sdssplitlen works properly",
            argcount == 3
            && strcmp(split[0], "hello") == 0
            && strcmp(split[1], "world") == 0
            && strcmp(split[2], "test") == 0)

        sdsfreesplitres(split, argcount);
        tosplit = "hello--LoNgSeP--world--LoNgSeP--test";
        split = sdssplit(tosplit, "--LoNgSeP--", &argcount);

        test_cond("whether sdssplit works on longer separators",
            argcount == 3
            && strcmp(split[0], "hello") == 0
            && strcmp(split[1], "world") == 0
            && strcmp(split[2], "test") == 0)
        sdsfreesplitres(split, argcount);
        split = sdssplitargs("hello world \t\n  test\n   ", &argcount);
        test_cond("whether sdssplitargs works properly",
            argcount == 3
            && strcmp(split[0], "hello") == 0
            && strcmp(split[1], "world") == 0
            && strcmp(split[2], "test") == 0)
        sdsfreesplitres(split, argcount);
    }

    x = sdsnew("123");
    x = sdsaddchar(x, '4');
    test_cond("whether sdsaddchar works in a base test",
        strcmp(x, "1234") == 0);

    sdsfree(x);
    x = sdsnew("123");
    x = sdsaddint(x, 456);
    test_cond("whether sdsaddint works in a base test",
        strcmp(x, "123456") == 0);

    sdsfree(x);
    x = sdsnew("123");
    x = sdsadduint(x, 456);
    test_cond("whether sdsadduint works in a base test",
        strcmp(x, "123456") == 0);

    sdsfree(x);
    x = sdsnew("*");
    x = sdsaddlonglong(x, LLONG_MAX);
    test_cond("whether sdsaddlonglong adds big signed numbers properly",
        strcmp(x, "*9223372036854775807") == 0)

    sdsfree(x);
    x = sdsnew("*");
    x = sdsaddulonglong(x, ULLONG_MAX);
    test_cond("whether sdsaddulonglong adds big unsigned numbers properly",
        strcmp(x, "*18446744073709551615") == 0)

    sdsfree(x);

#if SDSADD_TYPE != 0
    if (SDSADD_TYPE == 1)        /* C11 */
        puts("Testing sdsadd's C11 _Generic implementation...");
    else if (SDSADD_TYPE == 2) /* C++ */
        puts("Testing sdsadd's C++ overload/type_traits implementation...");
    else
        puts("Testing sdsadd's GCC extension implementation...");

    x = sdsnew("123");
    x = sdsadd(x, '4');
    test_cond("whether sdsadd properly detects '4' as a char literal",
        strcmp(x, "1234") == 0)

    sdsfree(x);
    x = sdsnew("123");
    x = sdsadd(x, '3' + 1);
    test_cond("whether sdsadd properly detects '3' + 1 as a char literal",
        strcmp(x, "1234") == 0)

    sdsfree(x);
    x = sdsnew("123");
    x = sdsadd(x, 1 + '3');
    test_cond("whether sdsadd properly detects 1 + '3' as a char literal",
        strcmp(x, "1234") == 0)

    sdsfree(x);
    x = sdsnew("123");
    x = sdsadd(x, (char)52);
    test_cond("whether sdsadd properly detects (char)52 as a char literal",
        strcmp(x, "1234") == 0)
    {
        char c = '4';

        sdsfree(x);
        x = sdsnew("123");
        x = sdsadd(x, c);
        test_cond("whether sdsadd adds an existing char properly",
            strcmp(x, "1234") == 0)
    }

    sdsfree(x);
    x = sdsnew("123");
    x = sdsadd(x, 4);
    test_cond("whether sdsadd adds an int properly",
        strcmp(x, "1234") == 0)

    sdsfree(x);
    x = sdsnew("*");
    x = sdsadd(x, LLONG_MAX);
    test_cond("whether sdsadd adds big signed numbers properly",
        strcmp(x, "*9223372036854775807") == 0)

    sdsfree(x);
    x = sdsnew("*");
    x = sdsadd(x, ULLONG_MAX);
    test_cond("whether sdsadd adds big unsigned numbers properly",
        strcmp(x, "*18446744073709551615") == 0)

    sdsfree(x);
    x = sdsnew("123");
    x = sdsadd(x, "4");
    test_cond("whether sdsadd adds a string literal properly",
        strcmp(x, "1234") == 0)

    sdsfree(x);
    /* To mess with the C++ literal overload */
    {
        const char *str = "4";
        x = sdsnew("123");
        x = sdsadd(x, str);
        test_cond("whether sdsadd adds an existing string properly",
            strcmp(x, "1234") == 0)

        sdsfree(x);
        x = sdsnew("123");
        x = sdsadd(x, str[0]);
        test_cond("whether sdsadd adds a char from an indexed string properly",
            strcmp(x, "1234") == 0)

        sdsfree(x);
        x = sdsnew("123");
        x = sdsadd(x, *str);
        test_cond("whether sdsadd adds a char from a dereferenced string properly",
            strcmp(x, "1234") == 0)


    }
    sdsfree(x);

#   if (defined(__cplusplus))
    {
        /* C++ std::string conversion */
        std::string stdstr = "456";

        x = sdsnew("123");
        x = sdsadd(x, stdstr);
        test_cond("whether sdsadd works on std::string",
            strcmp(x, "123456") == 0)

        sdsfree(x);
        x = sdsfromstdstr(stdstr);
        test_cond("whether conversion from std::string works properly",
             strcmp(x, "456") == 0)

        sdsfree(x);
        x = sdsnew("123");
        std::string stdstr2 = sds2stdstr(x);
        test_cond("whether conversion to std::string works properly",
            stdstr2 == "123")

        sdsfree(x);
    }
#    endif /* __cplusplus */
#else /* SDSADD_TYPE == 0 */
    puts("Not testing sdsadd. Try compiling with GCC, a C++11, or o C11 compiler, "
         "or manually defining SDSADD.");
#endif /* SDSADD_TYPE */
    test_report()
    return 0;
}

int main(void) {
    return sdsTest();
}
