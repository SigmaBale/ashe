#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "acommon.h"
#include "autils.h"
#include "ainput.h"
#include "ashell.h"
#include "aasync.h"
#include "aprompt.h"
#ifdef ASHE_DBG
#include "adbg.h"
#endif

/* control sequence introducer */
#define CSI	 "\033["
#define ESC(seq) CSI #seq
/* cursor */
#define csi_cursor_home		  ESC(H)
#define csi_cursor_left(n)	  ESC(n) "D"
#define csi_cursor_right(n)	  ESC(n) "C"
#define csi_cursor_up(n)	  ESC(n) "A"
#define csi_cursor_down(n)	  ESC(n) "B"
#define csi_cursor_save		  ESC(s)
#define csi_cursor_load		  ESC(u)
#define csi_cursor_col(col)	  ESC(col) "G"
#define csi_cursor_move(row, col) ESC(row ";" col) "H"
#define csi_cursor_position	  ESC(6n)
#define csi_cursor_hide ESC(?25l)
#define csi_cursor_show ESC(?25h)
/* clear */
#define csi_clear_down	     ESC(0J)
#define csi_clear_up	     ESC(1J)
#define csi_clear_all	     ESC(2J)
#define csi_clear_line_right ESC(0K)
#define csi_clear_line_left  ESC(1K)
#define csi_clear_line	     ESC(2K)

/* key code defs */
#define CTRL_KEY(k)    ((k) & 0x1f)
#define ESCAPE	       27
#define CR	       0x0D
#define IMPLEMENTED(c) (c != ESCAPE)

/* miscellaneous defs */
#define modlen(x, y) ((((x)-1) % (y)) + 1)

/* draw buffer */
#define dbf_draw_lit(strlit) write_or_panic(strlit, sizeof(strlit) - 1)
#define dbf_pushlit(strlit)  dbf_push_len(strlit, sizeof(strlit) - 1)
#define dbf_pushc(c)	     a_arr_char_push(&DBF, c)
#define dbf_push(s)	     a_arr_char_push_str(&DBF, s, strlen(s))
#define dbf_push_len(s, len) a_arr_char_push_str(&DBF, s, len)
#define dbf_push_movecol(n)       \
	do {                      \
		dbf_pushlit(CSI); \
		dbf_push_unum(n); \
		dbf_pushc('G');   \
	} while (0)
#define dbf_push_moveup(n)        \
	do {                      \
		dbf_pushlit(CSI); \
		dbf_push_unum(n); \
		dbf_pushc('A');   \
	} while (0)

/* defines to insure these are inlined */
#define init_dflterm(term) tcgetattr(STDIN_FILENO, term);

#define set_terminal_mode(tmode)                                     \
	if (unlikely(tcsetattr(STDIN_FILENO, TCSAFLUSH, tmode) < 0)) \
		panic(NULL);

#define opost_on()                                              \
	do {                                                    \
		ashe.sh_term.tm_rawtermios.c_oflag |= OPOST;    \
		set_terminal_mode(&ashe.sh_term.tm_rawtermios); \
	} while (0)

#define opost_off()                                             \
	do {                                                    \
		ashe.sh_term.tm_rawtermios.c_oflag &= ~(OPOST); \
		set_terminal_mode(&ashe.sh_term.tm_rawtermios); \
	} while (0)

#define shift_lines_right(row)                   \
	for (uint32 i = row; i < LINES.len; i++) \
		LINES.data[i].start++;

#define shift_lines_left(row)                    \
	for (uint32 i = row; i < LINES.len; i++) \
		LINES.data[i].start--;

/* Implemented keys */
enum termkey {
	BACKSPACE = 127,
	L_ARW = 1000,
	U_ARW,
	D_ARW,
	R_ARW,
	HOME_KEY,
	END_KEY,
	DEL_KEY
};

ASHE_PRIVATE inline void write_or_panic(const char *ptr, memmax n)
{
	if (unlikely(write(STDERR_FILENO, ptr, n) < 0)) {
		ashe_perrno();
		panic(NULL);
	}
	fflush(stderr);
	if (unlikely(ferror(stderr))) {
		ashe_perrno();
		panic(NULL);
	}
}

ASHE_PRIVATE inline void dbf_push_unum(memmax n)
{
	char temp[UINT_DIGITS];
	int32 chars;

	if (unlikely((chars = snprintf(temp, UINT_DIGITS, "%zu", n)) < 0)) {
		ashe_perrno();
		panic(NULL);
	}
	a_arr_char_push_str(&DBF, temp, chars);
}

