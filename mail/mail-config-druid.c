/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors:
 *    Jeffrey Stedfast <fejj@ximian.com>
 *    Dan Winship <danw@ximian.com>
 *    Iain Holmes <iain@ximian.com>
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/utsname.h>
#include <string.h>
#include <unistd.h>

#include <gal/util/e-util.h>
#include <glade/glade.h>
#include <gtkhtml/gtkhtml.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-druid.h>
#include <libgnomeui/gnome-druid-page-standard.h>
#include <bonobo/bonobo-exception.h>
#include <libgnome/gnome-i18n.h>

#include "mail-config-druid.h"
#include "mail-config.h"
#include "mail-ops.h"
#include "mail-session.h"
#include "mail-account-gui.h"

#include <evolution-wizard.h>
#include <e-util/e-account.h>
#include <e-util/e-icon-factory.h>

typedef enum {
	MAIL_CONFIG_WIZARD_PAGE_NONE = -1,
	MAIL_CONFIG_WIZARD_PAGE_IDENTITY,
	MAIL_CONFIG_WIZARD_PAGE_SOURCE,
	MAIL_CONFIG_WIZARD_PAGE_EXTRA,
	MAIL_CONFIG_WIZARD_PAGE_TRANSPORT,
	MAIL_CONFIG_WIZARD_PAGE_MANAGEMENT,

	MAIL_CONFIG_WIZARD_NUM_PAGES
} MailConfigWizardPage;

typedef struct {
	/* Only one of these will be set */
	GnomeDruid *druid;
	EvolutionWizard *corba_wizard;

	MailAccountGui *gui;

	GPtrArray *interior_pages;
	GnomeDruidPage *last_page;

	gboolean identity_copied;
	CamelProvider *last_source;
	MailConfigWizardPage page;
} MailConfigWizard;

static void
config_wizard_set_buttons_sensitive (MailConfigWizard *mcw,
				     gboolean prev_sensitive,
				     gboolean next_sensitive)
{
	if (mcw->corba_wizard) {
		evolution_wizard_set_buttons_sensitive (mcw->corba_wizard,
							prev_sensitive,
							next_sensitive,
							TRUE, NULL);
	} else {
		gnome_druid_set_buttons_sensitive (mcw->druid,
						   prev_sensitive,
						   next_sensitive,
						   TRUE, FALSE);
	}
}

static void
config_wizard_set_page (MailConfigWizard *mcw, MailConfigWizardPage page)
{
	if (mcw->corba_wizard)
		evolution_wizard_set_page (mcw->corba_wizard, page, NULL);
	else {
		if (page < mcw->interior_pages->len)
			gnome_druid_set_page (mcw->druid, mcw->interior_pages->pdata[page]);
		else
			gnome_druid_set_page (mcw->druid, mcw->last_page);
	}
}

/* Identity Page */
static void
identity_changed (GtkWidget *widget, gpointer data)
{
	MailConfigWizard *mcw = data;
	GtkWidget *incomplete;
	gboolean next_sensitive;
	
	if (mcw->page != MAIL_CONFIG_WIZARD_PAGE_IDENTITY)
		return;
	
	next_sensitive = mail_account_gui_identity_complete (mcw->gui, &incomplete);
	
	config_wizard_set_buttons_sensitive (mcw, TRUE, next_sensitive);
}

static void
identity_prepare (MailConfigWizard *mcw)
{
	mcw->page = MAIL_CONFIG_WIZARD_PAGE_IDENTITY;
	
	if (!gtk_entry_get_text (mcw->gui->full_name)) {
		char *uname;
		
		uname = g_locale_to_utf8 (g_get_real_name (), -1, NULL, NULL, NULL);
		gtk_entry_set_text (mcw->gui->full_name, uname ? uname : "");
		g_free (uname);
	}
	identity_changed (NULL, mcw);
}

static gboolean
identity_next (MailConfigWizard *mcw)
{
	if (!mcw->identity_copied) {
		char *username;
		const char *user;

		/* Copy the username part of the email address into
		 * the Username field of the source and transport pages.
		 */
		user = gtk_entry_get_text (mcw->gui->email_address);
		username = g_strndup (user, strcspn (user, "@"));
		gtk_entry_set_text (mcw->gui->source.username, username);
		gtk_entry_set_text (mcw->gui->transport.username, username);
		g_free (username);
		
		mcw->identity_copied = TRUE;
	}
	
	return FALSE;
}

