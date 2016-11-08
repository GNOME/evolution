/*
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
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_TAG_EDITOR_H
#define E_MAIL_TAG_EDITOR_H

#include <gtk/gtk.h>
#include <camel/camel.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_TAG_EDITOR \
	(e_mail_tag_editor_get_type ())
#define E_MAIL_TAG_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_TAG_EDITOR, EMailTagEditor))
#define E_MAIL_TAG_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_TAG_EDITOR, EMailTagEditorClass))
#define E_IS_MAIL_TAG_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_TAG_EDITOR))
#define E_IS_MAIL_TAG_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_TAG_EDITOR))
#define E_MAIL_TAG_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_TAG_EDITOR, EMailTagEditorClass))

G_BEGIN_DECLS

typedef struct _EMailTagEditor EMailTagEditor;
typedef struct _EMailTagEditorClass EMailTagEditorClass;
typedef struct _EMailTagEditorPrivate EMailTagEditorPrivate;

struct _EMailTagEditor {
	GtkDialog parent;
	EMailTagEditorPrivate *priv;
};

struct _EMailTagEditorClass {
	GtkDialogClass parent_class;
};

GType		e_mail_tag_editor_get_type	(void);
GtkWidget *	e_mail_tag_editor_new		(void);
gboolean	e_mail_tag_editor_get_completed	(EMailTagEditor *editor);
void		e_mail_tag_editor_set_completed (EMailTagEditor *editor,
						 gboolean completed);
CamelNameValueArray *
		e_mail_tag_editor_get_tag_list	(EMailTagEditor *editor);
void		e_mail_tag_editor_set_tag_list	(EMailTagEditor *editor,
						 const CamelNameValueArray *tag_list);
void		e_mail_tag_editor_add_message	(EMailTagEditor *editor,
						 const gchar *from,
						 const gchar *subject);

G_END_DECLS

#endif /* E_MAIL_TAG_EDITOR_H */
