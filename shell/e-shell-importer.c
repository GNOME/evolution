/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* importer.c
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Author: Iain Holmes  <iain@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string.h>

#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-druid.h>
#include <libgnomeui/gnome-druid-page-edge.h>
#include <libgnomeui/gnome-druid-page-standard.h>
#include <libgnomeui/gnome-file-entry.h>

#include <bonobo-activation/bonobo-activation.h>

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-widget.h>

#include "e-shell.h"
#include "e-shell-window.h"
#include "e-shell-constants.h"

#include "importer/evolution-importer-client.h"

#include <glade/glade.h>
#include <gal/widgets/e-gui-utils.h>

#include <e-util/e-gtk-utils.h>
#include <e-util/e-dialog-utils.h>
#include <e-util/e-icon-factory.h>

#include "e-shell-importer.h"
#include "importer/GNOME_Evolution_Importer.h"

typedef struct _ImportDialogFilePage {
	GtkWidget *vbox;
	GtkWidget *filename;
	GtkWidget *filetype;
	GtkWidget *menu;

	gboolean need_filename;
} ImportDialogFilePage;

typedef struct _ImportDialogDestPage {
	GtkWidget *vbox;
} ImportDialogDestPage;

typedef struct _ImportDialogTypePage {
	GtkWidget *vbox;
	GtkWidget *intelligent;
	GtkWidget *file;
} ImportDialogTypePage;

typedef struct _ImportDialogImporterPage {
	GtkWidget *vbox;

	GList *importers;
	gboolean prepared;
	int running;
} ImportDialogImporterPage;

typedef struct _ImportData {
	EShell *shell;
	EShellWindow *window;
	
	GladeXML *wizard;
	GtkWidget *dialog;
	GtkWidget *druid;
	GtkWidget *control;
	ImportDialogFilePage *filepage;
	ImportDialogDestPage *destpage;
	ImportDialogTypePage *typepage;
	ImportDialogImporterPage *importerpage;

	GtkWidget *filedialog;
	GtkWidget *typedialog;
	GtkWidget *destdialog;
	GtkWidget *intelligent;
	GnomeDruidPageEdge *start;
	GnomeDruidPageEdge *finish;
	GtkWidget *vbox;

	char *filename;
	char *choosen_iid;
	EvolutionImporterClient *client;
} ImportData;

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

#define IMPORTER_REPO_ID_QUERY "repo_ids.has ('IDL:GNOME/Evolution/Importer:" BASE_VERSION "')"
#define IMPORTER_INTEL_REPO_ID_QUERY "repo_ids.has ('IDL:GNOME/Evolution/IntelligentImporter:" BASE_VERSION "')"
#define IMPORTER_DEBUG

#ifdef IMPORTER_DEBUG
#define IN g_print ("=====> %s (%d)\n", G_GNUC_FUNCTION, __LINE__)
#define OUT g_print ("<==== %s (%d)\n", G_GNUC_FUNCTION, __LINE__)
#else
#define IN
#define OUT
#endif

static struct {
	char *name;
	char *text;
} info[] = {
	{ "type_html",
	  N_("Choose the type of importer to run:")
	},
	{ "file_html",
	  N_("Choose the file that you want to import into Evolution, "
	     "and select what type of file it is from the list.\n\n"
	     "You can select \"Automatic\" if you do not know, and "
	     "Evolution will attempt to work it out.")
	},
	{ "dest_html",
	  N_("Choose the destination for this import")
	},
	{ "intelligent_html",
	  N_("Please select the information that you would like to import:")
	},
	{ "nodata_html",
	  N_("Evolution checked for settings to import from the following\n"
	     "applications: Pine, Netscape, Elm, iCalendar. No settings\n"
	     "that could be imported where found. If you would like to\n"
	     "try again, please click the \"Back\" button.\n")
	}
};
#define num_info (sizeof (info) / sizeof (info[0]))

