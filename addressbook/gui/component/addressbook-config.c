/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>

#include <gnome.h>
#include <gtkhtml/gtkhtml.h>
#include <gal/widgets/e-unicode.h>
#include "e-util/e-html-utils.h"

typedef enum {
	ADDRESSBOOK_SOURCE_FILE,
#if HAVE_LDAP
	ADDRESSBOOK_SOURCE_LDAP,
#endif
	ADDRESSBOOK_SOURCE_LAST
} AddressbookSourceType;

#if HAVE_LDAP
typedef enum {
	LDAP_AUTH_NONE,
	LDAP_AUTH_SIMPLE,
#if LDAP_SASL
	LDAP_AUTH_SASL,
#endif
	LDAP_AUTH_LAST
} LDAPAuthType;
#endif

typedef struct _AddressbookSourceDialog AddressbookSourceDialog;
typedef struct _AddressbookSourcePageItem  AddressbookSourcePageItem;
typedef struct _LDAPAuthPageItem LDAPAuthPageItem;

struct _AddressbookSourceDialog {
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *name;
	GtkWidget *description;
	GtkWidget *source_option;
	GtkWidget *notebook;

	AddressbookSourcePageItem *source;
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
	GtkWidget *auth_optionmenu;
	GtkWidget *auth_notebook;

	LDAPAuthPageItem *auth;
};

#if HAVE_LDAP
struct _LDAPAuthPageItem {
	gint pnum;

	LDAPAuthType auth_type;

	AddressbookSourceDialog *dialog;
	AddressbookSourcePageItem *page;

	GtkWidget *item;
	GtkWidget *vbox;

	/* simple (password) auth */
	GtkWidget *binddn;
	GtkWidget *remember_passwd;
};
#endif

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

static const char *
addressbook_config_source_label (AddressbookSourceType type)
{
	switch (type) {
#if HAVE_LDAP
	case ADDRESSBOOK_SOURCE_LDAP:
		return _("LDAP Server");
#endif
	case ADDRESSBOOK_SOURCE_FILE:
		return _("File");
	default:
		g_assert(0);
		return _("Unknown addressbook type");
	}
}

#if HAVE_LDAP
static const char *
addressbook_config_auth_label (AddressbookSourceType type)
{
	switch (type) {
	case LDAP_AUTH_NONE:
		return _("None (anonymous mode)");
	case LDAP_AUTH_SIMPLE:
		return _("Password");
#if LDAP_SASL
	case LDAP_AUTH_SASL:
		return _("SASL");
#endif
	default:
		g_assert(0);
		return _("Unknown auth type");
	}
}
#endif

static void
addressbook_source_edit_changed (GtkWidget *item, AddressbookSourceDialog *dialog)
{
	char *data;
	gboolean complete = TRUE;
	AddressbookSourcePageItem *source = dialog->source;

	if (source == NULL)
		complete = FALSE;

	if (complete) {
		data = e_utf8_gtk_editable_get_chars (GTK_EDITABLE (dialog->name), 0, -1);
		if (!data || !*data)
			complete = FALSE;
		g_free (data);
	}

	if (complete) {
		if (source->source_type == ADDRESSBOOK_SOURCE_FILE) {
			if (complete) {
				data = e_utf8_gtk_editable_get_chars (GTK_EDITABLE (source->path), 0, -1);
				if (!data || !*data)
					complete = FALSE;
				g_free (data);
			}
		}
#if HAVE_LDAP
		else {
			if (complete) {
				data = e_utf8_gtk_editable_get_chars (GTK_EDITABLE (source->host), 0, -1);
				if (!data || !*data)
					complete = FALSE;
				g_free (data);
			}

			if (complete) {
				data = e_utf8_gtk_editable_get_chars (GTK_EDITABLE (source->port), 0, -1);
				if (!data || !*data)
					complete = FALSE;
				/* XXX more validation on port here */
				g_free (data);
			}

			if (complete) {
				LDAPAuthPageItem *auth_page = source->auth;

				if (auth_page->auth_type == LDAP_AUTH_SIMPLE) {
					data = e_utf8_gtk_editable_get_chars (GTK_EDITABLE (auth_page->binddn), 0, -1);
					if (!data || !*data)
						complete = FALSE;
					g_free (data);
				}
#ifdef LDAP_SASL
				else if (auth_page->auth_type == LDAP_AUTH_SASL) {
				}
#endif
				data = e_utf8_gtk_editable_get_chars (GTK_EDITABLE (source->port), 0, -1);
				if (!data || !*data)
					complete = FALSE;
				/* XXX more validation on port here */
				g_free (data);
			}
		}
#endif
	}

	gnome_dialog_set_sensitive (GNOME_DIALOG (dialog->dialog), 0, complete);
}

static void
source_type_menuitem_activate (GtkWidget *item, gpointer data)
{
	AddressbookSourcePageItem *sitem = data;

	gtk_notebook_set_page (GTK_NOTEBOOK(sitem->dialog->notebook), sitem->pnum);
	sitem->dialog->source = sitem;

	addressbook_source_edit_changed (item, sitem->dialog);	
}

