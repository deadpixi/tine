#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>
#include <stdlib.h>
#include <wchar.h>


typedef struct EDITOR EDITOR;
typedef struct VIEW VIEW;

#define CMD_NAME_MAX 2

typedef enum{
    ARG_NONE,
    ARG_STRING,
    ARG_EXCHANGE,
    ARG_NUMBER
} argtype;

typedef struct ARG ARG;
struct ARG{
    argtype t;
    size_t n1, n2;
    const wchar_t *s1, *s2;
};

typedef struct CMD CMD;
struct CMD{
   const wchar_t *n;
   argtype a;
   bool required;
   bool (*f)(EDITOR *, VIEW *, const ARG *);
};

const CMD *lookup(const wchar_t *s);
bool call(const CMD *c, EDITOR *e, VIEW *v, const ARG *a);

bool cmd_a(EDITOR *e, VIEW *v, const ARG *a); /* insert line after current */
bool cmd_b(EDITOR *e, VIEW *v, const ARG *a); /* move to bottom of file */
bool cmd_be(EDITOR *e, VIEW *v, const ARG *a); /* block end at cursor line */
bool cmd_bf(EDITOR *e, VIEW *v, const ARG *a); /* backwards find */
bool cmd_bm(EDITOR *e, VIEW *v, const ARG *a); /* set bookmark */
bool cmd_bs(EDITOR *e, VIEW *v, const ARG *a); /* block start at cursor line */
bool cmd_cb(EDITOR *e, VIEW *v, const ARG *a); /* clear block */
bool cmd_cd(EDITOR *e, VIEW *v, const ARG *a); /* cursor down, same column */
bool cmd_ce(EDITOR *e, VIEW *v, const ARG *a); /* cursor to end of line */
bool cmd_cf(EDITOR *e, VIEW *v, const ARG *a); /* call function key */
bool cmd_ch(EDITOR *e, VIEW *v, const ARG *a); /* clear highlight */
bool cmd_cj(EDITOR *e, VIEW *v, const ARG *a); /* cursor to end or beginning of line */
bool cmd_cm(EDITOR *e, VIEW *v, const ARG *a); /* change mode */
bool cmd_cl(EDITOR *e, VIEW *v, const ARG *a); /* cursor left */
bool cmd_cr(EDITOR *e, VIEW *v, const ARG *a); /* cursor right */
bool cmd_cs(EDITOR *e, VIEW *v, const ARG *a); /* cursor to start of line */
bool cmd_ct(EDITOR *e, VIEW *v, const ARG *a); /* collapse tabs */
bool cmd_cu(EDITOR *e, VIEW *v, const ARG *a); /* cursor up, same column */
bool cmd_d(EDITOR *e, VIEW *v, const ARG *a); /* delete line */
bool cmd_db(EDITOR *e, VIEW *v, const ARG *a); /* delete block */
bool cmd_dc(EDITOR *e, VIEW *v, const ARG *a); /* delete character at cursor */
bool cmd_df(EDITOR *e, VIEW *v, const ARG *a); /* display function definitions */
bool cmd_dl(EDITOR *e, VIEW *v, const ARG *a); /* delete character to left of cursor */
bool cmd_do(EDITOR *e, VIEW *v, const ARG *a); /* do a shell command */
bool cmd_dp(EDITOR *e, VIEW *v, const ARG *a); /* delete previous word */
bool cmd_dw(EDITOR *e, VIEW *v, const ARG *a); /* delete to end of current word */
bool cmd_e(EDITOR *e, VIEW *v, const ARG *a); /* exchange s/t */
bool cmd_el(EDITOR *e, VIEW *v, const ARG *a); /* erase to end of line */
bool cmd_ep(EDITOR *e, VIEW *v, const ARG *a); /* end/beginning of page */
bool cmd_eq(EDITOR *e, VIEW *v, const ARG *a); /* exchange s/t with query */
bool cmd_et(EDITOR *e, VIEW *v, const ARG *a); /* expand tabs */
bool cmd_ex(EDITOR *e, VIEW *v, const ARG *a); /* expand right margin */
bool cmd_f(EDITOR *e, VIEW *v, const ARG *a); /* find */
bool cmd_fb(EDITOR *e, VIEW *v, const ARG *a); /* filter block */
bool cmd_fc(EDITOR *e, VIEW *v, const ARG *a); /* flip case */
bool cmd_gb(EDITOR *e, VIEW *v, const ARG *a); /* go back */
bool cmd_gm(EDITOR *e, VIEW *v, const ARG *a); /* go to bookmark */
bool cmd_i(EDITOR *e, VIEW *v, const ARG *a); /* insert line before */
bool cmd_ib(EDITOR *e, VIEW *v, const ARG *a); /* insert block */
bool cmd_if(EDITOR *e, VIEW *v, const ARG *a); /* insert file */
bool cmd_j(EDITOR *e, VIEW *v, const ARG *a); /* join this line and next */
bool cmd_lc(EDITOR *e, VIEW *v, const ARG *a); /* case-sensitive searching */
bool cmd_m(EDITOR *e, VIEW *v, const ARG *a); /* move to line */
bool cmd_mc(EDITOR *e, VIEW *v, const ARG *a); /* remap control key */
bool cmd_n(EDITOR *e, VIEW *v, const ARG *a); /* move to beginning of next line */
bool cmd_p(EDITOR *e, VIEW *v, const ARG *a); /* move to beginning of previous line */
bool cmd_pd(EDITOR *e, VIEW *v, const ARG *a); /* page down */
bool cmd_ph(EDITOR *e, VIEW *v, const ARG *a); /* define page hieght */
bool cmd_pu(EDITOR *e, VIEW *v, const ARG *a); /* page up */
bool cmd_q(EDITOR *e, VIEW *v, const ARG *a); /* quit without save */
bool cmd_qo(EDITOR *e, VIEW *v, const ARG *a); /* quote next */
bool cmd_qy(EDITOR *e, VIEW *v, const ARG *a); /* quit without save */
bool cmd_rd(EDITOR *e, VIEW *v, const ARG *a); /* restore deleted */
bool cmd_rf(EDITOR *e, VIEW *v, const ARG *a); /* run command file */
bool cmd_rm(EDITOR *e, VIEW *v, const ARG *a); /* reset margins */
bool cmd_rp(EDITOR *e, VIEW *v, const ARG *a); /* execute last extended command */
bool cmd_ru(EDITOR *e, VIEW *v, const ARG *a); /* run extended command */
bool cmd_rs(EDITOR *e, VIEW *v, const ARG *a); /* run extended command */
bool cmd_s(EDITOR *e, VIEW *v, const ARG *a); /* split line */
bool cmd_sa(EDITOR *e, VIEW *v, const ARG *a); /* save text to file */
bool cmd_sb(EDITOR *e, VIEW *v, const ARG *a); /* show block on screen */
bool cmd_se(EDITOR *e, VIEW *v, const ARG *a); /* split line after moving to end */
bool cmd_sf(EDITOR *e, VIEW *v, const ARG *a); /* set function key */
bool cmd_sh(EDITOR *e, VIEW *v, const ARG *a); /* show information */
bool cmd_sl(EDITOR *e, VIEW *v, const ARG *a); /* set left margin */
bool cmd_sm(EDITOR *e, VIEW *v, const ARG *a); /* show matching brace */
bool cmd_sr(EDITOR *e, VIEW *v, const ARG *a); /* set right margin */
bool cmd_st(EDITOR *e, VIEW *v, const ARG *a); /* set tab distance */
bool cmd_t(EDITOR *e, VIEW *v, const ARG *a); /* move to top of file */
bool cmd_tb(EDITOR *e, VIEW *v, const ARG *a); /* move to next tabstop */
bool cmd_ty(EDITOR *e, VIEW *v, const ARG *a); /* type in characters */
bool cmd_u(EDITOR *e, VIEW *v, const ARG *a); /* undo */
bool cmd_uc(EDITOR *e, VIEW *v, const ARG *a); /* case-insensitive searching */
bool cmd_uk(EDITOR *e, VIEW *v, const ARG *a); /* unknown command */
bool cmd_vw(EDITOR *e, VIEW *v, const ARG *a); /* verify (redisplay) window */
bool cmd_wb(EDITOR *e, VIEW *v, const ARG *a); /* write block to file */
bool cmd_wn(EDITOR *e, VIEW *v, const ARG *a); /* next word */
bool cmd_wp(EDITOR *e, VIEW *v, const ARG *a); /* previous word */
bool cmd_x(EDITOR *e, VIEW *v, const ARG *a); /* exit with save */

#endif
