#ifndef _LOG_H
#define _LOG_H

void log_init(const char *ident);

void DEVELOPER(const char *format, ...);
void DEBUG(const char *format, ...);

void INFO(const char *format, ...);
void NOTICE(const char *format, ...);

void WARNING(const char *format, ...);
void ERROR(const char *format, ...);

void CRITICAL(const char *format, ...);
void ALERT(const char *format, ...);
void EMERGENCY(const char *format, ...);

#endif
