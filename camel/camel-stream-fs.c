/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-fs.c : file system based stream */

/* inspired by gnome-stream-fs.c in bonobo by Miguel de Icaza */
/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 HelixCode (http://www.helixcode.com) .
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
#include "camel-log.h"

static CamelSeekableStreamClass *parent_class=NULL;


/* Returns the class for a CamelStreamFS */
#define CSFS_CLASS(so) CAMEL_STREAM_FS_CLASS (GTK_OBJECT(so)->klass)

static gint _read (CamelStream *stream, gchar *buffer, gint n);
static gint _write (CamelStream *stream, const gchar *buffer, gint n);
static void _flush (CamelStream *stream);
static gint _available (CamelStream *stream);
static gboolean _eos (CamelStream *stream);
static void _close (CamelStream *stream);
static gint _seek (CamelSeekableStream *stream, gint offset, CamelStreamSeekPolicy policy);

static void _finalize (GtkObject *object);
static void _destroy (GtkObject *object);

static void _init_with_fd (CamelStreamFs *stream_fs, int fd);
static void _init_with_fd_and_bounds (CamelStreamFs *stream_fs, int fd, guint32 inf_bound, gint32 sup_bound);
static void _init_with_name (CamelStreamFs *stream_fs, const gchar *name, CamelStreamFsMode mode);
static void _init_with_name_and_bounds (CamelStreamFs *stream_fs, const gchar *name, CamelStreamFsMode mode,
					guint32 inf_bound, gint32 sup_bound);

static void
camel_stream_fs_class_init (CamelStreamFsClass *camel_stream_fs_class)
{
	CamelSeekableStreamClass *camel_seekable_stream_class = CAMEL_SEEKABLE_STREAM_CLASS (camel_stream_fs_class);
	CamelStreamClass *camel_stream_class = CAMEL_STREAM_CLASS (camel_stream_fs_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_stream_fs_class);

	parent_class = gtk_type_class (camel_seekable_stream_get_type ());
	
	/* virtual method definition */
	camel_stream_fs_class->init_with_fd = _init_with_fd;
	camel_stream_fs_class->init_with_fd_and_bounds = _init_with_fd_and_bounds;
	camel_stream_fs_class->init_with_name = _init_with_name;
	camel_stream_fs_class->init_with_name_and_bounds = _init_with_name_and_bounds;
	
	/* virtual method overload */
	camel_stream_class->read = _read;
	camel_stream_class->write = _write;
	camel_stream_class->flush = _flush;
	camel_stream_class->available = _available;
	camel_stream_class->eos = _eos;
	camel_stream_class->close = _close;

	camel_seekable_stream_class->seek = _seek;

	gtk_object_class->finalize = _finalize;
	gtk_object_class->destroy = _destroy;

}

static void
camel_stream_fs_init (gpointer   object,  gpointer   klass)
{
	CamelStreamFs *stream = CAMEL_STREAM_FS (object);

	stream->name = NULL;
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
	
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelStreamFs::destroy\n");
}


static void           
_finalize (GtkObject *object)
{
	CamelStreamFs *stream_fs = CAMEL_STREAM_FS (object);


	CAMEL_LOG_FULL_DEBUG ("Entering CamelStreamFs::finalize\n");
	
	g_free (stream_fs->name);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelStreamFs::finalize\n");
}



static void 
_set_bounds (CamelStreamFs *stream_fs, guint32 inf_bound, guint32 sup_bound)
{
	
	printf ("sup_bounds = %u\n", sup_bound);
	/* store the bounds */
	stream_fs->inf_bound = inf_bound;
	stream_fs->sup_bound = sup_bound;

	/* go to the first position */
	lseek (stream_fs->fd, inf_bound, SEEK_SET);

	stream_fs->cur_pos = inf_bound;
	
	CAMEL_LOG_FULL_DEBUG ("In CamelStreamFs::_set_bounds, "
			      "setting inf bound to %u, "
			      "sup bound to %ld, current postion to %u from %u\n", 
			      stream_fs->inf_bound, stream_fs->sup_bound,
			      stream_fs->cur_pos, inf_bound);
	
}




static void
_init_with_fd (CamelStreamFs *stream_fs, int fd)
{
	stream_fs->fd = fd;
	stream_fs->inf_bound = 0;
	stream_fs->sup_bound = -1;
	stream_fs->cur_pos = 0;
}




static void
_init_with_fd_and_bounds (CamelStreamFs *stream_fs, int fd, guint32 inf_bound, gint32 sup_bound)
{
	
	CSFS_CLASS (stream_fs)->init_with_fd (stream_fs, fd);
	_set_bounds (stream_fs, inf_bound, sup_bound);
	
}



static void
_init_with_name (CamelStreamFs *stream_fs, const gchar *name, CamelStreamFsMode mode)
{
	struct stat s;
	int v, fd;
	int flags;
	
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
			return;
	}

	if ( (mode & CAMEL_STREAM_FS_READ) && !(mode & CAMEL_STREAM_FS_WRITE) )
		if (v == -1) return;

	fd = open (name, flags, 0600);
	if (fd==-1) {
		CAMEL_LOG_WARNING ( "CamelStreamFs::new_with_name can not obtain fd for file \"%s\"\n", name);
		CAMEL_LOG_FULL_DEBUG ( "  Full error text is : %s\n", strerror(errno));
		return;
	}
	
	stream_fs->name = g_strdup (name);
	CSFS_CLASS (stream_fs)->init_with_fd (stream_fs, fd);
		
}



