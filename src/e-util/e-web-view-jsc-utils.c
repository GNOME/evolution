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

#include "evolution-config.h"

#include <webkit2/webkit2.h>

#include "e-web-view-jsc-utils.h"

gboolean
e_web_view_jsc_get_object_property_boolean (JSCValue *jsc_object,
					    const gchar *property_name,
					    gboolean default_value)
{
	JSCValue *value;
	gboolean res = default_value;

	g_return_val_if_fail (JSC_IS_VALUE (jsc_object), default_value);
	g_return_val_if_fail (property_name != NULL, default_value);

	value = jsc_value_object_get_property (jsc_object, property_name);
	if (!value)
		return default_value;

	if (jsc_value_is_boolean (value))
		res = jsc_value_to_boolean (value);

	g_clear_object (&value);

	return res;
}

gint32
e_web_view_jsc_get_object_property_int32 (JSCValue *jsc_object,
					  const gchar *property_name,
					  gint32 default_value)
{
	JSCValue *value;
	gint32 res = default_value;

	g_return_val_if_fail (JSC_IS_VALUE (jsc_object), default_value);
	g_return_val_if_fail (property_name != NULL, default_value);

	value = jsc_value_object_get_property (jsc_object, property_name);
	if (!value)
		return default_value;

	if (jsc_value_is_number (value))
		res = jsc_value_to_int32 (value);

	g_clear_object (&value);

	return res;
}

gdouble
e_web_view_jsc_get_object_property_double (JSCValue *jsc_object,
					   const gchar *property_name,
					   gdouble default_value)
{
	JSCValue *value;
	gdouble res = default_value;

	g_return_val_if_fail (JSC_IS_VALUE (jsc_object), default_value);
	g_return_val_if_fail (property_name != NULL, default_value);

	value = jsc_value_object_get_property (jsc_object, property_name);
	if (!value)
		return default_value;

	if (jsc_value_is_number (value))
		res = jsc_value_to_double (value);

	g_clear_object (&value);

	return res;
}

gchar *
e_web_view_jsc_get_object_property_string (JSCValue *jsc_object,
					   const gchar *property_name,
					   const gchar *default_value)
{
	JSCValue *value;
	gchar *res;

	g_return_val_if_fail (JSC_IS_VALUE (jsc_object), NULL);
	g_return_val_if_fail (property_name != NULL, NULL);

	value = jsc_value_object_get_property (jsc_object, property_name);
	if (!value)
		return g_strdup (default_value);

	if (jsc_value_is_string (value))
		res = jsc_value_to_string (value);
	else
		res = g_strdup (default_value);

	g_clear_object (&value);

	return res;
}

gchar *
e_web_view_jsc_printf_script (const gchar *script_format,
			      ...)
{
	gchar *script;
	va_list va;

	g_return_val_if_fail (script_format != NULL, NULL);

	va_start (va, script_format);
	script = e_web_view_jsc_vprintf_script (script_format, va);
	va_end (va);

	return script;
}

gchar *
e_web_view_jsc_vprintf_script (const gchar *script_format,
			       va_list va)
{
	GString *script;

	g_return_val_if_fail (script_format != NULL, NULL);

	script = g_string_sized_new (128);

	e_web_view_jsc_vprintf_script_gstring (script, script_format, va);

	return g_string_free (script, FALSE);
}

void
e_web_view_jsc_printf_script_gstring (GString *script,
				      const gchar *script_format,
				      ...)
{
	va_list va;

	g_return_if_fail (script != NULL);
	g_return_if_fail (script_format != NULL);

	va_start (va, script_format);
	e_web_view_jsc_vprintf_script_gstring (script, script_format, va);
	va_end (va);
}

