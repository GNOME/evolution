/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Standard ETextModelReposFn definitions
 *
 * Copyright (C) 2001 Ximian Inc.
 *
 * Author: Jon Trowbridge <trow@ximian.com>
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
