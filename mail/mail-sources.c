/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-sources.c: Mail source selection wizard */

/* 
 * Author : 
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

#include <sys/stat.h>

#include <gnome.h>
#include <gtkhtml/gtkhtml.h>

/* XXX */
#define default_mail_path "/var/mail"

struct {
	char *protocol, *name, *description, *authname[4], *authproto[4];
	gboolean authpasswd[4];
} providers[] = {
	{ "POP3", "Post Office Protocol, version 3",
	  "For connecting to POP3 servers. Some web mail providers and "
	  "proprietary email systems also provide POP3 interfaces.",
	  { "Password/APOP", "Kerberos 4" },
	  { NULL, "KERBEROS_V4" },
	  { TRUE, FALSE }
	},
	{ "IMAP", "Internet Mail Access Protocol",
	  "For connecting to IMAP servers. Allows you to keep all of "
	  "your mail on the IMAP server so that you can access it from "
	  "anywhere.",
	  { "Password/CRAM-MD5", "S/Key", "Kerberos 4", "GSSAPI" },
	  { NULL, "SKEY", "KERBEROS_V4", "GSSAPI" },
	  { TRUE, TRUE, FALSE, FALSE },
	}
};
#define nproviders 2

struct msinfo {
	GtkHTML *html;
	GtkWidget *prev, *next;
	int page;
	
	/* Locally-delivered mail. */
	gboolean get_local_mail, default_local_mail_path;
	char *local_mail_path;
	gboolean use_movemail;

	/* Remotely-delivered mail. */
	gboolean get_remote_mail;
	int remote_provider;
	char *remote_host, *remote_user, *remote_password;
	int remote_auth;
	gboolean remember_password;
	gboolean copy_local;

	/* Local store. */
	gboolean store_local;
	char *local_store_path;
};

static void display_intro (struct msinfo *msi);
static int finish_intro (struct msinfo *msi, int direction);
static void display_local (struct msinfo *msi);
static int finish_local (struct msinfo *msi, int direction);
static void display_remote (struct msinfo *msi);
static int finish_remote (struct msinfo *msi, int direction);
static void display_remconf (struct msinfo *msi);
static int finish_remconf (struct msinfo *msi, int direction);

static struct {
	void (*display) (struct msinfo *msi);
	int (*finish) (struct msinfo *msi, int direction);
} pages[] = {
	{ display_intro, finish_intro },
	{ display_local, finish_local },
#if 0
	{ display_movemail, finish_movemail },
#endif
	{ display_remote, finish_remote },
	{ display_remconf, finish_remconf },
	{ NULL, NULL }
};


/* Wrappers around gtkhtml */

static void
write_html (GtkHTML *html, GtkHTMLStreamHandle handle, const char *text)
{
	gtk_html_write (html, handle, text, strlen (text));
}

static GtkHTMLStreamHandle
start_html (GtkHTML *html)
{
	GtkHTMLStreamHandle handle;

	handle = gtk_html_begin (html, "");
	write_html (html, handle, "<body bgcolor=white>\n");
	return handle;
}

void
end_html (GtkHTML *html, GtkHTMLStreamHandle handle)
{
	write_html (html, handle, "</body>");
	gtk_html_end (html, handle, GTK_HTML_STREAM_OK);
}


/* Button callbacks */

static void
prev_clicked (GtkButton *button, gpointer data)
{
	struct msinfo *msi = data;

	if (msi->page == 3)
		gtk_widget_set_sensitive (msi->next, TRUE);
	msi->page = pages[msi->page].finish (data, -1);
	pages[msi->page].display (data);
	if (msi->page == 0)
		gtk_widget_set_sensitive (msi->prev, FALSE);
}

static void
next_clicked (GtkButton *button, gpointer data)
{
	struct msinfo *msi = data;

	if (msi->page == 0)
		gtk_widget_set_sensitive (msi->prev, TRUE);
	msi->page = pages[msi->page].finish (data, 1);
	pages[msi->page].display (data);
	if (msi->page == 3)
		gtk_widget_set_sensitive (msi->next, FALSE);
}

static void
cancel_clicked (GtkButton *button, gpointer data)
{
	exit (1);
}

static void
object_requested(GtkHTML *html, GtkHTMLEmbedded *eb)
{
	GtkWidget *w;

	w = gtk_object_get_data (GTK_OBJECT(html), eb->classid);
	gtk_container_add (GTK_CONTAINER(eb), w);
	gtk_widget_show_all (GTK_WIDGET(eb));
}



