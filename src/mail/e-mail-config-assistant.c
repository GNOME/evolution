/*
 * e-mail-config-assistant.c
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

#include <shell/e-shell.h>
#include <shell/e-shell-window.h>
#include <shell/e-shell-view.h>
#include <shell/e-shell-sidebar.h>

#include "e-mail-config-confirm-page.h"
#include "e-mail-config-identity-page.h"
#include "e-mail-config-lookup-page.h"
#include "e-mail-config-provider-page.h"
#include "e-mail-config-receiving-page.h"
#include "e-mail-config-sending-page.h"
#include "e-mail-config-summary-page.h"
#include "e-mail-config-welcome-page.h"

#include "em-folder-tree.h"

#include "e-mail-config-assistant.h"

#define E_MAIL_CONFIG_ASSISTANT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_CONFIG_ASSISTANT, EMailConfigAssistantPrivate))

/* GtkAssistant's back button label. */
#define BACK_BUTTON_LABEL N_("Go _Back")

typedef struct _AutoconfigContext AutoconfigContext;

struct _EMailConfigAssistantPrivate {
	EMailSession *session;
	ESource *identity_source;
	GPtrArray *account_sources;
	GPtrArray *transport_sources;
	EMailConfigServicePage *receiving_page;
	EMailConfigServicePage *sending_page;
	EMailConfigSummaryPage *summary_page;
	EMailConfigPage *identity_page;
	EMailConfigPage *lookup_page;
	GHashTable *visited_pages;
	gboolean auto_configured;

	/* GtkAssistant owns this. */
	GtkButton *back_button;  /* not referenced */
};

struct _AutoconfigContext {
	GtkAssistant *assistant;
	GCancellable *cancellable;
	GtkWidget *skip_button;  /* not referenced */
};

enum {
	PROP_0,
	PROP_ACCOUNT_BACKEND,
	PROP_ACCOUNT_SOURCE,
	PROP_IDENTITY_SOURCE,
	PROP_SESSION,
	PROP_TRANSPORT_BACKEND,
	PROP_TRANSPORT_SOURCE
};

/* XXX We implement EAlertSink but don't implement a custom submit_alert()
 *     method.  So any alert results in a pop-up message dialog, which is a
 *     fashion faux pas these days.  But it's only used when submitting the
 *     the newly-configured account fails, so should rarely be seen. */

G_DEFINE_TYPE_WITH_CODE (
	EMailConfigAssistant,
	e_mail_config_assistant,
	GTK_TYPE_ASSISTANT,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_ALERT_SINK, NULL)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

static void
autoconfig_skip_button_clicked_cb (GtkButton *button,
                                   GCancellable *cancellable)
{
	g_cancellable_cancel (cancellable);
}

static AutoconfigContext *
autoconfig_context_new (GtkAssistant *assistant)
{
	AutoconfigContext *context;
	const gchar *text;

	context = g_slice_new0 (AutoconfigContext);
	context->assistant = g_object_ref (assistant);
	context->cancellable = g_cancellable_new ();

	/* GtkAssistant sinks the floating button reference. */
	text = _("_Skip Lookup");
	context->skip_button = gtk_button_new_with_mnemonic (text);
	gtk_assistant_add_action_widget (
		context->assistant, context->skip_button);
	gtk_widget_show (context->skip_button);

	g_signal_connect_object (
		context->skip_button, "clicked",
		G_CALLBACK (autoconfig_skip_button_clicked_cb),
		context->cancellable, 0);

	return context;
}

static void
autoconfig_context_free (AutoconfigContext *context)
{
	gtk_assistant_remove_action_widget (
		context->assistant, context->skip_button);

	g_object_unref (context->assistant);
	g_object_unref (context->cancellable);

	g_slice_free (AutoconfigContext, context);
}

static gint
mail_config_assistant_provider_compare (gconstpointer data1,
                                        gconstpointer data2)
{
	const CamelProvider *provider1 = data1;
	const CamelProvider *provider2 = data2;

	/* The "none" provider comes first. */
	if (g_strcmp0 (provider1->protocol, "none") == 0)
		return -1;
	if (g_strcmp0 (provider2->protocol, "none") == 0)
		return 1;

	/* Then sort remote providers before local providers. */
	if (provider1->flags & CAMEL_PROVIDER_IS_REMOTE) {
		if (provider2->flags & CAMEL_PROVIDER_IS_REMOTE)
			return 0;
		else
			return -1;
	} else {
		if (provider2->flags & CAMEL_PROVIDER_IS_REMOTE)
			return 1;
		else
			return 0;
	}
}

static GList *
mail_config_assistant_list_providers (void)
{
	GList *list, *link;
	GQueue trash = G_QUEUE_INIT;

	list = camel_provider_list (TRUE);
	list = g_list_sort (list, mail_config_assistant_provider_compare);

	/* Keep only providers with a "mail" or "news" domain. */

	for (link = list; link != NULL; link = g_list_next (link)) {
		CamelProvider *provider = link->data;
		gboolean mail_or_news_domain;

		mail_or_news_domain =
			(g_strcmp0 (provider->domain, "mail") == 0) ||
			(g_strcmp0 (provider->domain, "news") == 0);

		if (mail_or_news_domain)
			continue;

		g_queue_push_tail (&trash, link);
	}

	while ((link = g_queue_pop_head (&trash)) != NULL)
		list = g_list_remove_link (list, link);

	return list;
}

