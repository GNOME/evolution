/*
 * e-text-model-repos.h - Standard ETextModelReposFn definitions
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Jon Trowbridge <trow@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TEXT_MODEL_REPOS_H
#define E_TEXT_MODEL_REPOS_H

#include "e-text-model.h"

typedef struct {
	ETextModel *model;
	gint pos;  /* Position to move to.  Negative values count from the end buffer.
		      (i.e. -1 puts cursor at the end, -2 one character from end, etc.) */
} EReposAbsolute;

gint e_repos_absolute (gint pos, gpointer data);

typedef struct {
	ETextModel *model;
	gint pos;  /* Location of first inserted character. */
	gint len;  /* Number of characters inserted. */
} EReposInsertShift;

gint e_repos_insert_shift (gint pos, gpointer data);

typedef struct {
	ETextModel *model;
	gint pos;  /* Location of first deleted character. */
	gint len;  /* Number of characters deleted. */
} EReposDeleteShift;

gint e_repos_delete_shift (gint pos, gpointer data);

#endif
