/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
  work before merge can occur:

  verify behaviour.
  work out what to do with the startup druid.

  also need to work out:
  how to remove unecessary items from a service url once
   configured (removing settings from other types).

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#include <glib/gstdio.h>

#include <string.h>
#include <stdarg.h>

#include <gconf/gconf-client.h>

#include <glade/glade.h>

#include <libgnomeui/gnome-druid.h>
#include <libgnomeui/gnome-druid-page-standard.h>

#include <libedataserver/e-account-list.h>
#include <e-util/e-signature-list.h>

#include "e-util/e-error.h"
#include "e-util/e-util-private.h"

#include "em-config.h"
#include "em-folder-selection-button.h"
#include "em-account-editor.h"
#include "mail-session.h"
#include "mail-send-recv.h"
#include "mail-signature-editor.h"
#include "mail-component.h"
#include "em-utils.h"
#include "em-composer-prefs.h"
#include "mail-config.h"
#include "mail-ops.h"
#include "mail-mt.h"

#if defined (HAVE_NSS)
#include "smime/gui/e-cert-selector.h"
#endif

#define d(x)

/* econfig item for the extra config hings */
struct _receive_options_item {
	EMConfigItem item;

	/* Only CAMEL_PROVIDER_CONF_ENTRYs GtkEntrys are stored here.
	   The auto-detect camel provider code will probably be removed */
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
	GtkLabel *userlabel;
	GtkEntry *username;
	GtkEntry *path;
	GtkLabel *pathlabel;
	GtkWidget *pathentry;
	GtkWidget *port_hint;

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

	GtkWidget *check_dialog;
	gint check_id;

	GList *authtypes;	/* if "Check supported" */
	CamelProvider *provider;
	CamelProviderType type;

	gint auth_changed_id;
} EMAccountEditorService;

struct _EMAccountEditorPrivate {
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
	GtkWidget *widgets[5];
	const gchar *widgets_name[5];
	gint widgets_index;

	/* for druid page preparation */
	guint identity_set:1;
	guint receive_set:1;
	guint send_set:1;
	guint management_set:1;
};

static void emae_refresh_authtype (EMAccountEditor *emae, EMAccountEditorService *service);
static void em_account_editor_construct (EMAccountEditor *emae, EAccount *account, em_account_editor_t type, const gchar *id);
static void emae_account_folder_changed (EMFolderSelectionButton *folder, EMAccountEditor *emae);
static GtkVBoxClass *emae_parent;

static void
emae_init (GObject *o)
{
	EMAccountEditor *emae = (EMAccountEditor *)o;

	emae->priv = g_malloc0(sizeof (*emae->priv));

	emae->priv->source.emae = emae;
	emae->priv->transport.emae = emae;
}

static void
emae_finalise (GObject *o)
{
	EMAccountEditor *emae = (EMAccountEditor *)o;
	EMAccountEditorPrivate *p = emae->priv;

	if (p->sig_added_id) {
		ESignatureList *signatures = mail_config_get_signatures ();

		g_signal_handler_disconnect (signatures, p->sig_added_id);
		g_signal_handler_disconnect (signatures, p->sig_removed_id);
		g_signal_handler_disconnect (signatures, p->sig_changed_id);
	}

	g_list_free (p->source.authtypes);
	g_list_free (p->transport.authtypes);

	g_list_free (p->providers);
	g_free (p);

	g_object_unref (emae->account);
	if (emae->original)
		g_object_unref (emae->original);

	((GObjectClass *)emae_parent)->finalize (o);
}

static void
emae_class_init (GObjectClass *klass)
{
	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	klass->finalize = emae_finalise;
}

GType
em_account_editor_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (EMAccountEditorClass),
			NULL, NULL,
			(GClassInitFunc)emae_class_init,
			NULL, NULL,
			sizeof (EMAccountEditor), 0,
			(GInstanceInitFunc)emae_init
		};
		emae_parent = g_type_class_ref (G_TYPE_OBJECT);
		type = g_type_register_static (G_TYPE_OBJECT, "EMAccountEditor", &info, 0);
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
EMAccountEditor *em_account_editor_new (EAccount *account, em_account_editor_t type, const gchar *id)
{
	EMAccountEditor *emae = g_object_new (em_account_editor_get_type (), NULL);

	em_account_editor_construct (emae, account, type, id);

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
EMAccountEditor *em_account_editor_new_for_pages (EAccount *account, em_account_editor_t type, gchar *id, GtkWidget **pages)
{
	EMAccountEditor *emae = g_object_new (em_account_editor_get_type (), NULL);
	emae->pages = pages;
	em_account_editor_construct (emae, account, type, id);

	return emae;
}

/* ********************************************************************** */

static struct {
	const gchar *label;
	const gchar *value;
} ssl_options[] = {
	/* Translators: This string is a "Use secure connection" option for
	   the Mailer. It will not use an encrypted connection. */
	{ N_("No encryption"), "never" },
	/* Translators: This string is a "Use secure connection" option for
	   the Mailer. TLS (Transport Layer Security) is commonly known by
	   this abbreviation. */
	{ N_("TLS encryption"), "when-possible" },
	/* Translators: This string is a "Use secure connection" option for
	   the Mailer. SSL (Secure Sockets Layer) is commonly known by this
	   abbreviation. */
	{ N_("SSL encryption"), "always" }
};

#define num_ssl_options (sizeof (ssl_options) / sizeof (ssl_options[0]))

static gboolean
is_email (const gchar *address)
{
	/* This is supposed to check if the address's domain could be
           an FQDN but alas, it's not worth the pain and suffering. */
	const gchar *at;

	at = strchr (address, '@');
	/* make sure we have an '@' and that it's not the first or last gchar */
	if (!at || at == address || *(at + 1) == '\0')
		return FALSE;

	return TRUE;
}

static CamelURL *
emae_account_url (EMAccountEditor *emae, gint urlid)
{
	CamelURL *url = NULL;
	const gchar *uri;

	uri = e_account_get_string (emae->account, urlid);

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
emae_license_state (GtkToggleButton *button, GtkDialog *dialog)
{
	gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_ACCEPT,
					  gtk_toggle_button_get_active (button));
}

static gboolean
emae_load_text (GtkTextView *view, const gchar *filename)
{
	FILE *fd;
	gchar filebuf[1024];
	GtkTextIter iter;
	GtkTextBuffer *buffer;
	gint count;

	g_return_val_if_fail (filename != NULL , FALSE);

	fd = g_fopen (filename, "r");
	if (fd) {
		buffer =  gtk_text_buffer_new (NULL);
		gtk_text_buffer_get_start_iter (buffer, &iter);
		while (!feof (fd) && !ferror (fd)) {
			count = fread (filebuf, 1, sizeof (filebuf), fd);
			gtk_text_buffer_insert (buffer, &iter, filebuf, count);
		}

		gtk_text_view_set_buffer (GTK_TEXT_VIEW (view), GTK_TEXT_BUFFER(buffer));
		fclose (fd);
	}

	return fd != NULL;
}

static gboolean
emae_display_license (EMAccountEditor *emae, CamelProvider *prov)
{
	GladeXML *xml;
	GtkWidget *w, *dialog;
	gchar *tmp;
	GtkResponseType response = GTK_RESPONSE_NONE;
	gchar *gladefile;

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "mail-dialogs.glade",
				      NULL);
	xml = glade_xml_new (gladefile, "license_dialog", GETTEXT_PACKAGE);
	g_free (gladefile);

	dialog = glade_xml_get_widget (xml, "license_dialog");
	gtk_dialog_set_response_sensitive ((GtkDialog *)dialog, GTK_RESPONSE_ACCEPT, FALSE);
	tmp = g_strdup_printf (_("%s License Agreement"), prov->license);
	gtk_window_set_title ((GtkWindow *)dialog, tmp);
	g_free (tmp);

	g_signal_connect (glade_xml_get_widget (xml, "license_checkbutton"),
			 "toggled", G_CALLBACK(emae_license_state), dialog);

	tmp = g_strdup_printf (_("\nPlease read carefully the license agreement\n"
				"for %s displayed below\n"
				"and tick the check box for accepting it\n"), prov->license);
	gtk_label_set_text ((GtkLabel *)glade_xml_get_widget (xml, "license_top_label"), tmp);
	g_free (tmp);

	w = glade_xml_get_widget (xml, "license_textview");
	if (emae_load_text ((GtkTextView *)w, prov->license_file)) {
		gtk_text_view_set_editable ((GtkTextView *)w, FALSE);
		response = gtk_dialog_run ((GtkDialog *)dialog);
	} else {
		e_error_run (emae->editor ? (GtkWindow *)gtk_widget_get_toplevel (emae->editor) : NULL,
			    "mail:no-load-license", prov->license_file, NULL);
	}

	gtk_widget_destroy (dialog);
	g_object_unref (xml);

	return (response == GTK_RESPONSE_ACCEPT);
}

static gboolean
emae_check_license (EMAccountEditor *emae, CamelProvider *prov)
{
	gboolean accepted = TRUE;

	if (prov->flags & CAMEL_PROVIDER_HAS_LICENSE) {
		GConfClient *gconf = mail_config_get_gconf_client ();
		GSList *providers_list, *l;

		providers_list = gconf_client_get_list (gconf, "/apps/evolution/mail/licenses", GCONF_VALUE_STRING, NULL);

		for (l = providers_list, accepted = FALSE; l && !accepted; l = g_slist_next (l))
			accepted = (strcmp ((gchar *)l->data, prov->protocol) == 0);

		if (!accepted
		    && (accepted = emae_display_license (emae, prov)) == TRUE) {
			providers_list = g_slist_append (providers_list, g_strdup (prov->protocol));
			gconf_client_set_list (gconf,
					      "/apps/evolution/mail/licenses",
					      GCONF_VALUE_STRING,
					      providers_list, NULL);
		}

		g_slist_foreach (providers_list, (GFunc)g_free, NULL);
		g_slist_free (providers_list);
	}

	return accepted;
}

static void
default_folders_clicked (GtkButton *button, gpointer user_data)
{
	EMAccountEditor *emae = user_data;
	const gchar *uri;

	uri = mail_component_get_folder_uri (NULL, MAIL_COMPONENT_FOLDER_DRAFTS);
	em_folder_selection_button_set_selection ((EMFolderSelectionButton *)emae->priv->drafts_folder_button, uri);
	emae_account_folder_changed ((EMFolderSelectionButton *)emae->priv->drafts_folder_button, emae);

	uri = mail_component_get_folder_uri (NULL, MAIL_COMPONENT_FOLDER_SENT);
	em_folder_selection_button_set_selection ((EMFolderSelectionButton *)emae->priv->sent_folder_button, uri);
	emae_account_folder_changed ((EMFolderSelectionButton *)emae->priv->sent_folder_button, emae);
}

/* custom widget factories */
GtkWidget *em_account_editor_folder_selector_button_new (gchar *widget_name, gchar *string1, gchar *string2, gint int1, gint int2);

GtkWidget *
em_account_editor_folder_selector_button_new (gchar *widget_name, gchar *string1, gchar *string2, gint int1, gint int2)
{
	return (GtkWidget *)em_folder_selection_button_new (string1 ? string1 : _("Select Folder"), NULL);
}

GtkWidget *em_account_editor_dropdown_new (gchar *widget_name, gchar *string1, gchar *string2, gint int1, gint int2);

GtkWidget *
em_account_editor_dropdown_new (gchar *widget_name, gchar *string1, gchar *string2, gint int1, gint int2)
{
	return (GtkWidget *)gtk_combo_box_new ();
}

GtkWidget *em_account_editor_ssl_selector_new (gchar *widget_name, gchar *string1, gchar *string2, gint int1, gint int2);

GtkWidget *
em_account_editor_ssl_selector_new (gchar *widget_name, gchar *string1, gchar *string2, gint int1, gint int2)
{
	GtkComboBox *dropdown = (GtkComboBox *)gtk_combo_box_new ();
	GtkCellRenderer *cell = gtk_cell_renderer_text_new ();
	GtkListStore *store;
	gint i;
	GtkTreeIter iter;

	gtk_widget_show ((GtkWidget *)dropdown);

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);

	for (i=0;i<num_ssl_options;i++) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, _(ssl_options[i].label), 1, ssl_options[i].value, -1);
	}

	gtk_cell_layout_pack_start ((GtkCellLayout *)dropdown, cell, TRUE);
	gtk_cell_layout_set_attributes ((GtkCellLayout *)dropdown, cell, "text", 0, NULL);

	gtk_combo_box_set_model (dropdown, (GtkTreeModel *)store);

	return (GtkWidget *)dropdown;
}

/* The camel provider auto-detect interface should be deprecated.
   But it still needs to be replaced with something of similar functionality.
   Just using the normal econfig plugin mechanism should be adequate. */
static void
emae_auto_detect_free (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_free (value);
}

