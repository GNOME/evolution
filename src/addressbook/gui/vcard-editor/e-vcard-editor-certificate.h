/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_VCARD_EDITOR_CERTIFICATE_H
#define E_VCARD_EDITOR_CERTIFICATE_H

#include <gtk/gtk.h>
#include <libebook-contacts/libebook-contacts.h>

G_BEGIN_DECLS

#define E_TYPE_VCARD_EDITOR_CERTIFICATE (e_vcard_editor_certificate_get_type ())
G_DECLARE_FINAL_TYPE (EVCardEditorCertificate, e_vcard_editor_certificate, E, VCARD_EDITOR_CERTIFICATE, GtkGrid)

GtkWidget *	e_vcard_editor_certificate_new	(void);
GtkWidget *	e_vcard_editor_certificate_new_from_chooser
						(GtkWindow *parent_window,
						 EContactField prefer_type,
						 gboolean pgp_supported,
						 GError **error);
void		e_vcard_editor_certificate_set_add_button_visible
						(EVCardEditorCertificate *self,
						 gboolean value);
gboolean	e_vcard_editor_certificate_get_add_button_visible
						(EVCardEditorCertificate *self);
void		e_vcard_editor_certificate_fill_widgets
						(EVCardEditorCertificate *self,
						 EVCardAttribute *attr);
gboolean	e_vcard_editor_certificate_fill_attr
						(EVCardEditorCertificate *self,
						 EVCardVersion to_version,
						 EVCardAttribute **out_attr,
						 gchar **out_error_message,
						 GtkWidget **out_error_widget);

G_END_DECLS

#endif /* E_VCARD_EDITOR_CERTIFICATE_H */
