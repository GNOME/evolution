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

/* file.c: index file read/write ops */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ibex_internal.h"

static unsigned long read_number (FILE *f);
static void write_number (FILE *f, unsigned long n);
static char *get_compressed_word (FILE *f, char **lastword);

static gint free_file (gpointer key, gpointer value, gpointer data);
static void free_word (gpointer key, gpointer value, gpointer data);

/* The file format is:
 *
 * version string (currently "ibex1")
 * file count
 * list of compressed filenames, separated by \0
 * word count
 * list of compressed words, each followed by \0, a count, and that
 *   many references.
 *
 * All numbers are stored 7-bit big-endian, with the high bit telling
 * whether or not the number continues to the next byte.
 *
 * compressed text consists of a byte telling how many characters the
 * line has in common with the line before it, followed by the rest of
 * the string. Obviously this only really works if the lists are sorted.
 */

/**
 * ibex_open: open (or possibly create) an ibex index
 * @file: the name of the file
 * @flags: open flags, see open(2).
 * @mode: If O_CREAT is passed in flags, then the file mode
 * to create the new file with.  It will be anded with the current
 * umask.
 *
 * Open and/or create the named ibex file and return a handle to it.
 *
 * Return value: an ibex handle, or NULL if an error occurred.
 **/
ibex *
ibex_open (char *file, int flags, int mode)
{
	ibex *ib;
	FILE *f;
	char vbuf[sizeof (IBEX_VERSION) - 1];
	char *word, *lastword;
	unsigned long nfiles, nwords, nrefs, ref;
	ibex_file **ibfs = NULL;
	int i;
	GPtrArray *refs;
	int fd;
	char *modestr;

	fd = open(file, flags, mode);
	if (fd == -1) {
		return NULL;
	}

	/* yuck, this is because we use FILE * interface
	   internally */
	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		modestr = "r";
		break;
	case O_RDWR:
		if (flags & O_APPEND)
			modestr = "a+";
		else
			modestr = "w+";
		break;
	case O_WRONLY:
		if (flags & O_APPEND)
			modestr = "a";
		else
			modestr = "w";
		break;
	default:
		if (flags & O_APPEND)
			modestr = "a+";
		else
			modestr = "r+";
		break;
	}

	f = fdopen(fd, modestr);
	if (f == NULL) {
		if (errno == 0)
			errno = ENOMEM;
		close(fd);
		return NULL;
	}

	ib = g_malloc (sizeof (ibex));
	ib->dirty = FALSE;
	ib->path = g_strdup (file);
	ib->files = g_tree_new ((GCompareFunc) strcmp);
	ib->words = g_hash_table_new (g_str_hash, g_str_equal);
	ib->oldfiles = g_ptr_array_new ();

	if (!f) {
		close(fd);
		return ib;
	}

	/* Check version.  If its empty, then we have just created it */
	if (fread (vbuf, 1, sizeof (vbuf), f) != sizeof (vbuf)) {
		if (feof (f)) {
			fclose(f);
			close(fd);
			return ib;
		}
	}
	if (strncmp (vbuf, IBEX_VERSION, sizeof (vbuf) != 0)) {
		errno = EINVAL;
		goto errout;
	}

	/* Read list of files. */
	nfiles = read_number (f);
	ibfs = g_malloc (nfiles * sizeof (ibex_file *));
	lastword = NULL;
	for (i = 0; i < nfiles; i++) {
		ibfs[i] = g_malloc (sizeof (ibex_file));
		ibfs[i]->name = get_compressed_word (f, &lastword);
		if (!ibfs[i]->name) {
			goto errout;
		}
		ibfs[i]->index = 0;
		g_tree_insert (ib->files, ibfs[i]->name, ibfs[i]);
	}

	/* Read list of words. */
	nwords = read_number (f);
	lastword = NULL;
	for (i = 0; i < nwords; i++) {
		word = get_compressed_word (f, &lastword);
		if (!word) {
			goto errout;
		}

		nrefs = read_number (f);
		refs = g_ptr_array_new ();
		g_ptr_array_set_size (refs, nrefs);
		while (nrefs--) {
			ref = read_number (f);
			if (ref >= nfiles) {
				goto errout;
			}
			refs->pdata[nrefs] = ibfs[ref];
		}

		g_hash_table_insert (ib->words, word, refs);
	}

	g_free (ibfs);
	fclose (f);
	close(fd);
	return ib;

