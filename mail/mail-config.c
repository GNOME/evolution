/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-config.c: Mail configuration dialogs/wizard. */

/* 
 * Authors: 
 *  Dan Winship <danw@helixcode.com>
 *  Jeffrey Stedfast <fejj@helixcode.com>
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
#include <glade/glade.h>

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

/* private prototypes - these are ugly, rename some of them? */
static void html_size_req (GtkWidget *widget, GtkRequisition *requisition);
static GtkWidget *html_new (gboolean white);
static void put_html (GtkHTML *html, char *text);
static void error_dialog (GtkWidget *parent_finder, const char *fmt, ...);
static void identity_note_doneness (GtkObject *page, gpointer user_data);
static void prepare_identity (GnomeDruidPage *page, gpointer arg1, gpointer user_data);
static gboolean identity_next (GnomeDruidPage *page, gpointer arg1, gpointer user_data);
static void destroy_identity (GtkObject *table, gpointer idrecp);
static void create_identity_page (GtkWidget *vbox, struct identity_record *idrec);
static void service_note_doneness (GtkObject *page, gpointer user_data);
static void prepare_service (GnomeDruidPage *page, gpointer arg1, gpointer user_data);
static void auth_menuitem_activate (GtkObject *menuitem, GtkHTML *html);
static void fill_auth_menu (GtkOptionMenu *optionmenu, GtkHTML *html, GList *authtypes);
static char *get_service_url (GtkObject *table);
static void set_service_url (GtkObject *table, char *url_str);
static void autodetect_cb (GtkWidget *button, GtkObject *table);
static gboolean service_acceptable (GtkNotebook *notebook);
static gboolean service_next (GnomeDruidPage *page, gpointer arg1, gpointer user_data);
static void destroy_service (GtkObject *notebook, gpointer urlp);
static void add_row (GtkWidget *table, int row, const char *label_text, const char *tag, int flag);
static GtkWidget *create_source (struct service_type *st);
static GtkWidget *create_transport (struct service_type *st);
static void stype_menuitem_activate (GtkObject *menuitem, GtkObject *table);
static void create_service_page (GtkWidget *vbox, const char *label_text, GList *services,
				 GtkWidget *(*create_service)(struct service_type *),
				 char **urlp);
static void create_source_page (GtkWidget *vbox, GList *sources, char **urlp);
static void create_transport_page (GtkWidget *vbox, GList *transports, char **urlp);
static GList *add_service (GList *services, CamelProviderType type, CamelProvider *prov);
static GdkImlibImage *load_image (const char *name);
static void prepare_first (GnomeDruidPage *page, GnomeDruid *druid, gpointer user_data);
static void cancel (GnomeDruid *druid, gpointer window);
static void finish (GnomeDruidPage *page, gpointer arg1, gpointer window);
static void on_cmdIdentityAdd_clicked (GtkWidget *widget, gpointer user_data);
static void on_cmdIdentityEdit_clicked (GtkWidget *widget, gpointer user_data);
static void on_cmdIdentityDelete_clicked (GtkWidget *widget, gpointer user_data);
static void on_cmdSourcesAdd_clicked (GtkWidget *widget, gpointer user_data);
static void on_cmdSourcesEdit_clicked (GtkWidget *widget, gpointer user_data);
static void on_cmdSourcesDelete_clicked (GtkWidget *widget, gpointer user_data);
static void on_cmdCamelServicesOK_clicked (GtkButton *button, gpointer user_data);
static void on_cmdCamelServicesCancel_clicked (GtkButton *button, gpointer user_data);



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


/* Error helper */
static void
error_dialog (GtkWidget *parent_finder, const char *fmt, ...)
{
	GtkWidget *parent, *dialog;
	char *msg;
	va_list ap;

	parent = gtk_widget_get_ancestor (parent_finder, GTK_TYPE_WINDOW);

	va_start (ap, fmt);
	msg = g_strdup_vprintf (fmt, ap);
	va_end (ap);

	dialog = gnome_error_dialog_parented (msg, GTK_WINDOW (parent));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	g_free (msg);
}


/* Identity page */

static void
identity_note_doneness (GtkObject *page, gpointer user_data)
{
	GtkWidget *exit_button;
	GtkEntry *entry;
	char *data;

	if (!(exit_button = gtk_object_get_data (page, "exit_button")))
		exit_button = gtk_object_get_data (page, "ok_button");

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
		error_dialog (GTK_WIDGET (page), "Email address must be "
			      "of the form \"user@domain\".");
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
	GtkWidget *entry;

	entry = gtk_object_get_data (table, "name");
	idrec->name = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

	entry = gtk_object_get_data (table, "addr");
	idrec->address = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

	entry = gtk_object_get_data (table, "org");
	idrec->organization = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

	entry = gtk_object_get_data (table, "sig");
	idrec->sigfile = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
}

