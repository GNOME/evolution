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

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <gdk/gdkkeysyms.h>

#include <glade/glade.h>

#include <libgnome/gnome-i18n.h>

#include <libgnomeui/gnome-druid.h>
#include <libgnomeui/gnome-druid-page-edge.h>
#include <libgnomeui/gnome-druid-page-standard.h>
#include <libgnomeui/gnome-file-entry.h>

#include "misc/e-gui-utils.h"

#include "e-util/e-dialog-utils.h"
#include "e-util/e-error.h"
#include "e-util/e-gtk-utils.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-import.h"
#include "e-util/e-util-private.h"

#include "e-shell.h"
#include "e-shell-window.h"
#include "e-shell-constants.h"

#include "e-shell-importer.h"

typedef struct _ImportDialogFilePage {
	GtkWidget *vbox;
	GtkWidget *filename;
	GtkWidget *filetype;
	GtkWidget *menu;

	GSList *items;

	EImportTargetURI *target;
	EImportImporter *importer;
} ImportDialogFilePage;

typedef struct _ImportDialogDestPage {
	GtkWidget *vbox;

	GtkWidget *control;
} ImportDialogDestPage;

typedef struct _ImportDialogTypePage {
	GtkWidget *vbox;
	GtkWidget *intelligent;
	GtkWidget *file;
} ImportDialogTypePage;

typedef struct _ImportDialogImporterPage {
	GtkWidget *vbox;

	GSList *importers;
	GSList *current;
	EImportTargetHome *target;
} ImportDialogImporterPage;

typedef struct _ImportData {
	EShell *shell;
	EShellWindow *window;
	
	GladeXML *wizard;
	GtkWidget *dialog;
	GtkWidget *druid;
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

	EImport *import;

	/* Used for importing phase of operation */
	EImportTarget *import_target;
	EImportImporter *import_importer;
	GtkWidget *import_dialog;
	GtkWidget *import_label;
	GtkWidget *import_progress;
} ImportData;

/*#define IMPORTER_DEBUG*/

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
	     "applications: Pine, Netscape, Elm, iCalendar. No importable\n"
	     "settings found. If you would like to\n"
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

static void
filename_changed (GtkEntry *entry,
		  ImportData *data)
{
	ImportDialogFilePage *page;
	const char *filename;
	int fileok;

	page = data->filepage;

	filename = gtk_entry_get_text (entry);

	fileok = filename && filename[0] && g_file_test(filename, G_FILE_TEST_IS_REGULAR);
	if (fileok) {
		GSList *l, *item;
		EImportImporter *first = NULL;
		int i=0, firstitem=0;

		g_free(page->target->uri_src);
		page->target->uri_src = g_filename_to_uri(filename, NULL, NULL);

		l = e_import_get_importers(data->import, (EImportTarget *)page->target);
		item = page->items;
		while (item) {
			EImportImporter *eii = g_object_get_data(item->data, "importer");

			if (g_slist_find(l, eii) != NULL) {
				if (first == NULL) {
					firstitem = i;
					first = eii;
				}
				gtk_widget_set_sensitive(item->data, TRUE);
				fileok = TRUE;
			} else {
				if (page->importer == eii)
					page->importer = NULL;
				gtk_widget_set_sensitive(item->data, FALSE);
			}
			i++;
			item = item->next;
		}
		g_slist_free(l);

		if (page->importer == NULL && first) {
			page->importer = first;
			gtk_option_menu_set_history((GtkOptionMenu *)page->filetype, firstitem);
		}
		fileok = first != NULL;
	} else {
		GSList *item;

		for (item = page->items;item;item=item->next)
			gtk_widget_set_sensitive(item->data, FALSE);
	}

	gnome_druid_set_buttons_sensitive(GNOME_DRUID (data->druid), TRUE, fileok, TRUE, FALSE);
}

static void
item_selected (GtkWidget *item,
	       ImportData *data)
{
	data->filepage->importer = g_object_get_data((GObject *)item, "importer");
	filename_changed((GtkEntry *)gnome_file_entry_gtk_entry((GnomeFileEntry *)data->filepage->filename), data);
}

#if 0
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
#endif

static ImportDialogFilePage *
importer_file_page_new (ImportData *data)
{
	ImportDialogFilePage *page;
	GtkWidget *table, *label, *entry;
	int row = 0;

	page = g_new0 (ImportDialogFilePage, 1);

	page->vbox = gtk_vbox_new (FALSE, 5);

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
	g_object_set (G_OBJECT (page->filename), "use_filechooser", TRUE, NULL);
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

	gtk_widget_show_all (page->vbox);

	return page;
}

