/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>

#include <gnome.h>
#include <gtkhtml/gtkhtml.h>
#include <glade/glade.h>
#include <gal/widgets/e-unicode.h>
#include "e-util/e-html-utils.h"
#include "addressbook-config.h"
#include "addressbook-storage.h"

/* #define INCLUDE_FILE_SOURCE */

typedef struct _AddressbookSourceDialog AddressbookSourceDialog;
typedef struct _AddressbookSourcePageItem  AddressbookSourcePageItem;
typedef struct _LDAPAuthPageItem LDAPAuthPageItem;

struct _AddressbookSourceDialog {
	GtkWidget *html;
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *name;
	GtkWidget *description;
	GtkWidget *source_option;
	GtkWidget *notebook;

	GList *source_pages;
	AddressbookSourcePageItem *source_page;

	gint id; /* button we closed the dialog with */

	AddressbookSource *source; /* our result if the Ok button was clicked */
};

struct _AddressbookSourcePageItem {
	gint pnum;

	AddressbookSourceType source_type;

	AddressbookSourceDialog *dialog;

	GtkWidget *item;
	GtkWidget *vbox;

	/* file: addressbook's */
	GtkWidget *path;
	GtkWidget *creat;

	/* ldap: addressbook's */
	GtkWidget *host;
	GtkWidget *port;
	GtkWidget *rootdn;
	GtkWidget *scope_optionmenu;
	GtkWidget *auth_optionmenu;
	GtkWidget *auth_notebook;

	GList *auths;
	LDAPAuthPageItem *auth;
};

struct _LDAPAuthPageItem {
	gint pnum;

	AddressbookLDAPAuthType auth_type;

	AddressbookSourceDialog *dialog;
	AddressbookSourcePageItem *page;

	GtkWidget *item;
	GtkWidget *vbox;

	/* simple (password) auth */
	GtkWidget *binddn;
	GtkWidget *remember_passwd;
};

static void
html_size_req (GtkWidget *widget, GtkRequisition *requisition)
{
	 requisition->height = GTK_LAYOUT (widget)->height;
}

/* Returns a GtkHTML which is already inside a GtkScrolledWindow. If
 * @white is TRUE, the GtkScrolledWindow will be inside a GtkFrame.
 */
static GtkWidget *
html_new (gboolean white)
{
	GtkWidget *html, *scrolled, *frame;
	GtkStyle *style;
	
	html = gtk_html_new ();
	GTK_LAYOUT (html)->height = 0;
	gtk_signal_connect (GTK_OBJECT (html), "size_request",
			    GTK_SIGNAL_FUNC (html_size_req), NULL);
	gtk_html_set_editable (GTK_HTML (html), FALSE);
	style = gtk_rc_get_style (html);
	if (style) {
		gtk_html_set_default_background_color (GTK_HTML (html),
						       white ? &style->white :
						       &style->bg[0]);
	}
	gtk_widget_set_sensitive (html, FALSE);
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_NEVER,
					GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (scrolled), html);
	
	if (white) {
		frame = gtk_frame_new (NULL);
		gtk_frame_set_shadow_type (GTK_FRAME (frame),
					   GTK_SHADOW_ETCHED_IN);
		gtk_container_add (GTK_CONTAINER (frame), scrolled);
		gtk_widget_show_all (frame);
	} else
		gtk_widget_show_all (scrolled);
	
	return html;
}

static void
put_html (GtkHTML *html, char *text)
{
	GtkHTMLStream *handle;
	
	text = e_text_to_html (text, E_TEXT_TO_HTML_CONVERT_NL);
	handle = gtk_html_begin (html);
	gtk_html_write (html, handle, "<HTML><BODY>", 12);
	gtk_html_write (html, handle, text, strlen (text));
	gtk_html_write (html, handle, "</BODY></HTML>", 14);
	g_free (text);
	gtk_html_end (html, handle, GTK_HTML_STREAM_OK);
}

#ifdef INCLUDE_FILE_SOURCE
static const char *
addressbook_config_source_label (AddressbookSourceType type)
{
	switch (type) {
	case ADDRESSBOOK_SOURCE_LDAP:
		return _("LDAP Server");
	case ADDRESSBOOK_SOURCE_FILE:
		return _("File");
	default:
		g_assert(0);
		return _("Unknown addressbook type");
	}
}
#endif

