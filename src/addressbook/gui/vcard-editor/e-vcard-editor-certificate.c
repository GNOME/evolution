/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <camel/camel.h>
#include <libebook-contacts/libebook-contacts.h>
#include <libedataserverui/libedataserverui.h>

#ifdef ENABLE_SMIME
#include "smime/lib/e-cert.h"
#endif

#include "e-util/e-util.h"

#include "e-vcard-editor-certificate.h"

struct _EVCardEditorCertificate {
	GtkGrid parent_object;

	GtkWidget *expander; /* not owned */
	GtkLabel *expander_label; /* not owned */
	ECertificateWidget *cert_widget; /* not owned */
	GtkTextView *text_view; /* not owned */
	GtkWidget *text_view_scrolled_window; /* not owned */
	GtkWidget *button_box; /* not owned */
	GtkWidget *save_button; /* not owned */
	GtkWidget *add_button; /* not owned */
	GtkWidget *add_placeholder; /* not owned */

	GtkSizeGroup *size_group;

	EVCardAttribute *original_attr;
	gchar *cert_data;
	gsize cert_data_len;
	EContactField field_id; /* X509, PGP, or LAST_FIELD for unset/unknown type */
	gboolean updating;
};

enum {
	CHANGED,
	ADD_CLICKED,
	REMOVE_CLICKED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EVCardEditorCertificate, e_vcard_editor_certificate, GTK_TYPE_GRID)

static void
eve_certificate_emit_changed (EVCardEditorCertificate *self)
{
	if (!self->updating)
		g_signal_emit (self, signals[CHANGED], 0, NULL);
}

static gchar *
eve_certificate_dup_x509_label (EVCardEditorCertificate *self)
{
	gchar *label = NULL;
	#ifdef ENABLE_SMIME
	ECert *ecert;
	#else
	GTlsCertificate *tls_cert;
	#endif

	if (!self->cert_data || !self->cert_data_len)
		return NULL;

	#ifdef ENABLE_SMIME
	ecert = e_cert_new_from_der (self->cert_data, self->cert_data_len);
	if (ecert) {
		const gchar *ident;

		ident = e_cert_get_cn (ecert);
		if (!ident || !*ident)
			ident = e_cert_get_email (ecert);
		if (!ident || !*ident)
			ident = e_cert_get_subject_name (ecert);

		if (ident && *ident) {
			/* Translators: the '%s' is replaced with the certificate info, like its common name, email or subject name */
			label = g_strdup_printf (_("X.509 Certificate (%s)"), ident);
		}

		g_object_unref (ecert);
	}
	#else
	tls_cert = g_tls_certificate_new_from_pem (self->cert_data, self->cert_data_len, NULL);
	if (!tls_cert) {
		gchar *encoded;

		encoded = g_base64_encode ((const guchar *) self->cert_data, self->cert_data_len);
		if (encoded) {
			GString *pem = g_string_sized_new (self->cert_data_len + 60);

			g_string_append (pem, "-----BEGIN CERTIFICATE-----\n");
			g_string_append (pem, encoded);
			g_string_append (pem, "\n-----END CERTIFICATE-----\n");

			tls_cert = g_tls_certificate_new_from_pem (pem->str, pem->len, NULL);

			g_string_free (pem, TRUE);
		}

		g_free (encoded);
	}

	if (tls_cert) {
		gchar *subject;

		subject = g_tls_certificate_get_subject_name (tls_cert);
		if (subject && *subject) {
			label = g_strdup_printf (_("X.509 Certificate (%s)"), subject);
		}

		g_free (subject);
		g_clear_object (&tls_cert);
	}
	#endif

	return label;
}

