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
 * Copyright (C) 2011 Dan Vratil <dvratil@redhat.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <webkit/webkitdom.h>

#include "e-util/e-util.h"

#include "em-format/e-mail-formatter-print.h"
#include "em-format/e-mail-part-utils.h"

#include "e-mail-printer.h"
#include "e-mail-display.h"

#define w(x)

#define E_MAIL_PRINTER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_PRINTER, EMailPrinterPrivate))

enum {
	BUTTON_SELECT_ALL,
	BUTTON_SELECT_NONE,
	BUTTON_TOP,
	BUTTON_UP,
	BUTTON_DOWN,
	BUTTON_BOTTOM,
	BUTTONS_COUNT
};

struct _EMailPrinterPrivate {
	EMailFormatter *formatter;
	EMailPartList *part_list;

	gchar *export_filename;

	GtkListStore *headers;

	WebKitWebView *webview; /* WebView to print from */
	gchar *uri;
	GtkWidget *buttons[BUTTONS_COUNT];
	GtkWidget *treeview;

	GtkPrintOperation *operation;
	GtkPrintOperationAction print_action;
};

enum {
	PROP_0,
	PROP_PART_LIST
};

enum {
	SIGNAL_DONE,
	LAST_SIGNAL
};

enum {
	COLUMN_ACTIVE,
	COLUMN_HEADER_NAME,
	COLUMN_HEADER_VALUE,
	COLUMN_HEADER_STRUCT,
	LAST_COLUMN
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	EMailPrinter,
	e_mail_printer,
	G_TYPE_OBJECT);

static gint
mail_printer_header_name_equal (const EMailFormatterHeader *h1,
                                const EMailFormatterHeader *h2)
{
	if ((h2->value == NULL) || (h1->value == NULL)) {
		return g_strcmp0 (h1->name, h2->name);
	} else {
		if ((g_strcmp0 (h1->name, h2->name) == 0) &&
		    (g_strcmp0 (h1->value, h2->value) == 0))
			return 0;
		else
			return 1;
	}
}

static void
mail_printer_draw_footer_cb (GtkPrintOperation *operation,
                             GtkPrintContext *context,
                             gint page_nr)
{
	PangoFontDescription *desc;
	PangoLayout *layout;
	gint n_pages;
	gdouble width, height;
	gchar *text;
	cairo_t *cr;

	cr = gtk_print_context_get_cairo_context (context);
	width = gtk_print_context_get_width (context);
	height = gtk_print_context_get_height (context);

	g_object_get (operation, "n-pages", &n_pages, NULL);
	text = g_strdup_printf (_("Page %d of %d"), page_nr + 1, n_pages);

	cairo_set_source_rgb (cr, 0.1, 0.1, 0.1);
	cairo_fill (cr);

	desc = pango_font_description_from_string ("Sans Regular 10");
	layout = gtk_print_context_create_pango_layout (context);
	pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
	pango_layout_set_font_description (layout, desc);
	pango_layout_set_text (layout, text, -1);
	pango_layout_set_width (layout, width * PANGO_SCALE);
	pango_font_description_free (desc);

	cairo_move_to (cr, 0, height + 5);
	pango_cairo_show_layout (cr, layout);

	g_object_unref (layout);
	g_free (text);
}

static void
mail_printer_printing_done_cb (GtkPrintOperation *operation,
                               GtkPrintOperationResult result,
                               EMailPrinter *printer)
{
	g_signal_emit (printer, signals[SIGNAL_DONE], 0, operation, result);
}

static gboolean
mail_printer_start_printing_timeout_cb (gpointer user_data)
{
	EMailPrinter *printer;
	WebKitWebFrame *frame;

	printer = E_MAIL_PRINTER (user_data);
	frame = webkit_web_view_get_main_frame (printer->priv->webview);

	webkit_web_frame_print_full (
		frame, printer->priv->operation,
		printer->priv->print_action, NULL);

	return FALSE;
}

