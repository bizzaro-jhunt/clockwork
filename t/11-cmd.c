/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

  This file is part of Clockwork.

  Clockwork is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clockwork is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Clockwork.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "test.h"
#include "../src/mesh.h"

static cmd_token_t* s_nth_token(cmd_t *c, unsigned int n)
{
	cmd_token_t *t;
	for_each_object(t, &c->tokens, l)
		if (--n == 0) return t;
	return NULL;
}

static const char * s_nth_value(cmd_t *c, unsigned int n)
{
	cmd_token_t *t = s_nth_token(c,n);
	return t ? t->value : NULL;
}

static int s_nth_type(cmd_t *c, unsigned int n)
{
	cmd_token_t *t = s_nth_token(c,n);
	return t ? t->type : -1;
}

TESTS {
	subtest {
		cmd_t *c = cmd_new();
		isnt_null(c, "cmd_new() allocated a new command");
		cmd_destroy(c);
	}

	subtest {
		cmd_t *c; const char *command = "ping";

		c = cmd_parse(command, COMMAND_LITERAL);
		isnt_null(c, "cmd_parse() allocated a new command");
		is_string(c->string, "ping", "original command retained");
		ok(c->string != command, "c->string is a unique pointer (self-allocated)");
		cmd_destroy(c);

		c = cmd_parse("   ping   ", COMMAND_LITERAL);
		isnt_null(c, "cmd_parse('   ping   ') allocated a new command");
		cmd_destroy(c);

		c = cmd_parse("\tshow  query\n   x\t\t\r\n", COMMAND_LITERAL);
		isnt_null(c, "cmd_parse('\\tshow  query\\n   x\\t\\t\\r\\n')");
		is_string(c->string, "show query x", "extraneous whitespace removed");
		cmd_destroy(c);

		c = cmd_parse("", COMMAND_LITERAL);
		is_null(c, "the empty command is invalid");
		c = cmd_parse(" \t   \r\n\n   ", COMMAND_LITERAL);
		is_null(c, "the empty (whitespace notwithstanding) command is invalid");
	}

	subtest {
		cmd_t *c;

		c = cmd_parse("show acl for %group", COMMAND_LITERAL);
		isnt_null(c, "cmd_parse('show acl for %%group')");
		is_string(c->string, "show acl for %group", "group name sigil preserved");
		is_string(s_nth_value(c, 1), "show",   "1st token is show");
		is_string(s_nth_value(c, 2), "acl",    "2nd token is acl");
		is_string(s_nth_value(c, 3), "for",    "3rd token is for");
		is_string(s_nth_value(c, 4), "%group", "4th token is %%group");
		is_null(s_nth_value(c, 5), "there is no 5th token");
		cmd_destroy(c);

		c = cmd_parse("show *", COMMAND_LITERAL);
		isnt_null(c, "cmd_parse('show *')");
		is_string(c->string, "show *", "wildcard at end of command preserved");
		is_string(s_nth_value(c, 1), "show",    "1st token is show");
		is_string(s_nth_value(c, 2), "*",       "2nd token is *");
		is_null(s_nth_value(c, 3), "there is no 3rd token");
		is_int(s_nth_type(c, 1), COMMAND_TOKEN_LITERAL, "1st token is a LITERAL");
		is_int(s_nth_type(c, 2), COMMAND_TOKEN_LITERAL, "2nd token is a LITERAL");
		cmd_destroy(c);

		c = cmd_parse("show *", COMMAND_PATTERN);
		isnt_null(c, "cmd_parse('show *')");
		is_string(c->string, "show *", "wildcard at end of command preserved");
		is_string(s_nth_value(c, 1), "show",    "1st token is show");
		is_string(s_nth_value(c, 2), "*",       "2nd token is *");
		is_null(s_nth_value(c, 3), "there is no 3rd token");
		is_int(s_nth_type(c, 1), COMMAND_TOKEN_LITERAL,  "1st token is a LITERAL");
		is_int(s_nth_type(c, 2), COMMAND_TOKEN_WILDCARD, "2nd token is a WILDCARD");
		cmd_destroy(c);

		c = cmd_parse("*", COMMAND_LITERAL);
		isnt_null(c, "cmd_parse('*')");
		is_string(c->string, "*", "wildcard command preserved");
		is_string(s_nth_value(c, 1), "*", "1st token is *");
		is_null(s_nth_value(c, 2), "there is no 2nd token");
		is_int(s_nth_type(c, 1), COMMAND_TOKEN_LITERAL, "1st token is a LITERAL");
		cmd_destroy(c);

		c = cmd_parse("*", COMMAND_PATTERN);
		isnt_null(c, "cmd_parse('*')");
		is_string(c->string, "*", "wildcard command preserved (pattern parse)");
		is_string(s_nth_value(c, 1), "*", "1st token is *");
		is_null(s_nth_value(c, 2), "there is no 2nd token");
		is_int(s_nth_type(c, 1), COMMAND_TOKEN_WILDCARD, "1st token is a WILDCARD");
		cmd_destroy(c);

		c = cmd_parse("exec usercommand arg1 \"quoted arg2\"", COMMAND_LITERAL);
		isnt_null(c, "cmd_parse('exec usercommand arg1 \"quoted arg2\"')");
		is_string(c->string, "exec usercommand arg1 \"quoted arg2\"", "quoted arg (double quote) preserved");
		is_string(s_nth_value(c, 1), "exec",        "1st token is exec");
		is_string(s_nth_value(c, 2), "usercommand", "2nd token is usercommand");
		is_string(s_nth_value(c, 3), "arg1",        "3rd token is arg1");
		is_string(s_nth_value(c, 4), "quoted arg2", "4th token is <unquoted>");
		is_null(s_nth_value(c, 5), "there is no 5th token");
		is_int(s_nth_type(c, 1), COMMAND_TOKEN_LITERAL, "1st token is a LITERAL");
		is_int(s_nth_type(c, 2), COMMAND_TOKEN_LITERAL, "2nd token is a LITERAL");
		is_int(s_nth_type(c, 3), COMMAND_TOKEN_LITERAL, "3rd token is a LITERAL");
		is_int(s_nth_type(c, 4), COMMAND_TOKEN_LITERAL, "4th token is a LITERAL");
		cmd_destroy(c);

		c = cmd_parse("exec usercommand arg1 'quoted \"arg2'", COMMAND_LITERAL);
		isnt_null(c, "cmd_parse('exec usercommand arg1 'quoted \"arg2'')");
		is_string(c->string, "exec usercommand arg1 \"quoted \\\"arg2\"", "single quotes replaced with double quotes");
		is_string(s_nth_value(c, 1), "exec",          "1st token is exec");
		is_string(s_nth_value(c, 2), "usercommand",   "2nd token is usercommand");
		is_string(s_nth_value(c, 3), "arg1",          "3rd token is arg1");
		is_string(s_nth_value(c, 4), "quoted \"arg2", "4th token is <unquoted>");
		is_null(s_nth_value(c, 5), "there is no 5th token");
		is_int(s_nth_type(c, 1), COMMAND_TOKEN_LITERAL, "1st token is a LITERAL");
		is_int(s_nth_type(c, 2), COMMAND_TOKEN_LITERAL, "2nd token is a LITERAL");
		is_int(s_nth_type(c, 3), COMMAND_TOKEN_LITERAL, "3rd token is a LITERAL");
		is_int(s_nth_type(c, 4), COMMAND_TOKEN_LITERAL, "4th token is a LITERAL");
		cmd_destroy(c);

		c = cmd_parse("query argument\\ with\\ spaces", COMMAND_LITERAL);
		isnt_null(c, "cmd_parse('query argument\\ with\\ spaces')");
		is_string(c->string, "query \"argument with spaces\"", "requoted argument with backslash escapes");
		is_string(s_nth_value(c, 1), "query",                "1st token is exec");
		is_string(s_nth_value(c, 2), "argument with spaces", "2nd token doesnt have backslashes");
		is_null(s_nth_value(c, 3), "there is no 3rd token");
		cmd_destroy(c);
	}

	subtest { /* argv-style parsing */
		cmd_t *c;
		const char *argv[4] = { NULL, NULL, NULL, NULL };

		argv[0] = "show";
		argv[1] = "version";
		argv[2] = NULL;
		c = cmd_parsev(argv, COMMAND_LITERAL);
		isnt_null(c, "parsed (show,version) from argument vector");
		is_string(s_nth_value(c, 1), "show",    "1st token is show");
		is_string(s_nth_value(c, 2), "version", "2nd token is version");
		is_null(s_nth_value(c, 3), "there is no 3rd token");
		is_string(c->string, "show version", "reconstructed string from argv list");
		cmd_destroy(c);

		argv[0] = "exec";
		argv[1] = "script";
		argv[2] = "argument with spaces";
		argv[3] = NULL;
		c = cmd_parsev(argv, COMMAND_LITERAL);
		isnt_null(c, "parsed (exec,script,'argument with spaces') from argv");
		is_string(s_nth_value(c, 1), "exec",                 "1st token");
		is_string(s_nth_value(c, 2), "script",               "2nd token");
		is_string(s_nth_value(c, 3), "argument with spaces", "3rd token");
		is_null(s_nth_value(c, 4), "there is no 4th token");
		is_string(c->string, "exec script \"argument with spaces\"",
			"double-quote args with spaces");
		cmd_destroy(c);

		argv[0] = "exec";
		argv[1] = "script";
		argv[2] = "\"both\" kinds of 'quotes'";
		argv[3] = NULL;
		c = cmd_parsev(argv, COMMAND_LITERAL);
		isnt_null(c, "parsed arguments with both kinds of quotes from argv");
		is_string(s_nth_value(c, 1), "exec",                       "1st token");
		is_string(s_nth_value(c, 2), "script",                     "2nd token");
		is_string(s_nth_value(c, 3), "\"both\" kinds of 'quotes'", "3rd token");
		is_null(s_nth_value(c, 4), "there is no 4th token");
		is_string(c->string, "exec script \"\\\"both\\\" kinds of 'quotes'\"",
			"double-quoted the last argument, and escaped the double quotes");
		cmd_destroy(c);

		argv[0] = "exec";
		argv[1] = "script";
		argv[2] = "escape \\ the escape \\";
		argv[3] = NULL;
		c = cmd_parsev(argv, COMMAND_LITERAL);
		isnt_null(c, "parsed arguments with literal escape chars from argv");
		is_string(s_nth_value(c, 1), "exec",                    "1st token");
		is_string(s_nth_value(c, 2), "script",                  "2nd token");
		is_string(s_nth_value(c, 3), "escape \\ the escape \\", "3rd token");
		is_null(s_nth_value(c, 4), "there is no 4th token");
		is_string(c->string, "exec script \"escape \\\\ the escape \\\\\"",
			"double-quoted the last argument, and escaped the literal backslashes");
		cmd_destroy(c);
	}

	subtest { /* non-wildcard pattern matching */
		cmd_t *pat, *c;

		pat = cmd_parse("show version", COMMAND_PATTERN);
		isnt_null(pat, "parsed pattern /show version/");
		c = cmd_parse("show version", COMMAND_LITERAL);
		isnt_null(c, "parsed literal 'show version'");

		ok(cmd_match(c, pat), "show version =~ show version");

		cmd_destroy(c);
		c = cmd_parse("show", COMMAND_LITERAL);
		isnt_null(c, "parsed literal 'show'");
		ok(!cmd_match(c, pat), "show !~ show version");

		cmd_destroy(c);
		c = cmd_parse("show stats", COMMAND_LITERAL);
		isnt_null(c, "parsed literal 'show stats'");
		ok(!cmd_match(c, pat), "show stats !~ show version");

		cmd_destroy(c);
		c = cmd_parse("ping", COMMAND_LITERAL);
		isnt_null(c, "parsed literal 'ping'");
		ok(!cmd_match(c, pat), "ping !~ show version");

		cmd_destroy(pat);
		cmd_destroy(c);
	}

	subtest { /* partial wildcard matches */
		cmd_t *pat, *c;

		pat = cmd_parse("show *", COMMAND_PATTERN);
		isnt_null(pat, "parsed pattern /show */");
		c = cmd_parse("show version", COMMAND_LITERAL);
		isnt_null(c, "parsed literal 'show version'");

		ok(cmd_match(c, pat), "show version =~ show version");

		cmd_destroy(c);
		c = cmd_parse("show", COMMAND_LITERAL);
		isnt_null(c, "parsed literal 'show'");
		ok(cmd_match(c, pat), "show =~ show *");

		cmd_destroy(c);
		c = cmd_parse("show stats that are fun and awesome to graph", COMMAND_LITERAL);
		isnt_null(c, "parsed literal 'show stats ...'");
		ok(cmd_match(c, pat), "show stats ... =~ show *");

		cmd_destroy(c);
		c = cmd_parse("ping", COMMAND_LITERAL);
		isnt_null(c, "parsed literal 'ping'");
		ok(!cmd_match(c, pat), "ping !~ show *");

		cmd_destroy(pat);
		cmd_destroy(c);
	}

	subtest { /* full wildcard matches */
		cmd_t *pat, *c;

		pat = cmd_parse("*", COMMAND_PATTERN);
		isnt_null(pat, "parsed pattern /*/");
		c = cmd_parse("show version", COMMAND_LITERAL);
		isnt_null(c, "parsed literal 'show version'");

		ok(cmd_match(c, pat), "show version =~ *");

		cmd_destroy(c);
		c = cmd_parse("show", COMMAND_LITERAL);
		isnt_null(c, "parsed literal 'show'");
		ok(cmd_match(c, pat), "show =~ *");

		cmd_destroy(c);
		c = cmd_parse("show stats that are fun and awesome to graph", COMMAND_LITERAL);
		isnt_null(c, "parsed literal 'show stats ...'");
		ok(cmd_match(c, pat), "show stats ... =~ *");

		cmd_destroy(c);
		c = cmd_parse("ping", COMMAND_LITERAL);
		isnt_null(c, "parsed literal 'ping'");
		ok(cmd_match(c, pat), "ping =~ *");

		cmd_destroy(pat);
		cmd_destroy(c);
	}

	subtest { /* invalid wildcard placement */
		cmd_t *c;

		c = cmd_parse("a * b", COMMAND_PATTERN);
		is_null(c, "'a * b' is not a valid pattern - wildcard is ambiguous");

		c = cmd_parse("* b", COMMAND_PATTERN);
		is_null(c, "'* b' is not a valid pattern - wildcard is ambiguous");

		c = cmd_parse("a * *", COMMAND_PATTERN);
		is_null(c, "'a * *' is not a valid pattern - too many wildcards");
	}

	done_testing();
}