static gboolean
eve_certificate_fill_widgets_from_data (EVCardEditorCertificate *self,
					gchar *cert_data, /* transfer full */
					gsize cert_data_len)
{
	GtkExpander *expander;
	gboolean has_cert = TRUE;

	g_clear_pointer (&self->original_attr, e_vcard_attribute_free);
	g_free (self->cert_data);
	self->cert_data = cert_data;
	self->cert_data_len = cert_data_len;
	self->field_id = E_CONTACT_FIELD_LAST;

	if (!self->expander)
		return FALSE;

	expander = GTK_EXPANDER (self->expander);

	if (!self->cert_data || !self->cert_data_len) {
		gtk_label_set_label (self->expander_label, _("Certificate not set"));
		gtk_expander_set_expanded (expander, FALSE);
		gtk_widget_set_sensitive (self->expander, FALSE);
	} else {
		gchar *mime_type = NULL;
		gchar *content_type;

		content_type = g_content_type_guess (NULL, (const guchar *) self->cert_data, self->cert_data_len, NULL);
		if (content_type) {
			mime_type = g_content_type_get_mime_type (content_type);
			g_free (content_type);
		}

		if (mime_type) {
			if (g_ascii_strcasecmp (mime_type, "application/pgp-keys") == 0) {
				gtk_label_set_label (self->expander_label, _("PGP key"));
				self->field_id = E_CONTACT_PGP_CERT;
			} else if (camel_strstrcase (mime_type, "x509")) {
				gtk_label_set_label (self->expander_label, _("X.509 certificate"));
				self->field_id = E_CONTACT_X509_CERT;
			} else {
				gtk_label_set_label (self->expander_label, _("Certificate"));
			}

			g_free (mime_type);
		} else {
			gtk_label_set_label (self->expander_label, _("Certificate"));
		}

		gtk_widget_set_visible (GTK_WIDGET (self->cert_widget), self->field_id != E_CONTACT_PGP_CERT);
		gtk_widget_set_visible (self->text_view_scrolled_window, self->field_id == E_CONTACT_PGP_CERT);

		if (self->field_id == E_CONTACT_PGP_CERT) {
			CamelCipherContext *cipher_context;
			GtkTextBuffer *buffer;
			GSList *infos = NULL, *link; /* CamelGpgKeyInfo * */

			buffer = gtk_text_view_get_buffer (self->text_view);

			cipher_context = camel_gpg_context_new (NULL);
			if (camel_gpg_context_get_key_data_info_sync (CAMEL_GPG_CONTEXT (cipher_context),
				(const guint8 *) self->cert_data, self->cert_data_len, 0, &infos, NULL, NULL) && infos) {
				GString *str = g_string_new ("");
				gboolean has_user_id = FALSE;
				const gchar *first_key_id = NULL;

				for (link = infos; link; link = g_slist_next (link)) {
					const CamelGpgKeyInfo *info = link->data;
					GSList *user_ids = camel_gpg_key_info_get_user_ids (info);
					const gchar *value;

					value = camel_gpg_key_info_get_id (info);
					if (!value || !*value)
						continue;

					if (str->len)
						g_string_append (str, "\n\n");
					g_string_append_printf (str, _("Key ID: %s"), value);

					if (!has_user_id) {
						if (!first_key_id)
							first_key_id = camel_gpg_key_info_get_id (info);

						if (user_ids && user_ids->data) {
							gchar *text;

							has_user_id = TRUE;
							value = user_ids->data;

							/* Translators: the '%s' is replaced with a user ID, which is usually a user email, or the key ID */
							text = g_strdup_printf (_("PGP key (%s)"), value);
							gtk_label_set_label (self->expander_label, text);
							g_free (text);
						}
					}

					if (user_ids) {
						GSList *id_link;
						GString *ids;

						ids = g_string_new ("");

						for (id_link = user_ids; id_link; id_link = g_slist_next (id_link)) {
							const gchar *id = id_link->data;

							if (id && *id) {
								if (ids->len > 0)
									g_string_append (ids, ", ");
								g_string_append (ids, id);
							}
						}

						if (ids->len > 0) {
							g_string_append_c (str, '\n');
							g_string_append_printf (str, _("User ID: %s"), ids->str);
						}

						g_string_free (ids, TRUE);
					}

					value = camel_gpg_key_info_get_fingerprint (info);
					if (value && *value) {
						g_string_append_c (str, '\n');
						g_string_append_printf (str, _("Fingerprint: %s"), value);
					}

					if (camel_gpg_key_info_get_creation_date (info) > 0) {
						gchar *fmt;

						fmt = e_datetime_format_format ("mail", "table", DTFormatKindDateTime, (time_t) camel_gpg_key_info_get_creation_date (info));
						if (fmt) {
							g_string_append_c (str, '\n');
							g_string_append_printf (str, _("Created: %s"), fmt);

							g_free (fmt);
						}
					}
				}

				gtk_text_buffer_set_text (buffer, str->str, -1);

				g_string_free (str, TRUE);

				if (!has_user_id && first_key_id) {
					gchar *text;

					if (strlen (first_key_id) > 8) {
						gchar key_id[9];
						strncpy (key_id, first_key_id, 8);
						key_id[8] = '\0';

						text = g_strdup_printf (_("PGP key (%s)"), key_id);
					} else {
						text = g_strdup_printf (_("PGP key (%s)"), first_key_id);
					}

					gtk_label_set_label (self->expander_label, text);
					g_free (text);
				}
			} else {
				gtk_text_buffer_set_text (buffer, _("No information found for the PGP key"), -1);
			}

			g_clear_object (&cipher_context);
		} else {
			e_certificate_widget_set_der (self->cert_widget, self->cert_data, self->cert_data_len);

			if (e_certificate_widget_get_has_data (self->cert_widget)) {
				gchar *label;

				self->field_id = E_CONTACT_X509_CERT;

				label = eve_certificate_dup_x509_label (self);
				if (label) {
					gtk_label_set_label (self->expander_label, label);
					g_free (label);
				}
			} else {
				has_cert = FALSE;

				gtk_label_set_label (self->expander_label, _("Unrecognized certificate"));
				gtk_widget_set_visible (GTK_WIDGET (self->cert_widget), FALSE);
				gtk_widget_set_visible (self->text_view_scrolled_window, FALSE);
			}
		}

		gtk_expander_set_expanded (expander, FALSE);
		gtk_widget_set_sensitive (self->expander, has_cert);
	}

	gtk_widget_set_sensitive (self->save_button, (self->cert_data && self->cert_data_len) || self->original_attr);

	eve_certificate_emit_changed (self);

	return has_cert;
}

