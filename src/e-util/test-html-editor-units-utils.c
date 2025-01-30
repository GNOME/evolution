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

#include <string.h>
#include <stdlib.h>

#include "e-util/e-util.h"

#include "test-html-editor-units-utils.h"

static guint event_processing_delay_ms = 25;
static gboolean in_background = FALSE;
static gboolean use_multiple_web_processes = FALSE;
static gboolean glob_keep_going = FALSE;
static GObject *global_web_context = NULL;

void
test_utils_set_event_processing_delay_ms (guint value)
{
	event_processing_delay_ms = value;
}

guint
test_utils_get_event_processing_delay_ms (void)
{
	return event_processing_delay_ms;
}

void
test_utils_set_background (gboolean background)
{
	in_background = background;
}

gboolean
test_utils_get_background (void)
{
	return in_background;
}

void
test_utils_set_multiple_web_processes (gboolean multiple_web_processes)
{
	use_multiple_web_processes = multiple_web_processes;
}

gboolean
test_utils_get_multiple_web_processes (void)
{
	return use_multiple_web_processes;
}

void
test_utils_set_keep_going (gboolean keep_going)
{
	glob_keep_going = keep_going;
}

gboolean
test_utils_get_keep_going (void)
{
	return glob_keep_going;
}

void
test_utils_free_global_memory (void)
{
	g_clear_object (&global_web_context);
}

typedef struct _GetContentData {
	EContentEditorContentHash *content_hash;
	gpointer async_data;
} GetContentData;

static void
get_editor_content_hash_ready_cb (GObject *source_object,
				  GAsyncResult *result,
				  gpointer user_data)
{
	GetContentData *gcd = user_data;
	GError *error = NULL;

	g_assert_nonnull (gcd);
	g_assert_true (E_IS_CONTENT_EDITOR (source_object));

	gcd->content_hash = e_content_editor_get_content_finish (E_CONTENT_EDITOR (source_object), result, &error);

	g_assert_no_error (error);

	g_clear_error (&error);

	test_utils_async_call_finish (gcd->async_data);
}

static EContentEditorContentHash *
test_utils_get_editor_content_hash_sync (EContentEditor *cnt_editor,
					 guint32 flags)
{
	GetContentData gcd;

	g_assert_true (E_IS_CONTENT_EDITOR (cnt_editor));

	gcd.content_hash = NULL;
	gcd.async_data = test_utils_async_call_prepare ();

	e_content_editor_get_content (cnt_editor, flags, "test-domain", NULL, get_editor_content_hash_ready_cb, &gcd);

	g_assert_true (test_utils_async_call_wait (gcd.async_data, MAX (event_processing_delay_ms / 25, 1) + 1));
	g_assert_nonnull (gcd.content_hash);

	return gcd.content_hash;
}

typedef struct _UndoContent {
	gchar *html;
	gchar *plain;
} UndoContent;

static UndoContent *
undo_content_new (TestFixture *fixture)
{
	EContentEditor *cnt_editor;
	EContentEditorContentHash *content_hash;
	UndoContent *uc;

	g_return_val_if_fail (fixture != NULL, NULL);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), NULL);

	cnt_editor = e_html_editor_get_content_editor (fixture->editor);
	content_hash = test_utils_get_editor_content_hash_sync (cnt_editor, E_CONTENT_EDITOR_GET_TO_SEND_HTML | E_CONTENT_EDITOR_GET_TO_SEND_PLAIN);

	uc = g_new0 (UndoContent, 1);
	uc->html = e_content_editor_util_steal_content_data (content_hash, E_CONTENT_EDITOR_GET_TO_SEND_HTML, NULL);
	uc->plain = e_content_editor_util_steal_content_data (content_hash, E_CONTENT_EDITOR_GET_TO_SEND_PLAIN, NULL);

	e_content_editor_util_free_content_hash (content_hash);

	g_warn_if_fail (uc->html != NULL);
	g_warn_if_fail (uc->plain != NULL);

	return uc;
}

static void
undo_content_free (gpointer ptr)
{
	UndoContent *uc = ptr;

	if (uc) {
		g_free (uc->html);
		g_free (uc->plain);
		g_free (uc);
	}
}

static gboolean
undo_content_test (TestFixture *fixture,
		   const UndoContent *uc,
		   gint cmd_index)
{
	EContentEditor *cnt_editor;
	EContentEditorContentHash *content_hash;
	const gchar *text;

	g_return_val_if_fail (fixture != NULL, FALSE);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), FALSE);
	g_return_val_if_fail (uc != NULL, FALSE);

	cnt_editor = e_html_editor_get_content_editor (fixture->editor);
	content_hash = test_utils_get_editor_content_hash_sync (cnt_editor, E_CONTENT_EDITOR_GET_TO_SEND_HTML | E_CONTENT_EDITOR_GET_TO_SEND_PLAIN);

	text = e_content_editor_util_get_content_data (content_hash, E_CONTENT_EDITOR_GET_TO_SEND_HTML);
	g_return_val_if_fail (text != NULL, FALSE);

	if (!test_utils_html_equal (fixture, text, uc->html)) {
		if (glob_keep_going)
			g_printerr ("%s: returned HTML\n---%s---\n and expected HTML\n---%s---\n do not match at command %d\n", G_STRFUNC, text, uc->html, cmd_index);
		else
			g_warning ("%s: returned HTML\n---%s---\n and expected HTML\n---%s---\n do not match at command %d", G_STRFUNC, text, uc->html, cmd_index);

		e_content_editor_util_free_content_hash (content_hash);

		return FALSE;
	}

	text = e_content_editor_util_get_content_data (content_hash, E_CONTENT_EDITOR_GET_TO_SEND_PLAIN);
	g_return_val_if_fail (text != NULL, FALSE);

	if (!test_utils_html_equal (fixture, text, uc->plain)) {
		if (glob_keep_going)
			g_printerr ("%s: returned Plain\n---%s---\n and expected Plain\n---%s---\n do not match at command %d\n", G_STRFUNC, text, uc->plain, cmd_index);
		else
			g_warning ("%s: returned Plain\n---%s---\n and expected Plain\n---%s---\n do not match at command %d", G_STRFUNC, text, uc->plain, cmd_index);

		e_content_editor_util_free_content_hash (content_hash);

		return FALSE;
	}

	e_content_editor_util_free_content_hash (content_hash);

	return TRUE;
}

