/*
 * SPDX-FileCopyrightText: (C) 2013, 2014  Ting-Wei Lan
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "e-autoar-gtk-chooser.h"

#ifdef HAVE_AUTOAR
/*
 * SECTION:e-autoar-gtk-chooser
 * @Short_description: GTK+ widgets to choose archive format and filter
 * @Title: e-autoar-gtk-chooser
 *
 * e-autoar-gtk-chooser contains two widgets for users to choose preferred
 * archive format and filter.
 */

static gboolean
e_autoar_gtk_chooser_advanced_get (GtkWidget *advanced,
                                   int *format,
                                   int *filter);

static GtkWidget *
e_autoar_gtk_chooser_advanced_new (AutoarFormat default_format,
                                   AutoarFilter default_filter);

/* e_autoar_gtk_chooser_simple */

enum {
	SIMPLE_COL_FORMAT,
	SIMPLE_COL_FILTER,
	SIMPLE_COL_DESCRIPTION,
	N_SIMPLE_COLS
};

static char*
format_filter_full_description (AutoarFormat format,
                                AutoarFilter filter)
{
	char *description, *extension, *full_description;

	description = autoar_format_filter_get_description (format, filter);
	extension = autoar_format_filter_get_extension (format, filter);
	full_description = g_strdup_printf ("%s (*%s)", description, extension);

	g_free (description);
	g_free (extension);

	return full_description;
}

static gboolean
simple_row_separator_cb (GtkTreeModel *model,
                         GtkTreeIter *iter,
                         void *data)
{
	char *description, first_char;

	gtk_tree_model_get (model, iter, SIMPLE_COL_DESCRIPTION, &description, -1);
	first_char = description != NULL ? *description : '\0';
	g_free (description);

	if (first_char == '\0')
		return TRUE;
	else
		return FALSE;
}

static void
simple_get_variable_row (GtkTreeModel *model,
                         GtkTreeIter *base_iter,
                         GtkTreeIter *dest_iter)
{
	GtkTreeIter iter;

	iter = *base_iter;
	if (!gtk_tree_model_iter_previous (model, &iter) ||
	    !gtk_tree_model_iter_previous (model, &iter) ||
	    !simple_row_separator_cb (model, &iter, NULL)) {
		/* Create two new rows if it does not exist */
		GtkListStore *store;

		store = GTK_LIST_STORE (model);
		gtk_list_store_insert_before (store, dest_iter, base_iter);
		gtk_list_store_insert_before (store, &iter, dest_iter);
		gtk_list_store_set (store, &iter,
				SIMPLE_COL_FORMAT, 0,
				SIMPLE_COL_FILTER, 0,
				SIMPLE_COL_DESCRIPTION, "", -1);
	} else {
		/* Use the existing row */

		iter = *base_iter;
		gtk_tree_model_iter_previous (model, &iter);
		*dest_iter = iter;
	}
}

static void
simple_set_active (GtkComboBox *simple,
                   GtkTreeModel *model,
                   AutoarFormat format,
                   AutoarFilter filter)
{
	GtkTreeIter iter, prev;
	AutoarFormat this_format;
	AutoarFilter this_filter;
	int get_format, get_filter;
	int *previous;

	previous = g_object_get_data ((GObject*)simple, "previous");
	if (autoar_format_is_valid (format) && autoar_filter_is_valid (filter)) {
	    gtk_tree_model_get_iter_first (model, &iter);
		do {
			gtk_tree_model_get (model, &iter,
			                    SIMPLE_COL_FORMAT, &this_format,
			                    SIMPLE_COL_FILTER, &this_filter, -1);
			if (this_format == format && this_filter == filter) {
				gtk_combo_box_set_active_iter (simple, &iter);
				previous[0] = format;
				previous[1] = filter;
				return;
			}
			prev = iter;
		} while (gtk_tree_model_iter_next (model, &iter));

		if (gtk_tree_model_iter_previous (model, &prev)) {
			GtkTreeIter active;
			char *description_string;

			simple_get_variable_row (model, &prev, &active);
			description_string = format_filter_full_description (format, filter);
			gtk_list_store_set (GTK_LIST_STORE (model), &active,
			                    SIMPLE_COL_FORMAT, format,
			                    SIMPLE_COL_FILTER, filter,
			                    SIMPLE_COL_DESCRIPTION, description_string, -1);
			g_free (description_string);

			gtk_combo_box_set_active_iter (simple, &active);
			previous[0] = format;
			previous[1] = filter;
			return;
		}
	}

	gtk_tree_model_get_iter_first (model, &iter);
	gtk_combo_box_set_active_iter (simple, &iter);
	gtk_tree_model_get (model, &iter,
	                    SIMPLE_COL_FORMAT, &get_format,
	                    SIMPLE_COL_FILTER, &get_filter, -1);
	previous[0] = format;
	previous[1] = filter;
}

