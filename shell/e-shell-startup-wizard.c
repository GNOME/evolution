/*
 * e-shell-startup-wizard.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <glade/glade.h>
#include <liboaf/liboaf.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo-conf/bonobo-config-database.h>

#include <widgets/e-timezone-dialog/e-timezone-dialog.h>
#include "importer/GNOME_Evolution_Importer.h"

#include <evolution-wizard.h>
#include "Evolution.h"

typedef struct _TimezoneDialogPage {
	GtkWidget *page;
	GtkWidget *vbox;
	GtkObject *etd;
} TimezoneDialogPage;

typedef struct _ImportDialogPage {
	GtkWidget *page;
	GtkWidget *vbox;
	GtkWidget *placeholder, *clist;

	GList *importers;
	
int running;
} ImportDialogPage;

typedef struct _MailDialogPage {
	GtkWidget *page;
	GtkWidget *vbox;
	GtkWidget *widget;
	
	Bonobo_Control control;
} MailDialogPage;

typedef struct _SWData {
	GladeXML *wizard;
	GtkWidget *dialog;
	GtkWidget *druid;

	GtkWidget *start, *finish;

	MailDialogPage *id_page;
	MailDialogPage *source_page;
	MailDialogPage *extra_page;
	MailDialogPage *transport_page;
	MailDialogPage *management_page;

	TimezoneDialogPage *timezone_page;
	ImportDialogPage *import_page;
	
	gboolean cancel;
	CORBA_Object mailer;
	Bonobo_EventSource event_source;
	BonoboListener *listener;
	int id;

	Bonobo_ConfigDatabase db;
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

static void
druid_event_notify_cb (BonoboListener *listener,
		       const char *name,
		       BonoboArg *arg,
		       CORBA_Environment *ev,
		       SWData *data)
{
	int buttons;

	if (strcmp (name, EVOLUTION_WIZARD_SET_BUTTONS_SENSITIVE) == 0) {
		buttons = (int) *((CORBA_short *)arg->_value);
		gnome_druid_set_buttons_sensitive (GNOME_DRUID (data->druid),
						   (buttons & 4) >> 2,
						   (buttons & 2) >> 1,
						   (buttons & 1));
	} else {
		g_print ("event_name: %s\n", name);
	}
}

static void
make_mail_dialog_pages (SWData *data)
{
	CORBA_Environment ev;
	CORBA_Object object;

	CORBA_exception_init (&ev);
	data->mailer = oaf_activate_from_id ("OAFIID:GNOME_Evolution_Mail_Wizard", 0, NULL, &ev);
	if (BONOBO_EX (&ev)) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Could not start the Evolution Mailer Wizard interface\n%s"), CORBA_exception_id (&ev));
		g_warning ("Could not start mailer (%s)", CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);
	if (data->mailer == CORBA_OBJECT_NIL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize the Evolution Mailer Wizard interface"));
		return;
	}

	CORBA_exception_init (&ev);
	data->event_source = Bonobo_Unknown_queryInterface (data->mailer, "IDL:Bonobo/EventSource:1.0", &ev);
	CORBA_exception_free (&ev);
	data->listener = bonobo_listener_new (NULL, NULL);
	gtk_signal_connect (GTK_OBJECT (data->listener), "event-notify",
			    GTK_SIGNAL_FUNC (druid_event_notify_cb), data);
	object = bonobo_object_corba_objref (BONOBO_OBJECT (data->listener));
	CORBA_exception_init (&ev);
	data->id = Bonobo_EventSource_addListener (data->event_source, object, &ev);
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
	GNOME_Evolution_Wizard_notifyAction (data->mailer, pagenum, GNOME_Evolution_Wizard_NEXT, &ev);
	CORBA_exception_free (&ev);
	return FALSE;
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
	GNOME_Evolution_Wizard_notifyAction (data->mailer, pagenum, GNOME_Evolution_Wizard_PREPARE, &ev);
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
	GNOME_Evolution_Wizard_notifyAction (data->mailer, pagenum, GNOME_Evolution_Wizard_BACK, &ev);
	CORBA_exception_free (&ev);
	return FALSE;
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

	for (l = GTK_CLIST (data->import_page->clist)->selection; l; l = l->next) {
		IntelligentImporterData *importer_data;
		SelectedImporterData *sid;
		char *iid;

		importer_data = g_list_nth_data (data->import_page->importers, GPOINTER_TO_INT (l->data));
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
	CORBA_Environment ev;
	char *displayname, *tz;

	/* Notify mailer */
	CORBA_exception_init (&ev);
	GNOME_Evolution_Wizard_notifyAction (data->mailer, 0, GNOME_Evolution_Wizard_FINISH, &ev);
	CORBA_exception_free (&ev);

	/* Set Timezone */
	CORBA_exception_init (&ev);

	e_timezone_dialog_get_timezone (E_TIMEZONE_DIALOG (data->timezone_page->etd), &displayname);
	if (displayname == NULL)
		tz = g_strdup ("");
	else
		tz = g_strdup (displayname);
	
	bonobo_config_set_string (data->db, "/Calendar/Display/Timezone", tz, &ev);
	g_free (tz);
	CORBA_exception_free (&ev);

	do_import (data);

	/* Free data */
	data->cancel = FALSE;
	gtk_widget_destroy (data->dialog);
	gtk_main_quit ();

	return TRUE;
}

