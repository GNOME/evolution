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

#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-druid.h>
#include <libgnomeui/gnome-druid-page-standard.h>
#include <glade/glade.h>
#include <gtkhtml/gtkhtml.h>
#include <gal/widgets/e-unicode.h>
#include "mail-config-druid.h"
#include "mail-config.h"
#include "mail-ops.h"
#include "mail.h"
#include "mail-session.h"

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-context.h>

#include <evolution-wizard.h>

static void mail_config_druid_class_init (MailConfigDruidClass *class);
static void mail_config_druid_finalize   (GtkObject *obj);

static GtkWindowClass *parent_class;

/* These globals need fixed FIXME FIXME FIXME FIXME*/
static GHashTable *page_hash = NULL;
static GList *page_list = NULL;
static EvolutionWizard *account_wizard;

#define WIZARD_IID "OAFIID:GNOME_Evolution_Mail_Wizard_Factory"

typedef enum {
	MAIL_CONFIG_WIZARD_PAGE_NONE = -1,
	MAIL_CONFIG_WIZARD_PAGE_IDENTITY,
	MAIL_CONFIG_WIZARD_PAGE_SOURCE,
	MAIL_CONFIG_WIZARD_PAGE_EXTRA,
	MAIL_CONFIG_WIZARD_PAGE_TRANSPORT,
	MAIL_CONFIG_WIZARD_PAGE_MANAGEMENT,
} MailConfigWizardPage;

typedef struct {
	MailAccountGui *gui;

	MailConfigAccount *account;
	EvolutionWizard *wizard;

	gboolean identity_copied;
	CamelProvider *last_source;
	MailConfigWizardPage page;
} MailConfigWizard;

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
	CORBA_Environment ev;

	gtk_object_destroy (GTK_OBJECT (druid->xml));
	
	CORBA_exception_init (&ev);
	Bonobo_EventSource_removeListener ((Bonobo_EventSource) druid->event_source, druid->id, &ev);
	CORBA_exception_free (&ev);

	bonobo_object_release_unref ((Bonobo_Unknown) druid->event_source, &ev);
	bonobo_object_unref (BONOBO_OBJECT (druid->listener));

        ((GtkObjectClass *)(parent_class))->finalize (obj);
}


static struct {
	char *name;
	char *text;
} info[] = {
	{ "identity_html",
	  N_("Please enter your name and email address below. The \"optional\" fields below do not need to be filled in, unless you wish to include this information in email you send.") },
	{ "source_html",
	  N_("Please enter information about your incoming mail server below. If you are not sure, ask your system administrator or Internet Service Provider.") },
	{ "extra_html",
	  N_("Please select among the following options") },
	{ "transport_html",
	  N_("Please enter information about the way you will send mail. If you are not sure, ask your system administrator or Internet Service Provider.") },
	{ "management_html",
	  N_("You are almost done with the mail configuration process. The identity, incoming mail server and outgoing mail transport method which you provided will be grouped together to make an Evolution mail account. Please enter a name for this account in the space below. This name will be used for display purposes only.") }
};
static int num_info = (sizeof (info) / sizeof (info[0]));

static GtkWidget *
create_label (const char *name)
{
	GtkWidget *widget, *align;
	int i;
	
	for (i = 0; i < num_info; i++) {
		if (!strcmp (name, info[i].name))
			break;
	}
	
	g_return_val_if_fail (i != num_info, NULL);
	
	widget = gtk_label_new (_(info[i].text));
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_FILL);
	gtk_widget_show (widget);
	
	align = gtk_alignment_new (0.0, 0.5, 1.0, 1.0);
	gtk_container_add (GTK_CONTAINER (align), widget);
	
	gtk_widget_show (align);
	
	return align;
}

