
# Include our configuration, if we've got it; otherwise,
# GNU make will run ./configure because we have a target for
# config; then it will load config like we asked.
#
# Thanks GNU make!
#
-include config

############################################################
# Global Variables

ROOT := $(shell pwd)

LEX_FLAGS  := --verbose --header-file --yylineno
YACC_FLAGS := -Wall --token-table --defines --report=all
CC_FLAGS   := -Wall -lssl -lpthread -lsqlite3 -laugeas

OPENSSL := $(shell if [ -f ext/openssl/lib/libssl.so ]; then echo local; else echo system; fi)
ifeq ($(OPENSSL), local)
  export LD_LIBRARY_PATH=./ext/openssl/lib

  # Have to specify -lcrypto so that gcc/ld look for it in
  # our -L override directories;  otherwise, ld complains
  # about undefined symbols that are in the local version,
  # but not the system version (i.e. EVP_idea_cbc)
  #
  CC_FLAGS := -L./ext/openssl/lib -lcrypto $(CC_FLAGS)
endif

ifeq ($(BUILD_MODE),development)
  # In development mode, turn on DWARF2 for Valgrind and gcov/lcov support
  CC_FLAGS += -gdwarf-2 -fprofile-arcs -ftest-coverage
  #CC_FLAGS += -pg # gprof runtime support

  # In development mode, turn on all debugging support
  LEX_FLAGS  += --debug
  YACC_FLAGS += --debug
  CC_FLAGS   += -g -DDEVEL

else
  # In release mode, turn off all debugging support
  CC_FLAGS += -DNDEBUG
endif

CC      := gcc $(CC_FLAGS)
LEX     := flex $(LEX_FLAGS)
YACC    := bison $(YACC_FLAGS)
VG      := build/valgrind
LCOV    := lcov --directory . --base-directory .
GENHTML := genhtml --prefix $(shell dirname `pwd`)
MOG     := ./mog
DOXYGEN := doxygen

APIDOC_CONF := doc/doxy.api.conf
APIDOC_ROOT := doc/api


############################################################
# Object Group Variables

UTILS := sha1sum polspec tplspec
CORE  := cwa cwcert cwca policyd
DEBUGGERS := debug/service-manager debug/package-manager

# Managers
MANAGER_OBJECTS := managers/service.o managers/package.o
MANAGER_HEADERS := managers/service.h managers/package.h

# Parser object files
SPEC_PARSER_OBJECTS := spec/lexer.o spec/grammar.o spec/parser.o
CONFIG_PARSER_OBJECTS := conf/lexer.o conf/grammar.o conf/parser.o
TEMPLATE_PARSER_OBJECTS := tpl/lexer.o tpl/grammar.o tpl/parser.o

# Template system object files
TEMPLATE_OBJECTS := template.o $(TEMPLATE_PARSER_OBJECTS)

# Resource types
RESOURCE_OBJECTS := resource.o resources.o job.o $(MANAGER_OBJECTS) $(TEMPLATE_OBJECTS)
RESOURCE_HEADERS := resource.h resources.h job.h $(MANAGER_HEADERS) template.h

# Supporting object files
CORE_OBJECTS := mem.o sha1.o pack.o hash.o stringlist.o userdb.o log.o cert.o prompt.o exec.o string.o

# Policy object files
POLICY_OBJECTS := policy.o $(RESOURCE_OBJECTS)

