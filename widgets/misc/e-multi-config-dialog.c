/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-multi-config-dialog.c
 *
 * Copyright (C) 2002  Ximian, Inc.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-multi-config-dialog.h"

#include "e-clipped-label.h"

#include <gal/e-table/e-table-scrolled.h>
#include <gal/e-table/e-table-memory-store.h>
#include <gal/e-table/e-cell-pixbuf.h>
#include <gal/e-table/e-cell-vbox.h>
#include <gal/e-table/e-cell-text.h>

#include <gdk-pixbuf/gdk-pixbuf.h>


#define SWITCH_PAGE_INTERVAL 250

struct _EMultiConfigDialogPrivate {
	GSList *pages;
	
	GtkWidget *list_e_table;
	ETableModel *list_e_table_model;
	
	GtkWidget *notebook;
	
	int set_page_timeout_id;
	int set_page_timeout_page;
};

G_DEFINE_TYPE (EMultiConfigDialog, e_multi_config_dialog, GTK_TYPE_DIALOG)


/* ETable stuff.  */

static char *list_e_table_spec =
	"<ETableSpecification cursor-mode=\"line\""
	"		      selection-mode=\"browse\""
	"                     no-headers=\"true\""
        "                     alternating-row-colors=\"false\""
        "                     horizontal-resize=\"true\""
        ">"
	"  <ETableColumn model_col=\"0\""
	"	         expansion=\"1.0\""
	"                cell=\"vbox\""
 	"                minimum_width=\"32\""
	"                resizable=\"true\""
	"	         _title=\"blah\""
	"                compare=\"string\"/>"
	"  <ETableState>"
	"    <column source=\"0\"/>"
	"    <grouping>"
	"    </grouping>"
	"  </ETableState>"
	"</ETableSpecification>";

/* Page handling.  */

static GtkWidget *
create_page_container (const char *description,
		       GtkWidget *widget)
{
	GtkWidget *vbox;

	vbox = gtk_vbox_new (FALSE, 0);

#if 0
	label = e_clipped_label_new (description);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);

	separator = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 0);

	gtk_widget_show (label);
	gtk_widget_show (separator);
#endif

	gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0); 

	gtk_widget_show (widget);
	gtk_widget_show (vbox);

	return vbox;
}

/* Timeout for switching pages (so it's more comfortable navigating with the
   keyboard).  */

static int
set_page_timeout_callback (void *data)
{
	EMultiConfigDialog *multi_config_dialog;
	EMultiConfigDialogPrivate *priv;

	multi_config_dialog = E_MULTI_CONFIG_DIALOG (data);
	priv = multi_config_dialog->priv;

	gtk_notebook_set_page (GTK_NOTEBOOK (priv->notebook), priv->set_page_timeout_page);

	priv->set_page_timeout_id = 0;
	return FALSE;
}


/* Button handling.  */

static void
do_close (EMultiConfigDialog *dialog)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));
}


/* ETable signals.  */

static void
table_cursor_change_callback (ETable *etable,
			      int row,
			      void *data)
{
	EMultiConfigDialog *dialog;
	EMultiConfigDialogPrivate *priv;

	dialog = E_MULTI_CONFIG_DIALOG (data);
	priv = dialog->priv;

	if (priv->set_page_timeout_id == 0)
		priv->set_page_timeout_id = g_timeout_add (SWITCH_PAGE_INTERVAL,
							   set_page_timeout_callback,
							   dialog);

	priv->set_page_timeout_page = row;
}


/* GObject methods.  */

static void
impl_finalize (GObject *object)
{
	EMultiConfigDialog *dialog;
	EMultiConfigDialogPrivate *priv;

	dialog = E_MULTI_CONFIG_DIALOG (object);
	priv = dialog->priv;

	if (priv->set_page_timeout_id != 0)
		g_source_remove (priv->set_page_timeout_id);

	g_slist_free (priv->pages);

	g_free (priv);

	(* G_OBJECT_CLASS (e_multi_config_dialog_parent_class)->finalize) (object);
}


/* GtkDialog methods.  */

static void
impl_response (GtkDialog *dialog, int response_id)
{
	EMultiConfigDialog *multi_config_dialog;
	EMultiConfigDialogPrivate *priv;
	GError *error;

	multi_config_dialog = E_MULTI_CONFIG_DIALOG (dialog);
	priv = multi_config_dialog->priv;

	error = NULL;

	switch (response_id) {
	case GTK_RESPONSE_HELP:
		gnome_help_display_desktop (NULL,
					    "evolution-" BASE_VERSION,
					    "evolution-" BASE_VERSION ".xml", 
					    "config-prefs",
					    &error);
		if (error != NULL)
			g_warning ("%s", error->message);
		break;
	case GTK_RESPONSE_CLOSE:
	default:
		do_close (multi_config_dialog);
		break;
	}
}


