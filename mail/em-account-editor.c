/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors:
 *    Dan Winship <danw@ximian.com>
 *    Jeffrey Stedfast <fejj@ximian.com>
 *    Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
#endif

#include <glib.h>

#include <string.h>
#include <stdarg.h>

#include <gconf/gconf-client.h>

#include <glade/glade.h>

#include <gtk/gtkentry.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtktextbuffer.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcelllayout.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtktable.h>

#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-druid.h>
#include <libgnomeui/gnome-druid-page-standard.h>

#include <e-util/e-account-list.h>
#include <e-util/e-signature-list.h>

#include <widgets/misc/e-error.h>

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
	GSList *widgets;

	/* Only CAMEL_PROVIDER_CONF_ENTRYs GtkEntrys are stored here.
	   The auto-detect camel provider code will probably be removed */
	GHashTable *extra_table;
};

typedef struct _EMAccountEditorService {
	EMAccountEditor *emae;	/* parent pointer, for callbacks */

	struct _GtkWidget *frame;
	struct _GtkWidget *container;

	struct _GtkLabel *description;
	struct _GtkEntry *hostname;
	struct _GtkEntry *username;
	struct _GtkEntry *path;
	struct _GtkWidget *ssl_frame;
	struct _GtkComboBox *use_ssl;
	struct _GtkWidget *ssl_hbox;
	struct _GtkWidget *no_ssl;

	struct _GtkWidget *auth_frame;
	struct _GtkComboBox *authtype;
	struct _GtkWidget *authitem;
	struct _GtkToggleButton *remember;
	struct _GtkButton *check_supported;
	struct _GtkToggleButton *needs_auth;

	struct _GtkWidget *check_dialog;
	int check_id;

	GList *authtypes;	/* if "Check supported" */
	CamelProvider *provider;
	CamelProviderType type;

	int auth_changed_id;
} EMAccountEditorService;

typedef struct _EMAccountEditorPrivate {
	struct _GladeXML *xml;
	struct _GladeXML *druidxml;
	struct _EMConfig *config;
	GList *providers;

	/* signatures */
	struct _GtkComboBox *signatures_dropdown;
	guint sig_added_id;
	guint sig_removed_id;
	guint sig_changed_id;
	const char *sig_uid;
	
	/* incoming mail */
	EMAccountEditorService source;
	
	/* extra incoming config */
	CamelProvider *extra_provider;
	GSList *extra_items;	/* this is freed by the econfig automatically */

	/* outgoing mail */
	EMAccountEditorService transport;
	
	/* account management */
	struct _GtkToggleButton *default_account;
	
	/* special folders */
	struct _GtkButton *drafts_folder_button;
	struct _GtkButton *sent_folder_button;
	struct _GtkButton *restore_folders_button;

	/* Security */
	struct _GtkEntry *pgp_key;
	struct _GtkToggleButton *pgp_encrypt_to_self;
	struct _GtkToggleButton *pgp_always_sign;
	struct _GtkToggleButton *pgp_no_imip_sign;
	struct _GtkToggleButton *pgp_always_trust;

	struct _GtkToggleButton *smime_sign_default;
	struct _GtkEntry *smime_sign_key;
	struct _GtkButton *smime_sign_key_select;
	struct _GtkButton *smime_sign_key_clear;
	struct _GtkButton *smime_sign_select;
	struct _GtkToggleButton *smime_encrypt_default;
	struct _GtkToggleButton *smime_encrypt_to_self;
	struct _GtkEntry *smime_encrypt_key;
	struct _GtkButton *smime_encrypt_key_select;
	struct _GtkButton *smime_encrypt_key_clear;

	/* for druid page preparation */
	unsigned int identity_set:1;
	unsigned int receive_set:1;
	unsigned int management_set:1;
} EMAccountEditorPrivate;

static GtkWidget *emae_setup_authtype(EMAccountEditor *emae, EMAccountEditorService *service);

static GtkVBoxClass *emae_parent;

static void
emae_init(GObject *o)
{
	EMAccountEditor *emae = (EMAccountEditor *)o;

	emae->priv = g_malloc0(sizeof(*emae->priv));

	emae->priv->source.emae = emae;
	emae->priv->transport.emae = emae;
}

static void
emae_finalise(GObject *o)
{
	EMAccountEditor *emae = (EMAccountEditor *)o;
	EMAccountEditorPrivate *p = emae->priv;

	if (p->sig_added_id) {
		ESignatureList *signatures = mail_config_get_signatures();

		g_signal_handler_disconnect(signatures, p->sig_added_id);
		g_signal_handler_disconnect(signatures, p->sig_removed_id);
		g_signal_handler_disconnect(signatures, p->sig_changed_id);
	}

	if (p->xml)
		g_object_unref(p->xml);
	if (p->druidxml)
		g_object_unref(p->druidxml);

	g_list_free(p->source.authtypes);
	g_list_free(p->transport.authtypes);

	g_list_free(p->providers);
	g_free(p);

	g_object_unref(emae->account);
	if (emae->original)
		g_object_unref(emae->original);

	((GObjectClass *)emae_parent)->finalize(o);
}

static void
emae_class_init(GObjectClass *klass)
{
	klass->finalize = emae_finalise;
}

GType
em_account_editor_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EMAccountEditorClass),
			NULL, NULL,
			(GClassInitFunc)emae_class_init,
			NULL, NULL,
			sizeof(EMAccountEditor), 0,
			(GInstanceInitFunc)emae_init
		};
		emae_parent = g_type_class_ref(G_TYPE_OBJECT);
		type = g_type_register_static(G_TYPE_OBJECT, "EMAccountEditor", &info, 0);
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
EMAccountEditor *em_account_editor_new(EAccount *account, em_account_editor_t type)
{
	EMAccountEditor *emae = g_object_new(em_account_editor_get_type(), 0);

	em_account_editor_construct(emae, account, type);

	return emae;
}

/* ********************************************************************** */

static struct {
	char *label;
	char *value;
} ssl_options[] = {
	{ N_("No encryption"), "never" },
	{ N_("TLS encryption"), "when-possible" },
	{ N_("SSL encryption"), "always" }
};

#define num_ssl_options (sizeof (ssl_options) / sizeof (ssl_options[0]))

static gboolean
is_email (const char *address)
{
	/* This is supposed to check if the address's domain could be
           an FQDN but alas, it's not worth the pain and suffering. */
	const char *at;
	
	at = strchr (address, '@');
	/* make sure we have an '@' and that it's not the first or last char */
	if (!at || at == address || *(at + 1) == '\0')
		return FALSE;
	
	return TRUE;
}

static CamelURL *
emae_account_url(EMAccountEditor *emae, int urlid)
{
	CamelURL *url = NULL;
	const char *uri;

	uri = e_account_get_string(emae->account, urlid);

	if (uri && uri[0])
		url = camel_url_new(uri, NULL);

	if (url == NULL) {
		url = camel_url_new("dummy:", NULL);
		camel_url_set_protocol(url, NULL);
	}

	return url;
}

/* ********************************************************************** */
static void
emae_license_state(GtkToggleButton *button, GtkDialog *dialog)
{
	gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_ACCEPT,
					  gtk_toggle_button_get_active(button));
}

static gboolean
emae_load_text(GtkTextView *view, const char *filename)
{
	FILE *fd;
	char filebuf[1024];
	GtkTextIter iter;
	GtkTextBuffer *buffer;
	int count;

	g_return_val_if_fail (filename != NULL , FALSE);

	fd = fopen (filename, "r");
	if (fd) {
		buffer =  gtk_text_buffer_new (NULL);
		gtk_text_buffer_get_start_iter (buffer, &iter);
		while (!feof (fd) && !ferror (fd)) {
			count = fread (filebuf, 1, sizeof (filebuf), fd);
			gtk_text_buffer_insert (buffer, &iter, filebuf, count);
		}

		gtk_text_view_set_buffer(GTK_TEXT_VIEW (view), GTK_TEXT_BUFFER(buffer));
		fclose (fd);
	}

	return fd != NULL;
}

static gboolean
emae_display_license(EMAccountEditor *emae, CamelProvider *prov)
{
	GladeXML *xml;
	GtkWidget *w, *dialog;
	char *tmp;
	GtkResponseType response = GTK_RESPONSE_NONE;
	
	xml = glade_xml_new (EVOLUTION_GLADEDIR "/mail-dialogs.glade", "license_dialog", NULL);
	dialog = glade_xml_get_widget(xml, "license_dialog");
	gtk_dialog_set_response_sensitive((GtkDialog *)dialog, GTK_RESPONSE_ACCEPT, FALSE);
	tmp = g_strdup_printf(_("%s License Agreement"), prov->license);
	gtk_window_set_title((GtkWindow *)dialog, tmp);
	g_free(tmp);

	g_signal_connect(glade_xml_get_widget(xml, "license_checkbutton"),
			 "toggled", G_CALLBACK(emae_license_state), dialog);

	tmp = g_strdup_printf(_("\nPlease read carefully the license agreement\n" 
				"for %s displayed below\n" 
				"and tick the check box for accepting it\n"), prov->license);
	gtk_label_set_text((GtkLabel *)glade_xml_get_widget(xml, "license_top_label"), tmp);
	g_free(tmp);

	w = glade_xml_get_widget(xml, "license_textview");
	if (emae_load_text((GtkTextView *)w, prov->license_file)) {
		gtk_text_view_set_editable((GtkTextView *)w, FALSE);
		response = gtk_dialog_run((GtkDialog *)dialog);
	} else {
		e_error_run((GtkWindow *)gtk_widget_get_toplevel(emae->editor),
			    "mail:no-load-license", prov->license_file, NULL);
	}

	gtk_widget_destroy(dialog);
	g_object_unref(xml);
	
	return (response == GTK_RESPONSE_ACCEPT);
}

static gboolean
emae_check_license(EMAccountEditor *emae, CamelProvider *prov)
{
	gboolean accepted = TRUE;

	if (prov->flags & CAMEL_PROVIDER_HAS_LICENSE) {
		GConfClient *gconf = mail_config_get_gconf_client();
		GSList *providers_list, *l;

		providers_list = gconf_client_get_list (gconf, "/apps/evolution/mail/licenses", GCONF_VALUE_STRING, NULL);
		
		for (l = providers_list, accepted = FALSE; l && !accepted; l = g_slist_next(l))
			accepted = (strcmp((char *)l->data, prov->protocol) == 0);

		if (!accepted
		    && (accepted = emae_display_license(emae, prov)) == TRUE) {
			providers_list = g_slist_append(providers_list, g_strdup(prov->protocol));
			gconf_client_set_list(gconf, 
					      "/apps/evolution/mail/licenses",
					      GCONF_VALUE_STRING,
					      providers_list, NULL);
		}

		g_slist_foreach(providers_list, (GFunc)g_free, NULL);
		g_slist_free(providers_list);
	}

	return accepted;
}

static void
default_folders_clicked (GtkButton *button, gpointer user_data)
{
	EMAccountEditor *emae = user_data;
	const char *uri;
	
	uri = mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_DRAFTS);
	em_folder_selection_button_set_selection((EMFolderSelectionButton *)emae->priv->drafts_folder_button, uri);

	uri = mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_SENT);
	em_folder_selection_button_set_selection((EMFolderSelectionButton *)emae->priv->sent_folder_button, uri);
}

