/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors:
 *    Jeffrey Stedfast <fejj@ximian.com>
 *    Dan Winship <danw@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/utsname.h>
#include <string.h>
#include <unistd.h>

#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <glade/glade.h>
#include <gtkhtml/gtkhtml.h>
#include <gal/widgets/e-unicode.h>
#include "mail-config-druid.h"
#include "mail-config.h"
#include "mail-ops.h"
#include "mail.h"
#include "mail-session.h"

static void mail_config_druid_class_init (MailConfigDruidClass *class);
static void mail_config_druid_finalize   (GtkObject *obj);

static GtkWindowClass *parent_class;

GtkType
mail_config_druid_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo type_info = {
			"MailConfigDruid",
			sizeof (MailConfigDruid),
			sizeof (MailConfigDruidClass),
			(GtkClassInitFunc) mail_config_druid_class_init,
			(GtkObjectInitFunc) NULL,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		type = gtk_type_unique (gtk_window_get_type (), &type_info);
	}

	return type;
}

static void
mail_config_druid_class_init (MailConfigDruidClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;
	parent_class = gtk_type_class (gtk_window_get_type ());

	/* override methods */
	object_class->finalize = mail_config_druid_finalize;
}

static void
mail_config_druid_finalize (GtkObject *obj)
{
	MailConfigDruid *druid = (MailConfigDruid *) obj;

	mail_account_gui_destroy (druid->gui);
        ((GtkObjectClass *)(parent_class))->finalize (obj);
}


static struct {
	char *name;
	char *text;
} info[] = {
	{ "identity_html",
	  N_("Please enter your name and email address below. The \"optional\" fields below do not need to be filled in, unless you wish to include this information in email you send.") },
	{ "source_html",
	  N_("Please enter information about your incoming mail server below. If you don't know what kind of server you use, contact your system administrator or Internet Service Provider.") },
	{ "extra_html",
	  "The following options mostly don't work yet and are here only to taunt you. Ha ha!" },
	{ "transport_html",
	  N_("Please enter information about your outgoing mail protocol below. If you don't know which protocol you use, contact your system administrator or Internet Service Provider.") },
	{ "management_html",
	  N_("You are almost done with the mail configuration process. The identity, incoming mail server and outgoing mail transport method which you provided will be grouped together to make an Evolution mail account. Please enter a name for this account in the space below. This name will be used for display purposes only.") }
};
static int num_info = (sizeof (info) / sizeof (info[0]));

static void
html_size_req (GtkWidget *widget, GtkRequisition *requisition)
{
	 requisition->height = GTK_LAYOUT (widget)->height;
}

static GtkWidget *
create_html (const char *name)
{
	GtkWidget *scrolled, *html;
	GtkHTMLStream *stream;
	GtkStyle *style;
	char *utf8;
	int i;

	html = gtk_html_new ();
	GTK_LAYOUT (html)->height = 0;
	gtk_signal_connect (GTK_OBJECT (html), "size_request",
			    GTK_SIGNAL_FUNC (html_size_req), NULL);
	gtk_html_set_editable (GTK_HTML (html), FALSE);
	style = gtk_rc_get_style (html);
	if (!style)
		style = gtk_widget_get_style (html);
	if (style) {
		gtk_html_set_default_background_color (GTK_HTML (html),
						       &style->bg[0]);
	}
	gtk_widget_show (html);

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolled);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_NEVER, GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (scrolled), html);

	for (i = 0; i < num_info; i++) {
		if (!strcmp (name, info[i].name))
			break;
	}
	g_return_val_if_fail (i != num_info, scrolled);

	stream = gtk_html_begin_content (GTK_HTML (html),
					 "text/html; charset=utf-8");
	gtk_html_write (GTK_HTML (html), stream, "<html><p>", 9);
	utf8 = e_utf8_from_locale_string (_(info[i].text));
	gtk_html_write (GTK_HTML (html), stream, utf8, strlen (utf8));
	g_free (utf8);
	gtk_html_write (GTK_HTML (html), stream, "</p></html>", 11);
	gtk_html_end (GTK_HTML (html), stream, GTK_HTML_STREAM_OK);

	return scrolled;
}

static void
druid_cancel (GnomeDruid *druid, gpointer user_data)
{
	/* Cancel the setup of the account */
	MailConfigDruid *config = user_data;

	gtk_widget_destroy (GTK_WIDGET (config));
}

static void
druid_finish (GnomeDruidPage *page, gpointer arg1, gpointer user_data)
{
	/* Cancel the setup of the account */
	MailConfigDruid *druid = user_data;
	MailAccountGui *gui = druid->gui;
	GSList *mini;

	mail_account_gui_save (gui);
	if (gui->account->source)
		gui->account->source->enabled = TRUE;
	mail_config_add_account (gui->account);
	mail_config_write ();

	mini = g_slist_prepend (NULL, gui->account);
	mail_load_storages (druid->shell, mini, TRUE);
	g_slist_free (mini);

	gtk_widget_destroy (GTK_WIDGET (druid));
}