/* GObject ctors.  */

static void
e_multi_config_dialog_class_init (EMultiConfigDialogClass *class)
{
	GObjectClass *object_class;
	GtkDialogClass *dialog_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = impl_finalize;

	dialog_class = GTK_DIALOG_CLASS (class);
	dialog_class->response = impl_response;
}

#define RGB_COLOR(color) (((color).red & 0xff00) << 8 | \
			   ((color).green & 0xff00) | \
			   ((color).blue & 0xff00) >> 8)

static void
fill_in_pixbufs (EMultiConfigDialog *dialog, int row)
{
	GdkPixbuf *original = e_table_model_value_at (dialog->priv->list_e_table_model, 1, row);
	GtkWidget *canvas;
	guint32 colors[3];
	int i;

	if (original == NULL)
		return;

	canvas = GTK_WIDGET (e_table_scrolled_get_table (E_TABLE_SCROLLED (dialog->priv->list_e_table))->table_canvas);

	colors[0] = RGB_COLOR (canvas->style->bg [GTK_STATE_SELECTED]);
	colors[1] = RGB_COLOR (canvas->style->bg [GTK_STATE_ACTIVE]);
	colors[2] = RGB_COLOR (canvas->style->base [GTK_STATE_NORMAL]);

	for (i = 0; i < 3; i++) {
		GdkPixbuf *pixbuf = gdk_pixbuf_composite_color_simple (original,
								       gdk_pixbuf_get_width (original),
								       gdk_pixbuf_get_height (original),
								       GDK_INTERP_BILINEAR,
								       255,
								       1,
								       colors[i], colors[i]);
		e_table_model_set_value_at (dialog->priv->list_e_table_model, i + 2, row, pixbuf);
		g_object_unref(pixbuf);
	}
}

static void
canvas_realize (GtkWidget *widget, EMultiConfigDialog *dialog)
{
	int i;
	int row_count;
	
	row_count = e_table_model_row_count (dialog->priv->list_e_table_model);
	for (i = 0; i < row_count; i++) {
		fill_in_pixbufs (dialog, i);
	}
}


static ETableMemoryStoreColumnInfo columns[] = {
	E_TABLE_MEMORY_STORE_STRING,
	E_TABLE_MEMORY_STORE_PIXBUF,
	E_TABLE_MEMORY_STORE_PIXBUF,
	E_TABLE_MEMORY_STORE_PIXBUF,
	E_TABLE_MEMORY_STORE_PIXBUF,
	E_TABLE_MEMORY_STORE_TERMINATOR
};

