/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CATEGORY_EDITOR_H
#define E_CATEGORY_EDITOR_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_CATEGORY_EDITOR \
	(e_category_editor_get_type ())
#define E_CATEGORY_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CATEGORY_EDITOR, ECategoryEditor))
#define E_CATEGORY_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CATEGORY_EDITOR, ECategoryEditorClass))
#define E_IS_CATEGORY_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CATEGORY_EDITOR))
#define E_IS_CATEGORY_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CATEGORY_EDITOR))
#define E_CATEGORY_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CATEGORY_EDITOR, ECategoryEditorClass))

G_BEGIN_DECLS

typedef struct _ECategoryEditor ECategoryEditor;
typedef struct _ECategoryEditorClass ECategoryEditorClass;
typedef struct _ECategoryEditorPrivate ECategoryEditorPrivate;

/**
 * ECategoryEditor:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 *
 * Since: 3.2
 **/
struct _ECategoryEditor {
	GtkDialog parent;
	ECategoryEditorPrivate *priv;
};

struct _ECategoryEditorClass {
	GtkDialogClass parent_class;
};

GType		e_category_editor_get_type	(void) G_GNUC_CONST;
ECategoryEditor *
		e_category_editor_new		(void);
const gchar *	e_category_editor_create_category
						(ECategoryEditor *editor);
gboolean	e_category_editor_edit_category	(ECategoryEditor *editor,
						 const gchar *category);

G_END_DECLS

#endif /* E_CATEGORY_EDITOR_H */
