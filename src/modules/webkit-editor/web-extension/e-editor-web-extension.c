/*
 * e-editor-web-extension.c
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

#include <webkit2/webkit-web-extension.h>

#include <libedataserver/libedataserver.h>

#define E_UTIL_INCLUDE_WITHOUT_WEBKIT 1
#include "e-util/e-util.h"
#undef E_UTIL_INCLUDE_WITHOUT_WEBKIT

#include "e-editor-web-extension.h"

struct _EEditorWebExtensionPrivate {
	WebKitWebExtension *wk_extension;
	ESpellChecker *spell_checker;
	GSList *known_plugins; /* gchar * - full filename to known plugins */
};

G_DEFINE_TYPE_WITH_PRIVATE (EEditorWebExtension, e_editor_web_extension, G_TYPE_OBJECT)

static void
e_editor_web_extension_dispose (GObject *object)
{
	EEditorWebExtension *extension = E_EDITOR_WEB_EXTENSION (object);

	g_clear_object (&extension->priv->wk_extension);
	g_clear_object (&extension->priv->spell_checker);

	g_slist_free_full (extension->priv->known_plugins, g_free);
	extension->priv->known_plugins = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_editor_web_extension_parent_class)->dispose (object);
}

static void
e_editor_web_extension_class_init (EEditorWebExtensionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = e_editor_web_extension_dispose;
}

static void
e_editor_web_extension_init (EEditorWebExtension *extension)
{
	extension->priv = e_editor_web_extension_get_instance_private (extension);
	extension->priv->spell_checker = NULL;
}

static gpointer
e_editor_web_extension_create_instance (gpointer data)
{
	return g_object_new (E_TYPE_EDITOR_WEB_EXTENSION, NULL);
}

EEditorWebExtension *
e_editor_web_extension_get_default (void)
{
	static GOnce once_init = G_ONCE_INIT;
	return E_EDITOR_WEB_EXTENSION (g_once (&once_init, e_editor_web_extension_create_instance, NULL));
}

static gboolean
use_sources_js_file (void)
{
	static gint res = -1;

	if (res == -1)
		res = g_strcmp0 (g_getenv ("E_HTML_EDITOR_TEST_SOURCES"), "1") == 0 ? 1 : 0;

	return res;
}

static gboolean
load_javascript_file (JSCContext *jsc_context,
		      const gchar *js_filename,
		      const gchar *filename)
{
	JSCValue *result;
	JSCException *exception;
	gchar *content, *resource_uri;
	gsize length = 0;
	GError *error = NULL;
	gboolean success = TRUE;

	g_return_val_if_fail (jsc_context != NULL, FALSE);

	if (!g_file_get_contents (filename, &content, &length, &error)) {
		g_warning ("Failed to load '%s': %s", filename, error ? error->message : "Unknown error");

		g_clear_error (&error);

		return FALSE;
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
		success = FALSE;
	}

	g_clear_object (&result);
	g_free (content);

	return success;
}

static void
load_javascript_plugins (JSCContext *jsc_context,
			 const gchar *top_path,
			 GSList **out_loaded_plugins)
{
	const gchar *dirfile;
	gchar *path;
	GDir *dir;

	g_return_if_fail (jsc_context != NULL);

	/* Do not load plugins during unit tests */
	if (use_sources_js_file ())
		return;

	path = g_build_filename (top_path, "webkit-editor-plugins", NULL);

	dir = g_dir_open (path, 0, NULL);
	if (!dir) {
		g_free (path);
		return;
	}

	while (dirfile = g_dir_read_name (dir), dirfile) {
		if (g_str_has_suffix (dirfile, ".js") ||
		    g_str_has_suffix (dirfile, ".Js") ||
		    g_str_has_suffix (dirfile, ".jS") ||
		    g_str_has_suffix (dirfile, ".JS")) {
			gchar *filename;

			filename = g_build_filename (path, dirfile, NULL);
			if (load_javascript_file (jsc_context, filename, filename))
				*out_loaded_plugins = g_slist_prepend (*out_loaded_plugins, filename);
			else
				g_free (filename);
		}
	}

	g_dir_close (dir);
	g_free (path);
}

