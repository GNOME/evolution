/*
 * e-import-assistant.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-import-assistant.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <libebackend/libebackend.h>

#include "e-dialog-utils.h"
#include "e-dialog-widgets.h"
#include "e-import.h"
#include "e-util-private.h"

#define E_IMPORT_ASSISTANT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_IMPORT_ASSISTANT, EImportAssistantPrivate))

typedef struct _ImportFilePage ImportFilePage;
typedef struct _ImportDestinationPage ImportDestinationPage;
typedef struct _ImportTypePage ImportTypePage;
typedef struct _ImportSelectionPage ImportSelectionPage;
typedef struct _ImportProgressPage ImportProgressPage;
typedef struct _ImportSimplePage ImportSimplePage;

struct _ImportFilePage {
	GtkWidget *filename;
	GtkWidget *filetype;

	EImportTargetURI *target;
	EImportImporter *importer;
};

struct _ImportDestinationPage {
	GtkWidget *control;
};

struct _ImportTypePage {
	GtkWidget *intelligent;
	GtkWidget *file;
};

struct _ImportSelectionPage {
	GSList *importers;
	GSList *current;
	EImportTargetHome *target;
};

struct _ImportProgressPage {
	GtkWidget *progress_bar;
};

struct _ImportSimplePage {
	GtkWidget *actionlabel;
	GtkWidget *filetypetable;
	GtkWidget *filetype;
	GtkWidget *control; /* importer's destination or preview widget in an alignment */
	gboolean has_preview; /* TRUE when 'control' holds a preview widget,
				   otherwise holds destination widget */

	EImportTargetURI *target;
	EImportImporter *importer;
};

struct _EImportAssistantPrivate {
	ImportFilePage file_page;
	ImportDestinationPage destination_page;
	ImportTypePage type_page;
	ImportSelectionPage selection_page;
	ImportProgressPage progress_page;
	ImportSimplePage simple_page;

	EImport *import;

	gboolean is_simple;
	GPtrArray *fileuris; /* each element is a file URI, as a newly allocated string */

	/* Used for importing phase of operation */
	EImportTarget *import_target;
	EImportImporter *import_importer;
};

enum {
	PAGE_START,
	PAGE_INTELI_OR_DIRECT,
	PAGE_INTELI_SOURCE,
	PAGE_FILE_CHOOSE,
	PAGE_FILE_DEST,
	PAGE_FINISH,
	PAGE_PROGRESS
};

enum {
	PROP_0,
	PROP_IS_SIMPLE
};

enum {
	FINISHED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (
	EImportAssistant,
	e_import_assistant,
	GTK_TYPE_ASSISTANT,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

/* Importing functions */

static void
import_assistant_finished (EImportAssistant *import_assistant,
			   const GError *error)
{
	if (error)
		e_notice (import_assistant, GTK_MESSAGE_ERROR, "%s", error->message);

	g_signal_emit (import_assistant, signals[FINISHED], 0);
}

static void
filename_changed (GtkWidget *widget,
                  GtkAssistant *assistant)
{
	EImportAssistantPrivate *priv;
	ImportFilePage *page;
	const gchar *filename;
	gint fileok;

	priv = E_IMPORT_ASSISTANT_GET_PRIVATE (assistant);
	page = &priv->file_page;

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));

	fileok =
		filename != NULL && *filename != '\0' &&
		g_file_test (filename, G_FILE_TEST_IS_REGULAR);

	if (fileok) {
		GtkTreeIter iter;
		GtkTreeModel *model;
		gboolean valid;
		GSList *l;
		EImportImporter *first = NULL;
		gint i = 0, firstitem = 0;

		g_free (page->target->uri_src);
		page->target->uri_src = g_filename_to_uri (filename, NULL, NULL);

		l = e_import_get_importers (
			priv->import, (EImportTarget *) page->target);
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
			} else {
				if (page->importer == eii)
					page->importer = NULL;
				gtk_list_store_set (GTK_LIST_STORE (model), &iter, 1, FALSE, -1);
			}
			i++;
			valid = gtk_tree_model_iter_next (model, &iter);
		}
		g_slist_free (l);

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

	widget = gtk_assistant_get_nth_page (assistant, PAGE_FILE_CHOOSE);
	gtk_assistant_set_page_complete (assistant, widget, fileok);
}

