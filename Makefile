

all: d sha1sum sizes

sizes: sizes.c res_user.h res_file.h res_group.h
	gcc -o $@ $<

d: main.o sha1.o res_file.o mem.o res_user.o res_group.o
	gcc -o $@ $+

sha1sum: sha1.o sha1sum.o
	gcc -o $@ $+

main.o: main.c res_file.h res_group.h res_user.h
	gcc -c -o $@ $<

%.o: %.c %.h
	gcc -c -o $@ $<

clean:
	rm -f *.o d sha1sum
