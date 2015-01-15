#
# Makefile.am - Clockwork Automake File
#

AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_AMFLAGS  = -I build

AM_TESTS_ENVIRONMENT = PERL5LIB='$(srcdir)'; export PERL5LIB;

############################################################

pkgsysconfdir = $(sysconfdir)/$(PACKAGE)
cachedir      = $(localstatedir)/cache
pkgcachedir   = $(cachedir)/$(PACKAGE)
sharedir      = $(datarootdir)/share
pkgsharedir   = $(sharedir)/$(PACKAGE)

AM_CFLAGS  = -Wall $(DEPS_CFLAGS)
AM_CFLAGS += -DCW_CACHE_DIR=\"$(pkgcachedir)\"
AM_CFLAGS += -DCW_GATHER_DIR=\"$(gatherdir)\"
AM_CFLAGS += -DCW_CLI_DIR=\"$(clidir)\"
AM_CFLAGS += -DCW_SYSCONF_DIR=\"$(pkgsysconfdir)\"
AM_CFLAGS += -DPACKAGE_LIBDIR=\"$(pkglibdir)\"
AM_CFLAGS += @AUGEAS_CFLAGS@
AM_CFLAGS += @GCOV_CFLAGS@

AM_LFLAGS = --header-file --yylineno
AM_YFLAGS = -d

LDADD = -lctap -lzmq -lsodium -lpam -luuid -lvigor

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

stdlibdir=$(pkglibdir)
nobase_stdlib_DATA  = stdlib.pn

############################################################

CTAP_TESTS  =
CTAP_TESTS += t/05-mem
CTAP_TESTS += t/07-sha1
CTAP_TESTS += t/10-facts
CTAP_TESTS += t/11-cmd
CTAP_TESTS += t/12-filter
CTAP_TESTS += t/13-acl
CTAP_TESTS += t/20-stree
CTAP_TESTS += t/25-resource
CTAP_TESTS += t/30-policy
CTAP_TESTS += t/41-authdb
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
CTAP_TESTS += t/90-meshd

PERL_TESTS  =

PERL_TESTS += t/80-pendulum.t
PERL_TESTS += t/81-gencode.t
PERL_TESTS += t/82-security.t
PERL_TESTS += t/83-bdfa.t

PERL_TESTS += t/cmd/01-show.t

PERL_TESTS += t/cw/01-ps.t
PERL_TESTS += t/cw/02-lock.t

SHELL_TESTS  =
SHELL_TESTS += t/memcheck/21-cwcc.t
SHELL_TESTS += t/memcheck/22-units.t
SHELL_TESTS += t/memcheck/31-cogd.t
SHELL_TESTS += t/memcheck/32-clockd.t
SHELL_TESTS += t/memcheck/33-cwtrust.t

TESTS          = $(CTAP_TESTS) $(PERL_TESTS) $(SHELL_TESTS)
BUILT_TESTS    = $(CTAP_TESTS)
check_PROGRAMS = $(CTAP_TESTS) TEST_cogd TEST_clockd TEST_ps TEST_lock TEST_meshd TEST_cmd

TEST_EXTRAS  = t/common.pm
TEST_EXTRAS += $(PERL_TESTS)
TEST_EXTRAS += t/memcheck/verify
TEST_EXTRAS += $(SHELL_TESTS)

test_source  = t/test.h
test_source += src/opcodes.h
test_source += src/base.h       src/base.c
test_source += src/mesh.h       src/mesh.c
test_source += src/sha1.h       src/sha1.c
test_source += src/userdb.h     src/userdb.c
test_source += src/authdb.h     src/authdb.c
test_source += src/policy.h     src/policy.c
test_source += src/resource.h   src/resource.c
test_source += src/resources.h  src/resources.c
test_source += src/vm.h         src/vm.c

test_source += src/spec/grammar.y
test_source += src/spec/lexer.l
test_source += src/spec/parser.h
test_source += src/spec/parser.c
test_source += src/spec/private.h