static gboolean
test_utils_web_process_terminated_cb (WebKitWebView *web_view,
				      WebKitWebProcessTerminationReason reason,
				      gpointer user_data)
{
	g_warning ("%s: reason: %s", G_STRFUNC,
		reason == WEBKIT_WEB_PROCESS_CRASHED ? "crashed" :
		reason == WEBKIT_WEB_PROCESS_EXCEEDED_MEMORY_LIMIT ? "exceeded memory limit" :
		reason == WEBKIT_WEB_PROCESS_TERMINATED_BY_API ? "terminated by API" : "unknown reason");

	return FALSE;
}

/* <Control>+<Shift>+I */
#define WEBKIT_INSPECTOR_MOD  (GDK_CONTROL_MASK | GDK_SHIFT_MASK)
#define WEBKIT_INSPECTOR_KEY  (GDK_KEY_I)

static gboolean
wk_editor_key_press_event_cb (WebKitWebView *web_view,
			      GdkEventKey *event)
{
	WebKitWebInspector *inspector;
	gboolean handled = FALSE;

	inspector = webkit_web_view_get_inspector (web_view);

	if ((event->state & WEBKIT_INSPECTOR_MOD) == WEBKIT_INSPECTOR_MOD &&
	    event->keyval == WEBKIT_INSPECTOR_KEY) {
		webkit_web_inspector_show (inspector);
		handled = TRUE;
	}

	return handled;
}

typedef struct _CreateData {
	gpointer async_data;
	TestFixture *fixture;
	gboolean created;
} CreateData;

static void
test_utils_html_editor_created_cb (GObject *source_object,
				   GAsyncResult *result,
				   gpointer user_data)
{
	CreateData *create_data = user_data;
	TestFixture *fixture;
	EContentEditor *cnt_editor;
	GtkWidget *html_editor;
	GError *error = NULL;

	g_return_if_fail (create_data != NULL);

	create_data->created = TRUE;

	fixture = create_data->fixture;

	html_editor = e_html_editor_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create editor: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
		return;
	}

	fixture->editor = E_HTML_EDITOR (html_editor);

	g_object_set (G_OBJECT (fixture->editor),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);
	gtk_widget_show (GTK_WIDGET (fixture->editor));
	gtk_container_add (GTK_CONTAINER (fixture->window), GTK_WIDGET (fixture->editor));

	fixture->focus_tracker = e_focus_tracker_new (GTK_WINDOW (fixture->window));

	e_html_editor_connect_focus_tracker (fixture->editor, fixture->focus_tracker);

	/* Make sure this is off */
	test_utils_fixture_change_setting_boolean (fixture,
		"org.gnome.evolution.mail", "prompt-on-composer-mode-switch", FALSE);

	cnt_editor = e_html_editor_get_content_editor (fixture->editor);
	g_object_set (G_OBJECT (cnt_editor),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"height-request", 150,
		NULL);

	g_signal_connect (cnt_editor, "web-process-terminated",
		G_CALLBACK (test_utils_web_process_terminated_cb), NULL);

	if (WEBKIT_IS_WEB_VIEW (cnt_editor)) {
		WebKitSettings *web_settings;

		web_settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (cnt_editor));
		webkit_settings_set_enable_developer_extras (web_settings, TRUE);
		webkit_settings_set_enable_write_console_messages_to_stdout (web_settings, TRUE);

		g_signal_connect (
			cnt_editor, "key-press-event",
			G_CALLBACK (wk_editor_key_press_event_cb), NULL);

		if (!test_utils_get_multiple_web_processes () && !global_web_context) {
			WebKitWebContext *web_context;

			web_context = webkit_web_view_get_context (WEBKIT_WEB_VIEW (cnt_editor));
			global_web_context = G_OBJECT (g_object_ref (web_context));
		}
	}

	gtk_window_set_focus (GTK_WINDOW (fixture->window), GTK_WIDGET (cnt_editor));
	gtk_widget_show (fixture->window);

	test_utils_async_call_finish (create_data->async_data);
}

/* The tests do not use the 'user_data' argument, thus the functions avoid them and the typecast is needed. */
typedef void (* ETestFixtureFunc) (TestFixture *fixture, gconstpointer user_data);

void
test_utils_add_test (const gchar *name,
		     ETestFixtureSimpleFunc func)
{
	g_test_add (name, TestFixture, NULL,
		test_utils_fixture_set_up, (ETestFixtureFunc) func, test_utils_fixture_tear_down);
}

static void test_utils_async_call_free (gpointer async_data);

