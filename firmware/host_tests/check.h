#pragma once
#include <stdio.h>
#include <string.h>

//Tiny test helper: counts passes and fails, prints the line of each fail
static int g_pass, g_fail;

//do/while(0) so it acts like one statement, safe inside an if with no braces
#define CHECK(cond) \
  do { \
    if (cond) { \
      g_pass++; \
    } else { \
      g_fail++; \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
  } while (0)

//Compare n raw bytes, good for checking frames and buffers
#define CHECK_BYTES(actual, expected, n) CHECK(memcmp((actual), (expected), (n)) == 0)

//Last line of every test's main(), exit code 1 if anything failed so CI goes red
#define CHECK_SUMMARY() \
  do { \
    printf("%d/%d checks passed\n", g_pass, g_pass + g_fail); \
    return g_fail ? 1 : 0; \
  } while (0)
