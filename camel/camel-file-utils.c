/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors:
 *   Michael Zucchi <notzed@ximian.com>
 *   Jeffrey Stedfast <fejj@ximian.com>
 *   Dan Winship <danw@ximian.com>
 *
 * Copyright (C) 2000, 2003 Ximian, Inc.
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

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "camel-file-utils.h"
#include "camel-operation.h"
#include "camel-url.h"

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define IO_TIMEOUT (60*4)

/**
 * camel_file_util_encode_uint32:
 * @out: file to output to
 * @value: value to output
 * 
 * Utility function to save an uint32 to a file.
 * 
 * Return value: 0 on success, -1 on error.
 **/
int
camel_file_util_encode_uint32 (FILE *out, guint32 value)
{
	int i;

	for (i = 28; i > 0; i -= 7) {
		if (value >= (1 << i)) {
			unsigned int c = (value >> i) & 0x7f;
			if (fputc (c, out) == -1)
				return -1;
		}
	}
	return fputc (value | 0x80, out);
}


/**
 * camel_file_util_decode_uint32:
 * @in: file to read from
 * @dest: pointer to a variable to store the value in
 * 
 * Retrieve an encoded uint32 from a file.
 * 
 * Return value: 0 on success, -1 on error.  @*dest will contain the
 * decoded value.
 **/
int
camel_file_util_decode_uint32 (FILE *in, guint32 *dest)
{
        guint32 value = 0;
	int v;

        /* until we get the last byte, keep decoding 7 bits at a time */
        while ( ((v = fgetc (in)) & 0x80) == 0 && v!=EOF) {
                value |= v;
                value <<= 7;
        }
	if (v == EOF) {
		*dest = value >> 7;
		return -1;
	}
	*dest = value | (v & 0x7f);

        return 0;
}


/**
 * camel_file_util_encode_fixed_int32:
 * @out: file to output to
 * @value: value to output
 * 
 * Encode a gint32, performing no compression, but converting
 * to network order.
 * 
 * Return value: 0 on success, -1 on error.
 **/
int
camel_file_util_encode_fixed_int32 (FILE *out, gint32 value)
{
	guint32 save;

	save = htonl (value);
	if (fwrite (&save, sizeof (save), 1, out) != 1)
		return -1;
	return 0;
}


/**
 * camel_file_util_decode_fixed_int32:
 * @in: file to read from
 * @dest: pointer to a variable to store the value in
 * 
 * Retrieve a gint32.
 * 
 * Return value: 0 on success, -1 on error.
 **/
int
camel_file_util_decode_fixed_int32 (FILE *in, gint32 *dest)
{
	guint32 save;

	if (fread (&save, sizeof (save), 1, in) == 1) {
		*dest = ntohl (save);
		return 0;
	} else {
		return -1;
	}
}

#define CFU_ENCODE_T(type)						\
int									\
camel_file_util_encode_##type(FILE *out, type value)			\
{									\
	int i;								\
									\
	for (i = sizeof (type) - 1; i >= 0; i--) {			\
		if (fputc((value >> (i * 8)) & 0xff, out) == -1)	\
			return -1;					\
	}								\
	return 0;							\
}

#define CFU_DECODE_T(type)				\
int							\
camel_file_util_decode_##type(FILE *in, type *dest)	\
{							\
	type save = 0;					\
	int i = sizeof(type) - 1;			\
	int v = EOF;					\
							\
        while (i >= 0 && (v = fgetc (in)) != EOF) {	\
		save |= ((type)v) << (i * 8);		\
		i--;					\
	}						\
	*dest = save;					\
	if (v == EOF)					\
		return -1;				\
	return 0;					\
}


/**
 * camel_file_util_encode_time_t:
 * @out: file to output to
 * @value: value to output
 * 
 * Encode a time_t value to the file.
 * 
 * Return value: 0 on success, -1 on error.
 **/
CFU_ENCODE_T(time_t)

/**
 * camel_file_util_decode_time_t:
 * @in: file to read from
 * @dest: pointer to a variable to store the value in
 * 
 * Decode a time_t value.
 * 
 * Return value: 0 on success, -1 on error.
 **/
CFU_DECODE_T(time_t)