static const char *
addressbook_config_auth_label (AddressbookLDAPAuthType type)
{
	switch (type) {
	case ADDRESSBOOK_LDAP_AUTH_NONE:
		return _("None (anonymous mode)");
	case ADDRESSBOOK_LDAP_AUTH_SIMPLE:
		return _("Password");
	case ADDRESSBOOK_LDAP_AUTH_SASL:
		return _("SASL");
	default:
		g_assert(0);
		return _("Unknown auth type");
	}
}

static const char *
addressbook_config_scope_label (AddressbookLDAPScopeType scope)
{
	switch (scope) {
	case ADDRESSBOOK_LDAP_SCOPE_BASE:
		return _("Base");
	case ADDRESSBOOK_LDAP_SCOPE_ONELEVEL:
		return _("One");
	case ADDRESSBOOK_LDAP_SCOPE_SUBTREE:
		return _("Subtree");
	default:
		g_assert (0);
		return _("Unknown scope type");
	}
}

static void
addressbook_source_edit_changed (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	char *data;
	gboolean complete = TRUE;
	AddressbookSourcePageItem *source_page = dialog->source_page;

	if (source_page == NULL)
		complete = FALSE;

	if (complete) {
		data = e_utf8_gtk_editable_get_chars (GTK_EDITABLE (dialog->name), 0, -1);
		if (!data || !*data)
			complete = FALSE;
		g_free (data);
	}

	if (complete) {
		if (source_page->source_type == ADDRESSBOOK_SOURCE_FILE) {
			if (complete) {
				data = e_utf8_gtk_editable_get_chars (GTK_EDITABLE (source_page->path), 0, -1);
				if (!data || !*data)
					complete = FALSE;
				g_free (data);
			}
		}
		else {
			if (complete) {
				data = e_utf8_gtk_editable_get_chars (GTK_EDITABLE (source_page->host), 0, -1);
				if (!data || !*data)
					complete = FALSE;
				g_free (data);
			}

			if (complete) {
				data = e_utf8_gtk_editable_get_chars (GTK_EDITABLE (source_page->port), 0, -1);
				if (!data || !*data)
					complete = FALSE;
				/* XXX more validation on port here */
				g_free (data);
			}

			if (complete) {
				LDAPAuthPageItem *auth_page = source_page->auth;

				if (auth_page->auth_type == ADDRESSBOOK_LDAP_AUTH_SIMPLE) {
					data = e_utf8_gtk_editable_get_chars (GTK_EDITABLE (auth_page->binddn), 0, -1);
					if (!data || !*data)
						complete = FALSE;
					g_free (data);
				}
				else if (auth_page->auth_type == ADDRESSBOOK_LDAP_AUTH_SASL) {
				}
				data = e_utf8_gtk_editable_get_chars (GTK_EDITABLE (source_page->port), 0, -1);
				if (!data || !*data)
					complete = FALSE;
				/* XXX more validation on port here */
				g_free (data);
			}
		}
	}

	gnome_dialog_set_sensitive (GNOME_DIALOG (dialog->dialog), 0, complete);
}

static void
source_type_menuitem_activate (GtkWidget *item, gpointer data)
{
	AddressbookSourcePageItem *sitem = data;

	gtk_notebook_set_page (GTK_NOTEBOOK(sitem->dialog->notebook), sitem->pnum);
	sitem->dialog->source_page = sitem;

	addressbook_source_edit_changed (item, sitem->dialog);	
}

typedef struct {
	AddressbookSourceDialog *dialog;
	char *help_text;
} FocusHelpClosure;

static gint
focus_help (GtkWidget *widget, GdkEventFocus *event, FocusHelpClosure *closure)
{
	put_html (GTK_HTML(closure->dialog->html), closure->help_text);
	return FALSE;
}

