/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 * Chris Toshok <toshok@ximian.com>
 * Chris Lahey <clahey@ximian.com>
 **/

/*#define STANDALONE*/
/*#define NEW_ADVANCED_UI*/

#include <config.h>

#include "addressbook-config.h"

#include "addressbook.h"
#include "addressbook-storage.h"

#include "evolution-config-control.h"
#include <shell/e-folder-list.h>

#include <gal/widgets/e-unicode.h>
#include <gal/e-table/e-table-memory-store.h>
#include <gal/e-table/e-table-scrolled.h>
#include <e-util/e-html-utils.h>

#include <gtkhtml/gtkhtml.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-stock.h>

#include <bonobo/bonobo-generic-factory.h>

#include <glade/glade.h>

#include <stdlib.h>
#include <sys/time.h>

#ifdef HAVE_LDAP
#include "ldap.h"
#include "ldap_schema.h"
#endif

#define LDAP_PORT_STRING "389"
#define LDAPS_PORT_STRING "636"

#define GLADE_FILE_NAME "ldap-config.glade"
#define CONFIG_CONTROL_FACTORY_ID "OAFIID:GNOME_Evolution_Addressbook_ConfigControlFactory"
#define LDAP_CONFIG_CONTROL_ID "OAFIID:GNOME_Evolution_LDAPStorage_ConfigControl"
#define ADDRESSBOOK_CONFIG_CONTROL_ID "OAFIID:GNOME_Evolution_Addressbook_ConfigControl"

#ifdef HAVE_LDAP
GtkWidget* addressbook_dialog_create_sources_table (char *name, char *string1, char *string2,
						    int num1, int num2);
GtkWidget* supported_bases_create_table (char *name, char *string1, char *string2,
					 int num1, int num2);

#ifdef NEW_ADVANCED_UI
GtkWidget* objectclasses_create_server_table (char *name, char *string1, char *string2,
					      int num1, int num2);
GtkWidget* objectclasses_create_evolution_table (char *name, char *string1, char *string2,
						 int num1, int num2);
#endif

/* default objectclasses */
#define TOP                  "top"
#define PERSON               "person"
#define ORGANIZATIONALPERSON "organizationalPerson"
#define INETORGPERSON        "inetOrgPerson"
#define EVOLUTIONPERSON      "evolutionPerson"
#define CALENTRY             "calEntry"


typedef struct {
	GtkWidget *notebook;
	int page_num;
} FocusHelpClosure;

static void
focus_help (GtkWidget *w, GdkEventFocus *event, FocusHelpClosure *closure)
{
	gtk_notebook_set_page (GTK_NOTEBOOK(closure->notebook), closure->page_num);
}

static void
add_focus_handler (GtkWidget *widget, GtkWidget *notebook, int page_num)
{
	FocusHelpClosure *focus_closure = g_new0 (FocusHelpClosure, 1);
	focus_closure->notebook = notebook;
	focus_closure->page_num = page_num;

	gtk_signal_connect_full (GTK_OBJECT (widget),
				 "focus_in_event" /* XXX */,
				 (GtkSignalFunc) focus_help, NULL,
				 focus_closure,
				 (GtkDestroyNotify) g_free,
				 FALSE, FALSE);
}


typedef struct {
	EvolutionConfigControl *config_control;
	GtkWidget *page;

	GladeXML *gui;
	GNOME_Evolution_Shell shell;

	GtkWidget *sourcesTable;
	ETableModel *sourcesModel;
	GtkWidget *addSource;
	GtkWidget *editSource;
	GtkWidget *deleteSource;

} AddressbookDialog;

typedef struct {
	AddressbookDialog *addressbook_dialog;
	GladeXML  *gui;

	GtkWidget *window;
	GtkWidget *druid; /* only used (obviously) in the druid */

	/* info page fields */
	GtkSignalFunc general_modify_func;
	GtkWidget *host;
	GtkWidget *auth_optionmenu;
	AddressbookLDAPAuthType auth; 
	GtkWidget *auth_label_notebook;
	GtkWidget *auth_entry_notebook;
	GtkWidget *email;
	GtkWidget *binddn;

	/* connecting page fields */
	GtkSignalFunc connecting_modify_func;
	GtkWidget *port_combo;
	GtkWidget *ssl_optionmenu;
	AddressbookLDAPSSLType ssl;

	/* searching page fields */
	GtkSignalFunc searching_modify_func;
	GtkWidget *rootdn;
	AddressbookLDAPScopeType scope;
	GtkWidget *scope_optionmenu;
	GtkWidget *timeout_scale;
	GtkWidget *limit_spinbutton;

	/* display name page fields */
	GtkWidget *display_name;
	gboolean display_name_changed; /* only used in the druid */

	gboolean schema_query_successful;

#ifdef NEW_ADVANCED_UI
	/* objectclasses tab fields */
	GPtrArray *server_objectclasses;    /* the objectclasses available on the server */
	GPtrArray *evolution_objectclasses; /* the objectclasses evolution will use */
	GPtrArray *default_objectclasses;   /* the objectclasses we default to (actually the
					       intersection between defaults and server_objectclasses) */
	GtkSignalFunc objectclasses_modify_func;
	GtkWidget *objectclasses_server_table;
	ETableModel *objectclasses_server_model;
	GtkWidget *objectclasses_evolution_table;
	ETableModel *objectclasses_evolution_model;
	GtkWidget *objectclasses_add_button;
	GtkWidget *objectclasses_remove_button;

	/* refs we keep around so we can add/hide the tabs */
	GtkWidget *objectclasses_tab;
	GtkWidget *objectclasses_label;
	GtkWidget *mappings_tab;
	GtkWidget *mappings_label;
	GtkWidget *dn_customization_tab;
	GtkWidget *dn_customization_label;
#endif

	/* stuff for the account editor window */
	int source_model_row;
	GtkWidget *ok_button;
	GtkWidget *apply_button;
	GtkWidget *close_button;
	GtkWidget *advanced_button_notebook;
	GtkWidget *notebook; /* the toplevel notebook */

	gboolean advanced;

} AddressbookSourceDialog;


/* ldap api foo */
static LDAP *
addressbook_ldap_init (AddressbookSource *source)
{
	LDAP *ldap = ldap_init (source->host, atoi(source->port));

	if (!ldap) {
		gnome_error_dialog (_("Failed to connect to LDAP server"));

		return NULL;
	}

	/* XXX do TLS if it's configured in */

	return ldap;
}

static int
addressbook_ldap_auth (AddressbookSource *source, LDAP *ldap)
{
	int ldap_error;

	/* XXX use auth info from source */
	ldap_error = ldap_simple_bind_s (ldap, NULL, NULL);
	if (LDAP_SUCCESS != ldap_error)
		gnome_error_dialog (_("Failed to authenticate with LDAP server"));
	return ldap_error;

}

static int
addressbook_root_dse_query (AddressbookSource *source, LDAP *ldap, char **attrs, LDAPMessage **resp)
{
	int ldap_error;
	struct timeval timeout;

	/* 3 second timeout */
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;

	ldap_error = ldap_search_ext_s (ldap,
					LDAP_ROOT_DSE, LDAP_SCOPE_BASE,
					"(objectclass=*)",
					attrs, 0, NULL, NULL, &timeout, LDAP_NO_LIMIT, resp);
	if (LDAP_SUCCESS != ldap_error)
		gnome_error_dialog (_("Could not perform query on Root DSE"));

	return ldap_error;
}


static AddressbookSource *
addressbook_dialog_get_source (AddressbookSourceDialog *dialog)
{
	AddressbookSource *source = g_new0 (AddressbookSource, 1);

	source->name       = e_utf8_gtk_entry_get_text (GTK_ENTRY (dialog->display_name));
	source->host       = e_utf8_gtk_entry_get_text (GTK_ENTRY (dialog->host));
	source->email_addr = e_utf8_gtk_entry_get_text (GTK_ENTRY (dialog->email));
	source->binddn     = e_utf8_gtk_entry_get_text (GTK_ENTRY (dialog->binddn));
	source->port       = e_utf8_gtk_entry_get_text (GTK_ENTRY (GTK_COMBO(dialog->port_combo)->entry));
	source->rootdn     = e_utf8_gtk_entry_get_text (GTK_ENTRY (dialog->rootdn));
	source->limit      = atoi(e_utf8_gtk_entry_get_text (GTK_ENTRY (dialog->limit_spinbutton)));
	source->scope      = dialog->scope;
	source->auth       = dialog->auth;
	source->ssl        = dialog->ssl;

	addressbook_storage_init_source_uri (source);

	return source;
}

