#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <ncursesw/ncurses.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "buffer.h"
#include "command.h"
#include "editor.h"
#include "parser.h"
#include "util.h"

/* UTILITY FUNCTIONS */
static bool
prompt(EDITOR *e, const char *p)
{
    werase(e->cmdview.w);
    mvwaddstr(e->cmdview.w, 0, 0, p);
    wrefresh(e->cmdview.w);
    KEYSTROKE k = getkeystroke(e, true);
    werase(e->cmdview.w);
    return k.c == L'y' || k.c == L'Y';
}

static void
fixblock(VIEW *v)
{
    cleartag(v->b, BLOCK);
    if (v->bs == NONE || v->be == NONE)
        return;
    if (v->bs > v->be){
        lineno l = v->bs;
        v->bs = v->be;
        v->be = l;
    }
    settag(v->b, BLOCK, pos(v->bs, 0), pos(v->be, SIZE_MAX - 1), A_REVERSE);
}

static bool
setfind(EDITOR *e, const wchar_t *s, size_t n)
{
    free(e->find);
    e->find = NULL;
    e->findn = 0;

   if (n){
        e->find = dupstr(s, n);
        if (!e->find)
            return error(e, "Out of memory");
        e->findn = n;
    }
    return true;
}

static wint_t
identity(wint_t wc)
{
    return wc;
}

static bool
find(EDITOR *e, VIEW *v, POS op, bool r)
{
    BUFFER *b = v->b;
    bool (*iter)(const BUFFER *, POS *) = r? prev : next;
    bool (*atend)(const BUFFER *b, POS) = r? attop : atbot;
    wint_t (*xform)(wint_t w) = v->uc? towupper : identity;
    if (!e->find || !e->findn)
        return error(e, "Empty target");

    cleartag(b, HIGHLIGHT);
    do{
        if (!iter(b, &op))
            return error(e, "Search failed");
        POS p = op;
        size_t i = 0;
        while (i < e->findn && xform((wint_t)e->find[i]) == xform(charat(b, p))){
            i++;
            next(b, &p);
        }
        if (i == e->findn){
            v->p = op;
            settag(b, HIGHLIGHT, op, p, A_UNDERLINE | A_BOLD);
            if (e->focusview == &e->cmdview)
               settag(b, VIRTCURS, op, pos(op.l, op.c + 1), A_REVERSE);
            redisplay(&e->docview);
            return true;
        }
    } while (!atend(b, op));

    return error(e, "Search failed");
}

static bool
exchange(EDITOR *e, VIEW *v, const ARG *a, bool query)
{
    if (a->n1 && !setfind(e, a->s1, a->n1))
        return false;
    if (!find(e, v, v->p, false))
        return false;
    if (!query || prompt(e, "Exchange?")){
        if (!deletetext(v->b, v->p, a->n1)
        ||  !inserttext(v->b, v->p, a->s2, a->n2))
            return false;
    }
    v->p.c += a->n2;
    return true;
}

static size_t
trimlength(const LINE *l)
{
   size_t n = l->n;
   while (n && iswspace(l->s[n - 1]))
      n--;
   return n;
}

static bool
safewrite(int fd, const char *s, size_t n)
{
   ssize_t nw = 0;
   while ((size_t)nw < n){
      ssize_t w = write(fd, s + nw, n - nw);
      if (w < 0)
         return false;
      nw += w;
   }
   return true;
}

static bool
writelines(int fd, const BUFFER *b, lineno ls, lineno le)
{
   char *s = NULL;
   bool rc = true;
   for (lineno l = ls; rc && b->n && l <= le; l++){
      if (b->l[l].n){
         s = wstos(b->l[l].s, trimlength(&b->l[l]));
         if (!s)
            return false;
         if (!(rc = safewrite(fd, s, strlen(s)))){
            free(s);
            break;
         }
         free(s);
      }
      safewrite(fd, "\n", strlen("\n"));
   }
   close(fd);
   return true;
}

/* COMMAND DEFINITIONS */
enum{
   NOFLAGS      = 0,
   MARK         = 1L<<0,
   CLEARSBLOCK  = 1L<<1,
   NEEDSLINES   = 1L<<2,
   NEEDSBLOCK   = 1L<<3,
   SETSHILITE   = 1L<<4,
   NOLOCATOR    = 1L<<5
};