static void
mail_config_assistant_notify_account_backend (EMailConfigServicePage *page,
                                              GParamSpec *pspec,
                                              EMailConfigAssistant *assistant)
{
	EMailConfigServiceBackend *backend;
	EMailConfigServicePage *sending_page;
	EMailConfigServicePageClass *page_class;
	CamelProvider *provider;

	backend = e_mail_config_service_page_get_active_backend (page);

	/* The Receiving Page combo box may not have an active item. */
	if (backend == NULL)
		goto notify;

	/* The Sending Page may not have been created yet. */
	if (assistant->priv->sending_page == NULL)
		goto notify;

	provider = e_mail_config_service_backend_get_provider (backend);

	/* XXX This should never fail, but the Camel macro below does
	 *     not check for NULL so better to malfunction than crash. */
	g_return_if_fail (provider != NULL);

	sending_page = assistant->priv->sending_page;
	page_class = E_MAIL_CONFIG_SERVICE_PAGE_GET_CLASS (sending_page);

	/* The Sending Page is invisible when the CamelProvider for the
	 * receiving type defines both a storage and transport service.
	 * This is common in CamelProviders for groupware products like
	 * Microsoft Exchange and Novell GroupWise. */
	if (CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (provider)) {
		backend = e_mail_config_service_page_lookup_backend (
			sending_page, provider->protocol);
		gtk_widget_hide (GTK_WIDGET (sending_page));
	} else {
		backend = e_mail_config_service_page_lookup_backend (
			sending_page, page_class->default_backend_name);
		gtk_widget_show (GTK_WIDGET (sending_page));
	}

	e_mail_config_service_page_set_active_backend (sending_page, backend);

notify:
	g_object_freeze_notify (G_OBJECT (assistant));

	g_object_notify (G_OBJECT (assistant), "account-backend");
	g_object_notify (G_OBJECT (assistant), "account-source");

	g_object_thaw_notify (G_OBJECT (assistant));
}

static void
mail_config_assistant_notify_transport_backend (EMailConfigServicePage *page,
                                                GParamSpec *pspec,
                                                EMailConfigAssistant *assistant)
{
	g_object_freeze_notify (G_OBJECT (assistant));

	g_object_notify (G_OBJECT (assistant), "transport-backend");
	g_object_notify (G_OBJECT (assistant), "transport-source");

	g_object_thaw_notify (G_OBJECT (assistant));
}

static void
mail_config_assistant_page_changed (EMailConfigPage *page,
                                    EMailConfigAssistant *assistant)
{
	gtk_assistant_set_page_complete (
		GTK_ASSISTANT (assistant), GTK_WIDGET (page),
		e_mail_config_page_check_complete (page));
}

static void
mail_config_assistant_autoconfigure_cb (GObject *source_object,
                                        GAsyncResult *result,
                                        gpointer user_data)
{
	EMailConfigAssistantPrivate *priv;
	AutoconfigContext *context;
	EMailAutoconfig *autoconfig;
	const gchar *email_address;
	gint n_pages, ii;
	GError *error = NULL;

	context = (AutoconfigContext *) user_data;
	priv = E_MAIL_CONFIG_ASSISTANT_GET_PRIVATE (context->assistant);

	autoconfig = e_mail_autoconfig_finish (result, &error);

	/* We don't really care about errors, we only capture the GError
	 * as a debugging aid.  If this doesn't work we simply proceed to
	 * the Receiving Email page. */
	if (error != NULL) {
		gtk_assistant_next_page (context->assistant);
		g_error_free (error);
		goto exit;
	}

	g_return_if_fail (E_IS_MAIL_AUTOCONFIG (autoconfig));

	/* Autoconfiguration worked!  Feed the results to the
	 * service pages and then skip to the Summary page. */

	/* For the summary page... */
	priv->auto_configured = TRUE;

	e_mail_config_service_page_auto_configure (
		priv->receiving_page, autoconfig);

	e_mail_config_service_page_auto_configure (
		priv->sending_page, autoconfig);

	/* Add these pages to the visited pages hash table to
	 * prevent calling e_mail_config_page_setup_defaults(). */

	g_hash_table_add (priv->visited_pages, priv->receiving_page);
	g_hash_table_add (priv->visited_pages, priv->sending_page);

	/* Also set the initial display name to the email address
	 * given so the user can just click past the Summary page. */
	email_address = e_mail_autoconfig_get_email_address (autoconfig);
	e_source_set_display_name (priv->identity_source, email_address);

	/* Go to the next page (Receiving Email) before skipping to the
	 * Summary Page to get it into GtkAssistant visited page history.
	 * We want the back button to return to Receiving Email. */
	gtk_assistant_next_page (context->assistant);

	/* XXX Can't find a better way to learn the page number of
	 *     the summary page.  Oh my god this API is horrible. */
	n_pages = gtk_assistant_get_n_pages (context->assistant);
	for (ii = 0; ii < n_pages; ii++) {
		GtkWidget *page;

		page = gtk_assistant_get_nth_page (context->assistant, ii);
		if (E_IS_MAIL_CONFIG_SUMMARY_PAGE (page))
			break;
	}

	g_warn_if_fail (ii < n_pages);
	gtk_assistant_set_current_page (context->assistant, ii);

exit:
	/* Set the page invisible so we never revisit it. */
	gtk_widget_set_visible (GTK_WIDGET (priv->lookup_page), FALSE);

	autoconfig_context_free (context);
}

