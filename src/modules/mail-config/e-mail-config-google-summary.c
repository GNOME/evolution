/*
 * e-mail-config-google-summary.c
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

#include <libedataserver/libedataserver.h>
#include <mail/e-mail-config-summary-page.h>
#include "shell/e-shell.h"

#include "e-mail-config-google-summary.h"

#define GOOGLE_IMAP_URI		"http://support.google.com/mail/bin/answer.py?hl=en&answer=77695"
#define GOOGLE_CALENDAR_URI	"https://calendar.google.com/calendar/syncselect"

struct _EMailConfigGoogleSummaryPrivate {
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

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EMailConfigGoogleSummary, e_mail_config_google_summary, E_TYPE_EXTENSION, 0,
	G_ADD_PRIVATE_DYNAMIC (EMailConfigGoogleSummary))

static EMailConfigSummaryPage *
mail_config_google_summary_get_summary_page (EMailConfigGoogleSummary *extension)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	return E_MAIL_CONFIG_SUMMARY_PAGE (extensible);
}

static gboolean
mail_config_google_summary_is_applicable (EMailConfigSummaryPage *page)
{
	ESource *source;
	const gchar *extension_name;
	const gchar *host = NULL;

	/* FIXME We should tie this into EMailAutoconfig to avoid
	 *       hard-coding Google domain names.  Maybe retain the
	 *       <emailProvider id="..."> it matched so we can just
	 *       check for, in this case, "googlemail.com".
	 *
	 *       Source:
	 *       http://api.gnome.org/evolution/autoconfig/1.1/google.com
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

	if (e_util_host_is_in_domain (host, "gmail.com"))
		return TRUE;

	if (e_util_host_is_in_domain (host, "googlemail.com"))
		return TRUE;

	return FALSE;
}

static gboolean
mail_config_google_summary_is_oauth2_supported (void)
{
	EShell *shell;
	ESourceRegistry *registry;

	shell = e_shell_get_default ();
	if (!shell)
		return FALSE;

	registry = e_shell_get_registry (shell);
	if (!registry)
		return FALSE;

	return e_oauth2_services_is_oauth2_alias (e_source_registry_get_oauth2_services (registry), "Google");
}

static void
mail_config_google_summary_refresh_cb (EMailConfigSummaryPage *page,
                                       EMailConfigGoogleSummary *extension)
{
	extension->priv->applicable =
		mail_config_google_summary_is_applicable (page);

	g_object_notify (G_OBJECT (extension), "applicable");
}

static void
mail_config_google_summary_commit_changes_cb (EMailConfigSummaryPage *page,
                                              GQueue *source_queue,
                                              EMailConfigGoogleSummary *extension)
{
	ESource *source;
	ESourceCollection *collection_extension;
	ESourceAuthentication *auth_extension;
	GtkToggleButton *toggle_button;
	GList *head, *link;
	const gchar *user;
	const gchar *parent_uid;
	const gchar *display_name;
	const gchar *extension_name;
	gboolean calendar_active;
	gboolean contacts_active;

	/* If this is not a Google account, do nothing (obviously). */
	if (!e_mail_config_google_summary_get_applicable (extension))
		return;

	toggle_button = GTK_TOGGLE_BUTTON (extension->priv->calendar_toggle);
	calendar_active = gtk_toggle_button_get_active (toggle_button);

	if (mail_config_google_summary_is_oauth2_supported ()) {
		toggle_button = GTK_TOGGLE_BUTTON (extension->priv->contacts_toggle);
		contacts_active = gtk_toggle_button_get_active (toggle_button);
	} else {
		contacts_active = FALSE;
	}

	/* If the user declined both Calendar and Contacts, do nothing,
	   but set the Google/OAuth2 authentication for the sources. */
	if (!calendar_active && !contacts_active) {
		if (mail_config_google_summary_is_oauth2_supported ()) {
			source = e_mail_config_summary_page_get_account_source (page);
			auth_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);
			e_source_authentication_set_method (auth_extension, "Google");

			head = g_queue_peek_head_link (source_queue);
			for (link = head; link != NULL; link = g_list_next (link)) {
				source = link->data;

				if (e_source_has_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION)) {
					auth_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);
					e_source_authentication_set_method (auth_extension, "Google");
				}
			}
		}
		return;
	}

	source = e_mail_config_summary_page_get_account_source (page);
	display_name = e_source_get_display_name (source);

	/* The collection identity is the mail account user name. */
	extension_name = E_SOURCE_EXTENSION_AUTHENTICATION;
	auth_extension = e_source_get_extension (source, extension_name);
	user = e_source_authentication_get_user (auth_extension);

	source = extension->priv->collection_source;
	e_source_set_display_name (source, display_name);

	extension_name = E_SOURCE_EXTENSION_COLLECTION;
	collection_extension = e_source_get_extension (source, extension_name);
	e_source_collection_set_identity (collection_extension, user);

	/* Always create the Authentication extension, thus the collection source
	   can be used for the credentials prompt. */
	auth_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_AUTHENTICATION);
	e_source_authentication_set_host (auth_extension, "");

	if (mail_config_google_summary_is_oauth2_supported ()) {
		e_source_authentication_set_user (auth_extension, user);
		e_source_authentication_set_method (auth_extension, "Google");
	}

	/* All queued sources become children of the collection source. */
	parent_uid = e_source_get_uid (source);
	head = g_queue_peek_head_link (source_queue);
	for (link = head; link != NULL; link = g_list_next (link))
		e_source_set_parent (E_SOURCE (link->data), parent_uid);

	/* Push this AFTER iterating over the source queue. */
	g_queue_push_head (source_queue, g_object_ref (source));

	/* The "google-backend" module in E-D-S will handle the rest. */
}