/**
 * camel_file_util_encode_off_t:
 * @out: file to output to
 * @value: value to output
 * 
 * Encode an off_t type.
 * 
 * Return value: 0 on success, -1 on error.
 **/
CFU_ENCODE_T(off_t)


/**
 * camel_file_util_decode_off_t:
 * @in: file to read from
 * @dest: pointer to a variable to put the value in
 * 
 * Decode an off_t type.
 * 
 * Return value: 0 on success, -1 on failure.
 **/
CFU_DECODE_T(off_t)

/**
 * camel_file_util_encode_size_t:
 * @out: file to output to
 * @value: value to output
 * 
 * Encode an size_t type.
 * 
 * Return value: 0 on success, -1 on error.
 **/
CFU_ENCODE_T(size_t)


/**
 * camel_file_util_decode_size_t:
 * @in: file to read from
 * @dest: pointer to a variable to put the value in
 * 
 * Decode an size_t type.
 * 
 * Return value: 0 on success, -1 on failure.
 **/
CFU_DECODE_T(size_t)


/**
 * camel_file_util_encode_string:
 * @out: file to output to
 * @str: value to output
 * 
 * Encode a normal string and save it in the output file.
 * 
 * Return value: 0 on success, -1 on error.
 **/
int
camel_file_util_encode_string (FILE *out, const char *str)
{
	register int len;

	if (str == NULL)
		return camel_file_util_encode_uint32 (out, 1);
	
	if ((len = strlen (str)) > 65536)
		len = 65536;
	
	if (camel_file_util_encode_uint32 (out, len+1) == -1)
		return -1;
	if (len == 0 || fwrite (str, len, 1, out) == 1)
		return 0;
	return -1;
}


/**
 * camel_file_util_decode_string:
 * @in: file to read from
 * @str: pointer to a variable to store the value in
 * 
 * Decode a normal string from the input file.
 * 
 * Return value: 0 on success, -1 on error.
 **/
int
camel_file_util_decode_string (FILE *in, char **str)
{
	guint32 len;
	register char *ret;

	if (camel_file_util_decode_uint32 (in, &len) == -1) {
		*str = NULL;
		return -1;
	}

	len--;
	if (len > 65536) {
		*str = NULL;
		return -1;
	}

	ret = g_malloc (len+1);
	if (len > 0 && fread (ret, len, 1, in) != 1) {
		g_free (ret);
		*str = NULL;
		return -1;
	}

	ret[len] = 0;
	*str = ret;
	return 0;
}


/**
 * camel_mkdir:
 * @path: directory path to create
 * @mode: permissions
 *
 * Creates the directory path described in @path, creating any parent
 * directories as necessary.
 *
 * Returns 0 on success or -1 on fail. In the case of failure, errno
 * will be set appropriately.
 **/
int
camel_mkdir (const char *path, mode_t mode)
{
	char *copy, *p;
	
	g_assert(path && path[0] == '/');
	
	p = copy = g_alloca (strlen (path) + 1);
	strcpy(copy, path);
	do {
		p = strchr(p + 1, '/');
		if (p)
			*p = '\0';
		if (access(copy, F_OK) == -1) {
			if (mkdir(copy, mode) == -1)
				return -1;
		}
		if (p)
			*p = '/';
	} while (p);
	
	return 0;
}


/**
 * camel_file_util_safe_filename:
 * @name: string to 'flattened' into a safe filename
 *
 * 'Flattens' @name into a safe filename string by hex encoding any
 * chars that may cause problems on the filesystem.
 *
 * Returns a safe filename string.
 **/
char *
camel_file_util_safe_filename (const char *name)
{
	if (name == NULL)
		return NULL;
	
	return camel_url_encode(name, "/?()'*");
}


/* FIXME: poll() might be more efficient and more portable? */

/**
 * camel_read:
 * @fd: file descriptor
 * @buf: buffer to fill
 * @n: number of bytes to read into @buf
 *
 * Cancellable libc read() replacement.
 *
 * Returns number of bytes read or -1 on fail. On failure, errno will
 * be set appropriately.
 **/