void
e_web_view_jsc_vprintf_script_gstring (GString *script,
				       const gchar *script_format,
				       va_list va)
{
	const gchar *ptr;

	g_return_if_fail (script != NULL);
	g_return_if_fail (script_format != NULL);

	if (script->len)
		g_string_append_c (script, '\n');

	for (ptr = script_format; *ptr; ptr++) {
		if (*ptr == '\\') {
			g_warn_if_fail (ptr[1]);

			g_string_append_c (script, ptr[0]);
			g_string_append_c (script, ptr[1]);

			ptr++;
		} else if (*ptr == '%') {
			g_warn_if_fail (ptr[1]);

			switch (ptr[1]) {
			case '%':
				g_string_append_c (script, ptr[1]);
				break;
			/* Using %x for boolean, because %b is unknown to gcc, thus it claims format warnings */
			case 'x': {
				gboolean arg = va_arg (va, gboolean);

				g_string_append (script, arg ? "true" : "false");
				} break;
			case 'd': {
				gint arg = va_arg (va, gint);

				g_string_append_printf (script, "%d", arg);
				} break;
			case 'f': {
				gdouble arg = va_arg (va, gdouble);

				g_string_append_printf (script, "%f", arg);
				} break;
			case 's': {
				const gchar *arg = va_arg (va, const gchar *);

				/* Enclose strings into double-quotes */
				g_string_append_c (script, '\"');

				/* Escape significant characters */
				if (arg && (strchr (arg, '\"') ||
				    strchr (arg, '\\') ||
				    strchr (arg, '\n') ||
				    strchr (arg, '\r') ||
				    strchr (arg, '\t'))) {
					const gchar *ptr2;

					for (ptr2 = arg; *ptr2; ptr2++) {
						if (*ptr2 == '\\')
							g_string_append (script, "\\\\");
						else if (*ptr2 == '\"')
							g_string_append (script, "\\\"");
						else if (*ptr2 == '\r')
							g_string_append (script, "\\r");
						else if (*ptr2 == '\n')
							g_string_append (script, "\\n");
						else if (*ptr2 == '\t')
							g_string_append (script, "\\t");
						else
							g_string_append_c (script, *ptr2);
					}
				} else if (arg && *arg) {
					g_string_append (script, arg);
				}

				g_string_append_c (script, '\"');

				} break;
			default:
				g_warning ("%s: Unknown percent tag '%c'", G_STRFUNC, *ptr);
				break;
			}

			ptr++;
		} else {
			g_string_append_c (script, *ptr);
		}
	}
}

static void
ewv_jsc_call_done_cb (GObject *source,
		      GAsyncResult *result,
		      gpointer user_data)
{
	JSCValue *value;
	gchar *script = user_data;
	GError *error = NULL;

	value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (source), result, &error);

	if (error) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
		    (!g_error_matches (error, WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED) ||
		     /* WebKit can return empty error message, thus ignore those. */
		     (error->message && *(error->message))))
			g_debug ("Failed to call '%s' function: %s:%d: %s", script, g_quark_to_string (error->domain), error->code, error->message);
		g_clear_error (&error);
	}

	if (value) {
		JSCException *exception;

		exception = jsc_context_get_exception (jsc_value_get_context (value));

		if (exception) {
			g_warning ("Failed to call '%s': %s", script, jsc_exception_get_message (exception));
			jsc_context_clear_exception (jsc_value_get_context (value));
		}

		g_clear_object (&value);
	}

	g_free (script);
}

void
e_web_view_jsc_run_script (WebKitWebView *web_view,
			   GCancellable *cancellable,
			   const gchar *script_format,
			   ...)
{
	gchar *script;
	va_list va;

	g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));
	g_return_if_fail (script_format != NULL);

	va_start (va, script_format);
	script = e_web_view_jsc_vprintf_script (script_format, va);
	va_end (va);

	e_web_view_jsc_run_script_take (web_view, script, cancellable);
}

/* Assumes ownership of the 'script' variable and frees is with g_free(), when no longe needed. */
void
e_web_view_jsc_run_script_take (WebKitWebView *web_view,
				gchar *script,
				GCancellable *cancellable)
{
	g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));
	g_return_if_fail (script != NULL);

	webkit_web_view_evaluate_javascript (web_view, script, -1, NULL, NULL, cancellable, ewv_jsc_call_done_cb, script);
}