static GtkWidget *
create_help (const char *name)
{
	GtkWidget *label;
	int i;

	for (i = 0; i < num_info; i++) {
		if (!strcmp (name, info[i].name))
			break;
	}
	g_assert(i != num_info);

	label = gtk_label_new(_(info[i].text));
	gtk_widget_show (label);
	gtk_label_set_line_wrap((GtkLabel *)label, TRUE);

	return label;
}

/* Importing functions */

/* Data to be passed around */
typedef struct _ImporterComponentData {
	EvolutionImporterClient *client;
	EvolutionImporterListener *listener;
	char *filename;

	GtkDialog *dialog;
	GtkWidget *contents;

	int item;

	gboolean stop;
} ImporterComponentData;

static gboolean importer_timeout_fn (gpointer data);
static void
import_cb (EvolutionImporterListener *listener,
	   EvolutionImporterResult result,
	   gboolean more_items,
	   void *data)
{
	ImporterComponentData *icd = (ImporterComponentData *) data;
	char *label;

	IN;
	if (icd->stop != TRUE) {
		if (result == EVOLUTION_IMPORTER_NOT_READY) {
			gtk_timeout_add (500, importer_timeout_fn, data);
			OUT;
			return;
		}
		
		if (result == EVOLUTION_IMPORTER_BUSY) {
			gtk_timeout_add (500, importer_timeout_fn, data);
			OUT;
			return;
		}

		if (more_items) {
			label = g_strdup_printf (_("Importing %s\nImporting item %d."),
						 icd->filename, ++(icd->item));
			gtk_label_set_text (GTK_LABEL (icd->contents), label);
			g_free (label);
			while (gtk_events_pending ())
				gtk_main_iteration ();
			
			g_idle_add_full (G_PRIORITY_LOW, importer_timeout_fn, 
					 data, NULL);
			OUT;
			return;
		}
	}
	
	g_free (icd->filename);
	if (icd->dialog != NULL)
		gtk_widget_destroy ((GtkWidget *)icd->dialog);
	bonobo_object_unref (BONOBO_OBJECT (icd->listener));
	g_object_unref (icd->client);
	g_free (icd);

	OUT;
}

static gboolean
importer_timeout_fn (gpointer data)
{
	ImporterComponentData *icd = (ImporterComponentData *) data;
	char *label;

	IN;
	label = g_strdup_printf (_("Importing %s\nImporting item %d."),
				 icd->filename, icd->item);
	gtk_label_set_text (GTK_LABEL (icd->contents), label);
	g_free (label);
	while (gtk_events_pending ())
		gtk_main_iteration ();
	
	evolution_importer_client_process_item (icd->client, icd->listener);
	OUT;
	return FALSE;
}

static void
dialog_response_cb (GtkDialog *dialog,
		   int button_number,
		   ImporterComponentData *icd)
{
	if (button_number != GTK_RESPONSE_CANCEL)
		return; /* Interesting... */

	icd->stop = TRUE;
}

static void
dialog_destroy_notify (void *data,
		       GObject *where_the_dialog_was)
{
	ImporterComponentData *icd = (ImporterComponentData *) data;

	icd->dialog = NULL;
	icd->stop = TRUE;
}

struct _IIDInfo {
	char *iid;
	char *name;
};

static void
free_iid_list (GList *list)
{
	for (; list; list = list->next) {
		struct _IIDInfo *iid = list->data;

		g_free (iid->iid);
		g_free (iid->name);
		g_free (iid);
	}
}

static const char *
get_name_from_component_info (const Bonobo_ServerInfo *info)
{
	Bonobo_ActivationProperty *property;
	const char *name;

	property = bonobo_server_info_prop_find ((Bonobo_ServerInfo *) info, "evolution:menu_name");
	if (property == NULL || property->v._d != Bonobo_ACTIVATION_P_STRING)
		return NULL;

	name = property->v._u.value_string;

	return name;
}

