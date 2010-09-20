
CC_FLAGS := -g

CC := gcc $(CC_FLAGS)
VG := valgrind --leak-check=full --show-reachable=yes

UTILS := d sha1sum sizes

all: test $(UTILS)

test: tests
	test/userdb

tests: test/userdb

memtest: test
	$(VG) test/userdb

test/userdb: test/test.o test/userdb.o userdb.o
	$(CC) -o $@ $+

sizes: sizes.c res_user.h res_file.h res_group.h
	$(CC) -o $@ $<

d: main.o sha1.o res_file.o mem.o res_user.o res_group.o
	$(CC) -o $@ $+

sha1sum: sha1.o sha1sum.o
	$(CC) -o $@ $+

main.o: main.c res_file.h res_group.h res_user.h
	$(CC) -c -o $@ $<

test/%.o: test/%.c test/test.h
	$(CC) -c -o $@ $<

%.o: %.c %.h
	$(CC) -c -o $@ $<

clean:
	find . -name '*.o' -o -name '*.gc??' | xargs rm -f
	rm -f $(UTILS) test/userdb