void
e_web_view_jsc_set_element_hidden (WebKitWebView *web_view,
				   const gchar *iframe_id,
				   const gchar *element_id,
				   gboolean value,
				   GCancellable *cancellable)
{
	g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));
	g_return_if_fail (element_id != NULL);

	e_web_view_jsc_run_script (web_view, cancellable,
		"Evo.SetElementHidden(%s,%s,%d)",
		iframe_id,
		element_id,
		value ? 1 : 0);
}

void
e_web_view_jsc_set_element_disabled (WebKitWebView *web_view,
				     const gchar *iframe_id,
				     const gchar *element_id,
				     gboolean value,
				     GCancellable *cancellable)
{
	g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));
	g_return_if_fail (element_id != NULL);

	e_web_view_jsc_run_script (web_view, cancellable,
		"Evo.SetElementDisabled(%s,%s,%d)",
		iframe_id,
		element_id,
		value ? 1 : 0);
}

void
e_web_view_jsc_set_element_checked (WebKitWebView *web_view,
				    const gchar *iframe_id,
				    const gchar *element_id,
				    gboolean value,
				    GCancellable *cancellable)
{
	g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));
	g_return_if_fail (element_id != NULL);

	e_web_view_jsc_run_script (web_view, cancellable,
		"Evo.SetElementChecked(%s,%s,%d)",
		iframe_id,
		element_id,
		value ? 1 : 0);
}

void
e_web_view_jsc_set_element_style_property (WebKitWebView *web_view,
					   const gchar *iframe_id,
					   const gchar *element_id,
					   const gchar *property_name,
					   const gchar *value,
					   GCancellable *cancellable)
{
	g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));
	g_return_if_fail (element_id != NULL);
	g_return_if_fail (property_name != NULL);

	e_web_view_jsc_run_script (web_view, cancellable,
		"Evo.SetElementStyleProperty(%s,%s,%s,%s)",
		iframe_id,
		element_id,
		property_name,
		value);
}

void
e_web_view_jsc_set_element_attribute (WebKitWebView *web_view,
				      const gchar *iframe_id,
				      const gchar *element_id,
				      const gchar *namespace_uri,
				      const gchar *qualified_name,
				      const gchar *value,
				      GCancellable *cancellable)
{
	g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));
	g_return_if_fail (element_id != NULL);
	g_return_if_fail (qualified_name != NULL);

	e_web_view_jsc_run_script (web_view, cancellable,
		"Evo.SetElementAttribute(%s,%s,%s,%s,%s)",
		iframe_id,
		element_id,
		namespace_uri,
		qualified_name,
		value);
}

void
e_web_view_jsc_create_style_sheet (WebKitWebView *web_view,
				   const gchar *iframe_id,
				   const gchar *style_sheet_id,
				   const gchar *content,
				   GCancellable *cancellable)
{
	g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));
	g_return_if_fail (style_sheet_id != NULL);

	e_web_view_jsc_run_script (web_view, cancellable,
		"Evo.CreateStyleSheet(%s,%s,%s)",
		iframe_id,
		style_sheet_id,
		content);
}

void
e_web_view_jsc_remove_style_sheet (WebKitWebView *web_view,
				   const gchar *iframe_id,
				   const gchar *style_sheet_id,
				   GCancellable *cancellable)
{
	g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));
	g_return_if_fail (style_sheet_id != NULL);

	e_web_view_jsc_run_script (web_view, cancellable,
		"Evo.RemoveStyleSheet(%s,%s)",
		iframe_id,
		style_sheet_id);
}