static char *
choose_importer_from_list (GList *importer_list)
{
	GtkWidget *dialog, *clist;
	GList *p;
	int ans;
	char *iid;

	dialog = gtk_dialog_new_with_buttons(_("Select importer"), NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					     GTK_STOCK_OK, GTK_RESPONSE_OK,
					     NULL);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	clist = gtk_clist_new (1);
	for (p = importer_list; p; p = p->next) {
		struct _IIDInfo *iid;
		char *text[1];
		int row;

		iid = p->data;
		text[0] = iid->name;
		row = gtk_clist_append (GTK_CLIST (clist), text);
		gtk_clist_set_row_data (GTK_CLIST (clist), row, iid->iid);
	}

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), clist, TRUE, TRUE, 0);
	gtk_clist_set_selection_mode (GTK_CLIST (clist), GTK_SELECTION_BROWSE);
	gtk_widget_show (clist);

	ans = gtk_dialog_run((GtkDialog *)dialog);
	switch (ans) {
	case GTK_RESPONSE_OK:
		ans = GPOINTER_TO_INT (GTK_CLIST (clist)->selection->data);
		iid = gtk_clist_get_row_data (GTK_CLIST (clist), ans);
		break;

	case 1:
	default:
		iid = NULL;
		break;
	}
	
	gtk_widget_destroy (dialog);

	return g_strdup (iid);
}

static gboolean
get_iid_for_filetype (const char *filename, char **ret_iid)
{
	Bonobo_ServerInfoList *info_list;
	CORBA_Environment ev;
	GList *can_handle = NULL;
	int i, len = 0;

	CORBA_exception_init (&ev);
	info_list = bonobo_activation_query (IMPORTER_REPO_ID_QUERY, NULL, &ev);

	for (i = 0; i < info_list->_length; i++) {
		CORBA_Environment ev2;
		CORBA_Object importer;
		const Bonobo_ServerInfo *info;
		
		info = info_list->_buffer + i;

		CORBA_exception_init (&ev2);
		importer = bonobo_activation_activate_from_id ((char *) info->iid, 0, NULL, &ev2);
		if (ev2._major != CORBA_NO_EXCEPTION) {
			g_warning ("Error activating %s", info->iid);
			CORBA_exception_free (&ev2);
			continue;
		}

		if (GNOME_Evolution_Importer_supportFormat (importer,
							    filename, &ev2)) {
			struct _IIDInfo *iid;

			iid = g_new (struct _IIDInfo, 1);
			iid->iid = g_strdup (info->iid);
			iid->name = g_strdup (get_name_from_component_info (info));
			
			can_handle = g_list_prepend (can_handle, iid);
			len++;
		}

		bonobo_object_release_unref (importer, &ev2);
		CORBA_exception_free (&ev2);
	}

	CORBA_free (info_list);

	if (len == 1) {
		struct _IIDInfo *iid;
		
		iid = can_handle->data;

		*ret_iid = g_strdup (iid->iid);
		
		free_iid_list (can_handle);
		g_list_free (can_handle);
		
		return TRUE;
	} else if (len > 1) {
		/* Display all the IIDs */
		*ret_iid = choose_importer_from_list (can_handle);
		
		free_iid_list (can_handle);
		g_list_free (can_handle);

		return *ret_iid ? TRUE : FALSE;
	} else {
		*ret_iid = NULL;
		
		return TRUE;
	}
}

