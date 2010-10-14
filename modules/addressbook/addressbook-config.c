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
 *		Chris Toshok <toshok@ximian.com>
 *		Chris Lahey <clahey@ximian.com>
 *		Michael Zucchi <notzed@ximian.com>
 *		And no doubt others ...
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*#define STANDALONE*/

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#ifdef G_OS_WIN32
/* Include <windows.h> early and work around DATADIR lossage */
#define DATADIR crap_DATADIR
#include <windows.h>
#undef DATADIR
#endif

#include "addressbook.h"
#include "addressbook-config.h"

#include "e-util/e-util.h"
#include "e-util/e-alert-dialog.h"
#include "e-util/e-util-private.h"

#include "addressbook/gui/widgets/eab-config.h"

#define d(x)

#ifdef HAVE_LDAP
#ifndef G_OS_WIN32
#include <ldap.h>
#ifndef SUNLDAP
#include <ldap_schema.h>
#endif
#else
#include <winldap.h>
#include "openldap-extract.h"
#endif
#endif

#define LDAP_PORT_STRING "389"
#define LDAPS_PORT_STRING "636"

/* default objectclasses */
#define TOP                  "top"
#define PERSON               "person"
#define ORGANIZATIONALPERSON "organizationalPerson"
#define INETORGPERSON        "inetOrgPerson"
#define EVOLUTIONPERSON      "evolutionPerson"
#define CALENTRY             "calEntry"


typedef struct _AddressbookSourceDialog AddressbookSourceDialog;

struct _AddressbookSourceDialog {
	GtkBuilder *builder;

	EABConfig *config;	/* the config manager */

	GtkWidget *window;

	/* Source selection (assistant only) */
	ESourceList *source_list;
	GSList *menu_source_groups;

	/* ESource we're currently editing */
	ESource *source;
	/* The original source in edit mode.  Also used to flag when we are in edit mode. */
	ESource *original_source;

	/* Source group we're creating/editing a source in */
	ESourceGroup *source_group;

	/* info page fields */
	GtkWidget *host;
	GtkWidget *auth_combobox;
	AddressbookLDAPAuthType auth;
	GtkWidget *auth_principal;

	/* connecting page fields */
	GtkWidget *port_comboentry;
	GtkWidget *ssl_combobox;
	AddressbookLDAPSSLType ssl;

	/* searching page fields */
	GtkWidget *rootdn;
	AddressbookLDAPScopeType scope;
	GtkWidget *scope_combobox;
	GtkWidget *search_filter;
	GtkWidget *timeout_scale;
	GtkWidget *limit_spinbutton;
	GtkWidget *canbrowsecheck;

	/* display name page fields */
	GtkWidget *display_name;
};



#ifdef HAVE_LDAP

static const gchar *
ldap_unparse_auth (AddressbookLDAPAuthType auth_type)
{
	switch (auth_type) {
	case ADDRESSBOOK_LDAP_AUTH_NONE:
		return "none";
	case ADDRESSBOOK_LDAP_AUTH_SIMPLE_EMAIL:
		return "ldap/simple-email";
	case ADDRESSBOOK_LDAP_AUTH_SIMPLE_BINDDN:
		return "ldap/simple-binddn";
	default:
		g_return_val_if_reached ("none");
	}
}

static AddressbookLDAPAuthType
ldap_parse_auth (const gchar *auth)
{
	if (!auth)
		return ADDRESSBOOK_LDAP_AUTH_NONE;

	if (!strcmp (auth, "ldap/simple-email") || !strcmp (auth, "simple"))
		return ADDRESSBOOK_LDAP_AUTH_SIMPLE_EMAIL;
	else if (!strcmp (auth, "ldap/simple-binddn"))
		return ADDRESSBOOK_LDAP_AUTH_SIMPLE_BINDDN;
	else
		return ADDRESSBOOK_LDAP_AUTH_NONE;
}

static const gchar *
ldap_unparse_scope (AddressbookLDAPScopeType scope_type)
{
	switch (scope_type) {
	case ADDRESSBOOK_LDAP_SCOPE_BASE:
		return "base";
	case ADDRESSBOOK_LDAP_SCOPE_ONELEVEL:
		return "one";
	case ADDRESSBOOK_LDAP_SCOPE_SUBTREE:
		return "sub";
	default:
		g_return_val_if_reached ("");
	}
}

static const gchar *
ldap_unparse_ssl (AddressbookLDAPSSLType ssl_type)
{
	switch (ssl_type) {
	case ADDRESSBOOK_LDAP_SSL_NEVER:
		return "never";
	case ADDRESSBOOK_LDAP_SSL_WHENEVER_POSSIBLE:
		return "whenever_possible";
	case ADDRESSBOOK_LDAP_SSL_ALWAYS:
		return "always";
	default:
		g_return_val_if_reached ("");
	}
}

static AddressbookLDAPSSLType
ldap_parse_ssl (const gchar *ssl)
{
	if (!ssl)
		return ADDRESSBOOK_LDAP_SSL_WHENEVER_POSSIBLE;

	if (!strcmp (ssl, "always"))
		return ADDRESSBOOK_LDAP_SSL_ALWAYS;
	else if (!strcmp (ssl, "whenever_possible"))
		return ADDRESSBOOK_LDAP_SSL_WHENEVER_POSSIBLE;
	else
		return ADDRESSBOOK_LDAP_SSL_NEVER;
}

