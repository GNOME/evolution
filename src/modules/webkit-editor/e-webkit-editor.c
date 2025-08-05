/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
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

#include <glib/gi18n-lib.h>

#include "e-webkit-editor.h"

#include "e-util/e-util.h"
#include "composer/e-msg-composer.h"
#include "mail/e-cid-request.h"
#include "mail/e-http-request.h"

#include <string.h>

/* FIXME WK2 Move to e-content-editor? */
#define UNICODE_NBSP "\xc2\xa0"
#define SPACES_PER_LIST_LEVEL 3
#define SPACES_ORDERED_LIST_FIRST_LEVEL 6

#define DEFAULT_CSS_STYLE \
	"pre,code,address {\n" \
	"  margin: 0px;\n" \
	"}\n" \
	"h1,h2,h3,h4,h5,h6 {\n" \
	"  margin-top: 0.2em;\n" \
	"  margin-bottom: 0.2em;\n" \
	"}\n" \
	"ol,ul {\n" \
	"  margin-top: 0em;\n" \
	"  margin-bottom: 0em;\n" \
	"}\n" \
	"blockquote {\n" \
	"  margin-top: 0em;\n" \
	"  margin-bottom: 0em;\n" \
	"}\n"

enum {
	PROP_0,
	PROP_IS_MALFUNCTION,
	PROP_CAN_COPY,
	PROP_CAN_CUT,
	PROP_CAN_PASTE,
	PROP_CAN_REDO,
	PROP_CAN_UNDO,
	PROP_CHANGED,
	PROP_EDITABLE,
	PROP_MODE,
	PROP_SPELL_CHECK_ENABLED,
	PROP_SPELL_CHECKER,
	PROP_START_BOTTOM,
	PROP_TOP_SIGNATURE,
	PROP_VISUALLY_WRAP_LONG_LINES,
	PROP_LAST_ERROR,

	PROP_ALIGNMENT,
	PROP_BACKGROUND_COLOR,
	PROP_BLOCK_FORMAT,
	PROP_BOLD,
	PROP_FONT_COLOR,
	PROP_FONT_NAME,
	PROP_FONT_SIZE,
	PROP_INDENT_LEVEL,
	PROP_ITALIC,
	PROP_STRIKETHROUGH,
	PROP_SUBSCRIPT,
	PROP_SUPERSCRIPT,
	PROP_UNDERLINE,

	PROP_NORMAL_PARAGRAPH_WIDTH,
	PROP_MAGIC_LINKS,
	PROP_MAGIC_SMILEYS,
	PROP_UNICODE_SMILEYS,
	PROP_WRAP_QUOTED_TEXT_IN_REPLIES,
	PROP_MINIMUM_FONT_SIZE,
	PROP_PASTE_PLAIN_PREFER_PRE,
	PROP_LINK_TO_TEXT
};

struct _EWebKitEditorPrivate {
	EContentEditorInitializedCallback initialized_callback;
	gpointer initialized_user_data;

	GHashTable *scheme_handlers; /* const gchar *scheme ~> EContentRequest */
	GCancellable *cancellable;

	EContentEditorMode mode;
	gboolean changed;
	gboolean can_copy;
	gboolean can_cut;
	gboolean can_paste;
	gboolean can_undo;
	gboolean can_redo;
	gboolean paste_plain_prefer_pre;

	guint32 style_flags;
	guint32 temporary_style_flags; /* that's for collapsed selection, format changes only after something is typed */
	guint32 last_button_press_time_ms;
	gint indent_level;

	GdkRGBA *background_color;
	GdkRGBA *font_color;
	GdkRGBA *body_fg_color;
	GdkRGBA *body_bg_color;
	GdkRGBA *body_link_color;
	GdkRGBA *body_vlink_color;

	GdkRGBA theme_bgcolor;
	GdkRGBA theme_fgcolor;
	GdkRGBA theme_link_color;
	GdkRGBA theme_vlink_color;

	gchar *font_name;
	gchar *body_font_name;

	guint font_size;
	gint normal_paragraph_width;
	gboolean magic_links;
	gboolean magic_smileys;
	gboolean unicode_smileys;
	gboolean wrap_quoted_text_in_replies;

	EContentEditorBlockFormat block_format;
	EContentEditorAlignment alignment;

	GdkRectangle caret_client_rect;

	/* For context menu */
	gchar *context_menu_caret_word;
	guint32 context_menu_node_flags; /* bit-or of EContentEditorNodeFlags */

	gchar *current_user_stylesheet;

	WebKitLoadEvent webkit_load_event;

	GQueue *post_reload_operations;

	GSettings *mail_settings;
	GSettings *font_settings;

	GHashTable *old_settings;

	ESpellChecker *spell_checker;
	gboolean spell_check_enabled;

	gboolean visually_wrap_long_lines;

	WebKitFindController *find_controller; /* not referenced; set to non-NULL only if the search is in progress */
	gboolean performing_replace_all;
	guint replaced_count;
	gchar *replace_with;
	gulong found_text_handler_id;
	gulong failed_to_find_text_handler_id;
	gboolean current_text_not_found;

	gboolean performing_drag;
	gulong drag_data_received_handler_id;

	gchar *last_hover_uri;

	EThreeState start_bottom;
	EThreeState top_signature;
	gboolean is_malfunction;

	GError *last_error;

	gint minimum_font_size;
	EHTMLLinkToText link_to_text;
};

static const GdkRGBA black = { 0, 0, 0, 1 };
static const GdkRGBA transparent = { 0, 0, 0, 0 };

typedef enum {
	E_WEBKIT_EDITOR_STYLE_NONE		= 0,
	E_WEBKIT_EDITOR_STYLE_IS_BOLD		= 1 << 0,
	E_WEBKIT_EDITOR_STYLE_IS_ITALIC		= 1 << 1,
	E_WEBKIT_EDITOR_STYLE_IS_UNDERLINE	= 1 << 2,
	E_WEBKIT_EDITOR_STYLE_IS_STRIKETHROUGH	= 1 << 3,
	E_WEBKIT_EDITOR_STYLE_IS_SUBSCRIPT	= 1 << 4,
	E_WEBKIT_EDITOR_STYLE_IS_SUPERSCRIPT	= 1 << 5
} EWebKitEditorStyleFlags;

typedef void (*PostReloadOperationFunc) (EWebKitEditor *wk_editor, gpointer data, EContentEditorInsertContentFlags flags);

typedef struct {
	PostReloadOperationFunc func;
	EContentEditorInsertContentFlags flags;
	gpointer data;
	GDestroyNotify data_free_func;
} PostReloadOperation;

static void e_webkit_editor_content_editor_init (EContentEditorInterface *iface);
static void e_webkit_editor_cid_resolver_init (ECidResolverInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EWebKitEditor, e_webkit_editor, WEBKIT_TYPE_WEB_VIEW,
	G_ADD_PRIVATE (EWebKitEditor)
	G_IMPLEMENT_INTERFACE (E_TYPE_CONTENT_EDITOR, e_webkit_editor_content_editor_init)
	G_IMPLEMENT_INTERFACE (E_TYPE_CID_RESOLVER, e_webkit_editor_cid_resolver_init));

typedef struct _EWebKitEditorFlagClass {
	GObjectClass parent_class;
} EWebKitEditorFlagClass;

typedef struct _EWebKitEditorFlag {
	GObject parent;
	gboolean is_set;
} EWebKitEditorFlag;

GType e_webkit_editor_flag_get_type (void);

G_DEFINE_TYPE (EWebKitEditorFlag, e_webkit_editor_flag, G_TYPE_OBJECT)

enum {
	E_WEBKIT_EDITOR_FLAG_FLAGGED,
	E_WEBKIT_EDITOR_FLAG_LAST_SIGNAL
};

static guint e_webkit_editor_flag_signals[E_WEBKIT_EDITOR_FLAG_LAST_SIGNAL];

static void
e_webkit_editor_flag_class_init (EWebKitEditorFlagClass *klass)
{
	e_webkit_editor_flag_signals[E_WEBKIT_EDITOR_FLAG_FLAGGED] = g_signal_new (
		"flagged",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);
}

static void
e_webkit_editor_flag_init (EWebKitEditorFlag *flag)
{
	flag->is_set = FALSE;
}

static void
e_webkit_editor_flag_set (EWebKitEditorFlag *flag)
{
	flag->is_set = TRUE;

	g_signal_emit (flag, e_webkit_editor_flag_signals[E_WEBKIT_EDITOR_FLAG_FLAGGED], 0, NULL);
}

static JSCValue * /* transfer full */
webkit_editor_call_jsc_sync (EWebKitEditor *wk_editor,
			     const gchar *script_format,
			     ...) G_GNUC_PRINTF (2, 3);

typedef struct _JSCCallData {
	EWebKitEditorFlag *flag;
	gchar *script;
	JSCValue *result;
} JSCCallData;

static void
webkit_editor_jsc_call_done_cb (GObject *source,
				GAsyncResult *result,
				gpointer user_data)
{
	JSCValue *value;
	JSCCallData *jcd = user_data;
	GError *error = NULL;

	value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (source), result, &error);

	if (error) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
		    (!g_error_matches (error, WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED) ||
		     /* WebKit can return empty error message, thus ignore those. */
		     (error->message && *(error->message))))
			g_warning ("Failed to call '%s' function: %s:%d: %s", jcd->script, g_quark_to_string (error->domain), error->code, error->message);
		g_clear_error (&error);
	}

	if (value) {
		JSCException *exception;

		exception = jsc_context_get_exception (jsc_value_get_context (value));

		if (exception) {
			g_warning ("Failed to call '%s': %s", jcd->script, jsc_exception_get_message (exception));
			jsc_context_clear_exception (jsc_value_get_context (value));
		} else if (!jsc_value_is_null (value) && !jsc_value_is_undefined (value)) {
			jcd->result = g_object_ref (value);
		}

		g_clear_object (&value);
	}

	e_webkit_editor_flag_set (jcd->flag);
}

static JSCValue * /* transfer full */
webkit_editor_call_jsc_sync (EWebKitEditor *wk_editor,
			     const gchar *script_format,
			     ...)
{
	JSCCallData jcd;
	va_list va;

	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), NULL);
	g_return_val_if_fail (script_format != NULL, NULL);

	va_start (va, script_format);
	jcd.script = e_web_view_jsc_vprintf_script (script_format, va);
	va_end (va);

	jcd.flag = g_object_new (e_webkit_editor_flag_get_type (), NULL);
	jcd.result = NULL;

	webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (wk_editor), jcd.script, -1, NULL,
		NULL, wk_editor->priv->cancellable, webkit_editor_jsc_call_done_cb, &jcd);

	if (!jcd.flag->is_set) {
		GMainLoop *loop;
		gulong handler_id;

		loop = g_main_loop_new (NULL, FALSE);

		handler_id = g_signal_connect_swapped (jcd.flag, "flagged", G_CALLBACK (g_main_loop_quit), loop);

		g_main_loop_run (loop);
		g_main_loop_unref (loop);

		g_signal_handler_disconnect (jcd.flag, handler_id);
	}

	g_clear_object (&jcd.flag);
	g_free (jcd.script);

	return jcd.result;
}

static gboolean
webkit_editor_extract_and_free_jsc_boolean (JSCValue *jsc_value,
					    gboolean default_value)
{
	gboolean value;

	if (jsc_value && jsc_value_is_boolean (jsc_value))
		value = jsc_value_to_boolean (jsc_value);
	else
		value = default_value;

	g_clear_object (&jsc_value);

	return value;
}

static gint32
webkit_editor_extract_and_free_jsc_int32 (JSCValue *jsc_value,
					  gint32 default_value)
{
	gint32 value;

	if (jsc_value && jsc_value_is_number (jsc_value))
		value = jsc_value_to_int32 (jsc_value);
	else
		value = default_value;

	g_clear_object (&jsc_value);

	return value;
}

static gchar *
webkit_editor_extract_and_free_jsc_string (JSCValue *jsc_value,
					   const gchar *default_value)
{
	gchar *value;

	if (jsc_value && jsc_value_is_string (jsc_value))
		value = jsc_value_to_string (jsc_value);
	else
		value = g_strdup (default_value);

	g_clear_object (&jsc_value);

	return value;
}

static const gchar *
webkit_editor_utils_int_to_string (gchar *inout_buff,
				   gulong buff_len,
				   gint value)
{
	g_snprintf (inout_buff, buff_len, "%d", value);

	return inout_buff;
}

static const gchar *
webkit_editor_utils_int_with_unit_to_string (gchar *inout_buff,
					     gulong buff_len,
					     gint value,
					     EContentEditorUnit unit)
{
	if (unit == E_CONTENT_EDITOR_UNIT_AUTO)
		g_snprintf (inout_buff, buff_len, "auto");
	else
		g_snprintf (inout_buff, buff_len, "%d%s",
			value,
			(unit == E_CONTENT_EDITOR_UNIT_PIXEL) ? "px" : "%");

	return inout_buff;
}

static const gchar *
webkit_editor_utils_color_to_string (gchar *inout_buff,
				     gulong buff_len,
				     const GdkRGBA *color)
{
	if (color && color->alpha > 1e-9)
		g_snprintf (inout_buff, buff_len, "#%06x", e_rgba_to_value (color));
	else if (buff_len)
		inout_buff[0] = '\0';

	return inout_buff;
}

static void
webkit_editor_dialog_utils_set_attribute (EWebKitEditor *wk_editor,
					  const gchar *selector,
					  const gchar *name,
					  const gchar *value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));
	g_return_if_fail (name != NULL);

	if (value) {
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.DialogUtilsSetAttribute(%s, %s, %s);",
			selector, name, value);
	} else {
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.DialogUtilsSetAttribute(%s, %s, null);",
			selector, name);
	}
}

static void
webkit_editor_dialog_utils_set_attribute_int (EWebKitEditor *wk_editor,
					      const gchar *selector,
					      const gchar *name,
					      gint value)
{
	gchar str_value[64];

	webkit_editor_dialog_utils_set_attribute (wk_editor, selector, name,
		webkit_editor_utils_int_to_string (str_value, sizeof (str_value), value));
}

static void
webkit_editor_dialog_utils_set_attribute_with_unit (EWebKitEditor *wk_editor,
						    const gchar *selector,
						    const gchar *name,
						    gint value,
						    EContentEditorUnit unit)
{
	gchar str_value[64];

	webkit_editor_dialog_utils_set_attribute (wk_editor, selector, name,
		webkit_editor_utils_int_with_unit_to_string (str_value, sizeof (str_value), value, unit));
}

static void
webkit_editor_dialog_utils_set_attribute_color (EWebKitEditor *wk_editor,
						const gchar *selector,
						const gchar *name,
						const GdkRGBA *color)
{
	gchar str_value[64];

	webkit_editor_dialog_utils_set_attribute (wk_editor, selector, name,
		webkit_editor_utils_color_to_string (str_value, sizeof (str_value), color));
}

static void
webkit_editor_dialog_utils_set_table_attribute (EWebKitEditor *wk_editor,
						EContentEditorScope scope,
						const gchar *name,
						const gchar *value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));
	g_return_if_fail (name != NULL);

	if (value) {
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.DialogUtilsTableSetAttribute(%d, %s, %s);",
			scope, name, value);
	} else {
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.DialogUtilsTableSetAttribute(%d, %s, null);",
			scope, name);
	}
}

static gchar *
webkit_editor_dialog_utils_get_attribute (EWebKitEditor *wk_editor,
					  const gchar *selector,
					  const gchar *name)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	return webkit_editor_extract_and_free_jsc_string (
		webkit_editor_call_jsc_sync (wk_editor,
			"EvoEditor.DialogUtilsGetAttribute(%s, %s);",
			selector, name),
		NULL);
}

static gint
webkit_editor_dialog_utils_get_attribute_int (EWebKitEditor *wk_editor,
					      const gchar *selector,
					      const gchar *name,
					      gint default_value)
{
	gchar *attr;
	gint value;

	attr = webkit_editor_dialog_utils_get_attribute (wk_editor, selector, name);

	if (attr && *attr)
		value = atoi (attr);
	else
		value = default_value;

	g_free (attr);

	return value;
}

static gint
webkit_editor_dialog_utils_get_attribute_with_unit (EWebKitEditor *wk_editor,
						    const gchar *selector,
						    const gchar *name,
						    gint default_value,
						    EContentEditorUnit *out_unit)
{
	gint result;
	gchar *value;

	*out_unit = E_CONTENT_EDITOR_UNIT_AUTO;

	if (wk_editor->priv->mode != E_CONTENT_EDITOR_MODE_HTML)
		return default_value;

	value = webkit_editor_dialog_utils_get_attribute (wk_editor, selector, name);

	if (value && *value) {
		result = atoi (value);

		if (strstr (value, "%"))
			*out_unit = E_CONTENT_EDITOR_UNIT_PERCENTAGE;
		else if (g_ascii_strncasecmp (value, "auto", 4) != 0)
			*out_unit = E_CONTENT_EDITOR_UNIT_PIXEL;
	} else {
		result = default_value;
	}

	g_free (value);

	return result;
}

static void
webkit_editor_dialog_utils_get_attribute_color (EWebKitEditor *wk_editor,
						const gchar *selector,
						const gchar *name,
						GdkRGBA *out_color)
{
	gchar *value;

	value = webkit_editor_dialog_utils_get_attribute (wk_editor, selector, name);

	if (!value || !*value || !gdk_rgba_parse (out_color, value))
		*out_color = transparent;

	g_free (value);
}

static gboolean
webkit_editor_dialog_utils_has_attribute (EWebKitEditor *wk_editor,
					  const gchar *name)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	return webkit_editor_extract_and_free_jsc_boolean (
		webkit_editor_call_jsc_sync (wk_editor,
			"EvoEditor.DialogUtilsHasAttribute(%s);",
			name),
		FALSE);
}

EWebKitEditor *
e_webkit_editor_new (void)
{
	return g_object_new (E_TYPE_WEBKIT_EDITOR, NULL);
}

static void
webkit_editor_set_last_error (EWebKitEditor *wk_editor,
			      const GError *error)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	g_clear_error (&wk_editor->priv->last_error);

	if (error)
		wk_editor->priv->last_error = g_error_copy (error);
}

static const GError *
webkit_editor_get_last_error (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), NULL);

	return wk_editor->priv->last_error;
}

static gboolean
webkit_editor_can_paste (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->can_paste;
}

static gboolean
webkit_editor_can_cut (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->can_cut;
}

static gboolean
webkit_editor_is_malfunction (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->is_malfunction;
}

static gboolean
webkit_editor_can_copy (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->can_copy;
}

static gboolean
webkit_editor_get_changed (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->changed;
}

static void
webkit_editor_set_changed (EWebKitEditor *wk_editor,
                           gboolean changed)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if (changed)
		e_content_editor_emit_content_changed (E_CONTENT_EDITOR (wk_editor));

	if (wk_editor->priv->changed == changed)
		return;

	wk_editor->priv->changed = changed;

	g_object_notify (G_OBJECT (wk_editor), "changed");
}

static void
webkit_editor_set_can_undo (EWebKitEditor *wk_editor,
                            gboolean can_undo)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if ((wk_editor->priv->can_undo ? 1 : 0) == (can_undo ? 1 : 0))
		return;

	wk_editor->priv->can_undo = can_undo;

	g_object_notify (G_OBJECT (wk_editor), "can-undo");
}

static void
webkit_editor_set_can_redo (EWebKitEditor *wk_editor,
                            gboolean can_redo)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if ((wk_editor->priv->can_redo ? 1 : 0) == (can_redo ? 1 : 0))
		return;

	wk_editor->priv->can_redo = can_redo;

	g_object_notify (G_OBJECT (wk_editor), "can-redo");
}

static void
content_changed_cb (WebKitUserContentManager *manager,
		    WebKitJavascriptResult *js_result,
		    gpointer user_data)
{
	EWebKitEditor *wk_editor = user_data;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	webkit_editor_set_changed (wk_editor, TRUE);
}

static void
context_menu_requested_cb (WebKitUserContentManager *manager,
			   WebKitJavascriptResult *js_result,
			   gpointer user_data)
{
	EWebKitEditor *wk_editor = user_data;
	JSCValue *jsc_params;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));
	g_return_if_fail (js_result != NULL);

	jsc_params = webkit_javascript_result_get_js_value (js_result);
	g_return_if_fail (jsc_value_is_object (jsc_params));

	g_clear_pointer (&wk_editor->priv->context_menu_caret_word, g_free);
	g_clear_pointer (&wk_editor->priv->last_hover_uri, g_free);

	wk_editor->priv->context_menu_node_flags = e_web_view_jsc_get_object_property_int32 (jsc_params, "nodeFlags", 0);
	wk_editor->priv->context_menu_caret_word = e_web_view_jsc_get_object_property_string (jsc_params, "caretWord", NULL);
	wk_editor->priv->last_hover_uri = e_web_view_jsc_get_object_property_string (jsc_params, "anchorHref", NULL);
}

static gboolean
webkit_editor_update_color_value (JSCValue *jsc_params,
				  const gchar *param_name,
				  GdkRGBA **out_rgba)
{
	JSCValue *jsc_value;
	GdkRGBA color;
	gboolean res = FALSE;

	g_return_val_if_fail (jsc_params != NULL, FALSE);
	g_return_val_if_fail (out_rgba != NULL, FALSE);

	jsc_value = jsc_value_object_get_property (jsc_params, param_name);
	if (jsc_value && jsc_value_is_string (jsc_value)) {
		gchar *value;

		value = jsc_value_to_string (jsc_value);

		if (value && *value && gdk_rgba_parse (&color, value)) {
			if (!(*out_rgba) || !gdk_rgba_equal (&color, *out_rgba)) {
				if (*out_rgba)
					gdk_rgba_free (*out_rgba);
				*out_rgba = gdk_rgba_copy (&color);

				res = TRUE;
			}
		} else {
			if (*out_rgba) {
				gdk_rgba_free (*out_rgba);
				res = TRUE;
			}

			*out_rgba = NULL;
		}

		g_free (value);
	}

	g_clear_object (&jsc_value);

	return res;
}

static void webkit_editor_update_styles (EContentEditor *editor);
static void webkit_editor_style_updated (EWebKitEditor *wk_editor, gboolean force);

