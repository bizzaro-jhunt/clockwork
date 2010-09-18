


d: main.o sha1.o res_file.o
	gcc -o $@ $+

sha1sum: sha1.o sha1sum.o
	gcc -o $@ $+

%.o: %.c %.h
	gcc -c -o $@ $<

clean:
	rm -f *.o d sha1sum