errout:

	fclose (f);
	close(fd);
	g_tree_traverse (ib->files, free_file, G_IN_ORDER, NULL);
	g_tree_destroy (ib->files);
	g_hash_table_foreach (ib->words, free_word, NULL);
	g_hash_table_destroy (ib->words);
	g_ptr_array_free (ib->oldfiles, TRUE);
	if (ibfs)
		g_free (ibfs);
	g_free (ib->path);
	g_free (ib);

	return NULL;
}

struct ibex_write_data {
	unsigned long index;
	FILE *f;
	char *lastname;
};

/* This is an internal function to find the longest common initial
 * prefix between the last-written word and the current word.
 */
static int
get_prefix (struct ibex_write_data *iwd, char *name)
{
	int i = 0;
	if (iwd->lastname) {
		while (!strncmp (iwd->lastname, name, i + 1))
			i++;
	}
	iwd->lastname = name;
	return i;
}

static gint
write_file (gpointer key, gpointer value, gpointer data)
{
	char *file = key;
	ibex_file *ibf = value;
	struct ibex_write_data *iwd = data;
	int prefix;

	ibf->index = iwd->index++;
	prefix = get_prefix (iwd, file);
	fprintf (iwd->f, "%c%s", prefix, file + prefix);
	fputc (0, iwd->f);
	return FALSE;
}

/* scans for words which still exist in the index (after
   index removals), and adds them to the ordered tree for
   writing out in order */
static void
store_word (gpointer key, gpointer value, gpointer data)
{
	GTree *wtree = data;
	GPtrArray *refs = value;
	int i;
	ibex_file *ibf;

	for (i = 0; i < refs->len; i++) {
		ibf = g_ptr_array_index (refs, i);
		if (ibf->index == -1) {
			g_ptr_array_remove_index_fast (refs, i);
			i--;
		}
	}

	if (refs->len > 0) {
		g_tree_insert (wtree, key, value);
	}
}

/* writes a word out, in order */
static gint
write_word (gpointer key, gpointer value, gpointer data)
{
	char *word = key;
	GPtrArray *refs = value;
	struct ibex_write_data *iwd = data;
	int i, prefix;
	ibex_file *ibf;

	prefix = get_prefix (iwd, word);
	fprintf (iwd->f, "%c%s", prefix, word + prefix);
	fputc (0, iwd->f);

	write_number (iwd->f, refs->len);

	for (i = 0; i < refs->len; i++) {
		ibf = g_ptr_array_index (refs, i);
		write_number (iwd->f, ibf->index);
	}
	return FALSE;
}

/**
 * ibex_write: Write an ibex out to disk.
 * @ib: the ibex
 *
 * This writes an ibex to disk.
 *
 * Return value: 0 for success, -1 for failure (in which case errno
 * is set).
 **/