void
test_utils_fixture_set_up (TestFixture *fixture,
			   gconstpointer user_data)
{
	CreateData create_data;

	fixture->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	fixture->undo_stack = NULL;
	fixture->key_state = 0;

	if (test_utils_get_background ()) {
		gtk_window_set_keep_below (GTK_WINDOW (fixture->window), TRUE);
		gtk_window_set_focus_on_map (GTK_WINDOW (fixture->window), FALSE);
	}

	create_data.async_data = test_utils_async_call_prepare ();
	create_data.fixture = fixture;
	create_data.created = FALSE;

	e_html_editor_new (test_utils_html_editor_created_cb, &create_data);

	if (create_data.created)
		test_utils_async_call_free (create_data.async_data);
	else
		test_utils_async_call_wait (create_data.async_data, 60);

	g_warn_if_fail (fixture->editor != NULL);
	g_warn_if_fail (E_IS_HTML_EDITOR (fixture->editor));
}

static void
free_old_settings (gpointer ptr)
{
	TestSettings *data = ptr;

	if (data) {
		GSettings *settings;

		settings = e_util_ref_settings (data->schema);
		g_settings_set_value (settings, data->key, data->old_value);
		g_clear_object (&settings);

		g_variant_unref (data->old_value);
		g_free (data->schema);
		g_free (data->key);
		g_free (data);
	}
}

void
test_utils_fixture_tear_down (TestFixture *fixture,
			      gconstpointer user_data)
{
	g_clear_object (&fixture->focus_tracker);

	gtk_widget_destroy (GTK_WIDGET (fixture->window));
	fixture->editor = NULL;

	g_slist_free_full (fixture->settings, free_old_settings);
	fixture->settings = NULL;

	g_slist_free_full (fixture->undo_stack, undo_content_free);
	fixture->undo_stack = NULL;
}

void
test_utils_fixture_change_setting (TestFixture *fixture,
				   const gchar *schema,
				   const gchar *key,
				   GVariant *value)
{
	TestSettings *data;
	GSettings *settings;

	g_return_if_fail (fixture != NULL);
	g_return_if_fail (schema != NULL);
	g_return_if_fail (key != NULL);
	g_return_if_fail (value != NULL);

	g_variant_ref_sink (value);

	settings = e_util_ref_settings (schema);

	data = g_new0 (TestSettings, 1);
	data->schema = g_strdup (schema);
	data->key = g_strdup (key);
	data->old_value = g_settings_get_value (settings, key);

	/* Use prepend, thus the restore comes in the opposite order, thus a change
	   of the same key is not a problem. */
	fixture->settings = g_slist_prepend (fixture->settings, data);

	g_settings_set_value (settings, key, value);

	g_clear_object (&settings);
	g_variant_unref (value);
}

void
test_utils_fixture_change_setting_boolean (TestFixture *fixture,
					   const gchar *schema,
					   const gchar *key,
					   gboolean value)
{
	test_utils_fixture_change_setting (fixture, schema, key, g_variant_new_boolean (value));
}

void
test_utils_fixture_change_setting_int32 (TestFixture *fixture,
					 const gchar *schema,
					 const gchar *key,
					 gint value)
{
	test_utils_fixture_change_setting (fixture, schema, key, g_variant_new_int32 (value));
}

void
test_utils_fixture_change_setting_string (TestFixture *fixture,
					  const gchar *schema,
					  const gchar *key,
					  const gchar *value)
{
	test_utils_fixture_change_setting (fixture, schema, key, g_variant_new_string (value));
}

static void
test_utils_flush_main_context (void)
{
	GMainContext *main_context;

	main_context = g_main_context_default ();

	while (g_main_context_pending (main_context)) {
		g_main_context_iteration (main_context, FALSE);
	}
}

static void
test_utils_async_call_free (gpointer async_data)
{
	GMainLoop *loop = async_data;

	test_utils_flush_main_context ();

	g_main_loop_unref (loop);
}

gpointer
test_utils_async_call_prepare (void)
{
	return g_main_loop_new (NULL, FALSE);
}

typedef struct _AsynCallData {
	GMainLoop *loop;
	gboolean timeout_reached;
} AsyncCallData;

static gboolean
test_utils_async_call_timeout_reached_cb (gpointer user_data)
{
	AsyncCallData *async_call_data = user_data;

	g_return_val_if_fail (async_call_data != NULL, FALSE);
	g_return_val_if_fail (async_call_data->loop != NULL, FALSE);
	g_return_val_if_fail (!async_call_data->timeout_reached, FALSE);

	if (!g_source_is_destroyed (g_main_current_source ())) {
		async_call_data->timeout_reached = TRUE;
		g_main_loop_quit (async_call_data->loop);
	}

	return FALSE;
}

gboolean
test_utils_async_call_wait (gpointer async_data,
			    guint timeout_seconds)
{
	GMainLoop *loop = async_data;
	AsyncCallData async_call_data;
	GSource *source = NULL;

	g_return_val_if_fail (loop != NULL, FALSE);

	async_call_data.loop = loop;
	async_call_data.timeout_reached = FALSE;

	/* 0 is to wait forever */
	if (timeout_seconds > 0) {
		source = g_timeout_source_new_seconds (timeout_seconds);
		g_source_set_callback (source, test_utils_async_call_timeout_reached_cb, &async_call_data, NULL);
		g_source_attach (source, NULL);
	}

	g_main_loop_run (loop);

	if (source) {
		g_source_destroy (source);
		g_source_unref (source);
	}

	test_utils_async_call_free (async_data);

	return !async_call_data.timeout_reached;
}

