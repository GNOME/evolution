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

#include <gconf/gconf-client.h>
#include <libedataserverui/e-passwords.h>

#include "shell/e-shell.h"
#include "e-util/e-util.h"
#include "e-util/e-alert-dialog.h"
#include "e-util/e-account-utils.h"
#include "e-util/e-dialog-utils.h"
#include "e-util/e-signature-list.h"
#include "e-util/e-signature-utils.h"
#include "e-util/e-util-private.h"
#include "widgets/misc/e-signature-editor.h"
#include "widgets/misc/e-port-entry.h"

#include "e-mail-backend.h"
#include "e-mail-folder-utils.h"
#include "e-mail-junk-options.h"
#include "e-mail-local.h"
#include "e-mail-store.h"
#include "em-config.h"
#include "em-folder-selection-button.h"
#include "em-account-editor.h"
#include "mail-send-recv.h"
#include "em-utils.h"
#include "mail-ops.h"
#include "mail-mt.h"

#if defined (HAVE_NSS) && defined (ENABLE_SMIME)
#include "smime/gui/e-cert-selector.h"
#endif

/* Option widgets whose sensitivity depends on another widget, such
 * as a checkbox being active, are indented to the right slightly for
 * better visual clarity.  This specifies how far to the right. */
#define INDENT_MARGIN 24

#define d(x)

static ServerData mail_servers[] = {
	{ "gmail", "imap.gmail.com", "smtp.gmail.com", "imap",
	  CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT },
	{ "googlemail", "imap.gmail.com", "smtp.gmail.com", "imap",
	  CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT },
	{ "yahoo", "pop3.yahoo.com", "smtp.yahoo.com", "pop",
	  CAMEL_NETWORK_SECURITY_METHOD_NONE },
	{ "aol", "imap.aol.com", "smtp.aol.com", "imap",
	  CAMEL_NETWORK_SECURITY_METHOD_NONE },
	{ "msn", "pop3.email.msn.com", "smtp.email.msn.com", "pop",
	  CAMEL_NETWORK_SECURITY_METHOD_NONE, "@", "@"},
	{ "hotmail", "pop3.live.com", "smtp.live.com", "pop",
	  CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT, "@", "@"},
	{ "live.com", "pop3.live.com", "smtp.live.com", "pop",
	  CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT, "@", "@"},

};

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
	GtkEntry *path;
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

	GCancellable *checking;
	GtkWidget *check_dialog;

	GList *auth_types;	/* if "Check supported" */
	CamelProvider *provider;
	CamelProviderType type;
	CamelSettings *settings;

	gint auth_changed_id;
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

	/* for e-config callbacks, each page sets up its widgets, then they are dealed out by the get_widget callback in order*/
	GHashTable *widgets;

	/* for assistant page preparation */
	guint identity_set : 1;
	guint receive_set : 1;
	guint send_set : 1;
	guint management_set : 1;

	ServerData *selected_server;
};

enum {
	PROP_0,
	PROP_BACKEND,
	PROP_MODIFIED_ACCOUNT,
	PROP_ORIGINAL_ACCOUNT
};

static void emae_refresh_authtype (EMAccountEditor *emae, EMAccountEditorService *service);
static void em_account_editor_construct (EMAccountEditor *emae, EMAccountEditorType type, const gchar *id);
static void emae_account_folder_changed (EMFolderSelectionButton *folder, EMAccountEditor *emae);
static ServerData * emae_check_servers (const gchar *email);

