/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 * Chris Toshok <toshok@ximian.com>
 * Chris Lahey <clahey@ximian.com>
 * Michael Zucchi <notzed@ximian.com>
 *  And no doubt others ...
 **/

/*#define STANDALONE*/

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include <gtk/gtkcombo.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkrange.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtkcelllayout.h>
#include <gtk/gtklabel.h>
#include <libgnome/gnome-i18n.h>

#include <bonobo/bonobo-generic-factory.h>

#include <glade/glade.h>

#include "addressbook.h"
#include "addressbook-component.h"
#include "addressbook-config.h"

#include "widgets/misc/e-error.h"

#include "evolution-config-control.h"

#include "addressbook/gui/widgets/eab-config.h"

#define d(x)

#ifdef HAVE_LDAP
#include "ldap.h"
#include "ldap_schema.h"
#endif

#define LDAP_PORT_STRING "389"
#define LDAPS_PORT_STRING "636"

#define GLADE_FILE_NAME "ldap-config.glade"
#define CONFIG_CONTROL_FACTORY_ID "OAFIID:GNOME_Evolution_Addressbook_ConfigControlFactory:" BASE_VERSION
#define LDAP_CONFIG_CONTROL_ID "OAFIID:GNOME_Evolution_LDAPStorage_ConfigControl:" BASE_VERSION

GtkWidget* supported_bases_create_table (char *name, char *string1, char *string2,
					 int num1, int num2);

/* default objectclasses */
#define TOP                  "top"
#define PERSON               "person"
#define ORGANIZATIONALPERSON "organizationalPerson"
#define INETORGPERSON        "inetOrgPerson"
#define EVOLUTIONPERSON      "evolutionPerson"
#define CALENTRY             "calEntry"


typedef struct _AddressbookSourceDialog AddressbookSourceDialog;

struct _AddressbookSourceDialog {
	GladeXML  *gui;

	EABConfig *config;	/* the config manager */

	GtkWidget *window;

	/* Source selection (druid only) */
	ESourceList *source_list;
	GSList *menu_source_groups;
	GtkWidget *group_optionmenu;

	/* ESource we're currently editing */
	ESource *source;
	/* The original source in edit mode.  Also used to flag when we are in edit mode. */
	ESource *original_source;

	/* Source group we're creating/editing a source in */
	ESourceGroup *source_group;

	/* info page fields */
	GtkWidget *host;
	GtkWidget *auth_optionmenu;
	AddressbookLDAPAuthType auth; 
	GtkWidget *auth_principal;

	/* connecting page fields */
	GtkWidget *port_combo;
	GtkWidget *ssl_optionmenu;
	AddressbookLDAPSSLType ssl;

	/* searching page fields */
	GtkWidget *rootdn;
	AddressbookLDAPScopeType scope;
	GtkWidget *scope_optionmenu;
	GtkWidget *timeout_scale;
	GtkWidget *limit_spinbutton;

	/* display name page fields */
	GtkWidget *display_name;
};



#ifdef HAVE_LDAP

static char *
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
		g_assert(0);
		return "none";
	}
}

static AddressbookLDAPAuthType
ldap_parse_auth (const char *auth)
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

static char *
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
		g_assert(0);
		return "";
	}
}

static char *
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
		g_assert(0);
		return "";
	}
}

static AddressbookLDAPSSLType
ldap_parse_ssl (const char *ssl)
{
	if (!ssl)
		return ADDRESSBOOK_LDAP_SSL_WHENEVER_POSSIBLE; /* XXX good default? */

	if (!strcmp (ssl, "always"))
		return ADDRESSBOOK_LDAP_SSL_ALWAYS;
	else if (!strcmp (ssl, "never"))
		return ADDRESSBOOK_LDAP_SSL_NEVER;
	else
		return ADDRESSBOOK_LDAP_SSL_WHENEVER_POSSIBLE;
}

static gboolean
source_to_uri_parts (ESource *source, gchar **host, gchar **rootdn, AddressbookLDAPScopeType *scope, gint *port)
{
	gchar       *uri;
	LDAPURLDesc *lud;
	gint         ldap_error;

	g_assert (source);

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
	int ldap_error;
	int protocol_version = LDAP_VERSION3;

	if (!source_to_uri_parts (source, &host, NULL, NULL, &port))
		return NULL;

	if (!(ldap = ldap_init (host, port))) {
		e_error_run ((GtkWindow *) window, "addressbook:ldap-init", NULL);
		goto done;
	}

	ldap_error = ldap_set_option (ldap, LDAP_OPT_PROTOCOL_VERSION, &protocol_version);
	if (LDAP_OPT_SUCCESS != ldap_error)
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
		e_error_run ((GtkWindow *) window, "addressbook:ldap-auth", NULL);
	
	return ldap_error;
}

