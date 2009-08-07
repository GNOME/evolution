/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Ashish Shrivastava <shashish@novell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <glib/gstdio.h>
#include <gconf/gconf-client.h>
#include "e-non-intrusive-error-dialog.h"

/* eni - non intrusive error */

static gboolean
eni_query_tooltip_cb (GtkTreeView *view,
                  gint x,
                  gint y,
                  gboolean keyboard_mode,
                  GtkTooltip *tooltip)
{
        GtkTreeViewColumn *column;
        GtkTreeModel *model;
        GtkTreePath *path;
        GtkTreeIter iter;
        gint level;

        if (!gtk_tree_view_get_tooltip_context (
                view, &x, &y, keyboard_mode, NULL, &path, &iter))
                return FALSE;

        /* Figure out which column we're pointing at. */
        if (keyboard_mode)
                gtk_tree_view_get_cursor (view, NULL, &column);
        else
                gtk_tree_view_get_path_at_pos (
                        view, x, y, NULL, &column, NULL, NULL);

        /* Restrict the tip area to a single cell. */
        gtk_tree_view_set_tooltip_cell (view, tooltip, path, column, NULL);

        /* This only works if the tree view is NOT reorderable. */
        if (column != gtk_tree_view_get_column (view, 0))
                return FALSE;

        model = gtk_tree_view_get_model (view);
        gtk_tree_model_get (model, &iter, COL_LEVEL, &level, -1);
        gtk_tooltip_set_text (tooltip, ldata[level].key);

        return TRUE;
}

static void
eni_render_pixbuf (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
               GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
        gint level;

        gtk_tree_model_get (model, iter, COL_LEVEL, &level, -1);
        g_object_set (
                renderer, "stock-id", ldata[level].stock_id,
                "stock-size", GTK_ICON_SIZE_MENU, NULL);
}

static void
eni_render_date (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
              GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
        time_t t;
        gchar sdt[100]; /* Should be sufficient? */

        gtk_tree_model_get (model, iter, COL_TIME, &t, -1);
        strftime (sdt, 100, "%x %X", localtime (&t));
        g_object_set (renderer, "text", sdt, NULL);
}

static void
eni_append_logs (const gchar *txt, GtkListStore *store)
{
        gchar **str;

        str = g_strsplit (txt,  ":", 3);
        if (str[0] && str[1] && str[2]) {
                GtkTreeIter iter;

                gtk_list_store_append (store, &iter);
                gtk_list_store_set (
                        store, &iter,
                        COL_LEVEL, atoi (str[0]),
                        COL_TIME, atol (str[1]),
                        COL_DATA, g_strstrip (str[2]),
                        -1);
        } else
                g_printerr ("Unable to decode error log: %s\n", txt);

        g_strfreev (str);
}

static guint
eni_config_get_error_level (const gchar *path)
{
        GConfClient *gconf_client;
        guint error_level;

        gconf_client = gconf_client_get_default ();
        error_level = gconf_client_get_int (gconf_client, path, NULL);

        g_object_unref (gconf_client);
        return error_level;
}

guint
eni_config_get_error_timeout (const gchar *path)
{
        GConfClient *gconf_client;
        guint error_time;

        gconf_client = gconf_client_get_default ();
        error_time = gconf_client_get_int (gconf_client, path, NULL);

        g_object_unref (gconf_client);
        return error_time;
}

static void
eni_error_timeout_changed (GtkSpinButton *b, gpointer data)
{
        GConfClient *gconf_client;
        gint value = gtk_spin_button_get_value_as_int (b);

        gconf_client = gconf_client_get_default ();

        gconf_client_set_int (gconf_client, (gchar *) data, value, NULL);
        g_object_unref (gconf_client);
}

static void
eni_error_level_value_changed (GtkComboBox *w, gpointer *data)
{
        GConfClient *gconf_client;
        gint value = gtk_combo_box_get_active (w);

        gconf_client = gconf_client_get_default ();

        gconf_client_set_int (gconf_client, (gchar *) data, value, NULL);

        g_object_unref (gconf_client);
}

void
eni_show_logger(ELogger *logger, GtkWidget *top,const gchar *error_timeout_path, const gchar *error_level_path)
{
	GtkWidget *container;
	GtkWidget *label;
	GtkWidget *toplevel;
	GtkWidget *vbox;
	GtkWidget *widget;
	GtkWidget *window;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	gint i;

	toplevel = gtk_widget_get_toplevel (top);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 500, 400);
	gtk_window_set_title (GTK_WINDOW (window), _("Debug Logs"));
	gtk_window_set_transient_for (
		GTK_WINDOW (window), GTK_WINDOW (toplevel));
	gtk_container_set_border_width (GTK_CONTAINER (window), 12);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	container = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), container, FALSE, FALSE, 0);

	/* Translators: This is the first part of the sentence
	 * "Show _errors in the status bar for" - XXX - "second(s)." */
	widget = gtk_label_new_with_mnemonic (
		_("Show _errors in the status bar for"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	label = widget;

	widget = gtk_spin_button_new_with_range (1.0, 60.0, 1.0);
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (widget),
		(gdouble) eni_config_get_error_timeout (error_timeout_path));
	g_signal_connect (
		widget, "value-changed",
		G_CALLBACK (eni_error_timeout_changed),
		(gpointer) error_timeout_path);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	/* Translators: This is the second part of the sentence
	 * "Show _errors in the status bar for" - XXX - "second(s)." */
	widget = gtk_label_new_with_mnemonic (_("second(s)."));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	container = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), container, FALSE, FALSE, 0);

	widget = gtk_label_new_with_mnemonic (_("Log Messages:"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	label = widget;

	widget = gtk_combo_box_new_text ();
	for (i = E_LOG_ERROR; i <= E_LOG_DEBUG; i++)
		gtk_combo_box_append_text (
				GTK_COMBO_BOX (widget), ldata[i].text);
	gtk_combo_box_set_active ((GtkComboBox *) widget, eni_config_get_error_level(error_level_path));

	g_signal_connect (
		widget, "changed",
		G_CALLBACK (eni_error_level_value_changed),
		(gpointer) error_level_path);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	store = gtk_list_store_new (3, G_TYPE_INT, G_TYPE_LONG, G_TYPE_STRING);
	e_logger_get_logs (logger, (ELogFunction) eni_append_logs, store);
	gtk_tree_sortable_set_sort_column_id (
		GTK_TREE_SORTABLE (store), COL_TIME, GTK_SORT_DESCENDING);

	container = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (container),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (container), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (vbox), container, TRUE, TRUE, 0);

	widget = gtk_tree_view_new();
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (widget), TRUE);
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (widget), FALSE);
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL (store));
	gtk_tree_view_set_search_column (GTK_TREE_VIEW (widget), COL_DATA);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), TRUE);
	gtk_widget_set_has_tooltip (widget, TRUE);
	gtk_container_add (GTK_CONTAINER (container), widget);

	g_signal_connect (
		widget, "query-tooltip",
		G_CALLBACK (eni_query_tooltip_cb), NULL);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Log Level"));
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (
		column, renderer, eni_render_pixbuf, NULL, NULL);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Time"));
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func (
		column, renderer, eni_render_date, NULL, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes(
		GTK_TREE_VIEW (widget), -1, _("Messages"),
		renderer, "markup", COL_DATA, NULL);

	container = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (container), GTK_BUTTONBOX_END);
	gtk_box_pack_start (GTK_BOX (vbox), container, FALSE, FALSE, 0);

	widget = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	gtk_widget_set_tooltip_text (widget, _("Close this window"));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (gtk_widget_destroy), window);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	gtk_widget_show_all (window);
}

