/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-movemail.c: mbox copying function */

/*
 * Author:
 *  Dan Winship <danw@ximian.com>
 *
 * Copyright 2000 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <sys/uio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include "camel-movemail.h"
#include "camel-exception.h"
#include "camel-mime-parser.h"
#include "camel-mime-filter.h"
#include "camel-mime-filter-from.h"
#include "camel-lock-client.h"
#include "camel-i18n.h"

#define d(x)

#ifdef MOVEMAIL_PATH
#include <sys/wait.h>

static void movemail_external (const char *source, const char *dest,
			       CamelException *ex);
#endif

#ifdef HAVE_BROKEN_SPOOL
static int camel_movemail_copy_filter(int fromfd, int tofd, off_t start, size_t bytes, CamelMimeFilter *filter);
static int camel_movemail_solaris (int oldsfd, int dfd, CamelException *ex);
#else
/* these could probably be exposed as a utility? (but only mbox needs it) */
static int camel_movemail_copy_file(int sfd, int dfd, CamelException *ex);
#endif

#if 0
static int camel_movemail_copy(int fromfd, int tofd, off_t start, size_t bytes);
#endif

/**
 * camel_movemail: Copy an mbox file from a shared spool directory to a
 * new folder in a Camel store
 * @source: source file
 * @dest: destination file
 * @ex: a CamelException
 *
 * This copies an mbox file from a shared directory with multiple
 * readers and writers into a private (presumably Camel-controlled)
 * directory. Dot locking is used on the source file (but not the
 * destination).
 *
 * Return Value: Returns -1 on error.
 **/
int
camel_movemail(const char *source, const char *dest, CamelException *ex)
{
	int lockid = -1;
	int res = -1;
	int sfd, dfd;
	struct stat st;

	camel_exception_clear(ex);

	/* Stat and then open the spool file. If it doesn't exist or
	 * is empty, the user has no mail. (There's technically a race
	 * condition here in that an MDA might have just now locked it
	 * to deliver a message, but we don't care. In that case,
	 * assuming it's unlocked is equivalent to pretending we were
	 * called a fraction earlier.)
	 */
	if (stat (source, &st) == -1) {
		if (errno != ENOENT) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Could not check mail file %s: %s"),
					      source, g_strerror (errno));
		}
		return -1;
	}

	if (st.st_size == 0)
		return 0;

	/* open files */
	sfd = open (source, O_RDWR);
	if (sfd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not open mail file %s: %s"),
				      source, g_strerror (errno));
		return -1;
	}

	dfd = open (dest, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
	if (dfd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not open temporary mail "
					"file %s: %s"), dest,
				      g_strerror (errno));
		close (sfd);
		return -1;
	}

	/* lock our source mailbox */
	lockid = camel_lock_helper_lock(source, ex);
	if (lockid == -1) {
		close(sfd);
		close(dfd);
		return -1;
	}

#ifdef HAVE_BROKEN_SPOOL
	res = camel_movemail_solaris(sfd, dfd, ex);
#else
	res = camel_movemail_copy_file(sfd, dfd, ex);
#endif

	/* If no errors occurred copying the data, and we successfully
	 * close the destination file, then truncate the source file.
	 */
	if (res != -1) {
		if (close (dfd) == 0) {
			ftruncate (sfd, 0);
		} else {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Failed to store mail in temp file %s: %s"),
					      dest, g_strerror (errno));
			res = -1;
		}
	} else
		close (dfd);
	close (sfd);

	camel_lock_helper_unlock(lockid);

	return res;
}

