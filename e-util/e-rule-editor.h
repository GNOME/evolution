/*
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
 *
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_RULE_EDITOR_H
#define E_RULE_EDITOR_H

#include <gtk/gtk.h>

#include <e-util/e-rule-context.h>
#include <e-util/e-filter-rule.h>

/* Standard GObject macros */
#define E_TYPE_RULE_EDITOR \
	(e_rule_editor_get_type ())
#define E_RULE_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_RULE_EDITOR, ERuleEditor))
#define E_RULE_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_RULE_EDITOR, ERuleEditorClass))
#define E_IS_RULE_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_RULE_EDITOR))
#define E_IS_RULE_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_RULE_EDITOR))
#define E_RULE_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_RULE_EDITOR, ERuleEditorClass))

G_BEGIN_DECLS

typedef struct _ERuleEditor ERuleEditor;
typedef struct _ERuleEditorClass ERuleEditorClass;
typedef struct _ERuleEditorPrivate ERuleEditorPrivate;

struct _ERuleEditor {
	GtkDialog parent;

	GtkListStore *model;
	GtkTreeView *list;

	ERuleContext *context;
	EFilterRule *current;
	EFilterRule *edit;	/* for editing/adding rules, so we only do 1 at a time */

	GtkWidget *dialog;

	gchar *source;

	ERuleEditorPrivate *priv;
};

struct _ERuleEditorClass {
	GtkDialogClass parent_class;

	void		(*set_sensitive)	(ERuleEditor *editor);
	void		(*set_source)		(ERuleEditor *editor,
						 const gchar *source);

	EFilterRule *	(*create_rule)		(ERuleEditor *editor);
};

GType		e_rule_editor_get_type		(void) G_GNUC_CONST;
ERuleEditor *	e_rule_editor_new		(ERuleContext *context,
						 const gchar *source,
						 const gchar *label);
void		e_rule_editor_construct		(ERuleEditor *editor,
						 ERuleContext *context,
						 GtkBuilder *builder,
						 const gchar *source,
						 const gchar *label);
void		e_rule_editor_set_source	(ERuleEditor *editor,
						 const gchar *source);
void		e_rule_editor_set_sensitive	(ERuleEditor *editor);
EFilterRule *	e_rule_editor_create_rule	(ERuleEditor *editor);

G_END_DECLS

#endif /* E_RULE_EDITOR_H */
