/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2004 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef EM_VFS_STREAM_H
#define EM_VFS_STREAM_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EM_VFS_STREAM_TYPE     (em_vfs_stream_get_type ())
#define EM_VFS_STREAM(obj)     (CAMEL_CHECK_CAST((obj), EM_VFS_STREAM_TYPE, EMVFSStream))
#define EM_VFS_STREAM_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), EM_VFS_STREAM_TYPE, EMVFSStreamClass))
#define EM_IS_VFS_STREAM(o)    (CAMEL_CHECK_TYPE((o), EM_VFS_STREAM_TYPE))

#include <glib.h>
#include <camel/camel-seekable-stream.h>
#include <libgnomevfs/gnome-vfs.h>

typedef struct _EMVFSStream EMVFSStream;
typedef struct _EMVFSStreamClass EMVFSStreamClass;

struct _EMVFSStream {
	CamelSeekableStream parent_stream;

	GnomeVFSHandle *handle;
};

struct _EMVFSStreamClass {
	CamelSeekableStreamClass parent_class;
};

CamelType    em_vfs_stream_get_type (void);
EMVFSStream *emvfs_stream_new(GnomeVFSHandle *handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EM_VFS_STREAM_H */
