/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-fs.h :stream based on unix filesystem */

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


#ifndef CAMEL_STREAM_FS_H
#define CAMEL_STREAM_FS_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include <stdio.h>
#include "camel-seekable-stream.h"

#define CAMEL_STREAM_FS_TYPE     (camel_stream_fs_get_type ())
#define CAMEL_STREAM_FS(obj)     (GTK_CHECK_CAST((obj), CAMEL_STREAM_FS_TYPE, CamelStreamFs))
#define CAMEL_STREAM_FS_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_STREAM_FS_TYPE, CamelStreamFsClass))
#define CAMEL_IS_STREAM_FS(o)    (GTK_CHECK_TYPE((o), CAMEL_STREAM_FS_TYPE))

typedef enum 
{
	CAMEL_STREAM_FS_READ   =   1,
	CAMEL_STREAM_FS_WRITE  =   2
} CamelStreamFsMode;


typedef struct 
{

	CamelSeekableStream parent_object;

	gchar *name;       /* name of the underlying file */
	gint fd;           /* file descriptor on the underlying file */
	guint32 inf_bound;   /* first valid position */
	gint32 sup_bound;    /* last valid position, -1 means, no sup bound */
	
} CamelStreamFs;



typedef struct {
	CamelSeekableStreamClass parent_class;
	
	/* Virtual methods */	
	void (*init_with_fd) (CamelStreamFs *stream_fs, int fd);
	void (*init_with_fd_and_bounds) (CamelStreamFs *stream_fs, int fd, guint32 inf_bound, gint32 sup_bound);
	void (*init_with_name) (CamelStreamFs *stream_fs, const gchar *name, CamelStreamFsMode mode);
	void (*init_with_name_and_bounds) (CamelStreamFs *stream_fs, const gchar *name, 
					   CamelStreamFsMode mode, guint32 inf_bound, gint32 sup_bound);

} CamelStreamFsClass;



/* Standard Gtk function */
GtkType camel_stream_fs_get_type (void);


/* public methods */
CamelStream *camel_stream_fs_new_with_name (const gchar *name, CamelStreamFsMode mode);
CamelStream *camel_stream_fs_new_with_name_and_bounds (const gchar *name, CamelStreamFsMode mode,
						       guint32 inf_bound, gint32 sup_bound);
CamelStream *camel_stream_fs_new_with_fd (int fd);
CamelStream *camel_stream_fs_new_with_fd_and_bounds (int fd,
						     guint32 inf_bound, gint32 sup_bound);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_STREAM_FS_H */
