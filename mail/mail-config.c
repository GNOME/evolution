/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-config.c: Mail configuration dialogs/wizard. */

/* 
 * Author: 
 *  Dan Winship <danw@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include <pwd.h>

#include <gnome.h>
#include <gtkhtml/gtkhtml.h>

#include "mail.h"
#include "e-util/e-html-utils.h"
#include "e-util/e-setup.h"

struct service_type {
	CamelProvider *provider;
	CamelService *service;
	GList *authtypes;
};

struct identity_record {
	char *name, *address, *organization, *sigfile;
};

static char *username = NULL;


/* HTML Helpers */

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
	gtk_html_set_default_background_color (GTK_HTML (html),
					       white ? &style->white :
					       &style->bg[0]);
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
	GtkHTMLStreamHandle *handle;

	text = e_text_to_html (text, E_TEXT_TO_HTML_CONVERT_NL);
	handle = gtk_html_begin (html, "");
	gtk_html_write (html, handle, "<HTML><BODY>", 12);
	gtk_html_write (html, handle, text, strlen (text));
	gtk_html_write (html, handle, "</BODY></HTML>", 14);
	g_free (text);
	gtk_html_end (html, handle, GTK_HTML_STREAM_OK);
}


/* Identity page */

static void
identity_note_doneness (GtkObject *page, gpointer user_data)
{
	GtkWidget *exit_button;
	GtkEntry *entry;
	char *data;

	exit_button = gtk_object_get_data (page, "exit_button");

	entry = gtk_object_get_data (page, "name");
	data = gtk_entry_get_text (entry);
	if (data && *data) {
		entry = gtk_object_get_data (page, "addr");
		data = gtk_entry_get_text (entry);
	}

	gtk_widget_set_sensitive (exit_button, data && *data);
}

static void
prepare_identity (GnomeDruidPage *page, gpointer arg1, gpointer user_data)
{
	identity_note_doneness (user_data, NULL);
}

static gboolean
identity_next (GnomeDruidPage *page, gpointer arg1, gpointer user_data)
{
	GtkObject *box = user_data;
	GtkEntry *addr = gtk_object_get_data (box, "addr");
	char *data, *at;

	/* FIXME: we need more sanity checking than this. */

	data = gtk_entry_get_text (addr);
	at = strchr (data, '@');
	if (!at || !strchr (at + 1, '.')) {
		GtkWidget *parent =
			gtk_widget_get_ancestor (GTK_WIDGET (page),
						 GTK_TYPE_WINDOW);
		gnome_error_dialog_parented ("Email address must be of the "
					     "form \"user@domain\".",
					     GTK_WINDOW (parent));
		return TRUE;
	}

	g_free (username);
	username = g_strndup (data, at - data);

	return FALSE;
}

static void
destroy_identity (GtkObject *table, gpointer idrecp)
{
	struct identity_record *idrec = idrecp;
	GtkEditable *editable;

	editable = gtk_object_get_data (table, "name");
	idrec->name = gtk_editable_get_chars (editable, 0, -1);
	editable = gtk_object_get_data (table, "addr");
	idrec->address = gtk_editable_get_chars (editable, 0, -1);
	editable = gtk_object_get_data (table, "org");
	idrec->organization = gtk_editable_get_chars (editable, 0, -1);
	editable = gtk_object_get_data (table, "sig");
	idrec->sigfile = gtk_editable_get_chars (editable, 0, -1);
}