/* custom widget factories */
GtkWidget *em_account_editor_folder_selector_button_new (char *widget_name, char *string1, char *string2, int int1, int int2);

GtkWidget *
em_account_editor_folder_selector_button_new (char *widget_name, char *string1, char *string2, int int1, int int2)
{
	return (GtkWidget *)em_folder_selection_button_new(_("Select Folder"), NULL);
}

GtkWidget *em_account_editor_dropdown_new(char *widget_name, char *string1, char *string2, int int1, int int2);

GtkWidget *
em_account_editor_dropdown_new(char *widget_name, char *string1, char *string2, int int1, int int2)
{
	return (GtkWidget *)gtk_combo_box_new();
}

GtkWidget *em_account_editor_ssl_selector_new(char *widget_name, char *string1, char *string2, int int1, int int2);

GtkWidget *
em_account_editor_ssl_selector_new(char *widget_name, char *string1, char *string2, int int1, int int2)
{
	GtkComboBox *dropdown = (GtkComboBox *)gtk_combo_box_new();
	GtkCellRenderer *cell = gtk_cell_renderer_text_new();
	GtkListStore *store;
	int i;
	GtkTreeIter iter;

	gtk_widget_show((GtkWidget *)dropdown);

	store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);

	for (i=0;i<num_ssl_options;i++) {
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, _(ssl_options[i].label), 1, ssl_options[i].value, -1);
	}

	gtk_cell_layout_pack_start((GtkCellLayout *)dropdown, cell, TRUE);
	gtk_cell_layout_set_attributes((GtkCellLayout *)dropdown, cell, "text", 0, NULL);

	gtk_combo_box_set_model(dropdown, (GtkTreeModel *)store);

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
emae_auto_detect(EMAccountEditor *emae)
{
	EMAccountEditorPrivate *gui = emae->priv;
	EMAccountEditorService *service = &gui->source;
	GHashTable *auto_detected;
	GSList *l;
	CamelProviderConfEntry *entries;
	char *value;
	int i;
	CamelURL *url;

	if (service->provider == NULL
	    || (entries = service->provider->extra_conf) == NULL)
		return;

	printf("Running auto-detect\n");

	url = emae_account_url(emae, E_ACCOUNT_SOURCE_URL);
	camel_provider_auto_detect(service->provider, url, &auto_detected, NULL);
	camel_url_free(url);
	if (auto_detected == NULL) {
		printf(" no values detected\n");
		return;
	}

	for (i = 0; entries[i].type != CAMEL_PROVIDER_CONF_END; i++) {
		struct _receive_options_item *item;
		GtkWidget *w;

		if (entries[i].name == NULL
		    || (value = g_hash_table_lookup (auto_detected, entries[i].name)) == NULL)
			continue;

		/* only 2 providers use this, and they only do it for 3 entries only */
		g_assert(entries[i].type == CAMEL_PROVIDER_CONF_ENTRY);

		w = NULL;
		for (l = emae->priv->extra_items;l;l=g_slist_next(l)) {
			item = l->data;
			if (item->extra_table && (w = g_hash_table_lookup(item->extra_table, entries[i].name)))
				break;
		}

		gtk_entry_set_text((GtkEntry *)w, value?value:"");
	}

	g_hash_table_foreach(auto_detected, emae_auto_detect_free, NULL);
	g_hash_table_destroy(auto_detected);
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
emae_signature_added(ESignatureList *signatures, ESignature *sig, EMAccountEditor *emae)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model(emae->priv->signatures_dropdown);

	gtk_list_store_append((GtkListStore *)model, &iter);
	gtk_list_store_set((GtkListStore *)model, &iter, 0, sig->autogen?_("Autogenerated"):sig->name, 1, sig->uid, -1);

	gtk_combo_box_set_active(emae->priv->signatures_dropdown, gtk_tree_model_iter_n_children(model, NULL)-1);
}

static int
emae_signature_get_iter(EMAccountEditor *emae, ESignature *sig, GtkTreeModel **modelp, GtkTreeIter *iter)
{
	GtkTreeModel *model;
	int found = 0;

	model = gtk_combo_box_get_model(emae->priv->signatures_dropdown);
	*modelp = model;
	if (!gtk_tree_model_get_iter_first(model, iter))
		return FALSE;

	do {
		char *uid;

		gtk_tree_model_get(model, iter, 1, &uid, -1);
		if (uid && !strcmp(uid, sig->uid))
			found = TRUE;
		g_free(uid);
	} while (!found && gtk_tree_model_iter_next(model, iter));

	return found;
}

static void
emae_signature_removed(ESignatureList *signatures, ESignature *sig, EMAccountEditor *emae)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (emae_signature_get_iter(emae, sig, &model, &iter))
		gtk_list_store_remove((GtkListStore *)model, &iter);
}

static void
emae_signature_changed(ESignatureList *signatures, ESignature *sig, EMAccountEditor *emae)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (emae_signature_get_iter(emae, sig, &model, &iter))
		gtk_list_store_set((GtkListStore *)model, &iter, 0, sig->autogen?_("Autogenerated"):sig->name, -1);
}

static void
emae_signaturetype_changed(GtkComboBox *dropdown, EMAccountEditor *emae)
{
	int id = gtk_combo_box_get_active(dropdown);
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *uid = NULL;

	if (id != -1) {
		model = gtk_combo_box_get_model(dropdown);
		if (gtk_tree_model_iter_nth_child(model, &iter, NULL, id))
			gtk_tree_model_get(model, &iter, 1, &uid, -1);
	}

	printf("signaturetype changed: %d uid=%s\n", id, uid?uid:"");

	e_account_set_string(emae->account, E_ACCOUNT_ID_SIGNATURE, uid);
	g_free(uid);
}

static void
emae_signature_new(GtkWidget *w, EMAccountEditor *emae)
{
	/* TODO: why is this in composer prefs? apart from it being somewhere to put it? */
	em_composer_prefs_new_signature((GtkWindow *)gtk_widget_get_toplevel(w),
					gconf_client_get_bool(mail_config_get_gconf_client(),
							      "/apps/evolution/mail/composer/send_html", NULL));
}

static GtkWidget *
emae_setup_signatures(EMAccountEditor *emae)
{
	EMAccountEditorPrivate *p = emae->priv;
	GtkComboBox *dropdown = (GtkComboBox *)glade_xml_get_widget(p->xml, "signature_dropdown");
	GtkCellRenderer *cell = gtk_cell_renderer_text_new();
	GtkListStore *store;
	int i, active=0;
	GtkTreeIter iter;
	ESignatureList *signatures;
	EIterator *it;
	const char *current = e_account_get_string(emae->account, E_ACCOUNT_ID_SIGNATURE);
	GtkWidget *button;

	emae->priv->signatures_dropdown = dropdown;
	gtk_widget_show((GtkWidget *)dropdown);

	store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, _("None"), 1, NULL, -1);

	signatures = mail_config_get_signatures ();

	p->sig_added_id = g_signal_connect(signatures, "signature-added", G_CALLBACK(emae_signature_added), emae);
	p->sig_removed_id = g_signal_connect(signatures, "signature-removed", G_CALLBACK(emae_signature_removed), emae);
	p->sig_changed_id = g_signal_connect(signatures, "signature-changed", G_CALLBACK(emae_signature_changed), emae);

	/* we need to count the 'none' entry before using the index */
	i = 1;
	it = e_list_get_iterator ((EList *) signatures);
	while (e_iterator_is_valid (it)) {
		ESignature *sig = (ESignature *)e_iterator_get(it);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, sig->autogen?_("Autogenerated"):sig->name, 1, sig->uid, -1);

		printf("check sig '%s' is ours '%s' = %s\n", sig->uid, current, (current && !strcmp(current, sig->uid))?"yep":"no");
		if (current && !strcmp(current, sig->uid))
			active = i;

		e_iterator_next(it);
		i++;
	}
	g_object_unref (it);

	gtk_cell_layout_pack_start((GtkCellLayout *)dropdown, cell, TRUE);
	gtk_cell_layout_set_attributes((GtkCellLayout *)dropdown, cell, "text", 0, NULL);

	gtk_combo_box_set_model(dropdown, (GtkTreeModel *)store);
	gtk_combo_box_set_active(dropdown, active);

	g_signal_connect(dropdown, "changed", G_CALLBACK(emae_signaturetype_changed), emae);
	gtk_widget_set_sensitive((GtkWidget *)dropdown, e_account_writable(emae->account, E_ACCOUNT_ID_SIGNATURE));

	button = glade_xml_get_widget(p->xml, "sigAddNew");
	g_signal_connect(button, "clicked", G_CALLBACK(emae_signature_new), emae);
	gtk_widget_set_sensitive(button,
				 gconf_client_key_is_writable(mail_config_get_gconf_client(),
							      "/apps/evolution/mail/signatures", NULL));

	return (GtkWidget *)dropdown;
}

static void
emae_account_entry_changed(GtkEntry *entry, EMAccountEditor *emae)
{
	int item = GPOINTER_TO_INT(g_object_get_data((GObject *)entry, "account-item"));

	e_account_set_string(emae->account, item, gtk_entry_get_text(entry));
}

static GtkEntry *
emae_account_entry(EMAccountEditor *emae, const char *name, int item)
{
	GtkEntry *entry;
	const char *text;

	entry = (GtkEntry *)glade_xml_get_widget(emae->priv->xml, name);
	text = e_account_get_string(emae->account, item);
	if (text)
		gtk_entry_set_text(entry, text);
	g_object_set_data((GObject *)entry, "account-item", GINT_TO_POINTER(item));
	g_signal_connect(entry, "changed", G_CALLBACK(emae_account_entry_changed), emae);
	gtk_widget_set_sensitive((GtkWidget *)entry, e_account_writable(emae->account, item));

	return entry;
}

static void
emae_account_toggle_changed(GtkToggleButton *toggle, EMAccountEditor *emae)
{
	int item = GPOINTER_TO_INT(g_object_get_data((GObject *)toggle, "account-item"));

	e_account_set_bool(emae->account, item, gtk_toggle_button_get_active(toggle));
}

static void
emae_account_toggle_widget(EMAccountEditor *emae, GtkToggleButton *toggle, int item)
{
	gtk_toggle_button_set_active(toggle, e_account_get_bool(emae->account, item));
	g_object_set_data((GObject *)toggle, "account-item", GINT_TO_POINTER(item));
	g_signal_connect(toggle, "toggled", G_CALLBACK(emae_account_toggle_changed), emae);
	gtk_widget_set_sensitive((GtkWidget *)toggle, e_account_writable(emae->account, item));
}

static GtkToggleButton *
emae_account_toggle(EMAccountEditor *emae, const char *name, int item)
{
	GtkToggleButton *toggle;

	toggle = (GtkToggleButton *)glade_xml_get_widget(emae->priv->xml, name);
	emae_account_toggle_widget(emae, toggle, item);

	return toggle;
}