static gboolean
mail_config_assistant_provider_page_visible (GBinding *binding,
                                             const GValue *source_value,
                                             GValue *target_value,
                                             gpointer unused)
{
	EMailConfigServiceBackend *active_backend;
	EMailConfigServiceBackend *page_backend;
	EMailConfigProviderPage *page;
	GObject *target_object;
	gboolean visible;

	target_object = g_binding_get_target (binding);
	page = E_MAIL_CONFIG_PROVIDER_PAGE (target_object);
	page_backend = e_mail_config_provider_page_get_backend (page);

	active_backend = g_value_get_object (source_value);
	visible = (page_backend == active_backend);
	g_value_set_boolean (target_value, visible);

	return TRUE;
}

static void
mail_config_assistant_select_account_node (const gchar *account_uid)
{
	EShell *shell;
	EShellWindow *shell_window;
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	EMFolderTree *folder_tree = NULL;
	GtkWindow *active_window;
	const gchar *active_view;

	g_return_if_fail (account_uid != NULL);

	shell = e_shell_get_default ();
	active_window = e_shell_get_active_window (shell);

	if (!E_IS_SHELL_WINDOW (active_window))
		return;

	shell_window = E_SHELL_WINDOW (active_window);
	active_view = e_shell_window_get_active_view (shell_window);

	if (g_strcmp0 (active_view, "mail") != 0)
		return;

	shell_view = e_shell_window_get_shell_view (shell_window, "mail");

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);

	em_folder_tree_select_store_when_added (folder_tree, account_uid);

	g_object_unref (folder_tree);

}

static void
mail_config_assistant_close_cb (GObject *object,
                                GAsyncResult *result,
                                gpointer user_data)
{
	EMailConfigAssistant *assistant;
	GdkWindow *gdk_window;
	GError *error = NULL;

	assistant = E_MAIL_CONFIG_ASSISTANT (object);

	/* Set the cursor back to normal. */
	gdk_window = gtk_widget_get_window (GTK_WIDGET (assistant));
	gdk_window_set_cursor (gdk_window, NULL);

	/* Allow user interaction with window content. */
	gtk_widget_set_sensitive (GTK_WIDGET (assistant), TRUE);

	e_mail_config_assistant_commit_finish (assistant, result, &error);

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);

	} else if (error != NULL) {
		e_alert_submit (
			E_ALERT_SINK (assistant),
			"system:simple-error",
			error->message, NULL);
		g_error_free (error);

	} else {
		ESource *source;

		source = e_mail_config_assistant_get_account_source (assistant);
		if (source != NULL) {
			const gchar *uid;

			uid = e_source_get_uid (source);
			mail_config_assistant_select_account_node (uid);
		}

		gtk_widget_destroy (GTK_WIDGET (assistant));
	}
}

static void
mail_config_assistant_find_back_button_cb (GtkWidget *widget,
                                           gpointer user_data)
{
	EMailConfigAssistant *assistant;

	assistant = E_MAIL_CONFIG_ASSISTANT (user_data);

	if (GTK_IS_BUTTON (widget)) {
		GtkButton *button;
		const gchar *gtk_label;
		const gchar *our_label;

		button = GTK_BUTTON (widget);

		/* XXX The gtkassistant.ui file assigns the back button
		 *     an ID of "back", but I don't think we have access
		 *     to it from here.  I guess just compare by label,
		 *     and hope our translation matches GTK's.  Yuck. */

		gtk_label = gtk_button_get_label (button);
		our_label = gettext (BACK_BUTTON_LABEL);

		if (g_strcmp0 (gtk_label, our_label) == 0)
			assistant->priv->back_button = button;

	} else if (GTK_IS_CONTAINER (widget)) {
		gtk_container_forall (
			GTK_CONTAINER (widget),
			mail_config_assistant_find_back_button_cb,
			assistant);
	}
}

static void
mail_config_assistant_set_session (EMailConfigAssistant *assistant,
                                   EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (assistant->priv->session == NULL);

	assistant->priv->session = g_object_ref (session);
}

