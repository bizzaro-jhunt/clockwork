

############################################################
# Global Variables

CC_FLAGS := -g

CC := gcc $(CC_FLAGS)

VG := valgrind --leak-check=full --show-reachable=yes

############################################################
# Object Group Variables

UTILS := d sha1sum sizes

############################################################
# Default Target

all: test $(UTILS)

############################################################
# Utilities

d: main.o sha1.o res_file.o mem.o res_user.o res_group.o
	$(CC) -o $@ $+

sha1sum: sha1.o sha1sum.o
	$(CC) -o $@ $+

sizes: sizes.c res_user.h res_file.h res_group.h
	$(CC) -o $@ $<

############################################################
# Unit Tests

test: tests
	test/run

memtest: tests
	$(VG) test/run

tests: test/run

test/run: test/run.o test/test.o test/userdb.o userdb.o
	$(CC) -o $@ $+

test/%.o: test/%.c test/test.h
	$(CC) -c -o $@ $<

############################################################
# Maintenance

clean:
	find . -name '*.o' -o -name '*.gc??' | xargs rm -f
	rm -f $(UTILS) test/userdb

############################################################
# "Extra" Dependencies

main.o: main.c res_file.h res_group.h res_user.h
	$(CC) -c -o $@ $<

############################################################
# Pattern Rules

%.o: %.c %.h
	$(CC) -c -o $@ $<