static void
emae_account_spinint_changed(GtkSpinButton *spin, EMAccountEditor *emae)
{
	int item = GPOINTER_TO_INT(g_object_get_data((GObject *)spin, "account-item"));

	e_account_set_int(emae->account, item, gtk_spin_button_get_value(spin));
}

static void
emae_account_spinint_widget(EMAccountEditor *emae, GtkSpinButton *spin, int item)
{
	gtk_spin_button_set_value(spin, e_account_get_int(emae->account, item));
	g_object_set_data((GObject *)spin, "account-item", GINT_TO_POINTER(item));
	g_signal_connect(spin, "value_changed", G_CALLBACK(emae_account_spinint_changed), emae);
	gtk_widget_set_sensitive((GtkWidget *)spin, e_account_writable(emae->account, item));
}

#if 0
static GtkSpinButton *
emae_account_spinint(EMAccountEditor *emae, const char *name, int item)
{
	GtkSpinButton *spin;

	spin = (GtkSpinButton *)glade_xml_get_widget(emae->priv->xml, name);
	emae_account_spinint_widget(emae, spin, item);

	return spin;
}
#endif

static void
emae_account_folder_changed(EMFolderSelectionButton *folder, EMAccountEditor *emae)
{
	int item = GPOINTER_TO_INT(g_object_get_data((GObject *)folder, "account-item"));

	e_account_set_string(emae->account, item, em_folder_selection_button_get_selection(folder));
}

static EMFolderSelectionButton *
emae_account_folder(EMAccountEditor *emae, const char *name, int item, int deffolder)
{
	EMFolderSelectionButton *folder;
	const char *uri;

	folder = (EMFolderSelectionButton *)glade_xml_get_widget(emae->priv->xml, name);
	uri = e_account_get_string(emae->account, item);
	if (uri) {
		char *tmp = em_uri_to_camel(uri);

		em_folder_selection_button_set_selection(folder, tmp);
		g_free(tmp);
	} else {
		em_folder_selection_button_set_selection(folder, mail_component_get_folder_uri(NULL, deffolder));
	}

	g_object_set_data((GObject *)folder, "account-item", GINT_TO_POINTER(item));
	g_object_set_data((GObject *)folder, "folder-default", GINT_TO_POINTER(deffolder));
	g_signal_connect(folder, "selected", G_CALLBACK(emae_account_folder_changed), emae);
	gtk_widget_show((GtkWidget *)folder);

	gtk_widget_set_sensitive((GtkWidget *)folder, e_account_writable(emae->account, item));

	return folder;
}

#if defined (HAVE_NSS)
static void
smime_changed(EMAccountEditor *emae)
{
	EMAccountEditorPrivate *gui = emae->priv;
	int act;
	const char *tmp;

	tmp = gtk_entry_get_text(gui->smime_sign_key);
	act = tmp && tmp[0];
	gtk_widget_set_sensitive((GtkWidget *)gui->smime_sign_key_clear, act);
	gtk_widget_set_sensitive((GtkWidget *)gui->smime_sign_default, act);
	if (!act)
		gtk_toggle_button_set_active(gui->smime_sign_default, FALSE);

	tmp = gtk_entry_get_text(gui->smime_encrypt_key);
	act = tmp && tmp[0];
	gtk_widget_set_sensitive((GtkWidget *)gui->smime_encrypt_key_clear, act);
	gtk_widget_set_sensitive((GtkWidget *)gui->smime_encrypt_default, act);
	gtk_widget_set_sensitive((GtkWidget *)gui->smime_encrypt_to_self, act);
	if (!act) {
		gtk_toggle_button_set_active(gui->smime_encrypt_default, FALSE);
		gtk_toggle_button_set_active(gui->smime_encrypt_to_self, FALSE);
	}
}

static void
smime_sign_key_selected(GtkWidget *dialog, const char *key, EMAccountEditor *emae)
{
	EMAccountEditorPrivate *gui = emae->priv;

	if (key != NULL) {
		gtk_entry_set_text(gui->smime_sign_key, key);
		smime_changed(emae);
	}

	gtk_widget_destroy(dialog);
}

static void
smime_sign_key_select(GtkWidget *button, EMAccountEditor *emae)
{
	EMAccountEditorPrivate *gui = emae->priv;
	GtkWidget *w;

	w = e_cert_selector_new(E_CERT_SELECTOR_SIGNER, gtk_entry_get_text(gui->smime_sign_key));
	gtk_window_set_modal((GtkWindow *)w, TRUE);
	gtk_window_set_transient_for((GtkWindow *)w, (GtkWindow *)gtk_widget_get_toplevel((GtkWidget *)emae));
	g_signal_connect(w, "selected", G_CALLBACK(smime_sign_key_selected), emae);
	gtk_widget_show(w);
}

static void
smime_sign_key_clear(GtkWidget *w, EMAccountEditor *emae)
{
	EMAccountEditorPrivate *gui = emae->priv;

	gtk_entry_set_text(gui->smime_sign_key, "");
	smime_changed(emae);
}

static void
smime_encrypt_key_selected(GtkWidget *dialog, const char *key, EMAccountEditor *emae)
{
	EMAccountEditorPrivate *gui = emae->priv;

	if (key != NULL) {
		gtk_entry_set_text(gui->smime_encrypt_key, key);
		smime_changed(emae);
	}

	gtk_widget_destroy(dialog);
}

static void
smime_encrypt_key_select(GtkWidget *button, EMAccountEditor *emae)
{
	EMAccountEditorPrivate *gui = emae->priv;
	GtkWidget *w;

	w = e_cert_selector_new(E_CERT_SELECTOR_SIGNER, gtk_entry_get_text(gui->smime_encrypt_key));
	gtk_window_set_modal((GtkWindow *)w, TRUE);
	gtk_window_set_transient_for((GtkWindow *)w, (GtkWindow *)gtk_widget_get_toplevel((GtkWidget *)emae));
	g_signal_connect(w, "selected", G_CALLBACK(smime_encrypt_key_selected), emae);
	gtk_widget_show(w);
}

static void
smime_encrypt_key_clear(GtkWidget *w, EMAccountEditor *emae)
{
	EMAccountEditorPrivate *gui = emae->priv;

	gtk_entry_set_text(gui->smime_encrypt_key, "");
	smime_changed(emae);
}
#endif

static void
emae_url_set_hostport(CamelURL *url, const char *txt)
{
	const char *port;
	char *host;

	if (txt && (port = strchr(txt, ':'))) {
		camel_url_set_port(url, atoi(port+1));
		host = g_alloca(port-txt+1);
		memcpy(host, txt, port-txt);
		host[port-txt] = 0;
	} else {
		host = (char *)txt;
	}
	camel_url_set_host(url, host);
}

/* This is used to map a funciton which will set on the url a string value.
   We need our own function for host:port decoding, as above */
struct _provider_host_info {
	guint32 flag;
	void (*setval)(CamelURL *, const char *);
	const char *widgets[3];
};

static struct _provider_host_info emae_source_host_info[] = {
	{ CAMEL_URL_PART_HOST, emae_url_set_hostport, { "source_host", "source_host_label" } },
	{ CAMEL_URL_PART_USER, camel_url_set_user, { "source_user", "source_user_label", } },
	{ CAMEL_URL_PART_PATH, camel_url_set_path, { "source_path", "source_path_label", "source_path_entry" } },
	{ CAMEL_URL_PART_AUTH, NULL, { NULL, "source_auth_frame" } },
	{ 0 },
};

static struct _provider_host_info emae_transport_host_info[] = {
	{ CAMEL_URL_PART_HOST, emae_url_set_hostport, { "transport_host", "transport_host_label" } },
	{ CAMEL_URL_PART_USER, camel_url_set_user, { "transport_user", "transport_user_label", } },
	{ CAMEL_URL_PART_AUTH, NULL, { NULL, "transport_auth_frame" } },
	{ 0 },
};

/* This is used to map each of the two services in a typical account to the widgets that represent each service.
   i.e. the receiving (source) service, and the sending (transport) service.
   It is used throughout the following code to drive each page */
static struct _service_info {
	int account_uri_key;
	int save_passwd_key;

	char *frame;
	char *type_dropdown;

	char *container;
	char *description;
	char *hostname;
	char *username;
	char *path;

	char *security_frame;
	char *ssl_hbox;
	char *use_ssl;
	char *ssl_disabled;

	char *needs_auth;
	char *auth_frame;

	char *authtype;
	char *authtype_check;

	char *remember_password;

	struct _provider_host_info *host_info;
} emae_service_info[CAMEL_NUM_PROVIDER_TYPES] = {
	{ E_ACCOUNT_SOURCE_URL, E_ACCOUNT_SOURCE_SAVE_PASSWD,
	  "source_frame", "source_type_dropdown",
	  "source_vbox", "source_description", "source_host", "source_user", "source_path",
	  "source_security_frame", "source_ssl_hbox", "source_use_ssl", "source_ssl_disabled",
	  NULL, "source_auth_frame",
	  "source_auth_dropdown", "source_check_supported",
	  "source_remember_password",
	  emae_source_host_info,
	},
	{ E_ACCOUNT_TRANSPORT_URL, E_ACCOUNT_TRANSPORT_SAVE_PASSWD,
	  "transport_frame", "transport_type_dropdown",
	  "transport_vbox", "transport_description", "transport_host", "transport_user", NULL,
	  "transport_security_frame", "transport_ssl_hbox", "transport_use_ssl", "transport_ssl_disabled",
	  "transport_needs_auth", "transport_auth_frame",
	  "transport_auth_dropdown", "transport_check_supported",
	  "transport_remember_password",
	  emae_transport_host_info,
	},
};

static void
emae_uri_changed(EMAccountEditorService *service, CamelURL *url)
{
	char *uri;

	uri = camel_url_to_string(url, 0);
	printf("uri changed: '%s'\n", uri);
	e_account_set_string(service->emae->account, emae_service_info[service->type].account_uri_key, uri);
	g_free(uri);
}

static void
emae_service_url_changed(EMAccountEditorService *service, void (*setval)(CamelURL *, const char *), GtkEntry *entry)
{
	CamelURL *url = emae_account_url(service->emae, emae_service_info[service->type].account_uri_key);

	setval(url, gtk_entry_get_text(entry));
	emae_uri_changed(service, url);
	camel_url_free(url);
}

static void
emae_hostname_changed(GtkEntry *entry, EMAccountEditorService *service)
{
	emae_service_url_changed(service, emae_url_set_hostport, entry);
}

static void
emae_username_changed(GtkEntry *entry, EMAccountEditorService *service)
{
	emae_service_url_changed(service, camel_url_set_user, entry);
}

static void
emae_path_changed(GtkEntry *entry, EMAccountEditorService *service)
{
	emae_service_url_changed(service, camel_url_set_path, entry);
}

static void
emae_needs_auth(GtkToggleButton *toggle, EMAccountEditorService *service)
{
	GtkWidget *w;
	int need = gtk_toggle_button_get_active(toggle);

	w = glade_xml_get_widget(service->emae->priv->xml, emae_service_info[service->type].auth_frame);
	gtk_widget_set_sensitive(w, need);
	/* if need ; service_changed? */
}

