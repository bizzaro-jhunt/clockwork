
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

LEX_FLAGS  := --header-file --yylineno
YFLAGS     := -Wall --token-table --defines
CFLAGS     := -Wall
LDFLAGS    := -lssl -lpthread -lsqlite3 -laugeas -lgear

openssl_mode := $(shell if [ -f ext/openssl/lib/libssl.so ]; then echo local; else echo system; fi)
ifeq ($(openssl_mode), local)
  export LD_LIBRARY_PATH=./ext/openssl/lib

  # Have to specify -lcrypto so that gcc/ld look for it in
  # our -L override directories;  otherwise, ld complains
  # about undefined symbols that are in the local version,
  # but not the system version (i.e. EVP_idea_cbc)
  #
  LDFLAGS := -L./ext/openssl/lib -lcrypto $(LDFLAGS)
endif

ifeq ($(BUILD_MODE),development)
  # Link with the ctest testing framework
  LDFLAGS += -lctest

  # Turn on gcov/lcov support
  CFLAGS += -fprofile-arcs -ftest-coverage

  # In development mode, turn on all debugging support
  LEX_FLAGS  += --debug --verbose
  YFLAGS     += --debug --report=all
  CFLAGS     += -gdwarf-2 -g -DDEVEL

else
  # In release mode, turn off all debugging support
  CFLAGS += -DNDEBUG

  # In release mode, optimize like crazy
  CFLAGS += -O3

  # In release mode, create REALLY small binaries
  CFLAGS += -fdata-sections -ffunction-sections
  LDFLAGS += --gc-sections -s
endif

inst_targets    :=
build_targets   :=
ifeq ($(BUILD_AGENT), Y)
  inst_targets  += install-agent
  build_targets += build-agent
endif

ifeq ($(BUILD_MASTER), Y)
  inst_targets  += install-master
  build_targets += build-master
endif

CC      := gcc $(CFLAGS)
LD      := ld $(LDFLAGS)
LEX     := flex $(LEX_FLAGS)
YACC    := bison $(YFLAGS)
VG      := tools/valgrind
LCOV    := lcov --directory . --base-directory .
GENHTML := genhtml --prefix $(shell dirname `pwd`)
CDOC    := cdoc


############################################################
# Object Group Variables

util_bin      := sha1sum polspec tplspec
agent_bin     := cwa cwcert
master_bin    := policyd cwca cwpol
debug_bin     := debug/service-manager debug/package-manager

# Compiled binaries (candidates for `make clean')
COMPILED      := $(util_bin) $(agent_bin) $(master_bin) $(debug_bin)