static void
simple_changed_cb (GtkComboBox *simple,
                   void *data)
{
	GtkTreeIter iter;
	GtkTreeModel* model;
	int format, filter;
	int *previous;

	if (!gtk_combo_box_get_active_iter (simple, &iter))
		return;

	model = gtk_combo_box_get_model (simple);
	gtk_tree_model_get (model, &iter,
	                    SIMPLE_COL_FORMAT, &format,
	                    SIMPLE_COL_FILTER, &filter, -1);

	previous = g_object_get_data ((GObject*)simple, "previous");

	if (!format) {
		GtkWidget *dialog_widget;
		GtkDialog *dialog;
		GtkWidget *dialog_content;

		GtkWidget *simple_widget;
		GtkWidget *advanced_widget;
		int response;

		simple_widget = GTK_WIDGET (simple);
		dialog_widget =
			gtk_dialog_new_with_buttons (
				_("Choose an archive format"),
				GTK_WINDOW (gtk_widget_get_ancestor (simple_widget, GTK_TYPE_WINDOW)),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				_("Cancel"), GTK_RESPONSE_CANCEL,
				_("OK"), GTK_RESPONSE_ACCEPT, NULL);
		dialog = GTK_DIALOG (dialog_widget);
		gtk_dialog_set_default_response (dialog, GTK_RESPONSE_ACCEPT);

		dialog_content = gtk_dialog_get_content_area (dialog);
		advanced_widget = e_autoar_gtk_chooser_advanced_new (previous[0], previous[1]);
		gtk_container_add (GTK_CONTAINER (dialog_content), advanced_widget);
		gtk_widget_show_all (dialog_widget);

		response = gtk_dialog_run (dialog);
		if (response == GTK_RESPONSE_ACCEPT &&
		    gtk_tree_model_iter_previous (model, &iter) &&
		    e_autoar_gtk_chooser_advanced_get (advanced_widget, &format, &filter))
			simple_set_active (simple, model, format, filter);
		else
			simple_set_active (simple, model, previous[0], previous[1]);

		gtk_widget_destroy (dialog_widget);
	} else {
		previous[0] = format;
		previous[1] = filter;
	}
}

/*
 * e_autoar_gtk_chooser_simple_new:
 * @default_format: an #AutoarFormat
 * @default_filter: an #AutoarFilter
 *
 * Create a #GtkComboBox with a list of common archive format. There is also
 * an option called "Other format…", which will use
 * e_autoar_gtk_chooser_advanced_new() and
 * e_autoar_gtk_chooser_advanced_get() to select less common archive
 * format. Arguments @default_format and @default_filter are the default archive
 * format selected on the returned widget. You may want to get the preferred
 * format of users using autoar_pref_get_default_format() and
 * autoar_pref_get_default_filter(), or just set them to 1 to select
 * the default archive format.
 *
 * Returns: (transfer full): a new #GtkComboBox widget
 */