static void
load_javascript_builtin_file (JSCContext *jsc_context,
			      const gchar *js_filename)
{
	gchar *filename = NULL;

	g_return_if_fail (jsc_context != NULL);

	if (use_sources_js_file ()) {
		const gchar *source_webkitdatadir;

		source_webkitdatadir = g_getenv ("EVOLUTION_SOURCE_WEBKITDATADIR");

		if (source_webkitdatadir && *source_webkitdatadir) {
			filename = g_build_filename (source_webkitdatadir, js_filename, NULL);

			if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
				g_warning ("Cannot find '%s', using installed file '%s/%s' instead", filename, EVOLUTION_WEBKITDATADIR, js_filename);

				g_clear_pointer (&filename, g_free);
			}
		} else {
			g_warning ("Environment variable 'EVOLUTION_SOURCE_WEBKITDATADIR' not set or invalid value, using installed file '%s/%s' instead", EVOLUTION_WEBKITDATADIR, js_filename);
		}
	}

	if (!filename)
		filename = g_build_filename (EVOLUTION_WEBKITDATADIR, js_filename, NULL);

	load_javascript_file (jsc_context, js_filename, filename);

	g_free (filename);
}

static void
evo_editor_find_pattern (const gchar *text,
			 const gchar *pattern,
			 gint *out_start,
			 gint *out_end)
{
	GRegex *regex;

	g_return_if_fail (out_start != NULL);
	g_return_if_fail (out_end != NULL);

	*out_start = -1;
	*out_end = -1;

	regex = g_regex_new (pattern, 0, 0, NULL);
	if (regex) {
		GMatchInfo *match_info = NULL;
		gint start = -1, end = -1;

		if (g_regex_match_all (regex, text, G_REGEX_MATCH_NOTEMPTY, &match_info) &&
		    g_match_info_fetch_pos (match_info, 0, &start, &end) &&
		    start >= 0 && end >= 0) {
			*out_start = start;
			*out_end = end;
		}

		if (match_info)
			g_match_info_free (match_info);
		g_regex_unref (regex);
	}
}

/* Returns 'null', when no match for magicLinks in 'text' were found, otherwise
   returns an array of 'object { text : string, [ href : string] };' with the text
   split into parts, where those with also 'href' property defined are meant
   to be anchors. */
