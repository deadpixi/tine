#include <libgen.h>
#include <ncursesw/ncurses.h>
#include <stdbool.h>
#include <string.h>

#include "structs.h"
#include "editor.h"
#include "mode.h"
#include "util.h"

#define DEFAULT_TS 3
#define DEFAULT_PH 12
static bool
initview(VIEW *v, WINDOW *w, MODE *m, void (*statuscb)(EDITOR *e, VIEW *v))
{
    v->b = openbuffer();
    if (!v->b)
        return false;
    v->w = w;
    v->m = m;
    v->bs = v->be = v->rm = NONE;
    v->gb = pos(NONE, NONE);
    v->lm = 0;
    v->ts = DEFAULT_TS;
    v->ph = DEFAULT_PH;
    v->statuscb = statuscb;
    v->delay = true;
    return true;
}

static void
freeview(VIEW *v)
{
    if (v){
        free(v->dl);
        closebuffer(v->b);
        if (v->w && v->w != stdscr)
            delwin(v->w);
    }
}

static void
docstatus(EDITOR *e, VIEW *v)
{
    WINDOW *w = e->cmdview.w;
    int lines, cols;

    cleartag(e->docview.b, VIRTCURS);

    getmaxyx(w, lines, cols); (void)lines;
    char buf[cols + 1];
    memset(buf, 0, sizeof(buf));

    char rm[25] = {0}, lm[25] = {0};
    if (v->rm != NONE)
        snprintf(rm, sizeof(rm) - 1, "%zu", v->rm + 1);
    else
        snprintf(rm, sizeof(rm) - 1, "?");

    if (v->ai)
        snprintf(lm, sizeof(lm) - 1, ">");
    else
        snprintf(lm, sizeof(lm) - 1, "%zu", v->lm == NONE? 1 : v->lm + 1);

    if (e->err[0])
        snprintf(buf, cols, "%s", e->err);
    else{
        char *fn = ellipsize(basename(e->name), 12, false);
        snprintf(buf, cols,
         "%sFile=%-15s Line=%-5zu Col=%-5zu Block=%s%s %sTabs=%-2zu %sMargins=%s-%s",
         v->b->dirty? "*" : " ",
         fn? fn : basename(e->name),
         v->p.l + 1, v->p.c + 1,
         v->bs == NONE? "?" : "S",
         v->be == NONE? "?" : "E",
         v->et? "*" : " ",
         v->ts,
         v->ex? "*" : " ",
         lm,
         rm);
         free(fn);
    }

    werase(w);
    mvwprintw(w, 0, 0, "%s", buf);
    e->err[0] = 0;
    wrefresh(w);
}

static void
cmdstatus(EDITOR *e, VIEW *v)
{
   VIEW *dv = &e->docview;
   cleartag(dv->b, VIRTCURS);
   int s = dv->p.l >= dv->bs && dv->p.l <= dv->be? A_NORMAL : A_REVERSE;
   settag(dv->b, VIRTCURS, dv->p, pos(dv->p.l, dv->p.c + 1), s);
   redisplay(dv);
}

EDITOR *
openeditor(const char *name, WINDOW *docwin, WINDOW *cmdwin)
{
    EDITOR *e = calloc(1, sizeof(EDITOR));
    if (!e
    ||  !initview(&e->cmdview, cmdwin, cmdmode, cmdstatus)
    ||  !initview(&e->docview, docwin, docmode, docstatus)){
        closeeditor(e);
        return NULL;
    }

    e->lc = pos(NONE, NONE);
    e->focusview = &e->docview;
    strncpy(e->name, name, FILENAME_MAX);
    for (int i = 0; i < CTRL_MAX; i++)
        e->ctrlmap[i] = i;
    for (int i = 0; i < BM_MAX; i++)
        e->bm[i] = pos(NONE, NONE);
    e->running = true;
    return e;
}

void
closeeditor(EDITOR *e)
{
    if (e){
        for (size_t i = 0; i < FUNC_MAX; i++)
            free(e->funcs[i]);
        free(e->find);
        freeview(&e->cmdview);
        freeview(&e->docview);
        free(e);
    }
}

static KEYSTROKE
remap(const EDITOR *e, KEYSTROKE k)
{
   if (k.o == OK && k.c < CTRL_MAX)
      k.c = e->ctrlmap[k.c];
   return k;
}

void
dispatch(EDITOR *e, VIEW *v, KEYSTROKE i)
{
    bool q = v->q && i.o == OK;
    KEYSTROKE k = remap(e, i);
    ARG a = {.t = ARG_STRING, .n1 = 1, .s1 = q? &i.c : &k.c};
    v->q = false;
    callback c = q? cmd_ty : lookupkeystroke(v->m, k, &a);
    if (!c)
        return;
    c(e, v, &a);
}

