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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
#include "certificate-manager.h"
#include "e-cert-db.h"
#endif

#include "e-mail-part-secure-button.h"

G_DEFINE_TYPE (EMailPartSecureButton, e_mail_part_secure_button, E_TYPE_MAIL_PART)

const gchar *e_mail_formatter_secure_button_get_encrypt_description (camel_cipher_validity_encrypt_t status);
const gchar *e_mail_formatter_secure_button_get_sign_description (camel_cipher_validity_sign_t status);


#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
static void
viewcert_clicked (GtkWidget *button,
                  GtkWidget *grid)
{
	CamelCipherCertInfo *info = g_object_get_data ((GObject *) button, "e-cert-info");
	ECert *ec = NULL;

	if (info->cert_data)
		ec = e_cert_new (CERT_DupCertificate (info->cert_data));

	if (ec != NULL) {
		GtkWidget *dialog, *parent;

		parent = gtk_widget_get_toplevel (grid);
		if (!parent || !GTK_IS_WINDOW (parent))
			parent = NULL;

		dialog = e_cert_manager_new_certificate_viewer ((GtkWindow *) parent, ec);

		gtk_widget_show (dialog);
		g_signal_connect (
			dialog, "response",
			G_CALLBACK (gtk_widget_destroy), NULL);

		g_object_unref (ec);
	} else {
		g_warning (
			"can't find certificate for %s <%s>",
			info->name ? info->name : "",
			info->email ? info->email : "");
	}
}
#endif

static void
info_response (GtkWidget *widget,
               guint button,
               gpointer user_data)
{
	gtk_widget_destroy (widget);
}

static void
add_cert_table (GtkWidget *grid,
                GQueue *certlist,
                gpointer user_data)
{
	GList *head, *link;
	GtkTable *table;
	gint n = 0;

	table = (GtkTable *) gtk_table_new (certlist->length, 2, FALSE);

	head = g_queue_peek_head_link (certlist);

	for (link = head; link != NULL; link = g_list_next (link)) {
		CamelCipherCertInfo *info = link->data;
		gchar *la = NULL;
		const gchar *l = NULL;

		if (info->name) {
			if (info->email && strcmp (info->name, info->email) != 0)
				l = la = g_strdup_printf ("%s <%s>", info->name, info->email);
			else
				l = info->name;
		} else {
			if (info->email)
				l = info->email;
		}

		if (l) {
			GtkWidget *w;
#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
			ECert *ec = NULL;
#endif
			w = gtk_label_new (l);
			gtk_misc_set_alignment ((GtkMisc *) w, 0.0, 0.5);
			g_free (la);
			gtk_table_attach (table, w, 0, 1, n, n + 1, GTK_FILL, GTK_FILL, 3, 3);
#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
			w = gtk_button_new_with_mnemonic (_("_View Certificate"));
			gtk_table_attach (table, w, 1, 2, n, n + 1, 0, 0, 3, 3);
			g_object_set_data ((GObject *) w, "e-cert-info", info);
			g_signal_connect (
				w, "clicked",
				G_CALLBACK (viewcert_clicked), grid);

			if (info->cert_data)
				ec = e_cert_new (CERT_DupCertificate (info->cert_data));

			if (ec == NULL)
				gtk_widget_set_sensitive (w, FALSE);
			else
				g_object_unref (ec);
#else
			w = gtk_label_new (_("This certificate is not viewable"));
			gtk_table_attach (table, w, 1, 2, n, n + 1, 0, 0, 3, 3);
#endif
			n++;
		}
	}

	gtk_container_add (GTK_CONTAINER (grid), GTK_WIDGET (table));
}