# Manpages
MANPAGE_SRC     := $(shell ls -1 man/*.1 man/*.5)
MANPAGE_OBJECTS := $(shell ls -1 man/*.1 man/*.5 | sed -e 's/\.\([0-9]\)/.\1.gz/')

NO_LCOV :=
NO_LCOV += test/test.c
NO_LCOV += spec/grammar.c spec/lexer.c
NO_LCOV += conf/grammar.c conf/lexer.c
NO_LCOV += log.c # Can't easily test syslog-based logging methods


############################################################
# Group Target

all: $(UTILS) $(CORE) manpages

manpages: $(MANPAGE_OBJECTS)

install: all
	@echo "Installing to $(DESTDIR)/"
	@echo install -d -u $(CLOCKWORK_USER) -g $(CLOCKWORK_GROUP) -m 0750 $(VARDIR)
	@echo install -d -u $(CLOCKWORK_USER) -g $(CLOCKWORK_GROUP) -m 0750 $(VARDIR)/db
	@echo install -d -u $(CLOCKWORK_USER) -g $(CLOCKWORK_GROUP) -m 0750 $(VARDIR)/run
	@echo install -d -u $(CLOCKWORK_USER) -g $(CLOCKWORK_GROUP) -m 0750 $(ETCDIR)
	@echo install -d -u $(CLOCKWORK_USER) -g $(CLOCKWORK_GROUP) -m 0750 $(ETCDIR)/ssl
	@echo install -d -u $(CLOCKWORK_USER) -g $(CLOCKWORK_GROUP) -m 0750 $(ETCDIR)/ssl/pending
	@echo install -d -u $(CLOCKWORK_USER) -g $(CLOCKWORK_GROUP) -m 0750 $(ETCDIR)/ssl/certs
	@echo install    -u $(CLOCKWORK_USER) -g $(CLOCKWORK_GROUP) -m 0640 samples/policyd.conf $(ETCDIR)
	@echo install    -u $(CLOCKWORK_USER) -g $(CLOCKWORK_GROUP) -m 0640 samples/cwa.conf     $(ETCDIR)
	@echo install    -u $(CLOCKWORK_USER) -g $(CLOCKWORK_GROUP) -m 0640 samples/manifest.pol $(ETCDIR)
	@echo install policyd cwca cwa cwcert $(SBINDIR)
	@echo install man/*.1.gz $(MANDIR)/man1
	@echo install man/*.5.gz $(MANDIR)/man5

config: config.dist
	./configure

debuggers: $(DEBUGGERS)

summary:
	@echo
	@echo "============="
	@echo "BUILD SUMMARY"
	@echo "============="
	@echo
	@echo "CONFIGURATION"
	@echo " Mode:     $(BUILD_MODE)"
	@echo " OpenSSL:  $(OPENSSL)"
	@echo
	@echo "PATHS"
	@echo " ROOT:     $(ROOT)"
	@echo
	@echo "COMMANDS"
	@echo " cc:       $(CC)"
	@echo " lex:      $(LEX)"
	@echo " yacc:     $(YACC)"
	@echo " valgrind: $(VG)"
	@echo " lcov:     $(LCOV)"
	@echo " genhtml:  $(GENHTML)"
	@echo

############################################################
# Main Binaries

policyd: policyd.o $(CORE_OBJECTS) $(POLICY_OBJECTS) $(SPEC_PARSER_OBJECTS) $(CONFIG_PARSER_OBJECTS) proto.o server.o db.o
	$(CC) -o $@ $+

cwa: cwa.o $(CORE_OBJECTS) $(POLICY_OBJECTS) $(CONFIG_PARSER_OBJECTS) proto.o client.o db.o
	$(CC) -o $@ $+

cwcert: cwcert.o $(CORE_OBJECTS) $(POLICY_OBJECTS) $(CONFIG_PARSER_OBJECTS) proto.o client.o
	$(CC) -o $@ $+

cwca: cwca.o $(CORE_OBJECTS) $(CONFIG_PARSER_OBJECTS) server.o
	$(CC) -o $@ $+

sha1sum: sha1.o sha1sum.o mem.o log.o
	$(CC) -o $@ $+

polspec: polspec.o $(CORE_OBJECTS) $(POLICY_OBJECTS) $(SPEC_PARSER_OBJECTS)
	$(CC) -o $@ $+

tplspec: tplspec.o $(CORE_OBJECTS) $(POLICY_OBJECTS)
	$(CC) -o $@ $+


############################################################
# External Dependencies

externals:
	./ext/build-openssl 0.9.8k $(ROOT)/ext


############################################################
# Debugging Tools (mainly for CW developers)

debug/service-manager: debug/service-manager.o managers/service.o log.o exec.o mem.o
	$(CC) -o $@ $+

debug/package-manager: debug/package-manager.o managers/package.o log.o exec.o mem.o
	$(CC) -o $@ $+

############################################################
# Lex/YACC Parsers

spec/lexer.c: spec/lexer.l spec/grammar.h spec/lexer_impl.c spec/parser.h spec/private.h
	$(LEX) --outfile=$@ $<

spec/grammar.c spec/grammar.h: spec/grammar.y spec/grammar_impl.c spec/parser.c spec/parser.h spec/private.h
	$(YACC) --output-file=spec/grammar.c $<

spec/parser.o: spec/parser.c spec/parser.h spec/private.h
	$(CC) -c -o $@ $<

tpl/lexer.c: tpl/lexer.l tpl/grammar.h tpl/lexer_impl.c tpl/parser.h tpl/private.h
	$(LEX) --outfile=$@ $<

tpl/grammar.c tpl/grammar.h: tpl/grammar.y tpl/parser.c tpl/parser.h tpl/private.h
	$(YACC) -p yytpl --output-file=tpl/grammar.c $<

tpl/parser.o: tpl/parser.c tpl/parser.h tpl/private.h
	$(CC) -c -o $@ $<

conf/lexer.c: conf/lexer.l conf/grammar.h conf/lexer_impl.c conf/private.h
	$(LEX) --outfile=$@ $<

conf/grammar.c conf/grammar.h: conf/grammar.y conf/parser.c conf/parser.h conf/private.h
	$(YACC) -p yyconf --output-file=conf/grammar.c $<

conf/parser.o: conf/parser.c conf/parser.h conf/private.h
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

UNIT_TEST_SRC        := $(shell ls -1 test/*.c)
UNIT_TEST_TARGET_SRC := $(shell cd test; ls -1 *.c | egrep -v '(assertions|bits|fact|list|run|stree|test).c') stringlist.c log.c prompt.c
UNIT_TEST_OBJ        := $(UNIT_TEST_SRC:.c=.o) $(TEMPLATE_PARSER_OBJECTS)
UNIT_TEST_TARGET_OBJ := $(UNIT_TEST_TARGET_SRC:.c=.o)

test: unit_tests functional_tests
	find . -name '*.gcda' | xargs rm -f
	test/setup.sh
	@echo; echo;
	test/run $(TESTS)
	@echo; echo;
	test/functional/run

memtest: unit_tests
	test/setup.sh
	$(VG) test/run $(TESTS)

coverage: lcov.info unit_tests functional_tests
	rm -rf doc/coverage
	mkdir -p doc/coverage
	$(GENHTML) -o doc/coverage lcov.info

unit: unit_tests
	find . -name '*.gcda' | xargs rm -f
	test/setup.sh
	@echo; echo;
	test/run $(TESTS)
	@echo; echo;
	$(LCOV) --capture -o $@.tmp
	$(LCOV) --remove $@.tmp $(NO_LCOV) > lcov.info
	rm -f $@.tmp
	rm -rf doc/coverage
	mkdir -p doc/coverage
	$(GENHTML) -o doc/coverage lcov.info

unit_tests: test/run

test/run: $(UNIT_TEST_OBJ) $(UNIT_TEST_TARGET_OBJ)
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
                  test/util/executive \
                  cwcert

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

test/util/daemoncfg.o: test/util/daemoncfg.c conf/lexer.l
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

clean_lcov:
	find . -name '*.o' -o -name '*.gc??' | xargs rm -f
	rm -f lcov.info

clean: clean_lcov
	rm -f $(UTILS) $(CORE) $(DEBUGGERS) test/run polspec
	rm -f spec/lexer.c spec/grammar.c spec/grammar.h spec/*.output
	rm -f conf/lexer.c conf/grammar.c conf/grammar.h conf/*.output
	rm -f tpl/lexer.c tpl/grammar.c tpl/grammar.h tpl/*.output
	rm -f test/util/includer test/util/factchecker test/util/presence test/util/daemoncfg test/util/executive test/util/prompter
	rm -rf $(APIDOC_ROOT)/*
	rm -rf doc/coverage/*
	rm -f man/*.*.gz

dist: clean
	rm -rf doc/coverage
	rm -rf ext/openssl
	rm -rf ext/build/*
	rm -f config

fixme:
	find . -name '*.[ch15]' -not -path './ext/**' -not -path './man/tpl/*' | xargs grep -n FIXME: | sed -e 's/:[^:]*FIXME: /:/' -e 's/ *\*\///' | column -t -s :

stats: clean
	find . -name '*.[ch]' -not -path './test/**' 2>/dev/null | xargs ./util/nocomment | wc -l


############################################################
# Pattern Rules

%.o: %.c %.h
	$(CC) -c -o $@ $<

%.gz: %
	gzip -c $+ > $@