static void
emae_auto_detect (EMAccountEditor *emae)
{
	EMAccountEditorPrivate *gui = emae->priv;
	EMAccountEditorService *service = &gui->source;
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
		for (l = emae->priv->extra_items;l;l=g_slist_next (l)) {
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
provider_compare (const CamelProvider *p1, const CamelProvider *p2)
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
emae_signature_added (ESignatureList *signatures, ESignature *sig, EMAccountEditor *emae)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	const gchar *name;
	const gchar *uid;

	name = e_signature_get_name (sig);
	uid = e_signature_get_uid (sig);

	model = gtk_combo_box_get_model (emae->priv->signatures_dropdown);

	gtk_list_store_append ((GtkListStore *)model, &iter);
	gtk_list_store_set ((GtkListStore *)model, &iter, 0, name, 1, uid, -1);

	gtk_combo_box_set_active (emae->priv->signatures_dropdown, gtk_tree_model_iter_n_children (model, NULL)-1);
}

static gint
emae_signature_get_iter (EMAccountEditor *emae, ESignature *sig, GtkTreeModel **modelp, GtkTreeIter *iter)
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
emae_signature_removed (ESignatureList *signatures, ESignature *sig, EMAccountEditor *emae)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (emae_signature_get_iter (emae, sig, &model, &iter))
		gtk_list_store_remove ((GtkListStore *)model, &iter);
}

static void
emae_signature_changed (ESignatureList *signatures, ESignature *sig, EMAccountEditor *emae)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	const gchar *name;

	name = e_signature_get_name (sig);

	if (emae_signature_get_iter (emae, sig, &model, &iter))
		gtk_list_store_set ((GtkListStore *)model, &iter, 0, name, -1);
}

static void
emae_signaturetype_changed (GtkComboBox *dropdown, EMAccountEditor *emae)
{
	gint id = gtk_combo_box_get_active (dropdown);
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *uid = NULL;

	if (id != -1) {
		model = gtk_combo_box_get_model (dropdown);
		if (gtk_tree_model_iter_nth_child (model, &iter, NULL, id))
			gtk_tree_model_get (model, &iter, 1, &uid, -1);
	}

	e_account_set_string (emae->account, E_ACCOUNT_ID_SIGNATURE, uid);
	g_free (uid);
}

static void
emae_signature_new (GtkWidget *w, EMAccountEditor *emae)
{
	/* TODO: why is this in composer prefs? apart from it being somewhere to put it? */
	em_composer_prefs_new_signature ((GtkWindow *)gtk_widget_get_toplevel (w),
					gconf_client_get_bool (mail_config_get_gconf_client (),
							      "/apps/evolution/mail/composer/send_html", NULL));
}

static GtkWidget *
emae_setup_signatures (EMAccountEditor *emae, GladeXML *xml)
{
	EMAccountEditorPrivate *p = emae->priv;
	GtkComboBox *dropdown = (GtkComboBox *)glade_xml_get_widget (xml, "signature_dropdown");
	GtkCellRenderer *cell = gtk_cell_renderer_text_new ();
	GtkListStore *store;
	gint i, active=0;
	GtkTreeIter iter;
	ESignatureList *signatures;
	EIterator *it;
	const gchar *current = e_account_get_string (emae->account, E_ACCOUNT_ID_SIGNATURE);
	GtkWidget *button;

	emae->priv->signatures_dropdown = dropdown;
	gtk_widget_show ((GtkWidget *)dropdown);

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, _("None"), 1, NULL, -1);

	signatures = mail_config_get_signatures ();

	if (p->sig_added_id == 0) {
		p->sig_added_id = g_signal_connect (signatures, "signature-added", G_CALLBACK(emae_signature_added), emae);
		p->sig_removed_id = g_signal_connect (signatures, "signature-removed", G_CALLBACK(emae_signature_removed), emae);
		p->sig_changed_id = g_signal_connect (signatures, "signature-changed", G_CALLBACK(emae_signature_changed), emae);
	}

	/* we need to count the 'none' entry before using the index */
	i = 1;
	it = e_list_get_iterator ((EList *) signatures);
	while (e_iterator_is_valid (it)) {
		ESignature *sig = (ESignature *)e_iterator_get (it);
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

	gtk_cell_layout_pack_start ((GtkCellLayout *)dropdown, cell, TRUE);
	gtk_cell_layout_set_attributes ((GtkCellLayout *)dropdown, cell, "text", 0, NULL);

	gtk_combo_box_set_model (dropdown, (GtkTreeModel *)store);
	gtk_combo_box_set_active (dropdown, active);

	g_signal_connect (dropdown, "changed", G_CALLBACK(emae_signaturetype_changed), emae);
	gtk_widget_set_sensitive ((GtkWidget *)dropdown, e_account_writable (emae->account, E_ACCOUNT_ID_SIGNATURE));

	button = glade_xml_get_widget (xml, "sigAddNew");
	g_signal_connect (button, "clicked", G_CALLBACK(emae_signature_new), emae);
	gtk_widget_set_sensitive (button,
				 gconf_client_key_is_writable (mail_config_get_gconf_client (),
							      "/apps/evolution/mail/signatures", NULL));

	return (GtkWidget *)dropdown;
}

static void
emae_receipt_policy_changed (GtkComboBox *dropdown, EMAccountEditor *emae)
{
	gint id = gtk_combo_box_get_active (dropdown);
	GtkTreeModel *model;
	GtkTreeIter iter;
	EAccountReceiptPolicy policy;

	if (id != -1) {
		model = gtk_combo_box_get_model (dropdown);
		if (gtk_tree_model_iter_nth_child (model, &iter, NULL, id)) {
			gtk_tree_model_get (model, &iter, 1, &policy, -1);
			e_account_set_int (emae->account, E_ACCOUNT_RECEIPT_POLICY, policy);
		}
	}

}

static GtkWidget *
emae_setup_receipt_policy (EMAccountEditor *emae, GladeXML *xml)
{
	GtkComboBox *dropdown = (GtkComboBox *)glade_xml_get_widget (xml, "receipt_policy_dropdown");
	GtkListStore *store;
	gint i = 0, active = 0;
	GtkTreeIter iter;
	EAccountReceiptPolicy current = emae->account->receipt_policy;
	static struct {
		EAccountReceiptPolicy policy;
		const gchar *label;
	} receipt_policies[] = {
		{ E_ACCOUNT_RECEIPT_NEVER,  N_("Never") },
		{ E_ACCOUNT_RECEIPT_ALWAYS, N_("Always") },
		{ E_ACCOUNT_RECEIPT_ASK,    N_("Ask for each message") }
	};

	gtk_widget_show ((GtkWidget *)dropdown);

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

	gtk_combo_box_set_model (dropdown, (GtkTreeModel *)store);
	gtk_combo_box_set_active (dropdown, active);

	g_signal_connect (dropdown, "changed", G_CALLBACK(emae_receipt_policy_changed), emae);
	gtk_widget_set_sensitive ((GtkWidget *)dropdown, e_account_writable (emae->account, E_ACCOUNT_RECEIPT_POLICY));

	return (GtkWidget *)dropdown;
}

static void
emae_account_entry_changed (GtkEntry *entry, EMAccountEditor *emae)
{
	gint item = GPOINTER_TO_INT(g_object_get_data ((GObject *)entry, "account-item"));

	e_account_set_string (emae->account, item, gtk_entry_get_text (entry));
}

static GtkEntry *
emae_account_entry (EMAccountEditor *emae, const gchar *name, gint item, GladeXML *xml)
{
	GtkEntry *entry;
	const gchar *text;

	entry = (GtkEntry *)glade_xml_get_widget (xml, name);
	text = e_account_get_string (emae->account, item);
	if (text)
		gtk_entry_set_text (entry, text);
	g_object_set_data ((GObject *)entry, "account-item", GINT_TO_POINTER(item));
	g_signal_connect (entry, "changed", G_CALLBACK(emae_account_entry_changed), emae);
	gtk_widget_set_sensitive ((GtkWidget *)entry, e_account_writable (emae->account, item));

	return entry;
}

static void
emae_account_toggle_changed (GtkToggleButton *toggle, EMAccountEditor *emae)
{
	gint item = GPOINTER_TO_INT(g_object_get_data ((GObject *)toggle, "account-item"));

	e_account_set_bool (emae->account, item, gtk_toggle_button_get_active (toggle));
}

static void
emae_account_toggle_widget (EMAccountEditor *emae, GtkToggleButton *toggle, gint item)
{
	gtk_toggle_button_set_active (toggle, e_account_get_bool (emae->account, item));
	g_object_set_data ((GObject *)toggle, "account-item", GINT_TO_POINTER(item));
	g_signal_connect (toggle, "toggled", G_CALLBACK(emae_account_toggle_changed), emae);
	gtk_widget_set_sensitive ((GtkWidget *)toggle, e_account_writable (emae->account, item));
}

static GtkToggleButton *
emae_account_toggle (EMAccountEditor *emae, const gchar *name, gint item, GladeXML *xml)
{
	GtkToggleButton *toggle;

	toggle = (GtkToggleButton *)glade_xml_get_widget (xml, name);
	emae_account_toggle_widget (emae, toggle, item);

	return toggle;
}

static void
emae_account_spinint_changed (GtkSpinButton *spin, EMAccountEditor *emae)
{
	gint item = GPOINTER_TO_INT(g_object_get_data ((GObject *)spin, "account-item"));

	e_account_set_int (emae->account, item, gtk_spin_button_get_value (spin));
}

static void
emae_account_spinint_widget (EMAccountEditor *emae, GtkSpinButton *spin, gint item)
{
	gtk_spin_button_set_value (spin, e_account_get_int (emae->account, item));
	g_object_set_data ((GObject *)spin, "account-item", GINT_TO_POINTER(item));
	g_signal_connect (spin, "value_changed", G_CALLBACK(emae_account_spinint_changed), emae);
	gtk_widget_set_sensitive ((GtkWidget *)spin, e_account_writable (emae->account, item));
}

#if 0
static GtkSpinButton *
emae_account_spinint (EMAccountEditor *emae, const gchar *name, gint item)
{
	GtkSpinButton *spin;

	spin = (GtkSpinButton *)glade_xml_get_widget (emae->priv->xml, name);
	emae_account_spinint_widget (emae, spin, item);

	return spin;
}
#endif

static void
emae_account_folder_changed (EMFolderSelectionButton *folder, EMAccountEditor *emae)
{
	gint item = GPOINTER_TO_INT(g_object_get_data ((GObject *)folder, "account-item"));

	e_account_set_string (emae->account, item, em_folder_selection_button_get_selection (folder));
}

static EMFolderSelectionButton *
emae_account_folder (EMAccountEditor *emae, const gchar *name, gint item, gint deffolder, GladeXML *xml)
{
	EMFolderSelectionButton *folder;
	const gchar *uri;

	folder = (EMFolderSelectionButton *)glade_xml_get_widget (xml, name);
	uri = e_account_get_string (emae->account, item);
	if (uri) {
		gchar *tmp = em_uri_to_camel (uri);

		em_folder_selection_button_set_selection (folder, tmp);
		g_free (tmp);
	} else {
		em_folder_selection_button_set_selection (folder, mail_component_get_folder_uri (NULL, deffolder));
	}

	g_object_set_data ((GObject *)folder, "account-item", GINT_TO_POINTER(item));
	g_object_set_data ((GObject *)folder, "folder-default", GINT_TO_POINTER(deffolder));
	g_signal_connect (folder, "selected", G_CALLBACK(emae_account_folder_changed), emae);
	gtk_widget_show ((GtkWidget *)folder);

	gtk_widget_set_sensitive ((GtkWidget *)folder, e_account_writable (emae->account, item));

	return folder;
}

#if defined (HAVE_NSS)
static void
smime_changed (EMAccountEditor *emae)
{
	EMAccountEditorPrivate *gui = emae->priv;
	gint act;
	const gchar *tmp;

	tmp = gtk_entry_get_text (gui->smime_sign_key);
	act = tmp && tmp[0];
	gtk_widget_set_sensitive ((GtkWidget *)gui->smime_sign_key_clear, act);
	gtk_widget_set_sensitive ((GtkWidget *)gui->smime_sign_default, act);
	if (!act)
		gtk_toggle_button_set_active (gui->smime_sign_default, FALSE);

	tmp = gtk_entry_get_text (gui->smime_encrypt_key);
	act = tmp && tmp[0];
	gtk_widget_set_sensitive ((GtkWidget *)gui->smime_encrypt_key_clear, act);
	gtk_widget_set_sensitive ((GtkWidget *)gui->smime_encrypt_default, act);
	gtk_widget_set_sensitive ((GtkWidget *)gui->smime_encrypt_to_self, act);
	if (!act) {
		gtk_toggle_button_set_active (gui->smime_encrypt_default, FALSE);
		gtk_toggle_button_set_active (gui->smime_encrypt_to_self, FALSE);
	}
}

static void
smime_sign_key_selected (GtkWidget *dialog, const gchar *key, EMAccountEditor *emae)
{
	EMAccountEditorPrivate *gui = emae->priv;

	if (key != NULL) {
		gtk_entry_set_text (gui->smime_sign_key, key);
		smime_changed (emae);
	}

	gtk_widget_destroy (dialog);
}

