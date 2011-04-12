

############################################################
# Global Variables

DO_PROFILING := yes
#DO_PROFILING := no

DO_DEBUGGING := yes
#DO_DEBUGGING := no


LEX_FLAGS := --verbose --header-file --yylineno

YACC_FLAGS := -Wall --token-table --defines --report=all

CC_FLAGS := -Wall -lssl -lpthread

ifeq ($(DO_DEBUGGING),yes)
  LEX_FLAGS += --debug

  YACC_FLAGS += --debug

  CC_FLAGS += -g #                       Debug syms for gdb
  CC_FLAGS += -DDEVEL
else
  CC_FLAGS += -DNDEBUG
endif

ifeq ($(DO_PROFILING),yes)
  CC_FLAGS += -gdwarf-2 #                DWARF3; for Valgrind
  CC_FLAGS += -pg #                      gprof runtime support
  CC_FLAGS += -fprofile-arcs #           gcov / lcov coverage
  CC_FLAGS += -ftest-coverage #          gcov / lcov coverage
endif

CC := gcc $(CC_FLAGS)
LEX := flex $(LEX_FLAGS)
YACC := bison $(YACC_FLAGS)

VG := valgrind --leak-check=full --show-reachable=yes --read-var-info=yes --track-origins=yes

LCOV := lcov --directory . --base-directory .
GENHTML := genhtml --prefix $(shell dirname `pwd`)

MOG := ./mog

APIDOC_CONF := doc/doxy.api.conf

APIDOC_ROOT := doc/api

DOXYGEN := doxygen


############################################################
# Object Group Variables

UTILS := sha1sum polspec
CORE  := cwa cwcert cwca policyd

# Resource types
RESOURCE_OBJECTS := resource.o resources.o report.o pkgmgr.o
RESOURCE_HEADERS := resource.h resources.h report.h pkgmgr.h

# Supporting object files
CORE_OBJECTS := mem.o sha1.o pack.o hash.o stringlist.o userdb.o log.o cert.o prompt.o exec.o

# Policy object files
POLICY_OBJECTS := policy.o $(RESOURCE_OBJECTS)

# Parser object files
SPEC_PARSER_OBJECTS := spec/lexer.o spec/grammar.o spec/parser.o
CONFIG_PARSER_OBJECTS := config/lexer.o config/grammar.o config/parser.o

NO_LCOV :=
NO_LCOV += test/test.c
NO_LCOV += spec/grammar.c   spec/lexer.c
NO_LCOV += config/grammar.c config/lexer.c
NO_LCOV += log.c # Can't easily test syslog-based logging methods


############################################################
# Default Target

all: $(UTILS) $(CORE)


############################################################
# Main Binaries

policyd: policyd.o $(CORE_OBJECTS) $(POLICY_OBJECTS) $(SPEC_PARSER_OBJECTS) $(CONFIG_PARSER_OBJECTS) proto.o server.o
	$(CC) -o $@ $+

cwa: cwa.o $(CORE_OBJECTS) $(POLICY_OBJECTS) $(CONFIG_PARSER_OBJECTS) proto.o client.o
	$(CC) -o $@ $+

cwcert: cwcert.o $(CORE_OBJECTS) $(POLICY_OBJECTS) $(CONFIG_PARSER_OBJECTS) proto.o client.o
	$(CC) -o $@ $+

cwca: cwca.o $(CORE_OBJECTS) $(CONFIG_PARSER_OBJECTS) server.o
	$(CC) -o $@ $+

sha1sum: sha1.o sha1sum.o mem.o log.o
	$(CC) -o $@ $+

polspec: $(CORE_OBJECTS) $(POLICY_OBJECTS) $(SPEC_PARSER_OBJECTS) polspec.o
	$(CC) -o $@ $+


############################################################
# Lex/YACC Parsers

spec/lexer.c: spec/lexer.l spec/grammar.h spec/lexer_impl.c spec/parser.h spec/private.h
	$(LEX) --outfile=$@ $<

spec/grammar.c spec/grammar.h: spec/grammar.y spec/grammar_impl.c spec/parser.c spec/parser.h spec/private.h
	$(YACC) --output-file=spec/grammar.c $<

spec/parser.o: spec/parser.c spec/parser.h spec/private.h
	$(CC) -c -o $@ $<


config/lexer.c: config/lexer.l config/grammar.h config/lexer_impl.c config/private.h
	$(LEX) --outfile=$@ $<

config/grammar.c config/grammar.h: config/grammar.y config/parser.c config/parser.h config/private.h
	$(YACC) -p yyconfig --output-file=config/grammar.c $<

config/parser.o: config/parser.c config/parser.h config/private.h
	$(CC) -c -o $@ $<


############################################################
# Documentation

docs: apidocs diagrams