static gpointer parent_class;

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

		e_account_set_string (
			modified_account, E_ACCOUNT_DRAFTS_FOLDER_URI,
			e_mail_local_get_folder_uri (E_MAIL_LOCAL_FOLDER_DRAFTS));

		e_account_set_string (
			modified_account, E_ACCOUNT_SENT_FOLDER_URI,
			e_mail_local_get_folder_uri (E_MAIL_LOCAL_FOLDER_SENT));

		/* encrypt to self by default */
		e_account_set_bool (modified_account, E_ACCOUNT_PGP_ENCRYPT_TO_SELF, TRUE);
		e_account_set_bool (modified_account, E_ACCOUNT_SMIME_ENCRYPT_TO_SELF, TRUE);
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
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
emae_dispose (GObject *object)
{
	EMAccountEditorPrivate *priv;

	priv = EM_ACCOUNT_EDITOR (object)->priv;

	if (priv->backend != NULL) {
		g_object_unref (priv->backend);
		priv->backend = NULL;
	}

	if (priv->modified_account != NULL) {
		g_object_unref (priv->modified_account);
		priv->modified_account = NULL;
	}

	if (priv->original_account != NULL) {
		g_object_unref (priv->original_account);
		priv->original_account = NULL;
	}

	if (priv->source.settings != NULL) {
		g_object_unref (priv->source.settings);
		priv->source.settings = NULL;
	}

	if (priv->transport.settings != NULL) {
		g_object_unref (priv->transport.settings);
		priv->transport.settings = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
emae_finalize (GObject *object)
{
	EMAccountEditor *emae = EM_ACCOUNT_EDITOR (object);
	EMAccountEditorPrivate *priv = emae->priv;

	if (priv->sig_added_id) {
		ESignatureList *signatures;

		signatures = e_get_signature_list ();
		g_signal_handler_disconnect (signatures, priv->sig_added_id);
		g_signal_handler_disconnect (signatures, priv->sig_removed_id);
		g_signal_handler_disconnect (signatures, priv->sig_changed_id);
	}

	g_list_free (priv->source.auth_types);
	g_list_free (priv->transport.auth_types);

	g_list_free (priv->providers);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
emae_class_init (GObjectClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMAccountEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = emae_set_property;
	object_class->get_property = emae_get_property;
	object_class->dispose = emae_dispose;
	object_class->finalize = emae_finalize;

	g_object_class_install_property (
		object_class,
		PROP_BACKEND,
		g_param_spec_object (
			"backend",
			"Mail Backend",
			NULL,
			E_TYPE_MAIL_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_MODIFIED_ACCOUNT,
		g_param_spec_object (
			"modified-account",
			"Modified Account",
			NULL,
			E_TYPE_ACCOUNT,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_ORIGINAL_ACCOUNT,
		g_param_spec_object (
			"original-account",
			"Original Account",
			NULL,
			E_TYPE_ACCOUNT,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
emae_init (EMAccountEditor *emae)
{
	emae->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		emae, EM_TYPE_ACCOUNT_EDITOR, EMAccountEditorPrivate);

	emae->priv->selected_server = NULL;
	emae->emae_check_servers = emae_check_servers;
	emae->priv->source.emae = emae;
	emae->priv->transport.emae = emae;
	emae->priv->widgets = g_hash_table_new (g_str_hash, g_str_equal);
}

GType
em_account_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMAccountEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) emae_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMAccountEditor),
			0,     /* n_preallocs */
			(GInstanceInitFunc) emae_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "EMAccountEditor", &type_info, 0);
	}

	return type;
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
	const gchar *uri;

	uri = e_mail_local_get_folder_uri (E_MAIL_LOCAL_FOLDER_DRAFTS);
	em_folder_selection_button_set_folder_uri ((EMFolderSelectionButton *) emae->priv->drafts_folder_button, uri);
	emae_account_folder_changed ((EMFolderSelectionButton *) emae->priv->drafts_folder_button, emae);

	uri = e_mail_local_get_folder_uri (E_MAIL_LOCAL_FOLDER_SENT);
	em_folder_selection_button_set_folder_uri ((EMFolderSelectionButton *) emae->priv->sent_folder_button, uri);
	emae_account_folder_changed ((EMFolderSelectionButton *) emae->priv->sent_folder_button, emae);

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
	GHashTable *auto_detected;
	GSList *l;
	CamelProviderConfEntry *entries;
	gchar *value;
	gint i;
	CamelURL *url;

	if (service->provider == NULL
	    || (entries = service->provider->extra_conf) == NULL)
		return;

	d (printf ("Running auto-detect\n"));

	url = emae_account_url (emae, E_ACCOUNT_SOURCE_URL);
	camel_provider_auto_detect (service->provider, url, &auto_detected, NULL);
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

static gint
provider_compare (const CamelProvider *p1,
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
	gtk_widget_set_sensitive ((GtkWidget *) dropdown, e_account_writable (account, E_ACCOUNT_ID_SIGNATURE));

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
	gtk_widget_set_sensitive ((GtkWidget *) dropdown, e_account_writable (account, E_ACCOUNT_RECEIPT_POLICY));

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
	gtk_widget_set_sensitive ((GtkWidget *) entry, e_account_writable (account, item));

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
	gboolean writable;

	account = em_account_editor_get_modified_account (emae);

	active = e_account_get_bool (account, item);
	gtk_toggle_button_set_active (toggle, active);

	writable = e_account_writable (account, item);
	gtk_widget_set_sensitive (GTK_WIDGET (toggle), writable);

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
	gboolean writable;
	gint v_int;

	account = em_account_editor_get_modified_account (emae);

	v_int = e_account_get_int (account, item);
	gtk_spin_button_set_value (spin, v_int);

	writable = e_account_writable (account, item);
	gtk_widget_set_sensitive (GTK_WIDGET (spin), writable);

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
	const gchar *uri;

	account = em_account_editor_get_modified_account (emae);
	backend = em_account_editor_get_backend (emae);

	folder = (EMFolderSelectionButton *) e_builder_get_widget (builder, name);
	em_folder_selection_button_set_backend (folder, backend);

	uri = e_account_get_string (account, item);
	if (uri != NULL) {
		em_folder_selection_button_set_folder_uri (folder, uri);
	} else {
		uri = e_mail_local_get_folder_uri (deffolder);
		em_folder_selection_button_set_folder_uri (folder, uri);
	}

	g_object_set_data ((GObject *)folder, "account-item", GINT_TO_POINTER(item));
	g_object_set_data ((GObject *)folder, "folder-default", GINT_TO_POINTER(deffolder));
	g_signal_connect (folder, "selected", G_CALLBACK(emae_account_folder_changed), emae);
	gtk_widget_show ((GtkWidget *) folder);

	gtk_widget_set_sensitive ((GtkWidget *) folder, e_account_writable (account, item));

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

static void
emae_url_set_host (CamelURL *url,
                   const gchar *txt)
{
	gchar *host;

	if (txt && *txt) {
		host = g_strdup (txt);
		g_strstrip (host);
		camel_url_set_host (url, host);
		g_free (host);
	}
}

static void
emae_url_set_port (CamelURL *url,
                   const gchar *port)
{
	if (port && *port)
		camel_url_set_port (url, atoi (port));
}

/* This is used to map a funciton which will set on the url a string value.
 * if widgets[0] is set, it is the entry which will be called against setval ()
 * We need our own function for host:port decoding, as above */
struct _provider_host_info {
	guint32 flag;
	void (*setval)(CamelURL *, const gchar *);
	glong widgets[3];
};

static struct _provider_host_info emae_source_host_info[] = {
	{ CAMEL_URL_PART_HOST, emae_url_set_host, { G_STRUCT_OFFSET (EMAccountEditorService, hostname), G_STRUCT_OFFSET (EMAccountEditorService, hostlabel), }, },
	{ CAMEL_URL_PART_PORT, emae_url_set_port,  { G_STRUCT_OFFSET (EMAccountEditorService, port), G_STRUCT_OFFSET (EMAccountEditorService, portlabel), }, },
	{ CAMEL_URL_PART_USER, camel_url_set_user, { G_STRUCT_OFFSET (EMAccountEditorService, username), G_STRUCT_OFFSET (EMAccountEditorService, userlabel), } },
	{ CAMEL_URL_PART_PATH, camel_url_set_path, { G_STRUCT_OFFSET (EMAccountEditorService, path), G_STRUCT_OFFSET (EMAccountEditorService, pathlabel), G_STRUCT_OFFSET (EMAccountEditorService, pathentry) }, },
	{ CAMEL_URL_PART_AUTH, NULL, { 0, G_STRUCT_OFFSET (EMAccountEditorService, auth_frame), }, },
	{ 0 },
};

static struct _provider_host_info emae_transport_host_info[] = {
	{ CAMEL_URL_PART_HOST, emae_url_set_host, { G_STRUCT_OFFSET (EMAccountEditorService, hostname), G_STRUCT_OFFSET (EMAccountEditorService, hostlabel), }, },
	{ CAMEL_URL_PART_PORT, emae_url_set_port, { G_STRUCT_OFFSET (EMAccountEditorService, port), G_STRUCT_OFFSET (EMAccountEditorService, portlabel), }, },
	{ CAMEL_URL_PART_USER, camel_url_set_user, { G_STRUCT_OFFSET (EMAccountEditorService, username), G_STRUCT_OFFSET (EMAccountEditorService, userlabel), } },
	{ CAMEL_URL_PART_AUTH, NULL, { 0, G_STRUCT_OFFSET (EMAccountEditorService, auth_frame), }, },
	{ 0 },
};

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

	struct _provider_host_info *host_info;

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

	  "source_remember_password",

	  emae_source_host_info },

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

	  "transport_remember_password",

	  emae_transport_host_info,
	},
};

static void
emae_uri_changed (EMAccountEditorService *service,
                  CamelURL *url)
{
	EAccount *account;
	gchar *uri;

	account = em_account_editor_get_modified_account (service->emae);
	uri = camel_url_to_string (url, 0);

	e_account_set_string (account, emae_service_info[service->type].account_uri_key, uri);

	/* small hack for providers which are store and transport - copy settings across */
	if (service->type == CAMEL_PROVIDER_STORE
	    && service->provider
	    && CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (service->provider))
		e_account_set_string (account, E_ACCOUNT_TRANSPORT_URL, uri);

	g_free (uri);
}

static void
emae_service_url_changed (EMAccountEditorService *service,
                          void (*setval)(CamelURL *, const gchar *),
                          GtkWidget *entry)
{
	GtkComboBox *dropdown;
	gint id;
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelServiceAuthType *authtype;
	gchar *text;

	CamelURL *url = emae_account_url (service->emae, emae_service_info[service->type].account_uri_key);

	if (GTK_IS_ENTRY (entry))
		text = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	else if (E_IS_PORT_ENTRY (entry)) {
		text = g_strdup_printf ("%i", e_port_entry_get_port (E_PORT_ENTRY (entry)));
	} else
		return;

	g_strstrip (text);

	setval (url, (text && text[0]) ? text : NULL);

	if (text && text[0] && setval == camel_url_set_user) {
		dropdown = service->authtype;
		if (dropdown) {
			id = gtk_combo_box_get_active (dropdown);
			if (id != -1) {
				model = gtk_combo_box_get_model (dropdown);
					if (gtk_tree_model_iter_nth_child (model, &iter, NULL, id)) {
						gtk_tree_model_get (model, &iter, 1, &authtype, -1);
						if (authtype)
							camel_url_set_authmech (url, authtype->authproto);
					}
			}
		}
	}

	emae_uri_changed (service, url);
	camel_url_free (url);
	g_free (text);
}

static void
emae_service_url_path_changed (EMAccountEditorService *service,
                               void (*setval)(CamelURL *, const gchar *),
                               GtkWidget *widget)
{
	GtkComboBox *dropdown;
	gint id;
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelServiceAuthType *authtype;

	CamelURL *url = emae_account_url (service->emae, emae_service_info[service->type].account_uri_key);
	const gchar *text = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));

	setval (url, (text && text[0]) ? text : NULL);

	if (text && text[0] && setval == camel_url_set_user) {
		dropdown = service->authtype;
		if (dropdown) {
			id = gtk_combo_box_get_active (dropdown);
			if (id != -1) {
				model = gtk_combo_box_get_model (dropdown);
					if (gtk_tree_model_iter_nth_child (model, &iter, NULL, id)) {
						gtk_tree_model_get (model, &iter, 1, &authtype, -1);
						if (authtype)
							camel_url_set_authmech (url, authtype->authproto);
					}
			}
		}
	}

	emae_uri_changed (service, url);
	camel_url_free (url);
}

