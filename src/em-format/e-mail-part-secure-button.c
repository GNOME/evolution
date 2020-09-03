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

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#if defined (ENABLE_SMIME)
#include "certificate-manager.h"
#include "e-cert-db.h"
#endif

#include "e-mail-part-secure-button.h"

G_DEFINE_TYPE (EMailPartSecureButton, e_mail_part_secure_button, E_TYPE_MAIL_PART)

#if defined (ENABLE_SMIME)

static void
secure_button_view_certificate (GtkWindow *parent,
				CamelCipherCertInfo *info)
{
	ECert *ec = NULL;

	g_return_if_fail (info != NULL);

	if (info->cert_data)
		ec = e_cert_new (CERT_DupCertificate (info->cert_data));

	if (ec != NULL) {
		GtkWidget *dialog;

		dialog = e_cert_manager_new_certificate_viewer (parent, ec);

		g_signal_connect (
			dialog, "response",
			G_CALLBACK (gtk_widget_destroy), NULL);

		gtk_widget_show (dialog);

		g_object_unref (ec);
	} else {
		g_warning ("%s: Can't find certificate for %s <%s>", G_STRFUNC,
			info->name ? info->name : "",
			info->email ? info->email : "");
	}
}

static gboolean
secure_button_get_raw_der (CERTCertificate *cert,
			   gchar **data,
			   guint32 *len)
{
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (len != NULL, FALSE);

	if (!cert || !cert->derCert.data || !cert->derCert.len)
		return FALSE;

	*data = (gchar *) cert->derCert.data;
	*len = cert->derCert.len;

	return TRUE;
}

static void
secure_button_import_certificate (GtkWindow *parent,
				  CamelCipherCertInfo *info,
				  EWebView *web_view,
				  const gchar *iframe_id,
				  const gchar *element_id)
{
	guint32 len = 0;
	gchar *data = NULL;
	GError *error = NULL;

	g_return_if_fail (info != NULL);

	g_warn_if_fail (secure_button_get_raw_der (info->cert_data, &data, &len));

	if (!e_cert_db_import_email_cert (e_cert_db_peek (), data, len, NULL, &error)) {
		e_notice (parent, GTK_MESSAGE_ERROR, _("Failed to import certificate: %s"),
			error ? error->message : _("Unknown error"));

		g_clear_error (&error);
	} else {
		e_web_view_jsc_set_element_disabled (WEBKIT_WEB_VIEW (web_view),
			iframe_id, element_id, TRUE,
			e_web_view_get_cancellable (web_view));
	}
}

static CamelCipherCertInfo *
secure_button_find_cert_info (EMailPart *part,
			      const gchar *element_value)
{
	CamelCipherCertInfo *info = NULL;
	GList *vlink;
	gchar tmp[128];

	if (!element_value)
		return NULL;

	/* element_value = part : validity : cert_data */
	g_return_val_if_fail (g_snprintf (tmp, sizeof (tmp), "%p:", part) < sizeof (tmp), NULL);

	if (!g_str_has_prefix (element_value, tmp))
		return NULL;

	element_value += strlen (tmp);

	for (vlink = g_queue_peek_head_link (&part->validities); vlink; vlink = g_list_next (vlink)) {
		EMailPartValidityPair *pair = vlink->data;

		if (pair) {
			g_return_val_if_fail (g_snprintf (tmp, sizeof (tmp), "%p:", pair->validity) < sizeof (tmp), NULL);

			if (g_str_has_prefix (element_value, tmp)) {
				GList *ilink;

				element_value += strlen (tmp);

				for (ilink = g_queue_peek_head_link (&pair->validity->sign.signers); ilink && !info; ilink = g_list_next (ilink)) {
					CamelCipherCertInfo *adept = ilink->data;

					if (adept && adept->cert_data) {
						g_return_val_if_fail (g_snprintf (tmp, sizeof (tmp), "%p", adept->cert_data) < sizeof (tmp), NULL);

						if (g_strcmp0 (element_value, tmp) == 0) {
							info = adept;
							break;
						}
					}
				}

				for (ilink = g_queue_peek_head_link (&pair->validity->encrypt.encrypters); ilink && !info; ilink = g_list_next (ilink)) {
					CamelCipherCertInfo *adept = ilink->data;

					if (adept && adept->cert_data) {
						g_return_val_if_fail (g_snprintf (tmp, sizeof (tmp), "%p", adept->cert_data) < sizeof (tmp), NULL);

						if (g_strcmp0 (element_value, tmp) == 0) {
							info = adept;
							break;
						}
					}
				}
				break;
			}
		}
	}

	return info;
}

static void
secure_button_view_certificate_clicked_cb (EWebView *web_view,
					   const gchar *iframe_id,
					   const gchar *element_id,
					   const gchar *element_class,
					   const gchar *element_value,
					   const GtkAllocation *element_position,
					   gpointer user_data)
{
	EMailPart *mail_part = user_data;
	CamelCipherCertInfo *info;

	g_return_if_fail (E_IS_MAIL_PART_SECURE_BUTTON (mail_part));

	if (!element_value)
		return;

	info = secure_button_find_cert_info (mail_part, element_value);

	if (info) {
		GtkWidget *toplevel;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (web_view));

		secure_button_view_certificate (GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL, info);
	}
}

