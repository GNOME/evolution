/*
 * e-mail-config-defaults-page.c
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

#include <libebackend/libebackend.h>

#include "libemail-engine/libemail-engine.h"

#include "e-mail-config-page.h"
#include "e-mail-config-activity-page.h"
#include "em-folder-selection-button.h"

#include "e-mail-config-defaults-page.h"

struct _EMailConfigDefaultsPagePrivate {
	EMailSession *session;
	ESource *account_source;
	ESource *collection_source;
	ESource *identity_source;
	ESource *original_source;
	ESource *transport_source;

	GtkWidget *drafts_button;  /* not referenced */
	GtkWidget *sent_button;    /* not referenced */
	GtkWidget *archive_button; /* not referenced */
	GtkWidget *templates_button; /* not referenced */
	GtkWidget *replies_toggle; /* not referenced */
	GtkWidget *trash_toggle;   /* not referenced */
	GtkWidget *junk_toggle;    /* not referenced */
};

enum {
	PROP_0,
	PROP_ACCOUNT_SOURCE,
	PROP_COLLECTION_SOURCE,
	PROP_IDENTITY_SOURCE,
	PROP_ORIGINAL_SOURCE,
	PROP_TRANSPORT_SOURCE,
	PROP_SESSION
};

/* Forward Declarations */
static void	e_mail_config_defaults_page_interface_init
					(EMailConfigPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EMailConfigDefaultsPage, e_mail_config_defaults_page, E_TYPE_MAIL_CONFIG_ACTIVITY_PAGE,
	G_ADD_PRIVATE (EMailConfigDefaultsPage)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_MAIL_CONFIG_PAGE, e_mail_config_defaults_page_interface_init))

static CamelSettings *
mail_config_defaults_page_maybe_get_settings (EMailConfigDefaultsPage *page)
{
	ESource *source;
	ESourceCamel *camel_ext;
	ESourceBackend *backend_ext;
	const gchar *backend_name;
	const gchar *extension_name;

	source = e_mail_config_defaults_page_get_account_source (page);

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	backend_ext = e_source_get_extension (source, extension_name);
	backend_name = e_source_backend_get_backend_name (backend_ext);
	extension_name = e_source_camel_get_extension_name (backend_name);

	/* Avoid accidentally creating a backend-specific extension
	 * in the mail account source if the mail account source is
	 * part of a collection, in which case the backend-specific
	 * extension is kept in the top-level collection source. */
	if (!e_source_has_extension (source, extension_name))
		return NULL;

	camel_ext = e_source_get_extension (source, extension_name);

	return e_source_camel_get_settings (camel_ext);
}

static CamelStore *
mail_config_defaults_page_ref_store (EMailConfigDefaultsPage *page)
{
	ESource *source;
	EMailSession *session;
	CamelService *service;
	const gchar *uid;

	session = e_mail_config_defaults_page_get_session (page);
	source = e_mail_config_defaults_page_get_account_source (page);

	uid = e_source_get_uid (source);
	service = camel_session_ref_service (CAMEL_SESSION (session), uid);

	if (service == NULL)
		return NULL;

	if (!CAMEL_IS_STORE (service)) {
		g_object_unref (service);
		return NULL;
	}

	return CAMEL_STORE (service);
}

static gboolean
mail_config_defaults_page_folder_name_to_uri (GBinding *binding,
                                              const GValue *source_value,
                                              GValue *target_value,
                                              gpointer data)
{
	EMailConfigDefaultsPage *page;
	CamelStore *store;
	const gchar *folder_name;
	gchar *folder_uri = NULL;

	page = E_MAIL_CONFIG_DEFAULTS_PAGE (data);
	store = mail_config_defaults_page_ref_store (page);
	g_return_val_if_fail (store != NULL, FALSE);

	folder_name = g_value_get_string (source_value);

	if (folder_name != NULL)
		folder_uri = e_mail_folder_uri_build (store, folder_name);

	g_value_set_string (target_value, folder_uri);

	g_free (folder_uri);

	g_object_unref (store);

	return TRUE;
}