static void
connect_page (GtkWidget *page,
	      SWData *data)
{
	gtk_signal_connect (GTK_OBJECT (page), "next",
			    GTK_SIGNAL_FUNC (next_func), data);
	gtk_signal_connect (GTK_OBJECT (page), "prepare",
			    GTK_SIGNAL_FUNC (prepare_func), data);
	gtk_signal_connect (GTK_OBJECT (page), "back",
			    GTK_SIGNAL_FUNC (back_func), data);
	gtk_signal_connect (GTK_OBJECT (page), "finish",
			    GTK_SIGNAL_FUNC (finish_func), data);
}

static MailDialogPage *
make_identity_page (SWData *data)
{
	MailDialogPage *page;
	CORBA_Environment ev;

	g_return_val_if_fail (data != NULL, NULL);
	g_return_val_if_fail (data->mailer != CORBA_OBJECT_NIL, NULL);
	
	page = g_new0 (MailDialogPage, 1);
	page->page = glade_xml_get_widget (data->wizard, "identity-page");
	g_return_val_if_fail (page->page != NULL, NULL);

	connect_page (page->page, data);
	g_hash_table_insert (page_hash, page->page, GINT_TO_POINTER (0));
	page->vbox = GNOME_DRUID_PAGE_STANDARD (page->page)->vbox;

	CORBA_exception_init (&ev);
	page->control = GNOME_Evolution_Wizard_getControl (data->mailer, 0, &ev);
	CORBA_exception_free (&ev);

	page->widget = bonobo_widget_new_control_from_objref (page->control, CORBA_OBJECT_NIL);
	gtk_box_pack_start (GTK_BOX (page->vbox), page->widget, TRUE, TRUE, 0);
	gtk_widget_show_all (page->widget);

	return page;
}

static MailDialogPage *
make_receive_page (SWData *data)
{
	MailDialogPage *page;
	CORBA_Environment ev;

	g_return_val_if_fail (data != NULL, NULL);
	g_return_val_if_fail (data->mailer != CORBA_OBJECT_NIL, NULL);

	page = g_new0 (MailDialogPage, 1);
	page->page = glade_xml_get_widget (data->wizard, "receive-page");
	g_return_val_if_fail (page->page != NULL, NULL);

	connect_page (page->page, data);
	g_hash_table_insert (page_hash, page->page, GINT_TO_POINTER (1));
	page->vbox = GNOME_DRUID_PAGE_STANDARD (page->page)->vbox;

	CORBA_exception_init (&ev);
	page->control = GNOME_Evolution_Wizard_getControl (data->mailer, 1, &ev);
	CORBA_exception_free (&ev);

	page->widget = bonobo_widget_new_control_from_objref (page->control, CORBA_OBJECT_NIL);
	gtk_box_pack_start (GTK_BOX (page->vbox), page->widget, TRUE, TRUE, 0);
	gtk_widget_show_all (page->widget);

	return page;
}

