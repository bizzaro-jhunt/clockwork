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

#ifndef LOG_H
#define LOG_H

#define LOG_LEVEL_ALL         8
#define LOG_LEVEL_DEBUG       7
#define LOG_LEVEL_INFO        6
#define LOG_LEVEL_NOTICE      5
#define LOG_LEVEL_WARNING     4
#define LOG_LEVEL_ERROR       3
#define LOG_LEVEL_CRITICAL    2
#define LOG_LEVEL_NONE        1

void log_init(const char *ident);
int log_set(int level);
int log_level(void);
const char* log_level_name(int level);

void CRITICAL(const char *format, ...);
void ERROR(const char *format, ...);
void WARNING(const char *format, ...);
void NOTICE(const char *format, ...);
void INFO(const char *format, ...);
void DEBUG(const char *format, ...);

#endif