TEST_cogd_SOURCES        = t/test-cogd.c   $(core_src) $(parser_spec_src)
TEST_clockd_SOURCES      = t/test-clockd.c $(core_src) $(parser_spec_src)
TEST_meshd_SOURCES       = t/test-meshd.c  $(core_src) $(parser_spec_src)
TEST_ps_SOURCES          = t/test-ps.c     $(core_src)
TEST_lock_SOURCES        = t/test-lock.c   $(core_src)
TEST_cmd_SOURCES         = t/test-cmd.c    $(core_src)

t_05_mem_SOURCES         = t/05-mem.c           $(test_source)
t_07_sha1_SOURCES        = t/07-sha1.c          $(test_source)
t_10_facts_SOURCES       = t/10-facts.c         $(test_source)
t_11_cmd_SOURCES         = t/11-cmd.c           $(test_source)
t_12_filter_SOURCES      = t/12-filter.c        $(test_source)
t_13_acl_SOURCES         = t/13-acl.c           $(test_source)
t_20_stree_SOURCES       = t/20-stree.c         $(test_source)
t_25_resource_SOURCES    = t/25-resource.c      $(test_source)
t_30_policy_SOURCES      = t/30-policy.c        $(test_source)
t_41_authdb_SOURCES      = t/41-authdb.c        $(test_source)
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
t_90_meshd_SOURCES       = t/90-meshd.c         $(test_source)

CLEANFILES       = $(BUILT_SOURCES)

EXTRA_DIST       = HACKING CHANGELOG
EXTRA_DIST      += opcodes.yml gencode
EXTRA_DIST      += bootstrap
EXTRA_DIST      += augeas gather.d
EXTRA_DIST      += examples extras
EXTRA_DIST      += help man
EXTRA_DIST      += $(TEST_EXTRAS)

dist_man_MANS    =
dist_man_MANS   += man/clockd.8
dist_man_MANS   += man/clockd.conf.5
dist_man_MANS   += man/clockwork.7
dist_man_MANS   += man/cogd.8
dist_man_MANS   += man/cogd.conf.5
dist_man_MANS   += man/cw.conf.5
dist_man_MANS   += man/cw-cc.1
dist_man_MANS   += man/cw-cert.1
dist_man_MANS   += man/cw-fact.1
dist_man_MANS   += man/cw-help.1
dist_man_MANS   += man/cw-localsys.8
dist_man_MANS   += man/cw-mesh.1
dist_man_MANS   += man/cw-shell.1
dist_man_MANS   += man/cw-template-erb.8
dist_man_MANS   += man/cw-trust.1
dist_man_MANS   += man/cw-uuid.1
dist_man_MANS   += man/mesh.7
dist_man_MANS   += man/meshd.8
dist_man_MANS   += man/meshd.conf.5
dist_man_MANS   += man/pn.1
dist_man_MANS   += man/res_dir.5
dist_man_MANS   += man/res_exec.5
dist_man_MANS   += man/res_file.5
dist_man_MANS   += man/res_group.5
dist_man_MANS   += man/res_host.5
dist_man_MANS   += man/res_package.5
dist_man_MANS   += man/res_service.5
dist_man_MANS   += man/res_symlink.5
dist_man_MANS   += man/res_sysctl.5
dist_man_MANS   += man/res_user.5

man/%.1: man/%.1.pod
	./man/build $< @PACKAGE_VERSION@ > $@
man/%.5: man/%.5.pod
	./man/build $< @PACKAGE_VERSION@ > $@
man/%.7: man/%.7.pod
	./man/build $< @PACKAGE_VERSION@ > $@
man/%.8: man/%.8.pod
	./man/build $< @PACKAGE_VERSION@ > $@

############################################################

core_src  = src/clockwork.h src/base.h src/base.c
core_src += src/opcodes.h
core_src += src/mesh.h src/mesh.c
core_src += src/sha1.h src/sha1.c src/userdb.h src/userdb.c
core_src += src/authdb.h src/authdb.c
core_src += src/policy.h src/policy.c
core_src += src/resource.h src/resource.c src/resources.h src/resources.c
core_src += src/vm.h src/vm.c