static void
filetype_changed_cb (GtkComboBox *combo_box,
                     GtkAssistant *assistant)
{
	EImportAssistantPrivate *priv;
	GtkTreeModel *model;
	GtkTreeIter iter;

	priv = E_IMPORT_ASSISTANT_GET_PRIVATE (assistant);

	g_return_if_fail (gtk_combo_box_get_active_iter (combo_box, &iter));

	model = gtk_combo_box_get_model (combo_box);
	gtk_tree_model_get (model, &iter, 2, &priv->file_page.importer, -1);
	filename_changed (priv->file_page.filename, assistant);
}

static GtkWidget *
import_assistant_file_page_init (EImportAssistant *import_assistant)
{
	GtkWidget *page;
	GtkWidget *label;
	GtkWidget *container;
	GtkWidget *widget;
	GtkCellRenderer *cell;
	GtkListStore *store;
	const gchar *text;
	gint row = 0;

	page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (page), 12);
	gtk_widget_show (page);

	container = page;

	text = _("Choose the file that you want to import into Evolution, "
		 "and select what type of file it is from the list.");

	widget = gtk_label_new (text);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	gtk_widget_show (widget);

	widget = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (widget), 2);
	gtk_table_set_col_spacings (GTK_TABLE (widget), 10);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 8);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new_with_mnemonic (_("F_ilename:"));
	gtk_misc_set_alignment (GTK_MISC (widget), 1, 0.5);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 1, row, row + 1, GTK_FILL, 0, 0, 0);
	gtk_widget_show (widget);

	label = widget;

	widget = gtk_file_chooser_button_new (
		_("Select a file"), GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_table_attach (
		GTK_TABLE (container), widget, 1, 2,
		row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	import_assistant->priv->file_page.filename = widget;
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "selection-changed",
		G_CALLBACK (filename_changed), import_assistant);

	row++;

	widget = gtk_label_new_with_mnemonic (_("File _type:"));
	gtk_misc_set_alignment (GTK_MISC (widget), 1, 0.5);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 1, row, row + 1, GTK_FILL, 0, 0, 0);
	gtk_widget_show (widget);

	label = widget;

	store = gtk_list_store_new (
		3, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_POINTER);
	widget = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	import_assistant->priv->file_page.filetype = widget;
	gtk_widget_show (widget);
	g_object_unref (store);

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), cell, TRUE);
	gtk_cell_layout_set_attributes (
		GTK_CELL_LAYOUT (widget), cell,
		"text", 0, "sensitive", 1, NULL);

	return page;
}

static GtkWidget *
import_assistant_destination_page_init (EImportAssistant *import_assistant)
{
	GtkWidget *page;
	GtkWidget *container;
	GtkWidget *widget;
	const gchar *text;

	page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (page), 12);
	gtk_widget_show (page);

	container = page;

	text = _("Choose the destination for this import");

	widget = gtk_label_new (text);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	gtk_widget_show (widget);

	return page;
}

static GtkWidget *
import_assistant_type_page_init (EImportAssistant *import_assistant)
{
	GtkRadioButton *radio_button;
	GtkWidget *page;
	GtkWidget *container;
	GtkWidget *widget;
	const gchar *text;

	page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (page), 12);
	gtk_widget_show (page);

	container = page;

	text = _("Choose the type of importer to run:");

	widget = gtk_label_new (text);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	gtk_widget_show (widget);

	widget = gtk_radio_button_new_with_mnemonic (
		NULL, _("Import data and settings from _older programs"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	import_assistant->priv->type_page.intelligent = widget;
	gtk_widget_show (widget);

	radio_button = GTK_RADIO_BUTTON (widget);

	widget = gtk_radio_button_new_with_mnemonic_from_widget (
		radio_button, _("Import a _single file"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	import_assistant->priv->type_page.file = widget;
	gtk_widget_show (widget);

	return page;
}

static GtkWidget *
import_assistant_selection_page_init (EImportAssistant *import_assistant)
{
	GtkWidget *page;
	GtkWidget *container;
	GtkWidget *widget;
	const gchar *text;

	page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (page), 12);
	gtk_widget_show (page);

	container = page;

	text = _("Please select the information "
		 "that you would like to import:");

	widget = gtk_label_new (text);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	gtk_widget_show (widget);

	widget = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	return page;
}

static GtkWidget *
import_assistant_progress_page_init (EImportAssistant *import_assistant)
{
	GtkWidget *page;
	GtkWidget *container;
	GtkWidget *widget;

	page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (page), 12);
	gtk_widget_show (page);

	container = page;

	widget = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, FALSE, 0);
	import_assistant->priv->progress_page.progress_bar = widget;
	gtk_widget_show (widget);

	return page;
}

