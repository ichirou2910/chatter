#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

char* rand_string(size_t size);
char** str_split(char* str, const char c);
void str_trim_lf(char* arr, int length);

#endif