apidocs:
	rm -rf $(APIDOC_ROOT)/*
	$(DOXYGEN) $(APIDOC_CONF) 2>&1 | tee $(APIDOC_ROOT)/doxygen.log
	@echo
	@echo === DOXYGEN ERROR SUMMARY ===================================================
	@echo
	@grep Warning $(APIDOC_ROOT)/doxygen.log | sed -e "s;.*$(PWD)/\([^:]*\).*;\1;" | sort | uniq -c
	@echo
	@echo === DOXYGEN ERROR DETAILS ===================================================
	@echo
	@grep Warning $(APIDOC_ROOT)/doxygen.log | sed -e 's/Parsing file //'

diagrams: doc/proto-agent.png doc/proto-cert.png

%.png: %.dot
	dot -Tpng $< -o $@


############################################################
# Unit Tests

test: unit_tests functional_tests
	find . -name '*.gcda' | xargs rm -f
	test/setup.sh
	@echo; echo;
	test/run
	@echo; echo;
	test/functional/run

memtest: unit_tests
	test/setup.sh
	$(VG) test/run

coverage: lcov.info unit_tests functional_tests
	rm -rf doc/coverage
	mkdir -p doc/coverage
	$(GENHTML) -o doc/coverage lcov.info

unit: unit_tests
	find . -name '*.gcda' | xargs rm -f
	test/setup.sh
	@echo; echo;
	test/run $(TEST)
	@echo; echo;
	$(LCOV) --capture -o $@.tmp
	$(LCOV) --remove $@.tmp $(NO_LCOV) > lcov.info
	rm -f $@.tmp
	rm -rf doc/coverage
	mkdir -p doc/coverage
	$(GENHTML) -o doc/coverage lcov.info

unit_tests: test/run

test/run: test/run.o test/test.o \
          test/assertions.o \
          mem.o exec.o \
          report.o log.o \
          test/list.o \
          test/stringlist.o stringlist.o \
          test/hash.o hash.o \
          test/userdb.o userdb.o \
          test/pack.o pack.o \
          test/resources.o resource.o resources.o \
          pkgmgr.o \
          test/policy.o test/stree.o test/fact.o policy.o \
          test/cert.o cert.o prompt.o \
          test/sha1.o sha1.o

	$(CC) -o $@ $+

test/%.o: test/%.c test/test.h
	$(CC) -c -o $@ $<

lcov.info: unit_tests functional_tests
	find . -name '*.gcda' | xargs rm -f
	test/setup.sh
	test/run
	test/functional/run
	$(LCOV) --capture -o $@.tmp
	$(LCOV) --remove $@.tmp $(NO_LCOV) > $@
	rm -f $@.tmp

test/resources.o: test/resources.c resources.h test/sha1_files.h
	$(CC) -c -o $@ $<

test/sha1_files.h:
	$(MOG) sha1_tests


############################################################
# Functional Tests

functional_tests: test/util/includer \
                  test/util/factchecker \
                  test/util/daemoncfg \
                  test/util/presence \
                  test/util/prompter \
                  test/util/executive

test/util/includer: test/util/includer.o \
                    $(CORE_OBJECTS) $(SPEC_PARSER_OBJECTS) $(POLICY_OBJECTS)
	$(CC) -o $@ $+

test/util/includer.o: test/util/includer.c spec/lexer.l
	$(CC) -c -o $@ $<

test/util/factchecker: test/util/factchecker.o \
                    $(CORE_OBJECTS) $(SPEC_PARSER_OBJECTS) $(POLICY_OBJECTS)
	$(CC) -o $@ $+

test/util/factchecker.o: test/util/factchecker.c spec/lexer.l
	$(CC) -c -o $@ $<

test/util/daemoncfg: test/util/daemoncfg.o \
                    $(CORE_OBJECTS) $(CONFIG_PARSER_OBJECTS)
	$(CC) -o $@ $+

test/util/daemoncfg.o: test/util/daemoncfg.c config/lexer.l
	$(CC) -c -o $@ $<

test/util/presence: test/util/presence.o \
                    $(CORE_OBJECTS) $(SPEC_PARSER_OBJECTS) $(POLICY_OBJECTS)
	$(CC) -o $@ $+

test/util/presence.o: test/util/presence.c spec/lexer.l
	$(CC) -c -o $@ $<

test/util/prompter: test/util/prompter.o \
                    $(CORE_OBJECTS)

test/util/executive: test/util/executive.o $(CORE_OBJECTS)
	$(CC) -o $@ $+


############################################################
# Maintenance

clean:
	find . -name '*.o' -o -name '*.gc??' | xargs rm -f
	rm -f lcov.info
	rm -f $(UTILS) $(CORE) test/run polspec
	rm -f spec/lexer.c spec/grammar.c spec/grammar.h spec/*.output
	rm -f config/lexer.c config/grammar.c config/grammar.h config/*.output
	rm -f test/util/includer test/util/factchecker test/util/presence test/util/daemoncfg test/util/executive
	rm -rf $(APIDOC_ROOT)/*
	rm -rf doc/coverage/*

dist: clean
	rm -rf doc/coverage

fixme:
	find . -name '*.[ch]' | xargs grep -n FIXME: | sed -e 's/:[^:]*FIXME: /:/' -e 's/ *\*\///' | column -t -s :

stats: clean
	find . -name '*.[ch]' -not -path './test/**' 2>/dev/null | xargs ./util/nocomment | wc -l


############################################################
# Pattern Rules

%.o: %.c %.h
	$(CC) -c -o $@ $<

