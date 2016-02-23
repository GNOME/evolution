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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "e-util/e-util.h"

#include "em-format/e-mail-formatter-print.h"
#include "em-format/e-mail-part-utils.h"

#include "e-mail-printer.h"
#include "e-mail-display.h"
#include "e-mail-print-config-headers.h"

#define w(x)

#define E_MAIL_PRINTER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_PRINTER, EMailPrinterPrivate))

typedef struct _AsyncContext AsyncContext;

struct _EMailPrinterPrivate {
	EMailFormatter *formatter;
	EMailPartList *part_list;
	EMailRemoteContent *remote_content;

	gchar *export_filename;

	GtkPrintOperationAction print_action;
};

struct _AsyncContext {
	WebKitWebView *web_view;
	gulong load_status_handler_id;

	GCancellable *cancellable;
	GMainContext *main_context;

	GtkPrintOperationAction print_action;
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

G_DEFINE_TYPE (
	EMailPrinter,
	e_mail_printer,
	G_TYPE_OBJECT);

static void
async_context_free (AsyncContext *async_context)
{
	if (async_context->load_status_handler_id > 0)
		g_signal_handler_disconnect (
			async_context->web_view,
			async_context->load_status_handler_id);

	g_clear_object (&async_context->web_view);
	g_clear_object (&async_context->cancellable);

	g_main_context_unref (async_context->main_context);

	g_slice_free (AsyncContext, async_context);
}

#if 0
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
                                GSimpleAsyncResult *simple)
{
	printf ("%s\n", __FUNCTION__);
}

static void
mail_printer_print_failed_cb (WebKitPrintOperation *print_operation,
                              GError *error,
                              GSimpleAsyncResult *simple)
{
	AsyncContext *async_context;

	printf ("%s\n", __FUNCTION__);
	async_context = g_simple_async_result_get_op_res_gpointer (simple);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);
	else
		g_warning ("WebKit print operation returned ERROR result without setting a GError");

	async_context->print_result = GTK_PRINT_OPERATION_RESULT_ERROR;
}

static gboolean
mail_printer_print_timeout_cb (gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;
	GCancellable *cancellable;
	WebKitPrintOperation *print_operation;
/* 	EMailPrinter *printer;
	GtkPrintOperationAction print_action;
	gulong draw_page_handler_id;*/
	gulong create_custom_widget_handler_id;
	gulong custom_widget_apply_handler_id;
	GError *error = NULL;

	simple = G_SIMPLE_ASYNC_RESULT (user_data);
	async_context = g_simple_async_result_get_op_res_gpointer (simple);

	cancellable = async_context->cancellable;
	/*
	print_action = async_context->print_action;
*/
	/* Check for cancellation one last time before printing. */
	if (g_cancellable_set_error_if_cancelled (cancellable, &error))
		goto exit;

	/* This returns a new reference. */
/*
	printer = (EMailPrinter *) g_async_result_get_source_object (
		G_ASYNC_RESULT (simple));
*/
	print_operation = webkit_print_operation_new (async_context->web_view);
/*
	if (async_context->print_action == GTK_PRINT_OPERATION_ACTION_EXPORT) {
		const gchar *export_filename;

		export_filename =
			e_mail_printer_get_export_filename (printer);
		gtk_print_operation_set_export_filename (
			print_operation, export_filename);
	}
*/
/*
	create_custom_widget_handler_id = g_signal_connect (
		print_operation, "create-custom-widget",
		G_CALLBACK (mail_printer_create_custom_widget_cb),
		async_context);

	custom_widget_apply_handler_id = g_signal_connect (
		print_operation, "custom-widget-apply",
		G_CALLBACK (mail_printer_custom_widget_apply_cb),
		async_context);
*/
	g_signal_connect (
		print_operation, "failed",
		G_CALLBACK (mail_printer_print_failed_cb),
		async_context);

	g_signal_connect (
		print_operation, "finished",
		G_CALLBACK (mail_printer_print_finished_cb),
		async_context);

/* FIXME WK2 - this will be hard to add back to WK2 API.. There is a CSS draft
 * that can be used to add a page numbers, but it is not in WebKit yet.
 * http://www.w3.org/TR/css3-page/
	draw_page_handler_id = g_signal_connect (
		print_operation, "draw-page",
		G_CALLBACK (mail_printer_draw_footer_cb),
		async_context->cancellable);
*/
	webkit_print_operation_run_dialog (
		print_operation,
		GTK_WINDOW (gtk_widget_get_toplevel (gtk_widget_get_toplevel (GTK_WIDGET (async_context->web_view)))));
/* FIXME WK2
	g_signal_handler_disconnect (
		print_operation, create_custom_widget_handler_id);

	g_signal_handler_disconnect (
		print_operation, custom_widget_apply_handler_id);

	g_signal_handler_disconnect (
		print_operation, draw_page_handler_id);
*/
	g_object_unref (print_operation);

/*
	g_object_unref (printer);*/

exit:
	if (error != NULL)
		g_simple_async_result_take_error (simple, error);

	g_simple_async_result_complete_in_idle (simple);

	return FALSE;
}