static void
smime_sign_key_select (GtkWidget *button, EMAccountEditor *emae)
{
	EMAccountEditorPrivate *gui = emae->priv;
	GtkWidget *w;

	w = e_cert_selector_new (E_CERT_SELECTOR_SIGNER, gtk_entry_get_text (gui->smime_sign_key));
	gtk_window_set_modal ((GtkWindow *)w, TRUE);
	gtk_window_set_transient_for ((GtkWindow *)w, (GtkWindow *)gtk_widget_get_toplevel (button));
	g_signal_connect (w, "selected", G_CALLBACK(smime_sign_key_selected), emae);
	gtk_widget_show (w);
}

static void
smime_sign_key_clear (GtkWidget *w, EMAccountEditor *emae)
{
	EMAccountEditorPrivate *gui = emae->priv;

	gtk_entry_set_text (gui->smime_sign_key, "");
	smime_changed (emae);
}

static void
smime_encrypt_key_selected (GtkWidget *dialog, const gchar *key, EMAccountEditor *emae)
{
	EMAccountEditorPrivate *gui = emae->priv;

	if (key != NULL) {
		gtk_entry_set_text (gui->smime_encrypt_key, key);
		smime_changed (emae);
	}

	gtk_widget_destroy (dialog);
}

static void
smime_encrypt_key_select (GtkWidget *button, EMAccountEditor *emae)
{
	EMAccountEditorPrivate *gui = emae->priv;
	GtkWidget *w;

	w = e_cert_selector_new (E_CERT_SELECTOR_SIGNER, gtk_entry_get_text (gui->smime_encrypt_key));
	gtk_window_set_modal ((GtkWindow *)w, TRUE);
	gtk_window_set_transient_for ((GtkWindow *)w, (GtkWindow *)gtk_widget_get_toplevel (button));
	g_signal_connect (w, "selected", G_CALLBACK(smime_encrypt_key_selected), emae);
	gtk_widget_show (w);
}

static void
smime_encrypt_key_clear (GtkWidget *w, EMAccountEditor *emae)
{
	EMAccountEditorPrivate *gui = emae->priv;

	gtk_entry_set_text (gui->smime_encrypt_key, "");
	smime_changed (emae);
}
#endif

static void
emae_url_set_hostport (CamelURL *url, const gchar *txt)
{
	const gchar *port;
	gchar *host;

	/* FIXME: what if this was a raw IPv6 address? */
	if (txt && (port = strchr (txt, ':'))) {
		camel_url_set_port (url, atoi (port+1));
		host = g_strdup (txt);
		host[port-txt] = 0;
	} else {
		/* "" is converted to NULL, but if we set NULL on the url,
		   camel_url_to_string strips lots of details */
		host = g_strdup ((txt?txt:""));
		camel_url_set_port (url, 0);
	}

	g_strstrip (host);
	if (txt && *txt)
		camel_url_set_host (url, host);

	g_free (host);
}

/* This is used to map a funciton which will set on the url a string value.
   if widgets[0] is set, it is the entry which will be called against setval ()
   We need our own function for host:port decoding, as above */
struct _provider_host_info {
	guint32 flag;
	void (*setval)(CamelURL *, const gchar *);
	glong widgets[3];
};

static struct _provider_host_info emae_source_host_info[] = {
	{ CAMEL_URL_PART_HOST, emae_url_set_hostport, { G_STRUCT_OFFSET(EMAccountEditorService, hostname), G_STRUCT_OFFSET(EMAccountEditorService, hostlabel), }, },
	{ CAMEL_URL_PART_USER, camel_url_set_user, { G_STRUCT_OFFSET(EMAccountEditorService, username), G_STRUCT_OFFSET(EMAccountEditorService, userlabel), } },
	{ CAMEL_URL_PART_PATH, camel_url_set_path, { G_STRUCT_OFFSET(EMAccountEditorService, path), G_STRUCT_OFFSET(EMAccountEditorService, pathlabel), G_STRUCT_OFFSET(EMAccountEditorService, pathentry) }, },
	{ CAMEL_URL_PART_AUTH, NULL, { 0, G_STRUCT_OFFSET(EMAccountEditorService, auth_frame), }, },
	{ 0 },
};

static struct _provider_host_info emae_transport_host_info[] = {
	{ CAMEL_URL_PART_HOST, emae_url_set_hostport, { G_STRUCT_OFFSET(EMAccountEditorService, hostname), G_STRUCT_OFFSET(EMAccountEditorService, hostlabel), }, },
	{ CAMEL_URL_PART_USER, camel_url_set_user, { G_STRUCT_OFFSET(EMAccountEditorService, username), G_STRUCT_OFFSET(EMAccountEditorService, userlabel), } },
	{ CAMEL_URL_PART_AUTH, NULL, { 0, G_STRUCT_OFFSET(EMAccountEditorService, auth_frame), }, },
	{ 0 },
};

/* This is used to map each of the two services in a typical account to the widgets that represent each service.
   i.e. the receiving (source) service, and the sending (transport) service.
   It is used throughout the following code to drive each page */
static struct _service_info {
	gint account_uri_key;
	gint save_passwd_key;

	const gchar *frame;
	const gchar *type_dropdown;

	const gchar *container;
	const gchar *description;
	const gchar *hostname;
	const gchar *hostlabel;
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
	{ E_ACCOUNT_SOURCE_URL, E_ACCOUNT_SOURCE_SAVE_PASSWD,
	  "source_frame", "source_type_dropdown",
	  "source_vbox", "source_description", "source_host", "source_host_label", "source_user", "source_user_label", "source_path", "source_path_label", "source_path_entry",
	  "source_security_frame", "source_ssl_hbox", "source_use_ssl", "source_ssl_disabled",
	  NULL, "source_auth_frame",
	  "source_auth_dropdown", "source_check_supported",
	  "source_remember_password",
	  emae_source_host_info,
	},
	{ E_ACCOUNT_TRANSPORT_URL, E_ACCOUNT_TRANSPORT_SAVE_PASSWD,
	  "transport_frame", "transport_type_dropdown",
	  "transport_vbox", "transport_description", "transport_host", "transport_host_label", "transport_user", "transport_user_label", NULL, NULL, NULL,
	  "transport_security_frame", "transport_ssl_hbox", "transport_use_ssl", "transport_ssl_disabled",
	  "transport_needs_auth", "transport_auth_frame",
	  "transport_auth_dropdown", "transport_check_supported",
	  "transport_remember_password",
	  emae_transport_host_info,
	},
};

static void
emae_uri_changed (EMAccountEditorService *service, CamelURL *url)
{
	gchar *uri;

	uri = camel_url_to_string (url, 0);

	e_account_set_string (service->emae->account, emae_service_info[service->type].account_uri_key, uri);

	/* small hack for providers which are store and transport - copy settings across */
	if (service->type == CAMEL_PROVIDER_STORE
	    && service->provider
	    && CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT(service->provider))
		e_account_set_string (service->emae->account, E_ACCOUNT_TRANSPORT_URL, uri);

	g_free (uri);
}

static void
emae_service_url_changed (EMAccountEditorService *service, void (*setval)(CamelURL *, const gchar *), GtkEntry *entry)
{
        GtkComboBox *dropdown;
        gint id;
        GtkTreeModel *model;
        GtkTreeIter iter;
        CamelServiceAuthType *authtype;

        CamelURL *url = emae_account_url (service->emae, emae_service_info[service->type].account_uri_key);
        const gchar *text = gtk_entry_get_text (entry);

        setval (url, (text && text[0])?text:NULL);

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
emae_service_url_path_changed (EMAccountEditorService *service, void (*setval)(CamelURL *, const gchar *), GtkWidget *widget)
{
	GtkComboBox *dropdown;
	gint id;
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelServiceAuthType *authtype;

	CamelURL *url = emae_account_url (service->emae, emae_service_info[service->type].account_uri_key);
	const gchar *text = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));

	setval (url, (text && text[0])?text:NULL);

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
emae_hostname_changed (GtkEntry *entry, EMAccountEditorService *service)
{
	emae_service_url_changed (service, emae_url_set_hostport, entry);
}

static void
emae_username_changed (GtkEntry *entry, EMAccountEditorService *service)
{
	emae_service_url_changed (service, camel_url_set_user, entry);
}

static void
emae_path_changed (GtkWidget *widget, EMAccountEditorService *service)
{
	emae_service_url_path_changed (service, camel_url_set_path, widget);
}

static gint
emae_ssl_update (EMAccountEditorService *service, CamelURL *url)
{
	gint id = gtk_combo_box_get_active (service->use_ssl);
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *ssl;

	if (id == -1)
		return 0;

	model = gtk_combo_box_get_model (service->use_ssl);
	if (gtk_tree_model_iter_nth_child (model, &iter, NULL, id)) {
		gtk_tree_model_get (model, &iter, 1, &ssl, -1);
		if (!strcmp (ssl, "none"))
			ssl = NULL;
		camel_url_set_param (url, "use_ssl", ssl);
		return 1;
	}

	return 0;
}

static void
emae_ssl_changed (GtkComboBox *dropdown, EMAccountEditorService *service)
{
	CamelURL *url = emae_account_url (service->emae, emae_service_info[service->type].account_uri_key);

	if (emae_ssl_update (service, url))
		emae_uri_changed (service, url);
	camel_url_free (url);
}

static void
emae_service_provider_changed (EMAccountEditorService *service)
{
	gint i, j;
	void (*show)(GtkWidget *);
	CamelURL *url = emae_account_url (service->emae, emae_service_info[service->type].account_uri_key);

	if (service->provider) {
		gint enable;
		GtkWidget *dwidget = NULL;

		camel_url_set_protocol (url, service->provider->protocol);
		gtk_label_set_text (service->description, service->provider->description);
		if (!emae_check_license (service->emae, service->provider))
			gtk_widget_hide (service->frame);
		else
			gtk_widget_show (service->frame);

		enable = e_account_writable_option (service->emae->account, service->provider->protocol, "auth");
		gtk_widget_set_sensitive ((GtkWidget *)service->authtype, enable);
		gtk_widget_set_sensitive ((GtkWidget *)service->check_supported, enable);

		enable = e_account_writable_option (service->emae->account, service->provider->protocol, "use_ssl");
		gtk_widget_set_sensitive ((GtkWidget *)service->use_ssl, enable);

		enable = e_account_writable (service->emae->account, emae_service_info[service->type].save_passwd_key);
		gtk_widget_set_sensitive ((GtkWidget *)service->remember, enable);

		for (i=0;emae_service_info[service->type].host_info[i].flag;i++) {
			GtkWidget *w;
			gint hide;
			struct _provider_host_info *info = &emae_service_info[service->type].host_info[i];

			enable = CAMEL_PROVIDER_ALLOWS(service->provider, info->flag);
			hide = CAMEL_PROVIDER_HIDDEN(service->provider, info->flag);
			show = (enable && !hide)?gtk_widget_show:gtk_widget_hide;

			for (j=0; j < sizeof (info->widgets)/sizeof (info->widgets[0]); j++) {
				if (info->widgets[j] && (w = G_STRUCT_MEMBER(GtkWidget *, service, info->widgets[j]))) {
					show (w);
					if (j == 0) {
						if (dwidget == NULL && enable)
							dwidget = w;

						if (info->setval && !hide)
							info->setval (url, enable?gtk_entry_get_text ((GtkEntry *)w):NULL);
					}
				}
			}
		}

		if (dwidget)
			gtk_widget_grab_focus (dwidget);

		if (CAMEL_PROVIDER_ALLOWS(service->provider, CAMEL_URL_PART_AUTH)) {
			GList *ll;

			/* try to keep the authmech from the current url, or clear it */
			if (url->authmech) {
				if (service->provider->authtypes) {
					for (ll = service->provider->authtypes;ll;ll = g_list_next (ll))
						if (!strcmp (url->authmech, ((CamelServiceAuthType *)ll->data)->authproto))
							break;
					if (ll == NULL)
						camel_url_set_authmech (url, NULL);
				} else {
					camel_url_set_authmech (url, NULL);
				}
			}

			emae_refresh_authtype (service->emae, service);
			if (service->needs_auth && !CAMEL_PROVIDER_NEEDS(service->provider, CAMEL_URL_PART_AUTH))
				gtk_widget_show ((GtkWidget *)service->needs_auth);
		} else {
			if (service->needs_auth)
				gtk_widget_hide ((GtkWidget *)service->needs_auth);
		}
#ifdef HAVE_SSL
		gtk_widget_hide (service->no_ssl);
		if (service->provider->flags & CAMEL_PROVIDER_SUPPORTS_SSL) {
			emae_ssl_update (service, url);
			show = gtk_widget_show;
		} else {
			camel_url_set_param (url, "use_ssl", NULL);
			show = gtk_widget_hide;
		}
		show (service->ssl_frame);
		show (service->ssl_hbox);
#else
		gtk_widget_hide (service->ssl_hbox);
		gtk_widget_show (service->no_ssl);
		camel_url_set_param (url, "use_ssl", NULL);
#endif
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
emae_provider_changed (GtkComboBox *dropdown, EMAccountEditorService *service)
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

	g_list_free (service->authtypes);
	service->authtypes = NULL;

	emae_service_provider_changed (service);

	e_config_target_changed ((EConfig *)service->emae->priv->config, E_CONFIG_TARGET_CHANGED_REBUILD);
}

static void
emae_refresh_providers (EMAccountEditor *emae, EMAccountEditorService *service)
{
	EAccount *account = emae->account;
	GtkListStore *store;
	GtkTreeIter iter;
	GList *l;
	GtkCellRenderer *cell = gtk_cell_renderer_text_new ();
	GtkComboBox *dropdown;
	gint active = 0, i;
	struct _service_info *info = &emae_service_info[service->type];
	const gchar *uri = e_account_get_string (account, info->account_uri_key);
	const gchar *tmp;
	gchar *current = NULL;
	CamelURL *url;

	dropdown = service->providers;
	gtk_widget_show ((GtkWidget *)dropdown);

	if (uri) {
		const gchar *colon = strchr (uri, ':');
		gint len;

		if (colon) {
			len = colon-uri;
			current = g_alloca (len+1);
			memcpy (current, uri, len);
			current[len] = 0;
		}
	} else {
		current = (gchar *)"imap";
	}

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);

	i = 0;

	/* We just special case each type here, its just easier */
	if (service->type == CAMEL_PROVIDER_STORE) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, _("None"), 1, NULL, -1);
		i++;
	}

	for (l=emae->priv->providers; l; l=l->next) {
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
			service->provider = provider;
			active = i;

			/* we need to set this value on the uri too */
			if (current == NULL) {
				CamelURL *url = emae_account_url (emae, info->account_uri_key);

				camel_url_set_protocol (url, provider->protocol);
				emae_uri_changed (service, url);
				camel_url_free (url);
			}
		}
		i++;
	}

	gtk_cell_layout_clear ((GtkCellLayout *)dropdown);
	gtk_combo_box_set_model (dropdown, (GtkTreeModel *)store);
	gtk_cell_layout_pack_start ((GtkCellLayout *)dropdown, cell, TRUE);
	gtk_cell_layout_set_attributes ((GtkCellLayout *)dropdown, cell, "text", 0, NULL);

	g_signal_handlers_disconnect_by_func (dropdown, emae_provider_changed, service);
	gtk_combo_box_set_active (dropdown, -1);	/* needed for gtkcombo bug (?) */
	gtk_combo_box_set_active (dropdown, active);
	g_signal_connect (dropdown, "changed", G_CALLBACK(emae_provider_changed), service);

	if (!uri  || (url = camel_url_new (uri, NULL)) == NULL) {
		return;
	}

	tmp = camel_url_get_param (url, "use_ssl");
	if (tmp == NULL)
		tmp = "never";
	for (i=0;i<num_ssl_options;i++) {
		if (!strcmp (ssl_options[i].value, tmp)) {
			gtk_combo_box_set_active (service->use_ssl, i);
			break;
		}
	}
}