static GtkWidget *
table_add_elem (AddressbookSourceDialog *dialog, GtkWidget *table, 
		int row,
		const char *label_text,
		const char *help_text)
{
	GtkWidget *label, *entry;
	FocusHelpClosure *focus_closure;

	label = gtk_label_new (label_text);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 
			  row, row + 1, GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

	entry = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), entry, 1, 3, row, row + 1,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);

	gtk_signal_connect (GTK_OBJECT (entry), "changed",
			    GTK_SIGNAL_FUNC (addressbook_source_edit_changed), dialog);

	focus_closure = g_new0 (FocusHelpClosure, 1);
	focus_closure->dialog = dialog;
	focus_closure->help_text = help_text;

	gtk_signal_connect_full (GTK_OBJECT (entry),
				 "focus_in_event" /* XXX */,
				 (GtkSignalFunc) focus_help, NULL,
				 focus_closure,
				 (GtkDestroyNotify) g_free,
				 FALSE, FALSE);
	return entry;
}

static void
ldap_auth_type_menuitem_activate (GtkWidget *item, gpointer data)
{
	LDAPAuthPageItem *auth_item = data;

	gtk_notebook_set_page (GTK_NOTEBOOK(auth_item->page->auth_notebook), auth_item->pnum);

	auth_item->page->auth = auth_item;

	addressbook_source_edit_changed (item, auth_item->dialog);	
}

static LDAPAuthPageItem *
addressbook_ldap_auth_item_new (AddressbookSourceDialog *dialog,
				AddressbookSourcePageItem *page,
				AddressbookLDAPAuthType type)
{
	LDAPAuthPageItem *item = g_new0 (LDAPAuthPageItem, 1);
	GtkWidget *table = NULL;
	int row = 0;

	item->pnum = type;
	item->auth_type = type;
	item->dialog = dialog;
	item->page = page;
	item->vbox = gtk_vbox_new (FALSE, 0);

	switch (type) {
	case ADDRESSBOOK_LDAP_AUTH_NONE:
		break;
	case ADDRESSBOOK_LDAP_AUTH_SIMPLE:
		table = gtk_table_new (2, 2, FALSE);
		item->binddn = table_add_elem (dialog, table, row++,
					       _("Bind DN:"),
					       _("FIXME Bind DN Help text here"));

		item->remember_passwd = gtk_check_button_new_with_label (_("Remember this password"));

		gtk_table_attach (GTK_TABLE (table), item->remember_passwd, 1, 2,
				  row, row + 1, GTK_FILL, 0, 0, 0);

		gtk_box_pack_start (GTK_BOX (item->vbox), table,
				    TRUE, TRUE, 0);
		break;
	case ADDRESSBOOK_LDAP_AUTH_SASL:
		break;
	default:
		g_assert (0);
		return item;
	}

	if (table) {
		gtk_table_set_row_spacings (GTK_TABLE (table), 2);
		gtk_table_set_col_spacings (GTK_TABLE (table), 10);
		gtk_container_set_border_width (GTK_CONTAINER (table), 8);
	}

	gtk_widget_show_all (item->vbox);

	return item;
}

