/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-fs.c : file system based stream */
/* inspired by gnome-stream-fs.c in bonobo by Miguel de Icaza */
/* 
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *	    Michael Zucchi <notzed@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#include <config.h>
#include "camel-stream-fs.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

static CamelSeekableStreamClass *parent_class=NULL;

/* Returns the class for a CamelStreamFS */
#define CSFS_CLASS(so) CAMEL_STREAM_FS_CLASS (GTK_OBJECT(so)->klass)

static gint stream_read (CamelStream *stream, gchar *buffer, gint n);
static gint stream_write (CamelStream *stream, const gchar *buffer, gint n);
static void stream_flush (CamelStream *stream);
static off_t stream_seek (CamelSeekableStream *stream, off_t offset, CamelStreamSeekPolicy policy);

static void finalize (GtkObject *object);

static void init_with_fd (CamelStreamFs *stream_fs, int fd);
static void init_with_fd_and_bounds (CamelStreamFs *stream_fs, int fd, off_t start, off_t end);
static int init_with_name (CamelStreamFs *stream_fs, const gchar *name, int flags, int mode);
static int init_with_name_and_bounds (CamelStreamFs *stream_fs, const gchar *name, int flags, int mode,
				       off_t start, off_t end);

static void
camel_stream_fs_class_init (CamelStreamFsClass *camel_stream_fs_class)
{
	CamelSeekableStreamClass *camel_seekable_stream_class = CAMEL_SEEKABLE_STREAM_CLASS (camel_stream_fs_class);
	CamelStreamClass *camel_stream_class = CAMEL_STREAM_CLASS (camel_stream_fs_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_stream_fs_class);

	parent_class = gtk_type_class (camel_seekable_stream_get_type ());
	
	/* virtual method definition */
	camel_stream_fs_class->init_with_fd = init_with_fd;
	camel_stream_fs_class->init_with_fd_and_bounds = init_with_fd_and_bounds;
	camel_stream_fs_class->init_with_name = init_with_name;
	camel_stream_fs_class->init_with_name_and_bounds = init_with_name_and_bounds;
	
	/* virtual method overload */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->flush = stream_flush;

	camel_seekable_stream_class->seek = stream_seek;

	gtk_object_class->finalize = finalize;
}

static void
camel_stream_fs_init (gpointer   object,  gpointer   klass)
{
	CamelStreamFs *stream = CAMEL_STREAM_FS (object);

	stream->name = NULL;
	stream->fd = -1;
}