static void
start_import (gpointer parent, const char *filename, EvolutionImporterClient *client)
{
	ImporterComponentData *icd;
	char *label;
	
	if (!g_file_test (filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		e_notice (parent, GTK_MESSAGE_ERROR, _("File %s does not exist"), filename);

		return;
	}

	icd = g_new (ImporterComponentData, 1);
	icd->client = g_object_ref (client);
	icd->stop = FALSE;
	icd->dialog = GTK_DIALOG (gtk_dialog_new_with_buttons(_("Importing"), NULL, 0,
							      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							      NULL));
	gtk_dialog_set_has_separator (icd->dialog, FALSE);
	g_signal_connect (icd->dialog, "response", G_CALLBACK (dialog_response_cb), icd);

	g_object_weak_ref (G_OBJECT(icd->dialog), dialog_destroy_notify, icd);

	label = g_strdup_printf (_("Importing %s.\n"), filename);
	icd->contents = gtk_label_new (label);
	g_free (label);
	
	gtk_box_pack_start (GTK_BOX (icd->dialog->vbox), icd->contents, TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (icd->dialog));
	while (gtk_events_pending ())
		gtk_main_iteration ();

	if (evolution_importer_client_load_file (icd->client, filename) == FALSE) {
		label = g_strdup_printf (_("Error loading %s"), filename);
		e_notice (icd->dialog, GTK_MESSAGE_ERROR, _("Error loading %s"), filename);

		gtk_label_set_text (GTK_LABEL (icd->contents), label);
		g_free (label);
		while (gtk_events_pending ())
			gtk_main_iteration ();
		
		g_object_unref (icd->client);
		if (icd->dialog)
			gtk_widget_destroy (GTK_WIDGET (icd->dialog));
		g_free (icd);
		return;
	}

	icd->filename = g_strdup (filename);
	icd->item = 1;
	
	label = g_strdup_printf (_("Importing %s\nImporting item 1."),
				 filename);
	gtk_label_set_text (GTK_LABEL (icd->contents), label);
	g_free (label);
	while (gtk_events_pending ())
		gtk_main_iteration ();

	icd->listener = evolution_importer_listener_new (import_cb, icd);
	evolution_importer_client_process_item (icd->client, icd->listener);
}

static void
filename_changed (GtkEntry *entry,
		  ImportData *data)
{
	ImportDialogFilePage *page;
	const char *filename;

	page = data->filepage;

	filename = gtk_entry_get_text (entry);
	if (filename != NULL && *filename != '\0')
		page->need_filename = FALSE;
	else
		page->need_filename = TRUE;

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (data->druid), 
					   TRUE, !page->need_filename, TRUE, FALSE);
}

static void
item_selected (GtkWidget *item,
	       ImportData *data)
{
	char *iid;

	g_free (data->choosen_iid);
	iid = g_object_get_data (G_OBJECT (item), "bonoboiid");
	if (iid == NULL)
		data->choosen_iid = g_strdup ("Automatic");
	else
		data->choosen_iid = g_strdup (iid);
}

static int
compare_info_name (const void *data1, const void  *data2)
{
	const Bonobo_ServerInfo *info1 = (Bonobo_ServerInfo *)data1;
	const Bonobo_ServerInfo *info2 = (Bonobo_ServerInfo *)data2;
	const char *name1 = get_name_from_component_info (info1);
	const char *name2 = get_name_from_component_info (info2);

	/* If we can't find a name for a plug-in, its iid will be used
	 * for display. Put such plug-ins at the end of the list since
	 * their displayed name won't be really user-friendly
	 */
	if (name1 == NULL) {
		return -1;
	}
	if (name2 == NULL) {
		return 1;
	}
	return g_utf8_collate (name1, name2);
}

static GtkWidget *
create_plugin_menu (ImportData *data)
{
	Bonobo_ServerInfoList *info_list;
	CORBA_Environment ev;
	int i;
	GtkWidget *menu;
	GtkWidget *item;

	menu = gtk_menu_new ();
	item = gtk_menu_item_new_with_label (_("Automatic"));
	g_object_set_data_full ((GObject *)item, "bonoboiid", g_strdup ("Automatic"), g_free);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	CORBA_exception_init (&ev);
	info_list = bonobo_activation_query (IMPORTER_REPO_ID_QUERY, NULL, &ev);
	/* Sort info list to get a consistent ordering of the items in the
	 * combo box from one run of evolution to another.
	 */
	qsort (info_list->_buffer, info_list->_length, 
	       sizeof (Bonobo_ServerInfo),
	       compare_info_name);

	for (i = 0; i < info_list->_length; i++) {
		const Bonobo_ServerInfo *info;
		char *name = NULL;

		info = info_list->_buffer + i;

		name = g_strdup (get_name_from_component_info (info));
		if (name == NULL) {
			name = g_strdup (info->iid);
		}

		item = gtk_menu_item_new_with_label (name);
		g_free (name);
		gtk_widget_show (item);
		g_signal_connect (item, "activate",
				  G_CALLBACK (item_selected), data);

		g_object_set_data_full ((GObject *)item, "bonoboiid", g_strdup (info->iid), g_free);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}
	CORBA_free (info_list);

	return menu;
}