static void
e_multi_config_dialog_init (EMultiConfigDialog *multi_config_dialog)
{
	EMultiConfigDialogPrivate *priv;
	ETableModel *list_e_table_model;
	GtkWidget *dialog_vbox;
	GtkWidget *hbox;
	GtkWidget *notebook;
	GtkWidget *list_e_table;
	ETableExtras *extras;
	ECell *pixbuf;
	ECell *text;
	ECell *vbox;

	gtk_dialog_set_has_separator (GTK_DIALOG (multi_config_dialog), FALSE);
	gtk_widget_realize (GTK_WIDGET (multi_config_dialog));
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (multi_config_dialog)->vbox), 0);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (multi_config_dialog)->action_area), 12);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 12); 
	dialog_vbox = GTK_DIALOG (multi_config_dialog)->vbox;
	
	gtk_container_add (GTK_CONTAINER (dialog_vbox), hbox);

	list_e_table_model = e_table_memory_store_new (columns);

	vbox = e_cell_vbox_new ();

	pixbuf = e_cell_pixbuf_new();
	g_object_set (G_OBJECT (pixbuf),
		      "focused_column", 2,
		      "selected_column", 3,
		      "unselected_column", 4,
		      NULL);
	e_cell_vbox_append (E_CELL_VBOX (vbox), pixbuf, 1);
	g_object_unref (pixbuf);

	text = e_cell_text_new (NULL, GTK_JUSTIFY_CENTER);
	e_cell_vbox_append (E_CELL_VBOX (vbox), text, 0);
	g_object_unref (text);

	extras = e_table_extras_new ();
	e_table_extras_add_cell (extras, "vbox", vbox);

	list_e_table = e_table_scrolled_new (list_e_table_model, extras, list_e_table_spec, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_e_table), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	g_signal_connect (e_table_scrolled_get_table (E_TABLE_SCROLLED (list_e_table)),
			  "cursor_change", G_CALLBACK (table_cursor_change_callback), multi_config_dialog);

	g_signal_connect (e_table_scrolled_get_table (E_TABLE_SCROLLED (list_e_table))->table_canvas,
			  "realize", G_CALLBACK (canvas_realize), multi_config_dialog);

	g_object_unref (extras);

	gtk_box_pack_start (GTK_BOX (hbox), list_e_table, FALSE, TRUE, 0);

	notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), notebook, TRUE, TRUE, 0);

	gtk_widget_show (hbox);
	gtk_widget_show (notebook);
	gtk_widget_show (list_e_table);

	gtk_dialog_add_buttons (GTK_DIALOG (multi_config_dialog),
				GTK_STOCK_HELP, GTK_RESPONSE_HELP,
				GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
				NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (multi_config_dialog), GTK_RESPONSE_OK);
	

	gtk_window_set_policy (GTK_WINDOW (multi_config_dialog),
			       FALSE /* allow_shrink */,
			       TRUE /* allow_grow */,
			       FALSE /* auto_shrink */);

	priv = g_new (EMultiConfigDialogPrivate, 1);
	priv->pages                 = NULL;
	priv->list_e_table          = list_e_table;
	priv->list_e_table_model    = list_e_table_model;
	priv->notebook              = notebook;
	priv->set_page_timeout_id   = 0;
	priv->set_page_timeout_page = 0;

	multi_config_dialog->priv = priv;
}


GtkWidget *
e_multi_config_dialog_new (void)
{
	EMultiConfigDialog *dialog;

	dialog = gtk_type_new (e_multi_config_dialog_get_type ());

	return GTK_WIDGET (dialog);
}


void
e_multi_config_dialog_add_page (EMultiConfigDialog *dialog,
				const char *title,
				const char *description,
				GdkPixbuf *icon,
				EConfigPage *page_widget)
{
	EMultiConfigDialogPrivate *priv;
	AtkObject *a11y;
	gint page_no;

	g_return_if_fail (E_IS_MULTI_CONFIG_DIALOG (dialog));
	g_return_if_fail (title != NULL);
	g_return_if_fail (description != NULL);
	g_return_if_fail (E_IS_CONFIG_PAGE (page_widget));

	priv = dialog->priv;

	priv->pages = g_slist_append (priv->pages, page_widget);

	e_table_memory_store_insert (E_TABLE_MEMORY_STORE (priv->list_e_table_model), -1, NULL, title, icon, NULL, NULL, NULL);

	if (GTK_WIDGET_REALIZED (e_table_scrolled_get_table (E_TABLE_SCROLLED (dialog->priv->list_e_table))->table_canvas)) {
		fill_in_pixbufs (dialog, e_table_model_row_count (priv->list_e_table_model) - 1);
	}

	page_no = gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
				  create_page_container (description, GTK_WIDGET (page_widget)),
				  NULL);

	a11y = gtk_widget_get_accessible (GTK_WIDGET(priv->notebook));
	AtkObject *a11yPage = atk_object_ref_accessible_child (a11y, page_no);
	if (a11yPage != NULL) {
		if (atk_object_get_role (a11yPage) == ATK_ROLE_PAGE_TAB)
			atk_object_set_name (a11yPage, title);
		g_object_unref (a11yPage);
	}
	if (priv->pages->next == NULL) {
		ETable *table;

		/* FIXME: This is supposed to select the first entry by default
		   but it doesn't seem to work at all.  */
		table = e_table_scrolled_get_table (E_TABLE_SCROLLED (priv->list_e_table));
		e_table_set_cursor_row (table, 0);
		e_selection_model_select_all (e_table_get_selection_model (table));
	}
}

void
e_multi_config_dialog_show_page (EMultiConfigDialog *dialog, int page)
{
	EMultiConfigDialogPrivate *priv;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (E_IS_MULTI_CONFIG_DIALOG (dialog));

	priv = dialog->priv;

	e_table_set_cursor_row (e_table_scrolled_get_table (E_TABLE_SCROLLED (priv->list_e_table)), page);
	gtk_notebook_set_page (GTK_NOTEBOOK (priv->notebook), page);
}