static void
mail_config_assistant_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			mail_config_assistant_set_session (
				E_MAIL_CONFIG_ASSISTANT (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_assistant_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACCOUNT_BACKEND:
			g_value_set_object (
				value,
				e_mail_config_assistant_get_account_backend (
				E_MAIL_CONFIG_ASSISTANT (object)));
			return;

		case PROP_ACCOUNT_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_assistant_get_account_source (
				E_MAIL_CONFIG_ASSISTANT (object)));
			return;

		case PROP_IDENTITY_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_assistant_get_identity_source (
				E_MAIL_CONFIG_ASSISTANT (object)));
			return;

		case PROP_SESSION:
			g_value_set_object (
				value,
				e_mail_config_assistant_get_session (
				E_MAIL_CONFIG_ASSISTANT (object)));
			return;

		case PROP_TRANSPORT_BACKEND:
			g_value_set_object (
				value,
				e_mail_config_assistant_get_transport_backend (
				E_MAIL_CONFIG_ASSISTANT (object)));
			return;

		case PROP_TRANSPORT_SOURCE:
			g_value_set_object (
				value,
				e_mail_config_assistant_get_transport_source (
				E_MAIL_CONFIG_ASSISTANT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_assistant_dispose (GObject *object)
{
	EMailConfigAssistantPrivate *priv;

	priv = E_MAIL_CONFIG_ASSISTANT_GET_PRIVATE (object);

	if (priv->session != NULL) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (priv->identity_source != NULL) {
		g_object_unref (priv->identity_source);
		priv->identity_source = NULL;
	}

	if (priv->receiving_page != NULL) {
		g_object_unref (priv->receiving_page);
		priv->receiving_page = NULL;
	}

	if (priv->sending_page != NULL) {
		g_object_unref (priv->sending_page);
		priv->sending_page = NULL;
	}

	if (priv->summary_page != NULL) {
		g_object_unref (priv->summary_page);
		priv->summary_page = NULL;
	}

	if (priv->lookup_page != NULL) {
		g_object_unref (priv->lookup_page);
		priv->lookup_page = NULL;
	}

	if (priv->identity_page != NULL) {
		g_object_unref (priv->identity_page);
		priv->identity_page = NULL;
	}

	g_ptr_array_set_size (priv->account_sources, 0);
	g_ptr_array_set_size (priv->transport_sources, 0);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_assistant_parent_class)->
		dispose (object);
}

static void
mail_config_assistant_finalize (GObject *object)
{
	EMailConfigAssistantPrivate *priv;

	priv = E_MAIL_CONFIG_ASSISTANT_GET_PRIVATE (object);

	g_ptr_array_free (priv->account_sources, TRUE);
	g_ptr_array_free (priv->transport_sources, TRUE);

	g_hash_table_destroy (priv->visited_pages);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_config_assistant_parent_class)->
		finalize (object);
}

