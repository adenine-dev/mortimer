#pragma once

#include <stdarg.h>
#include <stdio.h>

#define infoln(str, ...)                                                       \
  do {                                                                         \
    printf("\033[34m[INFO]: %s:%d \033[0m", __FILE__, __LINE__);               \
    printf(str, ##__VA_ARGS__);                                                \
    printf("\n");                                                              \
  } while (false)

#define warnln(str, ...)                                                       \
  do {                                                                         \
    printf("\033[33m[WARN]: %s:%d \033[0m", __FILE__, __LINE__);               \
    printf(str, ##__VA_ARGS__);                                                \
    printf("\n");                                                              \
  } while (false)

#define errorln(str, ...)                                                      \
  do {                                                                         \
    printf("\033[31m[ERROR]: %s:%d \033[0m", __FILE__, __LINE__);              \
    printf(str, ##__VA_ARGS__);                                                \
    printf("\n");                                                              \
  } while (false)