static void
druid_cancel (GnomeDruid *druid, gpointer user_data)
{
	MailConfigDruid *config = user_data;
	GNOME_Evolution_Wizard wiz;
	CORBA_Environment ev;

	wiz = bonobo_object_corba_objref (BONOBO_OBJECT (account_wizard));
	CORBA_exception_init (&ev);

	GNOME_Evolution_Wizard_notifyAction (wiz, 0, GNOME_Evolution_Wizard_CANCEL, &ev);
	CORBA_exception_free (&ev);

	if (page_list != NULL) {
		g_list_free (page_list);
		page_list = NULL;
	}

	if (page_hash != NULL) {
		g_hash_table_destroy (page_hash);
		page_hash = NULL;
	}

	gtk_widget_destroy (GTK_WIDGET (config));
}

static void
druid_finish (GnomeDruidPage *page, gpointer arg1, gpointer user_data)
{
	MailConfigDruid *druid = user_data;

	gtk_object_set_data (GTK_OBJECT (account_wizard), "account-data", NULL);
	if (page_list != NULL) {
		g_list_free (page_list);
		page_list = NULL;
	}

	if (page_hash != NULL) {
		g_hash_table_destroy (page_hash);
		page_hash = NULL;
	}

	gtk_widget_destroy (GTK_WIDGET (druid));
}

/* Identity Page */
static void
identity_changed (GtkWidget *widget, gpointer data)
{
	MailConfigWizard *gui = data;
	GtkWidget *incomplete;
	gboolean next_sensitive;
	
	if (gui->page != MAIL_CONFIG_WIZARD_PAGE_IDENTITY)
		return;
	
	next_sensitive = mail_account_gui_identity_complete (gui->gui, &incomplete);
	
	evolution_wizard_set_buttons_sensitive (gui->wizard, TRUE, next_sensitive, TRUE, NULL);
	
	if (!next_sensitive)
		gtk_widget_grab_focus (incomplete);
}

static void
identity_prepare (EvolutionWizard *wizard, gpointer data)
{
	MailConfigWizard *gui = data;
	const char *name;
	
	gui->page = MAIL_CONFIG_WIZARD_PAGE_IDENTITY;
	
	name = gtk_entry_get_text (gui->gui->full_name);
	if (!name) {
		name = g_get_real_name ();
		gtk_entry_set_text (gui->gui->full_name, name ? name : "");
		gtk_entry_select_region (gui->gui->full_name, 0, -1);
	}
	gtk_widget_grab_focus (GTK_WIDGET (gui->gui->full_name));
	identity_changed (NULL, data);
}

static gboolean
identity_next (EvolutionWizard *wizard, gpointer data)
{
	MailConfigWizard *gui = data;
	
	if (!gui->identity_copied) {
		char *username;
		
		/* Copy the username part of the email address into
		 * the Username field of the source and transport pages.
		 */
		username = gtk_entry_get_text (gui->gui->email_address);
		username = g_strndup (username, strcspn (username, "@"));
		gtk_entry_set_text (gui->gui->source.username, username);
		gtk_entry_set_text (gui->gui->transport.username, username);
		g_free (username);
		
		gui->identity_copied = TRUE;
	}
	
	return FALSE;
}

/* Incoming mail Page */
static void
source_changed (GtkWidget *widget, gpointer data)
{
	MailConfigWizard *gui = data;
	GtkWidget *incomplete;
	gboolean next_sensitive;
	
	if (gui->page != MAIL_CONFIG_WIZARD_PAGE_SOURCE)
		return;
	
	next_sensitive = mail_account_gui_source_complete (gui->gui, &incomplete);
	
	evolution_wizard_set_buttons_sensitive (gui->wizard, TRUE, next_sensitive, TRUE, NULL);
	
	if (!next_sensitive)
		gtk_widget_grab_focus (incomplete);
}

static void
source_prepare (EvolutionWizard *wizard, gpointer data)
{
	MailConfigWizard *gui = data;
	
	gui->page = MAIL_CONFIG_WIZARD_PAGE_SOURCE;
	source_changed (NULL, gui);
}

static gboolean
source_next (EvolutionWizard *wizard, gpointer data)
{
	MailConfigWizard *gui = data;
	
	/* FIXME: if online, check that the data is good. */
	
	if (gui->gui->source.provider && gui->gui->source.provider->extra_conf)
		return FALSE;
	
	/* Otherwise, skip to transport page. */
	evolution_wizard_set_page (gui->wizard, MAIL_CONFIG_WIZARD_PAGE_TRANSPORT, NULL);
	
	return TRUE;
}

