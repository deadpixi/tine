# Building tine

`tine` is written in portable C99;
build using `make`:

    make

Various configuration options are available to change the C
compiler, and options passed to the C compiler to enable various
POSIX standards and locate header and library files for _curses_.


## Configuration

Makefile variables control various things:

- CC the C compiler; default: c99
- CURSES a command-line `#define` to find the curses header
  file; must define the C preprocessor token `CURSES_INCLUDE`;
  default: -DCURSES_INCLUDE="<ncursesw/ncurses.h>"
- LDFLAGS as usual, the linker flags (the curses library will
  usually need to be specified like this); default:
  -lncursesw

Definitions of `make` variable can be changed by either editing
the Makefile or supplying new variable definitions on the
command line.

### macOS

On my 2022 macOS laptop the following works to compile `tine`:

make 'CURSES=-DCURSES_INCLUDE=<curses.h>' LDFLAGS=-lcurses

# END