static void
mail_printer_start_printing (GObject *object,
                             GParamSpec *pspec,
                             EMailPrinter *printer)
{
	WebKitWebView *web_view;
	WebKitLoadStatus load_status;

	web_view = WEBKIT_WEB_VIEW (object);
	load_status = webkit_web_view_get_load_status (web_view);

	if (load_status != WEBKIT_LOAD_FINISHED)
		return;

	/* WebKit reloads the page once more right before starting to print, so
	 * disconnect this handler after the first time, so that we don't start
	 * another printing operation */
	g_signal_handlers_disconnect_by_func (
		object, mail_printer_start_printing, printer);

	if (printer->priv->print_action == GTK_PRINT_OPERATION_ACTION_EXPORT)
		gtk_print_operation_set_export_filename (
			printer->priv->operation,
			printer->priv->export_filename);

	/* Give WebKit some time to perform layouting and rendering before
	 * we start printing. 500ms should be enough in most cases... */
	g_timeout_add_full (
		G_PRIORITY_DEFAULT, 500,
		mail_printer_start_printing_timeout_cb,
		g_object_ref (printer),
		(GDestroyNotify) g_object_unref);
}

static void
mail_printer_run_print_operation (EMailPrinter *printer,
                                  EMailFormatter *formatter)
{
	EMailPartList *part_list;
	CamelFolder *folder;
	const gchar *message_uid;
	const gchar *charset = NULL;
	const gchar *default_charset = NULL;
	gchar *mail_uri;

	part_list = printer->priv->part_list;
	folder = e_mail_part_list_get_folder (part_list);
	message_uid = e_mail_part_list_get_message_uid (part_list);

	if (formatter != NULL) {
		charset = e_mail_formatter_get_charset (formatter);
		default_charset = e_mail_formatter_get_default_charset (formatter);
	}

	if (charset == NULL)
		charset = "";
	if (default_charset == NULL)
		default_charset = "";

	mail_uri = e_mail_part_build_uri (
		folder, message_uid,
		"__evo-load-image", G_TYPE_BOOLEAN, TRUE,
		"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_PRINTING,
		"formatter_default_charset", G_TYPE_STRING, default_charset,
		"formatter_charset", G_TYPE_STRING, charset,
		NULL);

	/* Print_layout is a special EMPart created by EMFormatHTMLPrint */
	if (printer->priv->webview == NULL) {
		EMailFormatter *emp_formatter;

		printer->priv->webview = g_object_new (
			E_TYPE_MAIL_DISPLAY,
			"mode", E_MAIL_FORMATTER_MODE_PRINTING, NULL);
		e_web_view_set_enable_frame_flattening (
			E_WEB_VIEW (printer->priv->webview), FALSE);
		e_mail_display_set_force_load_images (
			E_MAIL_DISPLAY (printer->priv->webview), TRUE);

		emp_formatter = e_mail_display_get_formatter (
			E_MAIL_DISPLAY (printer->priv->webview));
		if (default_charset != NULL && *default_charset != '\0')
			e_mail_formatter_set_default_charset (
				emp_formatter, default_charset);
		if (charset != NULL && *charset != '\0')
			e_mail_formatter_set_charset (emp_formatter, charset);

		g_object_ref_sink (printer->priv->webview);
		g_signal_connect (
			printer->priv->webview, "notify::load-status",
			G_CALLBACK (mail_printer_start_printing), printer);

		w ({
			GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
			GtkWidget *sw = gtk_scrolled_window_new (NULL, NULL);
			gtk_container_add (GTK_CONTAINER (window), sw);
			gtk_container_add (
				GTK_CONTAINER (sw),
				GTK_WIDGET (printer->priv->webview));
			gtk_widget_show_all (window);
		});
	}

	e_mail_display_set_parts_list (
		E_MAIL_DISPLAY (printer->priv->webview),
		printer->priv->part_list);
	webkit_web_view_load_uri (printer->priv->webview, mail_uri);

	g_free (mail_uri);
}

static void
set_header_visible (EMailPrinter *emp,
                    EMailFormatterHeader *header,
                    gint index,
                    gboolean visible)
{
	WebKitDOMDocument *document;
	WebKitDOMNodeList *headers;
	WebKitDOMElement *element;
	WebKitDOMCSSStyleDeclaration *style;

	document = webkit_web_view_get_dom_document (emp->priv->webview);
	headers = webkit_dom_document_get_elements_by_class_name (document, "header-item");

	g_return_if_fail (index < webkit_dom_node_list_get_length (headers));

	element = WEBKIT_DOM_ELEMENT (webkit_dom_node_list_item (headers, index));
	style = webkit_dom_element_get_style (element);
	webkit_dom_css_style_declaration_set_property (
		style,
		"display", (visible ? "table-row" : "none"), "", NULL);
}

