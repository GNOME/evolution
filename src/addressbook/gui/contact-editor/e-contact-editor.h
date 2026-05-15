/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
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
typedef struct _EContactEditorPrivate EContactEditorPrivate;

struct _EContactEditor
{
	EABEditor object;
	EContactEditorPrivate *priv;
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

GObject *	e_contact_editor_util_show_for_contact
						(GtkWindow *parent,
						 EShell *shell,
						 EBookClient *book_client,
						 EContact *contact,
						 gboolean is_new_contact,
						 gboolean editable);

G_END_DECLS

#endif /* __E_CONTACT_EDITOR_H__ */