int
main (int argc, char **argv)
{
	struct msinfo *msi;
	GtkWidget *window, *vbox, *frame, *scrolled, *hbbox;
	GtkWidget *cancel;
	int page;

	gtk_init (&argc, &argv);
	gdk_imlib_init ();
	gdk_rgb_init ();
	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual (gdk_rgb_get_visual ());

	msi = g_new (struct msinfo, 1);

	/* Build window */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window),
			      "Mail Source Configuration");
	gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	frame = gtk_frame_new (NULL);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_ALWAYS);
	gtk_container_add (GTK_CONTAINER (frame), scrolled);

	msi->html = GTK_HTML (gtk_html_new());
	gtk_html_set_editable (msi->html, FALSE);
	gtk_container_add (GTK_CONTAINER (scrolled), GTK_WIDGET (msi->html));
	gtk_signal_connect (GTK_OBJECT (msi->html), "object_requested", 
			    GTK_SIGNAL_FUNC (object_requested), NULL);

	hbbox= gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (hbbox),
				   GTK_BUTTONBOX_END);
	gtk_box_pack_end (GTK_BOX (vbox), hbbox, FALSE, FALSE, 0);

	msi->prev = gnome_stock_button (GNOME_STOCK_BUTTON_PREV);
	msi->next = gnome_stock_button (GNOME_STOCK_BUTTON_NEXT);
	cancel = gnome_stock_button (GNOME_STOCK_BUTTON_CANCEL);

	gtk_box_pack_start (GTK_BOX (hbbox), msi->prev, TRUE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (hbbox), msi->next, TRUE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (hbbox), cancel, TRUE, FALSE, 0);

	GTK_WIDGET_SET_FLAGS (msi->prev, GTK_CAN_DEFAULT);
	GTK_WIDGET_SET_FLAGS (msi->next, GTK_CAN_DEFAULT);
	GTK_WIDGET_SET_FLAGS (cancel, GTK_CAN_DEFAULT);
	gtk_widget_grab_default (msi->next);

	gtk_signal_connect (GTK_OBJECT (msi->prev), "clicked",
			    prev_clicked, msi);
	gtk_signal_connect (GTK_OBJECT (msi->next), "clicked",
			    next_clicked, msi);
	gtk_signal_connect (GTK_OBJECT (cancel), "clicked",
			    cancel_clicked, NULL);

	msi->page = 0;
	msi->get_local_mail = msi->default_local_mail_path = -1;
	msi->use_movemail = -1;
	msi->get_remote_mail = msi->store_local = -1;
	msi->remember_password = msi->copy_local = -1;
	msi->local_mail_path = msi->local_store_path = NULL;
	msi->remote_provider = msi->remote_auth = -1;
	msi->remote_host = msi->remote_user = msi->remote_password = NULL;

	display_intro (msi);

	gtk_widget_show_all (window);
	gtk_main ();
	exit (0);
}

#define intro_text \
	"<h1>Evolution Mail Source Wizard</h1>\n" \
	"<p>Welcome to the Evolution Mail Source Wizard. This will " \
	"help you blah blah blah blah blah.</p>"

static void
display_intro (struct msinfo *msi)
{
	GtkHTMLStreamHandle handle;

	handle = start_html (msi->html);
	write_html (msi->html, handle, intro_text);
	end_html (msi->html, handle);
}

static int
finish_intro (struct msinfo *msi, int direction)
{
	return msi->page + direction;
}

#define local_text_1 \
	"<h1>Local mail source</h1>\n<hr>\n" \
	"<p>First you need to tell Evolution whether or not you " \
	"receive mail locally, and if so, where.</p>\n" \
	"<p>Your default mail file on this system is <b>"

#define local_text_2 \
	"</b>.</p>\n"

#define local_text_3_file \
	"<p>That file exists, so you almost certainly want to use it " \
	"as a mail source.</p>\n"

#define local_text_3_dir \
	"<p>That directory exists, but you currently have no mail " \
	"there. If you aren't sure whether or not you receive mail " \
	"on this machine, it's safest to leave it selected.</p>\n"

#define local_text_3_none \
	"<p>However, that directory does not exist.</p>\n"

#define local_text_label_1 \
	"Don't fetch local mail."

#define local_text_label_2 \
	"Fetch local mail from the default location."

#define local_text_label_3 \
	"Fetch local mail from an alternate location:"

