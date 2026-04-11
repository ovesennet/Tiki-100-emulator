#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <windows.h>

static int logEnabled = 0;
static FILE *logFile = NULL;
static const char *levelNames[] = { "TRACE", "INFO", "WARN", "ERROR" };

void logInit(int enabled) {
  logEnabled = enabled;
  if (enabled)
    logFile = fopen("tikiemul.log", "w");
}

void logMsg(int level, const char *fmt, ...) {
  if (!logEnabled && !logFile) return;

  LARGE_INTEGER freq, count;
  double ms = 0;
  if (QueryPerformanceFrequency(&freq) && QueryPerformanceCounter(&count)) {
    ms = (double)count.QuadPart * 1000.0 / (double)freq.QuadPart;
  }

  if (logEnabled) {
    fprintf(stderr, "[%10.1f] %-5s ", ms, levelNames[level]);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);
  }
  if (logFile) {
    fprintf(logFile, "[%10.1f] %-5s ", ms, levelNames[level]);
    va_list args;
    va_start(args, fmt);
    vfprintf(logFile, fmt, args);
    va_end(args);
    fprintf(logFile, "\n");
    fflush(logFile);
  }
}
