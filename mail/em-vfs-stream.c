/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors:  Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Ximian, Inc. (www.ximian.com)
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
 * A GnomeVFS to CamelStream mapper.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <libgnomevfs/gnome-vfs.h>

#include "em-vfs-stream.h"

#define LOG_STREAM

#define d(x) 

#define EMVS_CLASS(x) ((EMVFSStreamClass *)(((CamelObject *)(x))->klass))

static CamelStreamClass *parent_class = NULL;

static void
em_vfs_stream_init (CamelObject *object)
{
	/*EMVFSStream *emvfs = (EMVFSStream *)object;*/
}

static void
em_vfs_stream_finalize (CamelObject *object)
{
	EMVFSStream *emvfs = (EMVFSStream *)object;

	if (emvfs->handle)
		gnome_vfs_close(emvfs->handle);
}

static void
emvfs_set_errno(GnomeVFSResult res)
{
	switch(res) {
	case GNOME_VFS_OK:
		g_warning("em-vfs-stream: calling set_errno with no error");
		break;
	case GNOME_VFS_ERROR_NOT_FOUND:
	case GNOME_VFS_ERROR_HOST_NOT_FOUND:
	case GNOME_VFS_ERROR_INVALID_HOST_NAME:
	case GNOME_VFS_ERROR_HOST_HAS_NO_ADDRESS:
	case GNOME_VFS_ERROR_SERVICE_NOT_AVAILABLE:
		errno = ENOENT;
		break;
	case GNOME_VFS_ERROR_GENERIC:
	case GNOME_VFS_ERROR_INTERNAL:
	case GNOME_VFS_ERROR_IO:
	case GNOME_VFS_ERROR_EOF: /* will be caught by read before here anyway */
	case GNOME_VFS_ERROR_SERVICE_OBSOLETE:
	case GNOME_VFS_ERROR_PROTOCOL_ERROR:
	default:
		errno = EIO;
		break;
	case GNOME_VFS_ERROR_BAD_PARAMETERS:
	case GNOME_VFS_ERROR_NOT_SUPPORTED:
	case GNOME_VFS_ERROR_INVALID_URI:
	case GNOME_VFS_ERROR_NOT_OPEN:
	case GNOME_VFS_ERROR_INVALID_OPEN_MODE:
	case GNOME_VFS_ERROR_NOT_SAME_FILE_SYSTEM:
		errno = EINVAL;
		break;
	case GNOME_VFS_ERROR_CORRUPTED_DATA: /* not sure about these */
	case GNOME_VFS_ERROR_WRONG_FORMAT:
	case GNOME_VFS_ERROR_BAD_FILE:
		errno = EBADF;
		break;
	case GNOME_VFS_ERROR_TOO_BIG:
		errno = E2BIG;
		break;
	case GNOME_VFS_ERROR_NO_SPACE:
		errno = ENOSPC;
		break;
	case GNOME_VFS_ERROR_READ_ONLY:
	case GNOME_VFS_ERROR_READ_ONLY_FILE_SYSTEM:
		errno = EROFS;
		break;
	case GNOME_VFS_ERROR_TOO_MANY_OPEN_FILES:
		errno = EMFILE;
		break;
	case GNOME_VFS_ERROR_NOT_A_DIRECTORY:
		errno = ENOTDIR;
		break;
	case GNOME_VFS_ERROR_IN_PROGRESS:
		errno = EINPROGRESS;
		break;
	case GNOME_VFS_ERROR_INTERRUPTED:
		errno = EINTR;
		break;
	case GNOME_VFS_ERROR_FILE_EXISTS:
		errno = EEXIST;
	case GNOME_VFS_ERROR_LOOP:
		errno = ELOOP;
		break;
	case GNOME_VFS_ERROR_ACCESS_DENIED:
	case GNOME_VFS_ERROR_NOT_PERMITTED:
	case GNOME_VFS_ERROR_LOGIN_FAILED:
		errno = EPERM;
		break;
	case GNOME_VFS_ERROR_IS_DIRECTORY:
	case GNOME_VFS_ERROR_DIRECTORY_NOT_EMPTY: /* ?? */
		errno = EISDIR;
		break;
	case GNOME_VFS_ERROR_NO_MEMORY:
		errno = ENOMEM;
		break;
	case GNOME_VFS_ERROR_CANCELLED:
		errno = EINTR;
		break;
	case GNOME_VFS_ERROR_DIRECTORY_BUSY:
		errno = EBUSY;
		break;
	case GNOME_VFS_ERROR_TOO_MANY_LINKS:
		errno = EMLINK;
		break;
	case GNOME_VFS_ERROR_NAME_TOO_LONG:
		errno = ENAMETOOLONG;
		break;
	}
}