static GtkWidget *
import_assistant_simple_page_init (EImportAssistant *import_assistant)
{
	GtkWidget *page;
	GtkWidget *label;
	GtkWidget *container;
	GtkWidget *widget;
	GtkCellRenderer *cell;
	GtkListStore *store;
	gint row = 0;

	page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (page), 12);
	gtk_widget_show (page);

	container = page;

	widget = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	gtk_widget_show (widget);
	import_assistant->priv->simple_page.actionlabel = widget;

	widget = gtk_table_new (2, 1, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (widget), 2);
	gtk_table_set_col_spacings (GTK_TABLE (widget), 10);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 8);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	gtk_widget_show (widget);
	import_assistant->priv->simple_page.filetypetable = widget;

	container = widget;

	widget = gtk_label_new_with_mnemonic (_("File _type:"));
	gtk_misc_set_alignment (GTK_MISC (widget), 1, 0.5);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 1, row, row + 1, GTK_FILL, 0, 0, 0);
	gtk_widget_show (widget);

	label = widget;

	store = gtk_list_store_new (
		3, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_POINTER);
	widget = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	import_assistant->priv->simple_page.filetype = widget;
	gtk_widget_show (widget);
	g_object_unref (store);

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), cell, TRUE);
	gtk_cell_layout_set_attributes (
		GTK_CELL_LAYOUT (widget), cell,
		"text", 0, "sensitive", 1, NULL);

	import_assistant->priv->simple_page.control = NULL;

	return page;
}

static void
prepare_intelligent_page (GtkAssistant *assistant,
                          GtkWidget *vbox)
{
	EImportAssistantPrivate *priv;
	GSList *l;
	GtkWidget *table;
	gint row;
	ImportSelectionPage *page;

	priv = E_IMPORT_ASSISTANT_GET_PRIVATE (assistant);
	page = &priv->selection_page;

	if (page->target != NULL) {
		gtk_assistant_set_page_complete (assistant, vbox, FALSE);
		return;
	}

	page->target = e_import_target_new_home (priv->import);

	if (page->importers)
		g_slist_free (page->importers);
	l = page->importers =
		e_import_get_importers (
			priv->import, (EImportTarget *) page->target);

	if (l == NULL) {
		GtkWidget *widget;
		const gchar *text;

		text = _("Evolution checked for settings to import from "
			 "the following applications: Pine, Netscape, Elm, "
			 "iCalendar, KMail. No importable settings found. If you "
			 "would like to try again, please click the "
			 "\"Back\" button.");

		widget = gtk_label_new (text);
		gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
		gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, TRUE, 0);
		gtk_widget_show (widget);

		gtk_assistant_set_page_complete (assistant, vbox, FALSE);

		return;
	}

	table = gtk_table_new (g_slist_length (l), 2, FALSE);
	row = 0;
	for (; l; l = l->next) {
		EImportImporter *eii = l->data;
		gchar *str;
		GtkWidget *w, *label;

		w = e_import_get_widget (
			priv->import, (EImportTarget *) page->target, eii);

		str = g_strdup_printf (_("From %s:"), eii->name);
		label = gtk_label_new (str);
		gtk_widget_show (label);
		g_free (str);

		gtk_misc_set_alignment (GTK_MISC (label), 0, .5);

		gtk_table_attach (
			GTK_TABLE (table), label,
			0, 1, row, row + 1, GTK_FILL, 0, 0, 0);
		if (w)
			gtk_table_attach (
				GTK_TABLE (table), w,
				1, 2, row, row + 1, GTK_FILL, 0, 3, 0);
		row++;
	}

	gtk_widget_show (table);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);

	gtk_assistant_set_page_complete (assistant, vbox, TRUE);
}

static void
import_status (EImport *import,
               const gchar *what,
               gint percent,
               gpointer user_data)
{
	EImportAssistant *import_assistant = user_data;
	GtkProgressBar *progress_bar;

	progress_bar = GTK_PROGRESS_BAR (
		import_assistant->priv->progress_page.progress_bar);
	gtk_progress_bar_set_fraction (progress_bar, percent / 100.0);
	gtk_progress_bar_set_text (progress_bar, what);
}

static void
import_done (EImport *ei,
	     const GError *error,
             gpointer user_data)
{
	EImportAssistant *import_assistant = user_data;

	import_assistant_finished (import_assistant, error);
}