static void
addressbook_source_dialog_set_source (AddressbookSourceDialog *dialog, AddressbookSource *source)
{
	char *string;
	e_utf8_gtk_entry_set_text (GTK_ENTRY (dialog->display_name), source ? source->name : "");
	e_utf8_gtk_entry_set_text (GTK_ENTRY (dialog->host), source ? source->host : "");
	e_utf8_gtk_entry_set_text (GTK_ENTRY (dialog->email), source ? source->email_addr : "");
	e_utf8_gtk_entry_set_text (GTK_ENTRY (dialog->binddn), source ? source->binddn : "");
	e_utf8_gtk_entry_set_text (GTK_ENTRY (GTK_COMBO(dialog->port_combo)->entry), source ? source->port : LDAP_PORT_STRING);
	e_utf8_gtk_entry_set_text (GTK_ENTRY (dialog->rootdn), source ? source->rootdn : "");

	string = g_strdup_printf ("%d", source ? source->limit : 100);
	e_utf8_gtk_entry_set_text (GTK_ENTRY (dialog->limit_spinbutton), string);
	g_free (string);

	dialog->auth = source ? source->auth : ADDRESSBOOK_LDAP_AUTH_NONE;
	gtk_option_menu_set_history (GTK_OPTION_MENU(dialog->auth_optionmenu), dialog->auth);
	if (dialog->auth != ADDRESSBOOK_LDAP_AUTH_NONE) {
		gtk_notebook_set_page (GTK_NOTEBOOK(dialog->auth_label_notebook), dialog->auth - 1);
		gtk_notebook_set_page (GTK_NOTEBOOK(dialog->auth_entry_notebook), dialog->auth - 1);
	}
	gtk_widget_set_sensitive (dialog->auth_label_notebook, dialog->auth != ADDRESSBOOK_LDAP_AUTH_NONE);
	gtk_widget_set_sensitive (dialog->auth_entry_notebook, dialog->auth != ADDRESSBOOK_LDAP_AUTH_NONE);

	dialog->scope = source ? source->scope : ADDRESSBOOK_LDAP_SCOPE_ONELEVEL;
	gtk_option_menu_set_history (GTK_OPTION_MENU(dialog->scope_optionmenu), dialog->scope);

	dialog->ssl = source ? source->auth : ADDRESSBOOK_LDAP_SSL_WHENEVER_POSSIBLE;
	gtk_option_menu_set_history (GTK_OPTION_MENU(dialog->ssl_optionmenu), dialog->ssl);
}

static void
addressbook_source_dialog_destroy (GtkWidget *widget, AddressbookSourceDialog *dialog)
{
#ifdef NEW_ADVANCED_UI
#define IF_UNREF(x) if (x) gtk_object_unref (GTK_OBJECT ((x)))

	int i;

	if (dialog->server_objectclasses) {
		for (i = 0; i < dialog->server_objectclasses->len; i ++)
			ldap_objectclass_free (g_ptr_array_index (dialog->server_objectclasses, i));
		g_ptr_array_free (dialog->server_objectclasses, TRUE);
	}

	if (dialog->evolution_objectclasses) {
		for (i = 0; i < dialog->evolution_objectclasses->len; i ++)
			ldap_objectclass_free (g_ptr_array_index (dialog->evolution_objectclasses, i));
		g_ptr_array_free (dialog->evolution_objectclasses, TRUE);
	}

	if (dialog->default_objectclasses) {
		for (i = 0; i < dialog->default_objectclasses->len; i ++)
			ldap_objectclass_free (g_ptr_array_index (dialog->default_objectclasses, i));
		g_ptr_array_free (dialog->default_objectclasses, TRUE);
	}

	IF_UNREF (dialog->objectclasses_server_model);
	IF_UNREF (dialog->objectclasses_evolution_model);

	IF_UNREF (dialog->objectclasses_tab);
	IF_UNREF (dialog->objectclasses_label);
	IF_UNREF (dialog->mappings_tab);
	IF_UNREF (dialog->mappings_label);
	IF_UNREF (dialog->dn_customization_tab);
	IF_UNREF (dialog->dn_customization_label);

#undef IF_UNREF
#endif

	gtk_object_destroy (GTK_OBJECT (dialog->gui));

	g_free (dialog);
}

static void
addressbook_add_server_druid_cancel (GtkWidget *widget, AddressbookSourceDialog *dialog)
{
	gtk_widget_destroy (dialog->window);
}

static void
addressbook_add_server_druid_finish (GnomeDruidPage *druid_page, GtkWidget *gnome_druid, AddressbookSourceDialog *sdialog)
{
	AddressbookSource *source = addressbook_dialog_get_source (sdialog);
	AddressbookDialog *dialog = sdialog->addressbook_dialog;

	printf ("in finish (%s,%s)\n", source->name, source->host);

	e_table_memory_store_insert (E_TABLE_MEMORY_STORE (dialog->sourcesModel),
				     -1, source, source->name, source->host);

	evolution_config_control_changed (dialog->config_control);

	/* tear down the widgets */
	gtk_widget_destroy (sdialog->window);
}

static void
reparent_to_vbox (AddressbookSourceDialog *dialog, char *vbox_name, char *widget_name)
{
	GtkWidget *vbox, *widget;

	vbox = glade_xml_get_widget (dialog->gui, vbox_name);
	widget = glade_xml_get_widget (dialog->gui, widget_name);

	gtk_widget_reparent (widget, vbox);
	gtk_box_set_child_packing (GTK_BOX (vbox), widget, TRUE, TRUE, 0, GTK_PACK_START);
}

static void
auth_optionmenu_activated (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	dialog->auth = g_list_index (gtk_container_children (GTK_CONTAINER (item->parent)),
				    item);

	dialog->general_modify_func (item, dialog);

	if (dialog->auth == 0) {
		gtk_widget_set_sensitive (dialog->auth_label_notebook, FALSE);
		gtk_widget_set_sensitive (dialog->auth_entry_notebook, FALSE);
	}
	else {
		gtk_widget_set_sensitive (dialog->auth_label_notebook, TRUE);
		gtk_widget_set_sensitive (dialog->auth_entry_notebook, TRUE);
		gtk_notebook_set_page (GTK_NOTEBOOK(dialog->auth_label_notebook), dialog->auth - 1);
		gtk_notebook_set_page (GTK_NOTEBOOK(dialog->auth_entry_notebook), dialog->auth - 1);
	}
}

static void
add_auth_activate_cb (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (auth_optionmenu_activated), dialog);
}

static void
setup_general_tab (AddressbookSourceDialog *dialog, GtkSignalFunc modify_func)
{
	GtkWidget *general_tab_help;
	GtkWidget *menu;

	general_tab_help = glade_xml_get_widget (dialog->gui, "general-tab-help");

	dialog->general_modify_func = modify_func;
	dialog->host = glade_xml_get_widget (dialog->gui, "server-name-entry");
	gtk_signal_connect (GTK_OBJECT (dialog->host), "changed",
			    modify_func, dialog);
	add_focus_handler (dialog->host, general_tab_help, 0);

	dialog->auth_label_notebook = glade_xml_get_widget (dialog->gui, "auth-label-notebook");
	dialog->auth_entry_notebook = glade_xml_get_widget (dialog->gui, "auth-entry-notebook");
	dialog->email = glade_xml_get_widget (dialog->gui, "email-entry");
	gtk_signal_connect (GTK_OBJECT (dialog->email), "changed",
			    modify_func, dialog);
	add_focus_handler (dialog->email, general_tab_help, 1);
	dialog->binddn = glade_xml_get_widget (dialog->gui, "dn-entry");
	gtk_signal_connect (GTK_OBJECT (dialog->binddn), "changed",
			    modify_func, dialog);
	add_focus_handler (dialog->binddn, general_tab_help, 2);

	dialog->auth_optionmenu = glade_xml_get_widget (dialog->gui, "auth-optionmenu");
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU(dialog->auth_optionmenu));
	gtk_container_foreach (GTK_CONTAINER (menu), (GtkCallback)add_auth_activate_cb, dialog);
	add_focus_handler (dialog->auth_optionmenu, general_tab_help, 3);
}

static gboolean
general_tab_check (AddressbookSourceDialog *dialog)
{
	gboolean valid = TRUE;
	char *string;

	string = e_utf8_gtk_entry_get_text (GTK_ENTRY (dialog->host));
	if (!string || !string[0])
		valid = FALSE;
	g_free (string);

	if (valid) {
		if (dialog->auth != ADDRESSBOOK_LDAP_AUTH_NONE) {
			if (dialog->auth == ADDRESSBOOK_LDAP_AUTH_SIMPLE_BINDDN)
				string = e_utf8_gtk_entry_get_text (GTK_ENTRY (dialog->binddn));
			else
				string = e_utf8_gtk_entry_get_text (GTK_ENTRY (dialog->email));

			if (!string || !string[0])
				valid = FALSE;
			g_free (string);
		}
	}

	return valid;
}