static void
create_identity_page (GtkWidget *vbox, struct identity_record *idrec)
{
	GtkWidget *html, *table;
	GtkWidget *name, *addr, *org, *sig;
	GtkWidget *name_entry, *addr_entry, *org_entry, *sig_entry;
	GtkWidget *hsep;
	char *user;
	struct passwd *pw = NULL;

	html = html_new (FALSE);
	put_html (GTK_HTML (html),
		  _("Enter your name and email address to be used in "
		    "outgoing mail. You may also, optionally, enter the "
		    "name of your organization, and the name of a file "
		    "to read your signature from."));
	gtk_box_pack_start (GTK_BOX (vbox), html->parent, FALSE, TRUE, 0);

	table = gtk_table_new (5, 2, FALSE);
	gtk_widget_set_name (table, "table");
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

	if (!idrec || !idrec->name) {
		user = getenv ("USER");
		if (user)
			pw = getpwnam (user);
		else
			pw = getpwuid (getuid ());
	}
	if ((idrec && idrec->name) || (pw && pw->pw_gecos && *pw->pw_gecos)) {
		char *name;

		if (idrec && idrec->name)
			name = g_strdup (idrec->name);
		else
			name = g_strndup (pw->pw_gecos, strcspn (pw->pw_gecos, ","));
		gtk_entry_set_text (GTK_ENTRY (name_entry), name);
		g_free (name);
	}

	addr = gtk_label_new (_("Email address:"));
	gtk_table_attach (GTK_TABLE (table), addr, 0, 1, 1, 2,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (addr), 1, 0.5);

	addr_entry = gtk_entry_new ();
	if (idrec && idrec->address)
		gtk_entry_set_text (GTK_ENTRY (addr_entry), idrec->address);
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
	if (idrec && idrec->organization)
		gtk_entry_set_text (GTK_ENTRY (org_entry), idrec->organization);
	gtk_table_attach (GTK_TABLE (table), org_entry, 1, 2, 3, 4,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_object_set_data (GTK_OBJECT (vbox), "org", org_entry);

	sig = gtk_label_new (_("Signature file:"));
	gtk_table_attach (GTK_TABLE (table), sig, 0, 1, 4, 5,
			  GTK_FILL, GTK_FILL, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (sig), 1, 0);

	sig_entry = gnome_file_entry_new (NULL, _("Signature File"));
	if (idrec && idrec->sigfile)
		gtk_entry_set_text (GTK_ENTRY (sig_entry), idrec->sigfile);
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
	
	if (!(button = gtk_object_get_data (box, "exit_button")))
		button = gtk_object_get_data (box, "ok_button");

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

	table = gtk_notebook_get_nth_page (notebook,
					   gtk_notebook_get_current_page (notebook));

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
set_service_url (GtkObject *table, char *url_str)
{
	CamelURL *url;
	GtkEditable *editable;
	GtkOptionMenu *auth_optionmenu;
	CamelException *ex;

	g_return_if_fail (table != NULL);
	g_return_if_fail (url_str != NULL);

	ex = camel_exception_new ();
	
	url = camel_url_new (url_str, ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		camel_exception_free (ex);
		return;
	}

	editable = gtk_object_get_data (table, "user_entry");
	if (editable && url)
		gtk_entry_set_text (GTK_ENTRY (editable), url->user);

	editable = gtk_object_get_data (table, "server_entry");
	if (editable && url)
		gtk_entry_set_text (GTK_ENTRY (editable), url->host);

	editable = gtk_object_get_data (table, "path_entry");
	if (editable && url)
		gtk_entry_set_text (GTK_ENTRY (editable), url->path);

        /* How are we gonna do this? */
	auth_optionmenu = gtk_object_get_data (table, "auth_optionmenu");
	if (auth_optionmenu) {
#if 0
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
#endif
	}

	camel_exception_free (ex);
	camel_url_free (url);
}

static void
autodetect_cb (GtkWidget *button, GtkObject *table)
{
	char *url;
	CamelException *ex;
	CamelService *service;
	GList *authtypes;
	GtkHTML *html;
	GtkOptionMenu *optionmenu;
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
	camel_exception_free (ex);
	return;

 error:
	error_dialog (button, "Could not detect supported authentication "
		      "types:\n%s", camel_exception_get_description (ex));
	camel_exception_free (ex);
}

static gboolean
service_acceptable (GtkNotebook *notebook)
{
	char *url;
	GtkWidget *table;
	GtkToggleButton *check;
	int page, type;
	CamelService *service;
	CamelException *ex;
	gboolean ok;

	page = gtk_notebook_get_current_page (notebook);
	table = gtk_notebook_get_nth_page (notebook, page);
	check = gtk_object_get_data (GTK_OBJECT (table), "check");

	if (!check || !gtk_toggle_button_get_active (check))
		return TRUE;

	type = GPOINTER_TO_UINT (gtk_object_get_data (GTK_OBJECT (table),
						      "service_type"));
	url = get_service_url (GTK_OBJECT (table));

	ex = camel_exception_new ();
	camel_exception_clear (ex);
	service = camel_session_get_service (session, url, type, ex);
	g_free (url);

	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE)
		goto error;

	ok = camel_service_connect (service, ex);

	if (ok) {
		camel_service_disconnect (service, ex);
		gtk_object_unref (GTK_OBJECT (service));
		camel_exception_free (ex);
		return TRUE;
	}

	gtk_object_unref (GTK_OBJECT (service));

 error:
	error_dialog (GTK_WIDGET (notebook), camel_exception_get_description (ex));
	camel_exception_free (ex);
	return FALSE;
}

static gboolean
service_next (GnomeDruidPage *page, gpointer arg1, gpointer user_data)
{
	return !service_acceptable (user_data);
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
	service_flags = st->service->url_flags & ~CAMEL_SERVICE_URL_NEED_AUTH;

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
	service_flags = st->service->url_flags & ~CAMEL_SERVICE_URL_NEED_AUTH;

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
	gtk_object_set_data (GTK_OBJECT (vbox), "stype_optionmenu", stype_optionmenu);
	gtk_box_pack_start (GTK_BOX (hbox), stype_optionmenu, TRUE, TRUE, 0);
	stype_menu = gtk_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (stype_optionmenu), stype_menu);

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
		gtk_object_set_data (GTK_OBJECT (vbox), st->provider->name, menuitem);
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				    GTK_SIGNAL_FUNC (stype_menuitem_activate),
				    vbox);
		gtk_menu_append (GTK_MENU (stype_menu), menuitem);

		service = (*create_service) (st);
		gtk_notebook_append_page (GTK_NOTEBOOK (notebook), service, NULL);
		gtk_object_set_data (GTK_OBJECT (service), "box", vbox);

		gtk_object_set_data (GTK_OBJECT (menuitem), "page",
				     GUINT_TO_POINTER (page));
		gtk_object_set_data (GTK_OBJECT (menuitem), "description",
				     st->provider->description);
	}

	stype_menuitem_activate (GTK_OBJECT (first_menuitem), GTK_OBJECT (vbox));
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
	GtkWidget *html, *optionmenu, *menuitem;

	html = html_new (FALSE);
	put_html (GTK_HTML (html),
		  _("Select the method you would like to use to deliver your mail."));
	gtk_box_pack_start (GTK_BOX (vbox), html->parent, FALSE, TRUE, 0);

	create_service_page (vbox, "Mail transport type:", transports,
			     create_transport, urlp);

	optionmenu = gtk_object_get_data (GTK_OBJECT (vbox), "stype_optionmenu");

	if (*urlp && !strncasecmp(*urlp, "sendmail", 8)) {
		menuitem = gtk_object_get_data (GTK_OBJECT (vbox), "Sendmail");
		gtk_option_menu_set_history (GTK_OPTION_MENU (optionmenu), 1);
	} else {
		menuitem = gtk_object_get_data (GTK_OBJECT (vbox), "SMTP");
		gtk_option_menu_set_history (GTK_OPTION_MENU (optionmenu), 0);
	}

	stype_menuitem_activate (GTK_OBJECT (menuitem), GTK_OBJECT (vbox));
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
static char *source = NULL, *transport = NULL;
static gboolean format = FALSE;