static void
identity_activate_cb (GtkEntry *ent, gpointer user_data)
{
	MailConfigWizard *mcw = user_data;

	if (mail_account_gui_identity_complete (mcw->gui, NULL) &&
	    !identity_next (mcw))
		config_wizard_set_page (mcw, MAIL_CONFIG_WIZARD_PAGE_SOURCE);
}

/* Incoming mail Page */
static void
source_changed (GtkWidget *widget, gpointer data)
{
	MailConfigWizard *mcw = data;
	GtkWidget *incomplete;
	gboolean next_sensitive;
	
	if (mcw->page != MAIL_CONFIG_WIZARD_PAGE_SOURCE)
		return;
	
	next_sensitive = mail_account_gui_source_complete (mcw->gui, &incomplete);
	
	config_wizard_set_buttons_sensitive (mcw, TRUE, next_sensitive);
}

static void
source_prepare (MailConfigWizard *mcw)
{
	mcw->page = MAIL_CONFIG_WIZARD_PAGE_SOURCE;
	source_changed (NULL, mcw);
}

static gboolean
source_next (MailConfigWizard *mcw)
{
	/* FIXME: if online, check that the data is good. */
	
	if (mcw->gui->source.provider && mcw->gui->source.provider->extra_conf)
		return FALSE;
	
	/* Otherwise, skip to transport page. */
	config_wizard_set_page (mcw, MAIL_CONFIG_WIZARD_PAGE_TRANSPORT);
	return TRUE;
}

static void
source_activate_cb (GtkEntry *ent, gpointer user_data)
{
	MailConfigWizard *mcw = user_data;

	if (mail_account_gui_source_complete (mcw->gui, NULL) &&
	    !source_next (mcw))
		config_wizard_set_page (mcw, MAIL_CONFIG_WIZARD_PAGE_EXTRA);
}

/* Extra Config Page */
static void
extra_prepare (MailConfigWizard *mcw)
{
	mcw->page = MAIL_CONFIG_WIZARD_PAGE_EXTRA;
	if (mcw->gui->source.provider != mcw->last_source) {
		mcw->last_source = mcw->gui->source.provider;
		mail_account_gui_auto_detect_extra_conf (mcw->gui);
	}
}

/* Transport Page */
static gboolean
transport_next (MailConfigWizard *mcw)
{
	/* FIXME: if online, check that the data is good. */
	return FALSE;
}

static gboolean
transport_back (MailConfigWizard *mcw)
{
	if (mcw->gui->source.provider && mcw->gui->source.provider->extra_conf)
		return FALSE;
	else {
		config_wizard_set_page (mcw, MAIL_CONFIG_WIZARD_PAGE_SOURCE);
		return TRUE;
	}
}

static void
transport_changed (GtkWidget *widget, gpointer data)
{
	MailConfigWizard *mcw = data;
	GtkWidget *incomplete;
	gboolean next_sensitive;
	
	if (mcw->page != MAIL_CONFIG_WIZARD_PAGE_TRANSPORT)
		return;
	
	next_sensitive = mail_account_gui_transport_complete (mcw->gui, &incomplete);
	
	config_wizard_set_buttons_sensitive (mcw, TRUE, next_sensitive);
}

static void
transport_prepare (MailConfigWizard *mcw)
{
	mcw->page = MAIL_CONFIG_WIZARD_PAGE_TRANSPORT;
	transport_changed (NULL, mcw);
}

static void
transport_activate_cb (GtkEntry *ent, gpointer user_data)
{
	MailConfigWizard *mcw = user_data;

	if (mail_account_gui_transport_complete (mcw->gui, NULL) &&
	    !transport_next (mcw))
		config_wizard_set_page (mcw, MAIL_CONFIG_WIZARD_PAGE_MANAGEMENT);
}

/* Management page */
static gboolean
management_check (MailConfigWizard *mcw)
{
	gboolean next_sensitive;
	const char *text;
	
	text = gtk_entry_get_text (mcw->gui->account_name);
	next_sensitive = text && *text;
	
	/* no accounts with the same name */
	if (next_sensitive && mail_config_get_account_by_name (text))
		next_sensitive = FALSE;
	
	config_wizard_set_buttons_sensitive (mcw, TRUE, next_sensitive);
	return next_sensitive;
}