static int
emae_ssl_update(EMAccountEditorService *service, CamelURL *url)
{
	int id = gtk_combo_box_get_active(service->use_ssl);
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *ssl;

	if (id == -1)
		return 0;

	model = gtk_combo_box_get_model(service->use_ssl);
	if (gtk_tree_model_iter_nth_child(model, &iter, NULL, id)) {
		gtk_tree_model_get(model, &iter, 1, &ssl, -1);
		if (!strcmp(ssl, "none"))
			ssl = NULL;
		camel_url_set_param(url, "use_ssl", ssl);
		return 1;
	}

	return 0;
}

static void
emae_ssl_changed(GtkComboBox *dropdown, EMAccountEditorService *service)
{
	CamelURL *url = emae_account_url(service->emae, emae_service_info[service->type].account_uri_key);

	if (emae_ssl_update(service, url))
		emae_uri_changed(service, url);
	camel_url_free(url);
}

static void
emae_service_provider_changed(EMAccountEditorService *service)
{
	int i, j;
	void (*show)(GtkWidget *);
	CamelURL *url = emae_account_url(service->emae, emae_service_info[service->type].account_uri_key);

	if (service->provider) {
		int enable;
		GtkWidget *dwidget = NULL;

		camel_url_set_protocol(url, service->provider->protocol);
		gtk_label_set_text(service->description, service->provider->description);
		if (!emae_check_license(service->emae, service->provider))
			gtk_widget_hide(service->frame);
		else
			gtk_widget_show(service->frame);

		enable = e_account_writable_option(service->emae->account, service->provider->protocol, "auth");
		gtk_widget_set_sensitive((GtkWidget *)service->authtype, enable);
		gtk_widget_set_sensitive((GtkWidget *)service->check_supported, enable);

		enable = e_account_writable_option(service->emae->account, service->provider->protocol, "use_ssl");
		gtk_widget_set_sensitive((GtkWidget *)service->use_ssl, enable);
			
		enable = e_account_writable(service->emae->account, emae_service_info[service->type].save_passwd_key);
		gtk_widget_set_sensitive((GtkWidget *)service->remember, enable);

		for (i=0;emae_service_info[service->type].host_info[i].flag;i++) {
			const char *name;
			GtkWidget *w;
			struct _provider_host_info *info = &emae_service_info[service->type].host_info[i];

			enable = CAMEL_PROVIDER_ALLOWS(service->provider, info->flag);
			show = enable?gtk_widget_show:gtk_widget_hide;

			for (j=0; j < sizeof(info->widgets)/sizeof(info->widgets[0]); j++) {
				name = info->widgets[j];
				if (name) {
					w = glade_xml_get_widget(service->emae->priv->xml, name);
					show(w);
					if (j == 0) {
						if (dwidget == NULL && enable)
							dwidget = w;

						if (info->setval)
							info->setval(url, enable?gtk_entry_get_text((GtkEntry *)w):NULL);
					}
				}
			}
		}

		if (dwidget)
			gtk_widget_grab_focus(dwidget);

		if (CAMEL_PROVIDER_ALLOWS(service->provider, CAMEL_URL_PART_AUTH)) {
			camel_url_set_authmech(url, NULL);
			emae_setup_authtype(service->emae, service);
			if (service->needs_auth && !CAMEL_PROVIDER_NEEDS(service->provider, CAMEL_URL_PART_AUTH))
				gtk_widget_show((GtkWidget *)service->needs_auth);
		} else {
			if (service->needs_auth)
				gtk_widget_hide((GtkWidget *)service->needs_auth);
		}
#ifdef HAVE_SSL
		gtk_widget_hide(service->no_ssl);
		if (service->provider->flags & CAMEL_PROVIDER_SUPPORTS_SSL) {
			emae_ssl_update(service, url);
			show = gtk_widget_show;
		} else {
			camel_url_set_param(url, "use_ssl", NULL);
			show = gtk_widget_hide;
		}
		show(service->ssl_frame);
		show(service->ssl_hbox);
#else
		gtk_widget_hide(service->ssl_hbox);
		gtk_widget_show(service->no_ssl);
		camel_url_set_param(url, "use_ssl", NULL);
#endif
	} else {
		camel_url_set_protocol(url, NULL);
		gtk_label_set_text(service->description, "");
		gtk_widget_hide(service->frame);
		gtk_widget_hide(service->auth_frame);
		gtk_widget_hide(service->ssl_frame);
	}

	/* FIXME: linked services? */
	/* FIXME: permissions setup */

	emae_uri_changed(service, url);
	camel_url_free(url);
}

static void
emae_provider_changed(GtkComboBox *dropdown, EMAccountEditorService *service)
{
	int id = gtk_combo_box_get_active(dropdown);
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (id == -1)
		return;

	model = gtk_combo_box_get_model(dropdown);
	if (!gtk_tree_model_iter_nth_child(model, &iter, NULL, id))
		return;

	gtk_tree_model_get(model, &iter, 1, &service->provider, -1);

	g_list_free(service->authtypes);
	service->authtypes = NULL;

	emae_service_provider_changed(service);

	e_config_target_changed((EConfig *)service->emae->priv->config, E_CONFIG_TARGET_CHANGED_REBUILD);
}

static GtkWidget *
emae_setup_providers(EMAccountEditor *emae, EMAccountEditorService *service)
{
	EMAccountEditorPrivate *gui = emae->priv;
	EAccount *account = emae->account;
	GtkListStore *store;
	GtkTreeIter iter;
	GList *l;
	GtkCellRenderer *cell = gtk_cell_renderer_text_new();
	GtkComboBox *dropdown;
	int active = 0, i;
	struct _service_info *info = &emae_service_info[service->type];
	const char *uri = e_account_get_string(account, info->account_uri_key);
	char *current = NULL;

	dropdown = (GtkComboBox *)glade_xml_get_widget(gui->xml, info->type_dropdown);
	gtk_widget_show((GtkWidget *)dropdown);

	if (uri) {
		const char *colon = strchr(uri, ':');
		int len;

		if (colon) {
			len = colon-uri;
			current = g_alloca(len+1);
			memcpy(current, uri, len);
			current[len] = 0;
		}
	}
		
	store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);

	i = 0;

	/* We just special case each type here, its just easier */
	if (service->type == CAMEL_PROVIDER_STORE) {
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, _("None"), 1, NULL, -1);
		i++;
	}

	for (l=gui->providers; l; l=l->next) {
		CamelProvider *provider = l->data;

		if (!((strcmp(provider->domain, "mail") == 0
		       || strcmp (provider->domain, "news") == 0)
		      && provider->object_types[service->type]
		      && (service->type != CAMEL_PROVIDER_STORE || (provider->flags & CAMEL_PROVIDER_IS_SOURCE) != 0)))
			continue;

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, provider->name, 1, provider, -1);

		/* FIXME: GtkCombo doesn't support sensitiviy, we can hopefully kill this crap anyway */
#if 0
		if (type == CAMEL_PROVIDER_TRANSPORT
		    && CAMEL_PROVIDER_IS_STORE_AND_TRANSPORT (provider))
			gtk_widget_set_sensitive (item, FALSE);
#endif
		/* find the displayed and set default */
		if (i == 0 || (current && strcmp(provider->protocol, current) == 0)) {
			service->provider = provider;
			active = i;

			/* we need to set this value on the uri too */
			if (current == NULL) {
				CamelURL *url = emae_account_url(emae, info->account_uri_key);

				camel_url_set_protocol(url, provider->protocol);
				emae_uri_changed(service, url);
				camel_url_free(url);
			}
		}
		i++;
	}

	gtk_combo_box_set_model(dropdown, (GtkTreeModel *)store);
	gtk_cell_layout_pack_start((GtkCellLayout *)dropdown, cell, TRUE);
	gtk_cell_layout_set_attributes((GtkCellLayout *)dropdown, cell, "text", 0, NULL);

	gtk_combo_box_set_active(dropdown, -1);	/* needed for gtkcombo bug(?) */
	gtk_combo_box_set_active(dropdown, active);
	g_signal_connect(dropdown, "changed", G_CALLBACK(emae_provider_changed), service);

	return (GtkWidget *)dropdown;
}

static void
emae_authtype_changed(GtkComboBox *dropdown, EMAccountEditorService *service)
{
	int id = gtk_combo_box_get_active(dropdown);
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelServiceAuthType *authtype;
	CamelURL *url;

	if (id == -1)
		return;

	url = emae_account_url(service->emae, emae_service_info[service->type].account_uri_key);
	model = gtk_combo_box_get_model(dropdown);
	if (gtk_tree_model_iter_nth_child(model, &iter, NULL, id)) {
		gtk_tree_model_get(model, &iter, 1, &authtype, -1);
		if (authtype)
			camel_url_set_authmech(url, authtype->authproto);
		else
			camel_url_set_authmech(url, NULL);
		emae_uri_changed(service, url);
	}
	camel_url_free(url);

	gtk_widget_set_sensitive((GtkWidget *)service->remember, authtype?authtype->need_password:FALSE);
}

static void emae_check_authtype(GtkWidget *w, EMAccountEditorService *service);

static GtkWidget *
emae_setup_authtype(EMAccountEditor *emae, EMAccountEditorService *service)
{
	EMAccountEditorPrivate *gui = emae->priv;
	EAccount *account = emae->account;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkComboBox *dropdown;
	GtkWidget *w;
	int active = 0;
	int i;
	struct _service_info *info = &emae_service_info[service->type];
	const char *uri = e_account_get_string(account, info->account_uri_key);
	GList *l, *ll;
	CamelURL *url = NULL;

	dropdown = (GtkComboBox *)glade_xml_get_widget(gui->xml, info->authtype);
	gtk_widget_show((GtkWidget *)dropdown);

	store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_BOOLEAN);

	if (uri)
		url = camel_url_new(uri, NULL);

	if (service->provider) {
		for (i=0, l=service->provider->authtypes; l; l=l->next, i++) {
			CamelServiceAuthType *authtype = l->data;
			int avail;

			/* if we have some already shown */
			if (service->authtypes) {
				for (ll = service->authtypes;ll;ll = g_list_next(ll))
					if (!strcmp(authtype->authproto, ((CamelServiceAuthType *)ll->data)->authproto))
						break;
				avail = ll != NULL;
			} else {
				avail = TRUE;
			}
			
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter, 0, authtype->name, 1, authtype, 2, !avail, -1);

			if (url && url->authmech && !strcmp(url->authmech, authtype->authproto))
				active = i;
		}
	}

	gtk_combo_box_set_model(dropdown, (GtkTreeModel *)store);
	gtk_combo_box_set_active(dropdown, -1);

	if (service->auth_changed_id == 0) {
		GtkCellRenderer *cell = gtk_cell_renderer_text_new();

		gtk_cell_layout_pack_start((GtkCellLayout *)dropdown, cell, TRUE);
		gtk_cell_layout_set_attributes((GtkCellLayout *)dropdown, cell, "text", 0, "strikethrough", 2, NULL);

		service->auth_changed_id = g_signal_connect(dropdown, "changed", G_CALLBACK(emae_authtype_changed), service);
		w = glade_xml_get_widget(gui->xml, info->authtype_check);
		g_signal_connect(w, "clicked", G_CALLBACK(emae_check_authtype), service);
	}

	gtk_combo_box_set_active(dropdown, active);

	if (url)
		camel_url_free(url);

	return (GtkWidget *)dropdown;
}