GtkType
camel_stream_fs_get_type (void)
{
	static GtkType camel_stream_fs_type = 0;
	
	if (!camel_stream_fs_type)	{
		GtkTypeInfo camel_stream_fs_info =	
		{
			"CamelStreamFs",
			sizeof (CamelStreamFs),
			sizeof (CamelStreamFsClass),
			(GtkClassInitFunc) camel_stream_fs_class_init,
			(GtkObjectInitFunc) camel_stream_fs_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_stream_fs_type = gtk_type_unique (camel_seekable_stream_get_type (), &camel_stream_fs_info);
	}
	
	return camel_stream_fs_type;
}

static void           
finalize (GtkObject *object)
{
	CamelStreamFs *stream_fs = CAMEL_STREAM_FS (object);

	if (stream_fs->fd != -1) {
		if (close (stream_fs->fd) == -1)
			g_warning ("CamelStreamFs::finalise: Error closing file: %s", strerror (errno));
	}
	g_free (stream_fs->name);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
init_with_fd (CamelStreamFs *stream_fs, int fd)
{
	off_t offset;

	stream_fs->fd = fd;
	offset = lseek(fd, 0, SEEK_CUR);
	if (offset == -1)
		offset = 0;
	((CamelSeekableStream *)stream_fs)->position = offset;
}

static void
init_with_fd_and_bounds (CamelStreamFs *stream_fs, int fd, off_t start, off_t end)
{
	
	CSFS_CLASS (stream_fs)->init_with_fd (stream_fs, fd);
	camel_seekable_stream_set_bounds((CamelSeekableStream *)stream_fs, start, end);
}

static int
init_with_name (CamelStreamFs *stream_fs, const gchar *name, int flags, int mode)
{
	int fd;
	
	g_assert(name);
	g_assert(stream_fs);
	
	fd = open (name, flags, mode);
	if (fd==-1) {
		g_warning ("CamelStreamFs::new_with_name cannot open file: %s: %s", name, strerror(errno));
		return -1;
	}
	
	stream_fs->name = g_strdup (name);
	CSFS_CLASS (stream_fs)->init_with_fd (stream_fs, fd);
	return 0;
}

static int
init_with_name_and_bounds (CamelStreamFs *stream_fs, const gchar *name, int flags, int mode,
			   off_t start, off_t end)
{
	if (CSFS_CLASS (stream_fs)->init_with_name (stream_fs, name, flags, mode) == -1)
		return -1;
	camel_seekable_stream_set_bounds((CamelSeekableStream *)stream_fs, start, end);
	return 0;
}

/**
 * camel_stream_fs_new_with_name:
 * @name: 
 * @mode: 
 * 
 * 
 * 
 * Return value: 
 **/
CamelStream *
camel_stream_fs_new_with_name (const gchar *name, int flags, int mode)
{
	CamelStreamFs *stream_fs;
	stream_fs = gtk_type_new (camel_stream_fs_get_type ());
	if (CSFS_CLASS (stream_fs)->init_with_name (stream_fs, name, flags, mode) == -1) {
		gtk_object_unref (GTK_OBJECT (stream_fs));
		return NULL;
	}

	return (CamelStream *)stream_fs;
}

/**
 * camel_stream_fs_new_with_name_and_bounds:
 * @name: 
 * @mode: 
 * @inf_bound: 
 * @sup_bound: 
 * 
 * 
 * 
 * Return value: 
 **/
CamelStream *
camel_stream_fs_new_with_name_and_bounds (const gchar *name, int flags, int mode,
					  off_t start, off_t end)
{
	CamelStreamFs *stream_fs;
	stream_fs = gtk_type_new (camel_stream_fs_get_type ());
	if (CSFS_CLASS (stream_fs)->init_with_name_and_bounds (stream_fs, name, flags, mode, start, end) == -1) {
		gtk_object_unref (GTK_OBJECT (stream_fs));
		return NULL;
	}
	
	return (CamelStream *)stream_fs;
}

/**
 * camel_stream_fs_new_with_fd:
 * @fd: 
 * 
 * 
 * 
 * Return value: 
 **/
CamelStream *
camel_stream_fs_new_with_fd (int fd)
{
	CamelStreamFs *stream_fs;
	
	stream_fs = gtk_type_new (camel_stream_fs_get_type ());
	CSFS_CLASS (stream_fs)->init_with_fd (stream_fs, fd);
	
	return CAMEL_STREAM (stream_fs);
}

/**
 * camel_stream_fs_new_with_fd_and_bounds:
 * @fd: 
 * @inf_bound: 
 * @sup_bound: 
 * 
 * 
 * 
 * Return value: 
 **/
CamelStream *
camel_stream_fs_new_with_fd_and_bounds (int fd, off_t start, off_t end)
{
	CamelStreamFs *stream_fs;
	
	stream_fs = gtk_type_new (camel_stream_fs_get_type ());
	CSFS_CLASS (stream_fs)->init_with_fd_and_bounds (stream_fs, fd, start, end);
	
	return CAMEL_STREAM (stream_fs);
}

static gint
stream_read (CamelStream *stream, gchar *buffer, gint n)
{
	CamelStreamFs *stream_fs = CAMEL_STREAM_FS (stream);
	CamelSeekableStream *seekable = (CamelSeekableStream *)stream;
	gint v;

	g_assert (n);

	if (seekable->bound_end != CAMEL_STREAM_UNBOUND)
		n = MIN (seekable->bound_end - seekable->position, n);

	do {
		v = read ( stream_fs->fd, buffer, n);
	} while (v == -1 && errno == EINTR);

	if (v>0)
		seekable->position += v;

	if (v == 0)
		stream->eos = TRUE;

	return v;
}

static gint
stream_write (CamelStream *stream, const gchar *buffer, gint n)
{
	CamelStreamFs *stream_fs = CAMEL_STREAM_FS (stream);
	CamelSeekableStream *seekable = (CamelSeekableStream *)stream;
	int v;
	gint written = 0;

	g_assert (stream);
	g_assert (stream_fs->fd != -1);
	
	if (n <= 0)
		return 0;

	if (seekable->bound_end != CAMEL_STREAM_UNBOUND)
		n = MIN (seekable->bound_end - seekable->position, n);

	do {
		v = write ( stream_fs->fd, buffer, n);
		if (v>0)
			written += v;
	} while (v == -1 && errno == EINTR);
	
	if (written>0)
		seekable->position += written;

	return written;
}

static void
stream_flush (CamelStream *stream)
{
	fsync ((CAMEL_STREAM_FS (stream))->fd);
}

static off_t
stream_seek (CamelSeekableStream *stream, off_t offset, CamelStreamSeekPolicy policy)
{
	CamelStreamFs *stream_fs = CAMEL_STREAM_FS (stream);
	off_t real = 0;

	switch  (policy) {
	case CAMEL_STREAM_SET:
		real = offset;
		break;
	case CAMEL_STREAM_CUR:
		real = stream->position + offset;
		break;
	case CAMEL_STREAM_END:
		if (stream->bound_end != CAMEL_STREAM_UNBOUND) {
			real = lseek(stream_fs->fd, offset, SEEK_END);
			if (real != -1)
				stream->position = real;
			return real;
		}
		real = stream->bound_end + offset;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	if (stream->bound_end != CAMEL_STREAM_UNBOUND) {
		real = MIN (real, stream->bound_end);
	}
	real = MAX (real, stream->bound_start);

	real = lseek(stream_fs->fd, real, SEEK_SET);

	if (real == -1)
		return -1;

	if (real != stream->position && ((CamelStream *)stream)->eos)
		((CamelStream *)stream)->eos = FALSE;

	stream->position = real;
	
	return real;
}