static void
emae_authtype_changed (GtkComboBox *dropdown, EMAccountEditorService *service)
{
	gint id = gtk_combo_box_get_active (dropdown);
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelServiceAuthType *authtype;
	CamelURL *url;

	if (id == -1)
		return;

	url = emae_account_url (service->emae, emae_service_info[service->type].account_uri_key);
	model = gtk_combo_box_get_model (dropdown);
	if (gtk_tree_model_iter_nth_child (model, &iter, NULL, id)) {
		gtk_tree_model_get (model, &iter, 1, &authtype, -1);
		if (authtype)
			camel_url_set_authmech (url, authtype->authproto);
		else
			camel_url_set_authmech (url, NULL);
		emae_uri_changed (service, url);
	}
	camel_url_free (url);

	gtk_widget_set_sensitive ((GtkWidget *)service->remember,
				 authtype
				 ?(authtype->need_password && e_account_writable (service->emae->account, emae_service_info[service->type].save_passwd_key))
				 :FALSE);
}

static void
emae_needs_auth (GtkToggleButton *toggle, EMAccountEditorService *service)
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
emae_refresh_authtype (EMAccountEditor *emae, EMAccountEditorService *service)
{
	EAccount *account = emae->account;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkComboBox *dropdown;
	gint active = 0;
	gint i;
	struct _service_info *info = &emae_service_info[service->type];
	const gchar *uri = e_account_get_string (account, info->account_uri_key);
	GList *l, *ll;
	CamelURL *url = NULL;

	dropdown = service->authtype;
	gtk_widget_show ((GtkWidget *)dropdown);

	store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_BOOLEAN);

	if (uri)
		url = camel_url_new (uri, NULL);

	if (service->provider) {
		for (i=0, l=service->provider->authtypes; l; l=l->next, i++) {
			CamelServiceAuthType *authtype = l->data;
			gint avail;

			/* if we have some already shown */
			if (service->authtypes) {
				for (ll = service->authtypes;ll;ll = g_list_next (ll))
					if (!strcmp (authtype->authproto, ((CamelServiceAuthType *)ll->data)->authproto))
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

	gtk_combo_box_set_model (dropdown, (GtkTreeModel *)store);
	gtk_combo_box_set_active (dropdown, -1);

	if (service->auth_changed_id == 0) {
		GtkCellRenderer *cell = gtk_cell_renderer_text_new ();

		gtk_cell_layout_pack_start ((GtkCellLayout *)dropdown, cell, TRUE);
		gtk_cell_layout_set_attributes ((GtkCellLayout *)dropdown, cell, "text", 0, "strikethrough", 2, NULL);

		service->auth_changed_id = g_signal_connect (dropdown, "changed", G_CALLBACK(emae_authtype_changed), service);
		g_signal_connect (service->check_supported, "clicked", G_CALLBACK(emae_check_authtype), service);
	}

	gtk_combo_box_set_active (dropdown, active);

	if (url)
		camel_url_free (url);
}

static void emae_check_authtype_done (const gchar *uri, CamelProviderType type, GList *types, gpointer data)
{
	EMAccountEditorService *service = data;

	if (service->check_dialog) {
		if (service->authtypes)
			g_list_free (service->authtypes);

		service->authtypes = g_list_copy (types);
		emae_refresh_authtype (service->emae, service);
		gtk_widget_destroy (service->check_dialog);
	}

	if (service->emae->editor)
		gtk_widget_set_sensitive (service->emae->editor, TRUE);

	service->check_id = -1;
	g_object_unref (service->emae);
}

static void emae_check_authtype_response (GtkWidget *d, gint button, EMAccountEditorService *service)
{
	mail_msg_cancel (service->check_id);
	gtk_widget_destroy (service->check_dialog);
	service->check_dialog = NULL;

	if (service->emae->editor)
		gtk_widget_set_sensitive (service->emae->editor, TRUE);
}

static void emae_check_authtype (GtkWidget *w, EMAccountEditorService *service)
{
	EMAccountEditor *emae = service->emae;
	const gchar *uri;

	/* TODO: do we need to remove the auth mechanism from the uri? */
	uri = e_account_get_string (emae->account, emae_service_info[service->type].account_uri_key);
	g_object_ref (emae);

	service->check_dialog = e_error_new (emae->editor ? (GtkWindow *)gtk_widget_get_toplevel (emae->editor) : NULL,
					    "mail:checking-service", NULL);
	g_signal_connect (service->check_dialog, "response", G_CALLBACK(emae_check_authtype_response), service);
	gtk_widget_show (service->check_dialog);
	if (emae->editor)
		gtk_widget_set_sensitive (emae->editor, FALSE);
	service->check_id = mail_check_service (uri, service->type, emae_check_authtype_done, service);
}

static void
emae_setup_service (EMAccountEditor *emae, EMAccountEditorService *service, GladeXML *xml)
{
	struct _service_info *info = &emae_service_info[service->type];
	CamelURL *url = emae_account_url (emae, info->account_uri_key);
	const gchar *uri = e_account_get_string (emae->account, info->account_uri_key);
	const gchar *tmp;
	gint i;

	service->provider = uri?camel_provider_get (uri, NULL):NULL;
	service->frame = glade_xml_get_widget (xml, info->frame);
	service->container = glade_xml_get_widget (xml, info->container);
	service->description = GTK_LABEL (glade_xml_get_widget (xml, info->description));
	service->hostname = GTK_ENTRY (glade_xml_get_widget (xml, info->hostname));
	service->hostlabel = (GtkLabel *)glade_xml_get_widget (xml, info->hostlabel);
	service->username = GTK_ENTRY (glade_xml_get_widget (xml, info->username));
	service->userlabel = (GtkLabel *)glade_xml_get_widget (xml, info->userlabel);
	if (info->pathentry) {
		service->pathlabel = (GtkLabel *)glade_xml_get_widget (xml, info->pathlabel);
		service->pathentry = glade_xml_get_widget (xml, info->pathentry);
	}

	service->port_hint = glade_xml_get_widget (xml, "port_hint");
	if (emae->type == EMAE_PAGES && (!service->provider || CAMEL_PROVIDER_NEEDS(service->provider, CAMEL_URL_PART_HOST)))
		gtk_widget_show (service->port_hint);
	else
		gtk_widget_hide (service->port_hint);

	service->ssl_frame = glade_xml_get_widget (xml, info->security_frame);
	gtk_widget_hide (service->ssl_frame);
	service->ssl_hbox = glade_xml_get_widget (xml, info->ssl_hbox);
	service->use_ssl = (GtkComboBox *)glade_xml_get_widget (xml, info->use_ssl);
	service->no_ssl = glade_xml_get_widget (xml, info->ssl_disabled);

	/* configure ui for current settings */
	if (url->host) {
		if (url->port) {
			gchar *host = g_strdup_printf ("%s:%d", url->host, url->port);

			gtk_entry_set_text (service->hostname, host);
			g_free (host);
		} else
			gtk_entry_set_text (service->hostname, url->host);
	}
	if (url->user && *url->user) {
		gtk_entry_set_text (service->username, url->user);
	}
	if (service->pathentry) {
		GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;

		if (service->provider && (service->provider->url_flags & CAMEL_URL_NEED_PATH_DIR) == 0)
			action = GTK_FILE_CHOOSER_ACTION_OPEN;

		if (action != gtk_file_chooser_get_action (GTK_FILE_CHOOSER (service->pathentry)))
			gtk_file_chooser_set_action (GTK_FILE_CHOOSER (service->pathentry), action);

		if (url->path)
			gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (service->pathentry), url->path);
	}

	tmp = camel_url_get_param (url, "use_ssl");
	if (tmp == NULL)
		tmp = "never";

	for (i=0;i<num_ssl_options;i++) {
		if (!strcmp (ssl_options[i].value, tmp)) {
			gtk_combo_box_set_active (service->use_ssl, i);
			break;
		}
	}

	g_signal_connect (service->hostname, "changed", G_CALLBACK (emae_hostname_changed), service);
	g_signal_connect (service->username, "changed", G_CALLBACK (emae_username_changed), service);
	if (service->pathentry)
		g_signal_connect (GTK_FILE_CHOOSER (service->pathentry), "selection-changed", G_CALLBACK (emae_path_changed), service);

	g_signal_connect (service->use_ssl, "changed", G_CALLBACK(emae_ssl_changed), service);

	service->auth_frame = glade_xml_get_widget (xml, info->auth_frame);
	service->remember = emae_account_toggle (emae, info->remember_password, info->save_passwd_key, xml);
	service->check_supported = (GtkButton *)glade_xml_get_widget (xml, info->authtype_check);
	service->authtype = (GtkComboBox *)glade_xml_get_widget (xml, info->authtype);
	/* old authtype will be destroyed when we exit */
	service->auth_changed_id = 0;
	service->providers = (GtkComboBox *)glade_xml_get_widget (xml, info->type_dropdown);
	emae_refresh_providers (emae, service);
	emae_refresh_authtype (emae, service);

	if (info->needs_auth) {
		service->needs_auth = (GtkToggleButton *)glade_xml_get_widget (xml, info->needs_auth);
		gtk_toggle_button_set_active (service->needs_auth, url->authmech != NULL);
		g_signal_connect (service->needs_auth, "toggled", G_CALLBACK(emae_needs_auth), service);
		emae_needs_auth (service->needs_auth, service);
	} else {
		service->needs_auth = NULL;
	}

	if (!e_account_writable (emae->account, info->account_uri_key))
		gtk_widget_set_sensitive (service->container, FALSE);
	else
		gtk_widget_set_sensitive (service->container, TRUE);

	emae_service_provider_changed (service);

	camel_url_free (url);
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

/* its a bit obtuse, but its simple */
static void
emae_queue_widgets (EMAccountEditor *emae, GladeXML *xml, const gchar *first, ...)
{
	gint i = 0;
	va_list ap;

	va_start (ap, first);
	while (first) {
		emae->priv->widgets_name[i] = first;
		emae->priv->widgets[i++] = glade_xml_get_widget (xml, first);
		first = va_arg (ap, const gchar *);
	}
	va_end (ap);

	g_return_if_fail (i < sizeof (emae->priv->widgets)/sizeof (emae->priv->widgets[0]));

	emae->priv->widgets[i] = NULL;
	emae->priv->widgets_index = 0;
}

static GtkWidget *
emae_identity_page (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *gui = emae->priv;
	EAccount *account = emae->account;
	gint i;
	GtkWidget *w;
	GladeXML *xml;
	gchar *gladefile;

	/*if (old)
	  return old;*/

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "mail-config.glade",
				      NULL);
	xml = glade_xml_new (gladefile, item->label, GETTEXT_PACKAGE);
	g_free (gladefile);

	/* Management & Identity fields, in the druid the management frame is relocated to the last page later on */
	for (i=0;i<sizeof (emae_identity_entries)/sizeof (emae_identity_entries[0]);i++)
		gui->identity_entries[i] = emae_account_entry (emae, emae_identity_entries[i].name, emae_identity_entries[i].item, xml);

	gui->management_frame = glade_xml_get_widget (xml, "management_frame");

	gui->default_account = GTK_TOGGLE_BUTTON (glade_xml_get_widget (xml, "management_default"));
	if (!mail_config_get_default_account ()
		|| (account == mail_config_get_default_account ())
		|| (GPOINTER_TO_INT(g_object_get_data (G_OBJECT (emae->account), "default_flagged"))) )
			gtk_toggle_button_set_active (gui->default_account, TRUE);

	if (emae->do_signature) {
		emae_setup_signatures (emae, xml);
	} else {
		/* TODO: this could/should probably be neater */
		gtk_widget_hide (glade_xml_get_widget (xml, "sigLabel"));
#if 0
		gtk_widget_hide (glade_xml_get_widget (xml, "sigOption"));
#endif
		gtk_widget_hide (glade_xml_get_widget (xml, "sigAddNew"));
	}

	w = glade_xml_get_widget (xml, item->label);
	if (emae->type == EMAE_PAGES) {
		gtk_box_pack_start ((GtkBox *)emae->pages[0], w, TRUE, TRUE, 0);
	} else if (((EConfig *)gui->config)->type == E_CONFIG_DRUID) {
		GladeXML *druidxml;
		GtkWidget *page;

		gladefile = g_build_filename (EVOLUTION_GLADEDIR,
					      "mail-config.glade",
					      NULL);
		druidxml = glade_xml_new (gladefile, "identity_page", GETTEXT_PACKAGE);
		g_free (gladefile);

		page = glade_xml_get_widget (druidxml, "identity_page");

		gtk_box_pack_start ((GtkBox*)((GnomeDruidPageStandard *)page)->vbox, w, TRUE, TRUE, 0);
		w = page;
		g_object_unref (druidxml);
		gnome_druid_append_page ((GnomeDruid *)parent, (GnomeDruidPage *)page);
	} else {
		gtk_notebook_append_page ((GtkNotebook *)parent, w, gtk_label_new (_("Identity")));
	}

	emae_queue_widgets (emae, xml, "account_vbox", "identity_required_table", "identity_optional_table", NULL);

	g_object_unref (xml);

	return w;
}

static GtkWidget *
emae_receive_page (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *gui = emae->priv;
	GtkWidget *w;
	GladeXML *xml;
	gchar *gladefile;

	/*if (old)
	  return old;*/

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "mail-config.glade",
				      NULL);
	xml = glade_xml_new (gladefile, item->label, GETTEXT_PACKAGE);
	g_free (gladefile);

	gui->source.type = CAMEL_PROVIDER_STORE;
	emae_setup_service (emae, &gui->source, xml);

	w = glade_xml_get_widget (xml, item->label);
	if (emae->type == EMAE_PAGES) {
		gtk_box_pack_start ((GtkBox *)emae->pages[1], w, TRUE, TRUE, 0);
	} else if (((EConfig *)gui->config)->type == E_CONFIG_DRUID) {
		GladeXML *druidxml;
		GtkWidget *page;

		gladefile = g_build_filename (EVOLUTION_GLADEDIR,
					      "mail-config.glade",
					      NULL);
		druidxml = glade_xml_new (gladefile, "source_page", GETTEXT_PACKAGE);
		g_free (gladefile);

		page = glade_xml_get_widget (druidxml, "source_page");

		gtk_box_pack_start ((GtkBox*)((GnomeDruidPageStandard *)page)->vbox, w, TRUE, TRUE, 0);
		w = page;
		g_object_unref (druidxml);
		gnome_druid_append_page ((GnomeDruid *)parent, (GnomeDruidPage *)page);
	} else {
		gtk_notebook_append_page ((GtkNotebook *)parent, w, gtk_label_new (_("Receiving Email")));
	}

	emae_queue_widgets (emae, xml, "source_type_table", "table4", "vbox181", "vbox179", NULL);

	g_object_unref (xml);

	return w;
}

