/*
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
 * Copyright (C) 2011 Dan Vratil <dvratil@redhat.com>
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <camel/camel.h>

#include "e-util/e-util.h"

#include "em-format/e-mail-formatter-print.h"
#include "em-format/e-mail-part-utils.h"

#include "e-mail-printer.h"
#include "e-mail-display.h"
#include "e-mail-print-config-headers.h"

#define w(x)

typedef struct _AsyncContext AsyncContext;

struct _EMailPrinterPrivate {
	EMailFormatter *formatter;
	EMailPartList *part_list;
	EMailRemoteContent *remote_content;
	EMailFormatterMode mode;

	gchar *export_filename;
};

struct _AsyncContext {
	WebKitWebView *web_view;
	gulong load_status_handler_id;
	GError *error;

	GtkPrintOperationResult print_result;
};

enum {
	PROP_0,
	PROP_PART_LIST,
	PROP_REMOTE_CONTENT
};

enum {
	COLUMN_ACTIVE,
	COLUMN_HEADER_NAME,
	COLUMN_HEADER_VALUE,
	COLUMN_HEADER_STRUCT,
	LAST_COLUMN
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailPrinter, e_mail_printer, G_TYPE_OBJECT)

static void
async_context_free (AsyncContext *async_context)
{
	if (async_context->load_status_handler_id > 0)
		g_signal_handler_disconnect (
			async_context->web_view,
			async_context->load_status_handler_id);

	g_clear_object (&async_context->web_view);
	g_clear_error (&async_context->error);

	g_slice_free (AsyncContext, async_context);
}

#if 0 /* FIXME WK2 */
static GtkWidget *
mail_printer_create_custom_widget_cb (WebKitPrintOperation *operation,
                                      AsyncContext *async_context)
{
	EMailDisplay *display;
	EMailPartList *part_list;
	EMailPart *part;
	GtkWidget *widget;

	webkit_print_operation_set_custom_tab_label (operation, _("Headers"));

	display = E_MAIL_DISPLAY (async_context->web_view);
	part_list = e_mail_display_get_part_list (display);

	/* FIXME Hard-coding the part ID works for now but could easily
	 *       break silently.  Need a less brittle way of extracting
	 *       specific parts by either MIME type or GType. */
	part = e_mail_part_list_ref_part (part_list, ".message.headers");

	widget = e_mail_print_config_headers_new (E_MAIL_PART_HEADERS (part));

	g_object_unref (part);

	return widget;
}

