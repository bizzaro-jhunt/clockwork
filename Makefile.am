AUTOMAKE_OPTIONS = foreign subdir-objects

############################################################

# FIXME need a way to get these from ./configure
CW_VAR_DIR    = /var
CW_ETC_DIR    = /etc/clockwork
CW_DATA_DIR   = /var/lib/clockwork
CW_LIB_DIR    = /lib/clockwork
CW_CACHE_DIR  = /var/cache/clockwork

PKG_CFLAGS    = $(DEPS_CFLAGS) $(shell pkg-config --cflags augeas)
AM_CFLAGS     = -Wall $(PKG_CFLAGS)

LCOV    := lcov
GENHTML := genhtml

no_lcov_c := test/unit/**/* test/unit/* test/functional/*

AM_LFLAGS = --header-file --yylineno
AM_YFLAGS = -d

############################################################

CLEANFILES       = $(BUILT_SOURCES)

EXTRA_DIST       = share/db
EXTRA_DIST      += share/examples
EXTRA_DIST      += share/gather.d
EXTRA_DIST      += share/help
EXTRA_DIST      += share/vim
EXTRA_DIST      += var
EXTRA_DIST      += t

dist_man_MANS    = share/man/clockwork.7
dist_man_MANS   += share/man/cwa.1
dist_man_MANS   += share/man/cwa.conf.5
dist_man_MANS   += share/man/cwpol.1
dist_man_MANS   += share/man/policyd.1
dist_man_MANS   += share/man/policyd.conf.5
dist_man_MANS   += share/man/res_dir.5
dist_man_MANS   += share/man/res_exec.5
dist_man_MANS   += share/man/res_file.5
dist_man_MANS   += share/man/res_group.5
dist_man_MANS   += share/man/res_host.5
dist_man_MANS   += share/man/res_package.5
dist_man_MANS   += share/man/res_service.5
dist_man_MANS   += share/man/res_sysctl.5
dist_man_MANS   += share/man/res_user.5

############################################################

core_src     = src/mem.c src/sha1.c src/userdb.c src/cert.c src/prompt.c src/exec.c src/augcw.c
core_src    += src/mem.h src/sha1.h src/userdb.h src/cert.h src/prompt.h src/exec.h src/augcw.h
core_src    += src/policy.c src/resource.c src/resources.c src/job.c src/template.c
core_src    += src/policy.h src/resource.h src/resources.h src/job.h src/template.h
core_src    += src/gear/hash.c src/gear/log.c src/gear/string.c src/gear/pack.c src/gear/path.c
core_src    += src/gear/gear.h
core_src    += src/clockwork.h

client_src   = src/proto.c src/client.c
client_src  += src/proto.h src/client.h

server_src   = src/proto.c src/server.c
server_src  += src/proto.h src/server.h

manager_src  = src/managers/service.c src/managers/package.c
manager_src += src/managers/service.h src/managers/package.h

parser_spec_src  = src/spec/parser.c src/spec/parser.h src/spec/private.h
parser_spec_src += src/spec/lexer.l src/spec/lexer_impl.h
parser_spec_src += src/spec/grammar.y src/spec/grammar_impl.h

parser_conf_src  = src/conf/parser.c src/conf/parser.h src/conf/private.h
parser_conf_src += src/conf/lexer.l src/conf/lexer_impl.h
parser_conf_src += src/conf/grammar.y

parser_tpl_src   = src/tpl/parser.c src/tpl/parser.h src/tpl/private.h
parser_tpl_src  += src/tpl/lexer.l src/tpl/lexer_impl.h
parser_tpl_src  += src/tpl/grammar.y

core_src += $(parser_tpl_src)

test_obj  = src/mem.o src/sha1.o src/userdb.o src/cert.o src/prompt.o src/exec.o src/augcw.o
test_obj += src/policy.o src/resource.o src/resources.o src/job.o src/template.o
test_obj += src/gear/hash.o src/gear/log.o src/gear/string.o src/gear/pack.o src/gear/path.o
test_obj += src/managers/service.o src/managers/package.o
test_obj += src/spec/parser.o src/spec/lexer.o src/spec/grammar.o
test_obj += src/conf/parser.o src/conf/lexer.o src/conf/grammar.o
test_obj += src/tpl/parser.o src/tpl/lexer.o src/tpl/grammar.o
test_obj += src/proto.o

root_sbindir = "/sbin"
root_sbin_PROGRAMS = cwa cwcert policyd cwca cwpol
policyd_SOURCES = $(core_src) $(server_src) src/policyd.c $(parser_spec_src) $(parser_conf_src) src/db.c src/db.h
cwca_SOURCES    = $(core_src) $(server_src) src/cwca.c $(parser_conf_src)
cwa_SOURCES     = $(core_src) $(client_src) src/cwa.c $(manager_src) $(parser_conf_src) src/db.c src/db.h
cwcert_SOURCES  = $(core_src) $(client_src) src/cwcert.c $(parser_conf_src)
cwpol_SOURCES   = $(core_src) src/cwpol.c $(parser_spec_src)

############################################################

version:
	@echo $(VERSION)

distfile:
	@echo $(PACKAGE)-$(VERSION).tar.gz

manifest:
	@echo >&2 "nothin doin"

path-check:
	@echo "   CW_VAR_DIR = $(CW_VAR_DIR)"
	@echo "   CW_ETC_DIR = $(CW_ETC_DIR)"
	@echo "   CW_LIB_DIR = $(CW_LIB_DIR)"
	@echo "  CW_DATA_DIR = $(CW_DATA_DIR)"
	@echo " CW_CACHE_DIR = $(CW_CACHE_DIR)"

install-data-hook:
	$(INSTALL) -d -m 0755 $(DESTDIR)$(CW_VAR_DIR)/run
	$(INSTALL) -d -m 0755 $(DESTDIR)$(CW_VAR_DIR)/lock
	$(INSTALL) -d -m 0755 $(DESTDIR)$(CW_CACHE_DIR)
	$(INSTALL) -d -m 0755 $(DESTDIR)$(CW_DATA_DIR)
	$(INSTALL) -d -m 0755 $(DESTDIR)$(datadir)/vim/addons/syntax
	$(INSTALL) -d -m 0755 $(DESTDIR)$(datadir)/vim/addons/ftdetect
	$(INSTALL) -d -m 0755 $(DESTDIR)$(datadir)/vim/registry
	$(INSTALL)    -m 0644 share/vim/syntax/*.vim   $(DESTDIR)$(datadir)/vim/addons/syntax
	$(INSTALL)    -m 0644 share/vim/ftdetect/*.vim $(DESTDIR)$(datadir)/vim/addons/ftdetect
	$(INSTALL)    -m 0644 share/vim/registry/vim-clockwork.yaml $(DESTDIR)$(datadir)/vim/registry
	$(INSTALL) -d -m 0755 $(DESTDIR)$(CW_LIB_DIR)/augeas/lenses
	$(INSTALL)    -m 0644 var/augeas/lenses/* $(DESTDIR)$(CW_LIB_DIR)/augeas/lenses
	$(INSTALL) -d -m 0755 $(DESTDIR)$(pkgdatadir)/db
	$(INSTALL)    -m 0644 share/db/*.sql $(DESTDIR)$(pkgdatadir)/db
	$(INSTALL) -d -m 0755 $(DESTDIR)$(CW_LIB_DIR)/help
	$(INSTALL)    -m 0644 share/help/* $(DESTDIR)$(CW_LIB_DIR)/help
	$(INSTALL) -d -m 0755 $(DESTDIR)$(CW_ETC_DIR)
	$(INSTALL) -d -m 0755 $(DESTDIR)$(CW_ETC_DIR)/ssl/pending
	$(INSTALL) -d -m 0755 $(DESTDIR)$(CW_ETC_DIR)/ssl/signed
	$(INSTALL)    -m 0644 share/examples/cwa.conf $(DESTDIR)$(CW_ETC_DIR)/cwa.conf
	$(INSTALL)    -m 0644 share/examples/policyd.conf $(DESTDIR)$(CW_ETC_DIR)/policyd.conf
	$(INSTALL)    -m 0644 share/examples/manifest.pol $(DESTDIR)$(CW_ETC_DIR)/manifest.pol
	$(INSTALL) -d -m 0755 $(DESTDIR)$(CW_LIB_DIR)/gather.d
	$(INSTALL)    -m 0755 share/gather.d/* $(DESTDIR)$(CW_LIB_DIR)/gather.d

############################################################
# Lex/YACC Parsers

parserdefs: src/spec/lexer.c src/spec/grammar.c src/tpl/lexer.c src/tpl/grammar.c src/conf/lexer.c src/conf/grammar.c

src/spec/lexer.c: src/spec/lexer.l src/spec/grammar.h src/spec/lexer_impl.h src/spec/parser.h src/spec/private.h
	$(LEXCOMPILE) --outfile=$@ $<

src/spec/grammar.c: src/spec/grammar.y src/spec/grammar_impl.h src/spec/parser.c src/spec/parser.h src/spec/private.h
	$(YACCCOMPILE) --output-file=src/spec/grammar.c $<

src/tpl/lexer.c: src/tpl/lexer.l src/tpl/grammar.h src/tpl/lexer_impl.h src/tpl/parser.h src/tpl/private.h
	$(LEXCOMPILE) --outfile=$@ $<

src/tpl/grammar.c: src/tpl/grammar.y src/tpl/parser.c src/tpl/parser.h src/tpl/private.h
	$(YACCCOMPILE) -p yytpl --output-file=src/tpl/grammar.c $<

src/conf/lexer.c: src/conf/lexer.l src/conf/grammar.h src/conf/lexer_impl.h src/conf/private.h
	$(LEXCOMPILE) --outfile=$@ $<

src/conf/grammar.c: src/conf/grammar.y src/conf/parser.c src/conf/parser.h src/conf/private.h
	$(YACCCOMPILE) -p yyconf --output-file=src/conf/grammar.c $<

cov: clean-cov
	$(LCOV) --capture -o $@.info.tmp
	$(LCOV) --remove $@.info.tmp $(no_lcov_c) > lcov.info
	rm -f $@.info.tmp
	rm -rf coverage
	$(GENHTML) -o coverage lcov.info

clean-cov:
	find . -name '*.gcda' 2>/dev/null | xargs rm -f

TEST_SOURCE := $(patsubst %.c,%.t,$(shell find t -maxdepth 1 -name '*.c' | sort))
tests: $(TEST_SOURCE)
	prove -e '' $(TEST_SOURCE)
test: t/helpers/proto tests

t/%.t.o: t/%.c config.h
	$(CC) -I. -Isrc $(CFLAGS) $(PKG_CFLAGS) $(LDFLAGS) -c $< -o $@
t/%.t: t/%.t.o $(test_obj)
	$(CC) $(LDFLAGS) $+ $(LIBS) -o $@

t/helpers/%.o: t/helpers/%.c config.h
	$(CC) -I. -Isrc $(CFLAGS) $(PKG_CFLAGS) $(LDFLAGS) -c $< -o $@
t/helpers/proto: t/helpers/proto.o $(test_obj)
	$(CC) $(LDFLAGS) $+ $(LIBS) -o $@
