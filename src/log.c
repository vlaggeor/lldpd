/* -*- mode: c; c-file-style: "openbsd" -*- */
/*	$OpenBSD: log.c,v 1.11 2007/12/07 17:17:00 reyk Exp $	*/

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <time.h>

/* By default, logging is done on stderr. */
static int	 debug = 1;

/* Logging can be modified by providing an appropriate log handler. */
static void (*logh)(int severity, const char *msg) = NULL;

static void	 vlog(int, const char *, va_list);
static void	 logit(int, const char *, ...);

void
log_init(int n_debug, const char *progname)
{
	debug = n_debug;

	if (!debug)
		openlog(progname, LOG_PID | LOG_NDELAY, LOG_DAEMON);

	tzset();
}

void
log_register(void (*cb)(int, const char*))
{
	logh = cb;
}


static void
logit(int pri, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vlog(pri, fmt, ap);
	va_end(ap);
}

static char *
date()
{
	/* Return the current date as incomplete ISO 8601 (2012-12-12T16:13:30) */
	static char date[] = "2012-12-12T16:13:30";
	time_t t = time(NULL);
	struct tm *tmp = localtime(&t);
	strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", tmp);
	return date;
}

static const char *
translate(int fd, int priority)
{
	/* Translate a syslog priority to a string. With colors if the output is a terminal. */
	int tty = isatty(fd);
	switch (tty) {
	case 1:
		switch (priority) {
		case LOG_EMERG:   return "\033[1;37;41m[EMRG]\033[0m";
		case LOG_ALERT:   return "\033[1;37;41m[ALRT]\033[0m";
		case LOG_CRIT:    return "\033[1;37;41m[CRIT]\033[0m";
		case LOG_ERR:     return "\033[1;31m[ ERR]\033[0m";
		case LOG_WARNING: return "\033[1;33m[WARN]\033[0m";
		case LOG_NOTICE:  return "\033[1;34m[NOTI]\033[0m";
		case LOG_INFO:    return "\033[1;34m[INFO]\033[0m";
		case LOG_DEBUG:   return "\033[1;30m[ DBG]\033[0m";
		}
		break;
	default:
		switch (priority) {
		case LOG_EMERG:   return "[EMRG]";
		case LOG_ALERT:   return "[ALRT]";
		case LOG_CRIT:    return "[CRIT]";
		case LOG_ERR:     return "[ ERR]";
		case LOG_WARNING: return "[WARN]";
		case LOG_NOTICE:  return "[NOTI]";
		case LOG_INFO:    return "[INFO]";
		case LOG_DEBUG:   return "[ DBG]";
		}
	}
	return "[UNKN]";
}

static void
vlog(int pri, const char *fmt, va_list ap)
{
	if (logh) {
		char *result;
		if (vasprintf(&result, fmt, ap) != -1) {
			logh(pri, result);
			return;
		}
		/* Otherwise, fallback to output on stderr. */
	}
	if (debug || logh) {
		char *nfmt;
		/* best effort in out of mem situations */
		if (asprintf(&nfmt, "%s %s %s\n",
			date(),
			translate(STDERR_FILENO, pri),
			fmt) == -1) {
			vfprintf(stderr, fmt, ap);
			fprintf(stderr, "\n");
		} else {
			vfprintf(stderr, nfmt, ap);
			free(nfmt);
		}
		fflush(stderr);
	} else
		vsyslog(pri, fmt, ap);
}


void
log_warn(const char *emsg, ...)
{
	char	*nfmt;
	va_list	 ap;

	/* best effort to even work in out of memory situations */
	if (emsg == NULL)
		logit(LOG_WARNING, "%s", strerror(errno));
	else {
		va_start(ap, emsg);

		if (asprintf(&nfmt, "%s: %s", emsg, strerror(errno)) == -1) {
			/* we tried it... */
			vlog(LOG_WARNING, emsg, ap);
			logit(LOG_WARNING, "%s", strerror(errno));
		} else {
			vlog(LOG_WARNING, nfmt, ap);
			free(nfmt);
		}
		va_end(ap);
	}
}

void
log_warnx(const char *emsg, ...)
{
	va_list	 ap;

	va_start(ap, emsg);
	vlog(LOG_WARNING, emsg, ap);
	va_end(ap);
}

void
log_info(const char *emsg, ...)
{
	va_list	 ap;

	if (debug > 1 || logh) {
		va_start(ap, emsg);
		vlog(LOG_INFO, emsg, ap);
		va_end(ap);
	}
}

void
log_debug(const char *emsg, ...)
{
	va_list	 ap;

	if (debug > 2 || logh) {
		va_start(ap, emsg);
		vlog(LOG_DEBUG, emsg, ap);
		va_end(ap);
	}
}

void
fatal(const char *emsg)
{
	if (emsg == NULL)
		logit(LOG_CRIT, "fatal: %s", strerror(errno));
	else
		if (errno)
			logit(LOG_CRIT, "fatal: %s: %s",
			    emsg, strerror(errno));
		else
			logit(LOG_CRIT, "fatal: %s", emsg);

	exit(1);
}

void
fatalx(const char *emsg)
{
	errno = 0;
	fatal(emsg);
}
