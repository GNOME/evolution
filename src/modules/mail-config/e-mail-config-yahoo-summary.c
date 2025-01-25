/*
 * e-mail-config-yahoo-summary.c
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

#include <mail/e-mail-config-summary-page.h>

#include "e-mail-config-yahoo-summary.h"

struct _EMailConfigYahooSummaryPrivate {
	ESource *collection_source;

	/* Widgets (not referenced) */
	GtkWidget *calendar_toggle;
	GtkWidget *contacts_toggle;

	gboolean applicable;
};

enum {
	PROP_0,
	PROP_APPLICABLE
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EMailConfigYahooSummary, e_mail_config_yahoo_summary, E_TYPE_EXTENSION, 0,
	G_ADD_PRIVATE_DYNAMIC (EMailConfigYahooSummary))

static EMailConfigSummaryPage *
mail_config_yahoo_summary_get_summary_page (EMailConfigYahooSummary *extension)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	return E_MAIL_CONFIG_SUMMARY_PAGE (extensible);
}

static gboolean
mail_config_yahoo_summary_is_applicable (EMailConfigSummaryPage *page)
{
	ESource *source;
	const gchar *extension_name;
	const gchar *host = NULL;

	/* FIXME We should tie this into EMailAutoconfig to avoid
	 *       hard-coding Yahoo domain names.  Maybe retain the
	 *       <emailProvider id="..."> it matched so we can just
	 *       check for, in this case, "yahoo.com".
	 *
	 *       Source:
	 *       http://api.gnome.org/evolution/autoconfig/1.1/yahoo.com
	 */

	source = e_mail_config_summary_page_get_account_source (page);

	extension_name = E_SOURCE_EXTENSION_AUTHENTICATION;
	if (e_source_has_extension (source, extension_name)) {
		ESourceAuthentication *extension;
		extension = e_source_get_extension (source, extension_name);
		host = e_source_authentication_get_host (extension);
	}

	if (host == NULL)
		return FALSE;

	if (e_util_host_is_in_domain (host, "yahoo.com"))
		return TRUE;

	if (e_util_host_is_in_domain (host, "ymail.com"))
		return TRUE;

	if (e_util_host_is_in_domain (host, "rocketmail.com"))
		return TRUE;

	return FALSE;
}

static void
mail_config_yahoo_summary_refresh_cb (EMailConfigSummaryPage *page,
                                      EMailConfigYahooSummary *extension)
{
	extension->priv->applicable =
		mail_config_yahoo_summary_is_applicable (page);

	g_object_notify (G_OBJECT (extension), "applicable");
}

static void
mail_config_yahoo_summary_commit_changes_cb (EMailConfigSummaryPage *page,
                                             GQueue *source_queue,
                                             EMailConfigYahooSummary *extension)
{
	ESource *source;
	ESourceCollection *collection_extension;
	ESourceMailIdentity *identity_extension;
	ESourceAuthentication *auth_extension;
	GtkToggleButton *toggle_button;
	GList *head, *link;
	const gchar *address;
	const gchar *parent_uid;
	const gchar *display_name;
	const gchar *extension_name;
	gboolean calendar_active;
	gboolean contacts_active;

	/* If this is not a Yahoo! account, do nothing (obviously). */
	if (!e_mail_config_yahoo_summary_get_applicable (extension))
		return;

	toggle_button = GTK_TOGGLE_BUTTON (extension->priv->calendar_toggle);
	calendar_active = gtk_toggle_button_get_active (toggle_button);

	toggle_button = GTK_TOGGLE_BUTTON (extension->priv->contacts_toggle);
	contacts_active = gtk_toggle_button_get_active (toggle_button);

	/* If the user declined to add a Calendar, do nothing. */
	if (!calendar_active && !contacts_active)
		return;

	source = e_mail_config_summary_page_get_identity_source (page);
	display_name = e_source_get_display_name (source);

	/* The collection identity is the user's email address. */
	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	identity_extension = e_source_get_extension (source, extension_name);
	address = e_source_mail_identity_get_address (identity_extension);

	source = extension->priv->collection_source;
	e_source_set_display_name (source, display_name);

	extension_name = E_SOURCE_EXTENSION_COLLECTION;
	collection_extension = e_source_get_extension (source, extension_name);
	e_source_collection_set_identity (collection_extension, address);

	/* Always create the Authentication extension, thus the collection source
	   can be used for the credentials prompt. */
	auth_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);
	e_source_authentication_set_host (auth_extension, "");
	e_source_authentication_set_user (auth_extension, address);

	/* All queued sources become children of the collection source. */
	parent_uid = e_source_get_uid (source);
	head = g_queue_peek_head_link (source_queue);
	for (link = head; link != NULL; link = g_list_next (link)) {
		ESource *child = E_SOURCE (link->data);

		e_source_set_parent (child, parent_uid);

		/* Derive authentication method from the Mail Account */
		if (e_source_has_extension (child, E_SOURCE_EXTENSION_AUTHENTICATION) &&
		    e_source_has_extension (child, E_SOURCE_EXTENSION_MAIL_ACCOUNT)) {
			ESourceAuthentication *child_auth_extension;
			const gchar *auth_method;

			child_auth_extension = e_source_get_extension (child, E_SOURCE_EXTENSION_AUTHENTICATION);
			auth_method = e_source_authentication_get_method (child_auth_extension);
			e_source_authentication_set_method (auth_extension, auth_method);
		}
	}

	/* Push this AFTER iterating over the source queue. */
	g_queue_push_head (source_queue, g_object_ref (source));

	/* The "yahoo-backend" module in E-D-S will handle the rest. */
}

