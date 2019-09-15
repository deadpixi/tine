#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#include "editor.h"

bool
runextended(const wchar_t *c, size_t n, EDITOR *e);

#endif
