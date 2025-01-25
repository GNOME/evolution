/*
 * test-mail-signatures.c
 *
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

#include <stdlib.h>

#include <libedataserver/libedataserver.h>

#include <e-util/e-util.h>

static GCancellable *cancellable = NULL;

static void
signature_loaded_cb (EMailSignatureComboBox *combo_box,
                     GAsyncResult *result,
                     EWebView *web_view)
{
	gchar *contents = NULL;
	EContentEditorMode editor_mode = E_CONTENT_EDITOR_MODE_UNKNOWN;
	GError *error = NULL;

	e_mail_signature_combo_box_load_selected_finish (
		combo_box, result, &contents, NULL, &editor_mode, &error);

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (contents == NULL);
		g_object_unref (web_view);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (contents == NULL);
		e_alert_submit (
			E_ALERT_SINK (web_view),
			"widgets:no-load-signature",
			error->message, NULL);
		g_object_unref (web_view);
		g_error_free (error);
		return;
	}

	if (contents == NULL)
		e_web_view_clear (web_view);
	else if (editor_mode == E_CONTENT_EDITOR_MODE_HTML)
		e_web_view_load_string (web_view, contents);
	else {
		gchar *string;

		string = g_markup_printf_escaped ("<pre>%s</pre>", contents);
		e_web_view_load_string (web_view, string);
		g_free (string);
	}

	g_free (contents);

	g_object_unref (web_view);
}

static void
signature_combo_changed_cb (EMailSignatureComboBox *combo_box,
                            EWebView *web_view)
{
	if (cancellable != NULL) {
		g_cancellable_cancel (cancellable);
		g_object_unref (cancellable);
	}

	cancellable = g_cancellable_new ();

	e_mail_signature_combo_box_load_selected (
		combo_box, G_PRIORITY_DEFAULT, cancellable,
		(GAsyncReadyCallback) signature_loaded_cb,
		g_object_ref (web_view));
}

static gboolean
window_delete_event_cb (GtkWidget *widget,
			GdkEvent *event,
			gpointer user_data)
{
	gtk_main_quit ();

	return FALSE;
}

gint
main (gint argc,
      gchar **argv)
{
	ESourceRegistry *registry;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *vbox;
	GtkWidget *identity_combo;
	GtkWidget *signature_combo;
	GError *error = NULL;

	gtk_init (&argc, &argv);

	registry = e_source_registry_new_sync (NULL, &error);

	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		exit (EXIT_FAILURE);
	}

	/* Construct the widgets. */

	widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (widget), "Mail Signatures");
	gtk_window_set_default_size (GTK_WINDOW (widget), 400, 400);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 12);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "delete-event",
		G_CALLBACK (window_delete_event_cb), NULL);

	container = widget;

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = vbox = widget;

	widget = gtk_label_new ("<b>EMailSignatureComboBox</b>");
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start (widget, 12);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = e_mail_signature_combo_box_new (registry);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	signature_combo = widget;
	gtk_widget_show (widget);

	widget = e_mail_identity_combo_box_new (registry);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	identity_combo = widget;
	gtk_widget_show (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = e_web_view_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	g_signal_connect (
		signature_combo, "changed",
		G_CALLBACK (signature_combo_changed_cb), widget);

	container = vbox;

	widget = gtk_label_new ("<b>EMailSignatureManager</b>");
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = e_mail_signature_manager_new (registry);
	gtk_widget_set_margin_start (widget, 12);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		identity_combo, "active-id",
		signature_combo, "identity-uid",
		G_BINDING_SYNC_CREATE);

	gtk_main ();

	e_misc_util_free_global_memory ();

	return 0;
}