static AddressbookSourcePageItem *
addressbook_source_item_new (AddressbookSourceDialog *dialog, AddressbookSourceType type)
{
	AddressbookSourcePageItem *item = g_new0 (AddressbookSourcePageItem, 1);
	GtkWidget *table = NULL;
	int row = 0;

	item->pnum = type;
	item->source_type = type;
	item->dialog = dialog;

	item->vbox = gtk_vbox_new (FALSE, 0);

	switch (type) {
	case ADDRESSBOOK_SOURCE_LDAP: {
		GtkWidget *label;
		GtkWidget *menu;
		int i;
		LDAPAuthPageItem *first_item = NULL;
		int position;

		table = gtk_table_new (5, 2, FALSE);

		item->host = table_add_elem (dialog, table, row++,
					     _("Host:"),
					     _("FIXME Host help text here."));
		item->port = table_add_elem (dialog, table, row++,
					     _("Port:"),
					     _("FIXME Port help text here."));
		gtk_editable_insert_text (GTK_EDITABLE (item->port), "389", 3, &position);

		item->rootdn = table_add_elem (dialog, table, row++,
					       _("Root DN:"),
					       _("FIXME Root DN help text here."));

		item->scope_optionmenu = gtk_option_menu_new ();
		menu = gtk_menu_new ();

		for (i = 0; i < ADDRESSBOOK_LDAP_SCOPE_LAST; i ++) {
			GtkWidget *scope_item = gtk_menu_item_new_with_label (addressbook_config_scope_label (i));

			gtk_signal_connect (GTK_OBJECT (scope_item), "activate",
					    GTK_SIGNAL_FUNC (addressbook_source_edit_changed),
					    dialog);

			gtk_menu_append (GTK_MENU (menu), scope_item);
			gtk_widget_show (scope_item);
		}

		gtk_option_menu_set_menu (GTK_OPTION_MENU (item->scope_optionmenu), menu);
		//		ldap_auth_type_menuitem_activate (first_item->item, first_item);
		gtk_option_menu_set_history (GTK_OPTION_MENU(item->scope_optionmenu), 0);

		label = gtk_label_new (_("Search Scope:"));
		gtk_table_attach (GTK_TABLE (table), label, 0, 1,
				  row, row + 1, GTK_FILL, 0, 0, 0);
		gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

		gtk_table_attach (GTK_TABLE (table), 
				  item->scope_optionmenu, 
				  1, 2, row, row + 1, 
				  GTK_EXPAND | GTK_FILL, 0,
				  0, 0);

		row++;

		gtk_box_pack_start (GTK_BOX (item->vbox), table,
				    TRUE, FALSE, 0);

		item->auth_optionmenu = gtk_option_menu_new ();
		menu = gtk_menu_new ();

		item->auth_notebook = gtk_notebook_new();
		gtk_notebook_set_show_tabs (GTK_NOTEBOOK (item->auth_notebook), FALSE);

		for (i = 0; i < ADDRESSBOOK_LDAP_AUTH_LAST; i++) {
			LDAPAuthPageItem *auth_item;

#ifndef LDAP_SASL
			/* skip the sasl stuff if we're not configured for it. */
			if (i == ADDRESSBOOK_LDAP_AUTH_SASL)
				continue;
#endif
			auth_item = addressbook_ldap_auth_item_new (dialog, item, i);

			item->auths = g_list_append (item->auths, auth_item);

			if (!first_item)
				first_item = auth_item;

			auth_item->item = gtk_menu_item_new_with_label (addressbook_config_auth_label (i));

			gtk_notebook_append_page (GTK_NOTEBOOK (item->auth_notebook),
						  auth_item->vbox, NULL);

			gtk_signal_connect (GTK_OBJECT (auth_item->item), "activate",
					    GTK_SIGNAL_FUNC (ldap_auth_type_menuitem_activate),
					    auth_item);

			gtk_menu_append (GTK_MENU (menu), auth_item->item);
			gtk_widget_show (auth_item->item);
		}

		gtk_option_menu_set_menu (GTK_OPTION_MENU (item->auth_optionmenu), menu);
		ldap_auth_type_menuitem_activate (first_item->item, first_item);
		gtk_option_menu_set_history (GTK_OPTION_MENU(item->auth_optionmenu), 0);

		label = gtk_label_new (_("Authentication:"));
		gtk_table_attach (GTK_TABLE (table), label, 0, 1,
				  row, row + 1, GTK_FILL, 0, 0, 0);
		gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

		gtk_table_attach (GTK_TABLE (table), 
				  item->auth_optionmenu, 
				  1, 2, row, row + 1, 
				  GTK_EXPAND | GTK_FILL, 0,
				  0, 0);

		gtk_box_pack_start (GTK_BOX (item->vbox), item->auth_notebook,
				    TRUE, TRUE, 0);
		break;
	}
	case ADDRESSBOOK_SOURCE_FILE: {
		table = gtk_table_new (2, 2, FALSE);
		item->path = table_add_elem (dialog, table, row++,
					     _("Path:"),
					     _("FIXME Path Help text here"));

		gtk_box_pack_start (GTK_BOX (item->vbox), table,
				    TRUE, TRUE, 0);

		item->creat = gtk_check_button_new_with_label (_("Create path if it doesn't exist."));
		gtk_table_attach (GTK_TABLE (table), item->creat, 1, 2,
				  row, row + 1, GTK_FILL, 0, 0, 0);
		break;
	}
	default:
		g_assert(0);
		return item;
	}

	gtk_table_set_row_spacings (GTK_TABLE (table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (table), 10);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);

	gtk_widget_show_all (item->vbox);

	return item;
}

