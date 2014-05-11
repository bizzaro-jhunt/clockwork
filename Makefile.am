AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_AMFLAGS=-I build

############################################################

pkgsysconfdir = $(sysconfdir)/$(PACKAGE)
cachedir = $(localstatedir)/cache
pkgcachedir = $(cachedir)/$(PACKAGE)
sharedir = $(datarootdir)/share
pkgsharedir = $(sharedir)/$(PACKAGE)

AM_CFLAGS  = -Wall $(DEPS_CFLAGS)
# FIXME: use PKG_CHECK_MODULES(AUGEAS, augeas)...
AM_CFLAGS += $(shell pkg-config --cflags augeas)
AM_CFLAGS += -DTEST_DATA=\"$(srcdir)/t/data\"
AM_CFLAGS += -DCW_CACHE_DIR=\"$(pkgcachedir)\"
AM_CFLAGS += @GCOV_CFLAGS@

AM_LFLAGS = --header-file --yylineno
AM_YFLAGS = -d

LDADD = -lctap -lzmq


############################################################

helpdir=$(pkglibdir)
AM_CFLAGS += -DHELP_FILES_DIR=\"$(helpdir)/help\"
nobase_help_DATA  = help/about.help
nobase_help_DATA += help/clear.help
nobase_help_DATA += help/cli.help
nobase_help_DATA += help/fact.help
nobase_help_DATA += help/help.help
nobase_help_DATA += help/load.help
nobase_help_DATA += help/main
nobase_help_DATA += help/quit.help
nobase_help_DATA += help/show.help
nobase_help_DATA += help/use.help

augdir=$(pkglibdir)
nobase_aug_DATA  = augeas/lenses/hosts.aug
nobase_aug_DATA += augeas/lenses/util.aug

scriptsdir=$(pkglibdir)
debscriptsdir=$(scriptsdir)/bin/debian
debscripts_SCRIPTS  = scripts/debian/cw-pkg
debscripts_SCRIPTS += scripts/debian/cw-svc

rhscriptsdir=$(scriptsdir)/bin/redhat
rhscripts_SCRIPTS  = scripts/redhat/cw-pkg
rhscripts_SCRIPTS += scripts/redhat/cw-svc

defaultscriptsdir=$(scriptsdir)/bin/unspec
defaultscripts_SCRIPTS  = scripts/unspec/cw-pkg
defaultscripts_SCRIPTS += scripts/unspec/cw-svc

gatherdir=$(pkglibdir)/gather.d
gather_SCRIPTS  = gather.d/core
gather_SCRIPTS += gather.d/time

############################################################

CTAP_TESTS =
CTAP_TESTS += t/00-bits
CTAP_TESTS += t/01-string
CTAP_TESTS += t/04-hash
CTAP_TESTS += t/05-mem
CTAP_TESTS += t/07-sha1
CTAP_TESTS += t/08-list
CTAP_TESTS += t/10-facts
CTAP_TESTS += t/15-path
CTAP_TESTS += t/20-stree
CTAP_TESTS += t/25-resource
CTAP_TESTS += t/30-policy
CTAP_TESTS += t/40-userdb
CTAP_TESTS += t/61-res_user
CTAP_TESTS += t/62-res_file
CTAP_TESTS += t/63-res_group
CTAP_TESTS += t/64-res_dir
CTAP_TESTS += t/65-res_host
CTAP_TESTS += t/66-res_sysctl
CTAP_TESTS += t/67-res_service
CTAP_TESTS += t/68-res_package
CTAP_TESTS += t/69-res_exec

PERL_TESTS = 
PERL_TESTS += t/pendulum/01-basics.t
PERL_TESTS += t/pendulum/10-fs.t
PERL_TESTS += t/pendulum/11-users.t
PERL_TESTS += t/pendulum/12-groups.t
PERL_TESTS += t/pendulum/13-exec.t
PERL_TESTS += t/pendulum/14-augeas.t

PERL_TESTS += t/policy/01-files.t
PERL_TESTS += t/policy/02-users.t
PERL_TESTS += t/policy/02-users.t
PERL_TESTS += t/policy/03-groups.t
PERL_TESTS += t/policy/04-dirs.t
PERL_TESTS += t/policy/05-services.t
PERL_TESTS += t/policy/06-packages.t
PERL_TESTS += t/policy/07-hosts.t

TESTS          = $(CTAP_TESTS) $(PERL_TESTS)
BUILT_TESTS    = $(CTAP_TESTS)
check_PROGRAMS = $(CTAP_TESTS)

test_source  = t/test.h
test_source += src/cw.h src/cw.c
test_source += src/sha1.h       src/sha1.c
test_source += src/userdb.h     src/userdb.c
test_source += src/policy.h     src/policy.c
test_source += src/resource.h   src/resource.c
test_source += src/resources.h  src/resources.c

test_source += src/spec/grammar.y
test_source += src/spec/lexer.l
test_source += src/spec/parser.h
test_source += src/spec/parser.c
test_source += src/spec/private.h

test_source += src/gear/gear.h
test_source += src/gear/path.c
test_source += src/gear/string.c