static void
secure_button_show_validity_dialog (EWebView *web_view,
				    CamelCipherValidity *validity)
{
	GtkBuilder *builder;
	GtkWidget *grid, *w;
	GtkWidget *dialog;

	g_return_if_fail (validity != NULL);

	/* Make sure our custom widget classes are registered with
	 * GType before we load the GtkBuilder definition file. */
	g_type_ensure (E_TYPE_DATE_EDIT);

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "mail-dialogs.ui");

	dialog = e_builder_get_widget (builder, "message_security_dialog");

	w = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
	if (GTK_IS_WINDOW (w))
		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (w));

	grid = e_builder_get_widget (builder, "signature_grid");
	w = gtk_label_new (e_mail_formatter_secure_button_get_sign_description (validity->sign.status));
	gtk_misc_set_alignment ((GtkMisc *) w, 0.0, 0.5);
	gtk_label_set_line_wrap ((GtkLabel *) w, TRUE);
	gtk_container_add (GTK_CONTAINER (grid), w);
	if (validity->sign.description) {
		GtkTextBuffer *buffer;

		buffer = gtk_text_buffer_new (NULL);
		gtk_text_buffer_set_text (
			buffer, validity->sign.description,
			strlen (validity->sign.description));
		w = g_object_new (
			gtk_scrolled_window_get_type (),
			"hscrollbar_policy", GTK_POLICY_AUTOMATIC,
			"vscrollbar_policy", GTK_POLICY_AUTOMATIC,
			"shadow_type", GTK_SHADOW_IN,
			"expand", TRUE,
			"child", g_object_new (gtk_text_view_get_type (),
			"buffer", buffer,
			"cursor_visible", FALSE,
			"editable", FALSE,
			NULL),
			"width_request", 500,
			"height_request", 80,
			NULL);
		g_object_unref (buffer);

		gtk_container_add (GTK_CONTAINER (grid), w);
	}

	if (!g_queue_is_empty (&validity->sign.signers))
		add_cert_table (grid, &validity->sign.signers, NULL);

	gtk_widget_show_all (grid);

	grid = e_builder_get_widget (builder, "encryption_grid");
	w = gtk_label_new (e_mail_formatter_secure_button_get_encrypt_description (validity->encrypt.status));
	gtk_misc_set_alignment ((GtkMisc *) w, 0.0, 0.5);
	gtk_label_set_line_wrap ((GtkLabel *) w, TRUE);
	gtk_container_add (GTK_CONTAINER (grid), w);
	if (validity->encrypt.description) {
		GtkTextBuffer *buffer;

		buffer = gtk_text_buffer_new (NULL);
		gtk_text_buffer_set_text (
			buffer, validity->encrypt.description,
			strlen (validity->encrypt.description));
		w = g_object_new (
			gtk_scrolled_window_get_type (),
			"hscrollbar_policy", GTK_POLICY_AUTOMATIC,
			"vscrollbar_policy", GTK_POLICY_AUTOMATIC,
			"shadow_type", GTK_SHADOW_IN,
			"expand", TRUE,
			"child", g_object_new (gtk_text_view_get_type (),
			"buffer", buffer,
			"cursor_visible", FALSE,
			"editable", FALSE,
			NULL),
			"width_request", 500,
			"height_request", 80,
			NULL);
		g_object_unref (buffer);

		gtk_container_add (GTK_CONTAINER (grid), w);
	}

	if (!g_queue_is_empty (&validity->encrypt.encrypters))
		add_cert_table (grid, &validity->encrypt.encrypters, NULL);

	gtk_widget_show_all (grid);

	g_object_unref (builder);

	g_signal_connect (
		dialog, "response",
		G_CALLBACK (info_response), NULL);

	gtk_widget_show (dialog);
}

static void
secure_button_clicked_cb (EWebView *web_view,
			  const gchar *element_class,
			  const gchar *element_value,
			  const GtkAllocation *element_position,
			  gpointer user_data)
{
	EMailPart *mail_part = user_data;
	GList *head, *link;
	gboolean can_use;
	gchar *tmp;

	g_return_if_fail (E_IS_MAIL_PART_SECURE_BUTTON (mail_part));

	tmp = g_strdup_printf ("%p:", mail_part);
	can_use = element_value && g_str_has_prefix (element_value, tmp);
	if (can_use)
		element_value += strlen (tmp);
	g_free (tmp);

	if (!can_use)
		return;

	head = g_queue_peek_head_link (&mail_part->validities);
	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPartValidityPair *pair = link->data;

		if (!pair)
			continue;

		tmp = g_strdup_printf ("%p", pair->validity);
		can_use = g_strcmp0 (element_value, tmp) == 0;
		g_free (tmp);

		if (can_use) {
			secure_button_show_validity_dialog (web_view, pair->validity);
			break;
		}
	}
}

static void
mail_part_secure_button_web_view_loaded (EMailPart *mail_part,
					 EWebView *web_view)
{
	g_return_if_fail (E_IS_MAIL_PART_SECURE_BUTTON (mail_part));
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	e_web_view_register_element_clicked (web_view, "secure-button", secure_button_clicked_cb, mail_part);
}

static void
e_mail_part_secure_button_class_init (EMailPartSecureButtonClass *class)
{
	EMailPartClass *mail_part_class;

	mail_part_class = E_MAIL_PART_CLASS (class);
	mail_part_class->web_view_loaded = mail_part_secure_button_web_view_loaded;
}

static void
e_mail_part_secure_button_init (EMailPartSecureButton *part)
{
}

EMailPart *
e_mail_part_secure_button_new (CamelMimePart *mime_part,
                       const gchar *id)
{
	g_return_val_if_fail (id != NULL, NULL);

	return g_object_new (
		E_TYPE_MAIL_PART_SECURE_BUTTON,
		"id", id, "mime-part", mime_part, NULL);
}