static void emae_check_authtype_done(const char *uri, CamelProviderType type, GList *types, void *data)
{
	EMAccountEditorService *service = data;

	if (service->check_dialog) {
		if (service->authtypes)
			g_list_free(service->authtypes);

		service->authtypes = g_list_copy(types);
		emae_setup_authtype(service->emae, service);
		gtk_widget_destroy(service->check_dialog);
	}

	if (service->emae->editor)
		gtk_widget_set_sensitive(service->emae->editor, TRUE);

	service->check_id = -1;
	g_object_unref(service->emae);
}

static void emae_check_authtype_response(GtkWidget *d, int button, EMAccountEditorService *service)
{
	mail_msg_cancel(service->check_id);
	gtk_widget_destroy(service->check_dialog);
	service->check_dialog = NULL;

	if (service->emae->editor)
		gtk_widget_set_sensitive(service->emae->editor, TRUE);
}

static void emae_check_authtype(GtkWidget *w, EMAccountEditorService *service)
{
	EMAccountEditor *emae = service->emae;
	const char *uri;

	/* TODO: do we need to remove the auth mechanism from the uri? */
	uri = e_account_get_string(emae->account, emae_service_info[service->type].account_uri_key);
	g_object_ref(emae);

	service->check_dialog = e_error_new((GtkWindow *)gtk_widget_get_toplevel(emae->editor),
					    "mail:checking-service", NULL);
	g_signal_connect(service->check_dialog, "response", G_CALLBACK(emae_check_authtype_response), service);
	gtk_widget_show(service->check_dialog);
	gtk_widget_set_sensitive(emae->editor, FALSE);
	service->check_id = mail_check_service(uri, service->type, emae_check_authtype_done, service);
}

static void
emae_setup_service(EMAccountEditor *emae, EMAccountEditorService *service)
{
	EMAccountEditorPrivate *gui = emae->priv;
	struct _service_info *info = &emae_service_info[service->type];
	CamelURL *url = emae_account_url(emae, info->account_uri_key);
	const char *uri = e_account_get_string(emae->account, info->account_uri_key);
	const char *tmp;
	int i;

	service->provider = uri?camel_provider_get(uri, NULL):NULL;

	service->frame = glade_xml_get_widget(gui->xml, info->frame);
	service->container = glade_xml_get_widget(gui->xml, info->container);
	service->description = GTK_LABEL (glade_xml_get_widget (gui->xml, info->description));
	service->hostname = GTK_ENTRY (glade_xml_get_widget (gui->xml, info->hostname));
	service->username = GTK_ENTRY (glade_xml_get_widget (gui->xml, info->username));
	if (info->path)
		service->path = GTK_ENTRY (glade_xml_get_widget (gui->xml, info->path));

	service->ssl_frame = glade_xml_get_widget (gui->xml, info->security_frame);
	gtk_widget_hide (service->ssl_frame);
	service->ssl_hbox = glade_xml_get_widget (gui->xml, info->ssl_hbox);
	service->use_ssl = (GtkComboBox *)glade_xml_get_widget (gui->xml, info->use_ssl);
	service->no_ssl = glade_xml_get_widget (gui->xml, info->ssl_disabled);

	/* configure ui for current settings */
	if (url->host) {
		if (url->port) {
			char *host = g_strdup_printf("%s:%d", url->host, url->port);

			gtk_entry_set_text(service->hostname, host);
			g_free(host);
		} else
			gtk_entry_set_text(service->hostname, url->host);
	}
	if (url->user)
		gtk_entry_set_text(service->username, url->user);
	if (service->path && url->path)
		gtk_entry_set_text(service->path, url->path);

	tmp = camel_url_get_param(url, "use_ssl");
	if (tmp == NULL)
		tmp = "never";

	for (i=0;i<num_ssl_options;i++) {
		if (!strcmp(ssl_options[i].value, tmp)) {
			gtk_combo_box_set_active(service->use_ssl, i);
			break;
		}
	}
	camel_url_free(url);

	g_signal_connect (service->hostname, "changed", G_CALLBACK (emae_hostname_changed), service);
	g_signal_connect (service->username, "changed", G_CALLBACK (emae_username_changed), service);
	if (service->path)
		g_signal_connect (service->path, "changed", G_CALLBACK (emae_path_changed), service);

	g_signal_connect(service->use_ssl, "changed", G_CALLBACK(emae_ssl_changed), service);

	service->auth_frame = glade_xml_get_widget(gui->xml, info->auth_frame);
	service->remember = emae_account_toggle(emae, info->remember_password, info->save_passwd_key);
	service->check_supported = (GtkButton *)glade_xml_get_widget(gui->xml, info->authtype_check);
	if (info->needs_auth) {
		service->needs_auth = (GtkToggleButton *)glade_xml_get_widget (gui->xml, info->needs_auth);
		g_signal_connect(service->needs_auth, "toggled", G_CALLBACK(emae_needs_auth), service);
		emae_needs_auth(service->needs_auth, service);
	}

	emae_setup_providers(emae, service);
	service->authtype = (GtkComboBox *)emae_setup_authtype(emae, service);

	if (!e_account_writable (emae->account, info->account_uri_key))
		gtk_widget_set_sensitive(service->container, FALSE);
	else
		gtk_widget_set_sensitive(service->container, TRUE);

	emae_service_provider_changed(service);
}

static struct {
	char *name;
	int item;
} emae_identity_entries[] = {
	{ "management_name", E_ACCOUNT_NAME },
	{ "identity_full_name", E_ACCOUNT_ID_NAME },
	{ "identity_address", E_ACCOUNT_ID_ADDRESS },
	{ "identity_reply_to", E_ACCOUNT_ID_REPLY_TO },
	{ "identity_organization", E_ACCOUNT_ID_ORGANIZATION },
};

static GtkWidget *
emae_identity_page(EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *gui = emae->priv;
	EAccount *account = emae->account;
	int i;
	GtkWidget *w;

	if (old)
		return old;

	/* Management & Identity fields*/
	for (i=0;i<sizeof(emae_identity_entries)/sizeof(emae_identity_entries[0]);i++)
		emae_account_entry(emae, emae_identity_entries[i].name, emae_identity_entries[i].item);

	gui->default_account = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui->xml, "management_default"));
	if (!mail_config_get_default_account ()
	    || (account == mail_config_get_default_account ()))
		gtk_toggle_button_set_active (gui->default_account, TRUE);

	if (emae->do_signature) {
		emae_setup_signatures(emae);
	} else {
		/* TODO: this could/should probably be neater */
		gtk_widget_hide(glade_xml_get_widget(gui->xml, "sigLabel"));
		gtk_widget_hide(glade_xml_get_widget(gui->xml, "sigOption"));
		gtk_widget_hide(glade_xml_get_widget(gui->xml, "sigAddNew"));
	}
	
	w = glade_xml_get_widget(gui->xml, item->label);
	if (((EConfig *)gui->config)->type == E_CONFIG_DRUID) {
		GtkWidget *page = glade_xml_get_widget(gui->druidxml, "identity_page");

		/* need to set packing? */
		gtk_widget_reparent(w, ((GnomeDruidPageStandard *)page)->vbox);

		return page;
	}

	return w;
}

static GtkWidget *
emae_receive_page(EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *gui = emae->priv;
	GtkWidget *w;

	if (old)
		return old;

	gui->source.type = CAMEL_PROVIDER_STORE;
	emae_setup_service(emae, &gui->source);

	w = glade_xml_get_widget(gui->xml, item->label);
	if (((EConfig *)gui->config)->type == E_CONFIG_DRUID) {
		GtkWidget *page = glade_xml_get_widget(gui->druidxml, "source_page");

		/* need to set packing? */
		gtk_widget_reparent(w, ((GnomeDruidPageStandard *)page)->vbox);

		return page;
	}

	return w;
}

static void
emae_option_toggle_changed(GtkToggleButton *toggle, EMAccountEditorService *service)
{
	const char *name = g_object_get_data((GObject *)toggle, "option-name");
	GSList *depl = g_object_get_data((GObject *)toggle, "dependent-list");
	int active = gtk_toggle_button_get_active(toggle);
	CamelURL *url = emae_account_url(service->emae, emae_service_info[service->type].account_uri_key);

	for (;depl;depl = g_slist_next(depl))
		gtk_widget_set_sensitive((GtkWidget *)depl->data, active);

	camel_url_set_param(url, name, active?"":NULL);
	emae_uri_changed(service, url);
	camel_url_free(url);
}

static GtkWidget *
emae_option_toggle(EMAccountEditorService *service, CamelURL *url, const char *text, const char *name, int def)
{
	GtkWidget *w;

	/* FIXME: how do we get the default value ever? */
	w = g_object_new(gtk_check_button_get_type(),
			 "label", text,
			 "active", camel_url_get_param(url, name) != NULL,
			 NULL);
	g_object_set_data((GObject *)w, "option-name", (void *)name);
	g_signal_connect(w, "toggled", G_CALLBACK(emae_option_toggle_changed), service);
	gtk_widget_show(w);

	printf("adding option toggle '%s'\n", text);

	return w;
}

static void
emae_option_entry_changed(GtkEntry *entry, EMAccountEditorService *service)
{
	const char *name = g_object_get_data((GObject *)entry, "option-name");
	const char *text = gtk_entry_get_text(entry);
	CamelURL *url = emae_account_url(service->emae, emae_service_info[service->type].account_uri_key);

	camel_url_set_param(url, name, text && text[0]?text:NULL);
	emae_uri_changed(service, url);
	camel_url_free(url);
}

static GtkWidget *
emae_option_entry(EMAccountEditorService *service, CamelURL *url, const char *name, const char *def)
{
	GtkWidget *w;
	const char *val = camel_url_get_param(url, name);

	if (val == NULL)
		val = def;

	w = g_object_new(gtk_entry_get_type(),
			 "text", val,
			 NULL);
	g_object_set_data((GObject *)w, "option-name", (void *)name);
	g_signal_connect(w, "changed", G_CALLBACK(emae_option_entry_changed), service);
	gtk_widget_show(w);

	return w;
}

static void
emae_option_checkspin_changed(GtkSpinButton *spin, EMAccountEditorService *service)
{
	const char *name = g_object_get_data((GObject *)spin, "option-name");
	char value[16];
	CamelURL *url = emae_account_url(service->emae, emae_service_info[service->type].account_uri_key);

	sprintf(value, "%d", gtk_spin_button_get_value_as_int(spin));
	camel_url_set_param(url, name, value);
	emae_uri_changed(service, url);
	camel_url_free(url);
}

