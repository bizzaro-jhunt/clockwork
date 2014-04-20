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
AM_CFLAGS += -DCW_ETC_DIR=\"$(pkgsysconfdir)\"
AM_CFLAGS += -DCW_VAR_DIR=\"$(localstatedir)\"
AM_CFLAGS += -DCW_DATA_DIR=\"$(localstatedir)/lib/@PACKAGE_NAME@\"
AM_CFLAGS += -DCW_LIB_DIR=\"$(pkglibdir)\"
AM_CFLAGS += @GCOV_CFLAGS@

AM_LFLAGS = --header-file --yylineno
AM_YFLAGS = -d

LDADD = -lctap


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
AM_CFLAGS += -DAUGEAS_INCLUDES=\"$(augdir)/augeas/lenses\"
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

TESTS  = t/00-bits
TESTS += t/01-string
TESTS += t/04-hash
TESTS += t/05-mem
TESTS += t/06-pack
TESTS += t/07-sha1
TESTS += t/08-list
TESTS += t/10-facts
TESTS += t/15-path
TESTS += t/20-stree
TESTS += t/22-job
TESTS += t/25-resource
TESTS += t/30-policy
TESTS += t/31-template
TESTS += t/40-userdb
TESTS += t/50-proto
TESTS += t/55-cert
TESTS += t/61-res_user
TESTS += t/62-res_file
TESTS += t/63-res_group
TESTS += t/64-res_dir
TESTS += t/65-res_host
TESTS += t/66-res_sysctl
TESTS += t/67-res_service
TESTS += t/68-res_package
TESTS += t/69-res_exec
TESTS += t/70-managers

TEST_HELPERS  = t/helpers/executor
TEST_HELPERS += t/helpers/prompter
TEST_HELPERS += t/helpers/proto

check_PROGRAMS = $(TESTS) $(TEST_HELPERS)
BUILT_TESTS    = $(TESTS) $(TEST_HELPERS)


test_source  = t/test.h
test_source += src/mem.h        src/mem.c
test_source += src/sha1.h       src/sha1.c
test_source += src/userdb.h     src/userdb.c
test_source += src/cert.h       src/cert.c
test_source += src/prompt.h     src/prompt.c
test_source += src/exec.h       src/exec.c
test_source += src/augcw.h      src/augcw.c
test_source += src/policy.h     src/policy.c
test_source += src/resource.h   src/resource.c
test_source += src/resources.h  src/resources.c
test_source += src/job.h        src/job.c
test_source += src/template.h   src/template.c
test_source += src/service.h    src/service.c
test_source += src/package.h    src/package.c
test_source += src/proto.h      src/proto.c

test_source += src/conf/grammar.y
test_source += src/conf/lexer.l
test_source += src/conf/parser.h
test_source += src/conf/parser.c
test_source += src/conf/private.h

test_source += src/spec/grammar.y
test_source += src/spec/lexer.l
test_source += src/spec/parser.h
test_source += src/spec/parser.c
test_source += src/spec/private.h

test_source += src/tpl/grammar.y
test_source += src/tpl/lexer.l
test_source += src/tpl/parser.h
test_source += src/tpl/parser.c
test_source += src/tpl/private.h

test_source += src/gear/gear.h
test_source += src/gear/hash.c
test_source += src/gear/log.c
test_source += src/gear/pack.c
test_source += src/gear/path.c
test_source += src/gear/string.c

t_00_bits_SOURCES        = t/00-bits.c          $(test_source)
t_01_string_SOURCES      = t/01-string.c        $(test_source)
t_04_hash_SOURCES        = t/04-hash.c          $(test_source)
t_05_mem_SOURCES         = t/05-mem.c           $(test_source)
t_06_pack_SOURCES        = t/06-pack.c          $(test_source)
t_07_sha1_SOURCES        = t/07-sha1.c          $(test_source)
t_08_list_SOURCES        = t/08-list.c          $(test_source)
t_10_facts_SOURCES       = t/10-facts.c         $(test_source)
t_15_path_SOURCES        = t/15-path.c          $(test_source)
t_20_stree_SOURCES       = t/20-stree.c         $(test_source)
t_22_job_SOURCES         = t/22-job.c           $(test_source)
t_25_resource_SOURCES    = t/25-resource.c      $(test_source)
t_30_policy_SOURCES      = t/30-policy.c        $(test_source)
t_31_template_SOURCES    = t/31-template.c      $(test_source)
t_40_userdb_SOURCES      = t/40-userdb.c        $(test_source)
t_50_proto_SOURCES       = t/50-proto.c         $(test_source)
t_55_cert_SOURCES        = t/55-cert.c          $(test_source)
t_61_res_user_SOURCES    = t/61-res_user.c      $(test_source)
t_62_res_file_SOURCES    = t/62-res_file.c      $(test_source)
t_63_res_group_SOURCES   = t/63-res_group.c     $(test_source)
t_64_res_dir_SOURCES     = t/64-res_dir.c       $(test_source)
t_65_res_host_SOURCES    = t/65-res_host.c      $(test_source)
t_66_res_sysctl_SOURCES  = t/66-res_sysctl.c    $(test_source)
t_67_res_service_SOURCES = t/67-res_service.c   $(test_source)
t_68_res_package_SOURCES = t/68-res_package.c   $(test_source)
t_69_res_exec_SOURCES    = t/69-res_exec.c      $(test_source)
t_70_managers_SOURCES    = t/70-managers.c      $(test_source)