/* Extra Config Page */
static void
extra_prepare (EvolutionWizard *wizard, gpointer data)
{
	MailConfigWizard *gui = data;
	
	gui->page = MAIL_CONFIG_WIZARD_PAGE_EXTRA;
	if (gui->gui->source.provider != gui->last_source) {
		gui->last_source = gui->gui->source.provider;
		mail_account_gui_auto_detect_extra_conf (gui->gui);
	}
}

/* Transport Page */
static gboolean
transport_next (EvolutionWizard *wizard, gpointer data)
{
	/* FIXME: if online, check that the data is good. */
	return FALSE;
}

static gboolean
transport_back (EvolutionWizard *wizard, gpointer data)
{
	MailConfigWizard *gui = data;
	
	if (gui->gui->source.provider && gui->gui->source.provider->extra_conf)
		return FALSE;
	else {
		evolution_wizard_set_page (wizard, MAIL_CONFIG_WIZARD_PAGE_SOURCE, NULL);
		return TRUE;
	}
}

static void
transport_changed (GtkWidget *widget, gpointer data)
{
	MailConfigWizard *gui = data;
	GtkWidget *incomplete;
	gboolean next_sensitive;
	
	if (gui->page != MAIL_CONFIG_WIZARD_PAGE_TRANSPORT)
		return;
	
	next_sensitive = mail_account_gui_transport_complete (gui->gui, &incomplete);
	
	evolution_wizard_set_buttons_sensitive (gui->wizard, TRUE, next_sensitive, TRUE, NULL);
	
	if (!next_sensitive)
		gtk_widget_grab_focus (incomplete);
}

static void
transport_prepare (EvolutionWizard *wizard, gpointer data)
{
	MailConfigWizard *gui = data;
	
	gui->page = MAIL_CONFIG_WIZARD_PAGE_TRANSPORT;
	transport_changed (NULL, data);
}

/* Management page */
static gboolean
management_check (MailConfigWizard *wizard)
{
	gboolean next_sensitive;
	char *text;
	
	text = gtk_entry_get_text (wizard->gui->account_name);
	next_sensitive = text && *text;
	
	/* no accounts with the same name */
	if (next_sensitive && mail_config_get_account_by_name (text))
		next_sensitive = FALSE;
	
	evolution_wizard_set_buttons_sensitive (wizard->wizard, TRUE,
						next_sensitive, TRUE, NULL);
	return next_sensitive;
}

static void
management_prepare (EvolutionWizard *wizard, gpointer data)
{
	MailConfigWizard *gui = data;
	const char *name, *text;
	
	gui->page = MAIL_CONFIG_WIZARD_PAGE_MANAGEMENT;
	
	text = gtk_entry_get_text (gui->gui->account_name);
	if (!text || *text == '\0') {
		name = e_utf8_gtk_entry_get_text (gui->gui->email_address);
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
			
			e_utf8_gtk_entry_set_text (gui->gui->account_name, name);
		}
	}
	
	management_check (gui);
}

static void
management_changed (GtkWidget *widget, gpointer data)
{
	MailConfigWizard *gui = data;
	
	if (gui->page != MAIL_CONFIG_WIZARD_PAGE_MANAGEMENT)
		return;
	
	management_check (gui);
	
	gtk_widget_grab_focus (GTK_WIDGET (gui->gui->account_name));
}

static MailConfigAccount *
make_account (void)
{
	MailConfigAccount *account;
	char *name, *user;
	struct utsname uts;
	
	account = g_new0 (MailConfigAccount, 1);
	
	account->id = g_new0 (MailConfigIdentity, 1);
	name = g_get_real_name ();
	account->id->name = e_utf8_from_locale_string (name);
	user = g_get_user_name ();
	if (user && !uname (&uts) && strchr (uts.nodename, '.'))
		account->id->address = g_strdup_printf ("%s@%s", user, uts.nodename);
	
	if (mail_config_get_default_transport ())
		account->transport = service_copy (mail_config_get_default_transport ());
	
	return account;
}