static void
addressbook_source_dialog_set_source (AddressbookSourceDialog *dialog, AddressbookSource *source)
{
	AddressbookSourcePageItem *source_page;

	e_utf8_gtk_entry_set_text (GTK_ENTRY (dialog->name), source->name);
	e_utf8_gtk_entry_set_text (GTK_ENTRY (dialog->description), source->description);

	/* choose the correct server page */
	source_page = g_list_nth_data (dialog->source_pages, source->type);
	source_type_menuitem_activate (source_page->item, source_page);
	gtk_option_menu_set_history (GTK_OPTION_MENU(dialog->source_option), source->type);

	if (source->type == ADDRESSBOOK_SOURCE_LDAP) {
		LDAPAuthPageItem *auth_page;

		e_utf8_gtk_entry_set_text (GTK_ENTRY (source_page->host), source->ldap.host);
		e_utf8_gtk_entry_set_text (GTK_ENTRY (source_page->port), source->ldap.port);
		e_utf8_gtk_entry_set_text (GTK_ENTRY (source_page->rootdn), source->ldap.rootdn);

		gtk_option_menu_set_history (GTK_OPTION_MENU(source_page->scope_optionmenu), source->ldap.scope);

		auth_page = g_list_nth_data (source_page->auths, source->ldap.auth);
		ldap_auth_type_menuitem_activate (auth_page->item, auth_page);
		gtk_option_menu_set_history (GTK_OPTION_MENU(source_page->auth_optionmenu), auth_page->auth_type);

		if (auth_page->auth_type == ADDRESSBOOK_LDAP_AUTH_SIMPLE) {
			e_utf8_gtk_entry_set_text (GTK_ENTRY (auth_page->binddn), source->ldap.binddn);
		}
	}
	else {
		e_utf8_gtk_entry_set_text (GTK_ENTRY (source_page->path), source->file.path);
	}
}

static AddressbookSource *
addressbook_source_dialog_get_source (AddressbookSourceDialog *dialog)
{
	AddressbookSource *source = g_new0 (AddressbookSource, 1);
	AddressbookSourcePageItem *source_page;

	source_page = dialog->source_page;

	source->name = e_utf8_gtk_entry_get_text (GTK_ENTRY (dialog->name));
	source->description = e_utf8_gtk_entry_get_text (GTK_ENTRY (dialog->description));
	source->type = source_page->source_type;

	if (source->type == ADDRESSBOOK_SOURCE_FILE) {
		source->file.path = e_utf8_gtk_entry_get_text (GTK_ENTRY (source_page->path));
	}
	else {
		LDAPAuthPageItem *auth_page;

		source->ldap.host   = e_utf8_gtk_entry_get_text (GTK_ENTRY (source_page->host));
		source->ldap.port   = e_utf8_gtk_entry_get_text (GTK_ENTRY (source_page->port));
		source->ldap.rootdn = e_utf8_gtk_entry_get_text (GTK_ENTRY (source_page->rootdn));

		auth_page = source_page->auth;

		source->ldap.auth = auth_page->auth_type;
		if (source->ldap.auth == ADDRESSBOOK_LDAP_AUTH_SIMPLE) {
			source->ldap.binddn = e_utf8_gtk_entry_get_text (GTK_ENTRY (auth_page->binddn));
			source->ldap.remember_passwd = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (auth_page->remember_passwd));
		}

		ldap_auth_type_menuitem_activate (auth_page->item, auth_page);
		gtk_option_menu_set_history (GTK_OPTION_MENU(source_page->auth_optionmenu), auth_page->auth_type);
	}

	addressbook_storage_init_source_uri (source);

	return source;
}

static void
addressbook_source_dialog_ok_clicked (GtkWidget *widget, AddressbookSourceDialog *dialog)
{
	dialog->source = addressbook_source_dialog_get_source (dialog);
}