static void
druid_info_page_modify_cb (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	gnome_druid_set_buttons_sensitive (GNOME_DRUID(dialog->druid),
					   TRUE, /* back */
					   general_tab_check (dialog), /* next */
					   TRUE /* cancel */);
}

static void
druid_info_page_prepare (GnomeDruidPage *dpage, GtkWidget *gdruid, AddressbookSourceDialog *dialog)
{
	druid_info_page_modify_cb (NULL, dialog);
	/* stick the focus in the hostname field */
	gtk_widget_grab_focus (dialog->host);
}


/* connecting page */
static void
ssl_optionmenu_activated (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	dialog->ssl = g_list_index (gtk_container_children (GTK_CONTAINER (item->parent)),
				    item);

	dialog->connecting_modify_func (item, dialog);
}

static void
ssl_optionmenu_selected (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	GtkWidget *connecting_tab_help;
	int ssl_type = g_list_index (gtk_container_children (GTK_CONTAINER (item->parent)),
				     item);

	connecting_tab_help = glade_xml_get_widget (dialog->gui, "connecting-tab-help");

	gtk_notebook_set_page (GTK_NOTEBOOK(connecting_tab_help), ssl_type + 1);
}

static void
add_ssl_activate_cb (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (ssl_optionmenu_activated), dialog);
	gtk_signal_connect (GTK_OBJECT (item), "select",
			    GTK_SIGNAL_FUNC (ssl_optionmenu_selected), dialog);
}

static void
port_changed_func (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	/* if the port value is ldaps, set the SSL/TLS option menu to
	   Always and desensitize it */
	char *string = e_utf8_gtk_entry_get_text (GTK_ENTRY (item));

	dialog->connecting_modify_func (item, dialog);

	if (!strcmp (string, LDAPS_PORT_STRING)) {
		dialog->ssl = ADDRESSBOOK_LDAP_SSL_ALWAYS;
		gtk_option_menu_set_history (GTK_OPTION_MENU(dialog->ssl_optionmenu),
					     dialog->ssl);

		gtk_widget_set_sensitive (dialog->ssl_optionmenu, FALSE);
	}
	else {
		gtk_widget_set_sensitive (dialog->ssl_optionmenu, TRUE);
	}
		
		
	g_free (string);
}

static void
setup_connecting_tab (AddressbookSourceDialog *dialog, GtkSignalFunc modify_func)
{
	GtkWidget *menu;
	GtkWidget *connecting_tab_help;

	dialog->connecting_modify_func = modify_func;

	connecting_tab_help = glade_xml_get_widget (dialog->gui, "connecting-tab-help");

	dialog->port_combo = glade_xml_get_widget (dialog->gui, "port-combo");
	add_focus_handler (dialog->port_combo, connecting_tab_help, 0);
	add_focus_handler (GTK_COMBO(dialog->port_combo)->entry, connecting_tab_help, 0);
	gtk_signal_connect (GTK_OBJECT (GTK_COMBO(dialog->port_combo)->entry), "changed",
			    modify_func, dialog);
	gtk_signal_connect (GTK_OBJECT (GTK_COMBO(dialog->port_combo)->entry), "changed",
			    port_changed_func, dialog);
	dialog->ssl_optionmenu = glade_xml_get_widget (dialog->gui, "ssl-optionmenu");
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU(dialog->ssl_optionmenu));
	gtk_container_foreach (GTK_CONTAINER (menu), (GtkCallback)add_ssl_activate_cb, dialog);
}

static gboolean
connecting_tab_check (AddressbookSourceDialog *dialog)
{
	gboolean valid = TRUE;
	char *string;

	string = e_utf8_gtk_entry_get_text (GTK_ENTRY (GTK_COMBO(dialog->port_combo)->entry));
	if (!string || !string[0])
		valid = FALSE;
	g_free (string);

	return valid;
}

static void
druid_connecting_page_modify_cb (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	gnome_druid_set_buttons_sensitive (GNOME_DRUID(dialog->druid),
					   TRUE, /* back */
					   connecting_tab_check (dialog), /* next */
					   TRUE /* cancel */);
}

static void
druid_connecting_page_prepare (GnomeDruidPage *dpage, GtkWidget *gdruid, AddressbookSourceDialog *dialog)
{
	druid_connecting_page_modify_cb (NULL, dialog);
	/* stick the focus in the port combo */
	gtk_widget_grab_focus (GTK_COMBO(dialog->port_combo)->entry);
}


/* searching page */
static ETableMemoryStoreColumnInfo bases_table_columns[] = {
	E_TABLE_MEMORY_STORE_STRING,
	E_TABLE_MEMORY_STORE_TERMINATOR
};

#define BASES_TABLE_SPEC \
"<ETableSpecification cursor-mode=\"line\" no-headers=\"true\"> \
  <ETableColumn model_col= \"0\" _title=\"Base\" expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableState> \
    <column source=\"0\"/> \
    <grouping></grouping> \
  </ETableState> \
</ETableSpecification>"

GtkWidget*
supported_bases_create_table (char *name, char *string1, char *string2, int num1, int num2)
{
	GtkWidget *table;
	ETableModel *model;

	model = e_table_memory_store_new (bases_table_columns);

	table = e_table_scrolled_new (model, NULL, BASES_TABLE_SPEC, NULL);

	gtk_object_set_data (GTK_OBJECT (table), "model", model);

	return table;
}

static gboolean
do_ldap_root_dse_query (ETableModel *model, AddressbookSource *source, char ***rvalues)
{
	LDAP* ldap;
	char *attrs[2];
	int ldap_error;
	char **values;
	LDAPMessage *resp;
	int i;

	ldap = addressbook_ldap_init (source);
	if (!ldap)
		return FALSE;

	if (LDAP_SUCCESS != addressbook_ldap_auth (source, ldap))
		goto fail;

	attrs[0] = "namingContexts";
	attrs[1] = NULL;

	ldap_error = addressbook_root_dse_query (source, ldap, attrs, &resp);

	if (ldap_error != LDAP_SUCCESS)
		goto fail;

	values = ldap_get_values (ldap, resp, "namingContexts");
	if (!values || values[0] == NULL) {
		gnome_ok_dialog (_("The server responded with no supported search bases"));
		goto fail;
	}

	for (i = 0; values[i]; i++)
		e_table_memory_store_insert (E_TABLE_MEMORY_STORE (model),
					     -1, GINT_TO_POINTER(i), values[i]);

	*rvalues = values;

	ldap_unbind_s (ldap);
	return TRUE;

 fail:
	ldap_unbind_s (ldap);
	return FALSE;
}

static void
search_base_selection_model_changed (ESelectionModel *selection_model, GtkWidget *dialog)
{
	gnome_dialog_set_sensitive (GNOME_DIALOG (dialog),
				    0 /* OK */, e_selection_model_selected_count (selection_model) == 1);
}

static void
query_for_supported_bases (GtkWidget *button, AddressbookSourceDialog *sdialog)
{
	ESelectionModel *selection_model;
	AddressbookSource *source = addressbook_dialog_get_source (sdialog);
	GtkWidget *dialog;
	GtkWidget *supported_bases_table;
	ETableModel *model;
	int id;
	char **values;

	dialog = glade_xml_get_widget (sdialog->gui, "supported-bases-dialog");

	supported_bases_table = glade_xml_get_widget (sdialog->gui, "supported-bases-table");
	selection_model = e_table_get_selection_model (e_table_scrolled_get_table (E_TABLE_SCROLLED(supported_bases_table)));
	model = gtk_object_get_data (GTK_OBJECT (supported_bases_table), "model");

	gtk_signal_connect (GTK_OBJECT (selection_model), "selection_changed",
			    search_base_selection_model_changed, dialog);

	search_base_selection_model_changed (selection_model, dialog);

	if (do_ldap_root_dse_query (model, source, &values)) {
		gnome_dialog_close_hides (GNOME_DIALOG(dialog), TRUE);

		id = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));

		if (id == 0) {
			int i;
			/* OK was clicked */

			/* ugh. */
			for (i = 0; values[i]; i ++) {
				if (e_selection_model_is_row_selected (selection_model, i)) {
					e_utf8_gtk_entry_set_text (GTK_ENTRY (sdialog->rootdn), values[i]);
					break; /* single selection, so we can quit when we've found it. */
				}
			}
		}

		ldap_value_free (values);

		e_table_memory_store_clear (E_TABLE_MEMORY_STORE (model));
	}

	addressbook_source_free (source);

	gtk_object_unref (GTK_OBJECT (selection_model));
}

