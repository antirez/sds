LIBVERSION = 2.0.0

SDS_SRC = sds.c sds.h sdsalloc.h

PREFIX ?= /usr/local
INCLUDE_PATH ?= include/sds
LIBRARY_PATH ?= lib

INSTALL_INCLUDE_PATH = $(DESTDIR)$(PREFIX)/$(INCLUDE_PATH)
INSTALL_LIBRARY_PATH = $(DESTDIR)$(PREFIX)/$(LIBRARY_PATH)

INSTALL ?= cp -a

.PHONY: all

all: sds-test sds-install

sds-install: sds-lib
	mkdir -p $(INSTALL_INCLUDE_PATH) $(INSTALL_LIBRARY_PATH)
	$(INSTALL) libsds.so.$(LIBVERSION) $(INSTALL_LIBRARY_PATH)
	ln -s $(INSTALL_LIBRARY_PATH)/libsds.so.$(LIBVERSION) $(INSTALL_LIBRARY_PATH)/libsds.so
	$(INSTALL) sds.h $(INSTALL_INCLUDE_PATH)

sds-lib: sds.c sds.h sdsalloc.h
	$(CC) -fPIC -fstack-protector -std=c99 -pedantic -Wall -Werror -shared \
		-o libsds.so.$(LIBVERSION) -Wl,-soname=libsds.so.$(LIBVERSION) $(SDS_SRC)

sds-test: sds.c sds.h testhelp.h
	$(CC) -o sds-test sds.c -Wall -std=c99 -pedantic -O2 -DSDS_TEST_MAIN
	@echo ">>> Type ./sds-test to run the sds.c unit tests."

clean:
	rm -f sds-test