void
display_local (struct msinfo *msi)
{
	GtkHTMLStreamHandle handle;
	struct stat st;
	char *default_user_mail_path;
	GtkWidget *radio, *text;
	GSList *group = NULL;

	default_user_mail_path = g_strdup_printf ("%s/%s", default_mail_path,
						  getenv ("USER"));

	handle = start_html (msi->html);
	write_html (msi->html, handle, local_text_1);
	write_html (msi->html, handle, default_user_mail_path);
	write_html (msi->html, handle, local_text_2);

	if (stat (default_mail_path, &st) == 0) {
		if (stat (default_user_mail_path, &st) == 0)
			write_html (msi->html, handle, local_text_3_file);
		else
			write_html (msi->html, handle, local_text_3_dir);
		if (msi->get_local_mail == -1)
			msi->get_local_mail = TRUE;
	} else {
		write_html (msi->html, handle, local_text_3_none);
		if (msi->get_local_mail == -1)
			msi->get_local_mail = FALSE;
	}
	g_free (default_user_mail_path);

	radio = gtk_radio_button_new_with_label (group, local_text_label_1);
	group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio));
	if (!msi->get_local_mail)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
	gtk_object_set_data (GTK_OBJECT (msi->html), "local:no", radio);
	write_html (msi->html, handle,
		    "<object classid=\"local:no\"></object><br>\n");

	radio = gtk_radio_button_new_with_label (group, local_text_label_2);
	group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio));
	if (msi->get_local_mail && msi->default_local_mail_path)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
	gtk_object_set_data (GTK_OBJECT (msi->html), "local:default", radio);
	write_html (msi->html, handle,
		    "<object classid=\"local:default\"></object><br>\n");

	radio = gtk_radio_button_new_with_label (group, local_text_label_3);
	text = gtk_entry_new ();
	if (msi->get_local_mail && !msi->default_local_mail_path) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
		gtk_entry_set_text (GTK_ENTRY (text), msi->local_mail_path);
	}
	gtk_object_set_data (GTK_OBJECT (msi->html), "local:alt", radio);
	gtk_object_set_data (GTK_OBJECT (msi->html), "local:text", text);
	write_html (msi->html, handle,
		    "<object classid=\"local:alt\"></object> "
		    "<object classid=\"local:text\"></object>");

	end_html (msi->html, handle);
}

static int
finish_local (struct msinfo *msi, int direction)
{
	GtkWidget *radio, *text;

	radio = gtk_object_get_data (GTK_OBJECT (msi->html), "local:no");
	msi->get_local_mail =
		!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio));

	g_free (msi->local_mail_path);
	if (!msi->get_local_mail)
		msi->local_mail_path = NULL;
	else {
		radio = gtk_object_get_data (GTK_OBJECT (msi->html),
					     "local:default");
		msi->default_local_mail_path =
			gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio));
		if (msi->default_local_mail_path)
			msi->local_mail_path = NULL;
		else {
			text = gtk_object_get_data (GTK_OBJECT (msi->html),
						    "local:text");
			msi->local_mail_path =
				g_strdup (gtk_entry_get_text (GTK_ENTRY (text)));
		}
	}

	return msi->page + direction;
}

#define remote_text_1 \
	"<h1>Remote mail source</h1>\n<hr>\n<p>Now you need to " \
	"configure a remote mail source, if you have one.</p>\n" \
	"<p>Evolution supports the following protocols for reading " \
	"mail from remote servers:</p>"

#define remote_text_2 \
	"<p>To add a remote mail source, choose a protocol from " \
	"the list below and click \"Next\".</p>"

#define remote_text_3_must \
	"<p>You have not configured a local mail source, so you " \
	"must configure a remote one.</p>"

#define remote_label_none \
	"No remote mail source"