static void
reframe(VIEW *v)
{
    size_t lines, cols;
    getmaxyx(v->w, lines, cols);

    if (v->p.l >= v->tos.l && v->p.l < v->tos.l + lines)
        ; /* do nothing */
    else if (v->p.l < v->tos.l && v->tos.l - v->p.l < 5)
        v->tos.l -= (v->tos.l - v->p.l);
    else if (v->p.l < lines)
        v->tos.l = 0;
    else if (v->p.l > v->tos.l + (lines - 1) && v->p.l - (v->tos.l + lines - 1) < 5)
        v->tos.l += (v->p.l - (v->tos.l + lines - 1));
    else
        v->tos.l = v->p.l - 0.33 * lines;

    if (v->p.c - v->tos.c < cols)
        ; /* do nothing */
    else if (v->p.c < v->tos.c && v->tos.c - v->p.c < 5)
        v->tos.c -= (v->tos.c - v->p.c);
    else if (v->p.c < cols)
        v->tos.c = 0;
    else if (v->p.c > v->tos.c + (cols - 1) && v->p.c - (v->tos.c + cols - 1) < 5)
        v->tos.c += (v->p.c - (v->tos.c + cols - 1));
    else
        v->tos.c = v->p.c - 0.33 * cols;
}

static bool
between(POS p, POS a, POS b)
{
   return (a.l != NONE && a.c != NONE && b.l != NONE && b.c != NONE)
       && (p.l > a.l || (p.l == a.l && p.c >= a.c))
       && (p.l < b.l || (p.l == b.l && p.c < b.c));
}

static int
gettag(const BUFFER *b, POS p)
{
   for (int i = 0; i < TAG_MAX; i++){
      const TAG *t = b->tags + i;
      if (between(p, t->p1, t->p2))
         return t->v;
   }
   return 0;
}

void
redisplay(VIEW *v)
{
    reframe(v);

    size_t lines, cols, y = 0, x = 0;
    getmaxyx(v->w, lines, cols);

    werase(v->w);
    for (size_t l = 0; l < lines && v->tos.l + l < v->b->n; l++){
        size_t c = 0, i = 0;
        while (c < cols){
            wattrset(v->w, gettag(v->b, pos(v->tos.l + l, v->tos.c + i)));
            wmove(v->w, l, c);
            if (v->tos.l + l == v->p.l && v->tos.c + i == v->p.c)
                getyx(v->w, y, x);
            wchar_t w = charat(v->b, pos(v->tos.l + l, v->tos.c + i));
            if (w == '\t'){
                waddch(v->w, ' '); c++;
                while (c % v->ts){
                    waddch(v->w, ' ');
                    c++;
                }
            } else{
                int cw = wcwidth(w) > 0? wcwidth(w) : 0;
                wchar_t s[] = {w, 0, 0};
                if (iswcntrl(w)){
                  s[0] = L'^';
                  s[1] = L'@' + w;
                  if (wcwidth(s[1]) <= 0)
                     s[1] = L'?';
                  cw = 1 + wcwidth(s[1]);
                }
                waddwstr(v->w, s);
                c+= cw;
            }
            i++;
        }
    }
    wmove(v->w, y, x);
    wrefresh(v->w);
}

bool
error(EDITOR *e, const char *s)
{
    strncpy(e->err, s, ERR_MAX);
    return false;
}

KEYSTROKE
getkeystroke(EDITOR *e, bool delay)
{
    if (delay != e->focusview->delay){
        nodelay(e->focusview->w, !delay);
        e->focusview->delay = delay;
    }

    KEYSTROKE k = {0};
    wint_t c = 0;
    int o = wget_wch(e->focusview->w, &c);
    while (o == KEY_CODE_YES && c == KEY_RESIZE){
        redisplay(&e->docview);
        if (e->focusview->statuscb)
            e->focusview->statuscb(e, e->focusview);
        redisplay(e->focusview);
        o = wget_wch(e->focusview->w, &c);
    }
    k.o = c == WEOF? ERR : o;
    k.c = c;
    return k;
}

static void
fixviewcursor(VIEW *v)
{
    if (v->p.l >= v->b->n)
        v->p.l = v->b->n? v->b->n - 1 : 0;
}

void
fixcursor(EDITOR *e)
{
    fixviewcursor(&e->docview);
    fixviewcursor(&e->cmdview);
}

static bool
pbefore(POS p1, POS p2)
{
    return p1.l < p2.l || (p1.l == p2.l && p1.c < p2.c);
}