static void
create_identity_page (GtkWidget *vbox, struct identity_record *idrec)
{
	GtkWidget *html, *table;
	GtkWidget *name, *addr, *org, *sig;
	GtkWidget *name_entry, *addr_entry, *org_entry, *sig_entry;
	GtkWidget *hsep;
	char *user;
	struct passwd *pw;

	html = html_new (FALSE);
	put_html (GTK_HTML (html),
		  _("Enter your name and email address to be used in "
		    "outgoing mail. You may also, optionally, enter the "
		    "name of your organization, and the name of a file "
		    "to read your signature from."));
	gtk_box_pack_start (GTK_BOX (vbox), html->parent, FALSE, TRUE, 0);

	table = gtk_table_new (5, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 10);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);
	gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (vbox), "destroy",
			    GTK_SIGNAL_FUNC (destroy_identity), idrec);

	name = gtk_label_new (_("Full name:"));
	gtk_table_attach (GTK_TABLE (table), name, 0, 1, 0, 1,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (name), 1, 0.5);

	name_entry = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), name_entry, 1, 2, 0, 1,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_object_set_data (GTK_OBJECT (vbox), "name", name_entry);

	user = getenv ("USER");
	if (user)
		pw = getpwnam (user);
	else
		pw = getpwuid (getuid ());
	if (pw && pw->pw_gecos && *pw->pw_gecos) {
		char *name;
		int pos = 0;

		name = g_strndup (pw->pw_gecos, strcspn (pw->pw_gecos, ","));
		gtk_editable_insert_text (GTK_EDITABLE (name_entry),
					  name, strlen (name), &pos);
	}

	addr = gtk_label_new (_("Email address:"));
	gtk_table_attach (GTK_TABLE (table), addr, 0, 1, 1, 2,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (addr), 1, 0.5);

	addr_entry = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), addr_entry, 1, 2, 1, 2,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_object_set_data (GTK_OBJECT (vbox), "addr", addr_entry);

	gtk_signal_connect_object (GTK_OBJECT (name_entry), "changed",
				   GTK_SIGNAL_FUNC (identity_note_doneness),
				   GTK_OBJECT (vbox));
	gtk_signal_connect_object (GTK_OBJECT (addr_entry), "changed",
				   GTK_SIGNAL_FUNC (identity_note_doneness),
				   GTK_OBJECT (vbox));

	hsep = gtk_hseparator_new ();
	gtk_table_attach (GTK_TABLE (table), hsep, 0, 2, 2, 3,
			  GTK_FILL, 0, 0, 8);

	org = gtk_label_new (_("Organization:"));
	gtk_table_attach (GTK_TABLE (table), org, 0, 1, 3, 4,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (org), 1, 0.5);

	org_entry = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), org_entry, 1, 2, 3, 4,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_object_set_data (GTK_OBJECT (vbox), "org", org_entry);

	sig = gtk_label_new (_("Signature file:"));
	gtk_table_attach (GTK_TABLE (table), sig, 0, 1, 4, 5,
			  GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (sig), 1, 0);

	sig_entry = gnome_file_entry_new (NULL, _("Signature File"));
	gtk_table_attach (GTK_TABLE (table), sig_entry, 1, 2, 4, 5,
			  GTK_FILL, 0, 0, 0);
	gnome_file_entry_set_default_path (GNOME_FILE_ENTRY (sig_entry),
					   g_get_home_dir ());
	gtk_object_set_data (GTK_OBJECT (vbox), "sig",
			     gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (sig_entry)));

	gtk_widget_show_all (table);
}


/* Source/Transport pages */

static void
service_note_doneness (GtkObject *page, gpointer user_data)
{
	GtkObject *box;
	GtkWidget *button;
	GtkEntry *entry;
	char *data;
	gboolean sensitive = TRUE;

	entry = gtk_object_get_data (page, "server_entry");
	if (entry) {
		data = gtk_entry_get_text (entry);
		if (!data || !*data)
			sensitive = FALSE;
	}

	if (sensitive) {
		entry = gtk_object_get_data (page, "user_entry");
		if (entry) {
			data = gtk_entry_get_text (entry);
			if (!data || !*data)
				sensitive = FALSE;
		}
	}

	if (sensitive) {
		entry = gtk_object_get_data (page, "path_entry");
		if (entry) {
			data = gtk_entry_get_text (entry);
			if (!data || !*data)
				sensitive = FALSE;
		}
	}

	button = gtk_object_get_data (page, "autodetect");
	if (button)
		gtk_widget_set_sensitive (button, sensitive);

	box = gtk_object_get_data (page, "box");
	button = gtk_object_get_data (box, "exit_button");
	if (button)
		gtk_widget_set_sensitive (button, sensitive);
}