static void
mail_config_yahoo_summary_get_property (GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_APPLICABLE:
			g_value_set_boolean (
				value,
				e_mail_config_yahoo_summary_get_applicable (
				E_MAIL_CONFIG_YAHOO_SUMMARY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_yahoo_summary_dispose (GObject *object)
{
	EMailConfigYahooSummary *self = E_MAIL_CONFIG_YAHOO_SUMMARY (object);

	g_clear_object (&self->priv->collection_source);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_yahoo_summary_parent_class)->dispose (object);
}

static void
mail_config_yahoo_summary_constructed (GObject *object)
{
	EMailConfigYahooSummary *extension;
	EMailConfigSummaryPage *page;
	ESourceCollection *collection_extension;
	ESource *source;
	GtkWidget *container;
	GtkWidget *widget;
	GtkBox *main_box;
	const gchar *extension_name;
	const gchar *text;
	gchar *markup;

	extension = E_MAIL_CONFIG_YAHOO_SUMMARY (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_yahoo_summary_parent_class)->constructed (object);

	page = mail_config_yahoo_summary_get_summary_page (extension);
	main_box = e_mail_config_summary_page_get_internal_box (page);

	/* Use g_signal_connect_after() so the EMailConfigSummaryPage
	 * class methods run first.  They make changes to the sources
	 * that we either want to utilize or override. */

	g_signal_connect_after (
		page, "refresh",
		G_CALLBACK (mail_config_yahoo_summary_refresh_cb),
		extension);

	g_signal_connect_after (
		page, "commit-changes",
		G_CALLBACK (mail_config_yahoo_summary_commit_changes_cb),
		extension);

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 6);
	gtk_box_pack_start (main_box, widget, FALSE, FALSE, 0);

	e_binding_bind_property (
		extension, "applicable",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	container = widget;

	text = _("Yahoo! Features");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 2, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("Add Ca_lendar and Tasks to this account");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 2, 1);
	extension->priv->calendar_toggle = widget;  /* not referenced */
	gtk_widget_show (widget);

	widget = gtk_check_button_new_with_mnemonic (_("Add Con_tacts to this account"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 2, 1);
	extension->priv->contacts_toggle = widget;  /* not referenced */
	gtk_widget_show (widget);

	source = extension->priv->collection_source;
	extension_name = E_SOURCE_EXTENSION_COLLECTION;
	collection_extension = e_source_get_extension (source, extension_name);

	/* Can't bind the collection's display name here because
	 * the Summary Page has no sources yet.  Set the display
	 * name while committing instead. */

	e_binding_bind_property (
		extension->priv->calendar_toggle, "active",
		collection_extension, "calendar-enabled",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		extension->priv->contacts_toggle, "active",
		collection_extension, "contacts-enabled",
		G_BINDING_SYNC_CREATE);
}

static void
e_mail_config_yahoo_summary_class_init (EMailConfigYahooSummaryClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = mail_config_yahoo_summary_get_property;
	object_class->dispose = mail_config_yahoo_summary_dispose;
	object_class->constructed = mail_config_yahoo_summary_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_CONFIG_SUMMARY_PAGE;

	g_object_class_install_property (
		object_class,
		PROP_APPLICABLE,
		g_param_spec_boolean (
			"applicable",
			"Applicable",
			"Whether this extension is applicable "
			"to the current mail account settings",
			FALSE,
			G_PARAM_READABLE));
}

static void
e_mail_config_yahoo_summary_class_finalize (EMailConfigYahooSummaryClass *class)
{
}

static void
e_mail_config_yahoo_summary_init (EMailConfigYahooSummary *extension)
{
	ESource *source;
	ESourceBackend *backend_extension;
	const gchar *extension_name;

	extension->priv = e_mail_config_yahoo_summary_get_instance_private (extension);

	source = e_source_new (NULL, NULL, NULL);
	extension_name = E_SOURCE_EXTENSION_COLLECTION;
	backend_extension = e_source_get_extension (source, extension_name);
	e_source_backend_set_backend_name (backend_extension, "yahoo");
	extension->priv->collection_source = source;
}

void
e_mail_config_yahoo_summary_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_config_yahoo_summary_register_type (type_module);
}

gboolean
e_mail_config_yahoo_summary_get_applicable (EMailConfigYahooSummary *extension)
{
	g_return_val_if_fail (
		E_IS_MAIL_CONFIG_YAHOO_SUMMARY (extension), FALSE);

	return extension->priv->applicable;
}
