/*
 * e-shell-startup-wizard.c
 *
 * Copyright (C) 2001, 2002 Ximian, Inc.
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
 * Authors: Iain Holmes <iain@ximian.com>
 */

/* WARNING: I LEAK. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-startup-wizard.h"
#include "e-shell-corba-icon-utils.h"

#include "e-timezone-dialog/e-timezone-dialog.h"
#include "e-util/e-gtk-utils.h"
#include <e-util/e-icon-factory.h>
#include <gconf/gconf-client.h>

#include <evolution-wizard.h>
#include "Evolution.h"
#include "importer/GNOME_Evolution_Importer.h"

#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <glade/glade.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>

#include <bonobo-activation/bonobo-activation.h>

#include <gal/widgets/e-gui-utils.h>

#include <widgets/e-timezone-dialog/e-timezone-dialog.h>

typedef struct _TimezoneDialogPage {
	GtkWidget *page;
	GtkWidget *vbox;
	GObject *etd;

	gboolean prepared;
} TimezoneDialogPage;

typedef struct _ImportDialogPage {
	GtkWidget *page;
	GtkWidget *vbox;

	GList *importers;
	
	int running;
	gboolean prepared;
} ImportDialogPage;

typedef struct _SWData {
	GladeXML *wizard;
	GtkWidget *dialog;
	GtkWidget *druid;

	GtkWidget *start, *finish;
	GPtrArray *corba_pages;
	TimezoneDialogPage *timezone_page;
	ImportDialogPage *import_page;
	
	gboolean cancel;
	GNOME_Evolution_Wizard corba_wizard;
	BonoboListener *listener;
	GNOME_Evolution_Wizard_PageList *pagelist;
} SWData;

typedef struct _IntelligentImporterData {
	CORBA_Object object;
	Bonobo_Control control;
	GtkWidget *widget;

	char *name;
	char *blurb;
	char *iid;
} IntelligentImporterData;

typedef struct _SelectedImporterData{
	CORBA_Object importer;
	char *iid;
} SelectedImporterData;

static GHashTable *page_hash;
static GList *page_list = NULL;

static void
druid_event_notify_cb (BonoboListener *listener,
		       const char *name,
		       BonoboArg *arg,
		       CORBA_Environment *ev,
		       SWData *data)
{
	int buttons, pagenum;
	GnomeDruidPage *page;

	if (strcmp (name, EVOLUTION_WIZARD_SET_BUTTONS_SENSITIVE) == 0) {
		buttons = (int) *((CORBA_short *)arg->_value);
		gnome_druid_set_buttons_sensitive (GNOME_DRUID (data->druid),
						   (buttons & 4) >> 2,
						   (buttons & 2) >> 1,
						   (buttons & 1),
						   FALSE);
	} else if (strcmp (name, EVOLUTION_WIZARD_SET_SHOW_FINISH) == 0) {
		gnome_druid_set_show_finish (GNOME_DRUID (data->druid),
					     (gboolean) *((CORBA_boolean *) arg->_value));
	} else if (strcmp (name, EVOLUTION_WIZARD_SET_PAGE) == 0) {
		pagenum = (int) *((CORBA_short *) arg->_value);

		page = g_list_nth_data (page_list, pagenum);
		gnome_druid_set_page (GNOME_DRUID (data->druid), page);
	}
}

static GNOME_Evolution_Wizard
start_wizard (void)
{
	char *const selection_order[] = { "0-evolution:startup_wizard:priority", NULL };
	Bonobo_ServerInfoList *info;
	CORBA_Environment ev;
	GNOME_Evolution_Wizard wizard;
	char *query;
	int i;

	CORBA_exception_init (&ev);
	query = g_strdup_printf ("repo_ids.has ('IDL:GNOME/Evolution/StartupWizard:%s')", BASE_VERSION);
	info = bonobo_activation_query (query, selection_order, &ev);
	g_free (query);

	if (BONOBO_EX (&ev) || info == CORBA_OBJECT_NIL) {
		g_warning ("Cannot find startup wizard -- %s", BONOBO_EX_REPOID (&ev));
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	for (i = 0; i < info->_length; i++) {
		CORBA_exception_init (&ev);
		wizard = bonobo_activation_activate_from_id (info->_buffer[i].iid, 0, NULL, &ev);
		if (!BONOBO_EX (&ev) && wizard != CORBA_OBJECT_NIL) {
			CORBA_free (info);
			return wizard;
		}
		CORBA_exception_free (&ev);
	}

	if (info->_length)
		g_warning ("Could not start any startup wizard!");
	else
		g_warning ("No startup wizard available!");
	CORBA_free (info);
	return NULL;
}

static void
make_corba_dialog_pages (SWData *data)
{
	CORBA_Environment ev;
	CORBA_Object object;
	Bonobo_EventSource event_source;

	data->corba_wizard = start_wizard ();
	if (!data->corba_wizard)
		return;

	CORBA_exception_init (&ev);
	data->pagelist = GNOME_Evolution_Wizard__get_pages (data->corba_wizard, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Could not get CORBA wizard pages (%s)", CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		data->pagelist = CORBA_OBJECT_NIL;
		return;
	}

	CORBA_exception_init (&ev);
	event_source = Bonobo_Unknown_queryInterface (data->corba_wizard, "IDL:Bonobo/EventSource:1.0", &ev);
	CORBA_exception_free (&ev);
	data->listener = bonobo_listener_new (NULL, NULL);
	g_signal_connect (data->listener, "event-notify",
			  G_CALLBACK (druid_event_notify_cb), data);
	object = bonobo_object_corba_objref (BONOBO_OBJECT (data->listener));

	CORBA_exception_init (&ev);
	Bonobo_EventSource_addListener (event_source, object, &ev);
	CORBA_exception_free (&ev);
	bonobo_object_release_unref (event_source, &ev);
	CORBA_exception_free (&ev);
}

static int
page_to_num (GnomeDruidPage *page)
{
	return GPOINTER_TO_INT (g_hash_table_lookup (page_hash, page));
}

static gboolean
next_func (GnomeDruidPage *page,
	   GnomeDruid *druid,
	   SWData *data)
{
	CORBA_Environment ev;
	int pagenum;

	CORBA_exception_init (&ev);
	pagenum = page_to_num (page);
	GNOME_Evolution_Wizard_notifyAction (data->corba_wizard, pagenum, GNOME_Evolution_Wizard_NEXT, &ev);
	CORBA_exception_free (&ev);

	/* If on last page we own, let druid goto next page */
	if (pagenum == g_list_length(page_list)-1)
		return FALSE;

	return TRUE;
}