static void
emae_option_toggle_changed (GtkToggleButton *toggle, EMAccountEditorService *service)
{
	const gchar *name = g_object_get_data ((GObject *)toggle, "option-name");
	GSList *depl = g_object_get_data ((GObject *)toggle, "dependent-list");
	gint active = gtk_toggle_button_get_active (toggle);
	CamelURL *url = emae_account_url (service->emae, emae_service_info[service->type].account_uri_key);

	for (;depl;depl = g_slist_next (depl))
		gtk_widget_set_sensitive ((GtkWidget *)depl->data, active);

	camel_url_set_param (url, name, active?"":NULL);
	emae_uri_changed (service, url);
	camel_url_free (url);
}

static GtkWidget *
emae_option_toggle (EMAccountEditorService *service, CamelURL *url, const gchar *text, const gchar *name, gint def)
{
	GtkWidget *w;

	/* FIXME: how do we get the default value ever? */
	w = gtk_check_button_new_with_mnemonic (text);
	g_object_set_data ((GObject *)w, "option-name", (gpointer)name);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), camel_url_get_param (url, name) != NULL);
	g_signal_connect (w, "toggled", G_CALLBACK(emae_option_toggle_changed), service);
	gtk_widget_show (w);

	return w;
}

static void
emae_option_entry_changed (GtkEntry *entry, EMAccountEditorService *service)
{
	const gchar *name = g_object_get_data ((GObject *)entry, "option-name");
	const gchar *text = gtk_entry_get_text (entry);
	CamelURL *url = emae_account_url (service->emae, emae_service_info[service->type].account_uri_key);

	camel_url_set_param (url, name, text && text[0]?text:NULL);
	emae_uri_changed (service, url);
	camel_url_free (url);
}

static GtkWidget *
emae_option_entry (EMAccountEditorService *service, CamelURL *url, const gchar *name, const gchar *def, GtkWidget *l)
{
	GtkWidget *w;
	const gchar *val = camel_url_get_param (url, name);

	if (val == NULL) {
		if (def) {
			val = def;
			camel_url_set_param (url, name, val);
			emae_uri_changed (service, url);
		} else
			val = "";
	}

	w = g_object_new (gtk_entry_get_type (),
			 "text", val,
			 NULL);
	gtk_label_set_mnemonic_widget ((GtkLabel*)l, w);
	g_object_set_data ((GObject *)w, "option-name", (gpointer)name);
	g_signal_connect (w, "changed", G_CALLBACK(emae_option_entry_changed), service);
	gtk_widget_show (w);

	return w;
}

static void
emae_option_checkspin_changed (GtkSpinButton *spin, EMAccountEditorService *service)
{
	const gchar *name = g_object_get_data ((GObject *)spin, "option-name");
	gchar value[16];
	CamelURL *url = emae_account_url (service->emae, emae_service_info[service->type].account_uri_key);

	sprintf (value, "%d", gtk_spin_button_get_value_as_int (spin));
	camel_url_set_param (url, name, value);
	emae_uri_changed (service, url);
	camel_url_free (url);
}

static void
emae_option_checkspin_check_changed (GtkToggleButton *toggle, EMAccountEditorService *service)
{
	const gchar *name = g_object_get_data ((GObject *)toggle, "option-name");
	GtkSpinButton *spin = g_object_get_data ((GObject *)toggle, "option-target");

	if (gtk_toggle_button_get_active (toggle)) {
		gtk_widget_set_sensitive ((GtkWidget *)spin, TRUE);
		emae_option_checkspin_changed (spin, service);
	} else {
		CamelURL *url = emae_account_url (service->emae, emae_service_info[service->type].account_uri_key);

		camel_url_set_param (url, name, NULL);
		gtk_widget_set_sensitive ((GtkWidget *)spin, FALSE);
		emae_uri_changed (service, url);
		camel_url_free (url);
	}
}

/* this is a fugly api */
static GtkWidget *
emae_option_checkspin (EMAccountEditorService *service, CamelURL *url, const gchar *name, const gchar *fmt, const gchar *info)
{
	GtkWidget *hbox, *check, *spin, *label = NULL;
	double min, def, max;
	gchar *pre, *post;
	const gchar *val;
	gchar on;
	gint enable;

	pre = g_alloca (strlen (fmt)+1);
	strcpy (pre, fmt);
	post = strstr (pre, "%s");
	if (post) {
		*post = 0;
		post+=2;
	}

	if (sscanf (info, "%c:%lf:%lf:%lf", &on, &min, &def, &max) != 4) {
		min = 0.0;
		def = 0.0;
		max = 1.0;
	}

	if ((enable = (val = camel_url_get_param (url, name)) != NULL) )
		def = strtod (val, NULL);
	else
		enable = (on == 'y');

	hbox = gtk_hbox_new (FALSE, 0);
	check = g_object_new (gtk_check_button_get_type (), "label", pre, "use_underline", TRUE, "active", enable, NULL);

	spin = gtk_spin_button_new ((GtkAdjustment *)gtk_adjustment_new (def, min, max, 1, 1, 0), 1, 0);
	if (post)
		label = gtk_label_new_with_mnemonic (post);
	gtk_box_pack_start ((GtkBox *)hbox, check, FALSE, TRUE, 0);
	gtk_box_pack_start ((GtkBox *)hbox, spin, FALSE, TRUE, 0);
	if (label)
		gtk_box_pack_start ((GtkBox *)hbox, label, FALSE, TRUE, 4);

	g_object_set_data ((GObject *)spin, "option-name", (gpointer)name);
	g_object_set_data ((GObject *)check, "option-name", (gpointer)name);
	g_object_set_data ((GObject *)check, "option-target", (gpointer)spin);

	g_signal_connect (spin, "value_changed", G_CALLBACK(emae_option_checkspin_changed), service);
	g_signal_connect (check, "toggled", G_CALLBACK(emae_option_checkspin_check_changed), service);

	gtk_widget_show_all (hbox);

	return hbox;
}

static void
emae_option_options_changed (GtkComboBox *options, EMAccountEditorService *service)
{
	const gchar *name = g_object_get_data (G_OBJECT (options), "option-name");
	gchar *value = NULL;
	CamelURL *url = emae_account_url (service->emae, emae_service_info[service->type].account_uri_key);
	gint id = gtk_combo_box_get_active (options);

	if (id != -1) {
		GtkTreeModel *model;
		GtkTreeIter iter;

		model = gtk_combo_box_get_model (options);
		if (gtk_tree_model_iter_nth_child (model, &iter, NULL, id)) {
			gtk_tree_model_get (model, &iter, 0, &value, -1);
		}
	}

	camel_url_set_param (url, name, value);
	emae_uri_changed (service, url);
	camel_url_free (url);
	g_free (value);
}

/* 'values' is in format "value0:caption0:value2:caption2:...valueN:captionN" */
static GtkWidget *
emae_option_options (EMAccountEditorService *service, CamelURL *url, const gchar *name, const gchar *values, GtkWidget *l)
{
	GtkComboBox *w;
	GtkListStore *store;
	GtkTreeIter iter;
	const gchar *p, *value, *caption;
	GtkCellRenderer *cell;
	gint active = 0; /* the first item entered is always a default item */
	gint i;
	const gchar *val = camel_url_get_param (url, name);

	w = GTK_COMBO_BOX (gtk_combo_box_new ());

	/* value and caption */
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	p = values;
	for (p = values, i = 0; p; i++) {
		gchar *vl, *cp;

		value = p;
		caption = strchr (p, ':');
		if (caption) {
			caption++;
		} else {
			g_warning (G_STRLOC ": expected ':' not found at '%s'", p);
			break;
		}
		p = strchr (caption, ':');

		vl = g_strndup (value, caption - value - 1);
		if (p) {
			p++;
			cp = g_strndup (caption, p - caption - 1);
		} else
			cp = g_strdup (caption);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, vl, 1, dgettext (service->provider->translation_domain, cp), -1);

		if (val && g_ascii_strcasecmp (val, vl) == 0)
			active = i;

		g_free (vl);
		g_free (cp);
	}

	gtk_combo_box_set_model (w, (GtkTreeModel *)store);
	gtk_combo_box_set_active (w, i > 0 ? active : -1);

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (w), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (w), cell, "text", 1, NULL);

	gtk_widget_show (GTK_WIDGET (w));

	gtk_label_set_mnemonic_widget (GTK_LABEL (l), GTK_WIDGET (w));

	g_object_set_data (G_OBJECT (w), "option-name", (gpointer)name);
	g_signal_connect (w, "changed", G_CALLBACK (emae_option_options_changed), service);

	return GTK_WIDGET (w);
}