t_00_bits_SOURCES        = t/00-bits.c          $(test_source)
t_01_string_SOURCES      = t/01-string.c        $(test_source)
t_04_hash_SOURCES        = t/04-hash.c          $(test_source)
t_05_mem_SOURCES         = t/05-mem.c           $(test_source)
t_07_sha1_SOURCES        = t/07-sha1.c          $(test_source)
t_08_list_SOURCES        = t/08-list.c          $(test_source)
t_10_facts_SOURCES       = t/10-facts.c         $(test_source)
t_15_path_SOURCES        = t/15-path.c          $(test_source)
t_20_stree_SOURCES       = t/20-stree.c         $(test_source)
t_25_resource_SOURCES    = t/25-resource.c      $(test_source)
t_30_policy_SOURCES      = t/30-policy.c        $(test_source)
t_40_userdb_SOURCES      = t/40-userdb.c        $(test_source)
t_61_res_user_SOURCES    = t/61-res_user.c      $(test_source)
t_62_res_file_SOURCES    = t/62-res_file.c      $(test_source)
t_63_res_group_SOURCES   = t/63-res_group.c     $(test_source)
t_64_res_dir_SOURCES     = t/64-res_dir.c       $(test_source)
t_65_res_host_SOURCES    = t/65-res_host.c      $(test_source)
t_66_res_sysctl_SOURCES  = t/66-res_sysctl.c    $(test_source)
t_67_res_service_SOURCES = t/67-res_service.c   $(test_source)
t_68_res_package_SOURCES = t/68-res_package.c   $(test_source)
t_69_res_exec_SOURCES    = t/69-res_exec.c      $(test_source)

CLEANFILES       = $(BUILT_SOURCES)
CLEANFILES      += *.tmp
CLEANFILES      += t/tmp/*

EXTRA_DIST       = HACKING autogen.sh
EXTRA_DIST      += augeas gather.d scripts
EXTRA_DIST      += examples extras
EXTRA_DIST      += help man
EXTRA_DIST      += t/data

dist_man_MANS    = man/clockwork.7
dist_man_MANS   += man/cwpol.1
dist_man_MANS   += man/res_dir.5
dist_man_MANS   += man/res_exec.5
dist_man_MANS   += man/res_file.5
dist_man_MANS   += man/res_group.5
dist_man_MANS   += man/res_host.5
dist_man_MANS   += man/res_package.5
dist_man_MANS   += man/res_service.5
dist_man_MANS   += man/res_sysctl.5
dist_man_MANS   += man/res_user.5

############################################################

core_src     = src/cw.h src/cw.c
core_src    += src/sha1.c src/userdb.c
core_src    += src/sha1.h src/userdb.h
core_src    += src/policy.c src/resource.c src/resources.c
core_src    += src/policy.h src/resource.h src/resources.h
core_src    += src/gear/string.c src/gear/path.c
core_src    += src/gear/gear.h
core_src    += src/clockwork.h

pendulum_src     = src/pendulum.h       src/pendulum.c
pendulum_src    += src/pendulum_funcs.h src/pendulum_funcs.c

parser_spec_src  = src/spec/parser.c src/spec/parser.h src/spec/private.h
parser_spec_src += src/spec/lexer.l src/spec/lexer_impl.h
parser_spec_src += src/spec/grammar.y src/spec/grammar_impl.h

sbindir = "$(prefix)/sbin"
sbin_PROGRAMS = cwpol pn cwspec clockd cogd
cwpol_SOURCES   = $(core_src) src/cwpol.c $(parser_spec_src)
pn_SOURCES      = src/pn.c $(pendulum_src)
cwspec_SOURCES  = src/cwspec.c $(core_src) $(parser_spec_src)
clockd_SOURCES  = src/clockd.c $(core_src) $(parser_spec_src)
cogd_SOURCES    = src/cogd.c $(core_src) $(parser_spec_src) $(pendulum_src)

############################################################

version:
	@echo $(VERSION)

distfile: dist
	@echo $(PACKAGE)-$(VERSION).tar.gz

manifest:
	@echo >&2 "nothin doin"

test: check

############################################################
# Lex/YACC Parsers

src/spec/lexer.c: src/spec/lexer.l src/spec/grammar.h src/spec/lexer_impl.h src/spec/parser.h src/spec/private.h
	$(LEXCOMPILE) --outfile=$@ $<

src/spec/grammar.c: src/spec/grammar.y src/spec/grammar_impl.h src/spec/parser.c src/spec/parser.h src/spec/private.h
	$(YACCCOMPILE) --output-file=src/spec/grammar.c $<

if GCOV_ENABLED
coverage-clean:
	@rm -fr coverage
	@find . -name "*.gcda" -exec rm {} \;
	@lcov --directory . --zerocounters

coverage-report:
	@mkdir -p coverage
	@lcov --compat-libtool --directory . --base-directory . --capture --output-file coverage/app.info
	@genhtml -o coverage/ coverage/app.info

coverage:
	@make coverage-report

clean-local:
	@make coverage-clean

check:
	@make coverage

.PHONY: coverage-clean coverage-report coverage
else
coverage:
	@echo >&2 "nothin doin"
.PHONY: coverage
endif
