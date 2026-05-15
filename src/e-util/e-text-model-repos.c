/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Jon Trowbridge <trow@ximian.com>
 */

#include "evolution-config.h"

#include "e-text-model-repos.h"

#define MODEL_CLAMP(model, pos) (CLAMP((pos), 0, strlen((model)->text)))

gint
e_repos_absolute (gint pos,
                  gpointer data)
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
e_repos_insert_shift (gint pos,
                      gpointer data)
{
	EReposInsertShift *info = (EReposInsertShift *) data;
	g_return_val_if_fail (data, -1);

	if (pos >= info->pos)
		pos += info->len;

	return e_text_model_validate_position (info->model, pos);
}

gint
e_repos_delete_shift (gint pos,
                      gpointer data)
{
	EReposDeleteShift *info = (EReposDeleteShift *) data;
	g_return_val_if_fail (data, -1);

	if (pos > info->pos + info->len)
		pos -= info->len;
	else if (pos > info->pos)
		pos = info->pos;

	return e_text_model_validate_position (info->model, pos);
}
