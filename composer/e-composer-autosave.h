/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_COMPOSER_AUTOSAVE_H
#define E_COMPOSER_AUTOSAVE_H

#include "e-composer-common.h"
#include "e-msg-composer.h"

G_BEGIN_DECLS

GList *		e_composer_autosave_find_orphans (GError **error);

void		e_composer_autosave_register	 (EMsgComposer *composer);
void		e_composer_autosave_unregister	 (EMsgComposer *composer);
gboolean	e_composer_autosave_snapshot	 (EMsgComposer *composer);
gint		e_composer_autosave_get_fd	 (EMsgComposer *composer);
const gchar *	e_composer_autosave_get_filename (EMsgComposer *composer);
gboolean	e_composer_autosave_get_enabled  (EMsgComposer *composer);
void		e_composer_autosave_set_enabled	 (EMsgComposer *composer,
						  gboolean enabled);
gboolean	e_composer_autosave_get_saved	 (EMsgComposer *composer);
void		e_composer_autosave_set_saved	 (EMsgComposer *composer,
						  gboolean saved);

G_END_DECLS

#endif /* E_COMPOSER_AUTOSAVE_H */
