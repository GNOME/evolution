/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Standard ETextModelReposFn definitions
 *
 * Copyright (C) 2001 Ximian Inc.
 *
 * Author: Jon Trowbridge <trow@ximian.com>
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
