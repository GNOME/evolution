/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-buffered-fs.c : file system based stream with buffer*/

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org>
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
#include "camel-stream-buffered-fs.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "camel-log.h"

static CamelStreamFsClass *parent_class=NULL;


/* Returns the class for a CamelStreamBufferedFs */
#define CSBFS_CLASS(so) CAMEL_STREAM_BUFFERED_FS_CLASS (GTK_OBJECT(so)->klass)
#define CSFS_CLASS(so) CAMEL_STREAM_FS_CLASS (GTK_OBJECT(so)->klass)
#define CS_CLASS(so) CAMEL_STREAM_CLASS (GTK_OBJECT(so)->klass)

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
camel_stream_buffered_fs_class_init (CamelStreamBufferedFsClass *camel_stream_buffered_fs_class)
{
	CamelStreamClass *camel_stream_class = CAMEL_STREAM_CLASS (camel_stream_buffered_fs_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_stream_buffered_fs_class);

	parent_class = gtk_type_class (camel_stream_fs_get_type ());
	
	/* virtual method definition */

	/* virtual method overload */
	camel_stream_class->read = _read;

	gtk_object_class->finalize = _finalize;
	gtk_object_class->destroy = _destroy;

}

static void
camel_stream_buffered_fs_init (gpointer   object,  gpointer   klass)
{
	CamelStreamBufferedFs *stream = CAMEL_STREAM_BUFFERED_FS (object);

	stream->buffer_size = 200;
	stream->read_buffer = g_new (gchar, stream->buffer_size);
	stream->write_buffer = g_new (gchar, stream->buffer_size);
	stream->read_pos = 0;
	stream->read_pos_max = 0;
	stream->write_pos = 0;
	stream->write_pos_max = stream->buffer_size;
}

GtkType
camel_stream_buffered_fs_get_type (void)
{
	static GtkType camel_stream_buffered_fs_type = 0;
	
	gdk_threads_enter ();
	if (!camel_stream_buffered_fs_type)	{
		GtkTypeInfo camel_stream_buffered_fs_info =	
		{
			"CamelStreamBufferedFs",
			sizeof (CamelStreamBufferedFs),
			sizeof (CamelStreamBufferedFsClass),
			(GtkClassInitFunc) camel_stream_buffered_fs_class_init,
			(GtkObjectInitFunc) camel_stream_buffered_fs_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_stream_buffered_fs_type = gtk_type_unique (camel_stream_fs_get_type (), &camel_stream_buffered_fs_info);
	}
	gdk_threads_leave ();
	return camel_stream_buffered_fs_type;
}


static void           
_destroy (GtkObject *object)
{
	CamelStreamBufferedFs *stream_buffered_fs = CAMEL_STREAM_BUFFERED_FS (object);
	
	CAMEL_LOG_FULL_DEBUG ("Entering CamelStreamBufferedFs::destroy\n");
	

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
	
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelStreamBufferedFs::destroy\n");
}


static void           
_finalize (GtkObject *object)
{
	CamelStreamBufferedFs *stream_buffered_fs = CAMEL_STREAM_BUFFERED_FS (object);


	CAMEL_LOG_FULL_DEBUG ("Entering CamelStreamBufferedFs::finalize\n");
	
	g_free (stream_buffered_fs->read_buffer);
	g_free (stream_buffered_fs->write_buffer);
	
	GTK_OBJECT_CLASS (parent_class)->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelStreamBufferedFs::finalize\n");
}



CamelStream *
camel_stream_buffered_fs_new_with_name (const gchar *name, CamelStreamFsMode mode)
{
	CamelStreamFs *stream_buffered_fs;
	stream_buffered_fs = gtk_type_new (camel_stream_buffered_fs_get_type ());
	CSFS_CLASS (stream_buffered_fs)->init_with_name (stream_buffered_fs, name, mode);
	
	return CAMEL_STREAM (stream_buffered_fs);
	
}

CamelStream *
camel_stream_buffered_fs_new_with_fd (int fd)
{
	CamelStreamFs *stream_buffered_fs;
	
	CAMEL_LOG_FULL_DEBUG ( "Entering CamelStream::new_with_fd  fd=%d\n",fd);
	stream_buffered_fs = gtk_type_new (camel_stream_buffered_fs_get_type ());
	CSFS_CLASS (stream_buffered_fs)->init_with_fd (stream_buffered_fs, fd);

	
	return CAMEL_STREAM (stream_buffered_fs);
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
	CamelStreamBufferedFs *sbf = CAMEL_STREAM_BUFFERED_FS (stream);
	gint v;
	gint nb_bytes_buffered;
	gint nb_bytes_to_read = n;
	gint nb_bytes_read = 0;
	gint bytes_chunk;
	gboolean eof = FALSE;

	g_return_val_if_fail (n>0, n);

	nb_bytes_buffered = sbf->read_pos_max - sbf->read_pos;
	while ( (!eof) && (nb_bytes_to_read>0)) { 
		if (nb_bytes_buffered <= 0) {
			/* have to read some data on disk */
			v = CAMEL_STREAM_CLASS (parent_class)->read (stream, sbf->read_buffer, sbf->buffer_size);
			nb_bytes_buffered = v;
			sbf->read_pos_max = v;
			sbf->read_pos = 0;
		}
		
		/* nb of bytes to put inside buffer */
		bytes_chunk = MIN (nb_bytes_buffered, nb_bytes_to_read);

		if (bytes_chunk > 0) {
			/* copy some bytes from the cache */
			memcpy (buffer, sbf->read_buffer + sbf->read_pos, bytes_chunk);
			nb_bytes_buffered -= bytes_chunk;
			nb_bytes_to_read -= bytes_chunk;
			nb_bytes_read += bytes_chunk;
			sbf->read_pos += bytes_chunk;
			 
		} else /* nb_bytes_to_read is >0 so if bytes_chunk is <0 
			* there was no data available */
			eof = TRUE;
	}
			
	
	
	return nb_bytes_read;
}


static gint
_write (CamelStream *stream, const gchar *buffer, gint n)
{
	return 0;
}



static void
_flush (CamelStream *stream)
{
	
}



static gint 
_available (CamelStream *stream)
{
	return 0;
}


static gboolean
_eos (CamelStream *stream)
{
	return FALSE;
}



static void
_close (CamelStream *stream)
{
	
}


static gint
_seek (CamelStream *stream, gint offset, CamelStreamSeekPolicy policy)
{
	return 0;
}