static gboolean
mail_config_defaults_page_folder_uri_to_name (GBinding *binding,
                                              const GValue *source_value,
                                              GValue *target_value,
                                              gpointer data)
{
	EMailConfigDefaultsPage *page;
	EMailSession *session;
	const gchar *folder_uri;
	gchar *folder_name = NULL;
	GError *error = NULL;

	page = E_MAIL_CONFIG_DEFAULTS_PAGE (data);
	session = e_mail_config_defaults_page_get_session (page);

	folder_uri = g_value_get_string (source_value);

	if (folder_uri == NULL) {
		g_value_set_string (target_value, NULL);
		return TRUE;
	}

	e_mail_folder_uri_parse (
		CAMEL_SESSION (session), folder_uri,
		NULL, &folder_name, &error);

	if (error != NULL) {
		g_warn_if_fail (folder_name == NULL);
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		return FALSE;
	}

	g_return_val_if_fail (folder_name != NULL, FALSE);

	g_value_set_string (target_value, folder_name);

	g_free (folder_name);

	return TRUE;
}

static void
mail_config_defaults_page_restore_folders (EMailConfigDefaultsPage *page)
{
	EMFolderSelectionButton *button;
	EMailSession *session;
	EMailLocalFolder type;
	const gchar *folder_uri;

	session = e_mail_config_defaults_page_get_session (page);

	type = E_MAIL_LOCAL_FOLDER_DRAFTS;
	button = EM_FOLDER_SELECTION_BUTTON (page->priv->drafts_button);
	folder_uri = e_mail_session_get_local_folder_uri (session, type);
	em_folder_selection_button_set_folder_uri (button, folder_uri);

	type = E_MAIL_LOCAL_FOLDER_TEMPLATES;
	button = EM_FOLDER_SELECTION_BUTTON (page->priv->templates_button);
	folder_uri = e_mail_session_get_local_folder_uri (session, type);
	em_folder_selection_button_set_folder_uri (button, folder_uri);

	if (gtk_widget_is_sensitive (page->priv->sent_button)) {
		type = E_MAIL_LOCAL_FOLDER_SENT;
		button = EM_FOLDER_SELECTION_BUTTON (page->priv->sent_button);
		folder_uri = e_mail_session_get_local_folder_uri (session, type);
		em_folder_selection_button_set_folder_uri (button, folder_uri);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->priv->replies_toggle), FALSE);
	}
}

typedef struct _AsyncContext
{
	EActivity *activity;
	EMailConfigDefaultsPage *page;
	GtkWidget *button;
} AsyncContext;

static void
async_context_free (AsyncContext *async_context)
{
	if (async_context) {
		g_clear_object (&async_context->activity);
		g_clear_object (&async_context->page);
		g_clear_object (&async_context->button);

		g_slice_free (AsyncContext, async_context);
	}
}

static void
mail_config_defaults_initial_setup_done_cb (GObject *source_object,
					    GAsyncResult *result,
					    gpointer user_data)
{
	AsyncContext *async_context = user_data;
	CamelStore *store = CAMEL_STORE (source_object);
	EAlertSink *alert_sink;
	GHashTable *save_setup = NULL;
	GError *error = NULL;

	alert_sink = e_activity_get_alert_sink (async_context->activity);

	camel_store_initial_setup_finish (store, result, &save_setup, &error);

	if (e_activity_handle_cancellation (async_context->activity, error)) {
		g_warn_if_fail (save_setup == NULL);
		g_error_free (error);

	} else if (error != NULL) {
		g_warn_if_fail (save_setup == NULL);
		e_alert_submit (
			alert_sink,
			"mail:initial-setup-error",
			error->message, NULL);
		g_error_free (error);

	} else if (save_setup) {
		e_mail_store_save_initial_setup_sync (store, save_setup,
			async_context->page->priv->collection_source,
			async_context->page->priv->account_source,
			async_context->page->priv->identity_source,
			async_context->page->priv->transport_source,
			FALSE, NULL, NULL);

		g_hash_table_destroy (save_setup);
	}

	gtk_widget_set_sensitive (async_context->button, TRUE);

	async_context_free (async_context);
}

