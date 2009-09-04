/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Iain Holmes  <iain@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <glade/glade.h>

#include <glib/gi18n.h>

#include "misc/e-gui-utils.h"

#include "e-util/e-dialog-utils.h"
#include "e-util/e-error.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-import.h"
#include "e-util/e-util-private.h"

#include "e-shell.h"
#include "e-shell-window.h"

#include "e-shell-importer.h"

typedef struct _ImportDialogFilePage {
	GtkWidget *vbox;
	GtkWidget *filename;
	GtkWidget *filetype;

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
	EShellWindow *window;

	GtkWidget *assistant;
	ImportDialogFilePage *filepage;
	ImportDialogDestPage *destpage;
	ImportDialogTypePage *typepage;
	ImportDialogImporterPage *importerpage;

	GtkWidget *filedialog;
	GtkWidget *typedialog;
	GtkWidget *destdialog;
	GtkWidget *intelligent;
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
#define IN g_print ("=====> %s (%d)\n", G_STRFUNC, __LINE__)
#define OUT g_print ("<==== %s (%d)\n", G_STRFUNC, __LINE__)
#else
#define IN
#define OUT
#endif

static struct {
	const gchar *name;
	const gchar *text;
} info[] = {
	{ "type_html",
	  N_("Choose the type of importer to run:")
	},
	{ "file_html",
	  N_("Choose the file that you want to import into Evolution, "
	     "and select what type of file it is from the list.")
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
create_help (const gchar *name)
{
	GtkWidget *label;
	gint i;

	for (i = 0; i < num_info; i++) {
		if (!strcmp (name, info[i].name))
			break;
	}

	if (i >= num_info)
		g_warning ("i > num_info\n");

	label = gtk_label_new(i < num_info ? _(info[i].text): NULL);
	gtk_widget_show (label);
	gtk_label_set_line_wrap((GtkLabel *)label, TRUE);

	return label;
}

/* Importing functions */

static void
filename_changed (GtkWidget *widget,
		  ImportData *data)
{
	ImportDialogFilePage *page;
	const gchar *filename;
	gint fileok;

	page = data->filepage;

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));

	fileok = filename && filename[0] && g_file_test(filename, G_FILE_TEST_IS_REGULAR);
	if (fileok) {
		GtkTreeIter iter;
		GtkTreeModel *model;
		gboolean valid;
		GSList *l;
		EImportImporter *first = NULL;
		gint i=0, firstitem=0;

		g_free(page->target->uri_src);
		page->target->uri_src = g_filename_to_uri(filename, NULL, NULL);

		l = e_import_get_importers(data->import, (EImportTarget *)page->target);
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (page->filetype));
		valid = gtk_tree_model_get_iter_first (model, &iter);
		while (valid) {
			gpointer eii = NULL;

			gtk_tree_model_get (model, &iter, 2, &eii, -1);

			if (g_slist_find (l, eii) != NULL) {
				if (first == NULL) {
					firstitem = i;
					first = eii;
				}
				gtk_list_store_set (GTK_LIST_STORE (model), &iter, 1, TRUE, -1);
				fileok = TRUE;
			} else {
				if (page->importer == eii)
					page->importer = NULL;
				gtk_list_store_set (GTK_LIST_STORE (model), &iter, 1, FALSE, -1);
			}
			i++;
			valid = gtk_tree_model_iter_next (model, &iter);
		}
		g_slist_free(l);

		if (page->importer == NULL && first) {
			page->importer = first;
			gtk_combo_box_set_active (GTK_COMBO_BOX (page->filetype), firstitem);
		}
		fileok = first != NULL;
	} else {
		GtkTreeIter iter;
		GtkTreeModel *model;
		gboolean valid;

		model = gtk_combo_box_get_model (GTK_COMBO_BOX (page->filetype));
		for (valid = gtk_tree_model_get_iter_first (model, &iter);
		     valid;
		     valid = gtk_tree_model_iter_next (model, &iter)) {
			gtk_list_store_set (GTK_LIST_STORE (model), &iter, 1, FALSE, -1);
		}
	}

	gtk_assistant_set_page_complete (GTK_ASSISTANT (data->assistant), page->vbox, fileok);
}

static void
filetype_changed_cb (GtkWidget *combobox, ImportData *data)
{
	GtkTreeIter iter;

	g_return_if_fail (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combobox), &iter));

	gtk_tree_model_get (gtk_combo_box_get_model (GTK_COMBO_BOX (combobox)), &iter, 2, &data->filepage->importer, -1);
	filename_changed (data->filepage->filename, data);
}