#define COMMAND(name, fflags)                                                    \
   bool                                                                          \
   cmd_ ## name (EDITOR *e, VIEW *v, const ARG *a)                               \
   {                                                                             \
      long flags = fflags;                                                       \
      BUFFER *b = v->b; (void)b;                                                 \
      POS p = v->p; (void)p;                                                     \
      lineno ol = p.l;                                                           \
      bool haslines = v->b->n; (void)haslines;                                   \
      bool rc = true;                                                            \
      if ((flags & NEEDSBLOCK) && (v->bs == NONE || v->be == NONE || !haslines)) \
         ERROR("No block marked");                                               \
      if (flags & MARK)                                                          \
         mark(v->b);                                                             \

#define END                                              \
      goto endfunc;                                      \
      endfunc:                                           \
      fixblock(&e->docview);                             \
      fixcursor(e);                                      \
      if (flags & CLEARSBLOCK){                          \
         cleartag(v->b, BLOCK);                          \
         v->bs = v->be = NONE;                           \
      }                                                  \
      if (!(flags & SETSHILITE))                         \
         cleartag(v->b, HIGHLIGHT);                      \
      if (v->p.l != ol)                                  \
         v->ex = false;                                  \
      if (!(flags & NOLOCATOR))                          \
         v->gb = v->p;                                   \
      return rc;                                         \
   }

#define RETURN(x) do{rc = (x); goto endfunc;}while(false)
#define ERROR(s) RETURN(error(e, s))
#define SUCCEED RETURN(true)
#define FAIL RETURN(false)

COMMAND(a, MARK | CLEARSBLOCK) /* insert line after current */
   RETURN(insertline(b, p.l + 1) && inserttext(b, pos(p.l + 1, 0), a->s1, a->n1));
END

COMMAND(b, MARK | NOLOCATOR) /* move to bottom of file */
   v->p = pos(b->n? b->n - 1 : 0, 0);
END

COMMAND(be, NOLOCATOR) /* block end at cursor line */
   v->be = p.l;
END

COMMAND(bf, MARK | SETSHILITE | NOLOCATOR) /* backwards find */
   RETURN((!a->n1 || setfind(e, a->s1, a->n1)) && find(e, v, p, true));
END

COMMAND(bm, NOLOCATOR) /* set bookmark */
   if (a->n1 == 0 || a->n1 > BM_MAX)
      ERROR("Invalid bookmark");
   e->bm[a->n1 - 1] = p;
END

COMMAND(bs, NOLOCATOR) /* block start at cursor line */
   v->bs = p.l;
END

COMMAND(cb, NEEDSBLOCK | CLEARSBLOCK | NOLOCATOR) /* clear block */
   /* called for side effect */
END

COMMAND(cd, MARK) /* cursor down, same column */
   if (!haslines || p.l >= b->n - 1)
      ERROR("End of file");
   v->p.l++;
END

COMMAND(ce, NOFLAGS) /* cursor to end of line */
   v->p = pos(p.l, b->n? b->l[p.l].n : 0);
END

COMMAND(cf, NOLOCATOR) /* call a function key */
   if (a->n1 >= FUNC_MAX || !e->funcs[a->n1])
      ERROR("Invalid function number");
   RETURN(runextended(e->funcs[a->n1], wcslen(e->funcs[a->n1]), e));
END

COMMAND(cj, NOFLAGS) /* cursor jump to EOL/BOL */
   if (!haslines)
      SUCCEED;
   LINE *l = &b->l[p.l];
   v->p.c = (p.c == l->n)? 0 : l->n;
END

COMMAND(cl, NOFLAGS) /* cursor left */
   colno c = v->p.c;
   if (v->p.c)
      v->p.c--;
   while (v->p.c && wcwidth(charat(b, v->p)) == 0)
      v->p.c--;
   if (c == v->p.c)
      ERROR("Beginning of line");
END

COMMAND(cm, NOLOCATOR) /* enter command mode */
    e->focusview = &e->cmdview;
    werase(e->cmdview.w);
    wrefresh(e->cmdview.w);
    refresh();
END

COMMAND(cr, NOFLAGS) /* cursor right */
   if (p.c >= SIZE_MAX - 1)
      ERROR("End of line");
   v->p.c++;
   while (v->p.c < SIZE_MAX - 1 && wcwidth(charat(b, v->p)) == 0)
      v->p.c++;
END

COMMAND(cs, NOFLAGS) /* cursor to start of line */
   v->p.c = 0;
END

COMMAND(ct, NOLOCATOR) /* collapse tabs */
   v->et = false;
END

COMMAND(cu, MARK) /* cursor up, same column */
   if (!haslines || !p.l)
      ERROR("Top of file");
   v->p.l--;
END

COMMAND(d, MARK | CLEARSBLOCK) /* delete line */
   if (!haslines)
      SUCCEED;
   free(v->dl);
   v->dl = dupstr(b->l[p.l].s, b->l[p.l].n);
   v->dln = b->l[p.l].n;
   if (!v->dl)
      ERROR("Out of memory");
   RETURN(deleteline(b, p.l));
END

COMMAND(db, MARK | NEEDSBLOCK | CLEARSBLOCK) /* delete block */
    size_t n = v->be - v->bs;
    for (size_t i = 0; i <= n && b->n; i++)
        deleteline(b, v->bs);
END

COMMAND(dc, NOFLAGS | NEEDSLINES) /* delete character at cursor */
   if (p.c < b->l[p.l].n && b->l[p.l].n)
      RETURN(deletetext(b, p, 1));
END

