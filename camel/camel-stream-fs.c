/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-fs.c : file system based stream */

/*
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 1999-2003 Ximian, Inc. (www.ximian.com)
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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "camel-file-utils.h"
#include "camel-operation.h"
#include "camel-stream-fs.h"

static CamelSeekableStreamClass *parent_class = NULL;

/* Returns the class for a CamelStreamFS */
#define CSFS_CLASS(so) CAMEL_STREAM_FS_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static ssize_t stream_read   (CamelStream *stream, char *buffer, size_t n);
static ssize_t stream_write  (CamelStream *stream, const char *buffer, size_t n);
static int stream_flush  (CamelStream *stream);
static int stream_close  (CamelStream *stream);
static off_t stream_seek (CamelSeekableStream *stream, off_t offset,
			  CamelStreamSeekPolicy policy);

static void
camel_stream_fs_class_init (CamelStreamFsClass *camel_stream_fs_class)
{
	CamelSeekableStreamClass *camel_seekable_stream_class =
		CAMEL_SEEKABLE_STREAM_CLASS (camel_stream_fs_class);
	CamelStreamClass *camel_stream_class =
		CAMEL_STREAM_CLASS (camel_stream_fs_class);

	parent_class = CAMEL_SEEKABLE_STREAM_CLASS (camel_type_get_global_classfuncs (camel_seekable_stream_get_type ()));

	/* virtual method overload */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->flush = stream_flush;
	camel_stream_class->close = stream_close;

	camel_seekable_stream_class->seek = stream_seek;
}

static void
camel_stream_fs_init (gpointer object, gpointer klass)
{
	CamelStreamFs *stream = CAMEL_STREAM_FS (object);

	stream->fd = -1;
	((CamelSeekableStream *)stream)->bound_end = CAMEL_STREAM_UNBOUND;
}

static void
camel_stream_fs_finalize (CamelObject *object)
{
	CamelStreamFs *stream_fs = CAMEL_STREAM_FS (object);

	if (stream_fs->fd != -1)
		close (stream_fs->fd);
}


CamelType
camel_stream_fs_get_type (void)
{
	static CamelType camel_stream_fs_type = CAMEL_INVALID_TYPE;

	if (camel_stream_fs_type == CAMEL_INVALID_TYPE) {
		camel_stream_fs_type = camel_type_register (camel_seekable_stream_get_type (), "CamelStreamFs",
							    sizeof (CamelStreamFs),
							    sizeof (CamelStreamFsClass),
							    (CamelObjectClassInitFunc) camel_stream_fs_class_init,
							    NULL,
							    (CamelObjectInitFunc) camel_stream_fs_init,
							    (CamelObjectFinalizeFunc) camel_stream_fs_finalize);
	}

	return camel_stream_fs_type;
}

/**
 * camel_stream_fs_new_with_fd:
 * @fd: a file descriptor
 *
 * Creates a new fs stream using the given file descriptor @fd as the
 * backing store. When the stream is destroyed, the file descriptor
 * will be closed.
 *
 * Returns a new fs stream.
 **/
CamelStream *
camel_stream_fs_new_with_fd (int fd)
{
	CamelStreamFs *stream_fs;
	off_t offset;

	if (fd == -1)
		return NULL;

	stream_fs = CAMEL_STREAM_FS (camel_object_new (camel_stream_fs_get_type ()));
	stream_fs->fd = fd;
	offset = lseek (fd, 0, SEEK_CUR);
	if (offset == -1)
		offset = 0;
	CAMEL_SEEKABLE_STREAM (stream_fs)->position = offset;

	return CAMEL_STREAM (stream_fs);
}

/**
 * camel_stream_fs_new_with_fd_and_bounds:
 * @fd: a file descriptor
 * @start: the first valid position in the file
 * @end: the first invalid position in the file, or CAMEL_STREAM_UNBOUND
 *
 * Returns a stream associated with the given file descriptor and bounds.
 * When the stream is destroyed, the file descriptor will be closed.
 *
 * Return value: the stream
 **/
CamelStream *
camel_stream_fs_new_with_fd_and_bounds (int fd, off_t start, off_t end)
{
	CamelStream *stream;

	stream = camel_stream_fs_new_with_fd (fd);
	camel_seekable_stream_set_bounds (CAMEL_SEEKABLE_STREAM (stream), start, end);

	return stream;
}

