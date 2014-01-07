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
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_CONTACT_LIST_EDITOR_H__
#define __E_CONTACT_LIST_EDITOR_H__

#include "addressbook/gui/contact-editor/eab-editor.h"

#define E_TYPE_CONTACT_LIST_EDITOR \
	(e_contact_list_editor_get_type ())
#define E_CONTACT_LIST_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONTACT_LIST_EDITOR, EContactListEditor))
#define E_CONTACT_LIST_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONTACT_LIST_EDITOR, EContactListEditorClass))
#define E_IS_CONTACT_LIST_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONTACT_LIST_EDITOR))
#define E_IS_CONTACT_LIST_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_CONTACT_LIST_EDITOR))
#define E_CONTACT_LIST_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CONTACT_LIST_EDITOR, EContactListEditorClass))

G_BEGIN_DECLS

typedef struct _EContactListEditor EContactListEditor;
typedef struct _EContactListEditorClass EContactListEditorClass;
typedef struct _EContactListEditorPrivate EContactListEditorPrivate;

struct _EContactListEditor
{
	EABEditor parent;
	EContactListEditorPrivate *priv;
};

struct _EContactListEditorClass
{
	EABEditorClass parent_class;
};

GType		e_contact_list_editor_get_type	(void);
EABEditor *	e_contact_list_editor_new	(EShell *shell,
						 EBookClient *book_client,
						 EContact *list_contact,
						 gboolean is_new_list,
						 gboolean editable);
EBookClient *	e_contact_list_editor_get_client
						(EContactListEditor *editor);
void		e_contact_list_editor_set_client
						(EContactListEditor *editor,
						 EBookClient *book_client);
EContact *	e_contact_list_editor_get_contact
						(EContactListEditor *editor);
void		e_contact_list_editor_set_contact
						(EContactListEditor *editor,
						 EContact *contact);
gboolean	e_contact_list_editor_get_is_new_list
						(EContactListEditor *editor);
void		e_contact_list_editor_set_is_new_list
						(EContactListEditor *editor,
						 gboolean is_new_list);
gboolean	e_contact_list_editor_get_editable
						(EContactListEditor *editor);
void		e_contact_list_editor_set_editable
						(EContactListEditor *editor,
						 gboolean editable);
gboolean	e_contact_list_editor_request_close_all (void);

G_END_DECLS

#endif /* __E_CONTACT_LIST_EDITOR_H__ */
