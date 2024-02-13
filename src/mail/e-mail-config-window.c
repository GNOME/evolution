/*
 * e-mail-config-window.c
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

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-util/e-util.h"

#include "e-mail-config-notebook.h"
#include "e-mail-config-sidebar.h"

#include "e-mail-config-window.h"

struct _EMailConfigWindowPrivate {
	EMailSession *session;
	ESource *original_source;

	/* Scratch Sources */
	ESource *account_source;
	ESource *identity_source;
	ESource *transport_source;
	ESource *collection_source;  /* optional */

	GtkWidget *notebook;  /* not referenced */
	GtkWidget *alert_bar; /* not referenced */
};

enum {
	PROP_0,
	PROP_ORIGINAL_SOURCE,
	PROP_SESSION
};

enum {
	CHANGES_COMMITTED,
	LAST_SIGNAL
};

/* Forward Declarations */
static void	e_mail_config_window_alert_sink_init
					(EAlertSinkInterface *iface);

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (EMailConfigWindow, e_mail_config_window, GTK_TYPE_DIALOG,
	G_ADD_PRIVATE (EMailConfigWindow)
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, e_mail_config_window_alert_sink_init))

static ESource *
mail_config_window_clone_source (ESource *source)
{
	ESource *clone;
	GDBusObject *dbus_object;

	dbus_object = e_source_ref_dbus_object (source);

	clone = e_source_new (dbus_object, NULL, NULL);

	if (dbus_object != NULL)
		g_object_unref (dbus_object);

	return clone;
}

static void
mail_config_window_setup_scratch_sources (EMailConfigWindow *window)
{
	ESource *source;
	ESource *scratch_source;
	ESourceRegistry *registry;
	ESourceMailAccount *account_ext;
	ESourceMailSubmission *submission_ext;
	EMailSession *session;
	const gchar *extension_name;
	const gchar *uid;

	session = e_mail_config_window_get_session (window);
	registry = e_mail_session_get_registry (session);

	source = window->priv->original_source;
	scratch_source = mail_config_window_clone_source (source);
	window->priv->account_source = scratch_source;

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	account_ext = e_source_get_extension (source, extension_name);
	uid = e_source_mail_account_get_identity_uid (account_ext);
	source = e_source_registry_ref_source (registry, uid);
	scratch_source = mail_config_window_clone_source (source);
	window->priv->identity_source = scratch_source;
	g_object_unref (source);

	extension_name = E_SOURCE_EXTENSION_MAIL_SUBMISSION;
	submission_ext = e_source_get_extension (source, extension_name);
	uid = e_source_mail_submission_get_transport_uid (submission_ext);
	source = e_source_registry_ref_source (registry, uid);
	scratch_source = mail_config_window_clone_source (source);
	window->priv->transport_source = scratch_source;
	g_object_unref (source);

	extension_name = E_SOURCE_EXTENSION_COLLECTION;
	source = e_source_registry_find_extension (
		registry, window->priv->original_source, extension_name);
	if (source != NULL) {
		scratch_source = mail_config_window_clone_source (source);
		window->priv->collection_source = scratch_source;
		g_object_unref (source);
	}
}

static void
mail_config_window_commit_cb (GObject *object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	EMailConfigWindow *window;
	EMailConfigNotebook *notebook;
	GdkWindow *gdk_window;
	GError *error = NULL;

	window = E_MAIL_CONFIG_WINDOW (user_data);
	notebook = E_MAIL_CONFIG_NOTEBOOK (object);

	/* Set the cursor back to normal. */
	gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
	gdk_window_set_cursor (gdk_window, NULL);

	/* Allow user interaction with window content. */
	gtk_widget_set_sensitive (GTK_WIDGET (window), TRUE);

	e_mail_config_notebook_commit_finish (notebook, result, &error);

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_object_unref (window);
		g_error_free (error);

	} else if (error != NULL) {
		e_alert_submit (
			E_ALERT_SINK (window),
			"system:simple-error",
			error->message, NULL);
		g_object_unref (window);
		g_error_free (error);

	} else {
		g_signal_emit (window, signals[CHANGES_COMMITTED], 0);
		gtk_widget_destroy (GTK_WIDGET (window));
	}
}

