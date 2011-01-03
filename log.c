#include <stdarg.h>
#include <syslog.h>
#include <stdio.h>

#include "log.h"

static int syslog_initialized = 0;
static int log_threshold = LOG_LEVEL_ALL;

#define log2syslog(prio,level) do { \
	if (level > log_threshold) { return; } \
	va_list ap; \
	va_start(ap, format); \
	if (syslog_initialized) { \
		vsyslog(prio, format, ap); \
	} else { \
		vfprintf(stderr, format, ap); \
		fprintf(stderr, "\n"); \
	} \
	va_end(ap); \
} while (0)

void log_init(const char *ident) {
	openlog(ident, LOG_CONS | LOG_PID, LOG_DAEMON);
	syslog_initialized = 1;
}

void log_level(int level)
{
	if (level < LOG_LEVEL_NONE) {
		level = LOG_LEVEL_EMERGENCY;
	} else if (level > LOG_LEVEL_ALL) {
		level = LOG_LEVEL_ALL;
	}
	log_threshold = level;
}

void DEVELOPER(const char *format, ...)
{
#ifdef DEVEL
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fprintf(stderr, "\n");
#endif
}

void DEBUG(const char *format, ...)      { log2syslog(LOG_DEBUG,   LOG_LEVEL_DEBUG);     }
void INFO(const char *format, ...)       { log2syslog(LOG_INFO,    LOG_LEVEL_INFO);      }
void NOTICE(const char *format, ...)     { log2syslog(LOG_NOTICE,  LOG_LEVEL_NOTICE);    }
void WARNING(const char *format, ...)    { log2syslog(LOG_WARNING, LOG_LEVEL_WARNING);   }
void ERROR(const char *format, ...)      { log2syslog(LOG_ERR,     LOG_LEVEL_ERROR);     }
void CRITICAL(const char *format, ...)   { log2syslog(LOG_CRIT,    LOG_LEVEL_CRITICAL);  }
void ALERT(const char *format, ...)      { log2syslog(LOG_ALERT,   LOG_LEVEL_ALERT);     }
void EMERGENCY(const char *format, ...)  { log2syslog(LOG_EMERG,   LOG_LEVEL_EMERGENCY); }