static void
emae_hostname_changed (GtkEntry *entry,
                       EMAccountEditorService *service)
{
	emae_service_url_changed (service, emae_url_set_host, GTK_WIDGET (entry));
}

static void
emae_port_changed (EPortEntry *pentry,
                   EMAccountEditorService *service)
{
	emae_service_url_changed (service, emae_url_set_port, GTK_WIDGET (pentry));
}

static void
emae_username_changed (GtkEntry *entry,
                       EMAccountEditorService *service)
{
	emae_service_url_changed (service, camel_url_set_user, GTK_WIDGET (entry));
}

static void
emae_path_changed (GtkWidget *widget,
                   EMAccountEditorService *service)
{
	emae_service_url_path_changed (service, camel_url_set_path, widget);
}

static void
emae_ssl_changed (GtkComboBox *dropdown,
                  EMAccountEditorService *service)
{
	CamelURL *url;

	url = emae_account_url (
		service->emae,
		emae_service_info[service->type].account_uri_key);
	camel_url_set_port (url, e_port_entry_get_port (service->port));
	emae_uri_changed (service, url);
	camel_url_free (url);
}

static void
emae_setup_settings (EMAccountEditorService *service)
{
	EConfig *config;
	EMConfigTargetAccount *target;
	CamelServiceClass *class;
	GType service_type;
	CamelURL *url;

	url = emae_account_url (
		service->emae,
		emae_service_info[service->type].account_uri_key);

	/* Destroy any old CamelSettings instances.
	 * Changing CamelProviders invalidates them. */

	if (service->settings != NULL) {
		camel_settings_save_to_url (service->settings, url);
		g_object_unref (service->settings);
		service->settings = NULL;
	}

	g_return_if_fail (service->provider != NULL);

	service_type = service->provider->object_types[service->type];
	g_return_if_fail (g_type_is_a (service_type, CAMEL_TYPE_SERVICE));

	class = g_type_class_ref (service_type);

	if (g_type_is_a (class->settings_type, CAMEL_TYPE_SETTINGS)) {
		service->settings = g_object_new (class->settings_type, NULL);
		camel_settings_load_from_url (service->settings, url);
	}

	g_type_class_unref (class);
	camel_url_free (url);

	/* If settings implements CamelNetworkSettings, bind the
	 * "security-method" property to the security combo box
	 * and to the EPortEntry widget. */
	if (CAMEL_IS_NETWORK_SETTINGS (service->settings)) {
		g_object_bind_property_full (
			service->settings, "security-method",
			service->use_ssl, "active-id",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE,
			e_binding_transform_enum_value_to_nick,
			e_binding_transform_enum_nick_to_value,
			NULL, (GDestroyNotify) NULL);

		g_object_bind_property (
			service->settings, "security-method",
			service->port, "security-method",
			G_BINDING_SYNC_CREATE);
	}

	/* Update the EConfigTarget so it has the latest CamelSettings. */

	config = E_CONFIG (service->emae->priv->config);
	target = (EMConfigTargetAccount *) config->target;

	em_config_target_new_account_update_settings (
		config, target, service->emae->priv->source.settings);
}

static void
emae_service_provider_changed (EMAccountEditorService *service)
{
	EAccount *account;
	gint i, j;
	gint old_port;
	void (*show)(GtkWidget *);
	CamelURL *url = emae_account_url (service->emae, emae_service_info[service->type].account_uri_key);

	account = em_account_editor_get_modified_account (service->emae);

	if (service->provider) {
		gint enable;
		GtkWidget *dwidget = NULL;

		/* Remember the current port. Any following changes in SSL would overwrite it
		   and we don't want that since user can be using a non-standard port and we
		   would lost the value this way. */
		old_port = e_port_entry_get_port (service->port);

		emae_setup_settings (service);

		camel_url_set_protocol (url, service->provider->protocol);
		gtk_label_set_text (service->description, service->provider->description);
		gtk_widget_show (service->frame);

		enable = e_account_writable_option (account, service->provider->protocol, "auth");
		gtk_widget_set_sensitive ((GtkWidget *) service->authtype, enable);
		gtk_widget_set_sensitive ((GtkWidget *) service->check_supported, enable);

		enable = e_account_writable_option (account, service->provider->protocol, "use_ssl");
		gtk_widget_set_sensitive ((GtkWidget *) service->use_ssl, enable);

		enable = e_account_writable (account, emae_service_info[service->type].save_passwd_key);
		gtk_widget_set_sensitive ((GtkWidget *) service->remember, enable);

		for (i = 0; emae_service_info[service->type].host_info[i].flag; i++) {
			GtkWidget *w;
			gint hide;
			struct _provider_host_info *info = &emae_service_info[service->type].host_info[i];

			enable = CAMEL_PROVIDER_ALLOWS (service->provider, info->flag);
			hide = CAMEL_PROVIDER_HIDDEN (service->provider, info->flag);
			show = (enable && !hide) ? gtk_widget_show : gtk_widget_hide;

			for (j = 0; j < G_N_ELEMENTS (info->widgets); j++) {
				if (info->widgets[j] && (w = G_STRUCT_MEMBER (GtkWidget *, service, info->widgets[j]))) {
					show (w);
					if (j == 0) {
						if (dwidget == NULL && enable)
							dwidget = w;

						if (info->setval && !hide) {
							if (GTK_IS_ENTRY (w))
								info->setval (url, enable ? gtk_entry_get_text ((GtkEntry *) w) : NULL);
							else if (E_IS_PORT_ENTRY (w))
								info->setval (url, enable ? g_strdup_printf ("%i",
											e_port_entry_get_port (E_PORT_ENTRY (w))) : NULL);
						}
					}
				}
			}
		}

		if (dwidget)
			gtk_widget_grab_focus (dwidget);

		if (CAMEL_PROVIDER_ALLOWS (service->provider, CAMEL_URL_PART_AUTH)) {
			GList *ll;

			/* try to keep the authmech from the current url, or clear it */
			if (url->authmech) {
				if (service->provider->authtypes) {
					for (ll = service->provider->authtypes; ll; ll = g_list_next (ll))
						if (!strcmp (url->authmech, ((CamelServiceAuthType *) ll->data)->authproto))
							break;
					if (ll == NULL)
						camel_url_set_authmech (url, NULL);
				} else {
					camel_url_set_authmech (url, NULL);
				}
			}

			emae_refresh_authtype (service->emae, service);
			if (service->needs_auth && !CAMEL_PROVIDER_NEEDS (service->provider, CAMEL_URL_PART_AUTH))
				gtk_widget_show ((GtkWidget *) service->needs_auth);
		} else {
			if (service->needs_auth)
				gtk_widget_hide ((GtkWidget *) service->needs_auth);
		}
#ifdef HAVE_SSL
		gtk_widget_hide (service->no_ssl);
		if (service->provider->flags & CAMEL_PROVIDER_SUPPORTS_SSL) {
			camel_url_set_port (url, e_port_entry_get_port (service->port));
			show = gtk_widget_show;
		} else {
			show = gtk_widget_hide;
		}
		show (service->ssl_frame);
		show (service->ssl_hbox);
#else
		gtk_widget_hide (service->ssl_hbox);
		gtk_widget_show (service->no_ssl);
#endif

		/* When everything is set it is safe to put back user's original port. */
		if (url->port && service->provider->port_entries)
			e_port_entry_set_port (service->port, old_port);

	} else {
		camel_url_set_protocol (url, NULL);
		gtk_label_set_text (service->description, "");
		gtk_widget_hide (service->frame);
		gtk_widget_hide (service->auth_frame);
		gtk_widget_hide (service->ssl_frame);
	}

	/* FIXME: linked services? */
	/* FIXME: permissions setup */

	emae_uri_changed (service, url);
	camel_url_free (url);
}