static gboolean
prepare_func (GnomeDruidPage *page,
	      GnomeDruid *druid,
	      SWData *data)
{
	CORBA_Environment ev;
	int pagenum;

	CORBA_exception_init (&ev);
	pagenum = page_to_num (page);
	GNOME_Evolution_Wizard_notifyAction (data->corba_wizard, pagenum, GNOME_Evolution_Wizard_PREPARE, &ev);
	CORBA_exception_free (&ev);
	return FALSE;
}

static gboolean
back_func (GnomeDruidPage *page,
	   GnomeDruid *druid,
	   SWData *data)
{
	CORBA_Environment ev;
	int pagenum;

	CORBA_exception_init (&ev);
	pagenum = page_to_num (page);
	GNOME_Evolution_Wizard_notifyAction (data->corba_wizard, pagenum, GNOME_Evolution_Wizard_BACK, &ev);
	CORBA_exception_free (&ev);

	/* if we're on page 0, let the druid go back to the start page, if we have one */
	if (pagenum == 0)
		return FALSE;

	return TRUE;
}

static void
free_importers (SWData *data)
{
	GList *l;

	for (l = data->import_page->importers; l; l = l->next) {
		IntelligentImporterData *iid;

		iid = l->data;
		if (iid->object != CORBA_OBJECT_NIL) {
			bonobo_object_release_unref (iid->object, NULL);
		}
	}

	g_list_free (data->import_page->importers);
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
do_import (SWData *data)
{
	CORBA_Environment ev;
	GList *l, *selected = NULL;

	for (l = data->import_page->importers; l; l = l->next) {
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

	free_importers (data);

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
				
static gboolean
finish_func (GnomeDruidPage *page,
	     GnomeDruid *druid,
	     SWData *data)
{
	GConfClient *client;
	CORBA_Environment ev;
	icaltimezone *zone;

	/* Notify mailer */
	CORBA_exception_init (&ev);
	GNOME_Evolution_Wizard_notifyAction (data->corba_wizard, 0, GNOME_Evolution_Wizard_FINISH, &ev);
	CORBA_exception_free (&ev);

	/* Set Timezone */
	zone = e_timezone_dialog_get_timezone (E_TIMEZONE_DIALOG (data->timezone_page->etd));
	if (zone) {
		client = gconf_client_get_default ();
		gconf_client_set_string (client, "/apps/evolution/calendar/display/timezone",
					 icaltimezone_get_display_name (zone), NULL);
		g_object_unref (client);
	}

	do_import (data);

	/* Free data */
	data->cancel = FALSE;

	/* Need to do this otherwise the timezone widget gets destroyed but the
	   timezone object isn't, and we can get a crash like #22047.  */
	g_object_unref (data->timezone_page->etd);
	data->timezone_page->etd = NULL;

	gtk_widget_destroy (data->dialog);
	gtk_main_quit ();

	return TRUE;
}

static gboolean
back_finish_func (GnomeDruidPage *page,
	     GnomeDruid *druid,
	     SWData *data)
{
	if (data->import_page->running == 0)
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->timezone_page->page));
	else
		return FALSE;

	return TRUE;
}

static void
connect_page (GtkWidget *page,
	      SWData *data)
{
	g_signal_connect (page, "next",
			  G_CALLBACK (next_func), data);
	g_signal_connect (page, "back",
			  G_CALLBACK (back_func), data);
	g_signal_connect (page, "finish",
			  G_CALLBACK (finish_func), data);

	/* At least in 2.0 (and probably 2.2 too),
	 * GnomeDruidPageStandard is broken and you need to
	 * connect_after to "prepare" or else its default method will
	 * run after your signal handler and undo its button
	 * sensitivity changes.
	 */
	g_signal_connect_after (page, "prepare",
				G_CALLBACK (prepare_func), data);
}

static GtkWidget *
make_corba_page (SWData *data, int n, GtkWidget *prev)
{
	GNOME_Evolution_Wizard_Page *corba_page;
	GdkPixbuf *icon;
	GtkWidget *page, *body;

	corba_page = &data->pagelist->_buffer[n];
	icon = e_new_gdk_pixbuf_from_corba_icon (&corba_page->icon, -1, -1);
	page = gnome_druid_page_standard_new_with_vals (corba_page->title,
							icon, NULL);
	g_object_unref (icon);

	body = bonobo_widget_new_control_from_objref (corba_page->control,
						      CORBA_OBJECT_NIL);
	gtk_box_pack_start (GTK_BOX (GNOME_DRUID_PAGE_STANDARD (page)->vbox),
			    body, TRUE, TRUE, 0);
	gtk_widget_show_all (page);

	gnome_druid_insert_page (GNOME_DRUID (data->druid),
				 GNOME_DRUID_PAGE (prev),
				 GNOME_DRUID_PAGE (page));
	connect_page (page, data);
	g_hash_table_insert (page_hash, page, GINT_TO_POINTER (n));
	page_list = g_list_append (page_list, page);

	return page;
}

static gboolean
prepare_timezone_page (GnomeDruidPage *page,
		       GnomeDruid *druid,
		       SWData *data)
{
	GConfClient *client;
	icaltimezone *zone;
	char *zone_name;

	if (data->timezone_page->prepared)
		return TRUE;
	data->timezone_page->prepared = TRUE;

	client = gconf_client_get_default ();
	zone_name = gconf_client_get_string (client, "/apps/evolution/calendar/display/timezone", NULL);
	g_object_unref (client);

	if (!zone_name)
		return TRUE;

	zone = icaltimezone_get_builtin_timezone (zone_name);
	g_free (zone_name);

	if (zone)
		e_timezone_dialog_set_timezone (E_TIMEZONE_DIALOG (data->timezone_page->etd), zone);

	return TRUE;
}

static TimezoneDialogPage *
make_timezone_page (SWData *data)
{
	TimezoneDialogPage *page;
	ETimezoneDialog *etd;
	GdkPixbuf *pixbuf;
	
	g_return_val_if_fail (data != NULL, NULL);

	page = g_new0 (TimezoneDialogPage, 1);
	page->page = glade_xml_get_widget (data->wizard, "timezone-page");
	g_return_val_if_fail (page->page != NULL, NULL);

	pixbuf = e_icon_factory_get_icon ("stock_timezone", 48);
	gnome_druid_page_standard_set_logo (GNOME_DRUID_PAGE_STANDARD (page->page), pixbuf);
	g_object_unref (pixbuf);

	g_signal_connect_after (page->page, "prepare",
				G_CALLBACK (prepare_timezone_page), data);
	page->vbox = GTK_WIDGET (GNOME_DRUID_PAGE_STANDARD (page->page)->vbox);

	etd = e_timezone_dialog_new ();
	page->etd = G_OBJECT (etd);
	e_timezone_dialog_reparent (E_TIMEZONE_DIALOG (page->etd), page->vbox);

	return page;
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

static gboolean
prepare_importer_page (GnomeDruidPage *page,
		       GnomeDruid *druid,
		       SWData *data)
{
	GtkWidget *dialog;
	ImportDialogPage *import;
	GList *l, *importers;
	GtkWidget *table;
	int running = 0;

	if (data->import_page->prepared == TRUE) {
		if (data->import_page->running == 0)
			gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->finish));
		return TRUE;
	}

	data->import_page->prepared = TRUE;

	dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_INFO,
					 GTK_BUTTONS_NONE,
					 _("Please wait...\nScanning for existing setups"));
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	e_make_widget_backing_stored (dialog);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Starting import"));
	gtk_widget_show_all (dialog);
	gtk_widget_show_now (dialog);

	gtk_widget_queue_draw (dialog);
	gdk_flush ();

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}

	import = data->import_page;
	importers = get_intelligent_importers ();
	if (importers == NULL) {
		/* No importers, go directly to finish, do not pass go
		   Do not collect $200 */
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->finish));
		gtk_widget_destroy (dialog);
		return TRUE;
	}

	table = gtk_table_new (g_list_length (importers), 2, FALSE);
	for (l = importers; l; l = l->next) {
		GtkWidget *label;
		CORBA_Environment ev;
		gboolean can_run;
		char *str;
		IntelligentImporterData *id;

		id = g_new0 (IntelligentImporterData, 1);
		id->iid = g_strdup (l->data);

		CORBA_exception_init (&ev);
		id->object = bonobo_activation_activate_from_id ((char *) id->iid, 0, NULL, &ev);
		if (BONOBO_EX (&ev)) {
			g_warning ("Could not start %s:%s", id->iid,
				   CORBA_exception_id (&ev));

			CORBA_exception_free (&ev);
			/* Clean up the IID */
			g_free (id->iid);
			g_free (id);
			continue;
		}

		if (id->object == CORBA_OBJECT_NIL) {
			g_warning ("Could not activate component %s", id->iid);
			CORBA_exception_free (&ev);

			g_free (id->iid);
			g_free (id);
			continue;
		}

		can_run = GNOME_Evolution_IntelligentImporter_canImport (id->object, &ev);
		if (BONOBO_EX (&ev)) {
			g_warning ("Could not call canImport(%s): %s", id->iid,
				   CORBA_exception_id (&ev));
			bonobo_object_release_unref (id->object, &ev);
			CORBA_exception_free (&ev);

			g_free (id->iid);
			g_free (id);
			continue;
		}

		if (can_run == FALSE) {
			bonobo_object_release_unref (id->object, &ev);
			CORBA_exception_free (&ev);
			g_free (id->iid);
			g_free (id);
			continue;
		}

		running++;
		id->name = GNOME_Evolution_IntelligentImporter__get_importername (id->object, &ev);
		if (BONOBO_EX (&ev)) {
			g_warning ("Could not get name(%s): %s", id->iid,
				   CORBA_exception_id (&ev));
			bonobo_object_release_unref (id->object, &ev);
			CORBA_exception_free (&ev);
			g_free (id->iid);
			g_free (id);
			continue;
		}

		id->blurb = GNOME_Evolution_IntelligentImporter__get_message (id->object, &ev);
		if (BONOBO_EX (&ev)) {
			g_warning ("Could not get message(%s): %s",
				   id->iid, CORBA_exception_id (&ev));
			bonobo_object_release_unref (id->object, &ev);
			CORBA_exception_free (&ev);
			g_free (id->iid);
			CORBA_free (id->name);
			g_free (id);
			continue;
		}

		id->control = Bonobo_Unknown_queryInterface (id->object,
							     "IDL:Bonobo/Control:1.0", &ev);
		if (BONOBO_EX (&ev)) {
			g_warning ("Could not QI for Bonobo/Control:1.0 %s:%s",
				   id->iid, CORBA_exception_id (&ev));
			bonobo_object_release_unref (id->object, &ev);
			CORBA_exception_free (&ev);
			g_free (id->iid);
			CORBA_free (id->name);
			CORBA_free (id->blurb);
			continue;
		}

		if (id->control != CORBA_OBJECT_NIL) {
			id->widget = bonobo_widget_new_control_from_objref (id->control, CORBA_OBJECT_NIL);
			gtk_widget_show (id->widget);
		} else {
			printf("no control\n");
			id->widget = gtk_label_new ("");
			gtk_widget_show (id->widget);
		}

		CORBA_exception_free (&ev);

		import->importers = g_list_prepend (import->importers, id);
		str = g_strdup_printf (_("From %s:"), id->name);
		label = gtk_label_new (str);
		g_free (str);

		gtk_misc_set_alignment (GTK_MISC (label), 0, .5); 

		gtk_table_attach (GTK_TABLE (table), label, 0, 1, running - 1,
				  running, GTK_FILL, 0, 0, 0);
		gtk_table_attach (GTK_TABLE (table), id->widget, 1, 2,
				  running - 1, running, GTK_FILL, 0, 3, 0);
		gtk_widget_show_all (table);

		gtk_box_pack_start (GTK_BOX (data->import_page->vbox), table,
				    FALSE, FALSE, 0);
	}

	if (running == 0) {
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->finish));
		gtk_widget_destroy (dialog);
		return TRUE;
	}

	import->running = running;
	gtk_widget_destroy (dialog);

	return FALSE;
}

