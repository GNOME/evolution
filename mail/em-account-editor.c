/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Dan Winship <danw@ximian.com>
 *		Jeffrey Stedfast <fejj@ximian.com>
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * work before merge can occur:
 *
 * verify behaviour.
 * work out what to do with the startup assistant.
 *
 * also need to work out:
 * how to remove unecessary items from a service url once
 * configured (removing settings from other types).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <string.h>
#include <stdarg.h>

#include <libedataserverui/e-passwords.h>

#include "shell/e-shell.h"
#include "e-util/e-util.h"
#include "e-util/e-alert-dialog.h"
#include "libemail-utils/e-account-utils.h"
#include "e-util/e-dialog-utils.h"
#include "libemail-utils/e-signature-list.h"
#include "libemail-utils/e-signature-utils.h"
#include "e-util/e-util-private.h"
#include "widgets/misc/e-auth-combo-box.h"
#include "widgets/misc/e-signature-editor.h"
#include "widgets/misc/e-port-entry.h"

#include "e-mail-backend.h"
#include "libemail-engine/e-mail-folder-utils.h"
#include "e-mail-junk-options.h"
#include "em-config.h"
#include "em-folder-selection-button.h"
#include "em-account-editor.h"
#include "mail-send-recv.h"
#include "em-utils.h"
#include "mail-guess-servers.h"
#include "libemail-engine/mail-ops.h"
#include "libemail-utils/mail-mt.h"
#include "e-mail-ui-session.h"

#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
#include "smime/gui/e-cert-selector.h"
#endif

#define EM_ACCOUNT_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_ACCOUNT_EDITOR, EMAccountEditorPrivate))

/* Option widgets whose sensitivity depends on another widget, such
 * as a checkbox being active, are indented to the right slightly for
 * better visual clarity.  This specifies how far to the right. */
#define INDENT_MARGIN 24

#define d(x)

/* econfig item for the extra config hings */
struct _receive_options_item {
	EMConfigItem item;

	/* Only CAMEL_PROVIDER_CONF_ENTRYs GtkEntrys are stored here.
	 * The auto-detect camel provider code will probably be removed */
	GHashTable *extra_table;
};

typedef struct _EMAccountEditorService {
	EMAccountEditor *emae;	/* parent pointer, for callbacks */

	/* NOTE: keep all widgets together, first frame last check_dialog */
	GtkWidget *frame;
	GtkWidget *container;

	GtkComboBox *providers;

	GtkLabel *description;
	GtkLabel *hostlabel;
	GtkEntry *hostname;
	GtkLabel *portlabel;
	EPortEntry *port;
	GtkLabel *userlabel;
	GtkEntry *username;
	GtkLabel *pathlabel;
	GtkWidget *pathentry;

	GtkWidget *ssl_frame;
	GtkComboBox *use_ssl;
	GtkWidget *ssl_hbox;
	GtkWidget *no_ssl;

	GtkWidget *auth_frame;
	GtkComboBox *authtype;

	GtkWidget *authitem;
	GtkToggleButton *remember;
	GtkButton *check_supported;
	GtkToggleButton *needs_auth;
	gboolean requires_auth;

	GCancellable *checking;
	GtkWidget *check_dialog;

	const gchar *protocol;
	CamelProviderType type;
	CamelSettings *settings;

	gboolean visible_auth;
	gboolean visible_host;
	gboolean visible_path;
	gboolean visible_port;
	gboolean visible_user;
} EMAccountEditorService;

struct _EMAccountEditorPrivate {

	EMailBackend *backend;
	EAccount *modified_account;
	EAccount *original_account;
	gboolean new_account;

	struct _EMConfig *config;
	GList *providers;

	/* signatures */
	GtkComboBox *signatures_dropdown;
	guint sig_added_id;
	guint sig_removed_id;
	guint sig_changed_id;
	const gchar *sig_uid;

	/* incoming mail */
	EMAccountEditorService source;

	/* extra incoming config */
	CamelProvider *extra_provider;
	GSList *extra_items;	/* this is freed by the econfig automatically */

	/* outgoing mail */
	EMAccountEditorService transport;

	/* account management */
	GtkEntry *identity_entries[5];
	GtkToggleButton *default_account;
	GtkWidget *management_frame;

	/* special folders */
	GtkButton *drafts_folder_button;
	GtkButton *sent_folder_button;
	GtkToggleButton *trash_folder_check;
	GtkButton *trash_folder_button;
	GtkToggleButton *junk_folder_check;
	GtkButton *junk_folder_button;
	GtkButton *restore_folders_button;

	/* Security */
	GtkEntry *pgp_key;
	GtkToggleButton *pgp_encrypt_to_self;
	GtkToggleButton *pgp_always_sign;
	GtkToggleButton *pgp_no_imip_sign;
	GtkToggleButton *pgp_always_trust;

	GtkToggleButton *smime_sign_default;
	GtkEntry *smime_sign_key;
	GtkButton *smime_sign_key_select;
	GtkButton *smime_sign_key_clear;
	GtkButton *smime_sign_select;
	GtkToggleButton *smime_encrypt_default;
	GtkToggleButton *smime_encrypt_to_self;
	GtkEntry *smime_encrypt_key;
	GtkButton *smime_encrypt_key_select;
	GtkButton *smime_encrypt_key_clear;

	/* Review */
	GtkLabel *review_name;
	GtkLabel *review_email;
	GtkLabel *send_name;
	GtkLabel *send_stype;
	GtkLabel *send_saddress;
	GtkLabel *send_encryption;
	GtkLabel *receive_name;
	GtkLabel *receive_stype;
	GtkLabel *receive_saddress;
	GtkLabel *receive_encryption;
	GtkWidget *review_box;

	/* google and yahoo specific data*/
	gboolean is_gmail;
	gboolean is_yahoo;

	GtkWidget *calendar;
	GtkWidget *gcontacts;
	GtkWidget *info_label;

	GtkWidget *account_label;
	GtkWidget *gmail_link;
	GtkWidget *yahoo_cal_box;
	GtkWidget *yahoo_cal_entry;

	/* for e-config callbacks, each page sets up its widgets, then they are dealed out by the get_widget callback in order*/
	GHashTable *widgets;

	/* for assistant page preparation */
	guint identity_set : 1;
	guint receive_set : 1;
	guint receive_opt_set : 1;
	guint send_set : 1;
	guint review_set : 1;

	ServerData *selected_server;
};

enum {
	PROP_0,
	PROP_BACKEND,
	PROP_MODIFIED_ACCOUNT,
	PROP_ORIGINAL_ACCOUNT,
	PROP_STORE_PROVIDER,
	PROP_STORE_REQUIRES_AUTH,
	PROP_STORE_SETTINGS,
	PROP_STORE_VISIBLE_AUTH,
	PROP_STORE_VISIBLE_HOST,
	PROP_STORE_VISIBLE_PATH,
	PROP_STORE_VISIBLE_PORT,
	PROP_STORE_VISIBLE_USER,
	PROP_TRANSPORT_PROVIDER,
	PROP_TRANSPORT_REQUIRES_AUTH,
	PROP_TRANSPORT_SETTINGS,
	PROP_TRANSPORT_VISIBLE_AUTH,
	PROP_TRANSPORT_VISIBLE_HOST,
	PROP_TRANSPORT_VISIBLE_PATH,
	PROP_TRANSPORT_VISIBLE_PORT,
	PROP_TRANSPORT_VISIBLE_USER
};

static void em_account_editor_construct (EMAccountEditor *emae, EMAccountEditorType type, const gchar *id);
static void emae_account_folder_changed (EMFolderSelectionButton *folder, EMAccountEditor *emae);

G_DEFINE_TYPE (EMAccountEditor, em_account_editor, G_TYPE_OBJECT)

static void
emae_config_gone_cb (gpointer pemae, GObject *pconfig)
{
	EMAccountEditor *emae = pemae;
	struct _EMConfig *config = (struct _EMConfig *) pconfig;

	if (!emae)
		return;

	if (emae->config == config)
		emae->config = NULL;

	if (emae->priv && emae->priv->config == config)
		emae->priv->config = NULL;
}

static void
emae_config_target_changed_cb (EMAccountEditor *emae)
{
	e_config_target_changed (
		(EConfig *) emae->config,
		E_CONFIG_TARGET_CHANGED_STATE);
}

static gint
emae_provider_compare (const CamelProvider *p1,
                       const CamelProvider *p2)
{
	/* sort providers based on "location" (ie. local or remote) */
	if (p1->flags & CAMEL_PROVIDER_IS_REMOTE) {
		if (p2->flags & CAMEL_PROVIDER_IS_REMOTE)
			return 0;
		return -1;
	} else {
		if (p2->flags & CAMEL_PROVIDER_IS_REMOTE)
			return 1;
		return 0;
	}
}