static void
management_prepare (MailConfigWizard *mcw)
{
	const char *name, *text;
	
	mcw->page = MAIL_CONFIG_WIZARD_PAGE_MANAGEMENT;
	
	text = gtk_entry_get_text (mcw->gui->account_name);
	if (!text || *text == '\0') {
		name = gtk_entry_get_text(mcw->gui->email_address);
		if (name && *name) {
			if (mail_config_get_account_by_name (name)) {
				char *template;
				unsigned int i = 1, len;
				
				/* length of name + 1 char for ' ' + 1 char
				   for '(' + 10 chars for %d + 1 char for ')'
				   + 1 char for nul */
				len = strlen (name);
				template = alloca (len + 14);
				strcpy (template, name);
				name = template;
				do {
					sprintf (template + len, " (%d)", i++);
				} while (mail_config_get_account_by_name (name) && i != 0);
			}
			
			gtk_entry_set_text(mcw->gui->account_name, name);
		}
	}
	
	management_check (mcw);
}

static void
management_changed (GtkWidget *widget, gpointer data)
{
	MailConfigWizard *mcw = data;
	
	if (mcw->page != MAIL_CONFIG_WIZARD_PAGE_MANAGEMENT)
		return;
	
	management_check (mcw);
}

static void
management_activate_cb (GtkEntry *ent, gpointer user_data)
{
	MailConfigWizard *mcw = user_data;

	if (management_check (mcw))
		config_wizard_set_page (mcw, mcw->page + 1);
}

static struct {
	const char *page_name, *title, *icon_name;
	void (*prepare_func) (MailConfigWizard *mcw);
	gboolean (*back_func) (MailConfigWizard *mcw);
	gboolean (*next_func) (MailConfigWizard *mcw);
	const char *help_text;
} wizard_pages[] = {
	{ "identity_page", N_("Identity"), "stock_contact",
	  identity_prepare, NULL, identity_next,
	  N_("Please enter your name and email address below. "
	     "The \"optional\" fields below do not need to be "
	     "filled in, unless you wish to include this "
	     "information in email you send.")
	},

	{ "source_page", N_("Receiving Mail"), "stock_mail-receive",
	  source_prepare, NULL, source_next,
	  N_("Please enter information about your incoming "
	     "mail server below. If you are not sure, ask your "
	     "system administrator or Internet Service Provider.")
	},

	{ "extra_page", N_("Receiving Mail"), "stock_mail-receive",
	  extra_prepare, NULL, NULL,
	  N_("Please select among the following options")
	},

	{ "transport_page", N_("Sending Mail"), "stock_mail-send",
	  transport_prepare, transport_back, transport_next,
	  N_("Please enter information about the way you will "
	     "send mail. If you are not sure, ask your system "
	     "administrator or Internet Service Provider.")
	},

	{ "management_page", N_("Account Management"), "stock_person",
	  management_prepare, NULL, NULL,
	  N_("You are almost done with the mail configuration "
	     "process. The identity, incoming mail server and "
	     "outgoing mail transport method which you provided "
	     "will be grouped together to make an Evolution mail "
	     "account. Please enter a name for this account in "
	     "the space below. This name will be used for display "
	     "purposes only.")
	}
};
static const int num_wizard_pages = sizeof (wizard_pages) / sizeof (wizard_pages[0]);