/* Identity Page */
static void
identity_changed (GtkWidget *widget, gpointer data)
{
	MailConfigDruid *druid = data;
	gboolean next_sensitive = mail_account_gui_identity_complete (druid->gui);

	gnome_druid_set_buttons_sensitive (druid->druid, TRUE, next_sensitive, TRUE);
}

static void
identity_prepare (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;

	gtk_widget_grab_focus (GTK_WIDGET (config->gui->full_name));
	identity_changed (NULL, config);
}

static gboolean
identity_next (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;

	if (!config->identity_copied) {
		char *username;

		/* Copy the username part of the email address into
		 * the Username field of the source and transport pages.
		 */
		username = gtk_entry_get_text (config->gui->email_address);
		username = g_strndup (username, strcspn (username, "@"));
		gtk_entry_set_text (config->gui->source.username, username);
		gtk_entry_set_text (config->gui->transport.username, username);
		g_free (username);

		config->identity_copied = TRUE;
	}

	return FALSE;
}

/* Incoming mail Page */
static void
source_changed (GtkWidget *widget, gpointer data)
{
	MailConfigDruid *druid = data;
	gboolean next_sensitive = mail_account_gui_source_complete (druid->gui);

	gnome_druid_set_buttons_sensitive (druid->druid, TRUE, next_sensitive, TRUE);
}

static void
source_prepare (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;

	source_changed (NULL, config);
}

static gboolean
source_next (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;
	GtkWidget *transport_page;

	/* FIXME: if online, check that the data is good. */

	if (config->gui->source.provider && config->gui->source.provider->extra_conf)
		return FALSE;

	/* Otherwise, skip to transport page. */
	transport_page = glade_xml_get_widget (config->gui->xml, "transport_page");
	gnome_druid_set_page (config->druid, GNOME_DRUID_PAGE (transport_page));

	return TRUE;
}

/* Extra Config Page */
static void
extra_prepare (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;

	if (config->gui->source.provider != config->last_source) {
		config->last_source = config->gui->source.provider;
		mail_account_gui_build_extra_conf (config->gui, NULL);
	}
}

/* Transport Page */
static void
transport_prepare (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;
	gboolean next_sensitive = mail_account_gui_transport_complete (config->gui);

	gnome_druid_set_buttons_sensitive (config->druid, TRUE, next_sensitive, TRUE);
}

static gboolean
transport_next (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	/* FIXME: if online, check that the data is good. */
	return FALSE;
}

static gboolean
transport_back (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;

	if (config->gui->source.provider && config->gui->source.provider->extra_conf)
		return FALSE;
	else {
		/* jump to the source page, skipping over the extra page */
		GtkWidget *widget;

		widget = glade_xml_get_widget (config->gui->xml, "source_page");
		gnome_druid_set_page (config->druid, GNOME_DRUID_PAGE (widget));

		return TRUE;
	}
}

static void
transport_changed (GtkWidget *widget, gpointer data)
{
	transport_prepare (NULL, NULL, data);
}

/* Management page */
static void
management_check (MailConfigDruid *druid)
{
	gboolean next_sensitive;
	char *text;

	text = gtk_entry_get_text (druid->gui->account_name);
	next_sensitive = text && *text;

	gnome_druid_set_buttons_sensitive (druid->druid, TRUE, next_sensitive, TRUE);
}

static void
management_prepare (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;
	char *name;

	name = gtk_entry_get_text (config->gui->email_address);
	if (name && *name)
		gtk_entry_set_text (config->gui->account_name, name);

	management_check (config);
}

static void
management_changed (GtkWidget *widget, gpointer data)
{
	MailConfigDruid *druid = data;

	management_check (druid);
}


static MailConfigAccount *
make_default_account (void)
{
	MailConfigAccount *account;
	char *name, *user;
	struct utsname uts;

	account = g_new0 (MailConfigAccount, 1);
	if (!mail_config_get_default_account)
		account->default_account = TRUE;

	account->id = g_new0 (MailConfigIdentity, 1);
	name = g_get_real_name ();
	account->id->name = e_utf8_from_locale_string (name);
	user = getenv ("USER");
	if (user && !uname (&uts) && strchr (uts.nodename, '.'))
		account->id->address = g_strdup_printf ("%s@%s", user, uts.nodename);

	if (mail_config_get_default_transport ())
		account->transport = service_copy (mail_config_get_default_transport ());

	return account;
}

static struct {
	char *name;
	GtkSignalFunc next_func;
	GtkSignalFunc prepare_func;
	GtkSignalFunc back_func;
	GtkSignalFunc finish_func;
} pages[] = {
	{ "identity_page",
	  GTK_SIGNAL_FUNC (identity_next),
	  GTK_SIGNAL_FUNC (identity_prepare),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL) },
	{ "source_page",
	  GTK_SIGNAL_FUNC (source_next),
	  GTK_SIGNAL_FUNC (source_prepare),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL) },
	{ "extra_page",
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (extra_prepare),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL) },
	{ "transport_page",
	  GTK_SIGNAL_FUNC (transport_next),
	  GTK_SIGNAL_FUNC (transport_prepare),
	  GTK_SIGNAL_FUNC (transport_back),
	  GTK_SIGNAL_FUNC (NULL) },
	{ "management_page",
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (management_prepare),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL) },
	{ "finish_page",
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (druid_finish) },
	{ NULL,
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL) }
};