static const gchar *
ldap_get_ssl_tooltip (AddressbookLDAPSSLType ssl_type)
{
	switch (ssl_type) {
	case ADDRESSBOOK_LDAP_SSL_ALWAYS:
		return _("Selecting this option means that Evolution will only connect to your LDAP server if your LDAP server supports SSL.");
	case ADDRESSBOOK_LDAP_SSL_WHENEVER_POSSIBLE:
		return _("Selecting this option means that Evolution will only connect to your LDAP server if your LDAP server supports TLS.");
	case ADDRESSBOOK_LDAP_SSL_NEVER:
		return _("Selecting this option means that your server does not support either SSL or TLS. This means that your connection will be insecure, and that you will be vulnerable to security exploits.");
	}

	return NULL;
}

static gboolean
source_to_uri_parts (ESource *source, gchar **host, gchar **rootdn, AddressbookLDAPScopeType *scope, gchar **search_filter, gint *port)
{
	gchar       *uri;
	LDAPURLDesc *lud;
	gint         ldap_error;

	g_return_val_if_fail (source, FALSE);

	uri = e_source_get_uri (source);
	ldap_error = ldap_url_parse ((gchar *) uri, &lud);
	g_free (uri);

	if (ldap_error != LDAP_SUCCESS)
		return FALSE;

	if (host)
		*host = g_strdup (lud->lud_host ? lud->lud_host : "");
	if (rootdn)
		*rootdn = g_strdup (lud->lud_dn ? lud->lud_dn : "");
	if (port)
		*port = lud->lud_port ? lud->lud_port : LDAP_PORT;
	if (scope)
		*scope = lud->lud_scope == LDAP_SCOPE_BASE     ? ADDRESSBOOK_LDAP_SCOPE_BASE :
			 lud->lud_scope == LDAP_SCOPE_ONELEVEL ? ADDRESSBOOK_LDAP_SCOPE_ONELEVEL :
			 lud->lud_scope == LDAP_SCOPE_SUBTREE  ? ADDRESSBOOK_LDAP_SCOPE_SUBTREE :
			 ADDRESSBOOK_LDAP_SCOPE_ONELEVEL;
	if (search_filter && lud->lud_filter)
		*search_filter = g_strdup (lud->lud_filter);

	ldap_free_urldesc (lud);
	return TRUE;
}

static gboolean
source_group_is_remote (ESourceGroup *group)
{
	return strncmp ("ldap:", e_source_group_peek_base_uri (group), 5) == 0;
}

/* ldap api foo */
static LDAP *
addressbook_ldap_init (GtkWidget *window, ESource *source)
{
	LDAP  *ldap;
	gchar *host;
	gint   port;
	gint ldap_error;
	gint protocol_version = LDAP_VERSION3;

	if (!source_to_uri_parts (source, &host, NULL, NULL, NULL, &port))
		return NULL;

	if (!(ldap = ldap_init (host, port))) {
		e_alert_run_dialog_for_args ((GtkWindow *) window, "addressbook:ldap-init", NULL);
		goto done;
	}

	ldap_error = ldap_set_option (ldap, LDAP_OPT_PROTOCOL_VERSION, &protocol_version);
	if (LDAP_SUCCESS != ldap_error)
		g_warning ("failed to set protocol version to LDAPv3");

	/* XXX do TLS if it's configured in */

 done:
	g_free (host);
	return ldap;
}

static gint
addressbook_ldap_auth (GtkWidget *window, LDAP *ldap)
{
	gint ldap_error;

	/* XXX use auth info from source */
	ldap_error = ldap_simple_bind_s (ldap, NULL, NULL);
	if (LDAP_SUCCESS != ldap_error)
		e_alert_run_dialog_for_args ((GtkWindow *) window, "addressbook:ldap-auth", NULL);

	return ldap_error;
}

static gint
addressbook_root_dse_query (AddressbookSourceDialog *dialog, LDAP *ldap,
			    const gchar **attrs, LDAPMessage **resp)
{
	GtkAdjustment *adjustment;
	GtkRange *range;
	gint ldap_error;
	struct timeval timeout;

	range = GTK_RANGE (dialog->timeout_scale);
	adjustment = gtk_range_get_adjustment (range);

	timeout.tv_sec = (gint) gtk_adjustment_get_value (adjustment);
	timeout.tv_usec = 0;

	ldap_error = ldap_search_ext_s (ldap,
					LDAP_ROOT_DSE, LDAP_SCOPE_BASE,
					"(objectclass=*)",
					(gchar **) attrs, 0, NULL, NULL, &timeout, LDAP_NO_LIMIT, resp);
	if (LDAP_SUCCESS != ldap_error)
		e_alert_run_dialog_for_args (GTK_WINDOW (dialog->window), "addressbook:ldap-search-base", NULL);

	return ldap_error;
}

static gboolean
do_ldap_root_dse_query (AddressbookSourceDialog *sdialog, GtkListStore *model, ESource *source)
{
	LDAP *ldap;
	const gchar *attrs[2];
	gint ldap_error;
	gchar **values;
	LDAPMessage *resp;
	gint i;

	ldap = addressbook_ldap_init (sdialog->window, source);
	if (!ldap)
		return FALSE;

	if (LDAP_SUCCESS != addressbook_ldap_auth (sdialog->window, ldap))
		goto fail;

	attrs[0] = "namingContexts";
	attrs[1] = NULL;

	ldap_error = addressbook_root_dse_query (sdialog, ldap, attrs, &resp);

	if (ldap_error != LDAP_SUCCESS)
		goto fail;

	values = ldap_get_values (ldap, resp, "namingContexts");
	if (!values || values[0] == NULL || strlen (values[0]) == 0) {
		e_alert_run_dialog_for_args (GTK_WINDOW (sdialog->window), "addressbook:ldap-search-base", NULL);
		goto fail;
	}

	for (i = 0; values[i]; i++) {
		GtkTreeIter iter;

		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter, 0, values[i], -1);
	}

	ldap_value_free (values);
	ldap_unbind_s (ldap);
	return TRUE;

 fail:
	ldap_unbind_s (ldap);
	return FALSE;
}

