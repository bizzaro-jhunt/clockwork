#include <stdarg.h>
#include <syslog.h>
#include <stdio.h>

#include "log.h"

static int syslog_initialized = 0;

#define log2syslog(prio) do { \
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

void DEBUG(const char *format, ...)      { log2syslog(LOG_DEBUG);   }
void INFO(const char *format, ...)       { log2syslog(LOG_INFO);    }
void NOTICE(const char *format, ...)     { log2syslog(LOG_NOTICE);  }
void WARNING(const char *format, ...)    { log2syslog(LOG_WARNING); }
void ERROR(const char *format, ...)      { log2syslog(LOG_ERR);     }
void CRITICAL(const char *format, ...)   { log2syslog(LOG_CRIT);    }
void ALERT(const char *format, ...)      { log2syslog(LOG_ALERT);   }
void EMERGENCY(const char *format, ...)  { log2syslog(LOG_EMERG);   }