static gboolean
prepare_intelligent_page (GnomeDruidPage *dpage,
			  GnomeDruid *druid,
			  ImportData *data)
{
	GSList *l;
	GtkWidget *table;
	int row;
	ImportDialogImporterPage *page = data->importerpage;

	if (page->target != NULL)
		return FALSE;

	page->target = e_import_target_new_home(data->import, g_get_home_dir());

	if (data->importerpage->importers)
		g_slist_free(data->importerpage->importers);
	l = data->importerpage->importers = e_import_get_importers(data->import, (EImportTarget *)page->target);

	if (l == NULL) {
		gtk_box_pack_start(GTK_BOX (data->importerpage->vbox), create_help("nodata_html"), FALSE, TRUE, 0);
		gnome_druid_set_buttons_sensitive(druid, TRUE, FALSE, TRUE, FALSE);
		return TRUE;
	}

	table = gtk_table_new(g_slist_length(l), 2, FALSE);
	row = 0;
	for (;l;l=l->next) {
		EImportImporter *eii = l->data;
		char *str;
		GtkWidget *w, *label;

		w = e_import_get_widget(data->import, (EImportTarget *)page->target, eii);

		str = g_strdup_printf(_("From %s:"), eii->name);
		label = gtk_label_new(str);
		gtk_widget_show(label);
		g_free(str);

		gtk_misc_set_alignment((GtkMisc *)label, 0, .5); 

		gtk_table_attach((GtkTable *)table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);
		if (w)
			gtk_table_attach((GtkTable *)table, w, 1, 2, row, row+1, GTK_FILL, 0, 3, 0);
		row++;
	}

	gtk_widget_show(table);
	gtk_box_pack_start((GtkBox *)data->importerpage->vbox, table, FALSE, FALSE, 0);

	return FALSE;
}

static void
import_druid_cancel (GnomeDruid *druid,
		     ImportData *data)
{
  	gtk_widget_destroy (GTK_WIDGET (data->dialog));
}

static gboolean
import_druid_esc (GnomeDruid *druid, 
		  GdkEventKey *event, 
		  ImportData *data)
{
	if (event->keyval == GDK_Escape) {
		gtk_widget_destroy (GTK_WIDGET (data->dialog));
		return TRUE;
	} else
		return FALSE;
}

static void
import_druid_weak_notify (void *blah,
			  GObject *where_the_object_was)
{
	ImportData *data = (ImportData *) blah;

	if (data->importerpage->target)
		e_import_target_free(data->import, data->importerpage->target);
	g_slist_free(data->importerpage->importers);

	if (data->filepage->target)
		e_import_target_free(data->import, data->filepage->target);
	g_slist_free(data->filepage->items);

	g_object_unref(data->import);
		
	g_object_unref(data->wizard);
	g_free(data);
}

static void
import_status(EImport *import, const char *what, int pc, void *d)
{
	ImportData *data = d;

	gtk_progress_bar_set_fraction((GtkProgressBar *)data->import_progress, (gfloat)(pc/100.0));
	gtk_progress_bar_set_text((GtkProgressBar *)data->import_progress, what);
}

static void
import_dialog_response(GtkDialog *d, guint button, ImportData *data)
{
	if (button == GTK_RESPONSE_CANCEL)
		e_import_cancel(data->import, data->import_target, data->import_importer);
}

static void
import_done(EImport *ei, void *d)
{
	ImportData *data = d;

	gtk_widget_destroy(data->import_dialog);
	gtk_widget_destroy(data->dialog);
}

static void
import_intelligent_done(EImport *ei, void *d)
{
	ImportData *data = d;

	if (data->importerpage->current
	    && (data->importerpage->current = data->importerpage->current->next)) {
		import_status(ei, "", 0, d);
		data->import_importer = data->importerpage->current->data;
		e_import_import(data->import, (EImportTarget *)data->importerpage->target, data->import_importer, import_status, import_intelligent_done, data);
	} else
		import_done(ei, d);
}
				
static void
import_druid_finish (GnomeDruidPage *page,
		     GnomeDruid *druid,
		     ImportData *data)
{
	EImportCompleteFunc done = NULL;
	char *msg = NULL;

	if (gtk_toggle_button_get_active((GtkToggleButton *)data->typepage->intelligent)) {
		data->importerpage->current = data->importerpage->importers;
		if (data->importerpage->current) {
			data->import_target = (EImportTarget *)data->importerpage->target;
			data->import_importer = data->importerpage->current->data;
			done = import_intelligent_done;
			msg = g_strdup_printf(_("Importing data."));
		}
	} else {
		if (data->filepage->importer) {
			data->import_importer = data->filepage->importer;
			data->import_target = (EImportTarget *)data->filepage->target;
			done = import_done;
			msg = g_strdup_printf(_("Importing `%s'"), data->filepage->target->uri_src);
		}
	}

	if (done) {
		data->import_dialog = e_error_new(NULL, "shell:importing", msg, NULL);
		g_signal_connect(data->import_dialog, "response", G_CALLBACK(import_dialog_response), data);
		data->import_label = gtk_label_new(_("Please wait"));
		data->import_progress = gtk_progress_bar_new();
		gtk_box_pack_start(GTK_BOX(((GtkDialog *)data->import_dialog)->vbox), data->import_label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(((GtkDialog *)data->import_dialog)->vbox), data->import_progress, FALSE, FALSE, 0);
		gtk_widget_show_all(data->import_dialog);

		e_import_import(data->import, data->import_target, data->import_importer, import_status, import_done, data);
	} else {
		gtk_widget_destroy(data->dialog);
	}

	g_free(msg);
}