static void
emae_provider_changed (GtkComboBox *dropdown,
                       EMAccountEditorService *service)
{
	gint id = gtk_combo_box_get_active (dropdown);
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (id == -1)
		return;

	model = gtk_combo_box_get_model (dropdown);
	if (!gtk_tree_model_iter_nth_child (model, &iter, NULL, id))
		return;

	gtk_tree_model_get (model, &iter, 1, &service->provider, -1);

	g_list_free (service->auth_types);
	service->auth_types = NULL;

	emae_service_provider_changed (service);

	e_config_target_changed ((EConfig *) service->emae->priv->config, E_CONFIG_TARGET_CHANGED_REBUILD);
}

static void
emae_refresh_providers (EMAccountEditor *emae,
                        EMAccountEditorService *service)
{
	EAccount *account;
	GtkListStore *store;
	GtkTreeIter iter;
	GList *l;
	GtkCellRenderer *cell = gtk_cell_renderer_text_new ();
	GtkComboBox *dropdown;
	gint active = 0, i;
	struct _service_info *info = &emae_service_info[service->type];
	const gchar *uri;
	gchar *current = NULL;
	CamelURL *url;

	account = em_account_editor_get_modified_account (emae);
	uri = e_account_get_string (account, info->account_uri_key);

	dropdown = service->providers;
	gtk_widget_show ((GtkWidget *) dropdown);

	if (uri) {
		const gchar *colon = strchr (uri, ':');
		gint len;

		if (colon) {
			len = colon - uri;
			current = g_alloca (len + 1);
			memcpy (current, uri, len);
			current[len] = 0;
		}
	} else {
		/* Promote the newer IMAP provider over the older one. */
		current = (gchar *) "imapx";
	}

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);

	i = 0;

	/* We just special case each type here, its just easier */
	if (service->type == CAMEL_PROVIDER_STORE) {
		gtk_list_store_append (store, &iter);
		/* Translators: "None" for receiving account type, beside of IMAP, POP3, ... */
		gtk_list_store_set (store, &iter, 0, C_("mail-receiving", "None"), 1, NULL, -1);
		i++;
	}

	for (l = emae->priv->providers; l; l = l->next) {
		CamelProvider *provider = l->data;

		if (!((strcmp (provider->domain, "mail") == 0
		       || strcmp (provider->domain, "news") == 0)
		      && provider->object_types[service->type]
		      && (service->type != CAMEL_PROVIDER_STORE || (provider->flags & CAMEL_PROVIDER_IS_SOURCE) != 0))
		    /* hardcode not showing providers who's transport is done in the store */
		    || (service->type == CAMEL_PROVIDER_TRANSPORT
			&& CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (provider)))
			continue;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, provider->name, 1, provider, -1);

		/* find the displayed and set default */
		if (i == 0 || (current && strcmp (provider->protocol, current) == 0)) {
			CamelURL *url;

			service->provider = provider;
			emae_setup_settings (service);
			active = i;

			url = emae_account_url (emae, info->account_uri_key);
			if (current == NULL) {
				/* we need to set this value on the uri too */
				camel_url_set_protocol (url, provider->protocol);
			}

			emae_uri_changed (service, url);
			uri = e_account_get_string (account, info->account_uri_key);
			camel_url_free (url);
		}
		i++;
	}

	gtk_cell_layout_clear ((GtkCellLayout *) dropdown);
	gtk_combo_box_set_model (dropdown, (GtkTreeModel *) store);
	gtk_cell_layout_pack_start ((GtkCellLayout *) dropdown, cell, TRUE);
	gtk_cell_layout_set_attributes ((GtkCellLayout *)dropdown, cell, "text", 0, NULL);

	g_signal_handlers_disconnect_by_func (dropdown, emae_provider_changed, service);
	gtk_combo_box_set_active (dropdown, -1);	/* needed for gtkcombo bug (?) */
	gtk_combo_box_set_active (dropdown, active);
	g_signal_connect (dropdown, "changed", G_CALLBACK(emae_provider_changed), service);

	if (!uri  || (url = camel_url_new (uri, NULL)) == NULL) {
		return;
	}

	camel_url_free (url);
}

static void
emae_authtype_changed (GtkComboBox *dropdown,
                       EMAccountEditorService *service)
{
	EAccount *account;
	gint id = gtk_combo_box_get_active (dropdown);
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelURL *url;
	gboolean sensitive = FALSE;

	if (id == -1)
		return;

	account = em_account_editor_get_modified_account (service->emae);

	url = emae_account_url (service->emae, emae_service_info[service->type].account_uri_key);
	model = gtk_combo_box_get_model (dropdown);
	if (gtk_tree_model_iter_nth_child (model, &iter, NULL, id)) {
		CamelServiceAuthType *authtype;

		gtk_tree_model_get (model, &iter, 1, &authtype, -1);
		if (authtype)
			camel_url_set_authmech (url, authtype->authproto);
		else
			camel_url_set_authmech (url, NULL);
		emae_uri_changed (service, url);

		sensitive =
			authtype != NULL &&
			authtype->need_password &&
			e_account_writable (account,
			emae_service_info[service->type].save_passwd_key);
	}
	camel_url_free (url);

	gtk_widget_set_sensitive ((GtkWidget *) service->remember, sensitive);
}