static const char *pages[] = { 
	"identity_page",
	"source_page",
	"extra_page",
	"transport_page",
	"management_page",
	"finish_page",
	NULL
};

static int
page_to_num (gpointer page)
{
	gpointer r;

	r = g_hash_table_lookup (page_hash, page);
	if (r == NULL) {
		return 0;
	}

	return GPOINTER_TO_INT (r);
}

static gboolean
next_func (GnomeDruidPage *page,
	   GnomeDruid *druid,
	   gpointer data)
{
	GNOME_Evolution_Wizard wiz;
	CORBA_Environment ev;
	int pagenum;

	wiz = bonobo_object_corba_objref (BONOBO_OBJECT (account_wizard));
	CORBA_exception_init (&ev);

	pagenum = page_to_num (page);
	GNOME_Evolution_Wizard_notifyAction (wiz, pagenum, GNOME_Evolution_Wizard_NEXT, &ev);
	CORBA_exception_free (&ev);

	if (pagenum < 5-1)
		return TRUE;

	return FALSE;
}

static gboolean
prepare_func (GnomeDruidPage *page,
	      GnomeDruid *druid,
	      gpointer data)
{
	GNOME_Evolution_Wizard wiz;
	CORBA_Environment ev;
	int pagenum;

	wiz = bonobo_object_corba_objref (BONOBO_OBJECT (account_wizard));
	CORBA_exception_init (&ev);

	pagenum = page_to_num (page);
	GNOME_Evolution_Wizard_notifyAction (wiz, pagenum, GNOME_Evolution_Wizard_PREPARE, &ev);
	CORBA_exception_free (&ev);
	return FALSE;
}

static gboolean
back_func (GnomeDruidPage *page,
	   GnomeDruid *druid,
	   gpointer data)
{
	GNOME_Evolution_Wizard wiz;
	CORBA_Environment ev;
	int pagenum;

	wiz = bonobo_object_corba_objref (BONOBO_OBJECT (account_wizard));
	CORBA_exception_init (&ev);

	pagenum = page_to_num (page);
	GNOME_Evolution_Wizard_notifyAction (wiz, pagenum, GNOME_Evolution_Wizard_BACK, &ev);
	CORBA_exception_free (&ev);

	if (pagenum > 0)
		return TRUE;

	return FALSE;
}

static gboolean
finish_func (GnomeDruidPage *page,
	     GnomeDruid *druid,
	     gpointer data)
{
	GNOME_Evolution_Wizard wiz;
	CORBA_Environment ev;
	int pagenum;

	wiz = bonobo_object_corba_objref (BONOBO_OBJECT (account_wizard));
	CORBA_exception_init (&ev);

	pagenum = page_to_num (page);
	GNOME_Evolution_Wizard_notifyAction (wiz, 0, GNOME_Evolution_Wizard_FINISH, &ev);
	CORBA_exception_free (&ev);

	druid_finish (page, druid, data);
	return FALSE;
}

static void
wizard_listener_event (BonoboListener *listener,
		       char *event_name,
		       BonoboArg *event_data,
		       CORBA_Environment *ev,
		       MailConfigDruid *druid)
{
	CORBA_short buttons, pagenum;
	GnomeDruidPage *page;

	if (strcmp (event_name, EVOLUTION_WIZARD_SET_BUTTONS_SENSITIVE) == 0) {
		buttons = (int) *((CORBA_short *)event_data->_value);
		gnome_druid_set_buttons_sensitive (GNOME_DRUID (druid->druid),
						   (buttons & 4) >> 2,
						   (buttons & 2) >> 1,
						   (buttons & 1));
	} else if (strcmp (event_name, EVOLUTION_WIZARD_SET_SHOW_FINISH) == 0) {
		gnome_druid_set_show_finish (GNOME_DRUID (druid->druid),
					     (gboolean) *((CORBA_boolean *)event_data->_value));
	} else if (strcmp (event_name, EVOLUTION_WIZARD_SET_PAGE) == 0) {
		pagenum = (int) *((CORBA_short *) event_data->_value);

		page = g_list_nth_data (page_list, pagenum);
		gnome_druid_set_page (GNOME_DRUID (druid->druid), page);
	}
}