static void
search_base_selection_model_changed (GtkTreeSelection *selection, GtkWidget *dialog)
{
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
					   GTK_RESPONSE_OK,
					   gtk_tree_selection_get_selected (selection, NULL, NULL));
}

static void
query_for_supported_bases (GtkWidget *button, AddressbookSourceDialog *sdialog)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkWidget *dialog;
	GtkWidget *container;
	GtkWidget *supported_bases_table;
	GtkBuilder *builder;
	GtkTreeIter iter;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "ldap-config.ui");

	dialog = e_builder_get_widget (builder, "supported-bases-dialog");

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (sdialog->window));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	gtk_widget_ensure_style (dialog);

	container = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
	gtk_container_set_border_width (GTK_CONTAINER (container), 12);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_set_border_width (GTK_CONTAINER (container), 0);

	supported_bases_table = e_builder_get_widget (builder, "supported-bases-table");
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (supported_bases_table));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (supported_bases_table));
	g_signal_connect (selection, "changed", G_CALLBACK (search_base_selection_model_changed), dialog);
	search_base_selection_model_changed (selection, dialog);

	if (do_ldap_root_dse_query (sdialog, GTK_LIST_STORE (model), sdialog->source)) {
		gtk_widget_show (dialog);

		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK
		    && gtk_tree_selection_get_selected (selection, &model, &iter)) {
			gchar *dn;

			gtk_tree_model_get (model, &iter, 0, &dn, -1);
			gtk_entry_set_text ((GtkEntry *)sdialog->rootdn, dn);
			g_free (dn);
		}
	}

	gtk_widget_destroy (dialog);
}

#endif /* HAVE_LDAP */

GtkWidget*
addressbook_config_create_new_source (GtkWidget *parent)
{
	return addressbook_config_edit_source (parent, NULL);
}

/* ********************************************************************** */

static void
eabc_type_changed (GtkComboBox *dropdown, AddressbookSourceDialog *sdialog)
{
	gint id = gtk_combo_box_get_active (dropdown);
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model (dropdown);
	if (id == -1 || !gtk_tree_model_iter_nth_child (model, &iter, NULL, id))
		return;

	/* TODO: when we change the group type, we lose all of the pre-filled dialog info */

	gtk_tree_model_get (model, &iter, 1, &sdialog->source_group, -1);
	/* HACK: doesn't work if you don't do this */
	e_source_set_absolute_uri (sdialog->source, NULL);
	e_source_set_group (sdialog->source, sdialog->source_group);

	/* BIG HACK: We load the defaults for each type here.
	   I guess plugins will have to use the do it in their factory callbacks */
	if (!strncmp(e_source_group_peek_base_uri(sdialog->source_group), "groupwise:", 10)) {
		GSList *l;
		ESource *source;
		gchar *tmp;

		l = e_source_group_peek_sources (sdialog->source_group);
		if (l && l->data ) {
			source = l->data;
			e_source_set_property(sdialog->source, "auth", e_source_get_property(source, "auth"));
			e_source_set_property(sdialog->source, "user", e_source_get_property(source, "user"));
			e_source_set_property(sdialog->source, "user_ssl", e_source_get_property(source, "use_ssl"));
		}

		e_source_set_property(sdialog->source, "auth-domain", "Groupwise");
		tmp = g_strconcat (";", e_source_peek_name(sdialog->source), NULL);
		e_source_set_relative_uri (sdialog->source, tmp);
		g_free (tmp);
#ifdef HAVE_LDAP
	} else if (!strncmp(e_source_group_peek_base_uri(sdialog->source_group), "ldap:", 5)) {
		gchar *tmp;

		tmp = g_strdup_printf ("%s:%s/%s?" /* trigraph prevention */ "?%s",
				       "", LDAP_PORT_STRING,
				       "",
				       "one");
		e_source_set_relative_uri (sdialog->source, tmp);
		g_free (tmp);
		e_source_set_property(sdialog->source, "timeout", "3");
		e_source_set_property(sdialog->source, "limit", "100");
#endif
	} else {
		e_source_set_relative_uri (sdialog->source, e_source_peek_uid (sdialog->source));
	}

	e_config_target_changed ((EConfig *)sdialog->config, E_CONFIG_TARGET_CHANGED_REBUILD);
}

static GtkWidget *
eabc_general_type (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	AddressbookSourceDialog *sdialog = data;
	GtkComboBox *dropdown;
	GtkCellRenderer *cell;
	GtkListStore *store;
	GtkTreeIter iter;
	GSList *l;
	GtkWidget *w, *label;
	gint i, row = 0;

	if (old)
		return old;

	w = gtk_hbox_new (FALSE, 6);
	label = gtk_label_new_with_mnemonic(_("_Type:"));
	gtk_box_pack_start ((GtkBox *)w, label, FALSE, FALSE, 0);

	dropdown = (GtkComboBox *)gtk_combo_box_new ();
	cell = gtk_cell_renderer_text_new ();
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
	i = 0;
	for (l=sdialog->menu_source_groups;l;l=g_slist_next (l)) {
		ESourceGroup *group = l->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, e_source_group_peek_name (group), 1, group, -1);
		if (e_source_peek_group (sdialog->source) == group)
			row = i;
		i++;
	}

	gtk_cell_layout_pack_start ((GtkCellLayout *)dropdown, cell, TRUE);
	gtk_cell_layout_set_attributes((GtkCellLayout *)dropdown, cell, "text", 0, NULL);
	gtk_combo_box_set_model (dropdown, (GtkTreeModel *)store);
	gtk_combo_box_set_active (dropdown, -1);
	gtk_combo_box_set_active (dropdown, row);
	g_signal_connect(dropdown, "changed", G_CALLBACK(eabc_type_changed), sdialog);
	gtk_widget_show ((GtkWidget *)dropdown);
	gtk_box_pack_start ((GtkBox *)w, (GtkWidget *)dropdown, TRUE, TRUE, 0);
	gtk_label_set_mnemonic_widget ((GtkLabel *)label, (GtkWidget *)dropdown);

	gtk_box_pack_start ((GtkBox *)parent, (GtkWidget *)w, FALSE, FALSE, 0);

	gtk_widget_show_all (w);

	return (GtkWidget *)w;
}