static void
construct (MailConfigDruid *druid)
{
	GtkWidget *widget, *vbox;
	MailConfigAccount *account;
	int i;

	account = make_default_account ();
	druid->gui = mail_account_gui_new (account);

	/* get our toplevel widget and reparent it */
	widget = glade_xml_get_widget (druid->gui->xml, "druid");
	gtk_widget_reparent (widget, GTK_WIDGET (druid));

	druid->druid = GNOME_DRUID (widget);

	/* set window title */
	gtk_window_set_title (GTK_WINDOW (druid), _("Evolution Account Wizard"));
	gtk_window_set_policy (GTK_WINDOW (druid), FALSE, TRUE, FALSE);
	gtk_window_set_modal (GTK_WINDOW (druid), TRUE);
	gtk_object_set (GTK_OBJECT (druid), "type", GTK_WINDOW_DIALOG, NULL);

	/* attach to druid page signals */
	for (i = 0; pages[i].name != NULL; i++) {
		GtkWidget *page;

		page = glade_xml_get_widget (druid->gui->xml, pages[i].name);

		if (pages[i].next_func)
			gtk_signal_connect (GTK_OBJECT (page), "next",
					    pages[i].next_func, druid);
		if (pages[i].prepare_func)
			gtk_signal_connect (GTK_OBJECT (page), "prepare",
					    pages[i].prepare_func, druid);
		if (pages[i].back_func)
			gtk_signal_connect (GTK_OBJECT (page), "back",
					    pages[i].back_func, druid);
		if (pages[i].finish_func)
			gtk_signal_connect (GTK_OBJECT (page), "finish",
					    pages[i].finish_func, druid);
	}
	gtk_signal_connect (GTK_OBJECT (druid->druid), "cancel", druid_cancel, druid);

	/* Fill in the druid pages */
	vbox = glade_xml_get_widget (druid->gui->xml, "druid_identity_vbox");
	widget = create_html ("identity_html");
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
	widget = glade_xml_get_widget (druid->gui->xml, "identity_required_frame");
	gtk_widget_reparent (widget, vbox);
	gtk_box_set_child_packing (GTK_BOX (vbox), widget, FALSE, FALSE, 0, GTK_PACK_START);
	widget = glade_xml_get_widget (druid->gui->xml, "identity_optional_frame");
	gtk_widget_reparent (widget, vbox);
	gtk_box_set_child_packing (GTK_BOX (vbox), widget, FALSE, FALSE, 0, GTK_PACK_START);

	vbox = glade_xml_get_widget (druid->gui->xml, "druid_source_vbox");
	widget = create_html ("source_html");
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
	widget = glade_xml_get_widget (druid->gui->xml, "source_vbox");
	gtk_widget_reparent (widget, vbox);

	vbox = glade_xml_get_widget (druid->gui->xml, "druid_extra_vbox");
	widget = create_html ("extra_html");
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
	widget = glade_xml_get_widget (druid->gui->xml, "extra_vbox");
	gtk_widget_reparent (widget, vbox);

	vbox = glade_xml_get_widget (druid->gui->xml, "druid_transport_vbox");
	widget = create_html ("transport_html");
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
	widget = glade_xml_get_widget (druid->gui->xml, "transport_vbox");
	gtk_widget_reparent (widget, vbox);

	vbox = glade_xml_get_widget (druid->gui->xml, "druid_management_vbox");
	widget = create_html ("management_html");
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
	widget = glade_xml_get_widget (druid->gui->xml, "management_frame");
	gtk_widget_reparent (widget, vbox);

	/* set up signals, etc */
	gtk_signal_connect (GTK_OBJECT (druid->gui->account_name), "changed", management_changed, druid);
	gtk_signal_connect (GTK_OBJECT (druid->gui->full_name), "changed", identity_changed, druid);
	gtk_signal_connect (GTK_OBJECT (druid->gui->email_address), "changed", identity_changed, druid);
	gtk_signal_connect (GTK_OBJECT (druid->gui->source.hostname), "changed", source_changed, druid);
	gtk_signal_connect (GTK_OBJECT (druid->gui->source.username), "changed", source_changed, druid);
	gtk_signal_connect (GTK_OBJECT (druid->gui->source.path), "changed", source_changed, druid);
	gtk_signal_connect (GTK_OBJECT (druid->gui->transport.hostname), "changed", transport_changed, druid);
	gtk_signal_connect (GTK_OBJECT (druid->gui->transport.username), "changed", transport_changed, druid);

	mail_account_gui_setup (druid->gui, GTK_WIDGET (druid));

	gnome_druid_set_buttons_sensitive (druid->druid, FALSE, TRUE, TRUE);
}

MailConfigDruid *
mail_config_druid_new (GNOME_Evolution_Shell shell)
{
	MailConfigDruid *new;

	new = (MailConfigDruid *) gtk_type_new (mail_config_druid_get_type ());
	construct (new);
	new->shell = shell;

	return new;
}
