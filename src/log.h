#ifndef LOG_H
#define LOG_H

#define LOG_TRACE 0
#define LOG_INFO  1
#define LOG_WARN  2
#define LOG_ERROR 3

void logInit(int enabled);
void logMsg(int level, const char *fmt, ...);

#define LOG_T(...) logMsg(LOG_TRACE, __VA_ARGS__)
#define LOG_I(...) logMsg(LOG_INFO,  __VA_ARGS__)
#define LOG_W(...) logMsg(LOG_WARN,  __VA_ARGS__)
#define LOG_E(...) logMsg(LOG_ERROR, __VA_ARGS__)

#endif