static void
e_vcard_editor_certificate_grab_focus (GtkWidget *widget)
{
	EVCardEditorCertificate *self = E_VCARD_EDITOR_CERTIFICATE (widget);

	if (self->expander)
		gtk_widget_grab_focus (self->expander);
}

static void
eve_certificate_add_clicked_cb (GtkWidget *button,
				gpointer user_data)
{
	EVCardEditorCertificate *self = user_data;

	g_signal_emit (self, signals[ADD_CLICKED], 0, NULL);
}

static void
eve_certificate_remove_clicked_cb (GtkWidget *button,
				   gpointer user_data)
{
	EVCardEditorCertificate *self = user_data;

	g_signal_emit (self, signals[REMOVE_CLICKED], 0, NULL);
}

static void
eve_certificate_save_clicked_cb (GtkWidget *button,
				 gpointer user_data)
{
	EVCardEditorCertificate *self = user_data;
	GtkWidget *parent;
	GtkFileChooserNative *native;
	GtkFileChooser *file_chooser;
	GtkFileFilter *filter;

	parent = gtk_widget_get_ancestor (button, GTK_TYPE_WINDOW);

	native = gtk_file_chooser_native_new (
		self->field_id == E_CONTACT_PGP_CERT ? _("Save PGP key") : _("Save X.509 certificate"),
		parent ? GTK_WINDOW (parent) : NULL,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		_("_Save"), _("_Cancel"));

	file_chooser = GTK_FILE_CHOOSER (native);
	gtk_file_chooser_set_local_only (file_chooser, TRUE);
	gtk_file_chooser_set_select_multiple (file_chooser, FALSE);

	if (self->field_id == E_CONTACT_PGP_CERT) {
		filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, _("PGP keys"));
		gtk_file_filter_add_mime_type (filter, "application/pgp-keys");
		gtk_file_chooser_add_filter (file_chooser, filter);
	} else {
		filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, _("X.509 certificates"));
		gtk_file_filter_add_mime_type (filter, "application/x-x509-user-cert");
		gtk_file_chooser_add_filter (file_chooser, filter);
	}

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (file_chooser, filter);

	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (native)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename;
		GError *local_error = NULL;

		filename = gtk_file_chooser_get_filename (file_chooser);
		if (!filename) {
			g_set_error_literal (&local_error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, _("Chosen file is not a local file."));
		} else if (self->cert_data) {
			g_file_set_contents (filename, self->cert_data, self->cert_data_len, &local_error);
		} else if (self->original_attr) {
			GList *values, *link;
			GByteArray *bytes;

			bytes = g_byte_array_new ();

			values = e_vcard_attribute_get_values_decoded (self->original_attr);
			for (link = values; link; link = g_list_next (link)) {
				GString *str = link->data;

				if (str)
					g_byte_array_append (bytes, (const guint8 *) str->str, str->len);
			}

			g_file_set_contents (filename, (const gchar *) bytes->data, bytes->len, &local_error);

			g_byte_array_free (bytes, TRUE);
		}

		g_free (filename);

		if (local_error) {
			EMessagePopover *message_popover;

			gtk_widget_grab_focus (button);

			message_popover = e_message_popover_new (button, E_MESSAGE_POPOVER_FLAG_ERROR |
				E_MESSAGE_POPOVER_FLAG_HIGHLIGHT_POPOVER);

			e_message_popover_set_text (message_popover, _("Failed to save certificate: %s"), local_error->message);

			e_message_popover_show (message_popover);

			g_clear_error (&local_error);
		}
	}

	g_object_unref (native);
}