static void
name_changed_cb (GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	const gchar *text;
	gchar *stripped_name;

	text = gtk_entry_get_text (GTK_ENTRY (sdialog->display_name));

	stripped_name = g_strstrip (g_strdup (text));
	e_source_set_name (sdialog->source, stripped_name);
	g_free (stripped_name);
}

static GtkWidget *
eabc_general_name (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	AddressbookSourceDialog *sdialog = data;
	const gchar *uri;
	GtkWidget *w;
	GtkBuilder *builder;

	if (old)
		return old;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "ldap-config.ui");

	w = e_builder_get_widget (builder, item->label);
	gtk_box_pack_start ((GtkBox *)parent, w, FALSE, FALSE, 0);

	sdialog->display_name = e_builder_get_widget (builder, "account-editor-display-name-entry");
	g_signal_connect(sdialog->display_name, "changed", G_CALLBACK(name_changed_cb), sdialog);
	gtk_entry_set_text ((GtkEntry *)sdialog->display_name, e_source_peek_name (sdialog->source));

	/* Hardcoded: groupwise can't edit the name (or anything else) */
	if (sdialog->original_source) {
		uri = e_source_group_peek_base_uri (sdialog->source_group);
		if (uri && strncmp(uri, "groupwise:", 10) == 0) {
			gtk_widget_set_sensitive (GTK_WIDGET (sdialog->display_name), FALSE);
		}
	}

	g_object_unref (builder);

	return w;
}

/* TODO: This should be moved to plugins if B&A calendar setup is moved there */
static void
use_in_cal_changed_cb (GtkWidget *widget, AddressbookSourceDialog *sdialog)
{
	e_source_set_property (sdialog->source, "use-in-contacts-calendar", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)) ? "1" : "0");
}

static GtkWidget *
eabc_general_use_in_cal (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	AddressbookSourceDialog *sdialog = data;
	GtkWidget *use_in_cal_setting;
	const gchar *use_in_cal, *base_uri = NULL;
	ESourceGroup *group;

	if (old)
		return old;

	use_in_cal_setting = gtk_check_button_new_with_mnemonic (_("U_se in Birthday & Anniversaries calendar"));
	gtk_widget_show (use_in_cal_setting);
	gtk_container_add (GTK_CONTAINER (parent), use_in_cal_setting);

	use_in_cal =  e_source_get_property (sdialog->source, "use-in-contacts-calendar");
	group = e_source_peek_group (sdialog->source);

	if (group)
		base_uri = e_source_group_peek_base_uri (group);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (use_in_cal_setting), (use_in_cal && g_str_equal (use_in_cal, "1")) || (!use_in_cal && base_uri && g_str_has_prefix (base_uri, "local:")));

	g_signal_connect (use_in_cal_setting, "toggled", G_CALLBACK (use_in_cal_changed_cb), sdialog);

	return use_in_cal_setting;
}

static void
offline_status_changed_cb (GtkWidget *widget, AddressbookSourceDialog *sdialog)
{
	e_source_set_property (sdialog->source, "offline_sync", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)) ? "1" : "0");
}

static GtkWidget *
eabc_general_offline (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	AddressbookSourceDialog *sdialog = data;
	GtkWidget *offline_setting;
	const gchar *offline_sync;
	gboolean is_local_book;

	is_local_book = g_str_has_prefix (e_source_group_peek_base_uri (sdialog->source_group), "local:");
	offline_sync =  e_source_get_property (sdialog->source, "offline_sync");
	if (old)
		return old;
	else {
		offline_setting = gtk_check_button_new_with_mnemonic (_("Copy _book content locally for offline operation"));
		gtk_widget_show (offline_setting);
		gtk_container_add (GTK_CONTAINER (parent), offline_setting);
		g_signal_connect (offline_setting, "toggled", G_CALLBACK (offline_status_changed_cb), sdialog);

	}
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (offline_setting), (offline_sync && g_str_equal (offline_sync, "1"))  ? TRUE : FALSE);
	if (is_local_book)
		gtk_widget_hide (offline_setting);
	return offline_setting;

}

#ifdef HAVE_LDAP
static gchar *
form_ldap_search_filter (GtkWidget *w)
{
	gchar *filter;
	const gchar *search_filter = gtk_entry_get_text ((GtkEntry *) w);

	/* this function can be used to format the search filter entered */
	if ((strlen (search_filter) !=0) && *search_filter != '(' && *(search_filter + (strlen (search_filter-1))) != ')')
		filter = g_strdup_printf ("(%s)", search_filter);
	else
		filter = g_strdup_printf ("%s", search_filter);

	return filter;
}

static void
url_changed (AddressbookSourceDialog *sdialog)
{
	gchar *str, *search_filter;

	search_filter = form_ldap_search_filter (sdialog->search_filter);
	str = g_strdup_printf ("%s:%s/%s?" /* trigraph prevention */ "?%s?%s",
			       gtk_entry_get_text (GTK_ENTRY (sdialog->host)),
			       gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (sdialog->port_comboentry)))),
			       gtk_entry_get_text (GTK_ENTRY (sdialog->rootdn)),
			       ldap_unparse_scope (sdialog->scope),
			       search_filter);
	e_source_set_relative_uri (sdialog->source, str);
	g_free (search_filter);
	g_free (str);
}

