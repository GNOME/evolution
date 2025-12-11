/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_VCARD_EDITOR_H
#define E_VCARD_EDITOR_H

#include <gtk/gtk.h>
#include <libebook/libebook.h>
#include <e-util/e-util.h>
#include <shell/e-shell.h>

G_BEGIN_DECLS

typedef enum _EVCardEditorFlags {
	E_VCARD_EDITOR_FLAG_NONE	= 1 << 0,
	E_VCARD_EDITOR_FLAG_IS_NEW	= 1 << 1
} EVCardEditorFlags;

typedef enum _EVCardEditorContactKind {
	E_VCARD_EDITOR_CONTACT_KIND_UNKNOWN	= 0,
	E_VCARD_EDITOR_CONTACT_KIND_INDIVIDUAL	= 1 << 0,
	E_VCARD_EDITOR_CONTACT_KIND_GROUP	= 1 << 1,
	E_VCARD_EDITOR_CONTACT_KIND_ORG		= 1 << 2,
	E_VCARD_EDITOR_CONTACT_KIND_LOCATION	= 1 << 3,
	E_VCARD_EDITOR_CONTACT_KIND_CUSTOM	= 1 << 4
} EVCardEditorContactKind;

#define E_TYPE_VCARD_EDITOR (e_vcard_editor_get_type ())
G_DECLARE_FINAL_TYPE (EVCardEditor, e_vcard_editor, E, VCARD_EDITOR, GtkDialog)

EVCardEditor *	e_vcard_editor_new		(GtkWindow *parent,
						 EShell *shell,
						 EBookClient *source_client,
						 EContact *contact,
						 EVCardEditorFlags flags);
ESourceRegistry *
		e_vcard_editor_get_registry	(EVCardEditor *self);
EClientCache *	e_vcard_editor_get_client_cache	(EVCardEditor *self);
EBookClient *	e_vcard_editor_get_source_client(EVCardEditor *self);
EBookClient *	e_vcard_editor_get_target_client(EVCardEditor *self);
void		e_vcard_editor_set_target_client(EVCardEditor *self,
						 EBookClient *client);
EVCardEditorFlags
		e_vcard_editor_get_flags	(EVCardEditor *self);
void		e_vcard_editor_set_flags	(EVCardEditor *self,
						 EVCardEditorFlags flags);
EVCardEditorContactKind
		e_vcard_editor_get_contact_kind	(EVCardEditor *self);
EContact *	e_vcard_editor_get_contact	(EVCardEditor *self);
void		e_vcard_editor_set_contact	(EVCardEditor *self,
						 EContact *contact);
gboolean	e_vcard_editor_get_changed	(EVCardEditor *self);
void		e_vcard_editor_set_changed	(EVCardEditor *self,
						 gboolean value);
gboolean	e_vcard_editor_validate		(EVCardEditor *self,
						 gchar **out_error_message,
						 GtkWidget **out_error_widget);

G_END_DECLS

#endif /* E_VCARD_EDITOR_H */