static void
construct (MailConfigDruid *druid)
{
	GtkWidget *widget;
	GNOME_Evolution_Wizard corba_wizard;
	Bonobo_Listener corba_listener;
	CORBA_Environment ev;
	int i;

	/* Start account wizard */
	CORBA_exception_init (&ev);
	corba_wizard = oaf_activate_from_id ("OAFIID:GNOME_Evolution_Mail_Wizard", 0, NULL, &ev);
	CORBA_exception_free (&ev);
	g_assert (account_wizard != NULL);
	
	druid->xml = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", NULL);
	/* get our toplevel widget and reparent it */
	widget = glade_xml_get_widget (druid->xml, "druid");
	gtk_widget_reparent (widget, GTK_WIDGET (druid));
	
	druid->druid = GNOME_DRUID (widget);

	/* set window title */
	gtk_window_set_title (GTK_WINDOW (druid), _("Evolution Account Assistant"));
	gtk_window_set_policy (GTK_WINDOW (druid), FALSE, TRUE, FALSE);
	gtk_window_set_modal (GTK_WINDOW (druid), FALSE);
	gtk_object_set (GTK_OBJECT (druid), "type", GTK_WINDOW_DIALOG, NULL);

	druid->listener = bonobo_listener_new (NULL, NULL);
	gtk_signal_connect (GTK_OBJECT (druid->listener), "event-notify",
			    GTK_SIGNAL_FUNC (wizard_listener_event), druid);
	corba_listener = bonobo_object_corba_objref (BONOBO_OBJECT (druid->listener));
	CORBA_exception_init (&ev);
	druid->event_source = (Bonobo_Unknown) bonobo_object_query_interface (
		BONOBO_OBJECT (account_wizard), "IDL:Bonobo/EventSource:1.0");
	g_assert (druid->event_source != CORBA_OBJECT_NIL);
	druid->id = Bonobo_EventSource_addListener ((Bonobo_EventSource) druid->event_source, corba_listener, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Error adding listener (%s)",
			   CORBA_exception_id (&ev));
	}
	CORBA_exception_free (&ev);

	if (page_hash != NULL) {
		g_hash_table_destroy (page_hash);
	}
	page_hash = g_hash_table_new (NULL, NULL);
	for (i = 0; pages[i] != NULL; i++) {
		GtkWidget *page;
		GnomeDruidPageStandard *dpage;

		page = glade_xml_get_widget (druid->xml, pages[i]);
		/* Store pages */
		g_hash_table_insert (page_hash, page, GINT_TO_POINTER (i));
		page_list = g_list_append (page_list, page);

		gtk_signal_connect (GTK_OBJECT (page), "next",
				    GTK_SIGNAL_FUNC (next_func), druid);
		gtk_signal_connect (GTK_OBJECT (page), "prepare",
				    GTK_SIGNAL_FUNC (prepare_func), druid);
		gtk_signal_connect (GTK_OBJECT (page), "back",
				    GTK_SIGNAL_FUNC (back_func), druid);
	
		gtk_signal_connect (GTK_OBJECT (page), "finish", 
				    GTK_SIGNAL_FUNC (finish_func), druid);

		if (i !=  5) {
			Bonobo_Control control;
			GtkWidget *w;
			CORBA_Environment ev;
			
			dpage = GNOME_DRUID_PAGE_STANDARD (page);

			CORBA_exception_init (&ev);
			control = GNOME_Evolution_Wizard_getControl (corba_wizard, i, &ev);
			if (BONOBO_EX (&ev)) {
				g_warning ("Error getting page %d: %s", i, 
					   CORBA_exception_id (&ev));
				CORBA_exception_free (&ev);
				continue;
			}

			w = bonobo_widget_new_control_from_objref (control,
								   CORBA_OBJECT_NIL);
			gtk_box_pack_start (GTK_BOX (dpage->vbox), w, TRUE,
					    TRUE, 0);
			gtk_widget_show_all (w);
		}
	}
	gtk_signal_connect (GTK_OBJECT (druid->druid), "cancel", druid_cancel, druid);

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

