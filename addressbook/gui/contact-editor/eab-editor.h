/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* eab-editor.h
 * Copyright (C) 2004  Novell, Inc.
 * Author: Chris Toshok <toshok@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __EAB_EDITOR_H__
#define __EAB_EDITOR_H__

#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-app-helper.h>
#include <bonobo/bonobo-ui-component.h>
#include <glade/glade.h>

#include <libebook/e-book.h>
#include <libebook/e-contact.h>

#include <gtk/gtktreeview.h>
#include <gtk/gtkliststore.h>

G_BEGIN_DECLS

/* EABEditor - A dialog displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * card         ECard *         RW              The card currently being edited
 */

#define EAB_TYPE_EDITOR			(eab_editor_get_type ())
#define EAB_EDITOR(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), EAB_TYPE_EDITOR, EABEditor))
#define EAB_EDITOR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), EAB_TYPE_EDITOR, EABEditorClass))
#define EAB_IS_EDITOR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EAB_TYPE_EDITOR))
#define EAB_IS_EDITOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EAB_TYPE_EDITOR))
#define EAB_EDITOR_GET_CLASS(o)         (G_TYPE_INSTANCE_GET_CLASS ((o), EAB_EDITOR_TYPE, EABEditorClass))


typedef struct _EABEditor       EABEditor;
typedef struct _EABEditorClass  EABEditorClass;

struct _EABEditor
{
	GObject parent;
};

struct _EABEditorClass
{
	GObjectClass parent_class;

	/* virtual functions */
	void (* show)             (EABEditor *editor);
	void (* close) 	          (EABEditor *editor);
	void (* raise) 	          (EABEditor *editor);
	void (* save_contact)     (EABEditor *editor, gboolean should_close);
	gboolean (* is_valid)     (EABEditor *editor);
	gboolean (* is_changed)   (EABEditor *editor);
	GtkWindow* (* get_window) (EABEditor *editor);

	/* signals */
	void (* contact_added)    (EABEditor *editor, EBookStatus status, EContact *contact);
	void (* contact_modified) (EABEditor *editor, EBookStatus status, EContact *contact);
	void (* contact_deleted)  (EABEditor *editor, EBookStatus status, EContact *contact);
	void (* editor_closed)    (EABEditor *editor);
};

GType           eab_editor_get_type          (void);

/* virtual functions */
void            eab_editor_show              (EABEditor *editor);
void            eab_editor_close             (EABEditor *editor);
void            eab_editor_raise             (EABEditor *editor);
void            eab_editor_save_contact      (EABEditor *editor, gboolean should_close);
gboolean        eab_editor_is_valid          (EABEditor *editor);
gboolean        eab_editor_is_changed        (EABEditor *editor);
GtkWindow*      eab_editor_get_window        (EABEditor *editor);

gboolean        eab_editor_prompt_to_save_changes (EABEditor *editor, GtkWindow *window);
gboolean        eab_editor_confirm_delete    (GtkWindow *parent);

/* these four generate EABEditor signals */
void 		eab_editor_contact_added     (EABEditor *editor, EBookStatus status, EContact *contact);
void 		eab_editor_contact_modified  (EABEditor *editor, EBookStatus status, EContact *contact);
void 		eab_editor_contact_deleted   (EABEditor *editor, EBookStatus status, EContact *contact);
void 		eab_editor_closed            (EABEditor *editor);

/* these maintain the global list of editors so we can prompt the user
   if there are unsaved changes when they exit. */
void            eab_editor_add               (EABEditor *editor);
void            eab_editor_remove            (EABEditor *editor);
gboolean        eab_editor_request_close_all (void);
const GSList*   eab_editor_get_all_editors   (void);

G_END_DECLS

#endif /* __E_CONTACT_EDITOR_H__ */