static void
host_changed_cb (GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	url_changed (sdialog);
}

static void
port_entry_changed_cb (GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	const gchar *port = gtk_entry_get_text ((GtkEntry *)w);

	if (!strcmp (port, LDAPS_PORT_STRING)) {
		sdialog->ssl = ADDRESSBOOK_LDAP_SSL_ALWAYS;
		gtk_combo_box_set_active (GTK_COMBO_BOX (sdialog->ssl_combobox), sdialog->ssl);
		gtk_widget_set_sensitive (sdialog->ssl_combobox, FALSE);
	} else {
		gtk_widget_set_sensitive (sdialog->ssl_combobox, TRUE);
	}

	url_changed (sdialog);
}

static void
ssl_combobox_changed_cb (GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	sdialog->ssl = gtk_combo_box_get_active (GTK_COMBO_BOX (w));
	e_source_set_property (sdialog->source, "ssl", ldap_unparse_ssl (sdialog->ssl));

	gtk_widget_set_tooltip_text (sdialog->ssl_combobox, ldap_get_ssl_tooltip (sdialog->ssl));
}

static GtkWidget *
eabc_general_host (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	AddressbookSourceDialog *sdialog = data;
	const gchar *tmp;
	GtkWidget *w;
	gchar *uri, port[16];
	LDAPURLDesc *lud;
	GtkBuilder *builder;

	if (!source_group_is_remote (sdialog->source_group))
		return NULL;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "ldap-config.ui");

	w = e_builder_get_widget (builder, item->label);
	gtk_box_pack_start ((GtkBox *)parent, w, FALSE, FALSE, 0);

	uri = e_source_get_uri (sdialog->source);
	if (ldap_url_parse (uri, &lud) != LDAP_SUCCESS)
		lud = NULL;
	g_free (uri);

	sdialog->host = e_builder_get_widget (builder, "server-name-entry");
	gtk_entry_set_text((GtkEntry *)sdialog->host, lud && lud->lud_host ? lud->lud_host : "");
	g_signal_connect (sdialog->host, "changed", G_CALLBACK (host_changed_cb), sdialog);

	sdialog->port_comboentry = e_builder_get_widget (builder, "port-comboentry");
	gtk_combo_box_entry_set_text_column (GTK_COMBO_BOX_ENTRY (sdialog->port_comboentry), 0);
	gtk_widget_set_has_tooltip (sdialog->port_comboentry, TRUE);
	gtk_widget_set_tooltip_text (sdialog->port_comboentry, _("This is the port on the LDAP server that Evolution will try to connect to. A list of standard ports has been provided. Ask your system administrator what port you should specify."));
	sprintf(port, "%u", lud && lud->lud_port? lud->lud_port : LDAP_PORT);
	gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (sdialog->port_comboentry))), port);
	g_signal_connect (gtk_bin_get_child (GTK_BIN (sdialog->port_comboentry)), "changed", G_CALLBACK (port_entry_changed_cb), sdialog);

	if (lud)
		ldap_free_urldesc (lud);

	sdialog->ssl_combobox = e_builder_get_widget (builder, "ssl-combobox");
	gtk_widget_set_has_tooltip (sdialog->ssl_combobox, TRUE);
	tmp = e_source_get_property (sdialog->source, "ssl");
	sdialog->ssl = ldap_parse_ssl (tmp);
	gtk_combo_box_set_active (GTK_COMBO_BOX (sdialog->ssl_combobox), sdialog->ssl);
	gtk_widget_set_tooltip_text (sdialog->ssl_combobox, ldap_get_ssl_tooltip (sdialog->ssl));
	gtk_widget_set_sensitive (sdialog->ssl_combobox, strcmp (port, LDAPS_PORT_STRING) != 0);
	g_signal_connect (sdialog->ssl_combobox, "changed", G_CALLBACK (ssl_combobox_changed_cb), sdialog);

	g_object_unref (builder);

	return w;
}

static void
auth_entry_changed_cb (GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	const gchar *principal = gtk_entry_get_text ((GtkEntry *)w);

	/* seems messy ... but the api is */
	switch (sdialog->auth) {
	case ADDRESSBOOK_LDAP_AUTH_SIMPLE_BINDDN:
		e_source_set_property(sdialog->source, "email_addr", NULL);
		e_source_set_property(sdialog->source, "binddn", principal);
		break;
	case ADDRESSBOOK_LDAP_AUTH_SIMPLE_EMAIL:
		e_source_set_property(sdialog->source, "binddn", NULL);
		e_source_set_property(sdialog->source, "email_addr", principal);
		break;
	case ADDRESSBOOK_LDAP_AUTH_NONE:
	default:
		e_source_set_property(sdialog->source, "email_addr", NULL);
		e_source_set_property(sdialog->source, "binddn", NULL);
		break;
	}
}

static void
auth_combobox_changed_cb (GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	sdialog->auth = gtk_combo_box_get_active (GTK_COMBO_BOX (w));
	e_source_set_property (sdialog->source, "auth", ldap_unparse_auth (sdialog->auth));

	/* make sure the right property is set for the auth - ugh, funny api */
	auth_entry_changed_cb (sdialog->auth_principal, sdialog);
}