static void wizard_next_cb (EvolutionWizard *wizard, int page_num, MailConfigWizard *gui);

static void
goto_next_page (MailConfigWizard *gui)
{
	wizard_next_cb (gui->wizard, gui->page, gui);
}

static void
identity_activate_cb (GtkEntry *ent, gpointer user_data)
{
	MailConfigWizard *gui = (MailConfigWizard *) user_data;

	if (mail_account_gui_identity_complete (gui->gui, NULL))
		goto_next_page (gui);
}

static void
source_activate_cb (GtkEntry *ent, gpointer user_data)
{
	MailConfigWizard *gui = (MailConfigWizard *) user_data;

	if (mail_account_gui_source_complete (gui->gui, NULL))
		goto_next_page (gui);
}

static void
transport_activate_cb (GtkEntry *ent, gpointer user_data)
{
	MailConfigWizard *gui = (MailConfigWizard *) user_data;

	if (mail_account_gui_transport_complete (gui->gui, NULL))
		goto_next_page (gui);
}

static void
management_activate_cb (GtkEntry *ent, gpointer user_data)
{
	MailConfigWizard *gui = (MailConfigWizard *) user_data;

	if (management_check (gui))
		goto_next_page (gui);
}

static BonoboControl *
get_fn (EvolutionWizard *wizard,
        int page_num,
        void *closure)
{
        MailConfigWizard *gui = closure;
        BonoboControl *control;
        GtkWidget *vbox, *widget;
	static gboolean first_time = TRUE;
	
        if (gui->gui == NULL) {
		if (gui->account == NULL) {
			gui->account = make_account ();
			gtk_object_set_data (GTK_OBJECT (wizard), "account-data",
					     gui->account);
		}
		
		gui->gui = mail_account_gui_new (gui->account, NULL);
		
		/* set up signals, etc */
		gtk_signal_connect (GTK_OBJECT (gui->gui->account_name),
				    "changed", management_changed, gui);
		gtk_signal_connect (GTK_OBJECT (gui->gui->full_name), 
                                    "changed", identity_changed, gui);
                gtk_signal_connect (GTK_OBJECT (gui->gui->email_address),
                                    "changed", identity_changed, gui);
		gtk_signal_connect (GTK_OBJECT (gui->gui->reply_to),
                                    "changed", identity_changed, gui);
		gtk_signal_connect (GTK_OBJECT (gui->gui->source.hostname), 
				    "changed", source_changed, gui);
		gtk_signal_connect (GTK_OBJECT (gui->gui->source.username),
				    "changed", source_changed, gui);
		gtk_signal_connect (GTK_OBJECT (gui->gui->source.path), 
				    "changed", source_changed, gui);
		gtk_signal_connect (GTK_OBJECT (gui->gui->transport.hostname),
				    "changed", transport_changed, gui);
		gtk_signal_connect (GTK_OBJECT (gui->gui->transport.username),
				    "changed", transport_changed, gui);
		gtk_signal_connect (GTK_OBJECT (gui->gui->transport_needs_auth),
				    "toggled", transport_changed, gui);

		gtk_signal_connect (GTK_OBJECT (gui->gui->account_name),
				    "activate", management_activate_cb, gui);

		gtk_signal_connect (GTK_OBJECT (gui->gui->full_name), 
				    "activate", identity_activate_cb, gui);
                gtk_signal_connect (GTK_OBJECT (gui->gui->email_address),
				    "activate", identity_activate_cb, gui);
                gtk_signal_connect (GTK_OBJECT (gui->gui->reply_to),
				    "activate", identity_activate_cb, gui);
                gtk_signal_connect (GTK_OBJECT (gui->gui->organization),
				    "activate", identity_activate_cb, gui);

		gtk_signal_connect (GTK_OBJECT (gui->gui->source.hostname), 
				    "activate", source_activate_cb, gui);
		gtk_signal_connect (GTK_OBJECT (gui->gui->source.username),
				    "activate", source_activate_cb, gui);
		gtk_signal_connect (GTK_OBJECT (gui->gui->source.path), 
				    "activate", source_activate_cb, gui);

		gtk_signal_connect (GTK_OBJECT (gui->gui->transport.hostname),
				    "activate", transport_activate_cb, gui);
		gtk_signal_connect (GTK_OBJECT (gui->gui->transport.username),
				    "activate", transport_activate_cb, gui);
		first_time = TRUE;
        }
	
        /* Fill in the druid pages */
	vbox = gtk_vbox_new (FALSE, 0);
        switch (page_num) {
        case 0:
		widget = create_label ("identity_html");
		gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
		widget = glade_xml_get_widget (gui->gui->xml, "identity_required_frame");
		gtk_widget_reparent (widget, vbox);
		gtk_box_set_child_packing (GTK_BOX (vbox), widget, FALSE, FALSE, 0, GTK_PACK_START);
		widget = glade_xml_get_widget (gui->gui->xml, "identity_optional_frame");
		gtk_widget_reparent (widget, vbox);
		gtk_box_set_child_packing (GTK_BOX (vbox), widget, FALSE, FALSE, 0, GTK_PACK_START);
                break;
		
        case 1:
		widget = create_label ("source_html");
		gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
		widget = glade_xml_get_widget (gui->gui->xml, "source_vbox");
                gtk_widget_reparent (widget, vbox);
		gtk_widget_show (widget);
                break;
		
        case 2:
		widget = create_label ("extra_html");
		gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
                widget = glade_xml_get_widget (gui->gui->xml, "extra_vbox");
                gtk_widget_reparent (widget, vbox);
                break;
		
        case 3:
		widget = create_label ("transport_html");
		gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
                widget = glade_xml_get_widget (gui->gui->xml, "transport_vbox");
                gtk_widget_reparent (widget, vbox);
		gtk_widget_show (widget);
                break;
		
        case 4:
		widget = glade_xml_get_widget (gui->gui->xml, "management_frame");
		gtk_widget_reparent (widget, vbox);
		break;

        default:
                return NULL;
        }
	
        gtk_widget_show (vbox);
        control = bonobo_control_new (vbox);
	
	if (first_time) {
		mail_account_gui_setup (gui->gui, NULL);
		first_time = FALSE;
	}
	
        return control;
}