static void
formatting_changed_cb (WebKitUserContentManager *manager,
		       WebKitJavascriptResult *js_result,
		       gpointer user_data)
{
	EWebKitEditor *wk_editor = user_data;
	JSCValue *jsc_params, *jsc_value;
	GObject *object;
	gboolean changed, forced = FALSE;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	jsc_params = webkit_javascript_result_get_js_value (js_result);
	g_return_if_fail (jsc_value_is_object (jsc_params));

	object = G_OBJECT (wk_editor);

	g_object_freeze_notify (object);

	jsc_value = jsc_value_object_get_property (jsc_params, "forced");
	if (jsc_value && jsc_value_is_boolean (jsc_value)) {
		forced = jsc_value_to_boolean (jsc_value);
	}
	g_clear_object (&jsc_value);

	changed = FALSE;
	jsc_value = jsc_value_object_get_property (jsc_params, "mode");
	if (jsc_value && jsc_value_is_number (jsc_value)) {
		gint value = jsc_value_to_int32 (jsc_value);

		if ((value ? 1 : 0) != (wk_editor->priv->mode == E_CONTENT_EDITOR_MODE_HTML ? 1 : 0)) {
			wk_editor->priv->mode = value ? E_CONTENT_EDITOR_MODE_HTML : E_CONTENT_EDITOR_MODE_PLAIN_TEXT;
			changed = TRUE;
		}
	}
	g_clear_object (&jsc_value);

	if (changed) {
		/* Update fonts - in plain text we only want monospaced */
		webkit_editor_update_styles (E_CONTENT_EDITOR (wk_editor));
		webkit_editor_style_updated (wk_editor, FALSE);

		g_object_notify (object, "mode");
	}

	changed = FALSE;
	jsc_value = jsc_value_object_get_property (jsc_params, "alignment");
	if (jsc_value && jsc_value_is_number (jsc_value)) {
		gint value = jsc_value_to_int32 (jsc_value);

		if (value != wk_editor->priv->alignment) {
			wk_editor->priv->alignment = value;
			changed = TRUE;
		}
	}
	g_clear_object (&jsc_value);

	if (changed || forced)
		g_object_notify (object, "alignment");

	changed = FALSE;
	jsc_value = jsc_value_object_get_property (jsc_params, "blockFormat");
	if (jsc_value && jsc_value_is_number (jsc_value)) {
		gint value = jsc_value_to_int32 (jsc_value);

		if (value != wk_editor->priv->block_format) {
			wk_editor->priv->block_format = value;
			changed = TRUE;
		}
	}
	g_clear_object (&jsc_value);

	if (changed || forced)
		g_object_notify (object, "block-format");

	changed = FALSE;
	jsc_value = jsc_value_object_get_property (jsc_params, "indentLevel");
	if (jsc_value && jsc_value_is_number (jsc_value)) {
		gint value = jsc_value_to_int32 (jsc_value);

		if (value != wk_editor->priv->indent_level) {
			wk_editor->priv->indent_level = value;
			changed = TRUE;
		}
	}
	g_clear_object (&jsc_value);

	if (changed || forced)
		g_object_notify (object, "indent-level");

	#define update_style_flag(_flag, _set) \
		changed = (wk_editor->priv->style_flags & (_flag)) != ((_set) ? (_flag) : 0); \
		wk_editor->priv->style_flags = (wk_editor->priv->style_flags & ~(_flag)) | ((_set) ? (_flag) : 0);

	changed = FALSE;
	jsc_value = jsc_value_object_get_property (jsc_params, "bold");
	if (jsc_value && jsc_value_is_boolean (jsc_value)) {
		gboolean value = jsc_value_to_boolean (jsc_value);

		update_style_flag (E_WEBKIT_EDITOR_STYLE_IS_BOLD, value);
	}
	g_clear_object (&jsc_value);

	if (changed || forced)
		g_object_notify (G_OBJECT (wk_editor), "bold");

	changed = FALSE;
	jsc_value = jsc_value_object_get_property (jsc_params, "italic");
	if (jsc_value && jsc_value_is_boolean (jsc_value)) {
		gboolean value = jsc_value_to_boolean (jsc_value);

		update_style_flag (E_WEBKIT_EDITOR_STYLE_IS_ITALIC, value);
	}
	g_clear_object (&jsc_value);

	if (changed || forced)
		g_object_notify (G_OBJECT (wk_editor), "italic");

	changed = FALSE;
	jsc_value = jsc_value_object_get_property (jsc_params, "underline");
	if (jsc_value && jsc_value_is_boolean (jsc_value)) {
		gboolean value = jsc_value_to_boolean (jsc_value);

		update_style_flag (E_WEBKIT_EDITOR_STYLE_IS_UNDERLINE, value);
	}
	g_clear_object (&jsc_value);

	if (changed || forced)
		g_object_notify (G_OBJECT (wk_editor), "underline");

	changed = FALSE;
	jsc_value = jsc_value_object_get_property (jsc_params, "strikethrough");
	if (jsc_value && jsc_value_is_boolean (jsc_value)) {
		gboolean value = jsc_value_to_boolean (jsc_value);

		update_style_flag (E_WEBKIT_EDITOR_STYLE_IS_STRIKETHROUGH, value);
	}
	g_clear_object (&jsc_value);

	if (changed || forced)
		g_object_notify (G_OBJECT (wk_editor), "strikethrough");

	jsc_value = jsc_value_object_get_property (jsc_params, "script");
	if (jsc_value && jsc_value_is_number (jsc_value)) {
		gint value = jsc_value_to_int32 (jsc_value);

		update_style_flag (E_WEBKIT_EDITOR_STYLE_IS_SUBSCRIPT, value < 0);

		if (changed || forced)
			g_object_notify (object, "subscript");

		update_style_flag (E_WEBKIT_EDITOR_STYLE_IS_SUPERSCRIPT, value > 0);

		if (changed || forced)
			g_object_notify (object, "superscript");
	} else if (forced) {
		g_object_notify (object, "subscript");
		g_object_notify (object, "superscript");
	}
	g_clear_object (&jsc_value);

	wk_editor->priv->temporary_style_flags = wk_editor->priv->style_flags;

	#undef update_style_flag

	changed = FALSE;
	jsc_value = jsc_value_object_get_property (jsc_params, "fontSize");
	if (jsc_value && jsc_value_is_number (jsc_value)) {
		gint value = jsc_value_to_int32 (jsc_value);

		if (value != wk_editor->priv->font_size) {
			wk_editor->priv->font_size = value;
			changed = TRUE;
		}
	}
	g_clear_object (&jsc_value);

	if (changed || forced)
		g_object_notify (object, "font-size");

	changed = FALSE;
	jsc_value = jsc_value_object_get_property (jsc_params, "fontFamily");
	if (jsc_value && jsc_value_is_string (jsc_value)) {
		gchar *value = jsc_value_to_string (jsc_value);

		if (g_strcmp0 (value, wk_editor->priv->font_name) != 0) {
			g_free (wk_editor->priv->font_name);
			wk_editor->priv->font_name = value;
			changed = TRUE;
		} else {
			g_free (value);
		}
	}
	g_clear_object (&jsc_value);

	if (changed || forced)
		g_object_notify (object, "font-name");

	jsc_value = jsc_value_object_get_property (jsc_params, "bodyFontFamily");
	if (jsc_value && jsc_value_is_string (jsc_value)) {
		gchar *value = jsc_value_to_string (jsc_value);

		if (g_strcmp0 (value, wk_editor->priv->body_font_name) != 0) {
			g_free (wk_editor->priv->body_font_name);
			wk_editor->priv->body_font_name = value;
		} else {
			g_free (value);
		}
	}
	g_clear_object (&jsc_value);

	if (webkit_editor_update_color_value (jsc_params, "fgColor", &wk_editor->priv->font_color) || forced)
		g_object_notify (object, "font-color");

	if (webkit_editor_update_color_value (jsc_params, "bgColor", &wk_editor->priv->background_color) || forced)
		g_object_notify (object, "background-color");

	webkit_editor_update_color_value (jsc_params, "bodyFgColor", &wk_editor->priv->body_fg_color);
	webkit_editor_update_color_value (jsc_params, "bodyBgColor", &wk_editor->priv->body_bg_color);
	webkit_editor_update_color_value (jsc_params, "bodyLinkColor", &wk_editor->priv->body_link_color);
	webkit_editor_update_color_value (jsc_params, "bodyVlinkColor", &wk_editor->priv->body_vlink_color);

	g_object_thaw_notify (object);
}

static void
selection_changed_cb (WebKitUserContentManager *manager,
		      WebKitJavascriptResult *js_result,
		      gpointer user_data)
{
	EWebKitEditor *wk_editor = user_data;
	WebKitEditorState *editor_state;
	JSCValue *jsc_value;
	gboolean is_collapsed;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	jsc_value = webkit_javascript_result_get_js_value (js_result);
	g_return_if_fail (jsc_value_is_object (jsc_value));
	is_collapsed = e_web_view_jsc_get_object_property_boolean (jsc_value, "isCollapsed", FALSE);
	wk_editor->priv->caret_client_rect.x = e_web_view_jsc_get_object_property_int32 (jsc_value, "x", 0);
	wk_editor->priv->caret_client_rect.y = e_web_view_jsc_get_object_property_int32 (jsc_value, "y", 0);
	wk_editor->priv->caret_client_rect.width = e_web_view_jsc_get_object_property_int32 (jsc_value, "width", -1);
	wk_editor->priv->caret_client_rect.height = e_web_view_jsc_get_object_property_int32 (jsc_value, "height", -1);

	editor_state = webkit_web_view_get_editor_state (WEBKIT_WEB_VIEW (wk_editor));

	if (editor_state) {
		GObject *object = G_OBJECT (wk_editor);
		gboolean value;

		#define check_and_set_prop(_prop_var, _prop_name, _val) \
			value = _val; \
			if (_prop_var != value) { \
				_prop_var = value; \
				g_object_notify (object, _prop_name); \
			}

		g_object_freeze_notify (object);

		check_and_set_prop (wk_editor->priv->can_copy, "can-copy", !is_collapsed);
		check_and_set_prop (wk_editor->priv->can_cut, "can-cut", !is_collapsed);
		check_and_set_prop (wk_editor->priv->can_paste, "can-paste", webkit_editor_state_is_paste_available (editor_state));

		g_object_thaw_notify (object);

		#undef set_prop
	}
}

static void
undu_redo_state_changed_cb (WebKitUserContentManager *manager,
			    WebKitJavascriptResult *js_result,
			    gpointer user_data)
{
	EWebKitEditor *wk_editor = user_data;
	JSCValue *jsc_value;
	JSCValue *jsc_params;
	gint32 state;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));
	g_return_if_fail (js_result != NULL);

	jsc_params = webkit_javascript_result_get_js_value (js_result);
	g_return_if_fail (jsc_value_is_object (jsc_params));

	jsc_value = jsc_value_object_get_property (jsc_params, "state");
	g_return_if_fail (jsc_value_is_number (jsc_value));
	state = jsc_value_to_int32 (jsc_value);
	g_clear_object (&jsc_value);

	webkit_editor_set_can_undo (wk_editor, (state & E_UNDO_REDO_STATE_CAN_UNDO) != 0);
	webkit_editor_set_can_redo (wk_editor, (state & E_UNDO_REDO_STATE_CAN_REDO) != 0);
}

static void
webkit_editor_queue_post_reload_operation (EWebKitEditor *wk_editor,
                                           PostReloadOperationFunc func,
                                           gpointer data,
                                           GDestroyNotify data_free_func,
                                           EContentEditorInsertContentFlags flags)
{
	PostReloadOperation *op;

	g_return_if_fail (func != NULL);

	if (wk_editor->priv->post_reload_operations == NULL)
		wk_editor->priv->post_reload_operations = g_queue_new ();

	op = g_new0 (PostReloadOperation, 1);
	op->func = func;
	op->flags = flags;
	op->data = data;
	op->data_free_func = data_free_func;

	g_queue_push_head (wk_editor->priv->post_reload_operations, op);
}

static void
webkit_editor_show_inspector (EWebKitEditor *wk_editor)
{
	WebKitWebInspector *inspector;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	inspector = webkit_web_view_get_inspector (WEBKIT_WEB_VIEW (wk_editor));

	webkit_web_inspector_show (inspector);
}

static gboolean
webkit_editor_supports_mode (EContentEditor *content_editor,
			     EContentEditorMode mode)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (content_editor), FALSE);

	return mode == E_CONTENT_EDITOR_MODE_PLAIN_TEXT ||
		mode == E_CONTENT_EDITOR_MODE_HTML;
}

static void
webkit_editor_initialize (EContentEditor *content_editor,
                          EContentEditorInitializedCallback callback,
                          gpointer user_data)
{
	EWebKitEditor *wk_editor;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (content_editor));
	g_return_if_fail (callback != NULL);

	wk_editor = E_WEBKIT_EDITOR (content_editor);

	if (wk_editor->priv->webkit_load_event == WEBKIT_LOAD_FINISHED) {
		callback (content_editor, user_data);
	} else {
		g_return_if_fail (wk_editor->priv->initialized_callback == NULL);

		wk_editor->priv->initialized_callback = callback;
		wk_editor->priv->initialized_user_data = user_data;
	}
}

static void
webkit_editor_update_styles (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	gboolean mark_citations, use_custom_font;
	gchar *font, *citation_color;
	const gchar *styles[] = { "normal", "oblique", "italic" };
	gchar fsbuff[G_ASCII_DTOSTR_BUF_SIZE];
	GString *stylesheet;
	PangoFontDescription *ms, *vw;
	WebKitSettings *settings;
	WebKitUserContentManager *manager;
	WebKitUserStyleSheet *style_sheet;

	wk_editor = E_WEBKIT_EDITOR (editor);

	use_custom_font = g_settings_get_boolean (
		wk_editor->priv->mail_settings, "use-custom-font");

	if (use_custom_font) {
		font = g_settings_get_string (
			wk_editor->priv->mail_settings, "monospace-font");
		ms = pango_font_description_from_string (font && *font ? font : "monospace 10");
		g_free (font);
	} else {
		font = g_settings_get_string (
			wk_editor->priv->font_settings, "monospace-font-name");
		ms = pango_font_description_from_string (font && *font ? font : "monospace 10");
		g_free (font);
	}

	if (!pango_font_description_get_family (ms) ||
	    !pango_font_description_get_size (ms)) {
		pango_font_description_free (ms);
		ms = pango_font_description_from_string ("monospace 10");
	}

	if (wk_editor->priv->mode == E_CONTENT_EDITOR_MODE_HTML) {
		if (use_custom_font) {
			font = g_settings_get_string (
				wk_editor->priv->mail_settings, "variable-width-font");
			vw = pango_font_description_from_string (font && *font ? font : "serif 10");
			g_free (font);
		} else {
			font = g_settings_get_string (
				wk_editor->priv->font_settings, "font-name");
			vw = pango_font_description_from_string (font && *font ? font : "serif 10");
			g_free (font);
		}
	} else {
		/* When in plain text mode, force monospace font */
		vw = pango_font_description_copy (ms);
	}

	if (!pango_font_description_get_family (vw) ||
	    !pango_font_description_get_size (vw)) {
		pango_font_description_free (vw);
		vw = pango_font_description_from_string ("serif 10");
	}

	stylesheet = g_string_new ("");
	g_ascii_dtostr (fsbuff, G_ASCII_DTOSTR_BUF_SIZE,
		((gdouble) pango_font_description_get_size (vw)) / PANGO_SCALE);

	g_string_append_printf (
		stylesheet,
		"body {\n"
		"  font-family: '%s';\n"
		"  font-size: %spt;\n"
		"  font-weight: %d;\n"
		"  font-style: %s;\n"
		" -webkit-line-break: after-white-space;\n"
		"}\n",
		pango_font_description_get_family (vw),
		fsbuff,
		pango_font_description_get_weight (vw),
		styles[pango_font_description_get_style (vw)]);

	g_ascii_dtostr (fsbuff, G_ASCII_DTOSTR_BUF_SIZE,
		((gdouble) pango_font_description_get_size (ms)) / PANGO_SCALE);

	g_string_append_printf (
		stylesheet,
		"body, div, p, td {\n"
		"  unicode-bidi: plaintext;\n"
		"}\n"
		"pre,code,.pre {\n"
		"  font-family: '%s';\n"
		"  font-size: %spt;\n"
		"  font-weight: %d;\n"
		"  font-style: %s;\n"
		"}",
		pango_font_description_get_family (ms),
		fsbuff,
		pango_font_description_get_weight (ms),
		styles[pango_font_description_get_style (ms)]);

	/* See bug #689777 for details */
	g_string_append (stylesheet, DEFAULT_CSS_STYLE);

	/* When inserting a table into contenteditable element the width of the
	 * cells is nearly zero and the td { min-height } doesn't work so put
	 * unicode zero width space before each cell. */
	g_string_append (
		stylesheet,
		"td:before {\n"
		" content: \"\xe2\x80\x8b\";"
		"}\n");

	g_string_append (
		stylesheet,
		"img "
		"{\n"
		"  height: inherit; \n"
		"  width: inherit; \n"
		"}\n");

	g_string_append (
		stylesheet,
		"span.-x-evo-resizable-wrapper:hover "
		"{\n"
		"  outline: 1px dashed red; \n"
		"  resize: both; \n"
		"  overflow: hidden; \n"
		"  display: inline-block; \n"
		"}\n");

	g_string_append (
		stylesheet,
		"td:hover "
		"{\n"
		"  outline: 1px dotted red;\n"
		"}\n");

	g_string_append_printf (
		stylesheet,
		".-x-evo-plaintext-table "
		"{\n"
		"  border-collapse: collapse;\n"
		"  width: %dch;\n"
		"}\n",
		wk_editor->priv->normal_paragraph_width);

	g_string_append (
		stylesheet,
		".-x-evo-plaintext-table td "
		"{\n"
		"  vertical-align: top;\n"
		"}\n");

	if (wk_editor->priv->mode == E_CONTENT_EDITOR_MODE_HTML) {
		g_string_append (
			stylesheet,
			"body ul > li.-x-evo-align-center,ol > li.-x-evo-align-center "
			"{\n"
			"  list-style-position: inside;\n"
			"}\n");

		g_string_append (
			stylesheet,
			"body ul > li.-x-evo-align-right, ol > li.-x-evo-align-right "
			"{\n"
			"  list-style-position: inside;\n"
			"}\n");

		g_string_append (
			stylesheet,
			"body "
			"blockquote[type=cite] "
			"{\n"
			"  padding: 0ch 1ch 0ch 1ch;\n"
			"  margin: 0ch;\n"
			"  border-width: 0px 2px 0px 2px;\n"
			"  border-style: none solid none solid;\n"
			"  border-radius: 2px;\n"
			"}\n");

		g_string_append_printf (
			stylesheet,
			"body "
			"blockquote[type=cite] "
			"{\n"
			"  border-color: %s;\n"
			"}\n",
			e_web_view_get_citation_color_for_level (1));

		g_string_append_printf (
			stylesheet,
			"body "
			"blockquote[type=cite] "
			"blockquote[type=cite] "
			"{\n"
			"  border-color: %s;\n"
			"}\n",
			e_web_view_get_citation_color_for_level (2));

		g_string_append_printf (
			stylesheet,
			"body "
			"blockquote[type=cite] "
			"blockquote[type=cite] "
			"blockquote[type=cite] "
			"{\n"
			"  border-color: %s;\n"
			"}\n",
			e_web_view_get_citation_color_for_level (3));

		g_string_append_printf (
			stylesheet,
			"body "
			"blockquote[type=cite] "
			"blockquote[type=cite] "
			"blockquote[type=cite] "
			"blockquote[type=cite] "
			"{\n"
			"  border-color: %s;\n"
			"}\n",
			e_web_view_get_citation_color_for_level (4));

		g_string_append_printf (
			stylesheet,
			"body "
			"blockquote[type=cite] "
			"blockquote[type=cite] "
			"blockquote[type=cite] "
			"blockquote[type=cite] "
			"blockquote[type=cite] "
			"{\n"
			"  border-color: %s;\n"
			"}\n",
			e_web_view_get_citation_color_for_level (5));

		g_string_append_printf (
			stylesheet,
			"body "
			"blockquote[type=cite] "
			"blockquote[type=cite] "
			"blockquote[type=cite] "
			"blockquote[type=cite] "
			"blockquote[type=cite] "
			"blockquote[type=cite] "
			"{\n"
			"  border-color: %s;\n"
			"  padding: 0ch 0ch 0ch 1ch;\n"
			"  margin: 0ch;\n"
			"  border-width: 0px 0px 0px 2px;\n"
			"  border-style: none none none solid;\n"
			"}\n",
			e_web_view_get_citation_color_for_level (1));
	} else {
		g_string_append (
			stylesheet,
			"body "
			"{\n"
			"  font-family: Monospace; \n"
			"}\n");

		g_string_append_printf (
			stylesheet,
			"body ul "
			"{\n"
			"  list-style: outside none;\n"
			"  -webkit-padding-start: %dch; \n"
			"}\n", SPACES_PER_LIST_LEVEL);

		g_string_append_printf (
			stylesheet,
			"body ul > li "
			"{\n"
			"  list-style-position: outside;\n"
			"  text-indent: -%dch;\n"
			"}\n", SPACES_PER_LIST_LEVEL - 1);

		g_string_append (
			stylesheet,
			"body ul > li::before "
			"{\n"
			"  content: \"*" UNICODE_NBSP "\";\n"
			"}\n");

		g_string_append (
			stylesheet,
			"body ul ul > li::before, "
			"body ol ul > li::before "
			"{\n"
			"  content: \"-" UNICODE_NBSP "\";\n"
			"}\n");

		g_string_append (
			stylesheet,
			"body ul ul ul > li::before, "
			"body ol ul ul > li::before, "
			"body ul ol ul > li::before, "
			"body ol ol ul > li::before "
			"{\n"
			"  content: \"+" UNICODE_NBSP "\";\n"
			"}\n");

		g_string_append (
			stylesheet,
			"body ul ul ul ul > li::before, "
			"body ol ul ul ul > li::before, "
			"body ul ol ul ul > li::before, "
			"body ul ul ol ul > li::before, "
			"body ol ol ul ul > li::before, "
			"body ol ul ol ul > li::before, "
			"body ul ol ol ul > li::before, "
			"body ol ol ol ul > li::before "
			"{\n"
			"  content: \"*" UNICODE_NBSP "\";\n"
			"}\n");

		g_string_append (
			stylesheet,
			"body div "
			"{\n"
			"  word-wrap: break-word; \n"
			"  word-break: break-word; \n"
			"  white-space: pre-wrap; \n"
			"}\n");

		g_string_append (
			stylesheet,
			".-x-evo-quoted { -webkit-user-select: none; }\n");

		g_string_append_printf (
			stylesheet,
			".-x-evo-quote-character "
			"{\n"
			"  color: %s;\n"
			"}\n",
			e_web_view_get_citation_color_for_level (1));

		g_string_append_printf (
			stylesheet,
			".-x-evo-quote-character+"
			".-x-evo-quote-character"
			"{\n"
			"  color: %s;\n"
			"}\n",
			e_web_view_get_citation_color_for_level (2));

		g_string_append_printf (
			stylesheet,
			".-x-evo-quote-character+"
			".-x-evo-quote-character+"
			".-x-evo-quote-character"
			"{\n"
			"  color: %s;\n"
			"}\n",
			e_web_view_get_citation_color_for_level (3));

		g_string_append_printf (
			stylesheet,
			".-x-evo-quote-character+"
			".-x-evo-quote-character+"
			".-x-evo-quote-character+"
			".-x-evo-quote-character"
			"{\n"
			"  color: %s;\n"
			"}\n",
			e_web_view_get_citation_color_for_level (4));

		g_string_append_printf (
			stylesheet,
			".-x-evo-quote-character+"
			".-x-evo-quote-character+"
			".-x-evo-quote-character+"
			".-x-evo-quote-character+"
			".-x-evo-quote-character"
			"{\n"
			"  color: %s;\n"
			"}\n",
			e_web_view_get_citation_color_for_level (5));
	}

	g_string_append_printf (
		stylesheet,
		"ol "
		"{\n"
		"  -webkit-padding-start: %dch; \n"
		"}\n", SPACES_ORDERED_LIST_FIRST_LEVEL);

	if (wk_editor->priv->mode == E_CONTENT_EDITOR_MODE_HTML) {
		g_string_append (
			stylesheet,
			"a "
			"{\n"
			"  word-wrap: break-word; \n"
			"  word-break: break-all; \n"
			"}\n");
	} else {
		g_string_append (
			stylesheet,
			"a "
			"{\n"
			"  word-wrap: normal; \n"
			"  word-break: keep-all; \n"
			"}\n");
	}

	citation_color = g_settings_get_string (
		wk_editor->priv->mail_settings, "citation-color");
	mark_citations = g_settings_get_boolean (
		wk_editor->priv->mail_settings, "mark-citations");

	g_string_append (
		stylesheet,
		"blockquote[type=cite] "
		"{\n"
		"  padding: 0.0ex 0ex;\n"
		"  margin: 0ex;\n"
		"  -webkit-margin-start: 0em; \n"
		"  -webkit-margin-end : 0em; \n");

	if (mark_citations && citation_color)
		g_string_append_printf (
			stylesheet,
			"  color: %s !important; \n",
			citation_color);

	g_free (citation_color);
	citation_color = NULL;

	g_string_append (stylesheet, "}\n");

	if (wk_editor->priv->visually_wrap_long_lines) {
		g_string_append (
			stylesheet,
			"pre {\n"
			"  white-space: pre-wrap;\n"
			"}\n");
	}

	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (wk_editor));
	g_object_set (
		G_OBJECT (settings),
		"default-font-size",
		webkit_settings_font_size_to_pixels (
			pango_font_description_get_size (vw) / PANGO_SCALE),
		"default-font-family",
		pango_font_description_get_family (vw),
		"monospace-font-family",
		pango_font_description_get_family (ms),
		"default-monospace-font-size",
		webkit_settings_font_size_to_pixels (
			pango_font_description_get_size (ms) / PANGO_SCALE),
		NULL);

	manager = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (wk_editor));
	webkit_user_content_manager_remove_all_style_sheets (manager);

	style_sheet = webkit_user_style_sheet_new (
		stylesheet->str,
		WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
		WEBKIT_USER_STYLE_LEVEL_USER,
		NULL,
		NULL);

	webkit_user_content_manager_add_style_sheet (manager, style_sheet);

	g_free (wk_editor->priv->current_user_stylesheet);
	wk_editor->priv->current_user_stylesheet = g_string_free (stylesheet, FALSE);

	webkit_user_style_sheet_unref (style_sheet);

	pango_font_description_free (ms);
	pango_font_description_free (vw);
}

