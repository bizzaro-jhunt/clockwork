#include <stdarg.h>
#include <syslog.h>
#include <stdio.h>

#include "log.h"

static int SYSLOG_INITIALIZED = 0;
static int LOG_LEVEL = LOG_LEVEL_INFO;

#define log2syslog(prio,level) do { \
	if (level > LOG_LEVEL) { return; } \
	va_list ap; \
	va_start(ap, format); \
	if (SYSLOG_INITIALIZED) { \
		vsyslog(prio, format, ap); \
	} else { \
		vfprintf(stderr, format, ap); \
		fprintf(stderr, "\n"); \
	} \
	va_end(ap); \
} while (0)

void log_init(const char *ident) {
	if (SYSLOG_INITIALIZED == 0) {
		openlog(ident, LOG_CONS | LOG_PID, LOG_DAEMON);
		SYSLOG_INITIALIZED = 1;
	}
}

int log_level(int l)
{
	return LOG_LEVEL = (l > LOG_LEVEL_ALL ? LOG_LEVEL_ALL : (l < LOG_LEVEL_NONE ? LOG_LEVEL_NONE : l));
}

void CRITICAL(const char *format, ...)   { log2syslog(LOG_CRIT,    LOG_LEVEL_CRITICAL);  }
void ERROR(const char *format, ...)      { log2syslog(LOG_ERR,     LOG_LEVEL_ERROR);     }
void WARNING(const char *format, ...)    { log2syslog(LOG_WARNING, LOG_LEVEL_WARNING);   }
void NOTICE(const char *format, ...)     { log2syslog(LOG_NOTICE,  LOG_LEVEL_NOTICE);    }
void INFO(const char *format, ...)       { log2syslog(LOG_INFO,    LOG_LEVEL_INFO);      }
void DEBUG(const char *format, ...)      { log2syslog(LOG_DEBUG,   LOG_LEVEL_DEBUG);     }