static void
scope_optionmenu_activated (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	dialog->scope = g_list_index (gtk_container_children (GTK_CONTAINER (item->parent)),
				      item);

	dialog->searching_modify_func (item, dialog);
}

static void
add_scope_activate_cb (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (scope_optionmenu_activated), dialog);
}

static void
setup_searching_tab (AddressbookSourceDialog *dialog, GtkSignalFunc modify_func)
{
	GtkWidget *menu;
	GtkWidget *rootdn_button;
	GtkWidget *searching_tab_help;

	dialog->searching_modify_func = modify_func;

	searching_tab_help = glade_xml_get_widget (dialog->gui, "searching-tab-help");

	dialog->rootdn = glade_xml_get_widget (dialog->gui, "rootdn-entry");
	add_focus_handler (dialog->rootdn, searching_tab_help, 0);
	if (modify_func)
		gtk_signal_connect (GTK_OBJECT (dialog->rootdn), "changed",
				    modify_func, dialog);

	dialog->scope_optionmenu = glade_xml_get_widget (dialog->gui, "scope-optionmenu");
	add_focus_handler (dialog->scope_optionmenu, searching_tab_help, 1);
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU(dialog->scope_optionmenu));
	gtk_container_foreach (GTK_CONTAINER (menu), (GtkCallback)add_scope_activate_cb, dialog);

	dialog->timeout_scale = glade_xml_get_widget (dialog->gui, "timeout-scale");
	add_focus_handler (dialog->timeout_scale, searching_tab_help, 2);
	if (modify_func)
		gtk_signal_connect (GTK_OBJECT (GTK_RANGE(dialog->timeout_scale)->adjustment),
				    "value_changed",
				    modify_func, dialog);

	dialog->limit_spinbutton = glade_xml_get_widget (dialog->gui, "download-limit-spinbutton");
	if (modify_func)
		gtk_signal_connect (GTK_OBJECT (dialog->limit_spinbutton), "changed",
				    modify_func, dialog);

	/* special handling for the "Show Supported Bases button" */
	rootdn_button = glade_xml_get_widget (dialog->gui, "rootdn-button");
	gtk_signal_connect (GTK_OBJECT (rootdn_button), "clicked",
			    GTK_SIGNAL_FUNC(query_for_supported_bases), dialog);
}

static void
druid_searching_page_prepare (GnomeDruidPage *dpage, GtkWidget *gdruid, AddressbookSourceDialog *dialog)
{
	gnome_druid_set_buttons_sensitive (GNOME_DRUID(dialog->druid),
					   TRUE, /* back */
					   TRUE, /* next */
					   TRUE /* cancel */);
}


/* display name page */
static gboolean
display_name_check (AddressbookSourceDialog *dialog)
{
	gboolean valid = TRUE;
	char *string;

	string = e_utf8_gtk_entry_get_text (GTK_ENTRY (dialog->display_name));
	if (!string || !string[0])
		valid = FALSE;
	g_free (string);

	return valid;
}

static void
display_name_page_prepare (GtkWidget *page, GtkWidget *gnome_druid, AddressbookSourceDialog *dialog)
{
	if (!dialog->display_name_changed) {
		char *server_name = e_utf8_gtk_entry_get_text (GTK_ENTRY (dialog->host));
		e_utf8_gtk_entry_set_text (GTK_ENTRY (dialog->display_name), server_name);
		g_free (server_name);
	}

	gnome_druid_set_buttons_sensitive (GNOME_DRUID(dialog->druid),
					   TRUE, /* back */
					   display_name_check (dialog), /* next */
					   TRUE /* cancel */);
}

static void
druid_display_name_page_modify_cb (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	dialog->display_name_changed = TRUE;
	display_name_page_prepare (NULL, NULL, dialog);
}


#ifdef NEW_ADVANCED_UI
/* objectclasses page */
static ETableMemoryStoreColumnInfo objectclasses_table_columns[] = {
	E_TABLE_MEMORY_STORE_STRING,
	E_TABLE_MEMORY_STORE_TERMINATOR
};

#define OBJECTCLASSES_TABLE_SPEC \
"<ETableSpecification cursor-mode=\"line\" no-headers=\"true\"> \
  <ETableColumn model_col= \"0\" _title=\"Objectclass\" expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableState> \
    <column source=\"0\"/> \
    <grouping> <leaf column=\"0\" ascending=\"true\"/> </grouping> \
  </ETableState> \
</ETableSpecification>"

GtkWidget*
objectclasses_create_server_table (char *name, char *string1, char *string2,
				   int num1, int num2)
{
	GtkWidget *table;
	ETableModel *model;

	model = e_table_memory_store_new (objectclasses_table_columns);

	table = e_table_scrolled_new (model, NULL, OBJECTCLASSES_TABLE_SPEC, NULL);

	gtk_object_set_data (GTK_OBJECT (table), "model", model);

	return table;
}

GtkWidget*
objectclasses_create_evolution_table (char *name, char *string1, char *string2,
				      int num1, int num2)
{
	GtkWidget *table;
	ETableModel *model;

	model = e_table_memory_store_new (objectclasses_table_columns);

	table = e_table_scrolled_new (model, NULL, OBJECTCLASSES_TABLE_SPEC, NULL);

	gtk_object_set_data (GTK_OBJECT (table), "model", model);

	return table;
}

static void
objectclasses_add_foreach (int model_row, AddressbookSourceDialog *dialog)
{
	LDAPObjectClass *oc = e_table_memory_get_data (E_TABLE_MEMORY (dialog->objectclasses_server_model), model_row);	
	e_table_memory_store_remove (E_TABLE_MEMORY_STORE (dialog->objectclasses_server_model), model_row);
	/* XXX remove from the server array */
	e_table_memory_store_insert (E_TABLE_MEMORY_STORE (dialog->objectclasses_evolution_model),
				     -1, oc, oc->oc_names[0]);
	/* XXX add to the evolution array */
}

static void
objectclasses_add (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	ESelectionModel *esm = e_table_get_selection_model (e_table_scrolled_get_table (E_TABLE_SCROLLED(dialog->objectclasses_server_table)));

	e_selection_model_foreach (esm, (EForeachFunc)objectclasses_add_foreach, dialog);
	dialog->objectclasses_modify_func (item, dialog);
}

static void
objectclasses_server_double_click (ETable *et, int row, int col, GdkEvent *event, AddressbookSourceDialog *dialog)
{
	objectclasses_add_foreach (row, dialog);
	dialog->objectclasses_modify_func (GTK_WIDGET (et), dialog);
}

static void
objectclasses_remove_foreach (int model_row, AddressbookSourceDialog *dialog)
{
	LDAPObjectClass *oc = e_table_memory_get_data (E_TABLE_MEMORY (dialog->objectclasses_evolution_model), model_row);
	e_table_memory_store_remove (E_TABLE_MEMORY_STORE (dialog->objectclasses_evolution_model), model_row);
	/* XXX remove from the evolution array */
	e_table_memory_store_insert (E_TABLE_MEMORY_STORE (dialog->objectclasses_server_model),
				     -1, oc, oc->oc_names[0]);
	/* XXX add to the server array */
}

static void
objectclasses_remove (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	ESelectionModel *esm = e_table_get_selection_model (e_table_scrolled_get_table (E_TABLE_SCROLLED(dialog->objectclasses_evolution_table)));

	e_selection_model_foreach (esm, (EForeachFunc)objectclasses_add_foreach, dialog);

	dialog->objectclasses_modify_func (item, dialog);
}

static void
objectclasses_evolution_double_click (ETable *et, int row, int col, GdkEvent *event, AddressbookSourceDialog *dialog)
{
	objectclasses_remove_foreach (row, dialog);
	dialog->objectclasses_modify_func (GTK_WIDGET (et), dialog);
}

static void
objectclasses_restore_default (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	int i;
	
	dialog->objectclasses_modify_func (item, dialog);

	/* clear out our evolution list */
	for (i = 0; i < dialog->evolution_objectclasses->len; i ++) {
		g_ptr_array_add (dialog->server_objectclasses, g_ptr_array_index (dialog->evolution_objectclasses, i));
	}
	g_ptr_array_set_size (dialog->evolution_objectclasses, 0);

	e_table_memory_store_clear (E_TABLE_MEMORY_STORE (dialog->objectclasses_evolution_model));

	for (i = 0; i < dialog->default_objectclasses->len; i++) {
		LDAPObjectClass *oc = g_ptr_array_index (dialog->default_objectclasses, i);
		g_ptr_array_add (dialog->evolution_objectclasses, oc);
		e_table_memory_store_insert (E_TABLE_MEMORY_STORE (dialog->objectclasses_evolution_model),
					     i, oc, oc->oc_names[0]);
	}
}