static void
webkit_editor_add_color_style (GString *css,
			       const gchar *selector,
			       const gchar *property,
			       const GdkRGBA *value)
{
	g_return_if_fail (css != NULL);
	g_return_if_fail (selector != NULL);
	g_return_if_fail (property != NULL);

	if (!value || value->alpha <= 1e-9)
		return;

	g_string_append_printf (css, "%s { %s : #%06x; }\n", selector, property, e_rgba_to_value (value));
}

static void
webkit_editor_set_page_color_attribute (EContentEditor *editor,
					GString *script, /* serves two purposes, also says whether write to body or not */
					const gchar *attr_name,
					const GdkRGBA *value)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	if (value && value->alpha > 1e-9) {
		gchar color[64];

		webkit_editor_utils_color_to_string (color, sizeof (color), value);

		if (script) {
			e_web_view_jsc_printf_script_gstring (script,
				"document.documentElement.setAttribute(%s, %s);\n",
				attr_name,
				color);
		} else {
			e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
				"EvoEditor.SetBodyAttribute(%s, %s);",
				attr_name,
				color);
		}
	} else if (script) {
		e_web_view_jsc_printf_script_gstring (script,
			"document.documentElement.removeAttribute(%s);\n",
			attr_name);
	} else {
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.SetBodyAttribute(%s, null);",
			attr_name);
	}
}

static void
webkit_editor_page_set_text_color (EContentEditor *editor,
                                   const GdkRGBA *value)
{
	webkit_editor_set_page_color_attribute (editor, NULL, "text", value);
}

static void
webkit_editor_page_get_text_color (EContentEditor *editor,
                                   GdkRGBA *color)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	if (wk_editor->priv->mode == E_CONTENT_EDITOR_MODE_HTML &&
	    wk_editor->priv->body_fg_color) {
		*color = *wk_editor->priv->body_fg_color;
	} else {
		e_utils_get_theme_color (GTK_WIDGET (wk_editor), "theme_text_color", E_UTILS_DEFAULT_THEME_TEXT_COLOR, color);
	}
}

static void
webkit_editor_page_set_background_color (EContentEditor *editor,
                                         const GdkRGBA *value)
{
	webkit_editor_set_page_color_attribute (editor, NULL, "bgcolor", value);
}

static void
webkit_editor_page_get_background_color (EContentEditor *editor,
                                         GdkRGBA *color)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	if (wk_editor->priv->mode == E_CONTENT_EDITOR_MODE_HTML &&
	    wk_editor->priv->body_bg_color) {
		*color = *wk_editor->priv->body_bg_color;
	} else {
		e_utils_get_theme_color (GTK_WIDGET (wk_editor), "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, color);
	}
}

static void
webkit_editor_page_set_link_color (EContentEditor *editor,
                                   const GdkRGBA *value)
{
	webkit_editor_set_page_color_attribute (editor, NULL, "link", value);
}

static void
webkit_editor_page_get_link_color (EContentEditor *editor,
                                   GdkRGBA *color)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	if (wk_editor->priv->mode == E_CONTENT_EDITOR_MODE_HTML &&
	    wk_editor->priv->body_link_color) {
		*color = *wk_editor->priv->body_link_color;
	} else {
		color->alpha = 1;
		color->red = 0;
		color->green = 0;
		color->blue = 1;
	}
}

static void
webkit_editor_page_set_visited_link_color (EContentEditor *editor,
                                           const GdkRGBA *value)
{
	webkit_editor_set_page_color_attribute (editor, NULL, "vlink", value);
}

static void
webkit_editor_page_get_visited_link_color (EContentEditor *editor,
                                           GdkRGBA *color)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	if (wk_editor->priv->mode == E_CONTENT_EDITOR_MODE_HTML &&
	    wk_editor->priv->body_vlink_color) {
		*color = *wk_editor->priv->body_vlink_color;
	} else {
		color->alpha = 1;
		color->red = 1;
		color->green = 0;
		color->blue = 0;
	}
}

static void
webkit_editor_page_set_font_name (EContentEditor *editor,
				  const gchar *value)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.SetBodyFontName(%s);",
		value ? value : "");
}

static const gchar *
webkit_editor_page_get_font_name (EContentEditor *editor)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	if (wk_editor->priv->mode != E_CONTENT_EDITOR_MODE_HTML)
		return NULL;

	return wk_editor->priv->body_font_name;
}

static void
webkit_editor_style_updated (EWebKitEditor *wk_editor,
			     gboolean force)
{
	EContentEditor *cnt_editor;
	GdkRGBA bgcolor, fgcolor, link_color, vlink_color;
	GtkStateFlags state_flags;
	GtkStyleContext *style_context;
	GString *css, *script;
	gboolean backdrop;
	gboolean inherit_theme_colors;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	cnt_editor = E_CONTENT_EDITOR (wk_editor);

	inherit_theme_colors = g_settings_get_boolean (wk_editor->priv->mail_settings, "composer-inherit-theme-colors");
	state_flags = gtk_widget_get_state_flags (GTK_WIDGET (wk_editor));
	style_context = gtk_widget_get_style_context (GTK_WIDGET (wk_editor));
	backdrop = (state_flags & GTK_STATE_FLAG_BACKDROP) != 0;

	if (wk_editor->priv->mode == E_CONTENT_EDITOR_MODE_HTML && !inherit_theme_colors) {
		/* Default to white background when not inheriting theme colors */
		bgcolor.red = 1.0;
		bgcolor.green = 1.0;
		bgcolor.blue = 1.0;
		bgcolor.alpha = 1.0;
	} else if (!gtk_style_context_lookup_color (
			style_context,
			backdrop ? "theme_unfocused_base_color" : "theme_base_color",
			&bgcolor)) {
		gdk_rgba_parse (&bgcolor, E_UTILS_DEFAULT_THEME_BASE_COLOR);
	}

	if (wk_editor->priv->mode == E_CONTENT_EDITOR_MODE_HTML && !inherit_theme_colors) {
		/* Default to black text color when not inheriting theme colors */
		fgcolor.red = 0.0;
		fgcolor.green = 0.0;
		fgcolor.blue = 0.0;
		fgcolor.alpha = 1.0;
	} else if (!gtk_style_context_lookup_color (
			style_context,
			backdrop ? "theme_unfocused_fg_color" : "theme_fg_color",
			&fgcolor)) {
		gdk_rgba_parse (&fgcolor, E_UTILS_DEFAULT_THEME_FG_COLOR);
	}

	gtk_style_context_get_color (style_context, state_flags | GTK_STATE_FLAG_LINK, &link_color);
	gtk_style_context_get_color (style_context, state_flags | GTK_STATE_FLAG_VISITED, &vlink_color);

	if (!force &&
	    gdk_rgba_equal (&bgcolor, &wk_editor->priv->theme_bgcolor) &&
	    gdk_rgba_equal (&fgcolor, &wk_editor->priv->theme_fgcolor) &&
	    gdk_rgba_equal (&link_color, &wk_editor->priv->theme_link_color) &&
	    gdk_rgba_equal (&vlink_color, &wk_editor->priv->theme_vlink_color))
		return;

	wk_editor->priv->theme_bgcolor = bgcolor;
	wk_editor->priv->theme_fgcolor = fgcolor;
	wk_editor->priv->theme_link_color = link_color;
	wk_editor->priv->theme_vlink_color = vlink_color;

	css = g_string_sized_new (160);
	script = g_string_sized_new (256);

	webkit_editor_set_page_color_attribute (cnt_editor, script, "x-evo-bgcolor", &bgcolor);
	webkit_editor_set_page_color_attribute (cnt_editor, script, "x-evo-text", &fgcolor);
	webkit_editor_set_page_color_attribute (cnt_editor, script, "x-evo-link", &link_color);
	webkit_editor_set_page_color_attribute (cnt_editor, script, "x-evo-vlink", &vlink_color);

	webkit_editor_add_color_style (css, "html", "background-color", &bgcolor);
	webkit_editor_add_color_style (css, "html", "color", &fgcolor);
	webkit_editor_add_color_style (css, "a", "color", &link_color);
	webkit_editor_add_color_style (css, "a:visited", "color", &vlink_color);

	e_web_view_jsc_printf_script_gstring (script,
		"EvoEditor.UpdateThemeStyleSheet(%s);",
		css->str);

	e_web_view_jsc_run_script_take (WEBKIT_WEB_VIEW (wk_editor),
		g_string_free (script, FALSE),
		wk_editor->priv->cancellable);

	g_string_free (css, TRUE);
}

static void
webkit_editor_style_updated_cb (EWebKitEditor *wk_editor)
{
	webkit_editor_style_updated (wk_editor, FALSE);
}

static EContentEditorMode
webkit_editor_get_mode (EWebKitEditor *wk_editor)
{
	return wk_editor->priv->mode;
}

static gboolean
show_lose_formatting_dialog (EWebKitEditor *wk_editor)
{
	gboolean lose;
	GtkWidget *toplevel;
	GtkWindow *parent = NULL;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (wk_editor));

	if (GTK_IS_WINDOW (toplevel))
		parent = GTK_WINDOW (toplevel);

	lose = e_util_prompt_user (
		parent, "org.gnome.evolution.mail", "prompt-on-composer-mode-switch",
		"mail-composer:prompt-composer-mode-switch", NULL);

	if (!lose) {
		/* Nothing has changed, but notify anyway */
		g_object_notify (G_OBJECT (wk_editor), "mode");
		return FALSE;
	}

	return TRUE;
}

static void
webkit_editor_set_mode (EWebKitEditor *wk_editor,
			EContentEditorMode mode)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));
	g_return_if_fail (mode == E_CONTENT_EDITOR_MODE_PLAIN_TEXT || mode == E_CONTENT_EDITOR_MODE_HTML);

	if (mode == wk_editor->priv->mode)
		return;

	wk_editor->priv->mode = mode;

	if (mode == E_CONTENT_EDITOR_MODE_HTML) {
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.SetMode(EvoEditor.MODE_HTML);");
	} else {
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.SetMode(EvoEditor.MODE_PLAIN_TEXT);");
	}

	webkit_editor_update_styles (E_CONTENT_EDITOR (wk_editor));
	webkit_editor_style_updated (wk_editor, FALSE);

	g_object_notify (G_OBJECT (wk_editor), "mode");
}

static void
webkit_editor_insert_content (EContentEditor *editor,
                              const gchar *content,
                              EContentEditorInsertContentFlags flags)
{
	EWebKitEditor *wk_editor;
	gboolean prefer_pre;
	gboolean cleanup_sig_id;

	wk_editor = E_WEBKIT_EDITOR (editor);

	/* It can happen that the view is not ready yet (it is in the middle of
	 * another load operation) so we have to queue the current operation and
	 * redo it again when the view is ready. This was happening when loading
	 * the stuff in EMailSignatureEditor. */
	if (wk_editor->priv->webkit_load_event != WEBKIT_LOAD_FINISHED) {
		webkit_editor_queue_post_reload_operation (
			wk_editor,
			(PostReloadOperationFunc) webkit_editor_insert_content,
			g_strdup (content),
			g_free,
			flags);
		return;
	}

	prefer_pre = (flags & E_CONTENT_EDITOR_INSERT_CONVERT_PREFER_PRE) != 0;
	cleanup_sig_id = (flags & E_CONTENT_EDITOR_INSERT_CLEANUP_SIGNATURE_ID) != 0;

	if ((flags & E_CONTENT_EDITOR_INSERT_CONVERT) &&
	    !(flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL)) {
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.InsertContent(%s, %x, %x, %x);",
			content, (flags & E_CONTENT_EDITOR_INSERT_TEXT_HTML) != 0, FALSE, prefer_pre);
	} else if ((flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL) &&
		   (flags & E_CONTENT_EDITOR_INSERT_TEXT_HTML)) {
		if ((strstr (content, "data-evo-draft") ||
		     strstr (content, "data-evo-signature-plain-text-mode"))) {
			e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
				"EvoEditor.LoadHTML(%s);", content);
			if (cleanup_sig_id)
				e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable, "EvoEditor.CleanupSignatureID();");
			return;
		}

		/* Only convert messages that are in HTML */
		if (wk_editor->priv->mode != E_CONTENT_EDITOR_MODE_HTML) {
			if (strstr (content, "<!-- text/html -->") &&
			    !strstr (content, "<!-- disable-format-prompt -->")) {
				if (!show_lose_formatting_dialog (wk_editor)) {
					webkit_editor_set_mode (wk_editor, E_CONTENT_EDITOR_MODE_HTML);
					e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
						"EvoEditor.LoadHTML(%s);", content);
					if (cleanup_sig_id)
						e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable, "EvoEditor.CleanupSignatureID();");
					return;
				}
			}
		}

		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.LoadHTML(%s);", content);
	} else if ((flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL) &&
		   (flags & E_CONTENT_EDITOR_INSERT_TEXT_PLAIN)) {
		gchar *html, **lines;
		gint ii;

		lines = g_strsplit (content ? content : "", "\n", -1);

		for (ii = 0; lines && lines[ii]; ii++) {
			gchar *line = lines[ii];
			gint len = strlen (line);

			if (len > 0 && line[len - 1] == '\r') {
				line[len - 1] = 0;
				len--;
			}

			if (len)
				lines[ii] = g_markup_printf_escaped ("<div>%s</div>", line);
			else
				lines[ii] = g_strdup ("<div><br></div>");

			g_free (line);
		}

		html = g_strjoinv ("", lines);

		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.LoadHTML(%s);", html);

		g_strfreev (lines);
		g_free (html);
	} else if ((flags & E_CONTENT_EDITOR_INSERT_QUOTE_CONTENT) &&
		   !(flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL)) {
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.InsertContent(%s, %x, %x, %x);",
			content, (flags & E_CONTENT_EDITOR_INSERT_TEXT_HTML) != 0, TRUE, prefer_pre);
	} else if (!(flags & E_CONTENT_EDITOR_INSERT_CONVERT) &&
		   !(flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL)) {
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.InsertContent(%s, %x, %x, %x);",
			content, (flags & E_CONTENT_EDITOR_INSERT_TEXT_HTML) != 0, FALSE, prefer_pre);
	} else {
		g_warning ("%s: Unsupported flags combination (0x%x)", G_STRFUNC, flags);
	}

	if (cleanup_sig_id)
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable, "EvoEditor.CleanupSignatureID();");

	if (flags & E_CONTENT_EDITOR_INSERT_REPLACE_ALL)
		webkit_editor_style_updated (wk_editor, TRUE);
}

static void
webkit_editor_get_content (EContentEditor *editor,
			   guint32 flags, /* bit-or of EContentEditorGetContentFlags */
			   const gchar *inline_images_from_domain,
                           GCancellable *cancellable,
			   GAsyncReadyCallback callback,
			   gpointer user_data)
{
	gchar *script, *cid_uid_prefix;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));

	cid_uid_prefix = camel_header_msgid_generate (inline_images_from_domain ? inline_images_from_domain : "");
	script = e_web_view_jsc_printf_script ("EvoEditor.GetContent(%d, %s, %s)", flags, cid_uid_prefix, DEFAULT_CSS_STYLE);

	webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (editor), script, -1, NULL, NULL,
		cancellable, callback, user_data);

	g_free (cid_uid_prefix);
	g_free (script);
}

static EContentEditorContentHash *
webkit_editor_get_content_finish (EContentEditor *editor,
				  GAsyncResult *result,
				  GError **error)
{
	JSCValue *value;
	EContentEditorContentHash *content_hash = NULL;
	GError *local_error = NULL;

	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (editor), NULL);
	g_return_val_if_fail (result != NULL, NULL);

	value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (editor), result, &local_error);

	if (local_error) {
		g_propagate_error (error, local_error);
		g_clear_object (&value);
		return NULL;
	}

	if (value) {
		JSCException *exception;

		exception = jsc_context_get_exception (jsc_value_get_context (value));

		if (exception) {
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "EvoEditor.GetContent() call failed: %s", jsc_exception_get_message (exception));
			jsc_context_clear_exception (jsc_value_get_context (value));
			g_clear_object (&value);
			return NULL;
		}

		if (jsc_value_is_object (value)) {
			struct _formats {
				const gchar *name;
				guint32 flags;
			} formats[] = {
				{ "raw-body-html", E_CONTENT_EDITOR_GET_RAW_BODY_HTML },
				{ "raw-body-plain", E_CONTENT_EDITOR_GET_RAW_BODY_PLAIN },
				{ "raw-body-stripped", E_CONTENT_EDITOR_GET_RAW_BODY_STRIPPED },
				{ "raw-draft", E_CONTENT_EDITOR_GET_RAW_DRAFT },
				{ "to-send-html", E_CONTENT_EDITOR_GET_TO_SEND_HTML },
				{ "to-send-plain", E_CONTENT_EDITOR_GET_TO_SEND_PLAIN }
			};
			JSCValue *images_value;
			gint ii;

			content_hash = e_content_editor_util_new_content_hash ();

			for (ii = 0; ii < G_N_ELEMENTS (formats); ii++) {
				gchar *cnt;

				cnt = e_web_view_jsc_get_object_property_string (value, formats[ii].name, NULL);
				if (cnt)
					e_content_editor_util_take_content_data (content_hash, formats[ii].flags, cnt, g_free);
			}

			images_value = jsc_value_object_get_property (value, "images");

			if (images_value) {
				if (jsc_value_is_array (images_value)) {
					GSList *image_parts = NULL;
					gint length;

					length = e_web_view_jsc_get_object_property_int32 (images_value, "length", 0);

					for (ii = 0; ii < length; ii++) {
						JSCValue *item_value;

						item_value = jsc_value_object_get_property_at_index (images_value, ii);

						if (!item_value ||
						    jsc_value_is_null (item_value) ||
						    jsc_value_is_undefined (item_value)) {
							g_warn_if_reached ();
							g_clear_object (&item_value);
							break;
						}

						if (jsc_value_is_object (item_value)) {
							gchar *src, *cid, *name;

							src = e_web_view_jsc_get_object_property_string (item_value, "src", NULL);
							cid = e_web_view_jsc_get_object_property_string (item_value, "cid", NULL);
							name = e_web_view_jsc_get_object_property_string (item_value, "name", NULL);

							if (src && *src && cid && *cid) {
								CamelMimePart *part = NULL;

								if (g_ascii_strncasecmp (src, "cid:", 4) == 0)
									part = e_content_editor_emit_ref_mime_part (editor, src);

								if (!part) {
									part = e_content_editor_util_create_data_mimepart (src, cid, TRUE, name, NULL,
										E_WEBKIT_EDITOR (editor)->priv->cancellable);
								}

								if (part)
									image_parts = g_slist_prepend (image_parts, part);
							}

							g_free (name);
							g_free (src);
							g_free (cid);
						}

						g_clear_object (&item_value);
					}

					if (image_parts)
						e_content_editor_util_take_content_data_images (content_hash, g_slist_reverse (image_parts));
				} else if (!jsc_value_is_undefined (images_value) && !jsc_value_is_null (images_value)) {
					g_warn_if_reached ();
				}

				g_clear_object (&images_value);
			}
		} else {
			g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Failed to retrieve message content"));
		}

		g_clear_object (&value);
	}

	return content_hash;
}

static gboolean
webkit_editor_can_undo (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->can_undo;
}

static void
webkit_editor_undo (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));

	wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoUndoRedo.Undo();");
}

static gboolean
webkit_editor_can_redo (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->can_redo;
}

static void
webkit_editor_redo (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));

	wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoUndoRedo.Redo();");
}

static void
webkit_editor_move_caret_on_coordinates (EContentEditor *editor,
					 gint xx,
					 gint yy,
					 gboolean cancel_if_not_collapsed)
{
	EWebKitEditor *wk_editor;
	GtkSettings *gtk_settings;
	gint font_dpi = -1;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));

	wk_editor = E_WEBKIT_EDITOR (editor);
	gtk_settings = gtk_settings_get_default ();
	if (gtk_settings)
		g_object_get (gtk_settings, "gtk-xft-dpi", &font_dpi, NULL);

	if (font_dpi > 0) {
		gdouble factor = font_dpi / (1024.0 * 96.0);
		if (factor > 1e-7) {
			xx = (gint) (xx / factor);
			yy = (gint) (yy / factor);
		}
	}

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.MoveSelectionToPoint(%d, %d, %x);",
		xx, yy, cancel_if_not_collapsed);
}

static void
webkit_editor_insert_emoticon (EContentEditor *editor,
                               const EEmoticon *emoticon)
{
	EWebKitEditor *wk_editor;
	GSettings *settings;
	const gchar *text;
	gchar *image_uri = NULL;
	gint width = 0, height = 0;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));
	g_return_if_fail (emoticon != NULL);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	if (g_settings_get_boolean (settings, "composer-unicode-smileys")) {
		text = emoticon->unicode_character;
	} else {
		text = emoticon->text_face;
		image_uri = e_emoticon_dup_uri (emoticon);

		if (image_uri) {
			width = 16;
			height = 16;
		}
	}

	wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.InsertEmoticon(%s, %s, %d, %d);",
		text, image_uri, width, height);

	g_clear_object (&settings);
	g_free (image_uri);
}

static void
webkit_editor_select_all (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_web_view_execute_editing_command (
		WEBKIT_WEB_VIEW (wk_editor), WEBKIT_EDITING_COMMAND_SELECT_ALL);
}

static void
webkit_editor_selection_wrap (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));

	wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.WrapSelection();");
}

static gboolean
webkit_editor_get_indent_level (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->indent_level;
}

static void
webkit_editor_selection_indent (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.Indent(true);");
}

static void
webkit_editor_selection_unindent (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.Indent(false);");
}

static void
webkit_editor_cut (EContentEditor *editor)
{
	webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (editor), WEBKIT_EDITING_COMMAND_CUT);
}

static void
webkit_editor_copy (EContentEditor *editor)
{
	webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (editor), WEBKIT_EDITING_COMMAND_COPY);
}

static ESpellChecker *
webkit_editor_get_spell_checker (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), NULL);

	return wk_editor->priv->spell_checker;
}

static gchar *
webkit_editor_get_caret_word (EContentEditor *editor)
{
	return webkit_editor_extract_and_free_jsc_string (
		webkit_editor_call_jsc_sync (E_WEBKIT_EDITOR (editor), "EvoEditor.GetCaretWord();"),
		NULL);
}

static void
webkit_editor_set_spell_checking_languages (EContentEditor *editor,
                                            const gchar **languages)
{
	EWebKitEditor *wk_editor;
	WebKitWebContext *web_context;

	wk_editor = E_WEBKIT_EDITOR (editor);
	web_context = webkit_web_view_get_context (WEBKIT_WEB_VIEW (wk_editor));
	webkit_web_context_set_spell_checking_languages (web_context, (const gchar * const *) languages);
}

static void
webkit_editor_set_start_bottom (EWebKitEditor *wk_editor,
				EThreeState value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if (wk_editor->priv->start_bottom == value)
		return;

	wk_editor->priv->start_bottom = value;

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.START_BOTTOM = %x;",
		e_content_editor_util_three_state_to_bool (value, "composer-reply-start-bottom"));

	g_object_notify (G_OBJECT (wk_editor), "start-bottom");
}

static EThreeState
webkit_editor_get_start_bottom (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), E_THREE_STATE_INCONSISTENT);

	return wk_editor->priv->start_bottom;
}