static void
cancel (GnomeDruid *druid, gpointer window)
{
	gtk_window_set_modal (window, FALSE);
	gtk_widget_destroy (window);
	gtk_main_quit ();
}

static void
write_config (void)
{
	char *path;

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

	path = g_strdup_printf ("=%s/config=/mail/msg_format", evolution_dir);
	gnome_config_set_string (path, format ? "alternative" : "plain");
	g_free (path);

	gnome_config_sync ();
}

static void
finish (GnomeDruidPage *page, gpointer arg1, gpointer window)
{
	cancel (arg1, window);
	write_config();
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

		if (strcmp (prov->domain, "mail") != 0)
			continue;

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
	page = gnome_druid_page_start_new_with_vals (
		_("Mail Configuration"),
		"Welcome to the Evolution Mail configuration wizard!\n"
		"By filling in some information about your email\n"
		"settings,you can start sending and receiving email\n"
		"right away. Click \"Next\" to continue.",
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
	gtk_signal_connect (GTK_OBJECT (page), "next", 
			    GTK_SIGNAL_FUNC (service_next),
			    gtk_object_get_data (GTK_OBJECT (dpage->vbox),
						 "notebook"));
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
	gtk_signal_connect (GTK_OBJECT (page), "next", 
			    GTK_SIGNAL_FUNC (service_next),
			    gtk_object_get_data (GTK_OBJECT (dpage->vbox),
						 "notebook"));
	gtk_widget_show (page);


	/* Finish page */
	page = gnome_druid_page_finish_new_with_vals (
		_("Mail Configuration"),
		"Your email configuration is now complete.\n"
		"Click \"finish\" to save your new settings",
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

static gint identity_row = -1;
static gint source_row = -1;

struct identity_dialog_data {
	GtkWidget *clist;
	struct identity_record *idrec;
	gboolean new_entry;
};

struct source_dialog_data {
	GtkWidget *clist;
	char *source;
	gboolean new_entry;
};


static void
on_cmdIdentityConfigDialogOK_clicked (GtkWidget *dialog, gpointer user_data)
{
	struct identity_dialog_data *data = user_data;
	GtkWidget *vbox = gtk_object_get_data (GTK_OBJECT (dialog), "vbox");

	destroy_identity (GTK_OBJECT (vbox), data->idrec);

	gtk_clist_set_text (GTK_CLIST (data->clist), identity_row, 0, data->idrec->name);
	gtk_clist_set_text (GTK_CLIST (data->clist), identity_row, 1, data->idrec->address);
	gtk_clist_set_text (GTK_CLIST (data->clist), identity_row, 2, data->idrec->organization);
	gtk_clist_set_text (GTK_CLIST (data->clist), identity_row, 3, data->idrec->sigfile);

	gtk_clist_set_row_data (GTK_CLIST (data->clist), identity_row,
				g_memdup (data->idrec, sizeof (struct identity_record)));
}

static void
on_cmdIdentityConfigDialogCancel_clicked (GtkWidget *dialog, gpointer user_data)
{
	struct identity_dialog_data *data = user_data;
	int max_row;
	
	if (data && data->new_entry) {
		gtk_clist_remove (GTK_CLIST (data->clist), identity_row);
                max_row = GTK_CLIST (data->clist)->rows - 1;
		identity_row = identity_row > max_row ? max_row : identity_row;
		gtk_clist_select_row (GTK_CLIST (data->clist), identity_row, 0);
	}
}

static void
on_IdentityConfigDialogButton_clicked (GnomeDialog *dialog, int button, gpointer user_data)
{
	switch (button) {
	case 0: /* OK clicked */
		g_print ("OK clicked\n");
		on_cmdIdentityConfigDialogOK_clicked (GTK_WIDGET (dialog), user_data);
		break;
	case 1: /* Cancel clicked */
		g_print ("Cancel clicked\n");
		on_cmdIdentityConfigDialogCancel_clicked (GTK_WIDGET (dialog), user_data);
		break;
	}

	if (button != -1)
		gnome_dialog_close (dialog);
}

static GtkWidget*
create_identity_config_dialog (gboolean edit_mode, struct identity_record *idrecp, GtkWidget *clist)
{
	GtkWidget *config_dialog;
	GtkWidget *dialog_vbox1;
	GtkWidget *dialog_action_area1;
	GtkWidget *cmdConfigDialogOK;
	GtkWidget *cmdConfigDialogCancel;
	GtkWidget *vbox;
	struct identity_dialog_data *data = NULL;

	if (edit_mode)
		config_dialog = gnome_dialog_new (_("Edit Identity"), NULL);
	else
		config_dialog = gnome_dialog_new (_("Add Identity"), NULL);
	gtk_window_set_modal (GTK_WINDOW (config_dialog), TRUE);
	gtk_widget_set_name (config_dialog, "config_dialog");
	gtk_object_set_data (GTK_OBJECT (config_dialog), "config_dialog", config_dialog);
	gtk_window_set_policy (GTK_WINDOW (config_dialog), TRUE, TRUE, FALSE);
	
	dialog_vbox1 = GNOME_DIALOG (config_dialog)->vbox;
	gtk_widget_set_name (dialog_vbox1, "dialog_vbox1");
	gtk_object_set_data (GTK_OBJECT (config_dialog), "dialog_vbox1", dialog_vbox1);
	gtk_widget_show (dialog_vbox1);
	
	dialog_action_area1 = GNOME_DIALOG (config_dialog)->action_area;
	gtk_widget_set_name (dialog_action_area1, "dialog_action_area1");
	gtk_object_set_data (GTK_OBJECT (config_dialog), "dialog_action_area1", dialog_action_area1);
	gtk_widget_show (dialog_action_area1);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area1), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing (GTK_BUTTON_BOX (dialog_action_area1), 8);

	/* Create the vbox that we will pack the identity widget into */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_set_name (vbox, "vbox");
	gtk_object_set_data (GTK_OBJECT (config_dialog), "vbox", vbox);
	gtk_widget_ref (vbox);
	gtk_object_set_data_full (GTK_OBJECT (config_dialog), "vbox", vbox,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox, TRUE, TRUE, 0);
	
	gnome_dialog_append_button (GNOME_DIALOG (config_dialog), GNOME_STOCK_BUTTON_OK);
	cmdConfigDialogOK = g_list_last (GNOME_DIALOG (config_dialog)->buttons)->data;
	gtk_widget_set_name (cmdConfigDialogOK, "cmdConfigDialogOK");
	gtk_object_set_data (GTK_OBJECT (vbox), "ok_button", cmdConfigDialogOK);
	gtk_widget_ref (cmdConfigDialogOK);
	gtk_object_set_data_full (GTK_OBJECT (config_dialog), "cmdConfigDialogOK", cmdConfigDialogOK,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (cmdConfigDialogOK);
	GTK_WIDGET_SET_FLAGS (cmdConfigDialogOK, GTK_CAN_DEFAULT);
	gtk_widget_set_sensitive (cmdConfigDialogOK, FALSE);
	
	gnome_dialog_append_button (GNOME_DIALOG (config_dialog), GNOME_STOCK_BUTTON_CANCEL);
	cmdConfigDialogCancel = g_list_last (GNOME_DIALOG (config_dialog)->buttons)->data;
	gtk_widget_set_name (cmdConfigDialogCancel, "cmdConfigDialogCancel");
	gtk_widget_ref (cmdConfigDialogCancel);
	gtk_object_set_data_full (GTK_OBJECT (config_dialog), "cmdConfigDialogCancel", cmdConfigDialogCancel,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (cmdConfigDialogCancel);
	GTK_WIDGET_SET_FLAGS (cmdConfigDialogCancel, GTK_CAN_DEFAULT);
	
        /* create/pack our Identity widget */
	create_identity_page (vbox, &idrec);
	
	/*gtk_signal_disconnect_by_func (GTK_OBJECT (vbox), GTK_SIGNAL_FUNC (destroy_identity), NULL);*/

	data = g_malloc0 (sizeof (struct identity_dialog_data));
	data->clist  = clist;
	data->idrec  = idrecp;
	data->new_entry = !edit_mode;

	gtk_signal_connect(GTK_OBJECT (config_dialog), "clicked",
			   GTK_SIGNAL_FUNC (on_IdentityConfigDialogButton_clicked),
			   data);
	/*
	gtk_signal_connect (GTK_OBJECT (cmdConfigDialogOK), "clicked",
			    GTK_SIGNAL_FUNC (on_cmdIdentityConfigDialogOK_clicked),
			    data);
	
	gtk_signal_connect (GTK_OBJECT (cmdConfigDialogCancel), "clicked",
			    GTK_SIGNAL_FUNC (on_cmdIdentityConfigDialogCancel_clicked),
			    data);
	*/
	
	return config_dialog;
}

static void
on_SourceConfigDialogButton_clicked (GnomeDialog *dialog, int button, gpointer user_data)
{
	struct source_dialog_data *data = user_data;
	GtkWidget *vbox;
	GtkWidget *notebook;
	int max_row;

	switch (button) {
	case 0: /* OK clicked */
		vbox = gtk_object_get_data (GTK_OBJECT (dialog), "vbox");
		notebook = gtk_object_get_data (GTK_OBJECT (vbox), "notebook");
		
		destroy_service (GTK_OBJECT (notebook), &data->source);
		
		gtk_clist_set_text (GTK_CLIST (data->clist), source_row, 0, data->source);
		gtk_clist_set_row_data (GTK_CLIST (data->clist), source_row,
					g_strdup (data->source));
		source = data->source;
		break;
	case 1: /* Cancel clicked */
		g_print ("Cancel clicked\n");
		if (data && data->new_entry) {
			gtk_clist_remove (GTK_CLIST (data->clist), source_row);
			max_row = GTK_CLIST (data->clist)->rows - 1;
			source_row = source_row > max_row ? max_row : source_row;
			gtk_clist_select_row (GTK_CLIST (data->clist), source_row, 0);
		}
		break;
	}

	if (button != -1) {
		gnome_dialog_close (dialog);
	}
}

static GtkWidget*
create_source_config_dialog (gboolean edit_mode, char **sourcep, GtkWidget *clist)
{
	GtkWidget *config_dialog;
	GtkWidget *dialog_vbox1;
	GtkWidget *dialog_action_area1;
	GtkWidget *cmdConfigDialogOK;
	GtkWidget *cmdConfigDialogCancel;
	GtkWidget *vbox;
	GList *providers, *p, *sources, *transports;
	struct source_dialog_data *data = NULL;

        /* Fetch list of all providers. */
	providers = camel_session_list_providers (session, TRUE);
	sources = transports = NULL;
	for (p = providers; p; p = p->next) {
		CamelProvider *prov = p->data;

		if (strcmp (prov->domain, "mail") != 0)
			continue;

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
	
	if (edit_mode)
		config_dialog = gnome_dialog_new (_("Edit Source"), NULL);
	else
		config_dialog = gnome_dialog_new (_("Add Source"), NULL);
	gtk_window_set_modal (GTK_WINDOW (config_dialog), TRUE);
	gtk_widget_set_name (config_dialog, "config_dialog");
	gtk_object_set_data (GTK_OBJECT (config_dialog), "config_dialog", config_dialog);
	gtk_window_set_policy (GTK_WINDOW (config_dialog), TRUE, TRUE, FALSE);
	gtk_window_set_default_size (GTK_WINDOW (config_dialog), 380, 450);
	
	dialog_vbox1 = GNOME_DIALOG (config_dialog)->vbox;
	gtk_widget_set_name (dialog_vbox1, "dialog_vbox1");
	gtk_object_set_data (GTK_OBJECT (config_dialog), "dialog_vbox1", dialog_vbox1);
	gtk_widget_show (dialog_vbox1);
	
	dialog_action_area1 = GNOME_DIALOG (config_dialog)->action_area;
	gtk_widget_set_name (dialog_action_area1, "dialog_action_area1");
	gtk_object_set_data (GTK_OBJECT (config_dialog), "dialog_action_area1", dialog_action_area1);
	gtk_widget_show (dialog_action_area1);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area1), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing (GTK_BUTTON_BOX (dialog_action_area1), 8);

	/* Create the vbox that we will pack the source widget into */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_set_name (vbox, "vbox");
	gtk_object_set_data (GTK_OBJECT (config_dialog), "vbox", vbox);
	gtk_widget_ref (vbox);
	gtk_object_set_data_full (GTK_OBJECT (config_dialog), "vbox", vbox,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox, TRUE, TRUE, 0);
	
	gnome_dialog_append_button (GNOME_DIALOG (config_dialog), GNOME_STOCK_BUTTON_OK);
	cmdConfigDialogOK = g_list_last (GNOME_DIALOG (config_dialog)->buttons)->data;
	gtk_widget_set_name (cmdConfigDialogOK, "cmdConfigDialogOK");
	gtk_object_set_data (GTK_OBJECT (vbox), "ok_button", cmdConfigDialogOK);
	gtk_widget_ref (cmdConfigDialogOK);
	gtk_object_set_data_full (GTK_OBJECT (config_dialog), "cmdConfigDialogOK", cmdConfigDialogOK,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (cmdConfigDialogOK);
	GTK_WIDGET_SET_FLAGS (cmdConfigDialogOK, GTK_CAN_DEFAULT);
	gtk_widget_set_sensitive (cmdConfigDialogOK, FALSE);
	
	gnome_dialog_append_button (GNOME_DIALOG (config_dialog), GNOME_STOCK_BUTTON_CANCEL);
	cmdConfigDialogCancel = g_list_last (GNOME_DIALOG (config_dialog)->buttons)->data;
	gtk_widget_set_name (cmdConfigDialogCancel, "cmdConfigDialogCancel");
	gtk_widget_ref (cmdConfigDialogCancel);
	gtk_object_set_data_full (GTK_OBJECT (config_dialog), "cmdConfigDialogCancel", cmdConfigDialogCancel,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (cmdConfigDialogCancel);
	GTK_WIDGET_SET_FLAGS (cmdConfigDialogCancel, GTK_CAN_DEFAULT);
	
        /* create/pack our source widget */
	create_source_page (vbox, sources, sourcep);
	
	data = g_malloc0 (sizeof (struct source_dialog_data));
	data->clist = clist;
	data->source = *sourcep;
	data->new_entry = !edit_mode;

	gtk_signal_connect(GTK_OBJECT (config_dialog), "clicked",
			   GTK_SIGNAL_FUNC (on_SourceConfigDialogButton_clicked),
			   data);
	
	return config_dialog;
}

static void
on_clistIdentities_select_row (GtkWidget *widget, gint row, gint column,
			       GdkEventButton *event, gpointer data)
{
	identity_row = row;
}

static void
on_clistSources_select_row (GtkWidget *widget, gint row, gint column,
			       GdkEventButton *event, gpointer data)
{
	source_row = row;
}

static void
on_cmdIdentityAdd_clicked (GtkWidget *widget, gpointer user_data)
{
	GtkWidget *dialog;
	char *text[] = { "", "", "", "" };

	gtk_clist_append (GTK_CLIST (user_data), text);

	if (identity_row > -1)
		gtk_clist_unselect_row (GTK_CLIST (user_data), identity_row, 0);

	gtk_clist_select_row (GTK_CLIST (user_data), GTK_CLIST (user_data)->rows - 1, 0);

	/* now create the editing dialog */
	dialog = create_identity_config_dialog (FALSE, &idrec, GTK_WIDGET (user_data));
	gtk_widget_show (dialog);
}

static void
on_cmdIdentityEdit_clicked (GtkWidget *widget, gpointer user_data)
{
	GtkWidget *dialog;
	struct identity_record *idrecp;
	
	if (identity_row == -1)
		return;

	idrecp = gtk_clist_get_row_data (GTK_CLIST (user_data), identity_row);
	if (!idrecp) {
		idrecp = &idrec;
#if 0
		g_free (idrecp->name);
		idrecp->name = NULL;
		g_free (idrecp->address);
		idrecp->address = NULL;
		g_free (idrecp->organization);
		idrecp->organization = NULL;
		g_free (idrecp->sigfile);
		idrecp->sigfile = NULL;
#endif
	}

	/* now create the editing dialog */
	dialog = create_identity_config_dialog (TRUE, idrecp, GTK_WIDGET (user_data));
	gtk_widget_show (dialog);
}

static void
on_cmdIdentityDelete_clicked (GtkWidget *widget, gpointer user_data)
{
	if (identity_row == -1)
		return;
	
	gtk_clist_remove (GTK_CLIST (user_data), identity_row);
	identity_row = -1;
}

static void
on_cmdSourcesAdd_clicked (GtkWidget *widget, gpointer user_data)
{
	GtkWidget *dialog;
	char *text[] = { "" };

	gtk_clist_append (GTK_CLIST (user_data), text);

	if (source_row > -1)
		gtk_clist_unselect_row (GTK_CLIST (user_data), source_row, 0);

	gtk_clist_select_row (GTK_CLIST (user_data), GTK_CLIST (user_data)->rows - 1, 0);
	
	/* now create the editing dialog */
	dialog = create_source_config_dialog (FALSE, &source, GTK_WIDGET (user_data));
	gtk_widget_show (dialog);
}

static void
on_cmdSourcesEdit_clicked (GtkWidget *widget, gpointer user_data)
{
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *interior_notebook;
	GtkWidget *table;
	int page;
	char *sourcep;
	
	if (source_row == -1)
		return;

	sourcep = gtk_clist_get_row_data (GTK_CLIST (user_data), source_row);
	if (sourcep) {
		source = sourcep;
	}

	/* now create the editing dialog */
	dialog = create_source_config_dialog (TRUE, &sourcep, GTK_WIDGET (user_data));

        /* Set the data in the source editor */
	vbox = gtk_object_get_data (GTK_OBJECT (dialog), "vbox");
	interior_notebook = gtk_object_get_data (GTK_OBJECT (vbox), "notebook");
	page = gtk_notebook_get_current_page (GTK_NOTEBOOK (interior_notebook));
	table = gtk_notebook_get_nth_page (GTK_NOTEBOOK (interior_notebook), page);
	set_service_url (GTK_OBJECT (table), source);

	gtk_widget_show (dialog);
}

static void
on_cmdSourcesDelete_clicked (GtkWidget *widget, gpointer user_data)
{
	if (source_row == -1)
		return;
	
	gtk_clist_remove (GTK_CLIST (user_data), source_row);
	source_row = -1;
}

static void
on_cmdCamelServicesOK_clicked (GtkButton *button, gpointer user_data)
{
	GtkWidget *notebook, *interior_notebook;
        GtkWidget *vbox;

	notebook = gtk_object_get_data (GTK_OBJECT (user_data), "notebook");

	/* switch to the third page which is the transport page */
	gtk_notebook_set_page (GTK_NOTEBOOK (notebook), 2);

	vbox = gtk_object_get_data (GTK_OBJECT (notebook), "transport_page_vbox");
	interior_notebook = gtk_object_get_data (GTK_OBJECT (vbox), "notebook");

       	if (service_acceptable (GTK_NOTEBOOK (interior_notebook))) {
		destroy_service (GTK_OBJECT (interior_notebook), (gpointer) &transport);
		gtk_widget_destroy (GTK_WIDGET (user_data));
	}

	write_config();
}

static void
on_cmdCamelServicesCancel_clicked (GtkButton *button, gpointer user_data)
{
	gtk_widget_destroy(GTK_WIDGET (user_data));
}

static void
on_chkFormat_toggled (GtkWidget *widget, gpointer user_data)
{
	format = GTK_TOGGLE_BUTTON (widget)->active;
}

GtkWidget*
providers_config_new (void)
{
	GladeXML *gui;
	GtkWidget *providers_config;
	GtkWidget *dialog_vbox1;
	GtkWidget *notebook;
	GtkWidget *clistIdentities;
	GtkWidget *cmdIdentityAdd;
	GtkWidget *cmdIdentityEdit;
	GtkWidget *cmdIdentityDelete;
	GtkWidget *clistSources;
	GtkWidget *cmdSourcesAdd;
	GtkWidget *cmdSourcesEdit;
	GtkWidget *cmdSourcesDelete;
	GtkWidget *cmdCamelServicesOK;
	GtkWidget *cmdCamelServicesCancel;
	GtkWidget *transport_page_vbox;
	GtkWidget *chkFormat;
	GList *providers, *p, *sources, *transports;
	GtkWidget *table, *interior_notebook;
	char *path;
	gboolean configured;
	int page;


	/* Fetch list of all providers. */
	providers = camel_session_list_providers (session, TRUE);
	sources = transports = NULL;
	for (p = providers; p; p = p->next) {
		CamelProvider *prov = p->data;

		if (strcmp (prov->domain, "mail") != 0)
			continue;

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

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", NULL);
	providers_config = glade_xml_get_widget (gui, "dialog");

	gtk_widget_set_name (providers_config, "providers_config");
	gtk_object_set_data (GTK_OBJECT (providers_config), 
			     "providers_config", providers_config);

	dialog_vbox1 = glade_xml_get_widget (gui, "dialog_vbox1");
	gtk_object_set_data (GTK_OBJECT (providers_config), 
			     "dialog_vbox1", dialog_vbox1);

	notebook = glade_xml_get_widget (gui, "notebook");
	gtk_widget_ref (notebook);
	gtk_object_set_data_full (GTK_OBJECT (providers_config), 
				  "notebook", notebook,
				  (GtkDestroyNotify) gtk_widget_unref);

	clistIdentities = glade_xml_get_widget (gui, "clistIdentities");
	gtk_widget_ref (clistIdentities);
	gtk_object_set_data_full (GTK_OBJECT (providers_config), 
				  "clistIdentities", clistIdentities,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_clist_set_column_width (GTK_CLIST (clistIdentities), 0, 80);

	/* Find out if stuff has been configured */
	path = g_strdup_printf ("=%s/config=/mail/configured", evolution_dir);
	configured = gnome_config_get_bool (path);
	g_free (path);

	identity_row = -1;
	if (configured) {
		char *text[] = { "", "", "", "" };
		struct identity_record *data;

		/* add an entry to the identity clist */
		gtk_clist_append (GTK_CLIST (clistIdentities), text);

		path = g_strdup_printf ("=%s/config=/mail/id_name", evolution_dir);
		idrec.name = gnome_config_get_string (path);
		gtk_clist_set_text (GTK_CLIST (clistIdentities), 0, 0, idrec.name);
		g_free (path);

		path = g_strdup_printf ("=%s/config=/mail/id_addr", evolution_dir);
		idrec.address = gnome_config_get_string (path);
		gtk_clist_set_text (GTK_CLIST (clistIdentities), 0, 1, idrec.address);
		g_free (path);

		path = g_strdup_printf ("=%s/config=/mail/id_org", evolution_dir);
		idrec.organization = gnome_config_get_string (path);
		gtk_clist_set_text (GTK_CLIST (clistIdentities), 0, 2, idrec.organization);
		g_free (path);

		path = g_strdup_printf ("=%s/config=/mail/id_sig", evolution_dir);
		idrec.sigfile = gnome_config_get_string (path);
		gtk_clist_set_text (GTK_CLIST (clistIdentities), 0, 3, idrec.sigfile);
		g_free (path);

		data = g_malloc0 (sizeof (struct identity_record));
		data->name = g_strdup (idrec.name);
		data->address = g_strdup (idrec.address);
		data->organization = g_strdup (idrec.organization);
		data->sigfile = g_strdup (idrec.sigfile);
		gtk_clist_set_row_data (GTK_CLIST (clistIdentities), 0, data);
	}

	cmdIdentityAdd = glade_xml_get_widget (gui, "cmdIdentityAdd");
	cmdIdentityEdit = glade_xml_get_widget (gui, "cmdIdentityEdit");
	cmdIdentityDelete = glade_xml_get_widget (gui, "cmdIdentityDelete");

	clistSources = glade_xml_get_widget (gui, "clistSources");
	gtk_widget_ref (clistSources);
	gtk_object_set_data_full (GTK_OBJECT (providers_config), 
				  "clistSources", clistSources,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_clist_set_column_width (GTK_CLIST (clistSources), 0, 80);

	if (configured && !source) {
		path = g_strdup_printf ("=%s/config=/mail/source", evolution_dir);
		source = gnome_config_get_string (path);
		g_free (path);
	}

	source_row = -1;
	if (source) {
		char *text[] = { "" };

		gtk_clist_append (GTK_CLIST (clistSources), text);

		gtk_clist_set_text (GTK_CLIST (clistSources), 0, 0, source);
		gtk_clist_set_row_data (GTK_CLIST (clistSources), 0, g_strdup(source));
	}

	cmdSourcesAdd = glade_xml_get_widget (gui, "cmdSourcesAdd");
	cmdSourcesEdit = glade_xml_get_widget (gui, "cmdSourcesEdit");
	cmdSourcesDelete = glade_xml_get_widget (gui, "cmdSourcesDelete");

	/* Setup the transport page */
	transport_page_vbox = glade_xml_get_widget (gui, "transport_page_vbox");
	gtk_object_set_data (GTK_OBJECT (notebook), "transport_page_vbox", transport_page_vbox);
	gtk_widget_ref (transport_page_vbox);
	gtk_object_set_data_full (GTK_OBJECT (providers_config), "transport_page_vbox", transport_page_vbox,
				  (GtkDestroyNotify) gtk_widget_unref);

	if (configured && !transport) {
		path = g_strdup_printf ("=%s/config=/mail/transport", evolution_dir);
		transport = gnome_config_get_string (path);
		g_free (path);
	}
	create_transport_page (transport_page_vbox, transports, &transport);

	/* Set the data in the transports page */
	interior_notebook = gtk_object_get_data (GTK_OBJECT (transport_page_vbox), "notebook");
	page = gtk_notebook_get_current_page (GTK_NOTEBOOK (interior_notebook));
	if (transport != NULL && !strncasecmp(transport, "Sendmail", 8))
		page = 1;
	else
		page = 0;
	table = gtk_notebook_get_nth_page (GTK_NOTEBOOK (interior_notebook), page);
	set_service_url (GTK_OBJECT (table), transport);
	

	/* Lets make a page to mark Send HTML or text/plan...yay */
	chkFormat = glade_xml_get_widget (gui, "chkFormat");
	gtk_object_set_data (GTK_OBJECT (notebook), "chkFormat", chkFormat);
	gtk_widget_ref (chkFormat);
	gtk_object_set_data_full (GTK_OBJECT (providers_config), "chkFormat", chkFormat,
				  (GtkDestroyNotify) gtk_widget_unref);

	if (configured) {
		char *buf;

		path = g_strdup_printf ("=%s/config=/mail/msg_format", evolution_dir);
		buf = gnome_config_get_string (path);
		g_free (path);

		if (!buf || !strcmp(buf, "alternative"))
			format = TRUE;
		else
			format = FALSE;

		g_free (buf);
	}
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chkFormat), format);

	cmdCamelServicesOK = glade_xml_get_widget (gui, "cmdCamelServicesOK");
	cmdCamelServicesCancel = glade_xml_get_widget (gui, "cmdCamelServicesCancel");

	gtk_signal_connect (GTK_OBJECT (cmdIdentityAdd), "clicked",
			    GTK_SIGNAL_FUNC (on_cmdIdentityAdd_clicked),
			    clistIdentities);
	gtk_signal_connect (GTK_OBJECT (cmdIdentityEdit), "clicked",
			    GTK_SIGNAL_FUNC (on_cmdIdentityEdit_clicked),
			    clistIdentities);
	gtk_signal_connect (GTK_OBJECT (cmdIdentityDelete), "clicked",
			    GTK_SIGNAL_FUNC (on_cmdIdentityDelete_clicked),
			    clistIdentities);
	
	gtk_signal_connect (GTK_OBJECT (cmdSourcesAdd), "clicked",
			    GTK_SIGNAL_FUNC (on_cmdSourcesAdd_clicked),
			    clistSources);
	gtk_signal_connect (GTK_OBJECT (cmdSourcesEdit), "clicked",
			    GTK_SIGNAL_FUNC (on_cmdSourcesEdit_clicked),
			    clistSources);
	gtk_signal_connect (GTK_OBJECT (cmdSourcesDelete), "clicked",
			    GTK_SIGNAL_FUNC (on_cmdSourcesDelete_clicked),
			    clistSources);
	
	gtk_signal_connect (GTK_OBJECT (cmdCamelServicesOK), "clicked",
			    GTK_SIGNAL_FUNC (on_cmdCamelServicesOK_clicked),
			    providers_config);
	gtk_signal_connect (GTK_OBJECT (cmdCamelServicesCancel), "clicked",
			    GTK_SIGNAL_FUNC (on_cmdCamelServicesCancel_clicked),
			    providers_config);
	
	gtk_signal_connect (GTK_OBJECT (clistIdentities), "select_row",
			    GTK_SIGNAL_FUNC (on_clistIdentities_select_row),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (clistSources), "select_row",
			    GTK_SIGNAL_FUNC (on_clistSources_select_row),
			    NULL);

	gtk_signal_connect (GTK_OBJECT (chkFormat), "toggled",
			    GTK_SIGNAL_FUNC (on_chkFormat_toggled),
			    NULL);

	return providers_config;
}





