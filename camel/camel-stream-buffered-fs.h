/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-bufered-fs.h :stream based on unix filesystem */

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


#ifndef CAMEL_STREAM_BUFFERED_FS_H
#define CAMEL_STREAM_BUFFERED_FS_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include <stdio.h>
#include "camel-stream-fs.h"

#define CAMEL_STREAM_BUFFERED_FS_TYPE     (camel_stream_buffered_fs_get_type ())
#define CAMEL_STREAM_BUFFERED_FS(obj)     (GTK_CHECK_CAST((obj), CAMEL_STREAM_BUFFERED_FS_TYPE, CamelStreamBufferedFs))
#define CAMEL_STREAM_BUFFERED_FS_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_STREAM_BUFFERED_FS_TYPE, CamelStreamBufferedFsClass))
#define IS_CAMEL_STREAM_BUFFERED_FS(o)    (GTK_CHECK_TYPE((o), CAMEL_STREAM_BUFFERED_FS_TYPE))

typedef enum 
{
	CAMEL_STREAM_BUFFERED_FS_READ   =   1,
	CAMEL_STREAM_BUFFERED_FS_WRITE  =   2
} CamelStreamBufferedFsMode;


typedef struct 
{
	CamelStreamFs parent_object;

	gint buffer_size;
	gchar *read_buffer;
	gint read_pos;
	gint read_pos_max;

	gchar *write_buffer;
	gint write_pos;
	gint write_pos_max;

} CamelStreamBufferedFs;



typedef struct {
	CamelStreamFsClass parent_class;
	
	/* Virtual methods */	

} CamelStreamBufferedFsClass;



/* Standard Gtk function */
GtkType camel_stream_buffered_fs_get_type (void);


/* public methods */
CamelStream *camel_stream_buffered_fs_new_with_name (const gchar *name, CamelStreamBufferedFsMode mode);
CamelStream *camel_stream_buffered_fs_new_with_fd (int fd);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_STREAM_BUFFERED_FS_H */
