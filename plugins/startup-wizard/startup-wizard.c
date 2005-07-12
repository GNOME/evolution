/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  JP Rosevear <jpr@novell.com>
 *  Copyright (C) 2005 Novell, Inc.
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
 */

#include <gconf/gconf-client.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libgnomeui/libgnomeui.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-widget.h>
#include "widgets/e-timezone-dialog/e-timezone-dialog.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-gtk-utils.h"
#include "shell/es-event.h"
#include "mail/em-config.h"
#include "mail/em-account-editor.h"
#include "calendar/gui/calendar-config.h"

#define IMPORT_TIMEZONE_DIALOG "StartupWizard::TimezoneDialog"

void startup_wizard (EPlugin *ep, ESEventTargetUpgrade *target);
GtkWidget *startup_wizard_timezone_page (EPlugin *ep, EConfigHookItemFactoryData *hook_data);
GtkWidget *startup_wizard_importer_page (EPlugin *ep, EConfigHookItemFactoryData *hook_data);
gboolean startup_wizard_check (EPlugin *ep, EConfigHookPageCheckData *check_data);
void startup_wizard_commit (EPlugin *ep, EMConfigTargetAccount *target);
void startup_wizard_abort (EPlugin *ep, EMConfigTargetAccount *target);

static GList *useable_importers = NULL;
gboolean useable_importers_init = FALSE;

static GList *
get_intelligent_importers (void)
{
	return NULL;
}

static void
init_importers (void)
{
}

static void
startup_wizard_delete (void) {
	gtk_main_quit ();
	_exit (0);
}


void
startup_wizard (EPlugin *ep, ESEventTargetUpgrade *target)
{
	GConfClient *client;
	GSList *accounts;
	EMAccountEditor *emae;
	GnomeDruidPageEdge *start_page;
	
	client = gconf_client_get_default ();
	accounts = gconf_client_get_list (client, "/apps/evolution/mail/accounts", GCONF_VALUE_STRING, NULL);
	g_object_unref (client);
	
	if (accounts != NULL) {
		g_slist_foreach (accounts, (GFunc) g_free, NULL);
		g_slist_free (accounts);

		return;
	}	

	/** @HookPoint-EMConfig: New Mail Account Wizard
	 * @Id: org.gnome.evolution.mail.config.accountWizard
	 * @Type: E_CONFIG_DRUID
	 * @Class: org.gnome.evolution.mail.config:1.0
	 * @Target: EMConfigTargetAccount
	 *
	 * The new mail account druid.
	 */
	emae = em_account_editor_new (NULL, EMAE_DRUID, "org.gnome.evolution.mail.config.accountWizard");

	gtk_window_set_title (GTK_WINDOW (emae->editor), _("Evolution Setup Assistant"));

	start_page = GNOME_DRUID_PAGE_EDGE (e_config_page_get ((EConfig *) emae->config, "0.start"));
	gnome_druid_page_edge_set_title (start_page, _("Welcome"));
	gnome_druid_page_edge_set_text (start_page, _(""
					"Welcome to Evolution. The next few screens will allow Evolution to connect "
					"to your email accounts, and to import files from other applications. \n"
					"\n"
					"Please click the \"Forward\" button to continue. "));
	g_signal_connect (emae->editor, "delete-event", G_CALLBACK (startup_wizard_delete), NULL);
	
	gtk_widget_show (emae->editor);
	gtk_main ();
}

GtkWidget *
startup_wizard_timezone_page (EPlugin *ep, EConfigHookItemFactoryData *hook_data)
{
	ETimezoneDialog *etd;
	GtkWidget *page;
	icaltimezone *zone;
	
	etd = e_timezone_dialog_new ();
	g_object_set_data (G_OBJECT (hook_data->config), IMPORT_TIMEZONE_DIALOG, etd);
	
	page = gnome_druid_page_standard_new_with_vals ("Timezone", NULL, NULL);
	e_timezone_dialog_reparent (etd, GNOME_DRUID_PAGE_STANDARD (page)->vbox);

	zone = calendar_config_get_icaltimezone	();
	if (zone)
		e_timezone_dialog_set_timezone (etd, zone);
	
	gnome_druid_append_page (GNOME_DRUID (hook_data->parent), GNOME_DRUID_PAGE (page));

	return GTK_WIDGET (page);
}