void
display_remote (struct msinfo *msi)
{
	GtkHTMLStreamHandle handle;
	char *table, *item, *button, *nolabel;
	GtkWidget *widget;
	int i;
	GSList *group = NULL;

	handle = start_html (msi->html);
	write_html (msi->html, handle, remote_text_1);

	/* Write the table of available providers */
	table = "<blockquote><table border=1>\n";
	write_html (msi->html, handle, table);
	for (i = 0; i < nproviders; i++) {
		table = g_strdup_printf ("<tr><th width=\"15%%\" "
					 "rowspan=2 valign=top>%s</th>"
					 "<td>%s</td></tr>\n"
					 "<tr><td>%s</td>\n",
					 providers[i].protocol,
					 providers[i].name,
					 providers[i].description);
		write_html (msi->html, handle, table);
		g_free (table);
	}
	table = "</table></blockquote>\n";
	write_html (msi->html, handle, table);

	write_html (msi->html, handle, remote_text_2);
	if (!msi->get_local_mail)
		write_html (msi->html, handle, remote_text_3_must);

	/* Write the list of configurable sources */
	write_html (msi->html, handle, "<blockquote>");
	if (msi->get_local_mail) {
		widget = gtk_radio_button_new_with_label (NULL,
							  remote_label_none);
		group = gtk_radio_button_group (GTK_RADIO_BUTTON (widget));
		gtk_object_set_data (GTK_OBJECT (msi->html), "remote:no",
				     widget);
		write_html (msi->html, handle,
			    "\n<object classid=\"remote:no\"></object><br>");
	}

	for (i = 0; i < nproviders; i++) {
		button = g_strdup_printf ("remote:%s", providers[i].protocol);
		widget = gtk_radio_button_new_with_label (group, providers[i].protocol);
		if (msi->remote_provider == i) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
						      TRUE);
		}
		group = gtk_radio_button_group (GTK_RADIO_BUTTON (widget));
		gtk_object_set_data (GTK_OBJECT (msi->html), button, widget);
		g_free (button);

		button = g_strdup_printf ("\n<object classid=\"remote:%s\">"
					  "</object><br>",
					  providers[i].protocol);
		write_html (msi->html, handle, button);
		g_free (button);
	}
	write_html (msi->html, handle, "</blockquote>");

	end_html (msi->html, handle);
}

static int
finish_remote (struct msinfo *msi, int direction)
{
	GtkToggleButton *radio;
	char *button;
	int i;

	radio = gtk_object_get_data (GTK_OBJECT (msi->html), "remote:no");
	msi->get_remote_mail = !radio || !gtk_toggle_button_get_active (radio);
	if (msi->get_remote_mail) {
		for (i = 0; i < nproviders; i++) {
			button = g_strdup_printf ("remote:%s",
						  providers[i].protocol);
			radio = gtk_object_get_data (GTK_OBJECT (msi->html),
						     button);
			if (gtk_toggle_button_get_active (radio))
				break;
		}

		msi->remote_provider = i;
	} else if (direction == 1)
		direction = 2; /* Skip remconf page. */

	return msi->page + direction;
}

#define remconf_text_title \
	"<h1>Configure a remote mail source: %s</h1><hr>"

#define remconf_text_host_label "Server name:"
#define remconf_text_user_label "Account name:"
#define remconf_text_path_label "Path to mail on server:"
#define remconf_text_auth_label "Authentication method:"

#define remconf_text_password \
	"<p>If you would like to have Evolution remember the password " \
	"for this account, enter it below. If you would rather be " \
	"prompted for the password when Evolution needs it, choose " \
	"one of the other options.</p>\n"

#define remconf_text_password_remember "Remember my password"
#define remconf_text_password_confirm  "Enter password again for confirmation"
#define remconf_text_password_once \
	"Prompt me for the password once each Evolution session."
#define remconf_text_password_forget \
	"Prompt me for the password every time it is needed."

static void
resize_password (GtkWidget *html, GtkAllocation *alloc, gpointer data)
{
	GtkWidget *scrolled;

	scrolled = gtk_object_get_data (GTK_OBJECT (html), "remconf:htmlwin");
	gtk_widget_set_usize (scrolled, alloc->width - 20, 300);
}

