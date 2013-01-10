/*
 * e-editor-window.h
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
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
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_EDITOR_WINDOW_H
#define E_EDITOR_WINDOW_H

#include <gtk/gtk.h>
#include <e-util/e-editor.h>

/* Standard GObject macros */
#define E_TYPE_EDITOR_WINDOW \
	(e_editor_window_get_type ())
#define E_EDITOR_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_EDITOR_WINDOW, EEditorWindow))
#define E_EDITOR_WINDOW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_EDITOR_WINDOW, EEditorWindowClass))
#define E_IS_EDITOR_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_EDITOR_WINDOW))
#define E_IS_EDITOR_WINDOW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_EDITOR_WINDOW))
#define E_EDITOR_WINDOW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_EDITOR_WINDOW, EEditorWindowClass))

G_BEGIN_DECLS

typedef struct _EEditorWindow EEditorWindow;
typedef struct _EEditorWindowClass EEditorWindowClass;
typedef struct _EEditorWindowPrivate EEditorWindowPrivate;

struct _EEditorWindow {
	GtkWindow parent;
	EEditorWindowPrivate *priv;
};

struct _EEditorWindowClass {
	GtkWindowClass parent_class;
};

GType		e_editor_window_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_editor_window_new		(GtkWindowType type);
EEditor *	e_editor_window_get_editor	(EEditorWindow *window);
void		e_editor_window_pack_above	(EEditorWindow *window,
						 GtkWidget *child);
void		e_editor_window_pack_inside	(EEditorWindow *window,
						 GtkWidget *child);
void		e_editor_window_pack_below	(EEditorWindow *window,
						 GtkWidget *child);

G_END_DECLS

#endif /* E_EDITOR_WINDOW_H */
