all: sds-test

sds-test: sds.c sds.h testhelp.h mem-testing.h mem-testing.c
	$(CC) -c mem-testing.c
	$(CC) -o sds-test sds.c -Wall -std=c99 -pedantic -O2 -DSDS_TEST_MAIN mem-testing.o
	@echo ">>> Type ./sds-test to run the sds.c unit tests."

clean:
	rm -f sds-test
	rm -f mem-testing.o