static int
addressbook_root_dse_query (AddressbookSourceDialog *dialog, LDAP *ldap,
			    char **attrs, LDAPMessage **resp)
{
	int ldap_error;
	struct timeval timeout;

	timeout.tv_sec = (gint) gtk_adjustment_get_value (GTK_RANGE(dialog->timeout_scale)->adjustment);
	timeout.tv_usec = 0;

	ldap_error = ldap_search_ext_s (ldap,
					LDAP_ROOT_DSE, LDAP_SCOPE_BASE,
					"(objectclass=*)",
					attrs, 0, NULL, NULL, &timeout, LDAP_NO_LIMIT, resp);
	if (LDAP_SUCCESS != ldap_error)
		e_error_run (GTK_WINDOW (dialog->window), "addressbook:ldap-search-base", NULL);
	
	return ldap_error;
}

/* searching page */
GtkWidget*
supported_bases_create_table (char *name, char *string1, char *string2, int num1, int num2)
{
	GtkWidget *table, *scrolled;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkListStore *model;

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled), GTK_SHADOW_IN);

	model = gtk_list_store_new (1, G_TYPE_STRING);
	table = gtk_tree_view_new_with_model ((GtkTreeModel *) model);
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes ((GtkTreeView *) table, -1, _("Base"), renderer, "text", 0, NULL);
	gtk_tree_view_set_headers_visible ((GtkTreeView *) table, FALSE);
	selection = gtk_tree_view_get_selection ((GtkTreeView *) table);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	gtk_container_add (GTK_CONTAINER (scrolled), table);
	g_object_set_data((GObject *)scrolled, "table", table);

	return scrolled;
}

static gboolean
do_ldap_root_dse_query (AddressbookSourceDialog *sdialog, GtkListStore *model, ESource *source)
{
	LDAP *ldap;
	char *attrs[2];
	int ldap_error;
	char **values;
	LDAPMessage *resp;
	int i;

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
	if (!values || values[0] == NULL) {
		e_error_run (GTK_WINDOW (sdialog->window), "addressbook:ldap-search-base", NULL);
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
					   gtk_tree_selection_get_selected(selection, NULL, NULL));
}

static void
query_for_supported_bases (GtkWidget *button, AddressbookSourceDialog *sdialog)
{
	GtkTreeSelection *selection;
	GtkListStore *model;
	GtkTreeView *table;
	GtkWidget *dialog;
	GtkWidget *supported_bases_table;
	GladeXML *gui;
	GtkTreeIter iter;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/" GLADE_FILE_NAME, "supported-bases-dialog", NULL);
	dialog = glade_xml_get_widget (gui, "supported-bases-dialog");

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (sdialog->window));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	gtk_widget_ensure_style (dialog);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), 0);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 12);

	supported_bases_table = glade_xml_get_widget (gui, "supported-bases-table");
	gtk_widget_show_all (supported_bases_table);

	table = g_object_get_data((GObject *)supported_bases_table, "table");
	model = (GtkListStore *)gtk_tree_view_get_model(table);
	selection = gtk_tree_view_get_selection (table);
	g_signal_connect (selection, "changed", G_CALLBACK (search_base_selection_model_changed), dialog);
	search_base_selection_model_changed (selection, dialog);

	if (do_ldap_root_dse_query (sdialog, model, sdialog->source)) {
		gtk_widget_show (dialog);

		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK
		    && gtk_tree_selection_get_selected(selection, (GtkTreeModel **)&model, &iter)) {
			char *dn;

			gtk_tree_model_get ((GtkTreeModel *)model, &iter, 0, &dn, -1);
			gtk_entry_set_text((GtkEntry *)sdialog->rootdn, dn);
			g_free(dn);
		}
	}

	gtk_widget_destroy (dialog);
}

#endif /* HAVE_LDAP */

GtkWidget*
addressbook_config_create_new_source (GtkWidget *parent)
{
	return addressbook_config_edit_source(parent, NULL);
}

