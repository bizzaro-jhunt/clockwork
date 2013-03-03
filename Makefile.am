AUTOMAKE_OPTIONS = foreign subdir-objects

############################################################

# FIXME need a way to get these from ./configure
CW_VARDIR    = /var
CW_ETCDIR    = /etc/clockwork
CW_DATADIR   = /var/lib/clockwork
CW_LIBDIR    = /lib/clockwork
CW_CACHEDIR  = /var/cache/clockwork

CFLAGS  = -Wall $(DEPS_CFLAGS) $(shell pkg-config --cflags augeas)
CFLAGS += -DCW_VARDIR='"$(CW_VARDIR)"'
CFLAGS += -DCW_ETCDIR='"$(CW_ETCDIR)"'
CFLAGS += -DCW_DATADIR='"$(CW_DATADIR)"'
CFLAGS += -DCW_LIBDIR='"$(CW_LIBDIR)"'
CFLAGS += -DCW_CACHEDIR='"$(CW_CACHEDIR)"'

LCOV    := lcov
GENHTML := genhtml

no_lcov_c := test/unit/**/* test/unit/* test/functional/*

AM_LFLAGS = --header-file --yylineno
AM_YFLAGS = -d

############################################################

BUILT_SOURCES    = src/spec/lexer.c src/spec/grammar.c src/spec/grammar.h
BUILT_SOURCES   += src/conf/lexer.c src/conf/grammar.c src/conf/grammar.h
BUILT_SOURCES   += src/tpl/lexer.c  src/tpl/grammar.c  src/tpl/grammar.h

CLEANFILES       = $(BUILT_SOURCES)

EXTRA_DIST       = share/db
EXTRA_DIST      += share/examples
EXTRA_DIST      += share/gather.d
EXTRA_DIST      += share/help
EXTRA_DIST      += share/vim
EXTRA_DIST      += var

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
	@echo "   CW_VARDIR = $(CW_VARDIR)"
	@echo "   CW_ETCDIR = $(CW_ETCDIR)"
	@echo "   CW_LIBDIR = $(CW_LIBDIR)"
	@echo "  CW_DATADIR = $(CW_DATADIR)"
	@echo " CW_CACHEDIR = $(CW_CACHEDIR)"

install-data-hook:
	$(INSTALL) -d -m 0755 $(DESTDIR)$(datadir)/vim/addons/syntax
	$(INSTALL) -d -m 0755 $(DESTDIR)$(datadir)/vim/addons/ftdetect
	$(INSTALL) -d -m 0755 $(DESTDIR)$(datadir)/vim/registry
	$(INSTALL)    -m 0644 share/vim/syntax/*.vim   $(DESTDIR)$(datadir)/vim/addons/syntax
	$(INSTALL)    -m 0644 share/vim/ftdetect/*.vim $(DESTDIR)$(datadir)/vim/addons/ftdetect
	$(INSTALL)    -m 0644 share/vim/registry/vim-clockwork.yaml $(DESTDIR)$(datadir)/vim/registry
	$(INSTALL) -d -m 0755 $(DESTDIR)$(CW_LIBDIR)/augeas/lenses
	$(INSTALL)    -m 0644 var/augeas/lenses/* $(DESTDIR)$(CW_LIBDIR)/augeas/lenses
	$(INSTALL) -d -m 0755 $(DESTDIR)$(pkgdatadir)/db
	$(INSTALL)    -m 0644 share/db/*.sql $(DESTDIR)$(pkgdatadir)/db
	$(INSTALL) -d -m 0755 $(DESTDIR)$(CW_LIBDIR)/help
	$(INSTALL)    -m 0644 share/help/* $(DESTDIR)$(CW_LIBDIR)/help
	$(INSTALL) -d -m 0755 $(DESTDIR)$(CW_ETCDIR)
	$(INSTALL) -d -m 0755 $(DESTDIR)$(CW_ETCDIR)/ssl/pending
	$(INSTALL) -d -m 0755 $(DESTDIR)$(CW_ETCDIR)/ssl/signed
	$(INSTALL)    -m 0644 share/examples/cwa.conf $(DESTDIR)$(CW_ETCDIR)/cwa.conf
	$(INSTALL)    -m 0644 share/examples/policyd.conf $(DESTDIR)$(CW_ETCDIR)/policyd.conf
	$(INSTALL)    -m 0644 share/examples/manifest.pol $(DESTDIR)$(CW_ETCDIR)/manifest.pol
	$(INSTALL) -d -m 0755 $(DESTDIR)$(CW_LIBDIR)/gather.d
	$(INSTALL)    -m 0755 share/gather.d/* $(DESTDIR)$(CW_LIBDIR)/gather.d

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