ASHE_PRIVATE inline void dbf_flush()
{
	a_arr_char *drawbuf = &DBF;
	opost_on();
	write_or_panic(drawbuf->data, drawbuf->len);
	opost_off();
	drawbuf->len = 0;
}

ASHE_PRIVATE int32 get_window_size_fallback(uint32 *height, uint32 *width)
{
	int32 res;

	dbf_draw_lit(csi_cursor_save csi_cursor_right(99999)
			     csi_cursor_down(99999));
	res = get_cursor_pos(height, width);
	dbf_draw_lit(csi_cursor_load);
	return res;
}

ASHE_PUBLIC int32 get_window_size(uint32 *height, uint32 *width)
{
	struct winsize ws;

	if (unlikely(ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) < 0 ||
		     ws.ws_col == 0)) {
		return get_window_size_fallback(height, width);
	}
	if (height)
		*height = ws.ws_row;
	if (width)
		*width = ws.ws_col;
	return 0;
}

ASHE_PUBLIC int32 get_cursor_pos(uint32 *row, uint32 *col)
{
	char c;
	memmax i = 0;
	int32 nread;
	uint32 srow, scol;
	char buf[INT_DIGITS * 2 + sizeof(CSI ";")]; /* ESC [ Pn ; Pn R */

	dbf_draw_lit(csi_cursor_position);
	while ((nread = read(STDIN_FILENO, &c, 1)) == 1) {
		if (c == 'R')
			break;
		buf[i++] = c;
	}
	if (nread == -1 || sscanf(buf, "\033[%u;%u", &srow, &scol) != 2) {
		ashe_perrno();
		return -1;
	}
	if (row)
		*row = srow;
	if (col)
		*col = scol;
	return 0;
}

/* Auxiliary to 'Terminal_init()' */
ASHE_PRIVATE void init_rawterm(struct termios *rawterm)
{
	tcgetattr(STDIN_FILENO, rawterm);
	rawterm->c_iflag &= ~(BRKINT | INPCK | ISTRIP | IXON | ICRNL);
	rawterm->c_oflag &= ~(OPOST);
	rawterm->c_lflag &= ~(ECHO | ICANON | IEXTEN);
	rawterm->c_cflag |= (CS8);
	rawterm->c_cc[VMIN] = 1;
	rawterm->c_cc[VTIME] = 0;
}

ASHE_PUBLIC void TerminalInput_init(void)
{
	a_arr_char_init_cap(&IBF, 8);
	a_arr_char_init_cap(&DBF, 8);
	a_arr_line_init(&LINES);
	a_arr_line_push(&LINES, (struct a_line){ .len = 0, .start = IBF.data });
	IBFIDX = 0;
	COL = 0;
	ROW = 0;
}

ASHE_PUBLIC void TerminalInput_free(void)
{
	a_arr_char_free(&IBF, NULL);
	a_arr_char_free(&DBF, NULL);
	a_arr_line_free(&LINES, NULL);
}

ASHE_PUBLIC void Terminal_init(void)
{
	TerminalInput_init();
	init_dflterm(&TM.tm_dfltermios);
	init_rawterm(&TM.tm_rawtermios);
	get_winsize_or_panic(&TM.tm_rows, &TM.tm_columns);
	/* tm_col - gets set when reading and drawing */
	/* tm_promptlen - gets set in 'print_prompt()' */
	TM.tm_reading = 0;
}

ASHE_PUBLIC ubyte ashe_cursor_up(void)
{
	uint32 extra = ((ROW == 0) * PLEN);
	uint32 len = COL + 1 + extra;
	uint32 pwraps = (PLEN != 0) * ((PLEN - 1) / TCOLMAX);
	uint32 lwraps = (len - 1) / TCOLMAX;
	uint32 temp;

	temp = lwraps - pwraps;
	if (ROW == 0 && (temp == 0 || (temp < 2 && len % TCOLMAX == 0)))
		return 0;
	if (ROW == 0) {
		if (temp == 1 &&
		    (temp = modlen(PLEN, TCOLMAX)) >= modlen(len, TCOLMAX)) {
start:
			COL = 0;
			IBFIDX = 0;
			TCOL = temp + 1;
			goto uptocol_draw;
		} else {
			goto up;
		}
	} else if (lwraps == 0) {
		IBFIDX -= COL + 1; /* end of the line above */
		ROW--;
		extra = (ROW == 0) * PLEN;
		len = LINE.len + extra;
		lwraps = (len - 1) / TCOLMAX;
		if ((temp = modlen(len, TCOLMAX)) <= COL + 1) {
			COL = LINE.len - 1;
			TCOL = temp;
			goto uptocol_draw;
		} else {
			temp = modlen(COL + 1, TCOLMAX);
			if (ROW == 0 && lwraps - pwraps == 0 &&
			    modlen(PLEN, TCOLMAX) >= temp) {
				temp = modlen(PLEN + 1, TCOLMAX);
				goto start;
			}
			temp = modlen(len, TCOLMAX) - temp;
			COL = LINE.len - temp - 1;
			IBFIDX -= temp;
			goto up_draw;
		}
	} else { /* lwraps > 0 */
up:
		COL -= TCOLMAX;
		IBFIDX -= TCOLMAX;
up_draw:
		dbf_pushlit(csi_cursor_up(1));
		goto flush;
	}
uptocol_draw:
	dbf_pushlit(csi_cursor_up(1));
	dbf_push_movecol(TCOL);
flush:
	dbf_flush();
	return 1;
}

