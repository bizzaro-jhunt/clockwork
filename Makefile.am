AUTOMAKE_OPTIONS = foreign subdir-objects

BUILT_SOURCES  = spec/lexer.c spec/grammar.c spec/grammar.h
BUILT_SOURCES += conf/lexer.c conf/grammar.c conf/grammar.h
BUILT_SOURCES += tpl/lexer.c  tpl/grammar.c  tpl/grammar.h

############################################################

CFLAGS  := -Wall -DVERSION=\"$(VERSION)\" -DPROG="\"$(PACKAGE)\"" $(DEPS_CFLAGS) $(shell pkg-config --cflags augeas)
LDFLAGS := -lopenssl -laugeas -lsqlite3 -lcrypto -lpthread -lreadline $(DEPS_LFLAGS) $(shell pkg-config --libs augeas)

LFLAGS  := --header-file --yylineno
YFLAGS  := -Wall --token-table --defines

LCOV    := lcov
GENHTML := genhtml

no_lcov_c := test/unit/**/* test/unit/* test/functional/*

############################################################

core_src = mem.c sha1.c userdb.c cert.c prompt.c exec.c augcw.c
manager_src = managers/service.c managers/package.c
policy_src = policy.c resource.c resources.c job.c template.c
gear_c = gear/hash.c gear/log.c gear/string.c gear/pack.c gear/path.c

parser_spec_src = spec/lexer.l spec/grammar.y spec/parser.c
parser_conf_src = conf/lexer.l conf/grammar.y conf/parser.c
parser_tpl_src = tpl/lexer.l tpl/grammar.y tpl/parser.c

bin_PROGRAMS = cwa cwcert policyd cwca cwpol
policyd_SOURCES = $(core_src) $(policy_src) policyd.c $(parser_spec_src) $(parser_conf_src) proto.c server.c db.c
cwa_SOURCES = $(core_src) $(gear_src) $(policy_src) cwa.c $(parser_conf_src) proto.c client.c db.c
cwcert_SOURCES = $(core_src) $(gear_src) $(policy_src) cwcert.c $(parser_conf_src) proto.c client.c
cwca_SOURCES = $(core_src) $(policy_src) cwca.c $(parser_conf_src) server.c
cwpol_SOURCES = $(core_src) $(policy_src) cwpol.c $(parser_spec_src)

############################################################

manifest:
	@echo >&2 "nothin doin"

############################################################
# Lex/YACC Parsers

parserdefs: spec/lexer.c spec/grammar.c tpl/lexer.c tpl/grammar.c conf/lexer.c conf/grammar.c

spec/lexer.c: spec/lexer.l spec/grammar.h spec/lexer_impl.c spec/parser.h spec/private.h
	$(LEX) $(LFLAGS) --outfile=$@ $<

spec/grammar.c spec/grammar.h: spec/grammar.y spec/grammar_impl.c spec/parser.c spec/parser.h spec/private.h
	$(YACC) $(YFLAGS) --output-file=spec/grammar.c $<

tpl/lexer.c: tpl/lexer.l tpl/grammar.h tpl/lexer_impl.c tpl/parser.h tpl/private.h
	$(LEX) $(LFLAGS) --outfile=$@ $<

tpl/grammar.c tpl/grammar.h: tpl/grammar.y tpl/parser.c tpl/parser.h tpl/private.h
	$(YACC) $(YFLAGS) -p yytpl --output-file=tpl/grammar.c $<

conf/lexer.c: conf/lexer.l conf/grammar.h conf/lexer_impl.c conf/private.h
	$(LEX) $(LFLAGS) --outfile=$@ $<

conf/grammar.c conf/grammar.h: conf/grammar.y conf/parser.c conf/parser.h conf/private.h
	$(YACC) $(YFLAGS) -p yyconf --output-file=conf/grammar.c $<

cov: clean-cov
	$(LCOV) --capture -o $@.info.tmp
	$(LCOV) --remove $@.info.tmp $(no_lcov_c) > lcov.info
	rm -f $@.info.tmp
	rm -rf coverage
	$(GENHTML) -o coverage lcov.info

clean-cov:
	find . -name '*.gcda' 2>/dev/null | xargs rm -f