static void
addressbook_source_dialog_destroy (AddressbookSourceDialog *dialog)
{
	GList *s;

	for (s = dialog->source_pages; s; s = s->next) {
		AddressbookSourcePageItem *source_item = s->data;

		g_list_foreach (source_item->auths, (GFunc)g_free, NULL);
		g_list_free (source_item->auths);
		g_free (source_item);
	}

	g_list_free (dialog->source_pages);

	if (dialog->source)
		addressbook_source_free (dialog->source);
	g_free (dialog);
}

static AddressbookSourceDialog*
addressbook_source_dialog (AddressbookSource *source, GtkWidget *parent)
{
	GtkWidget *table;
	AddressbookSourceDialog *dialog = g_new0 (AddressbookSourceDialog, 1);
	GtkWidget *vbox, *dialog_vbox;
#ifdef INCLUDE_FILE_SOURCE
	GtkWidget *menu;
	AddressbookSourcePageItem *first_item = NULL;
#endif
	GtkWidget *area;
	int i;
	int row = 0;

	if (source)
		dialog->dialog = gnome_dialog_new (_("Edit Addressbook"), NULL);
	else
		dialog->dialog = gnome_dialog_new (_("Add Addressbook"), NULL);

	gtk_window_set_modal (GTK_WINDOW (dialog->dialog), TRUE);
	gtk_window_set_policy (GTK_WINDOW (dialog->dialog), 
			       FALSE, TRUE, FALSE);
	gtk_window_set_default_size (GTK_WINDOW (dialog->dialog), 300, 350);
	gnome_dialog_set_parent (GNOME_DIALOG (dialog->dialog),
				 GTK_WINDOW (parent));

	dialog->vbox = gtk_vbox_new (FALSE, 5);
	dialog_vbox = GNOME_DIALOG (dialog->dialog)->vbox;

	vbox = gtk_vbox_new (FALSE, 0);

	dialog->html = html_new (FALSE);
	put_html (GTK_HTML (dialog->html),
		  _("Select the kind of addressbook you have, and enter "
		    "the relevant information about it."));

	table = gtk_table_new (2, 2, FALSE);

	dialog->name = table_add_elem (dialog, table, row++,
				       _("Name:"),
				       _("FIXME Name help text here"));
	dialog->description = table_add_elem (dialog, table, row++,
					      _("Description:"),
					      _("FIXME Description help text here"));

	gtk_table_set_row_spacings (GTK_TABLE (table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (table), 10);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);

	dialog->notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (dialog->notebook), FALSE);

#ifdef INCLUDE_FILE_SOURCE
	dialog->source_option = gtk_option_menu_new ();
	menu = gtk_menu_new ();
#endif

	for (i =
#ifndef INCLUDE_FILE_SOURCE
		     ADDRESSBOOK_SOURCE_LDAP;
#else
		     ADDRESSBOOK_SOURCE_FILE;
#endif
	     i < ADDRESSBOOK_SOURCE_LAST;
	     i ++) {
		AddressbookSourcePageItem *item;

		item = addressbook_source_item_new (dialog, i);

		dialog->source_pages = g_list_append (dialog->source_pages, item);

#ifdef INCLUDE_FILE_SOURCE
		item->item = gtk_menu_item_new_with_label (addressbook_config_source_label (i));
		
		if (!first_item)
			first_item = item;
#endif

		gtk_notebook_append_page (GTK_NOTEBOOK (dialog->notebook),
					  item->vbox, NULL);

		gtk_signal_connect (GTK_OBJECT (item->item), "activate",
				    GTK_SIGNAL_FUNC (source_type_menuitem_activate),
				    item);

#ifdef INCLUDE_FILE_SOURCE
		gtk_menu_append (GTK_MENU (menu), item->item);
#endif
		gtk_widget_show (item->item);
	}

#ifdef INCLUDE_FILE_SOURCE
	gtk_option_menu_set_menu (GTK_OPTION_MENU (dialog->source_option), menu);
	source_type_menuitem_activate (first_item->item, first_item);
	gtk_option_menu_set_history (GTK_OPTION_MENU(dialog->source_option), 0);
#endif

	gtk_box_pack_start (GTK_BOX (vbox), dialog->html->parent, 
			    FALSE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), table,
			    FALSE, FALSE, 0);

