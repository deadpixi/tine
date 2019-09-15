#include <errno.h>
#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include "editor.h"
#include "util.h"

static EDITOR *editor;
static WINDOW *cmdwin;

static int
quit(const char *m, int rc)
{
    if (editor)
        closeeditor(editor);
    if (cmdwin)
        endwin();
    if (m)
        fputs(m, stderr);
    exit(rc);
}

static int
setupcommand(WINDOW *w, int i)
{
    (void)i;
    cmdwin = w;
    wbkgdset(w, A_REVERSE);
    werase(w);
    return OK;
}

static void
initializescreen(void)
{
    ripoffline(-1, setupcommand);
    if (!initscr())
        quit("could not initialize screen", EXIT_FAILURE);
    raw();
    noecho();
    nonl();
    keypad(stdscr, TRUE);
    keypad(cmdwin, TRUE);
    intrflush(stdscr, FALSE);
}

static bool
loadfile(const char *fn)
{
    wchar_t *s = stows(fn, strlen(fn));
    if (!s)
        quit("Out of memory\n", EXIT_FAILURE);
    ARG a = {.t = ARG_STRING, .s1 = s, .n1 = wcslen(s)};
    errno = 0;
    bool rc = cmd_if(editor, &editor->docview, &a);
    free(s);
    return rc;
}

static void
runfile(const char *fn)
{
    wchar_t *s = stows(fn, strlen(fn));
    if (!s)
        quit("out of memory\n", EXIT_FAILURE);
    ARG a = {.t = ARG_STRING, .s1 = s, .n1 = wcslen(s)};
    cmd_rf(editor, &editor->docview, &a);
    free(s);
}

static bool
tryrun(const char *p, const char *n)
{
   char fn[FILENAME_MAX + 1] = {0};
   snprintf(fn, FILENAME_MAX, "%s/%s", p, n);
   if (access(fn, R_OK) == 0){
      runfile(fn);
      return true;
   }
   return false;
}

static const char *
getdef(const char *env, const char *def)
{
   if (getenv(env))
      return getenv(env);
   return def;
}

static void
runstartup(const char *ext)
{
   char def[FILENAME_MAX + 1] = {0};
   char path[FILENAME_MAX + 1] = {0};
   char *s = NULL;

   snprintf(def, FILENAME_MAX, "%s/.config", getdef("HOME", "/"));
   snprintf(path, FILENAME_MAX, "tine/%s", ext);
   if (tryrun(getdef("XDG_CONFIG_HOME", def), path))
      return;
   if ((s = strdup(getdef("XDG_CONFIG_DIRS", "/etc/xdg")))){
      for (char *n = strtok(s, ":"); n; n = strtok(NULL, ":")){
         if (tryrun(n, path))
            break;
      }
      free(s);
      return;
   }
}

static void
runstartupfiles(const char *fn)
{
   char path[FILENAME_MAX + 1] = {0};
   snprintf(path, FILENAME_MAX, "tinerc.%s", strrchr(fn, '.')? strrchr(fn, '.') + 1 : fn);
   runstartup("tinerc");
   runstartup(path);
}

static void
run(void)
{
    if (editor->focusview->statuscb)
        editor->focusview->statuscb(editor, editor->focusview);
    redisplay(editor->focusview);

    while (editor->running){
        KEYSTROKE k = getkeystroke(editor, true);
        if (k.o == ERR)
            continue;
        dispatch(editor, editor->focusview, k);
        if (editor->focusview->statuscb)
            editor->focusview->statuscb(editor, editor->focusview);
        redisplay(editor->focusview);
    }
}

int
main(int argc, char **argv)
{
    setlocale(LC_ALL, "");

    int o = 0;
    bool runrc = true;
    while ((o = getopt(argc, argv, "n")) != -1){
       switch (o){
          case 'n':
            runrc = false;
            break;
          default:
            quit("usage: tine [-n] FILE [CMDFILE|+LINE]...\n", EXIT_FAILURE);
            break;
       }
    }
    argc -= optind;
    argv += optind;

    if (argc < 1)
        quit("usage: tine [-n] FILE [CMDFILE|+LINE]...\n", EXIT_FAILURE);

    initializescreen();
    if ((editor = openeditor(argv[0], stdscr, cmdwin)) == NULL)
        return fputs("out of memory", stderr), EXIT_FAILURE;
    if (loadfile(argv[0]) && runrc)
       runstartupfiles(argv[0]);
    enableundo(editor->docview.b);
    editor->docview.b->dirty = false;

    for (int i = 1; i < argc; i++){
        if (argv[i][0] == '+' && isdigit(argv[i][1])){
            ARG a = {.t = ARG_NUMBER, .n1 = (size_t)atol(argv[i] + 1)};
            cmd_m(editor, &editor->docview, &a);
        } else
            runfile(argv[i]);
    }

    run();

    return quit(NULL, EXIT_SUCCESS);
}