int
ibex_write (ibex *ib)
{
	struct ibex_write_data iwd;
	GTree *wtree;
	char *tmpfile;

	tmpfile = g_strdup_printf ("%s~", ib->path);
	iwd.f = fopen (tmpfile, "w");
	if (!iwd.f) {
		if (errno == 0)
			errno = ENOMEM;
		g_free (tmpfile);
		return -1;
	}

	fputs (IBEX_VERSION, iwd.f);
	if (ferror (iwd.f))
		goto lose;

	iwd.index = 0;
	iwd.lastname = NULL;
	write_number (iwd.f, g_tree_nnodes (ib->files));
	if (ferror (iwd.f))
		goto lose;
	g_tree_traverse (ib->files, write_file, G_IN_ORDER, &iwd);
	if (ferror (iwd.f))
		goto lose;

	iwd.lastname = NULL;
	wtree = g_tree_new ((GCompareFunc) strcmp);
	g_hash_table_foreach (ib->words, store_word, wtree);
	write_number (iwd.f, g_tree_nnodes(wtree));
	if (ferror (iwd.f))
		goto lose;
	g_tree_traverse (wtree, write_word, G_IN_ORDER, &iwd);
	g_tree_destroy (wtree);
	if (ferror (iwd.f))
		goto lose;

	if (fclose (iwd.f) == 0 && rename (tmpfile, ib->path) == 0) {
		g_free (tmpfile);
		ib->dirty = FALSE;
		return 0;
	}

lose:
	unlink (tmpfile);
	g_free (tmpfile);
	return -1;
}

/**
 * ibex_save:
 * @ib: 
 * 
 * Only write out an ibex if it is dirty.
 * 
 * Return value: Same as ibex_write.
 **/
int
ibex_save (ibex *ib)
{
	if (ib->dirty)
		return ibex_write(ib);
	return 0;
}

/**
 * ibex_close: Write out the ibex file (if it has changed) and free
 * the data associated with it.
 * @ib: the ibex
 *
 * If this ibex file has been modified since it was opened, this will
 * call ibex_write() to write it out to disk. It will then free all data
 * associated with the ibex. After calling ibex_close(), @ib will no
 * longer be a valid ibex.
 *
 * Return value: 0 on success, -1 on an ibex_write() failure (in which
 * case @ib will not be destroyed).
 **/
int
ibex_close (ibex *ib)
{
	ibex_file *ibf;

	if (ib->dirty && ibex_write (ib) == -1)
		return -1;

	g_tree_traverse (ib->files, free_file, G_IN_ORDER, NULL);
	g_tree_destroy (ib->files);
	g_hash_table_foreach (ib->words, free_word, NULL);
	g_hash_table_destroy (ib->words);

	while (ib->oldfiles->len) {
		ibf = g_ptr_array_remove_index (ib->oldfiles, 0);
		g_free (ibf->name);
		g_free (ibf);
	}
	g_ptr_array_free (ib->oldfiles, TRUE);
	g_free (ib->path);
	g_free (ib);

	return 0;
}

static gint
free_file (gpointer key, gpointer value, gpointer data)
{
	ibex_file *ibf = value;

	g_free (ibf->name);
	g_free (ibf);
	return FALSE;
}

static void
free_word (gpointer key, gpointer value, gpointer data)
{
	g_free (key);
	g_ptr_array_free (value, TRUE);
}

static char *
get_compressed_word (FILE *f, char **lastword)
{
	char *buf, *p;
	int c, size;

	c = getc (f);
	if (c == EOF)
		return NULL;

	size = c + 10;
	buf = g_malloc (size);
	if (*lastword)
		strncpy (buf, *lastword, c);
	p = buf + c;
	do {
		c = getc (f);
		if (c == EOF)
			return NULL;
		if (p == buf + size) {
			buf = g_realloc (buf, size + 10);
			p = buf + size;
			size += 10;
		}
		*p++ = c;
	} while (c != 0);

	*lastword = buf;
	return buf;
}

static void
write_number (FILE *f, unsigned long number)
{
	int i, flag = 0;
	char buf[4];

	i = 4;
	do {
		buf[--i] = (number & 0x7F) | flag;
		number = number >> 7;
		flag = 0x80;
	} while (number != 0);

	fwrite (buf + i, 1, 4 - i, f);
}

static unsigned long
read_number (FILE *f)
{
	int byte;
	unsigned long num;

	num = 0;
	do {
		byte = getc (f);
		num = num << 7 | (byte & 0x7F);
	} while (byte & 0x80);
  
	return num;
}

