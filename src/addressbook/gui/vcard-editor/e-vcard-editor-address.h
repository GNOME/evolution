/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_VCARD_EDITOR_ADDRESS_H
#define E_VCARD_EDITOR_ADDRESS_H

#include <gtk/gtk.h>
#include <libebook-contacts/libebook-contacts.h>

G_BEGIN_DECLS

#define E_TYPE_VCARD_EDITOR_ADDRESS (e_vcard_editor_address_get_type ())
G_DECLARE_FINAL_TYPE (EVCardEditorAddress, e_vcard_editor_address, E, VCARD_EDITOR_ADDRESS, GtkGrid)

GtkWidget *	e_vcard_editor_address_new	(void);
const gchar *	e_vcard_editor_address_get_address_type
						(EVCardEditorAddress *self);
void		e_vcard_editor_address_fill_widgets
						(EVCardEditorAddress *self,
						 const gchar *type,
						 const EContactAddress *addr); /* nullable */
gboolean	e_vcard_editor_address_fill_addr(EVCardEditorAddress *self,
						 gchar **out_type,
						 EContactAddress **out_addr,
						 gchar **out_error_message,
						 GtkWidget **out_error_widget);

G_END_DECLS

#endif /* E_VCARD_EDITOR_ADDRESS_H */