/**
 * camel_stream_fs_new_with_name:
 * @name: a local filename
 * @flags: flags as in open(2)
 * @mode: a file mode
 *
 * Creates a new CamelStream corresponding to the named file, flags,
 * and mode.
 *
 * Return value: the stream, or #NULL on error.
 **/
CamelStream *
camel_stream_fs_new_with_name (const char *name, int flags, mode_t mode)
{
	int fd;

	fd = open (name, flags, mode);
	if (fd == -1) {
		return NULL;
	}

	return camel_stream_fs_new_with_fd (fd);
}

/**
 * camel_stream_fs_new_with_name_and_bounds:
 * @name: a local filename
 * @flags: flags as in open(2)
 * @mode: a file mode
 * @start: the first valid position in the file
 * @end: the first invalid position in the file, or CAMEL_STREAM_UNBOUND
 *
 * Creates a new CamelStream corresponding to the given arguments.
 *
 * Return value: the stream, or NULL on error.
 **/
CamelStream *
camel_stream_fs_new_with_name_and_bounds (const char *name, int flags,
					  mode_t mode, off_t start, off_t end)
{
	CamelStream *stream;

	stream = camel_stream_fs_new_with_name (name, flags, mode);
	if (stream == NULL)
		return NULL;

	camel_seekable_stream_set_bounds (CAMEL_SEEKABLE_STREAM (stream),
					  start, end);

	return stream;
}


static ssize_t
stream_read (CamelStream *stream, char *buffer, size_t n)
{
	CamelStreamFs *stream_fs = CAMEL_STREAM_FS (stream);
	CamelSeekableStream *seekable = CAMEL_SEEKABLE_STREAM (stream);
	ssize_t nread;
	
	if (seekable->bound_end != CAMEL_STREAM_UNBOUND)
		n = MIN (seekable->bound_end - seekable->position, n);
	
	if ((nread = camel_read (stream_fs->fd, buffer, n)) > 0)
		seekable->position += nread;
	else if (nread == 0)
		stream->eos = TRUE;
	
	return nread;
}

static ssize_t
stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	CamelStreamFs *stream_fs = CAMEL_STREAM_FS (stream);
	CamelSeekableStream *seekable = CAMEL_SEEKABLE_STREAM (stream);
	ssize_t nwritten;
	
	if (seekable->bound_end != CAMEL_STREAM_UNBOUND)
		n = MIN (seekable->bound_end - seekable->position, n);
	
	if ((nwritten = camel_write (stream_fs->fd, buffer, n)) > 0)
		seekable->position += nwritten;
	
	return nwritten;
}

static int
stream_flush (CamelStream *stream)
{
	return fsync(((CamelStreamFs *)stream)->fd);
}

static int
stream_close (CamelStream *stream)
{
	if (close (((CamelStreamFs *)stream)->fd) == -1)
		return -1;
	
	((CamelStreamFs *)stream)->fd = -1;
	return 0;
}

static off_t
stream_seek (CamelSeekableStream *stream, off_t offset, CamelStreamSeekPolicy policy)
{
	CamelStreamFs *stream_fs = CAMEL_STREAM_FS (stream);
	off_t real = 0;

	switch (policy) {
	case CAMEL_STREAM_SET:
		real = offset;
		break;
	case CAMEL_STREAM_CUR:
		real = stream->position + offset;
		break;
	case CAMEL_STREAM_END:
		if (stream->bound_end == CAMEL_STREAM_UNBOUND) {
			real = lseek(stream_fs->fd, offset, SEEK_END);
			if (real != -1) {
				if (real<stream->bound_start)
					real = stream->bound_start;
				stream->position = real;
			}
			return real;
		}
		real = stream->bound_end + offset;
		break;
	}

	if (stream->bound_end != CAMEL_STREAM_UNBOUND)
		real = MIN (real, stream->bound_end);
	real = MAX (real, stream->bound_start);

	real = lseek(stream_fs->fd, real, SEEK_SET);
	if (real == -1)
		return -1;

	if (real != stream->position && ((CamelStream *)stream)->eos)
		((CamelStream *)stream)->eos = FALSE;

	stream->position = real;

	return real;
}