gboolean
test_utils_async_call_finish (gpointer async_data)
{
	GMainLoop *loop = async_data;

	g_return_val_if_fail (loop != NULL, FALSE);

	g_main_loop_quit (loop);

	return FALSE;
}

gboolean
test_utils_wait_milliseconds (guint milliseconds)
{
	gpointer async_data;

	async_data = test_utils_async_call_prepare ();
	g_timeout_add (milliseconds, test_utils_async_call_finish, async_data);

	return test_utils_async_call_wait (async_data, milliseconds / 1000 + 1);
}

static void
test_utils_send_key_event (GtkWidget *widget,
			   GdkEventType type,
			   guint keyval,
			   guint state)
{
	GdkKeymap *keymap;
	GdkKeymapKey *keys = NULL;
	gint n_keys;
	GdkEvent *event;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	event = gdk_event_new (type);
	event->key.is_modifier =
		keyval == GDK_KEY_Shift_L ||
		keyval == GDK_KEY_Shift_R ||
		keyval == GDK_KEY_Control_L ||
		keyval == GDK_KEY_Control_R ||
		keyval == GDK_KEY_Alt_L ||
		keyval == GDK_KEY_Alt_R;
	event->key.keyval = keyval;
	event->key.state = state;
	event->key.window = g_object_ref (gtk_widget_get_window (widget));
	event->key.send_event = TRUE;
	event->key.length = 0;
	event->key.string = NULL;
	event->key.hardware_keycode = 0;
	event->key.group = 0;
	event->key.time = GDK_CURRENT_TIME;

	gdk_event_set_device (event, gdk_seat_get_keyboard (gdk_display_get_default_seat (gtk_widget_get_display (widget))));

	keymap = gdk_keymap_get_for_display (gtk_widget_get_display (widget));
	if (gdk_keymap_get_entries_for_keyval (keymap, keyval, &keys, &n_keys)) {
		if (n_keys > 0) {
			event->key.hardware_keycode = keys[0].keycode;
			event->key.group = keys[0].group;
		}

		g_free (keys);
	}

	gtk_main_do_event (event);

	test_utils_wait_milliseconds (event_processing_delay_ms);

	gdk_event_free (event);
}

gboolean
test_utils_type_text (TestFixture *fixture,
		      const gchar *text)
{
	GtkWidget *widget;

	g_return_val_if_fail (fixture != NULL, FALSE);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), FALSE);

	widget = GTK_WIDGET (e_html_editor_get_content_editor (fixture->editor));
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	g_return_val_if_fail (text != NULL, FALSE);
	g_return_val_if_fail (g_utf8_validate (text, -1, NULL), FALSE);

	while (*text) {
		guint keyval;
		gunichar unichar;

		unichar = g_utf8_get_char (text);
		text = g_utf8_next_char (text);

		switch (unichar) {
		case '\n':
			keyval = GDK_KEY_Return;
			break;
		case '\t':
			keyval = GDK_KEY_Tab;
			break;
		case '\b':
			keyval = GDK_KEY_BackSpace;
			break;
		default:
			keyval = gdk_unicode_to_keyval (unichar);
			break;
		}

		test_utils_send_key_event (widget, GDK_KEY_PRESS, keyval, fixture->key_state);
		test_utils_send_key_event (widget, GDK_KEY_RELEASE, keyval, fixture->key_state);
	}

	test_utils_wait_milliseconds (event_processing_delay_ms);

	return TRUE;
}

typedef struct _HTMLEqualData {
	gpointer async_data;
	gboolean equal;
} HTMLEqualData;

static void
test_html_equal_done_cb (GObject *source_object,
			 GAsyncResult *result,
			 gpointer user_data)
{
	HTMLEqualData *hed = user_data;
	JSCException *exception;
	JSCValue *js_value;
	GError *error = NULL;

	g_return_if_fail (hed != NULL);

	js_value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (source_object), result, &error);

	g_assert_no_error (error);
	g_clear_error (&error);

	g_assert_nonnull (js_value);
	g_assert_true (jsc_value_is_boolean (js_value));

	hed->equal = jsc_value_to_boolean (js_value);

	exception = jsc_context_get_exception (jsc_value_get_context (js_value));

	if (exception) {
		g_warning ("Failed to call EvoEditorTest.isHTMLEqual: %s", jsc_exception_get_message (exception));
		jsc_context_clear_exception (jsc_value_get_context (js_value));
	}

	g_object_unref (js_value);

	test_utils_async_call_finish (hed->async_data);
}

