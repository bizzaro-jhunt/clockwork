

############################################################
# Global Variables


CC_FLAGS := -g #                       Debug syms for gdb
CC_FLAGS += -gdwarf-2 #                DWARF3; for Valgrind
CC_FLAGS += -fprofile-arcs #           gcov / lcov coverage
CC_FLAGS += -ftest-coverage #          gcov / lcov coverage
CC_FLAGS += -Wall #                    Warn on everything

CC_FLAGS += -lssl
CC_FLAGS += -lpthread

CC := gcc $(CC_FLAGS)

LEX_FLAGS :=
#LEX_FLAGS += --debug
LEX_FLAGS += --verbose
LEX_FLAGS += --header-file
LEX_FLAGS += --yylineno
LEX := flex $(LEX_FLAGS)

YACC_FLAGS :=
YACC_FLAGS += -Wall
#YACC_FLAGS += --debug
YACC_FLAGS += --token-table
YACC_FLAGS += --defines
YACC_FLAGS += --report=all
YACC := bison $(YACC_FLAGS)

VG := valgrind --leak-check=full --show-reachable=yes --read-var-info=yes --track-origins=yes

LCOV := lcov --directory . --base-directory .

MOG := ./mog

############################################################
# Object Group Variables

UTILS := d sha1sum sizes

# Resource types
RESOURCE_OBJECTS := res_user.o res_group.o res_file.o
RESOURCE_HEADERS := res_user.h res_group.h res_file.h

# Supporting object files
CORE_OBJECTS := mem.o sha1.o pack.o stringlist.o userdb.o

############################################################
# Default Target

all: test $(UTILS)

############################################################
# Utilities

d: main.o $(CORE_OBJECTS) $(RESOURCE_OBJECTS)
	$(CC) -o $@ $+

sha1sum: sha1.o sha1sum.o
	$(CC) -o $@ $+

sizes: sizes.c $(RESOURCE_HEADERS) userdb.h
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
          test/assertions.o \
          mem.o \
          test/list.o \
          test/stringlist.o stringlist.o \
          test/userdb.o userdb.o \
          test/pack.o pack.o \
          test/res_file.o res_file.o \
          test/res_group.o res_group.o \
          test/res_user.o res_user.o \
          test/policy.o policy.o \
          test/host_registry.o host_registry.o \
          test/fact.o fact.o \
          test/ast.o ast.o \
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
	rm -f spec/lexer.c spec/parser.c spec/parser.h spec/*.output

dist: clean
	rm -rf doc/coverage

fixme:
	find . -name '*.c' -o -name '*.c' | xargs grep -n FIXME: | sed -e 's/:[^:]*FIXME: /:/' -e 's/ *\*\///' | column -t -s :

############################################################
# "Extra" Dependencies

main.o: main.c $(RESOURCE_HEADERS)
	$(CC) -c -o $@ $<

############################################################
# EXPERIMENTAL

test_server: test_server.o proto.o
	$(CC) -o $@ $+

test_client: test_client.o proto.o
	$(CC) -o $@ $+

polspec: spec/lexer.o spec/parser.o \
         policy.o ast.o \
         $(RESOURCE_OBJECTS) \
         $(CORE_OBJECTS) \
         polspec.o
	$(CC) -o $@ $+

############################################################
# Pattern Rules

%.o: %.c %.h
	$(CC) -c -o $@ $<

spec/lexer.c: spec/lexer.l spec/parser.h
	$(LEX) --outfile=$@ $<

spec/parser.h: spec/parser.y
	$(YACC) --output-file=$@ $<

spec/parser.c: spec/parser.y
	$(YACC) --output-file=$@ $<
