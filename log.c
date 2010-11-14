#include <stdarg.h>
#include <syslog.h>

#include "log.h"

#define log2syslog(prio) do { \
	va_list ap; \
	va_start(ap, format); \
	vsyslog(prio, format, ap); \
} while (0)

void log_init(const char *ident) {
	openlog(ident, LOG_CONS | LOG_PID, LOG_DAEMON);
}

void DEBUG(const char *format, ...)      { log2syslog(LOG_DEBUG);   }
void INFO(const char *format, ...)       { log2syslog(LOG_INFO);    }
void NOTICE(const char *format, ...)     { log2syslog(LOG_NOTICE);  }
void WARNING(const char *format, ...)    { log2syslog(LOG_WARNING); }
void ERROR(const char *format, ...)      { log2syslog(LOG_ERR);     }
void CRITICAL(const char *format, ...)   { log2syslog(LOG_CRIT);    }
void ALERT(const char *format, ...)      { log2syslog(LOG_ALERT);   }
void EMERGENCY(const char *format, ...)  { log2syslog(LOG_EMERG);   }

