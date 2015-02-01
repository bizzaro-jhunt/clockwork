/*
  Copyright 2011-2015 James Hunt <james@jameshunt.us>

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

TESTS {
	subtest { /* JSON escapes */
		struct {
			const char *in;
			const char *out;
			const char *msg;
		} JSON[] = {
			{ "", "", "empty string" },
			{ "\"", "\\\"", "double quote" },
			{ "\t", "\\t",  "horizontal tab" },
			{ "\r", "\\r",  "carriage return" },
			{ "\n", "\\n",  "newline" },
			{ "\\", "\\\\", "backslash" },
			{ "string", "string", "nothing to escape" },

			{ "\"\r\t\n\r\t\"\\\\",
			  "\\\"\\r\\t\\n\\r\\t\\\"\\\\\\\\",
			  "big mess of nothing but escapes" },

			{ "there are \"lots\" of\nlines \\ and \"\"",
			  "there are \\\"lots\\\" of\\nlines \\\\ and \\\"\\\"",
			  "escapes with non-escapes" },
			{ 0, 0, 0 },
		};

		int i;
		for (i = 0; JSON[i].msg; i++) {
			char *e = cw_escape_json(JSON[i].in);
			is(e, JSON[i].out, JSON[i].msg);
			free(e);
		}
	}

	subtest { /* CDATA escapes */
		struct {
			const char *in;
			const char *out;
			const char *msg;
		} CDATA[] = {
			{ "",       "<![CDATA[]]>",                "empty string" },
			{ "...",    "<![CDATA[...]]>",             "simple string" },
			{ "\t\r\n", "<![CDATA[\t\r\n]]>",          "non-printables" },
			{ "]]>",    "<![CDATA[]]]]><![CDATA[>]]>", "embedded ']]>' string" },
			{ 0, 0, 0 },
		};

		int i;
		for (i = 0; CDATA[i].msg; i++) {
			char *e = cw_escape_cdata(CDATA[i].in);
			is(e, CDATA[i].out, CDATA[i].msg);
			free(e);
		}
	}
}