/* ********************************************************************** */

static void
eabc_type_changed(GtkComboBox *dropdown, AddressbookSourceDialog *sdialog)
{
	int id = gtk_combo_box_get_active(dropdown);
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model(dropdown);
	if (id == -1 || !gtk_tree_model_iter_nth_child(model, &iter, NULL, id))
		return;

	/* TODO: when we change the group type, we lose all of the pre-filled dialog info */

	gtk_tree_model_get(model, &iter, 1, &sdialog->source_group, -1);
	/* HACK: doesn't work if you don't do this */
	e_source_set_absolute_uri(sdialog->source, NULL);
	e_source_set_group(sdialog->source, sdialog->source_group);

	/* BIG HACK: We load the defaults for each type here.
	   I guess plugins will have to use the do it in their factory callbacks */
	if (!strncmp(e_source_group_peek_base_uri(sdialog->source_group), "groupwise:", 10)) {
		GSList *l;
		ESource *source;
		char *tmp;

		l = e_source_group_peek_sources(sdialog->source_group);
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
		char *tmp;

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

	e_config_target_changed((EConfig *)sdialog->config, E_CONFIG_TARGET_CHANGED_REBUILD);
}

static GtkWidget *
eabc_general_type(EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	AddressbookSourceDialog *sdialog = data;
	GtkComboBox *dropdown;
	GtkCellRenderer *cell;
	GtkListStore *store;
	GtkTreeIter iter;
	GSList *l;
	GtkWidget *w, *label;

	if (old)
		return old;

	w = gtk_hbox_new(FALSE, 6);
	label = gtk_label_new_with_mnemonic(_("_Type:"));
	gtk_box_pack_start((GtkBox *)w, label, FALSE, FALSE, 0);

	dropdown = (GtkComboBox *)gtk_combo_box_new();	
	cell = gtk_cell_renderer_text_new();
	store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);
	for (l=sdialog->menu_source_groups;l;l=g_slist_next(l)) {
		ESourceGroup *group = l->data;

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, e_source_group_peek_name(group), 1, group, -1);	
	}

	gtk_cell_layout_pack_start((GtkCellLayout *)dropdown, cell, TRUE);
	gtk_cell_layout_set_attributes((GtkCellLayout *)dropdown, cell, "text", 0, NULL);
	gtk_combo_box_set_model(dropdown, (GtkTreeModel *)store);
	gtk_combo_box_set_active(dropdown, -1);
	gtk_combo_box_set_active(dropdown, 0);
	g_signal_connect(dropdown, "changed", G_CALLBACK(eabc_type_changed), sdialog);
	gtk_widget_show((GtkWidget *)dropdown);
	gtk_box_pack_start((GtkBox *)w, (GtkWidget *)dropdown, TRUE, TRUE, 0);
	gtk_label_set_mnemonic_widget((GtkLabel *)label, (GtkWidget *)dropdown);

	gtk_box_pack_start((GtkBox *)parent, (GtkWidget *)w, FALSE, FALSE, 0);

	gtk_widget_show_all(w);

	return (GtkWidget *)w;
}

static void
name_changed_cb(GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	e_source_set_name (sdialog->source, gtk_entry_get_text (GTK_ENTRY (sdialog->display_name)));
}

static GtkWidget *
eabc_general_name(EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	AddressbookSourceDialog *sdialog = data;
	const char *uri;
	GtkWidget *w;
	GladeXML *gui;

	if (old)
		return old;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/" GLADE_FILE_NAME, item->label, NULL);
	w = glade_xml_get_widget(gui, item->label);
	gtk_box_pack_start((GtkBox *)parent, w, FALSE, FALSE, 0);

	sdialog->display_name = glade_xml_get_widget (gui, "account-editor-display-name-entry");
	g_signal_connect(sdialog->display_name, "changed", G_CALLBACK(name_changed_cb), sdialog);
	gtk_entry_set_text((GtkEntry *)sdialog->display_name, e_source_peek_name(sdialog->source));

	/* Hardcoded: groupwise can't edit the name (or anything else) */
	if (sdialog->original_source) {
		uri = e_source_group_peek_base_uri (sdialog->source_group);
		if (uri && strncmp(uri, "groupwise:", 10) == 0) {
			gtk_widget_set_sensitive (GTK_WIDGET(sdialog->display_name), FALSE);
		}
	}

	g_object_unref(gui);

	return w;
}