static GtkWidget *
get_page (GladeXML *xml, int page_num)
{
	GtkWidget *vbox, *widget;

	vbox = gtk_vbox_new (FALSE, 4);

	widget = gtk_label_new (_(wizard_pages[page_num].help_text));
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_FILL);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
	gtk_widget_show_all (vbox);
	
	switch (page_num) {
	case MAIL_CONFIG_WIZARD_PAGE_IDENTITY:
		widget = glade_xml_get_widget (xml, "identity_required_frame");
		gtk_container_set_border_width (GTK_CONTAINER (widget), 0);
		gtk_widget_reparent (widget, vbox);
		gtk_box_set_child_packing (GTK_BOX (vbox), widget, FALSE, FALSE, 0, GTK_PACK_START);
		widget = glade_xml_get_widget (xml, "identity_optional_frame");
		gtk_container_set_border_width (GTK_CONTAINER (widget), 0);
		gtk_widget_reparent (widget, vbox);
		gtk_box_set_child_packing (GTK_BOX (vbox), widget, FALSE, FALSE, 0, GTK_PACK_START);
		break;

	case MAIL_CONFIG_WIZARD_PAGE_SOURCE:
		widget = glade_xml_get_widget (xml, "source_vbox");
		gtk_container_set_border_width (GTK_CONTAINER (widget), 0);
		gtk_widget_reparent (widget, vbox);
		break;

	case MAIL_CONFIG_WIZARD_PAGE_EXTRA:
		widget = glade_xml_get_widget (xml, "extra_table");
		gtk_container_set_border_width (GTK_CONTAINER (widget), 0);
		gtk_widget_reparent (widget, vbox);
		break;

	case MAIL_CONFIG_WIZARD_PAGE_TRANSPORT:
		widget = glade_xml_get_widget (xml, "transport_vbox");
		gtk_container_set_border_width (GTK_CONTAINER (widget), 0);
		gtk_widget_reparent (widget, vbox);
		break;

	case MAIL_CONFIG_WIZARD_PAGE_MANAGEMENT:
		widget = glade_xml_get_widget (xml, "management_frame");
		gtk_container_set_border_width (GTK_CONTAINER (widget), 0);
		gtk_widget_reparent (widget, vbox);
		break;

	default:
		g_return_val_if_reached (NULL);
	}
	
	return vbox;
}


static MailConfigWizard *
config_wizard_new (void)
{
	MailConfigWizard *mcw;
	const char *user;
	EAccountService *xport;
	struct utsname uts;
	EAccount *account;
	
	/* Create a new account object with some defaults */
	account = e_account_new ();
	account->enabled = TRUE;
	
	account->id->name = g_locale_to_utf8 (g_get_real_name (), -1, NULL, NULL, NULL);
	user = g_get_user_name ();
	if (user && !uname (&uts) && strchr (uts.nodename, '.'))
		account->id->address = g_strdup_printf ("%s@%s", user, uts.nodename);
	
	if ((xport = mail_config_get_default_transport ())) {
		account->transport->url = g_strdup (xport->url);
		account->transport->save_passwd = xport->save_passwd;
	}
	
	/* Create the config wizard object */
	mcw = g_new0 (MailConfigWizard, 1);
	mcw->gui = mail_account_gui_new (account, NULL);
	g_object_unref (account);
	
	/* Set up gui */
	g_signal_connect (mcw->gui->account_name, "changed",
			  G_CALLBACK (management_changed), mcw);
	g_signal_connect (mcw->gui->full_name, "changed",
			  G_CALLBACK (identity_changed), mcw);
	g_signal_connect (mcw->gui->email_address, "changed",
			  G_CALLBACK (identity_changed), mcw);
	g_signal_connect (mcw->gui->reply_to,"changed",
			  G_CALLBACK (identity_changed), mcw);
	g_signal_connect (mcw->gui->source.hostname, "changed",
			  G_CALLBACK (source_changed), mcw);
	g_signal_connect (mcw->gui->source.username, "changed",
			  G_CALLBACK (source_changed), mcw);
	g_signal_connect (mcw->gui->source.path, "changed",
			  G_CALLBACK (source_changed), mcw);
	g_signal_connect (mcw->gui->transport.hostname, "changed",
			  G_CALLBACK (transport_changed), mcw);
	g_signal_connect (mcw->gui->transport.username, "changed",
			  G_CALLBACK (transport_changed), mcw);
	g_signal_connect (mcw->gui->transport_needs_auth, "toggled",
			  G_CALLBACK (transport_changed), mcw);
	
	g_signal_connect (mcw->gui->account_name, "activate",
			  G_CALLBACK (management_activate_cb), mcw);
	
	g_signal_connect (mcw->gui->full_name, "activate",
			  G_CALLBACK (identity_activate_cb), mcw);
	g_signal_connect (mcw->gui->email_address, "activate",
			  G_CALLBACK (identity_activate_cb), mcw);
	g_signal_connect (mcw->gui->reply_to,"activate",
			  G_CALLBACK (identity_activate_cb), mcw);
	g_signal_connect (mcw->gui->organization, "activate",
			  G_CALLBACK (identity_activate_cb), mcw);
	
	g_signal_connect (mcw->gui->source.hostname, "activate",
			  G_CALLBACK (source_activate_cb), mcw);
	g_signal_connect (mcw->gui->source.username, "activate",
			  G_CALLBACK (source_activate_cb), mcw);
	g_signal_connect (mcw->gui->source.path, "activate",
			  G_CALLBACK (source_activate_cb), mcw);
	
	g_signal_connect (mcw->gui->transport.hostname, "activate",
			  G_CALLBACK (transport_activate_cb), mcw);
	g_signal_connect (mcw->gui->transport.username, "activate",
			  G_CALLBACK (transport_activate_cb), mcw);

	return mcw;
}