static void
eve_certificate_notify_expanded_cb (EVCardEditorCertificate *self)
{
	if (self->expander && self->button_box) {
		gboolean expanded;

		expanded = gtk_expander_get_expanded (GTK_EXPANDER (self->expander));
		/* to not step in when not expanded with Tab */
		gtk_widget_set_sensitive (gtk_bin_get_child (GTK_BIN (self->expander)), expanded);

		if (expanded) {
			gtk_orientable_set_orientation (GTK_ORIENTABLE (self->button_box), GTK_ORIENTATION_VERTICAL);
			gtk_style_context_remove_class (gtk_widget_get_style_context (self->button_box), "vertical");
			gtk_widget_set_valign (self->button_box, GTK_ALIGN_END);
			gtk_widget_set_visible (self->add_placeholder, FALSE);
		} else {
			gtk_orientable_set_orientation (GTK_ORIENTABLE (self->button_box), GTK_ORIENTATION_HORIZONTAL);
			gtk_style_context_remove_class (gtk_widget_get_style_context (self->button_box), "horizontal");
			gtk_widget_set_valign (self->button_box, GTK_ALIGN_CENTER);
			gtk_widget_set_visible (self->add_placeholder, !gtk_widget_get_visible (self->add_button));
		}
	}
}

static void
e_vcard_editor_certificate_constructed (GObject *object)
{
	EVCardEditorCertificate *self = E_VCARD_EDITOR_CERTIFICATE (object);
	GtkWidget *widget;
	GtkBox *box;
	GtkGrid *grid;

	G_OBJECT_CLASS (e_vcard_editor_certificate_parent_class)->constructed (object);

	g_object_set (self,
		"halign", GTK_ALIGN_FILL,
		"hexpand", FALSE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"column-spacing", 4,
		"row-spacing", 4,
		NULL);

	grid = GTK_GRID (self);

	widget = gtk_expander_new (NULL);
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_CENTER,
		"vexpand", TRUE,
		"expanded", FALSE,
		"margin-top", 2,
		NULL);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	self->expander = widget;

	widget = gtk_label_new ("");
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_CENTER,
		"margin-start", 4,
		"visible", TRUE,
		"ellipsize", PANGO_ELLIPSIZE_END,
		NULL);
	gtk_expander_set_label_widget (GTK_EXPANDER (self->expander), widget);

	self->expander_label = GTK_LABEL (widget);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	g_object_set (widget,
		"sensitive", FALSE,
		NULL);
	gtk_container_add (GTK_CONTAINER (self->expander), widget);

	box = GTK_BOX (widget);

	widget = gtk_text_view_new ();
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		"accepts-tab", FALSE,
		"editable", FALSE,
		"wrap-mode", GTK_WRAP_WORD_CHAR,
		"left-margin", 6,
		"right-margin", 6,
		"top-margin", 6,
		"bottom-margin", 6,
		NULL);

	self->text_view = GTK_TEXT_VIEW (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", FALSE,
		"no-show-all", TRUE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"shadow-type", GTK_SHADOW_IN,
		"kinetic-scrolling", TRUE,
		"min-content-width", 200,
		"min-content-height", 30,
		"propagate-natural-height", TRUE,
		"propagate-natural-width", TRUE,
		NULL);

	gtk_box_pack_start (box, widget, TRUE, TRUE, 0);

	self->text_view_scrolled_window = widget;

	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (self->text_view));

	widget = e_certificate_widget_new ();
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"no-show-all", TRUE,
		"visible", TRUE,
		"shadow-type", GTK_SHADOW_IN,
		"min-content-width", 200,
		"min-content-height", 150,
		NULL);

	gtk_box_pack_start (box, widget, TRUE, TRUE, 0);

	self->cert_widget = E_CERTIFICATE_WIDGET (widget);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_grid_attach (grid, widget, 1, 0, 1, 1);

	self->button_box = widget;

	box = GTK_BOX (widget);

	widget = gtk_button_new_from_icon_name ("user-trash", GTK_ICON_SIZE_BUTTON);
	gtk_style_context_add_class (gtk_widget_get_style_context (widget), "flat");
	g_object_set (widget,
		"tooltip-text", _("Remove"),
		"always-show-image", TRUE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	g_signal_connect (widget, "clicked",
		G_CALLBACK (eve_certificate_remove_clicked_cb), self);

	self->size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);
	gtk_size_group_add_widget (self->size_group, widget);

	widget = gtk_button_new_from_icon_name ("document-save", GTK_ICON_SIZE_BUTTON);
	gtk_style_context_add_class (gtk_widget_get_style_context (widget), "flat");
	g_object_set (widget,
		"tooltip-text", _("Save to File"),
		"always-show-image", TRUE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	g_signal_connect (widget, "clicked",
		G_CALLBACK (eve_certificate_save_clicked_cb), self);

	self->save_button = widget;

	widget = gtk_button_new_from_icon_name ("list-add", GTK_ICON_SIZE_BUTTON);
	gtk_style_context_add_class (gtk_widget_get_style_context (widget), "flat");
	g_object_set (widget,
		"visible", FALSE,
		"tooltip-text", _("Add"),
		"no-show-all", TRUE,
		"always-show-image", TRUE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	self->add_button = widget;

	g_signal_connect (widget, "clicked",
		G_CALLBACK (eve_certificate_add_clicked_cb), self);

	widget = gtk_label_new ("");
	g_object_set (widget,
		"visible", TRUE,
		"no-show-all", TRUE,
		NULL);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	gtk_size_group_add_widget (self->size_group, widget);

	self->add_placeholder = widget;

	gtk_widget_show_all (GTK_WIDGET (self));

	g_signal_connect_object (self->expander, "notify::expanded",
		G_CALLBACK (eve_certificate_notify_expanded_cb), self, G_CONNECT_SWAPPED);
}

static void
e_vcard_editor_certificate_dispose (GObject *object)
{
	EVCardEditorCertificate *self = E_VCARD_EDITOR_CERTIFICATE (object);

	self->expander = NULL;
	self->expander_label = NULL;
	self->cert_widget = NULL;
	self->text_view = NULL;
	self->text_view_scrolled_window = NULL;
	self->button_box = NULL;
	self->save_button = NULL;
	self->add_button = NULL;
	self->add_placeholder = NULL;

	g_clear_object (&self->size_group);
	g_clear_pointer (&self->original_attr, e_vcard_attribute_free);
	g_clear_pointer (&self->cert_data, g_free);
	self->cert_data_len = 0;

	G_OBJECT_CLASS (e_vcard_editor_certificate_parent_class)->dispose (object);
}

static void
e_vcard_editor_certificate_class_init (EVCardEditorCertificateClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_vcard_editor_certificate_constructed;
	object_class->dispose = e_vcard_editor_certificate_dispose;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->grab_focus = e_vcard_editor_certificate_grab_focus;

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[ADD_CLICKED] = g_signal_new (
		"add-clicked",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[REMOVE_CLICKED] = g_signal_new (
		"remove-clicked",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_vcard_editor_certificate_init (EVCardEditorCertificate *self)
{
}

GtkWidget *
e_vcard_editor_certificate_new (void)
{
	return g_object_new (E_TYPE_VCARD_EDITOR_CERTIFICATE, NULL);
}

GtkWidget *
e_vcard_editor_certificate_new_from_chooser (GtkWindow *parent_window,
					     EContactField prefer_type,
					     gboolean pgp_supported,
					     GError **error)
{
	GtkWidget *cert_widget = NULL;
	GtkFileChooserNative *native;
	GtkFileChooser *file_chooser;
	GtkFileFilter *filter;

	native = gtk_file_chooser_native_new (
		prefer_type == E_CONTACT_PGP_CERT ? _("Open PGP key") : _("Open X.509 certificate"),
		parent_window,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Open"), _("_Cancel"));

	file_chooser = GTK_FILE_CHOOSER (native);
	gtk_file_chooser_set_local_only (file_chooser, TRUE);
	gtk_file_chooser_set_select_multiple (file_chooser, FALSE);

	/* offer both file types, when the type is not preferred */
	if (prefer_type == E_CONTACT_X509_CERT || prefer_type != E_CONTACT_PGP_CERT) {
		filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, _("X.509 certificates"));
		gtk_file_filter_add_mime_type (filter, "application/x-x509-user-cert");
		gtk_file_chooser_add_filter (file_chooser, filter);
	}

	if (prefer_type == E_CONTACT_PGP_CERT || (pgp_supported && prefer_type != E_CONTACT_X509_CERT)) {
		filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, _("PGP keys"));
		gtk_file_filter_add_mime_type (filter, "application/pgp-keys");
		gtk_file_chooser_add_filter (file_chooser, filter);
	}

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (file_chooser, filter);

	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (native)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename;
		gchar *content = NULL;
		gsize length = 0;

		filename = gtk_file_chooser_get_filename (file_chooser);
		if (!filename) {
			g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, _("Chosen file is not a local file."));
		} else if (g_file_get_contents (filename, &content, &length, error) && length > 0) {
			cert_widget = e_vcard_editor_certificate_new ();
			eve_certificate_fill_widgets_from_data (E_VCARD_EDITOR_CERTIFICATE (cert_widget), g_steal_pointer (&content), length);
		} else if (error && *error) {
			g_prefix_error (error, "%s", _("Failed to load certificate:"));
		}

		g_free (content);
		g_free (filename);
	}

	g_object_unref (native);

	return cert_widget;
}

void
e_vcard_editor_certificate_set_add_button_visible (EVCardEditorCertificate *self,
						   gboolean value)
{
	g_return_if_fail (E_IS_VCARD_EDITOR_CERTIFICATE (self));

	if (self->add_button) {
		gtk_widget_set_visible (self->add_button, value);
		gtk_widget_set_visible (self->add_placeholder, !value && !gtk_expander_get_expanded (GTK_EXPANDER (self->expander)));
	}
}

gboolean
e_vcard_editor_certificate_get_add_button_visible (EVCardEditorCertificate *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR_CERTIFICATE (self), FALSE);

	return self->add_button && gtk_widget_get_visible (self->add_button);
}