#ifdef HAVE_LDAP
static void
url_changed(AddressbookSourceDialog *sdialog)
{
	char *str;

	str = g_strdup_printf ("%s:%s/%s?" /* trigraph prevention */ "?%s",
			       gtk_entry_get_text (GTK_ENTRY (sdialog->host)),
			       gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (sdialog->port_combo)->entry)),
			       gtk_entry_get_text (GTK_ENTRY (sdialog->rootdn)),
			       ldap_unparse_scope (sdialog->scope));
	e_source_set_relative_uri (sdialog->source, str);
	g_free (str);
}

static void
host_changed_cb(GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	url_changed(sdialog);
}

static void
port_entry_changed_cb(GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	const char *port = gtk_entry_get_text((GtkEntry *)w);

	if (!strcmp (port, LDAPS_PORT_STRING)) {
		sdialog->ssl = ADDRESSBOOK_LDAP_SSL_ALWAYS;
		gtk_option_menu_set_history (GTK_OPTION_MENU(sdialog->ssl_optionmenu), sdialog->ssl);
		gtk_widget_set_sensitive (sdialog->ssl_optionmenu, FALSE);
	} else {
		gtk_widget_set_sensitive (sdialog->ssl_optionmenu, TRUE);
	}

	url_changed(sdialog);
}

static void
ssl_optionmenu_changed_cb(GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	sdialog->ssl = gtk_option_menu_get_history((GtkOptionMenu *)w);
	e_source_set_property (sdialog->source, "ssl", ldap_unparse_ssl (sdialog->ssl));
}

static GtkWidget *
eabc_general_host(EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	AddressbookSourceDialog *sdialog = data;
	const char *tmp;
	GtkWidget *w;
	char *uri, port[16];
	LDAPURLDesc *lud;
	GladeXML *gui;

	if (!source_group_is_remote(sdialog->source_group))
		return NULL;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/" GLADE_FILE_NAME, item->label, NULL);
	w = glade_xml_get_widget(gui, item->label);
	gtk_box_pack_start((GtkBox *)parent, w, FALSE, FALSE, 0);

	uri = e_source_get_uri(sdialog->source);
	if (ldap_url_parse(uri, &lud) != LDAP_SUCCESS)
		lud = NULL;
	g_free(uri);

	sdialog->host = glade_xml_get_widget (gui, "server-name-entry");
	gtk_entry_set_text((GtkEntry *)sdialog->host, lud && lud->lud_host ? lud->lud_host : "");
	g_signal_connect (sdialog->host, "changed", G_CALLBACK (host_changed_cb), sdialog);

	sdialog->port_combo = glade_xml_get_widget (gui, "port-combo");	
	sprintf(port, "%u", lud && lud->lud_port? lud->lud_port : LDAP_PORT);
	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (sdialog->port_combo)->entry), port);
	g_signal_connect (GTK_COMBO(sdialog->port_combo)->entry, "changed", G_CALLBACK (port_entry_changed_cb), sdialog);

	if (lud)
		ldap_free_urldesc (lud);

	sdialog->ssl_optionmenu = glade_xml_get_widget (gui, "ssl-optionmenu");
	tmp = e_source_get_property (sdialog->source, "ssl");
	sdialog->ssl = tmp ? ldap_parse_ssl (tmp) : ADDRESSBOOK_LDAP_SSL_WHENEVER_POSSIBLE;
	gtk_option_menu_set_history (GTK_OPTION_MENU(sdialog->ssl_optionmenu), sdialog->ssl);
	g_signal_connect(sdialog->ssl_optionmenu, "changed", G_CALLBACK(ssl_optionmenu_changed_cb), sdialog);

	g_object_unref(gui);

	return w;
}

