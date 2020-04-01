/*
 * e-web-extension.c
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
 */

#include "evolution-config.h"

#include <string.h>

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <webkit2/webkit-web-extension.h>

#define E_UTIL_INCLUDE_WITHOUT_WEBKIT
#include <e-util/e-util.h>
#undef E_UTIL_INCLUDE_WITHOUT_WEBKIT

#include "e-web-extension.h"

struct _EWebExtensionPrivate {
	WebKitWebExtension *wk_extension;

	gboolean initialized;
};

G_DEFINE_TYPE_WITH_CODE (EWebExtension, e_web_extension, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static void
e_web_extension_constructed (GObject *object)
{
	G_OBJECT_CLASS (e_web_extension_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
e_web_extension_dispose (GObject *object)
{
	EWebExtension *extension = E_WEB_EXTENSION (object);

	g_clear_object (&extension->priv->wk_extension);

	G_OBJECT_CLASS (e_web_extension_parent_class)->dispose (object);
}

static void
e_web_extension_class_init (EWebExtensionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	g_type_class_add_private (object_class, sizeof (EWebExtensionPrivate));

	object_class->constructed = e_web_extension_constructed;
	object_class->dispose = e_web_extension_dispose;
}

static void
e_web_extension_init (EWebExtension *extension)
{
	extension->priv = G_TYPE_INSTANCE_GET_PRIVATE (extension, E_TYPE_WEB_EXTENSION, EWebExtensionPrivate);

	extension->priv->initialized = FALSE;
}

static gpointer
e_web_extension_create_instance (gpointer data)
{
	return g_object_new (E_TYPE_WEB_EXTENSION, NULL);
}

EWebExtension *
e_web_extension_get (void)
{
	static GOnce once_init = G_ONCE_INIT;

	return E_WEB_EXTENSION (g_once (&once_init, e_web_extension_create_instance, NULL));
}

static gboolean
web_page_send_request_cb (WebKitWebPage *web_page,
                          WebKitURIRequest *request,
                          WebKitURIResponse *redirected_response,
                          EWebExtension *extension)
{
	const gchar *request_uri;
	const gchar *page_uri;

	request_uri = webkit_uri_request_get_uri (request);
	page_uri = webkit_web_page_get_uri (web_page);

	/* Always load the main resource. */
	if (g_strcmp0 (request_uri, page_uri) == 0 ||
	    /* Do not influence real pages, like those with eds OAuth sign-in */
	    g_str_has_prefix (page_uri, "http:") ||
	    g_str_has_prefix (page_uri, "https:"))
		return FALSE;

	if (g_str_has_prefix (request_uri, "http:") ||
	    g_str_has_prefix (request_uri, "https:")) {
		gchar *new_uri;

		new_uri = g_strconcat ("evo-", request_uri, NULL);

		webkit_uri_request_set_uri (request, new_uri);

		g_free (new_uri);
	}

	return FALSE;
}

static void
web_page_created_cb (WebKitWebExtension *wk_extension,
                     WebKitWebPage *web_page,
                     EWebExtension *extension)
{
	g_signal_connect_object (
		web_page, "send-request",
		G_CALLBACK (web_page_send_request_cb),
		extension, 0);
}

/* Returns 'null', when uri is empty or null, otherwise
   returns a string with the constructed uri message */
static JSCValue *
evo_jsc_get_uri_tooltip (const gchar *uri,
						 JSCContext *jsc_context)
{
	gchar *message;
	JSCValue *jsc_message;
	
	message = e_util_get_uri_tooltip (uri);
	jsc_message = jsc_value_new_string (jsc_context, message);
	
	return jsc_message;
}

static void
load_javascript_file (JSCContext *jsc_context,
		      const gchar *js_filename)
{
	JSCValue *result;
	JSCException *exception;
	gchar *content, *filename, *resource_uri;
	gsize length = 0;
	GError *error = NULL;

	g_return_if_fail (jsc_context != NULL);

	filename = g_build_filename (EVOLUTION_WEBKITDATADIR, js_filename, NULL);

	if (!g_file_get_contents (filename, &content, &length, &error)) {
		g_warning ("Failed to load '%s': %s", filename, error ? error->message : "Unknown error");

		g_clear_error (&error);
		g_free (filename);

		return;
	}

	resource_uri = g_strconcat ("resource:///", js_filename, NULL);

	result = jsc_context_evaluate_with_source_uri (jsc_context, content, length, resource_uri, 1);

	g_free (resource_uri);

	exception = jsc_context_get_exception (jsc_context);

	if (exception) {
		g_warning ("Failed to call script '%s': %d:%d: %s",
			filename,
			jsc_exception_get_line_number (exception),
			jsc_exception_get_column_number (exception),
			jsc_exception_get_message (exception));

		jsc_context_clear_exception (jsc_context);
	}

	g_clear_object (&result);
	g_free (filename);
	g_free (content);
}

static void
window_object_cleared_cb (WebKitScriptWorld *world,
			  WebKitWebPage *page,
			  WebKitFrame *frame,
			  gpointer user_data)
{
	JSCContext *jsc_context;
	JSCValue *jsc_editor;

	/* Load the javascript files only to the main frame, not to the subframes */
	if (!webkit_frame_is_main_frame (frame))
		return;

	jsc_context = webkit_frame_get_js_context (frame);

	/* Read e-convert.js first, because e-web-view.js uses it */
	load_javascript_file (jsc_context, "e-convert.js");
	load_javascript_file (jsc_context, "e-web-view.js");

	jsc_editor = jsc_context_get_value (jsc_context, "Evo");
	
	if (jsc_editor){
		JSCValue *jsc_function;
		const gchar *func_name;

		/* Evo.getUriTooltip (uri) */
		func_name = "getUriTooltip";
		jsc_function = jsc_value_new_function (jsc_context, func_name,
			G_CALLBACK (evo_jsc_get_uri_tooltip), g_object_ref (jsc_context), g_object_unref,
			JSC_TYPE_VALUE, 1, G_TYPE_STRING);

		jsc_value_object_set_property (jsc_editor, func_name, jsc_function);

		g_clear_object (&jsc_function);
	}

	g_clear_object (&jsc_context);
}

void
e_web_extension_initialize (EWebExtension *extension,
                            WebKitWebExtension *wk_extension)
{
	WebKitScriptWorld *script_world;

	g_return_if_fail (E_IS_WEB_EXTENSION (extension));

	if (extension->priv->initialized)
		return;

	extension->priv->initialized = TRUE;

	extension->priv->wk_extension = g_object_ref (wk_extension);

	g_signal_connect (
		wk_extension, "page-created",
		G_CALLBACK (web_page_created_cb), extension);

	script_world = webkit_script_world_get_default ();

	g_signal_connect (script_world, "window-object-cleared",
		G_CALLBACK (window_object_cleared_cb), NULL);
}

WebKitWebExtension *
e_web_extension_get_webkit_extension (EWebExtension *extension)
{
	g_return_val_if_fail (E_IS_WEB_EXTENSION (extension), NULL);

	return extension->priv->wk_extension;
}