static void
server_selection_model_changed (ESelectionModel *selection_model, AddressbookSourceDialog *dialog)
{
	gtk_widget_set_sensitive (dialog->objectclasses_add_button,
				  e_selection_model_selected_count (selection_model) > 0);
}

static void
evolution_selection_model_changed (ESelectionModel *selection_model, AddressbookSourceDialog *dialog)
{
	gtk_widget_set_sensitive (dialog->objectclasses_remove_button,
				  e_selection_model_selected_count (selection_model) > 0);
}

static void
setup_objectclasses_tab (AddressbookSourceDialog *dialog, GtkSignalFunc modify_func)
{
	ETable *table;
	GtkWidget *restore_default;
	ESelectionModel *esm;

	dialog->server_objectclasses = g_ptr_array_new ();
	dialog->evolution_objectclasses = g_ptr_array_new ();
	dialog->default_objectclasses = g_ptr_array_new ();

	dialog->objectclasses_modify_func = modify_func;

	dialog->objectclasses_server_table = glade_xml_get_widget (dialog->gui, "objectclasses-server-table");
	dialog->objectclasses_server_model = gtk_object_get_data (GTK_OBJECT (dialog->objectclasses_server_table), "model");

	dialog->objectclasses_evolution_table = glade_xml_get_widget (dialog->gui, "objectclasses-evolution-table");
	dialog->objectclasses_evolution_model = gtk_object_get_data (GTK_OBJECT (dialog->objectclasses_evolution_table), "model");

	table = e_table_scrolled_get_table (E_TABLE_SCROLLED(dialog->objectclasses_server_table));
	gtk_signal_connect (GTK_OBJECT (table), "double_click",
			    GTK_SIGNAL_FUNC (objectclasses_server_double_click), dialog);
	esm = e_table_get_selection_model (table);
	gtk_signal_connect (GTK_OBJECT (esm), "selection_changed",
			    server_selection_model_changed, dialog);

	table = e_table_scrolled_get_table (E_TABLE_SCROLLED(dialog->objectclasses_evolution_table));
	gtk_signal_connect (GTK_OBJECT (table), "double_click",
			    GTK_SIGNAL_FUNC (objectclasses_evolution_double_click), dialog);
	esm = e_table_get_selection_model (table);
	gtk_signal_connect (GTK_OBJECT (esm), "selection_changed",
			    evolution_selection_model_changed, dialog);

	dialog->objectclasses_add_button = glade_xml_get_widget (dialog->gui, "objectclasses-add-button");
	gtk_signal_connect (GTK_OBJECT (dialog->objectclasses_add_button), "clicked",
			    GTK_SIGNAL_FUNC(objectclasses_add), dialog);

	dialog->objectclasses_remove_button = glade_xml_get_widget (dialog->gui, "objectclasses-remove-button");
	gtk_signal_connect (GTK_OBJECT (dialog->objectclasses_remove_button), "clicked",
			    GTK_SIGNAL_FUNC(objectclasses_remove), dialog);

	restore_default = glade_xml_get_widget (dialog->gui, "objectclasses-default-button");
	gtk_signal_connect (GTK_OBJECT (restore_default), "clicked",
			    GTK_SIGNAL_FUNC(objectclasses_restore_default), dialog);
}
#endif


static AddressbookSourceDialog *
addressbook_add_server_druid (AddressbookDialog *dialog)
{
	AddressbookSourceDialog *sdialog = g_new0 (AddressbookSourceDialog, 1);
	GtkWidget *page;

	sdialog->addressbook_dialog = dialog;

	sdialog->gui = glade_xml_new (EVOLUTION_GLADEDIR "/" GLADE_FILE_NAME, NULL);

	sdialog->window = glade_xml_get_widget (sdialog->gui, "account-druid-window");
	sdialog->druid = glade_xml_get_widget (sdialog->gui, "account-druid");

	/* info page */
	page = glade_xml_get_widget (sdialog->gui, "add-server-druid-info-page");
	reparent_to_vbox (sdialog, "account-druid-general-vbox", "general-tab");
	setup_general_tab (sdialog, GTK_SIGNAL_FUNC (druid_info_page_modify_cb));
	gtk_signal_connect (GTK_OBJECT(page), "prepare",
			    GTK_SIGNAL_FUNC(druid_info_page_prepare), sdialog);

	/* connecting page */
	page = glade_xml_get_widget (sdialog->gui, "add-server-druid-connecting-page");
	reparent_to_vbox (sdialog, "account-druid-connecting-vbox", "connecting-tab");
	setup_connecting_tab (sdialog, druid_connecting_page_modify_cb);
	gtk_signal_connect (GTK_OBJECT(page), "prepare",
			    GTK_SIGNAL_FUNC(druid_connecting_page_prepare), sdialog);

	/* searching page */
	page = glade_xml_get_widget (sdialog->gui, "add-server-druid-searching-page");
	reparent_to_vbox (sdialog, "account-druid-searching-vbox", "searching-tab");
	setup_searching_tab (sdialog, NULL);
	gtk_signal_connect (GTK_OBJECT(page), "prepare",
			    GTK_SIGNAL_FUNC(druid_searching_page_prepare), sdialog);

	/* display name page */
	page = glade_xml_get_widget (sdialog->gui, "add-server-druid-display-name-page");
	sdialog->display_name = glade_xml_get_widget (sdialog->gui, "druid-display-name-entry");
	gtk_signal_connect (GTK_OBJECT (sdialog->display_name), "changed",
			    druid_display_name_page_modify_cb, sdialog);
	gtk_signal_connect (GTK_OBJECT(page), "prepare",
			    GTK_SIGNAL_FUNC(display_name_page_prepare), sdialog);

	page = glade_xml_get_widget (sdialog->gui, "add-server-druid-finish-page");
	gtk_signal_connect (GTK_OBJECT(page), "finish",
			    GTK_SIGNAL_FUNC(addressbook_add_server_druid_finish), sdialog);
	gtk_signal_connect (GTK_OBJECT(sdialog->druid), "cancel",
			    GTK_SIGNAL_FUNC(addressbook_add_server_druid_cancel), sdialog);
	gtk_signal_connect (GTK_OBJECT(sdialog->window), "destroy",
			    GTK_SIGNAL_FUNC(addressbook_source_dialog_destroy), sdialog);

	gtk_window_set_modal (GTK_WINDOW (sdialog->window), TRUE);

	gtk_widget_show (sdialog->window);

	return sdialog;
}

static void
editor_modify_cb (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	gboolean valid = TRUE;

	valid = display_name_check (dialog);
	if (valid)
		valid = general_tab_check (dialog);
#if 0
	if (valid)
		valid = connecting_tab_check (dialog);
	if (valid)
		valid = searching_tab_check (dialog);
#endif

	gtk_widget_set_sensitive (dialog->ok_button, valid);
	gtk_widget_set_sensitive (dialog->apply_button, valid);
}

static void
set_advanced_button_state (AddressbookSourceDialog *dialog)
{
	if (dialog->advanced) {
		gtk_notebook_set_page (GTK_NOTEBOOK(dialog->advanced_button_notebook), 0);
#ifdef NEW_ADVANCED_UI
		gtk_notebook_append_page (GTK_NOTEBOOK(dialog->notebook), dialog->objectclasses_tab, dialog->objectclasses_label);
		gtk_notebook_append_page (GTK_NOTEBOOK(dialog->notebook), dialog->mappings_tab, dialog->mappings_label);
		gtk_notebook_append_page (GTK_NOTEBOOK(dialog->notebook), dialog->dn_customization_tab, dialog->dn_customization_label);
#endif
	}
	else {
		gtk_notebook_set_page (GTK_NOTEBOOK(dialog->advanced_button_notebook), 1);
		
		/* hide the advanced tabs of the main notebook */
		gtk_notebook_remove_page (GTK_NOTEBOOK(dialog->notebook), 5);
		gtk_notebook_remove_page (GTK_NOTEBOOK(dialog->notebook), 4);
		gtk_notebook_remove_page (GTK_NOTEBOOK(dialog->notebook), 3);
	}
}