static void
emae_needs_auth (GtkToggleButton *toggle,
                 EMAccountEditorService *service)
{
	gint need = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (service->auth_frame, need);

	if (need)
		emae_authtype_changed (service->authtype, service);
	else {
		CamelURL *url = emae_account_url (service->emae, emae_service_info[service->type].account_uri_key);

		camel_url_set_authmech (url, NULL);
		emae_uri_changed (service, url);
		camel_url_free (url);
	}
}

static void emae_check_authtype (GtkWidget *w, EMAccountEditorService *service);

static void
emae_refresh_authtype (EMAccountEditor *emae,
                       EMAccountEditorService *service)
{
	EAccount *account;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkComboBox *dropdown;
	gint active = 0;
	gint i;
	struct _service_info *info = &emae_service_info[service->type];
	const gchar *uri;
	GList *l, *ll;
	CamelURL *url = NULL;

	account = em_account_editor_get_modified_account (emae);
	uri = e_account_get_string (account, info->account_uri_key);

	dropdown = service->authtype;
	gtk_widget_show ((GtkWidget *) dropdown);

	store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_BOOLEAN);

	if (uri)
		url = camel_url_new (uri, NULL);

	if (service->provider) {
		for (i = 0, l = service->provider->authtypes; l; l = l->next, i++) {
			CamelServiceAuthType *authtype = l->data;
			gint avail;

			/* if we have some already shown */
			if (service->auth_types) {
				for (ll = service->auth_types; ll; ll = g_list_next (ll))
					if (!strcmp (authtype->authproto, ((CamelServiceAuthType *) ll->data)->authproto))
						break;
				avail = ll != NULL;
			} else {
				avail = TRUE;
			}

			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter, 0, authtype->name, 1, authtype, 2, !avail, -1);

			if (url && url->authmech && !strcmp (url->authmech, authtype->authproto))
				active = i;
		}
	}

	gtk_combo_box_set_model (dropdown, (GtkTreeModel *) store);
	gtk_combo_box_set_active (dropdown, -1);

	if (service->auth_changed_id == 0) {
		GtkCellRenderer *cell = gtk_cell_renderer_text_new ();

		gtk_cell_layout_pack_start ((GtkCellLayout *) dropdown, cell, TRUE);
		gtk_cell_layout_set_attributes ((GtkCellLayout *)dropdown, cell, "text", 0, "strikethrough", 2, NULL);

		service->auth_changed_id = g_signal_connect (dropdown, "changed", G_CALLBACK(emae_authtype_changed), service);
		g_signal_connect (service->check_supported, "clicked", G_CALLBACK(emae_check_authtype), service);
	}

	gtk_combo_box_set_active (dropdown, active);

	if (url)
		camel_url_free (url);
}