#ifdef INCLUDE_FILE_SOURCE
	gtk_box_pack_start (GTK_BOX (vbox), dialog->source_option,
			    FALSE, FALSE, 0);
#endif

	gtk_box_pack_start (GTK_BOX (dialog->vbox), vbox, FALSE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (dialog->vbox), dialog->notebook, 
			    TRUE, TRUE, 0);

	/* hook our ui into the gnome-dialog */
	gtk_box_pack_start (GTK_BOX (dialog_vbox), dialog->vbox, TRUE, TRUE, 0);

	gtk_widget_show_all (dialog->vbox);

	/* Buttons */
	area = GNOME_DIALOG (dialog->dialog)->action_area;
	gtk_widget_show (area);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (area), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing (GTK_BUTTON_BOX (area), 8);

	gnome_dialog_append_button (GNOME_DIALOG (dialog->dialog), 
				    GNOME_STOCK_BUTTON_OK);
	gnome_dialog_append_button (GNOME_DIALOG (dialog->dialog), 
				    GNOME_STOCK_BUTTON_CANCEL);	

	gnome_dialog_set_default (GNOME_DIALOG (dialog->dialog), 0);

	/* fill in source info if there is some */
	if (source)
		addressbook_source_dialog_set_source (dialog, source);

	gnome_dialog_set_sensitive (GNOME_DIALOG (dialog->dialog), 0, FALSE);
	
	gnome_dialog_button_connect( GNOME_DIALOG (dialog->dialog), 0,
				     GTK_SIGNAL_FUNC (addressbook_source_dialog_ok_clicked),
				     dialog);

	return dialog;
}

static AddressbookSourceDialog *
addressbook_config_source (AddressbookSource *source, GtkWidget *parent)
{
	AddressbookSourceDialog* dialog = addressbook_source_dialog (source, parent);

	dialog->id = gnome_dialog_run_and_close (GNOME_DIALOG (dialog->dialog));

	return dialog;
}



typedef struct {
	GladeXML *gui;
	GNOME_Evolution_Shell shell;
	GtkWidget *dialog;
	GtkWidget *clistSources;
	GtkWidget *addSource;
	GtkWidget *editSource;
	GtkWidget *deleteSource;
	gint      source_row;
} AddressbookDialog;

static void
update_sensitivity (AddressbookDialog *dialog)
{
	gboolean sensitive = dialog->source_row != -1;

	gtk_widget_set_sensitive (dialog->editSource, sensitive);
	gtk_widget_set_sensitive (dialog->deleteSource, sensitive);
}

static void
add_source_clicked (GtkWidget *widget, AddressbookDialog *dialog)
{
	AddressbookSourceDialog *sdialog;

	sdialog = addressbook_config_source (NULL, dialog->dialog);
	if (sdialog->id == 0) {
		/* Ok was clicked */
		AddressbookSource *source = addressbook_source_copy(sdialog->source);
		gint row;
		gchar *text[2];

		text[0] = source->name;
		text[1] = source->uri;

		row = e_utf8_gtk_clist_append (GTK_CLIST(dialog->clistSources), text);
		gtk_clist_set_row_data_full (GTK_CLIST(dialog->clistSources), row, source, (GtkDestroyNotify) addressbook_source_free);
		gnome_property_box_changed (GNOME_PROPERTY_BOX (dialog->dialog));
		update_sensitivity (dialog);
	}

	addressbook_source_dialog_destroy (sdialog);
}

static void
edit_source_clicked (GtkWidget *widget, AddressbookDialog *dialog)
{
	AddressbookSource *source;
	AddressbookSourceDialog *sdialog;

	source = gtk_clist_get_row_data (GTK_CLIST (dialog->clistSources), dialog->source_row);

	sdialog = addressbook_config_source (source, dialog->dialog);
	if (sdialog->id == 0) {
		/* Ok was clicked */
		source = addressbook_source_copy(sdialog->source);

		e_utf8_gtk_clist_set_text (GTK_CLIST (dialog->clistSources), dialog->source_row, 0, source->name);
		e_utf8_gtk_clist_set_text (GTK_CLIST (dialog->clistSources), dialog->source_row, 1, source->uri);
		gtk_clist_set_row_data (GTK_CLIST (dialog->clistSources), dialog->source_row, source);
		gnome_property_box_changed (GNOME_PROPERTY_BOX (dialog->dialog));
		update_sensitivity (dialog);
	}

	addressbook_source_dialog_destroy (sdialog);
}