COMMAND(df, NOLOCATOR) /* display function definitions */
    WINDOW *w = e->docview.w;
    WINDOW *c = e->cmdview.w;
    int oc = curs_set(0);
    wattron(w, A_BOLD);
    werase(w);
    for (int i = 0; i < FUNC_MAX; i++){ /* XXX handle narrow windows */
        mvwprintw(w, i, 0, "Function key %2d: %ls", i + 1,
                  e->funcs[i]? e->funcs[i] : L"Not set");
    }
    mvwprintw(w, 12, 0, "Type any character to continue");
    wattroff(w, A_BOLD);
    wrefresh(w);
    werase(c);
    mvwprintw(c, 0, 0, "Display Functions");
    wrefresh(c);
    getkeystroke(e, true);
    if (oc != ERR)
      curs_set(oc);
END

COMMAND(dl, NOFLAGS) /* delete character to left */
   if (!p.c)
      ERROR("Beginning of line");
   v->p.c--;
   if (haslines && v->p.c < b->l[v->p.l].n)
      RETURN(deletetext(b, v->p, 1));
END

COMMAND(do, NOLOCATOR) /* do a shell command */
   if (!a->n1)
      ERROR("Empty command");
   char *s = wstos(a->s1, a->n1);
   if (!s)
      ERROR("Out of memory");
   endwin();
   system(s);
   puts("\nPress any key to continue");
   getkeystroke(e, true);
   refresh();
   free(s);
END

COMMAND(dp, MARK) /* delete previous */
   if (!v->p.c)
      ERROR("Beginning of line");
   bool cr = false;
   if (!iswspace(charat(b, v->p))){
      cr = true;
      cmd_cl(e, v, a);
   }

   while (iswspace(charat(b, v->p)) && cmd_dc(e, v, a) && cmd_cl(e, v, a))
      ;
   while (!iswspace(charat(b, v->p)) && cmd_dc(e, v, a) && cmd_cl(e, v, a))
      ;

   if (cr)
      cmd_cr(e, v, a);
END

COMMAND(dw, MARK) /* delete to end of current word */
   bool r = true;

   if (ateol(v->b, v->p))
       SUCCEED;
   if (iswspace(charat(b, v->p))){
       while (iswspace(charat(b, v->p)) && !ateol(b, v->p) && r)
           r = deletetext(b, v->p, 1);
   } else
       while (!iswspace(charat(b, v->p)) && !ateol(b, v->p) && r)
           r = deletetext(b, v->p, 1);
   RETURN(r);
END

COMMAND(e, MARK | NOLOCATOR) /* exchange s/t */
   RETURN(exchange(e, v, a, false));
END

COMMAND(el, MARK) /* delete to EOL */
   RETURN(!haslines || p.c >= b->l[p.l].n || deletetext(b, p, b->l[p.l].n - p.c));
END

COMMAND(ep, MARK | NOLOCATOR) /* go to beginning or end of page */
   size_t lines, cols;
   if (p.l >= b->n)
      SUCCEED;
   getmaxyx(v->w, lines, cols);
   (void)cols;
   lineno n = v->tos.l + lines - 1;
   if (v->tos.l + lines - 1 >= b->n)
       n = b->n - 1;
   LINE *l = &b->l[n];
   if (p.l != v->tos.l || p.c > 0)
       v->p = v->tos;
   else
       v->p = pos(n, l->n);
END

COMMAND(eq, MARK | NOLOCATOR) /* exchange with query */
   RETURN(exchange(e, v, a, true));
END

COMMAND(et, NOLOCATOR) /* expand tabs */
   v->et = true;
END

COMMAND(ex, NOLOCATOR) /* expand margins */
   v->ex = true;
END

COMMAND(f, MARK | SETSHILITE | NOLOCATOR) /* find forward */
    RETURN((!a->n1 || setfind(e, a->s1, a->n1)) && find(e, v, p, false));
END

COMMAND(fb, MARK | NEEDSBLOCK | CLEARSBLOCK) /* filter block through command */
   /* FIXME - we should two two pipes and a select instead of a temp file */
   POS wp = pos(v->bs, 0);
   char tfn[] = "/tmp/tineXXXXXX";
   int tfd = mkstemp(tfn);
   if (tfd < 0)
      ERROR("Could not open temporary file");

   int tochild[2] = {-1, -1};
   if (pipe(tochild) != 0){
      close(tfd);
      unlink(tfn);
      ERROR("Could not open pipe");
   }
   signal(SIGPIPE, SIG_IGN);

   char *s = wstos(a->s1, a->n1);
   if (!s){
      close(tfd); close(tochild[0]); close(tochild[1]); unlink(tfn);
      ERROR("Out of memory");
   }

   pid_t pid = fork();
   if (pid == -1)
      ERROR("Could not fork");
   else if (pid == 0){
      close(tochild[1]);
      if (dup2(tochild[0], STDIN_FILENO) == -1) exit(EXIT_FAILURE);
      if (dup2(tfd, STDOUT_FILENO) == -1)       exit(EXIT_FAILURE);
      if (dup2(tfd, STDERR_FILENO) == -1)       exit(EXIT_FAILURE);
      execl("/bin/sh", "sh", "-c", s, NULL);
   } else{
      free(s);
      close(tochild[0]);
      writelines(tochild[1], v->b, v->bs, v->be);
      while (waitpid(pid, NULL, WNOHANG) == 0){
         if (getkeystroke(e, false).o != ERR){
            kill(pid, SIGKILL);
            break;
         }
      }
   }
   close(tfd);
   close(tochild[1]);

   wchar_t *ws = stows(tfn, sizeof(tfn) - 1);
   if (!ws){
      free(ws); unlink(tfn);
      ERROR("Out of memory");
   }
   v->p = wp;
   ARG a1 = {.t = ARG_STRING, .s1 = ws, .n1 = wcslen(ws)};
   bool r = cmd_db(e, v, a) && cmd_if(e, v, &a1);
   free(ws); unlink(tfn);
   RETURN(r);