static void
webkit_editor_set_top_signature (EWebKitEditor *wk_editor,
				 EThreeState value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if (wk_editor->priv->top_signature == value)
		return;

	wk_editor->priv->top_signature = value;

	g_object_notify (G_OBJECT (wk_editor), "top-signature");
}

static EThreeState
webkit_editor_get_top_signature (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), E_THREE_STATE_INCONSISTENT);

	return wk_editor->priv->top_signature;
}

static void
webkit_editor_set_spell_check_enabled (EWebKitEditor *wk_editor,
                                       gboolean enable)
{
	WebKitWebContext *web_context;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if ((wk_editor->priv->spell_check_enabled ? 1 : 0) == (enable ? 1 : 0))
		return;

	wk_editor->priv->spell_check_enabled = enable;

	web_context = webkit_web_view_get_context (WEBKIT_WEB_VIEW (wk_editor));
	webkit_web_context_set_spell_checking_enabled (web_context, enable);

	g_object_notify (G_OBJECT (wk_editor), "spell-check-enabled");
}

static gboolean
webkit_editor_get_spell_check_enabled (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->spell_check_enabled;
}

static void
webkit_editor_set_visually_wrap_long_lines (EWebKitEditor *wk_editor,
					    gboolean value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if ((wk_editor->priv->visually_wrap_long_lines ? 1 : 0) == (value ? 1 : 0))
		return;

	wk_editor->priv->visually_wrap_long_lines = value;

	webkit_editor_update_styles (E_CONTENT_EDITOR (wk_editor));

	g_object_notify (G_OBJECT (wk_editor), "visually-wrap-long-lines");
}

static gboolean
webkit_editor_get_visually_wrap_long_lines (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->visually_wrap_long_lines;
}

static gboolean
webkit_editor_is_editable (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return webkit_web_view_is_editable (WEBKIT_WEB_VIEW (wk_editor));
}

static void
webkit_editor_set_editable (EWebKitEditor *wk_editor,
                                    gboolean editable)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	return webkit_web_view_set_editable (WEBKIT_WEB_VIEW (wk_editor), editable);
}

static gboolean
webkit_editor_is_ready (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	/* Editor is ready just in case that the web view is not loading. */
	return wk_editor->priv->webkit_load_event == WEBKIT_LOAD_FINISHED &&
		!webkit_web_view_is_loading (WEBKIT_WEB_VIEW (wk_editor));
}

static gchar *
webkit_editor_get_current_signature_uid (EContentEditor *editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (editor), NULL);

	return webkit_editor_extract_and_free_jsc_string (
		webkit_editor_call_jsc_sync (E_WEBKIT_EDITOR (editor), "EvoEditor.GetCurrentSignatureUid();"),
		NULL);
}

static gchar *
webkit_editor_insert_signature (EContentEditor *editor,
                                const gchar *content,
                                EContentEditorMode editor_mode,
				gboolean can_reposition_caret,
                                const gchar *signature_id,
                                gboolean *set_signature_from_message,
                                gboolean *check_if_signature_is_changed,
                                gboolean *ignore_next_signature_change)
{
	JSCValue *jsc_value;
	gchar *res = NULL, *tmp = NULL;

	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (editor), NULL);

	if (editor_mode != E_CONTENT_EDITOR_MODE_HTML && content && *content) {
		if (editor_mode == E_CONTENT_EDITOR_MODE_MARKDOWN_HTML)
			tmp = e_markdown_utils_text_to_html (content, -1);

		if (!tmp)
			tmp = camel_text_to_html (content, CAMEL_MIME_FILTER_TOHTML_PRE, 0);

		if (tmp)
			content = tmp;
	}

	jsc_value = webkit_editor_call_jsc_sync (E_WEBKIT_EDITOR (editor),
		"EvoEditor.InsertSignature(%s, %x, %x, %s, %x, %x, %x, %x, %x, %x);",
		content ? content : "",
		editor_mode == E_CONTENT_EDITOR_MODE_HTML,
		can_reposition_caret,
		signature_id,
		*set_signature_from_message,
		*check_if_signature_is_changed,
		*ignore_next_signature_change,
		e_content_editor_util_three_state_to_bool (e_content_editor_get_start_bottom (editor), "composer-reply-start-bottom"),
		e_content_editor_util_three_state_to_bool (e_content_editor_get_top_signature (editor), "composer-top-signature"),
		!e_content_editor_util_three_state_to_bool (E_THREE_STATE_INCONSISTENT, "composer-no-signature-delim"));

	g_free (tmp);

	if (jsc_value) {
		*set_signature_from_message = e_web_view_jsc_get_object_property_boolean (jsc_value, "fromMessage", FALSE);
		*check_if_signature_is_changed = e_web_view_jsc_get_object_property_boolean (jsc_value, "checkChanged", FALSE);
		*ignore_next_signature_change = e_web_view_jsc_get_object_property_boolean (jsc_value, "ignoreNextChange", FALSE);
		res = e_web_view_jsc_get_object_property_string (jsc_value, "newUid", NULL);

		g_clear_object (&jsc_value);
	}

	return res;
}

static void
webkit_editor_clear_undo_redo_history (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));

	wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoUndoRedo.Clear();");
}

static void
webkit_editor_replace_caret_word (EContentEditor *editor,
                                  const gchar *replacement)
{
	EWebKitEditor *wk_editor;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));

	wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.ReplaceCaretWord(%s);", replacement);
}

static void
webkit_editor_finish_search (EWebKitEditor *wk_editor)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if (!wk_editor->priv->find_controller)
		return;

	webkit_find_controller_search_finish (wk_editor->priv->find_controller);

	wk_editor->priv->performing_replace_all = FALSE;
	wk_editor->priv->replaced_count = 0;
	g_free (wk_editor->priv->replace_with);
	wk_editor->priv->replace_with = NULL;

	if (wk_editor->priv->found_text_handler_id) {
		g_signal_handler_disconnect (wk_editor->priv->find_controller, wk_editor->priv->found_text_handler_id);
		wk_editor->priv->found_text_handler_id = 0;
	}

	if (wk_editor->priv->failed_to_find_text_handler_id) {
		g_signal_handler_disconnect (wk_editor->priv->find_controller, wk_editor->priv->failed_to_find_text_handler_id);
		wk_editor->priv->failed_to_find_text_handler_id = 0;
	}

	wk_editor->priv->find_controller = NULL;
}

static guint32 /* WebKitFindOptions */
find_flags_to_webkit_find_options (guint32 flags /* EContentEditorFindFlags */)
{
	guint32 options = 0;

	if (flags & E_CONTENT_EDITOR_FIND_CASE_INSENSITIVE)
		options |= WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE;

	if (flags & E_CONTENT_EDITOR_FIND_WRAP_AROUND)
		options |= WEBKIT_FIND_OPTIONS_WRAP_AROUND;

	if (flags & E_CONTENT_EDITOR_FIND_MODE_BACKWARDS)
		options |= WEBKIT_FIND_OPTIONS_BACKWARDS;

	return options;
}

static void
webkit_editor_replace (EContentEditor *editor,
                       const gchar *replacement)
{
	EWebKitEditor *wk_editor;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));

	wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.ReplaceSelection(%s);", replacement);
}

static gboolean
search_next_on_idle (EWebKitEditor *wk_editor)
{
	webkit_find_controller_search_next (wk_editor->priv->find_controller);

	return G_SOURCE_REMOVE;
}

static void
webkit_find_controller_found_text_cb (WebKitFindController *find_controller,
                                      guint match_count,
                                      EWebKitEditor *wk_editor)
{
	wk_editor->priv->current_text_not_found = FALSE;

	if (wk_editor->priv->performing_replace_all) {
		if (!wk_editor->priv->replaced_count)
			wk_editor->priv->replaced_count = match_count;

		/* Repeatedly search for 'word', then replace selection by
		 * 'replacement'. Repeat until there's at least one occurrence of
		 * 'word' in the document */
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.ReplaceSelection(%s);", wk_editor->priv->replace_with);

		g_idle_add ((GSourceFunc) search_next_on_idle, wk_editor);
	} else {
		e_content_editor_emit_find_done (E_CONTENT_EDITOR (wk_editor), match_count);
	}
}

static void
webkit_find_controller_failed_to_find_text_cb (WebKitFindController *find_controller,
                                               EWebKitEditor *wk_editor)
{
	wk_editor->priv->current_text_not_found = TRUE;

	if (wk_editor->priv->performing_replace_all) {
		guint replaced_count = wk_editor->priv->replaced_count;

		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoUndoRedo.StopRecord(EvoUndoRedo.RECORD_KIND_GROUP, %s);", "ReplaceAll");

		webkit_editor_finish_search (wk_editor);
		e_content_editor_emit_replace_all_done (E_CONTENT_EDITOR (wk_editor), replaced_count);
	} else {
		e_content_editor_emit_find_done (E_CONTENT_EDITOR (wk_editor), 0);
	}
}

static void
webkit_editor_prepare_find_controller (EWebKitEditor *wk_editor)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));
	g_return_if_fail (wk_editor->priv->find_controller == NULL);

	wk_editor->priv->find_controller = webkit_web_view_get_find_controller (WEBKIT_WEB_VIEW (wk_editor));

	wk_editor->priv->found_text_handler_id = g_signal_connect (
		wk_editor->priv->find_controller, "found-text",
		G_CALLBACK (webkit_find_controller_found_text_cb), wk_editor);

	wk_editor->priv->failed_to_find_text_handler_id = g_signal_connect (
		wk_editor->priv->find_controller, "failed-to-find-text",
		G_CALLBACK (webkit_find_controller_failed_to_find_text_cb), wk_editor);

	wk_editor->priv->performing_replace_all = FALSE;
	wk_editor->priv->replaced_count = 0;
	wk_editor->priv->current_text_not_found = FALSE;
	g_free (wk_editor->priv->replace_with);
	wk_editor->priv->replace_with = NULL;
}

static void
webkit_editor_find (EContentEditor *editor,
                    guint32 flags,
                    const gchar *text)
{
	EWebKitEditor *wk_editor;
	guint32 wk_options;
	gboolean needs_init;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));
	g_return_if_fail (text != NULL);

	wk_editor = E_WEBKIT_EDITOR (editor);

	wk_options = find_flags_to_webkit_find_options (flags);

	needs_init = !wk_editor->priv->find_controller;
	if (needs_init) {
		webkit_editor_prepare_find_controller (wk_editor);
	} else {
		needs_init = wk_options != webkit_find_controller_get_options (wk_editor->priv->find_controller) ||
			g_strcmp0 (text, webkit_find_controller_get_search_text (wk_editor->priv->find_controller)) != 0;
	}

	if (needs_init) {
		webkit_find_controller_search (wk_editor->priv->find_controller, text, wk_options, G_MAXUINT);
	} else if ((flags & E_CONTENT_EDITOR_FIND_PREVIOUS) != 0) {
		webkit_find_controller_search_previous (wk_editor->priv->find_controller);
	} else {
		webkit_find_controller_search_next (wk_editor->priv->find_controller);
	}
}

static void
webkit_editor_replace_all (EContentEditor *editor,
                           guint32 flags,
                           const gchar *find_text,
                           const gchar *replace_with)
{
	EWebKitEditor *wk_editor;
	guint32 wk_options;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));
	g_return_if_fail (find_text != NULL);
	g_return_if_fail (replace_with != NULL);

	wk_editor = E_WEBKIT_EDITOR (editor);
	wk_options = find_flags_to_webkit_find_options (flags);

	/* Unset the two, because replace-all will be always from the beginning
	   of the document downwards, without wrap around, to avoid indefinite
	   cycle with similar search and replace words. */
	wk_options = wk_options & (~(WEBKIT_FIND_OPTIONS_BACKWARDS | WEBKIT_FIND_OPTIONS_WRAP_AROUND));

	if (!wk_editor->priv->find_controller)
		webkit_editor_prepare_find_controller (wk_editor);

	g_free (wk_editor->priv->replace_with);
	wk_editor->priv->replace_with = g_strdup (replace_with);

	wk_editor->priv->performing_replace_all = TRUE;
	wk_editor->priv->replaced_count = 0;

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoUndoRedo.StartRecord(EvoUndoRedo.RECORD_KIND_GROUP, %s);", "ReplaceAll");

	webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (wk_editor), "MoveToBeginningOfDocumentAndModifySelection");

	webkit_find_controller_search (wk_editor->priv->find_controller, find_text, wk_options, G_MAXUINT);
}

static void
webkit_editor_selection_save (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));

	wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.StoreSelection();");
}

static void
webkit_editor_selection_restore (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));

	wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.RestoreSelection();");
}

static void
webkit_editor_on_dialog_open (EContentEditor *editor,
			      const gchar *name)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.OnDialogOpen(%s);", name);

	if (g_strcmp0 (name, E_CONTENT_EDITOR_DIALOG_SPELLCHECK) == 0) {
		gchar **strv;

		strv = e_spell_checker_list_active_languages (wk_editor->priv->spell_checker, NULL);

		if (strv) {
			gint ii, len = 0;
			gchar *langs, *ptr;

			for (ii = 0; strv[ii]; ii++) {
				len += strlen (strv[ii]) + 1;
			}

			len++;

			langs = g_slice_alloc0 (len);
			ptr = langs;

			for (ii = 0; strv[ii]; ii++) {
				strcpy (ptr, strv[ii]);
				ptr += strlen (strv[ii]);
				if (strv[ii + 1]) {
					*ptr = '|';
					ptr++;
				}
			}

			*ptr = '\0';

			e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
				"EvoEditor.SetSpellCheckLanguages(%s);", langs);

			g_slice_free1 (len, langs);
			g_strfreev (strv);
		}
	}
}

static void
webkit_editor_on_dialog_close (EContentEditor *editor,
			       const gchar *name)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.OnDialogClose(%s);", name);

	if (g_strcmp0 (name, E_CONTENT_EDITOR_DIALOG_SPELLCHECK) == 0 ||
	    g_strcmp0 (name, E_CONTENT_EDITOR_DIALOG_FIND) == 0 ||
	    g_strcmp0 (name, E_CONTENT_EDITOR_DIALOG_REPLACE) == 0)
		webkit_editor_finish_search (E_WEBKIT_EDITOR (editor));
}

static void
webkit_editor_delete_cell_contents (EContentEditor *editor)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"var arr = EvoEditor.RemoveCurrentElementAttr();"
		"EvoEditor.DialogUtilsCurrentElementFromFocus(\"TABLE*\");"
		"EvoEditor.DialogUtilsTableDeleteCellContent();"
		"EvoEditor.RemoveCurrentElementAttr();"
		"EvoEditor.RestoreCurrentElementAttr(arr);");
}

static void
webkit_editor_delete_column (EContentEditor *editor)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"var arr = EvoEditor.RemoveCurrentElementAttr();"
		"EvoEditor.DialogUtilsCurrentElementFromFocus(\"TABLE*\");"
		"EvoEditor.DialogUtilsTableDeleteColumn();"
		"EvoEditor.RemoveCurrentElementAttr();"
		"EvoEditor.RestoreCurrentElementAttr(arr);");
}

static void
webkit_editor_delete_row (EContentEditor *editor)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"var arr = EvoEditor.RemoveCurrentElementAttr();"
		"EvoEditor.DialogUtilsCurrentElementFromFocus(\"TABLE*\");"
		"EvoEditor.DialogUtilsTableDeleteRow();"
		"EvoEditor.RemoveCurrentElementAttr();"
		"EvoEditor.RestoreCurrentElementAttr(arr);");
}

static void
webkit_editor_delete_table (EContentEditor *editor)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"var arr = EvoEditor.RemoveCurrentElementAttr();"
		"EvoEditor.DialogUtilsCurrentElementFromFocus(\"TABLE*\");"
		"EvoEditor.DialogUtilsTableDelete();"
		"EvoEditor.RemoveCurrentElementAttr();"
		"EvoEditor.RestoreCurrentElementAttr(arr);");
}

static void
webikt_editor_call_table_insert (EContentEditor *editor,
				 const gchar *what,
				 gint where)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"var arr = EvoEditor.RemoveCurrentElementAttr();"
		"EvoEditor.DialogUtilsCurrentElementFromFocus(\"TABLE*\");"
		"EvoEditor.DialogUtilsTableInsert(%s, %d);"
		"EvoEditor.RemoveCurrentElementAttr();"
		"EvoEditor.RestoreCurrentElementAttr(arr);",
		what, where);
}

static void
webkit_editor_delete_h_rule (EContentEditor *editor)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.DialogUtilsContextElementDelete();");
}

static void
webkit_editor_delete_image (EContentEditor *editor)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.DialogUtilsContextElementDelete();");
}

static void
webkit_editor_insert_column_after (EContentEditor *editor)
{
	webikt_editor_call_table_insert (editor, "column", +1);
}

static void
webkit_editor_insert_column_before (EContentEditor *editor)
{
	webikt_editor_call_table_insert (editor, "column", -1);
}

static void
webkit_editor_insert_row_above (EContentEditor *editor)
{
	webikt_editor_call_table_insert (editor, "row", -1);
}

static void
webkit_editor_insert_row_below (EContentEditor *editor)
{
	webikt_editor_call_table_insert (editor, "row", +1);
}

static void
webkit_editor_h_rule_set_align (EContentEditor *editor,
                                const gchar *value)
{
	webkit_editor_dialog_utils_set_attribute (E_WEBKIT_EDITOR (editor), NULL, "align", value);
}

static gchar *
webkit_editor_h_rule_get_align (EContentEditor *editor)
{
	gchar *value;

	value = webkit_editor_dialog_utils_get_attribute (E_WEBKIT_EDITOR (editor), NULL, "align");

	if (!value || !*value) {
		g_free (value);
		value = g_strdup ("center");
	}

	return value;
}

static void
webkit_editor_h_rule_set_size (EContentEditor *editor,
                               gint value)
{
	webkit_editor_dialog_utils_set_attribute_int (E_WEBKIT_EDITOR (editor), NULL, "size", value);
}

static gint
webkit_editor_h_rule_get_size (EContentEditor *editor)
{
	gint size;

	size = webkit_editor_dialog_utils_get_attribute_int (E_WEBKIT_EDITOR (editor), NULL, "size", 2);

	if (!size)
		size = 2;

	return size;
}

static void
webkit_editor_h_rule_set_width (EContentEditor *editor,
                                gint value,
                                EContentEditorUnit unit)
{
	webkit_editor_dialog_utils_set_attribute_with_unit (E_WEBKIT_EDITOR (editor), NULL, "width", value, unit);
}

static gint
webkit_editor_h_rule_get_width (EContentEditor *editor,
                                EContentEditorUnit *unit)
{
	gint value;

	value = webkit_editor_dialog_utils_get_attribute_with_unit (E_WEBKIT_EDITOR (editor), NULL, "width", 0, unit);

	if (!value && *unit == E_CONTENT_EDITOR_UNIT_AUTO) {
		*unit = E_CONTENT_EDITOR_UNIT_PERCENTAGE;
		value = 100;
	}

	return value;
}

static void
webkit_editor_h_rule_set_no_shade (EContentEditor *editor,
                                   gboolean value)
{
	webkit_editor_dialog_utils_set_attribute (E_WEBKIT_EDITOR (editor), NULL, "noshade", value ? "" : NULL);
}

static gboolean
webkit_editor_h_rule_get_no_shade (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_has_attribute (E_WEBKIT_EDITOR (editor), "noshade");
}

static void
webkit_editor_insert_image (EContentEditor *editor,
                            const gchar *image_uri)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);
	gint width = -1, height = -1;

	g_return_if_fail (image_uri != NULL);

	if (g_ascii_strncasecmp (image_uri, "file://", 7) == 0) {
		gchar *filename;

		filename = g_filename_from_uri (image_uri, NULL, NULL);

		if (filename) {
			if (!gdk_pixbuf_get_file_info (filename, &width, &height)) {
				width = -1;
				height = -1;
			}

			g_free (filename);
		}
	}

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.InsertImage(%s, %d, %d);",
		image_uri, width, height);
}

static void
webkit_editor_replace_image_src (EWebKitEditor *wk_editor,
                                 const gchar *selector,
                                 const gchar *image_uri)
{
	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.ReplaceImageSrc(%s, %s);",
		selector,
		image_uri);
}

static void
webkit_editor_image_set_src (EContentEditor *editor,
                             const gchar *value)
{
	webkit_editor_dialog_utils_set_attribute (E_WEBKIT_EDITOR (editor), NULL, "src", value);
}

static gchar *
webkit_editor_image_get_src (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute (E_WEBKIT_EDITOR (editor), NULL, "src");
}

static void
webkit_editor_image_set_alt (EContentEditor *editor,
                             const gchar *value)
{
	webkit_editor_dialog_utils_set_attribute (E_WEBKIT_EDITOR (editor), NULL, "alt", value);
}

static gchar *
webkit_editor_image_get_alt (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute (E_WEBKIT_EDITOR (editor), NULL, "alt");
}

static void
webkit_editor_image_set_url (EContentEditor *editor,
                             const gchar *value)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.DialogUtilsSetImageUrl(%s);",
		value);
}

static gchar *
webkit_editor_image_get_url (EContentEditor *editor)
{
	return webkit_editor_extract_and_free_jsc_string (
		webkit_editor_call_jsc_sync (E_WEBKIT_EDITOR (editor), "EvoEditor.DialogUtilsGetImageUrl();"),
		NULL);
}

static void
webkit_editor_image_set_vspace (EContentEditor *editor,
                                gint value)
{
	webkit_editor_dialog_utils_set_attribute_int (E_WEBKIT_EDITOR (editor), NULL, "vspace", value);
}

static gint
webkit_editor_image_get_vspace (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute_int (E_WEBKIT_EDITOR (editor), NULL, "vspace", 0);
}

static void
webkit_editor_image_set_hspace (EContentEditor *editor,
                                        gint value)
{
	webkit_editor_dialog_utils_set_attribute_int (E_WEBKIT_EDITOR (editor), NULL, "hspace", value);
}

static gint
webkit_editor_image_get_hspace (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute_int (E_WEBKIT_EDITOR (editor), NULL, "hspace", 0);
}

static void
webkit_editor_image_set_border (EContentEditor *editor,
                                gint value)
{
	webkit_editor_dialog_utils_set_attribute_int (E_WEBKIT_EDITOR (editor), NULL, "border", value);
}

static gint
webkit_editor_image_get_border (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute_int (E_WEBKIT_EDITOR (editor), NULL, "border", 0);
}

static void
webkit_editor_image_set_align (EContentEditor *editor,
                               const gchar *value)
{
	webkit_editor_dialog_utils_set_attribute (E_WEBKIT_EDITOR (editor), NULL, "align", value);
}

static gchar *
webkit_editor_image_get_align (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute (E_WEBKIT_EDITOR (editor), NULL, "align");
}

static gint32
webkit_editor_image_get_natural_width (EContentEditor *editor)
{
	return webkit_editor_extract_and_free_jsc_int32 (
		webkit_editor_call_jsc_sync (E_WEBKIT_EDITOR (editor), "EvoEditor.DialogUtilsGetImageWidth(true);"),
		0);
}

static gint32
webkit_editor_image_get_natural_height (EContentEditor *editor)
{
	return webkit_editor_extract_and_free_jsc_int32 (
		webkit_editor_call_jsc_sync (E_WEBKIT_EDITOR (editor), "EvoEditor.DialogUtilsGetImageHeight(true);"),
		0);
}

static void
webkit_editor_image_set_height (EContentEditor *editor,
                                gint value)
{
	webkit_editor_dialog_utils_set_attribute_with_unit (E_WEBKIT_EDITOR (editor), NULL, "height", value, E_CONTENT_EDITOR_UNIT_PIXEL);
}

static void
webkit_editor_image_set_width (EContentEditor *editor,
                               gint value)
{
	webkit_editor_dialog_utils_set_attribute_with_unit (E_WEBKIT_EDITOR (editor), NULL, "width", value, E_CONTENT_EDITOR_UNIT_PIXEL);
}

static void
webkit_editor_image_set_height_follow (EContentEditor *editor,
                                      gboolean value)
{
	webkit_editor_dialog_utils_set_attribute (E_WEBKIT_EDITOR (editor), NULL, "style", value ? "height: auto;" : NULL);
}