#ifdef NEW_ADVANCED_UI
static void
advanced_button_clicked (GtkWidget *button, AddressbookSourceDialog *dialog)
{
	dialog->advanced = !dialog->advanced;
	set_advanced_button_state (dialog);
}

static gboolean
do_schema_query (AddressbookSourceDialog *sdialog)
{
	LDAP *ldap;
	int ldap_error;
	char *schema_dn;
	char *attrs[3];
	char **values;
	int i;
	AddressbookSource *source = addressbook_dialog_get_source (sdialog);
	LDAPMessage *resp;
	struct timeval timeout;

	ldap = addressbook_ldap_init (source);
	if (!ldap)
		goto fail;

	if (LDAP_SUCCESS != addressbook_ldap_auth (source, ldap))
		goto fail;

	attrs[0] = "subschemaSubentry";
	attrs[1] = NULL;

	ldap_error = addressbook_root_dse_query (source, ldap, attrs, &resp);

	if (ldap_error != LDAP_SUCCESS)
		goto fail;

	values = ldap_get_values (ldap, resp, "subschemaSubentry");
	if (!values || values[0] == NULL) {
		gnome_ok_dialog (_("The server did not respond with a schema entry"));
		goto fail;
	}

	schema_dn = g_strdup (values[0]);

	ldap_value_free (values);
	ldap_msgfree (resp);

	attrs[0] = "objectClasses";
	attrs[1] = NULL;

	/* 3 second timeout */
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;

	ldap_error = ldap_search_ext_s (ldap, schema_dn, LDAP_SCOPE_BASE,
					"(objectClass=subschema)", attrs, 0,
					NULL, NULL, &timeout, LDAP_NO_LIMIT, &resp);
	if (LDAP_SUCCESS != ldap_error) {
		gnome_error_dialog (_("Could not query for schema information"));
		goto fail;
	}

	values = ldap_get_values (ldap, resp, "objectClasses");
	if (!values) {
		gnome_error_dialog (_("Server did not respond with valid schema information"));
		goto fail;
	}

	for (i = 0; values[i]; i ++) { 
		int j;
		int code;
		const char *err;
		LDAPObjectClass *oc = ldap_str2objectclass (values[i], &code, &err, 0);

		if (!oc)
			continue;

		/* we fill in the default list of classes here */
		for (j = 0; oc->oc_names[j]; j ++) {
			if (!g_strcasecmp (oc->oc_names[j], EVOLUTIONPERSON) ||
			    !g_strcasecmp (oc->oc_names[j], INETORGPERSON) ||
			    !g_strcasecmp (oc->oc_names[j], ORGANIZATIONALPERSON) ||
			    !g_strcasecmp (oc->oc_names[j], PERSON) ||
			    !g_strcasecmp (oc->oc_names[j], CALENTRY) ||
			    !g_strcasecmp (oc->oc_names[j], TOP))
				g_ptr_array_add (sdialog->default_objectclasses, oc);
		}

		g_ptr_array_add (sdialog->server_objectclasses, oc);
	}

	addressbook_source_free (source);
	ldap_unbind_s (ldap);
	return TRUE;

 fail:
	addressbook_source_free (source);
	if (ldap)
		ldap_unbind_s (ldap);
	return FALSE;
}

static void
edit_dialog_switch_page (GtkNotebook *notebook,
			 GtkNotebookPage *page, guint page_num,
			 AddressbookSourceDialog *sdialog)
{
	if (page_num >= 3 && !sdialog->schema_query_successful) {
		int i;

		gtk_widget_set_sensitive (GTK_WIDGET (notebook), FALSE);

		sdialog->schema_query_successful = do_schema_query (sdialog);

		if (sdialog->schema_query_successful) {
			/* fill in the objectclasses model */
			for (i = 0; i < sdialog->server_objectclasses->len; i ++) {
				LDAPObjectClass *oc = g_ptr_array_index (sdialog->server_objectclasses, i);
				e_table_memory_store_insert (E_TABLE_MEMORY_STORE (sdialog->objectclasses_server_model),
							     -1, oc, oc->oc_names[0]);
			}
			gtk_widget_set_sensitive (page->child, TRUE);
		}
		else {
			gtk_widget_set_sensitive (page->child, FALSE);
		}

		gtk_widget_set_sensitive (GTK_WIDGET (notebook), TRUE);
	}
}
#endif

static gboolean
edit_dialog_store_change (AddressbookSourceDialog *sdialog)
{
	AddressbookSource *source = addressbook_dialog_get_source (sdialog);
	AddressbookDialog *dialog = sdialog->addressbook_dialog;
	AddressbookSource *old_source;

	/* check the display name for uniqueness */
	if (FALSE /* XXX */) {
		return FALSE;
	}

	/* store the new source in the addressbook dialog */
	old_source = e_table_memory_get_data (E_TABLE_MEMORY (dialog->sourcesModel), sdialog->source_model_row);
	addressbook_source_free (old_source);
	e_table_memory_store_remove (E_TABLE_MEMORY_STORE (dialog->sourcesModel),
				     sdialog->source_model_row);

	e_table_memory_store_insert (E_TABLE_MEMORY_STORE (dialog->sourcesModel),
				     sdialog->source_model_row, source, source->name, source->host);

	/* and let the config control know about the change */
	evolution_config_control_changed (dialog->config_control);

	return TRUE;
}

static void
edit_dialog_apply_clicked (GtkWidget *item, AddressbookSourceDialog *sdialog)
{
	if (!edit_dialog_store_change (sdialog))
		return;

	/* resensitize the buttons */
	gtk_widget_set_sensitive (sdialog->ok_button, FALSE);
	gtk_widget_set_sensitive (sdialog->apply_button, FALSE);
}

static void
edit_dialog_close_clicked (GtkWidget *item, AddressbookSourceDialog *sdialog)
{
	gtk_widget_destroy (sdialog->window);
}

static void
edit_dialog_ok_clicked (GtkWidget *item, AddressbookSourceDialog *sdialog)
{
	if (edit_dialog_store_change (sdialog))
		edit_dialog_close_clicked (item, sdialog);
}