// clang-format off
ASHE_PUBLIC ubyte ashe_cursor_down(void)
{
	uint32 extra = (ROW == 0) * PLEN;
	uint32 linewraps = (LINE.len != 0) * ((LINE.len - 1 + extra) / TCOLMAX);
	uint32 colwraps = (COL != 0) * ((COL + extra) / TCOLMAX);
	ssize wrapdepth = linewraps - colwraps;
	ubyte temp; /* for edge cases */

	if (wrapdepth > 0) {
		if (wrapdepth > 1 || COL + extra + TCOLMAX <= LINE.len + extra - 1) {
			COL += TCOLMAX;
			IBFIDX += TCOLMAX;
			goto down;
		} else {
			temp = (ROW != LINES.len - 1);
			IBFIDX += LINE.len - COL - temp;
			COL = LINE.len - temp;
			TCOL = modlen(LINE.len + extra + !temp, TCOLMAX);
			goto downtocol;
		}
	} else if (ROW < LINES.len - 1) {
		IBFIDX += LINE.len - COL; /* start of new line */
		ROW++;
		if (LINE.len >= TCOLMAX || LINE.len >= modlen(COL + 1 + extra, TCOLMAX)) {
			IBFIDX += TCOL - 1;
			COL = TCOL - 1;
down:
			dbf_pushlit(csi_cursor_down(1));
			goto flush;
		} else {
			temp = (ROW != LINES.len - 1);
			IBFIDX += LINE.len - temp;
			COL = LINE.len - temp;
			TCOL = LINE.len + !temp;
downtocol:
			dbf_pushlit(csi_cursor_down(1));
			dbf_push_movecol(TCOL);
flush:
			dbf_flush();
		}
		return 1;
	}
	return 0;
}
// clang-format on

ASHE_PUBLIC ubyte ashe_cursor_left(void)
{
	if (COL > 0) {
		COL--;
		if (TCOL == 1) {
			TCOL = TCOLMAX;
			goto lineuptocol;
		} else {
			TCOL--;
			dbf_draw_lit(csi_cursor_left(1));
		}
	} else if (ROW > 0) {
		ROW--;
		COL = LINE.len - 1;
		TCOL = modlen(LINE.len + ((ROW == 0) * PLEN), TCOLMAX);
lineuptocol:
		dbf_pushlit(csi_cursor_up(1));
		dbf_push_movecol(TCOL);
		dbf_flush();
	} else {
		return 0;
	}
	IBFIDX--;
	return 1;
}

ASHE_PUBLIC ubyte ashe_cursor_right(void)
{
	if (COL < LINE.len - (LINES.len != 0 && ROW != LINES.len - 1)) {
		COL++;
		if (TCOL < TCOLMAX) {
			TCOL++;
			dbf_draw_lit(csi_cursor_right(1));
		} else {
			goto firstcoldown;
		}
	} else if (ROW < (LINES.len - 1)) {
		ROW++;
		COL = 0;
firstcoldown:
		TCOL = 1;
		dbf_draw_lit(csi_cursor_down(1) csi_cursor_col(1));
	} else {
		return 0;
	}
	IBFIDX++;
	return 1;
}