static ImportDialogFilePage *
importer_file_page_new (ImportData *data)
{
	ImportDialogFilePage *page;
	GtkWidget *table, *label, *entry;
	int row = 0;

	page = g_new0 (ImportDialogFilePage, 1);

	page->vbox = gtk_vbox_new (FALSE, 5);
	page->need_filename = TRUE;

	table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (table), 10);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);
	gtk_box_pack_start (GTK_BOX (page->vbox), table, TRUE, TRUE, 0);

	label = gtk_label_new_with_mnemonic (_("F_ilename:"));
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1, 
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

	page->filename = gnome_file_entry_new ("Evolution_Importer_FileName", _("Select a file"));
	entry = gnome_file_entry_gtk_entry((GnomeFileEntry *)page->filename);
	g_signal_connect (entry, "changed", G_CALLBACK (filename_changed), data);
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);

	gtk_table_attach (GTK_TABLE (table), page->filename, 1, 2, 
			  row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), page->filename);

	row++;

	label = gtk_label_new_with_mnemonic (_("File _type:"));
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

	page->filetype = gtk_option_menu_new ();
	page->menu = create_plugin_menu (data);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (page->filetype), page->menu);
	gtk_table_attach (GTK_TABLE (table), page->filetype, 1, 2, 
			  row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), page->filetype);

	gtk_widget_show_all (table);

	return page;
}


static ImportDialogDestPage *
importer_dest_page_new (ImportData *data)
{
	ImportDialogDestPage *page;

	page = g_new0 (ImportDialogDestPage, 1);

	page->vbox = gtk_vbox_new (FALSE, 5);

	return page;
}

static ImportDialogTypePage *
importer_type_page_new (ImportData *data)
{
	ImportDialogTypePage *page;

	page = g_new0 (ImportDialogTypePage, 1);

	page->vbox = gtk_vbox_new (FALSE, 5);
	page->intelligent = gtk_radio_button_new_with_mnemonic (NULL, 
							     _("Import data and settings from _older programs"));
	gtk_box_pack_start (GTK_BOX (page->vbox), page->intelligent, FALSE, FALSE, 0);
	page->file = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (page->intelligent),
								  _("Import a _single file"));
	gtk_box_pack_start (GTK_BOX (page->vbox), page->file, FALSE, FALSE, 0);
	gtk_widget_show_all (page->vbox);
	return page;
}

static ImportDialogImporterPage *
importer_importer_page_new (ImportData *data)
{
	ImportDialogImporterPage *page;
	GtkWidget *sep;

	page = g_new0 (ImportDialogImporterPage, 1);

	page->vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (page->vbox), 4);

	sep = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (page->vbox), sep, FALSE, FALSE, 0);

	page->prepared = FALSE;
	gtk_widget_show_all (page->vbox);

	return page;
}

static GList *
get_intelligent_importers (void)
{
	Bonobo_ServerInfoList *info_list;
	GList *iids_ret = NULL;
	CORBA_Environment ev;
	int i;

	CORBA_exception_init (&ev);
	info_list = bonobo_activation_query (IMPORTER_INTEL_REPO_ID_QUERY, NULL, &ev);
	CORBA_exception_free (&ev);

	for (i = 0; i < info_list->_length; i++) {
		const Bonobo_ServerInfo *info;

		info = info_list->_buffer + i;
		iids_ret = g_list_prepend (iids_ret, g_strdup (info->iid));
	}

	return iids_ret;
}