END

COMMAND(fc, NOFLAGS) /* flip case */
   wchar_t w = charat(b, p);
   bool r = true;
   w = iswupper(w)? towlower(w) : towupper(w);
   if (haslines && p.c < b->l[p.l].n)
      r = deletetext(b, p, 1) && inserttext(b, p, &w, 1);
   RETURN(r && cmd_cr(e, v, a));
END

COMMAND(gb, MARK | NOLOCATOR) /* go back to previous position */
   if (v->gb.l == NONE || v->gb.l >= b->n)
      ERROR("No previous position");
   v->p = v->gb;
END

COMMAND(gm, MARK | NOLOCATOR) /* go to mark */
   if (a->n1 == 0 || a->n1 > BM_MAX)
      ERROR("Invalid bookmark");
   size_t n = a->n1 - 1;
   if (e->bm[n].l == NONE || e->bm[n].l >= b->n)
      ERROR("Bookmark not set");
   v->p = e->bm[n];
END

COMMAND(i, MARK | CLEARSBLOCK) /* insert line before */
   RETURN(insertline(b, p.l) && inserttext(b, pos(p.l, 0), a->s1, a->n1));
END

COMMAND(ib, MARK | NEEDSBLOCK) /* insert block */
    if (p.l >= v->bs && p.l <= v->be)
        ERROR("Cursor inside block");

    size_t n = v->be - v->bs;
    lineno bs = p.l < v->bs? v->bs + n + 1 : v->bs;
    for (size_t i = 0; i <= n; i++){
        if (!insertline(v->b, v->p.l))
            FAIL;
    }

    for (size_t i = 0; i <= n; i++){
        const LINE *l = &v->b->l[bs + i];
        if (!inserttext(b, pos(p.l + i, 0), l->s, l->n))
            FAIL;
    }

    v->bs = bs;
    v->be = bs + n;
END

static bool
cmd_if_cb(const wchar_t *s, size_t n, void *p)
{
    VIEW *v = (VIEW *)p;
    v->p.c = 0;
    bool rc = insertline(v->b, v->p.l) && inserttext(v->b, v->p, s, n);
    v->p.l++;
    return rc;
}

COMMAND(if, MARK | CLEARSBLOCK) /* insert file */
    char *fn = wstos(a->s1, a->n1);
    if (!fn)
        ERROR("Out of memory");
    bool r = readfile(fn, cmd_if_cb, v);
    if (!r)
      snprintf(e->err, ERR_MAX, "Could not open file: %s", strerror(errno));
    v->p = p;
    free(fn);
    RETURN(r);
END

COMMAND(j, MARK | CLEARSBLOCK) /* join this line and next */
    if (b->n < 2 || p.l >= b->n - 1)
        ERROR("End of file");
    LINE *l1 = &b->l[p.l];
    LINE *l2 = &b->l[p.l + 1];
    RETURN(inserttext(b, pos(p.l, l1->n), l2->s, l2->n) && deleteline(b, p.l + 1));
END

COMMAND(lc, NOLOCATOR) /* case-sensitive searching */
   v->uc = false;
END

COMMAND(m, MARK | NOLOCATOR) /* move to line */
    if (a->n1 == NONE || a->n1 == 0 || a->n1 - 1 >= b->n || !b->n)
      ERROR("Invalid line");
    v->p.l = a->n1 - 1;
END

COMMAND(mc, NOLOCATOR) /* remap control key */
   if (a->n1 != 1 || a->n2 != 1 || !a->s1 || !a->s2)
      ERROR("Invalid mapping specification");
   wchar_t c1 = towupper(a->s1[0]) & 0x1f;
   wchar_t c2 = towupper(a->s2[0]) & 0x1f;
   if (!iswcntrl(c1) || !iswcntrl(c2) || c1 >= CTRL_MAX || c2 >= CTRL_MAX)
      ERROR("Invalid character specification");
   e->ctrlmap[c1] = c2;
END