static void
emae_check_authtype_done (CamelService *camel_service,
                          GAsyncResult *result,
                          EMAccountEditorService *service)
{
	EMailBackend *backend;
	EMailSession *session;
	GtkWidget *editor;
	GList *auth_types;
	GError *error = NULL;

	auth_types = camel_service_query_auth_types_finish (
		camel_service, result, &error);

	editor = E_CONFIG (service->emae->config)->window;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (auth_types == NULL);
		g_error_free (error);

	} else if (error != NULL) {
		g_warn_if_fail (auth_types == NULL);
		e_alert_run_dialog_for_args (
			GTK_WINDOW (service->check_dialog),
			"mail:checking-service-error",
			error->message, NULL);
		g_error_free (error);

	} else {
		g_list_free (service->auth_types);
		service->auth_types = auth_types;
		emae_refresh_authtype (service->emae, service);
	}

	gtk_widget_destroy (service->check_dialog);
	service->check_dialog = NULL;

	if (editor != NULL)
		gtk_widget_set_sensitive (editor, TRUE);

	backend = em_account_editor_get_backend (service->emae);
	session = e_mail_backend_get_session (backend);

	/* drop the temporary CamelService */
	camel_session_remove_service (
		CAMEL_SESSION (session),
		camel_service_get_uid (camel_service));

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
	EAccount *account;
	GtkWidget *editor;
	gpointer parent;
	gchar *uid;
	gchar *url_string;
	CamelURL *url;
	GError *error = NULL;

	account = em_account_editor_get_modified_account (service->emae);
	editor = E_CONFIG (service->emae->config)->window;

	backend = em_account_editor_get_backend (service->emae);
	session = e_mail_backend_get_session (backend);

	uid = g_strdup_printf ("emae-check-authtype-%p", service);
	url_string = (gchar *) e_account_get_string (
		account, emae_service_info[service->type].account_uri_key);
	url = camel_url_new (url_string, NULL);

	/* to test on actual data, not on previously used */
	camel_service = camel_session_add_service (
		CAMEL_SESSION (session), uid,
		url->protocol, service->type, &error);

	camel_url_free (url);
	g_free (url_string);
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
	EAccount *account;
	struct _service_info *info = &emae_service_info[service->type];
	CamelURL *url = emae_account_url (emae, info->account_uri_key);
	const gchar *uri;

	account = em_account_editor_get_modified_account (emae);
	uri = e_account_get_string (account, info->account_uri_key);

	service->provider = uri ? camel_provider_get (uri, NULL) : NULL;

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

	service->remember = emae_account_toggle (emae, info->remember_password, info->save_passwd_key, builder);

	if (info->needs_auth)
		service->needs_auth = (GtkToggleButton *) e_builder_get_widget (builder, info->needs_auth);
	else
		service->needs_auth = NULL;

	service->auth_changed_id = 0;

	g_signal_connect (service->hostname, "changed", G_CALLBACK (emae_hostname_changed), service);
	g_signal_connect (service->port, "changed", G_CALLBACK (emae_port_changed), service);
	g_signal_connect (service->username, "changed", G_CALLBACK (emae_username_changed), service);
	if (service->pathentry)
		g_signal_connect (GTK_FILE_CHOOSER (service->pathentry), "selection-changed", G_CALLBACK (emae_path_changed), service);

	g_signal_connect (service->use_ssl, "changed", G_CALLBACK(emae_ssl_changed), service);

	/* configure ui for current settings */
	if (url->host) {
		gtk_entry_set_text (service->hostname, url->host);
	}

	if (url->user && *url->user) {
		gtk_entry_set_text (service->username, url->user);
	}

	if (service->pathentry) {
		GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;

		if (service->provider && (service->provider->url_flags & CAMEL_URL_NEED_PATH_DIR) == 0)
			action = GTK_FILE_CHOOSER_ACTION_OPEN;

		if (service->pathlabel)
			gtk_label_set_text_with_mnemonic (GTK_LABEL (service->pathlabel),
				action == GTK_FILE_CHOOSER_ACTION_OPEN ? _("Fil_e:") : _("_Path:"));

		if (action != gtk_file_chooser_get_action (GTK_FILE_CHOOSER (service->pathentry)))
			gtk_file_chooser_set_action (GTK_FILE_CHOOSER (service->pathentry), action);

		if (url->path)
			gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (service->pathentry), url->path);
	}

	/* old authtype will be destroyed when we exit */
	emae_refresh_providers (emae, service);
	emae_refresh_authtype (emae, service);

	if (service->port && service->provider->port_entries)
		e_port_entry_set_camel_entries (service->port, service->provider->port_entries);

	/* Set the port after SSL is set, because it would overwrite the
	 * port value (through emae_ssl_changed signal) */
	if (url->port && service->provider->port_entries) {
		e_port_entry_set_port (service->port, url->port);
	}

	if (service->needs_auth != NULL) {
		gtk_toggle_button_set_active (service->needs_auth, url->authmech != NULL);
		g_signal_connect (service->needs_auth, "toggled", G_CALLBACK(emae_needs_auth), service);
		emae_needs_auth (service->needs_auth, service);
	}

	if (!e_account_writable (account, info->account_uri_key))
		gtk_widget_set_sensitive (service->container, FALSE);
	else
		gtk_widget_set_sensitive (service->container, TRUE);

	emae_service_provider_changed (service);

	camel_url_free (url);
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
	} else if (g_ascii_strcasecmp (page_id, "management_page") == 0) {
		title = _("Account Information");
		label = _("Please enter a descriptive name for this account below.\nThis name will be used for display purposes only.");
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
	GtkWidget *widget;
	GtkListStore *store;
	GtkTreeIter iter;
	const gchar *p;
	GtkCellRenderer *renderer;

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
			service->provider->translation_domain, cp), -1);

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
	GtkWidget *w, *box, *spin;
	guint row;

	if (emae->priv->source.provider == NULL
	    || emae->priv->source.provider->extra_conf == NULL)
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
	CamelProviderConfEntry *entries;
	guint row;
	GHashTable *extra;
	CamelURL *url;
	const gchar *section_name;
	gint ii;

	service = &emae->priv->source;
	section_name = eitem->user_data;

	if (emae->priv->source.provider == NULL)
		return NULL;

	if (emae->priv->source.provider->extra_conf == NULL)
		return NULL;

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

	entries = emae->priv->source.provider->extra_conf;
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
	GtkWidget *w;
	GtkBuilder *builder;

	/* no transport options page at all for these types of providers */
	if (priv->source.provider && CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (priv->source.provider)) {
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

		session = e_mail_backend_get_session (em_folder_selection_button_get_backend (sel_button));
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
	em_folder_selection_button_set_backend (button, backend);
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

	pspec = !settings ? NULL : g_object_class_find_property (
		G_OBJECT_GET_CLASS (settings), "use-real-trash-path");

	if (pspec != NULL)
		g_object_bind_property (
			settings, "use-real-trash-path",
			priv->trash_folder_check, "active",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);

	pspec = !settings ? NULL : g_object_class_find_property (
		G_OBJECT_GET_CLASS (settings), "real-trash-path");

	if (pspec != NULL) {
		set_real_folder_path (priv->trash_folder_button, settings, "real-trash-path", account);
		g_signal_connect (priv->trash_folder_button, "notify::folder-uri", G_CALLBACK (update_real_folder_cb), emae);
	}

	flags = CAMEL_PROVIDER_ALLOW_REAL_TRASH_FOLDER;
	visible = (emae->priv->source.provider != NULL) &&
		((emae->priv->source.provider->flags & flags) != 0);
	widget = GTK_WIDGET (priv->trash_folder_check);
	gtk_widget_set_visible (widget, visible);
	widget = GTK_WIDGET (priv->trash_folder_button);
	gtk_widget_set_visible (widget, visible);

	widget = e_builder_get_widget (builder, "junk_folder_check");
	priv->junk_folder_check = GTK_TOGGLE_BUTTON (widget);

	widget = e_builder_get_widget (builder, "junk_folder_butt");
	button = EM_FOLDER_SELECTION_BUTTON (widget);
	em_folder_selection_button_set_backend (button, backend);
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

	pspec = !settings ? NULL : g_object_class_find_property (
		G_OBJECT_GET_CLASS (settings), "use-real-junk-path");

	if (pspec != NULL)
		g_object_bind_property (
			settings, "use-real-junk-path",
			priv->junk_folder_check, "active",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);

	pspec = !settings ? NULL : g_object_class_find_property (
		G_OBJECT_GET_CLASS (settings), "real-junk-path");

	if (pspec != NULL) {
		set_real_folder_path (priv->junk_folder_button, settings, "real-junk-path", account);
		g_signal_connect (priv->junk_folder_button, "notify::folder-uri", G_CALLBACK (update_real_folder_cb), emae);
	}

	flags = CAMEL_PROVIDER_ALLOW_REAL_JUNK_FOLDER;
	visible = (emae->priv->source.provider != NULL) &&
		((emae->priv->source.provider->flags & flags) != 0);
	widget = GTK_WIDGET (priv->junk_folder_check);
	gtk_widget_set_visible (widget, visible);
	widget = GTK_WIDGET (priv->junk_folder_button);
	gtk_widget_set_visible (widget, visible);

	/* Special Folders "Reset Defaults" button */
	priv->restore_folders_button = (GtkButton *)e_builder_get_widget (builder, "default_folders_button");
	g_signal_connect (priv->restore_folders_button, "clicked", G_CALLBACK (default_folders_clicked), emae);

	/* Always Cc/Bcc */
	emae_account_toggle (emae, "always_cc", E_ACCOUNT_CC_ALWAYS, builder);
	emae_account_entry (emae, "cc_addrs", E_ACCOUNT_CC_ADDRS, builder);
	emae_account_toggle (emae, "always_bcc", E_ACCOUNT_BCC_ALWAYS, builder);
	emae_account_entry (emae, "bcc_addrs", E_ACCOUNT_BCC_ADDRS, builder);

	gtk_widget_set_sensitive ((GtkWidget *) priv->drafts_folder_button, e_account_writable (account, E_ACCOUNT_DRAFTS_FOLDER_URI));

	gtk_widget_set_sensitive ( (GtkWidget *) priv->sent_folder_button,
				  e_account_writable (account, E_ACCOUNT_SENT_FOLDER_URI)
				  &&
				  (emae->priv->source.provider ? !(emae->priv->source.provider->flags & CAMEL_PROVIDER_DISABLE_SENT_FOLDER): TRUE)
				);

	gtk_widget_set_sensitive ((GtkWidget *) priv->restore_folders_button,
				 (e_account_writable (account, E_ACCOUNT_SENT_FOLDER_URI)
				  && ((emae->priv->source.provider  && !( emae->priv->source.provider->flags & CAMEL_PROVIDER_DISABLE_SENT_FOLDER))
				      || e_account_writable (account, E_ACCOUNT_DRAFTS_FOLDER_URI))));

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
	gtk_widget_set_sensitive (GTK_WIDGET (combobox), e_account_writable (account, item));

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
emae_management_page (EConfig *ec,
                      EConfigItem *item,
                      GtkWidget *parent,
                      GtkWidget *old,
                      gint position,
                      gpointer data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *priv = emae->priv;
	GtkWidget *w;

	w = priv->management_frame;
	if (((EConfig *) priv->config)->type == E_CONFIG_ASSISTANT) {
		GtkWidget *page;

		page = emae_create_basic_assistant_page (
			emae, GTK_ASSISTANT (parent),
			"management_page", position);

		gtk_widget_reparent (w, page);

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

	{ E_CONFIG_PAGE, (gchar *) "40.management", (gchar *) "management_frame", emae_management_page },

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
emae_service_complete (EMAccountEditor *emae,
                       EMAccountEditorService *service)
{
	EAccount *account;
	CamelURL *url;
	gint ok = TRUE;
	const gchar *uri;

	if (service->provider == NULL)
		return TRUE;

	account = em_account_editor_get_modified_account (emae);

	uri = e_account_get_string (account, emae_service_info[service->type].account_uri_key);
	if (uri == NULL || (url = camel_url_new (uri, NULL)) == NULL)
		return FALSE;

	if (CAMEL_PROVIDER_NEEDS (service->provider, CAMEL_URL_PART_HOST)) {
		if (url->host == NULL || url->host[0] == 0 || (!e_port_entry_is_valid (service->port) && service->provider->port_entries))
			ok = FALSE;
	}
	/* We only need the user if the service needs auth as well, i think */
	if (ok
	    && (service->needs_auth == NULL
		|| CAMEL_PROVIDER_NEEDS (service->provider, CAMEL_URL_PART_AUTH)
		|| gtk_toggle_button_get_active (service->needs_auth))
	    && CAMEL_PROVIDER_NEEDS (service->provider, CAMEL_URL_PART_USER)
	    && (url->user == NULL || url->user[0] == 0))
		ok = FALSE;

	if (ok
	    && CAMEL_PROVIDER_NEEDS (service->provider, CAMEL_URL_PART_PATH)
	    && (url->path == NULL || url->path[0] == 0))
		ok = FALSE;

	camel_url_free (url);

	return ok;
}

static ServerData *
emae_check_servers (const gchar *email)
{
	gint len = G_N_ELEMENTS (mail_servers), i;
	gchar *server = strchr (email, '@');

	server++;

	for (i = 0; i < len; i++) {
		if (strstr (server, mail_servers[i].key) != NULL)
			return &mail_servers[i];
	}

	return NULL;
}

static void
emae_check_set_authtype (GtkComboBox *dropdown,
                         const gchar *auth)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint id;
	gint children;

	model = gtk_combo_box_get_model (dropdown);
	children = gtk_tree_model_iter_n_children (model, NULL);
	for (id = 0; id < children; id++)  {
		CamelServiceAuthType *authtype;

		gtk_tree_model_iter_nth_child (model, &iter, NULL, id);
		gtk_tree_model_get (model, &iter, 1, &authtype, -1);
		if (g_ascii_strcasecmp (authtype->authproto, auth) == 0)
			break;
	}

	if (id < children)
		gtk_combo_box_set_active (dropdown, id);
}

static gboolean
emae_check_complete (EConfig *ec,
                     const gchar *pageid,
                     gpointer data)
{
	EMAccountEditor *emae = data;
	EAccount *account;
	EAccount *original_account;
	gint ok = TRUE;
	const gchar *tmp;
	EAccount *ea;
	gboolean refresh = FALSE;
	gboolean new_account;

	account = em_account_editor_get_modified_account (emae);
	original_account = em_account_editor_get_original_account (emae);
	new_account = (original_account == NULL);

	/* We use the page-check of various pages to 'prepare' or
	 * pre-load their values, only in the assistant */
	if (pageid
	    && ((EConfig *) emae->priv->config)->type == E_CONFIG_ASSISTANT) {
		if (!strcmp (pageid, "00.identity")) {
			if (!emae->priv->identity_set) {
				gchar *uname;

				emae->priv->identity_set = 1;
#ifndef G_OS_WIN32
				uname = g_locale_to_utf8 (g_get_real_name (), -1, NULL, NULL, NULL);
#else
				uname = g_strdup (g_get_real_name ());
#endif
				if (uname) {
					gtk_entry_set_text (emae->priv->identity_entries[1], uname);
					g_free (uname);
				}
			}
		} else if (!strcmp (pageid, "10.receive")) {
			if (!emae->priv->receive_set) {
				ServerData *sdata;
				gchar *user, *at;
				gchar *uri = g_strdup (e_account_get_string (account, E_ACCOUNT_SOURCE_URL));
				CamelURL *url;

				emae->priv->receive_set = 1;
				tmp = (gchar *) e_account_get_string (account, E_ACCOUNT_ID_ADDRESS);
				at = strchr (tmp, '@');
				user = g_alloca (at - tmp + 1);
				memcpy (user, tmp, at - tmp);
				user[at - tmp] = 0;
				at++;

				sdata = emae->priv->selected_server = emae->emae_check_servers (tmp);
				if (new_account && uri && (url = camel_url_new (uri, NULL)) != NULL) {
					const gchar *use_user = user;
					refresh = TRUE;
					if (sdata && sdata->recv_user && *sdata->recv_user)
						use_user = g_str_equal (sdata->recv_user, "@") ? tmp : sdata->recv_user;
					camel_url_set_user (url, use_user);
					gtk_entry_set_text (emae->priv->source.username, use_user);

					if (sdata != NULL) {
						CamelNetworkSecurityMethod method;
						if (sdata->recv_security_method != CAMEL_NETWORK_SECURITY_METHOD_NONE)
							method = sdata->recv_security_method;
						else
							method = sdata->security_method;
						g_object_set (emae->priv->source.settings, "security-method", method, NULL);

						camel_url_set_protocol (url, sdata->proto);
						camel_url_set_host (url, sdata->recv);
						if (sdata->recv_port && *sdata->recv_port)
							camel_url_set_port (url, atoi (sdata->recv_port));
						gtk_entry_set_text (emae->priv->source.hostname, sdata->recv);
						gtk_entry_set_text (emae->priv->transport.hostname, sdata->send);
					} else {
						camel_url_set_host (url, "");
					}
					g_free (uri);
					uri = camel_url_to_string (url, 0);
					e_account_set_string (account, E_ACCOUNT_SOURCE_URL, uri);
					if (sdata != NULL && sdata->recv_auth && *sdata->recv_auth)
						emae_check_set_authtype (emae->priv->source.authtype, sdata->recv_auth);

					camel_url_free (url);
				} else
					gtk_entry_set_text (emae->priv->source.username, user);
				g_free (uri);

			}
		} else if (!strcmp (pageid, "30.send")) {
			if (!emae->priv->send_set) {
				CamelURL *url;
				gchar *at, *user;
				gchar *uri = (gchar *) e_account_get_string (account, E_ACCOUNT_TRANSPORT_URL);
				ServerData *sdata;

				emae->priv->send_set = 1;
				tmp = e_account_get_string (account, E_ACCOUNT_ID_ADDRESS);
				at = strchr (tmp, '@');
				user = g_alloca (at - tmp + 1);
				memcpy (user, tmp, at - tmp);
				user[at - tmp] = 0;
				at++;

				sdata = emae->priv->selected_server;
				if (sdata != NULL && uri && (url = camel_url_new (uri, NULL)) != NULL) {
					CamelNetworkSecurityMethod method;
					const gchar *use_user = user;

					refresh = TRUE;

					if (sdata->send_security_method != CAMEL_NETWORK_SECURITY_METHOD_NONE)
						method = sdata->send_security_method;
					else
						method = sdata->security_method;
					g_object_set (emae->priv->transport.settings, "security-method", method, NULL);

					camel_url_set_protocol (url, "smtp");
					camel_url_set_host (url, sdata->send);
					if (sdata->send_port && *sdata->send_port)
						camel_url_set_port (url, atoi (sdata->send_port));

					if (sdata->send_user && *sdata->send_user)
						use_user = g_str_equal (sdata->send_user, "@") ? tmp : sdata->send_user;
					camel_url_set_user (url, use_user);
					gtk_entry_set_text (emae->priv->transport.username, use_user);

					uri = camel_url_to_string (url, 0);
					e_account_set_string (account, E_ACCOUNT_TRANSPORT_URL, uri);
					g_free (uri);
					camel_url_free (url);
					gtk_toggle_button_set_active (emae->priv->transport.needs_auth, TRUE);
					if (sdata->send_auth && *sdata->send_auth)
						emae_check_set_authtype (emae->priv->transport.authtype, sdata->send_auth);
					else
						emae_authtype_changed (emae->priv->transport.authtype, &emae->priv->transport);
					uri = (gchar *) e_account_get_string (account, E_ACCOUNT_TRANSPORT_URL);
				} else
					gtk_entry_set_text (emae->priv->transport.username, user);
			}

		} else if (!strcmp (pageid, "20.receive_options")) {
			if (emae->priv->source.provider
			    && emae->priv->extra_provider != emae->priv->source.provider) {
				emae->priv->extra_provider = emae->priv->source.provider;
				emae_auto_detect (emae);
			}
		} else if (!strcmp (pageid, "40.management")) {
			if (!emae->priv->management_set) {
				gchar *template;
				guint i = 0, len;

				emae->priv->management_set = 1;
				tmp = e_account_get_string (account, E_ACCOUNT_ID_ADDRESS);
				len = strlen (tmp);
				template = g_alloca (len + 14);
				strcpy (template, tmp);
				while (e_get_account_by_name (template))
					sprintf (template + len, " (%d)", i++);

				gtk_entry_set_text (emae->priv->identity_entries[0], template);
			}
		}
	}

	/*
	 * Setting a flag on the Account if it is marked as default. It is done in this way instead of
	 * using a temporary variable so as to keep track of which account is marked as default in case of
	 * editing multiple accounts at a time
	 */
	if (gtk_toggle_button_get_active (emae->priv->default_account))
		g_object_set_data (G_OBJECT (account), "default_flagged", GINT_TO_POINTER(1));

	if (pageid == NULL || !strcmp (pageid, "00.identity")) {
		/* TODO: check the account name is set, and unique in the account list */
		ok = (tmp = e_account_get_string (account, E_ACCOUNT_ID_NAME))
			&& tmp[0]
			&& (tmp = e_account_get_string (account, E_ACCOUNT_ID_ADDRESS))
			&& is_email (tmp)
			&& ((tmp = e_account_get_string (account, E_ACCOUNT_ID_REPLY_TO)) == NULL
			    || tmp[0] == 0
			    || is_email (tmp));
		if (!ok) {
			d (printf ("identity incomplete\n"));
		}
	}

	if (ok && (pageid == NULL || !strcmp (pageid, "10.receive"))) {
		if (emae->type != EMAE_NOTEBOOK && refresh) {
			emae_refresh_providers (emae, &emae->priv->source);
			emae_provider_changed (emae->priv->source.providers, &emae->priv->source);
		}
		ok = emae_service_complete (emae, &emae->priv->source);
		if (!ok) {
			d (printf ("receive page incomplete\n"));
		}
	}

	if (ok && (pageid == NULL || !strcmp (pageid, "30.send"))) {
		if (emae->type != EMAE_NOTEBOOK && refresh) {
			emae_refresh_providers (emae, &emae->priv->transport);
			emae_provider_changed (emae->priv->transport.providers, &emae->priv->transport);
		}
		ok = emae_service_complete (emae, &emae->priv->transport);
		if (!ok) {
			d (printf ("send page incomplete\n"));
		}
	}

	if (ok && (pageid == NULL || !strcmp (pageid, "40.management"))) {
		ok = (tmp = e_account_get_string (account, E_ACCOUNT_NAME))
			&& tmp[0]
			&& ((ea = e_get_account_by_name (tmp)) == NULL
			    || ea == original_account);
		if (!ok) {
			d (printf ("management page incomplete\n"));
		}
	}

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

static void
emae_commit (EConfig *ec,
             EMAccountEditor *emae)
{
	EAccountList *accounts = e_get_account_list ();
	EAccount *account;
	EAccount *modified_account;
	EAccount *original_account;
	CamelURL *url;

	/* the mail-config*acconts* api needs a lot of work */

	modified_account = em_account_editor_get_modified_account (emae);
	original_account = em_account_editor_get_original_account (emae);

	url = camel_url_new (modified_account->source->url, NULL);
	if (url != NULL) {
		if (emae->priv->source.settings != NULL)
			camel_settings_save_to_url (
				emae->priv->source.settings, url);
		g_free (modified_account->source->url);
		modified_account->source->url = camel_url_to_string (url, 0);
		camel_url_free (url);
	}

	url = camel_url_new (modified_account->transport->url, NULL);
	if (url != NULL) {
		if (emae->priv->transport.settings != NULL)
			camel_settings_save_to_url (
				emae->priv->transport.settings, url);
		g_free (modified_account->transport->url);
		modified_account->transport->url = camel_url_to_string (url, 0);
		camel_url_free (url);
	}

	if (original_account != NULL) {
		d (printf ("Committing account '%s'\n", e_account_get_string (modified_account, E_ACCOUNT_NAME)));
		forget_password_if_needed (original_account, modified_account, E_ACCOUNT_SOURCE_SAVE_PASSWD, E_ACCOUNT_SOURCE_URL);
		forget_password_if_needed (original_account, modified_account, E_ACCOUNT_TRANSPORT_SAVE_PASSWD, E_ACCOUNT_TRANSPORT_URL);

		e_account_import (original_account, modified_account);
		account = original_account;
		e_account_list_change (accounts, account);
	} else {
		d (printf ("Adding new account '%s'\n", e_account_get_string (account, E_ACCOUNT_NAME)));
		e_account_list_add (accounts, modified_account);
		account = modified_account;

		/* HACK: this will add the account to the folder tree.
		 * We should just be listening to the account list directly for changed events */
		if (account->enabled
		    && emae->priv->source.provider
		    && (emae->priv->source.provider->flags & CAMEL_PROVIDER_IS_STORAGE)) {
			EMailBackend *backend;

			backend = em_account_editor_get_backend (emae);
			e_mail_store_add_by_account (backend, account);
		}
	}

	if (gtk_toggle_button_get_active (emae->priv->default_account))
		e_account_list_set_default (accounts, account);

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
	EAccount *original_account;
	EAccount *modified_account;
	gint i, index;
	GSList *l;
	GList *prov;
	EMConfig *ec;
	EMConfigTargetAccount *target;
	GHashTable *have;
	EConfigItem *items;

	emae->type = type;

	/* sort the providers, remote first */
	priv->providers = g_list_sort (camel_provider_list (TRUE), (GCompareFunc) provider_compare);

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

	original_account = em_account_editor_get_original_account (emae);
	modified_account = em_account_editor_get_modified_account (emae);
	target = em_config_target_new_account (
		ec, original_account, modified_account,
		emae->priv->source.settings);
	e_config_set_target ((EConfig *) ec, (EConfigTarget *) target);
}