parser_spec_src  = src/spec/parser.c src/spec/parser.h src/spec/private.h
parser_spec_src += src/spec/lexer.l src/spec/lexer_impl.h
parser_spec_src += src/spec/grammar.y

sbindir = "$(prefix)/sbin"
sbin_PROGRAMS       = clockd cogd authtest meshd
clockd_SOURCES      = src/clockd.c   $(core_src) $(parser_spec_src)
cogd_SOURCES        = src/cogd.c     $(core_src) $(parser_spec_src)
authtest_SOURCES    = src/authtest.c $(core_src)
meshd_SOURCES       = src/meshd.c    $(core_src)

bin_PROGRAMS        = bdfa pn pnc cw
bdfa_SOURCES        = src/bdfa.c src/base.h src/base.c
pn_SOURCES          = src/pn.c    $(core_src)
pnc_SOURCES         = src/pnc.c   $(core_src)
cw_SOURCES          = cli/cw.c    $(core_src) $(parser_spec_src)

clidir=$(pkglibdir)/cli
dist_cli_SCRIPTS  = cli/mesh-builtin
dist_cli_SCRIPTS += cli/cw-localsys
dist_cli_SCRIPTS += cli/cw-template-erb

cli_SCRIPTS  = cw-cfm
cli_SCRIPTS += cw-exec
cli_SCRIPTS += cw-package
cli_SCRIPTS += cw-ping
cli_SCRIPTS += cw-query
cli_SCRIPTS += cw-service
cli_SCRIPTS += cw-show

cli_SCRIPTS += cli/cw-localsys
cli_SCRIPTS += cli/cw-template-erb

cli_PROGRAMS = cw cw-shell cw-cc cw-mesh
cw_shell_SOURCES = cli/cw-shell.c     $(core_src) $(parser_spec_src)
cw_cc_SOURCES    = cli/cw-cc.c        $(core_src) $(parser_spec_src)
cw_mesh_SOURCES  = cli/cw-mesh.c      $(core_src)

BUILTINS  =
BUILTINS += cw-cert
BUILTINS += cw-fact
BUILTINS += cw-help
BUILTINS += cw-trust
BUILTINS += cw-uuid

$(BUILTINS): cw
	rm -f $@ && ln -s $< $@

CLEANFILES += $(BUILTINS)

MESH_BUILTINS  = cw-cfm
MESH_BUILTINS += cw-exec
MESH_BUILTINS += cw-package
MESH_BUILTINS += cw-ping
MESH_BUILTINS += cw-query
MESH_BUILTINS += cw-service
MESH_BUILTINS += cw-show

$(MESH_BUILTINS): cli/mesh-builtin
	rm -f $@ && cp $< $@

CLEANFILES += $(MESH_BUILTINS);

builtins: $(BUILTINS) $(MESH_BUILTINS)

all-local: builtins

install-exec-hook:
	$(MKDIR_P) $(DESTDIR)$(clidir)
	for x in $(BUILTINS); do \
		(cd $(DESTDIR)$(clidir) && rm -f $$x$(EXEEXT) && ln -s cw$(EXEEXT) $$x$(EXEEXT)); \
	done

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

#src/spec/lexer.c: src/spec/lexer.l src/spec/grammar.h src/spec/lexer_impl.h src/spec/parser.h src/spec/private.h
#	$(LEXCOMPILE) --outfile=$@ $<

#src/spec/grammar.c: src/spec/grammar.y src/spec/parser.c src/spec/parser.h src/spec/private.h
#	$(YACCCOMPILE) --output-file=src/spec/grammar.c $<

############################################################
# Opcodes
src/opcodes.h: opcodes.yml gencode
	./gencode $< > $@

.PHONY: coverage-clean coverage-report coverage clean-local-check clean-local-cov
clean-local: clean-local-check clean-local-gcda
clean-local-gcda:
	touch sac.gcda
	find . -name '*.gc??' | xargs rm
clean-local-check:
	rm -fr t/tmp

if GCOV_ENABLED
coverage-clean:
	@rm -fr coverage
	@find . -name "*.gc??" -exec rm {} \;
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
