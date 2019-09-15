#ifndef BUFFER_H
#define BUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#define NONE ((size_t)-1)
typedef size_t lineno;
typedef size_t colno;
typedef struct POS POS;
struct POS{
    lineno l;
    colno c;
};

typedef struct LINE LINE;
struct LINE{
    size_t n;
    wchar_t *s;
};

typedef uint64_t txn;
typedef struct JOURNAL JOURNAL;
typedef struct BUFFER BUFFER;
struct BUFFER{
    size_t n;
    LINE *l;

    bool canundo, dirty;
    int nbegin;
    txn t;
    JOURNAL *j;
};

BUFFER *openbuffer();
void closebuffer(BUFFER *b);

bool insertline(BUFFER *b, lineno l);
bool inserttext(BUFFER *b, POS p, const wchar_t *s, size_t n);
bool deleteline(BUFFER *b, lineno l);
bool deletetext(BUFFER *b, POS p, size_t n);

wint_t charat(const BUFFER *b, POS p);

bool prev(const BUFFER *b, POS *p);
bool next(const BUFFER *b, POS *p);
bool attop(const BUFFER *b, POS p);
bool atbot(const BUFFER *b, POS p);
bool ateol(const BUFFER *b, POS p);

POS pos(lineno l, colno c);

void enableundo(BUFFER *b);
bool mark(BUFFER *b);
void begin(BUFFER *b);
bool commit(BUFFER *b);
bool undo(BUFFER *b, POS *p);
void clearundo(BUFFER *b);

#endif
