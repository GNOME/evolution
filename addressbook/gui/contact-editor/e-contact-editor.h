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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_CONTACT_EDITOR_H__
#define __E_CONTACT_EDITOR_H__

#include "addressbook/gui/contact-editor/eab-editor.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* EContactEditor - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * card         ECard *         RW              The card currently being edited
 */

#define E_TYPE_CONTACT_EDITOR			(e_contact_editor_get_type ())
#define E_CONTACT_EDITOR(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CONTACT_EDITOR, EContactEditor))
#define E_CONTACT_EDITOR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CONTACT_EDITOR, EContactEditorClass))
#define E_IS_CONTACT_EDITOR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CONTACT_EDITOR))
#define E_IS_CONTACT_EDITOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_CONTACT_EDITOR))

typedef struct _EContactEditor       EContactEditor;
typedef struct _EContactEditorClass  EContactEditorClass;

struct _EContactEditor
{
	EABEditor object;

	/* item specific fields */
	EBookClient *source_client;
	EBookClient *target_client;
	EContact *contact;

	GtkBuilder *builder;
	GtkWidget *app;

	GtkWidget *file_selector;

	EContactName *name;

	/* Whether we are editing a new contact or an existing one */
	guint is_new_contact : 1;

	/* Whether an image is associated with a contact. */
	guint image_set : 1;

	/* Whether the contact has been changed since bringing up the contact editor */
	guint changed : 1;

	/* Wheter should check for contact to merge. Only when name or email are changed */
	guint check_merge : 1;

	/* Whether the contact editor will accept modifications, save */
	guint target_editable : 1;

	/* Whether an async wombat call is in progress */
	guint in_async_call : 1;

	/* Whether an image is changed */
	guint image_changed : 1;

	/* Whether to try to reduce space used */
	guint compress_ui : 1;

	GSList *writable_fields;

	GSList *required_fields;

	GCancellable *cancellable;

	/* signal ids for "writable_status" */
	gint target_editable_id;

	GtkWidget *fullname_dialog;
	GtkWidget *categories_dialog;
};

struct _EContactEditorClass
{
	EABEditorClass parent_class;
};

GType		e_contact_editor_get_type	(void);
EABEditor	*e_contact_editor_new		(EShell *shell,
						 EBookClient *book_client,
						 EContact *contact,
						 gboolean is_new_contact,
						 gboolean editable);

G_END_DECLS

#endif /* __E_CONTACT_EDITOR_H__ */