static void
mail_config_window_commit (EMailConfigWindow *window)
{
	GdkCursor *gdk_cursor;
	EMailConfigNotebook *notebook;

	notebook = E_MAIL_CONFIG_NOTEBOOK (window->priv->notebook);

	/* Clear any previous alerts. */
	e_alert_bar_clear (E_ALERT_BAR (window->priv->alert_bar));

	/* Make the cursor appear busy. */
	gdk_cursor = gdk_cursor_new_from_name (gtk_widget_get_display (GTK_WIDGET (window)), "wait");
	if (gdk_cursor) {
		GdkWindow *gdk_window;

		gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
		gdk_window_set_cursor (gdk_window, gdk_cursor);
		g_object_unref (gdk_cursor);
	}

	/* Prevent user interaction with window content. */
	gtk_widget_set_sensitive (GTK_WIDGET (window), FALSE);

	/* XXX This operation is not cancellable. */
	e_mail_config_notebook_commit (
		notebook, NULL,
		mail_config_window_commit_cb,
		g_object_ref (window));
}

static void
mail_config_window_set_original_source (EMailConfigWindow *window,
                                        ESource *original_source)
{
	g_return_if_fail (E_IS_SOURCE (original_source));
	g_return_if_fail (window->priv->original_source == NULL);

	window->priv->original_source = g_object_ref (original_source);
}

static void
mail_config_window_set_session (EMailConfigWindow *window,
                                EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (window->priv->session == NULL);

	window->priv->session = g_object_ref (session);
}

static void
mail_config_window_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ORIGINAL_SOURCE:
			mail_config_window_set_original_source (
				E_MAIL_CONFIG_WINDOW (object),
				g_value_get_object (value));
			return;

		case PROP_SESSION:
			mail_config_window_set_session (
				E_MAIL_CONFIG_WINDOW (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_window_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ORIGINAL_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_window_get_original_source (
				E_MAIL_CONFIG_WINDOW (object)));
			return;

		case PROP_SESSION:
			g_value_set_object (
				value,
				e_mail_config_window_get_session (
				E_MAIL_CONFIG_WINDOW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_window_dispose (GObject *object)
{
	EMailConfigWindow *self = E_MAIL_CONFIG_WINDOW (object);

	g_clear_object (&self->priv->session);
	g_clear_object (&self->priv->original_source);
	g_clear_object (&self->priv->account_source);
	g_clear_object (&self->priv->identity_source);
	g_clear_object (&self->priv->transport_source);
	g_clear_object (&self->priv->collection_source);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_window_parent_class)->dispose (object);
}

static void
mail_config_window_constructed (GObject *object)
{
	EMailConfigWindow *window;
	GtkWidget *container;
	GtkWidget *widget;
	GSList *children = NULL;
	gint ii, npages;
	GtkRequisition requisition;

	window = E_MAIL_CONFIG_WINDOW (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_window_parent_class)->constructed (object);

	mail_config_window_setup_scratch_sources (window);

	gtk_container_set_border_width (GTK_CONTAINER (window), 5);
	gtk_window_set_title (GTK_WINDOW (window), _("Account Editor"));
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);

	gtk_dialog_add_buttons (
		GTK_DIALOG (window),
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK,
		NULL);

	container = gtk_dialog_get_content_area (GTK_DIALOG (window));

	widget = gtk_grid_new ();
	gtk_grid_set_column_spacing (GTK_GRID (widget), 12);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 5);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = e_alert_bar_new ();
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 3, 1);
	window->priv->alert_bar = widget;  /* not referenced */
	/* EAlertBar controls its own visibility. */

	gtk_widget_get_preferred_size (GTK_WIDGET (window), &requisition, NULL);
	requisition.width += 12 + 5; /* column spacing + border width of the grid*/

	/* Add an extra-wide margin to the left and bottom.
	 *
	 * XXX The bottom margin is tricky.  We want a 24px margin between
	 *     the notebook and the dialog action buttons, but we have to
	 *     take style property defaults into consideration:
	 *
	 *     24 - action-area-border (5) - content-area-border (2) = 17
	 */
	widget = e_mail_config_notebook_new (
		window->priv->session,
		window->priv->original_source,
		window->priv->account_source,
		window->priv->identity_source,
		window->priv->transport_source,
		window->priv->collection_source);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_widget_set_margin_bottom (widget, 17);
	requisition.height += 17 + 5; /* margin bottom + border width of the grid */
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_grid_attach (GTK_GRID (container), widget, 2, 1, 1, 1);
	window->priv->notebook = widget;  /* not referenced */
	gtk_widget_show (widget);

	widget = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_widget_set_valign (widget, GTK_ALIGN_FILL);
	gtk_grid_attach (GTK_GRID (container), widget, 1, 1, 1, 1);
	gtk_widget_show (widget);

	widget = e_mail_config_sidebar_new (
		E_MAIL_CONFIG_NOTEBOOK (window->priv->notebook));
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	gtk_widget_show (widget);

	/* Make the Apply button insensitive when required
	 * fields in the notebook pages are incomplete. */

	widget = gtk_dialog_get_widget_for_response (
		GTK_DIALOG (window), GTK_RESPONSE_OK);

	e_binding_bind_property (
		window->priv->notebook, "complete",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->priv->notebook));
	for (ii = 0; ii < npages; ii++) {
		children = g_slist_prepend (children, gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->priv->notebook), ii));
	}

	e_util_resize_window_for_screen (GTK_WINDOW (window), requisition.width, requisition.height, children);

	g_slist_free (children);
}

