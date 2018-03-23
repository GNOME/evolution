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

#ifndef __E_CONTACT_EDITOR_FULLNAME_H__
#define __E_CONTACT_EDITOR_FULLNAME_H__

#include <gtk/gtk.h>
#include <libebook/libebook.h>

G_BEGIN_DECLS

/* EContactEditorFullname - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * name         ECardName *     RW              The card currently being edited. Returns a copy.
 */

#define E_TYPE_CONTACT_EDITOR_FULLNAME			(e_contact_editor_fullname_get_type ())
#define E_CONTACT_EDITOR_FULLNAME(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CONTACT_EDITOR_FULLNAME, EContactEditorFullname))
#define E_CONTACT_EDITOR_FULLNAME_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CONTACT_EDITOR_FULLNAME, EContactEditorFullnameClass))
#define E_IS_CONTACT_EDITOR_FULLNAME(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CONTACT_EDITOR_FULLNAME))
#define E_IS_CONTACT_EDITOR_FULLNAME_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_CONTACT_EDITOR_FULLNAME))

typedef struct _EContactEditorFullname       EContactEditorFullname;
typedef struct _EContactEditorFullnameClass  EContactEditorFullnameClass;

struct _EContactEditorFullname
{
	GtkDialog parent;

	/* item specific fields */
	EContactName *name;
	GtkBuilder *builder;

	/* Whether the dialog will accept modifications */
	guint editable : 1;
};

struct _EContactEditorFullnameClass
{
	GtkDialogClass parent_class;
};

GtkWidget *e_contact_editor_fullname_new (GtkWindow *parent,
					  const EContactName *name);
GType      e_contact_editor_fullname_get_type (void);

G_END_DECLS

#endif /* __E_CONTACT_EDITOR_FULLNAME_H__ */