static GtkWidget *
table_add_elem (AddressbookSourceDialog *dialog, GtkWidget *table, 
		int row, const char *label_text)
{
	GtkWidget *label, *entry;

	label = gtk_label_new (label_text);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 
			  row, row + 1, GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

	entry = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), entry, 1, 3, row, row + 1,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);

	gtk_signal_connect (GTK_OBJECT (entry), "changed",
			    GTK_SIGNAL_FUNC (addressbook_source_edit_changed), dialog);

	return entry;
}

#if HAVE_LDAP

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
				LDAPAuthType type)
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
	case LDAP_AUTH_NONE:
		break;
	case LDAP_AUTH_SIMPLE:
		table = gtk_table_new (2, 2, FALSE);
		item->binddn = table_add_elem (dialog, table, row++, _("Bind DN:"));

		item->remember_passwd = gtk_check_button_new_with_label (_("Remember this password"));

		gtk_table_attach (GTK_TABLE (table), item->remember_passwd, 1, 2,
				  row, row + 1, GTK_FILL, 0, 0, 0);

		gtk_box_pack_start (GTK_BOX (item->vbox), table,
				    TRUE, TRUE, 0);
		break;
#if LDAP_SASL
	case LDAP_AUTH_SASL:
		break;
#endif
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
#endif

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
#if HAVE_LDAP
	case ADDRESSBOOK_SOURCE_LDAP: {
		GtkWidget *label;
		GtkWidget *menu;
		int i;
		LDAPAuthPageItem *first_item = NULL;
		int position;

		table = gtk_table_new (5, 2, FALSE);

		item->host = table_add_elem (dialog, table, row++, _("Host:"));
		item->port = table_add_elem (dialog, table, row++, _("Port:"));
		gtk_editable_insert_text (GTK_EDITABLE (item->port), "389", 3, &position);

		item->rootdn = table_add_elem (dialog, table, row++, _("Root DN:"));

		gtk_box_pack_start (GTK_BOX (item->vbox), table,
				    TRUE, FALSE, 0);

		item->auth_optionmenu = gtk_option_menu_new ();
		menu = gtk_menu_new ();

		item->auth_notebook = gtk_notebook_new();
		gtk_notebook_set_show_tabs (GTK_NOTEBOOK (item->auth_notebook), FALSE);

		for (i = 0; i < LDAP_AUTH_LAST; i++) {
			LDAPAuthPageItem *auth_item;

			auth_item = addressbook_ldap_auth_item_new (dialog, item, i);

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
#endif
	case ADDRESSBOOK_SOURCE_FILE: {
		table = gtk_table_new (2, 2, FALSE);
		item->path = table_add_elem (dialog, table, row++, _("Path:"));

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
addressbook_source_ok_clicked (GtkWidget *widget, AddressbookSourceDialog *sdialog) 
{
}

static AddressbookSourceDialog*
addressbook_source_dialog (GtkWidget *parent)
{
	GtkWidget *html;
	GtkWidget *table;
	AddressbookSourceDialog *dialog = g_new0 (AddressbookSourceDialog, 1);
	GtkWidget *vbox, *dialog_vbox;
	GtkWidget *menu;
	GtkWidget *area;
	AddressbookSourcePageItem *first_item = NULL;
	int i;
	int row = 0;

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

	html = html_new (FALSE);
	put_html (GTK_HTML (html),
		  _("Select the kind of addressbook you have, and enter "
		    "the relevant information about it."));

	table = gtk_table_new (2, 2, FALSE);

	dialog->name = table_add_elem (dialog, table, row++, _("Name:"));
	dialog->description = table_add_elem (dialog, table, row++, _("Description:"));

	gtk_table_set_row_spacings (GTK_TABLE (table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (table), 10);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);

	dialog->notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (dialog->notebook), FALSE);

	dialog->source_option = gtk_option_menu_new ();
	menu = gtk_menu_new ();

	for (i = 0; i < ADDRESSBOOK_SOURCE_LAST; i ++) {
		AddressbookSourcePageItem *item;

		item = addressbook_source_item_new (dialog, i);

		item->item = gtk_menu_item_new_with_label (addressbook_config_source_label (i));
		
		if (!first_item)
			first_item = item;

		gtk_notebook_append_page (GTK_NOTEBOOK (dialog->notebook),
					  item->vbox, NULL);

		gtk_signal_connect (GTK_OBJECT (item->item), "activate",
				    GTK_SIGNAL_FUNC (source_type_menuitem_activate),
				    item);

		gtk_menu_append (GTK_MENU (menu), item->item);
		gtk_widget_show (item->item);
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (dialog->source_option), menu);
	source_type_menuitem_activate (first_item->item, first_item);
	gtk_option_menu_set_history (GTK_OPTION_MENU(dialog->source_option), 0);


	gtk_box_pack_start (GTK_BOX (vbox), html->parent, 
			    FALSE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), table,
			    FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), dialog->source_option,
			    FALSE, FALSE, 0);

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

	gnome_dialog_set_sensitive (GNOME_DIALOG (dialog->dialog), 0, FALSE);
	
	gnome_dialog_button_connect( GNOME_DIALOG (dialog->dialog), 0,
				     GTK_SIGNAL_FUNC (addressbook_source_ok_clicked),
				     dialog);

	return dialog;
}

void
addressbook_config_source ()
{
	AddressbookSourceDialog* dialog = addressbook_source_dialog (NULL);

	gnome_dialog_run_and_close (GNOME_DIALOG (dialog->dialog));
}