static void
mail_config_defaults_page_autodetect_folders_cb (EMailConfigDefaultsPage *page,
						 GtkWidget *button)
{
	CamelService *service;
	EActivity *activity;
	AsyncContext *async_context;
	GCancellable *cancellable;

	g_return_if_fail (E_IS_MAIL_CONFIG_DEFAULTS_PAGE (page));

	service = camel_session_ref_service (CAMEL_SESSION (page->priv->session), e_source_get_uid (page->priv->original_source));

	if (!service || !CAMEL_IS_STORE (service)) {
		g_clear_object (&service);
		return;
	}

	activity = e_mail_config_activity_page_new_activity (E_MAIL_CONFIG_ACTIVITY_PAGE (page));

	cancellable = e_activity_get_cancellable (activity);
	e_activity_set_text (activity, _("Checking server settings…"));

	gtk_widget_set_sensitive (button, FALSE);

	async_context = g_slice_new (AsyncContext);
	async_context->activity = activity;
	async_context->page = g_object_ref (page);
	async_context->button = g_object_ref (button);

	camel_store_initial_setup (
		CAMEL_STORE (service), G_PRIORITY_DEFAULT, cancellable,
		mail_config_defaults_initial_setup_done_cb, async_context);

	g_object_unref (service);
}

static void
mail_config_defaults_page_restore_real_folder (GtkToggleButton *toggle_button)
{
	gtk_toggle_button_set_active (toggle_button, FALSE);
}