static JSCValue *
evo_editor_jsc_split_text_with_links (const gchar *text,
				      JSCContext *jsc_context)
{
	/* stephenhay from https://mathiasbynens.be/demo/url-regex */
	const gchar *URL_PATTERN = "((?:(?:(?:"
				   "news|telnet|nntp|file|https?|s?ftp|webcals?|localhost|ssh"
				   ")\\:\\/\\/)|(?:www\\.|ftp\\.))[^\\s\\/\\$\\.\\?#].[^\\s]*+)";
	/* from camel-url-scanner.c */
	const gchar *URL_INVALID_TRAILING_CHARS = ",.:;?!-|}])\">";
	/* http://www.w3.org/TR/html5/forms.html#valid-e-mail-address */
	const gchar *EMAIL_PATTERN = "[a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}"
				     "[a-zA-Z0-9])?(?:\\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*+";
	JSCValue *array = NULL;
	guint array_len = 0;
	gboolean done = FALSE;

	if (!text || !*text)
		return jsc_value_new_null (jsc_context);

	#define add_to_array(_obj) G_STMT_START { \
		if (!array) \
			array = jsc_value_new_array (jsc_context, G_TYPE_NONE); \
		jsc_value_object_set_property_at_index (array, array_len, _obj); \
		array_len++; \
		} G_STMT_END

	while (!done) {
		gboolean is_email;
		gint start = -1, end = -1;

		done = TRUE;

		is_email = strchr (text, '@') && !strstr (text, "://");

		evo_editor_find_pattern (text, is_email ? EMAIL_PATTERN : URL_PATTERN, &start, &end);

		if (start >= 0 && end >= 0) {
			const gchar *url_end, *ptr;

			url_end = text + end - 1;

			/* Stop on the angle brackets, which cannot be part of the URL (see RFC 3986 Appendix C) */
			for (ptr = text + start; ptr <= url_end; ptr++) {
				if (*ptr == '<' || *ptr == '>') {
					end = ptr - text;
					url_end = text + end - 1;
					break;
				}
			}

			/* URLs are extremely unlikely to end with any punctuation, so
			 * strip any trailing punctuation off from link and put it after
			 * the link. Do the same for any closing double-quotes as well. */
			while (end > start && *url_end && strchr (URL_INVALID_TRAILING_CHARS, *url_end)) {
				gchar open_bracket = 0, close_bracket = *url_end;

				if (close_bracket == ')')
					open_bracket = '(';
				else if (close_bracket == '}')
					open_bracket = '{';
				else if (close_bracket == ']')
					open_bracket = '[';
				else if (close_bracket == '>')
					open_bracket = '<';

				if (open_bracket != 0) {
					gint n_opened = 0, n_closed = 0;

					for (ptr = text + start; ptr <= url_end; ptr++) {
						if (*ptr == open_bracket)
							n_opened++;
						else if (*ptr == close_bracket)
							n_closed++;
					}

					/* The closing bracket can match one inside the URL,
					   thus keep it there. */
					if (n_opened > 0 && n_opened - n_closed >= 0)
						break;
				}

				url_end--;
				end--;
			}

			if (end > start) {
				JSCValue *object, *string;
				gchar *url, *tmp;

				if (start > 0) {
					tmp = g_strndup (text, start);

					object = jsc_value_new_object (jsc_context, NULL, NULL);

					string = jsc_value_new_string (jsc_context, tmp);
					jsc_value_object_set_property (object, "text", string);
					g_clear_object (&string);

					add_to_array (object);

					g_clear_object (&object);
					g_free (tmp);
				}

				tmp = g_strndup (text + start, end - start);

				if (is_email)
					url = g_strconcat ("mailto:", tmp, NULL);
				else if (g_str_has_prefix (tmp, "www."))
					url = g_strconcat ("https://", tmp, NULL);
				else
					url = NULL;

				object = jsc_value_new_object (jsc_context, NULL, NULL);

				string = jsc_value_new_string (jsc_context, tmp);
				jsc_value_object_set_property (object, "text", string);
				g_clear_object (&string);

				string = jsc_value_new_string (jsc_context, url ? url : tmp);
				jsc_value_object_set_property (object, "href", string);
				g_clear_object (&string);

				add_to_array (object);

				g_clear_object (&object);
				g_free (tmp);
				g_free (url);

				text = text + end;
				done = FALSE;
			}
		}
	}

	if (array && *text) {
		JSCValue *object, *string;

		object = jsc_value_new_object (jsc_context, NULL, NULL);

		string = jsc_value_new_string (jsc_context, text);
		jsc_value_object_set_property (object, "text", string);
		g_clear_object (&string);

		add_to_array (object);

		g_clear_object (&object);
	}

	#undef add_to_array

	return array ? array : jsc_value_new_null (jsc_context);
}

/* Returns 'null' or an object { text : string, imageUri : string, width : nnn, height : nnn }
   where only the 'text' is required, describing an emoticon. */
static JSCValue *
evo_editor_jsc_lookup_emoticon (const gchar *iconName,
				gboolean use_unicode_smileys,
				JSCContext *jsc_context)
{
	JSCValue *object = NULL;

	if (iconName && *iconName) {
		const EEmoticon *emoticon;

		emoticon = e_emoticon_chooser_lookup_emoticon (iconName);

		if (emoticon) {
			JSCValue *value;

			object = jsc_value_new_object (jsc_context, NULL, NULL);

			if (use_unicode_smileys) {
				value = jsc_value_new_string (jsc_context, emoticon->unicode_character);
				jsc_value_object_set_property (object, "text", value);
				g_clear_object (&value);
			} else {
				gchar *image_uri;

				value = jsc_value_new_string (jsc_context, emoticon->text_face);
				jsc_value_object_set_property (object, "text", value);
				g_clear_object (&value);

				image_uri = e_emoticon_dup_uri (emoticon);

				if (image_uri) {
					value = jsc_value_new_string (jsc_context, image_uri);
					jsc_value_object_set_property (object, "imageUri", value);
					g_clear_object (&value);

					value = jsc_value_new_number (jsc_context, 16);
					jsc_value_object_set_property (object, "width", value);
					g_clear_object (&value);

					value = jsc_value_new_number (jsc_context, 16);
					jsc_value_object_set_property (object, "height", value);
					g_clear_object (&value);

					g_free (image_uri);
				}
			}
		}
	}

	return object ? object : jsc_value_new_null (jsc_context);
}

