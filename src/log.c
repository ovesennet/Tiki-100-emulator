#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <windows.h>

static int logEnabled = 0;
static const char *levelNames[] = { "TRACE", "INFO", "WARN", "ERROR" };

void logInit(int enabled) {
  logEnabled = enabled;
}

void logMsg(int level, const char *fmt, ...) {
  if (!logEnabled) return;

  LARGE_INTEGER freq, count;
  double ms = 0;
  if (QueryPerformanceFrequency(&freq) && QueryPerformanceCounter(&count)) {
    ms = (double)count.QuadPart * 1000.0 / (double)freq.QuadPart;
  }

  fprintf(stderr, "[%10.1f] %-5s ", ms, levelNames[level]);

  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);

  fprintf(stderr, "\n");
  fflush(stderr);
}