static void
header_active_renderer_toggled_cb (GtkCellRendererToggle *renderer,
                                   gchar *path,
                                   EMailPrinter *emp)
{
	GtkTreeIter iter;
	GtkTreePath *p;
	gboolean active;
	EMailFormatterHeader *header;
	gint *indices;

	gtk_tree_model_get_iter_from_string (
		GTK_TREE_MODEL (emp->priv->headers),
		&iter, path);

	gtk_tree_model_get (
		GTK_TREE_MODEL (emp->priv->headers), &iter,
		COLUMN_ACTIVE, &active, -1);
	gtk_tree_model_get (
		GTK_TREE_MODEL (emp->priv->headers), &iter,
		COLUMN_HEADER_STRUCT, &header, -1);
	gtk_list_store_set (
		GTK_LIST_STORE (emp->priv->headers), &iter,
		COLUMN_ACTIVE, !active, -1);

	p = gtk_tree_path_new_from_string (path);
	indices = gtk_tree_path_get_indices (p);
	set_header_visible (emp, header, indices[0], !active);
	gtk_tree_path_free (p);
}

static void
emp_headers_tab_toggle_selection (GtkWidget *button,
                                  gpointer user_data)
{
	EMailPrinter *emp = user_data;
	GtkTreeIter iter;
	gboolean select;

	if (button == emp->priv->buttons[BUTTON_SELECT_ALL])
		select = TRUE;
	else if (button == emp->priv->buttons[BUTTON_SELECT_NONE])
		select = FALSE;
	else
		return;

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (emp->priv->headers), &iter))
		return;

	do {
		EMailFormatterHeader *header;
		GtkTreePath *path;
		gint *indices;

		gtk_tree_model_get (
			GTK_TREE_MODEL (emp->priv->headers), &iter,
			COLUMN_HEADER_STRUCT, &header, -1);
		gtk_list_store_set (
			GTK_LIST_STORE (emp->priv->headers), &iter,
			COLUMN_ACTIVE, select, -1);

		path = gtk_tree_model_get_path (GTK_TREE_MODEL (emp->priv->headers), &iter);
		indices = gtk_tree_path_get_indices (path);
		set_header_visible (emp, header, indices[0], select);
		gtk_tree_path_free (path);

	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (emp->priv->headers), &iter));
}

static void
emp_headers_tab_selection_changed (GtkTreeSelection *selection,
                                   gpointer user_data)
{
	EMailPrinter *emp = user_data;
	gboolean enabled;
	GList *selected_rows;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreePath *path;

	if (gtk_tree_selection_count_selected_rows (selection) == 0) {
		gtk_widget_set_sensitive (emp->priv->buttons[BUTTON_TOP], FALSE);
		gtk_widget_set_sensitive (emp->priv->buttons[BUTTON_UP], FALSE);
		gtk_widget_set_sensitive (emp->priv->buttons[BUTTON_DOWN], FALSE);
		gtk_widget_set_sensitive (emp->priv->buttons[BUTTON_BOTTOM], FALSE);

		return;
	}

	model = GTK_TREE_MODEL (emp->priv->headers);
	selected_rows = gtk_tree_selection_get_selected_rows (selection, &model);

	path = gtk_tree_path_copy (selected_rows->data);
	enabled = gtk_tree_path_prev (path);
	gtk_widget_set_sensitive (emp->priv->buttons[BUTTON_TOP], enabled);
	gtk_widget_set_sensitive (emp->priv->buttons[BUTTON_UP], enabled);

	gtk_tree_model_get_iter (model, &iter, g_list_last (selected_rows)->data);
	enabled = gtk_tree_model_iter_next (model, &iter);
	gtk_widget_set_sensitive (emp->priv->buttons[BUTTON_DOWN], enabled);
	gtk_widget_set_sensitive (emp->priv->buttons[BUTTON_BOTTOM], enabled);

	g_list_foreach (selected_rows, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected_rows);
	gtk_tree_path_free (path);
}