static void
webkit_editor_image_set_width_follow (EContentEditor *editor,
                                     gboolean value)
{
	webkit_editor_dialog_utils_set_attribute (E_WEBKIT_EDITOR (editor), NULL, "style", value ? "width: auto;" : NULL);
}

static gint32
webkit_editor_image_get_width (EContentEditor *editor)
{
	return webkit_editor_extract_and_free_jsc_int32 (
		webkit_editor_call_jsc_sync (E_WEBKIT_EDITOR (editor), "EvoEditor.DialogUtilsGetImageWidth(false);"),
		0);
}

static gint32
webkit_editor_image_get_height (EContentEditor *editor)
{
	return webkit_editor_extract_and_free_jsc_int32 (
		webkit_editor_call_jsc_sync (E_WEBKIT_EDITOR (editor), "EvoEditor.DialogUtilsGetImageHeight(false);"),
		0);
}

static void
webkit_editor_selection_unlink (EContentEditor *editor)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.Unlink();");
}

static void
webkit_editor_link_set_properties (EContentEditor *editor,
				   const gchar *href,
				   const gchar *text,
				   const gchar *name)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.LinkSetProperties(%s, %s, %s);",
		href, text, name);
}

static void
webkit_editor_link_get_properties (EContentEditor *editor,
				   gchar **out_href,
				   gchar **out_text,
				   gchar **out_name)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);
	JSCValue *result;

	result = webkit_editor_call_jsc_sync (wk_editor, "EvoEditor.LinkGetProperties();");

	if (result) {
		*out_href = e_web_view_jsc_get_object_property_string (result, "href", NULL);
		*out_text = e_web_view_jsc_get_object_property_string (result, "text", NULL);
		*out_name = e_web_view_jsc_get_object_property_string (result, "name", NULL);

		g_clear_object (&result);
	} else {
		*out_href = NULL;
		*out_text = NULL;
		*out_name = NULL;
	}
}

static void
webkit_editor_set_alignment (EWebKitEditor *wk_editor,
                             EContentEditorAlignment value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.SetAlignment(%d);",
		value);
}

static EContentEditorAlignment
webkit_editor_get_alignment (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), E_CONTENT_EDITOR_ALIGNMENT_LEFT);

	return wk_editor->priv->alignment;
}

static void
webkit_editor_set_block_format (EWebKitEditor *wk_editor,
                                EContentEditorBlockFormat value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.SetBlockFormat(%d);",
		value);
}

static EContentEditorBlockFormat
webkit_editor_get_block_format (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), E_CONTENT_EDITOR_BLOCK_FORMAT_NONE);

	return wk_editor->priv->block_format;
}

static void
webkit_editor_set_background_color (EWebKitEditor *wk_editor,
                                    const GdkRGBA *value)
{
	gchar color[64];

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if ((!value && !wk_editor->priv->background_color) ||
	    (value && wk_editor->priv->background_color && gdk_rgba_equal (value, wk_editor->priv->background_color)))
		return;

	if (value && value->alpha > 1e-9) {
		webkit_editor_utils_color_to_string (color, sizeof (color), value);
		g_clear_pointer (&wk_editor->priv->background_color, gdk_rgba_free);
		wk_editor->priv->background_color = gdk_rgba_copy (value);
	} else {
		g_snprintf (color, sizeof (color), "inherit");
		g_clear_pointer (&wk_editor->priv->background_color, gdk_rgba_free);
		wk_editor->priv->background_color = NULL;
	}

	webkit_web_view_execute_editing_command_with_argument (WEBKIT_WEB_VIEW (wk_editor), "BackColor", color);
}

static const GdkRGBA *
webkit_editor_get_background_color (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), NULL);

	if (!wk_editor->priv->background_color)
		return &transparent;

	return wk_editor->priv->background_color;
}

static void
webkit_editor_set_font_name (EWebKitEditor *wk_editor,
                             const gchar *value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.SetFontName(%s);",
		value ? value : "");
}

static const gchar *
webkit_editor_get_font_name (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), NULL);

	if (wk_editor->priv->mode != E_CONTENT_EDITOR_MODE_HTML)
		return NULL;

	return wk_editor->priv->font_name;
}

static void
webkit_editor_set_font_color (EWebKitEditor *wk_editor,
                              const GdkRGBA *value)
{
	gchar color[64];

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if ((!value && !wk_editor->priv->font_color) ||
	    (value && wk_editor->priv->font_color && gdk_rgba_equal (value, wk_editor->priv->font_color)))
		return;

	webkit_editor_utils_color_to_string (color, sizeof (color), value);

	webkit_web_view_execute_editing_command_with_argument (WEBKIT_WEB_VIEW (wk_editor), "ForeColor",
		webkit_editor_utils_color_to_string (color, sizeof (color), value));
}

static const GdkRGBA *
webkit_editor_get_font_color (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), NULL);

	if (wk_editor->priv->mode != E_CONTENT_EDITOR_MODE_HTML || !wk_editor->priv->font_color)
		return &black;

	return wk_editor->priv->font_color;
}

static void
webkit_editor_set_font_size (EWebKitEditor *wk_editor,
                             gint value)
{
	gchar sz[2] = { 0, 0 };

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if (wk_editor->priv->font_size == value)
		return;

	if (value >= 1 && value <= 7) {
		sz[0] = '0' + value;
	} else {
		g_warn_if_reached ();
		return;
	}

	webkit_web_view_execute_editing_command_with_argument (WEBKIT_WEB_VIEW (wk_editor), "FontSize", sz);
}

static gint
webkit_editor_get_font_size (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), -1);

	return wk_editor->priv->font_size;
}

static void
webkit_editor_set_style_flag (EWebKitEditor *wk_editor,
			      EWebKitEditorStyleFlags flag,
			      gboolean do_set)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if (((wk_editor->priv->temporary_style_flags & flag) != 0 ? 1 : 0) == (do_set ? 1 : 0))
		return;

	switch (flag) {
	case E_WEBKIT_EDITOR_STYLE_NONE:
		break;
	case E_WEBKIT_EDITOR_STYLE_IS_BOLD:
		webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (wk_editor), "Bold");
		break;
	case E_WEBKIT_EDITOR_STYLE_IS_ITALIC:
		webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (wk_editor), "Italic");
		break;
	case E_WEBKIT_EDITOR_STYLE_IS_UNDERLINE:
		webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (wk_editor), "Underline");
		break;
	case E_WEBKIT_EDITOR_STYLE_IS_STRIKETHROUGH:
		webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (wk_editor), "Strikethrough");
		break;
	case E_WEBKIT_EDITOR_STYLE_IS_SUBSCRIPT:
		webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (wk_editor), "Subscript");
		break;
	case E_WEBKIT_EDITOR_STYLE_IS_SUPERSCRIPT:
		webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (wk_editor), "Superscript");
		break;
	}

	wk_editor->priv->temporary_style_flags = (wk_editor->priv->temporary_style_flags & (~flag)) | (do_set ? flag : 0);
}

static gboolean
webkit_editor_get_style_flag (EWebKitEditor *wk_editor,
			      EWebKitEditorStyleFlags flag)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return (wk_editor->priv->style_flags & flag) != 0;
}

static gchar *
webkit_editor_page_get_background_image_uri (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute (E_WEBKIT_EDITOR (editor), "body", "background");
}

static void
webkit_editor_page_set_background_image_uri (EContentEditor *editor,
                                             const gchar *uri)
{
	webkit_editor_replace_image_src (E_WEBKIT_EDITOR (editor), "body", uri);
}

static void
webkit_editor_cell_set_v_align (EContentEditor *editor,
                                const gchar *value,
                                EContentEditorScope scope)
{
	webkit_editor_dialog_utils_set_table_attribute (E_WEBKIT_EDITOR (editor), scope, "valign", value && *value ? value : NULL);
}

static gchar *
webkit_editor_cell_get_v_align (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute (E_WEBKIT_EDITOR (editor), NULL, "valign");
}

static void
webkit_editor_cell_set_align (EContentEditor *editor,
                              const gchar *value,
                              EContentEditorScope scope)
{
	webkit_editor_dialog_utils_set_table_attribute (E_WEBKIT_EDITOR (editor), scope, "align", value && *value ? value : NULL);
}

static gchar *
webkit_editor_cell_get_align (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute (E_WEBKIT_EDITOR (editor), NULL, "align");
}

static void
webkit_editor_cell_set_wrap (EContentEditor *editor,
                             gboolean value,
                             EContentEditorScope scope)
{
	webkit_editor_dialog_utils_set_table_attribute (E_WEBKIT_EDITOR (editor), scope, "nowrap", !value ? "" : NULL);
}

static gboolean
webkit_editor_cell_get_wrap (EContentEditor *editor)
{
	gboolean value = FALSE;
	gchar *nowrap;

	nowrap = webkit_editor_dialog_utils_get_attribute (E_WEBKIT_EDITOR (editor), NULL, "nowrap");
	value = !nowrap;

	g_free (nowrap);

	return value;
}

static void
webkit_editor_cell_set_header_style (EContentEditor *editor,
                                     gboolean value,
                                     EContentEditorScope scope)
{
	EWebKitEditor *wk_editor;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (editor));

	wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.DialogUtilsTableSetHeader(%d, %x);",
		scope, value);
}

static gboolean
webkit_editor_cell_is_header (EContentEditor *editor)
{
	return webkit_editor_extract_and_free_jsc_boolean (
		webkit_editor_call_jsc_sync (E_WEBKIT_EDITOR (editor),
			"EvoEditor.DialogUtilsTableGetCellIsHeader();"),
		FALSE);
}

static gint
webkit_editor_cell_get_width (EContentEditor *editor,
                              EContentEditorUnit *unit)
{
	return webkit_editor_dialog_utils_get_attribute_with_unit (E_WEBKIT_EDITOR (editor), NULL, "width", 0, unit);
}

static gint
webkit_editor_cell_get_row_span (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute_int (E_WEBKIT_EDITOR (editor), NULL, "rowspan", 0);
}

static gint
webkit_editor_cell_get_col_span (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute_int (E_WEBKIT_EDITOR (editor), NULL, "colspan", 0);
}

static gchar *
webkit_editor_cell_get_background_image_uri (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute (E_WEBKIT_EDITOR (editor), NULL, "background");
}

static void
webkit_editor_cell_get_background_color (EContentEditor *editor,
                                         GdkRGBA *color)
{
	webkit_editor_dialog_utils_get_attribute_color (E_WEBKIT_EDITOR (editor), NULL, "bgcolor", color);
}

static void
webkit_editor_cell_set_row_span (EContentEditor *editor,
                                 gint value,
                                 EContentEditorScope scope)
{
	gchar str_value[64];

	webkit_editor_dialog_utils_set_table_attribute (E_WEBKIT_EDITOR (editor), scope, "rowspan",
		webkit_editor_utils_int_to_string (str_value, sizeof (str_value), value));
}

static void
webkit_editor_cell_set_col_span (EContentEditor *editor,
                                 gint value,
                                 EContentEditorScope scope)
{
	gchar str_value[64];

	webkit_editor_dialog_utils_set_table_attribute (E_WEBKIT_EDITOR (editor), scope, "colspan",
		webkit_editor_utils_int_to_string (str_value, sizeof (str_value), value));
}

static void
webkit_editor_cell_set_width (EContentEditor *editor,
                              gint value,
                              EContentEditorUnit unit,
                              EContentEditorScope scope)
{
	gchar str_value[64];

	webkit_editor_dialog_utils_set_table_attribute (E_WEBKIT_EDITOR (editor), scope, "width",
		webkit_editor_utils_int_with_unit_to_string (str_value, sizeof (str_value), value, unit));
}

static void
webkit_editor_cell_set_background_color (EContentEditor *editor,
                                         const GdkRGBA *value,
                                         EContentEditorScope scope)
{
	gchar str_value[64];

	webkit_editor_dialog_utils_set_table_attribute (E_WEBKIT_EDITOR (editor), scope, "bgcolor",
		webkit_editor_utils_color_to_string (str_value, sizeof (str_value), value));
}

static void
webkit_editor_cell_set_background_image_uri (EContentEditor *editor,
                                             const gchar *uri)
{
	webkit_editor_replace_image_src (E_WEBKIT_EDITOR (editor), NULL, uri);
}

static void
webkit_editor_table_set_row_count (EContentEditor *editor,
                                   guint value)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.DialogUtilsTableSetRowCount(%d);",
		value);
}

static guint
webkit_editor_table_get_row_count (EContentEditor *editor)
{
	return webkit_editor_extract_and_free_jsc_int32 (
		webkit_editor_call_jsc_sync (E_WEBKIT_EDITOR (editor), "EvoEditor.DialogUtilsTableGetRowCount();"),
		0);
}

static void
webkit_editor_table_set_column_count (EContentEditor *editor,
                                      guint value)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.DialogUtilsTableSetColumnCount(%d);",
		value);
}

static guint
webkit_editor_table_get_column_count (EContentEditor *editor)
{
	return webkit_editor_extract_and_free_jsc_int32 (
		webkit_editor_call_jsc_sync (E_WEBKIT_EDITOR (editor), "EvoEditor.DialogUtilsTableGetColumnCount();"),
		0);
}

static void
webkit_editor_table_set_width (EContentEditor *editor,
                               gint value,
                               EContentEditorUnit unit)
{
	webkit_editor_dialog_utils_set_attribute_with_unit (E_WEBKIT_EDITOR (editor), NULL, "width", value, unit);
}

static guint
webkit_editor_table_get_width (EContentEditor *editor,
                               EContentEditorUnit *unit)
{
	return webkit_editor_dialog_utils_get_attribute_with_unit (E_WEBKIT_EDITOR (editor), NULL, "width", 0, unit);
}

static void
webkit_editor_table_set_align (EContentEditor *editor,
                               const gchar *value)
{
	webkit_editor_dialog_utils_set_attribute (E_WEBKIT_EDITOR (editor), NULL, "align", value);
}

static gchar *
webkit_editor_table_get_align (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute (E_WEBKIT_EDITOR (editor), NULL, "align");
}

static void
webkit_editor_table_set_padding (EContentEditor *editor,
                                 gint value)
{
	webkit_editor_dialog_utils_set_attribute_int (E_WEBKIT_EDITOR (editor), NULL, "cellpadding", value);
}

static gint
webkit_editor_table_get_padding (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute_int (E_WEBKIT_EDITOR (editor), NULL, "cellpadding", 0);
}

static void
webkit_editor_table_set_spacing (EContentEditor *editor,
                                 gint value)
{
	webkit_editor_dialog_utils_set_attribute_int (E_WEBKIT_EDITOR (editor), NULL, "cellspacing", value);
}

static gint
webkit_editor_table_get_spacing (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute_int (E_WEBKIT_EDITOR (editor), NULL, "cellspacing", 0);
}

static void
webkit_editor_table_set_border (EContentEditor *editor,
                                gint value)
{
	webkit_editor_dialog_utils_set_attribute_int (E_WEBKIT_EDITOR (editor), NULL, "border", value);
}

static gint
webkit_editor_table_get_border (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute_int (E_WEBKIT_EDITOR (editor), NULL, "border", 0);
}

static void
webkit_editor_table_get_background_color (EContentEditor *editor,
                                          GdkRGBA *color)
{
	webkit_editor_dialog_utils_get_attribute_color (E_WEBKIT_EDITOR (editor), NULL, "bgcolor", color);
}

static void
webkit_editor_table_set_background_color (EContentEditor *editor,
                                          const GdkRGBA *value)
{
	webkit_editor_dialog_utils_set_attribute_color (E_WEBKIT_EDITOR (editor), NULL, "bgcolor", value);
}

static gchar *
webkit_editor_table_get_background_image_uri (EContentEditor *editor)
{
	return webkit_editor_dialog_utils_get_attribute (E_WEBKIT_EDITOR (editor), NULL, "background");
}

static void
webkit_editor_table_set_background_image_uri (EContentEditor *editor,
                                              const gchar *uri)
{
	webkit_editor_replace_image_src (E_WEBKIT_EDITOR (editor), NULL, uri);
}

static gchar *
webkit_editor_spell_check_next_word (EContentEditor *editor,
                                     const gchar *word)
{
	return webkit_editor_extract_and_free_jsc_string (
		webkit_editor_call_jsc_sync (E_WEBKIT_EDITOR (editor), "EvoEditor.SpellCheckContinue(%x,%x);", word && *word, TRUE),
		NULL);
}

static gchar *
webkit_editor_spell_check_prev_word (EContentEditor *editor,
                                     const gchar *word)
{
	return webkit_editor_extract_and_free_jsc_string (
		webkit_editor_call_jsc_sync (E_WEBKIT_EDITOR (editor), "EvoEditor.SpellCheckContinue(%x,%x);", word && *word, FALSE),
		NULL);
}

static void
webkit_editor_uri_request_done_cb (GObject *source_object,
				   GAsyncResult *result,
				   gpointer user_data)
{
	WebKitURISchemeRequest *request = user_data;
	GInputStream *stream = NULL;
	gint64 stream_length = -1;
	gchar *mime_type = NULL;
	GError *error = NULL;

	g_return_if_fail (E_IS_CONTENT_REQUEST (source_object));
	g_return_if_fail (WEBKIT_IS_URI_SCHEME_REQUEST (request));

	if (!e_content_request_process_finish (E_CONTENT_REQUEST (source_object),
		result, &stream, &stream_length, &mime_type, &error)) {
		webkit_uri_scheme_request_finish_error (request, error);
		g_clear_error (&error);
	} else {
		webkit_uri_scheme_request_finish (request, stream, stream_length, mime_type);

		g_clear_object (&stream);
		g_free (mime_type);
	}

	g_object_unref (request);
}

static void
webkit_editor_process_uri_request_cb (WebKitURISchemeRequest *request,
				      gpointer user_data)
{
	WebKitWebView *web_view;
	EWebKitEditor *wk_editor;
	EContentRequest *content_request;
	const gchar *uri, *scheme;

	g_return_if_fail (WEBKIT_IS_URI_SCHEME_REQUEST (request));

	web_view = webkit_uri_scheme_request_get_web_view (request);

	if (!web_view) {
		GError *error;

		error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");
		webkit_uri_scheme_request_finish_error (request, error);
		g_clear_error (&error);

		return;
	}

	wk_editor = E_IS_WEBKIT_EDITOR (web_view) ? E_WEBKIT_EDITOR (web_view) : NULL;

	if (!wk_editor) {
		GError *error;

		error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED, "Unexpected WebView type");
		webkit_uri_scheme_request_finish_error (request, error);
		g_clear_error (&error);

		g_warning ("%s: Unexpected WebView type '%s' received", G_STRFUNC, web_view ? G_OBJECT_TYPE_NAME (web_view) : "null");

		return;
	}

	scheme = webkit_uri_scheme_request_get_scheme (request);
	g_return_if_fail (scheme != NULL);

	content_request = g_hash_table_lookup (wk_editor->priv->scheme_handlers, scheme);

	if (!content_request) {
		g_warning ("%s: Cannot find handler for scheme '%s'", G_STRFUNC, scheme);
		return;
	}

	uri = webkit_uri_scheme_request_get_uri (request);

	g_return_if_fail (e_content_request_can_process_uri (content_request, uri));

	e_content_request_process (content_request, uri, G_OBJECT (web_view), wk_editor ? wk_editor->priv->cancellable : NULL,
		webkit_editor_uri_request_done_cb, g_object_ref (request));
}

static void
webkit_editor_set_normal_paragraph_width (EWebKitEditor *wk_editor,
					  gint value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if (wk_editor->priv->normal_paragraph_width != value) {
		wk_editor->priv->normal_paragraph_width = value;

		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.SetNormalParagraphWidth(%d);",
			value);

		g_object_notify (G_OBJECT (wk_editor), "normal-paragraph-width");
	}
}

static gint
webkit_editor_get_normal_paragraph_width (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), -1);

	return wk_editor->priv->normal_paragraph_width;
}

static void
webkit_editor_set_magic_links (EWebKitEditor *wk_editor,
			       gboolean value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if ((wk_editor->priv->magic_links ? 1 : 0) != (value ? 1 : 0)) {
		wk_editor->priv->magic_links = value;

		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.MAGIC_LINKS = %x;",
			value);

		g_object_notify (G_OBJECT (wk_editor), "magic-links");
	}
}

static gboolean
webkit_editor_get_magic_links (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->magic_links;
}

static void
webkit_editor_set_magic_smileys (EWebKitEditor *wk_editor,
				 gboolean value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if ((wk_editor->priv->magic_smileys ? 1 : 0) != (value ? 1 : 0)) {
		wk_editor->priv->magic_smileys = value;

		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.MAGIC_SMILEYS = %x;",
			value);

		g_object_notify (G_OBJECT (wk_editor), "magic-smileys");
	}
}

static gboolean
webkit_editor_get_magic_smileys (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->magic_smileys;
}

static void
webkit_editor_set_unicode_smileys (EWebKitEditor *wk_editor,
				   gboolean value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if ((wk_editor->priv->unicode_smileys ? 1 : 0) != (value ? 1 : 0)) {
		wk_editor->priv->unicode_smileys = value;

		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.UNICODE_SMILEYS = %x;",
			value);

		g_object_notify (G_OBJECT (wk_editor), "unicode-smileys");
	}
}

static gboolean
webkit_editor_get_unicode_smileys (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->unicode_smileys;
}

static void
webkit_editor_set_wrap_quoted_text_in_replies (EWebKitEditor *wk_editor,
					       gboolean value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if ((wk_editor->priv->wrap_quoted_text_in_replies ? 1 : 0) != (value ? 1 : 0)) {
		wk_editor->priv->wrap_quoted_text_in_replies = value;

		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.WRAP_QUOTED_TEXT_IN_REPLIES = %x;",
			value);

		g_object_notify (G_OBJECT (wk_editor), "wrap-quoted-text-in-replies");
	}
}

static gboolean
webkit_editor_get_wrap_quoted_text_in_replies (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->wrap_quoted_text_in_replies;
}

static gint
webkit_editor_get_minimum_font_size (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), -1);

	return wk_editor->priv->minimum_font_size;
}

static void
webkit_editor_set_minimum_font_size (EWebKitEditor *wk_editor,
				     gint pixels)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if (wk_editor->priv->minimum_font_size != pixels) {
		WebKitSettings *wk_settings;

		wk_editor->priv->minimum_font_size = pixels;

		wk_settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (wk_editor));
		e_web_view_utils_apply_minimum_font_size (wk_settings);

		g_object_notify (G_OBJECT (wk_editor), "minimum-font-size");
	}
}

static void
webkit_editor_set_paste_plain_prefer_pre (EWebKitEditor *wk_editor,
					  gboolean value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if ((wk_editor->priv->paste_plain_prefer_pre ? 1 : 0) != (value ? 1 : 0)) {
		wk_editor->priv->paste_plain_prefer_pre = value;

		g_object_notify (G_OBJECT (wk_editor), "paste-plain-prefer-pre");
	}
}

static gboolean
webkit_editor_get_paste_plain_prefer_pre (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->paste_plain_prefer_pre;
}

static void
webkit_editor_set_link_to_text (EWebKitEditor *wk_editor,
				EHTMLLinkToText value)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if (wk_editor->priv->link_to_text != value) {
		wk_editor->priv->link_to_text = value;

		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.LINK_TO_TEXT = %d;",
			value);

		g_object_notify (G_OBJECT (wk_editor), "link-to-text");
	}
}

static gboolean
webkit_editor_get_link_to_text (EWebKitEditor *wk_editor)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	return wk_editor->priv->link_to_text;
}

static void
webkit_editor_clipboard_owner_changed_cb (GtkClipboard *clipboard,
					  GdkEventOwnerChange *event,
					  gpointer user_data)
{
	gboolean *out_is_from_self = user_data;

	g_return_if_fail (out_is_from_self != NULL);

	*out_is_from_self = event && event->owner && event->reason == GDK_OWNER_CHANGE_NEW_OWNER &&
		gdk_window_get_window_type (event->owner) != GDK_WINDOW_FOREIGN;
}

static gboolean wk_editor_clipboard_owner_is_from_self = FALSE;
static gboolean wk_editor_primary_clipboard_owner_is_from_self = FALSE;