static void
prepare_service (GnomeDruidPage *page, gpointer arg1, gpointer user_data)
{
	GtkObject *box = user_data;
	GtkNotebook *notebook = gtk_object_get_data (box, "notebook");
	GtkWidget *table;
	GtkEntry *entry;

	table = gtk_notebook_get_nth_page (
		notebook, gtk_notebook_get_current_page (notebook));

	if (username) {
		char *data = NULL;

		entry = gtk_object_get_data (GTK_OBJECT (table), "user_entry");
		if (entry) {
			data = gtk_entry_get_text (entry);
			if (!data || !*data)
				gtk_entry_set_text (entry, username);
		}
	}

	service_note_doneness (GTK_OBJECT (table), NULL);
}

static void
auth_menuitem_activate (GtkObject *menuitem, GtkHTML *html)
{
	CamelServiceAuthType *authtype;

	authtype = gtk_object_get_data (menuitem, "authtype");
	put_html (html, authtype->description);
}

static void
fill_auth_menu (GtkOptionMenu *optionmenu, GtkHTML *html, GList *authtypes)
{
	CamelServiceAuthType *authtype;
	GtkWidget *menu, *item, *firstitem = NULL;

	menu = gtk_menu_new ();
	gtk_option_menu_set_menu (optionmenu, menu);
	for (; authtypes; authtypes = authtypes->next) {
		authtype = authtypes->data;
		item = gtk_menu_item_new_with_label (_(authtype->name));
		if (!firstitem)
			firstitem = item;
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_object_set_data (GTK_OBJECT (item), "authtype", authtype);
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    GTK_SIGNAL_FUNC (auth_menuitem_activate),
				    html);
	}
	gtk_widget_show_all (menu);
	gtk_option_menu_set_history (optionmenu, 0);
	if (firstitem)
		auth_menuitem_activate (GTK_OBJECT (firstitem), html);
}

static char *
get_service_url (GtkObject *table)
{
	CamelURL *url;
	GtkEditable *editable;
	GtkOptionMenu *auth_optionmenu;
	char *url_str;

	url = g_new0 (CamelURL, 1);
	url->protocol = g_strdup (gtk_object_get_data (table, "protocol"));
	editable = gtk_object_get_data (table, "user_entry");
	if (editable)
		url->user = gtk_editable_get_chars (editable, 0, -1);
	editable = gtk_object_get_data (table, "server_entry");
	if (editable)
		url->host = gtk_editable_get_chars (editable, 0, -1);
	editable = gtk_object_get_data (table, "path_entry");
	if (editable)
		url->path = gtk_editable_get_chars (editable, 0, -1);

	auth_optionmenu = gtk_object_get_data (table, "auth_optionmenu");
	if (auth_optionmenu) {
		GtkWidget *menu, *item;
		CamelServiceAuthType *authtype;

		menu = gtk_option_menu_get_menu (auth_optionmenu);
		if (menu) {
			item = gtk_menu_get_active (GTK_MENU (menu));
			authtype = gtk_object_get_data (GTK_OBJECT (item),
							"authtype");
			if (*authtype->authproto)
				url->authmech = g_strdup (authtype->authproto);
		}
	}

	url_str = camel_url_to_string (url, FALSE);
	camel_url_free (url);
	return url_str;
}

static void
autodetect_cb (GtkWidget *button, GtkObject *table)
{
	char *url, *err;
	CamelException *ex;
	CamelService *service;
	GList *authtypes;
	GtkHTML *html;
	GtkOptionMenu *optionmenu;
	GtkWidget *parent;
	int type;

	type = GPOINTER_TO_UINT (gtk_object_get_data (table, "service_type"));
	url = get_service_url (table);

	ex = camel_exception_new ();
	service = camel_session_get_service (session, url, type, ex);
	g_free (url);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE)
		goto error;

	authtypes = camel_service_query_auth_types (service, ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE)
		goto error;

	html = gtk_object_get_data (table, "auth_html");
	optionmenu = gtk_object_get_data (table, "auth_optionmenu");
	fill_auth_menu (optionmenu, html, authtypes);
	return;

 error:
	err = g_strdup_printf ("Could not detect supported "
			       "authentication types:\n%s",
			       camel_exception_get_description (ex));
	camel_exception_free (ex);
	parent = gtk_widget_get_ancestor (button, GTK_TYPE_WINDOW);
	gnome_error_dialog_parented (err, GTK_WINDOW (parent));
	g_free (err);
}