static void
import_simple_done (EImport *ei,
		    const GError *error,
                    gpointer user_data)
{
	EImportAssistant *import_assistant = user_data;
	EImportAssistantPrivate *priv;

	g_return_if_fail (import_assistant != NULL);

	priv = import_assistant->priv;
	g_return_if_fail (priv != NULL);
	g_return_if_fail (priv->fileuris != NULL);
	g_return_if_fail (priv->simple_page.target != NULL);

	if (!error && import_assistant->priv->fileuris->len > 0) {
		import_status (ei, "", 0, import_assistant);

		/* process next file URI */
		g_free (priv->simple_page.target->uri_src);
		priv->simple_page.target->uri_src =
			g_ptr_array_remove_index (priv->fileuris, 0);

		e_import_import (
			priv->import, priv->import_target,
			priv->import_importer, import_status,
			import_simple_done, import_assistant);
	} else
		import_done (ei, error, import_assistant);
}

static void
import_intelligent_done (EImport *ei,
			 const GError *error,
                         gpointer user_data)
{
	EImportAssistant *import_assistant = user_data;
	ImportSelectionPage *page;

	page = &import_assistant->priv->selection_page;

	if (!error && page->current && (page->current = page->current->next)) {
		import_status (ei, "", 0, import_assistant);
		import_assistant->priv->import_importer = page->current->data;
		e_import_import (
			import_assistant->priv->import,
			(EImportTarget *) page->target,
			import_assistant->priv->import_importer,
			import_status, import_intelligent_done,
			import_assistant);
	} else
		import_done (ei, error, import_assistant);
}

static void
import_cancelled (EImportAssistant *assistant)
{
	e_import_cancel (
		assistant->priv->import,
		assistant->priv->import_target,
		assistant->priv->import_importer);
}

static void
prepare_file_page (GtkAssistant *assistant,
                   GtkWidget *vbox)
{
	EImportAssistantPrivate *priv;
	GSList *importers, *imp;
	GtkListStore *store;
	ImportFilePage *page;

	priv = E_IMPORT_ASSISTANT_GET_PRIVATE (assistant);
	page = &priv->file_page;

	if (page->target != NULL) {
		filename_changed (priv->file_page.filename, assistant);
		return;
	}

	page->target = e_import_target_new_uri (priv->import, NULL, NULL);
	importers = e_import_get_importers (priv->import, (EImportTarget *) page->target);

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

	filename_changed (priv->file_page.filename, assistant);

	g_signal_connect (
		page->filetype, "changed",
		G_CALLBACK (filetype_changed_cb), assistant);
}

static GtkWidget *
create_importer_control (EImport *import,
                         EImportTarget *target,
                         EImportImporter *importer)
{
	GtkWidget *control;

	control = e_import_get_widget (import, target, importer);
	if (control == NULL) {
		/* Coding error, not needed for translators */
		control = gtk_label_new (
			"** PLUGIN ERROR ** No settings for importer");
		gtk_widget_show (control);
	}

	return control;
}

static gboolean
prepare_destination_page (GtkAssistant *assistant,
                          GtkWidget *vbox)
{
	EImportAssistantPrivate *priv;
	ImportDestinationPage *page;

	priv = E_IMPORT_ASSISTANT_GET_PRIVATE (assistant);
	page = &priv->destination_page;

	if (page->control)
		gtk_container_remove (GTK_CONTAINER (vbox), page->control);

	page->control = create_importer_control (
		priv->import, (EImportTarget *)
		priv->file_page.target, priv->file_page.importer);

	gtk_box_pack_start (GTK_BOX (vbox), page->control, TRUE, TRUE, 0);
	gtk_assistant_set_page_complete (assistant, vbox, TRUE);

	return FALSE;
}

static void
prepare_progress_page (GtkAssistant *assistant,
                       GtkWidget *vbox)
{
	EImportAssistantPrivate *priv;
	EImportCompleteFunc done = NULL;
	ImportSelectionPage *page;
	GtkWidget *cancel_button;
	gboolean intelligent_import;
	gboolean is_simple = FALSE;

	priv = E_IMPORT_ASSISTANT_GET_PRIVATE (assistant);
	page = &priv->selection_page;

	/* Because we're a GTK_ASSISTANT_PAGE_PROGRESS, this will
	 * prevent the assistant window from being closed via window
	 * manager decorations while importing. */
	gtk_assistant_commit (assistant);

	/* Install a custom "Cancel Import" button. */
	cancel_button = e_dialog_button_new_with_icon ("process-stop", _("_Cancel Import"));
	g_signal_connect_swapped (
		cancel_button, "clicked",
		G_CALLBACK (import_cancelled), assistant);
	gtk_assistant_add_action_widget (assistant, cancel_button);
	gtk_widget_show (cancel_button);

	g_object_get (assistant, "is-simple", &is_simple, NULL);

	intelligent_import = is_simple ? FALSE : gtk_toggle_button_get_active (
		GTK_TOGGLE_BUTTON (priv->type_page.intelligent));

	if (is_simple) {
		priv->import_importer = priv->simple_page.importer;
		priv->import_target = (EImportTarget *) priv->simple_page.target;
		done = import_simple_done;
	} else if (intelligent_import) {
		page->current = page->importers;
		if (page->current) {
			priv->import_target = (EImportTarget *) page->target;
			priv->import_importer = page->current->data;
			done = import_intelligent_done;
		}
	} else {
		if (priv->file_page.importer) {
			priv->import_importer = priv->file_page.importer;
			priv->import_target = (EImportTarget *) priv->file_page.target;
			done = import_done;
		}
	}

	if (done)
		e_import_import (
			priv->import, priv->import_target,
			priv->import_importer, import_status,
			done, assistant);
	else
		import_assistant_finished (E_IMPORT_ASSISTANT (assistant), NULL);
}