static void
delete_source_clicked (GtkWidget *widget, AddressbookDialog *dialog)
{
	gtk_clist_remove (GTK_CLIST (dialog->clistSources), dialog->source_row);
	dialog->source_row = -1;
	gnome_property_box_changed (GNOME_PROPERTY_BOX(dialog->dialog));
	update_sensitivity (dialog);
}

static void
sources_select_row (GtkWidget *widget, gint row, gint column,
		    GdkEventButton *event, AddressbookDialog *dialog)
{
	dialog->source_row = row;

	update_sensitivity (dialog);
}

static void
addressbook_dialog_apply (GnomePropertyBox *property_box, gint page_num, AddressbookDialog *dialog)
{
	int i;

	if (page_num != -1)
		return;

	addressbook_storage_clear_sources();

	for (i = 0; i < GTK_CLIST(dialog->clistSources)->rows; i ++) {
		AddressbookSource *source = (AddressbookSource*)gtk_clist_get_row_data (GTK_CLIST (dialog->clistSources), i);
		addressbook_storage_add_source (addressbook_source_copy (source));
	}
}

static void
addressbook_dialog_close (GnomePropertyBox *property_box, AddressbookDialog *dialog)
{
	gtk_object_unref (GTK_OBJECT (dialog->gui));
	g_free (dialog);
}

void
addressbook_config (GNOME_Evolution_Shell shell)
{
	AddressbookDialog *dialog;
	GladeXML *gui;
	GtkWidget *clist;
	GList *l;

	dialog = g_new0 (AddressbookDialog, 1);

	dialog->source_row = -1;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/addressbook-config.glade", NULL);
	dialog->gui = gui;
	dialog->shell = shell;

	dialog->dialog = glade_xml_get_widget (gui, "dialog");

	clist = glade_xml_get_widget (gui, "clistSources");
	dialog->clistSources = clist;
	
	gtk_clist_column_titles_passive (GTK_CLIST (clist));
	gtk_clist_set_column_width (GTK_CLIST (clist), 0, 80);

	dialog->addSource = glade_xml_get_widget (gui, "addSource");
	gtk_signal_connect (GTK_OBJECT(dialog->addSource), "clicked",
			    GTK_SIGNAL_FUNC (add_source_clicked),
			    dialog);

	dialog->editSource = glade_xml_get_widget (gui, "editSource");
	gtk_signal_connect (GTK_OBJECT(dialog->editSource), "clicked",
			    GTK_SIGNAL_FUNC (edit_source_clicked),
			    dialog);

	dialog->deleteSource = glade_xml_get_widget (gui, "deleteSource");
	gtk_signal_connect (GTK_OBJECT(dialog->deleteSource), "clicked",
			    GTK_SIGNAL_FUNC (delete_source_clicked),
			    dialog);

	update_sensitivity (dialog);

	l = addressbook_storage_get_sources ();
	for (; l != NULL; l = l->next) {
		AddressbookSource *source;
		gint row;
		gchar *text[2];

		source = addressbook_source_copy ((AddressbookSource*)l->data);

		text[0] = source->name;
		text[1] = source->uri;

		row = e_utf8_gtk_clist_append (GTK_CLIST(clist), text);
		gtk_clist_set_row_data_full (GTK_CLIST(clist), row, source, (GtkDestroyNotify) addressbook_source_free);
	}

	gtk_signal_connect (GTK_OBJECT (clist), "select_row",
			    GTK_SIGNAL_FUNC (sources_select_row),
			    dialog);

	gtk_signal_connect (GTK_OBJECT (dialog->dialog), "apply",
			    addressbook_dialog_apply, dialog);

	gtk_signal_connect (GTK_OBJECT (dialog->dialog), "destroy",
			    addressbook_dialog_close, dialog);

	gtk_window_set_default_size (GTK_WINDOW (dialog->dialog), 300, 350);

	gtk_widget_show (dialog->dialog);
}