static ssize_t
emvfs_read(CamelStream *stream, char *buffer, size_t n)
{
	EMVFSStream *emvfs = EM_VFS_STREAM (stream);
	GnomeVFSFileSize count;
	GnomeVFSResult res;

	if (emvfs->handle == NULL) {
		errno = EINVAL;
		return -1;
	}

	/* TODO: handle camel cancellation? */

	res = gnome_vfs_read(emvfs->handle, buffer, n, &count);
	if (res == GNOME_VFS_OK)
		return (ssize_t)count;
	else if (res == GNOME_VFS_ERROR_EOF) {
		stream->eos = TRUE;
		return 0;
	}

	emvfs_set_errno(res);

	return -1;
}

static ssize_t
emvfs_write(CamelStream *stream, const char *buffer, size_t n)
{
	EMVFSStream *emvfs = EM_VFS_STREAM (stream);
	GnomeVFSFileSize count;
	GnomeVFSResult res;

	if (emvfs->handle == NULL) {
		errno = EINVAL;
		return -1;
	}

	res = gnome_vfs_write(emvfs->handle, buffer, n, &count);
	if (res == GNOME_VFS_OK)
		return (ssize_t)count;

	emvfs_set_errno(res);

	return -1;
}

static int
emvfs_close(CamelStream *stream)
{
	EMVFSStream *emvfs = EM_VFS_STREAM (stream);
	GnomeVFSResult res;

	if (emvfs->handle == NULL) {
		errno = EINVAL;
		return -1;
	}

	res = gnome_vfs_close(emvfs->handle);
	emvfs->handle = NULL;
	if (res == GNOME_VFS_OK)
		return 0;

	emvfs_set_errno(res);

	return -1;
}

static off_t
emvfs_seek(CamelSeekableStream *stream, off_t offset, CamelStreamSeekPolicy policy)
{
	EMVFSStream *emvfs = EM_VFS_STREAM (stream);
	GnomeVFSSeekPosition vpolicy;
	GnomeVFSFileSize pos;
	GnomeVFSResult res;

	if (emvfs->handle == NULL) {
		errno = EINVAL;
		return -1;
	}

	switch (policy) {
	case CAMEL_STREAM_SET:
	default:
		vpolicy = GNOME_VFS_SEEK_START;
		break;
	case CAMEL_STREAM_CUR:
		vpolicy = GNOME_VFS_SEEK_CURRENT;
		break;
	case CAMEL_STREAM_END:
		vpolicy = GNOME_VFS_SEEK_END;
		break;
	}

	if ( (res = gnome_vfs_seek(emvfs->handle, vpolicy, offset)) == GNOME_VFS_OK
	     && (res = gnome_vfs_tell(emvfs->handle, &pos)) == GNOME_VFS_OK)
		return pos;

	emvfs_set_errno(res);

	return -1;
}

static off_t
emvfs_tell(CamelSeekableStream *stream)
{
	EMVFSStream *emvfs = EM_VFS_STREAM (stream);
	GnomeVFSFileSize pos;
	GnomeVFSResult res;

	if (emvfs->handle == NULL) {
		errno = EINVAL;
		return -1;
	}

	if ((res = gnome_vfs_tell(emvfs->handle, &pos)) == GNOME_VFS_OK)
		return pos;

	emvfs_set_errno(res);

	return -1;
}

static void
em_vfs_stream_class_init (EMVFSStreamClass *klass)
{
	((CamelStreamClass *)klass)->read = emvfs_read;
	((CamelStreamClass *)klass)->write = emvfs_write;
	((CamelStreamClass *)klass)->close = emvfs_close;

	((CamelSeekableStreamClass *)klass)->seek = emvfs_seek;
	((CamelSeekableStreamClass *)klass)->tell = emvfs_tell;
	/* set_bounds? */
}

CamelType
em_vfs_stream_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		parent_class = (CamelStreamClass *)camel_seekable_stream_get_type();
		type = camel_type_register ((CamelType)parent_class,
					    "EMVFSStream",
					    sizeof (EMVFSStream),
					    sizeof (EMVFSStreamClass),
					    (CamelObjectClassInitFunc) em_vfs_stream_class_init,
					    NULL,
					    (CamelObjectInitFunc) em_vfs_stream_init,
					    (CamelObjectFinalizeFunc) em_vfs_stream_finalize);
	}
	
	return type;
}

/**
 * emvfs_stream_new:
 * @handle: 
 * 
 * Create a new camel stream from a GnomeVFS handle.  The camel stream
 * will own the handle from now on.
 * 
 * Return value: A CamelStream that will talk to @handle.  This function cannot fail.
 **/
EMVFSStream *
emvfs_stream_new(GnomeVFSHandle *handle)
{
	EMVFSStream *emvfs;

	emvfs = (EMVFSStream *)camel_object_new(em_vfs_stream_get_type());
	emvfs->handle = handle;

	return emvfs;
}