static void
emp_headers_tab_move (GtkWidget *button,
                      gpointer user_data)
{
	EMailPrinter *emp = user_data;
	GtkTreeSelection *selection;
	GList *selected_rows, *references, *l;
	GtkTreePath *path;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeRowReference *selection_middle;
	gint *indices;

	WebKitDOMDocument *document;
	WebKitDOMNodeList *headers;
	WebKitDOMNode *header, *parent;

	model = GTK_TREE_MODEL (emp->priv->headers);
	selection = gtk_tree_view_get_selection  (GTK_TREE_VIEW (emp->priv->treeview));
	selected_rows = gtk_tree_selection_get_selected_rows (selection, &model);

        /* The order of header rows in the HMTL document should be in sync with
	   order of headers in the listview and in efhp->headers_list */
	document = webkit_web_view_get_dom_document (emp->priv->webview);
	headers = webkit_dom_document_get_elements_by_class_name (document, "header-item");

	l = g_list_nth (selected_rows, g_list_length (selected_rows) / 2);
	selection_middle = gtk_tree_row_reference_new (model, l->data);

	references = NULL;
	for (l = selected_rows; l; l = l->next) {
		references = g_list_prepend (
			references,
			gtk_tree_row_reference_new (model, l->data));
	}

	if (button == emp->priv->buttons[BUTTON_TOP]) {

		for (l = references; l; l = l->next) {
                        /* Move the rows in the view  */
			path = gtk_tree_row_reference_get_path (l->data);
			gtk_tree_model_get_iter (model, &iter, path);
			gtk_list_store_move_after (emp->priv->headers, &iter, NULL);

                        /* Move the header row in HTML document */
			indices = gtk_tree_path_get_indices (path);
			header = webkit_dom_node_list_item (headers, indices[0]);
			parent = webkit_dom_node_get_parent_node (header);
			webkit_dom_node_remove_child (parent, header, NULL);
			webkit_dom_node_insert_before (parent, header,
				webkit_dom_node_get_first_child (parent), NULL);

			gtk_tree_path_free (path);
		}

       } else if (button == emp->priv->buttons[BUTTON_UP]) {

		GtkTreeIter *iter_prev;
		WebKitDOMNode *node2;

		references = g_list_reverse (references);

		for (l = references; l; l = l->next) {

			path = gtk_tree_row_reference_get_path (l->data);
			gtk_tree_model_get_iter (model, &iter, path);
			iter_prev = gtk_tree_iter_copy (&iter);
			gtk_tree_model_iter_previous (model, iter_prev);

			gtk_list_store_move_before (emp->priv->headers, &iter, iter_prev);

			indices = gtk_tree_path_get_indices (path);
			header = webkit_dom_node_list_item (headers, indices[0]);
			node2 = webkit_dom_node_get_previous_sibling (header);
			parent = webkit_dom_node_get_parent_node (header);

			webkit_dom_node_remove_child (parent, header, NULL);
			webkit_dom_node_insert_before (parent, header, node2, NULL);

			gtk_tree_path_free (path);
			gtk_tree_iter_free (iter_prev);
		}

	} else if (button == emp->priv->buttons[BUTTON_DOWN]) {

		GtkTreeIter *iter_next;
		WebKitDOMNode *node2;

		for (l = references; l; l = l->next) {

			path = gtk_tree_row_reference_get_path (l->data);
			gtk_tree_model_get_iter (model, &iter, path);
			iter_next = gtk_tree_iter_copy (&iter);
			gtk_tree_model_iter_next (model, iter_next);

			gtk_list_store_move_after (emp->priv->headers, &iter, iter_next);

			indices = gtk_tree_path_get_indices (path);
			header = webkit_dom_node_list_item (headers, indices[0]);
			node2 = webkit_dom_node_get_next_sibling (header);
			parent = webkit_dom_node_get_parent_node (header);

			webkit_dom_node_remove_child (parent, header, NULL);
			webkit_dom_node_insert_before (parent, header,
				webkit_dom_node_get_next_sibling (node2), NULL);

			gtk_tree_path_free (path);
			gtk_tree_iter_free (iter_next);
		}

	} else if (button == emp->priv->buttons[BUTTON_BOTTOM]) {

		references = g_list_reverse (references);

		for (l = references; l; l = l->next) {
			path = gtk_tree_row_reference_get_path (l->data);
			gtk_tree_model_get_iter (model, &iter, path);
			gtk_list_store_move_before (emp->priv->headers, &iter, NULL);

                        /* Move the header row in HTML document */
			indices = gtk_tree_path_get_indices (path);
			header = webkit_dom_node_list_item (headers, indices[0]);
			parent = webkit_dom_node_get_parent_node (header);
			webkit_dom_node_remove_child (parent, header, NULL);
			webkit_dom_node_append_child (parent, header, NULL);

			gtk_tree_path_free (path);
		}
	};

	g_list_foreach (references, (GFunc) gtk_tree_row_reference_free, NULL);
	g_list_free (references);

        /* Keep the selection in middle of the screen */
	path = gtk_tree_row_reference_get_path (selection_middle);
	gtk_tree_view_scroll_to_cell (
		GTK_TREE_VIEW (emp->priv->treeview),
		path, COLUMN_ACTIVE, TRUE, 0.5, 0.5);
	gtk_tree_path_free (path);
	gtk_tree_row_reference_free (selection_middle);

	g_list_foreach (selected_rows, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected_rows);

	emp_headers_tab_selection_changed (selection, user_data);
}

