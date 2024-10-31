/*
 * Copyright (C) 2019 Red Hat (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_WEB_VIEW_JSC_UTILS_H
#define E_WEB_VIEW_JSC_UTILS_H

#include <webkit2/webkit2.h>

G_BEGIN_DECLS

gboolean	e_web_view_jsc_get_object_property_boolean
						(JSCValue *jsc_object,
						 const gchar *property_name,
						 gboolean default_value);
gint32		e_web_view_jsc_get_object_property_int32
						(JSCValue *jsc_object,
						 const gchar *property_name,
						 gint32 default_value);
gdouble		e_web_view_jsc_get_object_property_double
						(JSCValue *jsc_object,
						 const gchar *property_name,
						 gdouble default_value);
gchar *		e_web_view_jsc_get_object_property_string
						(JSCValue *jsc_object,
						 const gchar *property_name,
						 const gchar *default_value);

gchar *		e_web_view_jsc_printf_script	(const gchar *script_format,
						 ...) G_GNUC_PRINTF (1, 2);
gchar *		e_web_view_jsc_vprintf_script	(const gchar *script_format,
						 va_list va);
void		e_web_view_jsc_printf_script_gstring
						(GString *script,
						 const gchar *script_format,
						 ...) G_GNUC_PRINTF (2, 3);
void		e_web_view_jsc_vprintf_script_gstring
						(GString *script,
						 const gchar *script_format,
						 va_list va);
void		e_web_view_jsc_run_script	(WebKitWebView *web_view,
						 GCancellable *cancellable,
						 const gchar *script_format,
						 ...) G_GNUC_PRINTF (3, 4);
void		e_web_view_jsc_run_script_take	(WebKitWebView *web_view,
						 gchar *script,
						 GCancellable *cancellable);
void		e_web_view_jsc_set_element_hidden
						(WebKitWebView *web_view,
						 const gchar *iframe_id,
						 const gchar *element_id,
						 gboolean value,
						 GCancellable *cancellable);
void		e_web_view_jsc_set_element_disabled
						(WebKitWebView *web_view,
						 const gchar *iframe_id,
						 const gchar *element_id,
						 gboolean value,
						 GCancellable *cancellable);
void		e_web_view_jsc_set_element_checked
						(WebKitWebView *web_view,
						 const gchar *iframe_id,
						 const gchar *element_id,
						 gboolean value,
						 GCancellable *cancellable);
void		e_web_view_jsc_set_element_style_property
						(WebKitWebView *web_view,
						 const gchar *iframe_id,
						 const gchar *element_id,
						 const gchar *property_name,
						 const gchar *value,
						 GCancellable *cancellable);
void		e_web_view_jsc_set_element_attribute
						(WebKitWebView *web_view,
						 const gchar *iframe_id,
						 const gchar *element_id,
						 const gchar *namespace_uri,
						 const gchar *qualified_name,
						 const gchar *value,
						 GCancellable *cancellable);
void		e_web_view_jsc_create_style_sheet
						(WebKitWebView *web_view,
						 const gchar *iframe_id,
						 const gchar *style_sheet_id,
						 const gchar *content,
						 GCancellable *cancellable);
void		e_web_view_jsc_remove_style_sheet
						(WebKitWebView *web_view,
						 const gchar *iframe_id,
						 const gchar *style_sheet_id,
						 GCancellable *cancellable);
void		e_web_view_jsc_add_rule_into_style_sheet
						(WebKitWebView *web_view,
						 const gchar *iframe_id,
						 const gchar *style_sheet_id,
						 const gchar *selector,
						 const gchar *style,
						 GCancellable *cancellable);
void		e_web_view_jsc_register_element_clicked
						(WebKitWebView *web_view,
						 const gchar *iframe_id,
						 const gchar *elem_classes,
						 GCancellable *cancellable);

typedef enum _ETextFormat {
	E_TEXT_FORMAT_PLAIN = 1,
	E_TEXT_FORMAT_HTML = 2,
	E_TEXT_FORMAT_BOTH = 3
} ETextFormat;

void		e_web_view_jsc_get_selection	(WebKitWebView *web_view,
						 ETextFormat format,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_web_view_jsc_get_selection_finish
						(WebKitWebView *web_view,
						 GAsyncResult *result,
						 GSList **out_texts,
						 GError **error);
void		e_web_view_jsc_get_document_content
						(WebKitWebView *web_view,
						 const gchar *iframe_id,
						 ETextFormat format,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_web_view_jsc_get_document_content_finish
						(WebKitWebView *web_view,
						 GAsyncResult *result,
						 GSList **out_texts,
						 GError **error);
void		e_web_view_jsc_get_element_content
						(WebKitWebView *web_view,
						 const gchar *iframe_id,
						 const gchar *element_id,
						 ETextFormat format,
						 gboolean use_outer_html,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_web_view_jsc_get_element_content_finish
						(WebKitWebView *web_view,
						 GAsyncResult *result,
						 GSList **out_texts,
						 GError **error);
void		e_web_view_jsc_get_element_from_point
						(WebKitWebView *web_view,
						 gint xx,
						 gint yy,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_web_view_jsc_get_element_from_point_finish
						(WebKitWebView *web_view,
						 GAsyncResult *result,
						 gchar **out_iframe_src,
						 gchar **out_iframe_id,
						 gchar **out_element_id,
						 GError **error);

G_END_DECLS

#endif /* E_WEB_VIEW_JSC_UTILS_H */
