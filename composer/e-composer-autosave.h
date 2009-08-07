/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifndef E_COMPOSER_AUTOSAVE_H
#define E_COMPOSER_AUTOSAVE_H

#include "e-composer-common.h"
#include "e-msg-composer.h"

G_BEGIN_DECLS

GList *		e_composer_autosave_find_orphans (GError **error);

void		e_composer_autosave_register	 (EMsgComposer *composer);
void		e_composer_autosave_unregister	 (EMsgComposer *composer,
						  gboolean delete_file);
void		e_composer_autosave_snapshot_async
						 (EMsgComposer *composer,
						  GAsyncReadyCallback callback,
						  gpointer user_data);
gboolean	e_composer_autosave_snapshot_finish
						 (EMsgComposer *composer,
						  GAsyncResult *result,
						  GError **error);
gchar *		e_composer_autosave_get_filename (EMsgComposer *composer);
gboolean	e_composer_autosave_get_enabled  (EMsgComposer *composer);
void		e_composer_autosave_set_enabled	 (EMsgComposer *composer,
						  gboolean enabled);
gboolean	e_composer_autosave_get_saved	 (EMsgComposer *composer);
void		e_composer_autosave_set_saved	 (EMsgComposer *composer,
						  gboolean saved);

G_END_DECLS

#endif /* E_COMPOSER_AUTOSAVE_H */