static gboolean
prepare_intelligent_page (GnomeDruidPage *page,
			  GnomeDruid *druid,
			  ImportData *data)
{
	GtkWidget *dialog;
	ImportDialogImporterPage *import;
	GList *l, *importers;
	GtkWidget *table, *no_data;
	int running = 0;

	if (data->importerpage->prepared == TRUE) {
		if (data->importerpage->running == 0)
			gnome_druid_set_buttons_sensitive(druid, TRUE, FALSE, TRUE, FALSE);
		return TRUE;
	}

	data->importerpage->prepared = TRUE;

	dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_INFO, GTK_BUTTONS_NONE, "%s",
					_("Please wait...\nScanning for existing setups"));
	e_make_widget_backing_stored (dialog);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Starting Intelligent Importers"));
	gtk_widget_show_all (dialog);
	gtk_widget_show_now (dialog);

	gtk_widget_queue_draw (dialog);
	gdk_flush ();

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}

	import = data->importerpage;
	importers = get_intelligent_importers ();
	if (importers == NULL) {
		/* No importers, go directly to finish, do not pass go
		   Do not collect $200 */
		import->running = 0;
		no_data = create_help ("nodata_html");
		gtk_box_pack_start (GTK_BOX (data->importerpage->vbox), no_data,
				    FALSE, TRUE, 0);
		gnome_druid_set_buttons_sensitive(druid, TRUE, FALSE, TRUE, FALSE);
		gtk_widget_destroy (dialog);
		return TRUE;
	}

	table = gtk_table_new (g_list_length (importers), 2, FALSE);
	for (l = importers; l; l = l->next) {
		GtkWidget *label;
		IntelligentImporterData *id;
		CORBA_Environment ev;
		gboolean can_run;
		char *str;
		
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
		gtk_box_pack_start (GTK_BOX (data->importerpage->vbox), table,
				    FALSE, FALSE, 0);
	}

	gtk_widget_show_all (table);

	if (running == 0) {
		no_data = create_help ("nodata_html");
		gtk_box_pack_start (GTK_BOX (data->importerpage->vbox), no_data,
				    FALSE, TRUE, 0);
		gnome_druid_set_buttons_sensitive(druid, TRUE, FALSE, TRUE, FALSE);
		gtk_widget_destroy (dialog);
		return TRUE;
	}

	import->running = running;
	gtk_widget_destroy (dialog);

	return FALSE;
}

static void
import_druid_cancel (GnomeDruid *druid,
		     ImportData *data)
{
  	gtk_widget_destroy (GTK_WIDGET (data->dialog));
}

static void
import_druid_weak_notify (void *blah,
			  GObject *where_the_object_was)
{
	ImportData *data = (ImportData *) blah;

	g_object_unref (data->wizard);
	g_free (data->choosen_iid);
	g_free (data);
}

static void
free_importers (ImportData *data)
{
	GList *l;

	for (l = data->importerpage->importers; l; l = l->next) {
		IntelligentImporterData *iid;

		iid = l->data;
		if (iid->object != CORBA_OBJECT_NIL) {
			bonobo_object_release_unref (iid->object, NULL);
		}
	}

	g_list_free (data->importerpage->importers);
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
do_import (ImportData *data)
{
	CORBA_Environment ev;
	GList *l, *selected = NULL;

	for (l = data->importerpage->importers; l; l = l->next) {
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
				
static void
import_druid_finish (GnomeDruidPage *page,
		     GnomeDruid *druid,
		     ImportData *data)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data->typepage->intelligent))) {
		do_import (data);
	} else {
		start_import (druid, data->filename, data->client);
	}

	gtk_widget_destroy (data->dialog);
}

static gboolean
prepare_file_page (GnomeDruidPage *page,
		   GnomeDruid *druid,
		   ImportData *data)
{
	gnome_druid_set_buttons_sensitive (druid, TRUE, 
					   !data->filepage->need_filename, 
					   TRUE, FALSE);
	return FALSE;
}