static void
simple_filetype_changed_cb (GtkComboBox *combo_box,
                            GtkAssistant *assistant)
{
	EImportAssistantPrivate *priv;
	ImportSimplePage *page;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *vbox;
	GtkWidget *control;

	priv = E_IMPORT_ASSISTANT_GET_PRIVATE (assistant);
	page = &priv->simple_page;

	g_return_if_fail (gtk_combo_box_get_active_iter (combo_box, &iter));

	model = gtk_combo_box_get_model (combo_box);
	gtk_tree_model_get (model, &iter, 2, &page->importer, -1);

	vbox = g_object_get_data (G_OBJECT (combo_box), "page-vbox");
	g_return_if_fail (vbox != NULL);

	if (page->control)
		gtk_widget_destroy (page->control);
	page->has_preview = FALSE;

	control = e_import_get_preview_widget (
		priv->import, (EImportTarget *)
		page->target, page->importer);
	if (control) {
		page->has_preview = TRUE;
		gtk_widget_set_size_request (control, 440, 360);
	} else
		control = create_importer_control (
			priv->import, (EImportTarget *)
			page->target, page->importer);

	page->control = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_widget_show (page->control);
	gtk_container_add (GTK_CONTAINER (page->control), control);

	gtk_box_pack_start (GTK_BOX (vbox), page->control, TRUE, TRUE, 0);
	gtk_assistant_set_page_complete (assistant, vbox, TRUE);
}

static void
prepare_simple_page (GtkAssistant *assistant,
                     GtkWidget *vbox)
{
	EImportAssistantPrivate *priv;
	GSList *importers, *imp;
	GtkListStore *store;
	ImportSimplePage *page;
	gchar *uri;

	priv = E_IMPORT_ASSISTANT_GET_PRIVATE (assistant);
	page = &priv->simple_page;

	g_return_if_fail (priv->fileuris != NULL);

	if (page->target != NULL) {
		return;
	}

	uri = g_ptr_array_remove_index (priv->fileuris, 0);
	page->target = e_import_target_new_uri (priv->import, uri, NULL);
	g_free (uri);
	importers = e_import_get_importers (priv->import, (EImportTarget *) page->target);

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

	gtk_combo_box_set_active (GTK_COMBO_BOX (page->filetype), 0);
	g_object_set_data (G_OBJECT (page->filetype), "page-vbox", vbox);

	simple_filetype_changed_cb (GTK_COMBO_BOX (page->filetype), assistant);

	g_signal_connect (
		page->filetype, "changed",
		G_CALLBACK (simple_filetype_changed_cb), assistant);

	if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) == 1) {
		gchar *title;

		/* only one importer found, make it even simpler */
		gtk_label_set_text (
			GTK_LABEL (page->actionlabel),
			page->has_preview ?
				_("Preview data to be imported") :
				_("Choose the destination for this import"));

		gtk_widget_hide (page->filetypetable);

		title = g_strconcat (
			_("Import Data"), " - ",
			((EImportImporter *) importers->data)->name, NULL);
		gtk_assistant_set_page_title (assistant, vbox, title);
		g_free (title);
	} else {
		/* multiple importers found, be able to choose from them */
		gtk_label_set_text (
			GTK_LABEL (page->actionlabel),
			_("Select what type of file you "
			"want to import from the list."));

		gtk_widget_show (page->filetypetable);

		gtk_assistant_set_page_title (assistant, vbox, _("Import Data"));
	}

	g_slist_free (importers);
}

