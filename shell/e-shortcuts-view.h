/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shortcuts-view.h
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

#ifndef _E_SHORTCUTS_VIEW_H_
#define _E_SHORTCUTS_VIEW_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "shortcut-bar/e-shortcut-bar.h"
#include "e-shortcuts.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_SHORTCUTS_VIEW			(e_shortcuts_view_get_type ())
#define E_SHORTCUTS_VIEW(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_SHORTCUTS_VIEW, EShortcutsView))
#define E_SHORTCUTS_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SHORTCUTS_VIEW, EShortcutsViewClass))
#define E_IS_SHORTCUTS_VIEW(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_SHORTCUTS_VIEW))
#define E_IS_SHORTCUTS_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_SHORTCUTS_VIEW))


typedef struct _EShortcutsView        EShortcutsView;
typedef struct _EShortcutsViewPrivate EShortcutsViewPrivate;
typedef struct _EShortcutsViewClass   EShortcutsViewClass;

struct _EShortcutsView {
	EShortcutBar parent;

	EShortcutsViewPrivate *priv;
};

struct _EShortcutsViewClass {
	EShortcutBarClass parent_class;

	void (*  activate_shortcut) (EShortcutsView *view,
				     EShortcuts *shortcuts,
				     const char *uri);
};


GtkType    e_shortcuts_view_get_type   (void);
void       e_shortcuts_view_construct  (EShortcutsView *shortcuts_view,
					EShortcuts     *shortcuts);
GtkWidget *e_shortcuts_view_new        (EShortcuts     *shortcuts);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SHORTCUTS_VIEW_H_ */