static void
mail_printer_custom_widget_apply_cb (WebKitPrintOperation *operation,
                                     GtkWidget *widget,
                                     AsyncContext *async_context)
{
	webkit_web_view_reload (async_context->web_view);
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
#endif
static void
mail_printer_print_finished_cb (WebKitPrintOperation *print_operation,
                                GTask *task)
{
	AsyncContext *async_context;

	if (camel_debug ("webkit:preview"))
		printf ("%s\n", G_STRFUNC);

	async_context = g_task_get_task_data (task);
	g_return_if_fail (async_context != NULL);

	if (async_context->print_result == GTK_PRINT_OPERATION_RESULT_IN_PROGRESS) {
		async_context->print_result = GTK_PRINT_OPERATION_RESULT_APPLY;
		g_task_return_boolean (task, TRUE);
	} else if (async_context->error) {
		g_task_return_error (task, g_error_copy (async_context->error));
	} else {
		g_task_return_boolean (task, FALSE);
	}

	g_object_unref (task);
}

static void
mail_printer_print_failed_cb (WebKitPrintOperation *print_operation,
                              const GError *error,
                              GTask *task)
{
	AsyncContext *async_context;

	if (camel_debug ("webkit:preview"))
		printf ("%s\n", G_STRFUNC);

	async_context = g_task_get_task_data (task);
	g_return_if_fail (async_context != NULL);
	async_context->print_result = GTK_PRINT_OPERATION_RESULT_ERROR;
	async_context->error = error ? g_error_copy (error) : NULL;
}

static gboolean
mail_printer_print_timeout_cb (GTask *task)
{
	AsyncContext *async_context;
	gpointer source_object;
	const gchar *export_filename;
	GtkPrintSettings *print_settings = NULL;
	GtkPageSetup *page_setup = NULL;
	WebKitPrintOperation *print_operation = NULL;
	WebKitPrintOperationResponse response;
	/* FIXME WK2
	gulong draw_page_handler_id;
	gulong create_custom_widget_handler_id;
	gulong custom_widget_apply_handler_id;*/

	async_context = g_task_get_task_data (task);

	g_return_val_if_fail (async_context != NULL, G_SOURCE_REMOVE);

	source_object = g_task_get_source_object (task);

	g_return_val_if_fail (E_IS_MAIL_PRINTER (source_object), G_SOURCE_REMOVE);

	e_print_load_settings (&print_settings, &page_setup);

	export_filename = e_mail_printer_get_export_filename (E_MAIL_PRINTER (source_object));

	if (!gtk_print_settings_get (print_settings, GTK_PRINT_SETTINGS_OUTPUT_DIR)) {
		const gchar *uri;

		uri = gtk_print_settings_get (print_settings, GTK_PRINT_SETTINGS_OUTPUT_URI);
		if (uri && g_str_has_prefix (uri, "file://")) {
			GFile *file, *parent;

			file = g_file_new_for_uri (uri);
			parent = g_file_get_parent (file);

			if (parent && g_file_peek_path (parent))
				gtk_print_settings_set (print_settings, GTK_PRINT_SETTINGS_OUTPUT_DIR, g_file_peek_path (parent));

			g_clear_object (&parent);
			g_clear_object (&file);
		}
	}

	gtk_print_settings_set (print_settings, GTK_PRINT_SETTINGS_OUTPUT_URI, NULL);
	gtk_print_settings_set (print_settings, GTK_PRINT_SETTINGS_OUTPUT_BASENAME, export_filename);

	print_operation = webkit_print_operation_new (async_context->web_view);
	webkit_print_operation_set_print_settings (print_operation, print_settings);
	webkit_print_operation_set_page_setup (print_operation, page_setup);
	g_clear_object (&print_settings);
	g_clear_object (&page_setup);

	g_signal_connect_data (
		print_operation, "failed",
		G_CALLBACK (mail_printer_print_failed_cb),
		g_object_ref (task),
		(GClosureNotify) g_object_unref, 0);

	g_signal_connect_data (
		print_operation, "finished",
		G_CALLBACK (mail_printer_print_finished_cb),
		g_object_ref (task),
		(GClosureNotify) g_object_unref, 0);

	/* FIXME WK2
	create_custom_widget_handler_id = g_signal_connect (
		print_operation, "create-custom-widget",
		G_CALLBACK (mail_printer_create_custom_widget_cb),
		async_context);

	custom_widget_apply_handler_id = g_signal_connect (
		print_operation, "custom-widget-apply",
		G_CALLBACK (mail_printer_custom_widget_apply_cb),
		async_context); */

	/* FIXME WK2 - this will be hard to add back to WK2 API.. There is a CSS draft
	 * that can be used to add a page numbers, but it is not in WebKit yet.
	 * http://www.w3.org/TR/css3-page/
	draw_page_handler_id = g_signal_connect (
		print_operation, "draw-page",
		G_CALLBACK (mail_printer_draw_footer_cb),
		async_context->cancellable); */

	response = webkit_print_operation_run_dialog (print_operation, NULL);

	/* FIXME WK2
	g_signal_handler_disconnect (
		print_operation, create_custom_widget_handler_id);

	g_signal_handler_disconnect (
		print_operation, custom_widget_apply_handler_id);

	g_signal_handler_disconnect (
		print_operation, draw_page_handler_id); */

	if (response == WEBKIT_PRINT_OPERATION_RESPONSE_PRINT) {
		print_settings = webkit_print_operation_get_print_settings (print_operation);
		page_setup = webkit_print_operation_get_page_setup (print_operation);
		e_print_save_settings (print_settings, page_setup);
	} else if (response == WEBKIT_PRINT_OPERATION_RESPONSE_CANCEL) {
		async_context->print_result = GTK_PRINT_OPERATION_RESULT_CANCEL;
		g_task_return_boolean (task, TRUE);
		g_object_unref (task);
	}

	g_clear_object (&print_operation);

	return G_SOURCE_REMOVE;
}

static void
mail_printer_load_changed_cb (WebKitWebView *web_view,
                              WebKitLoadEvent load_event,
                              GTask *task)
{
	AsyncContext *async_context;

	/* Note: we disregard WEBKIT_LOAD_FAILED and print what we can. */
	if (load_event != WEBKIT_LOAD_FINISHED)
		return;

	async_context = g_task_get_task_data (task);

	g_return_if_fail (async_context != NULL);

	/* WebKit reloads the page once more right before starting to print,
	 * so disconnect this handler after the first time to avoid starting
	 * another print operation. */
	g_signal_handler_disconnect (
		async_context->web_view,
		async_context->load_status_handler_id);
	async_context->load_status_handler_id = 0;

	/* Check if we've been cancelled. */
	if (g_task_return_error_if_cancelled (task)) {
		g_object_unref (task);
		return;
	} else {
		GSource *timeout_source;

		/* Give WebKit some time to perform layouting and rendering before
		 * we start printing. 500ms should be enough in most cases. */
		timeout_source = g_timeout_source_new (500);
		g_task_attach_source (
			task,
			timeout_source,
			(GSourceFunc) mail_printer_print_timeout_cb);
		g_source_unref (timeout_source);
	}
}

static WebKitWebView *
mail_printer_new_web_view (const gchar *charset,
			   const gchar *default_charset,
			   EMailFormatterMode mode)
{
	WebKitWebView *web_view;
	EMailFormatter *formatter;

	web_view = g_object_new (
		E_TYPE_MAIL_DISPLAY,
		"mode", mode, NULL);

	/* Do not load remote images, print what user sees in the preview panel */
	e_mail_display_set_force_load_images (E_MAIL_DISPLAY (web_view), FALSE);

	formatter = e_mail_display_get_formatter (E_MAIL_DISPLAY (web_view));
	if (charset != NULL && *charset != '\0')
		e_mail_formatter_set_charset (formatter, charset);
	if (default_charset != NULL && *default_charset != '\0')
		e_mail_formatter_set_default_charset (formatter, default_charset);

	return web_view;
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
mail_printer_set_remote_content (EMailPrinter *printer,
				 EMailRemoteContent *remote_content)
{
	g_return_if_fail (E_IS_MAIL_REMOTE_CONTENT (remote_content));
	g_return_if_fail (printer->priv->remote_content == NULL);

	printer->priv->remote_content = g_object_ref (remote_content);
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

		case PROP_REMOTE_CONTENT:
			mail_printer_set_remote_content (
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

		case PROP_REMOTE_CONTENT:
			g_value_take_object (
				value,
				e_mail_printer_ref_remote_content (
				E_MAIL_PRINTER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_printer_dispose (GObject *object)
{
	EMailPrinter *self = E_MAIL_PRINTER (object);

	g_clear_object (&self->priv->formatter);
	g_clear_object (&self->priv->part_list);
	g_clear_object (&self->priv->remote_content);
	g_clear_pointer (&self->priv->export_filename, g_free);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_printer_parent_class)->dispose (object);
}

static void
e_mail_printer_class_init (EMailPrinterClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_printer_set_property;
	object_class->get_property = mail_printer_get_property;
	object_class->dispose = mail_printer_dispose;

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

	g_object_class_install_property (
		object_class,
		PROP_REMOTE_CONTENT,
		g_param_spec_object (
			"remote-content",
			"Remote Content",
			NULL,
			E_TYPE_MAIL_REMOTE_CONTENT,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_mail_printer_init (EMailPrinter *printer)
{
	printer->priv = e_mail_printer_get_instance_private (printer);

	printer->priv->formatter = e_mail_formatter_print_new ();
	printer->priv->mode = E_MAIL_FORMATTER_MODE_PRINTING;
}

EMailPrinter *
e_mail_printer_new (EMailPartList *part_list,
		    EMailRemoteContent *remote_content)
{
	g_return_val_if_fail (E_IS_MAIL_PART_LIST (part_list), NULL);

	return g_object_new (E_TYPE_MAIL_PRINTER,
		"part-list", part_list,
		"remote-content", remote_content,
		NULL);
}

EMailPartList *
e_mail_printer_ref_part_list (EMailPrinter *printer)
{
	g_return_val_if_fail (E_IS_MAIL_PRINTER (printer), NULL);

	return g_object_ref (printer->priv->part_list);
}

EMailRemoteContent *
e_mail_printer_ref_remote_content (EMailPrinter *printer)
{
	g_return_val_if_fail (E_IS_MAIL_PRINTER (printer), NULL);

	if (!printer->priv->remote_content)
		return NULL;

	return g_object_ref (printer->priv->remote_content);
}

void
e_mail_printer_set_mode (EMailPrinter *printer,
			 EMailFormatterMode mode)
{
	g_return_if_fail (E_IS_MAIL_PRINTER (printer));

	printer->priv->mode = mode;
}

EMailFormatterMode
e_mail_printer_get_mode (EMailPrinter *printer)
{
	g_return_val_if_fail (E_IS_MAIL_PRINTER (printer), E_MAIL_FORMATTER_MODE_PRINTING);

	return printer->priv->mode;
}

void
e_mail_printer_print (EMailPrinter *printer,
                      GtkPrintOperationAction action, /* unused */
                      EMailFormatter *formatter,
                      GCancellable *cancellable,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
	GTask *task;
	AsyncContext *async_context;
	WebKitWebView *web_view;
	EMailPartList *part_list;
	CamelFolder *folder;
	const gchar *message_uid;
	const gchar *charset = NULL;
	const gchar *default_charset = NULL;
	gchar *mail_uri;
	gulong handler_id;

	g_return_if_fail (E_IS_MAIL_PRINTER (printer));
	/* EMailFormatter can be NULL. */

	async_context = g_slice_new0 (AsyncContext);
	async_context->print_result = GTK_PRINT_OPERATION_RESULT_IN_PROGRESS;
	async_context->error = NULL;

	part_list = e_mail_printer_ref_part_list (printer);
	folder = e_mail_part_list_get_folder (part_list);
	message_uid = e_mail_part_list_get_message_uid (part_list);

	if (formatter != NULL) {
		charset =
			e_mail_formatter_get_charset (formatter);
		default_charset =
			e_mail_formatter_get_default_charset (formatter);
	}

	if (charset == NULL)
		charset = "";
	if (default_charset == NULL)
		default_charset = "";

	task = g_task_new (printer, cancellable, callback, user_data);

	web_view = mail_printer_new_web_view (charset, default_charset, e_mail_printer_get_mode (printer));
	e_mail_display_set_part_list (E_MAIL_DISPLAY (web_view), part_list);

	async_context->web_view = g_object_ref_sink (web_view);

	handler_id = g_signal_connect_data (
		web_view, "load-changed",
		G_CALLBACK (mail_printer_load_changed_cb),
		g_object_ref (task),
		(GClosureNotify) g_object_unref, 0);
	async_context->load_status_handler_id = handler_id;
	g_task_set_task_data (task, async_context, (GDestroyNotify) async_context_free);

	mail_uri = e_mail_part_build_uri (
		folder, message_uid,
		"__evo-load-image", G_TYPE_BOOLEAN, TRUE,
		"mode", G_TYPE_INT, e_mail_printer_get_mode (printer),
		"formatter_default_charset", G_TYPE_STRING, default_charset,
		"formatter_charset", G_TYPE_STRING, charset,
		NULL);

	webkit_web_view_load_uri (web_view, mail_uri);

	g_free (mail_uri);
	g_object_unref (part_list);
}

GtkPrintOperationResult
e_mail_printer_print_finish (EMailPrinter *printer,
                             GAsyncResult *result,
                             GError **error)
{
	GTask *task;
	GtkPrintOperationResult print_result;
	AsyncContext *async_context;

	g_return_val_if_fail (g_task_is_valid (result, printer), GTK_PRINT_OPERATION_RESULT_ERROR);

	task = G_TASK (result);
	async_context = g_task_get_task_data (task);
	if (!g_task_propagate_boolean (task, error))
		return GTK_PRINT_OPERATION_RESULT_ERROR;

	g_return_val_if_fail (async_context != NULL, GTK_PRINT_OPERATION_RESULT_ERROR);

	print_result = async_context->print_result;
	g_warn_if_fail (print_result != GTK_PRINT_OPERATION_RESULT_ERROR);

	return print_result;
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