static void
evo_editor_jsc_set_spell_check_languages (const gchar *langs,
					  GWeakRef *wkrf_extension)
{
	EEditorWebExtension *extension;
	gchar **strv;

	g_return_if_fail (wkrf_extension != NULL);

	extension = g_weak_ref_get (wkrf_extension);

	if (!extension)
		return;

	if (langs && *langs)
		strv = g_strsplit (langs, "|", -1);
	else
		strv = NULL;

	if (!extension->priv->spell_checker)
		extension->priv->spell_checker = e_spell_checker_new ();

	e_spell_checker_set_active_languages (extension->priv->spell_checker, (const gchar * const *) strv);

	g_object_unref (extension);
	g_strfreev (strv);
}

/* Returns whether the 'word' is a properly spelled word. It checks
   with languages previously set by EvoEditor.SetSpellCheckLanguages(). */
static gboolean
evo_editor_jsc_spell_check_word (const gchar *word,
				 GWeakRef *wkrf_extension)
{
	EEditorWebExtension *extension;
	gboolean is_correct;

	g_return_val_if_fail (wkrf_extension != NULL, FALSE);

	extension = g_weak_ref_get (wkrf_extension);

	if (!extension)
		return TRUE;

	/* It should be created as part of EvoEditor.SetSpellCheckLanguages(). */
	g_warn_if_fail (extension->priv->spell_checker != NULL);

	if (!extension->priv->spell_checker)
		extension->priv->spell_checker = e_spell_checker_new ();

	is_correct = e_spell_checker_check_word (extension->priv->spell_checker, word, -1);

	g_object_unref (extension);

	return is_correct;
}

static gboolean
evo_convert_jsc_link_requires_reference (const gchar *href,
					 const gchar *text,
					 gpointer user_data)
{
	return e_util_link_requires_reference (href, text);
}

static void
window_object_cleared_cb (WebKitScriptWorld *world,
			  WebKitWebPage *page,
			  WebKitFrame *frame,
			  gpointer user_data)
{
	EEditorWebExtension *extension = user_data;
	JSCContext *jsc_context;
	JSCValue *jsc_editor;
	JSCValue *jsc_convert;

	g_return_if_fail (E_IS_EDITOR_WEB_EXTENSION (extension));

	/* Load the javascript files only to the main frame, not to the subframes */
	if (!webkit_frame_is_main_frame (frame))
		return;

	jsc_context = webkit_frame_get_js_context (frame);

	/* Read in order approximately as each other uses the previous */
	load_javascript_builtin_file (jsc_context, "e-convert.js");
	load_javascript_builtin_file (jsc_context, "e-selection.js");
	load_javascript_builtin_file (jsc_context, "e-undo-redo.js");
	load_javascript_builtin_file (jsc_context, "e-editor.js");

	jsc_editor = jsc_context_get_value (jsc_context, "EvoEditor");

	if (jsc_editor) {
		JSCValue *jsc_function;
		const gchar *func_name;

		/* EvoEditor.splitTextWithLinks(text) */
		func_name = "splitTextWithLinks";
		jsc_function = jsc_value_new_function (jsc_context, func_name,
			G_CALLBACK (evo_editor_jsc_split_text_with_links), g_object_ref (jsc_context), g_object_unref,
			JSC_TYPE_VALUE, 1, G_TYPE_STRING);

		jsc_value_object_set_property (jsc_editor, func_name, jsc_function);

		g_clear_object (&jsc_function);

		/* EvoEditor.lookupEmoticon(iconName, useUnicodeSmileys) */
		func_name = "lookupEmoticon";
		jsc_function = jsc_value_new_function (jsc_context, func_name,
			G_CALLBACK (evo_editor_jsc_lookup_emoticon), g_object_ref (jsc_context), g_object_unref,
			JSC_TYPE_VALUE, 2, G_TYPE_STRING, G_TYPE_BOOLEAN);

		jsc_value_object_set_property (jsc_editor, func_name, jsc_function);

		g_clear_object (&jsc_function);

		/* EvoEditor.SetSpellCheckLanguages(langs) */
		func_name = "SetSpellCheckLanguages";
		jsc_function = jsc_value_new_function (jsc_context, func_name,
			G_CALLBACK (evo_editor_jsc_set_spell_check_languages), e_weak_ref_new (extension), (GDestroyNotify) e_weak_ref_free,
			G_TYPE_NONE, 1, G_TYPE_STRING);

		jsc_value_object_set_property (jsc_editor, func_name, jsc_function);

		g_clear_object (&jsc_function);

		/* EvoEditor.SpellCheckWord(word) */
		func_name = "SpellCheckWord";
		jsc_function = jsc_value_new_function (jsc_context, func_name,
			G_CALLBACK (evo_editor_jsc_spell_check_word), e_weak_ref_new (extension), (GDestroyNotify) e_weak_ref_free,
			G_TYPE_BOOLEAN, 1, G_TYPE_STRING);

		jsc_value_object_set_property (jsc_editor, func_name, jsc_function);

		g_clear_object (&jsc_function);
		g_clear_object (&jsc_editor);
	}

	jsc_convert = jsc_context_get_value (jsc_context, "EvoConvert");

	if (jsc_convert) {
		JSCValue *jsc_function;
		const gchar *func_name;

		/* EvoConvert.linkRequiresReference(href, text) */
		func_name = "linkRequiresReference";
		jsc_function = jsc_value_new_function (jsc_context, func_name,
			G_CALLBACK (evo_convert_jsc_link_requires_reference), NULL, NULL,
			G_TYPE_BOOLEAN, 2, G_TYPE_STRING, G_TYPE_STRING);

		jsc_value_object_set_property (jsc_convert, func_name, jsc_function);

		g_clear_object (&jsc_function);
		g_clear_object (&jsc_convert);
	}

	if (extension->priv->known_plugins) {
		GSList *link;

		for (link = extension->priv->known_plugins; link; link = g_slist_next (link)) {
			const gchar *filename = link->data;

			if (filename)
				load_javascript_file (jsc_context, filename, filename);
		}
	} else {
		load_javascript_plugins (jsc_context, EVOLUTION_WEBKITDATADIR, &extension->priv->known_plugins);
		load_javascript_plugins (jsc_context, e_get_user_data_dir (), &extension->priv->known_plugins);

		if (!extension->priv->known_plugins)
			extension->priv->known_plugins = g_slist_prepend (extension->priv->known_plugins, NULL);
		else
			extension->priv->known_plugins = g_slist_reverse (extension->priv->known_plugins);
	}

	g_clear_object (&jsc_context);
}