t_helpers_executor_SOURCES = t/helpers/executor.c $(test_source)
t_helpers_prompter_SOURCES = t/helpers/prompter.c $(test_source)
t_helpers_proto_SOURCES    = t/helpers/proto.c    $(test_source)

CLEANFILES       = $(BUILT_SOURCES)
CLEANFILES      += *.tmp
CLEANFILES      += t/tmp/*

EXTRA_DIST       = HACKING autogen.sh
EXTRA_DIST      += augeas gather.d scripts
EXTRA_DIST      += examples extras
EXTRA_DIST      += help man
EXTRA_DIST      += t/data

dist_man_MANS    = man/clockwork.7
dist_man_MANS   += man/cwa.1
dist_man_MANS   += man/cwa.conf.5
dist_man_MANS   += man/cwca.1
dist_man_MANS   += man/cwcert.1
dist_man_MANS   += man/cwpol.1
dist_man_MANS   += man/cwtemplate.1
dist_man_MANS   += man/policyd.1
dist_man_MANS   += man/policyd.conf.5
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

core_src     = src/mem.c src/sha1.c src/userdb.c src/cert.c src/prompt.c src/exec.c src/augcw.c
core_src    += src/mem.h src/sha1.h src/userdb.h src/cert.h src/prompt.h src/exec.h src/augcw.h
core_src    += src/policy.c src/resource.c src/resources.c src/job.c src/template.c
core_src    += src/policy.h src/resource.h src/resources.h src/job.h src/template.h
core_src    += src/gear/hash.c src/gear/log.c src/gear/string.c src/gear/pack.c src/gear/path.c
core_src    += src/gear/gear.h
core_src    += src/clockwork.h
core_src    += src/service.c src/package.c
core_src    += src/service.h src/package.h

client_src   = src/proto.c src/client.c
client_src  += src/proto.h src/client.h

server_src   = src/proto.c src/server.c
server_src  += src/proto.h src/server.h

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

sbindir = "$(prefix)/sbin"
sbin_PROGRAMS = cwa cwcert policyd cwca cwpol cwtemplate
policyd_SOURCES = $(core_src) $(server_src) src/policyd.c $(parser_spec_src) $(parser_conf_src)
cwca_SOURCES    = $(core_src) $(server_src) src/cwca.c $(parser_conf_src)
cwa_SOURCES     = $(core_src) $(client_src) src/cwa.c $(parser_conf_src)
cwcert_SOURCES  = $(core_src) $(client_src) src/cwcert.c $(parser_conf_src)
cwpol_SOURCES   = $(core_src) src/cwpol.c $(parser_spec_src)
cwtemplate_SOURCES = $(core_src) $(client_src) src/cwtemplate.c $(parser_conf_src)

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

src/tpl/lexer.c: src/tpl/lexer.l src/tpl/grammar.h src/tpl/lexer_impl.h src/tpl/parser.h src/tpl/private.h
	$(LEXCOMPILE) --outfile=$@ $<

src/tpl/grammar.c: src/tpl/grammar.y src/tpl/parser.c src/tpl/parser.h src/tpl/private.h
	$(YACCCOMPILE) -p yytpl --output-file=src/tpl/grammar.c $<

src/conf/lexer.c: src/conf/lexer.l src/conf/grammar.h src/conf/lexer_impl.h src/conf/private.h
	$(LEXCOMPILE) --outfile=$@ $<

src/conf/grammar.c: src/conf/grammar.y src/conf/parser.c src/conf/parser.h src/conf/private.h
	$(YACCCOMPILE) -p yyconf --output-file=src/conf/grammar.c $<

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
endif