typedef gboolean (*NextFunc)(EvolutionWizard *wizard, gpointer data);

static struct {
        NextFunc next_func;
        GtkSignalFunc prepare_func;
        NextFunc back_func;
        GtkSignalFunc finish_func;
        GtkSignalFunc help_func;
} wizard_pages[] = {
        { identity_next,
          GTK_SIGNAL_FUNC (identity_prepare),
          NULL,
          GTK_SIGNAL_FUNC (NULL),
          GTK_SIGNAL_FUNC (NULL) },
        { source_next,
          GTK_SIGNAL_FUNC (source_prepare),
          NULL,
          GTK_SIGNAL_FUNC (NULL),
          GTK_SIGNAL_FUNC (NULL) },
        { NULL,
          GTK_SIGNAL_FUNC (extra_prepare),
          NULL,
          GTK_SIGNAL_FUNC (NULL),
          GTK_SIGNAL_FUNC (NULL) },
        { transport_next,
          GTK_SIGNAL_FUNC (transport_prepare),
          transport_back,
          GTK_SIGNAL_FUNC (NULL),
          GTK_SIGNAL_FUNC (NULL) },
        { NULL,
          GTK_SIGNAL_FUNC (management_prepare),
          NULL,
          GTK_SIGNAL_FUNC (NULL),
          GTK_SIGNAL_FUNC (NULL) }
};

static void
wizard_next_cb (EvolutionWizard *wizard,
                int page_num,
                MailConfigWizard *gui)
{
        if (wizard_pages[page_num].next_func == NULL
	    || !(wizard_pages[page_num].next_func (wizard, gui))) {
		if (page_num < 5-1) {
			evolution_wizard_set_page(wizard, page_num+1, NULL);
		}
	}
}