static void
destroy_service (GtkObject *notebook, gpointer urlp)
{
	char **url = urlp;
	GtkWidget *table;
	int page;

	page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	table = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), page);
	*url = get_service_url (GTK_OBJECT (table));
}

static void
add_row (GtkWidget *table, int row, const char *label_text,
	 const char *tag, int flag)
{
	GtkWidget *label, *entry;

	label = gtk_label_new (label_text);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

	entry = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), entry, 1, 3, row, row + 1,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_signal_connect_object (GTK_OBJECT (entry), "changed",
				   GTK_SIGNAL_FUNC (service_note_doneness),
				   GTK_OBJECT (table));
	gtk_object_set_data (GTK_OBJECT (table), tag, entry);
}

static GtkWidget *
create_source (struct service_type *st)
{
	GtkWidget *table;
	GtkWidget *auth, *auth_optionmenu, *auth_html;
	GtkWidget *autodetect;
	int row, service_flags;

	table = gtk_table_new (5, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (table), 10);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);
	gtk_object_set_data (GTK_OBJECT (table), "protocol",
			     st->provider->protocol);
	gtk_object_set_data (GTK_OBJECT (table), "service_type",
			     GUINT_TO_POINTER (CAMEL_PROVIDER_STORE));

	row = 0;
	service_flags = st->service->url_flags &
		~CAMEL_SERVICE_URL_NEED_AUTH;

	if (service_flags & CAMEL_SERVICE_URL_NEED_HOST) {
		add_row (table, row, _("Server:"), "server_entry",
			 CAMEL_SERVICE_URL_NEED_HOST);
		row++;
	}

	if (service_flags & CAMEL_SERVICE_URL_NEED_USER) {
		add_row (table, row, _("Username:"), "user_entry",
			 CAMEL_SERVICE_URL_NEED_USER);
		row++;
	}

	if (service_flags & CAMEL_SERVICE_URL_NEED_PATH) {
		add_row (table, row, _("Path:"), "path_entry",
			 CAMEL_SERVICE_URL_NEED_PATH);
		row++;
	}

	if (st->authtypes) {
		auth = gtk_label_new (_("Authentication:"));
		gtk_table_attach (GTK_TABLE (table), auth, 0, 1,
				  row, row + 1, GTK_FILL, 0, 0, 0);
		gtk_misc_set_alignment (GTK_MISC (auth), 1, 0.5);

		auth_optionmenu = gtk_option_menu_new ();
		gtk_table_attach (GTK_TABLE (table), auth_optionmenu, 1, 2,
				  row, row + 1, GTK_FILL | GTK_EXPAND,
				  0, 0, 0);
		gtk_object_set_data (GTK_OBJECT (table), "auth_optionmenu",
				     auth_optionmenu);

		autodetect = gtk_button_new_with_label (_("Detect supported types..."));
		gtk_table_attach (GTK_TABLE (table), autodetect, 2, 3,
				  row, row + 1, 0, 0, 0, 0);
		gtk_widget_set_sensitive (autodetect, FALSE);
		gtk_signal_connect (GTK_OBJECT (autodetect), "clicked",
				    GTK_SIGNAL_FUNC (autodetect_cb), table);
		gtk_object_set_data (GTK_OBJECT (table), "autodetect",
				     autodetect);

		auth_html = html_new (TRUE);
		gtk_table_attach (GTK_TABLE (table), auth_html->parent->parent,
				  0, 3, row + 1, row + 2,
				  GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
		gtk_object_set_data (GTK_OBJECT (table), "auth_html",
				     auth_html);

		fill_auth_menu (GTK_OPTION_MENU (auth_optionmenu),
				GTK_HTML (auth_html), st->authtypes);

		row += 2;
	}

	if (row != 0) {
		GtkWidget *check;

		check = gtk_check_button_new_with_label (
			_("Test these values before continuing"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);
		gtk_table_attach (GTK_TABLE (table), check, 0, 3,
				  row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
		gtk_object_set_data (GTK_OBJECT (table), "check", check);
		row += 1;
	}

	gtk_table_resize (GTK_TABLE (table), row, 3);
	gtk_widget_show_all (table);

	return table;
}

static GtkWidget *
create_transport (struct service_type *st)
{
	GtkWidget *table;
	GtkWidget *auth, *auth_optionmenu, *auth_html;
	GtkWidget *autodetect;
	int row, service_flags;

	table = gtk_table_new (5, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (table), 10);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);
	gtk_object_set_data (GTK_OBJECT (table), "protocol",
			     st->provider->protocol);
	gtk_object_set_data (GTK_OBJECT (table), "service_type",
			     GUINT_TO_POINTER (CAMEL_PROVIDER_TRANSPORT));

	row = 0;
	service_flags = st->service->url_flags &
		~CAMEL_SERVICE_URL_NEED_AUTH;

	if (service_flags & CAMEL_SERVICE_URL_NEED_HOST) {
		add_row (table, row, _("Server:"), "server_entry",
			 CAMEL_SERVICE_URL_NEED_HOST);
		row++;
	}

	if (st->authtypes) {
		auth = gtk_label_new (_("Authentication:"));
		gtk_table_attach (GTK_TABLE (table), auth, 0, 1,
				  row, row + 1, GTK_FILL, 0, 0, 0);
		gtk_misc_set_alignment (GTK_MISC (auth), 1, 0.5);

		auth_optionmenu = gtk_option_menu_new ();
		gtk_table_attach (GTK_TABLE (table), auth_optionmenu, 1, 2,
				  row, row + 1, GTK_FILL | GTK_EXPAND,
				  0, 0, 0);
		gtk_object_set_data (GTK_OBJECT (table), "auth_optionmenu",
				     auth_optionmenu);

		autodetect = gtk_button_new_with_label (_("Detect supported types..."));
		gtk_table_attach (GTK_TABLE (table), autodetect, 2, 3,
				  row, row + 1, 0, 0, 0, 0);
		gtk_widget_set_sensitive (autodetect, FALSE);
		gtk_object_set_data (GTK_OBJECT (table), "autodetect",
				     autodetect);

		auth_html = html_new (TRUE);
		gtk_table_attach (GTK_TABLE (table), auth_html->parent->parent,
				  0, 3, row + 1, row + 2,
				  GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

		fill_auth_menu (GTK_OPTION_MENU (auth_optionmenu),
				GTK_HTML (auth_html), st->authtypes);

		row += 2;
	}

	if (row != 0) {
		GtkWidget *check;

		check = gtk_check_button_new_with_label (
			_("Test these values before continuing"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);
		gtk_table_attach (GTK_TABLE (table), check, 0, 3,
				  row, row + 1, GTK_FILL | GTK_EXPAND,
				  GTK_FILL, 0, 0);
		gtk_object_set_data (GTK_OBJECT (table), "check", check);
		row += 1;
	}

	gtk_table_resize (GTK_TABLE (table), row, 3);
	gtk_widget_show_all (table);

	return table;
}

static void
stype_menuitem_activate (GtkObject *menuitem, GtkObject *table)
{
	GtkHTML *html;
	char *text;
	int page;
	GtkNotebook *notebook;

	text = gtk_object_get_data (menuitem, "description");
	html = gtk_object_get_data (table, "html");
	put_html (html, text);

	page = GPOINTER_TO_UINT (gtk_object_get_data (menuitem, "page"));
	notebook = gtk_object_get_data (table, "notebook");
	gtk_notebook_set_page (notebook, page);
	service_note_doneness (GTK_OBJECT (gtk_notebook_get_nth_page (notebook,
								      page)),
			       NULL);			       
}

/* Create the mail source/transport page. */
static void
create_service_page (GtkWidget *vbox, const char *label_text, GList *services,
		     GtkWidget *(*create_service)(struct service_type *),
		     char **urlp)
{
	GtkWidget *hbox, *stype, *stype_optionmenu, *stype_menu;
	GtkWidget *menuitem, *first_menuitem = NULL;
	GtkWidget *stype_html, *notebook, *service;
	int page;
	GList *s;

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

	stype = gtk_label_new (label_text);
	gtk_box_pack_start (GTK_BOX (hbox), stype, FALSE, FALSE, 0);
	gtk_misc_set_alignment (GTK_MISC (stype), 1, 0.5);

	stype_optionmenu = gtk_option_menu_new ();
	gtk_box_pack_start (GTK_BOX (hbox), stype_optionmenu, TRUE, TRUE, 0);
	stype_menu = gtk_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (stype_optionmenu),
				  stype_menu);

	stype_html = html_new (TRUE);
	gtk_object_set_data (GTK_OBJECT (vbox), "html", stype_html);
	gtk_box_pack_start (GTK_BOX (vbox), stype_html->parent->parent,
			    TRUE, TRUE, 0);

	notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);
	gtk_object_set_data (GTK_OBJECT (vbox), "notebook", notebook);
	gtk_signal_connect (GTK_OBJECT (notebook), "destroy",
			    GTK_SIGNAL_FUNC (destroy_service), urlp);

	for (s = services, page = 0; s; s = s->next, page++) {
		struct service_type *st = s->data;

		menuitem = gtk_menu_item_new_with_label (_(st->provider->name));
		if (!first_menuitem)
			first_menuitem = menuitem;
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				    GTK_SIGNAL_FUNC (stype_menuitem_activate),
				    vbox);
		gtk_menu_append (GTK_MENU (stype_menu), menuitem);

		service = (*create_service) (st);
		gtk_notebook_append_page (GTK_NOTEBOOK (notebook), service,
					  NULL);
		gtk_object_set_data (GTK_OBJECT (service), "box", vbox);

		gtk_object_set_data (GTK_OBJECT (menuitem), "page",
				     GUINT_TO_POINTER (page));
		gtk_object_set_data (GTK_OBJECT (menuitem), "description",
				     st->provider->description);
	}

	stype_menuitem_activate (GTK_OBJECT (first_menuitem),
				 GTK_OBJECT (vbox));
	gtk_option_menu_set_history (GTK_OPTION_MENU (stype_optionmenu), 0);

	gtk_widget_show_all (vbox);
}