gboolean
test_utils_html_equal (TestFixture *fixture,
		       const gchar *html1,
		       const gchar *html2)
{
	EContentEditor *cnt_editor;
	gchar *script;
	HTMLEqualData hed;

	g_return_val_if_fail (fixture != NULL, FALSE);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), FALSE);
	g_return_val_if_fail (html1 != NULL, FALSE);
	g_return_val_if_fail (html2 != NULL, FALSE);

	cnt_editor = e_html_editor_get_content_editor (fixture->editor);
	g_return_val_if_fail (cnt_editor != NULL, FALSE);
	g_return_val_if_fail (WEBKIT_IS_WEB_VIEW (cnt_editor), FALSE);

	script = e_web_view_jsc_printf_script (
		"var EvoEditorTest = {};\n"
		"EvoEditorTest.fixupElems = function(parent) {\n"
		"	var ii, elems = parent.getElementsByTagName(\"BLOCKQUOTE\");\n"
		"	for (ii = 0; ii < elems.length; ii++) {\n"
		"		elems[ii].removeAttribute(\"spellcheck\");\n"
		"	}\n"
		"	elems = parent.getElementsByTagName(\"STYLE\");\n"
		"	for (ii = elems.length; ii--;) {\n"
		"		elems[ii].remove();\n"
		"	}\n"
		"}\n"
		"EvoEditorTest.isHTMLEqual = function(html1, html2) {\n"
		"	var elem1, elem2;\n"
		"	elem1 = document.createElement(\"testHtmlEqual\");\n"
		"	elem2 = document.createElement(\"testHtmlEqual\");\n"
		"	elem1.innerHTML = html1.replace(/&nbsp;/g, \" \").replace(/ /g, \" \");\n"
		"	elem2.innerHTML = html2.replace(/&nbsp;/g, \" \").replace(/ /g, \" \");\n"
		"	EvoEditorTest.fixupElems(elem1);\n"
		"	EvoEditorTest.fixupElems(elem2);\n"
		"	elem1.normalize();\n"
		"	elem2.normalize();\n"
		"	return elem1.isEqualNode(elem2);\n"
		"}\n"
		"EvoEditorTest.isHTMLEqual(%s, %s);", html1, html2);

	hed.async_data = test_utils_async_call_prepare ();
	hed.equal = FALSE;

	webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (cnt_editor), script, -1,
		NULL, NULL, NULL, test_html_equal_done_cb, &hed);

	test_utils_async_call_wait (hed.async_data, 10);

	g_free (script);

	return hed.equal;
}

static gboolean
test_utils_process_sequence (TestFixture *fixture,
			     const gchar *sequence)
{
	GtkWidget *widget;
	const gchar *seq;
	guint keyval;
	gboolean success = TRUE;

	g_return_val_if_fail (fixture != NULL, FALSE);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), FALSE);
	g_return_val_if_fail (sequence != NULL, FALSE);

	widget = GTK_WIDGET (e_html_editor_get_content_editor (fixture->editor));
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	for (seq = sequence; *seq && success; seq++) {
		gboolean call_press = TRUE, call_release = TRUE;
		guint change_state = fixture->key_state;

		switch (*seq) {
		case 'S': /* Shift key press */
			keyval = GDK_KEY_Shift_L;

			if ((fixture->key_state & GDK_SHIFT_MASK) != 0) {
				success = FALSE;
				g_warning ("%s: Shift is already pressed", G_STRFUNC);
			} else {
				change_state |= GDK_SHIFT_MASK;
			}
			call_release = FALSE;
			break;
		case 's': /* Shift key release */
			keyval = GDK_KEY_Shift_L;

			if ((fixture->key_state & GDK_SHIFT_MASK) == 0) {
				success = FALSE;
				g_warning ("%s: Shift is already released", G_STRFUNC);
			} else {
				change_state &= ~GDK_SHIFT_MASK;
			}
			call_press = FALSE;
			break;
		case 'C': /* Ctrl key press */
			keyval = GDK_KEY_Control_L;

			if ((fixture->key_state & GDK_CONTROL_MASK) != 0) {
				success = FALSE;
				g_warning ("%s: Control is already pressed", G_STRFUNC);
			} else {
				change_state |= GDK_CONTROL_MASK;
			}
			call_release = FALSE;
			break;
		case 'c': /* Ctrl key release */
			keyval = GDK_KEY_Control_L;

			if ((fixture->key_state & GDK_CONTROL_MASK) == 0) {
				success = FALSE;
				g_warning ("%s: Control is already released", G_STRFUNC);
			} else {
				change_state &= ~GDK_CONTROL_MASK;
			}
			call_press = FALSE;
			break;
		case 'A': /* Alt key press */
			keyval = GDK_KEY_Alt_L;

			if ((fixture->key_state & GDK_MOD1_MASK) != 0) {
				success = FALSE;
				g_warning ("%s: Alt is already pressed", G_STRFUNC);
			} else {
				change_state |= GDK_MOD1_MASK;
			}
			call_release = FALSE;
			break;
		case 'a': /* Alt key release */
			keyval = GDK_KEY_Alt_L;

			if ((fixture->key_state & GDK_MOD1_MASK) == 0) {
				success = FALSE;
				g_warning ("%s: Alt is already released", G_STRFUNC);
			} else {
				change_state &= ~GDK_MOD1_MASK;
			}
			call_press = FALSE;
			break;
		case 'h': /* Home key press + release */
			keyval = GDK_KEY_Home;
			break;
		case 'e': /* End key press + release */
			keyval = GDK_KEY_End;
			break;
		case 'P': /* Page-Up key press + release */
			keyval = GDK_KEY_Page_Up;
			break;
		case 'p': /* Page-Down key press + release */
			keyval = GDK_KEY_Page_Down;
			break;
		case 'l': /* Arrow-Left key press + release */
			keyval = GDK_KEY_Left;
			break;
		case 'r': /* Arrow-Right key press + release */
			keyval = GDK_KEY_Right;
			break;
		case 'u': /* Arrow-Up key press + release */
			keyval = GDK_KEY_Up;
			break;
		case 'd': /* Arrow-Down key press + release */
			keyval = GDK_KEY_Down;
			break;
		case 'D': /* Delete key press + release */
			keyval = GDK_KEY_Delete;
			break;
		case 'b': /* Backspace key press + release */
			keyval = GDK_KEY_BackSpace;
			break;
		case 't': /* Tab key press + release */
			keyval = GDK_KEY_Tab;
			break;
		case 'n': /* Return key press + release */
			keyval = GDK_KEY_Return;
			break;
		case 'i': /* Insert key press + release */
			keyval = GDK_KEY_Insert;
			break;
		case '^': /* Escape key press + release */
			keyval = GDK_KEY_Escape;
			break;
		default:
			success = FALSE;
			g_warning ("%s: Unknown sequence command '%c' in sequence '%s'", G_STRFUNC, *seq, sequence);
			break;
		}

		if (success) {
			if (call_press)
				test_utils_send_key_event (widget, GDK_KEY_PRESS, keyval, fixture->key_state);

			if (call_release)
				test_utils_send_key_event (widget, GDK_KEY_RELEASE, keyval, fixture->key_state);
		}

		fixture->key_state = change_state;
	}

	test_utils_wait_milliseconds (event_processing_delay_ms);

	return success;
}