ASHE_PUBLIC ubyte ashe_insert(int32 c)
{
	memmax i;
	uint32 idx;
	ubyte relink;
	struct a_line *prev = NULL;
	struct a_line *curr = NULL;

	if (IBF.len >= ARG_MAX - 1)
		return 0;
	idx = IBFIDX;
	relink = (IBF.len >= IBF.cap);
	a_arr_char_insert(&IBF, IBFIDX, c);
	LINE.len++;
	prev = &LINES.data[0];
	prev->start = IBF.data;
	for (i = 1; i < relink * LINES.len; i++) {
		curr = a_arr_line_index(&LINES, i);
		curr->start = prev->start + prev->len;
		prev = curr;
	}
	shift_lines_right(ROW + 1);
	dbf_pushlit(csi_clear_line_right csi_clear_down);
	if (c == '\n' && TCOL < TCOLMAX) {
		dbf_pushc('\n');
		idx++;
	}
	dbf_pushlit(csi_cursor_save);
	dbf_push_len(&IBF.data[idx], IBF.len - idx);
	dbf_pushlit(csi_cursor_load);
	dbf_flush();
	if (c != '\n') /* otherwise we are in 'TerminalInput_cr()' */
		ashe_cursor_right();
	return 1;
}

ASHE_PUBLIC ubyte ashe_cr(void)
{
	struct a_line newline = { 0 };

	if (is_escaped(IBF.data, IBFIDX) || in_dq(IBF.data, IBFIDX)) {
		ashe_insert('\n');
		newline.start = LINE.start + COL + 1;
		newline.len = LINE.len - (COL + 1);
		LINE.len = COL + 1;
		ROW++;
		IBFIDX++;
		COL = 0;
		TCOL = 1;
		a_arr_line_insert(&LINES, ROW, newline);
		return 1;
	}
	return 0;
}

ASHE_PUBLIC ubyte ashe_remove(void)
{
	struct a_line *l;
	ubyte coalesce;

	if (IBFIDX <= 0)
		return 0;
	a_arr_char_remove(&IBF, IBFIDX - 1);
	shift_lines_left(ROW + 1);
	LINE.len -= !(coalesce = (COL == 0));
	l = &LINE; /* cache current line */
	ashe_cursor_left();
	if (coalesce) {
		LINE.len--; /* '\n' */
		LINE.len += l->len;
		ashe_assert(l == &LINES.data[ROW + 1]);
		ashe_assert(l->start == LINES.data[ROW + 1].start);
		ashe_assert(l->len == LINES.data[ROW + 1].len);
		a_arr_line_remove(&LINES, ROW + 1);
	}
	dbf_pushlit(csi_cursor_hide csi_clear_line_right csi_clear_down
			    csi_cursor_save);
	dbf_push_len(IBF.data + IBFIDX, IBF.len - IBFIDX);
	dbf_pushlit(csi_cursor_load csi_cursor_show);
	dbf_flush();
	return 1;
}

ASHE_PUBLIC ubyte ashe_cursor_lineend(void)
{
	uint32 extra = (ROW == 0) * PLEN;
	uint32 col = COL + extra;
	uint32 len = LINE.len + extra;
	uint32 temp;
	ubyte lastrow = (ROW == LINES.len - 1);

	if (LINE.len > 0 && ((temp = modlen(col + 1, TCOLMAX)) != TCOLMAX ||
			     COL != LINE.len - !lastrow)) {
		if (col / TCOLMAX < (len - 1) / TCOLMAX) {
			TCOL = TCOLMAX;
			COL += TCOLMAX - temp;
			IBFIDX += TCOLMAX - temp;
		} else {
			TCOL = (len % TCOLMAX) + lastrow;
			IBFIDX += LINE.len - COL - !lastrow;
			COL = LINE.len - !lastrow;
		}
		dbf_push_movecol(TCOL);
		dbf_flush();
		return 1;
	}
	return 0;
}

ASHE_PUBLIC ubyte ashe_cursor_linestart(void)
{
	uint32 extra = (ROW == 0) * PLEN;
	uint32 col = COL + extra;
	ssize temp;

	if (COL != 0 && (temp = modlen(col + 1, TCOLMAX)) != 1) {
		if (col < TCOLMAX) {
			IBFIDX -= COL;
			COL = 0;
			TCOL = extra + 1;
		} else {
			IBFIDX -= temp - 1;
			COL -= temp - 1;
			col = COL + extra;
			TCOL = modlen(col + 1, TCOLMAX);
		}
		dbf_push_movecol(TCOL);
		dbf_flush();
		return 1;
	}
	return 0;
}

ASHE_PUBLIC void ashe_redraw(void)
{
	uint32 col = COL + (ROW == 0) * PLEN;

	TCOL = modlen(col + 1, TCOLMAX);
	dbf_pushlit(csi_cursor_hide);
	dbf_push_len(IBF.data, IBFIDX);
	dbf_pushlit(csi_cursor_save);
	dbf_push_len(IBF.data + IBFIDX, IBF.len - IBFIDX);
	dbf_pushlit(csi_cursor_load csi_cursor_show);
	dbf_flush();
}