COMMAND(n, MARK) /* move to beginning of next line */
   if (!haslines || p.l >= b->n - 1)
      ERROR("End of file");
   v->p = pos(p.l + 1, 0);
END

COMMAND(p, MARK) /* move to beginning of previous line */
   if (!haslines || !p.l)
      ERROR("End of file");
   v->p = pos(p.l - 1, 0);
END

COMMAND(pd, NOLOCATOR | MARK) /* page down */
   if (b->n < v->ph || b->n - v->tos.l <= v->ph || b->n - p.l <= v->ph)
      ERROR("End of file");
   v->tos.l += v->ph;
   v->p.l += v->ph;
END

COMMAND(ph, NOLOCATOR) /* define page height */
    if (a->n1 == 0)
      ERROR("Invalid page height");
    v->ph = a->n1;
END

COMMAND(pu, NOLOCATOR | MARK) /* page up */
   if (v->tos.l < v->ph || v->p.l < v->ph)
      ERROR("Top of file");
   v->tos.l -= v->ph;
   v->p.l -= v->ph;
END

COMMAND(q, NOLOCATOR) /* quit without save */
    if (e->docview.b->dirty && !prompt(e, "File has changed. Lose changes?"))
        FAIL;
    e->running = false;
END

COMMAND(qo, NOFLAGS) /* quote next */
   v->q = true;
END

COMMAND(qy, NOFLAGS) /* force quit without save */
    e->running = false;
END

static bool
iscomment(const wchar_t *s, size_t n)
{
   if (!n)
      return true;
   size_t i = 0;
   while (i < n && iswspace(s[i]))
      i++;
   return s[i] == L'\'' || s[i] == 0;
}

COMMAND(rd, CLEARSBLOCK) /* restore deleted */
   if (!v->dl)
      ERROR("No saved line");
   ARG a1 = {.t = ARG_STRING, .s1 = v->dl, .n1 = v->dln};
   return cmd_i(e, v, &a1);
END

static bool
cmd_rf_cb(const wchar_t *s, size_t n, void *p)
{
   if (!n || iscomment(s, n))
      return true;
   return runextended(s, n, (EDITOR *)p);
}

COMMAND(rf, NOLOCATOR) /* run command file */
    char *fn = wstos(a->s1, a->n1);
    if (!fn)
      ERROR("Out of memory");
    bool r = readfile(fn, cmd_rf_cb, e);
    free(fn);
    RETURN(r);
END

COMMAND(rm, NOLOCATOR) /* reset margins */
    v->rm = NONE;
    v->lm = 0;
END

static bool
runcommand(EDITOR *e, VIEW *v, const ARG *a, bool stay)
{
    if (!v->b->n || v->p.l >= v->b->n)
        return error(e, "Commands abandoned");
    e->err[0] = 0;
    bool rc = true;
    if (trimlength(&v->b->l[v->p.l])){
        rc = runextended(v->b->l[v->p.l].s, v->b->l[v->p.l].n, e);
        e->lc = v->p;
        if (v->p.l == v->b->n - 1) /* only add a newline if we're not repeating */
            insertline(v->b, v->b->n);
    }
    v->p = pos(v->b->n - 1, 0);
    werase(v->w);
    if (!stay)
        e->focusview = &e->docview;
    return rc;
}

COMMAND(rp, MARK | NOLOCATOR) /* repeat last command */
   if (e->lc.l == NONE)
      SUCCEED;
   e->cmdview.p = e->lc;
   RETURN(runcommand(e, &e->cmdview, a, false));
END

COMMAND(rs, MARK | NOLOCATOR) /* run extended command and stay */
    RETURN(runcommand(e, v, a, true));
END

COMMAND(ru, MARK | NOLOCATOR) /* run extended command */
   RETURN(runcommand(e, v, a, false));
END

COMMAND(s, MARK | CLEARSBLOCK) /* split line */
    if (!b->n && !insertline(b, p.l))
        ERROR("Out of memory");

    size_t lines, cols;
    getmaxyx(v->w, lines, cols);
    (void)cols;

    colno lm = v->lm == NONE? 0 : v->lm;
    size_t ln = b->l[p.l].n;
    size_t n = p.c >= ln? 0 : ln - p.c;
    if (!insertline(v->b, p.l + 1)
    ||  !inserttext(v->b, pos(p.l + 1, lm), b->l[p.l].s + p.c, n)
    ||  !deletetext(v->b, pos(p.l, p.c), n))
        ERROR("Out of memory");
    v->p = pos(p.l + 1, lm);
    if (b->n - v->tos.l >= lines + 1) /* emulate a quirk of ED */
        v->tos.l++;
END

COMMAND(sa, NOLOCATOR) /* save text to file */
    char *fn = strdup(e->name);
    if (a->t == ARG_STRING && a->n1){
        free(fn);
        fn = wstos(a->s1, a->n1);
    }
    if (!fn)
        ERROR("Out of memory");

    int fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0){
       free(fn);
       ERROR("Could not open file");
    }
    bool r = writelines(fd, b, 0, b->n? b->n - 1 : 0);
    if (rc)
        e->docview.b->dirty = false;
    free(fn);
    RETURN(r);
