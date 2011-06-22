
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

  # In release mode, create REALLY small binaries
  CC_FLAGS += -fdata-sections -ffunction-sections 
  CC_FLAGS += -Wl,--gc-sections -Wl,-s
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

CC      := gcc $(CC_FLAGS)
LEX     := flex $(LEX_FLAGS)
YACC    := bison $(YACC_FLAGS)
VG      := tools/valgrind
LCOV    := lcov --directory . --base-directory .
GENHTML := genhtml --prefix $(shell dirname `pwd`)
MOG     := ./mog
DOXYGEN := doxygen

APIDOC_CONF := doc/doxy.api.conf
APIDOC_ROOT := doc/api


############################################################
# Object Group Variables

util_bin      := sha1sum polspec tplspec
agent_bin     := cwa cwcert
master_bin    := policyd cwca
debug_bin     := debug/service-manager debug/package-manager

# Compiled binaries (candidates for `make clean')
COMPILED      := $(util_bin) $(agent_bin) $(master_bin) $(debug_bin)

# C header files that are automatically generated
auto_h        := spec/grammar.h conf/grammar.h tpl/grammar.h

# C source files that are automatically generated
auto_c        := spec/lexer.c   conf/lexer.c   tpl/lexer.c
auto_c        += spec/grammar.c conf/grammar.c tpl/grammar.c

# C source files that should not participate in code coverage analysis
no_lcov_c     := test/test.c log.c $(auto_c)

# Parser object files
parser_spec_o := spec/lexer.o spec/grammar.o spec/parser.o
parser_conf_o := conf/lexer.o conf/grammar.o conf/parser.o
parser_tpl_o  := tpl/lexer.o tpl/grammar.o tpl/parser.o

# Core Supporting object files
core_o        := mem.o sha1.o pack.o hash.o stringlist.o userdb.o log.o
core_o        += cert.o prompt.o exec.o string.o path.o augcw.o

# Policy object files
policy_o      := policy.o resource.o resources.o job.o template.o
policy_o      += managers/service.o managers/package.o template.o
policy_o      += $(parser_tpl_o)