static MailDialogPage *
make_extra_page (SWData *data)
{
	MailDialogPage *page;
	CORBA_Environment ev;

	g_return_val_if_fail (data != NULL, NULL);
	g_return_val_if_fail (data->mailer != CORBA_OBJECT_NIL, NULL);

	page = g_new0 (MailDialogPage, 1);
	page->page = glade_xml_get_widget (data->wizard, "extra-page");
	g_return_val_if_fail (page->page != NULL, NULL);

	connect_page (page->page, data);
	g_hash_table_insert (page_hash, page->page, GINT_TO_POINTER (2));
	page->vbox = GNOME_DRUID_PAGE_STANDARD (page->page)->vbox;

	CORBA_exception_init (&ev);
	page->control = GNOME_Evolution_Wizard_getControl (data->mailer, 2, &ev);
	CORBA_exception_free (&ev);

	page->widget = bonobo_widget_new_control_from_objref (page->control, CORBA_OBJECT_NIL);
	gtk_box_pack_start (GTK_BOX (page->vbox), page->widget, TRUE, TRUE, 0);
	gtk_widget_show_all (page->widget);

	return page;
}

static MailDialogPage *
make_transport_page (SWData *data)
{
	MailDialogPage *page;
	CORBA_Environment ev;

	g_return_val_if_fail (data != NULL, NULL);
	g_return_val_if_fail (data->mailer != CORBA_OBJECT_NIL, NULL);

	page = g_new0 (MailDialogPage, 1);
	page->page = glade_xml_get_widget (data->wizard, "send-page");
	g_return_val_if_fail (page->page != NULL, NULL);

	connect_page (page->page, data);
	g_hash_table_insert (page_hash, page->page, GINT_TO_POINTER (3));
	page->vbox = GNOME_DRUID_PAGE_STANDARD (page->page)->vbox;

	CORBA_exception_init (&ev);
	page->control = GNOME_Evolution_Wizard_getControl (data->mailer, 3, &ev);
	CORBA_exception_free (&ev);

	page->widget = bonobo_widget_new_control_from_objref (page->control, CORBA_OBJECT_NIL);
	gtk_box_pack_start (GTK_BOX (page->vbox), page->widget, TRUE, TRUE, 0);
	gtk_widget_show_all (page->widget);

	return page;
}

static MailDialogPage *
make_management_page (SWData *data)
{
	MailDialogPage *page;
	CORBA_Environment ev;

	g_return_val_if_fail (data != NULL, NULL);
	g_return_val_if_fail (data->mailer != CORBA_OBJECT_NIL, NULL);

	page = g_new0 (MailDialogPage, 1);
	page->page = glade_xml_get_widget (data->wizard, "management-page");
	g_return_val_if_fail (page->page != NULL, NULL);

	connect_page (page->page, data);
	g_hash_table_insert (page_hash, page->page, GINT_TO_POINTER (4));
	page->vbox = GNOME_DRUID_PAGE_STANDARD (page->page)->vbox;

	CORBA_exception_init (&ev);
	page->control = GNOME_Evolution_Wizard_getControl (data->mailer, 4, &ev);
	CORBA_exception_free (&ev);

	page->widget = bonobo_widget_new_control_from_objref (page->control, CORBA_OBJECT_NIL);
	gtk_box_pack_start (GTK_BOX (page->vbox), page->widget, TRUE, TRUE, 0);
	gtk_widget_show_all (page->widget);

	return page;
}

static TimezoneDialogPage *
make_timezone_page (SWData *data)
{
	TimezoneDialogPage *page;
	
	g_return_val_if_fail (data != NULL, NULL);

	page = g_new0 (TimezoneDialogPage, 1);
	page->page = glade_xml_get_widget (data->wizard, "timezone-page");
	g_return_val_if_fail (page->page != NULL, NULL);

	page->vbox = GTK_WIDGET (GNOME_DRUID_PAGE_STANDARD (page->page)->vbox);

	page->etd = GTK_OBJECT (e_timezone_dialog_new ());
	e_timezone_dialog_reparent (E_TIMEZONE_DIALOG (page->etd), page->vbox);

	return page;
}

static GList *
get_intelligent_importers (void)
{
	OAF_ServerInfoList *info_list;
	GList *iids_ret = NULL;
	CORBA_Environment ev;
	int i;

	CORBA_exception_init (&ev);
	info_list = oaf_query ("repo_ids.has ('IDL:GNOME/Evolution/IntelligentImporter:1.0')", NULL, &ev);
	CORBA_exception_free (&ev);

	for (i = 0; i < info_list->_length; i++) {
		const OAF_ServerInfo *info;

		info = info_list->_buffer + i;
		iids_ret = g_list_prepend (iids_ret, g_strdup (info->iid));
	}

	return iids_ret;
}