static void
frob_password (GtkMenuItem *menuitem, gpointer data)
{
	struct msinfo *msi = data;
	GtkHTML *subhtml;
	GtkHTMLStreamHandle handle;
	GtkWidget *radio, *table, *text, *label;
	GSList *group = NULL;
	int id;

	id = GPOINTER_TO_UINT (gtk_object_get_data (GTK_OBJECT (menuitem),
						    "id"));
	gtk_object_set_data (GTK_OBJECT (msi->html), "remconf:authproto",
			     GUINT_TO_POINTER (id));
	subhtml = gtk_object_get_data (GTK_OBJECT (msi->html), "remconf:html");
	handle = start_html (subhtml);
	if (providers[msi->remote_provider].authpasswd[id]) {
		write_html (subhtml, handle, remconf_text_password);

		table = gtk_table_new (2, 2, FALSE);
		radio = gtk_radio_button_new_with_label (NULL, remconf_text_password_remember);
		group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio));
		gtk_object_set_data (GTK_OBJECT (subhtml), "sub:remember",
				     radio);
		gtk_table_attach (GTK_TABLE (table), radio, 0, 1, 0, 1,
				  GTK_FILL, GTK_SHRINK, 0, 0);
		text = gtk_entry_new ();
		gtk_entry_set_visibility (GTK_ENTRY (text), FALSE);
		if (msi->remote_password)
			gtk_entry_set_text (GTK_ENTRY (text), msi->remote_password);
		gtk_object_set_data (GTK_OBJECT (subhtml), "sub:passwd1",
				     text);
		gtk_table_attach (GTK_TABLE (table), text, 1, 2, 0, 1,
				  GTK_EXPAND, GTK_SHRINK, 0, 0);
		label = gtk_label_new (remconf_text_password_confirm);
		gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);
		gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
				  GTK_SHRINK, GTK_SHRINK, 5, 0);
		text = gtk_entry_new ();
		gtk_entry_set_visibility (GTK_ENTRY (text), FALSE);
		if (msi->remote_password)
			gtk_entry_set_text (GTK_ENTRY (text), msi->remote_password);
		gtk_object_set_data (GTK_OBJECT (subhtml), "sub:passwd2",
				     text);
		gtk_table_attach (GTK_TABLE (table), text, 1, 2, 1, 2,
				  GTK_EXPAND, GTK_SHRINK, 0, 0);

		gtk_object_set_data (GTK_OBJECT (subhtml), "sub:table", table);

		write_html (subhtml, handle, "<object classid=\"sub:table\">"
			    "</object>\n");

		radio = gtk_radio_button_new_with_label (group, remconf_text_password_once);
		group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio));
		gtk_object_set_data (GTK_OBJECT (subhtml), "sub:once",
				     radio);
		write_html (subhtml, handle, "<object classid=\"sub:once\">"
			    "</object>\n");

		radio = gtk_radio_button_new_with_label (group, remconf_text_password_forget);
		group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio));
		gtk_object_set_data (GTK_OBJECT (subhtml), "sub:forget",
				     radio);
		write_html (subhtml, handle, "<object classid=\"sub:forget\">"
			    "</object>\n");
	} else {
		gtk_object_set_data (GTK_OBJECT (subhtml), "sub:remember", NULL);
		gtk_object_set_data (GTK_OBJECT (subhtml), "sub:once", NULL);
		gtk_object_set_data (GTK_OBJECT (subhtml), "sub:forget", NULL);
	}
	end_html (subhtml, handle);
}