static void
wk_editor_change_existing_instances (gint inc)
{
	static gulong owner_change_primary_clipboard_cb_id = 0;
	static gulong owner_change_clipboard_cb_id = 0;
	static gint instances = 0;

	instances += inc;

	g_return_if_fail (instances >= 0);

	if (instances == 1 && inc > 0) {
		g_return_if_fail (!owner_change_clipboard_cb_id);
		g_return_if_fail (!owner_change_primary_clipboard_cb_id);

		owner_change_clipboard_cb_id = g_signal_connect (
			gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), "owner-change",
			G_CALLBACK (webkit_editor_clipboard_owner_changed_cb), &wk_editor_clipboard_owner_is_from_self);

		owner_change_primary_clipboard_cb_id = g_signal_connect (
			gtk_clipboard_get (GDK_SELECTION_PRIMARY), "owner-change",
			G_CALLBACK (webkit_editor_clipboard_owner_changed_cb), &wk_editor_primary_clipboard_owner_is_from_self);

		wk_editor_clipboard_owner_is_from_self = FALSE;
		wk_editor_primary_clipboard_owner_is_from_self = FALSE;
	} else if (instances == 0 && inc < 0) {
		if (owner_change_clipboard_cb_id > 0) {
			g_signal_handler_disconnect (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), owner_change_clipboard_cb_id);
			owner_change_clipboard_cb_id = 0;
		}

		if (owner_change_primary_clipboard_cb_id > 0) {
			g_signal_handler_disconnect (gtk_clipboard_get (GDK_SELECTION_PRIMARY), owner_change_primary_clipboard_cb_id);
			owner_change_primary_clipboard_cb_id = 0;
		}
	}
}

static void
e_webkit_editor_initialize_web_extensions_cb (WebKitWebContext *web_context,
					      gpointer user_data)
{
	EWebKitEditor *wk_editor = user_data;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	webkit_web_context_set_web_extensions_directory (web_context, EVOLUTION_WEB_EXTENSIONS_WEBKIT_EDITOR_DIR);
}

static void
webkit_editor_constructed (GObject *object)
{
	EWebKitEditor *wk_editor;
	EContentRequest *content_request;
	gchar **languages;
	WebKitWebContext *web_context;
	WebKitSettings *web_settings;
	WebKitWebView *web_view;
	WebKitUserContentManager *manager;
	GSettings *settings;

	wk_editor = E_WEBKIT_EDITOR (object);
	web_view = WEBKIT_WEB_VIEW (wk_editor);

	web_context = webkit_web_view_get_context (web_view);

	/* Do this before calling the parent's constructed(), because
	   that can emit the web_context's signal */
	g_signal_connect_object (web_context, "initialize-web-extensions",
		G_CALLBACK (e_webkit_editor_initialize_web_extensions_cb), wk_editor, 0);

	G_OBJECT_CLASS (e_webkit_editor_parent_class)->constructed (object);

	manager = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (wk_editor));

	g_signal_connect_object (manager, "script-message-received::contentChanged",
		G_CALLBACK (content_changed_cb), wk_editor, 0);
	g_signal_connect_object (manager, "script-message-received::contextMenuRequested",
		G_CALLBACK (context_menu_requested_cb), wk_editor, 0);
	g_signal_connect_object (manager, "script-message-received::formattingChanged",
		G_CALLBACK (formatting_changed_cb), wk_editor, 0);
	g_signal_connect_object (manager, "script-message-received::selectionChanged",
		G_CALLBACK (selection_changed_cb), wk_editor, 0);
	g_signal_connect_object (manager, "script-message-received::undoRedoStateChanged",
		G_CALLBACK (undu_redo_state_changed_cb), wk_editor, 0);

	webkit_user_content_manager_register_script_message_handler (manager, "contentChanged");
	webkit_user_content_manager_register_script_message_handler (manager, "contextMenuRequested");
	webkit_user_content_manager_register_script_message_handler (manager, "formattingChanged");
	webkit_user_content_manager_register_script_message_handler (manager, "selectionChanged");
	webkit_user_content_manager_register_script_message_handler (manager, "undoRedoStateChanged");

	/* Give spell check languages to WebKit */
	languages = e_spell_checker_list_active_languages (wk_editor->priv->spell_checker, NULL);

	webkit_web_context_set_spell_checking_enabled (web_context, TRUE);
	webkit_web_context_set_spell_checking_languages (web_context, (const gchar * const *) languages);
	g_strfreev (languages);

	/* When adding new scheme handlers add them also into webkit_editor_constructor() */
	g_hash_table_insert (wk_editor->priv->scheme_handlers, (gpointer) "cid", e_cid_request_new ());
	g_hash_table_insert (wk_editor->priv->scheme_handlers, (gpointer) "evo-file", e_file_request_new ());

	content_request = e_http_request_new ();
	g_hash_table_insert (wk_editor->priv->scheme_handlers, (gpointer) "evo-http", g_object_ref (content_request));
	g_hash_table_insert (wk_editor->priv->scheme_handlers, (gpointer) "evo-https", g_object_ref (content_request));
	g_object_unref (content_request);

	webkit_web_view_set_editable (web_view, TRUE);

	web_settings = webkit_web_view_get_settings (web_view);
	webkit_settings_set_allow_file_access_from_file_urls (web_settings, TRUE);
	webkit_settings_set_enable_write_console_messages_to_stdout (web_settings, e_util_get_webkit_developer_mode_enabled ());
	webkit_settings_set_enable_developer_extras (web_settings, e_util_get_webkit_developer_mode_enabled ());

	e_web_view_utils_apply_minimum_font_size (web_settings);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	g_settings_bind (
		settings, "composer-word-wrap-length",
		wk_editor, "normal-paragraph-width",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "composer-magic-links",
		wk_editor, "magic-links",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "composer-magic-smileys",
		wk_editor, "magic-smileys",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "composer-unicode-smileys",
		wk_editor, "unicode-smileys",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "composer-wrap-quoted-text-in-replies",
		wk_editor, "wrap-quoted-text-in-replies",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "composer-paste-plain-prefer-pre",
		wk_editor, "paste-plain-prefer-pre",
		G_SETTINGS_BIND_GET);

	g_settings_bind (settings, "html-link-to-text",
		wk_editor, "link-to-text",
		G_SETTINGS_BIND_GET);

	g_object_unref (settings);

	settings = e_util_ref_settings ("org.gnome.evolution.shell");

	g_settings_bind (
		settings, "webkit-minimum-font-size",
		wk_editor, "minimum-font-size",
		G_SETTINGS_BIND_GET);

	g_clear_object (&settings);

	webkit_web_view_load_html (WEBKIT_WEB_VIEW (wk_editor), "", "evo-file:///");
}

static GObjectConstructParam*
find_property (guint n_properties,
               GObjectConstructParam* properties,
               GParamSpec* param_spec)
{
	while (n_properties--) {
		if (properties->pspec == param_spec)
			return properties;
		properties++;
	}

	return NULL;
}

static GObject *
webkit_editor_constructor (GType type,
                           guint n_construct_properties,
                           GObjectConstructParam *construct_properties)
{
	GObjectClass* object_class;
	GParamSpec* param_spec;
	GObjectConstructParam *param = NULL;

	object_class = G_OBJECT_CLASS (g_type_class_ref (type));
	g_return_val_if_fail (object_class != NULL, NULL);

	if (construct_properties && n_construct_properties != 0) {
		param_spec = g_object_class_find_property (object_class, "settings");
		if ((param = find_property (n_construct_properties, construct_properties, param_spec)))
			g_value_take_object (param->value, e_web_view_get_default_webkit_settings ());
		param_spec = g_object_class_find_property (object_class, "user-content-manager");
		if ((param = find_property (n_construct_properties, construct_properties, param_spec)))
			g_value_take_object (param->value, webkit_user_content_manager_new ());
		param_spec = g_object_class_find_property (object_class, "web-context");
		if ((param = find_property (n_construct_properties, construct_properties, param_spec))) {
			/* Share one web_context between all editors, thus there is one WebProcess
			   for all the editors (and one for the preview). */
			static gpointer web_context = NULL;

			if (!web_context) {
				#ifdef ENABLE_MAINTAINER_MODE
				const gchar *source_webkitdatadir;
				#endif
				gchar *plugins_path;
				const gchar *schemes[] = {
					"cid",
					"evo-file",
					"evo-http",
					"evo-https"
				};
				gint ii;

				web_context = webkit_web_context_new ();

				webkit_web_context_set_cache_model (web_context, WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);
				webkit_web_context_set_web_extensions_directory (web_context, EVOLUTION_WEB_EXTENSIONS_WEBKIT_EDITOR_DIR);
				webkit_web_context_set_sandbox_enabled (web_context, TRUE);
				webkit_web_context_add_path_to_sandbox (web_context, EVOLUTION_WEBKITDATADIR, TRUE);

				plugins_path = g_build_filename (e_get_user_data_dir (), "webkit-editor-plugins", NULL);
				if (g_file_test (plugins_path, G_FILE_TEST_IS_DIR))
					webkit_web_context_add_path_to_sandbox (web_context, plugins_path, TRUE);
				g_free (plugins_path);

				#ifdef ENABLE_MAINTAINER_MODE
				source_webkitdatadir = g_getenv ("EVOLUTION_SOURCE_WEBKITDATADIR");
				if (source_webkitdatadir && *source_webkitdatadir)
					webkit_web_context_add_path_to_sandbox (web_context, source_webkitdatadir, TRUE);
				#endif

				g_object_add_weak_pointer (G_OBJECT (web_context), &web_context);

				for (ii = 0; ii < G_N_ELEMENTS (schemes); ii++) {
					webkit_web_context_register_uri_scheme (web_context, schemes[ii], webkit_editor_process_uri_request_cb, NULL, NULL);
				}
			} else {
				g_object_ref (web_context);
			}

			g_value_take_object (param->value, web_context);
		}
	}

	g_type_class_unref (object_class);

	return G_OBJECT_CLASS (e_webkit_editor_parent_class)->constructor (type, n_construct_properties, construct_properties);
}

static void
webkit_editor_dispose (GObject *object)
{
	EWebKitEditor *self = E_WEBKIT_EDITOR (object);

	if (self->priv->cancellable)
		g_cancellable_cancel (self->priv->cancellable);

	g_clear_pointer (&self->priv->current_user_stylesheet, g_free);

	if (self->priv->font_settings != NULL) {
		g_signal_handlers_disconnect_by_data (self->priv->font_settings, object);
		g_clear_object (&self->priv->font_settings);
	}

	if (self->priv->mail_settings != NULL) {
		g_signal_handlers_disconnect_by_data (self->priv->mail_settings, object);
		g_clear_object (&self->priv->mail_settings);
	}

	webkit_editor_finish_search (self);

	g_hash_table_remove_all (self->priv->scheme_handlers);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_webkit_editor_parent_class)->dispose (object);
}

static void
webkit_editor_finalize (GObject *object)
{
	EWebKitEditor *self = E_WEBKIT_EDITOR (object);

	g_clear_pointer (&self->priv->old_settings, g_hash_table_destroy);

	if (self->priv->post_reload_operations) {
		g_warn_if_fail (g_queue_is_empty (self->priv->post_reload_operations));

		g_queue_free (self->priv->post_reload_operations);
		self->priv->post_reload_operations = NULL;
	}

	g_clear_pointer (&self->priv->background_color, gdk_rgba_free);
	g_clear_pointer (&self->priv->font_color, gdk_rgba_free);
	g_clear_pointer (&self->priv->body_fg_color, gdk_rgba_free);
	g_clear_pointer (&self->priv->body_bg_color, gdk_rgba_free);
	g_clear_pointer (&self->priv->body_link_color, gdk_rgba_free);
	g_clear_pointer (&self->priv->body_vlink_color, gdk_rgba_free);

	g_clear_pointer (&self->priv->last_hover_uri, g_free);

	g_clear_object (&self->priv->spell_checker);
	g_clear_object (&self->priv->cancellable);
	g_clear_error (&self->priv->last_error);

	g_free (self->priv->body_font_name);
	g_free (self->priv->font_name);
	g_free (self->priv->context_menu_caret_word);

	g_hash_table_destroy (self->priv->scheme_handlers);

	wk_editor_change_existing_instances (-1);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_webkit_editor_parent_class)->finalize (object);
}