void
e_vcard_editor_certificate_fill_widgets (EVCardEditorCertificate *self,
					 EVCardAttribute *attr)
{
	EContactCert *cert;

	g_return_if_fail (E_IS_VCARD_EDITOR_CERTIFICATE (self));

	g_clear_pointer (&self->original_attr, e_vcard_attribute_free);
	g_clear_pointer (&self->cert_data, g_free);
	self->cert_data_len = 0;

	cert = e_contact_cert_from_attr (attr);

	if (cert && eve_certificate_fill_widgets_from_data (self, g_steal_pointer (&cert->data), cert->length)) {
		e_contact_cert_free (cert);
	} else {
		self->original_attr = e_vcard_attribute_copy (attr);

		if (self->expander) {
			gtk_label_set_label (self->expander_label, _("Unrecognized certificate"));
			gtk_expander_set_expanded (GTK_EXPANDER (self->expander), FALSE);
			gtk_widget_set_sensitive (self->expander, FALSE);
			gtk_widget_set_visible (GTK_WIDGET (self->cert_widget), FALSE);
			gtk_widget_set_visible (self->text_view_scrolled_window, FALSE);
			gtk_widget_set_sensitive (self->save_button, TRUE);
		}
	}
}

gboolean
e_vcard_editor_certificate_fill_attr (EVCardEditorCertificate *self,
				      EVCardVersion to_version,
				      EVCardAttribute **out_attr,
				      gchar **out_error_message,
				      GtkWidget **out_error_widget)
{
	EContactCert cert;
	EVCardAttribute *attr;

	g_return_val_if_fail (E_IS_VCARD_EDITOR_CERTIFICATE (self), FALSE);
	g_return_val_if_fail (out_attr != NULL, FALSE);

	if (self->original_attr) {
		*out_attr = e_vcard_attribute_copy (self->original_attr);

		return TRUE;
	}

	if (!self->cert_data || !self->cert_data_len)
		return TRUE;

	memset (&cert, 0, sizeof (EContactCert));

	cert.data = self->cert_data;
	cert.length = self->cert_data_len;

	attr = e_vcard_attribute_new (NULL, EVC_KEY);

	e_contact_cert_write_attr (&cert, to_version, attr);

	*out_attr = attr;

	return TRUE;
}