static GtkWidget *
emae_receive_options_item (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	EMAccountEditor *emae = data;
	GtkWidget *w, *box, *spin;
	gint row;

	if (emae->priv->source.provider == NULL
	    || emae->priv->source.provider->extra_conf == NULL)
		return NULL;

	if (old) {
		if (emae->type == EMAE_PAGES) {
			GtkWidget *box = gtk_hbox_new (FALSE, 12);
			gtk_widget_reparent (old, box);
			gtk_widget_show(box);
			gtk_box_set_child_packing ((GtkBox *)box, old, TRUE, TRUE, 12, GTK_PACK_START);			
			gtk_box_pack_end ((GtkBox *)emae->pages[2], box, FALSE, FALSE, 0);
		}
		return old;
	}

	if (emae->type == EMAE_PAGES)  {
		GtkWidget *box = gtk_hbox_new (FALSE, 12);
		gtk_widget_reparent (parent, box);
		gtk_widget_show(box);
		gtk_box_set_child_packing ((GtkBox *)box, parent, TRUE, TRUE, 12, GTK_PACK_START);
		gtk_box_pack_start ((GtkBox *)emae->pages[2], box, FALSE, FALSE, 0);
	}

	/* We have to add the automatic mail check item with the rest of the receive options */
	row = ((GtkTable *)parent)->nrows;

	box = gtk_hbox_new (FALSE, 4);
	w = gtk_check_button_new_with_mnemonic (_("Check for _new messages every"));
	emae_account_toggle_widget (emae, (GtkToggleButton *)w, E_ACCOUNT_SOURCE_AUTO_CHECK);
	gtk_box_pack_start ((GtkBox *)box, w, FALSE, FALSE, 0);

	spin = gtk_spin_button_new_with_range (1.0, 1440.0, 1.0);
	emae_account_spinint_widget (emae, (GtkSpinButton *)spin, E_ACCOUNT_SOURCE_AUTO_CHECK_TIME);
	gtk_box_pack_start ((GtkBox *)box, spin, FALSE, TRUE, 0);

	w = gtk_label_new_with_mnemonic (_("minu_tes"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (w), spin);
	gtk_box_pack_start ((GtkBox *)box, w, FALSE, FALSE, 0);

	gtk_widget_show_all (box);

	gtk_table_attach ((GtkTable *)parent, box, 0, 2, row, row+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);

	return box;
}

static GtkWidget *
emae_receive_options_extra_item (EConfig *ec, EConfigItem *eitem, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	EMAccountEditor *emae = data;
	struct _receive_options_item *item = (struct _receive_options_item *)eitem;
	GtkWidget *w, *l, *h;
	CamelProviderConfEntry *entries;
	GtkWidget *depw;
	GSList *depl = NULL, *n;
	EMAccountEditorService *service = &emae->priv->source;
	gint row, i;
	GHashTable *extra;
	CamelURL *url;

	if (emae->priv->source.provider == NULL
	    || emae->priv->source.provider->extra_conf == NULL)
		return NULL;
	if (emae->type == EMAE_PAGES) {
		GtkWidget *box = gtk_hbox_new (FALSE, 12);
		gtk_widget_reparent (parent, box);
		gtk_widget_show(box);
		gtk_box_set_child_packing ((GtkBox *)box, parent, TRUE, TRUE, 12, GTK_PACK_START);
		gtk_box_pack_start ((GtkBox *)emae->pages[2], box, FALSE, FALSE, 0);
	}

	entries = emae->priv->source.provider->extra_conf;
	for (i=0;entries && entries[i].type != CAMEL_PROVIDER_CONF_END;i++)
		if (entries[i].type == CAMEL_PROVIDER_CONF_SECTION_START
		    && entries[i].name
		    && strcmp (entries[i].name, eitem->user_data) == 0)
			goto section;

	return NULL;
section:
	d (printf ("Building extra section '%s'\n", eitem->path));
	url = emae_account_url (emae, emae_service_info[service->type].account_uri_key);
	item->extra_table = g_hash_table_new (g_str_hash, g_str_equal);
	extra = g_hash_table_new (g_str_hash, g_str_equal);
	row = ((GtkTable *)parent)->nrows;

	for (;entries[i].type != CAMEL_PROVIDER_CONF_END && entries[i].type != CAMEL_PROVIDER_CONF_SECTION_END;i++) {
		if (entries[i].depname) {
			depw = g_hash_table_lookup (extra, entries[i].depname);
			if (depw)
				depl = g_object_steal_data ((GObject *)depw, "dependent-list");
		} else
			depw = NULL;

		switch (entries[i].type) {
		case CAMEL_PROVIDER_CONF_SECTION_START:
		case CAMEL_PROVIDER_CONF_SECTION_END:
			break;
		case CAMEL_PROVIDER_CONF_LABEL:
			/* FIXME: This is a hack for exchange connector, labels should be removed from confentry */
			if (!strcmp (entries[i].name, "hostname"))
				l = (GtkWidget *)emae->priv->source.hostlabel;
			else if (!strcmp (entries[i].name, "username"))
				l = (GtkWidget *)emae->priv->source.userlabel;
			else
				l = NULL;

			if (l) {
				gtk_label_set_text_with_mnemonic ((GtkLabel *)l, entries[i].text);
				if (depw)
					depl = g_slist_prepend (depl, l);
			}
			break;
		case CAMEL_PROVIDER_CONF_CHECKBOX:
			w = emae_option_toggle (service, url, entries[i].text, entries[i].name, atoi (entries[i].value));
			gtk_table_attach ((GtkTable *)parent, w, 0, 2, row, row+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
			g_hash_table_insert (extra, (gpointer)entries[i].name, w);
			if (depw)
				depl = g_slist_prepend (depl, w);
			row++;
			/* HACK: keep_on_server is stored in the e-account, but is displayed as a properly on the uri,
			   make sure they track/match here */
			if (!strcmp (entries[i].name, "keep_on_server"))
				emae_account_toggle_widget (emae, (GtkToggleButton *)w, E_ACCOUNT_SOURCE_KEEP_ON_SERVER);
			break;
		case CAMEL_PROVIDER_CONF_ENTRY:
			l = g_object_new (gtk_label_get_type (), "label", entries[i].text, "xalign", 0.0, "use_underline", TRUE, NULL);
			gtk_widget_show (l);
			w = emae_option_entry (service, url, entries[i].name, entries[i].value, l);
			gtk_table_attach ((GtkTable *)parent, l, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);
			gtk_table_attach ((GtkTable *)parent, w, 1, 2, row, row+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
			if (depw) {
				depl = g_slist_prepend (depl, w);
				depl = g_slist_prepend (depl, l);
			}
			row++;
			/* FIXME: this is another hack for exchange/groupwise connector */
			g_hash_table_insert (item->extra_table, (gpointer)entries[i].name, w);
			break;
		case CAMEL_PROVIDER_CONF_CHECKSPIN:
			w = emae_option_checkspin (service, url, entries[i].name, entries[i].text, entries[i].value);
			gtk_table_attach ((GtkTable *)parent, w, 0, 2, row, row+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
			if (depw)
				depl = g_slist_prepend (depl, w);
			row++;
			break;
		case CAMEL_PROVIDER_CONF_OPTIONS:
			h = gtk_hbox_new (FALSE, 4);
			gtk_widget_show (h);
			l = g_object_new (gtk_label_get_type (), "label", entries[i].text, "xalign", 0.0, "use_underline", TRUE, NULL);
			gtk_widget_show (l);
			w = emae_option_options (service, url, entries[i].name, entries[i].value, l);
			gtk_box_pack_start (GTK_BOX (h), l, FALSE, FALSE, 0);
			gtk_box_pack_start (GTK_BOX (h), w, FALSE, FALSE, 0);
			gtk_table_attach ((GtkTable *)parent, h, 0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
			if (depw) {
				depl = g_slist_prepend (depl, h);
			}
			row++;
			break;
		default:
			break;
		}

		if (depw && depl) {
			gint act = gtk_toggle_button_get_active ((GtkToggleButton *)depw);

			g_object_set_data_full ((GObject *)depw, "dependent-list", depl, (GDestroyNotify)g_slist_free);
			for (n=depl;n;n=g_slist_next (n))
				gtk_widget_set_sensitive ((GtkWidget *)n->data, act);
		}
	}

	camel_url_free (url);

	/* Since EConfig destroys the factory widget when it changes, we
	 * need to destroy our own ones as well, and add a dummy item
	 * so it knows this section isn't empty */

	w = gtk_label_new ("");
	gtk_widget_hide (w);
	gtk_table_attach ((GtkTable *)parent, w, 0, 2, row, row+1, 0, 0, 0, 0);

	return w;
}

static GtkWidget *
emae_send_page (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *gui = emae->priv;
	GtkWidget *w;
	GladeXML *xml;
	gchar *gladefile;

	/* no transport options page at all for these types of providers */
	if (gui->source.provider && CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT(gui->source.provider)) {
		memset (&gui->transport.frame, 0, ((gchar *)&gui->transport.check_dialog)-((gchar *)&gui->transport.frame));
		return NULL;
	}

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "mail-config.glade",
				      NULL);
	xml = glade_xml_new (gladefile, item->label, GETTEXT_PACKAGE);
	g_free (gladefile);

	/* Transport */
	gui->transport.type = CAMEL_PROVIDER_TRANSPORT;
	emae_setup_service (emae, &gui->transport, xml);

	w = glade_xml_get_widget (xml, item->label);
	if (emae->type == EMAE_PAGES) {
		gtk_box_pack_start ((GtkBox *)emae->pages[3], w, TRUE, TRUE, 0);
	} else if (((EConfig *)gui->config)->type == E_CONFIG_DRUID) {
		GladeXML *druidxml;
		GtkWidget *page;

		gladefile = g_build_filename (EVOLUTION_GLADEDIR,
					      "mail-config.glade",
					      NULL);
		druidxml = glade_xml_new (gladefile, "transport_page", GETTEXT_PACKAGE);
		g_free (gladefile);

		page = glade_xml_get_widget (druidxml, "transport_page");

		gtk_box_pack_start ((GtkBox*)((GnomeDruidPageStandard *)page)->vbox, w, TRUE, TRUE, 0);
		w = page;
		g_object_unref (druidxml);
		gnome_druid_append_page ((GnomeDruid *)parent, (GnomeDruidPage *)page);
	} else {
		gtk_notebook_append_page ((GtkNotebook *)parent, w, gtk_label_new (_("Sending Email")));
	}

	emae_queue_widgets (emae, xml, "transport_type_table", "vbox12", "vbox183", "vbox61", NULL);

	g_object_unref (xml);

	return w;
}

static GtkWidget *
emae_defaults_page (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *gui = emae->priv;
	GtkWidget *w;
	GladeXML *xml;
	gchar *gladefile;

	/*if (old)
	  return old;*/

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "mail-config.glade",
				      NULL);
	xml = glade_xml_new (gladefile, item->label, GETTEXT_PACKAGE);
	g_free (gladefile);

	/* Special folders */
	gui->drafts_folder_button = (GtkButton *)emae_account_folder (emae, "drafts_button", E_ACCOUNT_DRAFTS_FOLDER_URI, MAIL_COMPONENT_FOLDER_DRAFTS, xml);
	gui->sent_folder_button = (GtkButton *)emae_account_folder (emae, "sent_button", E_ACCOUNT_SENT_FOLDER_URI, MAIL_COMPONENT_FOLDER_SENT, xml);

	/* Special Folders "Reset Defaults" button */
	gui->restore_folders_button = (GtkButton *)glade_xml_get_widget (xml, "default_folders_button");
	g_signal_connect (gui->restore_folders_button, "clicked", G_CALLBACK (default_folders_clicked), emae);

	/* Always Cc/Bcc */
	emae_account_toggle (emae, "always_cc", E_ACCOUNT_CC_ALWAYS, xml);
	emae_account_entry (emae, "cc_addrs", E_ACCOUNT_CC_ADDRS, xml);
	emae_account_toggle (emae, "always_bcc", E_ACCOUNT_BCC_ALWAYS, xml);
	emae_account_entry (emae, "bcc_addrs", E_ACCOUNT_BCC_ADDRS, xml);

	gtk_widget_set_sensitive ((GtkWidget *)gui->drafts_folder_button, e_account_writable (emae->account, E_ACCOUNT_DRAFTS_FOLDER_URI));

	gtk_widget_set_sensitive ( (GtkWidget *)gui->sent_folder_button,
				  e_account_writable (emae->account, E_ACCOUNT_SENT_FOLDER_URI)
				  &&
				  (emae->priv->source.provider ? !(emae->priv->source.provider->flags & CAMEL_PROVIDER_DISABLE_SENT_FOLDER): TRUE)
				);

	gtk_widget_set_sensitive ((GtkWidget *)gui->restore_folders_button,
				 (e_account_writable (emae->account, E_ACCOUNT_SENT_FOLDER_URI)
				  && ((emae->priv->source.provider  && !( emae->priv->source.provider->flags & CAMEL_PROVIDER_DISABLE_SENT_FOLDER))
				      || e_account_writable (emae->account, E_ACCOUNT_DRAFTS_FOLDER_URI))));

	/* Receipt policy */
	emae_setup_receipt_policy (emae, xml);

	w = glade_xml_get_widget (xml, item->label);
	gtk_notebook_append_page ((GtkNotebook *)parent, w, gtk_label_new (_("Defaults")));

	emae_queue_widgets (emae, xml, "vbox184", "table8", NULL);

	g_object_unref (xml);

	return w;
}

static GtkWidget *
emae_security_page (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	EMAccountEditor *emae = data;
#if defined (HAVE_NSS)
	EMAccountEditorPrivate *gui = emae->priv;
#endif
	GtkWidget *w;
	GladeXML *xml;
	gchar *gladefile;

	/*if (old)
	  return old;*/

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "mail-config.glade",
				      NULL);
	xml = glade_xml_new (gladefile, item->label, GETTEXT_PACKAGE);
	g_free (gladefile);

	/* Security */
	emae_account_entry (emae, "pgp_key", E_ACCOUNT_PGP_KEY, xml);
	emae_account_toggle (emae, "pgp_encrypt_to_self", E_ACCOUNT_PGP_ENCRYPT_TO_SELF, xml);
	emae_account_toggle (emae, "pgp_always_sign", E_ACCOUNT_PGP_ALWAYS_SIGN, xml);
	emae_account_toggle (emae, "pgp_no_imip_sign", E_ACCOUNT_PGP_NO_IMIP_SIGN, xml);
	emae_account_toggle (emae, "pgp_always_trust", E_ACCOUNT_PGP_ALWAYS_TRUST, xml);

#if defined (HAVE_NSS)
	/* TODO: this should handle its entry separately? */
	gui->smime_sign_key = emae_account_entry (emae, "smime_sign_key", E_ACCOUNT_SMIME_SIGN_KEY, xml);
	gui->smime_sign_key_select = (GtkButton *)glade_xml_get_widget (xml, "smime_sign_key_select");
	gui->smime_sign_key_clear = (GtkButton *)glade_xml_get_widget (xml, "smime_sign_key_clear");
	g_signal_connect (gui->smime_sign_key_select, "clicked", G_CALLBACK(smime_sign_key_select), emae);
	g_signal_connect (gui->smime_sign_key_clear, "clicked", G_CALLBACK(smime_sign_key_clear), emae);

	gui->smime_sign_default = emae_account_toggle (emae, "smime_sign_default", E_ACCOUNT_SMIME_SIGN_DEFAULT, xml);

	gui->smime_encrypt_key = emae_account_entry (emae, "smime_encrypt_key", E_ACCOUNT_SMIME_ENCRYPT_KEY, xml);
	gui->smime_encrypt_key_select = (GtkButton *)glade_xml_get_widget (xml, "smime_encrypt_key_select");
	gui->smime_encrypt_key_clear = (GtkButton *)glade_xml_get_widget (xml, "smime_encrypt_key_clear");
	g_signal_connect (gui->smime_encrypt_key_select, "clicked", G_CALLBACK(smime_encrypt_key_select), emae);
	g_signal_connect (gui->smime_encrypt_key_clear, "clicked", G_CALLBACK(smime_encrypt_key_clear), emae);

	gui->smime_encrypt_default = emae_account_toggle (emae, "smime_encrypt_default", E_ACCOUNT_SMIME_ENCRYPT_DEFAULT, xml);
	gui->smime_encrypt_to_self = emae_account_toggle (emae, "smime_encrypt_to_self", E_ACCOUNT_SMIME_ENCRYPT_TO_SELF, xml);
	smime_changed (emae);
#else
	{
		/* Since we don't have NSS, hide the S/MIME config options */
		GtkWidget *frame;

		frame = glade_xml_get_widget (xml, "smime_vbox");
		gtk_widget_destroy (frame);
	}
#endif /* HAVE_NSS */

	w = glade_xml_get_widget (xml, item->label);
	gtk_notebook_append_page ((GtkNotebook *)parent, w, gtk_label_new (_("Security")));

	g_object_unref (xml);

	return w;
}

static GtkWidget *
emae_widget_glade (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	EMAccountEditor *emae = data;
	gint i;

	for (i=0;emae->priv->widgets[i];i++)
		if (!strcmp (emae->priv->widgets_name[i], item->label))
			return emae->priv->widgets[i];

	g_warning ("Mail account widget '%s' not found", item->label);

	return NULL;
}

/* plugin meta-data for "org.gnome.evolution.mail.config.accountEditor" */
static EMConfigItem emae_editor_items[] = {
	{ E_CONFIG_BOOK, (gchar *) "" },
	{ E_CONFIG_PAGE, (gchar *) "00.identity", (gchar *) "vboxIdentityBorder", emae_identity_page },
	{ E_CONFIG_SECTION, (gchar *) "00.identity/00.name", (gchar *) "account_vbox", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "00.identity/10.required", (gchar *) "identity_required_table", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "00.identity/20.info", (gchar *) "identity_optional_table", emae_widget_glade },

	{ E_CONFIG_PAGE, (gchar *) "10.receive", (gchar *) "vboxSourceBorder", emae_receive_page },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "10.receive/00.type", (gchar *) "source_type_table", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "10.receive/10.config", (gchar *) "table4", emae_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "10.receive/20.security", (gchar *) "vbox181", emae_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "10.receive/30.auth", (gchar *) "vbox179", emae_widget_glade },

	/* Most sections for this is auto-generated from the camel config */
	{ E_CONFIG_PAGE, (gchar *) "20.receive_options", (gchar *) N_("Receiving Options"), },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "20.receive_options/10.mailcheck", (gchar *) N_("Checking for New Messages"), },
	{ E_CONFIG_ITEM_TABLE, (gchar *) "20.receive_options/10.mailcheck/00.autocheck", NULL, emae_receive_options_item, },

	{ E_CONFIG_PAGE, (gchar *) "30.send", (gchar *) "vboxTransportBorder", emae_send_page },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "30.send/00.type", (gchar *) "transport_type_table", emae_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "30.send/10.config", (gchar *) "vbox12", emae_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "30.send/20.security", (gchar *) "vbox183", emae_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "30.send/30.auth", (gchar *) "vbox61", emae_widget_glade },

	{ E_CONFIG_PAGE, (gchar *) "40.defaults", (gchar *) "vboxFoldersBorder", emae_defaults_page },
	{ E_CONFIG_SECTION, (gchar *) "40.defaults/00.folders", (gchar *) "vbox184", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "40.defaults/10.composing", (gchar *) "table8", emae_widget_glade },

	{ E_CONFIG_PAGE, (gchar *) "50.security", (gchar *) "vboxSecurityBorder", emae_security_page },
	/* 1x1 table (!) not vbox: { E_CONFIG_SECTION, "50.security/00.gpg", "table19", emae_widget_glade }, */
	/* table not vbox: { E_CONFIG_SECTION, "50.security/10.smime", "smime_table", emae_widget_glade }, */
	{ 0 },
};
static gboolean emae_editor_items_translated = FALSE;