void
e_web_view_jsc_add_rule_into_style_sheet (WebKitWebView *web_view,
					  const gchar *iframe_id,
					  const gchar *style_sheet_id,
					  const gchar *selector,
					  const gchar *style,
					  GCancellable *cancellable)
{
	g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));
	g_return_if_fail (style_sheet_id != NULL);

	e_web_view_jsc_run_script (web_view, cancellable,
		"Evo.AddRuleIntoStyleSheet(%s,%s,%s,%s)",
		iframe_id,
		style_sheet_id,
		selector,
		style);
}

void
e_web_view_jsc_register_element_clicked (WebKitWebView *web_view,
					 const gchar *iframe_id,
					 const gchar *elem_classes,
					 GCancellable *cancellable)
{
	g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));
	g_return_if_fail (elem_classes != NULL);

	e_web_view_jsc_run_script (web_view, cancellable,
		"Evo.RegisterElementClicked(%s,%s)",
		iframe_id,
		elem_classes);
}

static gboolean
ewv_jsc_get_content_finish (WebKitWebView *web_view,
			    GAsyncResult *result,
			    GSList **out_texts,
			    GError **error)
{
	JSCValue *value;
	GError *local_error = NULL;

	g_return_val_if_fail (WEBKIT_IS_WEB_VIEW (web_view), FALSE);
	g_return_val_if_fail (result != NULL, FALSE);
	g_return_val_if_fail (out_texts != NULL, FALSE);

	*out_texts = NULL;

	value = webkit_web_view_evaluate_javascript_finish (web_view, result, &local_error);

	if (local_error) {
		g_propagate_error (error, local_error);
		g_clear_object (&value);
		return FALSE;
	}

	if (value) {
		JSCException *exception;
		exception = jsc_context_get_exception (jsc_value_get_context (value));

		if (exception) {
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Call failed: %s", jsc_exception_get_message (exception));
			jsc_context_clear_exception (jsc_value_get_context (value));
			g_clear_object (&value);
			return FALSE;
		}

		if (jsc_value_is_string (value)) {
			*out_texts = g_slist_prepend (*out_texts, jsc_value_to_string (value));
		} else if (jsc_value_is_object (value)) {
			*out_texts = g_slist_prepend (*out_texts, e_web_view_jsc_get_object_property_string (value, "html", NULL));
			*out_texts = g_slist_prepend (*out_texts, e_web_view_jsc_get_object_property_string (value, "plain", NULL));
		}

		g_clear_object (&value);
	}

	return TRUE;
}

void
e_web_view_jsc_get_selection (WebKitWebView *web_view,
			      ETextFormat format,
			      GCancellable *cancellable,
			      GAsyncReadyCallback callback,
			      gpointer user_data)
{
	gchar *script;

	g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));

	script = e_web_view_jsc_printf_script ("Evo.GetSelection(%d)", format);

	webkit_web_view_evaluate_javascript (web_view, script, -1, NULL, NULL, cancellable, callback, user_data);

	g_free (script);
}

gboolean
e_web_view_jsc_get_selection_finish (WebKitWebView *web_view,
				     GAsyncResult *result,
				     GSList **out_texts,
				     GError **error)
{
	g_return_val_if_fail (WEBKIT_IS_WEB_VIEW (web_view), FALSE);
	g_return_val_if_fail (result != NULL, FALSE);
	g_return_val_if_fail (out_texts != NULL, FALSE);

	return ewv_jsc_get_content_finish (web_view, result, out_texts, error);
}

void
e_web_view_jsc_get_document_content (WebKitWebView *web_view,
				     const gchar *iframe_id,
				     ETextFormat format,
				     GCancellable *cancellable,
				     GAsyncReadyCallback callback,
				     gpointer user_data)
{
	gchar *script;

	g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));

	script = e_web_view_jsc_printf_script ("Evo.GetDocumentContent(%s,%d)", iframe_id, format);

	webkit_web_view_evaluate_javascript (web_view, script, -1, NULL, NULL, cancellable, callback, user_data);

	g_free (script);
}