static void
free_config_wizard (MailConfigWizard *mcw)
{
	mail_account_gui_destroy (mcw->gui);

	if (mcw->interior_pages)
		g_ptr_array_free (mcw->interior_pages, TRUE);

	g_free (mcw);
}

/* In-proc config druid */

static void
druid_cancel (GnomeDruid *druid, gpointer user_data)
{
	MailConfigWizard *mcw = user_data;
	GtkWidget *window;

	window = glade_xml_get_widget (mcw->gui->xml, "account_druid");
	gtk_widget_destroy (window);

	free_config_wizard (mcw);
}

static void
druid_finish (GnomeDruidPage *page, GnomeDruid *druid, gpointer user_data)
{
	MailConfigWizard *mcw = user_data;

	mail_account_gui_save (mcw->gui);
	druid_cancel (druid, user_data);
}

static void
druid_prepare (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigWizard *mcw = g_object_get_data (G_OBJECT (druid), "MailConfigWizard");
	int page_num = GPOINTER_TO_INT (data);

	if (wizard_pages[page_num].prepare_func)
		wizard_pages[page_num].prepare_func (mcw);
}
	
static gboolean
druid_back (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigWizard *mcw = g_object_get_data (G_OBJECT (druid), "MailConfigWizard");
	int page_num = GPOINTER_TO_INT (data);

	if (wizard_pages[page_num].back_func)
		return wizard_pages[page_num].back_func (mcw);
	else
		return FALSE;
}
	
static gboolean
druid_next (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigWizard *mcw = g_object_get_data (G_OBJECT (druid), "MailConfigWizard");
	int page_num = GPOINTER_TO_INT (data);

	if (wizard_pages[page_num].next_func)
		return wizard_pages[page_num].next_func (mcw);
	else
		return FALSE;
}


MailConfigDruid *
mail_config_druid_new (void)
{
	MailConfigWizard *mcw;
	GtkWidget *new, *page;
	GdkPixbuf *icon;
	int i;

	mcw = config_wizard_new ();
	mcw->druid = (GnomeDruid *)glade_xml_get_widget (mcw->gui->xml, "druid");
	g_object_set_data (G_OBJECT (mcw->druid), "MailConfigWizard", mcw);
	gtk_widget_show_all (GTK_WIDGET (mcw->druid));
	
	mcw->interior_pages = g_ptr_array_new ();
	for (i = 0; i < num_wizard_pages; i++) {
		page = glade_xml_get_widget (mcw->gui->xml,
					     wizard_pages[i].page_name);
		icon = e_icon_factory_get_icon (wizard_pages[i].icon_name, E_ICON_SIZE_DIALOG);
		gnome_druid_page_standard_set_logo (GNOME_DRUID_PAGE_STANDARD (page), icon);
		g_object_unref (icon);
		g_ptr_array_add (mcw->interior_pages, page);
		gtk_box_pack_start (GTK_BOX (GNOME_DRUID_PAGE_STANDARD (page)->vbox),
				    get_page (mcw->gui->xml, i),
				    FALSE, FALSE, 0);
		g_signal_connect (page, "back", G_CALLBACK (druid_back),
				  GINT_TO_POINTER (i));
		g_signal_connect (page, "next", G_CALLBACK (druid_next),
				  GINT_TO_POINTER (i));

		/* At least in 2.0 (and probably 2.2 too),
		 * GnomeDruidPageStandard is broken and you need to
		 * connect_after to "prepare" or else its default
		 * method will run after your signal handler and
		 * undo its button sensitivity changes.
		 */
		g_signal_connect_after (page, "prepare",
					G_CALLBACK (druid_prepare),
					GINT_TO_POINTER (i));
	}
	g_signal_connect (mcw->druid, "cancel", G_CALLBACK (druid_cancel), mcw);

	mcw->last_page = (GnomeDruidPage *)glade_xml_get_widget (mcw->gui->xml, "finish_page");
	g_signal_connect (mcw->last_page, "finish", G_CALLBACK (druid_finish), mcw);

	gnome_druid_set_buttons_sensitive (mcw->druid, FALSE, TRUE, TRUE, FALSE);
	/*gtk_widget_show_all (GTK_WIDGET (mcw->druid));*/
	mail_account_gui_setup (mcw->gui, NULL);
	
	new = glade_xml_get_widget (mcw->gui->xml, "account_druid");
	gtk_window_set_type_hint ((GtkWindow *) new, GDK_WINDOW_TYPE_HINT_DIALOG);
	
	return (MailConfigDruid *) new;
}