ASHE_PUBLIC void ashe_clear_screen_raw(void)
{
	dbf_draw_lit(csi_cursor_home csi_clear_all);
}

ASHE_PUBLIC void ashe_clear_screen(void)
{
	dbf_draw_lit(csi_cursor_home csi_clear_all);
	prompt_print();
	ashe_redraw();
}

/* Move cursor to the end of the input */
ASHE_PUBLIC void ashe_cursor_end(void)
{
	while (ashe_cursor_down())
		;
	ashe_cursor_lineend();
}

ASHE_PRIVATE enum termkey read_key(void)
{
	ubyte seq[3];
	int32 nread;
	byte c;

	ashe_mask_signals(SIG_UNBLOCK);
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (unlikely(nread == -1 && (errno != EINTR && !ashe.sh_int))) {
			ashe_mask_signals(SIG_BLOCK);
			panic("failed reading terminal input.");
		}
		ashe.sh_int = 0;
	}
	ashe_mask_signals(SIG_BLOCK);

	if (c == ESCAPE) {
		if (read(STDIN_FILENO, &seq[0], 1) != 1)
			return ESCAPE;
		if (read(STDIN_FILENO, &seq[1], 1) != 1)
			return ESCAPE;
		if (seq[0] == '[') {
			if (isdigit(seq[1])) {
				if (read(STDIN_FILENO, &seq[2], 1) != 1)
					return ESCAPE;
				if (seq[2] == '~') {
					switch (seq[1]) {
					case '3':
						return DEL_KEY;
					case '1':
					case '7':
						return HOME_KEY;
					case '4':
					case '8':
						return END_KEY;
					default:
						break;
					}
				}
			} else {
				switch (seq[1]) {
				case 'C':
					return R_ARW;
				case 'D':
					return L_ARW;
				case 'B':
					return D_ARW;
				case 'A':
					return U_ARW;
				case 'H':
					return HOME_KEY;
				case 'F':
					return END_KEY;
				default:
					break;
				}
			}
		} else if (seq[0] == 'O') {
			switch (seq[1]) {
			case 'H':
				return HOME_KEY;
			case 'F':
				return HOME_KEY;
			default:
				break;
			}
		}
		return ESCAPE;
	} else {
		return c;
	}
}

ASHE_PRIVATE ubyte process_key(void)
{
	int32 c;

	if (IMPLEMENTED((c = read_key()))) {
		switch (c) {
		case CR:
			if (!ashe_cr())
				return 0;
			break;
		case DEL_KEY:
		case BACKSPACE:
			ashe_remove();
			break;
		case END_KEY:
			ashe_cursor_lineend();
			break;
		case HOME_KEY:
			ashe_cursor_linestart();
			break;
		case CTRL_KEY('h'): // TODO: change back to 'l' after testing
			ashe_clear_screen();
			break;
		case L_ARW:
			ashe_cursor_left();
			break;
		case R_ARW:
			ashe_cursor_right();
			break;
		case U_ARW:
			ashe_cursor_up();
			break;
		case D_ARW:
			ashe_cursor_down();
			break;
		// case CTRL_KEY('h'): // TODO: Uncomment after testing
		case CTRL_KEY('x'):
			_exit(255); // TODO: Remove this after testing
		case CTRL_KEY('j'):
		case CTRL_KEY('k'):
		case CTRL_KEY('i'):
			break;
		default:
			if (isgraph(c) || isspace(c))
				ashe_insert(c);
			break;
		}
	}
#ifdef ASHE_DBG_CURSOR
	debug_cursor();
#endif
#ifdef ASHE_DBG_LINES
	debug_lines();
#endif
	return 1;
}

/* Read input from STDIN_FILENO. */
ASHE_PUBLIC void TerminalInput_read(void)
{
	ashe_assert(IBF.data != NULL);
	ashe_assert(DBF.data != NULL);
	ashe_mask_signals(SIG_BLOCK);
	TM.tm_reading = 1;
	set_terminal_mode(&TM.tm_rawtermios);
	get_winsize_or_panic(&TM.tm_rows, &TM.tm_columns);
	get_cursor_pos(NULL, &TCOL);
#ifdef ASHE_DBG_CURSOR
	debug_cursor();
#endif
#ifdef ASHE_DBG_LINES
	debug_lines();
#endif
	while (process_key())
		;
	a_arr_char_push(&IBF, '\0');
	ashe_cursor_end();
	ashe_print("\r\n", stderr);
	TM.tm_reading = 0;
	set_terminal_mode(&TM.tm_dfltermios);
}