static gboolean
web_page_send_request_cb (WebKitWebPage *web_page,
			  WebKitURIRequest *request,
			  WebKitURIResponse *redirected_response,
			  EEditorWebExtension *extension)
{
	const gchar *request_uri;
	const gchar *page_uri;

	request_uri = webkit_uri_request_get_uri (request);
	page_uri = webkit_web_page_get_uri (web_page);

	/* Always load the main resource. */
	if (g_strcmp0 (request_uri, page_uri) == 0)
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
web_page_document_loaded_cb (WebKitWebPage *web_page,
			     gpointer user_data)
{
	g_return_if_fail (WEBKIT_IS_WEB_PAGE (web_page));

	window_object_cleared_cb (NULL, web_page, webkit_web_page_get_main_frame (web_page), user_data);
}

static void
web_page_created_cb (WebKitWebExtension *wk_extension,
		     WebKitWebPage *web_page,
		     EEditorWebExtension *extension)
{
	g_return_if_fail (WEBKIT_IS_WEB_PAGE (web_page));
	g_return_if_fail (E_IS_EDITOR_WEB_EXTENSION (extension));

	window_object_cleared_cb (NULL, web_page, webkit_web_page_get_main_frame (web_page), extension);

	g_signal_connect (
		web_page, "send-request",
		G_CALLBACK (web_page_send_request_cb), extension);

	g_signal_connect (
		web_page, "document-loaded",
		G_CALLBACK (web_page_document_loaded_cb), extension);
}

void
e_editor_web_extension_initialize (EEditorWebExtension *extension,
				   WebKitWebExtension *wk_extension)
{
	WebKitScriptWorld *script_world;

	g_return_if_fail (E_IS_EDITOR_WEB_EXTENSION (extension));

	extension->priv->wk_extension = g_object_ref (wk_extension);

	g_signal_connect (
		wk_extension, "page-created",
		G_CALLBACK (web_page_created_cb), extension);

	script_world = webkit_script_world_get_default ();

	g_signal_connect (script_world, "window-object-cleared",
		G_CALLBACK (window_object_cleared_cb), extension);
}