static GtkWidget *
eabc_general_auth (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	AddressbookSourceDialog *sdialog = data;
	GtkWidget *w;
	const gchar *tmp;
	GtkBuilder *builder;

	if (!source_group_is_remote (sdialog->source_group))
		return NULL;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "ldap-config.ui");

	w = e_builder_get_widget (builder, item->label);
	gtk_box_pack_start ((GtkBox *)parent, w, FALSE, FALSE, 0);

	sdialog->auth_combobox = e_builder_get_widget (builder, "auth-combobox");
	gtk_widget_set_has_tooltip (sdialog->auth_combobox, TRUE);
	gtk_widget_set_tooltip_text (sdialog->auth_combobox, _("This is the method Evolution will use to authenticate you.  Note that setting this to \"Email Address\" requires anonymous access to your LDAP server."));
	tmp = e_source_get_property(sdialog->source, "auth");
	sdialog->auth = tmp ? ldap_parse_auth (tmp) : ADDRESSBOOK_LDAP_AUTH_NONE;
	gtk_combo_box_set_active (GTK_COMBO_BOX (sdialog->auth_combobox), sdialog->auth);
	g_signal_connect (sdialog->auth_combobox, "changed", G_CALLBACK(auth_combobox_changed_cb), sdialog);

	sdialog->auth_principal = e_builder_get_widget (builder, "auth-entry");
	switch (sdialog->auth) {
	case ADDRESSBOOK_LDAP_AUTH_SIMPLE_EMAIL:
		tmp = e_source_get_property(sdialog->source, "email_addr");
		break;
	case ADDRESSBOOK_LDAP_AUTH_SIMPLE_BINDDN:
		tmp = e_source_get_property(sdialog->source, "binddn");
		break;
	case ADDRESSBOOK_LDAP_AUTH_NONE:
	default:
		tmp = "";
		break;
	}
	gtk_entry_set_text((GtkEntry *)sdialog->auth_principal, tmp?tmp:"");
	g_signal_connect (sdialog->auth_principal, "changed", G_CALLBACK (auth_entry_changed_cb), sdialog);

	g_object_unref (builder);

	return w;
}

static void
rootdn_changed_cb (GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	url_changed (sdialog);
}

static void
search_filter_changed_cb (GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	url_changed (sdialog);
}

static void
scope_combobox_changed_cb (GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	sdialog->scope = gtk_combo_box_get_active (GTK_COMBO_BOX (w));
	url_changed (sdialog);
}

static GtkWidget *
eabc_details_search (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	AddressbookSourceDialog *sdialog = data;
	GtkWidget *w;
	LDAPURLDesc *lud;
	gchar *uri;
	GtkBuilder *builder;

	if (!source_group_is_remote (sdialog->source_group))
		return NULL;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "ldap-config.ui");

	w = e_builder_get_widget (builder, item->label);
	gtk_box_pack_start ((GtkBox *)parent, w, FALSE, FALSE, 0);

	uri = e_source_get_uri (sdialog->source);
	if (ldap_url_parse (uri, &lud) != LDAP_SUCCESS)
		lud = NULL;
	g_free (uri);

	sdialog->rootdn = e_builder_get_widget (builder, "rootdn-entry");
	gtk_entry_set_text((GtkEntry *)sdialog->rootdn, lud && lud->lud_dn ? lud->lud_dn : "");
	g_signal_connect (sdialog->rootdn, "changed", G_CALLBACK (rootdn_changed_cb), sdialog);

	sdialog->scope_combobox = e_builder_get_widget (builder, "scope-combobox");
	gtk_widget_set_has_tooltip (sdialog->scope_combobox, TRUE);
	gtk_widget_set_tooltip_text (sdialog->scope_combobox, _("The search scope defines how deep you would like the search to extend down the directory tree. A search scope of \"sub\" will include all entries below your search base. A search scope of \"one\" will only include the entries one level beneath your base."));
	if (lud) {
		switch (lud->lud_scope) {
		case LDAP_SCOPE_BASE:
			sdialog->scope = ADDRESSBOOK_LDAP_SCOPE_BASE;
			break;
		default:
		case LDAP_SCOPE_ONELEVEL:
			sdialog->scope = ADDRESSBOOK_LDAP_SCOPE_ONELEVEL;
			break;
		case LDAP_SCOPE_SUBTREE:
			sdialog->scope = ADDRESSBOOK_LDAP_SCOPE_SUBTREE;
			break;
		}
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (sdialog->scope_combobox), sdialog->scope);
	g_signal_connect (sdialog->scope_combobox, "changed", G_CALLBACK(scope_combobox_changed_cb), sdialog);

	sdialog->search_filter =  e_builder_get_widget (builder, "search-filter-entry");
	gtk_entry_set_text((GtkEntry *)sdialog->search_filter, lud && lud->lud_filter ? lud->lud_filter : "");
	g_signal_connect (sdialog->search_filter, "changed",  G_CALLBACK (search_filter_changed_cb), sdialog);

	g_signal_connect (e_builder_get_widget(builder, "rootdn-button"), "clicked",
			  G_CALLBACK (query_for_supported_bases), sdialog);

	if (lud)
		ldap_free_urldesc (lud);

	g_object_unref (builder);

	return w;
}

static void
timeout_changed_cb (GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	GtkAdjustment *adjustment;
	GtkRange *range;
	gchar *timeout;

	range = GTK_RANGE (sdialog->timeout_scale);
	adjustment = gtk_range_get_adjustment (range);
	timeout = g_strdup_printf("%f", gtk_adjustment_get_value (adjustment));
	e_source_set_property(sdialog->source, "timeout", timeout);
	g_free (timeout);
}

static void
limit_changed_cb (GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	gchar limit[16];

	sprintf(limit, "%d", gtk_spin_button_get_value_as_int((GtkSpinButton *)sdialog->limit_spinbutton));
	e_source_set_property(sdialog->source, "limit", limit);
}

static void
canbrowse_toggled_cb (GtkWidget *toggle_button, ESource *source)
{
	if (!source || !toggle_button)
		return;

	e_source_set_property (source, "can-browse", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle_button)) ? "1" : NULL);
}