static GList *
emae_list_providers (void)
{
	GList *list, *link;
	GQueue trash = G_QUEUE_INIT;

	list = camel_provider_list (TRUE);
	list = g_list_sort (list, (GCompareFunc) emae_provider_compare);

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
emae_set_original_account (EMAccountEditor *emae,
                           EAccount *original_account)
{
	EAccount *modified_account;

	g_return_if_fail (emae->priv->original_account == NULL);

	/* Editing an existing account. */
	if (original_account != NULL) {
		gchar *xml;

		xml = e_account_to_xml (original_account);
		modified_account = e_account_new_from_xml (xml);
		g_free (xml);

		g_object_ref (original_account);
		if (emae->type != EMAE_PAGES)
			emae->do_signature = TRUE;

	/* Creating a new account. */
	} else {
		modified_account = e_account_new ();
		modified_account->enabled = TRUE;
		emae->priv->new_account = TRUE;
	}

	emae->priv->original_account = original_account;
	emae->priv->modified_account = modified_account;
}

static void
emae_set_backend (EMAccountEditor *emae,
                  EMailBackend *backend)
{
	g_return_if_fail (E_IS_MAIL_BACKEND (backend));
	g_return_if_fail (emae->priv->backend == NULL);

	emae->priv->backend = g_object_ref (backend);
}

static CamelProvider *
emae_get_store_provider (EMAccountEditor *emae)
{
	CamelProvider *provider = NULL;
	const gchar *protocol;

	protocol = emae->priv->source.protocol;

	if (protocol != NULL)
		provider = camel_provider_get (protocol, NULL);

	return provider;
}

static gboolean
emae_get_store_requires_auth (EMAccountEditor *emae)
{
	return emae->priv->source.requires_auth;
}

static void
emae_set_store_requires_auth (EMAccountEditor *emae,
                              gboolean requires_auth)
{
	emae->priv->source.requires_auth = requires_auth;

	g_object_notify (G_OBJECT (emae), "store-requires-auth");
}

static CamelSettings *
emae_get_store_settings (EMAccountEditor *emae)
{
	return emae->priv->source.settings;
}

static void
emae_set_store_settings (EMAccountEditor *emae,
                         CamelSettings *settings)
{
	if (settings != NULL)
		g_object_ref (settings);

	if (emae->priv->source.settings != NULL) {
		g_signal_handlers_disconnect_by_func (
			emae->priv->source.settings,
			emae_config_target_changed_cb, emae);
		g_object_unref (emae->priv->source.settings);
	}

	emae->priv->source.settings = settings;

	g_object_notify (G_OBJECT (emae), "store-settings");
}

static gboolean
emae_get_store_visible_auth (EMAccountEditor *emae)
{
	return emae->priv->source.visible_auth;
}

static void
emae_set_store_visible_auth (EMAccountEditor *emae,
                             gboolean visible_auth)
{
	emae->priv->source.visible_auth = visible_auth;

	g_object_notify (G_OBJECT (emae), "store-visible-auth");
}

static gboolean
emae_get_store_visible_host (EMAccountEditor *emae)
{
	return emae->priv->source.visible_host;
}

static void
emae_set_store_visible_host (EMAccountEditor *emae,
                             gboolean visible_host)
{
	emae->priv->source.visible_host = visible_host;

	g_object_notify (G_OBJECT (emae), "store-visible-host");
}

static gboolean
emae_get_store_visible_path (EMAccountEditor *emae)
{
	return emae->priv->source.visible_path;
}

static void
emae_set_store_visible_path (EMAccountEditor *emae,
                             gboolean visible_path)
{
	emae->priv->source.visible_path = visible_path;

	g_object_notify (G_OBJECT (emae), "store-visible-path");
}

static gboolean
emae_get_store_visible_port (EMAccountEditor *emae)
{
	return emae->priv->source.visible_port;
}

static void
emae_set_store_visible_port (EMAccountEditor *emae,
                             gboolean visible_port)
{
	emae->priv->source.visible_port = visible_port;

	g_object_notify (G_OBJECT (emae), "store-visible-port");
}

static gboolean
emae_get_store_visible_user (EMAccountEditor *emae)
{
	return emae->priv->source.visible_user;
}

static void
emae_set_store_visible_user (EMAccountEditor *emae,
                             gboolean visible_user)
{
	emae->priv->source.visible_user = visible_user;

	g_object_notify (G_OBJECT (emae), "store-visible-user");
}

static CamelProvider *
emae_get_transport_provider (EMAccountEditor *emae)
{
	CamelProvider *provider = NULL;
	const gchar *protocol;

	protocol = emae->priv->transport.protocol;

	if (protocol != NULL)
		provider = camel_provider_get (protocol, NULL);

	return provider;
}

static gboolean
emae_get_transport_requires_auth (EMAccountEditor *emae)
{
	return emae->priv->transport.requires_auth;
}

static void
emae_set_transport_requires_auth (EMAccountEditor *emae,
                                  gboolean requires_auth)
{
	emae->priv->transport.requires_auth = requires_auth;

	g_object_notify (G_OBJECT (emae), "transport-requires-auth");
}

static CamelSettings *
emae_get_transport_settings (EMAccountEditor *emae)
{
	return emae->priv->transport.settings;
}

static void
emae_set_transport_settings (EMAccountEditor *emae,
                             CamelSettings *settings)
{
	if (settings != NULL)
		g_object_ref (settings);

	if (emae->priv->transport.settings != NULL) {
		g_signal_handlers_disconnect_by_func (
			emae->priv->transport.settings,
			emae_config_target_changed_cb, emae);
		g_object_unref (emae->priv->transport.settings);
	}

	emae->priv->transport.settings = settings;

	g_object_notify (G_OBJECT (emae), "transport-settings");
}

static gboolean
emae_get_transport_visible_auth (EMAccountEditor *emae)
{
	return emae->priv->transport.visible_auth;
}

static void
emae_set_transport_visible_auth (EMAccountEditor *emae,
                                 gboolean visible_auth)
{
	emae->priv->transport.visible_auth = visible_auth;

	g_object_notify (G_OBJECT (emae), "transport-visible-auth");
}

static gboolean
emae_get_transport_visible_host (EMAccountEditor *emae)
{
	return emae->priv->transport.visible_host;
}

static void
emae_set_transport_visible_host (EMAccountEditor *emae,
                                 gboolean visible_host)
{
	emae->priv->transport.visible_host = visible_host;

	g_object_notify (G_OBJECT (emae), "transport-visible-host");
}

static gboolean
emae_get_transport_visible_path (EMAccountEditor *emae)
{
	return emae->priv->transport.visible_path;
}

static void
emae_set_transport_visible_path (EMAccountEditor *emae,
                                 gboolean visible_path)
{
	emae->priv->transport.visible_path = visible_path;

	g_object_notify (G_OBJECT (emae), "transport-visible-path");
}

static gboolean
emae_get_transport_visible_port (EMAccountEditor *emae)
{
	return emae->priv->transport.visible_port;
}

static void
emae_set_transport_visible_port (EMAccountEditor *emae,
                                 gboolean visible_port)
{
	emae->priv->transport.visible_port = visible_port;

	g_object_notify (G_OBJECT (emae), "transport-visible-port");
}

static gboolean
emae_get_transport_visible_user (EMAccountEditor *emae)
{
	return emae->priv->transport.visible_user;
}

static void
emae_set_transport_visible_user (EMAccountEditor *emae,
                                 gboolean visible_user)
{
	emae->priv->transport.visible_user = visible_user;

	g_object_notify (G_OBJECT (emae), "transport-visible-user");
}

static void
emae_set_property (GObject *object,
                   guint property_id,
                   const GValue *value,
                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BACKEND:
			emae_set_backend (
				EM_ACCOUNT_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_ORIGINAL_ACCOUNT:
			emae_set_original_account (
				EM_ACCOUNT_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_STORE_REQUIRES_AUTH:
			emae_set_store_requires_auth (
				EM_ACCOUNT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_STORE_SETTINGS:
			emae_set_store_settings (
				EM_ACCOUNT_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_STORE_VISIBLE_AUTH:
			emae_set_store_visible_auth (
				EM_ACCOUNT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_STORE_VISIBLE_HOST:
			emae_set_store_visible_host (
				EM_ACCOUNT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_STORE_VISIBLE_PATH:
			emae_set_store_visible_path (
				EM_ACCOUNT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_STORE_VISIBLE_PORT:
			emae_set_store_visible_port (
				EM_ACCOUNT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_STORE_VISIBLE_USER:
			emae_set_store_visible_user (
				EM_ACCOUNT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_TRANSPORT_REQUIRES_AUTH:
			emae_set_transport_requires_auth (
				EM_ACCOUNT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_TRANSPORT_SETTINGS:
			emae_set_transport_settings (
				EM_ACCOUNT_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_TRANSPORT_VISIBLE_AUTH:
			emae_set_transport_visible_auth (
				EM_ACCOUNT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_TRANSPORT_VISIBLE_HOST:
			emae_set_transport_visible_host (
				EM_ACCOUNT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_TRANSPORT_VISIBLE_PATH:
			emae_set_transport_visible_path (
				EM_ACCOUNT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_TRANSPORT_VISIBLE_PORT:
			emae_set_transport_visible_port (
				EM_ACCOUNT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_TRANSPORT_VISIBLE_USER:
			emae_set_transport_visible_user (
				EM_ACCOUNT_EDITOR (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
emae_get_property (GObject *object,
                   guint property_id,
                   GValue *value,
                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BACKEND:
			g_value_set_object (
				value,
				em_account_editor_get_backend (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_MODIFIED_ACCOUNT:
			g_value_set_object (
				value,
				em_account_editor_get_modified_account (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_ORIGINAL_ACCOUNT:
			g_value_set_object (
				value,
				em_account_editor_get_original_account (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_STORE_PROVIDER:
			g_value_set_pointer (
				value,
				emae_get_store_provider (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_STORE_REQUIRES_AUTH:
			g_value_set_boolean (
				value,
				emae_get_store_requires_auth (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_STORE_SETTINGS:
			g_value_set_object (
				value,
				emae_get_store_settings (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_STORE_VISIBLE_AUTH:
			g_value_set_boolean (
				value,
				emae_get_store_visible_auth (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_STORE_VISIBLE_HOST:
			g_value_set_boolean (
				value,
				emae_get_store_visible_host (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_STORE_VISIBLE_PATH:
			g_value_set_boolean (
				value,
				emae_get_store_visible_path (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_STORE_VISIBLE_PORT:
			g_value_set_boolean (
				value,
				emae_get_store_visible_port (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_STORE_VISIBLE_USER:
			g_value_set_boolean (
				value,
				emae_get_store_visible_user (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_TRANSPORT_PROVIDER:
			g_value_set_pointer (
				value,
				emae_get_transport_provider (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_TRANSPORT_REQUIRES_AUTH:
			g_value_set_boolean (
				value,
				emae_get_transport_requires_auth (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_TRANSPORT_SETTINGS:
			g_value_set_object (
				value,
				emae_get_transport_settings (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_TRANSPORT_VISIBLE_AUTH:
			g_value_set_boolean (
				value,
				emae_get_transport_visible_auth (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_TRANSPORT_VISIBLE_HOST:
			g_value_set_boolean (
				value,
				emae_get_transport_visible_host (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_TRANSPORT_VISIBLE_PATH:
			g_value_set_boolean (
				value,
				emae_get_transport_visible_path (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_TRANSPORT_VISIBLE_PORT:
			g_value_set_boolean (
				value,
				emae_get_transport_visible_port (
				EM_ACCOUNT_EDITOR (object)));
			return;

		case PROP_TRANSPORT_VISIBLE_USER:
			g_value_set_boolean (
				value,
				emae_get_transport_visible_user (
				EM_ACCOUNT_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
emae_dispose (GObject *object)
{
	EMAccountEditorPrivate *priv;

	priv = EM_ACCOUNT_EDITOR_GET_PRIVATE (object);

	if (priv->backend != NULL) {
		g_object_unref (priv->backend);
		priv->backend = NULL;
	}

	if (priv->modified_account != NULL) {
		g_signal_handlers_disconnect_by_func (
			priv->modified_account,
			emae_config_target_changed_cb, object);
		g_object_unref (priv->modified_account);
		priv->modified_account = NULL;
	}

	if (priv->original_account != NULL) {
		g_object_unref (priv->original_account);
		priv->original_account = NULL;
	}

	if (priv->source.settings != NULL) {
		g_signal_handlers_disconnect_by_func (
			priv->source.settings,
			emae_config_target_changed_cb, object);
		g_object_unref (priv->source.settings);
		priv->source.settings = NULL;
	}

	if (priv->transport.settings != NULL) {
		g_signal_handlers_disconnect_by_func (
			priv->transport.settings,
			emae_config_target_changed_cb, object);
		g_object_unref (priv->transport.settings);
		priv->transport.settings = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_account_editor_parent_class)->dispose (object);
}

static void
emae_finalize (GObject *object)
{
	EMAccountEditor *emae = EM_ACCOUNT_EDITOR (object);
	EMAccountEditorPrivate *priv = emae->priv;

	if (priv->config)
		g_object_weak_unref ((GObject *) priv->config, emae_config_gone_cb, emae);

	if (priv->sig_added_id) {
		ESignatureList *signatures;

		signatures = e_get_signature_list ();
		g_signal_handler_disconnect (signatures, priv->sig_added_id);
		g_signal_handler_disconnect (signatures, priv->sig_removed_id);
		g_signal_handler_disconnect (signatures, priv->sig_changed_id);
	}

	g_list_free (priv->providers);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_account_editor_parent_class)->finalize (object);
}

static void
emae_constructed (GObject *object)
{
	EMAccountEditor *emae;

	emae = EM_ACCOUNT_EDITOR (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (em_account_editor_parent_class)->constructed (object);

	emae->priv->providers = emae_list_providers ();

	/* Set some defaults on the new account before we get started. */
	if (emae->priv->new_account) {
		EMailBackend *backend;
		EMailSession *session;

		backend = em_account_editor_get_backend (emae);
		session = e_mail_backend_get_session (backend);

		/* Pick local Drafts folder. */
		e_account_set_string (
			emae->priv->modified_account,
			E_ACCOUNT_DRAFTS_FOLDER_URI,
			e_mail_session_get_local_folder_uri (
			session, E_MAIL_LOCAL_FOLDER_DRAFTS));

		/* Pick local Sent folder. */
		e_account_set_string (
			emae->priv->modified_account,
			E_ACCOUNT_SENT_FOLDER_URI,
			e_mail_session_get_local_folder_uri (
			session, E_MAIL_LOCAL_FOLDER_SENT));

		/* Encrypt to self by default. */
		e_account_set_bool (
			emae->priv->modified_account,
			E_ACCOUNT_PGP_ENCRYPT_TO_SELF, TRUE);
		e_account_set_bool (
			emae->priv->modified_account,
			E_ACCOUNT_SMIME_ENCRYPT_TO_SELF, TRUE);
	}

	g_signal_connect_swapped (
		emae->priv->modified_account, "changed",
		G_CALLBACK (emae_config_target_changed_cb), emae);
}

static void
em_account_editor_class_init (EMAccountEditorClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EMAccountEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = emae_set_property;
	object_class->get_property = emae_get_property;
	object_class->dispose = emae_dispose;
	object_class->finalize = emae_finalize;
	object_class->constructed = emae_constructed;

	g_object_class_install_property (
		object_class,
		PROP_BACKEND,
		g_param_spec_object (
			"backend",
			"Mail Backend",
			NULL,
			E_TYPE_MAIL_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MODIFIED_ACCOUNT,
		g_param_spec_object (
			"modified-account",
			"Modified Account",
			NULL,
			E_TYPE_ACCOUNT,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_ORIGINAL_ACCOUNT,
		g_param_spec_object (
			"original-account",
			"Original Account",
			NULL,
			E_TYPE_ACCOUNT,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_STORE_PROVIDER,
		g_param_spec_pointer (
			"store-provider",
			"Store Provider",
			"CamelProvider for the storage service",
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_STORE_REQUIRES_AUTH,
		g_param_spec_boolean (
			"store-requires-auth",
			"Store Requires Auth",
			"Storage service requires authentication",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_STORE_SETTINGS,
		g_param_spec_object (
			"store-settings",
			"Store Settings",
			"CamelSettings for the storage service",
			CAMEL_TYPE_SETTINGS,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_STORE_VISIBLE_AUTH,
		g_param_spec_boolean (
			"store-visible-auth",
			"Store Visible Auth",
			"Show auth widgets for the storage service",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_STORE_VISIBLE_HOST,
		g_param_spec_boolean (
			"store-visible-host",
			"Store Visible Host",
			"Show host widgets for the storage service",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_STORE_VISIBLE_PATH,
		g_param_spec_boolean (
			"store-visible-path",
			"Store Visible Path",
			"Show path widgets for the storage service",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_STORE_VISIBLE_PORT,
		g_param_spec_boolean (
			"store-visible-port",
			"Store Visible Port",
			"Show port widgets for the storage service",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_STORE_VISIBLE_USER,
		g_param_spec_boolean (
			"store-visible-user",
			"Store Visible User",
			"Show user widgets for the storage service",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TRANSPORT_PROVIDER,
		g_param_spec_pointer (
			"transport-provider",
			"Transport Provider",
			"CamelProvider for the transport service",
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TRANSPORT_REQUIRES_AUTH,
		g_param_spec_boolean (
			"transport-requires-auth",
			"Transport Requires Auth",
			"Transport service requires authentication",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TRANSPORT_SETTINGS,
		g_param_spec_object (
			"transport-settings",
			"Transport Settings",
			"CamelSettings for the transport service",
			CAMEL_TYPE_SETTINGS,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TRANSPORT_VISIBLE_AUTH,
		g_param_spec_boolean (
			"transport-visible-auth",
			"Transport Visible Auth",
			"Show auth widgets for the transport service",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TRANSPORT_VISIBLE_HOST,
		g_param_spec_boolean (
			"transport-visible-host",
			"Transport Visible Host",
			"Show host widgets for the transport service",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TRANSPORT_VISIBLE_PATH,
		g_param_spec_boolean (
			"transport-visible-path",
			"Transport Visible Path",
			"Show path widgets for the transport service",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TRANSPORT_VISIBLE_PORT,
		g_param_spec_boolean (
			"transport-visible-port",
			"Transport Visible Port",
			"Show port widgets for the transport service",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TRANSPORT_VISIBLE_USER,
		g_param_spec_boolean (
			"transport-visible-user",
			"Transport Visible User",
			"Show user widgets for the transport service",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
}

static void
em_account_editor_init (EMAccountEditor *emae)
{
	emae->priv = EM_ACCOUNT_EDITOR_GET_PRIVATE (emae);

	emae->priv->selected_server = NULL;
	emae->priv->source.emae = emae;
	emae->priv->transport.emae = emae;
	emae->priv->widgets = g_hash_table_new (g_str_hash, g_str_equal);

	/* Pick default storage and transport protocols. */
	emae->priv->source.protocol = "imapx";
	emae->priv->transport.protocol = "smtp";

	emae->priv->is_gmail = FALSE;
	emae->priv->is_yahoo = FALSE;
}

/**
 * em_account_editor_new:
 * @account:
 * @type:
 *
 * Create a new account editor.  If @account is NULL then this is to
 * create a new account, else @account is copied to a working
 * structure and is for editing an existing account.
 *
 * Return value:
 **/
EMAccountEditor *
em_account_editor_new (EAccount *account,
                       EMAccountEditorType type,
                       EMailBackend *backend,
                       const gchar *id)
{
	EMAccountEditor *emae;

	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), NULL);

	emae = g_object_new (
		EM_TYPE_ACCOUNT_EDITOR,
		"original-account", account,
		"backend", backend, NULL);

	em_account_editor_construct (emae, type, id);

	return emae;
}

/**
 * em_account_editor_new_for_pages:
 * @account:
 * @type:
 *
 * Create a new account editor.  If @account is NULL then this is to
 * create a new account, else @account is copied to a working
 * structure and is for editing an existing account.
 *
 * Return value:
 **/
EMAccountEditor *
em_account_editor_new_for_pages (EAccount *account,
                                 EMAccountEditorType type,
                                 EMailBackend *backend,
                                 const gchar *id,
                                 GtkWidget **pages)
{
	EMAccountEditor *emae;

	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), NULL);

	emae = g_object_new (
		EM_TYPE_ACCOUNT_EDITOR,
		"original-account", account,
		"backend", backend, NULL);

	emae->pages = pages;
	em_account_editor_construct (emae, type, id);

	return emae;
}

EMailBackend *
em_account_editor_get_backend (EMAccountEditor *emae)
{
	g_return_val_if_fail (EM_IS_ACCOUNT_EDITOR (emae), NULL);

	return emae->priv->backend;
}

EAccount *
em_account_editor_get_modified_account (EMAccountEditor *emae)
{
	g_return_val_if_fail (EM_IS_ACCOUNT_EDITOR (emae), NULL);

	return emae->priv->modified_account;
}

EAccount *
em_account_editor_get_original_account (EMAccountEditor *emae)
{
	g_return_val_if_fail (EM_IS_ACCOUNT_EDITOR (emae), NULL);

	return emae->priv->original_account;
}

/* ********************************************************************** */

static gboolean
is_email (const gchar *address)
{
	/* This is supposed to check if the address's domain could be
	 * an FQDN but alas, it's not worth the pain and suffering. */
	const gchar *at;

	at = strchr (address, '@');
	/* make sure we have an '@' and that it's not the first or last gchar */
	if (!at || at == address || *(at + 1) == '\0')
		return FALSE;

	return TRUE;
}

static CamelURL *
emae_account_url (EMAccountEditor *emae,
                  gint urlid)
{
	EAccount *account;
	CamelURL *url = NULL;
	const gchar *uri;

	account = em_account_editor_get_modified_account (emae);
	uri = e_account_get_string (account, urlid);

	if (uri && uri[0])
		url = camel_url_new (uri, NULL);

	if (url == NULL) {
		url = camel_url_new ("dummy:", NULL);
		camel_url_set_protocol (url, NULL);
	}

	return url;
}

/* ********************************************************************** */

static void
default_folders_clicked (GtkButton *button,
                         gpointer user_data)
{
	EMAccountEditor *emae = user_data;
	EMFolderSelectionButton *folder_button;
	EMailBackend *backend;
	EMailSession *session;
	const gchar *folder_uri;

	backend = em_account_editor_get_backend (emae);
	session = e_mail_backend_get_session (backend);

	folder_button =
		EM_FOLDER_SELECTION_BUTTON (
		emae->priv->drafts_folder_button);
	folder_uri = e_mail_session_get_local_folder_uri (
		session, E_MAIL_LOCAL_FOLDER_DRAFTS);
	em_folder_selection_button_set_folder_uri (folder_button, folder_uri);
	emae_account_folder_changed (folder_button, emae);

	folder_button =
		EM_FOLDER_SELECTION_BUTTON (
		emae->priv->sent_folder_button);
	folder_uri = e_mail_session_get_local_folder_uri (
		session, E_MAIL_LOCAL_FOLDER_SENT);
	em_folder_selection_button_set_folder_uri (folder_button, folder_uri);
	emae_account_folder_changed (folder_button, emae);

	gtk_toggle_button_set_active (emae->priv->trash_folder_check, FALSE);
	gtk_toggle_button_set_active (emae->priv->junk_folder_check, FALSE);
}

/* The camel provider auto-detect interface should be deprecated.
 * But it still needs to be replaced with something of similar functionality.
 * Just using the normal econfig plugin mechanism should be adequate. */
static void
emae_auto_detect_free (gpointer key,
                       gpointer value,
                       gpointer user_data)
{
	g_free (key);
	g_free (value);
}

static void
emae_auto_detect (EMAccountEditor *emae)
{
	EMAccountEditorPrivate *priv = emae->priv;
	EMAccountEditorService *service = &priv->source;
	CamelProvider *provider;
	GHashTable *auto_detected;
	GSList *l;
	CamelProviderConfEntry *entries;
	gchar *value;
	gint i;
	CamelURL *url;

	provider = camel_provider_get (service->protocol, NULL);

	if (provider == NULL || provider->extra_conf == NULL)
		return;

	entries = provider->extra_conf;

	d (printf ("Running auto-detect\n"));

	url = emae_account_url (emae, E_ACCOUNT_SOURCE_URL);
	camel_provider_auto_detect (provider, url, &auto_detected, NULL);
	camel_url_free (url);

	if (auto_detected == NULL) {
		d (printf (" no values detected\n"));
		return;
	}

	for (i = 0; entries[i].type != CAMEL_PROVIDER_CONF_END; i++) {
		struct _receive_options_item *item;
		GtkWidget *w;

		if (entries[i].name == NULL
		    || (value = g_hash_table_lookup (auto_detected, entries[i].name)) == NULL)
			continue;

		/* only 2 providers use this, and they only do it for 3 entries only */
		g_return_if_fail (entries[i].type == CAMEL_PROVIDER_CONF_ENTRY);

		w = NULL;
		for (l = emae->priv->extra_items; l; l = g_slist_next (l)) {
			item = l->data;
			if (item->extra_table && (w = g_hash_table_lookup (item->extra_table, entries[i].name)))
				break;
		}

		gtk_entry_set_text ((GtkEntry *)w, value?value:"");
	}

	g_hash_table_foreach (auto_detected, emae_auto_detect_free, NULL);
	g_hash_table_destroy (auto_detected);
}

static void
emae_signature_added (ESignatureList *signatures,
                      ESignature *sig,
                      EMAccountEditor *emae)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	const gchar *name;
	const gchar *uid;

	name = e_signature_get_name (sig);
	uid = e_signature_get_uid (sig);

	model = gtk_combo_box_get_model (emae->priv->signatures_dropdown);

	gtk_list_store_append ((GtkListStore *) model, &iter);
	gtk_list_store_set ((GtkListStore *) model, &iter, 0, name, 1, uid, -1);

	gtk_combo_box_set_active (
		emae->priv->signatures_dropdown,
		gtk_tree_model_iter_n_children (model, NULL) - 1);
}

static gint
emae_signature_get_iter (EMAccountEditor *emae,
                         ESignature *sig,
                         GtkTreeModel **modelp,
                         GtkTreeIter *iter)
{
	GtkTreeModel *model;
	gint found = 0;

	model = gtk_combo_box_get_model (emae->priv->signatures_dropdown);
	*modelp = model;
	if (!gtk_tree_model_get_iter_first (model, iter))
		return FALSE;

	do {
		const gchar *signature_uid;
		gchar *uid;

		signature_uid = e_signature_get_uid (sig);

		gtk_tree_model_get (model, iter, 1, &uid, -1);
		if (uid && !strcmp (uid, signature_uid))
			found = TRUE;
		g_free (uid);
	} while (!found && gtk_tree_model_iter_next (model, iter));

	return found;
}

static void
emae_signature_removed (ESignatureList *signatures,
                        ESignature *sig,
                        EMAccountEditor *emae)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (emae_signature_get_iter (emae, sig, &model, &iter))
		gtk_list_store_remove ((GtkListStore *) model, &iter);
}

static void
emae_signature_changed (ESignatureList *signatures,
                        ESignature *sig,
                        EMAccountEditor *emae)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	const gchar *name;

	name = e_signature_get_name (sig);

	if (emae_signature_get_iter (emae, sig, &model, &iter))
		gtk_list_store_set ((GtkListStore *) model, &iter, 0, name, -1);
}

static void
emae_signaturetype_changed (GtkComboBox *dropdown,
                            EMAccountEditor *emae)
{
	EAccount *account;
	gint id = gtk_combo_box_get_active (dropdown);
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *uid = NULL;

	account = em_account_editor_get_modified_account (emae);

	if (id != -1) {
		model = gtk_combo_box_get_model (dropdown);
		if (gtk_tree_model_iter_nth_child (model, &iter, NULL, id))
			gtk_tree_model_get (model, &iter, 1, &uid, -1);
	}

	e_account_set_string (account, E_ACCOUNT_ID_SIGNATURE, uid);
	g_free (uid);
}

static void
emae_signature_new (GtkWidget *widget,
                    EMAccountEditor *emae)
{
	GtkWidget *editor;
	gpointer parent;

	parent = gtk_widget_get_toplevel (widget);
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	editor = e_signature_editor_new ();
	gtk_window_set_transient_for (GTK_WINDOW (editor), parent);
	gtk_widget_show (editor);
}

static GtkWidget *
emae_setup_signatures (EMAccountEditor *emae,
                       GtkBuilder *builder)
{
	EMAccountEditorPrivate *p = emae->priv;
	EAccount *account;
	GtkComboBox *dropdown = (GtkComboBox *)e_builder_get_widget (builder, "signature_dropdown");
	GtkCellRenderer *cell = gtk_cell_renderer_text_new ();
	GtkListStore *store;
	gint i, active = 0;
	GtkTreeIter iter;
	ESignatureList *signatures;
	EIterator *it;
	const gchar *current;
	GtkWidget *button;

	account = em_account_editor_get_modified_account (emae);
	current = e_account_get_string (account, E_ACCOUNT_ID_SIGNATURE);

	emae->priv->signatures_dropdown = dropdown;
	gtk_widget_show ((GtkWidget *) dropdown);

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	gtk_list_store_append (store, &iter);
	/* Translators: "None" as an option for a default signature of an account, part of "Signature: None" */
	gtk_list_store_set (store, &iter, 0, C_("mail-signature", "None"), 1, NULL, -1);

	signatures = e_get_signature_list ();

	if (p->sig_added_id == 0) {
		p->sig_added_id = g_signal_connect (signatures, "signature-added", G_CALLBACK(emae_signature_added), emae);
		p->sig_removed_id = g_signal_connect (signatures, "signature-removed", G_CALLBACK(emae_signature_removed), emae);
		p->sig_changed_id = g_signal_connect (signatures, "signature-changed", G_CALLBACK(emae_signature_changed), emae);
	}

	/* we need to count the 'none' entry before using the index */
	i = 1;
	it = e_list_get_iterator ((EList *) signatures);
	while (e_iterator_is_valid (it)) {
		ESignature *sig = (ESignature *) e_iterator_get (it);
		const gchar *name;
		const gchar *uid;

		name = e_signature_get_name (sig);
		uid = e_signature_get_uid (sig);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, name, 1, uid, -1);

		if (current && !strcmp (current, uid))
			active = i;

		e_iterator_next (it);
		i++;
	}
	g_object_unref (it);

	gtk_cell_layout_pack_start ((GtkCellLayout *) dropdown, cell, TRUE);
	gtk_cell_layout_set_attributes ((GtkCellLayout *)dropdown, cell, "text", 0, NULL);

	gtk_combo_box_set_model (dropdown, (GtkTreeModel *) store);
	gtk_combo_box_set_active (dropdown, active);

	g_signal_connect (dropdown, "changed", G_CALLBACK(emae_signaturetype_changed), emae);

	button = e_builder_get_widget (builder, "sigAddNew");
	g_signal_connect (button, "clicked", G_CALLBACK(emae_signature_new), emae);

	return (GtkWidget *) dropdown;
}

static void
emae_receipt_policy_changed (GtkComboBox *dropdown,
                             EMAccountEditor *emae)
{
	EAccount *account;
	gint id = gtk_combo_box_get_active (dropdown);
	GtkTreeModel *model;
	GtkTreeIter iter;
	EAccountReceiptPolicy policy;

	account = em_account_editor_get_modified_account (emae);

	if (id != -1) {
		model = gtk_combo_box_get_model (dropdown);
		if (gtk_tree_model_iter_nth_child (model, &iter, NULL, id)) {
			gtk_tree_model_get (model, &iter, 1, &policy, -1);
			e_account_set_int (account, E_ACCOUNT_RECEIPT_POLICY, policy);
		}
	}
}

static GtkWidget *
emae_setup_receipt_policy (EMAccountEditor *emae,
                           GtkBuilder *builder)
{
	EAccount *account;
	GtkComboBox *dropdown = (GtkComboBox *)e_builder_get_widget (builder, "receipt_policy_dropdown");
	GtkListStore *store;
	GtkCellRenderer *cell;
	gint i = 0, active = 0;
	GtkTreeIter iter;
	EAccountReceiptPolicy current;
	static struct {
		EAccountReceiptPolicy policy;
		const gchar *label;
	} receipt_policies[] = {
		{ E_ACCOUNT_RECEIPT_NEVER,  N_("Never") },
		{ E_ACCOUNT_RECEIPT_ALWAYS, N_("Always") },
		{ E_ACCOUNT_RECEIPT_ASK,    N_("Ask for each message") }
	};

	account = em_account_editor_get_modified_account (emae);
	current = account->receipt_policy;

	gtk_widget_show ((GtkWidget *) dropdown);

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);

	for (i = 0; i < 3; ++i) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    0, _(receipt_policies[i].label),
				    1, receipt_policies[i].policy,
				    -1);
		if (current == receipt_policies[i].policy)
			active = i;
	}

	gtk_combo_box_set_model (dropdown, (GtkTreeModel *) store);

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (dropdown), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (dropdown), cell, "text", 0, NULL);

	gtk_combo_box_set_active (dropdown, active);

	g_signal_connect (dropdown, "changed", G_CALLBACK(emae_receipt_policy_changed), emae);

	return (GtkWidget *) dropdown;
}

static void
emae_account_entry_changed (GtkEntry *entry,
                            EMAccountEditor *emae)
{
	EAccount *account;
	gchar *text;
	gpointer data;

	account = em_account_editor_get_modified_account (emae);
	data = g_object_get_data (G_OBJECT (entry), "account-item");
	text = g_strdup (gtk_entry_get_text (entry));

	g_strstrip (text);

	e_account_set_string (account, GPOINTER_TO_INT (data), text);

	g_free (text);
}

static GtkEntry *
emae_account_entry (EMAccountEditor *emae,
                    const gchar *name,
                    gint item,
                    GtkBuilder *builder)
{
	EAccount *account;
	GtkEntry *entry;
	const gchar *text;

	account = em_account_editor_get_modified_account (emae);
	entry = (GtkEntry *) e_builder_get_widget (builder, name);
	text = e_account_get_string (account, item);
	if (text)
		gtk_entry_set_text (entry, text);
	g_object_set_data ((GObject *)entry, "account-item", GINT_TO_POINTER(item));
	g_signal_connect (entry, "changed", G_CALLBACK(emae_account_entry_changed), emae);

	return entry;
}

static void
emae_account_toggle_changed (GtkToggleButton *toggle,
                             EMAccountEditor *emae)
{
	EAccount *account;
	gboolean active;
	gpointer data;

	account = em_account_editor_get_modified_account (emae);
	data = g_object_get_data (G_OBJECT (toggle), "account-item");
	active = gtk_toggle_button_get_active (toggle);

	e_account_set_bool (account, GPOINTER_TO_INT (data), active);
}

static void
emae_account_toggle_widget (EMAccountEditor *emae,
                            GtkToggleButton *toggle,
                            gint item)
{
	EAccount *account;
	gboolean active;

	account = em_account_editor_get_modified_account (emae);

	active = e_account_get_bool (account, item);
	gtk_toggle_button_set_active (toggle, active);

	g_object_set_data (
		G_OBJECT (toggle), "account-item",
		GINT_TO_POINTER (item));

	g_signal_connect (
		toggle, "toggled",
		G_CALLBACK (emae_account_toggle_changed), emae);
}

static GtkToggleButton *
emae_account_toggle (EMAccountEditor *emae,
                     const gchar *name,
                     gint item,
                     GtkBuilder *builder)
{
	GtkToggleButton *toggle;

	toggle = (GtkToggleButton *) e_builder_get_widget (builder, name);
	emae_account_toggle_widget (emae, toggle, item);

	return toggle;
}

static void
emae_account_spinint_changed (GtkSpinButton *spin,
                              EMAccountEditor *emae)
{
	EAccount *account;
	gpointer data;
	gint value;

	account = em_account_editor_get_modified_account (emae);
	data = g_object_get_data (G_OBJECT (spin), "account-item");
	value = gtk_spin_button_get_value (spin);

	e_account_set_int (account, GPOINTER_TO_INT (data), value);
}

static void
emae_account_spinint_widget (EMAccountEditor *emae,
                             GtkSpinButton *spin,
                             gint item)
{
	EAccount *account;
	gint v_int;

	account = em_account_editor_get_modified_account (emae);

	v_int = e_account_get_int (account, item);
	gtk_spin_button_set_value (spin, v_int);

	g_object_set_data (
		G_OBJECT (spin), "account-item",
		GINT_TO_POINTER (item));

	g_signal_connect (
		spin, "value-changed",
		G_CALLBACK (emae_account_spinint_changed), emae);
}

static void
emae_account_folder_changed (EMFolderSelectionButton *folder,
                             EMAccountEditor *emae)
{
	EAccount *account;
	gpointer data;
	const gchar *selection;

	account = em_account_editor_get_modified_account (emae);
	data = g_object_get_data (G_OBJECT (folder), "account-item");
	selection = em_folder_selection_button_get_folder_uri (folder);

	e_account_set_string (account, GPOINTER_TO_INT (data), selection);
}

static EMFolderSelectionButton *
emae_account_folder (EMAccountEditor *emae,
                     const gchar *name,
                     gint item,
                     gint deffolder,
                     GtkBuilder *builder)
{
	EAccount *account;
	EMFolderSelectionButton *folder;
	EMailBackend *backend;
	EMailSession *session;
	const gchar *uri;

	account = em_account_editor_get_modified_account (emae);
	backend = em_account_editor_get_backend (emae);
	session = e_mail_backend_get_session (backend);

	folder = (EMFolderSelectionButton *) e_builder_get_widget (builder, name);
	em_folder_selection_button_set_session (folder, session);

	uri = e_account_get_string (account, item);
	if (uri != NULL) {
		em_folder_selection_button_set_folder_uri (folder, uri);
	} else {
		uri = e_mail_session_get_local_folder_uri (session, deffolder);
		em_folder_selection_button_set_folder_uri (folder, uri);
	}

	g_object_set_data ((GObject *)folder, "account-item", GINT_TO_POINTER(item));
	g_object_set_data ((GObject *)folder, "folder-default", GINT_TO_POINTER(deffolder));
	g_signal_connect (folder, "selected", G_CALLBACK(emae_account_folder_changed), emae);
	gtk_widget_show ((GtkWidget *) folder);

	return folder;
}

#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
static void
smime_changed (EMAccountEditor *emae)
{
	EMAccountEditorPrivate *priv = emae->priv;
	gint act;
	const gchar *tmp;

	tmp = gtk_entry_get_text (priv->smime_sign_key);
	act = tmp && tmp[0];
	gtk_widget_set_sensitive ((GtkWidget *) priv->smime_sign_key_clear, act);
	gtk_widget_set_sensitive ((GtkWidget *) priv->smime_sign_default, act);
	if (!act)
		gtk_toggle_button_set_active (priv->smime_sign_default, FALSE);

	tmp = gtk_entry_get_text (priv->smime_encrypt_key);
	act = tmp && tmp[0];
	gtk_widget_set_sensitive ((GtkWidget *) priv->smime_encrypt_key_clear, act);
	gtk_widget_set_sensitive ((GtkWidget *) priv->smime_encrypt_default, act);
	gtk_widget_set_sensitive ((GtkWidget *) priv->smime_encrypt_to_self, act);
	if (!act) {
		gtk_toggle_button_set_active (priv->smime_encrypt_default, FALSE);
	}
}

static void
smime_sign_key_selected (GtkWidget *dialog,
                         const gchar *key,
                         EMAccountEditor *emae)
{
	EMAccountEditorPrivate *priv = emae->priv;

	if (key != NULL) {
		gtk_entry_set_text (priv->smime_sign_key, key);
		smime_changed (emae);
	}

	gtk_widget_destroy (dialog);
}

static void
smime_sign_key_select (GtkWidget *button,
                       EMAccountEditor *emae)
{
	EMAccountEditorPrivate *priv = emae->priv;
	GtkWidget *w;

	w = e_cert_selector_new (E_CERT_SELECTOR_SIGNER, gtk_entry_get_text (priv->smime_sign_key));
	gtk_window_set_modal ((GtkWindow *) w, TRUE);
	gtk_window_set_transient_for ((GtkWindow *) w, (GtkWindow *) gtk_widget_get_toplevel (button));
	g_signal_connect (w, "selected", G_CALLBACK(smime_sign_key_selected), emae);
	gtk_widget_show (w);
}

static void
smime_sign_key_clear (GtkWidget *w,
                      EMAccountEditor *emae)
{
	EMAccountEditorPrivate *priv = emae->priv;

	gtk_entry_set_text (priv->smime_sign_key, "");
	smime_changed (emae);
}

static void
smime_encrypt_key_selected (GtkWidget *dialog,
                            const gchar *key,
                            EMAccountEditor *emae)
{
	EMAccountEditorPrivate *priv = emae->priv;

	if (key != NULL) {
		gtk_entry_set_text (priv->smime_encrypt_key, key);
		smime_changed (emae);
	}

	gtk_widget_destroy (dialog);
}

static void
smime_encrypt_key_select (GtkWidget *button,
                          EMAccountEditor *emae)
{
	EMAccountEditorPrivate *priv = emae->priv;
	GtkWidget *w;

	w = e_cert_selector_new (E_CERT_SELECTOR_RECIPIENT, gtk_entry_get_text (priv->smime_encrypt_key));
	gtk_window_set_modal ((GtkWindow *) w, TRUE);
	gtk_window_set_transient_for ((GtkWindow *) w, (GtkWindow *) gtk_widget_get_toplevel (button));
	g_signal_connect (w, "selected", G_CALLBACK(smime_encrypt_key_selected), emae);
	gtk_widget_show (w);
}

static void
smime_encrypt_key_clear (GtkWidget *w,
                         EMAccountEditor *emae)
{
	EMAccountEditorPrivate *priv = emae->priv;

	gtk_entry_set_text (priv->smime_encrypt_key, "");
	smime_changed (emae);
}
#endif

/* This is used to map each of the two services in a typical account to
 * the widgets that represent each service.  i.e. the receiving (source)
 * service, and the sending (transport) service.  It is used throughout
 * the following code to drive each page. */
static struct _service_info {
	gint account_uri_key;
	gint save_passwd_key;

	const gchar *frame;
	const gchar *type_dropdown;

	const gchar *container;
	const gchar *description;
	const gchar *hostname;
	const gchar *hostlabel;
	const gchar *port;
	const gchar *portlabel;
	const gchar *username;
	const gchar *userlabel;
	const gchar *path;
	const gchar *pathlabel;
	const gchar *pathentry;

	const gchar *security_frame;
	const gchar *ssl_hbox;
	const gchar *use_ssl;
	const gchar *ssl_disabled;

	const gchar *needs_auth;
	const gchar *auth_frame;

	const gchar *authtype;
	const gchar *authtype_check;

	const gchar *remember_password;

} emae_service_info[CAMEL_NUM_PROVIDER_TYPES] = {

	{ E_ACCOUNT_SOURCE_URL,
	  E_ACCOUNT_SOURCE_SAVE_PASSWD,

	  "source-config-section",
	  "source_type_dropdown",

	  "vboxSourceBorder",
	  "source_description",
	  "source_host",
	  "source_host_label",
	  "source_port",
	  "source_port_label",
	  "source_user",
	  "source_user_label",
	  "source_path",
	  "source_path_label",
	  "source_path_entry",

	  "source-security-section",
	  "source_ssl_hbox",
	  "source_use_ssl",
	  "source_ssl_disabled",

	  NULL,
	  "source-auth-section",

	  "source_auth_dropdown",
	  "source_check_supported",

	  "source_remember_password"
	},

	{ E_ACCOUNT_TRANSPORT_URL,
	  E_ACCOUNT_TRANSPORT_SAVE_PASSWD,

	  "transport-server-section",
	  "transport_type_dropdown",

	  "vboxTransportBorder",
	  "transport_description",
	  "transport_host",
	  "transport_host_label",
	  "transport_port",
	  "transport_port_label",
	  "transport_user",
	  "transport_user_label",
	  NULL,
	  NULL,
	  NULL,

	  "transport-security-section",
	  "transport_ssl_hbox",
	  "transport_use_ssl",
	  "transport_ssl_disabled",

	  "transport_needs_auth",
	  "transport-auth-section",

	  "transport_auth_dropdown",
	  "transport_check_supported",

	  "transport_remember_password"
	}
};

static void
emae_file_chooser_changed (GtkFileChooser *file_chooser,
                           EMAccountEditorService *service)
{
	CamelLocalSettings *local_settings;
	const gchar *filename;

	local_settings = CAMEL_LOCAL_SETTINGS (service->settings);
	filename = gtk_file_chooser_get_filename (file_chooser);
	camel_local_settings_set_path (local_settings, filename);
}

static void
emae_ensure_auth_mechanism (CamelProvider *provider,
                            CamelSettings *settings)
{
	CamelServiceAuthType *auth_type;
	const gchar *auth_mechanism;

	auth_mechanism =
		camel_network_settings_get_auth_mechanism (
		CAMEL_NETWORK_SETTINGS (settings));

	/* If a mechanism name is already set, we're fine. */
	if (auth_mechanism != NULL)
		return;

	/* Check that the CamelProvider defines some auth mechanisms.
	 * If not, it's reasonable to leave the mechanism name unset. */
	if (provider->authtypes == NULL)
		return;

	/* No authentication mechanism has been chosen, so we'll choose
	 * one from the CamelProvider's list of available mechanisms. */

	auth_type = provider->authtypes->data;
	auth_mechanism = auth_type->authproto;

	camel_network_settings_set_auth_mechanism (
		CAMEL_NETWORK_SETTINGS (settings), auth_mechanism);
}

static void
emae_setup_settings (EMAccountEditorService *service)
{
	CamelServiceClass *class;
	CamelProvider *provider;
	CamelSettings *settings = NULL;
	GType service_type;
	GType settings_type;
	CamelURL *url;

	provider = camel_provider_get (service->protocol, NULL);
	g_return_if_fail (provider != NULL);

	service_type = provider->object_types[service->type];
	g_return_if_fail (g_type_is_a (service_type, CAMEL_TYPE_SERVICE));

	class = g_type_class_ref (service_type);
	settings_type = class->settings_type;
	g_type_class_unref (class);

	url = emae_account_url (
		service->emae,
		emae_service_info[service->type].account_uri_key);

	/* Destroy any old CamelSettings instances.
	 * Changing CamelProviders invalidates them. */

	if (service->settings != NULL)
		camel_settings_save_to_url (service->settings, url);

	if (g_type_is_a (settings_type, CAMEL_TYPE_SETTINGS)) {
		settings = g_object_new (settings_type, NULL);
		camel_settings_load_from_url (settings, url);

		g_signal_connect_swapped (
			settings, "notify",
			G_CALLBACK (emae_config_target_changed_cb),
			service->emae);
	}

	camel_url_free (url);

	if (CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (provider)) {
		emae_set_store_settings (service->emae, settings);
		emae_set_transport_settings (service->emae, settings);

	} else if (service->type == CAMEL_PROVIDER_STORE) {
		emae_set_store_settings (service->emae, settings);

	} else if (service->type == CAMEL_PROVIDER_TRANSPORT) {
		emae_set_transport_settings (service->emae, settings);
	}

	if (CAMEL_IS_NETWORK_SETTINGS (settings)) {

		/* Even if the service does not need to authenticate, we
		 * still need to initialize the auth mechanism combo box.
		 * So if CamelSettings does not already have a mechanism
		 * name set, choose one from the CamelProvider's list of
		 * available auth mechanisms.  Later in emae_commit(),
		 * if need be, we'll revert the setting back to NULL. */
		emae_ensure_auth_mechanism (provider, settings);

		g_object_bind_property (
			settings, "auth-mechanism",
			service->authtype, "active-id",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);

		g_object_bind_property (
			settings, "host",
			service->hostname, "text",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);

		g_object_bind_property_full (
			settings, "security-method",
			service->use_ssl, "active-id",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE,
			e_binding_transform_enum_value_to_nick,
			e_binding_transform_enum_nick_to_value,
			NULL, (GDestroyNotify) NULL);

		g_object_bind_property (
			settings, "port",
			service->port, "port",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);

		g_object_bind_property (
			settings, "security-method",
			service->port, "security-method",
			G_BINDING_SYNC_CREATE);

		g_object_bind_property (
			settings, "user",
			service->username, "text",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);
	}

	if (CAMEL_IS_LOCAL_SETTINGS (settings)) {
		const gchar *path;

		path = camel_local_settings_get_path (
			CAMEL_LOCAL_SETTINGS (settings));
		gtk_file_chooser_set_filename (
			GTK_FILE_CHOOSER (service->pathentry), path);
	}

	g_object_unref (settings);
}

static void
emae_service_provider_changed (EMAccountEditorService *service)
{
	EConfig *config;
	EMConfigTargetSettings *target;
	CamelProvider *provider = NULL;
	const gchar *description;

	/* Protocol is NULL when server type is 'None'. */
	if (service->protocol != NULL)
		provider = camel_provider_get (service->protocol, NULL);

	description = (provider != NULL) ? provider->description : "";
	gtk_label_set_text (service->description, description);

	if (provider != NULL) {
		gboolean visible_auth;
		gboolean visible_host;
		gboolean visible_path;
		gboolean visible_port;
		gboolean visible_user;
		gboolean visible_ssl;
		gboolean allows;
		gboolean hidden;

		emae_setup_settings (service);

		gtk_widget_show (service->frame);

		allows = CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_AUTH);
		hidden = CAMEL_PROVIDER_HIDDEN (provider, CAMEL_URL_PART_AUTH);
		visible_auth = (allows && !hidden);

		allows = CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_HOST);
		hidden = CAMEL_PROVIDER_HIDDEN (provider, CAMEL_URL_PART_HOST);
		visible_host = (allows && !hidden);

		allows = CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_PATH);
		hidden = CAMEL_PROVIDER_HIDDEN (provider, CAMEL_URL_PART_PATH);
		visible_path = (allows && !hidden);

		allows = CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_PORT);
		hidden = CAMEL_PROVIDER_HIDDEN (provider, CAMEL_URL_PART_PORT);
		visible_port = (allows && !hidden);

		allows = CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_USER);
		hidden = CAMEL_PROVIDER_HIDDEN (provider, CAMEL_URL_PART_USER);
		visible_user = (allows && !hidden);

		switch (service->type) {
			case CAMEL_PROVIDER_STORE:
				g_object_set (
					service->emae,
					"store-visible-auth", visible_auth,
					"store-visible-host", visible_host,
					"store-visible-path", visible_path,
					"store-visible-port", visible_port,
					"store-visible-user", visible_user,
					NULL);
				break;

			case CAMEL_PROVIDER_TRANSPORT:
				g_object_set (
					service->emae,
					"transport-visible-auth", visible_auth,
					"transport-visible-host", visible_host,
					"transport-visible-path", visible_path,
					"transport-visible-port", visible_port,
					"transport-visible-user", visible_user,
					NULL);
				break;

			default:
				g_warn_if_reached ();
		}

		if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_AUTH)) {
			if (service->needs_auth && !CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_AUTH))
				gtk_widget_show ((GtkWidget *) service->needs_auth);
		} else {
			if (service->needs_auth)
				gtk_widget_hide ((GtkWidget *) service->needs_auth);
		}
#ifdef HAVE_SSL
		visible_ssl =
			(provider->flags & CAMEL_PROVIDER_SUPPORTS_SSL);
		gtk_widget_set_visible (service->ssl_frame, visible_ssl);
		gtk_widget_set_visible (service->ssl_hbox, visible_ssl);
		gtk_widget_hide (service->no_ssl);
#else
		gtk_widget_hide (service->ssl_hbox);
		gtk_widget_show (service->no_ssl);
#endif

	} else {
		gtk_widget_hide (service->frame);
		gtk_widget_hide (service->auth_frame);
		gtk_widget_hide (service->ssl_frame);
	}

	/* Update the EConfigTarget so it has the latest CamelSettings. */

	config = E_CONFIG (service->emae->priv->config);
	target = (EMConfigTargetSettings *) config->target;

	em_config_target_update_settings (
		config, target,
		service->emae->priv->modified_account->id->address,
		service->emae->priv->source.protocol,
		service->emae->priv->source.settings,
		service->emae->priv->transport.protocol,
		service->emae->priv->transport.settings);
}

static void
emae_provider_changed (GtkComboBox *combo_box,
                       EMAccountEditorService *service)
{
	const gchar *active_protocol;

	active_protocol = gtk_combo_box_get_active_id (combo_box);

	if (g_strcmp0 (active_protocol, service->protocol) == 0)
		return;

	service->protocol = active_protocol;

	switch (service->type) {
		case CAMEL_PROVIDER_STORE:
			g_object_notify (
				G_OBJECT (service->emae),
				"store-provider");
			break;
		case CAMEL_PROVIDER_TRANSPORT:
			g_object_notify (
				G_OBJECT (service->emae),
				"transport-provider");
			break;
		default:
			g_warn_if_reached ();
	}

	emae_service_provider_changed (service);

	e_config_target_changed (
		(EConfig *) service->emae->priv->config,
		E_CONFIG_TARGET_CHANGED_REBUILD);
}

static void
emae_refresh_providers (EMAccountEditor *emae,
                        EMAccountEditorService *service)
{
	GtkComboBoxText *combo_box;
	GList *link;

	combo_box = GTK_COMBO_BOX_TEXT (service->providers);

	g_signal_handlers_block_by_func (
		combo_box, emae_provider_changed, service);

	gtk_combo_box_text_remove_all (combo_box);

	/* We just special case each type here, its just easier */
	if (service->type == CAMEL_PROVIDER_STORE)
		gtk_combo_box_text_append (
			combo_box, NULL,
			C_("mail-receiving", "None"));

	for (link = emae->priv->providers; link != NULL; link = link->next) {
		CamelProvider *provider = link->data;

		/* FIXME This expression is awesomely unreadable! */
		if (!(provider->object_types[service->type]
		      && (service->type != CAMEL_PROVIDER_STORE ||
			 (provider->flags & CAMEL_PROVIDER_IS_SOURCE) != 0))
		    /* hardcode not showing providers who's transport is done in the store */
		    || (service->type == CAMEL_PROVIDER_TRANSPORT
			&& CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (provider)))
			continue;

		gtk_combo_box_text_append (
			combo_box,
			provider->protocol,
			provider->name);
	}

	g_signal_handlers_unblock_by_func (
		combo_box, emae_provider_changed, service);

	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (combo_box), service->protocol);
}

static void
emae_authtype_changed (GtkComboBox *combo_box,
                       EMAccountEditorService *service)
{
	CamelServiceAuthType *authtype = NULL;
	const gchar *mechanism;
	gboolean sensitive = FALSE;

	mechanism = gtk_combo_box_get_active_id (combo_box);

	if (mechanism != NULL && *mechanism != '\0') {
		authtype = camel_sasl_authtype (mechanism);
		g_warn_if_fail (authtype != NULL);
	}

	sensitive = (authtype == NULL) || (authtype->need_password);
	gtk_widget_set_sensitive (GTK_WIDGET (service->remember), sensitive);
}

static void emae_check_authtype (GtkWidget *w, EMAccountEditorService *service);

static void
emae_check_authtype_done (CamelService *camel_service,
                          GAsyncResult *result,
                          EMAccountEditorService *service)
{
	EMailBackend *backend;
	EMailSession *session;
	GtkWidget *editor;
	GList *available_authtypes;
	GError *error = NULL;

	available_authtypes = camel_service_query_auth_types_finish (
		camel_service, result, &error);

	editor = NULL;
	if (service->emae && service->emae->config && E_IS_CONFIG (service->emae->config))
		editor = E_CONFIG (service->emae->config)->window;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (available_authtypes == NULL);
		g_error_free (error);

	} else if (error != NULL) {
		g_warn_if_fail (available_authtypes == NULL);
		if (service->check_dialog)
			e_alert_run_dialog_for_args (
				GTK_WINDOW (service->check_dialog),
				"mail:checking-service-error",
				error->message, NULL);
		g_error_free (error);

	} else {
		e_auth_combo_box_update_available (
			E_AUTH_COMBO_BOX (service->authtype),
			available_authtypes);
		g_list_free (available_authtypes);
	}

	if (service->check_dialog) {
		g_object_weak_unref (G_OBJECT (service->check_dialog), (GWeakNotify) g_nullify_pointer, &service->check_dialog);
		gtk_widget_destroy (service->check_dialog);
	}
	service->check_dialog = NULL;

	if (editor != NULL)
		gtk_widget_set_sensitive (editor, TRUE);

	backend = em_account_editor_get_backend (service->emae);
	session = e_mail_backend_get_session (backend);

	/* drop the temporary CamelService */
	camel_session_remove_service (
		CAMEL_SESSION (session), camel_service);

	g_object_unref (service->emae);
}

static void
emae_check_authtype_response (GtkDialog *dialog,
                              gint button,
                              GCancellable *cancellable)
{
	g_cancellable_cancel (cancellable);
}

static void
emae_check_authtype (GtkWidget *w,
                     EMAccountEditorService *service)
{
	CamelService *camel_service;
	EMailBackend *backend;
	EMailSession *session;
	GtkWidget *editor;
	gpointer parent;
	gchar *uid;
	GError *error = NULL;

	editor = E_CONFIG (service->emae->config)->window;

	backend = em_account_editor_get_backend (service->emae);
	session = e_mail_backend_get_session (backend);

	uid = g_strdup_printf ("emae-check-authtype-%p", service);

	/* to test on actual data, not on previously used */
	camel_service = camel_session_add_service (
		CAMEL_SESSION (session), uid,
		service->protocol, service->type, &error);

	g_free (uid);

	if (camel_service != NULL && service->settings != NULL)
		camel_service_set_settings (camel_service, service->settings);

	if (editor != NULL)
		parent = gtk_widget_get_toplevel (editor);
	else
		parent = gtk_widget_get_toplevel (w);

	if (error) {
		e_alert_run_dialog_for_args (
			parent, "mail:checking-service-error",
			error->message, NULL);
		g_clear_error (&error);
		return;
	}

	g_return_if_fail (CAMEL_IS_SERVICE (camel_service));

	if (service->checking != NULL) {
		g_cancellable_cancel (service->checking);
		g_object_unref (service->checking);
	}

	service->checking = g_cancellable_new ();

	service->check_dialog = e_alert_dialog_new_for_args (
		parent, "mail:checking-service", NULL);
	g_object_weak_ref (G_OBJECT (service->check_dialog), (GWeakNotify) g_nullify_pointer, &service->check_dialog);

	g_object_ref (service->emae);

	camel_service_query_auth_types (
		camel_service, G_PRIORITY_DEFAULT,
		service->checking, (GAsyncReadyCallback)
		emae_check_authtype_done, service);

	g_signal_connect (
		service->check_dialog, "response",
		G_CALLBACK (emae_check_authtype_response),
		service->checking);

	gtk_widget_show (service->check_dialog);

	if (editor != NULL)
		gtk_widget_set_sensitive (editor, FALSE);
}

static void
emae_setup_service (EMAccountEditor *emae,
                    EMAccountEditorService *service,
                    GtkBuilder *builder)
{
	struct _service_info *info = &emae_service_info[service->type];
	CamelProvider *provider = NULL;
	CamelURL *url;

	/* GtkComboBox internalizes ID strings, which for the provider
	 * combo box are protocol names.  So we'll do the same here. */
	url = emae_account_url (emae, info->account_uri_key);
	if (url != NULL && url->protocol != NULL)
		service->protocol = g_intern_string (url->protocol);
	camel_url_free (url);

	/* Protocol is NULL when server type is 'None'. */
	if (service->protocol != NULL)
		provider = camel_provider_get (service->protocol, NULL);

	/* Extract all widgets we need from the builder file. */

	service->frame = e_builder_get_widget (builder, info->frame);
	service->container = e_builder_get_widget (builder, info->container);
	service->description = GTK_LABEL (e_builder_get_widget (builder, info->description));
	service->hostname = GTK_ENTRY (e_builder_get_widget (builder, info->hostname));
	service->hostlabel = (GtkLabel *) e_builder_get_widget (builder, info->hostlabel);
	service->port = E_PORT_ENTRY (e_builder_get_widget (builder, info->port));
	service->portlabel = (GtkLabel *) e_builder_get_widget (builder, info->portlabel);
	service->username = GTK_ENTRY (e_builder_get_widget (builder, info->username));
	service->userlabel = (GtkLabel *) e_builder_get_widget (builder, info->userlabel);
	if (info->pathentry) {
		service->pathlabel = (GtkLabel *) e_builder_get_widget (builder, info->pathlabel);
		service->pathentry = e_builder_get_widget (builder, info->pathentry);
	}

	service->ssl_frame = e_builder_get_widget (builder, info->security_frame);
	gtk_widget_hide (service->ssl_frame);
	service->ssl_hbox = e_builder_get_widget (builder, info->ssl_hbox);
	service->use_ssl = (GtkComboBox *) e_builder_get_widget (builder, info->use_ssl);
	service->no_ssl = e_builder_get_widget (builder, info->ssl_disabled);

	service->auth_frame = e_builder_get_widget (builder, info->auth_frame);
	service->check_supported = (GtkButton *) e_builder_get_widget (builder, info->authtype_check);
	service->authtype = (GtkComboBox *) e_builder_get_widget (builder, info->authtype);
	service->providers = (GtkComboBox *) e_builder_get_widget (builder, info->type_dropdown);

	/* XXX GtkComboBoxText, when loaded from a GtkBuilder file,
	 *     needs further manual configuration to be fully usable.
	 *     Particularly the ID column has to be set explicitly.
	 *     https://bugzilla.gnome.org/show_bug.cgi?id=612396#c53 */
	g_object_set (
		service->providers,
		"entry-text-column", 0,
		"id-column", 1, NULL);

	service->remember = emae_account_toggle (emae, info->remember_password, info->save_passwd_key, builder);

	if (info->needs_auth) {
		service->needs_auth = (GtkToggleButton *) e_builder_get_widget (builder, info->needs_auth);
	} else {
		service->needs_auth = NULL;
	}

	g_signal_connect (
		service->providers, "changed",
		G_CALLBACK (emae_provider_changed), service);

	if (GTK_IS_FILE_CHOOSER (service->pathentry))
		g_signal_connect (
			service->pathentry, "selection-changed",
			G_CALLBACK (emae_file_chooser_changed), service);

	g_signal_connect (
		service->authtype, "changed",
		G_CALLBACK (emae_authtype_changed), service);

	g_signal_connect (
		service->check_supported, "clicked",
		G_CALLBACK (emae_check_authtype), service);

	switch (service->type) {
		case CAMEL_PROVIDER_STORE:
			g_object_bind_property (
				emae, "store-provider",
				service->authtype, "provider",
				G_BINDING_SYNC_CREATE);

			if (service->needs_auth != NULL) {
				g_object_bind_property (
					emae, "store-requires-auth",
					service->needs_auth, "active",
					G_BINDING_BIDIRECTIONAL |
					G_BINDING_SYNC_CREATE);
				g_object_bind_property (
					emae, "store-requires-auth",
					service->auth_frame, "sensitive",
					G_BINDING_SYNC_CREATE);
			}

			g_object_bind_property (
				emae, "store-visible-auth",
				service->auth_frame, "visible",
				G_BINDING_SYNC_CREATE);

			g_object_bind_property (
				emae, "store-visible-host",
				service->hostname, "visible",
				G_BINDING_SYNC_CREATE);

			g_object_bind_property (
				emae, "store-visible-host",
				service->hostlabel, "visible",
				G_BINDING_SYNC_CREATE);

			g_object_bind_property (
				emae, "store-visible-path",
				service->pathentry, "visible",
				G_BINDING_SYNC_CREATE);

			g_object_bind_property (
				emae, "store-visible-path",
				service->pathlabel, "visible",
				G_BINDING_SYNC_CREATE);

			g_object_bind_property (
				emae, "store-visible-port",
				service->port, "visible",
				G_BINDING_SYNC_CREATE);

			g_object_bind_property (
				emae, "store-visible-port",
				service->portlabel, "visible",
				G_BINDING_SYNC_CREATE);

			g_object_bind_property (
				emae, "store-visible-user",
				service->username, "visible",
				G_BINDING_SYNC_CREATE);

			g_object_bind_property (
				emae, "store-visible-user",
				service->userlabel, "visible",
				G_BINDING_SYNC_CREATE);

			break;

		case CAMEL_PROVIDER_TRANSPORT:
			g_object_bind_property (
				emae, "transport-provider",
				service->authtype, "provider",
				G_BINDING_SYNC_CREATE);

			if (service->needs_auth != NULL) {
				g_object_bind_property (
					emae, "transport-requires-auth",
					service->needs_auth, "active",
					G_BINDING_BIDIRECTIONAL |
					G_BINDING_SYNC_CREATE);
				g_object_bind_property (
					emae, "transport-requires-auth",
					service->auth_frame, "sensitive",
					G_BINDING_SYNC_CREATE);
			}

			g_object_bind_property (
				emae, "transport-visible-auth",
				service->auth_frame, "visible",
				G_BINDING_SYNC_CREATE);

			g_object_bind_property (
				emae, "transport-visible-host",
				service->hostname, "visible",
				G_BINDING_SYNC_CREATE);

			g_object_bind_property (
				emae, "transport-visible-host",
				service->hostlabel, "visible",
				G_BINDING_SYNC_CREATE);

			g_object_bind_property (
				emae, "transport-visible-port",
				service->port, "visible",
				G_BINDING_SYNC_CREATE);

			g_object_bind_property (
				emae, "transport-visible-port",
				service->portlabel, "visible",
				G_BINDING_SYNC_CREATE);

			g_object_bind_property (
				emae, "transport-visible-user",
				service->username, "visible",
				G_BINDING_SYNC_CREATE);

			g_object_bind_property (
				emae, "transport-visible-user",
				service->userlabel, "visible",
				G_BINDING_SYNC_CREATE);

			break;

		default:
			g_warn_if_reached ();
	}

	if (service->pathentry) {
		GtkFileChooserAction action;
		gboolean need_path_dir;
		const gchar *label;

		need_path_dir =
			(provider == NULL) ||
			((provider->url_flags & CAMEL_URL_NEED_PATH_DIR) != 0);

		if (need_path_dir) {
			action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
			label = _("_Path:");
		} else {
			action = GTK_FILE_CHOOSER_ACTION_OPEN;
			label = _("Fil_e:");
		}

		if (service->pathlabel)
			gtk_label_set_text_with_mnemonic (
				GTK_LABEL (service->pathlabel), label);

		if (action != gtk_file_chooser_get_action (GTK_FILE_CHOOSER (service->pathentry)))
			gtk_file_chooser_set_action (GTK_FILE_CHOOSER (service->pathentry), action);
	}

	/* old authtype will be destroyed when we exit */
	emae_refresh_providers (emae, service);

	if (provider != NULL && provider->port_entries)
		e_port_entry_set_camel_entries (
			service->port, provider->port_entries);

	emae_service_provider_changed (service);
}

static GtkWidget *
emae_create_basic_assistant_page (EMAccountEditor *emae,
                                  GtkAssistant *assistant,
                                  const gchar *page_id,
                                  gint position)
{
	const gchar *title = NULL, *label = NULL;
	GtkAssistantPageType page_type = GTK_ASSISTANT_PAGE_CONTENT;
	GtkWidget *vbox, *lbl;
	gboolean fill_space = FALSE;

	g_return_val_if_fail (page_id != NULL, NULL);

	if (g_ascii_strcasecmp (page_id, "start_page") == 0) {
		page_type = GTK_ASSISTANT_PAGE_INTRO;
		fill_space = TRUE;
		title = _("Mail Configuration");
		label = _("Welcome to the Evolution Mail Configuration Assistant.\n\nClick \"Continue\" to begin.");
	} else if (g_ascii_strcasecmp (page_id, "identity_page") == 0) {
		title = _("Identity");
		label = _("Please enter your name and email address below. The \"optional\" fields below do not need to be filled in, unless you wish to include this information in email you send.");
	} else if (g_ascii_strcasecmp (page_id, "source_page") == 0) {
		title = _("Receiving Email");
		label = _("Please configure the following account settings.");
	} else if (g_ascii_strcasecmp (page_id, "transport_page") == 0) {
		title = _("Sending Email");
		label = _("Please enter information about the way you will send mail. If you are not sure, ask your system administrator or Internet Service Provider.");
	} else if (g_ascii_strcasecmp (page_id, "review_page") == 0) {
		title = _("Account Summary");
		label = _("This is a summary of the settings which will be used to access your mail.");
	} else if (g_ascii_strcasecmp (page_id, "finish_page") == 0) {
		page_type = GTK_ASSISTANT_PAGE_CONFIRM;
		fill_space = TRUE;
		title = _("Done");
		label = _("Congratulations, your mail configuration is complete.\n\nYou are now ready to send and receive email using Evolution.\n\nClick \"Apply\" to save your settings.");
	} else {
		g_return_val_if_reached (NULL);
	}

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	gtk_widget_show (vbox);

	lbl = gtk_label_new (label);
	gtk_label_set_line_wrap (GTK_LABEL (lbl), TRUE);
	gtk_misc_set_alignment (GTK_MISC (lbl), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), lbl, fill_space, fill_space, 0);
	gtk_widget_show (lbl);

	if (g_ascii_strcasecmp (page_id, "start_page") == 0)
		g_hash_table_insert (emae->priv->widgets, (gchar *)"start_page_label", lbl);

	gtk_assistant_insert_page (assistant, vbox, position);
	gtk_assistant_set_page_title (assistant, vbox, title);
	gtk_assistant_set_page_type (assistant, vbox, page_type);

	return vbox;
}

/* do not re-order these, the order is used by various code to look up emae->priv->identity_entries[] */
static struct {
	const gchar *name;
	gint item;
} emae_identity_entries[] = {
	{ "management_name", E_ACCOUNT_NAME },
	{ "identity_full_name", E_ACCOUNT_ID_NAME },
	{ "identity_address", E_ACCOUNT_ID_ADDRESS },
	{ "identity_reply_to", E_ACCOUNT_ID_REPLY_TO },
	{ "identity_organization", E_ACCOUNT_ID_ORGANIZATION },
};

static void
emae_queue_widgets (EMAccountEditor *emae,
                    GtkBuilder *builder,
                    const gchar *first,
                    ...)
{
	va_list ap;

	va_start (ap, first);
	while (first) {
		g_hash_table_insert (emae->priv->widgets, (gchar *) first, e_builder_get_widget (builder, first));
		first = va_arg (ap, const gchar *);
	}
	va_end (ap);
}

static GtkWidget *
emae_identity_page (EConfig *ec,
                    EConfigItem *item,
                    GtkWidget *parent,
                    GtkWidget *old,
                    gint position,
                    gpointer data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *priv = emae->priv;
	EAccount *account;
	gint i;
	GtkWidget *w;
	GtkBuilder *builder;

	if (old && emae->type == EMAE_PAGES)
	  return old;

	account = em_account_editor_get_modified_account (emae);

	/* Make sure our custom widget classes are registered with
	 * GType before we load the GtkBuilder definition file. */
	E_TYPE_MAIL_JUNK_OPTIONS;
	EM_TYPE_FOLDER_SELECTION_BUTTON;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "mail-config.ui");

	/* Management & Identity fields, in the assistant the management frame is relocated to the last page later on */
	for (i = 0; i < G_N_ELEMENTS (emae_identity_entries); i++)
		priv->identity_entries[i] = emae_account_entry (emae, emae_identity_entries[i].name, emae_identity_entries[i].item, builder);

	priv->management_frame = e_builder_get_widget (builder, "management-section");

	priv->default_account = GTK_TOGGLE_BUTTON (e_builder_get_widget (builder, "management_default"));
	if (!e_get_default_account ()
		|| (account == e_get_default_account ())
		|| (GPOINTER_TO_INT(g_object_get_data (G_OBJECT (account), "default_flagged"))) )
			gtk_toggle_button_set_active (priv->default_account, TRUE);

	if (emae->do_signature) {
		emae_setup_signatures (emae, builder);
	} else {
		/* TODO: this could/should probably be neater */
		gtk_widget_hide (e_builder_get_widget (builder, "sigLabel"));
#if 0
		gtk_widget_hide (e_builder_get_widget (builder, "sigOption"));
#endif
		gtk_widget_hide (e_builder_get_widget (builder, "signature_dropdown"));
		gtk_widget_hide (e_builder_get_widget (builder, "sigAddNew"));
	}

	w = e_builder_get_widget (builder, item->label);
	if (emae->type == EMAE_PAGES) {
		gtk_box_pack_start ((GtkBox *) emae->pages[0], w, TRUE, TRUE, 0);
	} else if (((EConfig *) priv->config)->type == E_CONFIG_ASSISTANT) {
		GtkWidget *page;

		page = emae_create_basic_assistant_page (
			emae, GTK_ASSISTANT (parent),
			"identity_page", position);

		gtk_box_pack_start (GTK_BOX (page), w, TRUE, TRUE, 0);

		w = page;
	} else {
		gtk_notebook_insert_page (
			GTK_NOTEBOOK (parent), w,
			gtk_label_new (_("Identity")),
			position);
		gtk_container_child_set (
			GTK_CONTAINER (parent), w,
			"tab-fill", FALSE, "tab-expand", FALSE, NULL);
	}

	emae_queue_widgets (
		emae, builder,
		"account_vbox",
		"identity-required-table",
		"identity-optional-table",
		"identity-optional-section",
		"identity_address",
		NULL);

	g_object_unref (builder);

	return w;
}

static GtkWidget *
emae_receive_page (EConfig *ec,
                   EConfigItem *item,
                   GtkWidget *parent,
                   GtkWidget *old,
                   gint position,
                   gpointer data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *priv = emae->priv;
	GtkWidget *w;
	GtkBuilder *builder;

	/*if (old)
	  return old;*/

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "mail-config.ui");

	priv->source.type = CAMEL_PROVIDER_STORE;
	emae_setup_service (emae, &priv->source, builder);

	w = e_builder_get_widget (builder, item->label);
	if (emae->type == EMAE_PAGES) {
		gtk_box_pack_start ((GtkBox *) emae->pages[1], w, TRUE, TRUE, 0);
	} else if (((EConfig *) priv->config)->type == E_CONFIG_ASSISTANT) {
		GtkWidget *page;

		page = emae_create_basic_assistant_page (
			emae, GTK_ASSISTANT (parent),
			"source_page", position);

		gtk_box_pack_start (GTK_BOX (page), w, TRUE, TRUE, 0);

		w = page;
	} else {
		gtk_notebook_insert_page (
			GTK_NOTEBOOK (parent), w,
			gtk_label_new (_("Receiving Email")),
			position);
		gtk_container_child_set (
			GTK_CONTAINER (parent), w,
			"tab-fill", FALSE, "tab-expand", FALSE, NULL);
	}

	emae_queue_widgets (
		emae, builder,
		"source-type-table",
		"source-config-table",
		"source-security-vbox",
		"source-auth-vbox",
		NULL);

	g_object_unref (builder);

	return w;
}

static GtkWidget *
emae_option_toggle (EMAccountEditorService *service,
                    CamelProviderConfEntry *conf)
{
	GtkWidget *widget;

	widget = gtk_check_button_new_with_mnemonic (conf->text);

	g_object_bind_property (
		service->settings, conf->name,
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	if (conf->depname != NULL) {
		g_object_bind_property (
			service->settings, conf->depname,
			widget, "sensitive",
			G_BINDING_SYNC_CREATE);
		gtk_widget_set_margin_left (widget, INDENT_MARGIN);
	}

	return widget;
}

static GtkWidget *
emae_option_entry (EMAccountEditorService *service,
                   CamelProviderConfEntry *conf,
                   GtkLabel *label_for_mnemonic)
{
	GtkWidget *widget;

	widget = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (label_for_mnemonic, widget);

	g_object_bind_property (
		service->settings, conf->name,
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	if (conf->depname != NULL) {
		g_object_bind_property (
			service->settings, conf->depname,
			widget, "sensitive",
			G_BINDING_SYNC_CREATE);
		gtk_widget_set_margin_left (
			GTK_WIDGET (label_for_mnemonic), INDENT_MARGIN);
	}

	g_object_bind_property (
		widget, "sensitive",
		label_for_mnemonic, "sensitive",
		G_BINDING_SYNC_CREATE);

	return widget;
}

static GtkWidget *
emae_option_checkspin (EMAccountEditorService *service,
                       CamelProviderConfEntry *conf)
{
	GObjectClass *class;
	GParamSpec *pspec;
	GParamSpec *use_pspec;
	GtkAdjustment *adjustment;
	GtkWidget *hbox, *spin;
	GtkWidget *prefix;
	gchar *use_property_name;
	gchar *pre, *post;

	/* The conf->name property (e.g. "foo") should be numeric for the
	 * spin button.  If a "use" boolean property exists (e.g. "use-foo")
	 * then a checkbox is also shown. */

	g_return_val_if_fail (conf->name != NULL, NULL);

	class = G_OBJECT_GET_CLASS (service->settings);
	pspec = g_object_class_find_property (class, conf->name);
	g_return_val_if_fail (pspec != NULL, NULL);

	use_property_name = g_strconcat ("use-", conf->name, NULL);
	use_pspec = g_object_class_find_property (class, use_property_name);
	if (use_pspec != NULL && use_pspec->value_type != G_TYPE_BOOLEAN)
		use_pspec = NULL;
	g_free (use_property_name);

	/* Make sure we can convert to and from doubles. */
	g_return_val_if_fail (
		g_value_type_transformable (
		pspec->value_type, G_TYPE_DOUBLE), NULL);
	g_return_val_if_fail (
		g_value_type_transformable (
		G_TYPE_DOUBLE, pspec->value_type), NULL);

	if (G_IS_PARAM_SPEC_CHAR (pspec)) {
		GParamSpecChar *pspec_char;
		pspec_char = G_PARAM_SPEC_CHAR (pspec);
		adjustment = gtk_adjustment_new (
			(gdouble) pspec_char->default_value,
			(gdouble) pspec_char->minimum,
			(gdouble) pspec_char->maximum,
			1.0, 1.0, 0.0);

	} else if (G_IS_PARAM_SPEC_UCHAR (pspec)) {
		GParamSpecUChar *pspec_uchar;
		pspec_uchar = G_PARAM_SPEC_UCHAR (pspec);
		adjustment = gtk_adjustment_new (
			(gdouble) pspec_uchar->default_value,
			(gdouble) pspec_uchar->minimum,
			(gdouble) pspec_uchar->maximum,
			1.0, 1.0, 0.0);

	} else if (G_IS_PARAM_SPEC_INT (pspec)) {
		GParamSpecInt *pspec_int;
		pspec_int = G_PARAM_SPEC_INT (pspec);
		adjustment = gtk_adjustment_new (
			(gdouble) pspec_int->default_value,
			(gdouble) pspec_int->minimum,
			(gdouble) pspec_int->maximum,
			1.0, 1.0, 0.0);

	} else if (G_IS_PARAM_SPEC_UINT (pspec)) {
		GParamSpecUInt *pspec_uint;
		pspec_uint = G_PARAM_SPEC_UINT (pspec);
		adjustment = gtk_adjustment_new (
			(gdouble) pspec_uint->default_value,
			(gdouble) pspec_uint->minimum,
			(gdouble) pspec_uint->maximum,
			1.0, 1.0, 0.0);

	} else if (G_IS_PARAM_SPEC_LONG (pspec)) {
		GParamSpecLong *pspec_long;
		pspec_long = G_PARAM_SPEC_LONG (pspec);
		adjustment = gtk_adjustment_new (
			(gdouble) pspec_long->default_value,
			(gdouble) pspec_long->minimum,
			(gdouble) pspec_long->maximum,
			1.0, 1.0, 0.0);

	} else if (G_IS_PARAM_SPEC_ULONG (pspec)) {
		GParamSpecULong *pspec_ulong;
		pspec_ulong = G_PARAM_SPEC_ULONG (pspec);
		adjustment = gtk_adjustment_new (
			(gdouble) pspec_ulong->default_value,
			(gdouble) pspec_ulong->minimum,
			(gdouble) pspec_ulong->maximum,
			1.0, 1.0, 0.0);

	} else if (G_IS_PARAM_SPEC_FLOAT (pspec)) {
		GParamSpecFloat *pspec_float;
		pspec_float = G_PARAM_SPEC_FLOAT (pspec);
		adjustment = gtk_adjustment_new (
			(gdouble) pspec_float->default_value,
			(gdouble) pspec_float->minimum,
			(gdouble) pspec_float->maximum,
			1.0, 1.0, 0.0);

	} else if (G_IS_PARAM_SPEC_DOUBLE (pspec)) {
		GParamSpecDouble *pspec_double;
		pspec_double = G_PARAM_SPEC_DOUBLE (pspec);
		adjustment = gtk_adjustment_new (
			(gdouble) pspec_double->default_value,
			(gdouble) pspec_double->minimum,
			(gdouble) pspec_double->maximum,
			1.0, 1.0, 0.0);

	} else
		g_return_val_if_reached (NULL);

	pre = g_alloca (strlen (conf->text) + 1);
	strcpy (pre, conf->text);
	post = strstr (pre, "%s");
	if (post != NULL) {
		*post = '\0';
		post += 2;
	}

	hbox = gtk_hbox_new (FALSE, 3);

	if (use_pspec != NULL) {
		prefix = gtk_check_button_new_with_mnemonic (pre);

		g_object_bind_property (
			service->settings, use_pspec->name,
			prefix, "active",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);
	} else {
		prefix = gtk_label_new_with_mnemonic (pre);
	}
	gtk_box_pack_start (GTK_BOX (hbox), prefix, FALSE, TRUE, 0);
	gtk_widget_show (prefix);

	spin = gtk_spin_button_new (adjustment, 1.0, 0);
	gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, TRUE, 0);
	gtk_widget_show (spin);

	g_object_bind_property (
		service->settings, conf->name,
		spin, "value",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	if (use_pspec != NULL)
		g_object_bind_property (
			prefix, "active",
			spin, "sensitive",
			G_BINDING_SYNC_CREATE);

	if (post != NULL) {
		GtkWidget *label = gtk_label_new_with_mnemonic (post);
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), prefix);
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
		gtk_widget_show (label);
	}

	if (conf->depname != NULL) {
		g_object_bind_property (
			service->settings, conf->depname,
			hbox, "sensitive",
			G_BINDING_SYNC_CREATE);
		gtk_widget_set_margin_left (hbox, INDENT_MARGIN);
	}

	return hbox;
}

/* 'values' is in format "nick0:caption0:nick1:caption1:...nickN:captionN"
 * where 'nick' is the nickname of a GEnumValue belonging to a GEnumClass
 * determined by the type of the GObject property named "name". */
static GtkWidget *
emae_option_options (EMAccountEditorService *service,
                     CamelProviderConfEntry *conf,
                     GtkLabel *label)
{
	CamelProvider *provider;
	GtkWidget *widget;
	GtkListStore *store;
	GtkTreeIter iter;
	const gchar *p;
	GtkCellRenderer *renderer;

	provider = camel_provider_get (service->protocol, NULL);
	g_return_val_if_fail (provider != NULL, NULL);

	/* nick and caption */
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	p = conf->value;
	while (p != NULL) {
		const gchar *nick;
		const gchar *caption;
		gchar *vl, *cp;

		nick = p;
		caption = strchr (p, ':');
		if (caption) {
			caption++;
		} else {
			g_warning (G_STRLOC ": expected ':' not found at '%s'", p);
			break;
		}
		p = strchr (caption, ':');

		vl = g_strndup (nick, caption - nick - 1);
		if (p) {
			p++;
			cp = g_strndup (caption, p - caption - 1);
		} else
			cp = g_strdup (caption);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter, 0, vl, 1, dgettext (
			provider->translation_domain, cp), -1);

		g_free (vl);
		g_free (cp);
	}

	widget = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	gtk_combo_box_set_id_column (GTK_COMBO_BOX (widget), 0);
	gtk_widget_show (widget);

	g_object_bind_property_full (
		service->settings, conf->name,
		widget, "active-id",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_enum_value_to_nick,
		e_binding_transform_enum_nick_to_value,
		NULL, (GDestroyNotify) NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), renderer, TRUE);
	gtk_cell_layout_set_attributes (
		GTK_CELL_LAYOUT (widget), renderer, "text", 1, NULL);

	gtk_label_set_mnemonic_widget (label, widget);

	return widget;
}

static GtkWidget *
emae_receive_options_item (EConfig *ec,
                           EConfigItem *item,
                           GtkWidget *parent,
                           GtkWidget *old,
                           gint position,
                           gpointer data)
{
	EMAccountEditor *emae = data;
	CamelProvider *provider;
	GtkWidget *w, *box, *spin;
	guint row;

	provider = emae_get_store_provider (emae);

	if (provider == NULL || provider->extra_conf == NULL)
		return NULL;

	if (old) {
		if (emae->type == EMAE_PAGES) {
			GtkWidget *box = gtk_hbox_new (FALSE, 12);
			gtk_widget_reparent (old, box);
			gtk_widget_show (box);
			gtk_box_set_child_packing ((GtkBox *) box, old, TRUE, TRUE, 12, GTK_PACK_START);
			gtk_box_pack_end ((GtkBox *) emae->pages[2], box, FALSE, FALSE, 0);
		}
		return old;
	}

	if (emae->type == EMAE_PAGES)  {
		GtkWidget *box = gtk_hbox_new (FALSE, 12);
		gtk_widget_reparent (parent, box);
		gtk_widget_show (box);
		gtk_box_set_child_packing ((GtkBox *) box, parent, TRUE, TRUE, 12, GTK_PACK_START);
		gtk_box_pack_start ((GtkBox *) emae->pages[2], box, FALSE, FALSE, 0);
	}

	/* We have to add the automatic mail check item with the rest of the receive options */
	g_object_get (parent, "n-rows", &row, NULL);

	box = gtk_hbox_new (FALSE, 4);
	w = gtk_check_button_new_with_mnemonic (_("Check for _new messages every"));
	emae_account_toggle_widget (emae, (GtkToggleButton *) w, E_ACCOUNT_SOURCE_AUTO_CHECK);
	gtk_box_pack_start ((GtkBox *) box, w, FALSE, FALSE, 0);

	spin = gtk_spin_button_new_with_range (1.0, 1440.0, 1.0);
	emae_account_spinint_widget (emae, (GtkSpinButton *) spin, E_ACCOUNT_SOURCE_AUTO_CHECK_TIME);
	gtk_box_pack_start ((GtkBox *) box, spin, FALSE, TRUE, 0);

	w = gtk_label_new_with_mnemonic (_("minu_tes"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (w), spin);
	gtk_box_pack_start ((GtkBox *) box, w, FALSE, FALSE, 0);

	gtk_widget_show_all (box);

	gtk_table_attach ((GtkTable *) parent, box, 0, 2, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	return box;
}

static GtkWidget *
emae_receive_options_extra_item (EConfig *ec,
                                 EConfigItem *eitem,
                                 GtkWidget *parent,
                                 GtkWidget *old,
                                 gint position,
                                 gpointer data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorService *service;
	struct _receive_options_item *item = (struct _receive_options_item *) eitem;
	GtkWidget *box;
	GtkWidget *widget;
	GtkLabel *label;
	GtkTable *table;
	CamelProvider *provider;
	CamelProviderConfEntry *entries;
	guint row;
	GHashTable *extra;
	CamelURL *url;
	const gchar *section_name;
	gint ii;

	service = &emae->priv->source;
	section_name = eitem->user_data;

	provider = emae_get_store_provider (emae);

	if (provider == NULL || provider->extra_conf == NULL)
		return NULL;

	entries = provider->extra_conf;

	if (emae->type == EMAE_PAGES) {
		GtkWidget *box;

		box = gtk_hbox_new (FALSE, 12);
		gtk_widget_reparent (parent, box);
		gtk_widget_show (box);
		gtk_box_set_child_packing (
			GTK_BOX (box), parent,
			TRUE, TRUE, 12, GTK_PACK_START);
		gtk_box_pack_start (
			GTK_BOX (emae->pages[2]), box, FALSE, FALSE, 0);
	}

	for (ii = 0; entries && entries[ii].type != CAMEL_PROVIDER_CONF_END; ii++)
		if (entries[ii].type == CAMEL_PROVIDER_CONF_SECTION_START
		    && g_strcmp0 (entries[ii].name, section_name) == 0)
			goto section;

	return NULL;

section:
	d (printf ("Building extra section '%s'\n", eitem->path));
	widget = NULL;
	url = emae_account_url (
		emae, emae_service_info[service->type].account_uri_key);
	item->extra_table = g_hash_table_new (g_str_hash, g_str_equal);
	extra = g_hash_table_new (g_str_hash, g_str_equal);

	table = GTK_TABLE (parent);
	g_object_get (table, "n-rows", &row, NULL);

	for (; entries[ii].type != CAMEL_PROVIDER_CONF_END && entries[ii].type != CAMEL_PROVIDER_CONF_SECTION_END; ii++) {
		switch (entries[ii].type) {
		case CAMEL_PROVIDER_CONF_SECTION_START:
		case CAMEL_PROVIDER_CONF_SECTION_END:
			break;

		case CAMEL_PROVIDER_CONF_LABEL:
			/* FIXME This is a hack for exchange connector,
			 *       labels should be removed from confentry. */
			if (!strcmp (entries[ii].name, "hostname"))
				label = emae->priv->source.hostlabel;
			else if (!strcmp (entries[ii].name, "username"))
				label = emae->priv->source.userlabel;
			else
				label = NULL;

			if (label != NULL)
				gtk_label_set_text_with_mnemonic (
					label, entries[ii].text);
			break;

		case CAMEL_PROVIDER_CONF_CHECKBOX:
			widget = emae_option_toggle (service, &entries[ii]);
			gtk_table_attach (
				table, widget, 0, 2,
				row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
			gtk_widget_show (widget);

			g_hash_table_insert (
				extra, (gpointer) entries[ii].name, widget);

			row++;

			/* HACK: keep_on_server is stored in the e-account,
			 * but is displayed as a properly on the uri, make
			 * sure they track/match here. */
			if (strcmp (entries[ii].name, "keep-on-server") == 0)
				emae_account_toggle_widget (
					emae, (GtkToggleButton *) widget,
					E_ACCOUNT_SOURCE_KEEP_ON_SERVER);
			break;

		case CAMEL_PROVIDER_CONF_ENTRY:
			widget = gtk_label_new_with_mnemonic (entries[ii].text);
			gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
			gtk_table_attach (
				table, widget, 0, 1,
				row, row + 1, GTK_FILL, 0, 0, 0);
			gtk_widget_show (widget);

			label = GTK_LABEL (widget);

			widget = emae_option_entry (
				service, &entries[ii], label);
			gtk_table_attach (
				table, widget, 1, 2,
				row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
			gtk_widget_show (widget);

			row++;

			/* FIXME This is another hack for
			 *       exchange/groupwise connector. */
			g_hash_table_insert (
				item->extra_table,
				(gpointer) entries[ii].name, widget);
			break;

		case CAMEL_PROVIDER_CONF_CHECKSPIN:
			widget = emae_option_checkspin (service, &entries[ii]);
			gtk_table_attach (
				table, widget, 0, 2,
				row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
			gtk_widget_show (widget);
			row++;
			break;

		case CAMEL_PROVIDER_CONF_OPTIONS:
			box = gtk_hbox_new (FALSE, 4);
			gtk_table_attach (
				table, box, 0, 2,
				row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
			gtk_widget_show (box);

			widget = gtk_label_new_with_mnemonic (entries[ii].text);
			gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
			gtk_box_pack_start (
				GTK_BOX (box), widget, FALSE, FALSE, 0);
			gtk_widget_show (widget);

			label = GTK_LABEL (widget);

			widget = emae_option_options (
				service, &entries[ii], label);
			gtk_box_pack_start (
				GTK_BOX (box), widget, FALSE, FALSE, 0);
			gtk_widget_show (widget);

			row++;
			break;

		default:
			break;
		}
	}

	camel_url_free (url);

	if (widget != NULL)
		gtk_widget_show (widget);

	return widget;
}

static GtkWidget *
emae_send_page (EConfig *ec,
                EConfigItem *item,
                GtkWidget *parent,
                GtkWidget *old,
                gint position,
                gpointer data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *priv = emae->priv;
	CamelProvider *provider;
	GtkWidget *w;
	GtkBuilder *builder;

	provider = emae_get_store_provider (emae);

	/* no transport options page at all for these types of providers */
	if (provider && CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (provider)) {
		memset (&priv->transport.frame, 0, ((gchar *) &priv->transport.check_dialog) - ((gchar *) &priv->transport.frame));
		return NULL;
	}

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "mail-config.ui");

	/* Transport */
	priv->transport.type = CAMEL_PROVIDER_TRANSPORT;
	emae_setup_service (emae, &priv->transport, builder);

	w = e_builder_get_widget (builder, item->label);
	if (emae->type == EMAE_PAGES) {
		gtk_box_pack_start ((GtkBox *) emae->pages[3], w, TRUE, TRUE, 0);
	} else if (((EConfig *) priv->config)->type == E_CONFIG_ASSISTANT) {
		GtkWidget *page;

		page = emae_create_basic_assistant_page (
			emae, GTK_ASSISTANT (parent),
			"transport_page", position);

		gtk_box_pack_start (GTK_BOX (page), w, TRUE, TRUE, 0);

		w = page;
	} else {
		gtk_notebook_insert_page (
			GTK_NOTEBOOK (parent), w,
			gtk_label_new (_("Sending Email")),
			position);
		gtk_container_child_set (
			GTK_CONTAINER (parent), w,
			"tab-fill", FALSE, "tab-expand", FALSE, NULL);
	}

	emae_queue_widgets (
		emae, builder,
		"transport-type-table",
		"transport-server-table",
		"transport-security-table",
		"transport-auth-table",
		NULL);

	g_object_unref (builder);

	return w;
}

static void
emae_real_url_toggled (GtkToggleButton *check,
                       EMFolderSelectionButton *button)
{
	if (!gtk_toggle_button_get_active (check))
		em_folder_selection_button_set_folder_uri (button, "");
}

static void
set_real_folder_path (GtkButton *folder_button,
                      CamelSettings *settings,
                      const gchar *settings_prop,
                      EAccount *account)
{
	gchar *path = NULL, *uri;
	gchar *encoded_name;
	gchar *encoded_uid;
	const gchar *folder_name;

	g_return_if_fail (folder_button != NULL);
	g_return_if_fail (settings != NULL);
	g_return_if_fail (settings_prop != NULL);
	g_return_if_fail (account != NULL);

	g_object_get (G_OBJECT (settings), settings_prop, &path, NULL);

	if (!path || !*path) {
		g_free (path);
		return;
	}

	folder_name = path;

	/* Skip the leading slash, if present. */
	if (*folder_name == '/')
		folder_name++;

	encoded_uid = camel_url_encode (account->uid, ":;@/");
	encoded_name = camel_url_encode (folder_name, "#");

	uri = g_strdup_printf ("folder://%s/%s", encoded_uid, encoded_name);

	g_free (encoded_uid);
	g_free (encoded_name);
	g_free (path);

	em_folder_selection_button_set_folder_uri (EM_FOLDER_SELECTION_BUTTON (folder_button), uri);

	g_free (uri);
}

static void
update_real_folder_cb (GtkButton *folder_button,
                       GParamSpec *par_spec,
                       EMAccountEditor *emae)
{
	EMFolderSelectionButton *sel_button;
	CamelSettings *settings;
	const gchar *prop_name = NULL;
	const gchar *folder_uri;
	gchar *path = NULL;

	g_return_if_fail (folder_button != NULL);
	g_return_if_fail (emae != NULL);
	g_return_if_fail (emae->priv != NULL);

	settings = emae->priv->source.settings;
	if (folder_button == emae->priv->trash_folder_button)
		prop_name = "real-trash-path";
	else if (folder_button == emae->priv->junk_folder_button)
		prop_name = "real-junk-path";

	g_return_if_fail (prop_name != NULL);

	sel_button = EM_FOLDER_SELECTION_BUTTON (folder_button);
	g_return_if_fail (sel_button != NULL);

	folder_uri = em_folder_selection_button_get_folder_uri (sel_button);
	if (folder_uri && *folder_uri) {
		EMailSession *session;

		session = em_folder_selection_button_get_session (sel_button);
		if (!e_mail_folder_uri_parse (CAMEL_SESSION (session), folder_uri, NULL, &path, NULL))
			path = NULL;
	}

	g_object_set (G_OBJECT (settings), prop_name, path, NULL);
	g_free (path);
}

static GtkWidget *
emae_defaults_page (EConfig *ec,
                    EConfigItem *item,
                    GtkWidget *parent,
                    GtkWidget *old,
                    gint position,
                    gpointer data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *priv = emae->priv;
	EMFolderSelectionButton *button;
	CamelProviderFlags flags;
	CamelProvider *provider;
	CamelSettings *settings;
	CamelStore *store = NULL;
	EMailBackend *backend;
	EMailSession *session;
	EAccount *account;
	GtkWidget *widget;
	GtkBuilder *builder;
	GParamSpec *pspec;
	gboolean visible;

	/*if (old)
	  return old;*/
	if (((EConfig *) priv->config)->type == E_CONFIG_ASSISTANT && emae->type != EMAE_PAGES)
		return NULL;

	account = em_account_editor_get_modified_account (emae);
	backend = em_account_editor_get_backend (emae);

	session = e_mail_backend_get_session (backend);

	if (account != NULL) {
		CamelService *service;

		service = camel_session_get_service (
			CAMEL_SESSION (session), account->uid);

		if (CAMEL_IS_STORE (service))
			store = CAMEL_STORE (service);
	}

	provider = emae_get_store_provider (emae);
	settings = emae->priv->source.settings;

	/* Make sure we have a valid EMailBackend. */
	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), NULL);

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "mail-config.ui");

	/* Special folders */
	button = emae_account_folder (
		emae, "drafts_button",
		E_ACCOUNT_DRAFTS_FOLDER_URI,
		E_MAIL_LOCAL_FOLDER_DRAFTS, builder);
	priv->drafts_folder_button = GTK_BUTTON (button);

	button = emae_account_folder (
		emae, "sent_button",
		E_ACCOUNT_SENT_FOLDER_URI,
		E_MAIL_LOCAL_FOLDER_SENT, builder);
	priv->sent_folder_button = GTK_BUTTON (button);

	widget = e_builder_get_widget (builder, "trash_folder_check");
	priv->trash_folder_check = GTK_TOGGLE_BUTTON (widget);

	widget = e_builder_get_widget (builder, "trash_folder_butt");
	button = EM_FOLDER_SELECTION_BUTTON (widget);
	em_folder_selection_button_set_session (button, session);
	em_folder_selection_button_set_store (button, store);
	priv->trash_folder_button = GTK_BUTTON (button);

	g_signal_connect (
		priv->trash_folder_check, "toggled",
		G_CALLBACK (emae_real_url_toggled),
		priv->trash_folder_button);

	g_object_bind_property (
		priv->trash_folder_check, "active",
		priv->trash_folder_button, "sensitive",
		G_BINDING_SYNC_CREATE);

	if (settings != NULL)
		pspec = g_object_class_find_property (
			G_OBJECT_GET_CLASS (settings),
			"use-real-trash-path");
	else
		pspec = NULL;

	if (pspec != NULL)
		g_object_bind_property (
			settings, "use-real-trash-path",
			priv->trash_folder_check, "active",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);

	if (settings != NULL)
		pspec = g_object_class_find_property (
			G_OBJECT_GET_CLASS (settings),
			"real-trash-path");
	else
		pspec = NULL;

	if (pspec != NULL) {
		set_real_folder_path (
			priv->trash_folder_button,
			settings, "real-trash-path", account);
		g_signal_connect (
			priv->trash_folder_button, "notify::folder-uri",
			G_CALLBACK (update_real_folder_cb), emae);
	}

	flags = CAMEL_PROVIDER_ALLOW_REAL_TRASH_FOLDER;
	visible =
		(provider != NULL) &&
		((provider->flags & flags) != 0);
	widget = GTK_WIDGET (priv->trash_folder_check);
	gtk_widget_set_visible (widget, visible);
	widget = GTK_WIDGET (priv->trash_folder_button);
	gtk_widget_set_visible (widget, visible);

	widget = e_builder_get_widget (builder, "junk_folder_check");
	priv->junk_folder_check = GTK_TOGGLE_BUTTON (widget);

	widget = e_builder_get_widget (builder, "junk_folder_butt");
	button = EM_FOLDER_SELECTION_BUTTON (widget);
	em_folder_selection_button_set_session (button, session);
	em_folder_selection_button_set_store (button, store);
	priv->junk_folder_button = GTK_BUTTON (button);

	g_signal_connect (
		priv->junk_folder_check, "toggled",
		G_CALLBACK (emae_real_url_toggled),
		priv->junk_folder_button);

	g_object_bind_property (
		priv->junk_folder_check, "active",
		priv->junk_folder_button, "sensitive",
		G_BINDING_SYNC_CREATE);

	if (settings != NULL)
		pspec = g_object_class_find_property (
			G_OBJECT_GET_CLASS (settings),
			"use-real-junk-path");
	else
		pspec = NULL;

	if (pspec != NULL)
		g_object_bind_property (
			settings, "use-real-junk-path",
			priv->junk_folder_check, "active",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);

	if (settings != NULL)
		pspec = g_object_class_find_property (
			G_OBJECT_GET_CLASS (settings),
			"real-junk-path");
	else
		pspec = NULL;

	if (pspec != NULL) {
		set_real_folder_path (
			priv->junk_folder_button,
			settings, "real-junk-path", account);
		g_signal_connect (
			priv->junk_folder_button, "notify::folder-uri",
			G_CALLBACK (update_real_folder_cb), emae);
	}

	flags = CAMEL_PROVIDER_ALLOW_REAL_JUNK_FOLDER;
	visible =
		(provider != NULL) &&
		((provider->flags & flags) != 0);
	widget = GTK_WIDGET (priv->junk_folder_check);
	gtk_widget_set_visible (widget, visible);
	widget = GTK_WIDGET (priv->junk_folder_button);
	gtk_widget_set_visible (widget, visible);

	/* Special Folders "Reset Defaults" button */
	priv->restore_folders_button = (GtkButton *)e_builder_get_widget (builder, "default_folders_button");
	g_signal_connect (
		priv->restore_folders_button, "clicked",
		G_CALLBACK (default_folders_clicked), emae);

	/* Always Cc/Bcc */
	emae_account_toggle (emae, "always_cc", E_ACCOUNT_CC_ALWAYS, builder);
	emae_account_entry (emae, "cc_addrs", E_ACCOUNT_CC_ADDRS, builder);
	emae_account_toggle (emae, "always_bcc", E_ACCOUNT_BCC_ALWAYS, builder);
	emae_account_entry (emae, "bcc_addrs", E_ACCOUNT_BCC_ADDRS, builder);

	gtk_widget_set_sensitive (
		GTK_WIDGET (priv->sent_folder_button),
		(provider ? !(provider->flags & CAMEL_PROVIDER_DISABLE_SENT_FOLDER) : TRUE));

	gtk_widget_set_sensitive (
		GTK_WIDGET (priv->restore_folders_button),
		(provider && !(provider->flags & CAMEL_PROVIDER_DISABLE_SENT_FOLDER)));

	/* Receipt policy */
	emae_setup_receipt_policy (emae, builder);

	widget = e_builder_get_widget (builder, item->label);
	if (emae->type == EMAE_PAGES) {
		gtk_box_pack_start ((GtkBox *) emae->pages[4], widget, TRUE, TRUE, 0);
		gtk_widget_show  (widget);
	} else {
		gtk_notebook_insert_page (
			GTK_NOTEBOOK (parent), widget,
			gtk_label_new (_("Defaults")),
			position);
		gtk_container_child_set (
			GTK_CONTAINER (parent), widget,
			"tab-fill", FALSE, "tab-expand", FALSE, NULL);
	}

	emae_queue_widgets (
		emae, builder,
		"special-folders-table",
		"composing-messages-table",
		NULL);

	g_object_unref (builder);

	return widget;
}

static void
emae_account_hash_algo_combo_changed_cb (GtkComboBox *combobox,
                                         EMAccountEditor *emae)
{
	EAccount *account;
	gpointer data;
	const gchar *text = NULL;

	account = em_account_editor_get_modified_account (emae);
	data = g_object_get_data (G_OBJECT (combobox), "account-item");

	switch (gtk_combo_box_get_active (combobox)) {
	case 1: text = "sha1";
		break;
	case 2: text = "sha256";
		break;
	case 3:
		text = "sha384";
		break;
	case 4:
		text = "sha512";
		break;
	}

	e_account_set_string (account, GPOINTER_TO_INT (data), text);
}

static GtkComboBox *
emae_account_hash_algo_combo (EMAccountEditor *emae,
                              const gchar *name,
                              gint item,
                              GtkBuilder *builder)
{
	EAccount *account;
	GtkComboBox *combobox;
	const gchar *text;
	gint index = 0;

	account = em_account_editor_get_modified_account (emae);
	combobox = GTK_COMBO_BOX (e_builder_get_widget (builder, name));
	g_return_val_if_fail (combobox != NULL, NULL);

	text = e_account_get_string (account, item);
	if (text) {
		if (g_ascii_strcasecmp (text, "sha1") == 0)
			index = 1;
		else if (g_ascii_strcasecmp (text, "sha256") == 0)
			index = 2;
		else if (g_ascii_strcasecmp (text, "sha384") == 0)
			index = 3;
		else if (g_ascii_strcasecmp (text, "sha512") == 0)
			index = 4;
	}

	gtk_combo_box_set_active (combobox, index);

	g_object_set_data (G_OBJECT (combobox), "account-item", GINT_TO_POINTER (item));
	g_signal_connect (combobox, "changed", G_CALLBACK (emae_account_hash_algo_combo_changed_cb), emae);

	return combobox;
}

static GtkWidget *
emae_security_page (EConfig *ec,
                    EConfigItem *item,
                    GtkWidget *parent,
                    GtkWidget *old,
                    gint position,
                    gpointer data)
{
	EMAccountEditor *emae = data;
#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
	EMAccountEditorPrivate *priv = emae->priv;
#endif
	GtkWidget *w;
	GtkBuilder *builder;

	/*if (old)
	  return old;*/

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "mail-config.ui");

	/* Security */
	emae_account_entry (emae, "pgp_key", E_ACCOUNT_PGP_KEY, builder);
	emae_account_hash_algo_combo (emae, "pgp_hash_algo", E_ACCOUNT_PGP_HASH_ALGORITHM, builder);
	emae_account_toggle (emae, "pgp_encrypt_to_self", E_ACCOUNT_PGP_ENCRYPT_TO_SELF, builder);
	emae_account_toggle (emae, "pgp_always_sign", E_ACCOUNT_PGP_ALWAYS_SIGN, builder);
	emae_account_toggle (emae, "pgp_no_imip_sign", E_ACCOUNT_PGP_NO_IMIP_SIGN, builder);
	emae_account_toggle (emae, "pgp_always_trust", E_ACCOUNT_PGP_ALWAYS_TRUST, builder);

#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
	/* TODO: this should handle its entry separately? */
	priv->smime_sign_key = emae_account_entry (emae, "smime_sign_key", E_ACCOUNT_SMIME_SIGN_KEY, builder);
	priv->smime_sign_key_select = (GtkButton *)e_builder_get_widget (builder, "smime_sign_key_select");
	priv->smime_sign_key_clear = (GtkButton *)e_builder_get_widget (builder, "smime_sign_key_clear");
	g_signal_connect (priv->smime_sign_key_select, "clicked", G_CALLBACK(smime_sign_key_select), emae);
	g_signal_connect (priv->smime_sign_key_clear, "clicked", G_CALLBACK(smime_sign_key_clear), emae);

	emae_account_hash_algo_combo (emae, "smime_hash_algo", E_ACCOUNT_SMIME_HASH_ALGORITHM, builder);
	priv->smime_sign_default = emae_account_toggle (emae, "smime_sign_default", E_ACCOUNT_SMIME_SIGN_DEFAULT, builder);

	priv->smime_encrypt_key = emae_account_entry (emae, "smime_encrypt_key", E_ACCOUNT_SMIME_ENCRYPT_KEY, builder);
	priv->smime_encrypt_key_select = (GtkButton *)e_builder_get_widget (builder, "smime_encrypt_key_select");
	priv->smime_encrypt_key_clear = (GtkButton *)e_builder_get_widget (builder, "smime_encrypt_key_clear");
	g_signal_connect (priv->smime_encrypt_key_select, "clicked", G_CALLBACK(smime_encrypt_key_select), emae);
	g_signal_connect (priv->smime_encrypt_key_clear, "clicked", G_CALLBACK(smime_encrypt_key_clear), emae);

	priv->smime_encrypt_default = emae_account_toggle (emae, "smime_encrypt_default", E_ACCOUNT_SMIME_ENCRYPT_DEFAULT, builder);
	priv->smime_encrypt_to_self = emae_account_toggle (emae, "smime_encrypt_to_self", E_ACCOUNT_SMIME_ENCRYPT_TO_SELF, builder);
	smime_changed (emae);
#else
	{
		/* Since we don't have NSS, hide the S/MIME config options */
		GtkWidget *frame;

		frame = e_builder_get_widget (builder, "smime_vbox");
		gtk_widget_destroy (frame);
	}
#endif /* HAVE_NSS */

	w = e_builder_get_widget (builder, item->label);
	gtk_notebook_insert_page (
		GTK_NOTEBOOK (parent), w,
		gtk_label_new (_("Security")),
		position);
	gtk_container_child_set (
		GTK_CONTAINER (parent), w,
		"tab-fill", FALSE, "tab-expand", FALSE, NULL);

	g_object_unref (builder);

	return w;
}

/*
 * Allow some level of post creation customisation in plugins.
 */
GtkWidget *
em_account_editor_get_widget (EMAccountEditor *emae,
                              const gchar *name)
{
	GtkWidget *wid;

	wid = g_hash_table_lookup (emae->priv->widgets, name);
	if (wid)
		return wid;

	g_warning ("Mail account widget '%s' not found", name);

	return NULL;
}

static GtkWidget *
emae_widget_glade (EConfig *ec,
                   EConfigItem *item,
                   GtkWidget *parent,
                   GtkWidget *old,
                   gint position,
                   gpointer data)
{
	return em_account_editor_get_widget (data, item->label);
}

/* plugin meta-data for "org.gnome.evolution.mail.config.accountEditor" */
static EMConfigItem emae_editor_items[] = {
	{ E_CONFIG_BOOK, (gchar *) "" },
	{ E_CONFIG_PAGE, (gchar *) "00.identity", (gchar *) "vboxIdentityBorder", emae_identity_page },
	{ E_CONFIG_SECTION, (gchar *) "00.identity/00.name", (gchar *) "account_vbox", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "00.identity/10.required", (gchar *) "identity-required-table", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "00.identity/20.info", (gchar *) "identity-optional-table", emae_widget_glade },

	{ E_CONFIG_PAGE, (gchar *) "10.receive", (gchar *) "vboxSourceBorder", emae_receive_page },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "10.receive/00.type", (gchar *) "source-type-table", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "10.receive/10.config", (gchar *) "source-config-table", emae_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "10.receive/20.security", (gchar *) "source-security-vbox", emae_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "10.receive/30.auth", (gchar *) "source-auth-vbox", emae_widget_glade },

	/* Most sections for this is auto-generated from the camel config */
	{ E_CONFIG_PAGE, (gchar *) "20.receive_options", (gchar *) N_("Receiving Options"), },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "20.receive_options/10.mailcheck", (gchar *) N_("Checking for New Messages"), },
	{ E_CONFIG_ITEM_TABLE, (gchar *) "20.receive_options/10.mailcheck/00.autocheck", NULL, emae_receive_options_item, },

	{ E_CONFIG_PAGE, (gchar *) "30.send", (gchar *) "vboxTransportBorder", emae_send_page },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "30.send/00.type", (gchar *) "transport-type-table", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "30.send/10.config", (gchar *) "transport-server-table", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "30.send/20.security", (gchar *) "transport-security-table", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "30.send/30.auth", (gchar *) "transport-auth-table", emae_widget_glade },

	{ E_CONFIG_PAGE, (gchar *) "40.defaults", (gchar *) "vboxFoldersBorder", emae_defaults_page },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "40.defaults/00.folders", (gchar *) "special-folders-table", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "40.defaults/10.composing", (gchar *) "composing-messages-table", emae_widget_glade },

	{ E_CONFIG_PAGE, (gchar *) "50.security", (gchar *) "vboxSecurityBorder", emae_security_page },
	/* 1x1 table (!) not vbox: { E_CONFIG_SECTION, "50.security/00.gpg", "table19", emae_widget_glade }, */
	/* table not vbox: { E_CONFIG_SECTION, "50.security/10.smime", "smime_table", emae_widget_glade }, */
	{ 0 },
};
static gboolean emae_editor_items_translated = FALSE;

static GtkWidget *
emae_review_page (EConfig *ec,
                      EConfigItem *item,
                      GtkWidget *parent,
                      GtkWidget *old,
                      gint position,
                      gpointer data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *priv = emae->priv;
	GtkWidget *w;
	GtkBuilder *builder;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "mail-config.ui");

	priv->review_name = (GtkLabel*) e_builder_get_widget (builder, "personal-name-entry");
	priv->review_email = (GtkLabel*) e_builder_get_widget (builder, "personal-email-entry");
	priv->receive_stype = (GtkLabel*) e_builder_get_widget (builder, "receive_server_type");
	priv->send_stype = (GtkLabel*) e_builder_get_widget (builder, "send_server_type");
	priv->receive_saddress = (GtkLabel*) e_builder_get_widget (builder, "receive_server_address");
	priv->send_saddress = (GtkLabel*) e_builder_get_widget (builder, "send_server_address");
	priv->receive_name = (GtkLabel*) e_builder_get_widget (builder, "receive_username");
	priv->send_name = (GtkLabel*) e_builder_get_widget (builder, "send_username");
	priv->receive_encryption = (GtkLabel*) e_builder_get_widget (builder, "receive_encryption");
	priv->send_encryption = (GtkLabel*) e_builder_get_widget (builder, "send_encryption");

	w = e_builder_get_widget (builder, item->label);
	priv->review_box = gtk_vbox_new (FALSE, 2);
	gtk_widget_show (priv->review_box);
	if (((EConfig *) priv->config)->type == E_CONFIG_ASSISTANT) {
		GtkWidget *page;

		page = emae_create_basic_assistant_page (
			emae, GTK_ASSISTANT (parent),
			"review_page", position);

		gtk_box_pack_start (GTK_BOX (page), w, FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (page), priv->review_box, FALSE, FALSE, 0);
		gtk_widget_reparent (priv->management_frame, page);

		w = page;
	}

	return w;
}

static GtkWidget *
emae_widget_assistant_page (EConfig *ec,
                            EConfigItem *item,
                            GtkWidget *parent,
                            GtkWidget *old,
                            gint position,
                            gpointer data)
{
	EMAccountEditor *emae = (EMAccountEditor *) data;

	if (emae->type == EMAE_PAGES)
		return NULL;

	return emae_create_basic_assistant_page (
		emae, GTK_ASSISTANT (parent), item->label, position);
}

/* plugin meta-data for "org.gnome.evolution.mail.config.accountAssistant" */
static EMConfigItem emae_assistant_items[] = {
	{ E_CONFIG_ASSISTANT, (gchar *) "" },
	{ E_CONFIG_PAGE_START, (gchar *) "0.start", (gchar *) "start_page", emae_widget_assistant_page },

	{ E_CONFIG_PAGE, (gchar *) "00.identity", (gchar *) "vboxIdentityBorder", emae_identity_page },
	{ E_CONFIG_SECTION, (gchar *) "00.identity/00.name", (gchar *) "account_vbox", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "00.identity/10.required", (gchar *) "identity-required-table", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "00.identity/20.info", (gchar *) "identity-optional-table", emae_widget_glade },

	{ E_CONFIG_PAGE, (gchar *) "10.receive", (gchar *) "vboxSourceBorder", emae_receive_page },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "10.receive/00.type", (gchar *) "source-type-table", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "10.receive/10.config", (gchar *) "source-config-table", emae_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "10.receive/20.security", (gchar *) "source-security-vbox", emae_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "10.receive/30.auth", (gchar *) "source-auth-vbox", emae_widget_glade },

	/* Most sections for this is auto-generated fromt the camel config */
	{ E_CONFIG_PAGE, (gchar *) "20.receive_options", (gchar *) N_("Receiving Options"), },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "20.receive_options/10.mailcheck", (gchar *) N_("Checking for New Messages"), },
	{ E_CONFIG_ITEM_TABLE, (gchar *) "20.receive_options/10.mailcheck/00.autocheck", NULL, emae_receive_options_item, },

	{ E_CONFIG_PAGE, (gchar *) "30.send", (gchar *) "vboxTransportBorder", emae_send_page },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "30.send/00.type", (gchar *) "transport-type-table", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "30.send/10.config", (gchar *) "transport-server-table", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "30.send/20.security", (gchar *) "transport-security-table", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "30.send/30.auth", (gchar *) "transport-auth-table", emae_widget_glade },

	{ E_CONFIG_PAGE, (gchar *) "40.defaults", (gchar *) "vboxFoldersBorder", emae_defaults_page },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "40.defaults/00.folders", (gchar *) "special-folders-table", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "40.defaults/10.composing", (gchar *) "composing-messages-table", emae_widget_glade },

	{ E_CONFIG_PAGE, (gchar *) "50.review", (gchar *) "vboxReviewBorder", emae_review_page },

	{ E_CONFIG_PAGE_FINISH, (gchar *) "999.end", (gchar *) "finish_page", emae_widget_assistant_page },
	{ 0 },
};
static gboolean emae_assistant_items_translated = FALSE;

static void
emae_free (EConfig *ec,
           GSList *items,
           gpointer data)
{
	g_slist_free (items);
}

static void
emae_free_auto (EConfig *ec,
                GSList *items,
                gpointer data)
{
	GSList *l, *n;

	for (l = items; l;) {
		struct _receive_options_item *item = l->data;

		n = g_slist_next (l);
		g_free (item->item.path);
		if (item->extra_table)
			g_hash_table_destroy (item->extra_table);
		g_free (item);
		g_slist_free_1 (l);
		l = n;
	}
}

static gboolean
emae_check_service_complete (EMAccountEditor *emae,
                             EMAccountEditorService *service)
{
	CamelProvider *provider = NULL;
	const gchar *host = NULL;
	const gchar *path = NULL;
	const gchar *user = NULL;
	gboolean have_host;
	gboolean have_path;
	gboolean have_user;
	gboolean need_auth;
	gboolean need_host;
	gboolean need_path;
	gboolean need_port;
	gboolean need_user;

	/* Protocol is NULL when server type is 'None'. */
	if (service->protocol != NULL)
		provider = camel_provider_get (service->protocol, NULL);

	if (provider == NULL)
		return TRUE;

	if (CAMEL_IS_NETWORK_SETTINGS (service->settings)) {
		CamelNetworkSettings *network_settings;

		network_settings = CAMEL_NETWORK_SETTINGS (service->settings);
		host = camel_network_settings_get_host (network_settings);
		user = camel_network_settings_get_user (network_settings);
	}

	if (CAMEL_IS_LOCAL_SETTINGS (service->settings)) {
		CamelLocalSettings *local_settings;

		local_settings = CAMEL_LOCAL_SETTINGS (service->settings);
		path = camel_local_settings_get_path (local_settings);
	}

	need_auth = CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_AUTH);
	need_host = CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_HOST);
	need_path = CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_PATH);
	need_port = CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_PORT);
	need_user = CAMEL_PROVIDER_NEEDS (provider, CAMEL_URL_PART_USER);

	have_host = (host != NULL && *host != '\0');
	have_path = (path != NULL && *path != '\0');
	have_user = (user != NULL && *user != '\0');

	if (need_host && !have_host)
		return FALSE;

	if (need_port && !e_port_entry_is_valid (service->port))
		return FALSE;

	/* We only need the user if the service needs auth as well, i think */
	if (need_auth || service->requires_auth)
		if (need_user && !have_user)
			return FALSE;

	if (need_path && !have_path)
		return FALSE;

	return TRUE;
}

static ServerData *
emae_check_servers (const gchar *email)
{
	ServerData *sdata = g_new0 (ServerData, 1);
	EmailProvider *provider = g_new0 (EmailProvider, 1);
	gchar *dupe = g_strdup (email);
	gchar *tmp;

	/* FIXME: Find a way to free the provider once given to account settings. */
	provider->email = (gchar *) email;
	tmp = strchr (email, '@');
	tmp++;
	provider->domain = tmp;
	tmp = strchr (dupe, '@');
	*tmp = 0;
	provider->username = (gchar *) g_quark_to_string (g_quark_from_string (dupe));
	g_free (dupe);

	if (!mail_guess_servers (provider)) {
		g_free (provider);
		g_free (sdata);
		return NULL;
	}
	/*printf("Recv: %s\n%s(%s), %s by %s \n Send: %s\n%s(%s), %s by %s\n via %s to %s\n",
	  provider->recv_type, provider->recv_hostname, provider->recv_port, provider->recv_username, provider->recv_auth,
	  provider->send_type, provider->send_hostname, provider->send_port, provider->send_username, provider->send_auth,
	  provider->recv_socket_type, provider->send_socket_type); */

	sdata->recv = provider->recv_hostname;
	sdata->recv_port = provider->recv_port;
	sdata->send = provider->send_hostname;
	sdata->send_port = provider->send_port;
	if (strcmp (provider->recv_type, "pop3") == 0)
		sdata->proto = g_strdup ("pop");
	else if (strcmp (provider->recv_type, "imap") == 0)
		sdata->proto = g_strdup ("imapx");
	else
		sdata->proto = provider->recv_type;
	if (provider->recv_socket_type) {
		CamelNetworkSecurityMethod method;

		if (g_ascii_strcasecmp (provider->recv_socket_type, "SSL") == 0)
			method = CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT;
		else if (g_ascii_strcasecmp (provider->recv_socket_type, "secure") == 0)
			method = CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT;
		else if (g_ascii_strcasecmp (provider->recv_socket_type, "STARTTLS") == 0)
			method = CAMEL_NETWORK_SECURITY_METHOD_STARTTLS_ON_STANDARD_PORT;
		else if (g_ascii_strcasecmp (provider->recv_socket_type, "TLS") == 0)
			method = CAMEL_NETWORK_SECURITY_METHOD_STARTTLS_ON_STANDARD_PORT;
		else
			method = CAMEL_NETWORK_SECURITY_METHOD_NONE;

		sdata->security_method = method;
		sdata->recv_security_method = method;
	}

	if (provider->send_socket_type) {
		CamelNetworkSecurityMethod method;

		if (g_ascii_strcasecmp (provider->send_socket_type, "SSL") == 0)
			method = CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT;
		else if (g_ascii_strcasecmp (provider->send_socket_type, "secure") == 0)
			method = CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT;
		else if (g_ascii_strcasecmp (provider->send_socket_type, "STARTTLS") == 0)
			method = CAMEL_NETWORK_SECURITY_METHOD_STARTTLS_ON_STANDARD_PORT;
		else if (g_ascii_strcasecmp (provider->send_socket_type, "TLS") == 0)
			method = CAMEL_NETWORK_SECURITY_METHOD_STARTTLS_ON_STANDARD_PORT;
		else
			method = CAMEL_NETWORK_SECURITY_METHOD_NONE;

		sdata->send_security_method = method;
	}

	sdata->send_auth = g_ascii_strup (provider->send_auth, -1);
	sdata->recv_auth = g_ascii_strup (provider->recv_auth, -1);
	sdata->send_user = provider->send_username;
	sdata->recv_user = provider->recv_username;

	g_free (provider);

	return sdata;
}

static void
emae_destroy_widget (GtkWidget *widget)
{
	if (widget && GTK_IS_WIDGET (widget)) {
		gtk_widget_destroy (widget);
		widget = NULL;
	}
}

static gboolean
emae_display_name_in_use (EMAccountEditor *emae,
                          const gchar *display_name)
{
	EAccount *account;
	EAccount *original_account;

	/* XXX Trivial for now, less so when we dump EAccount. */

	account = e_get_account_by_name (display_name);
	original_account = em_account_editor_get_original_account (emae);

	return (account != NULL && account != original_account);
}

static void
emae_init_receive_page_for_new_account (EMAccountEditor *emae)
{
	EAccount *account;
	const gchar *address;
	gchar *user;

	account = em_account_editor_get_modified_account (emae);
	address = e_account_get_string (account, E_ACCOUNT_ID_ADDRESS);

	/* Extract an initial username from the email address. */
	user = g_strdup (address);
	if (user != NULL) {
		gchar *cp = strchr (user, '@');
		if (cp != NULL)
			*cp = '\0';
	}

	gtk_entry_set_text (emae->priv->source.username, user);

	g_free (user);
}

static void
emae_init_receive_page_for_server_data (EMAccountEditor *emae)
{
	ServerData *sdata;
	CamelNetworkSecurityMethod method;
	gint port = 0;

	sdata = emae->priv->selected_server;
	g_return_if_fail (sdata != NULL);

	if (sdata->recv_user == NULL) {
		; /* do nothing */
	} else if (*sdata->recv_user == '\0') {
		; /* do nothing */
	} else if (strcmp (sdata->recv_user, "@") == 0) {
		; /* do nothing */
	} else {
		gtk_entry_set_text (
			emae->priv->source.username,
			sdata->recv_user);
	}

	if (sdata->recv_security_method != CAMEL_NETWORK_SECURITY_METHOD_NONE)
		method = sdata->recv_security_method;
	else
		method = sdata->security_method;

	g_object_set (
		emae->priv->source.settings,
		"security-method", method, NULL);

	emae->priv->source.protocol = sdata->proto;

	if (sdata->recv_port != NULL && *sdata->recv_port != '\0')
		port = (gint) strtol (sdata->recv_port, NULL, 10);

	if (port > 0)
		e_port_entry_set_port (emae->priv->source.port, port);

	gtk_toggle_button_set_active (emae->priv->source.remember, TRUE);
	gtk_entry_set_text (emae->priv->source.hostname, sdata->recv);

	if (sdata->recv_auth != NULL && *sdata->recv_auth != '\0')
		gtk_combo_box_set_active_id (
			emae->priv->source.authtype,
			sdata->recv_auth);
}

static void
emae_init_send_page_for_new_account (EMAccountEditor *emae)
{
	EAccount *account;
	const gchar *address;
	gchar *user;

	account = em_account_editor_get_modified_account (emae);
	address = e_account_get_string (account, E_ACCOUNT_ID_ADDRESS);

	/* Extract an initial username from the email address. */
	user = g_strdup (address);
	if (user != NULL) {
		gchar *cp = strchr (user, '@');
		if (cp != NULL)
			*cp = '\0';
	}

	gtk_entry_set_text (emae->priv->transport.username, user);

	g_free (user);
}

static void
emae_init_send_page_for_server_data (EMAccountEditor *emae)
{
	ServerData *sdata;
	CamelNetworkSecurityMethod method;
	gint port = 0;

	sdata = emae->priv->selected_server;
	g_return_if_fail (sdata != NULL);

	if (sdata->recv_user == NULL) {
		; /* do nothing */
	} else if (*sdata->recv_user == '\0') {
		; /* do nothing */
	} else if (strcmp (sdata->recv_user, "@") == 0) {
		; /* do nothing */
	} else {
		gtk_entry_set_text (
			emae->priv->transport.username,
			sdata->send_user);
	}

	if (sdata->recv_security_method != CAMEL_NETWORK_SECURITY_METHOD_NONE)
		method = sdata->recv_security_method;
	else
		method = sdata->security_method;

	g_object_set (
		emae->priv->transport.settings,
		"security-method", method, NULL);

	emae->priv->transport.protocol = "smtp";

	if (sdata->recv_port != NULL && *sdata->recv_port != '\0')
		port = (gint) strtol (sdata->recv_port, NULL, 10);

	if (port > 0)
		e_port_entry_set_port (emae->priv->source.port, port);

	gtk_toggle_button_set_active (emae->priv->transport.remember, TRUE);
	gtk_toggle_button_set_active (emae->priv->transport.needs_auth, TRUE);
	gtk_entry_set_text (emae->priv->transport.hostname, sdata->send);

	if (sdata->send_auth != NULL && *sdata->send_auth != '\0')
		gtk_combo_box_set_active_id (
			emae->priv->source.authtype,
			sdata->send_auth);
	else
		emae_authtype_changed (
			emae->priv->transport.authtype,
			&emae->priv->transport);
}

static void
emae_init_review_page (EMAccountEditor *emae)
{
	EAccount *account;
	const gchar *address;
	gchar *display_name;
	gint ii = 1;

	account = em_account_editor_get_modified_account (emae);
	address = e_account_get_string (account, E_ACCOUNT_ID_ADDRESS);

	display_name = g_strdup (address);

	/* Use the email address as the initial display name for the
	 * mail account.  If necessary, append a number to the display
	 * name to make it unique among other mail accounts. */
	while (emae_display_name_in_use (emae, display_name)) {
		g_free (display_name);
		display_name = g_strdup_printf ("%s (%d)", address, ii++);
	}

	gtk_entry_set_text (emae->priv->identity_entries[0], display_name);

	g_free (display_name);
}

static void
emae_update_review_page (EMAccountEditor *emae)
{
	EAccount *account;
	const gchar *name;
	const gchar *address;

	account = em_account_editor_get_modified_account (emae);
	name = e_account_get_string (account, E_ACCOUNT_ID_NAME);
	address = e_account_get_string (account, E_ACCOUNT_ID_ADDRESS);

	gtk_label_set_text (emae->priv->review_name, name);
	gtk_label_set_text (emae->priv->review_email, address);

	if (CAMEL_IS_NETWORK_SETTINGS (emae->priv->source.settings)) {
		CamelNetworkSecurityMethod method;
		const gchar *encryption;
		const gchar *protocol;
		gchar *host = NULL;
		gchar *user = NULL;

		protocol = emae->priv->source.protocol;

		g_object_get (
			emae->priv->source.settings,
			"host", &host, "user", &user,
			"security-method", &method, NULL);

		switch (method) {
			case CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT:
				encryption = _("Always (SSL)");
				break;
			case CAMEL_NETWORK_SECURITY_METHOD_STARTTLS_ON_STANDARD_PORT:
				encryption = _("When possible (TLS)");
				break;
			default:
				encryption = _("Never");
				break;
		}

		gtk_label_set_text (emae->priv->receive_stype, protocol);
		gtk_label_set_text (emae->priv->receive_saddress, host);
		gtk_label_set_text (emae->priv->receive_name, user);
		gtk_label_set_text (emae->priv->receive_encryption, encryption);

		g_free (host);
		g_free (user);
	}

	if (CAMEL_IS_NETWORK_SETTINGS (emae->priv->transport.settings)) {
		CamelNetworkSecurityMethod method;
		const gchar *encryption;
		const gchar *protocol;
		gchar *host = NULL;
		gchar *user = NULL;

		protocol = emae->priv->transport.protocol;

		g_object_get (
			emae->priv->transport.settings,
			"host", &host, "user", &user,
			"security-method", &method, NULL);

		switch (method) {
			case CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT:
				encryption = _("Always (SSL)");
				break;
			case CAMEL_NETWORK_SECURITY_METHOD_STARTTLS_ON_STANDARD_PORT:
				encryption = _("When possible (TLS)");
				break;
			default:
				encryption = _("Never");
				break;
		}

		gtk_label_set_text (emae->priv->send_stype, protocol);
		gtk_label_set_text (emae->priv->send_saddress, host);
		gtk_label_set_text (emae->priv->send_name, user);
		gtk_label_set_text (emae->priv->send_encryption, encryption);

		g_free (host);
		g_free (user);
	}
}

static void
emae_update_review_page_for_google (EMAccountEditor *emae)
{
	GtkWidget *container;
	GtkWidget *widget;
	gchar *markup;

	emae_destroy_widget (emae->priv->gcontacts);
	emae_destroy_widget (emae->priv->calendar);
	emae_destroy_widget (emae->priv->account_label);
	emae_destroy_widget (emae->priv->gmail_link);

	container = emae->priv->review_box;

	widget = gtk_label_new (NULL);
	markup = g_markup_printf_escaped (
		"<span size=\"large\" weight=\"bold\">%s</span>",
		_("Google account settings:"));
	gtk_label_set_markup (GTK_LABEL (widget), markup);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	emae->priv->account_label = widget;
	gtk_widget_show (widget);
	g_free (markup);

	widget = gtk_check_button_new_with_mnemonic (
		_("Setup Google con_tacts with Evolution"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	emae->priv->gcontacts = widget;
	gtk_widget_show (widget);

	widget = gtk_check_button_new_with_mnemonic (
		_("Setup Google ca_lendar with Evolution"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	emae->priv->calendar = widget;
	gtk_widget_show (widget);

	widget = gtk_link_button_new_with_label (
		"https://mail.google.com/mail/?ui=2&amp;shva=1#settings/fwdandpop",
		_("You may need to enable IMAP access."));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	emae->priv->gmail_link = widget;
	gtk_widget_show (widget);
}

static void
emae_update_review_page_for_yahoo (EMAccountEditor *emae)
{
	EAccount *account;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *label;
	gchar *markup;
	gchar *name;

	account = em_account_editor_get_modified_account (emae);
	name = g_strdup (e_account_get_string (account, E_ACCOUNT_ID_NAME));

	g_strdelimit (name, " ", '_');

	emae_destroy_widget (emae->priv->calendar);
	emae_destroy_widget (emae->priv->info_label);
	emae_destroy_widget (emae->priv->yahoo_cal_entry);
	emae_destroy_widget (emae->priv->account_label);
	emae_destroy_widget (emae->priv->yahoo_cal_box);

	container = emae->priv->review_box;

	widget = gtk_label_new (NULL);
	markup = g_markup_printf_escaped (
		"<span size=\"large\" weight=\"bold\">%s</span>",
		_("Yahoo account settings:"));
	gtk_label_set_markup (GTK_LABEL (widget), markup);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	emae->priv->account_label = widget;
	gtk_widget_show (widget);
	g_free (markup);

	widget = gtk_check_button_new_with_mnemonic (
		_("Setup _Yahoo calendar with Evolution"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	emae->priv->calendar = widget;
	gtk_widget_show (widget);

	widget = gtk_label_new (
		_("Yahoo calendars are named as firstname_lastname. We have "
		  "tried to form the calendar name. So please confirm and "
		  "re-enter the calendar name if it is not correct."));
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_line_wrap_mode (GTK_LABEL (widget), PANGO_WRAP_WORD);
	gtk_label_set_selectable (GTK_LABEL (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	emae->priv->info_label = widget;
	gtk_widget_show (widget);

	widget = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	emae->priv->yahoo_cal_box = widget;
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new_with_mnemonic (
		_("Yahoo Calen_dar name:"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	label = widget;

	widget = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (widget), name);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	emae->priv->yahoo_cal_entry = widget;
	gtk_widget_show (widget);

	g_free (name);
}

static gboolean
emae_check_identity_complete (EMAccountEditor *emae)
{
	EAccount *account;
	const gchar *name;
	const gchar *address;
	const gchar *reply_to;

	account = em_account_editor_get_modified_account (emae);

	name = e_account_get_string (account, E_ACCOUNT_ID_NAME);
	address = e_account_get_string (account, E_ACCOUNT_ID_ADDRESS);
	reply_to = e_account_get_string (account, E_ACCOUNT_ID_REPLY_TO);

	if (name == NULL || *name == '\0')
		return FALSE;

	if (address == NULL || !is_email (address))
		return FALSE;

	/* An empty reply_to string is allowed. */
	if (reply_to != NULL && *reply_to != '\0' && !is_email (reply_to))
		return FALSE;

	return TRUE;
}

static gboolean
emae_check_review_complete (EMAccountEditor *emae)
{
	EAccount *account;
	const gchar *display_name;

	account = em_account_editor_get_modified_account (emae);
	display_name = e_account_get_string (account, E_ACCOUNT_NAME);

	if (display_name == NULL || *display_name == '\0')
		return FALSE;

	if (emae_display_name_in_use (emae, display_name))
		return FALSE;

	return TRUE;
}

static gboolean
emae_check_complete (EConfig *ec,
                     const gchar *pageid,
                     gpointer data)
{
	EMAccountEditor *emae = data;
	EAccount *account;
	enum _e_config_t config_type;
	gboolean refresh = FALSE;
	gboolean new_account;
	const gchar *address;
	gchar *host = NULL;
	gboolean ok = TRUE;

	config_type = ((EConfig *) emae->priv->config)->type;
	new_account = (em_account_editor_get_original_account (emae) == NULL);

	account = em_account_editor_get_modified_account (emae);
	address = e_account_get_string (account, E_ACCOUNT_ID_ADDRESS);

	if (CAMEL_IS_NETWORK_SETTINGS (emae->priv->source.settings))
		host = camel_network_settings_dup_host (
			CAMEL_NETWORK_SETTINGS (emae->priv->source.settings));

	/* We use the page-check of various pages to 'prepare' or
	 * pre-load their values, only in the assistant. */

	if (pageid == NULL || config_type != E_CONFIG_ASSISTANT)
		goto skip_prepare;

	if (strcmp (pageid, "10.receive") == 0) {
		if (!emae->priv->receive_set) {

			/* FIXME Do this asynchronously! */
			emae->priv->selected_server =
				emae_check_servers (address);

			if (new_account) {
				emae_init_receive_page_for_new_account (emae);
				refresh = TRUE;
			}
			if (emae->priv->selected_server != NULL)
				emae_init_receive_page_for_server_data (emae);

			emae->priv->receive_set = TRUE;
		}

	} else if (strcmp (pageid, "30.send") == 0) {
		if (!emae->priv->send_set) {
			if (new_account)
				emae_init_send_page_for_new_account (emae);
			if (emae->priv->selected_server != NULL) {
				emae_init_send_page_for_server_data (emae);
				refresh = TRUE;
			}

			emae->priv->send_set = TRUE;
		}

	} else if (strcmp (pageid, "20.receive_options") == 0) {
		if (!emae->priv->receive_opt_set) {
			CamelProvider *provider;

			provider = emae_get_store_provider (emae);

			if (provider != NULL
			&& emae->priv->extra_provider != provider) {
				emae->priv->extra_provider = provider;
				emae_auto_detect (emae);
			}

			emae->priv->receive_opt_set = TRUE;
		}

	/* Review page is only shown in the assistant. */
	} else if (strcmp (pageid, "50.review") == 0) {
		if (!emae->priv->review_set) {
			emae_init_review_page (emae);
			emae->priv->review_set = TRUE;
		}

		emae_update_review_page (emae);

		/* Google and Yahoo get special treatment. */

		emae->priv->is_gmail = FALSE;
		emae->priv->is_yahoo = FALSE;

		if (e_util_utf8_strstrcase (host, "gmail")) {
			emae->priv->is_gmail = TRUE;
			emae_update_review_page_for_google (emae);

		} else if (e_util_utf8_strstrcase (host, "googlemail")) {
			emae->priv->is_gmail = TRUE;
			emae_update_review_page_for_google (emae);

		} else if (e_util_utf8_strstrcase (host, "yahoo.")) {
			emae->priv->is_yahoo = TRUE;
			emae_update_review_page_for_yahoo (emae);

		} else if (e_util_utf8_strstrcase (host, "ymail.")) {
			emae->priv->is_yahoo = TRUE;
			emae_update_review_page_for_yahoo (emae);

		} else if (e_util_utf8_strstrcase (host, "rocketmail.")) {
			emae->priv->is_yahoo = TRUE;
			emae_update_review_page_for_yahoo (emae);
		}
	}

skip_prepare:
	/*
	 * Setting a flag on the Account if it is marked as default. It is
	 * done in this way instead of using a temporary variable so as to
	 * keep track of which account is marked as default in case of
	 * editing multiple accounts at a time.
	 */
	if (gtk_toggle_button_get_active (emae->priv->default_account))
		g_object_set_data (G_OBJECT (account), "default_flagged", GINT_TO_POINTER(1));

	if (ok && (pageid == NULL || strcmp (pageid, "00.identity") == 0))
		ok = emae_check_identity_complete (emae);

	if (ok && (pageid == NULL || strcmp (pageid, "10.receive") == 0)) {
		if (emae->type != EMAE_NOTEBOOK && refresh) {
			emae_refresh_providers (
				emae, &emae->priv->source);
			emae_provider_changed (
				emae->priv->source.providers,
				&emae->priv->source);
		}
		ok = emae_check_service_complete (emae, &emae->priv->source);
	}

	if (ok && (pageid == NULL || strcmp (pageid, "30.send") == 0)) {
		CamelProvider *provider;

		provider = emae_get_store_provider (emae);
		if (!provider || !CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (provider)) {
			if (emae->type != EMAE_NOTEBOOK && refresh) {
				emae_refresh_providers (
					emae, &emae->priv->transport);
				emae_provider_changed (
					emae->priv->transport.providers,
					&emae->priv->transport);
			}
			ok = emae_check_service_complete (
				emae, &emae->priv->transport);
		}
	}

	if (ok && (pageid == NULL || strcmp (pageid, "50.review") == 0))
		ok = emae_check_review_complete (emae);

	g_free (host);

	return ok;
}

gboolean
em_account_editor_check (EMAccountEditor *emae,
                         const gchar *page)
{
	return emae_check_complete ((EConfig *) emae->config, page, emae);
}

static void
forget_password_if_needed (EAccount *original_account,
                           EAccount *modified_account,
                           e_account_item_t save_pass_itm,
                           e_account_item_t url_itm)
{
	const gchar *orig_url, *modif_url;

	g_return_if_fail (original_account != NULL);
	g_return_if_fail (modified_account != NULL);

	orig_url = e_account_get_string (original_account, url_itm);
	modif_url = e_account_get_string (modified_account, url_itm);

	if (orig_url && !*orig_url)
		orig_url = NULL;

	if (modif_url && !*modif_url)
		modif_url = NULL;

	if ((e_account_get_bool (original_account, save_pass_itm) != e_account_get_bool (modified_account, save_pass_itm)
	    && !e_account_get_bool (modified_account, save_pass_itm) && orig_url) ||
	    (orig_url && !modif_url)) {
		CamelURL *url;
		gchar *url_str;

		url = camel_url_new (orig_url, NULL);
		if (!url)
			return;

		url_str = camel_url_to_string (url, CAMEL_URL_HIDE_PARAMS);
		if (url_str)
			e_passwords_forget_password (NULL, url_str);

		g_free (url_str);
		camel_url_free (url);
	}
}

#define CALENDAR_CALDAV_URI "caldav://%s@www.google.com/calendar/dav/%s/events"
#define GMAIL_CALENDAR_LOCATION "://www.google.com/calendar/feeds/"
#define CALENDAR_DEFAULT_PATH "/private/full"
#define SELECTED_CALENDARS "selected-calendars"
#define YAHOO_CALENDAR_LOCATION "%s@caldav.calendar.yahoo.com/dav/%s/Calendar/%s"

static gchar *
sanitize_user_mail (const gchar *user)
{
	if (!user)
		return NULL;

	if (strstr (user, "%40") != NULL) {
		return g_strdup (user);
	} else if (!is_email (user)) {
		return g_strconcat (user, "%40gmail.com", NULL);
	} else {
		gchar *tmp = g_malloc0 (sizeof (gchar) * (1 + strlen (user) + 2));
		gchar *at = strchr (user, '@');

		strncpy (tmp, user, at - user);
		strcat (tmp, "%40");
		strcat (tmp, at + 1);

		return tmp;
	}
}

static void
setup_google_addressbook (EMAccountEditor *emae)
{
	GConfClient *gconf;
	ESourceList *slist;
	ESourceGroup *sgrp;
	GSList *sources;
	gboolean source_already_exists = FALSE;
	CamelURL *url;
	gchar * username;

	gconf = gconf_client_get_default ();
	slist = e_source_list_new_for_gconf (gconf, "/apps/evolution/addressbook/sources" );
	sgrp = e_source_list_ensure_group (slist, _("Google"), "google://", TRUE);
	url = emae_account_url (emae, E_ACCOUNT_SOURCE_URL);
	username = g_strdup (url->user);

	sources = e_source_group_peek_sources (sgrp);
	for (; sources; sources = sources->next) {
		ESource *existing = (ESource *) sources->data;
		if (!g_strcmp0 (e_source_peek_relative_uri (existing), username)) {
			source_already_exists = TRUE;
			break;
		}
	}

	if (!source_already_exists) {
		ESource *abook;

		/* FIXME: Not sure if we should localize 'Contacts' */
		abook = e_source_new ("Contacts", "");
		e_source_set_property (abook, "default", "true");
		e_source_set_property (abook, "offline_sync", "1");
		e_source_set_property (abook, "auth", "plain/password");
		e_source_set_property (abook, "use-ssl", "true");
		e_source_set_property (abook, "remember_password", "true");
		e_source_set_property (abook, "refresh-interval", "86400");
		e_source_set_property (abook, "completion", "true");
		e_source_set_property (abook, "username", username);

		e_source_group_add_source (sgrp, abook, -1);
		e_source_set_relative_uri (abook, username);
		e_source_list_sync (slist, NULL);

		g_object_unref (abook);
	}

	g_free (username);
	g_object_unref (slist);
	g_object_unref (sgrp);
	g_object_unref (gconf);
}

static void
setup_google_calendar (EMAccountEditor *emae)
{
	GConfClient *gconf;
	ESourceList *slist;
	ESourceGroup *sgrp;
	ESource *calendar;
	gchar *sanitize_uname, *username;
	gchar *abs_uri, *rel_uri;
	gchar **ids;
	gint i;
	GPtrArray *array;
	CamelURL *url;
	GSettings *settings;

	gconf = gconf_client_get_default ();
	slist = e_source_list_new_for_gconf (gconf, "/apps/evolution/calendar/sources");
	sgrp = e_source_list_ensure_group (slist, _("Google"), "google://", TRUE);
	url = emae_account_url (emae, E_ACCOUNT_SOURCE_URL);
	username = g_strdup (url->user);

	/* FIXME: Not sure if we should localize 'Calendar' */
	calendar = e_source_new ("Calendar", "");
	e_source_set_property (calendar, "ssl", "1");
	e_source_set_property (calendar, "refresh", "30");
	e_source_set_property (calendar, "auth", "1");
	e_source_set_property (calendar, "offline_sync", "1");
	e_source_set_property (calendar, "username", username);
	e_source_set_property (calendar, "setup-username", username);
	e_source_set_property (calendar, "default", "true");
	e_source_set_property (calendar, "alarm", "true");
	e_source_set_readonly (calendar, FALSE);

	e_source_group_add_source (sgrp, calendar, -1);

	sanitize_uname = sanitize_user_mail (username);

	abs_uri = g_strdup_printf (CALENDAR_CALDAV_URI, sanitize_uname, username);
	e_source_set_absolute_uri (calendar, abs_uri);

	rel_uri = g_strconcat ("https", GMAIL_CALENDAR_LOCATION, sanitize_uname, CALENDAR_DEFAULT_PATH, NULL);
	e_source_set_relative_uri (calendar, rel_uri);

	e_source_list_sync (slist, NULL);

	settings = g_settings_new ("org.gnome.evolution.calendar");
	ids = g_settings_get_strv (settings, SELECTED_CALENDARS);
	array = g_ptr_array_new ();
	for (i = 0; ids[i] != NULL; i++)
		g_ptr_array_add (array, ids[i]);
	g_ptr_array_add (array, (gpointer) e_source_peek_uid (calendar));
	g_ptr_array_add (array, NULL);
	g_settings_set_strv (
		settings, SELECTED_CALENDARS,
		(const gchar * const *) array->pdata);

	g_strfreev (ids);
	g_ptr_array_free (array, TRUE);
	g_object_unref (settings);
	g_free (username);
	g_free (abs_uri);
	g_free (rel_uri);
	g_free (sanitize_uname);
	g_object_unref (slist);
	g_object_unref (sgrp);
	g_object_unref (calendar);
	g_object_unref (gconf);
}

static void
setup_yahoo_calendar (EMAccountEditor *emae)
{
	GConfClient *gconf;
	ESourceList *slist;
	ESourceGroup *sgrp;
	ESource *calendar;
	gchar *sanitize_uname;
	gchar *abs_uri, *rel_uri;
	const gchar *email;
	GSettings *settings;
	gchar **ids;
	gint i;
	GPtrArray *array;

	gconf = gconf_client_get_default ();
	email = e_account_get_string (em_account_editor_get_modified_account (emae), E_ACCOUNT_ID_ADDRESS);
	slist = e_source_list_new_for_gconf (gconf, "/apps/evolution/calendar/sources");
	sgrp = e_source_list_peek_group_by_base_uri (slist, "caldav://");
	if (!sgrp) {
		sgrp = e_source_list_ensure_group (slist, _("CalDAV"), "caldav://", TRUE);
	}

	printf("Setting up Yahoo Calendar: list:%p CalDAVGrp: %p\n", slist, sgrp);

	/* FIXME: Not sure if we should localize 'Calendar' */
	calendar = e_source_new ("Yahoo", "");
	e_source_set_property (calendar, "ssl", "1");
	e_source_set_property (calendar, "refresh", "30");
	e_source_set_property (calendar, "refresh-type", "0");
	e_source_set_property (calendar, "auth", "1");
	e_source_set_property (calendar, "offline_sync", "1");
	e_source_set_property (calendar, "username", email);
	e_source_set_property (calendar, "default", "true");
	e_source_set_property (calendar, "alarm", "true");

	e_source_set_readonly (calendar, FALSE);

	sanitize_uname = sanitize_user_mail (email);

	abs_uri = g_strdup_printf ("caldav://%s@caldav.calendar.yahoo.com/dav/%s/Calendar/%s/",
			sanitize_uname, email,  gtk_entry_get_text ((GtkEntry *) emae->priv->yahoo_cal_entry));
	rel_uri = g_strdup_printf (YAHOO_CALENDAR_LOCATION, sanitize_uname, email, gtk_entry_get_text ((GtkEntry *) emae->priv->yahoo_cal_entry));
	e_source_set_relative_uri (calendar, rel_uri);

	e_source_group_add_source (sgrp, calendar, -1);
	e_source_list_sync (slist, NULL);

	settings = g_settings_new ("org.gnome.evolution.calendar");
	ids = g_settings_get_strv (settings, SELECTED_CALENDARS);
	array = g_ptr_array_new ();
	for (i = 0; ids[i] != NULL; i++)
		g_ptr_array_add (array, ids[i]);
	g_ptr_array_add (array, (gpointer) e_source_peek_uid (calendar));
	g_ptr_array_add (array, NULL);
	g_settings_set_strv (
		settings, SELECTED_CALENDARS,
		(const gchar * const *) array->pdata);

	g_strfreev (ids);
	g_ptr_array_free (array, TRUE);
	g_object_unref (settings);
	g_free (abs_uri);
	g_free (rel_uri);
	g_free (sanitize_uname);
	g_object_unref (slist);
	g_object_unref (sgrp);
	g_object_unref (calendar);
	g_object_unref (gconf);
}

static void
emae_commit (EConfig *ec,
             EMAccountEditor *emae)
{
	EAccountList *accounts = e_get_account_list ();
	EAccount *account;
	EAccount *modified_account;
	EAccount *original_account;
	CamelSettings *settings;
	CamelURL *url;
	const gchar *protocol;
	gboolean requires_auth;

	modified_account = em_account_editor_get_modified_account (emae);
	original_account = em_account_editor_get_original_account (emae);

	/* check for google and yahoo specific settings */
	if (!original_account && emae->priv->is_gmail) {
		if (gtk_toggle_button_get_active ((GtkToggleButton *) emae->priv->gcontacts))
			setup_google_addressbook (emae);
		if (gtk_toggle_button_get_active ((GtkToggleButton *) emae->priv->calendar))
			setup_google_calendar (emae);
	} else if (!original_account && emae->priv->is_gmail) {
		if (gtk_toggle_button_get_active ((GtkToggleButton *) emae->priv->calendar))
			setup_yahoo_calendar (emae);
	}

	/* Do some last minute tweaking. */

	settings = emae->priv->source.settings;
	requires_auth = emae_get_store_requires_auth (emae);

	/* Override the selected authentication mechanism name if
	 * authentication is not required for the storage service. */
	if (CAMEL_IS_NETWORK_SETTINGS (settings) && !requires_auth)
		g_object_set (settings, "auth-mechanism", NULL, NULL);

	settings = emae->priv->transport.settings;
	requires_auth = emae_get_transport_requires_auth (emae);

	/* Override the selected authentication mechanism name if
	 * authentication is not required for the transport service. */
	if (CAMEL_IS_NETWORK_SETTINGS (settings) && !requires_auth)
		g_object_set (settings, "auth-mechanism", NULL, NULL);

	/* Dump the storage service settings to a URL string. */

	url = g_new0 (CamelURL, 1);

	protocol = emae->priv->source.protocol;
	settings = emae->priv->source.settings;

	if (protocol != NULL)
		camel_url_set_protocol (url, protocol);

	if (settings != NULL)
		camel_settings_save_to_url (settings, url);

	g_free (modified_account->source->url);
	modified_account->source->url = camel_url_to_string (url, 0);

	camel_url_free (url);

	/* Dump the transport service settings to a URL string. */

	url = g_new0 (CamelURL, 1);

	protocol = emae->priv->transport.protocol;
	settings = emae->priv->transport.settings;

	if (protocol != NULL)
		camel_url_set_protocol (url, protocol);

	if (settings != NULL)
		camel_settings_save_to_url (settings, url);

	g_free (modified_account->transport->url);
	modified_account->transport->url = camel_url_to_string (url, 0);

	camel_url_free (url);

	if (original_account != NULL) {
		forget_password_if_needed (original_account, modified_account, E_ACCOUNT_SOURCE_SAVE_PASSWD, E_ACCOUNT_SOURCE_URL);
		forget_password_if_needed (original_account, modified_account, E_ACCOUNT_TRANSPORT_SAVE_PASSWD, E_ACCOUNT_TRANSPORT_URL);

		e_account_import (original_account, modified_account);
		account = original_account;
		e_account_list_change (accounts, account);
	} else {
		e_account_list_add (accounts, modified_account);
		account = modified_account;
	}

	if (gtk_toggle_button_get_active (emae->priv->default_account)) {
		EMailBackend *backend;
		EMailSession *session;
		EMailAccountStore *store;
		CamelService *service;

		backend = em_account_editor_get_backend (emae);
		session = e_mail_backend_get_session (backend);

		service = camel_session_get_service (
			CAMEL_SESSION (session), account->uid);

		store = e_mail_ui_session_get_account_store (session);
		e_mail_account_store_set_default_service (store, service);
	}

	e_account_list_save (accounts);
}

void
em_account_editor_commit (EMAccountEditor *emae)
{
	emae_commit (E_CONFIG (emae->config), emae);
}

static void
em_account_editor_construct (EMAccountEditor *emae,
                             EMAccountEditorType type,
                             const gchar *id)
{
	EMAccountEditorPrivate *priv = emae->priv;
	gint i, index;
	GSList *l;
	GList *prov;
	EMConfig *ec;
	EMConfigTargetSettings *target;
	GHashTable *have;
	EConfigItem *items;

	emae->type = type;

	if (type == EMAE_NOTEBOOK) {
		ec = em_config_new (E_CONFIG_BOOK, id);
		items = emae_editor_items;
		if (!emae_editor_items_translated) {
			for (i = 0; items[i].path; i++) {
				if (items[i].label)
					items[i].label = gettext (items[i].label);
			}
			emae_editor_items_translated = TRUE;
		}
	} else {
		ec = em_config_new (E_CONFIG_ASSISTANT, id);
		items = emae_assistant_items;
		if (!emae_assistant_items_translated) {
			for (i = 0; items[i].path; i++) {
				if (items[i].label)
					items[i].label = _(items[i].label);
			}
			emae_assistant_items_translated = TRUE;
		}
	}

	/* Connect "after" to let plugins go first. */
	g_signal_connect_after (
		ec, "commit",
		G_CALLBACK (emae_commit), emae);

	g_object_weak_ref (G_OBJECT (ec), emae_config_gone_cb, emae);

	emae->config = priv->config = ec;
	l = NULL;
	for (i = 0; items[i].path; i++)
		l = g_slist_prepend (l, &items[i]);
	e_config_add_items ((EConfig *) ec, l, emae_free, emae);

	/* This is kinda yuck, we're dynamically mapping from the 'old style' extensibility api to the new one */
	l = NULL;
	have = g_hash_table_new (g_str_hash, g_str_equal);
	index = 20;
	for (prov = priv->providers; prov; prov = g_list_next (prov)) {
		CamelProviderConfEntry *entries = ((CamelProvider *) prov->data)->extra_conf;

		for (i = 0; entries && entries[i].type != CAMEL_PROVIDER_CONF_END; i++) {
			struct _receive_options_item *item;
			const gchar *name = entries[i].name;
			gint myindex = index;

			if (entries[i].type != CAMEL_PROVIDER_CONF_SECTION_START
			    || name == NULL
			    || g_hash_table_lookup (have, name))
				continue;

			/* override mailcheck since we also insert our own mailcheck item at this index */
			if (name && !strcmp (name, "mailcheck"))
				myindex = 10;

			item = g_malloc0 (sizeof (*item));
			item->item.type = E_CONFIG_SECTION_TABLE;
			item->item.path = g_strdup_printf ("20.receive_options/%02d.%s", myindex, name?name:"unnamed");
			item->item.label = g_strdup (entries[i].text);

			l = g_slist_prepend (l, item);

			item = g_malloc0 (sizeof (*item));
			item->item.type = E_CONFIG_ITEM_TABLE;
			item->item.path = g_strdup_printf ("20.receive_options/%02d.%s/80.camelitem", myindex, name?name:"unnamed");
			item->item.factory = emae_receive_options_extra_item;
			item->item.user_data = g_strdup (entries[i].name);

			l = g_slist_prepend (l, item);

			index += 10;
			g_hash_table_insert (have, (gpointer) entries[i].name, have);
		}
	}
	g_hash_table_destroy (have);
	e_config_add_items ((EConfig *) ec, l, emae_free_auto, emae);
	priv->extra_items = l;

	e_config_add_page_check ((EConfig *) ec, NULL, emae_check_complete, emae);

	target = em_config_target_new_settings (
		ec,
		emae->priv->modified_account->id->address,
		emae->priv->source.protocol,
		emae->priv->source.settings,
		emae->priv->transport.protocol,
		emae->priv->transport.settings);
	e_config_set_target ((EConfig *) ec, (EConfigTarget *) target);
}