static void
create_source_page (GtkWidget *vbox, GList *sources, char **urlp)
{
	GtkWidget *html;

	html = html_new (FALSE);
	put_html (GTK_HTML (html),
		  _("Select the kind of mail server you have, and enter "
		    "the relevant information about it.\n\nIf the server "
		    "requires authentication, you can click the "
		    "\"Detect supported types...\" button after entering "
		    "the other information."));
	gtk_box_pack_start (GTK_BOX (vbox), html->parent, FALSE, TRUE, 0);

	create_service_page (vbox, "Mail source type:", sources,
			     create_source, urlp);
}

static void
create_transport_page (GtkWidget *vbox, GList *transports, char **urlp)
{
	GtkWidget *html;

	html = html_new (FALSE);
	put_html (GTK_HTML (html),
		  _("Select the method you would like to use to deliver "
		    "your mail."));
	gtk_box_pack_start (GTK_BOX (vbox), html->parent, FALSE, TRUE, 0);

	create_service_page (vbox, "Mail transport type:", transports,
			     create_transport, urlp);
}


/* Generic stuff */

static GList *
add_service (GList *services, CamelProviderType type, CamelProvider *prov)
{
	CamelService *service;
	CamelException *ex;
	char *url;
	struct service_type *st;

	ex = camel_exception_new ();

	url = g_strdup_printf ("%s:", prov->protocol);
	service = camel_session_get_service (session, url, type, ex);
	g_free (url);
	if (!service) {
		camel_exception_free (ex);
		return services;
	}

	st = g_new (struct service_type, 1);
	st->provider = prov;
	st->service = service;
	st->authtypes = camel_service_query_auth_types (st->service, ex);
	camel_exception_free (ex);

	return g_list_append (services, st);
}