ssize_t
camel_read (int fd, char *buf, size_t n)
{
	ssize_t nread;
	int cancel_fd;
	
	if (camel_operation_cancel_check (NULL)) {
		errno = EINTR;
		return -1;
	}
	
	cancel_fd = camel_operation_cancel_fd (NULL);
	if (cancel_fd == -1) {
		do {
			nread = read (fd, buf, n);
		} while (nread == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
	} else {
		int errnosav, flags, fdmax;
		fd_set rdset;
		
		flags = fcntl (fd, F_GETFL);
		fcntl (fd, F_SETFL, flags | O_NONBLOCK);
		
		do {
			struct timeval tv;
			int res;

			FD_ZERO (&rdset);
			FD_SET (fd, &rdset);
			FD_SET (cancel_fd, &rdset);
			fdmax = MAX (fd, cancel_fd) + 1;
			tv.tv_sec = IO_TIMEOUT;
			tv.tv_usec = 0;
			nread = -1;

			res = select(fdmax, &rdset, 0, 0, &tv);
			if (res == -1)
				;
			else if (res == 0)
				errno = ETIMEDOUT;
			else if (FD_ISSET (cancel_fd, &rdset)) {
				errno = EINTR;
				goto failed;
			} else {				
				do {
					nread = read (fd, buf, n);
				} while (nread == -1 && errno == EINTR);
			}
		} while (nread == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
	failed:
		errnosav = errno;
		fcntl (fd, F_SETFL, flags);
		errno = errnosav;
	}
	
	return nread;
}


/**
 * camel_write:
 * @fd: file descriptor
 * @buf: buffer to write
 * @n: number of bytes of @buf to write
 *
 * Cancellable libc write() replacement.
 *
 * Returns number of bytes written or -1 on fail. On failure, errno will
 * be set appropriately.
 **/
ssize_t
camel_write (int fd, const char *buf, size_t n)
{
	ssize_t w, written = 0;
	int cancel_fd;
	
	if (camel_operation_cancel_check (NULL)) {
		errno = EINTR;
		return -1;
	}
	
	cancel_fd = camel_operation_cancel_fd (NULL);
	if (cancel_fd == -1) {
		do {
			do {
				w = write (fd, buf + written, n - written);
			} while (w == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
			
			if (w > 0)
				written += w;
		} while (w != -1 && written < n);
	} else {
		int errnosav, flags, fdmax;
		fd_set rdset, wrset;
		
		flags = fcntl (fd, F_GETFL);
		fcntl (fd, F_SETFL, flags | O_NONBLOCK);
		
		fdmax = MAX (fd, cancel_fd) + 1;
		do {
			struct timeval tv;
			int res;

			FD_ZERO (&rdset);
			FD_ZERO (&wrset);
			FD_SET (fd, &wrset);
			FD_SET (cancel_fd, &rdset);
			tv.tv_sec = IO_TIMEOUT;
			tv.tv_usec = 0;			
			w = -1;

			res = select (fdmax, &rdset, &wrset, 0, &tv);
			if (res == -1) {
				if (errno == EINTR)
					w = 0;
			} else if (res == 0)
				errno = ETIMEDOUT;
			else if (FD_ISSET (cancel_fd, &rdset))
				errno = EINTR;
			else {
				do {
					w = write (fd, buf + written, n - written);
				} while (w == -1 && errno == EINTR);
				
				if (w == -1) {
					if (errno == EAGAIN || errno == EWOULDBLOCK)
						w = 0;
				} else
					written += w;
			}
		} while (w != -1 && written < n);
		
		errnosav = errno;
		fcntl (fd, F_SETFL, flags);
		errno = errnosav;
	}
	
	if (w == -1)
		return -1;
	
	return written;
}

/**
 * camel_file_util_savename:
 * @filename: 
 * 
 * Builds a filename of the form ".#" + @filename, used to create
 * a two-stage commit file write.
 * 
 * Return value: ".#" + filename.  It must be free'd with g_free().
 **/
char *
camel_file_util_savename(const char *filename)
{
	char *name, *slash;
	int off;

	name = g_malloc(strlen(filename)+3);
	slash = strrchr(filename, '/');
	if (slash) {
		off = slash-filename;

		memcpy(name, filename, off+1);
		memcpy(name + off+1, ".#", 2);
		strcpy(name + off+3, filename+off+1);
	} else {
		sprintf(name, ".#%s", filename);
	}

	return name;
}