GtkWidget*
e_autoar_gtk_chooser_simple_new (AutoarFormat default_format,
                                 AutoarFilter default_filter)
{
	GtkWidget *simple_widget;
	GtkComboBox *simple_combo;
	GtkCellLayout *simple;
	GtkCellRenderer *cell_renderer;

	GtkTreeModel *model;
	GtkListStore *store;
	GtkTreeIter iter;
	int i;

	int *previous;

	struct format_filter {
		AutoarFormat format;
		AutoarFilter filter;
	};

	struct format_filter defaults [] = {
		{ AUTOAR_FORMAT_ZIP,   AUTOAR_FILTER_NONE  },
		{ AUTOAR_FORMAT_TAR,   AUTOAR_FILTER_NONE  },
		{ AUTOAR_FORMAT_TAR,   AUTOAR_FILTER_GZIP  },
		{ AUTOAR_FORMAT_TAR,   AUTOAR_FILTER_BZIP2 },
		{ AUTOAR_FORMAT_TAR,   AUTOAR_FILTER_XZ    },
		{ AUTOAR_FORMAT_CPIO,  AUTOAR_FILTER_NONE  },
		{ AUTOAR_FORMAT_7ZIP,  AUTOAR_FILTER_NONE  },
	};

	store = gtk_list_store_new (N_SIMPLE_COLS, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING);
	model = GTK_TREE_MODEL (store);
	for (i = 0; i < sizeof (defaults) / sizeof (struct format_filter); i++) {
		char *description;

		gtk_list_store_append (store, &iter);

		description = format_filter_full_description (defaults[i].format,
		                                              defaults[i].filter);
		gtk_list_store_set (store, &iter,
		                    SIMPLE_COL_FORMAT, defaults[i].format,
		                    SIMPLE_COL_FILTER, defaults[i].filter,
		                    SIMPLE_COL_DESCRIPTION, description, -1);
		g_free (description);
	}

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
	                    SIMPLE_COL_FORMAT, 0,
	                    SIMPLE_COL_FILTER, 0,
	                    SIMPLE_COL_DESCRIPTION, "", -1);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
	                    SIMPLE_COL_FORMAT, 0,
	                    SIMPLE_COL_FILTER, 0,
	                    SIMPLE_COL_DESCRIPTION, _("Other format…"), -1);

	simple_widget = gtk_combo_box_new_with_model (model);
	simple = GTK_CELL_LAYOUT (simple_widget);
	simple_combo = GTK_COMBO_BOX (simple_widget);
	cell_renderer = gtk_cell_renderer_text_new ();

	gtk_cell_layout_pack_start (simple, cell_renderer, FALSE);
	gtk_cell_layout_add_attribute (simple, cell_renderer, "text", SIMPLE_COL_DESCRIPTION);

	previous = g_new (int, 2);
	g_object_set_data_full ((GObject*)simple, "previous", previous, g_free);
	simple_set_active (simple_combo, model, default_format, default_filter);

	gtk_combo_box_set_row_separator_func (simple_combo, simple_row_separator_cb, NULL, NULL);
	g_signal_connect (simple, "changed", G_CALLBACK (simple_changed_cb), NULL);

	g_object_unref (store);

	return simple_widget;
}

/*
 * e_autoar_gtk_chooser_simple_get:
 * @simple: a #GtkComboBox returned by e_autoar_gtk_chooser_simple_new()
 * @format: (out): the place to store the #AutoarFormat selected by the user
 * @filter: (out): the place to store the #AutoarFilter selected by the user
 *
 * Gets the selected archive format of the widget created by
 * e_autoar_gtk_chooser_simple_new().
 *
 * Returns: %TRUE if @format and @filter are set. %FALSE if there is no
 * selected item on @simple, so @format and @filter are not modified.
 */
gboolean
e_autoar_gtk_chooser_simple_get (GtkWidget *simple,
                                 int *format,
                                 int *filter)
{
	GtkComboBox *combo;
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (!gtk_combo_box_get_active_iter (combo = GTK_COMBO_BOX (simple), &iter))
		return FALSE;

	model = gtk_combo_box_get_model (combo);
	gtk_tree_model_get (model, &iter,
	                    SIMPLE_COL_FORMAT, format,
	                    SIMPLE_COL_FILTER, filter, -1);
	return TRUE;
}

/* autoar_gtk_chooser_advanced */

enum {
	ADVANCED_FORMAT_COL_FORMAT,
	ADVANCED_FORMAT_COL_DESCRIPTION,
	N_ADVANCED_FORMAT_COLS
};

enum {
	ADVANCED_FILTER_COL_FILTER,
	ADVANCED_FILTER_COL_DESCRIPTION,
	N_ADVANCED_FILTER_COLS
};

static void
advanced_update_description_cb (GtkTreeView *unused_variable,
                                GtkWidget *advanced)
{
	int format;
	int filter;
	GtkLabel *description;
	char *description_string;

	if (!e_autoar_gtk_chooser_advanced_get (advanced, &format, &filter))
		return;

	description = GTK_LABEL (gtk_grid_get_child_at (GTK_GRID (advanced), 0, 1));
	description_string = format_filter_full_description (format, filter);
	gtk_label_set_text (description, description_string);
	g_free (description_string);
}

