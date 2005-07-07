/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  JP Rosevear <jpr@novell.com>
 *  Copyright (C) 2005 Novell, Inc.
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use, copy,
 *  modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
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
#include "shell/importer/GNOME_Evolution_Importer.h"
#include "mail/em-config.h"
#include "mail/em-account-editor.h"
#include "calendar/gui/calendar-config.h"

typedef struct _IntelligentImporterData {
	CORBA_Object object;
	Bonobo_Control control;
	
	char *name;
	char *blurb;
	char *iid;
} IntelligentImporterData;

typedef struct _SelectedImporterData{
	CORBA_Object importer;
	char *iid;
} SelectedImporterData;

#define IMPORT_PAGE_DATA "StartupWizard::ImportData"
#define IMPORT_TIMEZONE_DIALOG "StartupWizard::TimezoneDialog"

void startup_wizard (EPlugin *ep, ESEventTargetUpgrade *target);
GtkWidget *startup_wizard_timezone_page (EPlugin *ep, EConfigHookItemFactoryData *hook_data);
GtkWidget *startup_wizard_importer_page (EPlugin *ep, EConfigHookItemFactoryData *hook_data);
gboolean startup_wizard_check (EPlugin *ep, EConfigHookPageCheckData *check_data);
void startup_wizard_commit (EPlugin *ep, EMConfigTargetAccount *target);
void startup_wizard_abort (EPlugin *ep, EMConfigTargetAccount *target);

static GList *useable_importers = NULL;
gboolean useable_importers_init = FALSE;

static void
free_importers ()
{
	GList *l;

	for (l = useable_importers; l; l = l->next) {
		IntelligentImporterData *iid;

		/* FIXME free the rest */
		iid = l->data;
		if (iid->object != CORBA_OBJECT_NIL) 
			bonobo_object_release_unref (iid->object, NULL);
	}

	g_list_free (useable_importers);
	useable_importers = NULL;
}

static GList *
get_intelligent_importers (void)
{
	Bonobo_ServerInfoList *info_list;
	GList *iids_ret = NULL;
	CORBA_Environment ev;
	char *query;
	int i;

	CORBA_exception_init (&ev);
	query = g_strdup_printf ("repo_ids.has ('IDL:GNOME/Evolution/IntelligentImporter:%s')", BASE_VERSION);
	info_list = bonobo_activation_query (query, NULL, &ev);
	g_free (query);

	if (BONOBO_EX (&ev) || info_list == CORBA_OBJECT_NIL) {
		g_warning ("Cannot find importers -- %s", BONOBO_EX_REPOID (&ev));
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	for (i = 0; i < info_list->_length; i++) {
		const Bonobo_ServerInfo *info;

		info = info_list->_buffer + i;
		iids_ret = g_list_prepend (iids_ret, g_strdup (info->iid));
	}

	return iids_ret;
}

static void
init_importers ()
{
	GList *importer_ids, *l;

	if (useable_importers_init)
		return;
	
	useable_importers_init = TRUE;
	
	importer_ids = get_intelligent_importers ();
	if (!importer_ids)
		return;

	for (l = importer_ids; l; l = l->next) {
		CORBA_Environment ev;
		CORBA_Object object;
		Bonobo_Control control;
		char *iid = l->data;
		char *name, *blurb;
		IntelligentImporterData *id;
		gboolean can_run;

		CORBA_exception_init (&ev);
		object = bonobo_activation_activate_from_id (iid, 0, NULL, &ev);
		if (BONOBO_EX (&ev)) {
			g_warning ("Could not start %s:%s", iid, CORBA_exception_id (&ev));

			CORBA_exception_free (&ev);
			continue;
		}

		if (object == CORBA_OBJECT_NIL) {
			g_warning ("Could not activate component %s", iid);

			CORBA_exception_free (&ev);
			continue;
		}

		can_run = GNOME_Evolution_IntelligentImporter_canImport (object, &ev);
		if (BONOBO_EX (&ev)) {
			g_warning ("Could not call canImport(%s): %s", iid, CORBA_exception_id (&ev));

			bonobo_object_release_unref (object, &ev);
			CORBA_exception_free (&ev);
			continue;
		}
		
		if (can_run == FALSE) {
			bonobo_object_release_unref (object, &ev);
			CORBA_exception_free (&ev);
			continue;
		}

		name = GNOME_Evolution_IntelligentImporter__get_importername (object, &ev);
		if (BONOBO_EX (&ev)) {
			g_warning ("Could not get name(%s): %s", iid, CORBA_exception_id (&ev));

			bonobo_object_release_unref (object, &ev);
			CORBA_exception_free (&ev);
			continue;
		}

		blurb = GNOME_Evolution_IntelligentImporter__get_message (object, &ev);
		if (BONOBO_EX (&ev)) {
			g_warning ("Could not get message(%s): %s", iid, CORBA_exception_id (&ev));

			bonobo_object_release_unref (object, &ev);
			CORBA_exception_free (&ev);
			CORBA_free (name);
			continue;
		}

		control = Bonobo_Unknown_queryInterface (object, "IDL:Bonobo/Control:1.0", &ev);
		if (BONOBO_EX (&ev)) {
			g_warning ("Could not QI for Bonobo/Control:1.0 %s:%s", iid, CORBA_exception_id (&ev));

			bonobo_object_release_unref (object, &ev);
			CORBA_exception_free (&ev);
			CORBA_free (name);
			CORBA_free (blurb);
			continue;
		}

		if (control == CORBA_OBJECT_NIL) {
			g_warning ("Could not get importer control for %s", iid);

			bonobo_object_release_unref (object, &ev);
			CORBA_exception_free (&ev);
			CORBA_free (name);
			CORBA_free (blurb);
			continue;
		}

		CORBA_exception_free (&ev);

		id = g_new0 (IntelligentImporterData, 1);
		id->iid = g_strdup (iid);
		id->object = object;
		id->name = name;
		id->blurb = blurb;
		id->control = control;

		useable_importers = g_list_prepend (useable_importers, id);
	}
}

static void
startup_wizard_delete () {
	free_importers ();

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
	GtkWidget *page, *label, *sep, *table;
	GList *l;
	int i;
	
	init_importers ();
	if (!useable_importers)
		return NULL;
	
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
}

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

void
startup_wizard_commit (EPlugin *ep, EMConfigTargetAccount *target)
{
	EConfig *ec = ((EConfigTarget *)target)->config;
	ETimezoneDialog *etd;
	icaltimezone *zone;

	/* This frees the importers */
	do_import ();
	
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

	free_importers ();
	
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
