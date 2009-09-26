/*
 * e-import-assistant.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-import-assistant.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <glib/gi18n.h>
#include <glade/glade.h>
#include <gdk/gdkkeysyms.h>

#include "e-util/e-error.h"
#include "e-util/e-import.h"
#include "e-util/e-util-private.h"

#define E_IMPORT_ASSISTANT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_IMPORT_ASSISTANT, EImportAssistantPrivate))

typedef struct _ImportFilePage ImportFilePage;
typedef struct _ImportDestinationPage ImportDestinationPage;
typedef struct _ImportTypePage ImportTypePage;
typedef struct _ImportSelectionPage ImportSelectionPage;

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

struct _EImportAssistantPrivate {
	ImportFilePage file_page;
	ImportDestinationPage destination_page;
	ImportTypePage type_page;
	ImportSelectionPage selection_page;

	EImport *import;

	/* Used for importing phase of operation */
	EImportTarget *import_target;
	EImportImporter *import_importer;
	GtkWidget *import_dialog;
	GtkWidget *import_label;
	GtkWidget *import_progress;
};

enum {
	PAGE_START,
	PAGE_INTELI_OR_DIRECT,
	PAGE_INTELI_SOURCE,
	PAGE_FILE_CHOOSE,
	PAGE_FILE_DEST,
	PAGE_FINISH
};

enum {
	FINISHED,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

/* Importing functions */

static void
import_assistant_emit_finished (EImportAssistant *import_assistant)
{
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

	fileok = filename && filename[0] && g_file_test (filename, G_FILE_TEST_IS_REGULAR);
	if (fileok) {
		GtkTreeIter iter;
		GtkTreeModel *model;
		gboolean valid;
		GSList *l;
		EImportImporter *first = NULL;
		gint i=0, firstitem=0;

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
				fileok = TRUE;
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

	page = gtk_vbox_new (FALSE, 6);
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

	page = gtk_vbox_new (FALSE, 6);
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

	page = gtk_vbox_new (FALSE, 6);
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

	page = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (page), 12);
	gtk_widget_show (page);

	container = page;

	text = _("Please select the information "
		 "that you would like to import:");

	widget = gtk_label_new (text);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	gtk_widget_show (widget);

	widget = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

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
			 "the following\napplications: Pine, Netscape, Elm, "
			 "iCalendar. No importable\nsettings found. If you "
			 "would like to\ntry again, please click the "
			 "\"Back\" button.\n");

		widget = gtk_label_new (text);
		gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
		gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, TRUE, 0);
		gtk_widget_show (widget);

		gtk_assistant_set_page_complete (assistant, vbox, FALSE);

		return;
	}

	table = gtk_table_new (g_slist_length (l), 2, FALSE);
	row = 0;
	for (;l;l=l->next) {
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
			0, 1, row, row+1, GTK_FILL, 0, 0, 0);
		if (w)
			gtk_table_attach (
				GTK_TABLE (table), w,
				1, 2, row, row+1, GTK_FILL, 0, 3, 0);
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
		import_assistant->priv->import_progress);
	gtk_progress_bar_set_fraction (progress_bar, percent / 100.0);
	gtk_progress_bar_set_text (progress_bar, what);
}

static void
import_dialog_response (GtkDialog *dialog,
                        guint button,
                        EImportAssistant *import_assistant)
{
	if (button == GTK_RESPONSE_CANCEL)
		e_import_cancel (
			import_assistant->priv->import,
			import_assistant->priv->import_target,
			import_assistant->priv->import_importer);
}

static void
import_done (EImport *ei,
             gpointer user_data)
{
	EImportAssistant *import_assistant = user_data;

	import_assistant_emit_finished (import_assistant);
}