static gboolean
prepare_file_page (GnomeDruidPage *dpage,
		   GnomeDruid *druid,
		   ImportData *data)
{
	GtkWidget *item, *menu;
	GSList *importers;
	ImportDialogFilePage *page = data->filepage;

	if (page->target != NULL) {
		filename_changed((GtkEntry *)gnome_file_entry_gtk_entry((GnomeFileEntry *)data->filepage->filename), data);
		return FALSE;
	}

	page->target = e_import_target_new_uri(data->import, NULL, NULL);
	importers = e_import_get_importers(data->import, (EImportTarget *)page->target);

	menu = gtk_menu_new();
	while (importers) {
		EImportImporter *eii = importers->data;

		item = gtk_menu_item_new_with_label(eii->name);
		gtk_widget_show(item);
		g_object_set_data((GObject *)item, "importer", eii);
		g_signal_connect(item, "activate", G_CALLBACK(item_selected), data);
		gtk_menu_shell_append((GtkMenuShell *)menu, item);

		data->filepage->items = g_slist_append(data->filepage->items, item);
		importers = importers->next;
	}
	g_slist_free(importers);

	data->filepage->menu = menu;
	gtk_option_menu_set_menu((GtkOptionMenu *)data->filepage->filetype, menu);

	filename_changed((GtkEntry *)gnome_file_entry_gtk_entry((GnomeFileEntry *)data->filepage->filename), data);

	return FALSE;
}

static gboolean
next_file_page (GnomeDruidPage *page,
		GnomeDruid *druid,
		ImportData *data)
{
	/* We dont sensitise the next button if we're not already usable */
	return FALSE;
}

static gboolean
prepare_dest_page (GnomeDruidPage *dpage,
		   GnomeDruid *druid,
		   ImportData *data)
{
	ImportDialogDestPage *page = data->destpage;

	if (page->control)
		gtk_container_remove((GtkContainer *)page->vbox, page->control);

	page->control = e_import_get_widget(data->import, (EImportTarget *)data->filepage->target, data->filepage->importer);
	if (page->control == NULL) {
		/* Coding error, not needed for translators */
		page->control = gtk_label_new("** PLUGIN ERROR ** No settings for importer");
		gtk_widget_show(page->control);
	}

	gtk_box_pack_start((GtkBox *)data->destpage->vbox, page->control, TRUE, TRUE, 0);

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
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (data->intelligent));
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
	char *gladefile;

	if (dialog_open) {
		return;
	}

	data->import = e_import_new("org.gnome.evolution.shell.importer");

	icon = e_icon_factory_get_icon ("stock_mail-import", E_ICON_SIZE_DIALOG);
	
	dialog_open = TRUE;
	data->window = shell_window;
	data->shell = e_shell_window_peek_shell (data->window);

	gladefile = g_build_filename (EVOLUTION_GLADEDIR, "import.glade", NULL);
	data->wizard = glade_xml_new (gladefile, NULL, NULL);
	g_free (gladefile);
	data->dialog = glade_xml_get_widget (data->wizard, "importwizard");
	gtk_window_set_default_size (GTK_WINDOW (data->dialog), 480, 320);
	gtk_window_set_wmclass (GTK_WINDOW (data->dialog), "importdruid",
				"Evolution:shell");
	e_dialog_set_transient_for (GTK_WINDOW (data->dialog), GTK_WIDGET (shell_window));
	g_object_weak_ref ((GObject *)data->dialog, dialog_weak_notify, &dialog_open);
	
	data->druid = glade_xml_get_widget (data->wizard, "druid1");
	g_signal_connect (data->druid, "cancel",
			  G_CALLBACK (import_druid_cancel), data);
	g_signal_connect (data->druid, "key_press_event",
			  G_CALLBACK (import_druid_esc), data);

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
	gnome_druid_page_edge_set_logo (data->finish, icon);
	g_signal_connect (data->finish, "back",
			  G_CALLBACK (back_finish_page), data);

	g_signal_connect (data->finish, "finish",
			  G_CALLBACK (import_druid_finish), data);

	g_object_weak_ref ((GObject *)data->dialog, import_druid_weak_notify, data);

	g_object_unref (icon);

	gtk_widget_show_all (data->dialog);
}
