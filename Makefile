CFLAGS := -std=c99 -O2 -Wall -Wpedantic

all: sds-test

sds-test: sds.c sds.h testhelp.h sds-test.c
	$(CC) -o sds-test sds-test.c sds.c $(CFLAGS) -DSDS_TEST_MAIN
	@echo ">>> Type ./sds-test to run the sds.c unit tests."

clean:
	rm -f sds-test