static gboolean
test_utils_execute_action (TestFixture *fixture,
			   const gchar *action_name)
{
	EUIAction *action;

	g_return_val_if_fail (fixture != NULL, FALSE);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), FALSE);
	g_return_val_if_fail (action_name != NULL, FALSE);

	action = e_html_editor_get_action (fixture->editor, action_name);
	if (action) {
		GVariant *target;

		target = e_ui_action_ref_target (action);
		g_action_activate (G_ACTION (action), target);
		g_clear_pointer (&target, g_variant_unref);
	} else {
		g_warning ("%s: Failed to find action '%s'", G_STRFUNC, action_name);
		return FALSE;
	}

	return TRUE;
}

static gboolean
test_utils_set_font_name (TestFixture *fixture,
			  const gchar *font_name)
{
	EContentEditor *cnt_editor;

	g_return_val_if_fail (fixture != NULL, FALSE);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), FALSE);

	cnt_editor = e_html_editor_get_content_editor (fixture->editor);
	e_content_editor_set_font_name (cnt_editor, font_name);

	return TRUE;
}

/* Expects only the part like "undo" [ ":" number ] */
static gint
test_utils_maybe_extract_undo_number (const gchar *command)
{
	const gchar *ptr;
	gint number;

	g_return_val_if_fail (command != NULL, -1);

	ptr = strchr (command, ':');
	if (!ptr)
		return 1;

	number = atoi (ptr + 1);
	g_return_val_if_fail (number > 0, -1);

	return number;
}

static const UndoContent *
test_utils_pick_undo_content (const GSList *undo_stack,
			      gint number)
{
	const GSList *link;

	g_return_val_if_fail (undo_stack != NULL, NULL);

	number--;
	for (link = undo_stack; link && number > 0; link = g_slist_next (link)) {
		number--;
	}

	g_return_val_if_fail (link != NULL, NULL);
	g_return_val_if_fail (link->data != NULL, NULL);

	return link->data;
}

/* Each line of 'commands' contains one command.

   commands  = command *("\n" command)

   command   = actioncmd ; Execute an action
             / modecmd   ; Change editor mode to HTML or Plain Text
             / fnmcmd    ; Set font name
             / seqcmd    ; Sequence of special key strokes
             / typecmd   ; Type a text
             / undocmd   ; Undo/redo commands
             / waitcmd   ; Wait command

   actioncmd = "action:" name

   modecmd   = "mode:" ("html" / "plain")

   fnmcmd    = "font-name:" name

   seqcmd    = "seq:" sequence

   sequence  = "S" ; Shift key press
             / "s" ; Shift key release
             / "C" ; Ctrl key press
             / "c" ; Ctrl key release
             / "A" ; Alt key press
             / "a" ; Alt key release
             / "h" ; Home key press + release
             / "e" ; End key press + release
             / "P" ; Page-Up key press + release
             / "p" ; Page-Down key press + release
             / "l" ; Arrow-Left key press + release
             / "r" ; Arrow-Right key press + release
             / "u" ; Arrow-Up key press + release
             / "d" ; Arrow-Down key press + release
             / "D" ; Delete key press + release
             / "b" ; Backspace key press + release
             / "t" ; Tab key press + release
             / "n" ; Return key press + release
             / "i" ; Insert key press + release
	     / "^" ; Escape key press + release

   typecmd   = "type:" text ; the 'text' can contain escaped letters with a backslash, like "\\n" transforms into "\n"

   undocmd   = "undo:" undotype

   undotype  = "undo" [ ":" number ] ; Call 'undo', number-times; if 'number' is not provided, then call it exactly once
             / "redo" [ ":" number ] ; Call 'redo', number-times; if 'number' is not provided, then call it exactly once
	     / "save"                ; Save current content of the editor for later tests
	     / "drop" [ ":" number ] ; Forgets saved content, if 'number' is provided, then top number saves are forgotten
	     / "test" [ ":" number ] ; Tests current editor content against any previously saved state; the optional
                                     ; 'number' argument can be used to specify which exact previous state to use

   waitcmd   = "wait:" milliseconds  ; waits for 'milliseconds'
 */