static ImportDialogFilePage *
importer_file_page_new (ImportData *data)
{
	ImportDialogFilePage *page;
	GtkWidget *table, *label;
	GtkCellRenderer *cell;
	GtkListStore *store;
	gint row = 0;

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

	page->filename = gtk_file_chooser_button_new (_("Select a file"), GTK_FILE_CHOOSER_ACTION_OPEN);
	g_signal_connect (GTK_FILE_CHOOSER_BUTTON (page->filename), "selection-changed", G_CALLBACK (filename_changed), data);

	gtk_table_attach (GTK_TABLE (table), page->filename, 1, 2,
			  row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), page->filename);

	row++;

	label = gtk_label_new_with_mnemonic (_("File _type:"));
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

	store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_POINTER);
	page->filetype = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	g_object_unref (store);

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (page->filetype));

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (page->filetype), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (page->filetype), cell,
                                  "text", 0,
                                  "sensitive", 1,
                                  NULL);

	gtk_table_attach (GTK_TABLE (table), page->filetype, 1, 2,
			  row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), page->filetype);

	gtk_container_set_border_width (GTK_CONTAINER (page->vbox), 12);

	gtk_widget_show_all (table);

	return page;
}

static ImportDialogDestPage *
importer_dest_page_new (ImportData *data)
{
	ImportDialogDestPage *page;

	page = g_new0 (ImportDialogDestPage, 1);

	page->vbox = gtk_vbox_new (FALSE, 5);

	gtk_container_set_border_width (GTK_CONTAINER (page->vbox), 12);

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
	gtk_container_set_border_width (GTK_CONTAINER (page->vbox), 12);

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

	gtk_container_set_border_width (GTK_CONTAINER (page->vbox), 12);

	gtk_widget_show_all (page->vbox);

	return page;
}

static void
prepare_intelligent_page (GtkAssistant *assistant, GtkWidget *apage, ImportData *data)
{
	GSList *l;
	GtkWidget *table;
	gint row;
	ImportDialogImporterPage *page = data->importerpage;

	if (page->target != NULL) {
		gtk_assistant_set_page_complete (assistant, apage, FALSE);
		return;
	}

	page->target = e_import_target_new_home(data->import, g_get_home_dir());

	if (data->importerpage->importers)
		g_slist_free(data->importerpage->importers);
	l = data->importerpage->importers = e_import_get_importers(data->import, (EImportTarget *)page->target);

	if (l == NULL) {
		gtk_box_pack_start(GTK_BOX (data->importerpage->vbox), create_help("nodata_html"), FALSE, TRUE, 0);
		gtk_assistant_set_page_complete (assistant, apage, FALSE);
		return;
	}

	table = gtk_table_new(g_slist_length(l), 2, FALSE);
	row = 0;
	for (;l;l=l->next) {
		EImportImporter *eii = l->data;
		gchar *str;
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

	gtk_assistant_set_page_complete (assistant, apage, TRUE);
}

static void
import_assistant_cancel (GtkAssistant *assistant, ImportData *data)
{
	if (data->import_dialog)
		gtk_dialog_response (GTK_DIALOG (data->import_dialog), GTK_RESPONSE_CANCEL);
	else
		gtk_widget_destroy (GTK_WIDGET (data->assistant));
}

static gboolean
import_assistant_esc (GtkAssistant *assistant, GdkEventKey *event, ImportData *data)
{
	if (event->keyval == GDK_Escape) {
		gtk_widget_destroy (GTK_WIDGET (assistant));
		return TRUE;
	} else
		return FALSE;
}

static void
import_assistant_weak_notify (gpointer blah,
			  GObject *where_the_object_was)
{
	ImportData *data = (ImportData *) blah;

	if (data->import_dialog && (GObject *)data->import_dialog != where_the_object_was) {
		/* postpone freeing of 'data' after the 'import_dialog' will stop,
		   but also indicate that the 'dialog' gone already */
		data->assistant = NULL;
		g_object_weak_ref ((GObject *)data->import_dialog, import_assistant_weak_notify, data);
		gtk_dialog_response (GTK_DIALOG (data->import_dialog), GTK_RESPONSE_CANCEL);
		return;
	}

	if (data->importerpage->target)
		e_import_target_free(data->import, data->importerpage->target);
	g_slist_free(data->importerpage->importers);

	if (data->filepage->target)
		e_import_target_free(data->import, data->filepage->target);

	g_object_unref(data->import);

	g_free(data);
}

static void
import_status(EImport *import, const gchar *what, gint pc, gpointer d)
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
import_done(EImport *ei, gpointer d)
{
	ImportData *data = d;
	gboolean have_dialog = data->assistant !=  NULL;

	gtk_widget_destroy (data->import_dialog);

	/* if doesn't have dialog, then the 'data' pointer is freed
	   on the above destroy call */
	if (have_dialog) {
		data->import_dialog = NULL;
		gtk_widget_destroy (data->assistant);
	}
}

static void
import_intelligent_done(EImport *ei, gpointer d)
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
import_assistant_apply (GtkAssistant *assistant, ImportData *data)
{
	EImportCompleteFunc done = NULL;
	gchar *msg = NULL;

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
		gpointer parent;

		parent = gtk_widget_get_parent (data->assistant);
		parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

		data->import_dialog = e_error_new(parent, "shell:importing", msg, NULL);
		g_signal_connect(data->import_dialog, "response", G_CALLBACK(import_dialog_response), data);
		data->import_label = gtk_label_new(_("Please wait"));
		data->import_progress = gtk_progress_bar_new();
		gtk_box_pack_start(GTK_BOX(((GtkDialog *)data->import_dialog)->vbox), data->import_label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(((GtkDialog *)data->import_dialog)->vbox), data->import_progress, FALSE, FALSE, 0);
		gtk_widget_show_all(data->import_dialog);

		e_import_import(data->import, data->import_target, data->import_importer, import_status, import_done, data);
	} else {
		gtk_widget_destroy(data->assistant);
	}

	g_free(msg);
}

