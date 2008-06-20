/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-text-model-repos.h - Standard ETextModelReposFn definitions
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Authors:
 *   Jon Trowbridge <trow@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef E_TEXT_MODEL_REPOS_H
#define E_TEXT_MODEL_REPOS_H

#include "e-text-model.h"

typedef struct {
	ETextModel *model;
	gint change;  /* Relative change to position. */
} EReposShift;

gint e_repos_shift (gint pos, gpointer data);


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


/* For e_repos_clamp, data is a pointer to an ETextModel.  The only repositioning
   that occurs is to avoid buffer overruns. */

gint e_repos_clamp (gint pos, gpointer data);

#endif