#ifdef MOVEMAIL_PATH
static void
movemail_external (const char *source, const char *dest, CamelException *ex)
{
	sigset_t mask, omask;
	pid_t pid;
	int fd[2], len = 0, nread, status;
	char buf[BUFSIZ], *output = NULL;

	/* Block SIGCHLD so the app can't mess us up. */
	sigemptyset (&mask);
	sigaddset (&mask, SIGCHLD);
	sigprocmask (SIG_BLOCK, &mask, &omask);

	if (pipe (fd) == -1) {
		sigprocmask (SIG_SETMASK, &omask, NULL);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create pipe: %s"),
				      g_strerror (errno));
		return;
	}

	pid = fork ();
	switch (pid) {
	case -1:
		close (fd[0]);
		close (fd[1]);
		sigprocmask (SIG_SETMASK, &omask, NULL);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not fork: %s"),
				      g_strerror (errno));
		return;

	case 0:
		/* Child */
		close (fd[0]);
		close (STDIN_FILENO);
		dup2 (fd[1], STDOUT_FILENO);
		dup2 (fd[1], STDERR_FILENO);

		execl (MOVEMAIL_PATH, MOVEMAIL_PATH, source, dest, NULL);
		_exit (255);
		break;

	default:
		break;
	}

	/* Parent */
	close (fd[1]);

	/* Read movemail's output. */
	while ((nread = read (fd[0], buf, sizeof (buf))) > 0) {
		output = g_realloc (output, len + nread + 1);
		memcpy (output + len, buf, nread);
		len += nread;
		output[len] = '\0';
	}
	close (fd[0]);

	/* Now get the exit status. */
	while (waitpid (pid, &status, 0) == -1 && errno == EINTR)
		;
	sigprocmask (SIG_SETMASK, &omask, NULL);

	if (!WIFEXITED (status) || WEXITSTATUS (status) != 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Movemail program failed: %s"),
				      output ? output : _("(Unknown error)"));
	}
	g_free (output);
}
#endif

#ifndef HAVE_BROKEN_SPOOL
static int
camel_movemail_copy_file(int sfd, int dfd, CamelException *ex)
{
	int nread, nwrote;
	char buf[4096];

	while (1) {
		int written = 0;

		nread = read (sfd, buf, sizeof (buf));
		if (nread == 0)
			break;
		else if (nread == -1) {
			if (errno == EINTR)
				continue;
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Error reading mail file: %s"),
					      g_strerror (errno));
			return -1;
		}

		while (nread) {
			nwrote = write (dfd, buf + written, nread);
			if (nwrote == -1) {
				if (errno == EINTR)
					continue; /* continues inner loop */
				camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
						      _("Error writing mail temp file: %s"),
						      g_strerror (errno));
				return -1;
			}
			written += nwrote;
			nread -= nwrote;
		}
	}

	return 0;
}
#endif

#if 0
static int
camel_movemail_copy(int fromfd, int tofd, off_t start, size_t bytes)
{
        char buffer[4096];
        int written = 0;

	d(printf("writing %d bytes ... ", bytes));

	if (lseek(fromfd, start, SEEK_SET) != start)
		return -1;

        while (bytes>0) {
                int toread, towrite;

                toread = bytes;
                if (bytes>4096)
                        toread = 4096;
                else
                        toread = bytes;
		do {
			towrite = read(fromfd, buffer, toread);
		} while (towrite == -1 && errno == EINTR);

		if (towrite == -1)
			return -1;

                /* check for 'end of file' */
                if (towrite == 0) {
			d(printf("end of file?\n"));
                        break;
		}

		do {
			toread = write(tofd, buffer, towrite);
		} while (toread == -1 && errno == EINTR);

		if (toread == -1)
			return -1;

                written += toread;
                bytes -= toread;
        }

        d(printf("written %d bytes\n", written));

        return written;
}
#endif

#define PRE_SIZE (32)

#ifdef HAVE_BROKEN_SPOOL
static int
camel_movemail_copy_filter(int fromfd, int tofd, off_t start, size_t bytes, CamelMimeFilter *filter)
{
        char buffer[4096+PRE_SIZE];
        int written = 0;
	char *filterbuffer;
	int filterlen, filterpre;

	d(printf("writing %d bytes ... ", bytes));

	camel_mime_filter_reset(filter);

	if (lseek(fromfd, start, SEEK_SET) != start)
		return -1;

        while (bytes>0) {
                int toread, towrite;

                toread = bytes;
                if (bytes>4096)
                        toread = 4096;
                else
                        toread = bytes;
		do {
			towrite = read(fromfd, buffer+PRE_SIZE, toread);
		} while (towrite == -1 && errno == EINTR);

		if (towrite == -1)
			return -1;

		d(printf("read %d unfiltered bytes\n", towrite));

                /* check for 'end of file' */
                if (towrite == 0) {
			d(printf("end of file?\n"));
			camel_mime_filter_complete(filter, buffer+PRE_SIZE, towrite, PRE_SIZE,
						   &filterbuffer, &filterlen, &filterpre);
			towrite = filterlen;
			if (towrite == 0)
				break;
		} else {
			camel_mime_filter_filter(filter, buffer+PRE_SIZE, towrite, PRE_SIZE,
						 &filterbuffer, &filterlen, &filterpre);
			towrite = filterlen;
		}

		d(printf("writing %d filtered bytes\n", towrite));

		do {
			toread = write(tofd, filterbuffer, towrite);
		} while (toread == -1 && errno == EINTR);

		if (toread == -1)
			return -1;

                written += toread;
                bytes -= toread;
        }

        d(printf("written %d bytes\n", written));

        return written;
}