static GtkWidget *
emp_create_headers_tab (GtkPrintOperation *operation,
                        EMailPrinter *emp)
{
	GtkWidget *vbox, *hbox, *scw, *button;
	GtkTreeView *view;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
	gtk_box_pack_end (GTK_BOX (hbox), vbox, FALSE, FALSE, 5);

	emp->priv->treeview = gtk_tree_view_new_with_model (
		GTK_TREE_MODEL (emp->priv->headers));
	view = GTK_TREE_VIEW (emp->priv->treeview);
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (emp_headers_tab_selection_changed), emp);

	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (
		renderer, "toggled",
		G_CALLBACK (header_active_renderer_toggled_cb), emp);
	column = gtk_tree_view_column_new_with_attributes (
		_("Print"), renderer,
		"active", COLUMN_ACTIVE, NULL);
	gtk_tree_view_append_column (view, column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (
		_("Header Name"), renderer,
		"text", COLUMN_HEADER_NAME, NULL);
	gtk_tree_view_append_column (view, column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (
		_("Header Value"), renderer,
		"text", COLUMN_HEADER_VALUE, NULL);
	gtk_tree_view_append_column (view, column);

	scw = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (scw), GTK_WIDGET (view));
	gtk_box_pack_start (GTK_BOX (hbox), scw, TRUE, TRUE, 0);

	button = gtk_button_new_from_stock (GTK_STOCK_SELECT_ALL);
	emp->priv->buttons[BUTTON_SELECT_ALL] = button;
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (emp_headers_tab_toggle_selection), emp);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, TRUE, 5);

	button = gtk_button_new_from_stock (GTK_STOCK_CLEAR);
	emp->priv->buttons[BUTTON_SELECT_NONE] = button;
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (emp_headers_tab_toggle_selection), emp);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, TRUE, 5);

	button = gtk_button_new_from_stock (GTK_STOCK_GOTO_TOP);
	emp->priv->buttons[BUTTON_TOP] = button;
	gtk_widget_set_sensitive (button, FALSE);
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (emp_headers_tab_move), emp);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, TRUE, 5);

	button = gtk_button_new_from_stock (GTK_STOCK_GO_UP);
	emp->priv->buttons[BUTTON_UP] = button;
	gtk_widget_set_sensitive (button, FALSE);
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (emp_headers_tab_move), emp);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, TRUE, 5);

	button = gtk_button_new_from_stock (GTK_STOCK_GO_DOWN);
	emp->priv->buttons[BUTTON_DOWN] = button;
	gtk_widget_set_sensitive (button, FALSE);
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (emp_headers_tab_move), emp);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, TRUE, 5);

	button = gtk_button_new_from_stock (GTK_STOCK_GOTO_BOTTOM);
	emp->priv->buttons[BUTTON_BOTTOM] = button;
	gtk_widget_set_sensitive (button, FALSE);
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (emp_headers_tab_move), emp);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, TRUE, 5);

	gtk_print_operation_set_custom_tab_label (operation, _("Headers"));
	gtk_widget_show_all (hbox);

	return hbox;
}