static void
_init_with_name_and_bounds (CamelStreamFs *stream_fs, const gchar *name, CamelStreamFsMode mode,
			    guint32 inf_bound, gint32 sup_bound)
{
	CSFS_CLASS (stream_fs)->init_with_name (stream_fs, name, mode);
	_set_bounds (stream_fs, inf_bound, (gint32)sup_bound);
	CAMEL_LOG_FULL_DEBUG ("In CamelStreamFs::init_with_name_and_bounds, "
			      "setting inf bound to %u, "
			      "sup bound to %ld, current postion to %u\n", 
			      stream_fs->inf_bound, stream_fs->sup_bound,
			      stream_fs->cur_pos);
}




CamelStream *
camel_stream_fs_new_with_name (const gchar *name, CamelStreamFsMode mode)
{
	CamelStreamFs *stream_fs;
	stream_fs = gtk_type_new (camel_stream_fs_get_type ());
	CSFS_CLASS (stream_fs)->init_with_name (stream_fs, name, mode);
	
	return CAMEL_STREAM (stream_fs);
	
}
 

CamelStream *
camel_stream_fs_new_with_name_and_bounds (const gchar *name, CamelStreamFsMode mode,
					  guint32 inf_bound, gint32 sup_bound)
{
	CamelStreamFs *stream_fs;
	stream_fs = gtk_type_new (camel_stream_fs_get_type ());
	CSFS_CLASS (stream_fs)->init_with_name_and_bounds (stream_fs, name, mode, inf_bound, sup_bound);
	
	return CAMEL_STREAM (stream_fs);
	
}






CamelStream *
camel_stream_fs_new_with_fd (int fd)
{
	CamelStreamFs *stream_fs;
	
	CAMEL_LOG_FULL_DEBUG ( "Entering CamelStream::new_with_fd  fd=%d\n",fd);
	stream_fs = gtk_type_new (camel_stream_fs_get_type ());
	CSFS_CLASS (stream_fs)->init_with_fd (stream_fs, fd);

	
	return CAMEL_STREAM (stream_fs);
}



CamelStream *
camel_stream_fs_new_with_fd_and_bounds (int fd, guint32 inf_bound, gint32 sup_bound)
{
	CamelStreamFs *stream_fs;
	
	CAMEL_LOG_FULL_DEBUG ( "Entering CamelStream::new_with_fd  fd=%d\n",fd);
	stream_fs = gtk_type_new (camel_stream_fs_get_type ());
	CSFS_CLASS (stream_fs)->init_with_fd_and_bounds (stream_fs, fd, inf_bound, sup_bound);
	
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
	CamelStreamFs *stream_fs = CAMEL_STREAM_FS (stream);
	gint v;
	gint nb_to_read;
	
	if (stream_fs->sup_bound != -1)
		nb_to_read = MIN (stream_fs->sup_bound - stream_fs->cur_pos, n);
	else 
		nb_to_read = n;
	
	do {
		v = read ( (CAMEL_STREAM_FS (stream))->fd, buffer, nb_to_read);
	} while (v == -1 && errno == EINTR);
	if (v<0)
		CAMEL_LOG_FULL_DEBUG ("CamelStreamFs::read v=%d\n", v);
	else 
		stream_fs->cur_pos += v;

	return v;
}


/**
 * _write: write bytes to a stream
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
	CamelStreamFs *stream_fs = CAMEL_STREAM_FS (stream);
	int v;
	gint nb_to_write;
	
	g_assert (stream);
	g_assert (stream_fs->fd);
	CAMEL_LOG_FULL_DEBUG ( "CamelStreamFs:: entering write. n=%d\n", n);

	if (stream_fs->sup_bound != -1)
		nb_to_write =  MIN (stream_fs->sup_bound - stream_fs->cur_pos, n);
	else 
		nb_to_write = n;

	do {
		v = write ( stream_fs->fd, buffer, nb_to_write);
	} while (v == -1 && errno == EINTR);
	
#if HARD_LOG_LEVEL >= FULL_DEBUG
	if (v==-1) {
		perror("");
		CAMEL_LOG_FULL_DEBUG ( "CamelStreamFs::write could not write bytes in stream\n");
	}
#endif

	if (v>0)
		stream_fs->cur_pos += v;

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
	return -1;
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
	return FALSE;
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
_seek (CamelSeekableStream *stream, gint offset, CamelStreamSeekPolicy policy)
{
	int whence;
	gint return_position;
	gint real_offset; 
	CamelStreamFs *stream_fs = CAMEL_STREAM_FS (stream);

	switch  (policy) {
	case CAMEL_STREAM_SET:
		real_offset = MAX (stream_fs->inf_bound + offset, stream_fs->inf_bound);
		if (stream_fs->sup_bound > 0)
			real_offset = MIN (real_offset, stream_fs->sup_bound);
		whence = SEEK_SET;		
		break;

	case CAMEL_STREAM_CUR:
		if ((stream_fs->sup_bound == -1) && ((stream_fs->cur_pos + offset) > stream_fs->sup_bound)) {
			real_offset = stream_fs->sup_bound;
			whence = SEEK_SET;	
		} else if ((stream_fs->cur_pos + offset) < stream_fs->inf_bound) {
			real_offset = stream_fs->inf_bound;
			whence = SEEK_SET;	
		} else 
			{
				real_offset = offset;
				whence = SEEK_CUR;
			}
		break;

	case CAMEL_STREAM_END:
		if (stream_fs->sup_bound != -1) {
			real_offset = stream_fs->sup_bound - offset;
			whence = SEEK_SET;
		} else {
			real_offset = offset;
			whence = SEEK_END;
		}
		
		
		break;
	default:
		return -1;
	}
		
	
		
	return_position =  lseek (stream_fs->fd, real_offset, whence) - stream_fs->inf_bound;
	stream_fs->cur_pos = return_position;

	return return_position;
}