static void
dialog_mapped (GtkWidget *w,
	       gpointer data)
{
	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}
}

static gboolean
prepare_importer_page (GnomeDruidPage *page,

		       GnomeDruid *druid,
		       SWData *data)
{
	GtkWidget *dialog;
	ImportDialogPage *import;
	GList *l, *importers;
	GtkWidget *dummy;
	int running = 0;

	dialog = gnome_message_box_new (_("Please wait...\nScanning for existing setups"), GNOME_MESSAGE_BOX_INFO, NULL);
	gtk_signal_connect (GTK_OBJECT (dialog), "map",
			    GTK_SIGNAL_FUNC (dialog_mapped), NULL);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Starting Intelligent Importers"));
	gtk_widget_show_all (dialog);
	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}

	import = data->import_page;
	importers = get_intelligent_importers ();
	if (importers == NULL) {
		/* No importers, go directly to finish, do not pass go
		   Do not collect $200 */
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->finish))
;
		gtk_widget_destroy (dialog);
		return TRUE;
	}

	for (l = importers; l; l = l->next) {
		IntelligentImporterData *id;
		CORBA_Environment ev;
		gboolean can_run;
		char *text[1];
		
		id = g_new0 (IntelligentImporterData, 1);
		id->iid = g_strdup (l->data);

		CORBA_exception_init (&ev);
		id->object = oaf_activate_from_id ((char *) id->iid, 0, NULL, &ev);
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
			id->widget = gtk_label_new ("");
			gtk_widget_show (id->widget);
		}

		CORBA_exception_free (&ev);

		import->importers = g_list_prepend (import->importers, id);
		gtk_notebook_prepend_page (GTK_NOTEBOOK (import->placeholder),
					   id->widget, NULL);
		text[0] = id->name;
		gtk_clist_prepend (GTK_CLIST (import->clist), text);
	}

	if (running == 0) {
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->finish));
		gtk_widget_destroy (dialog);
		return TRUE;
	}

	import->running = running;
	dummy = gtk_drawing_area_new ();
	gtk_widget_show (dummy);
	gtk_notebook_append_page (GTK_NOTEBOOK (import->placeholder), dummy, NULL);
	/* Set the start to the blank page */
	gtk_notebook_set_page (GTK_NOTEBOOK (import->placeholder), running);

	gtk_widget_destroy (dialog);
	return FALSE;
}

static void
select_row_cb (GtkCList *clist,
	       int row,
	       int column,
	       GdkEvent *ev,
	       SWData *data)
{
	ImportDialogPage *page;

	page = data->import_page;
	gtk_notebook_set_page (GTK_NOTEBOOK (page->placeholder), row);
}

static void
unselect_row_cb (GtkCList *clist,
		 int row,
		 int column,
		 GdkEvent *ev,
		 SWData *data)
{
	ImportDialogPage *page;

	page = data->import_page;
	if (clist->selection == NULL) {
		gtk_notebook_set_page (GTK_NOTEBOOK (page->placeholder), page->running);
	} else {
		gtk_notebook_set_page (GTK_NOTEBOOK (page->placeholder), 
				       GPOINTER_TO_INT (clist->selection->data));
	}
}

static ImportDialogPage *
make_importer_page (SWData *data)
{
	ImportDialogPage *page;
	char *titles[1];
	GtkWidget *hbox, *sw;
	
	g_return_val_if_fail (data != NULL, NULL);

	page = g_new0 (ImportDialogPage, 1);
	page->page = glade_xml_get_widget (data->wizard, "import-page");
	g_return_val_if_fail (page->page != NULL, NULL);

	gtk_signal_connect (GTK_OBJECT (page->page), "prepare",
			    GTK_SIGNAL_FUNC (prepare_importer_page), data);
	page->vbox = GNOME_DRUID_PAGE_STANDARD (page->page)->vbox;

	titles[0] = ".";
	page->clist = gtk_clist_new_with_titles (1, titles);
	gtk_clist_column_titles_hide (GTK_CLIST (page->clist));
  	gtk_clist_set_selection_mode (GTK_CLIST (page->clist), GTK_SELECTION_MULTIPLE);
	gtk_signal_connect (GTK_OBJECT (page->clist), "select-row",
			    GTK_SIGNAL_FUNC (select_row_cb), data);
	gtk_signal_connect (GTK_OBJECT (page->clist), "unselect-row",
			    GTK_SIGNAL_FUNC (unselect_row_cb), data);

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
	gtk_box_pack_start (GTK_BOX (page->vbox), hbox, TRUE, TRUE, 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
  	gtk_widget_set_usize (sw, 300, 150); 
	gtk_container_add (GTK_CONTAINER (sw), page->clist);
	gtk_box_pack_start (GTK_BOX (hbox), sw, FALSE, FALSE, 0);

	page->placeholder = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (page->placeholder), FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), page->placeholder, TRUE, TRUE, 0);

	return page;
}