gboolean
e_web_view_jsc_get_document_content_finish (WebKitWebView *web_view,
					    GAsyncResult *result,
					    GSList **out_texts,
					    GError **error)
{
	g_return_val_if_fail (WEBKIT_IS_WEB_VIEW (web_view), FALSE);
	g_return_val_if_fail (result != NULL, FALSE);
	g_return_val_if_fail (out_texts != NULL, FALSE);

	return ewv_jsc_get_content_finish (web_view, result, out_texts, error);
}

void
e_web_view_jsc_get_element_content (WebKitWebView *web_view,
				    const gchar *iframe_id,
				    const gchar *element_id,
				    ETextFormat format,
				    gboolean use_outer_html,
				    GCancellable *cancellable,
				    GAsyncReadyCallback callback,
				    gpointer user_data)
{
	gchar *script;

	g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));
	g_return_if_fail (element_id != NULL);

	script = e_web_view_jsc_printf_script ("Evo.GetElementContent(%s,%s,%d,%x)", iframe_id, element_id, format, use_outer_html);

	webkit_web_view_evaluate_javascript (web_view, script, -1, NULL, NULL, cancellable, callback, user_data);

	g_free (script);
}

gboolean
e_web_view_jsc_get_element_content_finish (WebKitWebView *web_view,
					   GAsyncResult *result,
					   GSList **out_texts,
					   GError **error)
{
	g_return_val_if_fail (WEBKIT_IS_WEB_VIEW (web_view), FALSE);
	g_return_val_if_fail (result != NULL, FALSE);
	g_return_val_if_fail (out_texts != NULL, FALSE);

	return ewv_jsc_get_content_finish (web_view, result, out_texts, error);
}

void
e_web_view_jsc_get_element_from_point (WebKitWebView *web_view,
				       gint xx,
				       gint yy,
				       GCancellable *cancellable,
				       GAsyncReadyCallback callback,
				       gpointer user_data)
{
	gchar *script;

	g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));

	script = e_web_view_jsc_printf_script ("Evo.GetElementFromPoint(%d,%d)", xx, yy);

	webkit_web_view_evaluate_javascript (web_view, script, -1, NULL, NULL, cancellable, callback, user_data);

	g_free (script);
}

/* Can return TRUE, but set all out parameters to NULL */
gboolean
e_web_view_jsc_get_element_from_point_finish (WebKitWebView *web_view,
					      GAsyncResult *result,
					      gchar **out_iframe_src,
					      gchar **out_iframe_id,
					      gchar **out_element_id,
					      GError **error)
{
	JSCValue *value;
	GError *local_error = NULL;

	g_return_val_if_fail (WEBKIT_IS_WEB_VIEW (web_view), FALSE);
	g_return_val_if_fail (result != NULL, FALSE);

	if (out_iframe_src)
		*out_iframe_src = NULL;
	if (out_iframe_id)
		*out_iframe_id = NULL;
	if (out_element_id)
		*out_element_id = NULL;

	value = webkit_web_view_evaluate_javascript_finish (web_view, result, &local_error);

	if (local_error) {
		g_propagate_error (error, local_error);
		g_clear_object (&value);
		return FALSE;
	}

	if (value) {
		JSCException *exception;

		exception = jsc_context_get_exception (jsc_value_get_context (value));

		if (exception) {
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Call failed: %s", jsc_exception_get_message (exception));
			jsc_context_clear_exception (jsc_value_get_context (value));
			g_clear_object (&value);
			return FALSE;
		}

		if (jsc_value_is_object (value)) {
			if (out_iframe_src)
				*out_iframe_src = e_web_view_jsc_get_object_property_string (value, "iframe-src", NULL);
			if (out_iframe_id)
				*out_iframe_id = e_web_view_jsc_get_object_property_string (value, "iframe-id", NULL);
			if (out_element_id)
				*out_element_id = e_web_view_jsc_get_object_property_string (value, "elem-id", NULL);
		} else if (!jsc_value_is_null (value)) {
			g_warn_if_reached ();
		}

		g_clear_object (&value);
	}

	return TRUE;
}