static GdkImlibImage *
load_image (const char *name)
{
	char *path;
	GdkImlibImage *image;

	path = g_strdup_printf ("/usr/local/share/images/evolution/%s", name);
	image = gdk_imlib_load_image (path);
	g_free (path);

	return image;
}

static void
prepare_first (GnomeDruidPage *page, GnomeDruid *druid, gpointer user_data)
{
	gnome_druid_set_buttons_sensitive (druid, TRUE, TRUE, TRUE);
}

static struct identity_record idrec;
static char *source, *transport;

static void
cancel (GnomeDruid *druid, gpointer window)
{
	gtk_window_set_modal (window, FALSE);
	gtk_widget_destroy (window);
	gtk_main_quit ();
}

static void
finish (GnomeDruidPage *page, gpointer arg1, gpointer window)
{
	char *path;

	cancel (arg1, window);

	/* According to the API docs, there's an easier way to do this,
	 * except that it doesn't work. Anyway, this will be replaced
	 * by GConf eventually. FIXME.
	 */

	path = g_strdup_printf ("=%s/config=/mail/configured", evolution_dir);
	gnome_config_set_bool (path, TRUE);
	g_free (path);

	path = g_strdup_printf ("=%s/config=/mail/id_name", evolution_dir);
	gnome_config_set_string (path, idrec.name);
	g_free (path);

	path = g_strdup_printf ("=%s/config=/mail/id_addr", evolution_dir);
	gnome_config_set_string (path, idrec.address);
	g_free (path);

	path = g_strdup_printf ("=%s/config=/mail/id_org", evolution_dir);
	gnome_config_set_string (path, idrec.organization);
	g_free (path);

	path = g_strdup_printf ("=%s/config=/mail/id_sig", evolution_dir);
	gnome_config_set_string (path, idrec.sigfile);
	g_free (path);

	path = g_strdup_printf ("=%s/config=/mail/source", evolution_dir);
	gnome_config_set_string (path, source);
	g_free (path);

	path = g_strdup_printf ("=%s/config=/mail/transport", evolution_dir);
	gnome_config_set_string (path, transport);
	g_free (path);

	gnome_config_sync ();
}