static GtkWidget *
eabc_details_limit (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	AddressbookSourceDialog *sdialog = data;
	GtkAdjustment *adjustment;
	GtkRange *range;
	GtkWidget *w;
	const gchar *tmp;
	GtkBuilder *builder;

	if (!source_group_is_remote (sdialog->source_group))
		return NULL;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "ldap-config.ui");

	w = e_builder_get_widget (builder, item->label);
	gtk_box_pack_start ((GtkBox *)parent, w, FALSE, FALSE, 0);

	sdialog->timeout_scale = e_builder_get_widget (builder, "timeout-scale");
	range = GTK_RANGE (sdialog->timeout_scale);
	adjustment = gtk_range_get_adjustment (range);
	tmp = e_source_get_property(sdialog->source, "timeout");
	gtk_adjustment_set_value (adjustment, tmp?g_strtod (tmp, NULL):3.0);
	g_signal_connect (
		adjustment, "value_changed",
		G_CALLBACK (timeout_changed_cb), sdialog);

	sdialog->limit_spinbutton = e_builder_get_widget (builder, "download-limit-spinbutton");
	tmp = e_source_get_property(sdialog->source, "limit");
	gtk_spin_button_set_value ((GtkSpinButton *)sdialog->limit_spinbutton, tmp?g_strtod (tmp, NULL):100.0);
	g_signal_connect (sdialog->limit_spinbutton, "value_changed", G_CALLBACK (limit_changed_cb), sdialog);

	sdialog->canbrowsecheck = e_builder_get_widget (builder, "canbrowsecheck");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sdialog->canbrowsecheck), e_source_get_property (sdialog->source, "can-browse") && strcmp (e_source_get_property (sdialog->source, "can-browse"), "1") == 0);
	g_signal_connect (sdialog->canbrowsecheck, "toggled", G_CALLBACK (canbrowse_toggled_cb), sdialog->source);

	g_object_unref (builder);

	return w;
}
#endif

static EConfigItem eabc_items[] = {
	{ E_CONFIG_BOOK, (gchar *) (gchar *) "", },
	{ E_CONFIG_PAGE, (gchar *) "00.general", (gchar *) N_("General") },
	{ E_CONFIG_SECTION, (gchar *) "00.general/10.display", (gchar *) N_("Address Book") },
	{ E_CONFIG_ITEM, (gchar *) "00.general/10.display/10.name", (gchar *) "hbox122", eabc_general_name },
	{ E_CONFIG_ITEM, (gchar *) "00.general/10.display/20.calendar", NULL, eabc_general_use_in_cal },
	{ E_CONFIG_ITEM, (gchar *) "00.general/10.display/30.offline", NULL, eabc_general_offline },
#ifdef HAVE_LDAP
	{ E_CONFIG_SECTION, (gchar *) "00.general/20.server", (gchar *) N_("Server Information") },
	{ E_CONFIG_ITEM, (gchar *) "00.general/20.server/00.host", (gchar *) "table31", eabc_general_host },
	{ E_CONFIG_SECTION, (gchar *) "00.general/30.auth", (gchar *) N_("Authentication") },
	{ E_CONFIG_ITEM, (gchar *) "00.general/30.auth/00.auth", (gchar *) "table32", eabc_general_auth },

	{ E_CONFIG_PAGE, (gchar *) "10.details", (gchar *) N_("Details") },
	{ E_CONFIG_SECTION, (gchar *) "10.details/00.search", (gchar *) N_("Searching") },
	{ E_CONFIG_ITEM, (gchar *) "10.details/00.search/00.search", (gchar *) "table33", eabc_details_search },
	{ E_CONFIG_SECTION, (gchar *) "10.details/10.limit", (gchar *) N_("Downloading") },
	{ E_CONFIG_ITEM, (gchar *) "10.details/10.limit/00.limit", (gchar *) "table34", eabc_details_limit },
#endif
	{ 0 },
};

/* items needed for the 'new addressbook' window */
static EConfigItem eabc_new_items[] = {
	{ E_CONFIG_ITEM, (gchar *) "00.general/10.display/00.type", NULL, eabc_general_type },
	{ 0 },
};

static void
eabc_commit (EConfig *ec, GSList *items, gpointer data)
{
	AddressbookSourceDialog *sdialog = data;
	xmlNodePtr xml;
#if d(!)0
	gchar *txt;
#endif
	if (sdialog->original_source) {
		d(printf("committing addressbook changes\n"));

		/* these api's kinda suck */
		xml = xmlNewNode(NULL, (const guchar *)"dummy");
		e_source_dump_to_xml_node (sdialog->source, xml);
		e_source_update_from_xml_node (sdialog->original_source, xml->children, NULL);
		xmlFreeNode (xml);
#if d(!)0
		txt = e_source_to_standalone_xml (sdialog->original_source);
		printf("source is now:\n%s\n", txt);
		g_free (txt);
#endif
	} else {
		d(printf("committing new source\n"));
		e_source_group_add_source (sdialog->source_group, sdialog->source, -1);
		e_source_list_sync (sdialog->source_list, NULL);
	}

#if d(!)0
	txt = e_source_to_standalone_xml (sdialog->source);
	printf("running source is now:\n%s\n", txt);
	g_free (txt);
#endif
}

static void
eabc_free (EConfig *ec, GSList *items, gpointer data)
{
	AddressbookSourceDialog *sdialog = data;

	g_slist_free (items);

	g_object_unref (sdialog->source);
	if (sdialog->original_source)
		g_object_unref (sdialog->original_source);
	if (sdialog->source_list)
		g_object_unref (sdialog->source_list);
	g_slist_free (sdialog->menu_source_groups);

	g_object_unref (sdialog->builder);

	g_free (sdialog);
}

