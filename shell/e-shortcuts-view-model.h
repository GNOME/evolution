/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shortcuts-view-model.h
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifndef _E_SHORTCUTS_VIEW_MODEL_H_
#define _E_SHORTCUTS_VIEW_MODEL_H_

#include "e-shortcuts.h"

#include "shortcut-bar/e-shortcut-model.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_SHORTCUTS_VIEW_MODEL			(e_shortcuts_view_model_get_type ())
#define E_SHORTCUTS_VIEW_MODEL(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_SHORTCUTS_VIEW_MODEL, EShortcutsViewModel))
#define E_SHORTCUTS_VIEW_MODEL_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SHORTCUTS_VIEW_MODEL, EShortcutsViewModelClass))
#define E_IS_SHORTCUTS_VIEW_MODEL(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_SHORTCUTS_VIEW_MODEL))
#define E_IS_SHORTCUTS_VIEW_MODEL_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_SHORTCUTS_VIEW_MODEL))


typedef struct _EShortcutsViewModel        EShortcutsViewModel;
typedef struct _EShortcutsViewModelPrivate EShortcutsViewModelPrivate;
typedef struct _EShortcutsViewModelClass   EShortcutsViewModelClass;

struct _EShortcutsViewModel {
	EShortcutModel parent;

	EShortcutsViewModelPrivate *priv;
};

struct _EShortcutsViewModelClass {
	EShortcutModelClass parent_class;
};


GtkType              e_shortcuts_view_model_get_type   (void);
void                 e_shortcuts_view_model_construct  (EShortcutsViewModel *model,
							EShortcuts          *shortcuts);
EShortcutsViewModel *e_shortcuts_view_model_new        (EShortcuts          *shortcuts);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SHORTCUTS_VIEW_MODEL_H_ */