static void
mail_config_google_summary_get_property (GObject *object,
                                         guint property_id,
                                         GValue *value,
                                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_APPLICABLE:
			g_value_set_boolean (
				value,
				e_mail_config_google_summary_get_applicable (
				E_MAIL_CONFIG_GOOGLE_SUMMARY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_google_summary_dispose (GObject *object)
{
	EMailConfigGoogleSummary *self = E_MAIL_CONFIG_GOOGLE_SUMMARY (object);

	g_clear_object (&self->priv->collection_source);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_google_summary_parent_class)->dispose (object);
}

static void
mail_config_google_summary_constructed (GObject *object)
{
	EMailConfigGoogleSummary *extension;
	EMailConfigSummaryPage *page;
	ESourceCollection *collection_extension;
	ESource *source;
	GtkWidget *container;
	GtkWidget *widget;
	GtkBox *main_box;
	const gchar *extension_name;
	const gchar *text;
	gchar *markup, *imap_url, *cal_url;

	extension = E_MAIL_CONFIG_GOOGLE_SUMMARY (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_google_summary_parent_class)->constructed (object);

	page = mail_config_google_summary_get_summary_page (extension);
	main_box = e_mail_config_summary_page_get_internal_box (page);

	/* Use g_signal_connect_after() so the EMailConfigSummaryPage
	 * class methods run first.  They make changes to the sources
	 * the we either want to utilize or override. */

	g_signal_connect_after (
		page, "refresh",
		G_CALLBACK (mail_config_google_summary_refresh_cb),
		extension);

	g_signal_connect_after (
		page, "commit-changes",
		G_CALLBACK (mail_config_google_summary_commit_changes_cb),
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

	text = _("Google Features");
	markup = g_markup_printf_escaped ("<b>%s</b>", text);
	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 0, 1, 1);
	gtk_widget_show (widget);
	g_free (markup);

	text = _("Add Ca_lendar to this account");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	gtk_widget_set_margin_start (widget, 12);
	gtk_grid_attach (GTK_GRID (container), widget, 0, 1, 1, 1);
	extension->priv->calendar_toggle = widget;  /* not referenced */
	gtk_widget_show (widget);

	if (mail_config_google_summary_is_oauth2_supported ()) {
		text = _("Add Con_tacts to this account");
		widget = gtk_check_button_new_with_mnemonic (text);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
		gtk_widget_set_margin_start (widget, 12);
		gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 1, 1);
		extension->priv->contacts_toggle = widget;  /* not referenced */
		gtk_widget_show (widget);
	}

	/* Translators: This is part of a sentence "You may need to enable IMAP access and Calendars to synchronize",
	   where the 'IMAP access' and 'Calendars to synchronize' are clickable. */
	imap_url = g_markup_printf_escaped ("<a href=\"%s\">%s</a>", GOOGLE_IMAP_URI, C_("GoggleSummary", "IMAP access"));
	/* Translators: This is part of a sentence "You may need to enable IMAP access and Calendars to synchronize",
	   where the 'IMAP access' and 'Calendars to synchronize' are clickable. */
	cal_url = g_markup_printf_escaped ("<a href=\"%s\">%s</a>", GOOGLE_CALENDAR_URI, C_("GoggleSummary", "Calendars to synchronize"));
	/* Translators: This is part of a sentence "You may need to enable IMAP access and Calendars to synchronize",
	   where the 'IMAP access' and 'Calendars to synchronize' are clickable. */
	markup = g_strdup_printf (C_("GoggleSummary", "You may need to enable %s and %s"), imap_url, cal_url);

	widget = gtk_label_new (markup);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_widget_set_margin_start (widget, 12);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	if (mail_config_google_summary_is_oauth2_supported ())
		gtk_grid_attach (GTK_GRID (container), widget, 0, 3, 1, 1);
	else
		gtk_grid_attach (GTK_GRID (container), widget, 0, 2, 1, 1);
	gtk_widget_show (widget);

	g_free (imap_url);
	g_free (cal_url);
	g_free (markup);

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

	if (mail_config_google_summary_is_oauth2_supported ())
		e_binding_bind_property (
			extension->priv->contacts_toggle, "active",
			collection_extension, "contacts-enabled",
			G_BINDING_SYNC_CREATE);
	else
		g_object_set (G_OBJECT (collection_extension), "contacts-enabled", FALSE, NULL);
}

static void
e_mail_config_google_summary_class_init (EMailConfigGoogleSummaryClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = mail_config_google_summary_get_property;
	object_class->dispose = mail_config_google_summary_dispose;
	object_class->constructed = mail_config_google_summary_constructed;

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
e_mail_config_google_summary_class_finalize (EMailConfigGoogleSummaryClass *class)
{
}

static void
e_mail_config_google_summary_init (EMailConfigGoogleSummary *extension)
{
	ESource *source;
	ESourceBackend *backend_extension;
	const gchar *extension_name;

	extension->priv = e_mail_config_google_summary_get_instance_private (extension);

	source = e_source_new (NULL, NULL, NULL);
	extension_name = E_SOURCE_EXTENSION_COLLECTION;
	backend_extension = e_source_get_extension (source, extension_name);
	e_source_backend_set_backend_name (backend_extension, "google");
	extension->priv->collection_source = source;
}

void
e_mail_config_google_summary_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_config_google_summary_register_type (type_module);
}

gboolean
e_mail_config_google_summary_get_applicable (EMailConfigGoogleSummary *extension)
{
	g_return_val_if_fail (
		E_IS_MAIL_CONFIG_GOOGLE_SUMMARY (extension), FALSE);

	return extension->priv->applicable;
}