static void
emae_option_checkspin_check_changed(GtkToggleButton *toggle, EMAccountEditorService *service)
{
	const char *name = g_object_get_data((GObject *)toggle, "option-name");
	GtkSpinButton *spin = g_object_get_data((GObject *)toggle, "option-target");

	if (gtk_toggle_button_get_active(toggle)) {
		gtk_widget_set_sensitive((GtkWidget *)spin, TRUE);
		emae_option_checkspin_changed(spin, service);
	} else {
		CamelURL *url = emae_account_url(service->emae, emae_service_info[service->type].account_uri_key);

		camel_url_set_param(url, name, NULL);
		gtk_widget_set_sensitive((GtkWidget *)spin, FALSE);
		emae_uri_changed(service, url);
		camel_url_free(url);
	}
}

/* this is a fugly api */
static GtkWidget *
emae_option_checkspin(EMAccountEditorService *service, CamelURL *url, const char *name, const char *fmt, const char *info)
{
	GtkWidget *hbox, *check, *spin, *label;
	double min, def, max;
	char *pre, *post;
	const char *val;
	char on;
	int enable;

	pre = g_alloca(strlen(fmt)+1);
	strcpy(pre, fmt);
	post = strstr(pre, "%s");
	if (post) {
		*post = 0;
		post+=2;
	}

	if (sscanf(info, "%c:%lf:%lf:%lf", &on, &min, &def, &max) != 4) {
		min = 0.0;
		def = 0.0;
		max = 1.0;
	}

	if ((enable = (val = camel_url_get_param(url, name)) != NULL) )
		def = strtod(val, NULL);
	else
		enable = (on == 'y');

	hbox = gtk_hbox_new(FALSE, 0);
	check = g_object_new(gtk_check_button_get_type(), "label", pre, "active", enable, NULL);
	spin = gtk_spin_button_new((GtkAdjustment *)gtk_adjustment_new(def, min, max, 1, 1, 1), 1, 0);
	if (post)
		label = gtk_label_new(post);
	gtk_box_pack_start((GtkBox *)hbox, check, FALSE, TRUE, 0);
	gtk_box_pack_start((GtkBox *)hbox, spin, FALSE, TRUE, 0);
	if (label)
		gtk_box_pack_start((GtkBox *)hbox, label, FALSE, TRUE, 4);

	g_object_set_data((GObject *)spin, "option-name", (void *)name);
	g_object_set_data((GObject *)check, "option-name", (void *)name);
	g_object_set_data((GObject *)check, "option-target", (void *)spin);

	g_signal_connect(spin, "value_changed", G_CALLBACK(emae_option_checkspin_changed), service);
	g_signal_connect(check, "toggled", G_CALLBACK(emae_option_checkspin_check_changed), service);

	gtk_widget_show_all(hbox);

	return hbox;
}

static GtkWidget *
emae_receive_options_item(EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	EMAccountEditor *emae = data;
	GtkWidget *w, *box;
	int row;

	if (emae->priv->source.provider == NULL
	    || emae->priv->source.provider->extra_conf == NULL)
		return NULL;

	if (old)
		return old;

	/* We have to add the automatic mail check item with the rest of the receive options */
	row = ((GtkTable *)parent)->nrows;

	box = gtk_hbox_new(FALSE, 4);
	w = gtk_check_button_new_with_label(_("Automatically check for _new mail every"));
	emae_account_toggle_widget(emae, (GtkToggleButton *)w, E_ACCOUNT_SOURCE_AUTO_CHECK);
	gtk_box_pack_start((GtkBox *)box, w, FALSE, FALSE, 0);

	w = gtk_spin_button_new_with_range(1.0, 1440.0, 1.0);
	emae_account_spinint_widget(emae, (GtkSpinButton *)w, E_ACCOUNT_SOURCE_AUTO_CHECK_TIME);
	gtk_box_pack_start((GtkBox *)box, w, FALSE, TRUE, 0);

	w = gtk_label_new(_("minutes"));
	gtk_box_pack_start((GtkBox *)box, w, FALSE, FALSE, 0);

	gtk_widget_show_all(box);

	gtk_table_attach((GtkTable *)parent, box, 0, 2, row, row+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);

	return box;
}