static void
mail_printer_load_changed_cb (WebKitWebView *web_view,
                              WebKitLoadEvent load_event,
                              GSimpleAsyncResult *simple)
{
	AsyncContext *async_context;
	GCancellable *cancellable;
	GError *error = NULL;

	/* Note: we disregard WEBKIT_LOAD_FAILED and print what we can. */
	if (load_event != WEBKIT_LOAD_FINISHED)
		return;

	/* Signal handlers are holding the only GSimpleAsyncResult
	 * references.  This is to avoid finalizing it prematurely. */
	g_object_ref (simple);

	async_context = g_simple_async_result_get_op_res_gpointer (simple);
	cancellable = async_context->cancellable;

	/* WebKit reloads the page once more right before starting to print,
	 * so disconnect this handler after the first time to avoid starting
	 * another print operation. */
	g_signal_handler_disconnect (
		async_context->web_view,
		async_context->load_status_handler_id);
	async_context->load_status_handler_id = 0;

	/* Check if we've been cancelled. */
	if (g_cancellable_set_error_if_cancelled (cancellable, &error)) {
		g_simple_async_result_take_error (simple, error);
		g_simple_async_result_complete_in_idle (simple);

	/* Give WebKit some time to perform layouting and rendering before
	 * we start printing.  500ms should be enough in most cases. */
	} else {
		GSource *timeout_source;

		timeout_source = g_timeout_source_new (500);
		g_source_set_callback (
			timeout_source,
			mail_printer_print_timeout_cb,
			g_object_ref (simple),
			(GDestroyNotify) g_object_unref);
		g_source_attach (
			timeout_source, async_context->main_context);
		g_source_unref (timeout_source);
	}

	g_object_unref (simple);
}

static WebKitWebView *
mail_printer_new_web_view (const gchar *charset,
                           const gchar *default_charset)
{
	WebKitWebView *web_view;
	WebKitSettings *web_settings;
	EMailFormatter *formatter;

	web_view = g_object_new (
		E_TYPE_MAIL_DISPLAY,
		"mode", E_MAIL_FORMATTER_MODE_PRINTING, NULL);

	/* XXX EMailDisplay enables frame flattening to prevent scrollable
	 *     subparts in an email, which understandable.  This resets it
	 *     to allow scrollable subparts for reasons I don't understand. */
	web_settings = webkit_web_view_get_settings (web_view);
	g_object_set (
		G_OBJECT (web_settings),
		"enable-frame-flattening", FALSE, NULL);

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
	EMailPrinterPrivate *priv;

	priv = E_MAIL_PRINTER_GET_PRIVATE (object);

	g_clear_object (&priv->formatter);
	g_clear_object (&priv->part_list);
	g_clear_object (&priv->remote_content);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_printer_parent_class)->dispose (object);
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
	printer->priv = E_MAIL_PRINTER_GET_PRIVATE (printer);

	printer->priv->formatter = e_mail_formatter_print_new ();
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
e_mail_printer_print (EMailPrinter *printer,
                      GtkPrintOperationAction action,
                      EMailFormatter *formatter,
                      GCancellable *cancellable,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
	GSimpleAsyncResult *simple;
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
	async_context->print_action = action;
	async_context->main_context = g_main_context_ref_thread_default ();

	if (G_IS_CANCELLABLE (cancellable))
		async_context->cancellable = g_object_ref (cancellable);

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

	simple = g_simple_async_result_new (
		G_OBJECT (printer), callback,
		user_data, e_mail_printer_print);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, async_context, (GDestroyNotify) async_context_free);

	web_view = mail_printer_new_web_view (charset, default_charset);
	e_mail_display_set_part_list (E_MAIL_DISPLAY (web_view), part_list);

	async_context->web_view = g_object_ref_sink (web_view);

	handler_id = g_signal_connect_data (
		web_view, "load-changed",
		G_CALLBACK (mail_printer_load_changed_cb),
		g_object_ref (simple),
		(GClosureNotify) g_object_unref, 0);
	async_context->load_status_handler_id = handler_id;

	mail_uri = e_mail_part_build_uri (
		folder, message_uid,
		"__evo-load-image", G_TYPE_BOOLEAN, TRUE,
		"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_PRINTING,
		"formatter_default_charset", G_TYPE_STRING, default_charset,
		"formatter_charset", G_TYPE_STRING, charset,
		NULL);

	webkit_web_view_load_uri (web_view, mail_uri);

	g_free (mail_uri);

	g_object_unref (simple);

	g_object_unref (part_list);
}

GtkPrintOperationResult
e_mail_printer_print_finish (EMailPrinter *printer,
                             GAsyncResult *result,
                             GError **error)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (printer), e_mail_printer_print),
		GTK_PRINT_OPERATION_RESULT_ERROR);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	async_context = g_simple_async_result_get_op_res_gpointer (simple);

	if (g_simple_async_result_propagate_error (simple, error))
		return GTK_PRINT_OPERATION_RESULT_ERROR;

	g_warn_if_fail (
		async_context->print_result !=
		GTK_PRINT_OPERATION_RESULT_ERROR);

	return async_context->print_result;
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