static void
secure_button_import_certificate_clicked_cb (EWebView *web_view,
					     const gchar *iframe_id,
					     const gchar *element_id,
					     const gchar *element_class,
					     const gchar *element_value,
					     const GtkAllocation *element_position,
					     gpointer user_data)
{
	EMailPart *mail_part = user_data;
	CamelCipherCertInfo *info;

	g_return_if_fail (E_IS_MAIL_PART_SECURE_BUTTON (mail_part));

	if (!element_value)
		return;

	info = secure_button_find_cert_info (mail_part, element_value);

	if (info) {
		GtkWidget *toplevel;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (web_view));

		secure_button_import_certificate (GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL, info, web_view, iframe_id, element_id);
	}
}

#endif

static void
secure_button_clicked_cb (EWebView *web_view,
			  const gchar *iframe_id,
			  const gchar *element_id,
			  const gchar *element_class,
			  const gchar *element_value,
			  const GtkAllocation *element_position,
			  gpointer user_data)
{
	EMailPart *mail_part = user_data;
	GList *link;
	gchar tmp[128];

	g_return_if_fail (E_IS_MAIL_PART_SECURE_BUTTON (mail_part));

	if (!element_value)
		return;

	g_return_if_fail (g_snprintf (tmp, sizeof (tmp), "%p:", mail_part) < sizeof (tmp));

	if (!g_str_has_prefix (element_value, tmp))
		return;

	element_value += strlen (tmp);

	for (link = g_queue_peek_head_link (&mail_part->validities); link != NULL; link = g_list_next (link)) {
		EMailPartValidityPair *pair = link->data;

		if (!pair)
			continue;

		g_return_if_fail (g_snprintf (tmp, sizeof (tmp), "%p", pair->validity) < sizeof (tmp));

		if (g_strcmp0 (element_value, tmp) == 0) {
			g_return_if_fail (g_snprintf (tmp, sizeof (tmp), "secure-button-details-%p", pair->validity) < sizeof (tmp));

			e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
				"var elem = Evo.FindElement(%s, %s);\n"
				"if (elem) {\n"
				"	elem.hidden = !elem.hidden;\n"
				"}\n",
				iframe_id, tmp);
			break;
		}
	}
}

static void
secure_button_details_clicked_cb (EWebView *web_view,
				  const gchar *iframe_id,
				  const gchar *element_id,
				  const gchar *element_class,
				  const gchar *element_value,
				  const GtkAllocation *element_position,
				  gpointer user_data)
{
	EMailPart *mail_part = user_data;
	gchar tmp[128];

	g_return_if_fail (E_IS_MAIL_PART_SECURE_BUTTON (mail_part));

	if (!element_id || !element_value)
		return;

	g_return_if_fail (g_snprintf (tmp, sizeof (tmp), "%p:", mail_part) < sizeof (tmp));

	if (g_str_has_prefix (element_id, tmp)) {
		g_return_if_fail (g_snprintf (tmp, sizeof (tmp), "%s-img", element_value) < sizeof (tmp));

		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (web_view), e_web_view_get_cancellable (web_view),
			"var elem = Evo.FindElement(%s, %s);\n"
			"if (elem) {\n"
			"	elem.hidden = !elem.hidden;\n"
			"}\n"
			"elem = Evo.FindElement(%s, %s);\n"
			"if (elem) {\n"
			"	var tmp = elem.src;\n"
			"	elem.src = elem.getAttribute(\"othersrc\");\n"
			"	elem.setAttribute(\"othersrc\", tmp);\n"
			"}\n",
			iframe_id, element_value,
			iframe_id, tmp);
	}
}

static void
mail_part_secure_button_content_loaded (EMailPart *mail_part,
					EWebView *web_view,
					const gchar *iframe_id)
{
	g_return_if_fail (E_IS_MAIL_PART_SECURE_BUTTON (mail_part));
	g_return_if_fail (E_IS_WEB_VIEW (web_view));

	e_web_view_register_element_clicked (web_view, "secure-button", secure_button_clicked_cb, mail_part);
	e_web_view_register_element_clicked (web_view, "secure-button-details", secure_button_details_clicked_cb, mail_part);

#if defined (ENABLE_SMIME)
	e_web_view_register_element_clicked (web_view, "secure-button-view-certificate", secure_button_view_certificate_clicked_cb, mail_part);
	e_web_view_register_element_clicked (web_view, "secure-button-import-certificate", secure_button_import_certificate_clicked_cb, mail_part);
#endif
}

static void
e_mail_part_secure_button_class_init (EMailPartSecureButtonClass *class)
{
	EMailPartClass *mail_part_class;

	mail_part_class = E_MAIL_PART_CLASS (class);
	mail_part_class->content_loaded = mail_part_secure_button_content_loaded;
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
