AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_AMFLAGS=-I build

AM_TESTS_ENVIRONMENT = PERL5LIB='$(srcdir)'; export PERL5LIB;

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
AM_CFLAGS += -DCW_GATHER_DIR=\"$(gatherdir)\"
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

gatherdir=$(pkglibdir)/gather.d
gather_SCRIPTS  = gather.d/core
gather_SCRIPTS += gather.d/time

############################################################

CTAP_TESTS =
CTAP_TESTS += t/00-bits
CTAP_TESTS += t/01-string
CTAP_TESTS += t/02-pdu
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
CTAP_TESTS += t/70-res_symlink

PERL_TESTS  =
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
PERL_TESTS += t/policy/08-sysctl.t
PERL_TESTS += t/policy/09-exec.t
PERL_TESTS += t/policy/10-symlink.t
PERL_TESTS += t/policy/30-deps.t
PERL_TESTS += t/policy/99-fail.t

PERL_TESTS += t/bdfa/01-basics.t

SHELL_TESTS  =
SHELL_TESTS += t/memcheck/00-pn-usage.t
SHELL_TESTS += t/memcheck/01-pn-basics.t
SHELL_TESTS += t/memcheck/02-pn-fs.t
SHELL_TESTS += t/memcheck/03-pn-users.t
SHELL_TESTS += t/memcheck/04-pn-groups.t
SHELL_TESTS += t/memcheck/05-pn-exec.t
SHELL_TESTS += t/memcheck/06-pn-augeas.t
SHELL_TESTS += t/memcheck/21-cwcc.t
SHELL_TESTS += t/memcheck/22-units.t
SHELL_TESTS += t/memcheck/31-cogd.t
SHELL_TESTS += t/memcheck/32-clockd.t

TESTS          = $(CTAP_TESTS) $(PERL_TESTS) $(SHELL_TESTS)
BUILT_TESTS    = $(CTAP_TESTS)
check_PROGRAMS = $(CTAP_TESTS) TEST_cogd TEST_clockd
check_DATA     = t/tmp/check-data-install.stamp
t/tmp/check-data-install.stamp:
	mkdir -p t/tmp
	cp -a $(srcdir)/t/data t/tmp
	cp -a $(srcdir)/augeas t/tmp
	find t/tmp -type d ! -perm -200 -exec chmod u+w {} ';'
	touch $@

TEST_EXTRAS  = t/data t/common.pm
TEST_EXTRAS += $(PERL_TESTS)
TEST_EXTRAS += t/memcheck/verify
TEST_EXTRAS += $(SHELL_TESTS)

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

TEST_cogd_SOURCES        = t/test-cogd.c $(core_src) $(parser_spec_src) $(pendulum_src)
TEST_clockd_SOURCES      = t/test-clockd.c $(core_src) $(parser_spec_src)
t_00_bits_SOURCES        = t/00-bits.c          $(test_source)
t_01_string_SOURCES      = t/01-string.c        $(test_source)
t_02_pdu_SOURCES         = t/02-pdu.c           $(test_source)
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
t_70_res_symlink_SOURCES = t/70-res_symlink.c   $(test_source)

CLEANFILES       = $(BUILT_SOURCES)
CLEANFILES      += *.tmp

EXTRA_DIST       = HACKING autogen.sh
EXTRA_DIST      += augeas gather.d
EXTRA_DIST      += examples extras
EXTRA_DIST      += help man
EXTRA_DIST      += $(TEST_EXTRAS)

dist_man_MANS    = man/clockwork.7
dist_man_MANS   += man/cwpol.1
dist_man_MANS   += man/cwtool.1
dist_man_MANS   += man/cwcc.1
dist_man_MANS   += man/cogd.1 man/cogd.conf.5
dist_man_MANS   += man/clockd.1 man/clockd.conf.5
dist_man_MANS   += man/fact.1
dist_man_MANS   += man/pn.1
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
sbin_PROGRAMS = cwpol cwcc clockd cogd
cwpol_SOURCES   = src/cwpol.c  $(core_src) $(parser_spec_src)
cwcc_SOURCES    = src/cwcc.c $(core_src) $(parser_spec_src)
clockd_SOURCES  = src/clockd.c $(core_src) $(parser_spec_src)
cogd_SOURCES    = src/cogd.c   $(core_src) $(parser_spec_src) $(pendulum_src)

dist_bin_SCRIPTS = cwtool cw-template-erb

bin_PROGRAMS = bdfa fact pn
bdfa_SOURCES = src/bdfa.c src/cw.h src/cw.c
fact_SOURCES = src/fact.c $(core_src)
pn_SOURCES   = src/pn.c $(core_src) $(pendulum_src)

############################################################

version:
	@echo $(VERSION)

distfile: dist
	@echo $(PACKAGE)-$(VERSION).tar.gz

manifest:
	@echo >&2 "nothin doin"

test: check
test-data:
	rm -f t/tmp/check-data-install.stamp
	make t/tmp/check-data-install.stamp

############################################################
# Lex/YACC Parsers

src/spec/lexer.c: src/spec/lexer.l src/spec/grammar.h src/spec/lexer_impl.h src/spec/parser.h src/spec/private.h
	$(LEXCOMPILE) --outfile=$@ $<

src/spec/grammar.c: src/spec/grammar.y src/spec/grammar_impl.h src/spec/parser.c src/spec/parser.h src/spec/private.h
	$(YACCCOMPILE) --output-file=src/spec/grammar.c $<

.PHONY: coverage-clean coverage-report coverage clean-local-check clean-local-cov
clean-local: clean-local-check
clean-local-check:
	rm -fr t/tmp/*
	rm -f check-data-install.stamp

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

clean-local-cov:
	@make coverage-clean

check:
	@make coverage

else
coverage:
	@echo >&2 "nothin doin"
clean-local-cov:
endif
