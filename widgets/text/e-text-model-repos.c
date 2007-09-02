/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-text-model-repos.c - Standard ETextModelReposFn definitions
 * Copyright 2000, 2001, Ximian, Inc.
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

#include "e-text-model-repos.h"

#define MODEL_CLAMP(model, pos) (CLAMP((pos), 0, strlen((model)->text)))

gint
e_repos_shift (gint pos, gpointer data)
{
	EReposShift *info = (EReposShift *) data;
	g_return_val_if_fail (data, -1);

	return e_text_model_validate_position (info->model, pos + info->change);
}

gint
e_repos_absolute (gint pos, gpointer data)
{
	EReposAbsolute *info = (EReposAbsolute *) data;
	g_return_val_if_fail (data, -1);

	pos = info->pos;
	if (pos < 0) {
		gint len = e_text_model_get_text_length (info->model);
		pos += len + 1;
	}
	
	return e_text_model_validate_position (info->model, pos);
}

gint
e_repos_insert_shift (gint pos, gpointer data)
{
	EReposInsertShift *info = (EReposInsertShift *) data;
	g_return_val_if_fail (data, -1);

	if (pos >= info->pos)
		pos += info->len;

	return e_text_model_validate_position (info->model, pos);
}

gint
e_repos_delete_shift (gint pos, gpointer data)
{
	EReposDeleteShift *info = (EReposDeleteShift *) data;
	g_return_val_if_fail (data, -1);

	if (pos > info->pos + info->len)
		pos -= info->len;
	else if (pos > info->pos)
		pos = info->pos;

	return e_text_model_validate_position (info->model, pos);
}

gint
e_repos_clamp (gint pos, gpointer data)
{
	ETextModel *model;

	g_return_val_if_fail (data != NULL && E_IS_TEXT_MODEL (data), -1);
	model = E_TEXT_MODEL (data);

	return e_text_model_validate_position (model, pos);
}