static AddressbookSourceDialog*
addressbook_edit_server_dialog (AddressbookDialog *dialog, AddressbookSource *source, int model_row)
{
	AddressbookSourceDialog *sdialog = g_new0 (AddressbookSourceDialog, 1);
	GtkWidget *general_tab_help;
	GtkWidget *fewer_options_button, *more_options_button;

	sdialog->addressbook_dialog = dialog;
	sdialog->source_model_row = model_row;

	sdialog->gui = glade_xml_new (EVOLUTION_GLADEDIR "/" GLADE_FILE_NAME, NULL);

	sdialog->window = glade_xml_get_widget (sdialog->gui, "account-editor-window");

	/* general tab */
	general_tab_help = glade_xml_get_widget (dialog->gui, "general-tab-help");
	reparent_to_vbox (sdialog, "account-editor-general-vbox", "general-tab");
	setup_general_tab (sdialog, GTK_SIGNAL_FUNC (editor_modify_cb));
	sdialog->display_name = glade_xml_get_widget (sdialog->gui, "account-editor-display-name-entry");
	gtk_signal_connect (GTK_OBJECT (sdialog->display_name), "changed",
			    editor_modify_cb, sdialog);
	add_focus_handler (sdialog->display_name, general_tab_help, 4);

	/* connecting tab */
	reparent_to_vbox (sdialog, "account-editor-connecting-vbox", "connecting-tab");
	setup_connecting_tab (sdialog, GTK_SIGNAL_FUNC (editor_modify_cb));

	/* searching tab */
	reparent_to_vbox (sdialog, "account-editor-searching-vbox", "searching-tab");
	setup_searching_tab (sdialog, GTK_SIGNAL_FUNC (editor_modify_cb));

#ifdef NEW_ADVANCED_UI
	/* objectclasses tab */
	reparent_to_vbox (sdialog, "account-editor-objectclasses-vbox", "objectclasses-tab");
	setup_objectclasses_tab (sdialog, GTK_SIGNAL_FUNC (editor_modify_cb));

	/* mappings tab */
	reparent_to_vbox (sdialog, "account-editor-mappings-vbox", "mappings-tab");
	/* XXX setup_mappings_tab */

	/* dn customization tab */
	reparent_to_vbox (sdialog, "account-editor-dn-customization-vbox", "dn-customization-tab");
	/* XXX setup_dn_customization_tab */
#endif

	sdialog->ok_button = glade_xml_get_widget (sdialog->gui, "account-editor-ok-button");
	sdialog->apply_button = glade_xml_get_widget (sdialog->gui, "account-editor-apply-button");
	sdialog->close_button = glade_xml_get_widget (sdialog->gui, "account-editor-close-button");

	sdialog->advanced_button_notebook = glade_xml_get_widget (sdialog->gui, "account-editor-advanced-button-notebook");
	fewer_options_button = glade_xml_get_widget (sdialog->gui, "account-editor-fewer-options-button");
	more_options_button = glade_xml_get_widget (sdialog->gui, "account-editor-more-options-button");

	sdialog->notebook = glade_xml_get_widget (sdialog->gui, "account-editor-notebook");
#ifdef NEW_ADVANCED_UI
	sdialog->objectclasses_label = glade_xml_get_widget (sdialog->gui, "account-editor-objectclasses-label");
	gtk_object_ref (GTK_OBJECT (sdialog->objectclasses_label));
	sdialog->objectclasses_tab = glade_xml_get_widget (sdialog->gui, "account-editor-objectclasses-vbox");
	gtk_object_ref (GTK_OBJECT (sdialog->objectclasses_tab));
	sdialog->mappings_label = glade_xml_get_widget (sdialog->gui, "account-editor-mappings-label");
	gtk_object_ref (GTK_OBJECT (sdialog->mappings_label));
	sdialog->mappings_tab = glade_xml_get_widget (sdialog->gui, "account-editor-mappings-vbox");
	gtk_object_ref (GTK_OBJECT (sdialog->mappings_tab));
	sdialog->dn_customization_label = glade_xml_get_widget (sdialog->gui, "account-editor-dn-customization-label");
	gtk_object_ref (GTK_OBJECT (sdialog->dn_customization_label));
	sdialog->dn_customization_tab = glade_xml_get_widget (sdialog->gui, "account-editor-dn-customization-vbox");
	gtk_object_ref (GTK_OBJECT (sdialog->dn_customization_tab));
#endif

	addressbook_source_dialog_set_source (sdialog, source);

	set_advanced_button_state (sdialog);

#ifdef NEW_ADVANCED_UI
	gtk_signal_connect (GTK_OBJECT (fewer_options_button),
			    "clicked", advanced_button_clicked, sdialog);
	gtk_signal_connect (GTK_OBJECT (more_options_button),
			    "clicked", advanced_button_clicked, sdialog);

#else
	gtk_widget_hide (sdialog->advanced_button_notebook);
#endif

#ifdef NEW_ADVANCED_UI
	/* set up a signal handler to query for schema info if the user switches to the advanced tabs */
	gtk_signal_connect (GTK_OBJECT (sdialog->notebook), "switch_page",
			    GTK_SIGNAL_FUNC (edit_dialog_switch_page), sdialog);
#endif

	gtk_signal_connect (GTK_OBJECT (sdialog->ok_button),
			    "clicked", GTK_SIGNAL_FUNC(edit_dialog_ok_clicked), sdialog);
	gtk_signal_connect (GTK_OBJECT (sdialog->apply_button),
			    "clicked", GTK_SIGNAL_FUNC(edit_dialog_apply_clicked), sdialog);
	gtk_signal_connect (GTK_OBJECT (sdialog->close_button),
			    "clicked", GTK_SIGNAL_FUNC(edit_dialog_close_clicked), sdialog);
	gtk_signal_connect (GTK_OBJECT(sdialog->window), "destroy",
			    GTK_SIGNAL_FUNC(addressbook_source_dialog_destroy), sdialog);

	gtk_widget_set_sensitive (sdialog->ok_button, FALSE);
	gtk_widget_set_sensitive (sdialog->apply_button, FALSE);

	gtk_window_set_modal (GTK_WINDOW (sdialog->window), TRUE);

	gtk_widget_show (sdialog->window);

	return sdialog;
}

static void
add_source_clicked (GtkWidget *widget, AddressbookDialog *dialog)
{
	addressbook_add_server_druid (dialog);
}

static void
edit_source_clicked (GtkWidget *widget, AddressbookDialog *dialog)
{
	int i;
	ESelectionModel *selection_model;
	int row_count;
	
	selection_model = e_table_get_selection_model (e_table_scrolled_get_table (E_TABLE_SCROLLED(dialog->sourcesTable)));
	row_count = e_selection_model_row_count (selection_model);

	for (i = 0; i < row_count; i ++) {
		if (e_selection_model_is_row_selected (selection_model, i)) {
			AddressbookSource *source = e_table_memory_get_data (E_TABLE_MEMORY(dialog->sourcesModel), i);
			AddressbookSourceDialog *sdialog;
			sdialog = addressbook_edit_server_dialog (dialog, source, i);
			break; /* single select so we're done now */
		}
	}

#if 0
	AddressbookSource *source;
	AddressbookSourceDialog *sdialog;

	source = gtk_clist_get_row_data (GTK_CLIST (dialog->clistSources), dialog->source_row);

	sdialog = addressbook_config_source_with_gui (dialog->gui, source, dialog->page);
	if (sdialog->id == 0) {
		/* Ok was clicked */
		source = addressbook_source_copy(sdialog->source);

		e_utf8_gtk_clist_set_text (GTK_CLIST (dialog->clistSources), dialog->source_row, 0, source->name);
		e_utf8_gtk_clist_set_text (GTK_CLIST (dialog->clistSources), dialog->source_row, 1, source->host);
		gtk_clist_set_row_data (GTK_CLIST (dialog->clistSources), dialog->source_row, source);

		evolution_config_control_changed (dialog->config_control);

		update_sensitivity (dialog);
	}
#endif
}

static void
delete_source_clicked (GtkWidget *widget, AddressbookDialog *dialog)
{
	int i;
	ESelectionModel *selection_model;
	int row_count;
	
	selection_model = e_table_get_selection_model (e_table_scrolled_get_table (E_TABLE_SCROLLED(dialog->sourcesTable)));
	row_count = e_selection_model_row_count (selection_model);

	for (i = 0; i < row_count; i ++) {
		if (e_selection_model_is_row_selected (selection_model, i)) {
			AddressbookSource *source = e_table_memory_get_data (E_TABLE_MEMORY(dialog->sourcesModel), i);
			e_table_memory_store_remove (E_TABLE_MEMORY_STORE (dialog->sourcesModel), i);
			addressbook_source_free (source);

			break; /* single select so we're done now */
		}
	}

	evolution_config_control_changed (dialog->config_control);
}

static void
ldap_config_control_destroy_callback (EvolutionConfigControl *config_control,
				      void *data)
{
	AddressbookDialog *dialog;

	dialog = (AddressbookDialog *) data;

	gtk_object_unref (GTK_OBJECT (dialog->gui));

	/* XXX free more stuff here */

	g_free (dialog);
}

static void
ldap_config_control_apply_callback (EvolutionConfigControl *config_control,
				    void *data)
{
	AddressbookDialog *dialog;
	int i;
	int count;

	dialog = (AddressbookDialog *) data;

	addressbook_storage_clear_sources();

	count = e_table_model_row_count (E_TABLE_MODEL (dialog->sourcesModel));

	for (i = 0; i < count; i ++) {
		AddressbookSource *source = e_table_memory_get_data (E_TABLE_MEMORY (dialog->sourcesModel), i);
		addressbook_storage_add_source (addressbook_source_copy (source));
	}

	addressbook_storage_write_sources();
}

static void
sources_selection_changed (ESelectionModel *esm, AddressbookDialog *dialog)
{
	gboolean sensitive = e_selection_model_selected_count (esm) == 1;

	gtk_widget_set_sensitive (dialog->editSource, sensitive);
	gtk_widget_set_sensitive (dialog->deleteSource, sensitive);
}