END

COMMAND(sb, NOLOCATOR | MARK) /* show block on screen */
    if (v->bs == NONE)
        ERROR("No block defined");
    v->p.l = v->bs;
END

COMMAND(se, CLEARSBLOCK | MARK) /* split line after move to end */
    RETURN(cmd_ce(e, v, a) && cmd_s(e, v, a));
END

COMMAND(sf, NOLOCATOR) /* set function key */
    if (!a->n1)
        ERROR("Invalid function number");

    long long n = wcstoll(a->s1, NULL, 10) - 1;
    if (n < 0 || n >= FUNC_MAX)
        ERROR("Invalid function number");

    free(e->funcs[n]);
    if ((e->funcs[n] = dupstr(a->s2, a->n2)) == NULL)
        ERROR("Out of memory");
END

COMMAND(sh, NOLOCATOR) /* show information */
    WINDOW *w = e->docview.w;
    WINDOW *c = e->cmdview.w;

    char *bs = NULL;
    char *be = NULL;
    int oc = curs_set(0);
    if (v->bs != NONE)
        bs = wstos(v->b->l[v->bs].s, v->b->l[v->bs].n);
    if (v->be != NONE)
        be = wstos(v->b->l[v->be].s, v->b->l[v->be].n);

    wattron(w, A_BOLD);
    werase(w);
    mvwprintw(w, 0, 0, "Editing file    %s", basename(e->name));
    mvwprintw(w, 1, 0, "Line count      %zu", v->b->n);
    mvwprintw(w, 2, 0, "Tab distance    %zu", v->ts);
    mvwprintw(w, 3, 0, "Page height     %zu", v->ph);
    mvwprintw(w, 4, 0, "Left margin     %zu", v->lm == NONE? 1 : v->lm + 1);
    if (v->rm == NONE)
        mvwprintw(w, 5, 0, "Right margin    Not set");
    else
        mvwprintw(w, 5, 0, "Right margin    %zu", v->rm + 1);
    mvwprintw(w, 6, 0, "Block start     %s", bs? bs : "Not set");
    mvwprintw(w, 7, 0, "Block end       %s", be? be : "Not set");
    mvwprintw(w, 9, 0, "Type any character to continue");
    wattroff(w, A_BOLD);
    wrefresh(w);
    free(bs);
    free(be);

    werase(c);
    mvwprintw(c, 0, 0,
        "tine Copyright (C) 2019 Rob King. See COPYING for details.");
    wrefresh(c);
    getkeystroke(e, true);
    if (oc != ERR)
       curs_set(oc);
END

COMMAND(sl, NOLOCATOR) /* set left margin */
    if (v->rm != NONE && a->n1 >= v->rm)
        ERROR("Left margin would exceed right margin");
    if (a->n1 >= SIZE_MAX - 1)
        ERROR("Invalid margin");
    v->lm = a->n1 > 0? a->n1 - 1 : p.c;
END

COMMAND(sm, MARK | NOLOCATOR) /* show matching brace */
   wint_t s = charat(b, p), m = 0;
   int c = 1;
   bool r = 0;
   switch (s){
      case L'(': m = L')';           break;
      case L')': m = L'('; r = true; break;
      case L'{': m = L'}';           break;
      case L'}': m = L'{'; r = true; break;
      case L'[': m = L']';           break;
      case L']': m = L'['; r = true; break;
      case L'<': m = L'>';           break;
      case L'>': m = L'<'; r = true; break;
      default: ERROR("Search failed");
   }

   bool (*iter)(const BUFFER *, POS *) = r? prev : next;
   bool (*atend)(const BUFFER *, POS) = r? attop : atbot;
   do{
      if (!iter(b, &p))
         ERROR("Search failed");
      if (charat(b, p) == s)
         c++;
      else if (charat(b, p) == m)
         c--;
      if (c == 0){
         v->p = p;
         SUCCEED;
      }
   } while (!atend(b, p));
   ERROR("Search failed");
END

COMMAND(sr, NOLOCATOR) /* set right margin */
    if (v->lm != NONE && a->n1 <= v->lm)
        ERROR("Right margin would exceed left margin");
    if (a->n1 >= SIZE_MAX - 1)
        ERROR("Invalid margin");
    v->rm = a->n1 > 0? a->n1 - 1 : p.c;
END

COMMAND(st, NOLOCATOR) /* set tab distance */
    if (a->n1 == 0 || a->n1 >= SIZE_MAX - 1)
        ERROR("Invalid tab width");
    v->ts = a->n1;
END

COMMAND(t, MARK | NOLOCATOR) /* move to top of file */
    v->p = pos(0, 0);
END