/* write the headers back out again, but not he Content-Length header, because we dont
   want	to maintain it! */
static int
solaris_header_write(int fd, struct _camel_header_raw *header)
{
        struct iovec iv[4];
        int outlen = 0, len;

        iv[1].iov_base = ":";
        iv[1].iov_len = 1;
        iv[3].iov_base = "\n";
        iv[3].iov_len = 1;

        while (header) {
		if (strcasecmp(header->name, "Content-Length")) {
			iv[0].iov_base = header->name;
			iv[0].iov_len = strlen(header->name);
			iv[2].iov_base = header->value;
			iv[2].iov_len = strlen(header->value);
		
			do {
				len = writev(fd, iv, 4);
			} while (len == -1 && errno == EINTR);
			
			if (len == -1)
				return -1;
			outlen += len;
		}
                header = header->next;
        }

	do {
		len = write(fd, "\n", 1);
	} while (len == -1 && errno == EINTR);

	if (len == -1)
		return -1;

	outlen += 1;

	d(printf("Wrote %d bytes of headers\n", outlen));

        return outlen;
}

/* Well, since Solaris is a tad broken wrt its 'mbox' folder format,
   we must convert it to a real mbox format.  Thankfully this is
   mostly pretty easy */
static int
camel_movemail_solaris (int oldsfd, int dfd, CamelException *ex)
{
	CamelMimeParser *mp;
	char *buffer;
	int len;
	int sfd;
	CamelMimeFilterFrom *ffrom;
	int ret = 1;
	char *from = NULL;

	/* need to dup as the mime parser will close on finish */
	sfd = dup(oldsfd);
	if (sfd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Error copying mail temp file: %s"),
				      g_strerror (errno));
		return -1;
	}

	mp = camel_mime_parser_new();
	camel_mime_parser_scan_from(mp, TRUE);
	camel_mime_parser_init_with_fd(mp, sfd);

	ffrom = camel_mime_filter_from_new();

	while (camel_mime_parser_step(mp, &buffer, &len) == CAMEL_MIME_PARSER_STATE_FROM) {
		g_assert(camel_mime_parser_from_line(mp));
		from = g_strdup(camel_mime_parser_from_line(mp));
		if (camel_mime_parser_step(mp, &buffer, &len) != CAMEL_MIME_PARSER_STATE_FROM_END) {
			const char *cl;
			int length;
			int start, body;
			off_t newpos;

			ret = 0;

			start = camel_mime_parser_tell_start_from(mp);
			body = camel_mime_parser_tell(mp);

			if (write(dfd, from, strlen(from)) != strlen(from))
				goto fail;

			/* write out headers, but NOT content-length header */
			if (solaris_header_write(dfd, camel_mime_parser_headers_raw(mp)) == -1)
				goto fail;

			cl = camel_mime_parser_header(mp, "content-length", NULL);
			if (cl == NULL) {
				g_warning("Required Content-Length header is missing from solaris mail box @ %d", (int)camel_mime_parser_tell(mp));
				camel_mime_parser_drop_step(mp);
				camel_mime_parser_drop_step(mp);
				camel_mime_parser_step(mp, &buffer, &len);
				camel_mime_parser_unstep(mp);
				length = camel_mime_parser_tell_start_from(mp) - body;
				newpos = -1;
			} else {
				length = atoi(cl);
				camel_mime_parser_drop_step(mp);
				camel_mime_parser_drop_step(mp);
				newpos = length+body;
			}
			/* copy body->length converting From lines */
			if (camel_movemail_copy_filter(sfd, dfd, body, length, (CamelMimeFilter *)ffrom) == -1)
				goto fail;
			if (newpos != -1)
				camel_mime_parser_seek(mp, newpos, SEEK_SET);
		} else {
			g_error("Inalid parser state: %d", camel_mime_parser_state(mp));
		}
		g_free(from);
	}

	camel_object_unref((CamelObject *)mp);
	camel_object_unref((CamelObject *)ffrom);

	return ret;

fail:
	g_free(from);

	camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
			      _("Error copying mail temp file: %s"),
			      g_strerror (errno));


	camel_object_unref((CamelObject *)mp);
	camel_object_unref((CamelObject *)ffrom);

	return -1;
}
#endif /* HAVE_BROKEN_SPOOL */