static AddressbookDialog *
ldap_dialog_new (GNOME_Evolution_Shell shell)
{
	AddressbookDialog *dialog;
	GList *l;
	ESelectionModel *esm;

	dialog = g_new0 (AddressbookDialog, 1);

	dialog->gui = glade_xml_new (EVOLUTION_GLADEDIR "/" GLADE_FILE_NAME, NULL);
	dialog->shell = shell;

	dialog->sourcesTable = glade_xml_get_widget (dialog->gui, "sourcesTable");
	dialog->sourcesModel = gtk_object_get_data (GTK_OBJECT (dialog->sourcesTable), "model");
	
	dialog->addSource = glade_xml_get_widget (dialog->gui, "addSource");
	gtk_signal_connect (GTK_OBJECT(dialog->addSource), "clicked",
			    GTK_SIGNAL_FUNC (add_source_clicked),
			    dialog);

	dialog->editSource = glade_xml_get_widget (dialog->gui, "editSource");
	gtk_signal_connect (GTK_OBJECT(dialog->editSource), "clicked",
			    GTK_SIGNAL_FUNC (edit_source_clicked),
			    dialog);

	dialog->deleteSource = glade_xml_get_widget (dialog->gui, "deleteSource");
	gtk_signal_connect (GTK_OBJECT(dialog->deleteSource), "clicked",
			    GTK_SIGNAL_FUNC (delete_source_clicked),
			    dialog);

	l = addressbook_storage_get_sources ();
	for (; l != NULL; l = l->next) {
		AddressbookSource *source;

		source = addressbook_source_copy ((AddressbookSource*)l->data);

		e_table_memory_store_insert (E_TABLE_MEMORY_STORE (dialog->sourcesModel),
					     -1, source, source->name, source->host);
	}

	esm = e_table_get_selection_model (e_table_scrolled_get_table (E_TABLE_SCROLLED(dialog->sourcesTable)));
	gtk_signal_connect (GTK_OBJECT (esm), "selection_changed",
			    GTK_SIGNAL_FUNC (sources_selection_changed), dialog);

	sources_selection_changed (esm, dialog);

	dialog->page = glade_xml_get_widget (dialog->gui, "addressbook-sources");

	return dialog;
}

static ETableMemoryStoreColumnInfo sources_table_columns[] = {
	E_TABLE_MEMORY_STORE_STRING,
	E_TABLE_MEMORY_STORE_STRING,
	E_TABLE_MEMORY_STORE_TERMINATOR
};

#define SOURCES_TABLE_SPEC \
"<ETableSpecification cursor-mode=\"line\"> \
  <ETableColumn model_col= \"0\" _title=\"Account Name\" expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableColumn model_col= \"1\" _title=\"Server Name\"  expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
  <ETableState> \
    <column source=\"0\"/> \
    <column source=\"1\"/> \
    <grouping></grouping> \
  </ETableState> \
</ETableSpecification>"

GtkWidget*
addressbook_dialog_create_sources_table (char *name, char *string1, char *string2, int num1, int num2)
{
	GtkWidget *table;
	ETableModel *model;
	
	model = e_table_memory_store_new (sources_table_columns);

	table = e_table_scrolled_new (model, NULL, SOURCES_TABLE_SPEC, NULL);

	gtk_object_set_data (GTK_OBJECT (table), "model", model);

	return table;
}
#endif /* HAVE_LDAP */

static EvolutionConfigControl *
ldap_config_control_new (GNOME_Evolution_Shell shell)
{
	GtkWidget *control_widget;
	EvolutionConfigControl *control;

#ifdef HAVE_LDAP
	AddressbookDialog *dialog;

	dialog = ldap_dialog_new (shell);

	control_widget = dialog->page;

	gtk_widget_ref (control_widget);

	gtk_container_remove (GTK_CONTAINER (control_widget->parent), control_widget);
#else
	control_widget = gtk_label_new (_("LDAP was not enabled in this build of Evolution"));
	gtk_widget_set_sensitive (control_widget, FALSE);
	gtk_widget_show (control_widget);
#endif

	control = evolution_config_control_new (control_widget);

#ifdef HAVE_LDAP
	dialog->config_control = control;
	gtk_signal_connect (GTK_OBJECT (dialog->config_control), "apply",
			    GTK_SIGNAL_FUNC (ldap_config_control_apply_callback), dialog);
	gtk_signal_connect (GTK_OBJECT (dialog->config_control), "destroy",
			    GTK_SIGNAL_FUNC (ldap_config_control_destroy_callback), dialog);

	gtk_widget_unref (dialog->page);
#endif

	return control;
}

static void
addressbook_folder_list_changed_callback (EFolderList *efl,
					  EvolutionConfigControl *control)
{
	evolution_config_control_changed (control);
}

static void
addressbook_config_control_destroy_callback (EvolutionConfigControl *config_control,
					     void *data)
{
}


static void
addressbook_config_control_apply_callback (EvolutionConfigControl *config_control,
					   EFolderList *list)
{
	Bonobo_ConfigDatabase config_db;
	char *xml;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	config_db = addressbook_config_database(&ev);

	xml = e_folder_list_get_xml (list);
	bonobo_config_set_string (config_db, "/Addressbook/Completion/uris", xml, &ev);
	g_free (xml);

	CORBA_exception_free (&ev);
}

static EvolutionConfigControl *
addressbook_config_control_new (GNOME_Evolution_Shell shell)
{
	GtkWidget *control_widget;
	EvolutionConfigControl *control;
	EvolutionShellClient *shell_client;
	Bonobo_ConfigDatabase config_db;
	char *xml;
	CORBA_Environment ev;
	static const char *possible_types[] = { "contacts", NULL };

	CORBA_exception_init (&ev);

	config_db = addressbook_config_database(&ev);

	xml = bonobo_config_get_string (config_db, "/Addressbook/Completion/uris", &ev);
	shell_client = evolution_shell_client_new (shell);
	control_widget = e_folder_list_new (shell_client,
					    xml);
	bonobo_object_client_unref (BONOBO_OBJECT_CLIENT (shell_client), NULL);
	g_free (xml);

	gtk_object_set (GTK_OBJECT (control_widget),
			"title", _("Extra Completion folders"),
			"possible_types", possible_types,
			NULL);

	gtk_widget_show (control_widget);

	control = evolution_config_control_new (control_widget);

	gtk_signal_connect (GTK_OBJECT (control_widget), "changed",
			    GTK_SIGNAL_FUNC (addressbook_folder_list_changed_callback), control);
	gtk_signal_connect (GTK_OBJECT (control), "apply",
			    GTK_SIGNAL_FUNC (addressbook_config_control_apply_callback), control_widget);
	gtk_signal_connect (GTK_OBJECT (control), "destroy",
			    GTK_SIGNAL_FUNC (addressbook_config_control_destroy_callback), control_widget);

	CORBA_exception_free (&ev);

	return control;
}


/* Implementation of the factory for the configuration control.  */

static BonoboGenericFactory *factory = NULL;

static BonoboObject *
config_control_factory_fn (BonoboGenericFactory *factory,
			   const char *component_id,
			   void *data)
{
	GNOME_Evolution_Shell shell;
	EvolutionConfigControl *control;

	shell = (GNOME_Evolution_Shell) data;

	if (!strcmp (component_id, LDAP_CONFIG_CONTROL_ID)) {
		control = ldap_config_control_new (shell);
	} else if (!strcmp (component_id, ADDRESSBOOK_CONFIG_CONTROL_ID)) {
		control = addressbook_config_control_new (shell);
	} else {
		control = NULL;
		g_assert_not_reached ();
	}

	return BONOBO_OBJECT (control);
}

gboolean
addressbook_config_register_factory (GNOME_Evolution_Shell shell)
{
	g_return_val_if_fail (shell != CORBA_OBJECT_NIL, FALSE);

	factory = bonobo_generic_factory_new_multi (CONFIG_CONTROL_FACTORY_ID,
						    config_control_factory_fn,
						    shell);

	if (factory != NULL) {
		return TRUE;
	} else {
		g_warning ("Cannot register factory %s", CONFIG_CONTROL_FACTORY_ID);
		return FALSE;
	}
}

void
addressbook_config_create_new_source (const char *new_source, GtkWidget *parent)
{
#ifdef HAVE_LDAP
#if 0
	AddressbookSourceDialog *dialog;
	GladeXML *gui;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/" GLADE_FILE_NAME, NULL);

	dialog = addressbook_source_dialog (gui, NULL, parent);

	e_utf8_gtk_entry_set_text (GTK_ENTRY (dialog->name), new_source);

	gnome_dialog_close_hides (GNOME_DIALOG(dialog->dialog), TRUE);

	dialog->id = gnome_dialog_run_and_close (GNOME_DIALOG (dialog->dialog));
	
	gtk_object_unref (GTK_OBJECT (dialog->gui));

	if (dialog->id == 0) {
		/* Ok was clicked */
		addressbook_storage_add_source (addressbook_source_copy(dialog->source));
		addressbook_storage_write_sources();
	}
#endif
#endif /* HAVE_LDAP */
}

#ifdef STANDALONE
int
main(int argc, char **argv)
{
	AddressbookDialog *dialog;

	gnome_init_with_popt_table ("evolution-addressbook", "0.0",
				    argc, argv, oaf_popt_options, 0, NULL);

	glade_gnome_init ();

	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

#if 0
	g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	dialog = ldap_dialog_new (NULL);

	gtk_widget_show (glade_xml_get_widget (dialog->gui, "addressbook-sources-window"));

	gtk_main();

	return 0;
}
#endif