static void
import_intelligent_done (EImport *ei,
                         gpointer user_data)
{
	EImportAssistant *import_assistant = user_data;
	ImportSelectionPage *page;

	page = &import_assistant->priv->selection_page;

	if (page->current && (page->current = page->current->next)) {
		import_status (ei, "", 0, import_assistant);
		import_assistant->priv->import_importer = page->current->data;
		e_import_import (
			import_assistant->priv->import,
			(EImportTarget *) page->target,
			import_assistant->priv->import_importer,
			import_status, import_intelligent_done,
			import_assistant);
	} else
		import_done (ei, import_assistant);
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
	importers = e_import_get_importers (priv->import, (EImportTarget *)page->target);

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

	page->control = e_import_get_widget (
		priv->import, (EImportTarget *)
		priv->file_page.target, priv->file_page.importer);
	if (page->control == NULL) {
		/* Coding error, not needed for translators */
		page->control = gtk_label_new (
			"** PLUGIN ERROR ** No settings for importer");
		gtk_widget_show (page->control);
	}

	gtk_box_pack_start (GTK_BOX (vbox), page->control, TRUE, TRUE, 0);
	gtk_assistant_set_page_complete (assistant, vbox, TRUE);

	return FALSE;
}

static gint
forward_cb (gint current_page,
            EImportAssistant *import_assistant)
{
	GtkToggleButton *toggle_button;

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

	if (priv->import != NULL) {
		g_object_unref (priv->import);
		priv->import = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
import_assistant_finalize (GObject *object)
{
	EImportAssistantPrivate *priv;

	priv = E_IMPORT_ASSISTANT_GET_PRIVATE (object);

	g_slist_free (priv->selection_page.importers);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
import_assistant_key_press_event (GtkWidget *widget,
                                  GdkEventKey *event)
{
	GtkWidgetClass *widget_class;

	if (event->keyval == GDK_Escape) {
		g_signal_emit_by_name (widget, "cancel");
		return TRUE;
	}

	/* Chain up to parent's key_press_event () method. */
	widget_class = GTK_WIDGET_CLASS (parent_class);
	return widget_class->key_press_event (widget, event);
}

static void
import_assistant_prepare (GtkAssistant *assistant,
                          GtkWidget *page)
{
	switch (gtk_assistant_get_current_page (assistant)) {
		case PAGE_INTELI_SOURCE:
			prepare_intelligent_page (assistant, page);
			break;
		case PAGE_FILE_CHOOSE:
			prepare_file_page (assistant, page);
			break;
		case PAGE_FILE_DEST:
			prepare_destination_page (assistant, page);
			break;
		default:
			break;
	}
}

static void
import_assistant_apply (GtkAssistant *assistant)
{
	EImportAssistantPrivate *priv;
	EImportCompleteFunc done = NULL;
	GtkToggleButton *toggle_button;
	ImportSelectionPage *page;
	gchar *msg = NULL;

	priv = E_IMPORT_ASSISTANT_GET_PRIVATE (assistant);
	page = &priv->selection_page;

	toggle_button = GTK_TOGGLE_BUTTON (priv->type_page.intelligent);

	if (gtk_toggle_button_get_active (toggle_button)) {
		page->current = page->importers;
		if (page->current) {
			priv->import_target = (EImportTarget *) page->target;
			priv->import_importer = page->current->data;
			done = import_intelligent_done;
			msg = g_strdup (_("Importing Data"));
		}
	} else {
		if (priv->file_page.importer) {
			priv->import_importer = priv->file_page.importer;
			priv->import_target = (EImportTarget *)priv->file_page.target;
			done = import_done;
			msg = g_strdup_printf (
				_("Importing \"%s\""),
				priv->file_page.target->uri_src);
		}
	}

	if (done) {
		GtkWidget *content_area;

		priv->import_dialog = e_error_new (
			GTK_WINDOW (assistant), "shell:importing", msg, NULL);
		g_signal_connect (priv->import_dialog, "response", G_CALLBACK(import_dialog_response), assistant);
		priv->import_label = gtk_label_new (_("Please wait"));
		priv->import_progress = gtk_progress_bar_new ();
		content_area = gtk_dialog_get_content_area (
			GTK_DIALOG (priv->import_dialog));
		gtk_box_pack_start (
			GTK_BOX (content_area),
			priv->import_label, FALSE, FALSE, 0);
		gtk_box_pack_start (
			GTK_BOX (content_area),
			priv->import_progress, FALSE, FALSE, 0);
		gtk_widget_show_all (priv->import_dialog);

		e_import_import (
			priv->import, priv->import_target,
			priv->import_importer, import_status,
			import_done, assistant);
	} else {
		import_assistant_emit_finished (E_IMPORT_ASSISTANT (assistant));
	}

	g_free (msg);
}

static void
import_assistant_class_init (EImportAssistantClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkAssistantClass *assistant_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EImportAssistantPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = import_assistant_dispose;
	object_class->finalize = import_assistant_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->key_press_event = import_assistant_key_press_event;

	assistant_class = GTK_ASSISTANT_CLASS (class);
	assistant_class->prepare = import_assistant_prepare;
	assistant_class->apply = import_assistant_apply;

	signals[FINISHED] = g_signal_new (
		"finished",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
import_assistant_init (EImportAssistant *import_assistant)
{
	GtkAssistant *assistant;
	GtkIconTheme *icon_theme;
	GtkWidget *page;
	GdkPixbuf *pixbuf;
	gint size;

	assistant = GTK_ASSISTANT (import_assistant);

	import_assistant->priv =
		E_IMPORT_ASSISTANT_GET_PRIVATE (import_assistant);

	import_assistant->priv->import =
		e_import_new ("org.gnome.evolution.shell.importer");

	icon_theme = gtk_icon_theme_get_default ();
	gtk_icon_size_lookup (GTK_ICON_SIZE_DIALOG, &size, NULL);
	pixbuf = gtk_icon_theme_load_icon (
		icon_theme, "stock_mail-import", size, 0, NULL);

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
		"With this assistant you will be guided through the "
		"process of importing external files into Evolution."));
	gtk_widget_show (page);

	gtk_assistant_append_page (assistant, page);
	gtk_assistant_set_page_header_image (assistant, page, pixbuf);
	gtk_assistant_set_page_title (assistant, page, _("Evolution Import Assistant"));
	gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_INTRO);
	gtk_assistant_set_page_complete (assistant, page, TRUE);

	/* Intelligent or direct import page */
	page = import_assistant_type_page_init (import_assistant);

	gtk_assistant_append_page (assistant, page);
	gtk_assistant_set_page_header_image (assistant, page, pixbuf);
	gtk_assistant_set_page_title (assistant, page, _("Importer Type"));
	gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONTENT);
	gtk_assistant_set_page_complete (assistant, page, TRUE);

	/* Intelligent importer source page */
	page = import_assistant_selection_page_init (import_assistant);

	gtk_assistant_append_page (assistant, page);
	gtk_assistant_set_page_header_image (assistant, page, pixbuf);
	gtk_assistant_set_page_title (assistant, page, _("Select Information to Import"));
	gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONTENT);

	/* File selection and file type page */
	page = import_assistant_file_page_init (import_assistant);

	gtk_assistant_append_page (assistant, page);
	gtk_assistant_set_page_header_image (assistant, page, pixbuf);
	gtk_assistant_set_page_title (assistant, page, _("Select a File"));
	gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONTENT);

	/* File destination page */
	page = import_assistant_destination_page_init (import_assistant);

	gtk_assistant_append_page (assistant, page);
	gtk_assistant_set_page_header_image (assistant, page, pixbuf);
	gtk_assistant_set_page_title (assistant, page, _("Import Location"));
	gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONTENT);

	/* Finish page */
	page = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (page), 0.5, 0.5);
	gtk_label_set_text (
		GTK_LABEL (page), _("Click \"Apply\" to "
		"begin importing the file into Evolution."));
	gtk_widget_show (page);

	gtk_assistant_append_page (assistant, page);
	gtk_assistant_set_page_header_image (assistant, page, pixbuf);
	gtk_assistant_set_page_title (assistant, page, _("Import File"));
	gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONFIRM);
	gtk_assistant_set_page_complete (assistant, page, TRUE);

	gtk_assistant_set_forward_page_func (
		assistant, (GtkAssistantPageFunc)
		forward_cb, import_assistant, NULL);

	g_object_unref (pixbuf);

	gtk_assistant_update_buttons_state (assistant);
}

GType
e_import_assistant_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EImportAssistantClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) import_assistant_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EImportAssistant),
			0,     /* n_preallocs */
			(GInstanceInitFunc) import_assistant_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_ASSISTANT, "EImportAssistant", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_import_assistant_new (GtkWindow *parent)
{
	return g_object_new (
		E_TYPE_IMPORT_ASSISTANT,
		"transient-for", parent, NULL);
}