void
display_remconf (struct msinfo *msi)
{
	GtkHTMLStreamHandle handle;
	char *text;
	int prov = msi->remote_provider;
	GtkWidget *widget, *menu, *menuitem, *mi1 = NULL;
	GtkWidget *scrolled, *subhtml;

	handle = start_html (msi->html);

	text = g_strdup_printf (remconf_text_title, providers[prov].protocol);
	write_html (msi->html, handle, text);
	g_free (text);

	write_html (msi->html, handle, "<table>\n");

	if (1) {
		write_html (msi->html, handle, "<tr><td>");
		write_html (msi->html, handle, remconf_text_host_label);
		widget = gtk_entry_new ();
		if (msi->remote_host) {
			gtk_entry_set_text (GTK_ENTRY (widget),
					    msi->remote_host);
		}
		gtk_object_set_data (GTK_OBJECT (msi->html), "remconf:host",
				     widget);
		write_html (msi->html, handle, "</td><td><object "
			    "classid=\"remconf:host\"></object></td></tr>");
	}

	if (1) {
		write_html (msi->html, handle, "<tr><td>");
		write_html (msi->html, handle, remconf_text_user_label);
		widget = gtk_entry_new ();
		if (msi->remote_user) {
			gtk_entry_set_text (GTK_ENTRY (widget),
					    msi->remote_user);
		}
		gtk_object_set_data (GTK_OBJECT (msi->html), "remconf:user",
				     widget);
		write_html (msi->html, handle, "</td><td><object "
			    "classid=\"remconf:user\"></object></td></tr>");
	}

	if (0) {
		write_html (msi->html, handle, "<tr><td>");
		write_html (msi->html, handle, remconf_text_path_label);
		widget = gtk_entry_new ();
#if 0
		if (msi->remote_path) {
			gtk_entry_set_text (GTK_ENTRY (widget),
					    msi->remote_path);
		}
#endif
		gtk_object_set_data (GTK_OBJECT (msi->html), "remconf:path",
				     widget);
		write_html (msi->html, handle, "</td><td><object "
			    "classid=\"remconf:path\"></object></td></tr>");
	}

	if (1) {
		int i;

		write_html (msi->html, handle, "<tr><td>");
		write_html (msi->html, handle, remconf_text_auth_label);
		menu = gtk_menu_new ();
		for (i = 0; i < 4 && providers[prov].authname[i]; i++) {
			menuitem = gtk_menu_item_new_with_label (providers[prov].authname[i]);
			gtk_widget_show (menuitem);
			gtk_signal_connect (GTK_OBJECT (menuitem), "activate", 
					    GTK_SIGNAL_FUNC (frob_password),
					    msi);
			gtk_object_set_data (GTK_OBJECT (menuitem), "id",
					     GUINT_TO_POINTER (i));
			if (!mi1)
				mi1 = menuitem;
			gtk_menu_append (GTK_MENU (menu), menuitem);
		}
		widget = gtk_option_menu_new ();
		gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);
		gtk_option_menu_set_history (GTK_OPTION_MENU (widget),
					     msi->remote_auth ?
					     msi->remote_auth : 0);
		gtk_object_set_data (GTK_OBJECT (msi->html), "remconf:auth",
				     widget);
		write_html (msi->html, handle, "</td><td><object "
			    "classid=\"remconf:auth\"></object></td></tr>");
	}
	write_html (msi->html, handle, "</table>\n");

	subhtml = gtk_html_new ();
	gtk_html_set_editable (GTK_HTML (subhtml), FALSE);
	gtk_signal_connect (GTK_OBJECT (subhtml), "object_requested", 
			    GTK_SIGNAL_FUNC (object_requested), NULL);
	gtk_object_set_data (GTK_OBJECT (msi->html), "remconf:html",
			     subhtml);
	frob_password (GTK_MENU_ITEM (mi1), msi);
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_NEVER, GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (scrolled), subhtml);
	gtk_object_set_data (GTK_OBJECT (msi->html), "remconf:htmlwin",
			     scrolled);
	write_html (msi->html, handle, "<object classid=\"remconf:htmlwin\">"
		    "</object>\n");
	write_html (msi->html, handle, "<p>foo</p>");

	gtk_signal_connect (GTK_OBJECT (msi->html), "size-allocate",
			    GTK_SIGNAL_FUNC (resize_password), NULL);

	end_html (msi->html, handle);
}

static int
finish_remconf (struct msinfo *msi, int direction)
{
	GtkEntry *host, *user, *passwd1, *passwd2;
	char *data;
	GtkWidget *menu, *menuitem;
	GtkObject *subhtml;
	GtkToggleButton *radio;

	gtk_signal_disconnect_by_func (GTK_OBJECT (msi->html),
				       GTK_SIGNAL_FUNC (resize_password),
				       NULL);

	host = gtk_object_get_data (GTK_OBJECT (msi->html), "remconf:host");
	data = gtk_entry_get_text (GTK_ENTRY (host));
	if (data && *data)
		msi->remote_host = g_strdup (data);

	user = gtk_object_get_data (GTK_OBJECT (msi->html), "remconf:user");
	data = gtk_entry_get_text (GTK_ENTRY (user));
	if (data && *data)
		msi->remote_user = g_strdup (data);

	msi->remote_auth = GPOINTER_TO_UINT (gtk_object_get_data (GTK_OBJECT (msi->html), "remconf:authproto"));

	subhtml = gtk_object_get_data (GTK_OBJECT (msi->html), "remconf:html");
	radio = gtk_object_get_data (subhtml, "sub:remember");
	if (radio && gtk_toggle_button_get_active (radio)) {
		passwd1 = gtk_object_get_data (subhtml, "sub:passwd1");
		passwd2 = gtk_object_get_data (subhtml, "sub:passwd2");

		/* XXX compare */
		data = gtk_entry_get_text (GTK_ENTRY (passwd1));
		printf ("%s\n", data);
		if (data && *data) {
			msi->remote_password = g_strdup (data);
			msi->remember_password = TRUE;
		}
	} else {
		radio = gtk_object_get_data (subhtml, "sub:once");
		msi->remember_password = gtk_toggle_button_get_active (radio);
	}

	return msi->page + direction;
}