static void
prepare_file_page (GtkAssistant *assistant, GtkWidget *apage, ImportData *data)
{
	GSList *importers, *imp;
	GtkListStore *store;
	ImportDialogFilePage *page = data->filepage;

	if (page->target != NULL) {
		filename_changed(data->filepage->filename, data);
		return;
	}

	page->target = e_import_target_new_uri(data->import, NULL, NULL);
	importers = e_import_get_importers (data->import, (EImportTarget *)page->target);

	store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (page->filetype)));
	gtk_list_store_clear (store);

	for (imp = importers; imp; imp = imp->next) {
		GtkTreeIter iter;
		EImportImporter *eii = imp->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			0, eii->name,
			1, TRUE,
			2, eii,
			-1);
	}

	g_slist_free (importers);

	gtk_combo_box_set_active (GTK_COMBO_BOX (page->filetype), 0);

	filename_changed (data->filepage->filename, data);

	g_signal_connect (page->filetype, "changed", G_CALLBACK (filetype_changed_cb), data);
}

static gboolean
prepare_dest_page (GtkAssistant *assistant, GtkWidget *apage, ImportData *data)
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

	gtk_box_pack_start ((GtkBox *)data->destpage->vbox, page->control, TRUE, TRUE, 0);
	gtk_assistant_set_page_complete (assistant, apage, TRUE);

	return FALSE;
}

static void
dialog_weak_notify (gpointer data,
		    GObject *where_the_dialog_was)
{
	gboolean *dialog_open = (gboolean *) data;

	*dialog_open = FALSE;
}

enum {
	PAGE_START,
	PAGE_INTELI_OR_DIRECT,
	PAGE_INTELI_SOURCE,
	PAGE_FILE_CHOOSE,
	PAGE_FILE_DEST,
	PAGE_FINISH
};

static gint
forward_cb (gint current_page, gpointer user_data)
{
	ImportData *data = user_data;

	switch (current_page) {
	case PAGE_INTELI_OR_DIRECT:
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data->typepage->intelligent)))
			return PAGE_INTELI_SOURCE;
		else
			return PAGE_FILE_CHOOSE;
	case PAGE_INTELI_SOURCE:
		return PAGE_FINISH;
	}

	return current_page + 1;
}

static void
import_assistant_prepare (GtkAssistant *assistant, GtkWidget *page, gpointer user_data)
{
	ImportData *data = user_data;

	if (page == data->importerpage->vbox)
		prepare_intelligent_page (assistant, page, data);
	else if (page == data->filepage->vbox)
		prepare_file_page (assistant, page, data);
	else  if (page == data->destpage->vbox)
		prepare_dest_page (assistant, page, data);
}