static gboolean
prepare_simple_destination_page (GtkAssistant *assistant,
                          GtkWidget *vbox)
{
	EImportAssistantPrivate *priv;
	ImportDestinationPage *page;
	ImportSimplePage *simple_page;

	priv = E_IMPORT_ASSISTANT_GET_PRIVATE (assistant);
	page = &priv->destination_page;
	simple_page = &priv->simple_page;

	if (page->control)
		gtk_container_remove (GTK_CONTAINER (vbox), page->control);

	page->control = create_importer_control (
		priv->import, (EImportTarget *)
		simple_page->target, simple_page->importer);

	gtk_box_pack_start (GTK_BOX (vbox), page->control, TRUE, TRUE, 0);
	gtk_assistant_set_page_complete (assistant, vbox, TRUE);

	return FALSE;
}

static gint
forward_cb (gint current_page,
            EImportAssistant *import_assistant)
{
	GtkToggleButton *toggle_button;
	gboolean is_simple = FALSE;

	g_object_get (import_assistant, "is-simple", &is_simple, NULL);

	if (is_simple) {
		if (!import_assistant->priv->simple_page.has_preview)
			current_page++;

		return current_page + 1;
	}

	toggle_button = GTK_TOGGLE_BUTTON (
		import_assistant->priv->type_page.intelligent);

	switch (current_page) {
		case PAGE_INTELI_OR_DIRECT:
			if (gtk_toggle_button_get_active (toggle_button))
				return PAGE_INTELI_SOURCE;
			else
				return PAGE_FILE_CHOOSE;
		case PAGE_INTELI_SOURCE:
			return PAGE_FINISH;
	}

	return current_page + 1;
}

static gboolean
set_import_uris (EImportAssistant *assistant,
                 const gchar * const *uris)
{
	EImportAssistantPrivate *priv;
	GPtrArray *fileuris = NULL;
	gint i;

	g_return_val_if_fail (assistant != NULL, FALSE);
	g_return_val_if_fail (assistant->priv != NULL, FALSE);
	g_return_val_if_fail (assistant->priv->import != NULL, FALSE);
	g_return_val_if_fail (uris != NULL, FALSE);

	priv = E_IMPORT_ASSISTANT_GET_PRIVATE (assistant);

	for (i = 0; uris[i]; i++) {
		const gchar *uri = uris[i];
		gchar *filename;

		filename = g_filename_from_uri (uri, NULL, NULL);
		if (!filename)
			filename = g_strdup (uri);

		if (filename && *filename &&
		    g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
			gchar *furi;

			if (!g_path_is_absolute (filename)) {
				gchar *tmp, *curr;

				curr = g_get_current_dir ();
				tmp = g_build_filename (curr, filename, NULL);
				g_free (curr);

				g_free (filename);
				filename = tmp;
			}

			if (fileuris == NULL) {
				EImportTargetURI *target;
				GSList *importers;

				furi = g_filename_to_uri (filename, NULL, NULL);
				target = e_import_target_new_uri (priv->import, furi, NULL);
				importers = e_import_get_importers (
					priv->import, (EImportTarget *) target);

				if (importers != NULL) {
					/* there is at least one importer which can be used,
					 * thus there can be done an import */
					fileuris = g_ptr_array_new ();
				}

				g_slist_free (importers);
				e_import_target_free (priv->import, target);
				g_free (furi);

				if (fileuris == NULL) {
					g_free (filename);
					break;
				}
			}

			furi = g_filename_to_uri (filename, NULL, NULL);
			if (furi)
				g_ptr_array_add (fileuris, furi);
		}

		g_free (filename);
	}

	if (fileuris != NULL) {
		priv->fileuris = fileuris;
	}

	return fileuris != NULL;
}