static GtkWidget *
emae_receive_options_extra_item(EConfig *ec, EConfigItem *eitem, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	EMAccountEditor *emae = data;
	struct _receive_options_item *item = (struct _receive_options_item *)eitem;
	GtkWidget *w, *l;
	CamelProviderConfEntry *entries;
	GtkWidget *depw;
	GSList *depl = NULL, *widgets = NULL, *n;
	EMAccountEditorService *service = &emae->priv->source;
	int row, i;
	GHashTable *extra;
	CamelURL *url;

	/* Clean up any widgets we had setup before */
	if (item->widgets) {
		g_slist_foreach(item->widgets, (GFunc)gtk_widget_destroy, NULL);
		g_hash_table_destroy(item->extra_table);
		item->widgets = NULL;
		item->extra_table = NULL;
	}

	if (emae->priv->source.provider == NULL
	    || emae->priv->source.provider->extra_conf == NULL)
		return NULL;

	entries = emae->priv->source.provider->extra_conf;
	for (i=0;entries && entries[i].type != CAMEL_PROVIDER_CONF_END;i++)
		if (entries[i].type == CAMEL_PROVIDER_CONF_SECTION_START
		    && entries[i].name
		    && strcmp(entries[i].name, eitem->user_data) == 0)
			goto section;

	return NULL;
section:
	printf("Building extra section '%s'\n", eitem->path);

	url = emae_account_url(emae, emae_service_info[service->type].account_uri_key);
	item->extra_table = g_hash_table_new(g_str_hash, g_str_equal);
	extra = g_hash_table_new(g_str_hash, g_str_equal);
	row = ((GtkTable *)parent)->nrows;

	for (;entries[i].type != CAMEL_PROVIDER_CONF_END && entries[i].type != CAMEL_PROVIDER_CONF_SECTION_END;i++) {
		if (entries[i].depname) {
			depw = g_hash_table_lookup(extra, entries[i].depname);
			if (depw)
				depl = g_object_steal_data((GObject *)depw, "dependent-list");
		} else
			depw = NULL;

		switch (entries[i].type) {
		case CAMEL_PROVIDER_CONF_SECTION_START:
		case CAMEL_PROVIDER_CONF_SECTION_END:
			break;
		case CAMEL_PROVIDER_CONF_LABEL:
			/* FIXME: This is a hack for exchange connector, labels should be removed from confentry */
			if (!strcmp(entries[i].name, "hostname"))
				l = glade_xml_get_widget(emae->priv->xml, "source_host_label");
			else if (!strcmp(entries[i].name, "username"))
				l = glade_xml_get_widget(emae->priv->xml,"source_user_label");
			else
				l = NULL;

			if (l) {
				gtk_label_set_text_with_mnemonic((GtkLabel *)l, entries[i].text);
				if (depw)
					depl = g_slist_prepend(depl, l);
			}
			break;
		case CAMEL_PROVIDER_CONF_CHECKBOX:
			w = emae_option_toggle(service, url, entries[i].text, entries[i].name, atoi(entries[i].value));
			gtk_table_attach((GtkTable *)parent, w, 0, 2, row, row+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
			g_hash_table_insert(extra, entries[i].name, w);
			if (depw)
				depl = g_slist_prepend(depl, w);
			widgets = g_slist_prepend(widgets, w);
			row++;
			break;
		case CAMEL_PROVIDER_CONF_ENTRY:
			l = g_object_new(gtk_label_get_type(), "label", entries[i].text, "xalign", 0.0, NULL);
			gtk_widget_show(l);
			w = emae_option_entry(service, url, entries[i].name, entries[i].value);
			gtk_table_attach((GtkTable *)parent, l, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);
			gtk_table_attach((GtkTable *)parent, w, 1, 2, row, row+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
			if (depw) {
				depl = g_slist_prepend(depl, w);
				depl = g_slist_prepend(depl, l);
			}
			widgets = g_slist_prepend(widgets, w);
			widgets = g_slist_prepend(widgets, l);
			row++;
			/* FIXME: this is another hack for exchange/groupwise connector */
			g_hash_table_insert(item->extra_table, entries[i].name, w);
			break;
		case CAMEL_PROVIDER_CONF_CHECKSPIN:
			w = emae_option_checkspin(service, url, entries[i].name, entries[i].text, entries[i].value);
			gtk_table_attach((GtkTable *)parent, w, 0, 2, row, row+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);
			if (depw)
				depl = g_slist_prepend(depl, w);
			widgets = g_slist_prepend(widgets, w);
			row++;
			break;
		default:
			break;
		}

		if (depw && depl) {
			int act = gtk_toggle_button_get_active((GtkToggleButton *)depw);

			g_object_set_data_full((GObject *)depw, "dependent-list", depl, (GDestroyNotify)g_slist_free);
			for (n=depl;n;n=g_slist_next(n))
				gtk_widget_set_sensitive((GtkWidget *)n->data, act);
		}
	}

	camel_url_free(url);

	/* Since EConfig destroys the factory widget when it changes, we
	 * need to destroy our own ones as well, and add a dummy item
	 * so it knows this section isn't empty */

	w = gtk_label_new("");
	gtk_widget_hide(w);
	gtk_table_attach((GtkTable *)parent, w, 0, 2, row, row+1, 0, 0, 0, 0);
	item->widgets = widgets;

	return w;
}

static GtkWidget *
emae_send_page(EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *gui = emae->priv;
	GtkWidget *w;

	if (old)
		return old;

	/* Transport */
	gui->transport.type = CAMEL_PROVIDER_TRANSPORT;
	emae_setup_service(emae, &gui->transport);

	w = glade_xml_get_widget(gui->xml, item->label);
	if (((EConfig *)gui->config)->type == E_CONFIG_DRUID) {
		GtkWidget *page = glade_xml_get_widget(gui->druidxml, "transport_page");

		/* need to set packing? */
		gtk_widget_reparent(w, ((GnomeDruidPageStandard *)page)->vbox);

		return page;
	}

	return w;
}

static GtkWidget *
emae_defaults_page(EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *gui = emae->priv;

	if (old)
		return old;

	/* Special folders */
	gui->drafts_folder_button = (GtkButton *)emae_account_folder(emae, "drafts_button", E_ACCOUNT_DRAFTS_FOLDER_URI, MAIL_COMPONENT_FOLDER_DRAFTS);
	gui->sent_folder_button = (GtkButton *)emae_account_folder(emae, "sent_button", E_ACCOUNT_SENT_FOLDER_URI, MAIL_COMPONENT_FOLDER_SENT);

	/* Special Folders "Reset Defaults" button */
	gui->restore_folders_button = (GtkButton *)glade_xml_get_widget (gui->xml, "default_folders_button");
	g_signal_connect (gui->restore_folders_button, "clicked", G_CALLBACK (default_folders_clicked), emae);
	
	/* Always Cc/Bcc */
	emae_account_toggle(emae, "always_cc", E_ACCOUNT_CC_ALWAYS);
	emae_account_entry(emae, "cc_addrs", E_ACCOUNT_CC_ADDRS);
	emae_account_toggle(emae, "always_bcc", E_ACCOUNT_BCC_ALWAYS);
	emae_account_entry(emae, "bcc_addrs", E_ACCOUNT_BCC_ADDRS);

	gtk_widget_set_sensitive((GtkWidget *)gui->drafts_folder_button, e_account_writable(emae->account, E_ACCOUNT_DRAFTS_FOLDER_URI));
	gtk_widget_set_sensitive((GtkWidget *)gui->sent_folder_button, e_account_writable(emae->account, E_ACCOUNT_SENT_FOLDER_URI));
	gtk_widget_set_sensitive((GtkWidget *)gui->restore_folders_button,
				 e_account_writable(emae->account, E_ACCOUNT_SENT_FOLDER_URI)
				 || e_account_writable(emae->account, E_ACCOUNT_DRAFTS_FOLDER_URI));
	
	return glade_xml_get_widget(gui->xml, item->label);
}

static GtkWidget *
emae_security_page(EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *gui = emae->priv;

	if (old)
		return old;

	/* Security */
	emae_account_entry(emae, "pgp_key", E_ACCOUNT_PGP_KEY);
	emae_account_toggle(emae, "pgp_encrypt_to_self", E_ACCOUNT_PGP_ENCRYPT_TO_SELF);
	emae_account_toggle(emae, "pgp_always_sign", E_ACCOUNT_PGP_ALWAYS_SIGN);
	emae_account_toggle(emae, "pgp_no_imip_sign", E_ACCOUNT_PGP_NO_IMIP_SIGN);
	emae_account_toggle(emae, "pgp_always_trust", E_ACCOUNT_PGP_ALWAYS_TRUST);
	
#if defined (HAVE_NSS)
	/* TODO: this should handle its entry separately? */
	gui->smime_sign_key = emae_account_entry(emae, "smime_sign_key", E_ACCOUNT_SMIME_SIGN_KEY);
	gui->smime_sign_key_select = (GtkButton *)glade_xml_get_widget (gui->xml, "smime_sign_key_select");
	gui->smime_sign_key_clear = (GtkButton *)glade_xml_get_widget (gui->xml, "smime_sign_key_clear");
	g_signal_connect(gui->smime_sign_key_select, "clicked", G_CALLBACK(smime_sign_key_select), emae);
	g_signal_connect(gui->smime_sign_key_clear, "clicked", G_CALLBACK(smime_sign_key_clear), emae);

	gui->smime_sign_default = emae_account_toggle(emae, "smime_sign_default", E_ACCOUNT_SMIME_SIGN_DEFAULT);

	gui->smime_encrypt_key = emae_account_entry(emae, "smime_encrypt_key", E_ACCOUNT_SMIME_ENCRYPT_KEY);
	gui->smime_encrypt_key_select = (GtkButton *)glade_xml_get_widget (gui->xml, "smime_encrypt_key_select");
	gui->smime_encrypt_key_clear = (GtkButton *)glade_xml_get_widget (gui->xml, "smime_encrypt_key_clear");
	g_signal_connect(gui->smime_encrypt_key_select, "clicked", G_CALLBACK(smime_encrypt_key_select), emae);
	g_signal_connect(gui->smime_encrypt_key_clear, "clicked", G_CALLBACK(smime_encrypt_key_clear), emae);

	gui->smime_encrypt_default = emae_account_toggle(emae, "smime_encrypt_default", E_ACCOUNT_SMIME_ENCRYPT_DEFAULT);
	gui->smime_encrypt_to_self = emae_account_toggle(emae, "smime_encrypt_to_self", E_ACCOUNT_SMIME_ENCRYPT_TO_SELF);
	smime_changed(emae);
#else
	{
		/* Since we don't have NSS, hide the S/MIME config options */
		GtkWidget *frame;
		
		frame = glade_xml_get_widget (gui->xml, "smime_vbox");
		gtk_widget_destroy (frame);
	}
#endif /* HAVE_NSS */

	return glade_xml_get_widget(gui->xml, item->label);
}

static GtkWidget *
emae_widget_glade(EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	EMAccountEditor *emae = data;

	if (old)
		return old;

	printf("getting widget '%s' = %p\n", item->label, glade_xml_get_widget(emae->priv->xml, item->label));

	return glade_xml_get_widget(emae->priv->xml, item->label);
}

/* plugin meta-data for "org.gnome.evolution.mail.config.accountEditor" */
static EMConfigItem emae_editor_items[] = {
	{ E_CONFIG_BOOK, "", "account_editor_notebook", emae_widget_glade },
	{ E_CONFIG_PAGE, "00.identity", "vboxIdentityBorder", emae_identity_page },
	{ E_CONFIG_SECTION, "00.identity/00.name", "account_vbox", emae_widget_glade },
	/* table not vbox: { E_CONFIG_SECTION, "00.identity/10.required", "identity_required_table", emae_widget_glade }, */
	/* table not vbox: { E_CONFIG_SECTION, "00.identity/20.info", "identity_optional_table", emae_widget_glade }, */

	{ E_CONFIG_PAGE, "10.receive", "vboxSourceBorder", emae_receive_page },
	/* table not vbox: { E_CONFIG_SECTION, "10.receive/00.type", "source_type_table", emcp_widget_glade }, */
	/* table not vbox: { E_CONFIG_SECTION, "10.receive/10.config", "table13", emcp_widget_glade }, */
	{ E_CONFIG_SECTION, "10.receive/20.security", "vbox181", emae_widget_glade },
	{ E_CONFIG_SECTION, "10.receive/30.auth", "vbox179", emae_widget_glade },

	/* Most sections for this is auto-generated fromt the camel config */
	{ E_CONFIG_PAGE, "20.receive_options", N_("Receiving Options"), },
	{ E_CONFIG_SECTION_TABLE, "20.receive_options/10.mailcheck", N_("Checking for New Mail"), },
	{ E_CONFIG_ITEM_TABLE, "20.receive_options/10.mailcheck/00.autocheck", NULL, emae_receive_options_item, },

	{ E_CONFIG_PAGE, "30.send", "vboxTransportBorder", emae_send_page },
	/* table not vbox: { E_CONFIG_SECTION, "30.send/00.type", "transport_type_table", emcp_widget_glade }, */
	{ E_CONFIG_SECTION, "30.send/10.config", "vbox12", emae_widget_glade },
	{ E_CONFIG_SECTION, "30.send/20.security", "vbox183", emae_widget_glade },
	{ E_CONFIG_SECTION, "30.send/30.auth", "vbox61", emae_widget_glade },

	{ E_CONFIG_PAGE, "40.defaults", "vboxFoldersBorder", emae_defaults_page },
	{ E_CONFIG_SECTION, "40.defaults/00.folders", "vbox184", emae_widget_glade },
	/* table not vbox: { E_CONFIG_SECTION, "40.defaults/10.composing", "table8", emae_widget_glade }, */

	{ E_CONFIG_PAGE, "50.security", "vboxSecurityBorder", emae_security_page },
	/* 1x1 table(!) not vbox: { E_CONFIG_SECTION, "50.security/00.gpg", "table19", emae_widget_glade }, */
	/* table not vbox: { E_CONFIG_SECTION, "50.security/10.smime", "smime_table", emae_widget_glade }, */
	{ 0 },
};

static GtkWidget *
emae_management_page(EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	EMAccountEditor *emae = data;
	EMAccountEditorPrivate *gui = emae->priv;
	GtkWidget *w;

	if (old)
		return old;

	w = glade_xml_get_widget(gui->xml, item->label);
	if (((EConfig *)gui->config)->type == E_CONFIG_DRUID) {
		GtkWidget *page = glade_xml_get_widget(gui->druidxml, "management_page");

		/* need to set packing? */
		gtk_widget_reparent(w, ((GnomeDruidPageStandard *)page)->vbox);

		return page;
	}

	return w;
}

static GtkWidget *
emae_widget_druid_glade(EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	EMAccountEditor *emae = data;
	GtkWidget *w;

	if (old)
		return old;

	printf("getting widget '%s' = %p\n", item->label, glade_xml_get_widget(emae->priv->druidxml, item->label));

	w = glade_xml_get_widget(emae->priv->druidxml, item->label);
	/* i think the glade file has issues, we need to show all on at least the end page */
	gtk_widget_show_all(w);

	return w;
}

/* plugin meta-data for "org.gnome.evolution.mail.config.accountDruid" */
static EMConfigItem emae_druid_items[] = {
	{ E_CONFIG_DRUID, "", "druid", emae_widget_druid_glade },
	{ E_CONFIG_PAGE_START, "0.start", "start_page", emae_widget_druid_glade },

	{ E_CONFIG_PAGE, "00.identity", "vboxIdentityBorder", emae_identity_page },
	{ E_CONFIG_SECTION, "00.identity/00.name", "account_vbox", emae_widget_glade },
	/* table not vbox: { E_CONFIG_SECTION, "00.identity/10.required", "identity_required_table", emae_widget_glade }, */
	/* table not vbox: { E_CONFIG_SECTION, "00.identity/20.info", "identity_optional_table", emae_widget_glade }, */

	{ E_CONFIG_PAGE, "10.receive", "vboxSourceBorder", emae_receive_page },
	/* table not vbox: { E_CONFIG_SECTION, "10.receive/00.type", "source_type_table", emcp_widget_glade }, */
	/* table not vbox: { E_CONFIG_SECTION, "10.receive/10.config", "table13", emcp_widget_glade }, */
	{ E_CONFIG_SECTION, "10.receive/20.security", "vbox181", emae_widget_glade },
	{ E_CONFIG_SECTION, "10.receive/30.auth", "vbox179", emae_widget_glade },

	/* Most sections for this is auto-generated fromt the camel config */
	{ E_CONFIG_PAGE, "20.receive_options", N_("Receiving Options"), },
	{ E_CONFIG_SECTION_TABLE, "20.receive_options/10.mailcheck", N_("Checking for New Mail"), },
	{ E_CONFIG_ITEM_TABLE, "20.receive_options/10.mailcheck/00.autocheck", NULL, emae_receive_options_item, },

	{ E_CONFIG_PAGE, "30.send", "vboxTransportBorder", emae_send_page },
	/* table not vbox: { E_CONFIG_SECTION, "30.send/00.type", "transport_type_table", emcp_widget_glade }, */
	{ E_CONFIG_SECTION, "30.send/10.config", "vbox12", emae_widget_glade },
	{ E_CONFIG_SECTION, "30.send/20.security", "vbox183", emae_widget_glade },
	{ E_CONFIG_SECTION, "30.send/30.auth", "vbox61", emae_widget_glade },

	{ E_CONFIG_PAGE, "40.management", "management_frame", emae_management_page },

	{ E_CONFIG_PAGE_FINISH, "999.end", "finish_page", emae_widget_druid_glade },
	{ 0 },
};

static void
emae_free(EConfig *ec, GSList *items, void *data)
{
	g_slist_free(items);
}

static void
emae_free_auto(EConfig *ec, GSList *items, void *data)
{
	GSList *l, *n;

	for (l=items;l;) {
		struct _receive_options_item *item = l->data;

		n = g_slist_next(l);
		g_free(item->item.path);
		if (item->extra_table)
			g_hash_table_destroy(item->extra_table);
		g_slist_free(item->widgets);
		g_free(item);
		g_slist_free_1(l);
		l = n;
	}
}

static gboolean
emae_service_complete(EMAccountEditor *emae, EMAccountEditorService *service)
{
	CamelURL *url;
	int ok = TRUE;
	const char *uri;

	if (service->provider == NULL)
		return TRUE;

	uri = e_account_get_string(emae->account, emae_service_info[service->type].account_uri_key);
	if (uri == NULL || (url = camel_url_new(uri, NULL)) == NULL)
		return FALSE;

	if (CAMEL_PROVIDER_NEEDS(service->provider, CAMEL_URL_PART_HOST)
	    && (url->host == NULL || url->host[0] == 0))
		ok = FALSE;

	/* We only need the user if the service needs auth as well, i think */
	if (ok
	    && (service->needs_auth == NULL
		|| CAMEL_PROVIDER_NEEDS(service->provider, CAMEL_URL_PART_AUTH)
		|| gtk_toggle_button_get_active(service->needs_auth))
	    && CAMEL_PROVIDER_NEEDS(service->provider, CAMEL_URL_PART_USER)
	    && (url->user == NULL || url->user[0] == 0))
		ok = FALSE;

	if (ok
	    && CAMEL_PROVIDER_NEEDS(service->provider, CAMEL_URL_PART_PATH)
	    && (url->path == NULL || url->path[0] == 0))
		ok = FALSE;

	camel_url_free(url);

	return ok;
}

static gboolean
emae_check_complete(EConfig *ec, const char *pageid, void *data)
{
	EMAccountEditor *emae = data;
	int ok = TRUE;
	const char *tmp;
	EAccount *ea;

	if (pageid == NULL || !strcmp(pageid, "00.identity")) {
		/* TODO: check the account name is set, and unique in the account list */
		ok = (tmp = e_account_get_string(emae->account, E_ACCOUNT_ID_NAME))
			&& tmp[0]
			&& (tmp = e_account_get_string(emae->account, E_ACCOUNT_ID_ADDRESS))
			&& is_email(tmp)
			&& ((tmp = e_account_get_string(emae->account, E_ACCOUNT_ID_REPLY_TO)) == NULL
			    || tmp[0] == 0
			    || is_email(tmp));
		if (!ok)
			printf("identity incomplete\n");
	}

	if (ok && (pageid == NULL || !strcmp(pageid, "10.receive"))) {
		ok = emae_service_complete(emae, &emae->priv->source);
		if (!ok)
			printf("receive page incomplete\n");
	}

	if (ok && (pageid == NULL || !strcmp(pageid, "30.send"))) {
		ok = emae_service_complete(emae, &emae->priv->transport);
		if (!ok)
			printf("send page incomplete\n");
	}

	if (ok && (pageid == NULL || !strcmp(pageid, "40.management"))) {
		ok = (tmp = e_account_get_string(emae->account, E_ACCOUNT_NAME))
			&& tmp[0]
			&& ((ea = mail_config_get_account_by_name(tmp)) == NULL
			    || ea == emae->original);
		if (!ok)
			printf("management page incomplete\n");
	}

	/* We use the page-check of various pages to 'prepare' or
	   pre-load their values, only in the druid */
	if (pageid
	    && ((EConfig *)emae->priv->config)->type == E_CONFIG_DRUID) {
		if (!strcmp(pageid, "00.identity")) {
			if (!emae->priv->identity_set) {
				char *uname;

				emae->priv->identity_set = 1;
				uname = g_locale_to_utf8(g_get_real_name(), -1, NULL, NULL, NULL);
				if (uname) {
					gtk_entry_set_text((GtkEntry *)glade_xml_get_widget(emae->priv->xml, "management_name"), uname);
					g_free(uname);
				}
			}
		} else if (!strcmp(pageid, "10.receive")) {
			if (!emae->priv->receive_set) {
				char *user, *at;

				emae->priv->receive_set = 1;
				tmp = e_account_get_string(emae->account, E_ACCOUNT_ID_ADDRESS);
				at = strchr(tmp, '@');
				user = g_alloca(at-tmp+1);
				memcpy(user, tmp, at-tmp);
				user[at-tmp] = 0;
				gtk_entry_set_text(emae->priv->source.username, user);
				gtk_entry_set_text(emae->priv->transport.username, user);
			}
		} else if (!strcmp(pageid, "20.receive_options")) {
			if (emae->priv->source.provider
			    && emae->priv->extra_provider != emae->priv->source.provider) {
				emae->priv->extra_provider = emae->priv->source.provider;
				emae_auto_detect(emae);
			}
		} else if (!strcmp(pageid, "40.management")) {
			if (!emae->priv->management_set) {
				char *template;
				unsigned int i = 0, len;

				emae->priv->management_set = 1;
				tmp = e_account_get_string(emae->account, E_ACCOUNT_ID_ADDRESS);
				len = strlen(tmp);
				template = alloca(len + 14);
				strcpy(template, tmp);
				while (mail_config_get_account_by_name(template))
					sprintf(template + len, " (%d)", i++);

				gtk_entry_set_text((GtkEntry *)glade_xml_get_widget(emae->priv->xml, "management_name"), template);
			}
		}
	}

	return ok;
}

/* HACK: FIXME: the component should listen to the account object directly */
static void
add_new_store (char *uri, CamelStore *store, void *user_data)
{
	MailComponent *component = mail_component_peek ();
	EAccount *account = user_data;
	
	if (store == NULL)
		return;
	
	mail_component_add_store (component, store, account->name);
}

static void
emae_commit(EConfig *ec, GSList *items, void *data)
{
	EMAccountEditor *emae = data;
	EAccountList *accounts = mail_config_get_accounts();
	EAccount *account;

	/* the mail-config*acconts* api needs a lot of work */

	if (emae->original) {
		printf("Committing account '%s'\n", e_account_get_string(emae->account, E_ACCOUNT_NAME));
		e_account_import(emae->original, emae->account);
		account = emae->original;
		e_account_list_change(accounts, account);
	} else {
		printf("Adding new account '%s'\n", e_account_get_string(emae->account, E_ACCOUNT_NAME));
		e_account_list_add(accounts, emae->account);
		account = emae->account;

		/* HACK: this will add the account to the folder tree.
		   We should just be listening to the account list directly for changed events */
		if (account->enabled
		    && emae->priv->source.provider
		    && (emae->priv->source.provider->flags & CAMEL_PROVIDER_IS_STORAGE))
			mail_get_store(e_account_get_string(emae->account, E_ACCOUNT_SOURCE_URL), NULL, add_new_store, account);
	}

	if (gtk_toggle_button_get_active(emae->priv->default_account))
		e_account_list_set_default(accounts, account);

	e_account_list_save(accounts);
}

static void
emae_editor_destroyed(GtkWidget *dialog, EMAccountEditor *emae)
{
	emae->editor = NULL;
	g_object_unref(emae);
}

void
em_account_editor_construct(EMAccountEditor *emae, EAccount *account, em_account_editor_t type)
{
	EMAccountEditorPrivate *gui = emae->priv;
	int i, index;
	GSList *l;
	GList *prov;
	EMConfig *ec;
	EMConfigTargetAccount *target;
	GHashTable *have;
	EConfigItem *items;

	emae->type = type;
	emae->original = account;
	if (emae->original) {
		char *xml;

		g_object_ref(emae->original);
		xml = e_account_to_xml(emae->original);
		emae->account = e_account_new_from_xml(xml);
		g_free(xml);

		emae->do_signature = TRUE;
	} else {
		/* TODO: have a get_default_account thing?? */
		emae->account = e_account_new();
		emae->account->enabled = TRUE;
		e_account_set_string(emae->account, E_ACCOUNT_DRAFTS_FOLDER_URI,
				     mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_DRAFTS));
		e_account_set_string(emae->account, E_ACCOUNT_SENT_FOLDER_URI,
				     mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_SENT));
	}

	gui->xml = glade_xml_new(EVOLUTION_GLADEDIR "/mail-config.glade", "account_editor_notebook", NULL);
	if (type == EMAE_DRUID)
		gui->druidxml = glade_xml_new(EVOLUTION_GLADEDIR "/mail-config.glade", "druid", NULL);

	/* sort the providers, remote first */
	gui->providers = g_list_sort(camel_provider_list(TRUE), (GCompareFunc)provider_compare);

	if (type == EMAE_NOTEBOOK) {
		ec = em_config_new(E_CONFIG_BOOK, "org.gnome.evolution.mail.config.accountEditor");
		items = emae_editor_items;
	} else {
		ec = em_config_new(E_CONFIG_DRUID, "org.gnome.evolution.mail.config.accountDruid");
		items = emae_druid_items;
	}

	emae->config = gui->config = ec;
	l = NULL;
	for (i=0;items[i].path;i++)
		l = g_slist_prepend(l, &items[i]);
	e_config_add_items((EConfig *)ec, l, emae_commit, NULL, emae_free, emae);

	/* This is kinda yuck, we're dynamically mapping from the 'old style' extensibility api to the new one */
	l = NULL;
	have = g_hash_table_new(g_str_hash, g_str_equal);
	index = 20;
	for (prov=gui->providers;prov;prov=g_list_next(prov)) {
		CamelProviderConfEntry *entries = ((CamelProvider *)prov->data)->extra_conf;

		for (i=0;entries && entries[i].type != CAMEL_PROVIDER_CONF_END;i++) {
			struct _receive_options_item *item;
			char *name = entries[i].name;
			int myindex = index;

			if (entries[i].type != CAMEL_PROVIDER_CONF_SECTION_START
			    || name == NULL
			    || g_hash_table_lookup(have, name))
				continue;

			/* override mailcheck since we also insert our own mailcheck item at this index */
			if (name && !strcmp(name, "mailcheck"))
				myindex = 10;

			item = g_malloc0(sizeof(*item));
			item->item.type = E_CONFIG_SECTION_TABLE;
			item->item.path = g_strdup_printf("20.receive_options/%02d.%s", myindex, name?name:"unnamed");
			item->item.label = entries[i].text;

			l = g_slist_prepend(l, item);

			item = g_malloc0(sizeof(*item));
			item->item.type = E_CONFIG_ITEM_TABLE;
			item->item.path = g_strdup_printf("20.receive_options/%02d.%s/80.camelitem", myindex, name?name:"unnamed");
			item->item.factory = emae_receive_options_extra_item;
			item->item.user_data = entries[i].name;

			l = g_slist_prepend(l, item);

			index += 10;
			g_hash_table_insert(have, entries[i].name, have);
		}
	}
	g_hash_table_destroy(have);
	e_config_add_items((EConfig *)ec, l, NULL, NULL, emae_free_auto, emae);
	gui->extra_items = l;

	e_config_add_page_check((EConfig *)ec, NULL, emae_check_complete, emae);

	target = em_config_target_new_account(ec, emae->account);
	e_config_set_target((EConfig *)ec, (EConfigTarget *)target);
	emae->editor = e_config_create_window((EConfig *)ec, NULL, type==EMAE_NOTEBOOK?_("Account Editor"):_("Evolution Account Assistant"));

	g_object_ref(emae);
	g_signal_connect(emae->editor, "destroy", G_CALLBACK(emae_editor_destroyed), emae);
}