COMMAND(tb, NOFLAGS) /* move to next tabstop */
    if (v->et){
       ARG a2 = {.t = ARG_STRING, .n1 = 1, .s1 = L"\t"};
       RETURN(cmd_ty(e, v, &a2));
    }

    if (!cmd_cr(e, v, a))
      FAIL;
    while (v->p.c % v->ts){
       if (!cmd_cr(e, v, a))
         FAIL;
    }
END

static void
wordwrap(EDITOR *e, VIEW *v, const ARG *a)
{
   colno lm = v->lm == NONE? 0 : v->lm;
   POS p = pos(v->p.l, v->p.c - 1);
   while (!iswspace(charat(v->b, p)) && p.c > lm && p.c)
      p.c--;
   if (p.c > lm){

      v->p = pos(p.l, p.c + 1);
      cmd_s(e, v, a);
      cmd_ce(e, v, a);
      v->p.c = v->p.c < lm? lm : v->p.c;
   }
}

COMMAND(ty, NOFLAGS) /* type in characters */
    if (!haslines && !insertline(b, p.l))
        ERROR("Out of memory");

    for (size_t i = 0; i < a->n1; i++){
        if (v->rm != NONE && v->p.c != NONE && v->p.c && v->p.c == v->rm && !v->ex){
           if (iswspace(a->s1[i])){
              cmd_s(e, v, a);
              continue;
           }
           wordwrap(e, v, a);
        }
        if (!inserttext(b, v->p, a->s1 + i, 1))
            ERROR("Out of memory");
        v->p.c++;
    }
END

COMMAND(u, CLEARSBLOCK) /* undo */
    if (!b->j)
        ERROR("Nothing to undo");
    if (!undo(b, &p))
        ERROR("Out of memory");
    v->p = p;
END

COMMAND(uc, NOLOCATOR) /* case-insensitive searching */
    v->uc = true;
END

COMMAND(uk, NOLOCATOR) /* unknown command */
   ERROR("Unknown command");
END

COMMAND(vw, NOLOCATOR) /* verify window */
    redisplay(&e->docview);
    redisplay(&e->cmdview);
    if (v->statuscb)
       v->statuscb(e, v);
    redrawwin(e->docview.w);
    redrawwin(e->cmdview.w);
END

COMMAND(wb, NOLOCATOR) /* write block */
    if (v->bs == NONE || v->be == NONE)
        ERROR("No block defined");

    char *fn = strdup(e->name);
    if (a->t == ARG_STRING && a->n1){
        free(fn);
        fn = wstos(a->s1, a->n1);
    }
    if (!fn)
        ERROR("Out of memory");

    int fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0){
        free(fn);
        ERROR("Could not open file");
    }
    bool r = writelines(fd, v->b, v->bs, v->be);
    free(fn);
    RETURN(r);
END

COMMAND(wn, MARK) /* next word */
    while (!iswspace(charat(b, v->p)) && !atbot(b, v->p))
        next(b, &v->p);
    while (iswspace(charat(v->b, v->p)) && !atbot(v->b, v->p))
        next(b, &v->p);
    if (atbot(b, v->p))
        RETURN(cmd_cr(e, v, a)); /* emulate a quirk in ED */
END

COMMAND(wp, MARK) /* previous word */
    /* no previous words */
    if (attop(b, p))
        ERROR("Top of file");

    /* right after a word */
    if (iswspace(charat(b, v->p)) && prev(b, &v->p) && !attop(b, v->p)){
        while (!iswspace(charat(b, v->p)) && !attop(b, v->p))
            prev(b, &v->p);
        while (iswspace(charat(b, v->p)) && !attop(b, v->p))
            prev(b, &v->p);
    } else if (iswspace(charat(b, v->p))){ /* we're several spaces past a word */
        while (iswspace(charat(b, v->p)) && !attop(b, v->p))
            prev(b, &v->p);
    } else{ /* we're inside a word */
        while (!iswspace(charat(b, v->p)) && !attop(b, v->p))
            prev(b, &v->p);
        while (iswspace(charat(b, v->p)) && !attop(b, v->p))
            prev(b, &v->p);
    }
    if (!attop(b, v->p))
        cmd_cr(e, v, a);
END

COMMAND(x, NOFLAGS) /* exit with save */
    RETURN(cmd_sa(e, v, a) && cmd_q(e, v, a));
END

COMMAND(xq, NOLOCATOR) /* exit with save and query */
    if (!e->docview.b->dirty)
        RETURN(cmd_q(e, v, a));
    if (prompt(e, "File has been changed - Type Y to save and exit:"))
        RETURN(cmd_x(e, v, a));
    FAIL;
END