static void
mail_printer_build_model (EMailPrinter *printer,
                          EMailPartList *part_list)
{
	CamelMediumHeader *header;
	CamelMimeMessage *message;
	GArray *headers;
	GQueue *headers_queue;
	gint i;
	GtkTreeIter last_known = { 0 };

	if (printer->priv->headers != NULL)
		g_object_unref (printer->priv->headers);
	printer->priv->headers = gtk_list_store_new (
		5,
		G_TYPE_BOOLEAN,  /* COLUMN_ACTIVE */
		G_TYPE_STRING,   /* COLUMN_HEADER_NAME */
		G_TYPE_STRING,   /* COLUMN_HEADER_VALUE */
		G_TYPE_POINTER,  /* COLUMN_HEADER_STRUCT */
		G_TYPE_INT);     /* ??? */

	message = e_mail_part_list_get_message (part_list);
	headers = camel_medium_get_headers (CAMEL_MEDIUM (message));
	if (!headers)
		return;

	headers_queue = e_mail_formatter_dup_headers (printer->priv->formatter);
	for (i = 0; i < headers->len; i++) {
		GtkTreeIter iter;
		GList *found_header;
		EMailFormatterHeader *emfh;

		header = &g_array_index (headers, CamelMediumHeader, i);
		emfh = e_mail_formatter_header_new (header->name, header->value);

		found_header = g_queue_find_custom (
			headers_queue, emfh, (GCompareFunc)
			mail_printer_header_name_equal);

		if (!found_header) {
			emfh->flags |= E_MAIL_FORMATTER_HEADER_FLAG_HIDDEN;
			e_mail_formatter_add_header_struct (
				printer->priv->formatter, emfh);
			gtk_list_store_append (printer->priv->headers, &iter);
		} else {
			if (gtk_list_store_iter_is_valid (printer->priv->headers, &last_known))
				gtk_list_store_insert_after (printer->priv->headers, &iter, &last_known);
			else
				gtk_list_store_insert_after (printer->priv->headers, &iter, NULL);

			last_known = iter;
		}

		gtk_list_store_set (
			printer->priv->headers, &iter,
			COLUMN_ACTIVE, (found_header != NULL),
			COLUMN_HEADER_NAME, emfh->name,
			COLUMN_HEADER_VALUE, emfh->value,
			COLUMN_HEADER_STRUCT, emfh, -1);
	}

	g_queue_free_full (
		headers_queue,
		(GDestroyNotify) e_mail_formatter_header_free);

	camel_medium_free_headers (CAMEL_MEDIUM (message), headers);
}

static void
mail_printer_set_part_list (EMailPrinter *printer,
                            EMailPartList *part_list)
{
	g_return_if_fail (E_IS_MAIL_PART_LIST (part_list));
	g_return_if_fail (printer->priv->part_list == NULL);

	printer->priv->part_list = g_object_ref (part_list);
}