static gboolean
next_file_page (GnomeDruidPage *page,
		GnomeDruid *druid,
		ImportData *data)
{
	char *real_iid = NULL;

	/* Get and test the file name */
	if (data->filename)
		g_free (data->filename);
	data->filename = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (data->filepage->filename), FALSE);
	
	if (!g_file_test (data->filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		e_notice (druid, GTK_MESSAGE_ERROR, _("File %s does not exist"), data->filename);
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->filedialog));

		return TRUE;
	}

	/* Work out the component to use */
	if (data->choosen_iid == NULL || strcmp (data->choosen_iid, "Automatic") == 0) {
		if (!get_iid_for_filetype (data->filename, &real_iid)) {
			gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->filedialog));

			return TRUE;			
		}	
	} else {
		real_iid = g_strdup (data->choosen_iid);
	}

	if (!real_iid) {
		e_notice (druid, GTK_MESSAGE_ERROR, _("No importer available for file %s"), data->filename);
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->filedialog));

		return TRUE;
	}

	if (data->client)
		g_object_unref (data->client);
	data->client = evolution_importer_client_new_from_id (real_iid);
	g_free (real_iid);

	if (!data->client) {
		e_notice (druid, GTK_MESSAGE_ERROR, _("Unable to execute importer"));
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->filedialog));

		return TRUE;
	}
	
	return FALSE;
}

static gboolean
prepare_dest_page (GnomeDruidPage *page,
		   GnomeDruid *druid,
		   ImportData *data)
{	
	/* Add the widget */
	if (data->control)
		gtk_container_remove (GTK_CONTAINER (data->destpage->vbox), data->control);
	data->control = evolution_importer_client_create_control (data->client);
	gtk_box_pack_start((GtkBox *)data->destpage->vbox, data->control, FALSE, FALSE, 0);
	gtk_widget_show_all (data->destpage->vbox);

	return FALSE;
}

static gboolean
next_dest_page (GnomeDruidPage *page,
		GnomeDruid *druid,
		ImportData *data)
{
	gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->finish));
	return TRUE;
}

static gboolean
next_type_page (GnomeDruidPage *page,
		GnomeDruid *druid,
		ImportData *data)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data->typepage->intelligent))) {
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->intelligent));
	} else {
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->filedialog));
	}

	return TRUE;
}

static gboolean
back_finish_page (GnomeDruidPage *page,
		  GnomeDruid *druid,
		  ImportData *data)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data->typepage->intelligent))) {
		if (data->importerpage->running != 0) {
			gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->intelligent));
		} else {
			gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->typedialog));
		}
	} else {
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->destdialog));
	}

	return TRUE;
}

static gboolean
back_intelligent_page (GnomeDruidPage *page,
		       GnomeDruid *druid,
		       ImportData *data)
{
	gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->typedialog));
	return TRUE;
}

static void
dialog_weak_notify (void *data,
		    GObject *where_the_dialog_was)
{
	gboolean *dialog_open = (gboolean *) data;

	*dialog_open = FALSE;
}

