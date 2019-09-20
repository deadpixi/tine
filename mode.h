#ifndef MODE_H
#define MODE_H

#include <stdbool.h>
#include <wchar.h>

#include "structs.h"
#include "command.h"

typedef bool (*callback)(EDITOR *e, VIEW *v, const ARG *a);

extern MODE *cmdmode;
extern MODE *docmode;

struct KEYSTROKE{
    int o;
    wchar_t c;
};

callback
lookupkeystroke(const MODE *m, KEYSTROKE k, ARG *a);

#endif