static void
mail_printer_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PART_LIST:
			mail_printer_set_part_list (
				E_MAIL_PRINTER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_printer_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PART_LIST:
			g_value_take_object (
				value,
				e_mail_printer_ref_part_list (
				E_MAIL_PRINTER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_printer_dispose (GObject *object)
{
	EMailPrinterPrivate *priv;

	priv = E_MAIL_PRINTER_GET_PRIVATE (object);

	if (priv->headers != NULL) {
		GtkTreeModel *model;
		GtkTreeIter iter;
		gboolean valid;

		model = GTK_TREE_MODEL (priv->headers);

		valid = gtk_tree_model_get_iter_first (model, &iter);
		while (valid) {
			EMailFormatterHeader *header = NULL;

			gtk_tree_model_get (
				model, &iter,
				COLUMN_HEADER_STRUCT, &header, -1);
			e_mail_formatter_header_free (header);

			valid = gtk_tree_model_iter_next (model, &iter);
		}
	}

	g_clear_object (&priv->formatter);
	g_clear_object (&priv->part_list);
	g_clear_object (&priv->headers);
	g_clear_object (&priv->webview);
	g_clear_object (&priv->operation);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_printer_parent_class)->dispose (object);
}

static void
mail_printer_finalize (GObject *object)
{
	EMailPrinterPrivate *priv;

	priv = E_MAIL_PRINTER_GET_PRIVATE (object);

	g_free (priv->uri);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_printer_parent_class)->finalize (object);
}

static void
mail_printer_constructed (GObject *object)
{
	EMailPrinter *printer;
	EMailPartList *part_list;

	printer = E_MAIL_PRINTER (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_printer_parent_class)->constructed (object);

	part_list = e_mail_printer_ref_part_list (printer);
	mail_printer_build_model (printer, part_list);
	g_object_unref (part_list);
}

static void
e_mail_printer_class_init (EMailPrinterClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EMailPrinterPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_printer_set_property;
	object_class->get_property = mail_printer_get_property;
	object_class->dispose = mail_printer_dispose;
	object_class->finalize = mail_printer_finalize;
	object_class->constructed = mail_printer_constructed;

	g_object_class_install_property (
		object_class,
		PROP_PART_LIST,
		g_param_spec_object (
			"part-list",
			"Part List",
			NULL,
			E_TYPE_MAIL_PART_LIST,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	signals[SIGNAL_DONE] = g_signal_new (
		"done",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMailPrinterClass, done),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		GTK_TYPE_PRINT_OPERATION,
		GTK_TYPE_PRINT_OPERATION_RESULT);
}

static void
e_mail_printer_init (EMailPrinter *printer)
{
	printer->priv = E_MAIL_PRINTER_GET_PRIVATE (printer);

	printer->priv->formatter = e_mail_formatter_print_new ();
}

EMailPrinter *
e_mail_printer_new (EMailPartList *part_list)
{
	g_return_val_if_fail (E_IS_MAIL_PART_LIST (part_list), NULL);

	return g_object_new (
		E_TYPE_MAIL_PRINTER,
		"part-list", part_list, NULL);
}

EMailPartList *
e_mail_printer_ref_part_list (EMailPrinter *printer)
{
	g_return_val_if_fail (E_IS_MAIL_PRINTER (printer), NULL);

	return g_object_ref (printer->priv->part_list);
}

void
e_mail_printer_print (EMailPrinter *printer,
                      GtkPrintOperationAction action,
                      EMailFormatter *formatter,
                      GCancellable *cancellable)
{
	g_return_if_fail (E_IS_MAIL_PRINTER (printer));

	if (printer->priv->operation)
		g_object_unref (printer->priv->operation);
	printer->priv->operation = e_print_operation_new ();
	printer->priv->print_action = action;
	gtk_print_operation_set_unit (printer->priv->operation, GTK_UNIT_PIXEL);

	gtk_print_operation_set_show_progress (printer->priv->operation, TRUE);
	g_signal_connect (
		printer->priv->operation, "create-custom-widget",
		G_CALLBACK (emp_create_headers_tab), printer);
	g_signal_connect (
		printer->priv->operation, "done",
		G_CALLBACK (mail_printer_printing_done_cb), printer);
	g_signal_connect (
		printer->priv->operation, "draw-page",
		G_CALLBACK (mail_printer_draw_footer_cb), NULL);

	if (cancellable)
		g_signal_connect_swapped (
			cancellable, "cancelled",
			G_CALLBACK (gtk_print_operation_cancel),
			printer->priv->operation);

	mail_printer_run_print_operation (printer, formatter);
}

const gchar *
e_mail_printer_get_export_filename (EMailPrinter *printer)
{
	g_return_val_if_fail (E_IS_MAIL_PRINTER (printer), NULL);

	return printer->priv->export_filename;
}

void
e_mail_printer_set_export_filename (EMailPrinter *printer,
                                    const gchar *filename)
{
	g_return_if_fail (E_IS_MAIL_PRINTER (printer));

	g_free (printer->priv->export_filename);
	printer->priv->export_filename = g_strdup (filename);
}