gboolean
test_utils_process_commands (TestFixture *fixture,
			     const gchar *commands)
{
	gchar **cmds;
	gint cc;
	gboolean success = TRUE;

	g_return_val_if_fail (fixture != NULL, FALSE);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), FALSE);
	g_return_val_if_fail (commands != NULL, FALSE);

	cmds = g_strsplit (commands, "\n", -1);
	for (cc = 0; cmds && cmds[cc] && success; cc++) {
		const gchar *command = cmds[cc];

		if (g_str_has_prefix (command, "action:")) {
			test_utils_execute_action (fixture, command + 7);
		} else if (g_str_has_prefix (command, "mode:")) {
			const gchar *mode_change = command + 5;

			if (g_str_equal (mode_change, "html")) {
				test_utils_execute_action (fixture, "mode-html");
			} else if (g_str_equal (mode_change, "plain")) {
				test_utils_execute_action (fixture, "mode-plain");
			} else {
				success = FALSE;
				g_warning ("%s: Unknown mode '%s'", G_STRFUNC, mode_change);
			}
		} else if (g_str_has_prefix (command, "font-name:")) {
			success = test_utils_set_font_name (fixture, command + 10);
		} else if (g_str_has_prefix (command, "seq:")) {
			success = test_utils_process_sequence (fixture, command + 4);
		} else if (g_str_has_prefix (command, "type:")) {
			gchar *text;

			text = g_strcompress (command + 5);
			success = test_utils_type_text (fixture, text);
			if (!success)
				g_warning ("%s: Failed to type text '%s'", G_STRFUNC, text);
			g_free (text);
		} else if (g_str_has_prefix (command, "undo:")) {
			gint number;

			command += 5;

			if (g_str_equal (command, "undo") || g_str_has_prefix (command, "undo:")) {
				number = test_utils_maybe_extract_undo_number (command);
				while (number > 0 && success) {
					success = test_utils_execute_action (fixture, "undo");
					number--;
				}
			} else if (g_str_has_prefix (command, "redo") || g_str_has_prefix (command, "redo:")) {
				number = test_utils_maybe_extract_undo_number (command);
				while (number > 0 && success) {
					success = test_utils_execute_action (fixture, "redo");
					number--;
				}
			} else if (g_str_equal (command, "save")) {
				UndoContent *uc;

				uc = undo_content_new (fixture);
				fixture->undo_stack = g_slist_prepend (fixture->undo_stack, uc);
			} else if (g_str_equal (command, "drop") || g_str_has_prefix (command, "drop:")) {
				number = test_utils_maybe_extract_undo_number (command);
				g_warn_if_fail (number <= g_slist_length (fixture->undo_stack));

				while (number > 0 && fixture->undo_stack) {
					UndoContent *uc = fixture->undo_stack->data;

					fixture->undo_stack = g_slist_remove (fixture->undo_stack, uc);
					undo_content_free (uc);
					number--;
				}
			} else if (g_str_equal (command, "test") || g_str_has_prefix (command, "test:")) {
				const UndoContent *uc;

				number = test_utils_maybe_extract_undo_number (command);
				uc = test_utils_pick_undo_content (fixture->undo_stack, number);
				success = uc && undo_content_test (fixture, uc, cc);
			} else {
				g_warning ("%s: Unknown command 'undo:%s'", G_STRFUNC, command);
				success = FALSE;
			}

			test_utils_wait_milliseconds (event_processing_delay_ms);
		} else if (g_str_has_prefix (command, "wait:")) {
			test_utils_wait_milliseconds (atoi (command + 5));
		} else if (*command) {
			g_warning ("%s: Unknown command '%s'", G_STRFUNC, command);
			success = FALSE;
		}

		/* Wait at least 100 ms, to give a chance to move the cursor and
		   other things for WebKit, for example before executing actions. */
		test_utils_wait_milliseconds (MAX (event_processing_delay_ms, 100));
	}

	g_strfreev (cmds);

	if (success) {
		/* Give the editor some time to finish any ongoing async operations */
		test_utils_wait_milliseconds (MAX (event_processing_delay_ms, 100));
	}

	return success;
}

gboolean
test_utils_run_simple_test (TestFixture *fixture,
			    const gchar *commands,
			    const gchar *expected_html,
			    const gchar *expected_plain)
{
	EContentEditor *cnt_editor;
	EContentEditorContentHash *content_hash;
	const gchar *text;

	g_return_val_if_fail (fixture != NULL, FALSE);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), FALSE);
	g_return_val_if_fail (commands != NULL, FALSE);

	cnt_editor = e_html_editor_get_content_editor (fixture->editor);

	if (!test_utils_process_commands (fixture, commands))
		return FALSE;

	content_hash = test_utils_get_editor_content_hash_sync (cnt_editor, E_CONTENT_EDITOR_GET_TO_SEND_HTML | E_CONTENT_EDITOR_GET_TO_SEND_PLAIN);

	if (expected_html) {
		text = e_content_editor_util_get_content_data (content_hash, E_CONTENT_EDITOR_GET_TO_SEND_HTML);
		g_return_val_if_fail (text != NULL, FALSE);

		if (!test_utils_html_equal (fixture, text, expected_html)) {
			if (glob_keep_going)
				g_printerr ("%s: returned HTML\n---%s---\n and expected HTML\n---%s---\n do not match\n", G_STRFUNC, text, expected_html);
			else
				g_warning ("%s: returned HTML\n---%s---\n and expected HTML\n---%s---\n do not match", G_STRFUNC, text, expected_html);

			e_content_editor_util_free_content_hash (content_hash);

			return FALSE;
		}
	}

	if (expected_plain) {
		text = e_content_editor_util_get_content_data (content_hash, E_CONTENT_EDITOR_GET_TO_SEND_PLAIN);
		g_return_val_if_fail (text != NULL, FALSE);

		if (!test_utils_html_equal (fixture, text, expected_plain)) {
			if (glob_keep_going)
				g_printerr ("%s: returned Plain\n---%s---\n and expected Plain\n---%s---\n do not match\n", G_STRFUNC, text, expected_plain);
			else
				g_warning ("%s: returned Plain\n---%s---\n and expected Plain\n---%s---\n do not match", G_STRFUNC, text, expected_plain);

			e_content_editor_util_free_content_hash (content_hash);

			return FALSE;
		}
	}

	e_content_editor_util_free_content_hash (content_hash);

	return TRUE;
}

