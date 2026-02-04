#pragma once

#include "compiler/context.h"

#define stringfy__(s) #s
#define stringfy(s) stringfy__(s)

#define SRC_LOCATION __FILE__ ":" stringfy(__LINE__)

#ifdef OS_BROWSER
#define LOG_NO_COLOR
#endif

#ifndef LOG_NO_COLOR
#define LOG_ERROR "\033[91m[ERROR]\033[0m"
#define LOG_INFO "\033[36m[INFO]\033[0m"
#define LOG_PERF "\033[34m[PERF]\033[0m"
#else
#define LOG_ERROR "[ERROR]"
#define LOG_INFO "[INFO]"
#define LOG_PERF "[PERF]"
#endif
#ifdef DEBUG
#include <stdio.h>

#define logError(fmt, ...)          \
  fprintf(stderr,                   \
          "\n" LOG_ERROR " " fmt    \
          "\n"                      \
          "[" SRC_LOCATION "]\n\n", \
          ##__VA_ARGS__)
#define logInfo(fmt, ...) printf(LOG_INFO " " fmt "\n", ##__VA_ARGS__)
#define logPerf(fmt, ...) printf(LOG_PERF " " fmt "\n", ##__VA_ARGS__)
#define DebugLog(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define logError(fmt, ...) ((void)0)
#define logInfo(fmt, ...) ((void)0)
#define logPerf(fmt, ...) ((void)0)
#define DebugLog(fmt, ...) ((void)0)
#endif /* DEBUG */