void
mail_config_druid (void)
{
	GnomeDruid *druid;
	GtkWidget *page, *window;
	GnomeDruidPageStandard *dpage;
	GList *providers, *p, *sources, *transports;
	GdkImlibImage *mail_logo, *identity_logo;
	GdkImlibImage *source_logo, *transport_logo;

	/* Fetch list of all providers. */
	providers = camel_session_list_providers (session, TRUE);
	sources = transports = NULL;
	for (p = providers; p; p = p->next) {
		CamelProvider *prov = p->data;

		if (prov->object_types[CAMEL_PROVIDER_STORE]) {
			sources = add_service (sources,
					       CAMEL_PROVIDER_STORE,
					       prov);
		} else if (prov->object_types[CAMEL_PROVIDER_TRANSPORT]) {
			transports = add_service (transports,
						  CAMEL_PROVIDER_TRANSPORT,
						  prov);
		}
	}

	mail_logo = load_image ("evolution-inbox.png");
	identity_logo = load_image ("malehead.png");
	source_logo = mail_logo;
	transport_logo = load_image ("envelope.png");

	window = gtk_window_new (GTK_WINDOW_DIALOG);
	druid = GNOME_DRUID (gnome_druid_new ());
	gtk_signal_connect (GTK_OBJECT (druid), "cancel",
			    GTK_SIGNAL_FUNC (cancel), window);

	/* Start page */
	page = gnome_druid_page_start_new_with_vals (_("Mail Configuration"),
						     "blah blah blah",
						     mail_logo, NULL);
	gnome_druid_page_start_set_logo_bg_color (
		GNOME_DRUID_PAGE_START (page),
		&GNOME_DRUID_PAGE_START (page)->background_color);
	gnome_druid_append_page (druid, GNOME_DRUID_PAGE (page));
	gtk_signal_connect (GTK_OBJECT (page), "prepare",
			    GTK_SIGNAL_FUNC (prepare_first), NULL);
	gtk_widget_show_all (page);


	/* Identity page */
	page = gnome_druid_page_standard_new_with_vals (_("Identity"),
							identity_logo);
	dpage = GNOME_DRUID_PAGE_STANDARD (page);
	gnome_druid_page_standard_set_logo_bg_color (dpage,
						     &dpage->background_color);
	gtk_container_set_border_width (GTK_CONTAINER (dpage->vbox), 8);
	gtk_box_set_spacing (GTK_BOX (dpage->vbox), 5);
	create_identity_page (dpage->vbox, &idrec);
	gtk_object_set_data (GTK_OBJECT (dpage->vbox), "exit_button",
			     druid->next);
	gnome_druid_append_page (druid, GNOME_DRUID_PAGE (page));
	gtk_signal_connect (GTK_OBJECT (page), "prepare",
			    GTK_SIGNAL_FUNC (prepare_identity), dpage->vbox);
	gtk_signal_connect (GTK_OBJECT (page), "next",
			    GTK_SIGNAL_FUNC (identity_next), dpage->vbox);
	gtk_widget_show (page);


	/* Source page */
	page = gnome_druid_page_standard_new_with_vals (_("Mail Source"),
							source_logo);
	dpage = GNOME_DRUID_PAGE_STANDARD (page);
	gnome_druid_page_standard_set_logo_bg_color (dpage,
						     &dpage->background_color);
	gtk_container_set_border_width (GTK_CONTAINER (dpage->vbox), 8);
	gtk_box_set_spacing (GTK_BOX (dpage->vbox), 5);
	create_source_page (dpage->vbox, sources, &source);
	gtk_object_set_data (GTK_OBJECT (dpage->vbox), "exit_button",
			     druid->next);
	gnome_druid_append_page (druid, GNOME_DRUID_PAGE (page));
	gtk_signal_connect (GTK_OBJECT (page), "prepare", 
			    GTK_SIGNAL_FUNC (prepare_service), dpage->vbox);
	gtk_widget_show (page);


	/* Transport page */
	page = gnome_druid_page_standard_new_with_vals (_("Mail Transport"),
							transport_logo);
	dpage = GNOME_DRUID_PAGE_STANDARD (page);
	gnome_druid_page_standard_set_logo_bg_color (dpage,
						     &dpage->background_color);
	gtk_container_set_border_width (GTK_CONTAINER (dpage->vbox), 8);
	gtk_box_set_spacing (GTK_BOX (dpage->vbox), 5);
	create_transport_page (dpage->vbox, transports, &transport);
	gtk_object_set_data (GTK_OBJECT (dpage->vbox), "exit_button",
			     druid->next);
	gnome_druid_append_page (druid, GNOME_DRUID_PAGE (page));
	gtk_signal_connect (GTK_OBJECT (page), "prepare", 
			    GTK_SIGNAL_FUNC (prepare_service), dpage->vbox);
	gtk_widget_show (page);


	/* Finish page */
	page = gnome_druid_page_finish_new_with_vals (_("Mail Configuration"),
						      "blah blah blah done",
						      mail_logo, NULL);
	gnome_druid_page_finish_set_logo_bg_color (
		GNOME_DRUID_PAGE_FINISH (page),
		&GNOME_DRUID_PAGE_FINISH (page)->background_color);
	gnome_druid_append_page (druid, GNOME_DRUID_PAGE (page));
	gtk_signal_connect (GTK_OBJECT (page), "finish",
			    GTK_SIGNAL_FUNC (finish), window);
	gtk_widget_show_all (page);

	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (druid));

	gtk_widget_show (GTK_WIDGET (druid));
	gtk_widget_show (window);
	gtk_widget_queue_resize (window);
	gnome_druid_set_buttons_sensitive (druid, FALSE, TRUE, TRUE);

	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_main ();
}