void
test_utils_insert_content (TestFixture *fixture,
			   const gchar *content,
			   EContentEditorInsertContentFlags flags)
{
	EContentEditor *cnt_editor;

	g_return_if_fail (fixture != NULL);
	g_return_if_fail (E_IS_HTML_EDITOR (fixture->editor));
	g_return_if_fail (content != NULL);

	cnt_editor = e_html_editor_get_content_editor (fixture->editor);
	e_content_editor_insert_content (cnt_editor, content, flags);
}

void
test_utils_set_clipboard_text (const gchar *text,
			       gboolean is_html)
{
	GtkClipboard *clipboard;

	g_return_if_fail (text != NULL);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	g_return_if_fail (clipboard != NULL);

	gtk_clipboard_clear (clipboard);

	if (is_html) {
		e_clipboard_set_html (clipboard, text, -1);
	} else {
		gtk_clipboard_set_text (clipboard, text, -1);
	}
}

gchar *
test_utils_get_clipboard_text (gboolean request_html)
{
	GtkClipboard *clipboard;
	gchar *text;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	g_return_val_if_fail (clipboard != NULL, NULL);

	if (request_html) {
		g_return_val_if_fail (e_clipboard_wait_is_html_available (clipboard), NULL);
		text = e_clipboard_wait_for_html (clipboard);
	} else {
		g_return_val_if_fail (gtk_clipboard_wait_is_text_available (clipboard), NULL);
		text = gtk_clipboard_wait_for_text (clipboard);
	}

	g_return_val_if_fail (text != NULL, NULL);

	return text;
}

EContentEditor *
test_utils_get_content_editor (TestFixture *fixture)
{
	EContentEditor *cnt_editor;

	g_return_val_if_fail (fixture != NULL, NULL);
	g_return_val_if_fail (E_IS_HTML_EDITOR (fixture->editor), NULL);

	cnt_editor = e_html_editor_get_content_editor (fixture->editor);
	return cnt_editor;
}

gchar *
test_utils_dup_image_uri (const gchar *path)
{
	gchar *image_uri = NULL;
	GError *error = NULL;

	if (path && strchr (path, G_DIR_SEPARATOR)) {
		image_uri = g_filename_to_uri (path, NULL, &error);
	} else {
		gchar *filename;

		filename = e_icon_factory_get_icon_filename (path, GTK_ICON_SIZE_MENU);
		if (filename) {
			image_uri = g_filename_to_uri (filename, NULL, &error);
			g_free (filename);
		} else {
			g_set_error (&error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Icon '%s' not found", path);
		}
	}

	if (image_uri) {
		gchar *tmp;

		tmp = g_strconcat ("evo-", image_uri, NULL);
		g_free (image_uri);
		image_uri = tmp;
	}

	g_assert_no_error (error);
	g_assert_nonnull (image_uri);

	return image_uri;
}

static void
test_utils_insert_signature_done_cb (GObject *source_object,
				     GAsyncResult *result,
				     gpointer user_data)
{
	gpointer async_data = user_data;
	JSCException *exception;
	JSCValue *js_value;
	GError *error = NULL;

	g_return_if_fail (async_data != NULL);

	js_value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (source_object), result, &error);

	g_assert_no_error (error);
	g_clear_error (&error);

	g_assert_nonnull (js_value);

	exception = jsc_context_get_exception (jsc_value_get_context (js_value));

	if (exception) {
		g_warning ("Failed to call EvoEditor.InsertSignature: %s", jsc_exception_get_message (exception));
		jsc_context_clear_exception (jsc_value_get_context (js_value));
	}

	g_object_unref (js_value);

	test_utils_async_call_finish (async_data);
}

void
test_utils_insert_signature (TestFixture *fixture,
			     const gchar *content,
			     gboolean is_html,
			     const gchar *uid,
			     gboolean start_bottom,
			     gboolean top_signature,
			     gboolean add_delimiter)
{
	EContentEditor *cnt_editor;
	gchar *script;
	gpointer async_data;

	g_return_if_fail (fixture != NULL);
	g_return_if_fail (E_IS_HTML_EDITOR (fixture->editor));
	g_return_if_fail (content != NULL);
	g_return_if_fail (uid != NULL);

	cnt_editor = e_html_editor_get_content_editor (fixture->editor);
	g_return_if_fail (cnt_editor != NULL);
	g_return_if_fail (WEBKIT_IS_WEB_VIEW (cnt_editor));

	script = e_web_view_jsc_printf_script (
		"EvoEditor.InsertSignature(%s, %x, false, %s, false, false, true, %x, %x, %x);",
		content, is_html, uid, start_bottom, top_signature, add_delimiter);

	async_data = test_utils_async_call_prepare ();

	webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (cnt_editor), script, -1,
		NULL, NULL, NULL, test_utils_insert_signature_done_cb, async_data);

	test_utils_async_call_wait (async_data, 10);

	g_free (script);
}