static ImportDialogPage *
make_importer_page (SWData *data)
{
	ImportDialogPage *page;
	GtkWidget *label, *sep;
	GdkPixbuf *pixbuf;
	
	g_return_val_if_fail (data != NULL, NULL);

	page = g_new0 (ImportDialogPage, 1);
	page->page = glade_xml_get_widget (data->wizard, "import-page");
	g_return_val_if_fail (page->page != NULL, NULL);

	pixbuf = e_icon_factory_get_icon ("stock_mail-import", 48);
	gnome_druid_page_standard_set_logo (GNOME_DRUID_PAGE_STANDARD (page->page), pixbuf);
	g_object_unref (pixbuf);

	g_signal_connect_after (page->page, "prepare",
				G_CALLBACK (prepare_importer_page), data);
	page->vbox = GNOME_DRUID_PAGE_STANDARD (page->page)->vbox;
	gtk_container_set_border_width (GTK_CONTAINER (page->vbox), 4);

	label = gtk_label_new (_("Please select the information that you would like to import:"));
	gtk_box_pack_start (GTK_BOX (page->vbox), label, FALSE, FALSE, 3);

	sep = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (page->vbox), sep, FALSE, FALSE, 3);

	page->prepared = FALSE;
	return page;
}

static void
startup_wizard_cancel (GnomeDruid *druid,
		       SWData *data)
{
	/* Free data */
	data->cancel = TRUE;

	if (data->timezone_page->etd != NULL) {
		/* Need to do this otherwise the timezone widget gets destroyed but the
		   timezone object isn't, and we can get a crash like #22047.  */
		g_object_unref (data->timezone_page->etd);
		data->timezone_page->etd = NULL;
	}

	gtk_widget_destroy (data->dialog);
	gtk_main_quit ();
}