# C source files that should not participate in code coverage analysis
no_lcov_c     := test/unit/**/* test/unit/* test/functional/*

# Parser object files
parser_spec_o := spec/lexer.o spec/grammar.o spec/parser.o
parser_conf_o := conf/lexer.o conf/grammar.o conf/parser.o
parser_tpl_o  := tpl/lexer.o tpl/grammar.o tpl/parser.o

# Core Supporting object files
core_o        := mem.o sha1.o userdb.o cert.o prompt.o exec.o augcw.o

# External Implementation Manager object giles
manager_o     := managers/service.o
manager_o     += managers/package.o

# Policy object files
policy_o      := policy.o resource.o resources.o job.o template.o
policy_o      += $(manager_o)
policy_o      += $(parser_tpl_o)

# Manpages
man_gz        := $(shell ls -1 man/*.1 man/*.5 man/*.7 | \
                   sed -e 's/\.\([0-9]\)/.\1.gz/')

# Unit Tests (test suites + test targets)
unit_test_o   := $(subst .c,.o,$(shell ls -1 test/unit/*.c))
unit_test_o   += $(subst .c,.o,$(shell cd test/unit; ls -1 *.c | \
                   egrep -v '(assertions|bits|fact|list|run|stree|test).c'))
unit_test_o   += prompt.o augcw.o
unit_test_o   += $(parser_tpl_o)
unit_test_o   += $(parser_spec_o)
unit_test_o   += $(manager_o)
unit_test_o   += $(core_o)

# Functional Test runners
fun_tests     := test/functional/includer
fun_tests     += test/functional/factchecker
fun_tests     += test/functional/daemoncfg
fun_tests     += test/functional/presence
fun_tests     += test/functional/prompter
fun_tests     += test/functional/executive

# Unit Test runners
unit_tests    := test/unit/run
unit_tests    += test/unit/helpers/proto_helper

# Configuration for API docs
apidocs_root  := doc/api
apidocs_theme := doc/theme


############################################################
# Group Target

all: $(COMPILED) manpages

build: $(build_targets)

build-utils:  $(util_bin)
build-agent:  $(agent_bin)
build-master: $(master_bin)

manpages: $(man_gz)

install: $(inst_targets)

install-agent: install-base build-agent manpages
	install    -o $(CWUSER) -g $(CWGROUP) -m 0700 cwa cwcert            $(DESTDIR)$(SBINDIR)
	install    -o $(CWUSER) -g $(CWGROUP) -m 0640 samples/cwa.conf      $(DESTDIR)$(ETCDIR)
	install    -o root      -g root       -m 0644 man/cwa.1.gz          $(DESTDIR)$(MANDIR)/man1
	install    -o root      -g root       -m 0644 man/cwa.conf.5.gz     $(DESTDIR)$(MANDIR)/man5
	install    -o root      -g root       -m 0644 extra/cwa.vim         $(DESTDIR)$(USRDIR)/share/clockwork-$(CWVERSION)/vim

install-master: install-base build-master manpages
	install -d -o $(CWUSER) -g $(CWGROUP) -m 0750 $(DESTDIR)$(VARDIR)/run
	install -d -o $(CWUSER) -g $(CWGROUP) -m 0750 $(DESTDIR)$(ETCDIR)/ssl/pending
	install -d -o $(CWUSER) -g $(CWGROUP) -m 0750 $(DESTDIR)$(ETCDIR)/ssl/certs
	install    -o $(CWUSER) -g $(CWGROUP) -m 0700 policyd cwca          $(DESTDIR)$(SBINDIR)
	install    -o $(CWUSER) -g $(CWGROUP) -m 0640 samples/policyd.conf  $(DESTDIR)$(ETCDIR)
	install    -o $(CWUSER) -g $(CWGROUP) -m 0640 samples/manifest.pol  $(DESTDIR)$(ETCDIR)
	install    -o root      -g root       -m 0644 man/policyd.1.gz      $(DESTDIR)$(MANDIR)/man1
	install    -o root      -g root       -m 0644 man/policyd.conf.5.gz $(DESTDIR)$(MANDIR)/man5
	install    -o root      -g root       -m 0644 man/res_*.5.gz        $(DESTDIR)$(MANDIR)/man5
	install    -o root      -g root       -m 0644 man/clockwork.7.gz    $(DESTDIR)$(MANDIR)/man7
	install    -o root      -g root       -m 0644 extra/policyd.vim     $(DESTDIR)$(USRDIR)/share/clockwork-$(CWVERSION)/vim
	install    -o root      -g root       -m 0644 extra/pol.vim         $(DESTDIR)$(USRDIR)/share/clockwork-$(CWVERSION)/vim

install-base:
	getent group $(CWGROUP) >/dev/null || groupadd -r $(CWGROUP)
	getent passwd $(CWUSER) >/dev/null || useradd -Mr -c "Clockwork" -g $(CWGROUP) -d $(HOMEDIR) $(CWUSER)
	install -d -o root      -g root       -m 0755 $(DESTDIR)$(SBINDIR)
	install -d -o root      -g root       -m 0755 $(DESTDIR)$(MANDIR)
	install -d -o root      -g root       -m 0755 $(DESTDIR)$(MANDIR)/man1
	install -d -o root      -g root       -m 0755 $(DESTDIR)$(MANDIR)/man5
	install -d -o root      -g root       -m 0755 $(DESTDIR)$(MANDIR)/man7
	install -d -o root      -g root       -m 0755 $(DESTDIR)$(USRDIR)/share/clockwork-$(CWVERSION)
	install -d -o root      -g root       -m 0755 $(DESTDIR)$(USRDIR)/share/clockwork-$(CWVERSION)/vim
	install -d -o $(CWUSER) -g $(CWGROUP) -m 0750 $(DESTDIR)$(VARDIR)
	install -d -o $(CWUSER) -g $(CWGROUP) -m 0750 $(DESTDIR)$(VARDIR)/db
	install -d -o $(CWUSER) -g $(CWGROUP) -m 0750 $(DESTDIR)$(ETCDIR)
	install -d -o $(CWUSER) -g $(CWGROUP) -m 0750 $(DESTDIR)$(ETCDIR)/ssl

config: config.dist
	./configure

debuggers: $(debug_bin)

summary:
	@./configure --show
	@echo "Build-specific options"
	@echo
	@echo "ROOT:      $(ROOT)"
	@echo
	@echo "Externals"
	@echo " OpenSSL:  $(openssl_mode)"
	@echo
	@echo "CDOC (API Docs)"
	@echo " root:     $(apidocs_root)"
	@echo " theme:    $(apidocs_theme)"
	@echo
	@echo "Commands"
	@echo " cc:       $(CC)"
	@echo " lex:      $(LEX)"
	@echo " yacc:     $(YACC)"
	@echo " valgrind: $(VG)"
	@echo " lcov:     $(LCOV)"
	@echo " genhtml:  $(GENHTML)"
	@echo " cdoc:     $(CDOC)"
	@echo

############################################################
# Main Binaries

policyd: $(core_o) $(policy_o) policyd.o $(parser_spec_o) $(parser_conf_o) proto.o server.o db.o
cwa:     $(core_o) $(policy_o) cwa.o     $(parser_conf_o) proto.o client.o db.o
cwcert:  $(core_o) $(policy_o) cwcert.o  $(parser_conf_o) proto.o client.o
cwca:    $(core_o) $(policy_o) cwca.o    $(parser_conf_o) server.o
cwpol:   $(core_o) $(policy_o) cwpol.o   $(parser_spec_o)
polspec: $(core_o) $(policy_o) polspec.o $(parser_spec_o)
tplspec: $(core_o) $(policy_o) tplspec.o
sha1sum: sha1.o sha1sum.o mem.o $(core_o) $(policy_o) $(parser_spec_o) $(parser_conf_o)


############################################################
# Debugging Tools (mainly for CW developers)

debug/service-manager: debug/service-manager.o managers/service.o exec.o mem.o
debug/package-manager: debug/package-manager.o managers/package.o exec.o mem.o


############################################################
# External Dependencies

externals:
	./ext/build-openssl 0.9.8k $(ROOT)/ext


############################################################
# Lex/YACC Parsers

parserdefs: spec/lexer.c spec/grammar.c tpl/lexer.c tpl/grammar.c conf/lexer.c conf/grammar.c

spec/lexer.c: spec/lexer.l spec/grammar.h spec/lexer_impl.c spec/parser.h spec/private.h
	$(LEX) --outfile=$@ $<

spec/grammar.c spec/grammar.h: spec/grammar.y spec/grammar_impl.c spec/parser.c spec/parser.h spec/private.h
	$(YACC) --output-file=spec/grammar.c $<

tpl/lexer.c: tpl/lexer.l tpl/grammar.h tpl/lexer_impl.c tpl/parser.h tpl/private.h
	$(LEX) --outfile=$@ $<

tpl/grammar.c tpl/grammar.h: tpl/grammar.y tpl/parser.c tpl/parser.h tpl/private.h
	$(YACC) -p yytpl --output-file=tpl/grammar.c $<

conf/lexer.c: conf/lexer.l conf/grammar.h conf/lexer_impl.c conf/private.h
	$(LEX) --outfile=$@ $<

conf/grammar.c conf/grammar.h: conf/grammar.y conf/parser.c conf/parser.h conf/private.h
	$(YACC) -p yyconf --output-file=conf/grammar.c $<


############################################################
# Documentation

docs: apidocs diagrams

apidocs:
	mkdir -p $(apidocs_root)
	rm -rf $(apidocs_root)/*
	cp -a $(apidocs_theme)/images $(apidocs_root)
	$(CDOC) --root $(apidocs_root) --theme $(apidocs_theme) *.c *.h

diagrams: doc/proto-agent.png doc/proto-cert.png

%.png: %.dot
	dot -Tpng $< -o $@


############################################################
# Regression Test Targets

test: coverage-clean unit-tests run-unit-tests run-functional-tests
test-unit: coverage-clean run-unit-tests
test-functional: coverage-clean run-functional-tests

coverage-clean:
	find . -name '*.gcda' 2>/dev/null | xargs rm -f

test/defs.h: test/prep
	./test/prep data

run-unit-tests: unit-tests
	test/prep setup
	#test/unit/setup
	@echo; echo;
	test/unit/run $(TESTS)
	@echo; echo;

run-functional-tests: functional-tests
	@echo; echo;
	test/functional/run
	@echo; echo;

##
## Conditional Test Targets
##
## Unless we are running in development mode, the `coverage' and
## `memtest' targets are nearly useless:
##
##  coverage - Depends on gcov support (only enabled in development mode)
##  memtest  - If you do find mem leaks, there is no way to track them
##             down, because the release-mode binaries are stripped.
##
ifeq ($(BUILD_MODE),development)
memtest: coverage-clean unit-tests functional-tests
	test/setup.sh
	@echo; echo;
	$(VG) test/unit/run $(TEST) $(TESTS)
	@echo; echo;
	$(VG) test/functional/run

coverage:
	$(LCOV) --capture -o $@.tmp
	$(LCOV) --remove $@.tmp $(no_lcov_c) > lcov.info
	rm -f $@.tmp
	rm -rf doc/coverage
	$(GENHTML) -o doc/coverage lcov.info

else
coverage:
	@echo "The \`coverage' target only makes sense in 'development' mode"
	@echo "(use ./configure --development)"

memtest:
	@echo "The \`memtest' target only makes sense in 'development' mode"
	@echo "(use ./configure --development)"
endif


############################################################
# Unit Tests

# Build all unit tests
unit-tests: test/defs.h test/unit/run test/unit/helpers/proto_helper

test/unit/run: $(unit_test_o)
test/unit/helpers/proto_helper: $(core_o) $(policy_o) proto.o


############################################################
# Functional Tests

functional-tests: $(fun_tests) cwcert

test/functional/includer:    test/functional/includer.o    $(core_o) $(parser_spec_o) $(policy_o)
test/functional/factchecker: test/functional/factchecker.o $(core_o) $(parser_spec_o) $(policy_o)
test/functional/daemoncfg:   test/functional/daemoncfg.o   $(core_o) $(parser_conf_o)
test/functional/presence:    test/functional/presence.o    $(core_o) $(parser_spec_o) $(policy_o)
test/functional/prompter:    test/functional/prompter.o    $(core_o)
test/functional/executive:   test/functional/executive.o   $(core_o)


############################################################
# Maintenance

cleandep:
	rm -f Makefile.deps

tidy:
	find . -name '*.o' -o -name '*.gc??' 2>/dev/null | xargs rm -f
	rm -f lcov.info

clean: tidy cleandep
	rm -f $(COMPILED) $(unit_tests) $(fun_tests) man/*.*.gz
	rm -f spec/*.output conf/*.output tpl/*.output
	rm -rf $(apidocs_root)/* doc/coverage/*

dist: clean
	rm -rf doc/coverage ext/openssl ext/build/*
	rm -rf test/unit/data test/unit/tmp
	rm -f config test/defs.h

fixme:
	find . -name '*.[ch15]' -not -path './ext/**' -not -path './man/tpl/*' -not -path './local/**' 2>/dev/null | \
	  xargs grep -n FIXME: | sed -e 's/:[^:]*FIXME: /:/' -e 's/ *\*\///' | column -t -s :


############################################################
# Pattern Rules

# gzip man pages
%.gz: %
	gzip -c $^ > $@


Makefile.deps:
	gcc -MM *.c > Makefile.deps

-include Makefile.deps
