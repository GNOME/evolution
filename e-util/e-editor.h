/*
 * e-editor.h
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_EDITOR_H
#define E_EDITOR_H

#include <gtk/gtk.h>
#include <e-util/e-editor-widget.h>

/* Standard GObject macros */
#define E_TYPE_EDITOR \
	(e_editor_get_type ())
#define E_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_EDITOR, EEditor))
#define E_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_EDITOR, EEditorClass))
#define E_IS_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_EDITOR))
#define E_IS_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_EDITOR))
#define E_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_EDITOR, EEditorClass))

G_BEGIN_DECLS

typedef struct _EEditor EEditor;
typedef struct _EEditorClass EEditorClass;
typedef struct _EEditorPrivate EEditorPrivate;

struct _EEditor {
	GtkGrid parent;
	EEditorPrivate *priv;
};

struct _EEditorClass {
	GtkGridClass parent_class;

	void		(*update_actions)	(EEditor *editor,
						 GdkEventButton *event);
	void		(*spell_languages_changed)
						(EEditor *editor,
						 GList *dictionaries);
};

GType		e_editor_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_editor_new			(void);
EEditorWidget *	e_editor_get_editor_widget	(EEditor *editor);
GtkBuilder *	e_editor_get_builder		(EEditor *editor);
GtkUIManager *	e_editor_get_ui_manager		(EEditor *editor);
GtkAction *	e_editor_get_action		(EEditor *editor,
						 const gchar *action_name);
GtkActionGroup *e_editor_get_action_group	(EEditor *editor,
						 const gchar *group_name);
GtkWidget *	e_editor_get_widget		(EEditor *editor,
						 const gchar *widget_name);
GtkWidget *	e_editor_get_managed_widget	(EEditor *editor,
						 const gchar *widget_path);
const gchar *	e_editor_get_filename		(EEditor *editor);
void		e_editor_set_filename		(EEditor *editor,
						 const gchar *filename);
void		e_editor_pack_above		(EEditor *editor,
						 GtkWidget *child);
void		e_editor_emit_spell_languages_changed
						(EEditor *editor);

/*****************************************************************************
 * High-Level Editing Interface
 *****************************************************************************/

gboolean	e_editor_save			(EEditor *editor,
						 const gchar *filename,
						 gboolean as_html,
						 GError **error);

G_END_DECLS

#endif /* E_EDITOR_H */