/* EXTENDED COMMAND LOOKUP */
static CMD cmdtab[] ={
    /* nm   argtype         req'd  func */
    {L"A",  ARG_STRING,     false, cmd_a},
    {L"B",  ARG_NONE,       true,  cmd_b},
    {L"BE", ARG_NONE,       true,  cmd_be},
    {L"BF", ARG_STRING,     false, cmd_bf},
    {L"BM", ARG_NUMBER,     true,  cmd_bm},
    {L"BS", ARG_NONE,       true,  cmd_bs},
    {L"CB", ARG_NONE,       true,  cmd_cb},
    {L"CD", ARG_NONE,       true,  cmd_cd},
    {L"CE", ARG_NONE,       true,  cmd_ce},
    {L"CF", ARG_NUMBER,     true,  cmd_cf},
    {L"CJ", ARG_NUMBER,     true,  cmd_cj},
    {L"CL", ARG_NONE,       true,  cmd_cl},
    {L"DP", ARG_NONE,       true,  cmd_dp},
    {L"CR", ARG_NONE,       true,  cmd_cr},
    {L"CS", ARG_NONE,       true,  cmd_cs},
    {L"CT", ARG_NONE,       true,  cmd_ct},
    {L"CU", ARG_NONE,       true,  cmd_cu},
    {L"D",  ARG_NONE,       true,  cmd_d},
    {L"DB", ARG_NONE,       true,  cmd_db},
    {L"DC", ARG_NONE,       true,  cmd_dc},
    {L"DF", ARG_NONE,       true,  cmd_df},
    {L"DL", ARG_NONE,       true,  cmd_dl},
    {L"DO", ARG_STRING,     true,  cmd_do},
    {L"DW", ARG_NONE,       true,  cmd_dw},
    {L"E",  ARG_EXCHANGE,   true,  cmd_e},
    {L"EL", ARG_NONE,       true,  cmd_el},
    {L"EP", ARG_NONE,       true,  cmd_ep},
    {L"EQ", ARG_EXCHANGE,   true,  cmd_eq},
    {L"ET", ARG_NONE,       true,  cmd_et},
    {L"EX", ARG_NONE,       true,  cmd_ex},
    {L"F",  ARG_STRING,     false, cmd_f},
    {L"FB", ARG_STRING,     true,  cmd_fb},
    {L"FC", ARG_NONE,       false, cmd_fc},
    {L"GB", ARG_NONE,       true,  cmd_gb},
    {L"GM", ARG_NUMBER,     true,  cmd_gm},
    {L"I",  ARG_STRING,     false, cmd_i},
    {L"IB", ARG_NONE,       true,  cmd_ib},
    {L"IF", ARG_STRING,     true,  cmd_if},
    {L"J",  ARG_NONE,       true,  cmd_j},
    {L"LC", ARG_NONE,       true,  cmd_lc},
    {L"M",  ARG_NUMBER,     true,  cmd_m},
    {L"MC", ARG_EXCHANGE,   true,  cmd_mc},
    {L"N",  ARG_NONE,       true,  cmd_n},
    {L"P",  ARG_NONE,       true,  cmd_p},
    {L"PD", ARG_NONE,       true,  cmd_pd},
    {L"PH", ARG_NUMBER,     true,  cmd_ph},
    {L"PU", ARG_NONE,       true,  cmd_pu},
    {L"Q",  ARG_STRING,     false, cmd_q},
    {L"QY", ARG_STRING,     false, cmd_qy},
    {L"RF", ARG_STRING,     true,  cmd_rf},
    {L"RM", ARG_NONE,       true,  cmd_rm},
    {L"SA", ARG_STRING,     false, cmd_sa},
    {L"S",  ARG_NONE,       true,  cmd_s},
    {L"SB", ARG_NONE,       true,  cmd_sb},
    {L"SF", ARG_EXCHANGE,   true,  cmd_sf},
    {L"SH", ARG_NONE,       true,  cmd_sh},
    {L"SL", ARG_NUMBER,     false, cmd_sl},
    {L"SM", ARG_NONE,       true,  cmd_sm},
    {L"SR", ARG_NUMBER,     false, cmd_sr},
    {L"ST", ARG_NUMBER,     true,  cmd_st},
    {L"T",  ARG_NONE,       true,  cmd_t},
    {L"TY", ARG_STRING,     true,  cmd_ty},
    {L"U",  ARG_NONE,       true,  cmd_u},
    {L"UC", ARG_NONE,       true,  cmd_uc},
    {L"WB", ARG_STRING,     true,  cmd_wb},
    {L"WN", ARG_NONE,       true,  cmd_wn},
    {L"WP", ARG_NONE,       true,  cmd_wp},
    {L"X",  ARG_NONE,       true,  cmd_x},
    {L"XQ", ARG_NONE,       true,  cmd_xq},
    {NULL,  ARG_NONE,       false, NULL}
};


const CMD *
lookup(const wchar_t *s)
{
    for (const CMD *c = cmdtab; c->n; c++){
        if (wcscmp(c->n, s) == 0)
            return c;
    }
    return false;
}

bool
call(const CMD *c, EDITOR *e, VIEW *v, const ARG *a)
{
    ARG d = {0};
    if (!c->required && a->t == ARG_NONE){
        d.t = c->a;
        a = &d;
    }
    if (a->t != c->a)
        return error(e, "Invalid argument type");
    bool rc = c->f(e, v, a);
    fixcursor(e);
    return rc;
}
