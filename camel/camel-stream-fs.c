/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-fs.c : file system based stream */

/* inspired by gnome-stream-fs.c in bonobo by Miguel de Icaza */
/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr> .
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "camel-log.h"

static CamelStreamClass *parent_class=NULL;


/* Returns the class for a CamelStreamFS */
#define CS_CLASS(so) CAMEL_STREAM_FS_CLASS (GTK_OBJECT(so)->klass)

static gint _read (CamelStream *stream, gchar *buffer, gint n);
static gint _write (CamelStream *stream, const gchar *buffer, gint n);
static void _flush (CamelStream *stream);
static gint _available (CamelStream *stream);
static gboolean _eos (CamelStream *stream);
static void _close (CamelStream *stream);
static gint _seek (CamelStream *stream, gint offset, CamelStreamSeekPolicy policy);

static void _finalize (GtkObject *object);
static void _destroy (GtkObject *object);

static void
camel_stream_fs_class_init (CamelStreamFsClass *camel_stream_fs_class)
{
	CamelStreamClass *camel_stream_class = CAMEL_STREAM_CLASS (camel_stream_fs_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_stream_fs_class);

	parent_class = gtk_type_class (gtk_object_get_type ());
	
	/* virtual method definition */

	/* virtual method overload */
	camel_stream_class->read = _read;
	camel_stream_class->write = _write;
	camel_stream_class->flush = _flush;
	camel_stream_class->available = _available;
	camel_stream_class->eos = _eos;
	camel_stream_class->close = _close;
	camel_stream_class->seek = _seek;

	gtk_object_class->finalize = _finalize;
	gtk_object_class->destroy = _destroy;

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
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_stream_fs_type = gtk_type_unique (camel_stream_get_type (), &camel_stream_fs_info);
	}
	
	return camel_stream_fs_type;
}