static GtkWidget *
emae_management_page (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *gui = emae->priv;
	GtkWidget *w;

	w = gui->management_frame;
	if (((EConfig *)gui->config)->type == E_CONFIG_DRUID) {
		GladeXML *druidxml;
		GtkWidget *page;
		gchar *gladefile;

		gladefile = g_build_filename (EVOLUTION_GLADEDIR,
					      "mail-config.glade",
					      NULL);
		druidxml = glade_xml_new (gladefile, "management_page", GETTEXT_PACKAGE);
		g_free (gladefile);

		page = glade_xml_get_widget (druidxml, "management_page");

		gtk_widget_reparent (w, ((GnomeDruidPageStandard *)page)->vbox);
		w = page;
		g_object_unref (druidxml);
		gnome_druid_append_page ((GnomeDruid *)parent, (GnomeDruidPage *)page);
	}

	return w;
}

static GtkWidget *
emae_widget_druid_glade (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	GladeXML *druidxml;
	GtkWidget *w;
	gchar *gladefile;
	EMAccountEditor *emae = (EMAccountEditor *)data;

	if (emae->type == EMAE_PAGES)
		return NULL;

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "mail-config.glade",
				      NULL);
	druidxml = glade_xml_new (gladefile, item->label, GETTEXT_PACKAGE);
	g_free (gladefile);

	w = glade_xml_get_widget (druidxml, item->label);
	/* i think the glade file has issues, we need to show all on at least the end page */
	gtk_widget_show_all (w);
	g_object_unref (druidxml);

	gnome_druid_append_page ((GnomeDruid *)parent, (GnomeDruidPage *)w);

	return w;
}

/* plugin meta-data for "org.gnome.evolution.mail.config.accountDruid" */
static EMConfigItem emae_druid_items[] = {
	{ E_CONFIG_DRUID, (gchar *) "" },
	{ E_CONFIG_PAGE_START, (gchar *) "0.start", (gchar *) "start_page", emae_widget_druid_glade },

	{ E_CONFIG_PAGE, (gchar *) "00.identity", (gchar *) "vboxIdentityBorder", emae_identity_page },
	{ E_CONFIG_SECTION, (gchar *) "00.identity/00.name", (gchar *) "account_vbox", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "00.identity/10.required", (gchar *) "identity_required_table", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "00.identity/20.info", (gchar *) "identity_optional_table", emae_widget_glade },

	{ E_CONFIG_PAGE, (gchar *) "10.receive", (gchar *) "vboxSourceBorder", emae_receive_page },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "10.receive/00.type", (gchar *) "source_type_table", emae_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "10.receive/10.config", (gchar *) "table4", emae_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "10.receive/20.security", (gchar *) "vbox181", emae_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "10.receive/30.auth", (gchar *) "vbox179", emae_widget_glade },

	/* Most sections for this is auto-generated fromt the camel config */
	{ E_CONFIG_PAGE, (gchar *) "20.receive_options", (gchar *) N_("Receiving Options"), },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "20.receive_options/10.mailcheck", (gchar *) N_("Checking for New Messages"), },
	{ E_CONFIG_ITEM_TABLE, (gchar *) "20.receive_options/10.mailcheck/00.autocheck", NULL, emae_receive_options_item, },

	{ E_CONFIG_PAGE, (gchar *) "30.send", (gchar *) "vboxTransportBorder", emae_send_page },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "30.send/00.type", (gchar *) "transport_type_table", emae_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "30.send/10.config", (gchar *) "vbox12", emae_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "30.send/20.security", (gchar *) "vbox183", emae_widget_glade },
	{ E_CONFIG_SECTION, (gchar *) "30.send/30.auth", (gchar *) "vbox61", emae_widget_glade },

	{ E_CONFIG_PAGE, (gchar *) "40.management", (gchar *) "management_frame", emae_management_page },

	{ E_CONFIG_PAGE_FINISH, (gchar *) "999.end", (gchar *) "finish_page", emae_widget_druid_glade },
	{ 0 },
};
static gboolean emae_druid_items_translated = FALSE;

static void
emae_free (EConfig *ec, GSList *items, gpointer data)
{
	g_slist_free (items);
}

static void
emae_free_auto (EConfig *ec, GSList *items, gpointer data)
{
	GSList *l, *n;

	for (l=items;l;) {
		struct _receive_options_item *item = l->data;

		n = g_slist_next (l);
		g_free (item->item.path);
		if (item->extra_table)
			g_hash_table_destroy (item->extra_table);
		g_free (item);
		g_slist_free_1(l);
		l = n;
	}
}

static gboolean
emae_service_complete (EMAccountEditor *emae, EMAccountEditorService *service)
{
	CamelURL *url;
	gint ok = TRUE;
	const gchar *uri;

	if (service->provider == NULL)
		return TRUE;

	uri = e_account_get_string (emae->account, emae_service_info[service->type].account_uri_key);
	if (uri == NULL || (url = camel_url_new (uri, NULL)) == NULL)
		return FALSE;

	if (CAMEL_PROVIDER_NEEDS(service->provider, CAMEL_URL_PART_HOST)) {
		if (url->host == NULL || url->host[0] == 0)
			ok = FALSE;
	}
	/* We only need the user if the service needs auth as well, i think */
	if (ok
	    && (service->needs_auth == NULL
		|| CAMEL_PROVIDER_NEEDS(service->provider, CAMEL_URL_PART_AUTH)
		|| gtk_toggle_button_get_active (service->needs_auth))
	    && CAMEL_PROVIDER_NEEDS(service->provider, CAMEL_URL_PART_USER)
	    && (url->user == NULL || url->user[0] == 0))
		ok = FALSE;

	if (ok
	    && CAMEL_PROVIDER_NEEDS(service->provider, CAMEL_URL_PART_PATH)
	    && (url->path == NULL || url->path[0] == 0))
		ok = FALSE;

	camel_url_free (url);

	return ok;
}

enum {
	GMAIL = 0,
	YAHOO,
	AOL
};
struct _server_prefill {
	const gchar *key;
	const gchar *recv;
	const gchar *send;
	const gchar *proto;
	const gchar *ssl;
} mail_servers [] = {
	{"gmail", "imap.gmail.com", "smtp.gmail.com", "imap", "always"},
	{"googlemail", "imap.gmail.com", "smtp.gmail.com", "imap", "always"},
	{"yahoo", "pop3.yahoo.com", "smtp.yahoo.com", "pop", "never"},
	{"aol", "imap.aol.com", "smtp.aol.com", "pop", "never"},
	{"msn", "pop3.email.msn.com", "smtp.email.msn.com", "pop", "never"},
	{"hotmail", "pop3.live.com", "smtp.live.com", "pop", "always"},
	{"live.com", "pop3.live.com", "smtp.live.com", "pop", "always"},

};