static GtkTreeModel*
advanced_format_store (void)
{
	GtkListStore *store;
	int i, last;

	store = gtk_list_store_new (N_ADVANCED_FORMAT_COLS, G_TYPE_INT, G_TYPE_STRING);
	last = autoar_format_last ();
	for (i = 1; i < last; i++) {
		GtkTreeIter iter;
		const char *description;

		gtk_list_store_append (store, &iter);
		description = autoar_format_get_description (i);
		gtk_list_store_set (store, &iter,
		                    ADVANCED_FORMAT_COL_FORMAT, i,
		                    ADVANCED_FORMAT_COL_DESCRIPTION, description, -1);
	}

	return GTK_TREE_MODEL (store);
}

static GtkTreeModel*
advanced_filter_store (void)
{
	GtkListStore *store;
	int i, last;

	store = gtk_list_store_new (N_ADVANCED_FILTER_COLS, G_TYPE_INT, G_TYPE_STRING);
	last = autoar_filter_last ();
	for (i = 1; i < last; i++) {
		GtkTreeIter iter;
		const char *description;

		gtk_list_store_append (store, &iter);
		description = autoar_filter_get_description (i);
		gtk_list_store_set (store, &iter,
		                    ADVANCED_FILTER_COL_FILTER, i,
		                    ADVANCED_FILTER_COL_DESCRIPTION, description, -1);
	}

	return GTK_TREE_MODEL (store);
}

/*
 * e_autoar_gtk_chooser_advanced_new:
 * @default_format: an #AutoarFormat
 * @default_filter: an #AutoarFilter
 *
 * Create a #GtkGrid with two lists. One list shows all available formats,
 * and the other list shows all available filters.
 *
 * Returns: (transfer full): a new #GtkGrid widget
 */
static GtkWidget*
e_autoar_gtk_chooser_advanced_new (AutoarFormat default_format,
                                   AutoarFilter default_filter)
{
	GtkWidget *advanced_widget;
	GtkGrid *advanced;

	GtkTreeModel *format_model;
	GtkWidget *format_widget;
	GtkTreeView *format;
	GtkTreeSelection *format_selection;
	GtkCellRenderer *format_renderer;
	GtkTreePath *format_path;

	GtkTreeModel *filter_model;
	GtkWidget *filter_widget;
	GtkTreeView *filter;
	GtkTreeSelection *filter_selection;
	GtkCellRenderer *filter_renderer;
	GtkTreePath *filter_path;

	GtkWidget *description_widget;
	GtkLabel *description;

	advanced_widget = gtk_grid_new ();
	advanced = GTK_GRID (advanced_widget);
	gtk_grid_set_row_spacing (advanced, 5);
	gtk_grid_set_column_spacing (advanced, 5);
	gtk_grid_set_column_homogeneous (advanced, TRUE);

	format_model = advanced_format_store ();
	format_widget = gtk_tree_view_new_with_model (format_model);
	format = GTK_TREE_VIEW (format_widget);
	format_selection = gtk_tree_view_get_selection (format);
	format_renderer = gtk_cell_renderer_text_new ();
	gtk_tree_selection_set_mode (format_selection, GTK_SELECTION_SINGLE);
	gtk_tree_view_insert_column_with_attributes (format, -1, _("Format"),
	                                             format_renderer, "text",
	                                             ADVANCED_FORMAT_COL_DESCRIPTION,
	                                             NULL);
	if (autoar_format_is_valid (default_format)) {
		GtkTreeIter iter;
		gboolean valid;
		format_path = NULL;
		for (valid = gtk_tree_model_get_iter_first (format_model, &iter);
		     valid;
		     valid = gtk_tree_model_iter_next (format_model, &iter)) {
			int get_format;
			gtk_tree_model_get (format_model, &iter,
			                    ADVANCED_FORMAT_COL_FORMAT, &get_format, -1);
			if (default_format == get_format) {
				format_path = gtk_tree_model_get_path (format_model, &iter);
				break;
			}
	}
	if (format_path == NULL)
		format_path = gtk_tree_path_new_first ();
	} else {
		format_path = gtk_tree_path_new_first ();
	}

	gtk_tree_view_set_cursor (format, format_path, NULL, FALSE);
	gtk_tree_path_free (format_path);
	gtk_grid_attach (advanced, format_widget, 0, 0, 1, 1);
	g_object_unref (format_model);

	filter_model = advanced_filter_store ();
	filter_widget = gtk_tree_view_new_with_model (filter_model);
	filter = GTK_TREE_VIEW (filter_widget);
	filter_selection = gtk_tree_view_get_selection (filter);
	filter_renderer = gtk_cell_renderer_text_new ();
	gtk_tree_selection_set_mode (filter_selection, GTK_SELECTION_SINGLE);
	gtk_tree_view_insert_column_with_attributes (filter, -1, _("Filter"),
	                                             filter_renderer, "text",
	                                             ADVANCED_FILTER_COL_DESCRIPTION,
	                                             NULL);
	if (autoar_filter_is_valid (default_filter)) {
		GtkTreeIter iter;
		gboolean valid;
		filter_path = NULL;
		for (valid = gtk_tree_model_get_iter_first (filter_model, &iter);
		     valid;
		     valid = gtk_tree_model_iter_next (filter_model, &iter)) {
			int get_filter;
			gtk_tree_model_get (filter_model, &iter,
			                    ADVANCED_FILTER_COL_FILTER, &get_filter, -1);
			if (default_filter == get_filter) {
				filter_path = gtk_tree_model_get_path (filter_model, &iter);
				break;
			}
		}
		if (filter_path == NULL)
			filter_path = gtk_tree_path_new_first ();
	} else {
		filter_path = gtk_tree_path_new_first ();
	}

	gtk_tree_view_set_cursor (filter, filter_path, NULL, FALSE);
	gtk_tree_path_free (filter_path);
	gtk_grid_attach (advanced, filter_widget, 1, 0, 1, 1);
	g_object_unref (filter_model);

	description_widget = gtk_label_new (NULL);
	description = GTK_LABEL (description_widget);
	gtk_label_set_justify (description, GTK_JUSTIFY_CENTER);
	gtk_grid_attach (advanced, description_widget, 0, 1, 2, 1);

	g_signal_connect (format_widget, "cursor-changed",
	                  G_CALLBACK (advanced_update_description_cb), advanced);
	g_signal_connect (filter_widget, "cursor-changed",
	                  G_CALLBACK (advanced_update_description_cb), advanced);

	/* Run the callback now to set the initial text on the label */
	advanced_update_description_cb (NULL, advanced_widget);

	return advanced_widget;
}

