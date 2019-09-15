#ifndef MODE_H
#define MODE_H

#include <stdbool.h>
#include <wchar.h>

#include "command.h"

typedef struct EDITOR EDITOR;
typedef bool (*callback)(EDITOR *e, VIEW *v, const ARG *a);

typedef struct MODE MODE;
extern MODE *cmdmode;
extern MODE *docmode;

typedef struct KEYSTROKE KEYSTROKE;
struct KEYSTROKE{
    int o;
    wchar_t c;
};

callback
lookupkeystroke(const MODE *m, KEYSTROKE k, ARG *a);

#endif