static void           
_destroy (GtkObject *object)
{
	CamelStreamFs *stream_fs = CAMEL_STREAM_FS (object);
	gint close_error;
	
	CAMEL_LOG_FULL_DEBUG ("Entering CamelStreamFs::destroy\n");
	
	close_error = close (stream_fs->fd);
	if (close_error) {
		CAMEL_LOG_FULL_DEBUG ("CamelStreamFs::destroy Error while closing file descriptor\n");
		CAMEL_LOG_FULL_DEBUG ( "  Full error text is : %s\n", strerror(errno));
	}
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
	g_free (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelStreamFs::destroy\n");
}


static void           
_finalize (GtkObject *object)
{
	CamelStreamFs *stream_fs = CAMEL_STREAM_FS (object);


	CAMEL_LOG_FULL_DEBUG ("Entering CamelStreamFs::finalize\n");
	
	if (stream_fs->name) g_free (stream_fs->name);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelStreamFs::finalize\n");
}


CamelStream *
camel_stream_fs_new_with_name (gchar *name, CamelStreamFsMode mode)
{
	struct stat s;
	int v, fd;
	int flags;
	CamelStreamFs *stream_fs;

	g_assert (name);
	CAMEL_LOG_FULL_DEBUG ( "Entering CamelStream::new_with_name, name=\"%s\", mode=%d\n", name, mode); 
	v = stat (name, &s);
	
	if (mode & CAMEL_STREAM_FS_READ){
		if (mode & CAMEL_STREAM_FS_WRITE)
			flags = O_RDWR | O_CREAT;
		else
			flags = O_RDONLY;
	} else {
		if (mode & CAMEL_STREAM_FS_WRITE)
			flags = O_WRONLY | O_CREAT;
		else
			return NULL;
	}
	if ( (mode & CAMEL_STREAM_FS_READ) && !(mode & CAMEL_STREAM_FS_WRITE) )
		if (v == -1) return NULL;

	fd = open (name, flags, 0600);
	if (fd==-1) {
		CAMEL_LOG_WARNING ( "CamelStreamFs::new_with_name can not obtain fd for file \"%s\"\n", name);
		CAMEL_LOG_FULL_DEBUG ( "  Full error text is : %s\n", strerror(errno));
		return NULL;
	}
	
	stream_fs = CAMEL_STREAM_FS (camel_stream_fs_new_with_fd (fd));
	stream_fs->name = name;

	return CAMEL_STREAM (stream_fs);
	
}

CamelStream *
camel_stream_fs_new_with_fd (int fd)
{
	CamelStreamFs *stream_fs;
	
	CAMEL_LOG_FULL_DEBUG ( "Entering CamelStream::new_with_fd  fd=%d\n",fd);
	stream_fs = gtk_type_new (camel_stream_fs_get_type ());
	stream_fs->fd = fd;
	return CAMEL_STREAM (stream_fs);
}

/**
 * _read: read bytes from a stream
 * @stream: stream
 * @buffer: buffer where bytes are stored
 * @n: max number of bytes to read
 * 
 * 
 * 
 * Return value: number of bytes actually read.
 **/
static gint
_read (CamelStream *stream, gchar *buffer, gint n)
{
	gint v;
	do {
		v = read ( (CAMEL_STREAM_FS (stream))->fd, buffer, n);
	} while (v == -1 && errno == EINTR);
	if (v<0)
		CAMEL_LOG_FULL_DEBUG ("CamelStreamFs::read v=%d\n", v);
	return v;
}


/**
 * _write: read bytes to a stream
 * @stream: the stream
 * @buffer: byte buffer
 * @n: number of bytes to write
 * 
 * 
 * 
 * Return value: the number of bytes actually written
 *  in the stream.
 **/
static gint
_write (CamelStream *stream, const gchar *buffer, gint n)
{
	int v;
	g_assert (stream);
	g_assert ((CAMEL_STREAM_FS (stream))->fd);
	CAMEL_LOG_FULL_DEBUG ( "CamelStreamFs:: entering write. n=%d\n", n);
	do {
		v = write ( (CAMEL_STREAM_FS (stream))->fd, buffer, n);
	} while (v == -1 && errno == EINTR);
	
#if HARD_LOG_LEVEL >= FULL_DEBUG
	if (v==-1) {
		perror("");
		CAMEL_LOG_FULL_DEBUG ( "CamelStreamFs::write could not write bytes in stream\n");
	}
#endif
	return v;

}



/**
 * _flush: flush pending changes 
 * @stream: the stream
 * 
 * 
 **/
static void
_flush (CamelStream *stream)
{
	fsync ((CAMEL_STREAM_FS (stream))->fd);
}



/**
 * _available: return the number of bytes available for reading
 * @stream: the stream
 * 
 * Return the number of bytes available without blocking.
 * 
 * Return value: the number of bytes available
 **/
static gint 
_available (CamelStream *stream)
{
	g_warning ("Not implemented yet");
}


/**
 * _eos: test if there are bytes left to read
 * @stream: the stream
 * 
 * 
 * 
 * Return value: true if all stream has been read
 **/
static gboolean
_eos (CamelStream *stream)
{
	g_warning ("Not implemented yet");
}


/**
 * _close: close a stream
 * @stream: the stream
 * 
 * 
 **/
static void
_close (CamelStream *stream)
{
	close ((CAMEL_STREAM_FS (stream))->fd);
}


static gint
_seek (CamelStream *stream, gint offset, CamelStreamSeekPolicy policy)
{
	int whence;
	switch  (policy) {
	case CAMEL_STREAM_SET:
		whence = SEEK_SET;
		break;
	case CAMEL_STREAM_CUR:
		whence = SEEK_CUR;
		break;
	case CAMEL_STREAM_END:
		whence = SEEK_END;
		break;
	default:
		return -1;
	}
		
		
	return lseek ((CAMEL_STREAM_FS (stream))->fd, offset, whence);
}