/* CORBA wizard */

static void
wizard_next_cb (EvolutionWizard *wizard,
		int page_num,
		MailConfigWizard *mcw)
{
	if (page_num >= MAIL_CONFIG_WIZARD_PAGE_MANAGEMENT)
		return;

	if (wizard_pages[page_num].next_func &&
	    wizard_pages[page_num].next_func (mcw))
		return;

	evolution_wizard_set_page (wizard, page_num + 1, NULL);
}

static void
wizard_prepare_cb (EvolutionWizard *wizard,
		   int page_num,
		   MailConfigWizard *mcw)
{
	if (wizard_pages[page_num].prepare_func)
		wizard_pages[page_num].prepare_func (mcw);
}

static void
wizard_back_cb (EvolutionWizard *wizard,
		int page_num,
		MailConfigWizard *mcw) 
{
	if (page_num >= MAIL_CONFIG_WIZARD_NUM_PAGES) {
		evolution_wizard_set_page (wizard, MAIL_CONFIG_WIZARD_PAGE_MANAGEMENT, NULL);
		return;
	}

	if (wizard_pages[page_num].back_func &&
	    wizard_pages[page_num].back_func (mcw))
		return;

	if (page_num > 0)
		evolution_wizard_set_page (wizard, page_num - 1, NULL);
}

static void
wizard_finish_cb (EvolutionWizard *wizard,
		  MailConfigWizard *w)
{
	MailAccountGui *gui = w->gui;
	
	/* Save the settings for that account */
	if (mail_account_gui_save (gui) == FALSE)
		/* problem. Um, how to keep the druid alive? */
		return;
	
	/* Write out the config info */
	mail_config_write ();
	mail_account_gui_destroy (gui);
	w->gui = NULL;
}

static void
wizard_cancel_cb (EvolutionWizard *wizard,
		  MailConfigWizard *mcw)
{
	mail_account_gui_destroy (mcw->gui);
	mcw->gui = NULL;
}

static void
wizard_help_cb (EvolutionWizard *wizard,
		int page_num,
		MailConfigWizard *mcw)
{
}

BonoboObject *
evolution_mail_config_wizard_new (void)
{
	EvolutionWizard *wizard;
	MailConfigWizard *mcw;
	GdkPixbuf *icon;
	int i;
	
	mcw = config_wizard_new ();	
	mail_account_gui_setup (mcw->gui, NULL);
	
	wizard = evolution_wizard_new ();
	for (i = 0; i < MAIL_CONFIG_WIZARD_NUM_PAGES; i++) {
		icon = e_icon_factory_get_icon (wizard_pages[i].icon_name, E_ICON_SIZE_DIALOG);
		evolution_wizard_add_page (wizard, _(wizard_pages[i].title),
					   icon, get_page (mcw->gui->xml, i));
		g_object_unref (icon);
	}
	
	g_object_set_data_full (G_OBJECT (wizard), "MailConfigWizard",
				mcw, (GDestroyNotify)free_config_wizard);
	mcw->corba_wizard = wizard;
	
	g_signal_connect (wizard, "next", G_CALLBACK (wizard_next_cb), mcw);
	g_signal_connect (wizard, "prepare", G_CALLBACK (wizard_prepare_cb), mcw);
	g_signal_connect (wizard, "back", G_CALLBACK (wizard_back_cb), mcw);
	g_signal_connect (wizard, "finish", G_CALLBACK (wizard_finish_cb), mcw);
	g_signal_connect (wizard, "cancel", G_CALLBACK (wizard_cancel_cb), mcw);
	g_signal_connect (wizard, "help", G_CALLBACK (wizard_help_cb), mcw);
	
	return BONOBO_OBJECT (wizard);
}