static void
mail_config_window_response (GtkDialog *dialog,
                             gint response_id)
{
	/* Do not chain up.  GtkDialog does not implement this method. */

	switch (response_id) {
		case GTK_RESPONSE_OK:
			mail_config_window_commit (
				E_MAIL_CONFIG_WINDOW (dialog));
			break;
		case GTK_RESPONSE_CANCEL:
			gtk_widget_destroy (GTK_WIDGET (dialog));
			break;
		default:
			break;
	}
}

static void
mail_config_window_submit_alert (EAlertSink *alert_sink,
                                 EAlert *alert)
{
	EMailConfigWindow *self = E_MAIL_CONFIG_WINDOW (alert_sink);

	e_alert_bar_submit_alert (E_ALERT_BAR (self->priv->alert_bar), alert);
}

static void
e_mail_config_window_class_init (EMailConfigWindowClass *class)
{
	GObjectClass *object_class;
	GtkDialogClass *dialog_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_window_set_property;
	object_class->get_property = mail_config_window_get_property;
	object_class->dispose = mail_config_window_dispose;
	object_class->constructed = mail_config_window_constructed;

	dialog_class = GTK_DIALOG_CLASS (class);
	dialog_class->response = mail_config_window_response;

	g_object_class_install_property (
		object_class,
		PROP_ORIGINAL_SOURCE,
		g_param_spec_object (
			"original-source",
			"Original Source",
			"Original mail account source",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			"Session",
			"Mail session",
			E_TYPE_MAIL_SESSION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	signals[CHANGES_COMMITTED] = g_signal_new (
		"changes-committed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailConfigWindowClass, changes_committed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_mail_config_window_alert_sink_init (EAlertSinkInterface *iface)
{
	iface->submit_alert = mail_config_window_submit_alert;
}

static void
e_mail_config_window_init (EMailConfigWindow *window)
{
	window->priv = e_mail_config_window_get_instance_private (window);
}

GtkWidget *
e_mail_config_window_new (EMailSession *session,
                          ESource *original_source)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);
	g_return_val_if_fail (E_IS_SOURCE (original_source), NULL);

	return g_object_new (
		E_TYPE_MAIL_CONFIG_WINDOW,
		"original-source", original_source,
		"session", session, NULL);
}

EMailSession *
e_mail_config_window_get_session (EMailConfigWindow *window)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_WINDOW (window), NULL);

	return window->priv->session;
}

ESource *
e_mail_config_window_get_original_source (EMailConfigWindow *window)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_WINDOW (window), NULL);

	return window->priv->original_source;
}