static void
auth_entry_changed_cb(GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	const char *principal = gtk_entry_get_text((GtkEntry *)w);

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
auth_optionmenu_changed_cb(GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	sdialog->auth = gtk_option_menu_get_history((GtkOptionMenu *)w);
	e_source_set_property (sdialog->source, "auth", ldap_unparse_auth (sdialog->auth));

	/* make sure the right property is set for the auth - ugh, funny api */
	auth_entry_changed_cb(sdialog->auth_principal, sdialog);
}

static GtkWidget *
eabc_general_auth(EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	AddressbookSourceDialog *sdialog = data;
	GtkWidget *w;
	const char *tmp;
	GladeXML *gui;

	if (!source_group_is_remote(sdialog->source_group))
		return NULL;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/" GLADE_FILE_NAME, item->label, NULL);
	w = glade_xml_get_widget(gui, item->label);
	gtk_box_pack_start((GtkBox *)parent, w, FALSE, FALSE, 0);

	sdialog->auth_optionmenu = glade_xml_get_widget (gui, "auth-optionmenu");
	tmp = e_source_get_property(sdialog->source, "auth");
	sdialog->auth = tmp ? ldap_parse_auth(tmp) : ADDRESSBOOK_LDAP_AUTH_NONE;
	gtk_option_menu_set_history (GTK_OPTION_MENU(sdialog->auth_optionmenu), sdialog->auth);	
	g_signal_connect(sdialog->auth_optionmenu, "changed", G_CALLBACK(auth_optionmenu_changed_cb), sdialog);

	sdialog->auth_principal = glade_xml_get_widget (gui, "auth-entry");
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

	g_object_unref(gui);

	return w;
}

static void
rootdn_changed_cb(GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	url_changed(sdialog);
}

static void
scope_optionmenu_changed_cb(GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	sdialog->scope = gtk_option_menu_get_history((GtkOptionMenu *)w);
	url_changed(sdialog);
}

static GtkWidget *
eabc_details_search(EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	AddressbookSourceDialog *sdialog = data;
	GtkWidget *w;
	LDAPURLDesc *lud;
	char *uri;
	GladeXML *gui;

	if (!source_group_is_remote(sdialog->source_group))
		return NULL;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/" GLADE_FILE_NAME, item->label, NULL);
	w = glade_xml_get_widget(gui, item->label);
	gtk_box_pack_start((GtkBox *)parent, w, FALSE, FALSE, 0);

	uri = e_source_get_uri(sdialog->source);
	if (ldap_url_parse(uri, &lud) != LDAP_SUCCESS)
		lud = NULL;
	g_free(uri);

	sdialog->rootdn = glade_xml_get_widget (gui, "rootdn-entry");
	gtk_entry_set_text((GtkEntry *)sdialog->rootdn, lud && lud->lud_dn ? lud->lud_dn : "");
	g_signal_connect (sdialog->rootdn, "changed", G_CALLBACK (rootdn_changed_cb), sdialog);

	sdialog->scope_optionmenu = glade_xml_get_widget (gui, "scope-optionmenu");
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
	gtk_option_menu_set_history (GTK_OPTION_MENU(sdialog->scope_optionmenu), sdialog->scope);
	g_signal_connect(sdialog->scope_optionmenu, "changed", G_CALLBACK(scope_optionmenu_changed_cb), sdialog);

	g_signal_connect (glade_xml_get_widget(gui, "rootdn-button"), "clicked",
			  G_CALLBACK(query_for_supported_bases), sdialog);

	if (lud)
		ldap_free_urldesc (lud);

	g_object_unref(gui);

	return w;
}

static void
timeout_changed_cb(GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	char *timeout;

	timeout = g_strdup_printf("%f", gtk_adjustment_get_value(((GtkRange *)sdialog->timeout_scale)->adjustment));
	e_source_set_property(sdialog->source, "timeout", timeout);
	g_free(timeout);
}

static void
limit_changed_cb(GtkWidget *w, AddressbookSourceDialog *sdialog)
{
	char limit[16];

	sprintf(limit, "%d", gtk_spin_button_get_value_as_int((GtkSpinButton *)sdialog->limit_spinbutton));
	e_source_set_property(sdialog->source, "limit", limit);
}

static GtkWidget *
eabc_details_limit(EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	AddressbookSourceDialog *sdialog = data;
	GtkWidget *w;
	const char *tmp;
	GladeXML *gui;

	if (!source_group_is_remote(sdialog->source_group))
		return NULL;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/" GLADE_FILE_NAME, item->label, NULL);
	w = glade_xml_get_widget(gui, item->label);
	gtk_box_pack_start((GtkBox *)parent, w, FALSE, FALSE, 0);

	sdialog->timeout_scale = glade_xml_get_widget (gui, "timeout-scale");
	tmp = e_source_get_property(sdialog->source, "timeout");
	gtk_adjustment_set_value(((GtkRange *)sdialog->timeout_scale)->adjustment, tmp?g_strtod(tmp, NULL):3.0);
	g_signal_connect (GTK_RANGE(sdialog->timeout_scale)->adjustment, "value_changed", G_CALLBACK (timeout_changed_cb), sdialog);

	sdialog->limit_spinbutton = glade_xml_get_widget (gui, "download-limit-spinbutton");
	tmp = e_source_get_property(sdialog->source, "limit");
	gtk_spin_button_set_value((GtkSpinButton *)sdialog->limit_spinbutton, tmp?g_strtod(tmp, NULL):100.0);
	g_signal_connect (sdialog->limit_spinbutton, "changed", G_CALLBACK (limit_changed_cb), sdialog);

	g_object_unref(gui);

	return w;
}
#endif

static EConfigItem eabc_items[] = {
	{ E_CONFIG_BOOK, "", },
	{ E_CONFIG_PAGE, "00.general", N_("General") },
	{ E_CONFIG_SECTION, "00.general/10.display", N_("Addressbook") },
	{ E_CONFIG_ITEM, "00.general/10.display/10.name", "hbox122", eabc_general_name },
#ifdef HAVE_LDAP
	{ E_CONFIG_SECTION, "00.general/20.server", N_("Server Information") },
	{ E_CONFIG_ITEM, "00.general/20.server/00.host", "table31", eabc_general_host },
	{ E_CONFIG_SECTION, "00.general/30.auth", N_("Authentication") },
	{ E_CONFIG_ITEM, "00.general/30.auth/00.auth", "table32", eabc_general_auth },

	{ E_CONFIG_PAGE, "10.details", N_("Details") },
	{ E_CONFIG_SECTION, "10.details/00.search", N_("Searching") },
	{ E_CONFIG_ITEM, "10.details/00.search/00.search", "table33", eabc_details_search },
	{ E_CONFIG_SECTION, "10.details/10.limit", N_("Downloading") },
	{ E_CONFIG_ITEM, "10.details/10.limit/00.limit", "table34", eabc_details_limit },
#endif
	{ 0 },
};

/* items needed for the 'new addressbook' window */
static EConfigItem eabc_new_items[] = {
	{ E_CONFIG_ITEM, "00.general/10.display/00.type", NULL, eabc_general_type },
	{ 0 },
};

static void
eabc_commit(EConfig *ec, GSList *items, void *data)
{
	AddressbookSourceDialog *sdialog = data;
	xmlNodePtr xml;
#if d(!)0
	char *txt;
#endif
	if (sdialog->original_source) {
		d(printf("committing addressbook changes\n"));

		/* these api's kinda suck */
		xml = xmlNewNode(NULL, "dummy");
		e_source_dump_to_xml_node(sdialog->source, xml);
		e_source_update_from_xml_node(sdialog->original_source, xml->children, NULL);
		xmlFreeNode(xml);
#if d(!)0
		txt = e_source_to_standalone_xml(sdialog->original_source);
		printf("source is now:\n%s\n", txt);
		g_free(txt);
#endif
	} else {
		d(printf("committing new source\n"));

		e_source_group_add_source(sdialog->source_group, sdialog->source, -1);
		e_source_list_sync(sdialog->source_list, NULL);
	}

#if d(!)0
	txt = e_source_to_standalone_xml(sdialog->source);
	printf("running source is now:\n%s\n", txt);
	g_free(txt);
#endif
}

static void
eabc_free(EConfig *ec, GSList *items, void *data)
{
	AddressbookSourceDialog *sdialog = data;

	g_slist_free(items);

	g_object_unref(sdialog->source);
	if (sdialog->original_source)
		g_object_unref(sdialog->original_source);
	if (sdialog->source_list)
		g_object_unref(sdialog->source_list);
	g_slist_free(sdialog->menu_source_groups);

	g_object_unref(sdialog->gui);

	g_free(sdialog);
}

static gboolean
eabc_check_complete(EConfig *ec, const char *pageid, void *data)
{
	AddressbookSourceDialog *sdialog = data;
	int valid = TRUE;
	const char *tmp;
	ESource *source;

	d(printf("check complete, pageid = '%s'\n", pageid?pageid:"<all>"));
	/* have name, and unique */
	tmp = e_source_peek_name(sdialog->source);
	valid = tmp && tmp[0] != 0
		&& ((source = e_source_group_peek_source_by_name(sdialog->source_group, tmp)) == NULL
		    || source == sdialog->original_source);

#ifdef HAVE_LDAP
	if (valid && source_group_is_remote(sdialog->source_group)) {
		char *uri = e_source_get_uri(sdialog->source);
		LDAPURLDesc *lud;

		/* check host and port set */
		if (ldap_url_parse(uri, &lud) == LDAP_SUCCESS) {
			valid = lud->lud_host != NULL
				&& lud->lud_host[0] != 0
				&& lud->lud_port != 0;
			ldap_free_urldesc (lud);
		} else
			valid = FALSE;
		g_free(uri);

		/* check auth name provided if auth set */
		if (valid && (tmp = e_source_get_property(sdialog->source, "auth"))) {
			switch (ldap_parse_auth(tmp)) {
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
			valid = tmp && g_strtod(tmp, NULL) > 0.0;
		}
	}
#endif
	return valid;
}

/* debug only: */
#if d(!)0
static void
source_changed(ESource *source, AddressbookSourceDialog *sdialog)
{
	char *xml;

	xml = e_source_to_standalone_xml(source);
	printf("source changed:\n%s\n", xml);
	g_free(xml);
}
#endif

GtkWidget*
addressbook_config_edit_source (GtkWidget *parent, ESource *source)
{
	AddressbookSourceDialog *sdialog = g_new0 (AddressbookSourceDialog, 1);
	EABConfig *ec;
	int i;
	GSList *items = NULL;
	EABConfigTargetSource *target;
	char *xml;

	sdialog->gui = glade_xml_new (EVOLUTION_GLADEDIR "/" GLADE_FILE_NAME, "account-editor-notebook", NULL);

	if (source) {
		sdialog->original_source = source;
		g_object_ref(source);
		sdialog->source_group = e_source_peek_group (source);
		xml = e_source_to_standalone_xml(source);
		sdialog->source = e_source_new_from_standalone_xml(xml);
		g_free(xml);
	} else {
		GConfClient *gconf;
		GSList *l;

		sdialog->source = e_source_new("", "");
		gconf = gconf_client_get_default();
		sdialog->source_list = e_source_list_new_for_gconf(gconf, "/apps/evolution/addressbook/sources");
		l = e_source_list_peek_groups(sdialog->source_list);
		sdialog->menu_source_groups = g_slist_copy(l);
#ifndef HAVE_LDAP
		for (;l;l = g_slist_next(l))
			if (!strncmp("ldap:", e_source_group_peek_base_uri(l->data), 5))
				sdialog->menu_source_groups = g_slist_remove (sdialog->menu_source_groups, l->data);
#endif
		sdialog->source_group = (ESourceGroup *)sdialog->menu_source_groups->data;
		for (i=0;eabc_new_items[i].path;i++)
			items = g_slist_prepend(items, &eabc_new_items[i]);
		g_object_unref(gconf);
	}

	/* HACK: doesn't work if you don't do this */
	e_source_set_absolute_uri(sdialog->source, NULL);
	e_source_set_group(sdialog->source, sdialog->source_group);

#if d(!)0
	xml = e_source_to_standalone_xml(sdialog->source);
	printf("but working standalone xml: %s\n", xml);
	g_free(xml);
	g_signal_connect(sdialog->source, "changed", source_changed, sdialog);
#endif

	sdialog->config = ec = eab_config_new(E_CONFIG_BOOK, "com.novell.evolution.addressbook.config.accountEditor");

	for (i=0;eabc_items[i].path;i++)
		items = g_slist_prepend(items, &eabc_items[i]);

	e_config_add_items((EConfig *)ec, items, eabc_commit, NULL, eabc_free, sdialog);
	e_config_add_page_check((EConfig *)ec, NULL, eabc_check_complete, sdialog);

	target = eab_config_target_new_source(ec, sdialog->source);
	e_config_set_target((EConfig *)ec, (EConfigTarget *)target);

	sdialog->window = e_config_create_window((EConfig *)ec, NULL, _("Address Book Properties"));

	/* forces initial validation */
	if (!sdialog->original_source)
		e_config_target_changed((EConfig *)ec, E_CONFIG_TARGET_CHANGED_STATE);

	return sdialog->window;
}
