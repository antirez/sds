CC ?= gcc
CXX ?= g++

FLAGS := -O2 -Wall -Wpedantic

all: c11 c++ gcc base

c11: sds-test-c11
c++: sds-test-c++
gcc: sds-test-gcc
base: sds-test

# Base tests (no sdsadd)
sds-test: sds.c sds.h testhelp.h sds-test.c
	$(CC) -o sds-test sds-test.c sds.c -std=c99 -DSDSADD_TYPE=0 $(FLAGS)
	@echo ">>> Type ./sds-test to run the sds base unit tests."

# C11 (sdsadd: _Generic)
sds-test-c11: sds.c sds.h testhelp.h sds-test.c
	$(CC) -o sds-test-c11 sds-test.c sds.c -std=c11 -DSDSADD_TYPE=1 $(FLAGS)
	@echo ">>> Type ./sds-test-c11 to run the sds C11 unit tests."

# C++11: overloads and type_traits
sds-test-c++: sds.c sds.h testhelp.h sds-test.c
	$(CXX) -x c++ -o sds-test-c++ sds-test.c sds.c -std=c++11 -DSDSADD_TYPE=2 $(FLAGS)
	@echo ">>> Type ./sds-test-c++ to run the sds C++ unit tests."

# GCC extensions: __builtin_types_compatible_p and __builtin_check_expr
sds-test-gcc: sds.c sds.h testhelp.h sds-test.c
	$(CC) -o sds-test-gcc sds-test.c sds.c -std=gnu99 -DSDSADD_TYPE=3 $(FLAGS)
	@echo ">>> Type ./sds-test-gcc to run the sds GCC extension unit tests."

clean:
	rm -f sds-test sds-test-c++ sds-test-c11 sds-test-gcc

.PHONY: clean all c11 c++ gcc base
