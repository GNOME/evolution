/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* index.c: high-level indexing ops */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ibex_internal.h"

/**
 * ibex_index_file: Index a file by name
 * @ib: an ibex
 * @filename: name of the file to index
 *
 * This indexes the given file into @ib. If @filename is already in
 * the ibex, the existing index entries for it are discarded and the
 * file is indexed anew.
 *
 * Return value: 0 on success, -1 on failure.
 **/
int
ibex_index_file (ibex *ib, char *filename)
{
	int fd;
	int status;
	struct stat st;

	fd = open (filename, O_RDONLY);
	if (fd < 0)
		return -1;

	if (fstat (fd, &st) == -1) {
		close (fd);
		return -1;
	}
	if (!S_ISREG (st.st_mode)) {
		close (fd);
		errno = EINVAL;
		return -1;
	}

	ibex_unindex (ib, filename);
	status = ibex_index_fd (ib, filename, fd, st.st_size);
	close (fd);
	return status;
}

/**
 * ibex_index_fd: Index a file given a file descriptor
 * @ib: an ibex
 * @name: the name of the file being indexed
 * @fd: a file descriptor, open for reading
 * @len: the number of bytes to read from the file
 *
 * This indexes a file, or a part of a file, given an open file
 * descriptor and a size. There is no requirement that @name
 * actually correspond to @fd in any particular way.
 *
 * If the function returns successfully, the file descriptor offset of
 * @fd will be exactly @len bytes beyond where it was when the
 * function was called. The indexer assumes that this point is a word
 * boundary.
 *
 * The behavior of this function is not defined if it is not
 * possible to read @len bytes off @fd.
 *
 * Return value: 0 on success, -1 on failure.
 **/
int
ibex_index_fd (ibex *ib, char *name, int fd, size_t len)
{
	char *buf;
	int off = 0, nread, status;

	buf = g_malloc (len);
	do {
		nread = read (fd, buf + off, len - off);
		if (nread == -1) {
			g_free (buf);
			return -1;
		}
		off += nread;
	} while (off != len);

	status = ibex_index_buffer (ib, name, buf, len, NULL);
	g_free (buf);

	return status;
}

/**
 * ibex_unindex: Remove a file from the ibex
 * @ib: an ibex
 * @name: name of the file to remove
 *
 * This removes all references to @name from @ib. No memory is freed
 * right away, but further searches on @ib will never return @name.
 **/
void
ibex_unindex (ibex *ib, char *name)
{
	ibex_file *ibf;

	ibf = g_tree_lookup (ib->files, name);
	if (ibf) {
		ibf->index = -1;
		g_tree_remove (ib->files, name);
		g_ptr_array_add (ib->oldfiles, ibf);
		ib->dirty = TRUE;
	}
}

/**
 * ibex_rename: Rename a file in the ibex
 * @ib: an ibex
 * @oldname: the old name of the file
 * @newname: the new name of the file
 *
 * This renames a file in the ibex.
 **/
void
ibex_rename (ibex *ib, char *oldname, char *newname)
{
	ibex_file *ibf;

	ibf = g_tree_lookup (ib->files, oldname);
	if (ibf) {
		g_tree_remove (ib->files, oldname);
		g_free (ibf->name);
		ibf->name = g_strdup (newname);
		g_tree_insert (ib->files, ibf->name, ibf);
	}
}