static gboolean
startup_wizard_delete (GnomeDruid *druid,
		       GdkEventAny *ev,
	       	       SWData *data)
{
	startup_wizard_cancel(druid, data);
	return FALSE;
}

static gboolean
key_press_event_callback (GtkWidget   *widget,
			  GdkEventKey *keyev,
			  SWData      *data)
{
	if (keyev->keyval == GDK_Escape) {
		GtkWidget *confirm_dialog;
		int returnvalue;
		char *confirmations;

		confirmations = _("If you quit the Evolution Setup Assistant now, all of the information that "
				  "you have entered will be forgotten. You will need to run this assistant again "
				  "before using Evolution.\n\nDo you want to quit using the Assistant now?");
 
		confirm_dialog = gtk_message_dialog_new (GTK_WINDOW (data->dialog),
							 GTK_DIALOG_MODAL,
							 GTK_MESSAGE_WARNING,
							 GTK_BUTTONS_NONE,
							 confirmations);

		gtk_dialog_add_button (GTK_DIALOG (confirm_dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
		gtk_dialog_add_button (GTK_DIALOG (confirm_dialog), GTK_STOCK_QUIT, GTK_RESPONSE_OK);
		gtk_dialog_set_has_separator (GTK_DIALOG (confirm_dialog),
					      FALSE);

		returnvalue = gtk_dialog_run (GTK_DIALOG (confirm_dialog));
		gtk_widget_destroy (confirm_dialog);
 
		if (returnvalue == GTK_RESPONSE_OK) {
		        startup_wizard_cancel ((GnomeDruid *)data->druid, data);
			return TRUE;
		}
	}
	return FALSE;
}

gboolean
e_shell_startup_wizard_create (void)
{
	GConfClient *client;
	SWData *data;
	GSList *accounts;
	GtkWidget *page;
	int i;

	client = gconf_client_get_default ();
	accounts = gconf_client_get_list (client, "/apps/evolution/mail/accounts", GCONF_VALUE_STRING, NULL);
	g_object_unref (client);

	if (accounts != NULL) {
		g_slist_foreach (accounts, (GFunc) g_free, NULL);
		g_slist_free (accounts);
		return TRUE;
	}

	data = g_new0 (SWData, 1);

	data->wizard = glade_xml_new (EVOLUTION_GLADEDIR "/evolution-startup-wizard.glade", NULL, NULL);
	g_return_val_if_fail (data->wizard != NULL, FALSE);
	data->dialog = glade_xml_get_widget (data->wizard, "startup-wizard");
	g_return_val_if_fail (data->dialog != NULL, FALSE);
	gtk_window_set_wmclass (GTK_WINDOW (data->dialog), "startup-wizard",
				"Evolution:shell");

	g_signal_connect_after (data->dialog, "delete_event",
			  G_CALLBACK (startup_wizard_delete), data);

	page_hash = g_hash_table_new (NULL, NULL);
	data->druid = glade_xml_get_widget (data->wizard, "startup-druid");
	g_return_val_if_fail (data->druid != NULL, FALSE);
	gnome_druid_set_buttons_sensitive (GNOME_DRUID (data->druid),
					   FALSE, TRUE, FALSE, FALSE);

	g_signal_connect (data->druid, "cancel",
			  G_CALLBACK (startup_wizard_cancel), data);

	g_signal_connect (data->dialog, "key_press_event",
			  G_CALLBACK (key_press_event_callback), data);

	data->start = glade_xml_get_widget (data->wizard, "start-page");
	data->finish = glade_xml_get_widget (data->wizard, "done-page");
	g_return_val_if_fail (data->start != NULL, FALSE);
	g_return_val_if_fail (data->finish != NULL, FALSE);
	g_signal_connect (data->finish, "finish", G_CALLBACK (finish_func), data);
	g_signal_connect (data->finish, "back", G_CALLBACK (back_finish_func), data);

	make_corba_dialog_pages (data);
	g_return_val_if_fail (data->corba_wizard != CORBA_OBJECT_NIL, TRUE);
	g_return_val_if_fail (data->pagelist != CORBA_OBJECT_NIL, TRUE);

	page = data->start;
	for (i = 0; i < data->pagelist->_length; i++)
		page = make_corba_page (data, i, page);

	data->timezone_page = make_timezone_page (data);
	data->import_page = make_importer_page (data);

	g_return_val_if_fail (data->timezone_page != NULL, TRUE);
	g_return_val_if_fail (data->import_page != NULL, TRUE);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (data->druid), FALSE, TRUE, TRUE, FALSE);
	gtk_widget_show_all (data->dialog);

	gtk_main ();

	return !data->cancel;
}