# Manpages
man_gz        := $(shell ls -1 man/*.1 man/*.5 | \
                   sed -e 's/\.\([0-9]\)/.\1.gz/')

# Unit Tests (test suites + test targets)
unit_test_o   := $(subst .c,.o,$(shell ls -1 test/*.c))
unit_test_o   += $(subst .c,.o,$(shell cd test; ls -1 *.c | \
                   egrep -v '(assertions|bits|fact|list|run|stree|test).c'))
unit_test_o   += stringlist.o log.o prompt.o
unit_test_o   += $(parser_tpl_o)

# Functional Test runners
fun_tests     := test/util/includer test/util/factchecker test/util/daemoncfg
fun_tests     += test/util/presence test/util/prompter    test/util/executive


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
	install    -o $(CWUSER) -g $(CWGROUP) -m 0700 cwa cwcert            $(SBINDIR)
	install    -o $(CWUSER) -g $(CWGROUP) -m 0640 samples/cwa.conf      $(ETCDIR)
	install    -o root      -g root       -m 0644 man/cwa.1.gz          $(MANDIR)/man1
	install    -o root      -g root       -m 0644 man/cwa.conf.5.gz     $(MANDIR)/man5

install-master: install-base build-master manpages
	install -d -o $(CWUSER) -g $(CWGROUP) -m 0750 $(VARDIR)/run
	install -d -o $(CWUSER) -g $(CWGROUP) -m 0750 $(ETCDIR)/ssl/pending
	install -d -o $(CWUSER) -g $(CWGROUP) -m 0750 $(ETCDIR)/ssl/certs
	install    -o $(CWUSER) -g $(CWGROUP) -m 0700 policyd cwca          $(SBINDIR)
	install    -o $(CWUSER) -g $(CWGROUP) -m 0640 samples/policyd.conf  $(ETCDIR)
	install    -o $(CWUSER) -g $(CWGROUP) -m 0640 samples/manifest.pol  $(ETCDIR)
	install    -o root      -g root       -m 0644 man/policyd.1.gz      $(MANDIR)/man1
	install    -o root      -g root       -m 0644 man/policyd.conf.5.gz $(MANDIR)/man5
	install    -o root      -g root       -m 0644 man/res_*.5.gz        $(MANDIR)/man5

install-base:
	getent group $(CWGROUP) >/dev/null || groupadd -r $(CWGROUP)
	getent passwd $(CWUSER) >/dev/null || useradd -MNr -c "Clockwork" -g $(CWGROUP) -d $(HOMEDIR) $(CWUSER)
	install -d -o root      -g root       -m 0755 $(SBINDIR)
	install -d -o root      -g root       -m 0755 $(MANDIR)
	install -d -o root      -g root       -m 0755 $(MANDIR)/man1
	install -d -o root      -g root       -m 0755 $(MANDIR)/man5
	install -d -o $(CWUSER) -g $(CWGROUP) -m 0750 $(VARDIR)
	install -d -o $(CWUSER) -g $(CWGROUP) -m 0750 $(VARDIR)/db
	install -d -o $(CWUSER) -g $(CWGROUP) -m 0750 $(ETCDIR)
	install -d -o $(CWUSER) -g $(CWGROUP) -m 0750 $(ETCDIR)/ssl

config: config.dist
	./configure

debuggers: $(debug_bin)

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

policyd: $(core_o) $(policy_o) policyd.o $(parser_spec_o) $(parser_conf_o) proto.o server.o db.o
cwa:     $(core_o) $(policy_o) cwa.o     $(parser_conf_o) proto.o client.o db.o
cwcert:  $(core_o) $(policy_o) cwcert.o  $(parser_conf_o) proto.o client.o
cwca:    $(core_o) $(policy_o) cwca.o    $(parser_conf_o) server.o
polspec: $(core_o) $(policy_o) polspec.o $(parser_spec_o)
tplspec: $(core_o) $(policy_o) tplspec.o
sha1sum: sha1.o sha1sum.o mem.o log.o $(core_o) $(policy_o) $(parser_spec_o) $(parser_conf_o)


############################################################
# Debugging Tools (mainly for CW developers)

debug/service-manager: debug/service-manager.o managers/service.o log.o exec.o mem.o
debug/package-manager: debug/package-manager.o managers/package.o log.o exec.o mem.o


############################################################
# External Dependencies

externals:
	./ext/build-openssl 0.9.8k $(ROOT)/ext


############################################################
# Lex/YACC Parsers

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
	find . -name '*.gcda' 2>/dev/null | xargs rm -f
	test/setup.sh
	@echo; echo;
	test/run $(TEST) $(TESTS)
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
	find . -name '*.gcda' 2>/dev/null | xargs rm -f
	test/setup.sh
	@echo; echo;
	test/run $(TESTS)
	@echo; echo;
	$(LCOV) --capture -o $@.tmp
	$(LCOV) --remove $@.tmp $(no_lcov_c) > lcov.info
	rm -f $@.tmp
	rm -rf doc/coverage
	mkdir -p doc/coverage
	$(GENHTML) -o doc/coverage lcov.info

unit_tests: test/run

test/run: $(unit_test_o)

lcov.info: unit_tests functional_tests
	find . -name '*.gcda' 2>/dev/null | xargs rm -f
	test/setup.sh
	test/run
	test/functional/run
	$(LCOV) --capture -o $@.tmp
	$(LCOV) --remove $@.tmp $(no_lcov_c) > $@
	rm -f $@.tmp

test/sha1_files.h:
	$(MOG) sha1_tests


############################################################
# Functional Tests

functional_tests: $(fun_tests) cwcert

test/util/includer:    test/util/includer.o    $(core_o) $(parser_spec_o) $(policy_o)
test/util/factchecker: test/util/factchecker.o $(core_o) $(parser_spec_o) $(policy_o)
test/util/daemoncfg:   test/util/daemoncfg.o   $(core_o) $(parser_conf_o)
test/util/presence:    test/util/presence.o    $(core_o) $(parser_spec_o) $(policy_o)
test/util/prompter:    test/util/prompter.o    $(core_o)
test/util/executive:   test/util/executive.o   $(core_o)


############################################################
# Maintenance

tidy:
	find . -name '*.o' -o -name '*.gc??' 2>/dev/null | xargs rm -f
	rm -f lcov.info

clean: tidy
	rm -f $(COMPILED) test/run $(fun_tests) $(auto_c) $(auto_h) man/*.*.gz
	rm -f spec/*.output conf/*.output tpl/*.output Makefile.deps
	rm -rf $(APIDOC_ROOT)/* doc/coverage/*

dist: clean
	rm -rf doc/coverage ext/openssl ext/build/*
	rm -f config

fixme:
	find . -name '*.[ch15]' -not -path './ext/**' -not -path './man/tpl/*' 2>/dev/null | \
	  xargs grep -n FIXME: | sed -e 's/:[^:]*FIXME: /:/' -e 's/ *\*\///' | column -t -s :


############################################################
# Pattern Rules

# gzip man pages
%.gz: %
	gzip -c $^ > $@


Makefile.deps:
	gcc -MM *.c > Makefile.deps

-include Makefile.deps
