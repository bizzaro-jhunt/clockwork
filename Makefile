

############################################################
# Global Variables


CC_FLAGS := -g #                       Debug syms for gdb
CC_FLAGS += -fprofile-arcs #           gcov / lcov coverage
CC_FLAGS += -ftest-coverage #          gcov / lcov coverage

CC := gcc $(CC_FLAGS)

VG := valgrind --leak-check=full --show-reachable=yes

LCOV := lcov --directory . --base-directory .

MOG := ./mog

############################################################
# Object Group Variables

UTILS := d sha1sum sizes

############################################################
# Default Target

all: test $(UTILS)

############################################################
# Utilities

d: main.o sha1.o res_file.o mem.o res_user.o res_group.o userdb.o
	$(CC) -o $@ $+

sha1sum: sha1.o sha1sum.o
	$(CC) -o $@ $+

sizes: sizes.c res_user.h res_file.h res_group.h userdb.h
	$(CC) -o $@ $<

############################################################
# Unit Tests

test: tests
	test/setup.sh
	test/run

memtest: tests
	test/setup.sh
	$(VG) test/run

coverage: lcov.info tests
	rm -rf doc/coverage
	mkdir -p doc/coverage
	genhtml -o doc/coverage lcov.info

tests: test/run

test/run: test/run.o test/test.o \
          mem.o \
          test/userdb.o userdb.o \
          test/res_file.o res_file.o \
          test/res_group.o res_group.o \
          test/res_user.o res_user.o \
          test/sha1.o sha1.o

	$(CC) -o $@ $+

test/%.o: test/%.c test/test.h
	$(CC) -c -o $@ $<

lcov.info: tests
	find . -name '*.gcda' | xargs rm -f
	test/setup.sh
	test/run
	$(LCOV) --capture -o $@.tmp
	$(LCOV) --remove $@.tmp test/* > $@
	rm -f $@.tmp

test/res_file.o: test/res_file.c res_file.h test/sha1_files.h
	$(CC) -c -o $@ $<

test/sha1_files.h:
	$(MOG) sha1_tests

############################################################
# Maintenance

clean:
	find . -name '*.o' -o -name '*.gc??' | xargs rm -f
	rm -f lcov.info
	rm -f $(UTILS) test/userdb

dist: clean
	rm -rf doc/coverage

fixme:
	find . -name '*.c' -o -name '*.c' | xargs grep -n FIXME:

############################################################
# "Extra" Dependencies

main.o: main.c res_file.h res_group.h res_user.h
	$(CC) -c -o $@ $<

############################################################
# Pattern Rules

%.o: %.c %.h
	$(CC) -c -o $@ $<