static void
mail_config_assistant_constructed (GObject *object)
{
	EMailConfigAssistant *assistant;
	ESource *identity_source;
	ESourceRegistry *registry;
	ESourceExtension *extension;
	ESourceMailComposition *mail_composition_extension;
	ESourceMailIdentity *mail_identity_extension;
	ESourceMailSubmission *mail_submission_extension;
	EMailSession *session;
	EMailConfigPage *page;
	GtkWidget *autodiscover_check;
	GList *list, *link;
	const gchar *extension_name;
	const gchar *title;
	GtkRequisition requisition;
	GSList *children = NULL;
	gint ii, npages;

	assistant = E_MAIL_CONFIG_ASSISTANT (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_assistant_parent_class)->constructed (object);

	title = _("Evolution Account Assistant");
	gtk_window_set_title (GTK_WINDOW (assistant), title);
	gtk_window_set_position (GTK_WINDOW (assistant), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size (GTK_WINDOW (assistant), 640, 480);

	session = e_mail_config_assistant_get_session (assistant);
	registry = e_mail_session_get_registry (session);

	/* XXX Locate the GtkAssistant's internal "Go Back" button so
	 *     we can temporarily rename it for autoconfigure results.
	 *     Walking the container like this is an extremely naughty
	 *     and brittle hack, but GtkAssistant does not provide API
	 *     to access it directly. */
	gtk_container_forall (
		GTK_CONTAINER (assistant),
		mail_config_assistant_find_back_button_cb,
		assistant);

	/* Configure a new identity source. */

	identity_source = e_source_new (NULL, NULL, NULL);
	assistant->priv->identity_source = identity_source;
	session = e_mail_config_assistant_get_session (assistant);

	extension_name = E_SOURCE_EXTENSION_MAIL_COMPOSITION;
	extension = e_source_get_extension (identity_source, extension_name);
	mail_composition_extension = E_SOURCE_MAIL_COMPOSITION (extension);

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	extension = e_source_get_extension (identity_source, extension_name);
	mail_identity_extension = E_SOURCE_MAIL_IDENTITY (extension);

	extension_name = E_SOURCE_EXTENSION_MAIL_SUBMISSION;
	extension = e_source_get_extension (identity_source, extension_name);
	mail_submission_extension = E_SOURCE_MAIL_SUBMISSION (extension);

	e_source_mail_identity_set_name (mail_identity_extension, g_get_real_name ());

	e_source_mail_composition_set_drafts_folder (
		mail_composition_extension,
		e_mail_session_get_local_folder_uri (
		session, E_MAIL_LOCAL_FOLDER_DRAFTS));

	e_source_mail_composition_set_templates_folder (
		mail_composition_extension,
		e_mail_session_get_local_folder_uri (
		session, E_MAIL_LOCAL_FOLDER_TEMPLATES));

	e_source_mail_submission_set_sent_folder (
		mail_submission_extension,
		e_mail_session_get_local_folder_uri (
		session, E_MAIL_LOCAL_FOLDER_SENT));

	gtk_widget_get_preferred_size (GTK_WIDGET (assistant), &requisition, NULL);
	requisition.width += 2 * 12;
	requisition.height += 2 * 12;

	/*** Welcome Page ***/

	page = e_mail_config_welcome_page_new ();
	e_mail_config_assistant_add_page (assistant, page);

	/*** Identity Page ***/

	page = e_mail_config_identity_page_new (registry, identity_source);
	e_mail_config_identity_page_set_show_account_info (
		E_MAIL_CONFIG_IDENTITY_PAGE (page), FALSE);
	e_mail_config_identity_page_set_show_signatures (
		E_MAIL_CONFIG_IDENTITY_PAGE (page), FALSE);
	e_mail_config_identity_page_set_show_autodiscover_check (
		E_MAIL_CONFIG_IDENTITY_PAGE (page), TRUE);
	autodiscover_check = e_mail_config_identity_page_get_autodiscover_check (
		E_MAIL_CONFIG_IDENTITY_PAGE (page));
	e_mail_config_assistant_add_page (assistant, page);
	assistant->priv->identity_page = g_object_ref (page);

	/*** Lookup Page ***/

	page = e_mail_config_lookup_page_new ();
	e_mail_config_assistant_add_page (assistant, page);
	assistant->priv->lookup_page = g_object_ref (page);

	e_binding_bind_property (
		autodiscover_check, "active",
		page, "visible",
		G_BINDING_SYNC_CREATE);

	/*** Receiving Page ***/

	page = e_mail_config_receiving_page_new (registry);
	e_mail_config_assistant_add_page (assistant, page);
	assistant->priv->receiving_page = g_object_ref (page);

	e_binding_bind_object_text_property (
		mail_identity_extension, "address",
		page, "email-address",
		G_BINDING_SYNC_CREATE);

	e_signal_connect_notify (
		page, "notify::active-backend",
		G_CALLBACK (mail_config_assistant_notify_account_backend),
		assistant);

	/*** Receiving Options (multiple) ***/

	/* Populate the Receiving Email page while at the same time
	 * adding a Receiving Options page for each account type. */

	list = mail_config_assistant_list_providers ();

	for (link = list; link != NULL; link = g_list_next (link)) {
		EMailConfigServiceBackend *backend;
		CamelProvider *provider = link->data;
		ESourceBackend *backend_extension;
		ESource *scratch_source;
		const gchar *backend_name;

		if (provider->object_types[CAMEL_PROVIDER_STORE] == 0)
			continue;

		/* ESource uses "backend_name" and CamelProvider
		 * uses "protocol", but the terms are synonymous. */
		backend_name = provider->protocol;

		scratch_source = e_source_new (NULL, NULL, NULL);
		backend_extension = e_source_get_extension (
			scratch_source, E_SOURCE_EXTENSION_MAIL_ACCOUNT);
		e_source_backend_set_backend_name (
			backend_extension, backend_name);

		/* Keep display names synchronized. */
		e_binding_bind_property (
			identity_source, "display-name",
			scratch_source, "display-name",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);

		/* We always pass NULL for the collection argument.
		 * The backend generates its own scratch collection
		 * source if implements the new_collection() method. */
		backend = e_mail_config_service_page_add_scratch_source (
			assistant->priv->receiving_page, scratch_source, NULL);

		g_object_unref (scratch_source);

		page = e_mail_config_provider_page_new (backend);

		/* Note: We exclude this page if it has no options,
		 *       but we don't know that until we create it. */
		if (e_mail_config_provider_page_is_empty (
				E_MAIL_CONFIG_PROVIDER_PAGE (page))) {
			g_object_unref (g_object_ref_sink (page));
			continue;
		} else {
			e_mail_config_assistant_add_page (assistant, page);
		}

		/* Each Receiving Options page is only visible when its
		 * service backend is active on the Receiving Email page. */
		e_binding_bind_property_full (
			assistant->priv->receiving_page, "active-backend",
			page, "visible",
			G_BINDING_SYNC_CREATE,
			mail_config_assistant_provider_page_visible,
			NULL,
			NULL, (GDestroyNotify) NULL);
	}

	g_list_free (list);

	/*** Sending Page ***/

	page = e_mail_config_sending_page_new (registry);
	e_mail_config_assistant_add_page (assistant, page);
	assistant->priv->sending_page = g_object_ref (page);

	e_binding_bind_object_text_property (
		mail_identity_extension, "address",
		page, "email-address",
		G_BINDING_SYNC_CREATE);

	e_signal_connect_notify (
		page, "notify::active-backend",
		G_CALLBACK (mail_config_assistant_notify_transport_backend),
		assistant);

	list = mail_config_assistant_list_providers ();

	for (link = list; link != NULL; link = g_list_next (link)) {
		CamelProvider *provider = link->data;
		ESourceBackend *backend_extension;
		ESource *scratch_source;
		const gchar *backend_name;

		if (provider->object_types[CAMEL_PROVIDER_TRANSPORT] == 0)
			continue;

		/* ESource uses "backend_name" and CamelProvider
		 * uses "protocol", but the terms are synonymous. */
		backend_name = provider->protocol;

		scratch_source = e_source_new (NULL, NULL, NULL);
		backend_extension = e_source_get_extension (
			scratch_source, E_SOURCE_EXTENSION_MAIL_TRANSPORT);
		e_source_backend_set_backend_name (
			backend_extension, backend_name);

		/* Keep display names synchronized. */
		e_binding_bind_property (
			identity_source, "display-name",
			scratch_source, "display-name",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);

		/* We always pass NULL for the collection argument.
		 * The backend generates its own scratch collection
		 * source if implements the new_collection() method. */
		e_mail_config_service_page_add_scratch_source (
			assistant->priv->sending_page, scratch_source, NULL);

		g_object_unref (scratch_source);
	}

	g_list_free (list);

	/*** Summary Page ***/

	page = e_mail_config_summary_page_new ();
	e_mail_config_assistant_add_page (assistant, page);
	assistant->priv->summary_page = g_object_ref (page);

	e_binding_bind_property (
		assistant, "account-backend",
		page, "account-backend",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		assistant, "identity-source",
		page, "identity-source",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		assistant, "transport-backend",
		page, "transport-backend",
		G_BINDING_SYNC_CREATE);

	/*** Confirm Page ***/

	page = e_mail_config_confirm_page_new ();
	e_mail_config_assistant_add_page (assistant, page);

	e_extensible_load_extensions (E_EXTENSIBLE (assistant));

	npages = gtk_assistant_get_n_pages (GTK_ASSISTANT (assistant));
	for (ii = 0; ii < npages; ii++) {
		children = g_slist_prepend (children, gtk_assistant_get_nth_page (GTK_ASSISTANT (assistant), ii));
	}

	e_util_resize_window_for_screen (GTK_WINDOW (assistant), requisition.width, requisition.height, children);

	g_slist_free (children);
}

static void
mail_config_assistant_remove (GtkContainer *container,
                              GtkWidget *widget)
{
	if (E_IS_MAIL_CONFIG_PAGE (widget))
		g_signal_handlers_disconnect_by_func (
			widget, mail_config_assistant_page_changed,
			E_MAIL_CONFIG_ASSISTANT (container));

	/* Chain up to parent's remove() method. */
	GTK_CONTAINER_CLASS (e_mail_config_assistant_parent_class)->
		remove (container, widget);
}

static void
mail_config_assistant_prepare (GtkAssistant *assistant,
                               GtkWidget *page)
{
	EMailConfigAssistantPrivate *priv;
	gboolean first_visit = FALSE;

	priv = E_MAIL_CONFIG_ASSISTANT_GET_PRIVATE (assistant);

	/* Only setup defaults the first time a page is visited. */
	if (!g_hash_table_contains (priv->visited_pages, page)) {
		if (E_IS_MAIL_CONFIG_PAGE (page))
			e_mail_config_page_setup_defaults (
				E_MAIL_CONFIG_PAGE (page));
		g_hash_table_add (priv->visited_pages, page);
		first_visit = TRUE;
	}

	/* Are we viewing autoconfiguration results?  If so, temporarily
	 * rename the back button to clarify that account details can be
	 * revised.  Otherwise reset the button to its original label. */
	if (priv->back_button != NULL) {
		gboolean auto_configure_results;
		const gchar *label;

		auto_configure_results =
			E_IS_MAIL_CONFIG_SUMMARY_PAGE (page) &&
			priv->auto_configured && first_visit;

		if (auto_configure_results)
			label = _("_Revise Details");
		else
			label = gettext (BACK_BUTTON_LABEL);

		gtk_button_set_label (priv->back_button, label);
	}

	if (E_IS_MAIL_CONFIG_LOOKUP_PAGE (page)) {
		AutoconfigContext *context;
		ESource *source;
		ESourceRegistry *registry;
		ESourceMailIdentity *extension;
		const gchar *email_address;
		const gchar *extension_name;

		registry = e_mail_session_get_registry (priv->session);

		source = priv->identity_source;
		extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
		extension = e_source_get_extension (source, extension_name);
		email_address = e_source_mail_identity_get_address (extension);

		context = autoconfig_context_new (assistant);

		e_mail_autoconfig_new (
			registry,
			email_address,
			G_PRIORITY_DEFAULT,
			context->cancellable,
			mail_config_assistant_autoconfigure_cb,
			context);
	}

	if (E_IS_MAIL_CONFIG_RECEIVING_PAGE (page) && first_visit) {
		ESource *source;
		ESourceMailIdentity *extension;
		const gchar *email_address;
		const gchar *extension_name;

		/* Use the email address from the Identity Page as
		 * the initial display name, so in case we have to
		 * query a remote mail server, the password prompt
		 * will have a more meaningful description. */

		source = priv->identity_source;
		extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
		extension = e_source_get_extension (source, extension_name);
		email_address = e_source_mail_identity_get_address (extension);
		e_source_set_display_name (source, email_address);
	}

	if (first_visit && (
	    E_IS_MAIL_CONFIG_LOOKUP_PAGE (page) ||
	    E_IS_MAIL_CONFIG_RECEIVING_PAGE (page)))
		e_mail_config_identity_page_set_show_autodiscover_check (
			E_MAIL_CONFIG_IDENTITY_PAGE (priv->identity_page), FALSE);
}

static void
mail_config_assistant_close (GtkAssistant *assistant)
{
	GdkCursor *gdk_cursor;
	GdkWindow *gdk_window;

	/* Do not chain up.  GtkAssistant does not implement this method. */

	/* Make the cursor appear busy. */
	gdk_cursor = gdk_cursor_new (GDK_WATCH);
	gdk_window = gtk_widget_get_window (GTK_WIDGET (assistant));
	gdk_window_set_cursor (gdk_window, gdk_cursor);
	g_object_unref (gdk_cursor);

	/* Prevent user interaction with window content. */
	gtk_widget_set_sensitive (GTK_WIDGET (assistant), FALSE);

	/* XXX This operation is not cancellable. */
	e_mail_config_assistant_commit (
		E_MAIL_CONFIG_ASSISTANT (assistant),
		NULL, mail_config_assistant_close_cb, NULL);
}

static void
mail_config_assistant_cancel (GtkAssistant *assistant)
{
	/* Do not chain up.  GtkAssistant does not implement this method. */

	gtk_widget_destroy (GTK_WIDGET (assistant));
}

static void
e_mail_config_assistant_class_init (EMailConfigAssistantClass *class)
{
	GObjectClass *object_class;
	GtkContainerClass *container_class;
	GtkAssistantClass *assistant_class;

	g_type_class_add_private (class, sizeof (EMailConfigAssistantPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_assistant_set_property;
	object_class->get_property = mail_config_assistant_get_property;
	object_class->dispose = mail_config_assistant_dispose;
	object_class->finalize = mail_config_assistant_finalize;
	object_class->constructed = mail_config_assistant_constructed;

	container_class = GTK_CONTAINER_CLASS (class);
	container_class->remove = mail_config_assistant_remove;

	assistant_class = GTK_ASSISTANT_CLASS (class);
	assistant_class->prepare = mail_config_assistant_prepare;
	assistant_class->close = mail_config_assistant_close;
	assistant_class->cancel = mail_config_assistant_cancel;

	g_object_class_install_property (
		object_class,
		PROP_ACCOUNT_BACKEND,
		g_param_spec_object (
			"account-backend",
			"Account Backend",
			"Active mail account service backend",
			E_TYPE_MAIL_CONFIG_SERVICE_BACKEND,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_ACCOUNT_SOURCE,
		g_param_spec_object (
			"account-source",
			"Account Source",
			"Mail account source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_IDENTITY_SOURCE,
		g_param_spec_object (
			"identity-source",
			"Identity Source",
			"Mail identity source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READABLE |
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

	g_object_class_install_property (
		object_class,
		PROP_TRANSPORT_BACKEND,
		g_param_spec_object (
			"transport-backend",
			"Transport Backend",
			"Active mail transport service backend",
			E_TYPE_MAIL_CONFIG_SERVICE_BACKEND,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TRANSPORT_SOURCE,
		g_param_spec_object (
			"transport-source",
			"Transport Source",
			"Mail transport source being edited",
			E_TYPE_SOURCE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_config_assistant_init (EMailConfigAssistant *assistant)
{
	GObject *action_area;
	GtkBuilder *builder;

	builder = gtk_builder_new ();
	action_area = gtk_buildable_get_internal_child (
		GTK_BUILDABLE (assistant), builder, "action_area");
	if (action_area)
		gtk_container_set_border_width (GTK_CONTAINER (action_area), 12);
	g_object_unref (builder);

	assistant->priv = E_MAIL_CONFIG_ASSISTANT_GET_PRIVATE (assistant);

	assistant->priv->account_sources =
		g_ptr_array_new_with_free_func (
		(GDestroyNotify) g_object_unref);

	assistant->priv->transport_sources =
		g_ptr_array_new_with_free_func (
		(GDestroyNotify) g_object_unref);

	assistant->priv->visited_pages = g_hash_table_new (NULL, NULL);
}

GtkWidget *
e_mail_config_assistant_new (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return g_object_new (
		E_TYPE_MAIL_CONFIG_ASSISTANT,
		"session", session, NULL);
}

EMailSession *
e_mail_config_assistant_get_session (EMailConfigAssistant *assistant)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_ASSISTANT (assistant), NULL);

	return assistant->priv->session;
}

EMailConfigServiceBackend *
e_mail_config_assistant_get_account_backend (EMailConfigAssistant *assistant)
{
	EMailConfigServicePage *page;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_ASSISTANT (assistant), NULL);

	page = assistant->priv->receiving_page;

	return e_mail_config_service_page_get_active_backend (page);
}

ESource *
e_mail_config_assistant_get_account_source (EMailConfigAssistant *assistant)
{
	EMailConfigServiceBackend *backend;
	ESource *source = NULL;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_ASSISTANT (assistant), NULL);

	backend = e_mail_config_assistant_get_account_backend (assistant);

	if (backend != NULL)
		source = e_mail_config_service_backend_get_source (backend);

	return source;
}

ESource *
e_mail_config_assistant_get_identity_source (EMailConfigAssistant *assistant)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_ASSISTANT (assistant), NULL);

	return assistant->priv->identity_source;
}

EMailConfigServiceBackend *
e_mail_config_assistant_get_transport_backend (EMailConfigAssistant *assistant)
{
	EMailConfigServicePage *page;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_ASSISTANT (assistant), NULL);

	page = assistant->priv->sending_page;

	return e_mail_config_service_page_get_active_backend (page);
}

ESource *
e_mail_config_assistant_get_transport_source (EMailConfigAssistant *assistant)
{
	EMailConfigServiceBackend *backend;
	ESource *source = NULL;

	g_return_val_if_fail (E_IS_MAIL_CONFIG_ASSISTANT (assistant), NULL);

	backend = e_mail_config_assistant_get_transport_backend (assistant);

	if (backend != NULL)
		source = e_mail_config_service_backend_get_source (backend);

	return source;
}

void
e_mail_config_assistant_add_page (EMailConfigAssistant *assistant,
                                  EMailConfigPage *page)
{
	EMailConfigPageInterface *page_interface;
	GtkAssistantPageType page_type;
	GtkWidget *page_widget;
	gint n_pages, position;
	const gchar *page_title;
	gboolean complete;

	g_return_if_fail (E_IS_MAIL_CONFIG_ASSISTANT (assistant));
	g_return_if_fail (E_IS_MAIL_CONFIG_PAGE (page));

	page_widget = GTK_WIDGET (page);
	page_interface = E_MAIL_CONFIG_PAGE_GET_INTERFACE (page);
	page_type = page_interface->page_type;
	page_title = page_interface->title;

	/* Determine the position to insert the page. */
	n_pages = gtk_assistant_get_n_pages (GTK_ASSISTANT (assistant));
	for (position = 0; position < n_pages; position++) {
		GtkWidget *nth_page;

		nth_page = gtk_assistant_get_nth_page (
			GTK_ASSISTANT (assistant), position);
		if (e_mail_config_page_compare (page_widget, nth_page) < 0)
			break;
	}

	gtk_widget_show (page_widget);

	/* Some pages can be clicked through unchanged. */
	complete = e_mail_config_page_check_complete (page);

	gtk_assistant_insert_page (
		GTK_ASSISTANT (assistant), page_widget, position);
	gtk_assistant_set_page_type (
		GTK_ASSISTANT (assistant), page_widget, page_type);
	gtk_assistant_set_page_title (
		GTK_ASSISTANT (assistant), page_widget, page_title);
	gtk_assistant_set_page_complete (
		GTK_ASSISTANT (assistant), page_widget, complete);

	/* XXX GtkAssistant has no equivalent to GtkNotebook's
	 *     "page-added" and "page-removed" signals.  Fortunately
	 *     removing a page does trigger GtkContainer::remove, so
	 *     we can override that method and disconnect our signal
	 *     handler before chaining up.  But I don't see any way
	 *     for a subclass to intercept GtkAssistant pages being
	 *     added, so we have to connect our signal handler here.
	 *     Not really an issue, I'm just being pedantic. */

	g_signal_connect (
		page, "changed",
		G_CALLBACK (mail_config_assistant_page_changed),
		assistant);
}

/********************* e_mail_config_assistant_commit() **********************/

static void
mail_config_assistant_commit_cb (GObject *object,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	GSimpleAsyncResult *simple;
	GError *error = NULL;

	simple = G_SIMPLE_ASYNC_RESULT (user_data);

	e_source_registry_create_sources_finish (
		E_SOURCE_REGISTRY (object), result, &error);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);

	g_simple_async_result_complete (simple);

	g_object_unref (simple);
}

void
e_mail_config_assistant_commit (EMailConfigAssistant *assistant,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
	EMailConfigServiceBackend *backend;
	GSimpleAsyncResult *simple;
	ESourceRegistry *registry;
	EMailSession *session;
	ESource *source;
	GQueue *queue;
	gint n_pages, ii;

	g_return_if_fail (E_IS_MAIL_CONFIG_ASSISTANT (assistant));

	session = e_mail_config_assistant_get_session (assistant);
	registry = e_mail_session_get_registry (session);

	queue = g_queue_new ();

	/* Queue the collection data source if one is defined. */
	backend = e_mail_config_assistant_get_account_backend (assistant);
	source = e_mail_config_service_backend_get_collection (backend);
	if (source != NULL)
		g_queue_push_tail (queue, g_object_ref (source));

	/* Queue the mail-related data sources for the account. */
	source = e_mail_config_assistant_get_account_source (assistant);
	if (source != NULL)
		g_queue_push_tail (queue, g_object_ref (source));
	source = e_mail_config_assistant_get_identity_source (assistant);
	if (source != NULL)
		g_queue_push_tail (queue, g_object_ref (source));
	source = e_mail_config_assistant_get_transport_source (assistant);
	if (source != NULL)
		g_queue_push_tail (queue, g_object_ref (source));

	n_pages = gtk_assistant_get_n_pages (GTK_ASSISTANT (assistant));

	/* Tell all EMailConfigPages to commit their UI state to their
	 * scratch ESources and push any additional data sources on to
	 * the given source queue, such as calendars or address books
	 * to be bundled with the mail account. */
	for (ii = 0; ii < n_pages; ii++) {
		GtkWidget *widget;

		widget = gtk_assistant_get_nth_page (
			GTK_ASSISTANT (assistant), ii);

		if (E_IS_MAIL_CONFIG_PAGE (widget)) {
			EMailConfigPage *page;
			page = E_MAIL_CONFIG_PAGE (widget);
			e_mail_config_page_commit_changes (page, queue);
		}
	}

	simple = g_simple_async_result_new (
		G_OBJECT (assistant), callback, user_data,
		e_mail_config_assistant_commit);

	e_source_registry_create_sources (
		registry, g_queue_peek_head_link (queue),
		cancellable, mail_config_assistant_commit_cb, simple);

	g_queue_free_full (queue, (GDestroyNotify) g_object_unref);
}

gboolean
e_mail_config_assistant_commit_finish (EMailConfigAssistant *assistant,
                                       GAsyncResult *result,
                                       GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (assistant),
		e_mail_config_assistant_commit), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	/* Assume success unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
}