static void
startup_wizard_cancel (GnomeDruid *druid,
		       SWData *data)
{
	/* Free data */
	data->cancel = TRUE;
	gtk_widget_destroy (data->dialog);
	gtk_main_quit ();
}

gboolean
e_shell_startup_wizard_create (void)
{
	SWData *data;
	CORBA_Environment ev;
	gboolean runbefore;

	data = g_new0 (SWData, 1);

	CORBA_exception_init (&ev);
	data->db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	if (BONOBO_EX (&ev) || data->db == CORBA_OBJECT_NIL) {
		g_warning ("Error starting wombat: (%s)", CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		g_free (data);

		return FALSE;
	}

	runbefore = bonobo_config_get_boolean (data->db, "/Shell/RunBefore", &ev);
	CORBA_exception_free (&ev);

	if (runbefore == TRUE) {
		g_print ("Already run\n");
		bonobo_object_release_unref (data->db, NULL);
		g_free (data);

		return TRUE;
	}

	data->wizard = glade_xml_new (EVOLUTION_GLADEDIR "/evolution-startup-wizard.glade", NULL);
	g_return_val_if_fail (data->wizard != NULL, FALSE);
	data->dialog = glade_xml_get_widget (data->wizard, "startup-wizard");
	g_return_val_if_fail (data->dialog != NULL, FALSE);
	gtk_window_set_wmclass (GTK_WINDOW (data->dialog), "startup-wizard",
				"Evolution:shell");

	page_hash = g_hash_table_new (NULL, NULL);
	data->druid = glade_xml_get_widget (data->wizard, "startup-druid");
	g_return_val_if_fail (data->druid != NULL, FALSE);
	gnome_druid_set_buttons_sensitive (GNOME_DRUID (data->druid),
					   FALSE, TRUE, FALSE);

	gtk_signal_connect (GTK_OBJECT (data->druid), "cancel",
			    GTK_SIGNAL_FUNC (startup_wizard_cancel), data);

	data->start = glade_xml_get_widget (data->wizard, "start-page");
	data->finish = glade_xml_get_widget (data->wizard, "done-page");
	g_return_val_if_fail (data->start != NULL, FALSE);
	g_return_val_if_fail (data->finish != NULL, FALSE);
	gtk_signal_connect (GTK_OBJECT (data->finish), "finish",
			    GTK_SIGNAL_FUNC (finish_func), data);

	make_mail_dialog_pages (data);
	g_return_val_if_fail (data->mailer != CORBA_OBJECT_NIL, TRUE);

	data->id_page = make_identity_page (data);
	data->source_page = make_receive_page (data);
	data->extra_page = make_extra_page (data);
	data->transport_page = make_transport_page (data);
	data->management_page = make_management_page (data);

	data->timezone_page = make_timezone_page (data);
	data->import_page = make_importer_page (data);

	g_return_val_if_fail (data->id_page != NULL, TRUE);
	g_return_val_if_fail (data->source_page != NULL, TRUE);
	g_return_val_if_fail (data->extra_page != NULL, TRUE);
	g_return_val_if_fail (data->transport_page != NULL, TRUE);
	g_return_val_if_fail (data->management_page != NULL, TRUE);
	g_return_val_if_fail (data->timezone_page != NULL, TRUE);
	g_return_val_if_fail (data->import_page != NULL, TRUE);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (data->druid), FALSE, TRUE, TRUE);
	gtk_widget_show_all (data->dialog);

	gtk_main ();

	/* Sync database */
	bonobo_config_set_boolean (data->db, "/Shell/RunBefore", TRUE, &ev);
	Bonobo_ConfigDatabase_sync (data->db, &ev);
	bonobo_object_release_unref (data->db, NULL);
	CORBA_exception_free (&ev);

	return !data->cancel;
}
