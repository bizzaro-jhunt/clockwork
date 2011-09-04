/*
  Copyright 2011 James Hunt <james@jameshunt.us>

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

#include "clockwork.h"
#include "prompt.h"

#include <termios.h>

#define PROMPT_BUFFER_SIZE 8192

char* prompt(const char *text)
{
	char buf[PROMPT_BUFFER_SIZE];
	char *result = NULL, *p;
	size_t len = 0, bytes = 0;

	printf("%s", text);

	for (;;) {
		fgets(buf, PROMPT_BUFFER_SIZE, stdin);

		bytes = strlen(buf);

		/* don't use realloc; if memory has to be
		   moved to accommodate a large chunk, the
		   old memory does *not* get zeroed. */

		p = malloc(len + bytes + 1);
		memcpy(p,       result, len);
		memcpy(p + len, buf,    bytes);

		/* zero out old buffer. */
		memset(result, 0, len);
		len += bytes;
		result = p;
		result[len] = '\0';

		if (len > 0 && result[len - 1] == '\n') {
			result[len - 1] = '\0';
			break;
		}
	}

	/* zero out temporary buffer. */
	memset(buf, 0, PROMPT_BUFFER_SIZE);

	return result;
}

#if 0
char* prompt_without_echo(const char *prompt)
{
	char *result;
	struct termios tio;

	tcgetattr(0, &tio);
	tio.c_lflag ^= ECHO;
	tcsetattr(0, TCSANOW, &tio);

	result = prompt_with_echo(prompt);
	printf("\n");

	tio.c_lflag |= ECHO;
	tcsetattr(0, TCSANOW, &tio);

	return result;
}
#endif