void
e_shell_importer_start_import (EShellWindow *shell_window)
{
	ImportData *data = g_new0 (ImportData, 1);
	GtkWidget *html;
	static gboolean dialog_open = FALSE;
	GdkPixbuf *icon;

	if (dialog_open) {
		return;
	}
	
	icon = e_icon_factory_get_icon ("stock_import", E_ICON_SIZE_DIALOG);
	
	dialog_open = TRUE;
	data->window = shell_window;
	data->shell = e_shell_window_peek_shell (data->window);

	data->wizard = glade_xml_new (EVOLUTION_GLADEDIR "/import.glade", NULL, NULL);
	data->dialog = glade_xml_get_widget (data->wizard, "importwizard");
	gtk_window_set_default_size (GTK_WINDOW (data->dialog), 480, 320);
	gtk_window_set_wmclass (GTK_WINDOW (data->dialog), "importdruid",
				"Evolution:shell");
	e_dialog_set_transient_for (GTK_WINDOW (data->dialog), GTK_WIDGET (shell_window));
	g_object_weak_ref ((GObject *)data->dialog, dialog_weak_notify, &dialog_open);
	
	data->druid = glade_xml_get_widget (data->wizard, "druid1");
	g_signal_connect (data->druid, "cancel",
			  G_CALLBACK (import_druid_cancel), data);

	gtk_button_set_use_underline ((GtkButton *)((GnomeDruid *)data->druid)->finish, TRUE);
	gtk_button_set_label((GtkButton *)((GnomeDruid *)data->druid)->finish, _("_Import"));

	/* Start page */
	data->start = GNOME_DRUID_PAGE_EDGE (glade_xml_get_widget (data->wizard, "page0"));
	gnome_druid_page_edge_set_logo (data->start, icon);

	/* Intelligent or direct import page */
	data->typedialog = glade_xml_get_widget (data->wizard, "page1");
	gnome_druid_page_standard_set_logo (GNOME_DRUID_PAGE_STANDARD (data->typedialog), icon);
	g_signal_connect (data->typedialog, "next",
			  G_CALLBACK (next_type_page), data);
	data->typepage = importer_type_page_new (data);
	html = create_help ("type_html");
	gtk_box_pack_start (GTK_BOX (data->typepage->vbox), html, FALSE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX (data->typepage->vbox), html, 0);

	gtk_box_pack_start (GTK_BOX (GNOME_DRUID_PAGE_STANDARD (data->typedialog)->vbox), data->typepage->vbox, TRUE, TRUE, 0);

	/* Intelligent importer source page */
	data->intelligent = glade_xml_get_widget (data->wizard, "page2-intelligent");
	gnome_druid_page_standard_set_logo (GNOME_DRUID_PAGE_STANDARD (data->intelligent), icon);
	g_signal_connect (data->intelligent, "back",
			  G_CALLBACK (back_intelligent_page), data);
	g_signal_connect_after (data->intelligent, "prepare",
			  G_CALLBACK (prepare_intelligent_page), data);

	data->importerpage = importer_importer_page_new (data);
	html = create_help ("intelligent_html");
	gtk_box_pack_start (GTK_BOX (data->importerpage->vbox), html, FALSE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX (data->importerpage->vbox), html, 0);
	
	gtk_box_pack_start (GTK_BOX (GNOME_DRUID_PAGE_STANDARD (data->intelligent)->vbox), data->importerpage->vbox, TRUE, TRUE, 0);
	

	/* File selection and file type page */
	data->filedialog = glade_xml_get_widget (data->wizard, "page2-file");
	gnome_druid_page_standard_set_logo (GNOME_DRUID_PAGE_STANDARD (data->filedialog), icon);
	g_signal_connect_after (data->filedialog, "prepare",
			  G_CALLBACK (prepare_file_page), data);
	g_signal_connect (data->filedialog, "next",
			  G_CALLBACK (next_file_page), data);
	gnome_druid_page_edge_set_logo (data->finish, icon);
	data->filepage = importer_file_page_new (data);

	html = create_help ("file_html");
	gtk_box_pack_start (GTK_BOX (data->filepage->vbox), html, FALSE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX (data->filepage->vbox), html, 0);

	gtk_box_pack_start (GTK_BOX (GNOME_DRUID_PAGE_STANDARD (data->filedialog)->vbox), data->filepage->vbox, TRUE, TRUE, 0);

	/* File destination page */
	data->destdialog = glade_xml_get_widget (data->wizard, "page3-file");
	g_signal_connect_after (data->destdialog, "prepare",
			  G_CALLBACK (prepare_dest_page), data);
	g_signal_connect (data->destdialog, "next",
			  G_CALLBACK (next_dest_page), data);

	data->destpage = importer_dest_page_new (data);

	gtk_box_pack_start (GTK_BOX (GNOME_DRUID_PAGE_STANDARD (data->destdialog)->vbox), data->destpage->vbox, TRUE, TRUE, 0);

	/* Finish page */
	data->finish = GNOME_DRUID_PAGE_EDGE (glade_xml_get_widget (data->wizard, "page4"));
	g_signal_connect (data->finish, "back",
			  G_CALLBACK (back_finish_page), data);

	g_signal_connect (data->finish, "finish",
			  G_CALLBACK (import_druid_finish), data);

	g_object_weak_ref ((GObject *)data->dialog, import_druid_weak_notify, data);

	g_object_unref (icon);

	gtk_widget_show_all (data->dialog);
}
