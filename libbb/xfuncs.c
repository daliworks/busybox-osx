/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2006 Rob Landley
 * Copyright (C) 2006 Denys Vlasenko
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

/* We need to have separate xfuncs.c and xfuncs_printf.c because
 * with current linkers, even with section garbage collection,
 * if *.o module references any of XXXprintf functions, you pull in
 * entire printf machinery. Even if you do not use the function
 * which uses XXXprintf.
 *
 * xfuncs.c contains functions (not necessarily xfuncs)
 * which do not pull in printf, directly or indirectly.
 * xfunc_printf.c contains those which do.
 *
 * TODO: move xmalloc() and xatonum() here.
 */

#include "libbb.h"

/* Turn on nonblocking I/O on a fd */
int FAST_FUNC ndelay_on(int fd)
{
	return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

int FAST_FUNC ndelay_off(int fd)
{
	return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
}

int FAST_FUNC close_on_exec_on(int fd)
{
	return fcntl(fd, F_SETFD, FD_CLOEXEC);
}

char* FAST_FUNC strncpy_IFNAMSIZ(char *dst, const char *src)
{
#ifndef IFNAMSIZ
	enum { IFNAMSIZ = 16 };
#endif
	return strncpy(dst, src, IFNAMSIZ);
}


// Convert unsigned integer to ascii, writing into supplied buffer.
// A truncated result contains the first few digits of the result ala strncpy.
// Returns a pointer past last generated digit, does _not_ store NUL.
void BUG_sizeof_unsigned_not_4(void);
char* FAST_FUNC utoa_to_buf(unsigned n, char *buf, unsigned buflen)
{
	unsigned i, out, res;
	if (sizeof(unsigned) != 4)
		BUG_sizeof_unsigned_not_4();
	if (buflen) {
		out = 0;
		for (i = 1000000000; i; i /= 10) {
			res = n / i;
			if (res || out || i == 1) {
				if (!--buflen) break;
				out++;
				n -= res*i;
				*buf++ = '0' + res;
			}
		}
	}
	return buf;
}

/* Convert signed integer to ascii, like utoa_to_buf() */
char* FAST_FUNC itoa_to_buf(int n, char *buf, unsigned buflen)
{
	if (buflen && n < 0) {
		n = -n;
		*buf++ = '-';
		buflen--;
	}
	return utoa_to_buf((unsigned)n, buf, buflen);
}

// The following two functions use a static buffer, so calling either one a
// second time will overwrite previous results.
//
// The largest 32 bit integer is -2 billion plus null terminator, or 12 bytes.
// It so happens that sizeof(int) * 3 is enough for 32+ bits.
// (sizeof(int) * 3 + 2 is correct for any width, even 8-bit)

static char local_buf[sizeof(int) * 3];

// Convert unsigned integer to ascii using a static buffer (returned).
char* FAST_FUNC utoa(unsigned n)
{
	*(utoa_to_buf(n, local_buf, sizeof(local_buf))) = '\0';

	return local_buf;
}

/* Convert signed integer to ascii using a static buffer (returned). */
char* FAST_FUNC itoa(int n)
{
	*(itoa_to_buf(n, local_buf, sizeof(local_buf))) = '\0';

	return local_buf;
}

/* Emit a string of hex representation of bytes */
char* FAST_FUNC bin2hex(char *p, const char *cp, int count)
{
	while (count) {
		unsigned char c = *cp++;
		/* put lowercase hex digits */
		*p++ = 0x20 | bb_hexdigits_upcase[c >> 4];
		*p++ = 0x20 | bb_hexdigits_upcase[c & 0xf];
		count--;
	}
	return p;
}

/* Convert "[x]x[:][x]x[:][x]x[:][x]x" hex string to binary, no more than COUNT bytes */
char* FAST_FUNC hex2bin(char *dst, const char *str, int count)
{
	errno = EINVAL;
	while (*str && count) {
		uint8_t val;
		uint8_t c = *str++;
		if (isdigit(c))
			val = c - '0';
		else if ((c|0x20) >= 'a' && (c|0x20) <= 'f')
			val = (c|0x20) - ('a' - 10);
		else
			return NULL;
		val <<= 4;
		c = *str;
		if (isdigit(c))
			val |= c - '0';
		else if ((c|0x20) >= 'a' && (c|0x20) <= 'f')
			val |= (c|0x20) - ('a' - 10);
		else if (c == ':' || c == '\0')
			val >>= 4;
		else
			return NULL;

		*dst++ = val;
		if (c != '\0')
			str++;
		if (*str == ':')
			str++;
		count--;
	}
	errno = (*str ? ERANGE : 0);
	return dst;
}

/* Return how long the file at fd is, if there's any way to determine it. */
#ifdef UNUSED
off_t FAST_FUNC fdlength(int fd)
{
	off_t bottom = 0, top = 0, pos;
	long size;

	// If the ioctl works for this, return it.

	if (ioctl(fd, BLKGETSIZE, &size) >= 0) return size*512;

	// FIXME: explain why lseek(SEEK_END) is not used here!

	// If not, do a binary search for the last location we can read.  (Some
	// block devices don't do BLKGETSIZE right.)

	do {
		char temp;

		pos = bottom + (top - bottom) / 2;

		// If we can read from the current location, it's bigger.

		if (lseek(fd, pos, SEEK_SET)>=0 && safe_read(fd, &temp, 1)==1) {
			if (bottom == top) bottom = top = (top+1) * 2;
			else bottom = pos;

		// If we can't, it's smaller.

		} else {
			if (bottom == top) {
				if (!top) return 0;
				bottom = top/2;
			}
			else top = pos;
		}
	} while (bottom + 1 != top);

	return pos + 1;
}
#endif

int FAST_FUNC bb_putchar_stderr(char ch)
{
	return write(STDERR_FILENO, &ch, 1);
}

ssize_t FAST_FUNC full_write1_str(const char *str)
{
	return full_write(STDOUT_FILENO, str, strlen(str));
}

ssize_t FAST_FUNC full_write2_str(const char *str)
{
	return full_write(STDERR_FILENO, str, strlen(str));
}

static int wh_helper(int value, int def_val, const char *env_name, int *err)
{
	if (value == 0) {
		char *s = getenv(env_name);
		if (s) {
			value = atoi(s);
			/* If LINES/COLUMNS are set, pretent that there is
			 * no error getting w/h, this prevents some ugly
			 * cursor tricks by our callers */
			*err = 0;
		}
	}
	if (value <= 1 || value >= 30000)
		value = def_val;
	return value;
}

/* It is perfectly ok to pass in a NULL for either width or for
 * height, in which case that value will not be set.  */
int FAST_FUNC get_terminal_width_height(int fd, unsigned *width, unsigned *height)
{
	struct winsize win;
	int err;

	win.ws_row = 0;
	win.ws_col = 0;
	/* I've seen ioctl returning 0, but row/col is (still?) 0.
	 * We treat that as an error too.  */
	err = ioctl(fd, TIOCGWINSZ, &win) != 0 || win.ws_row == 0;
	if (height)
		*height = wh_helper(win.ws_row, 24, "LINES", &err);
	if (width)
		*width = wh_helper(win.ws_col, 80, "COLUMNS", &err);
	return err;
}

int FAST_FUNC tcsetattr_stdin_TCSANOW(const struct termios *tp)
{
	return tcsetattr(STDIN_FILENO, TCSANOW, tp);
}