static gint
check_servers (gchar *server)
{
	gint len = G_N_ELEMENTS(mail_servers), i;

	for (i=0; i<len; i++) {
		if (strstr (server, mail_servers[i].key) != NULL)
			return i;
	}

	return -1;
}

static gboolean
emae_check_complete (EConfig *ec, const gchar *pageid, gpointer data)
{
	EMAccountEditor *emae = data;
	gint ok = TRUE;
	const gchar *tmp;
	EAccount *ea;
	gboolean refresh = FALSE;
	gboolean edit = emae->original != NULL;

	/* We use the page-check of various pages to 'prepare' or
	   pre-load their values, only in the druid */
	if (pageid
	    && ((EConfig *)emae->priv->config)->type == E_CONFIG_DRUID) {
		if (!strcmp (pageid, "00.identity")) {
			if (!emae->priv->identity_set) {
				gchar *uname;

				emae->priv->identity_set = 1;
#ifndef G_OS_WIN32
				uname = g_locale_to_utf8(g_get_real_name (), -1, NULL, NULL, NULL);
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
				gchar *user, *at;
				gint index;
				gchar *uri = g_strdup (e_account_get_string (emae->account, E_ACCOUNT_SOURCE_URL));
				CamelURL *url;

				emae->priv->receive_set = 1;
				tmp = (gchar *)e_account_get_string (emae->account, E_ACCOUNT_ID_ADDRESS);
				at = strrchr (tmp, '@');
				user = g_alloca (at-tmp+1);
				memcpy (user, tmp, at-tmp);
				user[at-tmp] = 0;
				at++;

				index = check_servers (at);
				gtk_entry_set_text (emae->priv->source.username, user);
				gtk_entry_set_text (emae->priv->transport.username, user);
				if (!edit && uri && (url = camel_url_new (uri, NULL)) != NULL) {
					refresh = TRUE;
					camel_url_set_user (url, user);
					if (index != -1) {
						camel_url_set_protocol (url, mail_servers[index].proto);
						camel_url_set_param (url, "use_ssl", mail_servers[index].ssl);
						camel_url_set_host (url, mail_servers[index].recv);
						gtk_entry_set_text (emae->priv->source.hostname, mail_servers[index].recv);
						gtk_entry_set_text (emae->priv->transport.hostname, mail_servers[index].send);
						camel_url_set_host (url, mail_servers[index].recv);

					} else {
						camel_url_set_host (url, "");
					}
					camel_url_set_user (url, user);
					g_free (uri);
					uri = camel_url_to_string (url, 0);
					e_account_set_string (emae->account, E_ACCOUNT_SOURCE_URL, uri);
					g_free (uri);
					camel_url_free (url);
				} else {
					g_free (uri);
				}

			}
		} else if (!strcmp (pageid, "30.send")) {
			if (!emae->priv->send_set) {
				CamelURL *url;
				gchar *at, *user;
				gint index;
				gchar *uri = (gchar *)e_account_get_string (emae->account, E_ACCOUNT_TRANSPORT_URL);
				emae->priv->send_set = 1;
				tmp = e_account_get_string (emae->account, E_ACCOUNT_ID_ADDRESS);
				at = strrchr (tmp, '@');
				user = g_alloca (at-tmp+1);
				memcpy (user, tmp, at-tmp);
				user[at-tmp] = 0;
				at++;

				index = check_servers (at);
				if (index != -1 && uri  && (url = camel_url_new (uri, NULL)) != NULL) {
					refresh = TRUE;
					camel_url_set_protocol (url, "smtp");
					camel_url_set_param (url, "use_ssl", mail_servers[index].ssl);
					camel_url_set_host (url, mail_servers[index].send);
					camel_url_set_user (url, user);
					uri = camel_url_to_string (url, 0);
					e_account_set_string (emae->account, E_ACCOUNT_TRANSPORT_URL, uri);
					g_free (uri);
					camel_url_free (url);
				}

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
				tmp = e_account_get_string (emae->account, E_ACCOUNT_ID_ADDRESS);
				len = strlen (tmp);
				template = alloca (len + 14);
				strcpy (template, tmp);
				while (mail_config_get_account_by_name (template))
					sprintf (template + len, " (%d)", i++);

				gtk_entry_set_text (emae->priv->identity_entries[0], template);
			}
		}
	}

	/*
	   Setting a flag on the Account if it is marked as default. It is done in this way instead of
	   using a temporary variable so as to keep track of which account is marked as default in case of
	   editing multiple accounts at a time
	 */
	if (gtk_toggle_button_get_active (emae->priv->default_account))
		g_object_set_data (G_OBJECT (emae->account), "default_flagged", GINT_TO_POINTER(1));

	if (pageid == NULL || !strcmp (pageid, "00.identity")) {
		/* TODO: check the account name is set, and unique in the account list */
		ok = (tmp = e_account_get_string (emae->account, E_ACCOUNT_ID_NAME))
			&& tmp[0]
			&& (tmp = e_account_get_string (emae->account, E_ACCOUNT_ID_ADDRESS))
			&& is_email (tmp)
			&& ((tmp = e_account_get_string (emae->account, E_ACCOUNT_ID_REPLY_TO)) == NULL
			    || tmp[0] == 0
			    || is_email (tmp));
		if (!ok) {
			d (printf ("identity incomplete\n"));
		}
	}

	if (ok && (pageid == NULL || !strcmp (pageid, "10.receive"))) {
		if (emae->type == EMAE_PAGES && refresh) {
			emae_refresh_providers (emae, &emae->priv->source);
		}
		ok = emae_service_complete (emae, &emae->priv->source);
		if (!ok) {
			d (printf ("receive page incomplete\n"));
		}
	}

	if (ok && (pageid == NULL || !strcmp (pageid, "30.send"))) {
		if (emae->type == EMAE_PAGES && refresh) {
			emae_refresh_providers (emae, &emae->priv->transport);
		}
		ok = emae_service_complete (emae, &emae->priv->transport);
		if (!ok) {
			d (printf ("send page incomplete\n"));
		}
	}

	if (ok && (pageid == NULL || !strcmp (pageid, "40.management"))) {
		ok = (tmp = e_account_get_string (emae->account, E_ACCOUNT_NAME))
			&& tmp[0]
			&& ((ea = mail_config_get_account_by_name (tmp)) == NULL
			    || ea == emae->original);
		if (!ok) {
			d (printf ("management page incomplete\n"));
		}
	}

	return ok;
}

gboolean
em_account_editor_check (EMAccountEditor *emae, const gchar *page)
{
	return emae_check_complete ((EConfig *)emae->config, page, emae);
}

/* HACK: FIXME: the component should listen to the account object directly */
static void
add_new_store (gchar *uri, CamelStore *store, gpointer user_data)
{
	MailComponent *component = mail_component_peek ();
	EAccount *account = user_data;

	if (store == NULL)
		return;

	mail_component_add_store (component, store, account->name);
}

static void
emae_commit (EConfig *ec, GSList *items, gpointer data)
{
	EMAccountEditor *emae = data;
	EAccountList *accounts = mail_config_get_accounts ();
	EAccount *account;

	/* the mail-config*acconts* api needs a lot of work */

	if (emae->original) {
		d (printf ("Committing account '%s'\n", e_account_get_string (emae->account, E_ACCOUNT_NAME)));
		e_account_import (emae->original, emae->account);
		account = emae->original;
		e_account_list_change (accounts, account);
	} else {
		d (printf ("Adding new account '%s'\n", e_account_get_string (emae->account, E_ACCOUNT_NAME)));
		e_account_list_add (accounts, emae->account);
		account = emae->account;

		/* HACK: this will add the account to the folder tree.
		   We should just be listening to the account list directly for changed events */
		if (account->enabled
		    && emae->priv->source.provider
		    && (emae->priv->source.provider->flags & CAMEL_PROVIDER_IS_STORAGE))
			mail_get_store (e_account_get_string (emae->account, E_ACCOUNT_SOURCE_URL), NULL, add_new_store, account);
	}

	if (gtk_toggle_button_get_active (emae->priv->default_account))
		e_account_list_set_default (accounts, account);

	e_account_list_save (accounts);
}

void
em_account_editor_commit (EMAccountEditor *emae)
{
	emae_commit ((EConfig *)emae->config, NULL, emae);
}

static void
emae_editor_destroyed (GtkWidget *dialog, EMAccountEditor *emae)
{
	emae->editor = NULL;
	g_object_unref (emae);
}

static void
em_account_editor_construct (EMAccountEditor *emae, EAccount *account, em_account_editor_t type, const gchar *id)
{
	EMAccountEditorPrivate *gui = emae->priv;
	gint i, index;
	GSList *l;
	GList *prov;
	EMConfig *ec;
	EMConfigTargetAccount *target;
	GHashTable *have;
	EConfigItem *items;

	emae->type = type;
	emae->original = account;
	if (emae->original) {
		gchar *xml;

		g_object_ref (emae->original);
		xml = e_account_to_xml (emae->original);
		emae->account = e_account_new_from_xml (xml);
		g_free (xml);

		if (type != EMAE_PAGES)
			emae->do_signature = TRUE;
	} else {
		/* TODO: have a get_default_account thing?? */
		emae->account = e_account_new ();
		emae->account->enabled = TRUE;
		e_account_set_string (emae->account, E_ACCOUNT_DRAFTS_FOLDER_URI,
				     mail_component_get_folder_uri (NULL, MAIL_COMPONENT_FOLDER_DRAFTS));
		e_account_set_string (emae->account, E_ACCOUNT_SENT_FOLDER_URI,
				     mail_component_get_folder_uri (NULL, MAIL_COMPONENT_FOLDER_SENT));
	}

	/* sort the providers, remote first */
	gui->providers = g_list_sort (camel_provider_list (TRUE), (GCompareFunc)provider_compare);

	if (type == EMAE_NOTEBOOK) {
		ec = em_config_new (E_CONFIG_BOOK, id);
		items = emae_editor_items;
		if (!emae_editor_items_translated) {
			for (i=0;items[i].path;i++) {
				if (items[i].label)
					items[i].label = gettext (items[i].label);
			}
			emae_editor_items_translated = TRUE;
		}
	} else {
		ec = em_config_new (E_CONFIG_DRUID, id);
		items = emae_druid_items;
		if (!emae_druid_items_translated) {
			for (i=0;items[i].path;i++) {
				if (items[i].label)
					items[i].label = _(items[i].label);
			}
			emae_druid_items_translated = TRUE;
		}
	}

	emae->config = gui->config = ec;
	l = NULL;
	for (i=0;items[i].path;i++)
		l = g_slist_prepend (l, &items[i]);
	e_config_add_items ((EConfig *)ec, l, emae_commit, NULL, emae_free, emae);

	/* This is kinda yuck, we're dynamically mapping from the 'old style' extensibility api to the new one */
	l = NULL;
	have = g_hash_table_new (g_str_hash, g_str_equal);
	index = 20;
	for (prov=gui->providers;prov;prov=g_list_next (prov)) {
		CamelProviderConfEntry *entries = ((CamelProvider *)prov->data)->extra_conf;

		for (i=0;entries && entries[i].type != CAMEL_PROVIDER_CONF_END;i++) {
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

			item = g_malloc0(sizeof (*item));
			item->item.type = E_CONFIG_SECTION_TABLE;
			item->item.path = g_strdup_printf ("20.receive_options/%02d.%s", myindex, name?name:"unnamed");
			item->item.label = g_strdup (entries[i].text);

			l = g_slist_prepend (l, item);

			item = g_malloc0(sizeof (*item));
			item->item.type = E_CONFIG_ITEM_TABLE;
			item->item.path = g_strdup_printf ("20.receive_options/%02d.%s/80.camelitem", myindex, name?name:"unnamed");
			item->item.factory = emae_receive_options_extra_item;
			item->item.user_data = g_strdup (entries[i].name);

			l = g_slist_prepend (l, item);

			index += 10;
			g_hash_table_insert (have, (gpointer)entries[i].name, have);
		}
	}
	g_hash_table_destroy (have);
	e_config_add_items ((EConfig *)ec, l, NULL, NULL, emae_free_auto, emae);
	gui->extra_items = l;

	e_config_add_page_check ((EConfig *)ec, NULL, emae_check_complete, emae);

	target = em_config_target_new_account (ec, emae->account);
	e_config_set_target ((EConfig *)ec, (EConfigTarget *)target);

	if (type != EMAE_PAGES) {
		emae->editor = e_config_create_window ((EConfig *)ec, NULL, type==EMAE_NOTEBOOK?_("Account Editor"):_("Evolution Account Assistant"));
		g_signal_connect (emae->editor, "destroy", G_CALLBACK(emae_editor_destroyed), emae);
	} else {
		e_config_create_widget ((EConfig *)ec);
	}
}