static GtkWidget *
mail_config_defaults_page_add_real_folder (EMailConfigDefaultsPage *page,
                                           GtkSizeGroup *size_group,
                                           GtkButton *revert_button,
                                           const gchar *toggle_label,
                                           const gchar *dialog_caption,
                                           const gchar *property_name,
                                           const gchar *use_property_name)
{
	GtkWidget *box;
	GtkWidget *check_button;
	GtkWidget *folder_button;
	EMailSession *session;
	CamelSettings *settings;
	CamelStore *store;
	GObjectClass *class;

	session = e_mail_config_defaults_page_get_session (page);
	settings = mail_config_defaults_page_maybe_get_settings (page);

	if (settings == NULL)
		return NULL;

	/* These folder settings are backend-specific, so check if
	 * the CamelSettings class has the property names we need. */

	class = G_OBJECT_GET_CLASS (settings);

	if (g_object_class_find_property (class, property_name) == NULL)
		return NULL;

	if (g_object_class_find_property (class, use_property_name) == NULL)
		return NULL;

	store = mail_config_defaults_page_ref_store (page);
	g_return_val_if_fail (store != NULL, NULL);

	/* We're good to go. */

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

	check_button = gtk_check_button_new_with_mnemonic (toggle_label);
	g_object_set (check_button, "xalign", 1.0, NULL);
	gtk_size_group_add_widget (size_group, check_button);
	gtk_box_pack_start (GTK_BOX (box), check_button, FALSE, FALSE, 0);
	gtk_widget_show (check_button);

	e_binding_bind_property (
		settings, use_property_name,
		check_button, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	folder_button = em_folder_selection_button_new (
		session, "", dialog_caption);
	em_folder_selection_button_set_store (
		EM_FOLDER_SELECTION_BUTTON (folder_button), store);
	gtk_box_pack_start (GTK_BOX (box), folder_button, TRUE, TRUE, 0);
	gtk_widget_show (folder_button);

	/* XXX CamelSettings only stores the folder's path name, but the
	 *     EMFolderSelectionButton requires a full folder URI, so we
	 *     have to do some fancy transforms for the binding to work. */
	e_binding_bind_property_full (
		settings, property_name,
		folder_button, "folder-uri",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		mail_config_defaults_page_folder_name_to_uri,
		mail_config_defaults_page_folder_uri_to_name,
		g_object_ref (page),
		(GDestroyNotify) g_object_unref);

	e_binding_bind_property (
		check_button, "active",
		folder_button, "sensitive",
		G_BINDING_SYNC_CREATE);

	g_signal_connect_swapped (
		revert_button, "clicked",
		G_CALLBACK (mail_config_defaults_page_restore_real_folder),
		check_button);

	g_object_unref (store);

	return box;
}

static void
mail_config_defaults_page_set_collection_source (EMailConfigDefaultsPage *page,
						 ESource *collection_source)
{
	if (collection_source)
		g_return_if_fail (E_IS_SOURCE (collection_source));
	g_return_if_fail (page->priv->collection_source == NULL);

	page->priv->collection_source = collection_source ? g_object_ref (collection_source) : NULL;
}

static void
mail_config_defaults_page_set_account_source (EMailConfigDefaultsPage *page,
                                              ESource *account_source)
{
	g_return_if_fail (E_IS_SOURCE (account_source));
	g_return_if_fail (page->priv->account_source == NULL);

	page->priv->account_source = g_object_ref (account_source);
}

static void
mail_config_defaults_page_set_identity_source (EMailConfigDefaultsPage *page,
                                               ESource *identity_source)
{
	g_return_if_fail (E_IS_SOURCE (identity_source));
	g_return_if_fail (page->priv->identity_source == NULL);

	page->priv->identity_source = g_object_ref (identity_source);
}

static void
mail_config_defaults_page_set_original_source (EMailConfigDefaultsPage *page,
					       ESource *original_source)
{
	if (original_source)
		g_return_if_fail (E_IS_SOURCE (original_source));
	g_return_if_fail (page->priv->original_source == NULL);

	page->priv->original_source = original_source ? g_object_ref (original_source) : NULL;
}

static void
mail_config_defaults_page_set_transport_source (EMailConfigDefaultsPage *page,
						ESource *transport_source)
{
	if (transport_source)
		g_return_if_fail (E_IS_SOURCE (transport_source));
	g_return_if_fail (page->priv->transport_source == NULL);

	page->priv->transport_source = transport_source ? g_object_ref (transport_source) : NULL;
}

static void
mail_config_defaults_page_set_session (EMailConfigDefaultsPage *page,
                                       EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (page->priv->session == NULL);

	page->priv->session = g_object_ref (session);
}

static void
mail_config_defaults_page_set_property (GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_SOURCE:
			mail_config_defaults_page_set_account_source (
				E_MAIL_CONFIG_DEFAULTS_PAGE (object),
				g_value_get_object (value));
			return;

		case PROP_COLLECTION_SOURCE:
			mail_config_defaults_page_set_collection_source (
				E_MAIL_CONFIG_DEFAULTS_PAGE (object),
				g_value_get_object (value));
			return;

		case PROP_IDENTITY_SOURCE:
			mail_config_defaults_page_set_identity_source (
				E_MAIL_CONFIG_DEFAULTS_PAGE (object),
				g_value_get_object (value));
			return;

		case PROP_ORIGINAL_SOURCE:
			mail_config_defaults_page_set_original_source (
				E_MAIL_CONFIG_DEFAULTS_PAGE (object),
				g_value_get_object (value));
			return;

		case PROP_TRANSPORT_SOURCE:
			mail_config_defaults_page_set_transport_source (
				E_MAIL_CONFIG_DEFAULTS_PAGE (object),
				g_value_get_object (value));
			return;

		case PROP_SESSION:
			mail_config_defaults_page_set_session (
				E_MAIL_CONFIG_DEFAULTS_PAGE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_defaults_page_get_property (GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_defaults_page_get_account_source (
				E_MAIL_CONFIG_DEFAULTS_PAGE (object)));
			return;

		case PROP_COLLECTION_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_defaults_page_get_collection_source (
				E_MAIL_CONFIG_DEFAULTS_PAGE (object)));
			return;

		case PROP_IDENTITY_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_defaults_page_get_identity_source (
				E_MAIL_CONFIG_DEFAULTS_PAGE (object)));
			return;

		case PROP_ORIGINAL_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_defaults_page_get_original_source (
				E_MAIL_CONFIG_DEFAULTS_PAGE (object)));
			return;

		case PROP_TRANSPORT_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_defaults_page_get_transport_source (
				E_MAIL_CONFIG_DEFAULTS_PAGE (object)));
			return;

		case PROP_SESSION:
			g_value_set_object (
				value,
				e_mail_config_defaults_page_get_session (
				E_MAIL_CONFIG_DEFAULTS_PAGE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_defaults_page_dispose (GObject *object)
{
	EMailConfigDefaultsPage *self = E_MAIL_CONFIG_DEFAULTS_PAGE (object);

	g_clear_object (&self->priv->account_source);
	g_clear_object (&self->priv->collection_source);
	g_clear_object (&self->priv->identity_source);
	g_clear_object (&self->priv->transport_source);
	g_clear_object (&self->priv->session);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_defaults_page_parent_class)->dispose (object);
}

static void
mail_config_defaults_page_constructed (GObject *object)
{
	EMailConfigDefaultsPage *page;
	EMailSession *session;
	ESource *source;
	ESourceBackend *account_ext;
	ESourceMailComposition *composition_ext;
	ESourceMailSubmission *submission_ext;
	GtkLabel *label;
	GtkButton *button;
	GtkWidget *widget;
	GtkWidget *container;
	GtkWidget *hbox, *main_box;
	GtkSizeGroup *size_group;
	CamelProvider *provider = NULL;
	const gchar *extension_name;
	const gchar *text;
	gint row = 0;
	gchar *markup;
	gboolean disable_sent_folder;

	page = E_MAIL_CONFIG_DEFAULTS_PAGE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_defaults_page_parent_class)->constructed (object);

	source = e_mail_config_defaults_page_get_account_source (page);
	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	account_ext = e_source_get_extension (source, extension_name);
	if (e_source_backend_get_backend_name (account_ext))
		provider = camel_provider_get (e_source_backend_get_backend_name (account_ext), NULL);

	session = e_mail_config_defaults_page_get_session (page);
	source = e_mail_config_defaults_page_get_identity_source (page);

	extension_name = E_SOURCE_EXTENSION_MAIL_COMPOSITION;
	composition_ext = e_source_get_extension (source, extension_name);

	extension_name = E_SOURCE_EXTENSION_MAIL_SUBMISSION;
	submission_ext = e_source_get_extension (source, extension_name);

	main_box = e_mail_config_activity_page_get_internal_box (E_MAIL_CONFIG_ACTIVITY_PAGE (page));

	gtk_box_set_spacing (GTK_BOX (main_box), 12);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	/*** Special Folders ***/

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Special Folders");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, row, 2, 1);
	gtk_widget_show (widget);
	g_free (markup);
	row++;

	text = _("Draft Messages _Folder:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, row, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	text = _("Choose a folder for saving draft messages.");
	widget = em_folder_selection_button_new (session, "", text);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, row, 1, 1);
	page->priv->drafts_button = widget;  /* not referenced */
	gtk_widget_show (widget);
	row++;

	e_binding_bind_object_text_property (
		composition_ext, "drafts-folder",
		widget, "folder-uri",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	disable_sent_folder = provider && (provider->flags & CAMEL_PROVIDER_DISABLE_SENT_FOLDER) != 0;

	text = _("Sent _Messages Folder:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 0, row, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	text = _("Choose a folder for saving sent messages.");
	widget = em_folder_selection_button_new (session, "", text);
	gtk_widget_set_hexpand (widget, TRUE);
	if (disable_sent_folder)
		gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, row, 1, 1);
	page->priv->sent_button = widget;  /* not referenced */
	gtk_widget_show (widget);
	row++;

	if (disable_sent_folder) {
		gtk_widget_set_sensitive (GTK_WIDGET (label), FALSE);
		gtk_widget_set_sensitive (widget, FALSE);
	}

	e_binding_bind_object_text_property (
		submission_ext, "sent-folder",
		widget, "folder-uri",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	widget = gtk_check_button_new_with_mnemonic (_("Save s_ent messages into the Sent folder"));
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"sensitive", !disable_sent_folder,
		"visible", TRUE,
		NULL);
	gtk_grid_attach (GTK_GRID (container), widget, 0, row, 2, 1);
	row++;

	e_binding_bind_property (
		submission_ext, "use-sent-folder",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	widget = gtk_check_button_new_with_mnemonic (_("S_ave replies and forwards in the folder of the original message"));
	g_object_set (widget, "xalign", 0.0, NULL);
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (container), widget, 0, row, 2, 1);
	page->priv->replies_toggle = widget; /* not referenced */
	gtk_widget_show (widget);
	row++;

	if (disable_sent_folder) {
		gtk_widget_set_sensitive (widget, FALSE);
	} else {
		e_binding_bind_property (
			submission_ext, "use-sent-folder",
			widget, "sensitive",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);
	}

	e_binding_bind_property (
		submission_ext, "replies-to-origin-folder",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	text = _("Archi_ve Folder:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, row, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	text = _("Choose a folder to archive messages to.");
	widget = em_folder_selection_button_new (session, "", text);
	em_folder_selection_button_set_can_none (EM_FOLDER_SELECTION_BUTTON (widget), TRUE);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, row, 1, 1);
	page->priv->archive_button = widget;  /* not referenced */
	gtk_widget_show (widget);
	row++;

	e_binding_bind_object_text_property (
		account_ext, "archive-folder",
		widget, "folder-uri",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	text = _("_Templates Folder:");
	widget = gtk_label_new_with_mnemonic (text);
	gtk_widget_set_margin_start (widget, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, row, 1, 1);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	text = _("Choose a folder to use for template messages.");
	widget = em_folder_selection_button_new (session, "", text);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_grid_attach (GTK_GRID (container), widget, 1, row, 1, 1);
	page->priv->templates_button = widget;  /* not referenced */
	gtk_widget_show (widget);
	row++;

	e_binding_bind_object_text_property (
		composition_ext, "templates-folder",
		widget, "folder-uri",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

	widget = gtk_button_new_with_mnemonic (_("_Restore Defaults"));
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (mail_config_defaults_page_restore_folders),
		page);

	if (page->priv->original_source) {
		CamelService *service;

		service = camel_session_ref_service (CAMEL_SESSION (session), e_source_get_uid (page->priv->original_source));

		if (service && CAMEL_IS_STORE (service) &&
		    (camel_store_get_flags (CAMEL_STORE (service)) & CAMEL_STORE_SUPPORTS_INITIAL_SETUP) != 0) {
			widget = gtk_button_new_with_mnemonic (_("_Lookup Folders"));
			gtk_widget_set_halign (widget, GTK_ALIGN_START);
			gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
			gtk_widget_show (widget);

			g_signal_connect_swapped (
				widget, "clicked",
				G_CALLBACK (mail_config_defaults_page_autodetect_folders_cb),
				page);
		}

		g_clear_object (&service);
	}

	button = GTK_BUTTON (widget);

	widget = mail_config_defaults_page_add_real_folder (
		page, size_group, button,
		_("Use a Real Folder for _Trash:"),
		_("Choose a folder for deleted messages."),
		"real-trash-path", "use-real-trash-path");
	if (widget != NULL) {
		gtk_grid_attach (GTK_GRID (container), widget, 0, row, 2, 1);
		gtk_widget_show (widget);
		row++;
	}

	widget = mail_config_defaults_page_add_real_folder (
		page, size_group, button,
		_("Use a Real Folder for _Junk:"),
		_("Choose a folder for junk messages."),
		"real-junk-path", "use-real-junk-path");
	if (widget != NULL) {
		GtkWidget *restore_widget;

		gtk_grid_attach (GTK_GRID (container), widget, 0, row, 2, 1);
		gtk_widget_show (widget);
		row++;

		restore_widget = mail_config_defaults_page_add_real_folder (
			page, size_group, button,
			_("Restore _Not-Junk to Folder:"),
			_("Choose a folder to restore not-junk messages from the Junk folder to."),
			"real-not-junk-path", "use-real-not-junk-path");
		if (restore_widget != NULL) {
			CamelSettings *settings;

			settings = mail_config_defaults_page_maybe_get_settings (page);
			g_warn_if_fail (settings != NULL);

			e_binding_bind_property (
				settings, "use-real-junk-path",
				restore_widget, "sensitive",
				G_BINDING_SYNC_CREATE);

			gtk_grid_attach (GTK_GRID (container), restore_widget, 0, row, 2, 1);
			gtk_widget_show (restore_widget);
			row++;
		}
	}

	gtk_grid_attach (GTK_GRID (container), hbox, 1, row, 1, 1);
	gtk_widget_show (hbox);
	row++;

	g_object_unref (size_group);

	/* Other options */
	markup = g_markup_printf_escaped ("<b>%s</b>", _("Miscellaneous"));
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, row, 2, 1);
	gtk_widget_show (widget);
	g_free (markup);
	row++;

	widget = e_dialog_new_mark_seen_box (account_ext);
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, row, 2, 1);
	gtk_widget_show (widget);
	row++;

	e_mail_config_page_set_content (E_MAIL_CONFIG_PAGE (page), main_box);

	e_extensible_load_extensions (E_EXTENSIBLE (page));
}

static void
e_mail_config_defaults_page_class_init (EMailConfigDefaultsPageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_defaults_page_set_property;
	object_class->get_property = mail_config_defaults_page_get_property;
	object_class->dispose = mail_config_defaults_page_dispose;
	object_class->constructed = mail_config_defaults_page_constructed;

	g_object_class_install_property (
		object_class,
		PROP_ACCOUNT_SOURCE,
		g_param_spec_object (
			"account-source",
			"Account Source",
			"Mail account source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_COLLECTION_SOURCE,
		g_param_spec_object (
			"collection-source",
			"Collection Source",
			"Collection source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_IDENTITY_SOURCE,
		g_param_spec_object (
			"identity-source",
			"Identity Source",
			"Mail identity source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_ORIGINAL_SOURCE,
		g_param_spec_object (
			"original-source",
			"Original Source",
			"Mail account original source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TRANSPORT_SOURCE,
		g_param_spec_object (
			"transport-source",
			"Transport Source",
			"Mail transport source being edited",
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
}

static void
e_mail_config_defaults_page_interface_init (EMailConfigPageInterface *iface)
{
	iface->title = _("Defaults");
	iface->sort_order = E_MAIL_CONFIG_DEFAULTS_PAGE_SORT_ORDER;
}

static void
e_mail_config_defaults_page_init (EMailConfigDefaultsPage *page)
{
	page->priv = e_mail_config_defaults_page_get_instance_private (page);
}

EMailConfigPage *
e_mail_config_defaults_page_new (EMailSession *session,
				 ESource *original_source,
				 ESource *collection_source,
                                 ESource *account_source,
                                 ESource *identity_source,
				 ESource *transport_source)
{
	/* original, collection and transport sources are optional */
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);
	g_return_val_if_fail (E_IS_SOURCE (account_source), NULL);
	g_return_val_if_fail (E_IS_SOURCE (identity_source), NULL);

	return g_object_new (
		E_TYPE_MAIL_CONFIG_DEFAULTS_PAGE,
		"collection-source", collection_source,
		"account-source", account_source,
		"identity-source", identity_source,
		"original-source", original_source,
		"transport-source", transport_source,
		"session", session, NULL);
}

EMailSession *
e_mail_config_defaults_page_get_session (EMailConfigDefaultsPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_DEFAULTS_PAGE (page), NULL);

	return page->priv->session;
}

ESource *
e_mail_config_defaults_page_get_account_source (EMailConfigDefaultsPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_DEFAULTS_PAGE (page), NULL);

	return page->priv->account_source;
}

ESource *
e_mail_config_defaults_page_get_collection_source (EMailConfigDefaultsPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_DEFAULTS_PAGE (page), NULL);

	return page->priv->collection_source;
}

ESource *
e_mail_config_defaults_page_get_identity_source (EMailConfigDefaultsPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_DEFAULTS_PAGE (page), NULL);

	return page->priv->identity_source;
}

ESource *
e_mail_config_defaults_page_get_original_source (EMailConfigDefaultsPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_DEFAULTS_PAGE (page), NULL);

	return page->priv->original_source;
}

ESource *
e_mail_config_defaults_page_get_transport_source (EMailConfigDefaultsPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_DEFAULTS_PAGE (page), NULL);

	return page->priv->transport_source;
}