GtkWidget *
startup_wizard_importer_page (EPlugin *ep, EConfigHookItemFactoryData *hook_data)
{
	return NULL;
#if 0	
	page = gnome_druid_page_standard_new_with_vals ("Importing files", NULL, NULL);

	label = gtk_label_new (_("Please select the information that you would like to import:"));
	gtk_box_pack_start (GTK_BOX (GNOME_DRUID_PAGE_STANDARD (page)->vbox), label, FALSE, FALSE, 3);

	sep = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (GNOME_DRUID_PAGE_STANDARD (page)->vbox), sep, FALSE, FALSE, 3);
	
	table = gtk_table_new (g_list_length (useable_importers), 2, FALSE);
	for (l = useable_importers, i = 0; l; l = l->next, i++) {
		IntelligentImporterData *id = l->data;
		GtkWidget *widget;
		char *str;
		
		CORBA_Environment ev;
		CORBA_exception_init (&ev);

		str = g_strdup_printf (_("From %s:"), id->name);
		label = gtk_label_new (str);
		g_free (str);
	
		gtk_misc_set_alignment (GTK_MISC (label), 0, .5);

		widget = bonobo_widget_new_control_from_objref (id->control, CORBA_OBJECT_NIL);

		gtk_table_attach (GTK_TABLE (table), label, 0, 1, i, i + 1, GTK_FILL, 0, 0, 0);
		gtk_table_attach (GTK_TABLE (table), widget, 1, 2, i, i + 1, GTK_FILL, 0, 3, 0);
		gtk_widget_show_all (table);
	
		gtk_box_pack_start (GTK_BOX (GNOME_DRUID_PAGE_STANDARD (page)->vbox), table,
				    FALSE, FALSE, 0);
	}
	
	gnome_druid_append_page (GNOME_DRUID (hook_data->parent), GNOME_DRUID_PAGE (page));

	return GTK_WIDGET (page);
#endif
}

#if 0
static void
start_importers (GList *p)
{
	CORBA_Environment ev;
	
	for (; p; p = p->next) {
		SelectedImporterData *sid = p->data;

		CORBA_exception_init (&ev);
		GNOME_Evolution_IntelligentImporter_importData (sid->importer, &ev);
		if (BONOBO_EX (&ev)) {
			g_warning ("Error importing %s\n%s", sid->iid,
				   CORBA_exception_id (&ev));
		}
		CORBA_exception_free (&ev);
	}
}

static void
do_import ()
{
	CORBA_Environment ev;
	GList *l, *selected = NULL;

	for (l = useable_importers; l; l = l->next) {
		IntelligentImporterData *importer_data;
		SelectedImporterData *sid;
		char *iid;

		importer_data = l->data;
		iid = g_strdup (importer_data->iid);

		sid = g_new (SelectedImporterData, 1);
		sid->iid = iid;

		CORBA_exception_init (&ev);
		sid->importer = bonobo_object_dup_ref (importer_data->object, &ev);
		if (BONOBO_EX (&ev)) {
			g_warning ("Error duplication %s\n(%s)", iid,
				   CORBA_exception_id (&ev));
			g_free (iid);
			CORBA_exception_free (&ev);
			g_free (sid);
			continue;
		}
		CORBA_exception_free (&ev);

		selected = g_list_prepend (selected, sid);
	}

	free_importers ();

	if (selected != NULL) {
		start_importers (selected);

		for (l = selected; l; l = l->next) {
			SelectedImporterData *sid = l->data;

			CORBA_exception_init (&ev);
			bonobo_object_release_unref (sid->importer, &ev);
			CORBA_exception_free (&ev);

			g_free (sid->iid);
			g_free (sid);
		}
		g_list_free (selected);
	}
}
#endif

void
startup_wizard_commit (EPlugin *ep, EMConfigTargetAccount *target)
{
	EConfig *ec = ((EConfigTarget *)target)->config;
	ETimezoneDialog *etd;
	icaltimezone *zone;

	/*FIXME: do_import ();*/

	/* Set Timezone */
	etd = g_object_get_data (G_OBJECT (ec), IMPORT_TIMEZONE_DIALOG);
	if (etd) {
		zone = e_timezone_dialog_get_timezone (E_TIMEZONE_DIALOG (etd));
		if (zone)
			calendar_config_set_timezone (icaltimezone_get_display_name (zone));

		/* Need to do this otherwise the timezone widget gets destroyed but the
		   timezone object isn't, and we can get a crash like #22047.  */
		g_object_unref (etd);
		g_object_set_data (G_OBJECT (ec), IMPORT_TIMEZONE_DIALOG, NULL);
	}
	

	gtk_main_quit ();
}

void
startup_wizard_abort (EPlugin *ep, EMConfigTargetAccount *target)
{
	EConfig *ec = ((EConfigTarget *)target)->config;
	ETimezoneDialog *etd;

	etd = g_object_get_data (G_OBJECT (ec), IMPORT_TIMEZONE_DIALOG);
	if (etd) {
		/* Need to do this otherwise the timezone widget gets destroyed but the
		   timezone object isn't, and we can get a crash like #22047.  */
		g_object_unref (etd);
		g_object_set_data (G_OBJECT (ec), IMPORT_TIMEZONE_DIALOG, NULL);
	}

	gtk_main_quit ();
	_exit (0);
}