static void
wizard_prepare_cb (EvolutionWizard *wizard,
                   int page_num,
                   MailConfigWizard *gui)
{
        if (wizard_pages[page_num].prepare_func != NULL) {
                wizard_pages[page_num].prepare_func (wizard, gui);
        }
}

static void
wizard_back_cb (EvolutionWizard *wizard,
                int page_num,
                MailConfigWizard *gui) 
{
	if (page_num >= 5)
		evolution_wizard_set_page(wizard, 4, NULL);
        else if (wizard_pages[page_num].back_func == NULL
	    || !(wizard_pages[page_num].back_func (wizard, gui))) {
		if (page_num > 0)
			evolution_wizard_set_page(wizard, page_num-1, NULL);
        }
}

static void
wizard_finish_cb (EvolutionWizard *wizard,
		  int page_num,
		  MailConfigWizard *w)
{
	MailAccountGui *gui = w->gui;

	/* Save the settings for that account */
	if (mail_account_gui_save (gui) == FALSE)
		/* problem. Um, how to keep the druid alive? */
		return;
	
	if (gui->account->source)
		gui->account->source->enabled = TRUE;
	
	/* Write out the config info */
	mail_config_write ();
	mail_account_gui_destroy (gui);
	w->gui = NULL;
	w->account = NULL;
}

static void
wizard_cancel_cb (EvolutionWizard *wizard,
                  int page_num,
                  MailConfigWizard *gui)
{
	mail_account_gui_destroy (gui->gui);
	gui->gui = NULL;
}

static void
wizard_help_cb (EvolutionWizard *wizard,
                int page_num,
                MailConfigWizard *gui)
{
}

static void
wizard_free (MailConfigWizard *wizard)
{
	if (wizard->gui)
		mail_account_gui_destroy (wizard->gui);
	
	if (wizard->account)
		account_destroy (wizard->account);
	
	g_free (wizard);
}

static BonoboObject *
evolution_mail_config_wizard_factory_fn (BonoboGenericFactory *factory,
					 void *closure)
{
	EvolutionWizard *wizard;
        MailConfigAccount *account;
        MailConfigWizard *gui;
	
        account = make_account ();
	
        gui = g_new (MailConfigWizard, 1);
	gui->gui = NULL;
        gui->account = account;
	gui->identity_copied = FALSE;
	gui->last_source = NULL;
	gui->page = MAIL_CONFIG_WIZARD_PAGE_NONE;
	
        wizard = evolution_wizard_new (get_fn, 5, gui);
	account_wizard = wizard;
	
	gtk_object_set_data_full (GTK_OBJECT (account_wizard),
				  "account-data", gui,
				  (GtkDestroyNotify) wizard_free);
	gui->wizard = wizard;
	
        gtk_signal_connect (GTK_OBJECT (wizard), "next",
                            GTK_SIGNAL_FUNC (wizard_next_cb), gui);
        gtk_signal_connect (GTK_OBJECT (wizard), "prepare",
                            GTK_SIGNAL_FUNC (wizard_prepare_cb), gui);
        gtk_signal_connect (GTK_OBJECT (wizard), "back",
                            GTK_SIGNAL_FUNC (wizard_back_cb), gui);
        gtk_signal_connect (GTK_OBJECT (wizard), "finish",
                            GTK_SIGNAL_FUNC (wizard_finish_cb), gui);
        gtk_signal_connect (GTK_OBJECT (wizard), "cancel",
                            GTK_SIGNAL_FUNC (wizard_cancel_cb), gui);
        gtk_signal_connect (GTK_OBJECT (wizard), "help",
                            GTK_SIGNAL_FUNC (wizard_help_cb), gui);
	
        return BONOBO_OBJECT (wizard);
}

void
evolution_mail_config_wizard_init (void)
{
	BonoboGenericFactory *factory;

	factory = bonobo_generic_factory_new (WIZARD_IID,
					      evolution_mail_config_wizard_factory_fn, NULL);

	if (factory == NULL) {
		g_warning ("Error starting factory");
		return;
	}

	bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));
}