void
e_shell_importer_start_import (EShellWindow *shell_window)
{
	const gchar *empty_xpm_img[] = {
		"48 1 2 1",
		" 	c None",
		".	c #FFFFFF",
		"                                                "};
	ImportData *data = g_new0 (ImportData, 1);
	GtkWidget *html, *page;
	static gboolean dialog_open = FALSE;
	GdkPixbuf *icon, *spacer;
	GtkAssistant *assistant;

	if (dialog_open) {
		return;
	}

	data->import = e_import_new ("org.gnome.evolution.shell.importer");

	icon = e_icon_factory_get_icon ("stock_mail-import", GTK_ICON_SIZE_DIALOG);
	spacer = gdk_pixbuf_new_from_xpm_data (empty_xpm_img);

	dialog_open = TRUE;
	data->window = shell_window;
	data->assistant = gtk_assistant_new ();

	assistant = GTK_ASSISTANT (data->assistant);

	gtk_window_set_position (GTK_WINDOW (assistant), GTK_WIN_POS_CENTER);
	gtk_window_set_title (GTK_WINDOW (assistant), _("Evolution Import Assistant"));
	gtk_window_set_default_size (GTK_WINDOW (assistant), 500, 330);

	/* Start page */
	page = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (page), TRUE);
	gtk_misc_set_alignment (GTK_MISC (page), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (page), 12, 12);
	gtk_label_set_text (GTK_LABEL (page), _(
		"Welcome to the Evolution Import Assistant.\n"
		"With this assistant you will be guided through the process of importing external files into Evolution."));

	gtk_assistant_append_page (assistant, page);
	gtk_assistant_set_page_header_image (assistant, page, icon);
	gtk_assistant_set_page_title (assistant, page, _("Evolution Import Assistant"));
	gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_INTRO);
	gtk_assistant_set_page_side_image (assistant, page, spacer);
	gtk_assistant_set_page_complete (assistant, page, TRUE);

	/* Intelligent or direct import page */
	data->typepage = importer_type_page_new (data);
	html = create_help ("type_html");
	gtk_box_pack_start (GTK_BOX (data->typepage->vbox), html, FALSE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX (data->typepage->vbox), html, 0);

	page = data->typepage->vbox;
	gtk_assistant_append_page (assistant, page);
	gtk_assistant_set_page_header_image (assistant, page, icon);
	gtk_assistant_set_page_title (assistant, page, _("Importer Type"));
	gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONTENT);
	gtk_assistant_set_page_complete (assistant, page, TRUE);

	/* Intelligent importer source page */
	data->importerpage = importer_importer_page_new (data);
	html = create_help ("intelligent_html");
	gtk_box_pack_start (GTK_BOX (data->importerpage->vbox), html, FALSE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX (data->importerpage->vbox), html, 0);

	page = data->importerpage->vbox;
	gtk_assistant_append_page (assistant, page);
	gtk_assistant_set_page_header_image (assistant, page, icon);
	gtk_assistant_set_page_title (assistant, page, _("Select Information to Import"));
	gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONTENT);

	/* File selection and file type page */
	data->filepage = importer_file_page_new (data);
	html = create_help ("file_html");
	gtk_box_pack_start (GTK_BOX (data->filepage->vbox), html, FALSE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX (data->filepage->vbox), html, 0);

	page = data->filepage->vbox;
	gtk_assistant_append_page (assistant, page);
	gtk_assistant_set_page_header_image (assistant, page, icon);
	gtk_assistant_set_page_title (assistant, page, _("Select a File"));
	gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONTENT);

	/* File destination page */
	data->destpage = importer_dest_page_new (data);

	page = data->destpage->vbox;
	gtk_assistant_append_page (assistant, page);
	gtk_assistant_set_page_header_image (assistant, page, icon);
	gtk_assistant_set_page_title (assistant, page, _("Import Location"));
	gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONTENT);

	/* Finish page */
	page = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (page), 0.5, 0.5);
	gtk_label_set_text (GTK_LABEL (page), _("Click \"Apply\" to begin importing the file into Evolution."));

	gtk_assistant_append_page (assistant, page);
	gtk_assistant_set_page_header_image (assistant, page, icon);
	gtk_assistant_set_page_title (assistant, page, _("Import File"));
	gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONFIRM);
	gtk_assistant_set_page_side_image (assistant, page, spacer);
	gtk_assistant_set_page_complete (assistant, page, TRUE);

	/* setup the rest */
	g_object_weak_ref ((GObject *)assistant, dialog_weak_notify, &dialog_open);

	gtk_assistant_set_forward_page_func (assistant, forward_cb, data, NULL);

	g_signal_connect (assistant, "key_press_event", G_CALLBACK (import_assistant_esc), data);
	g_signal_connect (assistant, "cancel", G_CALLBACK (import_assistant_cancel), data);
	g_signal_connect (assistant, "prepare", G_CALLBACK (import_assistant_prepare), data);
	g_signal_connect (assistant, "apply", G_CALLBACK (import_assistant_apply), data);

	g_object_weak_ref ((GObject *)assistant, import_assistant_weak_notify, data);

	g_object_unref (icon);
	g_object_unref (spacer);

	gtk_assistant_update_buttons_state (assistant);

	gtk_widget_show_all (data->assistant);
}