static void
import_assistant_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	EImportAssistantPrivate *priv;

	priv = E_IMPORT_ASSISTANT_GET_PRIVATE (object);

	switch (property_id) {
		case PROP_IS_SIMPLE:
			priv->is_simple = g_value_get_boolean (value);
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
import_assistant_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	EImportAssistantPrivate *priv;

	priv = E_IMPORT_ASSISTANT_GET_PRIVATE (object);

	switch (property_id) {
		case PROP_IS_SIMPLE:
			g_value_set_boolean (value, priv->is_simple);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
import_assistant_dispose (GObject *object)
{
	EImportAssistantPrivate *priv;

	priv = E_IMPORT_ASSISTANT_GET_PRIVATE (object);

	if (priv->file_page.target != NULL) {
		e_import_target_free (
			priv->import, (EImportTarget *)
			priv->file_page.target);
		priv->file_page.target = NULL;
	}

	if (priv->selection_page.target != NULL) {
		e_import_target_free (
			priv->import, (EImportTarget *)
			priv->selection_page.target);
		priv->selection_page.target = NULL;
	}

	if (priv->simple_page.target != NULL) {
		e_import_target_free (
			priv->import, (EImportTarget *)
			priv->simple_page.target);
		priv->simple_page.target = NULL;
	}

	if (priv->import != NULL) {
		g_object_unref (priv->import);
		priv->import = NULL;
	}

	if (priv->fileuris != NULL) {
		g_ptr_array_foreach (priv->fileuris, (GFunc) g_free, NULL);
		g_ptr_array_free (priv->fileuris, TRUE);
		priv->fileuris = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_import_assistant_parent_class)->dispose (object);
}

static void
import_assistant_finalize (GObject *object)
{
	EImportAssistantPrivate *priv;

	priv = E_IMPORT_ASSISTANT_GET_PRIVATE (object);

	g_slist_free (priv->selection_page.importers);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_import_assistant_parent_class)->finalize (object);
}

static gboolean
import_assistant_key_press_event (GtkWidget *widget,
                                  GdkEventKey *event)
{
	GtkWidgetClass *parent_class;

	if (event->keyval == GDK_KEY_Escape) {
		g_signal_emit_by_name (widget, "cancel");
		return TRUE;
	}

	/* Chain up to parent's key_press_event () method. */
	parent_class = GTK_WIDGET_CLASS (e_import_assistant_parent_class);
	return parent_class->key_press_event (widget, event);
}

static void
import_assistant_prepare (GtkAssistant *assistant,
                          GtkWidget *page)
{
	gint page_no = gtk_assistant_get_current_page (assistant);
	gboolean is_simple = FALSE;

	g_object_get (assistant, "is-simple", &is_simple, NULL);

	if (is_simple) {
		if (page_no == 0) {
			prepare_simple_page (assistant, page);
		} else if (page_no == 1) {
			prepare_simple_destination_page (assistant, page);
		} else if (page_no == 2) {
			prepare_progress_page (assistant, page);
		}

		return;
	}

	switch (page_no) {
		case PAGE_INTELI_SOURCE:
			prepare_intelligent_page (assistant, page);
			break;
		case PAGE_FILE_CHOOSE:
			prepare_file_page (assistant, page);
			break;
		case PAGE_FILE_DEST:
			prepare_destination_page (assistant, page);
			break;
		case PAGE_PROGRESS:
			prepare_progress_page (assistant, page);
			break;
		default:
			break;
	}
}

static void
e_import_assistant_class_init (EImportAssistantClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkAssistantClass *assistant_class;

	g_type_class_add_private (class, sizeof (EImportAssistantPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = import_assistant_dispose;
	object_class->finalize = import_assistant_finalize;
	object_class->set_property = import_assistant_set_property;
	object_class->get_property = import_assistant_get_property;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->key_press_event = import_assistant_key_press_event;

	#if GTK_CHECK_VERSION (3, 20, 0)
	gtk_widget_class_set_css_name (widget_class, G_OBJECT_CLASS_NAME (class));
	#endif

	assistant_class = GTK_ASSISTANT_CLASS (class);
	assistant_class->prepare = import_assistant_prepare;

	g_object_class_install_property (
		object_class,
		PROP_IS_SIMPLE,
		g_param_spec_boolean (
			"is-simple",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	signals[FINISHED] = g_signal_new (
		"finished",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
import_assistant_construct (EImportAssistant *import_assistant)
{
	GtkAssistant *assistant;
	GtkWidget *page;

	assistant = GTK_ASSISTANT (import_assistant);

	import_assistant->priv->import =
		e_import_new ("org.gnome.evolution.shell.importer");

	gtk_window_set_position (GTK_WINDOW (assistant), GTK_WIN_POS_CENTER);
	gtk_window_set_title (GTK_WINDOW (assistant), _("Evolution Import Assistant"));
	gtk_window_set_default_size (GTK_WINDOW (assistant), 500, 330);

	e_extensible_load_extensions (E_EXTENSIBLE (import_assistant));

	if (import_assistant->priv->is_simple) {
		/* simple import assistant page, URIs of files will be known later */
		page = import_assistant_simple_page_init (import_assistant);

		gtk_assistant_append_page (assistant, page);
		gtk_assistant_set_page_title (assistant, page, _("Import Data"));
		gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONTENT);

		/* File destination page - when with preview*/
		page = import_assistant_destination_page_init (import_assistant);

		gtk_assistant_append_page (assistant, page);
		gtk_assistant_set_page_title (assistant, page, _("Import Location"));
		gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONTENT);
	} else {
		/* complex import assistant pages */

		/* Start page */
		page = gtk_label_new ("");
		gtk_label_set_line_wrap (GTK_LABEL (page), TRUE);
		gtk_misc_set_alignment (GTK_MISC (page), 0.0, 0.5);
		gtk_misc_set_padding (GTK_MISC (page), 12, 12);
		gtk_label_set_text (GTK_LABEL (page), _(
			"Welcome to the Evolution Import Assistant.\n"
			"With this assistant you will be guided through the "
			"process of importing external files into Evolution."));
		gtk_widget_show (page);

		gtk_assistant_append_page (assistant, page);
		gtk_assistant_set_page_title (
			assistant, page, _("Evolution Import Assistant"));
		gtk_assistant_set_page_type (
			assistant, page, GTK_ASSISTANT_PAGE_INTRO);
		gtk_assistant_set_page_complete (assistant, page, TRUE);

		/* Intelligent or direct import page */
		page = import_assistant_type_page_init (import_assistant);

		gtk_assistant_append_page (assistant, page);
		gtk_assistant_set_page_title (
			assistant, page, _("Importer Type"));
		gtk_assistant_set_page_type (
			assistant, page, GTK_ASSISTANT_PAGE_CONTENT);
		gtk_assistant_set_page_complete (assistant, page, TRUE);

		/* Intelligent importer source page */
		page = import_assistant_selection_page_init (import_assistant);

		gtk_assistant_append_page (assistant, page);
		gtk_assistant_set_page_title (
			assistant, page, _("Select Information to Import"));
		gtk_assistant_set_page_type (
			assistant, page, GTK_ASSISTANT_PAGE_CONTENT);

		/* File selection and file type page */
		page = import_assistant_file_page_init (import_assistant);

		gtk_assistant_append_page (assistant, page);
		gtk_assistant_set_page_title (
			assistant, page, _("Select a File"));
		gtk_assistant_set_page_type (
			assistant, page, GTK_ASSISTANT_PAGE_CONTENT);

		/* File destination page */
		page = import_assistant_destination_page_init (import_assistant);

		gtk_assistant_append_page (assistant, page);
		gtk_assistant_set_page_title (
			assistant, page, _("Import Location"));
		gtk_assistant_set_page_type (
			assistant, page, GTK_ASSISTANT_PAGE_CONTENT);

		/* Finish page */
		page = gtk_label_new ("");
		gtk_misc_set_alignment (GTK_MISC (page), 0.5, 0.5);
		gtk_label_set_text (
			GTK_LABEL (page), _("Click \"Apply\" to "
			"begin importing the file into Evolution."));
		gtk_widget_show (page);

		gtk_assistant_append_page (assistant, page);
		gtk_assistant_set_page_title (assistant, page, _("Import Data"));
		gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONFIRM);
		gtk_assistant_set_page_complete (assistant, page, TRUE);
	}

	/* Progress Page */
	page = import_assistant_progress_page_init (import_assistant);

	gtk_assistant_append_page (assistant, page);
	gtk_assistant_set_page_title (assistant, page, _("Import Data"));
	gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_PROGRESS);
	gtk_assistant_set_page_complete (assistant, page, TRUE);

	gtk_assistant_set_forward_page_func (
		assistant, (GtkAssistantPageFunc)
		forward_cb, import_assistant, NULL);

	gtk_assistant_update_buttons_state (assistant);
}

static void
e_import_assistant_init (EImportAssistant *import_assistant)
{
	import_assistant->priv =
		E_IMPORT_ASSISTANT_GET_PRIVATE (import_assistant);
}

GtkWidget *
e_import_assistant_new (GtkWindow *parent)
{
	GtkWidget *assistant;

	assistant = g_object_new (
			E_TYPE_IMPORT_ASSISTANT,
			"transient-for", parent, NULL);

	import_assistant_construct (E_IMPORT_ASSISTANT (assistant));

	return assistant;
}

/* Creates a simple assistant with only page to choose an import type
 * and where to import, and then finishes. It shows import types based
 * on the first valid URI given.
 *
 * Returns: EImportAssistant widget.
 */
GtkWidget *
e_import_assistant_new_simple (GtkWindow *parent,
                               const gchar * const *uris)
{
	GtkWidget *assistant;

	assistant = g_object_new (
		E_TYPE_IMPORT_ASSISTANT,
		"transient-for", parent,
		"is-simple", TRUE,
		NULL);

	import_assistant_construct (E_IMPORT_ASSISTANT (assistant));

	if (!set_import_uris (E_IMPORT_ASSISTANT (assistant), uris)) {
		g_object_ref_sink (assistant);
		g_object_unref (assistant);
		return NULL;
	}

	return assistant;
}