static void
webkit_editor_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHANGED:
			webkit_editor_set_changed (
				E_WEBKIT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_EDITABLE:
			webkit_editor_set_editable (
				E_WEBKIT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_MODE:
			webkit_editor_set_mode (
				E_WEBKIT_EDITOR (object),
				g_value_get_enum (value));
			return;

		case PROP_NORMAL_PARAGRAPH_WIDTH:
			webkit_editor_set_normal_paragraph_width (
				E_WEBKIT_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_MAGIC_LINKS:
			webkit_editor_set_magic_links (
				E_WEBKIT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_MAGIC_SMILEYS:
			webkit_editor_set_magic_smileys (
				E_WEBKIT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_UNICODE_SMILEYS:
			webkit_editor_set_unicode_smileys (
				E_WEBKIT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_WRAP_QUOTED_TEXT_IN_REPLIES:
			webkit_editor_set_wrap_quoted_text_in_replies (
				E_WEBKIT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_ALIGNMENT:
			webkit_editor_set_alignment (
				E_WEBKIT_EDITOR (object),
				g_value_get_enum (value));
			return;

		case PROP_BACKGROUND_COLOR:
			webkit_editor_set_background_color (
				E_WEBKIT_EDITOR (object),
				g_value_get_boxed (value));
			return;

		case PROP_BOLD:
			webkit_editor_set_style_flag (
				E_WEBKIT_EDITOR (object),
				E_WEBKIT_EDITOR_STYLE_IS_BOLD,
				g_value_get_boolean (value));
			return;

		case PROP_FONT_COLOR:
			webkit_editor_set_font_color (
				E_WEBKIT_EDITOR (object),
				g_value_get_boxed (value));
			return;

		case PROP_BLOCK_FORMAT:
			webkit_editor_set_block_format (
				E_WEBKIT_EDITOR (object),
				g_value_get_enum (value));
			return;

		case PROP_FONT_NAME:
			webkit_editor_set_font_name (
				E_WEBKIT_EDITOR (object),
				g_value_get_string (value));
			return;

		case PROP_FONT_SIZE:
			webkit_editor_set_font_size (
				E_WEBKIT_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_ITALIC:
			webkit_editor_set_style_flag (
				E_WEBKIT_EDITOR (object),
				E_WEBKIT_EDITOR_STYLE_IS_ITALIC,
				g_value_get_boolean (value));
			return;

		case PROP_STRIKETHROUGH:
			webkit_editor_set_style_flag (
				E_WEBKIT_EDITOR (object),
				E_WEBKIT_EDITOR_STYLE_IS_STRIKETHROUGH,
				g_value_get_boolean (value));
			return;

		case PROP_SUBSCRIPT:
			webkit_editor_set_style_flag (
				E_WEBKIT_EDITOR (object),
				E_WEBKIT_EDITOR_STYLE_IS_SUBSCRIPT,
				g_value_get_boolean (value));
			return;

		case PROP_SUPERSCRIPT:
			webkit_editor_set_style_flag (
				E_WEBKIT_EDITOR (object),
				E_WEBKIT_EDITOR_STYLE_IS_SUPERSCRIPT,
				g_value_get_boolean (value));
			return;

		case PROP_UNDERLINE:
			webkit_editor_set_style_flag (
				E_WEBKIT_EDITOR (object),
				E_WEBKIT_EDITOR_STYLE_IS_UNDERLINE,
				g_value_get_boolean (value));
			return;

		case PROP_START_BOTTOM:
			webkit_editor_set_start_bottom (
				E_WEBKIT_EDITOR (object),
				g_value_get_enum (value));
			return;

		case PROP_TOP_SIGNATURE:
			webkit_editor_set_top_signature (
				E_WEBKIT_EDITOR (object),
				g_value_get_enum (value));
			return;

		case PROP_SPELL_CHECK_ENABLED:
			webkit_editor_set_spell_check_enabled (
				E_WEBKIT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_VISUALLY_WRAP_LONG_LINES:
			webkit_editor_set_visually_wrap_long_lines (
				E_WEBKIT_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_LAST_ERROR:
			webkit_editor_set_last_error (
				E_WEBKIT_EDITOR (object),
				g_value_get_boxed (value));
			return;

		case PROP_MINIMUM_FONT_SIZE:
			webkit_editor_set_minimum_font_size (
				E_WEBKIT_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_PASTE_PLAIN_PREFER_PRE:
			webkit_editor_set_paste_plain_prefer_pre (
				E_WEBKIT_EDITOR (object),
				g_value_get_boolean (value));
			return;
		case PROP_LINK_TO_TEXT:
			webkit_editor_set_link_to_text (
				E_WEBKIT_EDITOR (object),
				g_value_get_enum (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
webkit_editor_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_IS_MALFUNCTION:
			g_value_set_boolean (
				value, webkit_editor_is_malfunction (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_CAN_COPY:
			g_value_set_boolean (
				value, webkit_editor_can_copy (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_CAN_CUT:
			g_value_set_boolean (
				value, webkit_editor_can_cut (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_CAN_PASTE:
			g_value_set_boolean (
				value, webkit_editor_can_paste (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_CAN_REDO:
			g_value_set_boolean (
				value, webkit_editor_can_redo (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_CAN_UNDO:
			g_value_set_boolean (
				value, webkit_editor_can_undo (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_CHANGED:
			g_value_set_boolean (
				value, webkit_editor_get_changed (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_MODE:
			g_value_set_enum (
				value, webkit_editor_get_mode (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_EDITABLE:
			g_value_set_boolean (
				value, webkit_editor_is_editable (
				E_WEBKIT_EDITOR (object)));
			return;

		case PROP_NORMAL_PARAGRAPH_WIDTH:
			g_value_set_int (value,
				webkit_editor_get_normal_paragraph_width (E_WEBKIT_EDITOR (object)));
			return;

		case PROP_MAGIC_LINKS:
			g_value_set_boolean (value,
				webkit_editor_get_magic_links (E_WEBKIT_EDITOR (object)));
			return;

		case PROP_MAGIC_SMILEYS:
			g_value_set_boolean (value,
				webkit_editor_get_magic_smileys (E_WEBKIT_EDITOR (object)));
			return;

		case PROP_UNICODE_SMILEYS:
			g_value_set_boolean (value,
				webkit_editor_get_unicode_smileys (E_WEBKIT_EDITOR (object)));
			return;

		case PROP_WRAP_QUOTED_TEXT_IN_REPLIES:
			g_value_set_boolean (value,
				webkit_editor_get_wrap_quoted_text_in_replies (E_WEBKIT_EDITOR (object)));
			return;

		case PROP_ALIGNMENT:
			g_value_set_enum (
				value,
				webkit_editor_get_alignment (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_BACKGROUND_COLOR:
			g_value_set_boxed (
				value,
				webkit_editor_get_background_color (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_BLOCK_FORMAT:
			g_value_set_enum (
				value,
				webkit_editor_get_block_format (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_BOLD:
			g_value_set_boolean (
				value,
				webkit_editor_get_style_flag (
					E_WEBKIT_EDITOR (object),
					E_WEBKIT_EDITOR_STYLE_IS_BOLD));
			return;

		case PROP_FONT_COLOR:
			g_value_set_boxed (
				value,
				webkit_editor_get_font_color (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_FONT_NAME:
			g_value_set_string (
				value,
				webkit_editor_get_font_name (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_FONT_SIZE:
			g_value_set_int (
				value,
				webkit_editor_get_font_size (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_INDENT_LEVEL:
			g_value_set_int (
				value,
				webkit_editor_get_indent_level (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_ITALIC:
			g_value_set_boolean (
				value,
				webkit_editor_get_style_flag (
					E_WEBKIT_EDITOR (object),
					E_WEBKIT_EDITOR_STYLE_IS_ITALIC));
			return;

		case PROP_STRIKETHROUGH:
			g_value_set_boolean (
				value,
				webkit_editor_get_style_flag (
					E_WEBKIT_EDITOR (object),
					E_WEBKIT_EDITOR_STYLE_IS_STRIKETHROUGH));
			return;

		case PROP_SUBSCRIPT:
			g_value_set_boolean (
				value,
				webkit_editor_get_style_flag (
					E_WEBKIT_EDITOR (object),
					E_WEBKIT_EDITOR_STYLE_IS_SUBSCRIPT));
			return;

		case PROP_SUPERSCRIPT:
			g_value_set_boolean (
				value,
				webkit_editor_get_style_flag (
					E_WEBKIT_EDITOR (object),
					E_WEBKIT_EDITOR_STYLE_IS_SUPERSCRIPT));
			return;

		case PROP_UNDERLINE:
			g_value_set_boolean (
				value,
				webkit_editor_get_style_flag (
					E_WEBKIT_EDITOR (object),
					E_WEBKIT_EDITOR_STYLE_IS_UNDERLINE));
			return;

		case PROP_START_BOTTOM:
			g_value_set_enum (
				value,
				webkit_editor_get_start_bottom (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_TOP_SIGNATURE:
			g_value_set_enum (
				value,
				webkit_editor_get_top_signature (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_SPELL_CHECK_ENABLED:
			g_value_set_boolean (
				value,
				webkit_editor_get_spell_check_enabled (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_SPELL_CHECKER:
			g_value_set_object (
				value,
				webkit_editor_get_spell_checker (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_VISUALLY_WRAP_LONG_LINES:
			g_value_set_boolean (
				value,
				webkit_editor_get_visually_wrap_long_lines (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_LAST_ERROR:
			g_value_set_boxed (
				value,
				webkit_editor_get_last_error (
					E_WEBKIT_EDITOR (object)));
			return;

		case PROP_MINIMUM_FONT_SIZE:
			g_value_set_int (value,
				webkit_editor_get_minimum_font_size (E_WEBKIT_EDITOR (object)));
			return;

		case PROP_PASTE_PLAIN_PREFER_PRE:
			g_value_set_boolean (value,
				webkit_editor_get_paste_plain_prefer_pre (E_WEBKIT_EDITOR (object)));
			return;

		case PROP_LINK_TO_TEXT:
			g_value_set_enum (value,
				webkit_editor_get_link_to_text (E_WEBKIT_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
webkit_editor_move_caret_on_current_coordinates (GtkWidget *widget)
{
	gint x, y;
	GdkDeviceManager *device_manager;
	GdkDevice *pointer;

	device_manager = gdk_display_get_device_manager (gtk_widget_get_display (widget));
	pointer = gdk_device_manager_get_client_pointer (device_manager);
	gdk_window_get_device_position (
		gtk_widget_get_window (widget), pointer, &x, &y, NULL);
	webkit_editor_move_caret_on_coordinates
		(E_CONTENT_EDITOR (widget), x, y, FALSE);
}

static void
webkit_editor_settings_changed_cb (GSettings *settings,
                                   const gchar *key,
                                   EWebKitEditor *wk_editor)
{
	GVariant *new_value, *old_value;

	new_value = g_settings_get_value (settings, key);
	old_value = g_hash_table_lookup (wk_editor->priv->old_settings, key);

	if (!new_value || !old_value || !g_variant_equal (new_value, old_value)) {
		if (new_value)
			g_hash_table_insert (wk_editor->priv->old_settings, g_strdup (key), new_value);
		else
			g_hash_table_remove (wk_editor->priv->old_settings, key);

		webkit_editor_update_styles (E_CONTENT_EDITOR (wk_editor));
	} else if (new_value) {
		g_variant_unref (new_value);
	}
}

static void
webkit_editor_style_settings_changed_cb (GSettings *settings,
					 const gchar *key,
					 EWebKitEditor *wk_editor)
{
	GVariant *new_value, *old_value;

	new_value = g_settings_get_value (settings, key);
	old_value = g_hash_table_lookup (wk_editor->priv->old_settings, key);

	if (!new_value || !old_value || !g_variant_equal (new_value, old_value)) {
		if (new_value)
			g_hash_table_insert (wk_editor->priv->old_settings, g_strdup (key), new_value);
		else
			g_hash_table_remove (wk_editor->priv->old_settings, key);

		webkit_editor_style_updated (wk_editor, FALSE);
	} else if (new_value) {
		g_variant_unref (new_value);
	}
}

static void
webkit_editor_can_paste_cb (GObject *source_object,
			    GAsyncResult *result,
			    gpointer user_data)
{
	EWebKitEditor *wk_editor;
	gboolean can;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (source_object));

	wk_editor = E_WEBKIT_EDITOR (source_object);

	can = webkit_web_view_can_execute_editing_command_finish (WEBKIT_WEB_VIEW (wk_editor), result, NULL);

	if (wk_editor->priv->can_paste != can) {
		wk_editor->priv->can_paste = can;
		g_object_notify (G_OBJECT (wk_editor), "can-paste");
	}
}

static void
webkit_editor_load_changed_cb (EWebKitEditor *wk_editor,
                               WebKitLoadEvent load_event)
{
	wk_editor->priv->webkit_load_event = load_event;

	if (load_event != WEBKIT_LOAD_FINISHED ||
	    !webkit_editor_is_ready (E_CONTENT_EDITOR (wk_editor)))
		return;

	e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
		"EvoEditor.NORMAL_PARAGRAPH_WIDTH = %d;"
		"EvoEditor.START_BOTTOM = %x;"
		"EvoEditor.MAGIC_LINKS = %x;"
		"EvoEditor.MAGIC_SMILEYS = %x;"
		"EvoEditor.UNICODE_SMILEYS = %x;"
		"EvoEditor.WRAP_QUOTED_TEXT_IN_REPLIES = %x;"
		"EvoEditor.LINK_TO_TEXT = %d;",
		wk_editor->priv->normal_paragraph_width,
		e_content_editor_util_three_state_to_bool (wk_editor->priv->start_bottom, "composer-reply-start-bottom"),
		wk_editor->priv->magic_links,
		wk_editor->priv->magic_smileys,
		wk_editor->priv->unicode_smileys,
		wk_editor->priv->wrap_quoted_text_in_replies,
		wk_editor->priv->link_to_text);

	/* Dispatch queued operations - as we are using this just for load
	 * operations load just the latest request and throw away the rest. */
	if (wk_editor->priv->post_reload_operations &&
	    !g_queue_is_empty (wk_editor->priv->post_reload_operations)) {

		PostReloadOperation *op;

		op = g_queue_pop_head (wk_editor->priv->post_reload_operations);

		op->func (wk_editor, op->data, op->flags);

		if (op->data_free_func)
			op->data_free_func (op->data);
		g_free (op);

		while ((op = g_queue_pop_head (wk_editor->priv->post_reload_operations))) {
			if (op->data_free_func)
				op->data_free_func (op->data);
			g_free (op);
		}

		g_queue_clear (wk_editor->priv->post_reload_operations);
	}

	webkit_editor_style_updated (wk_editor, FALSE);

	if (wk_editor->priv->initialized_callback) {
		EContentEditorInitializedCallback initialized_callback;
		gpointer initialized_user_data;

		initialized_callback = wk_editor->priv->initialized_callback;
		initialized_user_data = wk_editor->priv->initialized_user_data;

		wk_editor->priv->initialized_callback = NULL;
		wk_editor->priv->initialized_user_data = NULL;

		initialized_callback (E_CONTENT_EDITOR (wk_editor), initialized_user_data);
	}

	webkit_web_view_can_execute_editing_command (WEBKIT_WEB_VIEW (wk_editor),
		WEBKIT_EDITING_COMMAND_PASTE, NULL, webkit_editor_can_paste_cb, NULL);

	e_content_editor_emit_load_finished (E_CONTENT_EDITOR (wk_editor));
}

static gboolean
is_libreoffice_content (GdkAtom *targets,
			gint n_targets)
{
	struct _prefixes {
		const gchar *prefix;
		gint len;
	} prefixes[] = {
		{ "application/x-openoffice", 0 },
		{ "application/x-libreoffice", 0 }
	};
	gint ii, jj;

	for (ii = 0; ii < n_targets; ii++) {
		gchar *name = gdk_atom_name (targets[ii]);

		if (!name)
			continue;

		for (jj = 0; jj < G_N_ELEMENTS (prefixes); jj++) {
			if (!prefixes[jj].len)
				prefixes[jj].len = strlen (prefixes[jj].prefix);
			if (g_ascii_strncasecmp (name, prefixes[jj].prefix, prefixes[jj].len) == 0)
				break;
		}

		g_free (name);

		if (jj < G_N_ELEMENTS (prefixes))
			break;
	}

	return ii < n_targets;
}

static void
webkit_editor_paste_clipboard_targets_cb (GtkClipboard *clipboard,
                                          GdkAtom *targets,
                                          gint n_targets,
					  gboolean is_from_self,
					  gboolean is_primary_paste,
                                          gpointer user_data)
{
	EWebKitEditor *wk_editor = user_data;
	gchar *content = NULL;
	gboolean is_html = FALSE;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	if (targets == NULL || n_targets < 0)
		return;

	/* If view doesn't have focus, focus it */
	if (!gtk_widget_has_focus (GTK_WIDGET (wk_editor)))
		gtk_widget_grab_focus (GTK_WIDGET (wk_editor));

	/* Save the text content before we try to insert the image as it could
	 * happen that we fail to save the image from clipboard (not by our
	 * fault - right now it looks like current web engines can't handle IMG
	 * with SRCSET attribute in clipboard correctly). And if this fails the
	 * source application can cancel the content and we could not fallback
	 * to at least some content. */
	if (wk_editor->priv->mode == E_CONTENT_EDITOR_MODE_HTML) {
		if (e_targets_include_html (targets, n_targets)) {
			content = e_clipboard_wait_for_html (clipboard);
			is_html = TRUE;
		} else if (gtk_targets_include_text (targets, n_targets))
			content = gtk_clipboard_wait_for_text (clipboard);
	} else {
		if (gtk_targets_include_text (targets, n_targets))
			content = gtk_clipboard_wait_for_text (clipboard);
		else if (e_targets_include_html (targets, n_targets)) {
			content = e_clipboard_wait_for_html (clipboard);
			is_html = TRUE;
		}
	}

	if (wk_editor->priv->mode == E_CONTENT_EDITOR_MODE_HTML &&
	    gtk_targets_include_image (targets, n_targets, TRUE) &&
	    (!content || !*content || !is_libreoffice_content (targets, n_targets))) {
		gchar *uri;

		if (!(uri = e_util_save_image_from_clipboard (clipboard)))
			goto fallback;

		webkit_editor_set_changed (wk_editor, TRUE);

		webkit_editor_insert_image (E_CONTENT_EDITOR (wk_editor), uri);

		g_free (content);
		g_free (uri);

		return;
	}

 fallback:

	if (!content || !*content) {
		g_free (content);
		if (is_primary_paste)
			e_content_editor_emit_paste_primary_clipboard (E_CONTENT_EDITOR (wk_editor));
		else
			e_content_editor_emit_paste_clipboard (E_CONTENT_EDITOR (wk_editor));
		return;
	}

	if (is_html) {
		if (is_from_self) {
			gchar *paste_content;

			paste_content = g_strconcat ("<meta name=\"x-evolution-is-paste\">", content, NULL);

			webkit_editor_insert_content (
				E_CONTENT_EDITOR (wk_editor),
				paste_content,
				E_CONTENT_EDITOR_INSERT_TEXT_HTML);

			g_free (paste_content);
		} else {
			webkit_editor_insert_content (
				E_CONTENT_EDITOR (wk_editor),
				content,
				E_CONTENT_EDITOR_INSERT_TEXT_HTML);
		}
	} else {
		webkit_editor_insert_content (
			E_CONTENT_EDITOR (wk_editor),
			content,
			E_CONTENT_EDITOR_INSERT_TEXT_PLAIN |
			E_CONTENT_EDITOR_INSERT_CONVERT |
			(wk_editor->priv->paste_plain_prefer_pre ? E_CONTENT_EDITOR_INSERT_CONVERT_PREFER_PRE : 0));
	}

	g_free (content);
}

static void
webkit_editor_paste_primary (EContentEditor *editor)
{

	GtkClipboard *clipboard;
	GdkAtom *targets = NULL;
	gint n_targets;
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	webkit_editor_move_caret_on_current_coordinates (GTK_WIDGET (wk_editor));

	clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);

	if (gtk_clipboard_wait_for_targets (clipboard, &targets, &n_targets)) {
		webkit_editor_paste_clipboard_targets_cb (clipboard, targets, n_targets, wk_editor_primary_clipboard_owner_is_from_self, TRUE, wk_editor);
		g_free (targets);
	}
}

static void
webkit_editor_paste (EContentEditor *editor)
{
	GtkClipboard *clipboard;
	GdkAtom *targets = NULL;
	gint n_targets;
	EWebKitEditor *wk_editor;

	wk_editor = E_WEBKIT_EDITOR (editor);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	if (gtk_clipboard_wait_for_targets (clipboard, &targets, &n_targets)) {
		webkit_editor_paste_clipboard_targets_cb (clipboard, targets, n_targets, wk_editor_clipboard_owner_is_from_self, FALSE, wk_editor);
		g_free (targets);
	}
}

static const gchar *
webkit_editor_sanitize_link_uri (const gchar *link_uri)
{
	if (!link_uri)
		return link_uri;

	/* these might be in-document links, like '#top' */
	if (g_str_has_prefix (link_uri, "evo-file:///"))
		return link_uri + 12;

	return link_uri;
}

static void
webkit_editor_mouse_target_changed_cb (EWebKitEditor *wk_editor,
                                       WebKitHitTestResult *hit_test_result,
                                       guint modifiers,
                                       gpointer user_data)
{
	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	g_clear_pointer (&wk_editor->priv->last_hover_uri, g_free);

	if (webkit_hit_test_result_context_is_link (hit_test_result)) {
		if (wk_editor->priv->mode == E_CONTENT_EDITOR_MODE_HTML)
			wk_editor->priv->last_hover_uri = g_strdup (webkit_editor_sanitize_link_uri (webkit_hit_test_result_get_link_uri (hit_test_result)));
		else
			wk_editor->priv->last_hover_uri = g_strdup (webkit_hit_test_result_get_link_label (hit_test_result));
	}
}

static gboolean
webkit_editor_context_menu_cb (EWebKitEditor *wk_editor,
                               WebKitContextMenu *context_menu,
                               GdkEvent *event,
                               WebKitHitTestResult *hit_test_result)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (wk_editor), FALSE);

	e_content_editor_emit_context_menu_requested (E_CONTENT_EDITOR (wk_editor),
		wk_editor->priv->context_menu_node_flags,
		wk_editor->priv->context_menu_caret_word,
		event);

	wk_editor->priv->context_menu_node_flags = E_CONTENT_EDITOR_NODE_UNKNOWN;
	g_clear_pointer (&wk_editor->priv->context_menu_caret_word, g_free);

	return TRUE;
}

static void
webkit_editor_drag_begin_cb (EWebKitEditor *wk_editor,
                             GdkDragContext *context)
{
	wk_editor->priv->performing_drag = TRUE;
}

static void
webkit_editor_drag_failed_cb (EWebKitEditor *wk_editor,
                              GdkDragContext *context,
                              GtkDragResult result)
{
	wk_editor->priv->performing_drag = FALSE;
}

static void
webkit_editor_drag_end_cb (EWebKitEditor *wk_editor,
                           GdkDragContext *context)
{
	wk_editor->priv->performing_drag = FALSE;
}

static void
webkit_editor_drag_data_received_cb (GtkWidget *widget,
                                     GdkDragContext *context,
                                     gint x,
                                     gint y,
                                     GtkSelectionData *selection,
                                     guint info,
                                     guint time)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (widget);
	gboolean is_move = FALSE;

	g_signal_handler_disconnect (wk_editor, wk_editor->priv->drag_data_received_handler_id);
	wk_editor->priv->drag_data_received_handler_id = 0;

	is_move = gdk_drag_context_get_selected_action(context) == GDK_ACTION_MOVE;

	/* Leave DnD inside the view on WebKit */
	/* Leave the text on WebKit to handle it. */
	if (wk_editor->priv->performing_drag ||
	    info == E_DND_TARGET_TYPE_UTF8_STRING || info == E_DND_TARGET_TYPE_STRING ||
	    info == E_DND_TARGET_TYPE_TEXT_PLAIN || info == E_DND_TARGET_TYPE_TEXT_PLAIN_UTF8) {
		gdk_drag_status (context, gdk_drag_context_get_selected_action(context), time);
		if (!GTK_WIDGET_CLASS (e_webkit_editor_parent_class)->drag_drop ||
		    !GTK_WIDGET_CLASS (e_webkit_editor_parent_class)->drag_drop (widget, context, x, y, time)) {
			goto process_ourselves;
		} else {
			if (GTK_WIDGET_CLASS (e_webkit_editor_parent_class)->drag_leave)
				GTK_WIDGET_CLASS (e_webkit_editor_parent_class)->drag_leave (widget, context, time);
			g_signal_stop_emission_by_name (widget, "drag-data-received");
			e_content_editor_emit_drop_handled (E_CONTENT_EDITOR (widget));
		}
		return;
	}

	if (info == E_DND_TARGET_TYPE_TEXT_HTML) {
		const guchar *data;
		gint length;
		gint list_len, len;
		gchar *text;

 process_ourselves:
		data = gtk_selection_data_get_data (selection);
		length = gtk_selection_data_get_length (selection);

		if (!data || length < 0) {
			gtk_drag_finish (context, FALSE, is_move, time);
			g_signal_stop_emission_by_name (widget, "drag-data-received");
			return;
		}

		webkit_editor_move_caret_on_coordinates (E_CONTENT_EDITOR (widget), x, y, FALSE);

		list_len = length;
		do {
			text = e_util_next_uri_from_uri_list ((guchar **) &data, &len, &list_len);
			webkit_editor_insert_content (
				E_CONTENT_EDITOR (wk_editor),
				text,
				E_CONTENT_EDITOR_INSERT_TEXT_HTML);
			g_free (text);
		} while (list_len);

		gtk_drag_finish (context, TRUE, is_move, time);
		g_signal_stop_emission_by_name (widget, "drag-data-received");
		e_content_editor_emit_drop_handled (E_CONTENT_EDITOR (widget));
		return;
	}
}

static void
webkit_editor_drag_leave_cb (EWebKitEditor *wk_editor,
                             GdkDragContext *context,
                             guint time)
{
	/* Don't pass drag-leave to WebKit otherwise the drop won't be handled by it.
	 * We will emit it later when WebKit is expecting it. */
	g_signal_stop_emission_by_name (GTK_WIDGET (wk_editor), "drag-leave");
}

static gboolean
webkit_editor_drag_motion_cb (GtkWidget *widget,
			      GdkDragContext *context,
			      gint x,
			      gint y,
			      guint time,
			      gpointer user_data)
{
	static GdkAtom x_uid_list = GDK_NONE, x_moz_url = GDK_NONE;
	GdkAtom chosen;

	chosen = gtk_drag_dest_find_target (widget, context, NULL);

	if (x_uid_list == GDK_NONE)
		x_uid_list = gdk_atom_intern_static_string ("x-uid-list");

	/* This is when dragging message from the message list, which can eventually freeze
	   Evolution, if PDF file format is set, when processes by WebKitGTK itself. */
	if (chosen != GDK_NONE && chosen == x_uid_list) {
		gdk_drag_status (context, GDK_ACTION_COPY, time);
		return TRUE;
	}

	if (x_moz_url == GDK_NONE)
		x_moz_url = gdk_atom_intern_static_string ("text/x-moz-url");

	if (chosen != GDK_NONE && chosen == x_moz_url) {
		gdk_drag_status (context, GDK_ACTION_COPY, time);
		return TRUE;
	}

	return FALSE;
}

static gboolean
webkit_editor_drag_drop_cb (EWebKitEditor *wk_editor,
                            GdkDragContext *context,
                            gint x,
                            gint y,
                            guint time)
{
	wk_editor->priv->drag_data_received_handler_id = g_signal_connect (
		wk_editor, "drag-data-received",
		G_CALLBACK (webkit_editor_drag_data_received_cb), NULL);

	webkit_editor_set_changed (wk_editor, TRUE);

	return FALSE;
}

static void
webkit_editor_web_process_terminated_cb (EWebKitEditor *wk_editor,
					 WebKitWebProcessTerminationReason reason)
{
	GtkWidget *widget;

	g_return_if_fail (E_IS_WEBKIT_EDITOR (wk_editor));

	wk_editor->priv->is_malfunction = TRUE;
	g_object_notify (G_OBJECT (wk_editor), "is-malfunction");

	widget = GTK_WIDGET (wk_editor);
	while (widget) {
		if (E_IS_ALERT_SINK (widget)) {
			e_alert_submit (E_ALERT_SINK (widget), "mail-composer:webkit-web-process-crashed", NULL);
			break;
		}

		if (E_IS_MSG_COMPOSER (widget)) {
			EHTMLEditor *html_editor;

			html_editor = e_msg_composer_get_editor (E_MSG_COMPOSER (widget));
			if (html_editor) {
				e_alert_submit (E_ALERT_SINK (html_editor), "mail-composer:webkit-web-process-crashed", NULL);
				break;
			}
		}

		widget = gtk_widget_get_parent (widget);
	}

	/* No suitable EAlertSink found as the parent widget */
	if (!widget) {
		g_warning (
			"WebKitWebProcess (page id %" G_GUINT64_FORMAT ") for EWebKitEditor crashed",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (wk_editor)));
	}
}

static void
paste_quote_text (EContentEditor *editor,
		  const gchar *text,
		  gboolean is_html)
{
	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (text != NULL);

	e_content_editor_insert_content (
		editor,
		text,
		E_CONTENT_EDITOR_INSERT_QUOTE_CONTENT |
		(is_html ? E_CONTENT_EDITOR_INSERT_TEXT_HTML : E_CONTENT_EDITOR_INSERT_TEXT_PLAIN));
}

static void
clipboard_html_received_for_paste_quote (GtkClipboard *clipboard,
                                         const gchar *text,
                                         gpointer user_data)
{
	EContentEditor *editor = user_data;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (text != NULL);

	paste_quote_text (editor, text, TRUE);
}

static void
clipboard_text_received_for_paste_quote (GtkClipboard *clipboard,
                                         const gchar *text,
                                         gpointer user_data)
{
	EContentEditor *editor = user_data;

	g_return_if_fail (E_IS_CONTENT_EDITOR (editor));
	g_return_if_fail (text != NULL);

	paste_quote_text (editor, text, FALSE);
}

static void
paste_primary_clipboard_quoted (EContentEditor *editor)
{
	EWebKitEditor *wk_editor;
	GtkClipboard *clipboard;

	wk_editor = E_WEBKIT_EDITOR (editor);

	clipboard = gtk_clipboard_get_for_display (
		gdk_display_get_default (),
		GDK_SELECTION_PRIMARY);

       if (wk_editor->priv->mode == E_CONTENT_EDITOR_MODE_HTML) {
               if (e_clipboard_wait_is_html_available (clipboard))
                       e_clipboard_request_html (clipboard, clipboard_html_received_for_paste_quote, editor);
               else if (gtk_clipboard_wait_is_text_available (clipboard))
                       gtk_clipboard_request_text (clipboard, clipboard_text_received_for_paste_quote, editor);
       } else {
               if (gtk_clipboard_wait_is_text_available (clipboard))
                       gtk_clipboard_request_text (clipboard, clipboard_text_received_for_paste_quote, editor);
               else if (e_clipboard_wait_is_html_available (clipboard))
                       e_clipboard_request_html (clipboard, clipboard_html_received_for_paste_quote, editor);
       }
}

static CamelMimePart *
e_webkit_editor_cid_resolver_ref_part (ECidResolver *resolver,
				       const gchar *cid_uri)
{
	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (resolver), NULL);

	return e_content_editor_emit_ref_mime_part (E_CONTENT_EDITOR (resolver), cid_uri);
}

static const gchar *
webkit_editor_get_hover_uri (EContentEditor *editor)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	return wk_editor->priv->last_hover_uri;
}

static void
webkit_editor_get_caret_client_rect (EContentEditor *editor,
				     GdkRectangle *out_rect)
{
	EWebKitEditor *wk_editor = E_WEBKIT_EDITOR (editor);

	out_rect->x = wk_editor->priv->caret_client_rect.x;
	out_rect->y = wk_editor->priv->caret_client_rect.y;
	out_rect->width = wk_editor->priv->caret_client_rect.width;
	out_rect->height = wk_editor->priv->caret_client_rect.height;
}

typedef struct _MoveToAnchorData {
	GWeakRef weakref; /* EWebKitEditor */
	gchar *anchor_name;
} MoveToAnchorData;

static void
move_to_anchor_data_free (gpointer ptr)
{
	MoveToAnchorData *data = ptr;

	if (data) {
		g_weak_ref_clear (&data->weakref);
		g_free (data->anchor_name);
		g_free (data);
	}
}

static gboolean
webkit_editor_move_to_anchor_idle_cb (gpointer user_data)
{
	MoveToAnchorData *data = user_data;
	EWebKitEditor *wk_editor;

	wk_editor = g_weak_ref_get (&data->weakref);
	if (wk_editor) {
		e_web_view_jsc_run_script (WEBKIT_WEB_VIEW (wk_editor), wk_editor->priv->cancellable,
			"EvoEditor.MoveToAnchor(%s);",
			data->anchor_name);

		g_object_unref (wk_editor);
	}

	return G_SOURCE_REMOVE;
}

static gboolean
webkit_editor_query_tooltip_cb (GtkWidget *widget,
				gint xx,
				gint yy,
				gboolean keyboard_mode,
				GtkTooltip *tooltip,
				gpointer user_data)
{
	EWebKitEditor *wk_editor;
	gchar *str;

	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (widget), FALSE);

	wk_editor = E_WEBKIT_EDITOR (widget);

	if (!wk_editor->priv->last_hover_uri || !*wk_editor->priv->last_hover_uri)
		return FALSE;

	if (*wk_editor->priv->last_hover_uri == '#') {
		str = g_strdup_printf (_("Ctrl-click to go to the section “%s” of the message"), wk_editor->priv->last_hover_uri + 1);
	} else {
		/* Translators: The "%s" is replaced with a link, constructing a text like: "Ctrl-click to open a link “http://www.example.com”" */
		str = g_strdup_printf (_("Ctrl-click to open a link “%s”"), wk_editor->priv->last_hover_uri);
	}
	gtk_tooltip_set_text (tooltip, str);
	g_free (str);

	return TRUE;
}

static gboolean
webkit_editor_button_press_event (GtkWidget *widget,
                                  GdkEventButton *event)
{
	EWebKitEditor *wk_editor;

	g_return_val_if_fail (E_IS_WEBKIT_EDITOR (widget), FALSE);

	wk_editor = E_WEBKIT_EDITOR (widget);

	if (event->button == 2) {
		if (event->type == GDK_BUTTON_PRESS) {
			if (event->time) {
				GtkSettings *settings;
				guint32 last_button_press_time_ms;

				last_button_press_time_ms = wk_editor->priv->last_button_press_time_ms;

				settings = gtk_settings_get_for_screen (gtk_widget_get_screen (widget));
				if (settings) {
					gint double_click_time_ms = 0;

					g_object_get (G_OBJECT (settings), "gtk-double-click-time", &double_click_time_ms, NULL);

					/* it's too early, double-click or triple-click event will follow */
					if (double_click_time_ms >= event->time - last_button_press_time_ms)
						return TRUE;
				}

				wk_editor->priv->last_button_press_time_ms = event->time;
			}

			if ((event->state & GDK_SHIFT_MASK) != 0) {
				paste_primary_clipboard_quoted (E_CONTENT_EDITOR (widget));
			} else if (!e_content_editor_emit_paste_primary_clipboard (E_CONTENT_EDITOR (widget)))
				webkit_editor_paste_primary (E_CONTENT_EDITOR( (widget)));
		}

		return TRUE;
	}

	/* Ctrl + Left Click on link opens it. */
	if (event->button == 1 && wk_editor->priv->last_hover_uri &&
	    *wk_editor->priv->last_hover_uri &&
	    (event->state & GDK_CONTROL_MASK) != 0 &&
	    (event->state & GDK_SHIFT_MASK) == 0 &&
	    (event->state & GDK_MOD1_MASK) == 0) {
		if (*wk_editor->priv->last_hover_uri == '#') {
			MoveToAnchorData *data;

			data = g_new0 (MoveToAnchorData, 1);
			g_weak_ref_init (&data->weakref, wk_editor);
			data->anchor_name = g_strdup (wk_editor->priv->last_hover_uri + 1);

			g_idle_add_full (G_PRIORITY_HIGH_IDLE, webkit_editor_move_to_anchor_idle_cb, data,
				move_to_anchor_data_free);
		} else {
			GtkWidget *toplevel;

			toplevel = gtk_widget_get_toplevel (GTK_WIDGET (wk_editor));

			e_show_uri (GTK_WINDOW (toplevel), wk_editor->priv->last_hover_uri);
		}
	}

	/* Chain up to parent's button_press_event() method. */
	return GTK_WIDGET_CLASS (e_webkit_editor_parent_class)->button_press_event &&
	       GTK_WIDGET_CLASS (e_webkit_editor_parent_class)->button_press_event (widget, event);
}

static gboolean
webkit_editor_button_release_event (GtkWidget *widget,
				    GdkEventButton *event)
{
	if (event->button == 2) {
		/* WebKitGTK 2.46.1 changed the middle-click paste behavior and moved
		   the paste handler from the button-press event into the button-release
		   event, which causes double paste of the clipboard content. As the paste
		   is handled in the webkit_editor_button_press_event() above, make sure
		   the release handler is not called here regardless whether the user
		   uses the changed WebkitGTK or not. */
		return TRUE;
	}

	/* Chain up to parent's method. */
	return GTK_WIDGET_CLASS (e_webkit_editor_parent_class)->button_release_event (widget, event);
}

static gboolean
webkit_editor_key_press_event (GtkWidget *widget,
                               GdkEventKey *event)
{
	GdkKeymapKey key = { 0, 0, 0 };
	guint keyval;
	gboolean is_shift, is_ctrl;
	gboolean is_webkit_keypress = FALSE;

	key.keycode = event->hardware_keycode;

	/* Translate the keyval to the base group, thus it's independent of the current user keyboard layout */
	keyval = gdk_keymap_lookup_key (gdk_keymap_get_for_display (gtk_widget_get_display (widget)), &key);
	if (!keyval)
		keyval = event->keyval;

	is_shift = ((event)->state & GDK_SHIFT_MASK) != 0;
	is_ctrl = ((event)->state & GDK_CONTROL_MASK) != 0;

	/* Copy */
	if (is_ctrl && !is_shift && (keyval == GDK_KEY_c || keyval == GDK_KEY_C))
		is_webkit_keypress = TRUE;

	/* Copy - secondary shortcut */
	if (is_ctrl && !is_shift && keyval == GDK_KEY_Insert) {
		webkit_editor_copy (E_CONTENT_EDITOR (widget));
		return TRUE;
	}

	/* Cut */
	if (is_ctrl && !is_shift && (keyval == GDK_KEY_x || keyval == GDK_KEY_X))
		is_webkit_keypress = TRUE;

	/* Cut - secondary shortcut */
	if (!is_ctrl && is_shift && keyval == GDK_KEY_Delete) {
		webkit_editor_cut (E_CONTENT_EDITOR (widget));
		return TRUE;
	}

	/* Paste */
	if (is_ctrl && !is_shift && (keyval == GDK_KEY_v || keyval == GDK_KEY_V))
		is_webkit_keypress = TRUE;

	/* Paste - secondary shortcut */
	if (!is_ctrl && is_shift && keyval == GDK_KEY_Insert) {
		webkit_editor_paste (E_CONTENT_EDITOR (widget));
		return TRUE;
	}

	/* Undo */
	if (is_ctrl && !is_shift && (keyval == GDK_KEY_z || keyval == GDK_KEY_Z))
		is_webkit_keypress = TRUE;

	/* Redo */
	if (is_ctrl && is_shift && (keyval == GDK_KEY_z || keyval == GDK_KEY_Z))
		is_webkit_keypress = TRUE;

	if (is_ctrl && is_shift && (keyval == GDK_KEY_i || keyval == GDK_KEY_I) &&
	    e_util_get_webkit_developer_mode_enabled ()) {
		webkit_editor_show_inspector (E_WEBKIT_EDITOR (widget));
		return TRUE;
	}

	/* This is to prevent WebKitGTK+ to process standard key presses, which are
	   supposed to be handled by Evolution instead. By returning FALSE, the parent
	   widget in the hierarchy will process the key press, instead of the WebKitWebView. */
	if (is_webkit_keypress)
		return FALSE;

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (e_webkit_editor_parent_class)->key_press_event &&
	       GTK_WIDGET_CLASS (e_webkit_editor_parent_class)->key_press_event (widget, event);
}

static gdouble
webkit_editor_get_zoom_level (EContentEditor *editor)
{
	return webkit_web_view_get_zoom_level (WEBKIT_WEB_VIEW (editor));
}

static void
webkit_editor_set_zoom_level (EContentEditor *editor,
			      gdouble level)
{
	webkit_web_view_set_zoom_level (WEBKIT_WEB_VIEW (editor), level);
}

static void
e_webkit_editor_class_init (EWebKitEditorClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = webkit_editor_constructed;
	object_class->constructor = webkit_editor_constructor;
	object_class->get_property = webkit_editor_get_property;
	object_class->set_property = webkit_editor_set_property;
	object_class->dispose = webkit_editor_dispose;
	object_class->finalize = webkit_editor_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = webkit_editor_button_press_event;
	widget_class->button_release_event = webkit_editor_button_release_event;
	widget_class->key_press_event = webkit_editor_key_press_event;

	g_object_class_override_property (
		object_class, PROP_IS_MALFUNCTION, "is-malfunction");
	g_object_class_override_property (
		object_class, PROP_CAN_COPY, "can-copy");
	g_object_class_override_property (
		object_class, PROP_CAN_CUT, "can-cut");
	g_object_class_override_property (
		object_class, PROP_CAN_PASTE, "can-paste");
	g_object_class_override_property (
		object_class, PROP_CAN_REDO, "can-redo");
	g_object_class_override_property (
		object_class, PROP_CAN_UNDO, "can-undo");
	g_object_class_override_property (
		object_class, PROP_CHANGED, "changed");
	g_object_class_override_property (
		object_class, PROP_MODE, "mode");
	g_object_class_override_property (
		object_class, PROP_EDITABLE, "editable");
	g_object_class_override_property (
		object_class, PROP_ALIGNMENT, "alignment");
	g_object_class_override_property (
		object_class, PROP_BACKGROUND_COLOR, "background-color");
	g_object_class_override_property (
		object_class, PROP_BLOCK_FORMAT, "block-format");
	g_object_class_override_property (
		object_class, PROP_BOLD, "bold");
	g_object_class_override_property (
		object_class, PROP_FONT_COLOR, "font-color");
	g_object_class_override_property (
		object_class, PROP_FONT_NAME, "font-name");
	g_object_class_override_property (
		object_class, PROP_FONT_SIZE, "font-size");
	g_object_class_override_property (
		object_class, PROP_INDENT_LEVEL, "indent-level");
	g_object_class_override_property (
		object_class, PROP_ITALIC, "italic");
	g_object_class_override_property (
		object_class, PROP_STRIKETHROUGH, "strikethrough");
	g_object_class_override_property (
		object_class, PROP_SUBSCRIPT, "subscript");
	g_object_class_override_property (
		object_class, PROP_SUPERSCRIPT, "superscript");
	g_object_class_override_property (
		object_class, PROP_UNDERLINE, "underline");
	g_object_class_override_property (
		object_class, PROP_START_BOTTOM, "start-bottom");
	g_object_class_override_property (
		object_class, PROP_TOP_SIGNATURE, "top-signature");
	g_object_class_override_property (
		object_class, PROP_SPELL_CHECK_ENABLED, "spell-check-enabled");
	g_object_class_override_property (
		object_class, PROP_VISUALLY_WRAP_LONG_LINES, "visually-wrap-long-lines");
	g_object_class_override_property (
		object_class, PROP_LAST_ERROR, "last-error");
	g_object_class_override_property (
		object_class, PROP_SPELL_CHECKER, "spell-checker");

	g_object_class_install_property (
		object_class,
		PROP_NORMAL_PARAGRAPH_WIDTH,
		g_param_spec_int (
			"normal-paragraph-width",
			NULL,
			NULL,
			G_MININT32,
			G_MAXINT32,
			71, /* Should be the same as e-editor.js:EvoEditor.NORMAL_PARAGRAPH_WIDTH and in the init()*/
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MAGIC_LINKS,
		g_param_spec_boolean (
			"magic-links",
			NULL,
			NULL,
			TRUE, /* Should be the same as e-editor.js:EvoEditor.MAGIC_LINKS and in the init() */
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MAGIC_SMILEYS,
		g_param_spec_boolean (
			"magic-smileys",
			NULL,
			NULL,
			FALSE, /* Should be the same as e-editor.js:EvoEditor.MAGIC_SMILEYS and in the init() */
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_UNICODE_SMILEYS,
		g_param_spec_boolean (
			"unicode-smileys",
			NULL,
			NULL,
			FALSE, /* Should be the same as e-editor.js:EvoEditor.UNICODE_SMILEYS and in the init() */
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_WRAP_QUOTED_TEXT_IN_REPLIES,
		g_param_spec_boolean (
			"wrap-quoted-text-in-replies",
			NULL,
			NULL,
			TRUE, /* Should be the same as e-editor.js:EvoEditor.WRAP_QUOTED_TEXT_IN_REPLIES and in the init() */
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MINIMUM_FONT_SIZE,
		g_param_spec_int (
			"minimum-font-size",
			"Minimum Font Size",
			NULL,
			G_MININT, G_MAXINT, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_PASTE_PLAIN_PREFER_PRE,
		g_param_spec_boolean (
			"paste-plain-prefer-pre",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_LINK_TO_TEXT,
		g_param_spec_enum (
			"link-to-text",
			NULL,
			NULL,
			E_TYPE_HTML_LINK_TO_TEXT,
			E_HTML_LINK_TO_TEXT_REFERENCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
}

static void
e_webkit_editor_init (EWebKitEditor *wk_editor)
{
	GSettings *g_settings;

	wk_editor->priv = e_webkit_editor_get_instance_private (wk_editor);

	/* To be able to cancel any pending calls when 'dispose' is called. */
	wk_editor->priv->cancellable = g_cancellable_new ();
	wk_editor->priv->scheme_handlers = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
	wk_editor->priv->is_malfunction = FALSE;
	wk_editor->priv->spell_check_enabled = TRUE;
	wk_editor->priv->spell_checker = e_spell_checker_new ();
	wk_editor->priv->old_settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);
	wk_editor->priv->visually_wrap_long_lines = FALSE;

	wk_editor->priv->normal_paragraph_width = 71;
	wk_editor->priv->magic_links = TRUE;
	wk_editor->priv->magic_smileys = FALSE;
	wk_editor->priv->unicode_smileys = FALSE;
	wk_editor->priv->wrap_quoted_text_in_replies = TRUE;

	g_signal_connect (
		wk_editor, "load-changed",
		G_CALLBACK (webkit_editor_load_changed_cb), NULL);

	g_signal_connect (
		wk_editor, "context-menu",
		G_CALLBACK (webkit_editor_context_menu_cb), NULL);

	g_signal_connect (
		wk_editor, "mouse-target-changed",
		G_CALLBACK (webkit_editor_mouse_target_changed_cb), NULL);

	g_signal_connect (
		wk_editor, "drag-begin",
		G_CALLBACK (webkit_editor_drag_begin_cb), NULL);

	g_signal_connect (
		wk_editor, "drag-failed",
		G_CALLBACK (webkit_editor_drag_failed_cb), NULL);

	g_signal_connect (
		wk_editor, "drag-end",
		G_CALLBACK (webkit_editor_drag_end_cb), NULL);

	g_signal_connect (
		wk_editor, "drag-leave",
		G_CALLBACK (webkit_editor_drag_leave_cb), NULL);

	g_signal_connect (
		wk_editor, "drag-drop",
		G_CALLBACK (webkit_editor_drag_drop_cb), NULL);

	g_signal_connect (
		wk_editor, "drag-motion",
		G_CALLBACK (webkit_editor_drag_motion_cb), NULL);

	g_signal_connect (
		wk_editor, "web-process-terminated",
		G_CALLBACK (webkit_editor_web_process_terminated_cb), NULL);

	g_signal_connect (
		wk_editor, "style-updated",
		G_CALLBACK (webkit_editor_style_updated_cb), NULL);

	g_signal_connect (
		wk_editor, "state-flags-changed",
		G_CALLBACK (webkit_editor_style_updated_cb), NULL);

	gtk_widget_set_has_tooltip (GTK_WIDGET (wk_editor), TRUE);

	g_signal_connect (
		wk_editor, "query-tooltip",
		G_CALLBACK (webkit_editor_query_tooltip_cb), NULL);

	g_settings = e_util_ref_settings ("org.gnome.desktop.interface");
	g_signal_connect (
		g_settings, "changed::font-name",
		G_CALLBACK (webkit_editor_settings_changed_cb), wk_editor);
	g_signal_connect (
		g_settings, "changed::monospace-font-name",
		G_CALLBACK (webkit_editor_settings_changed_cb), wk_editor);
	wk_editor->priv->font_settings = g_settings;

	g_settings = e_util_ref_settings ("org.gnome.evolution.mail");
	wk_editor->priv->mail_settings = g_settings;

	g_signal_connect (
		g_settings, "changed::composer-inherit-theme-colors",
		G_CALLBACK (webkit_editor_style_settings_changed_cb), wk_editor);

	wk_editor->priv->mode = E_CONTENT_EDITOR_MODE_HTML;
	wk_editor->priv->changed = FALSE;
	wk_editor->priv->can_copy = FALSE;
	wk_editor->priv->can_cut = FALSE;
	wk_editor->priv->can_paste = FALSE;
	wk_editor->priv->can_undo = FALSE;
	wk_editor->priv->can_redo = FALSE;
	wk_editor->priv->current_user_stylesheet = NULL;

	wk_editor->priv->font_color = NULL;
	wk_editor->priv->background_color = NULL;
	wk_editor->priv->body_font_name = NULL;
	wk_editor->priv->font_name = NULL;
	wk_editor->priv->font_size = E_CONTENT_EDITOR_FONT_SIZE_NORMAL;
	wk_editor->priv->block_format = E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH;
	wk_editor->priv->alignment = E_CONTENT_EDITOR_ALIGNMENT_LEFT;

	wk_editor->priv->caret_client_rect.x = 0;
	wk_editor->priv->caret_client_rect.y = 0;
	wk_editor->priv->caret_client_rect.width = -1;
	wk_editor->priv->caret_client_rect.height = -1;

	wk_editor->priv->start_bottom = E_THREE_STATE_INCONSISTENT;
	wk_editor->priv->top_signature = E_THREE_STATE_INCONSISTENT;

	wk_editor->priv->link_to_text = E_HTML_LINK_TO_TEXT_REFERENCE;

	wk_editor_change_existing_instances (+1);
}

static void
e_webkit_editor_content_editor_init (EContentEditorInterface *iface)
{
	iface->supports_mode = webkit_editor_supports_mode;
	iface->initialize = webkit_editor_initialize;
	iface->update_styles = webkit_editor_update_styles;
	iface->insert_content = webkit_editor_insert_content;
	iface->get_content = webkit_editor_get_content;
	iface->get_content_finish = webkit_editor_get_content_finish;
	iface->insert_image = webkit_editor_insert_image;
	iface->insert_emoticon = webkit_editor_insert_emoticon;
	iface->move_caret_on_coordinates = webkit_editor_move_caret_on_coordinates;
	iface->cut = webkit_editor_cut;
	iface->copy = webkit_editor_copy;
	iface->paste = webkit_editor_paste;
	iface->paste_primary = webkit_editor_paste_primary;
	iface->undo = webkit_editor_undo;
	iface->redo = webkit_editor_redo;
	iface->clear_undo_redo_history = webkit_editor_clear_undo_redo_history;
	iface->set_spell_checking_languages = webkit_editor_set_spell_checking_languages;
	iface->get_caret_word = webkit_editor_get_caret_word;
	iface->replace_caret_word = webkit_editor_replace_caret_word;
	iface->select_all = webkit_editor_select_all;
	iface->selection_indent = webkit_editor_selection_indent;
	iface->selection_unindent = webkit_editor_selection_unindent;
	iface->selection_unlink = webkit_editor_selection_unlink;
	iface->find = webkit_editor_find;
	iface->replace = webkit_editor_replace;
	iface->replace_all = webkit_editor_replace_all;
	iface->selection_save = webkit_editor_selection_save;
	iface->selection_restore = webkit_editor_selection_restore;
	iface->selection_wrap = webkit_editor_selection_wrap;
	iface->get_current_signature_uid =  webkit_editor_get_current_signature_uid;
	iface->is_ready = webkit_editor_is_ready;
	iface->insert_signature = webkit_editor_insert_signature;
	iface->on_dialog_open = webkit_editor_on_dialog_open;
	iface->on_dialog_close = webkit_editor_on_dialog_close;
	iface->delete_cell_contents = webkit_editor_delete_cell_contents;
	iface->delete_column = webkit_editor_delete_column;
	iface->delete_row = webkit_editor_delete_row;
	iface->delete_table = webkit_editor_delete_table;
	iface->insert_column_after = webkit_editor_insert_column_after;
	iface->insert_column_before = webkit_editor_insert_column_before;
	iface->insert_row_above = webkit_editor_insert_row_above;
	iface->insert_row_below = webkit_editor_insert_row_below;
	iface->h_rule_set_align = webkit_editor_h_rule_set_align;
	iface->h_rule_get_align = webkit_editor_h_rule_get_align;
	iface->h_rule_set_size = webkit_editor_h_rule_set_size;
	iface->h_rule_get_size = webkit_editor_h_rule_get_size;
	iface->h_rule_set_width = webkit_editor_h_rule_set_width;
	iface->h_rule_get_width = webkit_editor_h_rule_get_width;
	iface->h_rule_set_no_shade = webkit_editor_h_rule_set_no_shade;
	iface->h_rule_get_no_shade = webkit_editor_h_rule_get_no_shade;
	iface->image_set_src = webkit_editor_image_set_src;
	iface->image_get_src = webkit_editor_image_get_src;
	iface->image_set_alt = webkit_editor_image_set_alt;
	iface->image_get_alt = webkit_editor_image_get_alt;
	iface->image_set_url = webkit_editor_image_set_url;
	iface->image_get_url = webkit_editor_image_get_url;
	iface->image_set_vspace = webkit_editor_image_set_vspace;
	iface->image_get_vspace = webkit_editor_image_get_vspace;
	iface->image_set_hspace = webkit_editor_image_set_hspace;
	iface->image_get_hspace = webkit_editor_image_get_hspace;
	iface->image_set_border = webkit_editor_image_set_border;
	iface->image_get_border = webkit_editor_image_get_border;
	iface->image_set_align = webkit_editor_image_set_align;
	iface->image_get_align = webkit_editor_image_get_align;
	iface->image_get_natural_width = webkit_editor_image_get_natural_width;
	iface->image_get_natural_height = webkit_editor_image_get_natural_height;
	iface->image_set_height = webkit_editor_image_set_height;
	iface->image_set_width = webkit_editor_image_set_width;
	iface->image_set_height_follow = webkit_editor_image_set_height_follow;
	iface->image_set_width_follow = webkit_editor_image_set_width_follow;
	iface->image_get_width = webkit_editor_image_get_width;
	iface->image_get_height = webkit_editor_image_get_height;
	iface->link_set_properties = webkit_editor_link_set_properties;
	iface->link_get_properties = webkit_editor_link_get_properties;
	iface->page_set_text_color = webkit_editor_page_set_text_color;
	iface->page_get_text_color = webkit_editor_page_get_text_color;
	iface->page_set_background_color = webkit_editor_page_set_background_color;
	iface->page_get_background_color = webkit_editor_page_get_background_color;
	iface->page_set_link_color = webkit_editor_page_set_link_color;
	iface->page_get_link_color = webkit_editor_page_get_link_color;
	iface->page_set_visited_link_color = webkit_editor_page_set_visited_link_color;
	iface->page_get_visited_link_color = webkit_editor_page_get_visited_link_color;
	iface->page_set_font_name = webkit_editor_page_set_font_name;
	iface->page_get_font_name = webkit_editor_page_get_font_name;
	iface->page_set_background_image_uri = webkit_editor_page_set_background_image_uri;
	iface->page_get_background_image_uri = webkit_editor_page_get_background_image_uri;
	iface->cell_set_v_align = webkit_editor_cell_set_v_align;
	iface->cell_get_v_align = webkit_editor_cell_get_v_align;
	iface->cell_set_align = webkit_editor_cell_set_align;
	iface->cell_get_align = webkit_editor_cell_get_align;
	iface->cell_set_wrap = webkit_editor_cell_set_wrap;
	iface->cell_get_wrap = webkit_editor_cell_get_wrap;
	iface->cell_set_header_style = webkit_editor_cell_set_header_style;
	iface->cell_is_header = webkit_editor_cell_is_header;
	iface->cell_get_width = webkit_editor_cell_get_width;
	iface->cell_set_width = webkit_editor_cell_set_width;
	iface->cell_get_row_span = webkit_editor_cell_get_row_span;
	iface->cell_set_row_span = webkit_editor_cell_set_row_span;
	iface->cell_get_col_span = webkit_editor_cell_get_col_span;
	iface->cell_set_col_span = webkit_editor_cell_set_col_span;
	iface->cell_get_background_image_uri = webkit_editor_cell_get_background_image_uri;
	iface->cell_set_background_image_uri = webkit_editor_cell_set_background_image_uri;
	iface->cell_get_background_color = webkit_editor_cell_get_background_color;
	iface->cell_set_background_color = webkit_editor_cell_set_background_color;
	iface->table_set_row_count = webkit_editor_table_set_row_count;
	iface->table_get_row_count = webkit_editor_table_get_row_count;
	iface->table_set_column_count = webkit_editor_table_set_column_count;
	iface->table_get_column_count = webkit_editor_table_get_column_count;
	iface->table_set_width = webkit_editor_table_set_width;
	iface->table_get_width = webkit_editor_table_get_width;
	iface->table_set_align = webkit_editor_table_set_align;
	iface->table_get_align = webkit_editor_table_get_align;
	iface->table_set_padding = webkit_editor_table_set_padding;
	iface->table_get_padding = webkit_editor_table_get_padding;
	iface->table_set_spacing = webkit_editor_table_set_spacing;
	iface->table_get_spacing = webkit_editor_table_get_spacing;
	iface->table_set_border = webkit_editor_table_set_border;
	iface->table_get_border = webkit_editor_table_get_border;
	iface->table_get_background_image_uri = webkit_editor_table_get_background_image_uri;
	iface->table_set_background_image_uri = webkit_editor_table_set_background_image_uri;
	iface->table_get_background_color = webkit_editor_table_get_background_color;
	iface->table_set_background_color = webkit_editor_table_set_background_color;
	iface->spell_check_next_word = webkit_editor_spell_check_next_word;
	iface->spell_check_prev_word = webkit_editor_spell_check_prev_word;
	iface->delete_h_rule = webkit_editor_delete_h_rule;
	iface->delete_image = webkit_editor_delete_image;
	iface->get_hover_uri = webkit_editor_get_hover_uri;
	iface->get_caret_client_rect = webkit_editor_get_caret_client_rect;
	iface->get_zoom_level = webkit_editor_get_zoom_level;
	iface->set_zoom_level = webkit_editor_set_zoom_level;
}

static void
e_webkit_editor_cid_resolver_init (ECidResolverInterface *iface)
{
	iface->ref_part = e_webkit_editor_cid_resolver_ref_part;
	/* iface->dup_mime_type = e_webkit_editor_cid_resolver_dup_mime_part; - not needed here */
}