/*
 * e_autoar_gtk_chooser_advanced_get:
 * @advanced: a #GtkGrid returned by e_autoar_gtk_chooser_advanced_new()
 * @format: (out): the place to store the #AutoarFormat selected by the user
 * @filter: (out): the place to store the #AutoarFilter selected by the user
 *
 * Gets the selected archive format of the widget created by
 * e_autoar_gtk_chooser_advanced_new().
 *
 * Returns: %TRUE if @format and @filter are set. %FALSE if there is no
 * selected item on @advanced, so @format and @filter are not modified.
 */
static gboolean
e_autoar_gtk_chooser_advanced_get (GtkWidget *advanced,
                                   int *format,
                                   int *filter)
{
	GtkGrid *grid;
	GtkTreeIter format_iter, filter_iter;
	GtkTreeView *format_view, *filter_view;
	GtkTreePath *format_path, *filter_path;
	GtkTreeModel *format_model, *filter_model;

	grid = GTK_GRID (advanced);
	format_view = GTK_TREE_VIEW (gtk_grid_get_child_at (grid, 0, 0));
	filter_view = GTK_TREE_VIEW (gtk_grid_get_child_at (grid, 1, 0));

	gtk_tree_view_get_cursor (format_view, &format_path, NULL);
	gtk_tree_view_get_cursor (filter_view, &filter_path, NULL);
	if (format_path == NULL || filter_path == NULL) {
		gtk_tree_path_free (format_path);
		gtk_tree_path_free (filter_path);
		return FALSE;
	}

	format_model = gtk_tree_view_get_model (format_view);
	filter_model = gtk_tree_view_get_model (filter_view);
	if (!gtk_tree_model_get_iter (format_model, &format_iter, format_path) ||
	    !gtk_tree_model_get_iter (filter_model, &filter_iter, filter_path)) {
		gtk_tree_path_free (format_path);
		gtk_tree_path_free (filter_path);
		return FALSE;
	}
	gtk_tree_path_free (format_path);
	gtk_tree_path_free (filter_path);

	gtk_tree_model_get (format_model, &format_iter,
	                    ADVANCED_FORMAT_COL_FORMAT, format, -1);
	gtk_tree_model_get (filter_model, &filter_iter,
	                    ADVANCED_FILTER_COL_FILTER, filter, -1);

	return TRUE;
}

#endif /* HAVE_AUTOAR */