static gboolean
eabc_check_complete (EConfig *ec, const gchar *pageid, gpointer data)
{
	AddressbookSourceDialog *sdialog = data;
	gint valid = TRUE;
	const gchar *tmp;
	ESource *source;

	d(printf("check complete, pageid = '%s'\n", pageid?pageid:"<all>"));
	/* have name, and unique */
	tmp = e_source_peek_name (sdialog->source);
	valid = tmp && tmp[0] != 0
		&& ((source = e_source_group_peek_source_by_name (sdialog->source_group, tmp)) == NULL
		    || source == sdialog->original_source);

#ifdef HAVE_LDAP
	if (valid && source_group_is_remote (sdialog->source_group)) {
		gchar *uri = e_source_get_uri (sdialog->source);
		LDAPURLDesc *lud;

		/* check host and port set */
		if (ldap_url_parse (uri, &lud) == LDAP_SUCCESS) {
			valid = lud->lud_host != NULL
				&& lud->lud_host[0] != 0
				&& lud->lud_port != 0;
			ldap_free_urldesc (lud);
		} else
			valid = FALSE;
		g_free (uri);

		/* check auth name provided if auth set */
		if (valid && (tmp = e_source_get_property(sdialog->source, "auth"))) {
			switch (ldap_parse_auth (tmp)) {
			case ADDRESSBOOK_LDAP_AUTH_SIMPLE_EMAIL:
				tmp = e_source_get_property(sdialog->source, "email_addr");
				break;
			case ADDRESSBOOK_LDAP_AUTH_SIMPLE_BINDDN:
				tmp = e_source_get_property(sdialog->source, "binddn");
				break;
			default:
				tmp = "dummy";
				break;
			}
			valid = tmp && tmp[0];
		}

		/* check timeout isn't too short (why don't we just force it?) */
		if (valid) {
			tmp = e_source_get_property(sdialog->source, "timeout");
			valid = tmp && g_strtod (tmp, NULL) > 0.0;
		}
	}
#endif
	return valid;
}

/* debug only: */
#if d(!)0
static void
source_changed (ESource *source, AddressbookSourceDialog *sdialog)
{
	gchar *xml;

	xml = e_source_to_standalone_xml (source);
	printf("source changed:\n%s\n", xml);
	g_free (xml);
}
#endif

GtkWidget*
addressbook_config_edit_source (GtkWidget *parent, ESource *source)
{
	AddressbookSourceDialog *sdialog = g_new0 (AddressbookSourceDialog, 1);
	EABConfig *ec;
	gint i;
	GSList *items = NULL;
	EABConfigTargetSource *target;
	gchar *xml;

	sdialog->builder = gtk_builder_new ();
	e_load_ui_builder_definition (sdialog->builder, "ldap-config.ui");

	if (source) {
		sdialog->original_source = source;
		g_object_ref (source);
		sdialog->source_group = e_source_peek_group (source);
		xml = e_source_to_standalone_xml (source);
		sdialog->source = e_source_new_from_standalone_xml (xml);
		g_free (xml);
	} else {
		GConfClient *gconf;
		GSList *l;

		sdialog->source = e_source_new("", "");
		gconf = gconf_client_get_default ();
		sdialog->source_list = e_source_list_new_for_gconf(gconf, "/apps/evolution/addressbook/sources");
		l = e_source_list_peek_groups (sdialog->source_list);
		if (!l) {
			g_warning ("Address Book source groups are missing! Check your GConf setup.");
			g_object_unref (gconf);
			g_free (sdialog);
			return NULL;
		}

		sdialog->menu_source_groups = g_slist_copy (l);
#ifndef HAVE_LDAP
		for (;l;l = g_slist_next (l))
			if (!strncmp("ldap:", e_source_group_peek_base_uri(l->data), 5))
				sdialog->menu_source_groups = g_slist_remove (sdialog->menu_source_groups, l->data);
#endif
		sdialog->source_group = (ESourceGroup *)sdialog->menu_source_groups->data;
		for (i=0;eabc_new_items[i].path;i++)
			items = g_slist_prepend (items, &eabc_new_items[i]);
		g_object_unref (gconf);
	}

	/* HACK: doesn't work if you don't do this */
	e_source_set_group (sdialog->source, sdialog->source_group);

#if d(!)0
	xml = e_source_to_standalone_xml (sdialog->source);
	printf("but working standalone xml: %s\n", xml);
	g_free (xml);
	g_signal_connect(sdialog->source, "changed", source_changed, sdialog);
#endif

	sdialog->config = ec = eab_config_new(E_CONFIG_BOOK, "com.novell.evolution.addressbook.config.accountEditor");

	for (i=0;eabc_items[i].path;i++) {
		if (eabc_items[i].label)
			eabc_items[i].label = gettext (eabc_items[i].label);
		items = g_slist_prepend (items, &eabc_items[i]);
	}

	e_config_add_items ((EConfig *)ec, items, eabc_commit, NULL, eabc_free, sdialog);
	e_config_add_page_check ((EConfig *)ec, NULL, eabc_check_complete, sdialog);

	target = eab_config_target_new_source (ec, sdialog->source);
	e_config_set_target ((EConfig *)ec, (EConfigTarget *)target);

	if (source)
		sdialog->window = e_config_create_window((EConfig *)ec, NULL, _("Address Book Properties"));
	else
		sdialog->window = e_config_create_window((EConfig *)ec, NULL, _("New Address Book"));

	/* forces initial validation */
	if (!sdialog->original_source)
		e_config_target_changed ((EConfig *)ec, E_CONFIG_TARGET_CHANGED_STATE);

	return sdialog->window;
}
