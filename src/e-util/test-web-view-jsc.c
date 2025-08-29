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

#include <glib.h>
#include <glib/gstdio.h>

#include <locale.h>
#include <e-util/e-util.h>

enum {
	LOAD_ALL = -1,
	LOAD_MAIN = 0,
	LOAD_FRM1 = 1,
	LOAD_FRM1_1 = 2,
	LOAD_FRM2 = 3
};

typedef struct _TestFlagClass {
	GObjectClass parent_class;
} TestFlagClass;

typedef struct _TestFlag {
	GObject parent;
	gboolean is_set;
} TestFlag;

GType test_flag_get_type (void);

G_DEFINE_TYPE (TestFlag, test_flag, G_TYPE_OBJECT)

enum {
	FLAGGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
test_flag_class_init (TestFlagClass *klass)
{
	signals[FLAGGED] = g_signal_new (
		"flagged",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);
}

static void
test_flag_init (TestFlag *flag)
{
	flag->is_set = FALSE;
}

static void
test_flag_set (TestFlag *flag)
{
	flag->is_set = TRUE;

	g_signal_emit (flag, signals[FLAGGED], 0, NULL);
}

typedef struct _TestFixture {
	GtkWidget *window;
	WebKitWebView *web_view;

	TestFlag *flag;
} TestFixture;

typedef void (* ETestFixtureSimpleFunc) (TestFixture *fixture);

/* The tests do not use the 'user_data' argument, thus the functions avoid them and the typecast is needed. */
typedef void (* ETestFixtureFunc) (TestFixture *fixture, gconstpointer user_data);

static gboolean
window_key_press_event_cb (GtkWindow *window,
			   GdkEventKey *event,
			   gpointer user_data)
{
	WebKitWebView *web_view = user_data;
	WebKitWebInspector *inspector;
	gboolean handled = FALSE;

	inspector = webkit_web_view_get_inspector (web_view);

	if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK) &&
	    event->keyval == GDK_KEY_I) {
		webkit_web_inspector_show (inspector);
		handled = TRUE;
	}

	return handled;
}

static gboolean
window_delete_event_cb (GtkWidget *widget,
			GdkEvent *event,
			gpointer user_data)
{
	TestFixture *fixture = user_data;

	gtk_widget_destroy (fixture->window);
	fixture->window = NULL;
	fixture->web_view = NULL;

	test_flag_set (fixture->flag);

	return TRUE;
}

static void
test_utils_fixture_set_up (TestFixture *fixture,
			   gconstpointer user_data)
{
	WebKitSettings *settings;

	fixture->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_window_set_default_size (GTK_WINDOW (fixture->window), 320, 240);

	fixture->web_view = WEBKIT_WEB_VIEW (e_web_view_new ());
	g_object_set (G_OBJECT (fixture->web_view),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);

	gtk_container_add (GTK_CONTAINER (fixture->window), GTK_WIDGET (fixture->web_view));

	settings = webkit_web_view_get_settings (fixture->web_view);
	webkit_settings_set_enable_developer_extras (settings, TRUE);
	webkit_settings_set_enable_write_console_messages_to_stdout (settings, TRUE);

	g_signal_connect (
		fixture->window, "key-press-event",
		G_CALLBACK (window_key_press_event_cb), fixture->web_view);

	g_signal_connect (
		fixture->window, "delete-event",
		G_CALLBACK (window_delete_event_cb), fixture);

	gtk_widget_show_all (fixture->window);

	fixture->flag = g_object_new (test_flag_get_type (), NULL);
}

static void
test_utils_fixture_tear_down (TestFixture *fixture,
			      gconstpointer user_data)
{
	if (fixture->window) {
		gtk_widget_destroy (fixture->window);
		fixture->web_view = NULL;
	}

	g_clear_object (&fixture->flag);
}

static void
test_utils_add_test (const gchar *name,
		     ETestFixtureSimpleFunc func)
{
	g_test_add (name, TestFixture, NULL,
		test_utils_fixture_set_up, (ETestFixtureFunc) func, test_utils_fixture_tear_down);
}

static void
test_utils_wait (TestFixture *fixture)
{
	GMainLoop *loop;
	gulong handler_id;

	g_return_if_fail (fixture != NULL);
	g_return_if_fail (fixture->window != NULL);
	g_return_if_fail (fixture->flag != NULL);

	if (fixture->flag->is_set) {
		fixture->flag->is_set = FALSE;
		return;
	}

	loop = g_main_loop_new (NULL, FALSE);

	handler_id = g_signal_connect_swapped (fixture->flag, "flagged", G_CALLBACK (g_main_loop_quit), loop);

	g_main_loop_run (loop);
	g_main_loop_unref (loop);

	g_signal_handler_disconnect (fixture->flag, handler_id);

	fixture->flag->is_set = FALSE;
}

static void
test_utils_jsc_call_done_cb (GObject *source_object,
			     GAsyncResult *result,
			     gpointer user_data)
{
	gchar *script = user_data;
	JSCValue *value;
	GError *error = NULL;

	g_return_if_fail (script != NULL);

	value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (source_object), result, &error);

	if (error) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
		    (!g_error_matches (error, WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED) ||
		     /* WebKit can return empty error message, thus ignore those. */
		     (error->message && *(error->message))))
			g_warning ("Failed to call '%s' function: %s", script, error->message);
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

typedef struct _JSCCallData {
	TestFixture *fixture;
	const gchar *script;
	JSCValue **out_result;
} JSCCallData;

static void
test_utils_jsc_call_sync_done_cb (GObject *source_object,
				  GAsyncResult *result,
				  gpointer user_data)
{
	JSCCallData *jcd = user_data;
	JSCValue *value;
	GError *error = NULL;

	g_return_if_fail (jcd != NULL);

	value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (source_object), result, &error);

	if (error) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
		    (!g_error_matches (error, WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED) ||
		     /* WebKit can return empty error message, thus ignore those. */
		     (error->message && *(error->message))))
			g_warning ("Failed to call '%s': %s", jcd->script, error->message);
		g_clear_error (&error);
	}

	if (value) {
		JSCException *exception;

		exception = jsc_context_get_exception (jsc_value_get_context (value));

		if (exception) {
			g_warning ("Failed to call '%s': %s", jcd->script, jsc_exception_get_message (exception));
			jsc_context_clear_exception (jsc_value_get_context (value));
		} else if (jcd->out_result) {
			*(jcd->out_result) = value ? g_object_ref (value) : NULL;
		}

		g_clear_object (&value);
	}

	test_flag_set (jcd->fixture->flag);
}

static void
test_utils_jsc_call (TestFixture *fixture,
		     const gchar *script)
{
	g_return_if_fail (fixture != NULL);
	g_return_if_fail (fixture->web_view != NULL);
	g_return_if_fail (script != NULL);

	webkit_web_view_evaluate_javascript (fixture->web_view, script, -1, NULL, NULL, NULL, test_utils_jsc_call_done_cb, g_strdup (script));
}

static void
test_utils_jsc_call_sync (TestFixture *fixture,
			  const gchar *script,
			  JSCValue **out_result)
{
	JSCCallData jcd;

	g_return_if_fail (fixture != NULL);
	g_return_if_fail (fixture->web_view != NULL);
	g_return_if_fail (script != NULL);

	if (out_result)
		*out_result = NULL;

	jcd.fixture = fixture;
	jcd.script = script;
	jcd.out_result = out_result;

	webkit_web_view_evaluate_javascript (fixture->web_view, script, -1, NULL, NULL, NULL, test_utils_jsc_call_sync_done_cb, &jcd);

	test_utils_wait (fixture);
}

static gboolean
test_utils_jsc_call_bool_sync (TestFixture *fixture,
			       const gchar *script)
{
	JSCValue *result = NULL;
	gboolean res;

	test_utils_jsc_call_sync (fixture, script, &result);

	g_assert_nonnull (result);
	g_assert_true (jsc_value_is_boolean (result));

	res = jsc_value_to_boolean (result);

	g_clear_object (&result);

	return res;
}

static gint32
test_utils_jsc_call_int32_sync (TestFixture *fixture,
				const gchar *script)
{
	JSCValue *result = NULL;
	gint32 res;

	test_utils_jsc_call_sync (fixture, script, &result);

	g_assert_nonnull (result);
	g_assert_true (jsc_value_is_number (result));

	res = jsc_value_to_int32 (result);

	g_clear_object (&result);

	return res;
}

static gchar *
test_utils_jsc_call_string_sync (TestFixture *fixture,
				 const gchar *script)
{
	JSCValue *result = NULL;
	gchar *res;

	test_utils_jsc_call_sync (fixture, script, &result);

	g_assert_nonnull (result);
	g_assert_true (jsc_value_is_null (result) || jsc_value_is_string (result));

	if (jsc_value_is_null (result))
		res = NULL;
	else
		res = jsc_value_to_string (result);

	g_clear_object (&result);

	return res;
}

static void
test_utils_jsc_call_string_and_verify (TestFixture *fixture,
				       const gchar *script,
				       const gchar *expected_value)
{
	gchar *value;

	value = test_utils_jsc_call_string_sync (fixture, script);

	g_assert_cmpstr (value, ==, expected_value);

	g_free (value);
}

static void
test_utils_wait_noop (TestFixture *fixture)
{
	test_utils_jsc_call_sync (fixture, "javascript:void(0);", NULL);
}

static void
test_utils_iframe_loaded_cb (EWebView *web_view,
			     const gchar *iframe_id,
			     gpointer user_data)
{
	TestFixture *fixture = user_data;

	g_return_if_fail (fixture != NULL);

	test_flag_set (fixture->flag);
}

static void
test_utils_load_iframe_content (TestFixture *fixture,
				const gchar *iframe_id,
				const gchar *content)
{
	gchar *script;
	gulong handler_id;

	handler_id = g_signal_connect (fixture->web_view, "content-loaded",
		G_CALLBACK (test_utils_iframe_loaded_cb), fixture);

	script = e_web_view_jsc_printf_script ("Evo.SetIFrameContent(%s,%s)", iframe_id, content);

	test_utils_jsc_call (fixture, script);

	g_free (script);

	test_utils_wait (fixture);

	g_signal_handler_disconnect (fixture->web_view, handler_id);

	test_utils_wait_noop (fixture);
}

static void
load_changed_cb (WebKitWebView *web_view,
		 WebKitLoadEvent load_event,
		 gpointer user_data)
{
	TestFixture *fixture = user_data;

	g_return_if_fail (fixture != NULL);

	if (load_event == WEBKIT_LOAD_FINISHED)
		test_flag_set (fixture->flag);
}

static void
test_utils_load_string (TestFixture *fixture,
			const gchar *content)
{
	gulong handler_id;

	handler_id = g_signal_connect (fixture->web_view, "load-changed",
		G_CALLBACK (load_changed_cb), fixture);

	e_web_view_load_string (E_WEB_VIEW (fixture->web_view), content);

	test_utils_wait (fixture);

	g_signal_handler_disconnect (fixture->web_view, handler_id);

	test_utils_wait_noop (fixture);
}

static void
test_utils_load_body (TestFixture *fixture,
		      gint index)
{
	if (index == LOAD_MAIN || index == LOAD_ALL) {
		test_utils_load_string (fixture,
			"<html><body>"
			"Top<br>"
			"<input id=\"btn1\" class=\"cbtn1\" type=\"button\" value=\"Button1\"><br>"
			"<input id=\"chk1\" class=\"cchk1\" type=\"checkbox\" value=\"Check1\">"
			"<iframe id=\"frm1\" src=\"empty:///frm1\"></iframe><br>"
			"<iframe id=\"frm2\" src=\"empty:///frm2\"></iframe><br>"
			"<input id=\"btn3\" class=\"cbtn3\" type=\"button\" value=\"Button3\">"
			"<a name=\"dots\" id=\"dots1\" class=\"cdots\">...</a>"
			"</body></html>");
	}

	if (index == LOAD_FRM1 || index == LOAD_ALL) {
		test_utils_load_iframe_content (fixture, "frm1",
			"<html><body>"
			"frm1<br>"
			"<iframe id=\"frm1_1\" src=\"empty:///frm1_1\"></iframe><br>"
			"<input id=\"btn1\" class=\"cbtn1\" type=\"button\" value=\"Button1\">"
			"</body></html>");
	}

	if (index == LOAD_FRM1_1 || index == LOAD_ALL) {
		test_utils_load_iframe_content (fixture, "frm1_1",
			"<html><body>"
			"frm1_1<br>"
			"<input id=\"btn1\" class=\"cbtn1\" type=\"button\" value=\"Button1\">"
			"<input id=\"chk1\" class=\"cchk1\" type=\"checkbox\" value=\"Check1\">"
			"<a name=\"dots\" id=\"dots2\" class=\"cdots\">...</a>"
			"<input id=\"btn2\" class=\"cbtn2\" type=\"button\" value=\"Button2\">"
			"</body></html>");
	}

	if (index == LOAD_FRM2 || index == LOAD_ALL) {
		test_utils_load_iframe_content (fixture, "frm2",
			"<html><body>"
			"frm2<br>"
			"<input id=\"btn1\" class=\"cbtn1\" type=\"button\" value=\"Button1\">"
			"<input id=\"btn2\" class=\"cbtn2\" type=\"button\" value=\"Button2\">"
			"<input id=\"chk2\" class=\"cchk2\" type=\"checkbox\" value=\"Check2\">"
			"</body></html>");
	}
}

static void
test_jsc_object_properties (TestFixture *fixture)
{
	JSCValue *jsc_object = NULL;
	gchar *str;

	str = e_web_view_jsc_printf_script (
		"test_obj_props = function()\n"
		"{\n"
		"	var arrobj = {};\n"
		"	arrobj[\"btrue\"] = true;\n"
		"	arrobj[\"bfalse\"] = false;\n"
		"	arrobj[\"i2\"] = 2;\n"
		"	arrobj[\"i67890\"] = 67890;\n"
		"	arrobj[\"i-12345\"] = -12345;\n"
		"	arrobj[\"d-54.32\"] = -54.32;\n"
		"	arrobj[\"d67.89\"] = 67.89;\n"
		"	arrobj[\"s-it\"] = \"it\";\n"
		"	arrobj[\"s-Is\"] = \"Is\";\n"
		"	return arrobj;\n"
		"}\n"
		"test_obj_props();\n");

	test_utils_jsc_call_sync (fixture, str, &jsc_object);

	g_free (str);

	g_assert_nonnull (jsc_object);
	g_assert_true (jsc_value_is_object (jsc_object));

	g_assert_true (e_web_view_jsc_get_object_property_boolean (jsc_object, "btrue", FALSE));
	g_assert_true (!e_web_view_jsc_get_object_property_boolean (jsc_object, "bfalse", TRUE));
	g_assert_true (!e_web_view_jsc_get_object_property_boolean (jsc_object, "budenfined", FALSE));
	g_assert_true (e_web_view_jsc_get_object_property_boolean (jsc_object, "budenfined", TRUE));
	g_assert_cmpint (e_web_view_jsc_get_object_property_int32 (jsc_object, "i2", 0), ==, 2);
	g_assert_cmpint (e_web_view_jsc_get_object_property_int32 (jsc_object, "i67890", 0), ==, 67890);
	g_assert_cmpint (e_web_view_jsc_get_object_property_int32 (jsc_object, "i-12345", 0), ==, -12345);
	g_assert_cmpint (e_web_view_jsc_get_object_property_int32 (jsc_object, "iundefined", 333), ==, 333);
	g_assert_cmpfloat (e_web_view_jsc_get_object_property_double (jsc_object, "d-54.32", 0.0), ==, -54.32);
	g_assert_cmpfloat (e_web_view_jsc_get_object_property_double (jsc_object, "d67.89", 0.0), ==, 67.89);
	g_assert_cmpfloat (e_web_view_jsc_get_object_property_double (jsc_object, "dundefined", 123.456), ==, 123.456);

	str = e_web_view_jsc_get_object_property_string (jsc_object, "s-it", NULL);
	g_assert_cmpstr (str, ==, "it");
	g_free (str);

	str = e_web_view_jsc_get_object_property_string (jsc_object, "s-Is", NULL);
	g_assert_cmpstr (str, ==, "Is");
	g_free (str);

	str = e_web_view_jsc_get_object_property_string (jsc_object, "sundefined", "xxx");
	g_assert_cmpstr (str, ==, "xxx");
	g_free (str);

	str = e_web_view_jsc_get_object_property_string (jsc_object, "sundefined", NULL);
	g_assert_null (str);

	g_clear_object (&jsc_object);
}

static void
test_set_element_hidden (TestFixture *fixture)
{
	test_utils_load_body (fixture, LOAD_ALL);

	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn3\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn2\").hidden"));

	e_web_view_jsc_set_element_hidden (fixture->web_view, "", "btn1", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn3\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn2\").hidden"));

	e_web_view_jsc_set_element_hidden (fixture->web_view, "frm1_1", "btn1", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn3\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1\", \"btn1\").hidden"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn2\").hidden"));

	e_web_view_jsc_set_element_hidden (fixture->web_view, "frm2", "btn2", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn3\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1\", \"btn1\").hidden"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn1\").hidden"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn2\").hidden"));

	e_web_view_jsc_set_element_hidden (fixture->web_view, "", "btn1", FALSE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn3\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1\", \"btn1\").hidden"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn1\").hidden"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn2\").hidden"));

	e_web_view_jsc_set_element_hidden (fixture->web_view, "frm2", "btn1", FALSE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn3\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1\", \"btn1\").hidden"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn1\").hidden"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn2\").hidden"));

	e_web_view_jsc_set_element_hidden (fixture->web_view, "frm1_1", "btn1", FALSE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn3\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").hidden"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn1\").hidden"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn2\").hidden"));
}

static void
test_set_element_disabled (TestFixture *fixture)
{
	test_utils_load_body (fixture, LOAD_ALL);

	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn3\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn2\").disabled"));

	e_web_view_jsc_set_element_disabled (fixture->web_view, "", "btn1", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn3\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn2\").disabled"));

	e_web_view_jsc_set_element_disabled (fixture->web_view, "frm1_1", "btn1", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn3\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1\", \"btn1\").disabled"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn2\").disabled"));

	e_web_view_jsc_set_element_disabled (fixture->web_view, "frm2", "btn2", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn3\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1\", \"btn1\").disabled"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn1\").disabled"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn2\").disabled"));

	e_web_view_jsc_set_element_disabled (fixture->web_view, "", "btn1", FALSE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn3\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1\", \"btn1\").disabled"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn1\").disabled"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn2\").disabled"));

	e_web_view_jsc_set_element_disabled (fixture->web_view, "frm2", "btn1", FALSE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn3\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1\", \"btn1\").disabled"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn1\").disabled"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn2\").disabled"));

	e_web_view_jsc_set_element_disabled (fixture->web_view, "frm1_1", "btn1", FALSE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"btn3\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").disabled"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn1\").disabled"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"btn2\").disabled"));
}

static void
test_set_element_checked (TestFixture *fixture)
{
	test_utils_load_body (fixture, LOAD_ALL);

	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"chk1\").checked"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"chk1\").checked"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"chk2\").checked"));

	e_web_view_jsc_set_element_checked (fixture->web_view, "", "chk1", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"chk1\").checked"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"chk1\").checked"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"chk2\").checked"));

	e_web_view_jsc_set_element_checked (fixture->web_view, "frm1_1", "chk1", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"chk1\").checked"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"chk1\").checked"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"chk2\").checked"));

	e_web_view_jsc_set_element_checked (fixture->web_view, "", "chk1", FALSE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"chk1\").checked"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"chk1\").checked"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"chk2\").checked"));

	e_web_view_jsc_set_element_checked (fixture->web_view, "frm2", "chk2", FALSE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"chk1\").checked"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"chk1\").checked"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"chk2\").checked"));

	e_web_view_jsc_set_element_checked (fixture->web_view, "", "chk1", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"chk1\").checked"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"chk1\").checked"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"chk2\").checked"));

	e_web_view_jsc_set_element_checked (fixture->web_view, "frm1_1", "chk1", FALSE, NULL);
	e_web_view_jsc_set_element_checked (fixture->web_view, "frm2", "chk2", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"\", \"chk1\").checked"));
	g_assert_true (!test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm1_1\", \"chk1\").checked"));
	g_assert_true (test_utils_jsc_call_bool_sync (fixture, "Evo.FindElement(\"frm2\", \"chk2\").checked"));
}

static void
test_set_element_style_property (TestFixture *fixture)
{
	test_utils_load_body (fixture, LOAD_ALL);

	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"\", \"btn3\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm1\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm2\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm2\", \"btn2\").style.getPropertyValue(\"color\")", "");

	e_web_view_jsc_set_element_style_property (fixture->web_view, "", "btn1", "color", "blue", NULL);
	test_utils_wait_noop (fixture);

	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"\", \"btn1\").style.getPropertyValue(\"color\")", "blue");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"\", \"btn3\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm1\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm2\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm2\", \"btn2\").style.getPropertyValue(\"color\")", "");

	e_web_view_jsc_set_element_style_property (fixture->web_view, "frm2", "btn1", "color", "green", NULL);
	test_utils_wait_noop (fixture);

	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"\", \"btn1\").style.getPropertyValue(\"color\")", "blue");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"\", \"btn3\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm1\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm2\", \"btn1\").style.getPropertyValue(\"color\")", "green");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm2\", \"btn2\").style.getPropertyValue(\"color\")", "");

	e_web_view_jsc_set_element_style_property (fixture->web_view, "frm2", "btn1", "color", NULL, NULL);
	test_utils_wait_noop (fixture);

	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"\", \"btn1\").style.getPropertyValue(\"color\")", "blue");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"\", \"btn3\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm1\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm2\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm2\", \"btn2\").style.getPropertyValue(\"color\")", "");
}

static void
test_set_element_attribute (TestFixture *fixture)
{
	test_utils_load_body (fixture, LOAD_ALL);

	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"\", \"btn3\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm1\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm2\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm2\", \"btn2\").getAttributeNS(\"\", \"myattr\")", NULL);

	e_web_view_jsc_set_element_attribute (fixture->web_view, "", "btn1", NULL, "myattr", "val1", NULL);
	test_utils_wait_noop (fixture);

	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"\", \"btn1\").getAttributeNS(\"\", \"myattr\")", "val1");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"\", \"btn3\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm1\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm2\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm2\", \"btn2\").getAttributeNS(\"\", \"myattr\")", NULL);

	e_web_view_jsc_set_element_attribute (fixture->web_view, "frm2", "btn1", NULL, "myattr", "val2", NULL);
	test_utils_wait_noop (fixture);

	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"\", \"btn1\").getAttributeNS(\"\", \"myattr\")", "val1");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"\", \"btn3\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm1\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm2\", \"btn1\").getAttributeNS(\"\", \"myattr\")", "val2");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm2\", \"btn2\").getAttributeNS(\"\", \"myattr\")", NULL);

	e_web_view_jsc_set_element_attribute (fixture->web_view, "frm2", "btn1", NULL, "myattr", NULL, NULL);
	test_utils_wait_noop (fixture);

	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"\", \"btn1\").getAttributeNS(\"\", \"myattr\")", "val1");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"\", \"btn3\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm1\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm1_1\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm2\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.FindElement(\"frm2\", \"btn2\").getAttributeNS(\"\", \"myattr\")", NULL);
}

static void
test_style_sheets (TestFixture *fixture)
{
	test_utils_load_body (fixture, LOAD_ALL);

	test_utils_jsc_call_sync (fixture,
		"var Test = {};\n"
		"\n"
		"Test.nStyles = function(iframe_id)\n"
		"{\n"
		"	return Evo.findIFrameDocument(iframe_id).head.getElementsByTagName(\"style\").length;\n"
		"}\n"
		"\n"
		"Test.hasStyle = function(iframe_id, style_sheet_id)\n"
		"{\n"
		"	var doc = Evo.findIFrameDocument(iframe_id);\n"
		"	var styles = doc.head.getElementsByTagName(\"style\"), ii;\n"
		"\n"
		"	for (ii = styles.length - 1; ii >= 0; ii--) {\n"
		"		if (styles[ii].id == style_sheet_id) {\n"
		"			return true;\n"
		"		}\n"
		"	}\n"
		"	return false;\n"
		"}\n"
		"\n"
		"Test.getStyle = function(iframe_id, style_sheet_id, selector, property_name)\n"
		"{\n"
		"	var styles = Evo.findIFrameDocument(iframe_id).head.getElementsByTagName(\"style\"), ii;\n"
		"\n"
		"	for (ii = 0; ii < styles.length; ii++) {\n"
		"		if (styles[ii].id == style_sheet_id) {\n"
		"			break;\n"
		"		}\n"
		"	}\n"
		"\n"
		"	if (ii >= styles.length)\n"
		"		return null;\n"
		"\n"
		"	styles = styles[ii].sheet;\n"
		"\n"
		"	for (ii = 0; ii < styles.cssRules.length; ii++) {\n"
		"		if (styles.cssRules[ii].selectorText == selector) {\n"
		"			var value = styles.cssRules[ii].style.getPropertyValue(property_name);\n"
		"			return (!value || value == \"\") ? null : value;\n"
		"		}\n"
		"	}\n"
		"\n"
		"	return null;\n"
		"}\n"
		"\n"
		"javascript:void(0);\n",
		NULL);

	g_assert_cmpint (1, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"\")"));
	g_assert_cmpint (1, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1\")"));
	g_assert_cmpint (1, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1_1\")"));
	g_assert_cmpint (1, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm2\")"));
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"\", \"sheet1\", \"body\", \"color\")", NULL);

	e_web_view_jsc_create_style_sheet (fixture->web_view, "", "sheet1", "body { color:green; }", NULL);
	test_utils_wait_noop (fixture);

	g_assert_cmpint (2, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"\")"));
	g_assert_cmpint (1, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1\")"));
	g_assert_cmpint (1, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1_1\")"));
	g_assert_cmpint (1, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm2\")"));
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"\", \"sheet1\", \"body\", \"color\")", "green");
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"frm1_1\", \"sheet2\", \"input\", \"background-color\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"frm1_1\", \"sheet2\", \"table\", \"color\")", NULL);

	e_web_view_jsc_add_rule_into_style_sheet (fixture->web_view, "frm1_1", "sheet2", "input", "background-color:black;", NULL);
	e_web_view_jsc_add_rule_into_style_sheet (fixture->web_view, "frm1_1", "sheet2", "table", "color:green;", NULL);
	test_utils_wait_noop (fixture);

	g_assert_cmpint (2, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"\")"));
	g_assert_cmpint (1, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1\")"));
	g_assert_cmpint (2, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1_1\")"));
	g_assert_cmpint (1, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm2\")"));
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"\", \"sheet1\", \"body\", \"color\")", "green");
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"frm1_1\", \"sheet2\", \"input\", \"background-color\")", "black");
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"frm1_1\", \"sheet2\", \"table\", \"color\")", "green");
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"frm1_1\", \"sheet2\", \"table\", \"background-color\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"\", \"sheet3\", \"body\", \"color\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"frm1\", \"sheet3\", \"body\", \"color\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"frm1_1\", \"sheet3\", \"body\", \"color\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"frm2\", \"sheet3\", \"body\", \"color\")", NULL);

	e_web_view_jsc_add_rule_into_style_sheet (fixture->web_view, "*", "sheet3", "body", "color:orange;", NULL);
	e_web_view_jsc_add_rule_into_style_sheet (fixture->web_view, "frm1_1", "sheet2", "table", "color:red; background-color:white;", NULL);
	test_utils_wait_noop (fixture);

	g_assert_cmpint (3, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"\")"));
	g_assert_cmpint (2, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1\")"));
	g_assert_cmpint (3, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1_1\")"));
	g_assert_cmpint (2, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm2\")"));
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"\", \"sheet1\", \"body\", \"color\")", "green");
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"frm1_1\", \"sheet2\", \"input\", \"background-color\")", "black");
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"frm1_1\", \"sheet2\", \"table\", \"color\")", "red");
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"frm1_1\", \"sheet2\", \"table\", \"background-color\")", "white");
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"\", \"sheet3\", \"body\", \"color\")", "orange");
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"frm1\", \"sheet3\", \"body\", \"color\")", "orange");
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"frm1_1\", \"sheet3\", \"body\", \"color\")", "orange");
	test_utils_jsc_call_string_and_verify (fixture, "Test.getStyle(\"frm2\", \"sheet3\", \"body\", \"color\")", "orange");

	g_assert_cmpint (0, ==, test_utils_jsc_call_bool_sync (fixture, "Test.hasStyle(\"\", \"sheetA\")") ? 1 : 0);
	g_assert_cmpint (0, ==, test_utils_jsc_call_bool_sync (fixture, "Test.hasStyle(\"frm1\", \"sheetA\")") ? 1 : 0);
	g_assert_cmpint (0, ==, test_utils_jsc_call_bool_sync (fixture, "Test.hasStyle(\"frm1_1\", \"sheetA\")") ? 1 : 0);

	e_web_view_jsc_add_rule_into_style_sheet (fixture->web_view, "", "sheetA", "body", "color:blue;", NULL);
	e_web_view_jsc_add_rule_into_style_sheet (fixture->web_view, "frm1", "sheetA", "body", "color:blue;", NULL);
	e_web_view_jsc_add_rule_into_style_sheet (fixture->web_view, "frm1_1", "sheetA", "body", "color:blue;", NULL);
	test_utils_wait_noop (fixture);

	g_assert_cmpint (1, ==, test_utils_jsc_call_bool_sync (fixture, "Test.hasStyle(\"\", \"sheetA\")") ? 1 : 0);
	g_assert_cmpint (1, ==, test_utils_jsc_call_bool_sync (fixture, "Test.hasStyle(\"frm1\", \"sheetA\")") ? 1 : 0);
	g_assert_cmpint (1, ==, test_utils_jsc_call_bool_sync (fixture, "Test.hasStyle(\"frm1_1\", \"sheetA\")") ? 1 : 0);
	g_assert_cmpint (4, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"\")"));
	g_assert_cmpint (3, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1\")"));
	g_assert_cmpint (4, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1_1\")"));
	g_assert_cmpint (2, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm2\")"));

	e_web_view_jsc_remove_style_sheet (fixture->web_view, "frm1", "sheetA", NULL);
	test_utils_wait_noop (fixture);

	g_assert_cmpint (1, ==, test_utils_jsc_call_bool_sync (fixture, "Test.hasStyle(\"\", \"sheetA\")") ? 1 : 0);
	g_assert_cmpint (0, ==, test_utils_jsc_call_bool_sync (fixture, "Test.hasStyle(\"frm1\", \"sheetA\")") ? 1 : 0);
	g_assert_cmpint (1, ==, test_utils_jsc_call_bool_sync (fixture, "Test.hasStyle(\"frm1_1\", \"sheetA\")") ? 1 : 0);
	g_assert_cmpint (4, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"\")"));
	g_assert_cmpint (2, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1\")"));
	g_assert_cmpint (4, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1_1\")"));
	g_assert_cmpint (2, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm2\")"));

	e_web_view_jsc_remove_style_sheet (fixture->web_view, "*", "sheetA", NULL);
	test_utils_wait_noop (fixture);

	g_assert_cmpint (0, ==, test_utils_jsc_call_bool_sync (fixture, "Test.hasStyle(\"\", \"sheetA\")") ? 1 : 0);
	g_assert_cmpint (0, ==, test_utils_jsc_call_bool_sync (fixture, "Test.hasStyle(\"frm1\", \"sheetA\")") ? 1 : 0);
	g_assert_cmpint (0, ==, test_utils_jsc_call_bool_sync (fixture, "Test.hasStyle(\"frm1_1\", \"sheetA\")") ? 1 : 0);
	g_assert_cmpint (3, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"\")"));
	g_assert_cmpint (2, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1\")"));
	g_assert_cmpint (3, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1_1\")"));
	g_assert_cmpint (2, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm2\")"));

	e_web_view_jsc_remove_style_sheet (fixture->web_view, "frm1_1", "*", NULL);
	test_utils_wait_noop (fixture);

	g_assert_cmpint (3, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"\")"));
	g_assert_cmpint (2, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1\")"));
	g_assert_cmpint (0, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1_1\")"));
	g_assert_cmpint (2, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm2\")"));

	e_web_view_jsc_add_rule_into_style_sheet (fixture->web_view, "", "sheetB", "body", "color:green;", NULL);
	e_web_view_jsc_add_rule_into_style_sheet (fixture->web_view, "frm1", "sheetD", "body", "color:blue;", NULL);
	e_web_view_jsc_add_rule_into_style_sheet (fixture->web_view, "frm1_1", "sheetB", "body", "color:green;", NULL);
	e_web_view_jsc_add_rule_into_style_sheet (fixture->web_view, "frm1_1", "sheetC", "body", "color:yellow;", NULL);
	e_web_view_jsc_add_rule_into_style_sheet (fixture->web_view, "frm2", "sheetD", "body", "color:blue;", NULL);
	e_web_view_jsc_add_rule_into_style_sheet (fixture->web_view, "frm2", "sheetE", "body", "color:orange;", NULL);
	test_utils_wait_noop (fixture);

	g_assert_cmpint (4, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"\")"));
	g_assert_cmpint (3, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1\")"));
	g_assert_cmpint (2, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1_1\")"));
	g_assert_cmpint (4, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm2\")"));

	e_web_view_jsc_remove_style_sheet (fixture->web_view, "", "*", NULL);
	test_utils_wait_noop (fixture);

	g_assert_cmpint (0, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"\")"));
	g_assert_cmpint (3, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1\")"));
	g_assert_cmpint (2, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1_1\")"));
	g_assert_cmpint (4, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm2\")"));

	e_web_view_jsc_add_rule_into_style_sheet (fixture->web_view, "", "sheetC", "body", "color:yellow;", NULL);
	test_utils_wait_noop (fixture);

	g_assert_cmpint (1, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"\")"));
	g_assert_cmpint (3, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1\")"));
	g_assert_cmpint (2, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1_1\")"));
	g_assert_cmpint (4, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm2\")"));

	e_web_view_jsc_remove_style_sheet (fixture->web_view, "*", "*", NULL);
	test_utils_wait_noop (fixture);

	g_assert_cmpint (0, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"\")"));
	g_assert_cmpint (0, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1\")"));
	g_assert_cmpint (0, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm1_1\")"));
	g_assert_cmpint (0, ==, test_utils_jsc_call_int32_sync (fixture, "Test.nStyles(\"frm2\")"));
}

typedef struct _ElementClickedData {
	TestFixture *fixture;
	const gchar *iframe_id;
	const gchar *element_id;
	const gchar *element_class;
	const gchar *element_value;
} ElementClickedData;

static void
test_verify_element_clicked_cb (EWebView *web_view,
				const gchar *iframe_id,
				const gchar *element_id,
				const gchar *element_class,
				const gchar *element_value,
				const GtkAllocation *element_position,
				gpointer user_data)
{
	ElementClickedData *expects = user_data;

	g_assert_nonnull (expects);
	g_assert_cmpstr (iframe_id, ==, expects->iframe_id);
	g_assert_cmpstr (element_id, ==, expects->element_id);
	g_assert_cmpstr (element_class, ==, expects->element_class);
	g_assert_cmpstr (element_value, ==, expects->element_value);
	g_assert_cmpint (element_position->x, >, 0);
	g_assert_cmpint (element_position->y, >, 0);
	g_assert_cmpint (element_position->width, >, 0);
	g_assert_cmpint (element_position->height, >, 0);

	test_flag_set (expects->fixture->flag);
}

static void
test_verify_element_clicked (TestFixture *fixture,
			     ElementClickedData *expects,
			     const gchar *iframe_id,
			     const gchar *element_id,
			     const gchar *element_class,
			     const gchar *element_value,
			     gboolean wait_response)
{
	gchar *script;

	expects->iframe_id = iframe_id;
	expects->element_id = element_id;
	expects->element_class = element_class;
	expects->element_value = element_value;

	script = e_web_view_jsc_printf_script ("Evo.findIFrameDocument(%s).getElementById(%s).click();",
		iframe_id, element_id);

	test_utils_jsc_call (fixture, script);

	g_free (script);

	if (wait_response)
		test_utils_wait (fixture);
}

static void
test_element_clicked (TestFixture *fixture)
{
	ElementClickedData expects;

	test_utils_load_body (fixture, LOAD_MAIN);

	expects.fixture = fixture;

	e_web_view_register_element_clicked (E_WEB_VIEW (fixture->web_view), "cbtn1", test_verify_element_clicked_cb, &expects);

	test_verify_element_clicked (fixture, &expects, "", "dots1", "cdots", NULL, FALSE);
	test_verify_element_clicked (fixture, &expects, "", "btn1", "cbtn1", "Button1", TRUE);

	test_utils_load_body (fixture, LOAD_FRM1);

	test_verify_element_clicked (fixture, &expects, "", "btn1", "cbtn1", "Button1", TRUE);
	test_verify_element_clicked (fixture, &expects, "frm1", "btn1", "cbtn1", "Button1", TRUE);

	test_utils_load_body (fixture, LOAD_FRM1_1);

	test_verify_element_clicked (fixture, &expects, "frm1_1", "btn2", "cbtn2", "Button2", FALSE);

	e_web_view_register_element_clicked (E_WEB_VIEW (fixture->web_view), "cbtn2", test_verify_element_clicked_cb, &expects);

	test_verify_element_clicked (fixture, &expects, "frm1_1", "btn1", "cbtn1", "Button1", TRUE);
	test_verify_element_clicked (fixture, &expects, "frm1_1", "btn2", "cbtn2", "Button2", TRUE);

	test_utils_load_body (fixture, LOAD_FRM2);

	test_verify_element_clicked (fixture, &expects, "frm1_1", "btn2", "cbtn2", "Button2", TRUE);
	test_verify_element_clicked (fixture, &expects, "frm2", "btn2", "cbtn2", "Button2", TRUE);

	e_web_view_register_element_clicked (E_WEB_VIEW (fixture->web_view), "cdots", test_verify_element_clicked_cb, &expects);

	test_verify_element_clicked (fixture, &expects, "", "btn3", "cbtn3", "Button3", FALSE);
	test_verify_element_clicked (fixture, &expects, "", "dots1", "cdots", NULL, TRUE);
	test_verify_element_clicked (fixture, &expects, "", "btn1", "cbtn1", "Button1", TRUE);
	test_verify_element_clicked (fixture, &expects, "frm1", "btn1", "cbtn1", "Button1", TRUE);
	test_verify_element_clicked (fixture, &expects, "frm1_1", "btn1", "cbtn1", "Button1", TRUE);
	test_verify_element_clicked (fixture, &expects, "frm1_1", "btn2", "cbtn2", "Button2", TRUE);
	test_verify_element_clicked (fixture, &expects, "frm2", "btn1", "cbtn1", "Button1", TRUE);
	test_verify_element_clicked (fixture, &expects, "frm2", "btn2", "cbtn2", "Button2", TRUE);
	test_verify_element_clicked (fixture, &expects, "frm1_1", "dots2", "cdots", NULL, TRUE);

	e_web_view_unregister_element_clicked (E_WEB_VIEW (fixture->web_view), "cbtn1", test_verify_element_clicked_cb, &expects);

	test_verify_element_clicked (fixture, &expects, "", "btn3", "cbtn3", "Button3", FALSE);
	test_verify_element_clicked (fixture, &expects, "", "btn1", "cbtn1", "Button1", FALSE);
	test_verify_element_clicked (fixture, &expects, "frm1", "btn1", "cbtn1", "Button1", FALSE);
	test_verify_element_clicked (fixture, &expects, "frm1_1", "btn1", "cbtn1", "Button1", FALSE);
	test_verify_element_clicked (fixture, &expects, "frm2", "btn1", "cbtn1", "Button1", FALSE);
	test_verify_element_clicked (fixture, &expects, "", "dots1", "cdots", NULL, TRUE);
	test_verify_element_clicked (fixture, &expects, "frm1_1", "btn2", "cbtn2", "Button2", TRUE);
	test_verify_element_clicked (fixture, &expects, "frm2", "btn2", "cbtn2", "Button2", TRUE);
	test_verify_element_clicked (fixture, &expects, "frm1_1", "dots2", "cdots", NULL, TRUE);

	e_web_view_unregister_element_clicked (E_WEB_VIEW (fixture->web_view), "cbtn2", test_verify_element_clicked_cb, &expects);
	e_web_view_unregister_element_clicked (E_WEB_VIEW (fixture->web_view), "cdots", test_verify_element_clicked_cb, &expects);

	test_verify_element_clicked (fixture, &expects, "", "btn3", "cbtn3", "Button3", FALSE);
	test_verify_element_clicked (fixture, &expects, "", "dots1", "cdots", NULL, FALSE);
	test_verify_element_clicked (fixture, &expects, "", "btn1", "cbtn1", "Button1", FALSE);
	test_verify_element_clicked (fixture, &expects, "frm1", "btn1", "cbtn1", "Button1", FALSE);
	test_verify_element_clicked (fixture, &expects, "frm1_1", "btn1", "cbtn1", "Button1", FALSE);
	test_verify_element_clicked (fixture, &expects, "frm1_1", "btn2", "cbtn2", "Button2", FALSE);
	test_verify_element_clicked (fixture, &expects, "frm2", "btn1", "cbtn1", "Button1", FALSE);
	test_verify_element_clicked (fixture, &expects, "frm2", "btn2", "cbtn2", "Button2", FALSE);
	test_verify_element_clicked (fixture, &expects, "frm1_1", "dots2", "cdots", NULL, FALSE);

	test_utils_wait_noop (fixture);
}

typedef struct _NeedInputData {
	TestFixture *fixture;
	gboolean expects;
} NeedInputData;

static void
test_verify_need_input_cb (GObject *object,
			   GParamSpec *param,
			   gpointer user_data)
{
	NeedInputData *nid = user_data;

	g_assert_nonnull (nid);
	g_assert_cmpint ((e_web_view_get_need_input (E_WEB_VIEW (nid->fixture->web_view)) ? 1 : 0), ==, (nid->expects ? 1 : 0));

	test_flag_set (nid->fixture->flag);
}

static void
test_verify_need_input (TestFixture *fixture,
			NeedInputData *nid,
			const gchar *iframe_id,
			const gchar *element_id,
			gboolean expects)
{
	nid->expects = expects;

	if (iframe_id) {
		gchar *script;

		script = e_web_view_jsc_printf_script ("Evo.findIFrameDocument(%s).getElementById(%s).focus();",
			iframe_id, element_id);

		test_utils_jsc_call (fixture, script);

		g_free (script);
	} else {
		test_utils_jsc_call (fixture, "document.activeElement.blur();");
	}

	test_utils_wait (fixture);
}

static void
test_need_input_changed (TestFixture *fixture)
{
	gulong handler_id;
	NeedInputData nid;

	test_utils_load_string (fixture,
		"<html><body>"
		"Top<br>"
		"<input id=\"btn1\" class=\"cbtn1\" type=\"button\" value=\"Button1\"><br>"
		"<iframe id=\"frm1_1\" src=\"empty:///\"></iframe><br>"
		"<input id=\"btn3\" class=\"cbtn3\" type=\"button\" value=\"Button3\">"
		"<a name=\"dots\" id=\"dots1\" class=\"cdots\">...</a>"
		"<label for=\"inptrdo\" id=\"lblradio\">Radio</label>"
		"<input type=\"radio\" name=\"rdo\" id=\"inptrdo\" value=\"rdoval\"><br>"
		"<textarea id=\"txt\" rows=\"3\" cols=\"20\">Text area text</textarea><br>"
		"<select id=\"slct\">"
		"   <option value=\"opt1\">opt1</option>"
		"   <option value=\"opt2\">opt2</option>"
		"   <option value=\"opt3\">opt3</option>"
		"</select><br>"
		"<button id=\"bbtn\" type=\"button\">Button</button>"
		"</body></html>");

	test_utils_load_body (fixture, LOAD_FRM1_1);

	g_assert_true (!e_web_view_get_need_input (E_WEB_VIEW (fixture->web_view)));

	nid.fixture = fixture;
	nid.expects = FALSE;

	handler_id = g_signal_connect (fixture->web_view, "notify::need-input",
		G_CALLBACK (test_verify_need_input_cb), &nid);

	test_verify_need_input (fixture, &nid, "", "btn1", TRUE);
	test_verify_need_input (fixture, &nid, NULL, NULL, FALSE);
	test_verify_need_input (fixture, &nid, "frm1_1", "btn2", TRUE);
	test_verify_need_input (fixture, &nid, NULL, NULL, FALSE);
	test_verify_need_input (fixture, &nid, "", "btn3", TRUE);
	test_verify_need_input (fixture, &nid, NULL, NULL, FALSE);
	test_verify_need_input (fixture, &nid, "", "lblradio", TRUE);
	test_verify_need_input (fixture, &nid, NULL, NULL, FALSE);
	test_verify_need_input (fixture, &nid, "", "inptrdo", TRUE);
	test_verify_need_input (fixture, &nid, NULL, NULL, FALSE);
	test_verify_need_input (fixture, &nid, "", "inptrdo", TRUE);
	test_verify_need_input (fixture, &nid, NULL, NULL, FALSE);
	test_verify_need_input (fixture, &nid, "", "txt", TRUE);
	test_verify_need_input (fixture, &nid, NULL, NULL, FALSE);
	test_verify_need_input (fixture, &nid, "", "slct", TRUE);
	test_verify_need_input (fixture, &nid, NULL, NULL, FALSE);
	test_verify_need_input (fixture, &nid, "", "bbtn", TRUE);
	test_verify_need_input (fixture, &nid, NULL, NULL, FALSE);

	g_signal_handler_disconnect (fixture->web_view, handler_id);

	g_assert_true (!e_web_view_get_need_input (E_WEB_VIEW (fixture->web_view)));
}

static void
test_selection_select_in_iframe (TestFixture *fixture,
				 const gchar *iframe_id,
				 const gchar *start_elem_id,
				 const gchar *end_elem_id)
{
	gchar *script;

	script = e_web_view_jsc_printf_script (
		/* Clean selection in both places first, otherwise the previous selection
		   can stay when changing it only in one of them. */
		"Evo.findIFrameDocument(\"\").defaultView.getSelection().empty();\n"
		"Evo.findIFrameDocument(\"frm1\").defaultView.getSelection().empty();\n"
		"\n"
		"var doc, range;\n"
		"doc = Evo.findIFrameDocument(%s);\n"
		"range = doc.createRange();\n"
		"range.selectNodeContents(doc.getElementById(%s));"
		"doc.defaultView.getSelection().addRange(range);\n"
		"doc.defaultView.getSelection().extend(doc.getElementById(%s));\n",
		iframe_id, start_elem_id, end_elem_id);

	test_utils_jsc_call_sync (fixture, script, NULL);

	g_free (script);

	/* Wait for the notification from JS about changed selection
	   to be propagated into EWebView's has-selection. */
	test_utils_wait_noop (fixture);
}

typedef struct _GetContentData {
	TestFixture *fixture;
	const gchar *expect_plain;
	const gchar *expect_html;
} GetContentData;

static void
test_verify_get_content_data (GetContentData *gcd,
			      const GSList *texts)
{
	g_assert_nonnull (gcd);

	if (gcd->expect_plain && gcd->expect_html) {
		g_assert_cmpint (g_slist_length ((GSList *) texts), ==, 2);
		g_assert_cmpstr (texts->data, ==, gcd->expect_plain);
		g_assert_cmpstr (texts->next->data, ==, gcd->expect_html);
	} else if (gcd->expect_plain) {
		g_assert_cmpint (g_slist_length ((GSList *) texts), ==, 1);
		g_assert_cmpstr (texts->data, ==, gcd->expect_plain);
	} else if (gcd->expect_html) {
		g_assert_cmpint (g_slist_length ((GSList *) texts), ==, 1);
		g_assert_cmpstr (texts->data, ==, gcd->expect_html);
	} else {
		g_assert_cmpint (g_slist_length ((GSList *) texts), ==, 0);
	}
}

static void
test_selection_ready_cb (GObject *source_object,
			 GAsyncResult *result,
			 gpointer user_data)
{
	GetContentData *gcd = user_data;
	GSList *texts = NULL;
	gboolean success;
	GError *error = NULL;

	g_assert_true (WEBKIT_IS_WEB_VIEW (source_object));
	g_assert_nonnull (gcd);

	success = e_web_view_jsc_get_selection_finish (WEBKIT_WEB_VIEW (source_object), result, &texts, &error);

	g_assert_no_error (error);
	g_assert_true (success);

	test_verify_get_content_data (gcd, texts);

	g_slist_free_full (texts, g_free);

	test_flag_set (gcd->fixture->flag);
}

static void
test_selection_verify (TestFixture *fixture,
		       const gchar *expect_plain,
		       const gchar *expect_html)
{
	ETextFormat format;
	GetContentData gcd;

	if (expect_plain && expect_html)
		format = E_TEXT_FORMAT_BOTH;
	else if (expect_html)
		format = E_TEXT_FORMAT_HTML;
	else
		format = E_TEXT_FORMAT_PLAIN;

	gcd.fixture = fixture;
	gcd.expect_plain = expect_plain;
	gcd.expect_html = expect_html;

	e_web_view_jsc_get_selection (fixture->web_view, format, NULL, test_selection_ready_cb, &gcd);

	test_utils_wait (fixture);
}

static void
test_selection (TestFixture *fixture)
{
	test_utils_load_string (fixture,
		"<html><body>"
		"<pre id=\"pr\">Out<span id=\"pr1\"></span>er text\nin PR<span id=\"pr2\"></span>E</pre><br id=\"br1\">"
		"o<font color=\"orange\">rang</font>e; <b>bold</b><i>italic</i><br id=\"br2\">"
		"<iframe id=\"frm1\" src=\"empty:///\"></iframe><br>"
		"</body></html>");

	test_utils_load_iframe_content (fixture, "frm1",
		"<html><body>"
		"frm1<br>"
		"<div id=\"plain\">unformatted text</div><br>"
		"<div id=\"rgb\">"
		"<font color=\"red\">R</font>"
		"<font color=\"green\">G</font>"
		"<font color=\"blue\">B</font>"
		"<span id=\"rgb-end\"></span>"
		"</div>"
		"<div id=\"styled\">"
		"<span style=\"color:blue;\">bb</span>"
		"<span style=\"color:green;\">gg</span>"
		"<span style=\"color:red;\">rr</span>"
		"</div>"
		"<div id=\"end\"></div>"
		"</body></html>");

	g_assert_cmpint (e_web_view_has_selection (E_WEB_VIEW (fixture->web_view)) ? 1 : 0, ==, 0);
	test_selection_verify (fixture, NULL, NULL);

	test_selection_select_in_iframe (fixture, "", "pr1", "pr2");

	g_assert_cmpint (e_web_view_has_selection (E_WEB_VIEW (fixture->web_view)) ? 1 : 0, ==, 1);
	test_selection_verify (fixture, "er text\nin PR", NULL);
	test_selection_verify (fixture, NULL, "<pre><span id=\"pr1\"></span>er text\nin PR<span id=\"pr2\"></span></pre>");
	test_selection_verify (fixture, "er text\nin PR", "<pre><span id=\"pr1\"></span>er text\nin PR<span id=\"pr2\"></span></pre>");

	test_selection_select_in_iframe (fixture, "", "br1", "br2");

	g_assert_cmpint (e_web_view_has_selection (E_WEB_VIEW (fixture->web_view)) ? 1 : 0, ==, 1);
	test_selection_verify (fixture, "\norange; bolditalic\n", NULL);
	test_selection_verify (fixture, NULL, "<br id=\"br1\">o<font color=\"orange\">rang</font>e; <b>bold</b><i>italic</i><br id=\"br2\">");
	test_selection_verify (fixture, "\norange; bolditalic\n", "<br id=\"br1\">o<font color=\"orange\">rang</font>e; <b>bold</b><i>italic</i><br id=\"br2\">");

	test_selection_select_in_iframe (fixture, "frm1", "plain", "rgb");

	g_assert_cmpint (e_web_view_has_selection (E_WEB_VIEW (fixture->web_view)) ? 1 : 0, ==, 1);
	test_selection_verify (fixture, "unformatted text\n", NULL);
	test_selection_verify (fixture, NULL, "<div id=\"plain\">unformatted text</div><br><div id=\"rgb\"></div>");
	test_selection_verify (fixture, "unformatted text\n", "<div id=\"plain\">unformatted text</div><br><div id=\"rgb\"></div>");

	test_selection_select_in_iframe (fixture, "frm1", "rgb", "styled");

	g_assert_cmpint (e_web_view_has_selection (E_WEB_VIEW (fixture->web_view)) ? 1 : 0, ==, 1);
	test_selection_verify (fixture, "RGB\n", NULL);
	test_selection_verify (fixture, NULL, "<div id=\"rgb\"><font color=\"red\">R</font><font color=\"green\">G</font><font color=\"blue\">B</font><span id=\"rgb-end\"></span></div><div id=\"styled\"></div>");
	test_selection_verify (fixture, "RGB\n", "<div id=\"rgb\"><font color=\"red\">R</font><font color=\"green\">G</font><font color=\"blue\">B</font><span id=\"rgb-end\"></span></div><div id=\"styled\"></div>");

	test_selection_select_in_iframe (fixture, "frm1", "rgb", "rgb-end");

	g_assert_cmpint (e_web_view_has_selection (E_WEB_VIEW (fixture->web_view)) ? 1 : 0, ==, 1);
	test_selection_verify (fixture, "RGB", NULL);
	test_selection_verify (fixture, NULL, "<font color=\"red\">R</font><font color=\"green\">G</font><font color=\"blue\">B</font><span id=\"rgb-end\"></span>");
	test_selection_verify (fixture, "RGB", "<font color=\"red\">R</font><font color=\"green\">G</font><font color=\"blue\">B</font><span id=\"rgb-end\"></span>");

	test_selection_select_in_iframe (fixture, "frm1", "styled", "end");

	g_assert_cmpint (e_web_view_has_selection (E_WEB_VIEW (fixture->web_view)) ? 1 : 0, ==, 1);
	test_selection_verify (fixture, "bbggrr\n", NULL);
	test_selection_verify (fixture, NULL, "<div id=\"styled\"><span style=\"color:blue;\">bb</span><span style=\"color:green;\">gg</span><span style=\"color:red;\">rr</span></div><div id=\"end\"></div>");
	test_selection_verify (fixture, "bbggrr\n", "<div id=\"styled\"><span style=\"color:blue;\">bb</span><span style=\"color:green;\">gg</span><span style=\"color:red;\">rr</span></div><div id=\"end\"></div>");

	test_selection_select_in_iframe (fixture, "frm1", "end", "end");

	g_assert_cmpint (e_web_view_has_selection (E_WEB_VIEW (fixture->web_view)) ? 1 : 0, ==, 0);
	test_selection_verify (fixture, NULL, NULL);
}

static void
test_get_document_content_ready_cb (GObject *source_object,
				    GAsyncResult *result,
				    gpointer user_data)
{
	GetContentData *gcd = user_data;
	GSList *texts = NULL;
	gboolean success;
	GError *error = NULL;

	g_assert_true (WEBKIT_IS_WEB_VIEW (source_object));
	g_assert_nonnull (gcd);

	success = e_web_view_jsc_get_document_content_finish (WEBKIT_WEB_VIEW (source_object), result, &texts, &error);

	g_assert_no_error (error);
	g_assert_true (success);

	test_verify_get_content_data (gcd, texts);

	g_slist_free_full (texts, g_free);

	test_flag_set (gcd->fixture->flag);
}

static void
test_get_document_content_verify (TestFixture *fixture,
				  const gchar *iframe_id,
				  const gchar *expect_plain,
				  const gchar *expect_html)
{
	ETextFormat format;
	GetContentData gcd;

	if (expect_plain && expect_html)
		format = E_TEXT_FORMAT_BOTH;
	else if (expect_html)
		format = E_TEXT_FORMAT_HTML;
	else
		format = E_TEXT_FORMAT_PLAIN;

	gcd.fixture = fixture;
	gcd.expect_plain = expect_plain;
	gcd.expect_html = expect_html;

	e_web_view_jsc_get_document_content (fixture->web_view, iframe_id, format, NULL, test_get_document_content_ready_cb, &gcd);

	test_utils_wait (fixture);
}

static void
test_get_element_content_ready_cb (GObject *source_object,
				   GAsyncResult *result,
				   gpointer user_data)
{
	GetContentData *gcd = user_data;
	GSList *texts = NULL;
	gboolean success;
	GError *error = NULL;

	g_assert_true (WEBKIT_IS_WEB_VIEW (source_object));
	g_assert_nonnull (gcd);

	success = e_web_view_jsc_get_element_content_finish (WEBKIT_WEB_VIEW (source_object), result, &texts, &error);

	g_assert_no_error (error);
	g_assert_true (success);

	test_verify_get_content_data (gcd, texts);

	g_slist_free_full (texts, g_free);

	test_flag_set (gcd->fixture->flag);
}

static void
test_get_element_content_verify (TestFixture *fixture,
				 const gchar *iframe_id,
				 const gchar *element_id,
				 gboolean use_outer_html,
				 const gchar *expect_plain,
				 const gchar *expect_html)
{
	ETextFormat format;
	GetContentData gcd;

	if (expect_plain && expect_html)
		format = E_TEXT_FORMAT_BOTH;
	else if (expect_html)
		format = E_TEXT_FORMAT_HTML;
	else
		format = E_TEXT_FORMAT_PLAIN;

	gcd.fixture = fixture;
	gcd.expect_plain = expect_plain;
	gcd.expect_html = expect_html;

	e_web_view_jsc_get_element_content (fixture->web_view, iframe_id, element_id, format, use_outer_html, NULL, test_get_element_content_ready_cb, &gcd);

	test_utils_wait (fixture);
}

static void
test_get_content (TestFixture *fixture)
{
	const gchar *html_main =
		"<html><head><meta charset=\"utf-8\"></head><body>"
		"<div id=\"frst\">first div</div>"
		"<div id=\"scnd\">second div</div>"
		"<iframe id=\"frm1\" src=\"empty:///\"></iframe>"
		"</body></html>";
	const gchar *html_frm1 =
		"<html><head><meta name=\"keywords\" value=\"test\"></head><body>"
		"<span id=\"frm1p\">"
		"<div id=\"frst\">frm1 div</div>"
		"</span>"
		"</body></html>";
	const gchar *expect_html, *expect_plain;

	test_utils_load_string (fixture, html_main);
	test_utils_load_iframe_content (fixture, "frm1", html_frm1);

	/* Clean up styles added by EWebView */
	test_utils_jsc_call_sync (fixture, "Evo.SetElementStyleProperty(\"\",\"*html\",\"color\",null);", NULL);
	test_utils_jsc_call_sync (fixture, "Evo.SetElementStyleProperty(\"\",\"*html\",\"background-color\",null);", NULL);
	test_utils_jsc_call_sync (fixture, "Evo.SetElementAttribute(\"\",\"*body\",\"\",\"class\",null);", NULL);
	test_utils_jsc_call_sync (fixture, "Evo.SetElementStyleProperty(\"frm1\",\"*html\",\"color\",null);", NULL);
	test_utils_jsc_call_sync (fixture, "Evo.SetElementStyleProperty(\"frm1\",\"*html\",\"background-color\",null);", NULL);
	test_utils_jsc_call_sync (fixture, "Evo.SetElementAttribute(\"frm1\",\"*body\",\"\",\"class\",null);", NULL);
	test_utils_jsc_call_sync (fixture, "Evo.RemoveStyleSheet(\"*\",\"*\");", NULL);

	expect_plain = "first div\nsecond div\n";
	expect_html = html_main;

	test_get_document_content_verify (fixture, "", expect_plain, NULL);
	test_get_document_content_verify (fixture, "", NULL, expect_html);
	test_get_document_content_verify (fixture, "", expect_plain, expect_html);

	expect_plain = "frm1 div\n";
	expect_html = html_frm1;
	test_get_document_content_verify (fixture, "frm1", expect_plain, NULL);
	test_get_document_content_verify (fixture, "frm1", NULL, expect_html);
	test_get_document_content_verify (fixture, "frm1", expect_plain, expect_html);

	expect_plain = "";
	expect_html = "<meta charset=\"utf-8\">";
	test_get_element_content_verify (fixture, "", "*head", FALSE, expect_plain, NULL);
	test_get_element_content_verify (fixture, "", "*head", FALSE, NULL, expect_html);
	test_get_element_content_verify (fixture, "", "*head", FALSE, expect_plain, expect_html);

	expect_html = "<head><meta charset=\"utf-8\"></head>";
	test_get_element_content_verify (fixture, "", "*head", TRUE, expect_plain, NULL);
	test_get_element_content_verify (fixture, "", "*head", TRUE, NULL, expect_html);
	test_get_element_content_verify (fixture, "", "*head", TRUE, expect_plain, expect_html);

	expect_html = "<meta name=\"keywords\" value=\"test\">";
	test_get_element_content_verify (fixture, "frm1", "*head", FALSE, expect_plain, NULL);
	test_get_element_content_verify (fixture, "frm1", "*head", FALSE, NULL, expect_html);
	test_get_element_content_verify (fixture, "frm1", "*head", FALSE, expect_plain, expect_html);

	expect_html = "<head><meta name=\"keywords\" value=\"test\"></head>";
	test_get_element_content_verify (fixture, "frm1", "*head", TRUE, expect_plain, NULL);
	test_get_element_content_verify (fixture, "frm1", "*head", TRUE, NULL, expect_html);
	test_get_element_content_verify (fixture, "frm1", "*head", TRUE, expect_plain, expect_html);

	expect_plain = "first div\nsecond div\n";
	expect_html =
		"<div id=\"frst\">first div</div>"
		"<div id=\"scnd\">second div</div>"
		"<iframe id=\"frm1\" src=\"empty:///\"></iframe>";
	test_get_element_content_verify (fixture, "", "*body", FALSE, expect_plain, NULL);
	test_get_element_content_verify (fixture, "", "*body", FALSE, NULL, expect_html);
	test_get_element_content_verify (fixture, "", "*body", FALSE, expect_plain, expect_html);

	expect_html = "<body>"
		"<div id=\"frst\">first div</div>"
		"<div id=\"scnd\">second div</div>"
		"<iframe id=\"frm1\" src=\"empty:///\"></iframe></body>";
	test_get_element_content_verify (fixture, "", "*body", TRUE, expect_plain, NULL);
	test_get_element_content_verify (fixture, "", "*body", TRUE, NULL, expect_html);
	test_get_element_content_verify (fixture, "", "*body", TRUE, expect_plain, expect_html);

	expect_plain = "frm1 div\n";
	expect_html ="<span id=\"frm1p\"><div id=\"frst\">frm1 div</div></span>";
	test_get_element_content_verify (fixture, "frm1", "*body", FALSE, expect_plain, NULL);
	test_get_element_content_verify (fixture, "frm1", "*body", FALSE, NULL, expect_html);
	test_get_element_content_verify (fixture, "frm1", "*body", FALSE, expect_plain, expect_html);

	expect_html = "<body><span id=\"frm1p\"><div id=\"frst\">frm1 div</div></span></body>";
	test_get_element_content_verify (fixture, "frm1", "*body", TRUE, expect_plain, NULL);
	test_get_element_content_verify (fixture, "frm1", "*body", TRUE, NULL, expect_html);
	test_get_element_content_verify (fixture, "frm1", "*body", TRUE, expect_plain, expect_html);

	expect_plain = "first div";
	expect_html = "first div";
	test_get_element_content_verify (fixture, "", "frst", FALSE, expect_plain, NULL);
	test_get_element_content_verify (fixture, "", "frst", FALSE, NULL, expect_html);
	test_get_element_content_verify (fixture, "", "frst", FALSE, expect_plain, expect_html);

	expect_html = "<div id=\"frst\">first div</div>";
	test_get_element_content_verify (fixture, "", "frst", TRUE, expect_plain, NULL);
	test_get_element_content_verify (fixture, "", "frst", TRUE, NULL, expect_html);
	test_get_element_content_verify (fixture, "", "frst", TRUE, expect_plain, expect_html);

	expect_plain = "frm1 div";
	expect_html = "frm1 div";
	test_get_element_content_verify (fixture, "frm1", "frst", FALSE, expect_plain, NULL);
	test_get_element_content_verify (fixture, "frm1", "frst", FALSE, NULL, expect_html);
	test_get_element_content_verify (fixture, "frm1", "frst", FALSE, expect_plain, expect_html);

	expect_html = "<div id=\"frst\">frm1 div</div>";
	test_get_element_content_verify (fixture, "frm1", "frst", TRUE, expect_plain, NULL);
	test_get_element_content_verify (fixture, "frm1", "frst", TRUE, NULL, expect_html);
	test_get_element_content_verify (fixture, "frm1", "frst", TRUE, expect_plain, expect_html);

	expect_plain = "frm1 div\n";
	test_get_element_content_verify (fixture, "frm1", "frm1p", FALSE, expect_plain, NULL);
	test_get_element_content_verify (fixture, "frm1", "frm1p", FALSE, NULL, expect_html);
	test_get_element_content_verify (fixture, "frm1", "frm1p", FALSE, expect_plain, expect_html);

	expect_html = "<span id=\"frm1p\"><div id=\"frst\">frm1 div</div></span>";
	test_get_element_content_verify (fixture, "frm1", "frm1p", TRUE, expect_plain, NULL);
	test_get_element_content_verify (fixture, "frm1", "frm1p", TRUE, NULL, expect_html);
	test_get_element_content_verify (fixture, "frm1", "frm1p", TRUE, expect_plain, expect_html);
}

typedef struct _GetElementFromPoint {
	TestFixture *fixture;
	const gchar *expect_iframe_src;
	const gchar *expect_iframe_id;
	const gchar *expect_element_id;
} GetElementFromPoint;

static void
test_get_element_from_point_ready_cb (GObject *source_object,
				      GAsyncResult *result,
				      gpointer user_data)
{
	GetElementFromPoint *gefp = user_data;
	gchar *iframe_src = NULL, *iframe_id = NULL, *element_id = NULL;
	gboolean success;
	GError *error = NULL;

	g_assert_true (WEBKIT_IS_WEB_VIEW (source_object));
	g_assert_nonnull (gefp);

	success = e_web_view_jsc_get_element_from_point_finish (WEBKIT_WEB_VIEW (source_object), result, &iframe_src, &iframe_id, &element_id, &error);

	g_assert_no_error (error);
	g_assert_true (success);

	g_assert_cmpstr (iframe_src, ==, gefp->expect_iframe_src);
	g_assert_cmpstr (iframe_id, ==, gefp->expect_iframe_id);
	g_assert_cmpstr (element_id, ==, gefp->expect_element_id);

	g_free (iframe_src);
	g_free (iframe_id);
	g_free (element_id);

	test_flag_set (gefp->fixture->flag);
}

static void
test_utils_verify_get_element_from_point (TestFixture *fixture,
					  gint xx,
					  gint yy,
					  const gchar *expect_iframe_src,
					  const gchar *expect_iframe_id,
					  const gchar *expect_element_id)
{
	GetElementFromPoint gefp;

	gefp.fixture = fixture;
	gefp.expect_iframe_src = expect_iframe_src;
	gefp.expect_iframe_id = expect_iframe_id;
	gefp.expect_element_id = expect_element_id;

	e_web_view_jsc_get_element_from_point (fixture->web_view, xx, yy, NULL, test_get_element_from_point_ready_cb, &gefp);

	test_utils_wait (fixture);
}

static void
window_size_allocated_cb (GtkWidget *widget,
			  GdkRectangle *allocation,
			  gpointer user_data)
{
	TestFixture *fixture = user_data;

	test_flag_set (fixture->flag);
}

static void
test_get_element_from_point (TestFixture *fixture)
{
	struct _elems {
		const gchar *iframe_src;
		const gchar *iframe_id;
		const gchar *elem_id;
	} elems[] = {
		{ NULL, "", "btn1" },
		{ NULL, "", "btn3" },
		{ NULL, "", "dots1" },
		{ "empty:///frm1", "frm1", "btn1" },
		{ "empty:///frm1_1", "frm1_1", "btn1" },
		{ "empty:///frm1_1", "frm1_1", "dots2" },
		{ "empty:///frm1_1", "frm1_1", "btn2" },
		{ "empty:///frm2", "frm2", "btn1" },
		{ "empty:///frm2", "frm2", "btn2" }
	};
	gchar *script;
	gint ii, scroll_x, scroll_y, client_width, client_height, tested;

	test_utils_load_body (fixture, LOAD_ALL);

	ii = test_utils_jsc_call_int32_sync (fixture,
		"function TestGetPosition(iframe_id, elem_id)\n"
		"{\n"
		"	var elem = Evo.FindElement(iframe_id, elem_id);\n"
		"	var xx = 0, yy = 0, off_elem, check_elem;\n"
		"	for (check_elem = elem; check_elem; check_elem = check_elem.ownerDocument.defaultView.frameElement) {\n"
		"		for (ii = check_elem; ii; ii = ii.parentOffset) {\n"
		"			xx += ii.offsetLeft - ii.scrollLeft;\n"
		"			yy += ii.offsetTop - ii.scrollTop;\n"
		"		}\n"
		"	}\n"
		"	var res = [];\n"
		"	res[\"left\"] = xx + (elem.offsetWidth / 2) - window.scrollX;\n"
		"	res[\"right\"] = xx + elem.offsetWidth - 2 - window.scrollX;\n"
		"	res[\"top\"] = yy + (elem.offsetHeight / 2) - window.scrollY;\n"
		"	return res;"
		"}\n"
		"\n"
		/* To not scroll in the frm1 */
		"document.getElementById(\"frm1\").height = document.getElementById(\"frm1\").contentDocument.getElementById(\"btn1\").offsetTop +"
		" 2 * document.getElementById(\"frm1\").contentDocument.getElementById(\"btn1\").offsetHeight;\n"
		"\n"
		"document.body.scrollHeight;\n");

	/* To not scroll in the overall document */
	gtk_widget_set_size_request (fixture->window, -1, ii);

	/* Window/widget resize is done asynchronously, thus wait for it */
	g_signal_connect (fixture->window, "size-allocate",
		G_CALLBACK (window_size_allocated_cb), fixture);

	test_utils_wait (fixture);

	g_signal_handlers_disconnect_by_func (fixture->window, G_CALLBACK (window_size_allocated_cb), fixture);

	for (ii = 0; ii < G_N_ELEMENTS (elems); ii++) {
		const gchar *iframe_src;
		JSCValue *value;
		gint xx, yy;

		script = e_web_view_jsc_printf_script ("TestGetPosition(%s, %s);", elems[ii].iframe_id, elems[ii].elem_id);

		test_utils_jsc_call_sync (fixture, script, &value);

		g_assert_nonnull (value);
		g_assert_true (jsc_value_is_object (value));

		xx = e_web_view_jsc_get_object_property_int32 (value, "left", -1);
		yy = e_web_view_jsc_get_object_property_int32 (value, "top", -1);

		g_assert_cmpint (xx, >, -1);
		g_assert_cmpint (yy, >, -1);

		g_clear_object (&value);
		g_free (script);

		iframe_src = elems[ii].iframe_src;

		if (!iframe_src)
			iframe_src = webkit_web_view_get_uri (fixture->web_view);

		test_utils_verify_get_element_from_point (fixture, xx, yy, iframe_src, elems[ii].iframe_id, elems[ii].elem_id);
	}

	test_utils_verify_get_element_from_point (fixture, -1, -1, webkit_web_view_get_uri (fixture->web_view), "", "");

	test_utils_jsc_call_sync (fixture, "Evo.findIFrameDocument(\"\").getElementById(\"btn3\").focus();", NULL);

	test_utils_verify_get_element_from_point (fixture, -1, -1, webkit_web_view_get_uri (fixture->web_view), "", "btn3");

	scroll_x = test_utils_jsc_call_int32_sync (fixture, "document.body.scrollWidth;") / 2;
	scroll_y = test_utils_jsc_call_int32_sync (fixture, "document.body.scrollHeight;") / 2;

	/* To scroll in the overall document */
	gtk_widget_set_size_request (fixture->window, -1, -1);
	gtk_window_resize (GTK_WINDOW (fixture->window), scroll_x, scroll_y);

	/* Window/widget resize is done asynchronously, thus wait for it */
	g_signal_connect (fixture->window, "size-allocate",
		G_CALLBACK (window_size_allocated_cb), fixture);

	test_utils_wait (fixture);

	g_signal_handlers_disconnect_by_func (fixture->window, G_CALLBACK (window_size_allocated_cb), fixture);

	/* Scroll by some value */
	scroll_x /= 2;
	scroll_y /= 2;
	script = e_web_view_jsc_printf_script ("window.scrollBy(%d,%d);", scroll_x, scroll_y);
	test_utils_jsc_call_sync (fixture, script, NULL);
	g_free (script);

	test_utils_wait_noop (fixture);

	client_width = test_utils_jsc_call_int32_sync (fixture, "document.body.clientWidth;");
	client_height = test_utils_jsc_call_int32_sync (fixture, "document.body.clientHeight;");

	tested = 0;

	for (ii = 0; ii < G_N_ELEMENTS (elems); ii++) {
		const gchar *iframe_src;
		JSCValue *value;
		gint xx, yy;

		script = e_web_view_jsc_printf_script ("TestGetPosition(%s, %s);", elems[ii].iframe_id, elems[ii].elem_id);

		test_utils_jsc_call_sync (fixture, script, &value);

		g_assert_nonnull (value);
		g_assert_true (jsc_value_is_object (value));

		xx = e_web_view_jsc_get_object_property_int32 (value, "right", -1);
		yy = e_web_view_jsc_get_object_property_int32 (value, "top", -1);

		g_clear_object (&value);
		g_free (script);

		if (xx >= 0 && xx < client_width && yy >= 0 && yy < client_height) {
			iframe_src = elems[ii].iframe_src;

			if (!iframe_src)
				iframe_src = webkit_web_view_get_uri (fixture->web_view);

			test_utils_verify_get_element_from_point (fixture, xx, yy, iframe_src, elems[ii].iframe_id, elems[ii].elem_id);

			tested++;
		}
	}

	g_assert_cmpint (tested, >, 0);
}

static void
test_convert_to_plain (TestFixture *fixture)
{
	#define HTML(_body) ("<html><head><style><!-- span.Apple-tab-span { white-space:pre; } --></style></head><body style='font-family:monospace;'>" _body "</body></html>")
	#define TAB "<span class='Apple-tab-span' style='white-space:pre;'>\t</span>"
	#define BREAK_STYLE " word-break:break-word; word-wrap:break-word; line-break:after-white-space;"
	#define WRAP_STYLE(_type) " white-space:" _type "; " BREAK_STYLE
	#define ALIGN_STYLE(_type) " text-align:" _type ";"
	#define INDENT_STYLE(_type, _val) " margin-" _type ":" _val "ch;"
	#define DIR_STYLE(_type) " direction:" _type ";"

	struct _tests {
		const gchar *html;
		const gchar *plain;
		gint normal_div_width;
	} tests[] = {
	/* 0 */	{ HTML ("<div style='width:10ch; " BREAK_STYLE "'>123 5678 0123 5678 0123  678 1234567890abcdefghijklmnopq 012345         356  9         " TAB TAB "0</div>"),
		  "123 5678\n0123 5678\n0123 678\n1234567890\nabcdefghij\nklmnopq\n012345 356\n9\n0\n",
		  -1 },
	/* 1 */	{ HTML ("<div style='width:10ch; " WRAP_STYLE ("normal") "'>123 5678 0123 5678 0123  678 1234567890abcdefghijklmnopq 012345         356  9         " TAB TAB "0</div>"),
		  "123 5678\n0123 5678\n0123 678\n1234567890\nabcdefghij\nklmnopq\n012345 356\n9\n0\n",
		  -1 },
	/* 2 */	{ HTML ("<div style='width:10ch; " WRAP_STYLE ("nowrap") "'>123 5678 0123 5678 0123  678 1234567890abcdefghijklmnopq 012345         356  9         " TAB TAB "0</div>"),
		  "123 5678 0123 5678 0123 678 1234567890abcdefghijklmnopq 012345 356 9 0\n",
		  -1 },
	/* 3 */	{ HTML ("<div style='width:10ch; " WRAP_STYLE ("pre") "'>123 5678 0123 5678 0123  678 1234567890abcdefghijklmnopq 012345         356  9         " TAB TAB "0</div>"),
		  "123 5678 0\n123 5678 0\n123  678 1\n234567890a\nbcdefghijk\nlmnopq 012\n345       \n  356  9  \n       \t\t\n0\n",
		  -1 },
	/* 4 */	{ HTML ("<div style='width:10ch; " WRAP_STYLE ("pre") "'>123 5678 0123 5678 0123  678 1234567890abcdefghijklmnopq 012345         356  9          " TAB TAB "0</div>"),
		  "123 5678 0\n123 5678 0\n123  678 1\n234567890a\nbcdefghijk\nlmnopq 012\n345       \n  356  9  \n        \t\n\t0\n",
		  -1 },
	/* 5 */	{ HTML ("<div style='width:10ch; " WRAP_STYLE ("pre-line") "'>123 5678 0123 5678 0123  678 1234567890abcdefghijklmnopq 012345         356  9         " TAB TAB "0</div>"),
		  "123 5678\n0123 5678\n0123 678\n1234567890\nabcdefghij\nklmnopq\n012345 356\n9\n0\n",
		  -1 },
	/* 6 */	{ HTML ("<div style='width:10ch; " WRAP_STYLE ("pre-wrap") "'>123 5678 0123 5678 0123  678 1234567890abcdefghijklmnopq 012345         356  9         " TAB TAB "0</div>"),
		  "123 5678\n0123 5678\n0123  678\n1234567890\nabcdefghij\nklmnopq\n012345   \n356  9   \n\t\n\t0\n",
		  -1 },
	/* 7 */	{ HTML ("<pre>123456789012345\n1\t90123\n123   78901\n  34567   <br>123 5</pre>"),
		  "123456789012345\n1\t90123\n123   78901\n  34567   \n123 5\n",
		  -1 },
	/* 8 */	{ HTML ("<pre>123456789012345\n1\t90123\n123   78901\n  34567   <br>123 5</pre>"),
		  "123456789012345\n1\t90123\n123   78901\n  34567   \n123 5\n",
		  10 },
	/* 9 */	{ HTML ("<h1>Header1</h1>"
			"<div style='width:10ch; " WRAP_STYLE ("normal") "'>123456 789 123 4567890 123456 789 122</div>"
			"<div style='width:10ch; " WRAP_STYLE ("normal") "'>987654321 987 654 321 12345678901234567890</div>"),
		  "Header1\n"
		  "123456 789\n123\n4567890\n123456 789\n122\n"
		  "987654321\n987 654\n321\n1234567890\n1234567890\n",
		  -1 },
	/* 10 */{ HTML ("<h1>Header1</h1>"
			"<div style='width:10ch; " WRAP_STYLE ("pre-wrap") "'>123456 789 123 4567890 123456 789 122</div>"
			"<div style='width:10ch; " WRAP_STYLE ("pre-wrap") "'>987654321 987 654 321 12345678901234567890</div>"),
		  "Header1\n"
		  "123456 789\n123\n4567890\n123456 789\n122\n"
		  "987654321\n987 654\n321\n1234567890\n1234567890\n",
		  -1 },
	/* 11 */{ HTML ("<h1>H1</h1><h2>H2</h2><h3>H3</h3><h4>H4</h4><h5>H5</h5><h6>H6</h6>"),
		  "H1\nH2\nH3\nH4\nH5\nH6\n",
		  -1 },
	/* 12 */{ HTML ("<address>Line 1<br>Line 2<br>Line 3 ...</address>"),
		  "Line 1\nLine 2\nLine 3 ...\n",
		  -1 },
	/* 13 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") "'>1</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") "'>1 2</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") "'>1 2 3</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") "'>1 2 3 4</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") "'>1 2 3 4 5</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") "'>1 2 3 4 5 6</div>"),
		  "1\n1 2\n1 2 3\n1 2 3 4\n1 2 3 4 5\n1 2 3 4 5\n6\n",
		  -1 },
	/* 14 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") "'>1</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") "'>1 2</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") "'>1 2 3</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") "'>1 2 3 4</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") "'>1 2 3 4 5</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") "'>1 2 3 4 5 6</div>"),
		  "    1\n   1 2\n  1 2 3\n 1 2 3 4\n1 2 3 4 5\n1 2 3 4 5\n    6\n",
		  -1 },
	/* 15 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("right") "'>1</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("right") "'>1 2</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("right") "'>1 2 3</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("right") "'>1 2 3 4</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("right") "'>1 2 3 4 5</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("right") "'>1 2 3 4 5 6</div>"),
		  "         1\n"
		  "       1 2\n"
		  "     1 2 3\n"
		  "   1 2 3 4\n"
		  " 1 2 3 4 5\n"
		  " 1 2 3 4 5\n"
		  "         6\n",
		  -1 },
	/* 16 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") "'>1 aaaaaaaaa</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") "'>1 2 aaaaaaa</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") "'>1 2 3 aaaaa</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") "'>1 2 3 4 aaa</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") "'>1 2 3 4 5 a</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") "'>1 2 3 4 5 6 7</div>"),
		  "1\n"
		  "aaaaaaaaa\n"
		  "1        2\n"
		  "aaaaaaa\n"
		  "1    2   3\n"
		  "aaaaa\n"
		  "1  2  3  4\n"
		  "aaa\n"
		  "1  2 3 4 5\n"
		  "a\n"
		  "1  2 3 4 5\n"
		  "6 7\n",
		  -1 },
	/* 17 */{ HTML ("<div style='width:10ch; " BREAK_STYLE "'>123456 789 123 4567890 123456 789 122</div>"),
		  "123456 789\n123\n4567890\n123456 789\n122\n",
		  -1 },
	/* 18 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") "'>123456 789 123 4567890 123456 789 122</div>"),
		  "123456 789\n123\n4567890\n123456 789\n122\n",
		  -1 },
	/* 19 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") "'>123456 789 123 4567890 123456 789 122</div>"),
		  "123456 789\n"
		  "   123\n"
		  " 4567890\n"
		  "123456 789\n"
		  "   122\n",
		  -1 },
	/* 20 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("right") "'>123456 789 123 4567890 123456 789 122</div>"),
		  "123456 789\n"
		  "       123\n"
		  "   4567890\n"
		  "123456 789\n"
		  "       122\n",
		  -1 },
	/* 21 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") "'>123456 789 1 3 456 890 1234 789 1 2 3 4 5 122</div>"),
		  "123456 789\n"
		  "1   3  456\n"
		  "890   1234\n"
		  "789  1 2 3\n"
		  "4 5 122\n",
		  -1 },
	/* 22 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") DIR_STYLE ("rtl") "'>1</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") DIR_STYLE ("rtl") "'>1 2</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") DIR_STYLE ("rtl") "'>1 2 3</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") DIR_STYLE ("rtl") "'>1 2 3 4</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") DIR_STYLE ("rtl") "'>1 2 3 4 5</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") DIR_STYLE ("rtl") "'>1 2 3 4 5 6</div>"),
		  "1         \n"
		  "1 2       \n"
		  "1 2 3     \n"
		  "1 2 3 4   \n"
		  "1 2 3 4 5 \n"
		  "1 2 3 4 5 \n"
		  "6         \n",
		  -1 },
	/* 23 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") DIR_STYLE ("rtl") "'>1</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") DIR_STYLE ("rtl") "'>1 2</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") DIR_STYLE ("rtl") "'>1 2 3</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") DIR_STYLE ("rtl") "'>1 2 3 4</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") DIR_STYLE ("rtl") "'>1 2 3 4 5</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") DIR_STYLE ("rtl") "'>1 2 3 4 5 6</div>"),
		  "1    \n1 2   \n1 2 3  \n1 2 3 4 \n1 2 3 4 5\n1 2 3 4 5\n6    \n",
		  -1 },
	/* 24 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("right") DIR_STYLE ("rtl") "'>1</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("right") DIR_STYLE ("rtl") "'>1 2</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("right") DIR_STYLE ("rtl") "'>1 2 3</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("right") DIR_STYLE ("rtl") "'>1 2 3 4</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("right") DIR_STYLE ("rtl") "'>1 2 3 4 5</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("right") DIR_STYLE ("rtl") "'>1 2 3 4 5 6</div>"),
		  "1\n1 2\n1 2 3\n1 2 3 4\n1 2 3 4 5\n1 2 3 4 5\n6\n",
		  -1 },
	/* 25 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") DIR_STYLE ("rtl") "'>1 aaaaaaaaa</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") DIR_STYLE ("rtl") "'>1 2 aaaaaaa</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") DIR_STYLE ("rtl") "'>1 2 3 aaaaa</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") DIR_STYLE ("rtl") "'>1 2 3 4 aaa</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") DIR_STYLE ("rtl") "'>1 2 3 4 5 a</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") DIR_STYLE ("rtl") "'>1 2 3 4 5 6 7</div>"),
		  "1\n"
		  "aaaaaaaaa\n"
		  "1        2\n"
		  "aaaaaaa\n"
		  "1    2   3\n"
		  "aaaaa\n"
		  "1  2  3  4\n"
		  "aaa\n"
		  "1  2 3 4 5\n"
		  "a\n"
		  "1  2 3 4 5\n"
		  "6 7\n",
		  -1 },
	/* 26 */{ HTML ("<div style='width:10ch; " BREAK_STYLE DIR_STYLE ("rtl") "'>123456 789 123 4567890 123456 789 122</div>"),
		  "123456 789\n"
		  "123\n"
		  "4567890\n"
		  "123456 789\n"
		  "122\n",
		  -1 },
	/* 27 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") DIR_STYLE ("rtl") "'>123456 789 123 4567890 123456 789 122</div>"),
		  "123456 789\n"
		  "123       \n"
		  "4567890   \n"
		  "123456 789\n"
		  "122       \n",
		  -1 },
	/* 28 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") DIR_STYLE ("rtl") "'>123456 789 123 4567890 123456 789 122</div>"),
		  "123456 789\n"
		  "123   \n"
		  "4567890 \n"
		  "123456 789\n"
		  "122   \n",
		  -1 },
	/* 29 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("right") DIR_STYLE ("rtl") "'>123456 789 123 4567890 123456 789 122</div>"),
		  "123456 789\n123\n4567890\n123456 789\n122\n",
		  -1 },
	/* 30 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") DIR_STYLE ("rtl") "'>123456 789 1 3 456 890 1234 789 1 2 3 4 5 122</div>"),
		  "123456 789\n"
		  "1   3  456\n"
		  "890   1234\n"
		  "789  1 2 3\n"
		  "4 5 122\n",
		  -1 },
	/* 31 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") INDENT_STYLE ("left", "3") "'>123 567 901 345 789 123 5</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") INDENT_STYLE ("left", "6") "'>987 543 109 765 321 098 6</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("left") INDENT_STYLE ("left", "3") "'>111 222 333 444 555 666 7</div>"),
		  "   123 567\n"
		  "   901 345\n"
		  "   789 123 5\n"
		  "      987 543\n"
		  "      109 765\n"
		  "      321 098 6\n"
		  "   111 222\n"
		  "   333 444\n"
		  "   555 666 7\n",
		  -1 },
	/* 32 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") INDENT_STYLE ("left", "3") "'>123 567 901 345 789 123 5</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") INDENT_STYLE ("left", "6") "'>987 543 109 765 321 098 6</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("center") INDENT_STYLE ("left", "3") "'>111 222 333 444 555 666 7</div>"),
		  "    123 567\n"
		  "    901 345\n"
		  "   789 123 5\n"
		  "       987 543\n"
		  "       109 765\n"
		  "      321 098 6\n"
		  "    111 222\n"
		  "    333 444\n"
		  "   555 666 7\n",
		  -1 },
	/* 33 */{ HTML ("<div style='width:10ch; " BREAK_STYLE DIR_STYLE ("rtl") INDENT_STYLE ("right", "3") "'>123 567 901 345 789 123 5</div>"
			"<div style='width:10ch; " BREAK_STYLE DIR_STYLE ("rtl") INDENT_STYLE ("right", "6") "'>987 543 109 765 321 098 6</div>"
			"<div style='width:10ch; " BREAK_STYLE DIR_STYLE ("rtl") INDENT_STYLE ("right", "3") "'>111 222 333 444 555 666 7</div>"),
		  "123 567   \n"
		  "901 345   \n"
		  "789 123 5   \n"
		  "987 543      \n"
		  "109 765      \n"
		  "321 098 6      \n"
		  "111 222   \n"
		  "333 444   \n"
		  "555 666 7   \n",
		  -1 },
	/* 34 */{ HTML ("<div style='width:10ch; " BREAK_STYLE DIR_STYLE ("ltr") ALIGN_STYLE ("right") INDENT_STYLE ("left", "3") "'>123 567 901 345 789 123 5</div>"
			"<div style='width:10ch; " BREAK_STYLE DIR_STYLE ("ltr") ALIGN_STYLE ("right") INDENT_STYLE ("left", "6") "'>987 543 109 765 321 098 6</div>"
			"<div style='width:10ch; " BREAK_STYLE DIR_STYLE ("ltr") ALIGN_STYLE ("right") INDENT_STYLE ("left", "3") "'>111 222 333 444 555 666 7</div>"),
		  "   123 567\n"
		  "   901 345\n"
		  " 789 123 5\n"
		  "   987 543\n"
		  "   109 765\n"
		  " 321 098 6\n"
		  "   111 222\n"
		  "   333 444\n"
		  " 555 666 7\n",
		  -1 },
	/* 35 */{ HTML ("<div style='width:10ch; " BREAK_STYLE DIR_STYLE ("rtl") ALIGN_STYLE ("left") INDENT_STYLE ("right", "3") "'>123 567 901 345 789 123 5</div>"
			"<div style='width:10ch; " BREAK_STYLE DIR_STYLE ("rtl") ALIGN_STYLE ("left") INDENT_STYLE ("right", "6") "'>987 543 109 765 321 098 6</div>"
			"<div style='width:10ch; " BREAK_STYLE DIR_STYLE ("rtl") ALIGN_STYLE ("left") INDENT_STYLE ("right", "3") "'>111 222 333 444 555 666 7</div>"),
		  "123 567   \n"
		  "901 345   \n"
		  "789 123 5 \n"
		  "987 543   \n"
		  "109 765   \n"
		  "321 098 6 \n"
		  "111 222   \n"
		  "333 444   \n"
		  "555 666 7 \n",
		  -1 },
	/* 36 */{ HTML ("<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") INDENT_STYLE ("left", "3") "'>123 567 901 345 789 123 5</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") INDENT_STYLE ("left", "6") "'>987 543 109 765 321 098 6</div>"
			"<div style='width:10ch; " BREAK_STYLE ALIGN_STYLE ("justify") INDENT_STYLE ("left", "3") "'>111 222 333 444 555 666 7</div>"),
		  "   123    567\n"
		  "   901    345\n"
		  "   789 123 5\n"
		  "      987    543\n"
		  "      109    765\n"
		  "      321 098 6\n"
		  "   111    222\n"
		  "   333    444\n"
		  "   555 666 7\n",
		  -1 },
	/* 37 */{ HTML ("<ul style='width: 9ch;'>"
			"<li>1 11 111 1111 111 11 1</li>"
			"<li>2 22 222 2222 222 22 2</li>"
			"<li>3 33 333 3333 33333 333 33 3</li>"
			"</ul>"),
		  " * 1 11 111\n"
		  "   1111 111\n"
		  "   11 1\n"
		  " * 2 22 222\n"
		  "   2222 222\n"
		  "   22 2\n"
		  " * 3 33 333\n"
		  "   3333\n"
		  "   33333 333\n"
		  "   33 3\n",
		  -1 },
	/* 38 */{ HTML ("<ol style='width: 9ch;'>"
			"<li>1 11 111 1111 111 11 1</li>"
			"<li>2 22 222 2222 222 22 2</li>"
			"<li>3 33 333 3333 33333 333 33 3</li>"
			"</ol>"),
		  "   1. 1 11 111\n"
		  "      1111 111\n"
		  "      11 1\n"
		  "   2. 2 22 222\n"
		  "      2222 222\n"
		  "      22 2\n"
		  "   3. 3 33 333\n"
		  "      3333\n"
		  "      33333 333\n"
		  "      33 3\n",
		  -1 },
	/* 39 */{ HTML ("<ol type='A' style='width: 9ch;'>"
			"<li>1 11 111 1111 111 11 1</li>"
			"<li>2 22 222 2222 222 22 2</li>"
			"<li>3 33 333 3333 33333 333 33 3</li>"
			"</ol>"),
		  "   A. 1 11 111\n"
		  "      1111 111\n"
		  "      11 1\n"
		  "   B. 2 22 222\n"
		  "      2222 222\n"
		  "      22 2\n"
		  "   C. 3 33 333\n"
		  "      3333\n"
		  "      33333 333\n"
		  "      33 3\n",
		  -1 },
	/* 40 */{ HTML ("<ol type='a' style='width: 9ch;'>"
			"<li>1 11 111 1111 111 11 1</li>"
			"<li>2 22 222 2222 222 22 2</li>"
			"<li>3 33 333 3333 33333 333 33 3</li>"
			"</ol>"),
		  "   a. 1 11 111\n"
		  "      1111 111\n"
		  "      11 1\n"
		  "   b. 2 22 222\n"
		  "      2222 222\n"
		  "      22 2\n"
		  "   c. 3 33 333\n"
		  "      3333\n"
		  "      33333 333\n"
		  "      33 3\n",
		  -1 },
	/* 41 */{ HTML ("<ol type='I' style='width: 9ch;'>"
			"<li>1 11 111 1111 111 11 1</li>"
			"<li>2 22 222 2222 222 22 2</li>"
			"<li>3 33 333 3333 33333 333 33 3</li>"
			"</ol>"),
		  "   I. 1 11 111\n"
		  "      1111 111\n"
		  "      11 1\n"
		  "  II. 2 22 222\n"
		  "      2222 222\n"
		  "      22 2\n"
		  " III. 3 33 333\n"
		  "      3333\n"
		  "      33333 333\n"
		  "      33 3\n",
		  -1 },
	/* 42 */{ HTML ("<ol type='i' style='width: 9ch;'>"
			"<li>1 11 111 1111 111 11 1</li>"
			"<li>2 22 222 2222 222 22 2</li>"
			"<li>3 33 333 3333 33333 333 33 3</li>"
			"</ol>"),
		  "   i. 1 11 111\n"
		  "      1111 111\n"
		  "      11 1\n"
		  "  ii. 2 22 222\n"
		  "      2222 222\n"
		  "      22 2\n"
		  " iii. 3 33 333\n"
		  "      3333\n"
		  "      33333 333\n"
		  "      33 3\n",
		  -1 },
	/* 43 */{ HTML ("<ol type='i' style='width: 9ch;'>"
			"<li>1</li>"
			"<li>2</li>"
			"<li>3</li>"
			"<li>4</li>"
			"<li>5</li>"
			"<li>6</li>"
			"<li>7</li>"
			"<li>8</li>"
			"<li>9</li>"
			"<li>10</li>"
			"<li>11</li>"
			"<li>12</li>"
			"<li>13</li>"
			"<li>14</li>"
			"<li>15</li>"
			"<li>16</li>"
			"<li>17</li>"
			"<li>18</li>"
			"<li>19</li>"
			"<li>20</li>"
			"</ol>"),
		  "    i. 1\n"
		  "   ii. 2\n"
		  "  iii. 3\n"
		  "   iv. 4\n"
		  "    v. 5\n"
		  "   vi. 6\n"
		  "  vii. 7\n"
		  " viii. 8\n"
		  "   ix. 9\n"
		  "    x. 10\n"
		  "   xi. 11\n"
		  "  xii. 12\n"
		  " xiii. 13\n"
		  "  xiv. 14\n"
		  "   xv. 15\n"
		  "  xvi. 16\n"
		  " xvii. 17\n"
		  "xviii. 18\n"
		  "  xix. 19\n"
		  "   xx. 20\n",
		  -1 },
	/* 44 */{ HTML ("<ol style='width: 9ch; " DIR_STYLE ("rtl") "'>"
			"<li>1 11 111 1111 111 11 1</li>"
			"<li>2 22 222 2222 222 22 2</li>"
			"<li>3 33 333 3333 33333 333 33 3</li>"
			"<li>4</li>"
			"</ol>"),
		  "1 11 111 .1   \n"
		  "1111 111      \n"
		  "11 1      \n"
		  "2 22 222 .2   \n"
		  "2222 222      \n"
		  "22 2      \n"
		  "3 33 333 .3   \n"
		  "3333      \n"
		  "33333 333      \n"
		  "33 3      \n"
		  "4 .4   \n",
		  -1 },
	/* 45 */{ HTML ("<ol type='I' style='width: 9ch; " DIR_STYLE ("rtl") "'>"
			"<li>1</li>"
			"<li>2</li>"
			"<li>3</li>"
			"<li>4</li>"
			"<li>5</li>"
			"<li>6</li>"
			"<li>7</li>"
			"<li>8</li>"
			"<li>9</li>"
			"<li>10</li>"
			"<li>11</li>"
			"<li>12</li>"
			"<li>13</li>"
			"<li>14</li>"
			"<li>15</li>"
			"<li>16</li>"
			"<li>17</li>"
			"<li>18</li>"
			"<li>19</li>"
			"<li>20</li>"
			"</ol>"),
		  "1 .I    \n"
		  "2 .II   \n"
		  "3 .III  \n"
		  "4 .IV   \n"
		  "5 .V    \n"
		  "6 .VI   \n"
		  "7 .VII  \n"
		  "8 .VIII \n"
		  "9 .IX   \n"
		  "10 .X    \n"
		  "11 .XI   \n"
		  "12 .XII  \n"
		  "13 .XIII \n"
		  "14 .XIV  \n"
		  "15 .XV   \n"
		  "16 .XVI  \n"
		  "17 .XVII \n"
		  "18 .XVIII\n"
		  "19 .XIX  \n"
		  "20 .XX   \n",
		  -1 },
	/* 46 */{ HTML ("<ul style='width: 15ch; padding-inline-start: 3ch;'>"
			"<li>AA 1 2 3 4 5 6 7 8 9 11 22 33</li>"
			"<ul style='width: 12ch; padding-inline-start: 3ch;'>"
			"<li>BA 1 2 3 4 5 6 7 8 9</li>"
			"<li>BB 1 2 3 4 5 6 7 8 9</li>"
			"<ul style='width: 9ch; padding-inline-start: 3ch;'>"
			"<li>CA 1 2 3 4 5 6</li>"
			"<li>CB 1 2 3 4 5 6</li>"
			"</ul>"
			"<li>BC 1 2 3 4 5 6</li>"
			"</ul>"
			"<li>AB 1 2 3 4 5 6 7</li>"
			"</ul>"),
		  " * AA 1 2 3 4 5 6\n"
		  "   7 8 9 11 22 33\n"
		  "    - BA 1 2 3 4 5\n"
		  "      6 7 8 9\n"
		  "    - BB 1 2 3 4 5\n"
		  "      6 7 8 9\n"
		  "       + CA 1 2 3\n"
		  "         4 5 6\n"
		  "       + CB 1 2 3\n"
		  "         4 5 6\n"
		  "    - BC 1 2 3 4 5\n"
		  "      6\n"
		  " * AB 1 2 3 4 5 6\n"
		  "   7\n",
		  -1 },
	/* 47 */{ HTML ("<ol>"
			  "<li>1</li>"
			  "<ul>"
			    "<li>1.-</li>"
			    "<ol type='i'>"
			      "<li>1.-.i</li>"
			      "<ol type='a'>"
			        "<li>1.-.i.a</li>"
			        "<li>1.-.i.b</li>"
			      "</ol>"
			      "<li>1.-.ii</li>"
			      "<ol type='A'>"
			        "<li>1.-.ii.A</li>"
			        "<ul>"
			          "<li>1.-.ii.A.-</li>"
			          "<ul>"
			            "<li>1.-.ii.A.-.+</li>"
				    "<ol type='I'>"
			               "<li>1.-.ii.A.-.+.I</li>"
			               "<li>1.-.ii.A.-.+.II</li>"
			               "<li>1.-.ii.A.-.+.III</li>"
				    "</ol>"
			            "<li>1.-.ii.A.-.+</li>"
			          "</ul>"
			          "<li>1.-.ii.A.-</li>"
			        "</ul>"
			        "<li>1.-.ii.B</li>"
			      "</ol>"
			      "<li>1.-.iii</li>"
			    "</ol>"
			    "<li>1.-</li>"
			  "</ul>"
			  "<li>2</li>"
			"</ol>"),
		  "   1. 1\n"
		  "       - 1.-\n"
		  "            i. 1.-.i\n"
		  "                  a. 1.-.i.a\n"
		  "                  b. 1.-.i.b\n"
		  "           ii. 1.-.ii\n"
		  "                  A. 1.-.ii.A\n"
		  "                      - 1.-.ii.A.-\n"
		  "                         + 1.-.ii.A.-.+\n"
		  "                              I. 1.-.ii.A.-.+.I\n"
		  "                             II. 1.-.ii.A.-.+.II\n"
		  "                            III. 1.-.ii.A.-.+.III\n"
		  "                         + 1.-.ii.A.-.+\n"
		  "                      - 1.-.ii.A.-\n"
		  "                  B. 1.-.ii.B\n"
		  "          iii. 1.-.iii\n"
		  "       - 1.-\n"
		  "   2. 2\n",
		  -1 },
	/* 48 */{ HTML ("<div style='width:10ch'>123456789 1234567890123456789 12345678901234567890 123456789012345678901</div>"),
		"123456789\n"
		"1234567890\n"
		"123456789\n"
		"1234567890\n"
		"1234567890\n"
		"1234567890\n"
		"1234567890\n"
		"1\n",
		10 },
	/* 49 */{ HTML ("<div style='width:70ch'>before <img src='https://no.where/img.img'> after</div>"),
		"before after\n",
		70 },
	/* 50 */{ HTML ("<div style='width:70ch'>before <img src='https://no.where/img.img' alt='alt'> after</div>"),
		"before alt after\n",
		70 },
	/* 51 */{ HTML ("<div style='width:70ch'>before <a href='https://no.where/'>https://no.where/</a> after</div>"),
		"before https://no.where/ after\n",
		70 },
	/* 52 */{ HTML ("<div style='width:70ch'>before <a href='https://no.where/'>here</a> after</div>"),
		"before here after\n",
		70 },
	/* 53 */{ HTML ("<div style='width:31ch'>before <a href='https://no.where/'>https://no.where/</a> after</div>"),
		"before https://no.where/ after\n",
		31 },
	/* 54 */{ HTML ("<div style='width:26ch'>before <a href='https://no.where/'>https://no.where/</a> after</div>"),
		"before https://no.where/\n"
		"after\n",
		26 },
	/* 55 */{ HTML ("<div style='width:20ch'>before <a href='https://no.where/'>https://no.where/</a> after</div>"),
		"before\n"
		"https://no.where/\n"
		"after\n",
		20 },
	/* 56 */{ HTML ("<div style='width:20ch'>before <a href='https://no.where/'>https://no.where/1234567890/123457890/1234567890</a> after</div>"),
		"before\n"
		"https://no.where/1234567890/123457890/1234567890\n"
		"after\n",
		20 },
	/* 57 */{ HTML ("<p><div style='width:20ch'>before <a href='https://no.where/'>https://no.where/</a> after</div></p>"),
		"before\n"
		"https://no.where/\n"
		"after\n",
		20 },
	/* 58 */{ HTML ("<p><div style='width:20ch'>before <a href='https://no.where/'>https://no.where/1234567890/123457890/1234567890</a> after</div></p>"),
		"before\n"
		"https://no.where/1234567890/123457890/1234567890\n"
		"after\n",
		20 },
	/* 59 */{ HTML ("<div style='width:16ch'>before <a href='https://no.where/'>anchor text</a> after</div>"),
		"before anchor\n"
		"text after\n",
		16 },
	/* 60 */{ HTML ("<div>text before<br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\"><a href='https://no.where/'>https://no.where/1234567890/123457890/1234567890</a><br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\">text after</div>"),
		"text\n"
		"before\n"
		"https://no.where/1234567890/123457890/1234567890\n"
		"text\n"
		"after\n",
		6 },
	/* 61 */{ HTML ("<div>text before<br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\"><a href='https://no.where/'>https://no.where/1234567890/123457890/1234567890</a><br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\">text after</div>"),
		"text\n"
		"before\n"
		"https://no.where/1234567890/123457890/1234567890\n"
		"text\n"
		"after\n",
		9 },
	/* 62 */{ HTML ("<div>text before<br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\"><a href='https://no.where/'>https://no.where/1234567890/123457890/1234567890</a><br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\">text after</div>"),
		"text\n"
		"before\n"
		"https://no.where/1234567890/123457890/1234567890\n"
		"text after\n",
		10 },
	/* 63 */{ HTML ("<div>text before<br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\"><a href='https://no.where/'>https://no.where/1234567890/123457890/1234567890</a><br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\">text after</div>"),
		"text before\n"
		"https://no.where/1234567890/123457890/1234567890\n"
		"text after\n",
		11 },
	/* 64 */{ HTML ("<div>text before<br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\"><a href='https://no.where/'>https://no.where/1234567890/123457890/1234567890</a><br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\">text after</div>"),
		"text before\n"
		"https://no.where/1234567890/123457890/1234567890\n"
		"text after\n",
		12 },
	/* 65 */{ HTML ("<div>text before<br><a href='https://no.where/'>https://no.where/1234567890/123457890/1234567890</a><br>text after</div>"),
		"text before\n"
		"https://no.where/1234567890/123457890/1234567890\n"
		"text after\n",
		12 },
	/* 66 */{ HTML ("<div>line1<br>\n"
		"line2<br>\n"
		"line3<br>\n"
		"<br>\n"
		"<br>\n"
		"line6<br>\n"
		"</div>"),
		"line1\n"
		"line2\n"
		"line3\n"
		"\n"
		"\n"
		"line6\n",
		71 },
	/* 67 */{ HTML ("<div>123 456-78 123456 7-890123 123-456-789- 1122------------- 789 --------------------- 123</div>"),
		"123 456-78\n"
		"123456 7-\n"
		"890123\n"
		"123-456-\n"
		"789- 1122-\n"
		"----------\n"
		"-- 789 ---\n"
		"----------\n"
		"--------\n"
		"123\n",
		10 },
	/* 68 */{ HTML ("<div>123<div>456</div><div><br></div><div>7 8 9<b>b</b><div>abc</div>def<br><div>ghi</div></div></div>"),
		"123\n"
		"456\n"
		"\n"
		"7 8 9b\n"
		"abc\n"
		"def\n"
		"ghi\n",
		10 },
	/* 69 */{ HTML ("<div>123<div>456</div><div><br></div><div><div>7 8 9<b>b</b></div><div>abc</div>def<br><div>ghi</div></div></div>"),
		"123\n"
		"456\n"
		"\n"
		"7 8 9b\n"
		"abc\n"
		"def\n"
		"ghi\n",
		10 },
	/* 70 */{ HTML ("<div>123\n"
			"   <div>456</div>\n"
			"   <div><br></div>\n"
			"   <div>\n"
			"	<div>7 8 9<b>b</b></div>\n"
			"	<div>abc</div>\n"
			"	def<br>\n"
			"   	<div>ghi</div>\n"
			"   </div>\n"
			"</div>"),
		"123 \n" /* The space should not be there, but see EvoConvert.appendNodeText() */
		"456\n"
		"\n"
		"7 8 9b\n"
		"abc\n"
		"def\n"
		"ghi\n",
		10 },
	/* 71 */{ HTML ("<div>aaa bbb,\n"
			"<div><div><br></div>\n"
			"<div>cc dd ee\n"
			"</div>\n"
			"<div><br></div>\n"
			"<div>ff,<b>gg</b></div>\n"
			"<div>-- <br>\n"
			"   <div>\n"
			"      <div>hh ii<div>jj kk</div>\n"
			"   </div>\n"
			"</div>\n"
			"</div></div></div>\n"),
		"aaa bbb,\n"
		"\n"
		"cc dd ee\n"
		"\n"
		"ff,gg\n"
		"-- \n"
		"hh ii\n"
		"jj kk\n",
		10 },
	/* 72 */{ HTML ("<div>a72<b>\n</b>b72<div>"),
		"a72\nb72\n",
		10 },
	/* 73 */{ HTML ("<div>a73<b> </b>b73<div>"),
		"a73 b73\n",
		10 },
	/* 74 */{ HTML ("<div>a74<b>    \t   </b>b74<div>"),
		"a74 b74\n",
		10 },
	/* 75 */{ HTML ("<div>a75<b>  \n \t \r\n  </b>b75<div>"),
		"a75 b75\n",
		10 },
	/* 76 */{ HTML ("<div>a76  \n <b> x  </b>\r\n  </b>b76<div>"),
		"a76 x b76\n",
		10 }
	};

	#undef HTML
	#undef TAB
	#undef BREAK_STYLE
	#undef WRAP_STYLE
	#undef ALIGN_STYLE
	#undef INDENT_STYLE
	#undef DIR_STYLE

	gchar *script;
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (tests); ii++) {
		test_utils_load_string (fixture, tests[ii].html);

		script = e_web_view_jsc_printf_script ("EvoConvert.ToPlainText(document.body, %d);", tests[ii].normal_div_width);

		test_utils_jsc_call_string_and_verify (fixture, script, tests[ii].plain);

		g_free (script);
	}
}

static void
test_convert_to_plain_quoted (TestFixture *fixture)
{
	#define HTML(_body) ("<html><head><style><!-- span.Apple-tab-span { white-space:pre; } --></style></head><body style='font-family:monospace;'>" _body "</body></html>")
	#define QUOTE_SPAN(x) "<span class='-x-evo-quoted'>" x "</span>"
	#define QUOTE_CHR "<span class='-x-evo-quote-character'>&gt; </span>"

	struct _tests {
		const gchar *html;
		const gchar *plain;
		gint normal_div_width;
	} tests[] = {
	/* 0 */ { HTML ("<div style='width:10ch;'>123 456 789 123</div>"
		"<blockquote type='cite'>"
			"<div style='width:8ch;'>123 456 789 1 2 3 4</div>"
			"<div style='width:8ch;'>abc def ghi j k l m</div>"
		"</blockquote>"
		"<div>end</div>"),
		"123 456\n"
		"789 123\n"
		"> 123 456\n"
		"> 789 1 2\n"
		"> 3 4\n"
		"> abc def\n"
		"> ghi j k\n"
		"> l m\n"
		"end\n",
		10 },
	/* 1 */ { HTML ("<div style='width:12ch;'>123 456</div>"
		"<blockquote type='cite'>"
			"<div style='width:10ch;'>123 456</div>"
			"<blockquote>"
				"<div style='width:8ch;'>789 1 2 3 4</div>"
			"</blockquote>"
			"<div style='width:10ch;'>mid</div>"
			"<blockquote>"
				"<blockquote>"
					"<div style='width:6ch;'>abc</div>"
					"<div style='width:6ch;'>def ghi j k l m</div>"
				"</blockquote>"
				"<div style='width:8ch;'>abc d e f g h i j</div>"
			"</blockquote>"
			"<div style='width:10ch;'>l1 a b c d e f g</div>"
		"</blockquote>"
		"<div>end</div>"),
		"123 456\n"
		"> 123 456\n"
		"> > 789 1 2\n"
		"> > 3 4\n"
		"> mid\n"
		"> > > abc\n"
		"> > > def\n"
		"> > > ghi j\n"
		"> > > k l m\n"
		"> > abc d e\n"
		"> > f g h i\n"
		"> > j\n"
		"> l1 a b c d\n"
		"> e f g\n"
		"end\n",
		10 },
	/* 2 */ { HTML ("<div style='width:10ch;'>123 456<br>789 123</div>"
		"<blockquote type='cite'>"
			"<div style='width:8ch;'>123 456<br>789 1 2 3 4</div>"
			"<blockquote type='cite'>"
				"<div style='width:6ch;'>abc<br>def g h i j k</div>"
			"</blockquote>"
		"</blockquote>"
		"<div>end</div>"),
		"123 456\n"
		"789 123\n"
		"> 123 456\n"
		"> 789 1 2\n"
		"> 3 4\n"
		"> > abc\n"
		"> > def g\n"
		"> > h i j\n"
		"> > k\n"
		"end\n",
		10 },
	/* 3 */ { HTML ("<p style='width:10ch;'>123 456<br>789 123</p>"
		"<blockquote type='cite'>"
			"<p style='width:8ch;'>123 456<br>789 1 2 3 4</p>"
			"<blockquote type='cite'>"
				"<p style='width:6ch;'>abc<br>def g h i j k</p>"
			"</blockquote>"
		"</blockquote>"
		"<p>end</p>"),
		"123 456\n"
		"789 123\n"
		"> 123 456\n"
		"> 789 1 2\n"
		"> 3 4\n"
		"> > abc\n"
		"> > def g\n"
		"> > h i j\n"
		"> > k\n"
		"end\n",
		10 },
	/* 4 */ { HTML ("<pre>123 456 789 123</pre>"
		"<blockquote type='cite'>"
			"<pre>123 456 789 1 2 3 4</pre>"
			"<blockquote type='cite'>"
				"<pre>abc def g h i j k</pre>"
			"</blockquote>"
		"</blockquote>"
		"<pre>end</pre>"),
		"123 456 789 123\n"
		"> 123 456 789 1 2 3 4\n"
		"> > abc def g h i j k\n"
		"end\n",
		10 },
	/* 5 */ { HTML ("<pre>123 456\n789 123</pre>"
		"<blockquote type='cite'>"
			"<pre>123 456\n789 1 2 3 4</pre>"
			"<blockquote type='cite'>"
				"<pre>abc def\ng h\ni j k</pre>"
			"</blockquote>"
			"<pre>a b\nc\nd e f</pre>"
		"</blockquote>"
		"<pre>end</pre>"),
		"123 456\n"
		"789 123\n"
		"> 123 456\n"
		"> 789 1 2 3 4\n"
		"> > abc def\n"
		"> > g h\n"
		"> > i j k\n"
		"> a b\n"
		"> c\n"
		"> d e f\n"
		"end\n",
		10 },
	/* 6 */ { HTML ("<blockquote type='cite'>"
			"<div style='width:70ch'>before <a href='https://no.where/'>https://no.where/</a> after</div>"
		"</blockquote>"),
		"> before https://no.where/ after\n",
		70 },
	/* 7 */ { HTML ("<blockquote type='cite'>"
			"<div style='width:70ch'>before <a href='https://no.where/'>here</a> after</div>"
		"</blockquote>"),
		"> before here after\n",
		70 },
	/* 8 */ { HTML ("<blockquote type='cite'>"
			"<div style='width:31ch'>before <a href='https://no.where/'>https://no.where/</a> after</div>"
		"</blockquote>"),
		"> before https://no.where/ after\n",
		33 },
	/* 9 */ { HTML ("<blockquote type='cite'>"
			"<div style='width:26ch'>before <a href='https://no.where/'>https://no.where/</a> after</div>"
		"</blockquote>"),
		"> before https://no.where/\n"
		"> after\n",
		26 },
	/* 10 */{ HTML ("<blockquote type='cite'>"
			"<div style='width:20ch'>before <a href='https://no.where/'>https://no.where/</a> after</div>"
		"</blockquote>"),
		"> before\n"
		"> https://no.where/\n"
		"> after\n",
		20 },
	/* 11 */{ HTML ("<blockquote type='cite'>"
			"<div style='width:20ch'>before <a href='https://no.where/'>https://no.where/1234567890/123457890/1234567890</a> after</div>"
		"</blockquote>"),
		"> before\n"
		"> https://no.where/1234567890/123457890/1234567890\n"
		"> after\n",
		20 },
	/* 12 */{ HTML ("<blockquote type='cite'>"
			"<blockquote type='cite'>"
				"<div style='width:20ch'>before <a href='https://no.where/'>https://no.where/</a> after</div>"
			"</blockquote>"
		"</blockquote>"),
		"> > before\n"
		"> > https://no.where/\n"
		"> > after\n",
		20 },
	/* 13 */{ HTML ("<blockquote type='cite'>"
			"<blockquote type='cite'>"
				"<div style='width:20ch'>before <a href='https://no.where/'>https://no.where/1234567890/123457890/1234567890</a> after</div>"
			"</blockquote>"
		"</blockquote>"),
		"> > before\n"
		"> > https://no.where/1234567890/123457890/1234567890\n"
		"> > after\n",
		20 },
	/* 14 */{ HTML ("<blockquote type='cite'>"
			"<blockquote type='cite'>"
				"<div style='width:16ch'>before <a href='https://no.where/'>anchor text</a> after</div>"
			"</blockquote>"
		"</blockquote>"),
		"> > before anchor\n"
		"> > text after\n",
		16 },
	/* 15 */{ HTML ("<blockquote type='cite'>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "text before<br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\">"
			QUOTE_SPAN (QUOTE_CHR) "<a href='https://no.where/'>https://no.where/1234567890/123457890/1234567890</a><br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\">"
			QUOTE_SPAN (QUOTE_CHR) "text after</div>"
		"</blockquote>"),
		"> text\n"
		"> before\n"
		"> https://no.where/1234567890/123457890/1234567890\n"
		"> text\n"
		"> after\n",
		8 },
	/* 16 */{ HTML ("<blockquote type='cite'>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "text before<br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\">"
			QUOTE_SPAN (QUOTE_CHR) "<a href='https://no.where/'>https://no.where/1234567890/123457890/1234567890</a><br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\">"
			QUOTE_SPAN (QUOTE_CHR) "text after</div>"
		"</blockquote>"),
		"> text\n"
		"> before\n"
		"> https://no.where/1234567890/123457890/1234567890\n"
		"> text\n"
		"> after\n",
		11 },
	/* 17 */{ HTML ("<blockquote type='cite'>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "text before<br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\">"
			QUOTE_SPAN (QUOTE_CHR) "<a href='https://no.where/'>https://no.where/1234567890/123457890/1234567890</a><br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\">"
			QUOTE_SPAN (QUOTE_CHR) "text after</div>"
		"</blockquote>"),
		"> text\n"
		"> before\n"
		"> https://no.where/1234567890/123457890/1234567890\n"
		"> text after\n",
		12 },
	/* 18 */{ HTML ("<blockquote type='cite'>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "text before<br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\">"
			QUOTE_SPAN (QUOTE_CHR) "<a href='https://no.where/'>https://no.where/1234567890/123457890/1234567890</a><br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\">"
			QUOTE_SPAN (QUOTE_CHR) "text after</div>"
		"</blockquote>"),
		"> text before\n"
		"> https://no.where/1234567890/123457890/1234567890\n"
		"> text after\n",
		13 },
	/* 19 */{ HTML ("<blockquote type='cite'>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "text before<br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\">"
			QUOTE_SPAN (QUOTE_CHR) "<a href='https://no.where/'>https://no.where/1234567890/123457890/1234567890</a><br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\">"
			QUOTE_SPAN (QUOTE_CHR) "text after</div>"
		"</blockquote>"),
		"> text before\n"
		"> https://no.where/1234567890/123457890/1234567890\n"
		"> text after\n",
		14 },
	/* 20 */{ HTML ("<blockquote type='cite'>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "text before<br>"
			QUOTE_SPAN (QUOTE_CHR) "<a href='https://no.where/'>https://no.where/1234567890/123457890/1234567890</a><br>"
			QUOTE_SPAN (QUOTE_CHR) "text after</div>"
		"</blockquote>"),
		"> text before\n"
		"> https://no.where/1234567890/123457890/1234567890\n"
		"> text after\n",
		14 },
	/* 21 */{ HTML ("<blockquote type='cite'>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "line1<br>\n"
			QUOTE_SPAN (QUOTE_CHR) "line2<br>\n"
			QUOTE_SPAN (QUOTE_CHR) "line3<br>\n"
			QUOTE_SPAN (QUOTE_CHR) "<br>\n"
			QUOTE_SPAN (QUOTE_CHR) "<br>\n"
			QUOTE_SPAN (QUOTE_CHR) "line6<br>\n"
			"</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "paragraph 2<br>\n</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "paragraph 3\n</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "paragraph 4</div>"
		"</blockquote>"),
		"> line1\n"
		"> line2\n"
		"> line3\n"
		"> \n"
		"> \n"
		"> line6\n"
		"> paragraph 2\n"
		"> paragraph 3\n"
		"> paragraph 4\n",
		71 },
	/* 22 */{ HTML ("<div>level 0<br><div>level 0 nested</div></div>"
			"<blockquote type='cite'>"
				"<div>level 1<br>"
				"<div>level 1 nested</div></div>"
				"<blockquote type='cite'>"
					"<div>level 2<br>"
					"<div>level 2 nested</div>"
					"<div><pre>level 2 nested^2</pre></div>"
					"<blockquote type='cite'>"
						"<div>level 3 bq-nested<br>"
						"<div>level 3 bq-nested nested</div></div>"
					"</blockquote>"
					"level 2 back</div>"
					"<div>level 2 nested repeat</div>"
					"</div>"
					"<div>level 2 repeat</div>"
				"</blockquote>"
				"<div>level 1 back<br>"
				"<div>level 1 repeat - nested</div></div>"
				"<div>level 1 repeat</div>"
			"</blockquote>"
		"<div>level 0 back<br><div>level 0 back nested</div></div>"),
		"level 0\n"
		"level 0 nested\n"
		"> level 1\n"
		"> level 1 nested\n"
		"> > level 2\n"
		"> > level 2 nested\n"
		"> > level 2 nested^2\n"
		"> > > level 3 bq-nested\n"
		"> > > level 3 bq-nested nested\n"
		"> > level 2 back\n"
		"> > level 2 nested repeat\n"
		"> > level 2 repeat\n"
		"> level 1 back\n"
		"> level 1 repeat - nested\n"
		"> level 1 repeat\n"
		"level 0 back\n"
		"level 0 back nested\n",
		71 },
	/* 23 */{ HTML ("<blockquote type='cite'>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "123 456-78 123456 7-890123 123-456-789- 1122------------- 789 --------------------- 123</div>"
		"</blockquote>"),
		"> 123 456-78\n"
		"> 123456 7-\n"
		"> 890123\n"
		"> 123-456-\n"
		"> 789- 1122-\n"
		"> ----------\n"
		"> -- 789 ---\n"
		"> ----------\n"
		"> --------\n"
		"> 123\n",
		12 }
	};

	#undef QUOTE_SPAN
	#undef QUOTE_CHR
	#undef HTML

	gchar *script;
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (tests); ii++) {
		test_utils_load_string (fixture, tests[ii].html);

		script = e_web_view_jsc_printf_script ("EvoConvert.ToPlainText(document.body, %d);", tests[ii].normal_div_width);

		test_utils_jsc_call_string_and_verify (fixture, script, tests[ii].plain);

		g_free (script);
	}
}

static void
test_convert_to_plain_links_none (TestFixture *fixture)
{
	#define HTML(_body) ("<html><head><style><!-- span.Apple-tab-span { white-space:pre; } --></style></head><body style='font-family:monospace;'>" _body "</body></html>")

	struct _tests {
		const gchar *html;
		const gchar *plain;
		gint normal_div_width;
	} tests[] = {
	/* 0 */ { HTML ("<div>before <a href='https://gnome.org'>https://gnome.org</a> after</div>"),
		"before https://gnome.org after\n",
		71 },
	/* 1 */ { HTML ("<div>before <a href='https://gnome.org'>GNOME</a> after</div>"),
		"before GNOME after\n",
		71 },
	/* 2 */ { HTML ("<div>b1 <a href='https://gnome.org/a'>GNOME-a</a> a1</div>"
		"<div>b2 <a href='https://gnome.org/'>https://gnome.org/</a> a2</div>"
		"<div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div>"
		"<div>b4 <a href='https://gnome.org/c'>GNOME-c</a> a4</div>"),
		"b1 GNOME-a a1\n"
		"b2 https://gnome.org/ a2\n"
		"b3 GNOME-b a3\n"
		"b4 GNOME-c a4\n",
		71 },
	/* 3 */ { HTML ("<div>b1 <a href='https://gnome.org/a'>GNOME-a</a> a1</div>"
		"<div>b2 <a href='https://gnome.org'>https://gnome.org/</a> a2</div>"
		"<div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div>"
		"<div>b4 <a href='https://gnome.org/c'>GNOME-c</a> a4</div>"
		"<div>b5 <a href='https://gnome.org/d'>GNOME-d</a> a5</div>"
		"<div>b6 <a href='https://gnome.org/e'>GNOME-e</a> a6</div>"
		"<div>b7 <a href='https://gnome.org/f'>GNOME-f</a> a7</div>"
		"<div>b8 <a href='https://gnome.org/g'>GNOME-g</a> a8</div>"
		"<div>b9 <a href='https://gnome.org/h'>GNOME-h</a> a9</div>"
		"<div>b10 <a href='https://gnome.org/i'>GNOME-i</a> a10</div>"),
		"b1 GNOME-a a1\n"
		"b2 https://gnome.org/ a2\n"
		"b3 GNOME-b a3\n"
		"b4 GNOME-c a4\n"
		"b5 GNOME-d a5\n"
		"b6 GNOME-e a6\n"
		"b7 GNOME-f a7\n"
		"b8 GNOME-g a8\n"
		"b9 GNOME-h a9\n"
		"b10 GNOME-i a10\n",
		71 },
	/* 4 */ { HTML ("<div>b1 <a href='https://gnome.org/a'>GNOME-a<br>next line<br>text</a> a1</div>"
		"<div>b2 <a href='https://gnome.org/'>https://gnome.org</a> a2</div>"
		"<div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div>"
		"<div>b4 <a href='https://gnome.org/c'>GNOME-c</a> a4</div>"
		"<div>b5 <a href='https://gnome.org/d'>GNOME-d</a> a5</div>"
		"<div>b6 <a href='https://gnome.org/e'>GNOME-e</a> a6</div>"
		"<div>b7 <a href='https://gnome.org/f'>GNOME-f</a> a7</div>"
		"<div>b8 <a href='https://gnome.org/g'>GNOME-g</a> a8</div>"
		"<div>b9 <a href='https://gnome.org/h'>GNOME-h</a> a9</div>"
		"<div>b10 <a href='https://gnome.org/i'>GNOME-i</a> a10</div>"
		"<div>b11 <a href='https://gnome.org/j'>GNOME-j</a> a11</div>"),
		"b1 GNOME-a\nnext line\ntext a1\n"
		"b2 https://gnome.org a2\n"
		"b3 GNOME-b a3\n"
		"b4 GNOME-c a4\n"
		"b5 GNOME-d a5\n"
		"b6 GNOME-e a6\n"
		"b7 GNOME-f a7\n"
		"b8 GNOME-g a8\n"
		"b9 GNOME-h a9\n"
		"b10 GNOME-i a10\n"
		"b11 GNOME-j a11\n",
		71 },
	/* 5 */ { HTML ("<ul><li>first line</li>"
		"<li>b2 <a href='https://gnome.org/'>GNOME</a> a2</li>"
		"<li><div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div></li>"
		"<li>last line</li></ul>"),
		" * first line\n"
		" * b2 GNOME a2\n"
		" * b3 GNOME-b a3\n"
		" * last line\n",
		71 },
	/* 6 */ { HTML ("<ol><li>first line</li>"
		"<li>b2 <a href='https://gnome.org/'>GNOME</a> a2</li>"
		"<li><div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div></li>"
		"<li>last line</li></ol>"),
		"   1. first line\n"
		"   2. b2 GNOME a2\n"
		"   3. b3 GNOME-b a3\n"
		"   4. last line\n",
		71 },
	/* 7 */ { HTML ("<div>before <a href='gnome.org'>GNOME</a> after</div>"
		"<div>before <a href='mailto:user@no.where'>Mail the User</a> after</div>"
		"<div>before <a href='https://gnome.org'>https GNOME</a> after</div>"
		"<div>before <a href='http://gnome.org'>http GNOME</a> after</div>"),
		"before GNOME after\n"
		"before Mail the User after\n"
		"before https GNOME after\n"
		"before http GNOME after\n",
		71 },
	/* 8 */ { HTML ("<div>before <a href='https://gnome.org/#%C3%A4bc'>https://gnome.org/#äbc</a> after</div>"),
		"before https://gnome.org/#äbc after\n",
		71 }
	};

	#undef HTML

	gchar *script;
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (tests); ii++) {
		test_utils_load_string (fixture, tests[ii].html);

		script = e_web_view_jsc_printf_script ("EvoConvert.ToPlainText(document.body, %d, %d);", tests[ii].normal_div_width, E_HTML_LINK_TO_TEXT_NONE);

		test_utils_jsc_call_string_and_verify (fixture, script, tests[ii].plain);

		g_free (script);
	}
}

static void
test_convert_to_plain_links_inline (TestFixture *fixture)
{
	#define HTML(_body) ("<html><head><style><!-- span.Apple-tab-span { white-space:pre; } --></style></head><body style='font-family:monospace;'>" _body "</body></html>")

	struct _tests {
		const gchar *html;
		const gchar *plain;
		gint normal_div_width;
	} tests[] = {
	/* 0 */ { HTML ("<div>before <a href='https://gnome.org'>https://gnome.org</a> after</div>"),
		"before https://gnome.org after\n",
		71 },
	/* 1 */ { HTML ("<div>before <a href='https://gnome.org'>GNOME</a> after</div>"),
		"before GNOME <https://gnome.org/> after\n",
		71 },
	/* 2 */ { HTML ("<div>b1 <a href='https://gnome.org/a'>GNOME-a</a> a1</div>"
		"<div>b2 <a href='https://gnome.org/'>https://gnome.org/</a> a2</div>"
		"<div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div>"
		"<div>b4 <a href='https://gnome.org/c'>GNOME-c</a> a4</div>"),
		"b1 GNOME-a <https://gnome.org/a> a1\n"
		"b2 https://gnome.org/ a2\n"
		"b3 GNOME-b <https://gnome.org/b> a3\n"
		"b4 GNOME-c <https://gnome.org/c> a4\n",
		71 },
	/* 3 */ { HTML ("<div>b1 <a href='https://gnome.org/a'>GNOME-a</a> a1</div>"
		"<div>b2 <a href='https://gnome.org'>https://gnome.org/</a> a2</div>"
		"<div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div>"
		"<div>b4 <a href='https://gnome.org/c'>GNOME-c</a> a4</div>"
		"<div>b5 <a href='https://gnome.org/d'>GNOME-d</a> a5</div>"
		"<div>b6 <a href='https://gnome.org/e'>GNOME-e</a> a6</div>"
		"<div>b7 <a href='https://gnome.org/f'>GNOME-f</a> a7</div>"
		"<div>b8 <a href='https://gnome.org/g'>GNOME-g</a> a8</div>"
		"<div>b9 <a href='https://gnome.org/h'>GNOME-h</a> a9</div>"
		"<div>b10 <a href='https://gnome.org/i'>GNOME-i</a> a10</div>"),
		"b1 GNOME-a <https://gnome.org/a> a1\n"
		"b2 https://gnome.org/ a2\n"
		"b3 GNOME-b <https://gnome.org/b> a3\n"
		"b4 GNOME-c <https://gnome.org/c> a4\n"
		"b5 GNOME-d <https://gnome.org/d> a5\n"
		"b6 GNOME-e <https://gnome.org/e> a6\n"
		"b7 GNOME-f <https://gnome.org/f> a7\n"
		"b8 GNOME-g <https://gnome.org/g> a8\n"
		"b9 GNOME-h <https://gnome.org/h> a9\n"
		"b10 GNOME-i <https://gnome.org/i> a10\n",
		71 },
	/* 4 */ { HTML ("<div>b1 <a href='https://gnome.org/a'>GNOME-a<br>next line<br>text</a> a1</div>"
		"<div>b2 <a href='https://gnome.org/'>https://gnome.org</a> a2</div>"
		"<div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div>"
		"<div>b4 <a href='https://gnome.org/c'>GNOME-c</a> a4</div>"
		"<div>b5 <a href='https://gnome.org/d'>GNOME-d</a> a5</div>"
		"<div>b6 <a href='https://gnome.org/e'>GNOME-e</a> a6</div>"
		"<div>b7 <a href='https://gnome.org/f'>GNOME-f</a> a7</div>"
		"<div>b8 <a href='https://gnome.org/g'>GNOME-g</a> a8</div>"
		"<div>b9 <a href='https://gnome.org/h'>GNOME-h</a> a9</div>"
		"<div>b10 <a href='https://gnome.org/i'>GNOME-i</a> a10</div>"
		"<div>b11 <a href='https://gnome.org/j'>GNOME-j</a> a11</div>"),
		"b1 GNOME-a\nnext line\ntext <https://gnome.org/a> a1\n"
		"b2 https://gnome.org a2\n"
		"b3 GNOME-b <https://gnome.org/b> a3\n"
		"b4 GNOME-c <https://gnome.org/c> a4\n"
		"b5 GNOME-d <https://gnome.org/d> a5\n"
		"b6 GNOME-e <https://gnome.org/e> a6\n"
		"b7 GNOME-f <https://gnome.org/f> a7\n"
		"b8 GNOME-g <https://gnome.org/g> a8\n"
		"b9 GNOME-h <https://gnome.org/h> a9\n"
		"b10 GNOME-i <https://gnome.org/i> a10\n"
		"b11 GNOME-j <https://gnome.org/j> a11\n",
		71 },
	/* 5 */ { HTML ("<ul><li>first line</li>"
		"<li>b2 <a href='https://gnome.org/'>GNOME</a> a2</li>"
		"<li><div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div></li>"
		"<li>last line</li></ul>"),
		" * first line\n"
		" * b2 GNOME <https://gnome.org/> a2\n"
		" * b3 GNOME-b <https://gnome.org/b> a3\n"
		" * last line\n",
		71 },
	/* 6 */ { HTML ("<ol><li>first line</li>"
		"<li>b2 <a href='https://gnome.org/'>GNOME</a> a2</li>"
		"<li><div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div></li>"
		"<li>last line</li></ol>"),
		"   1. first line\n"
		"   2. b2 GNOME <https://gnome.org/> a2\n"
		"   3. b3 GNOME-b <https://gnome.org/b> a3\n"
		"   4. last line\n",
		71 },
	/* 7 */ { HTML ("<div>before <a href='http://gnome.org'>GNOME</a> after</div>"
		"<div>before <a href='mailto:user@no.where'>Mail the User</a> after</div>"
		"<div>before <a href='https://gnome.org'>https GNOME</a> after</div>"
		"<div>before <a href='http://gnome.org/'>http GNOME</a> after</div>"),
		"before GNOME <http://gnome.org/> after\n"
		"before Mail the User after\n"
		"before https GNOME <https://gnome.org/> after\n"
		"before http GNOME <http://gnome.org/> after\n",
		71 },
	/* 8 */ { HTML ("<div>before <a href='https://gnome.org/#%C3%A4bc'>https://gnome.org/#äbc</a> after</div>"),
		"before https://gnome.org/#äbc after\n",
		71 }
	};

	#undef HTML

	gchar *script;
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (tests); ii++) {
		test_utils_load_string (fixture, tests[ii].html);

		script = e_web_view_jsc_printf_script ("EvoConvert.ToPlainText(document.body, %d, %d);", tests[ii].normal_div_width, E_HTML_LINK_TO_TEXT_INLINE);

		test_utils_jsc_call_string_and_verify (fixture, script, tests[ii].plain);

		g_free (script);
	}
}

static void
test_convert_to_plain_links_reference (TestFixture *fixture)
{
	#define HTML(_body) ("<html><head><style><!-- span.Apple-tab-span { white-space:pre; } --></style></head><body style='font-family:monospace;'>" _body "</body></html>")

	struct _tests {
		const gchar *html;
		const gchar *plain;
		gint normal_div_width;
	} tests[] = {
	/* 0 */ { HTML ("<div>before <a href='https://gnome.org'>https://gnome.org</a> after</div>"),
		"before https://gnome.org after\n",
		40 },
	/* 1 */ { HTML ("<div>before <a href='https://gnome.org'>GNOME</a> after</div>"),
		"before GNOME [1] after\n"
		"\n"
		"[1] GNOME https://gnome.org/\n",
		40 },
	/* 2 */ { HTML ("<div>b1 <a href='https://gnome.org/a'>GNOME-a</a> a1</div>"
		"<div>b2 <a href='https://gnome.org/'>https://gnome.org/</a> a2</div>"
		"<div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div>"
		"<div>b4 <a href='https://gnome.org/c'>GNOME-c</a> a4</div>"),
		"b1 GNOME-a [1] a1\n"
		"b2 https://gnome.org/ a2\n"
		"b3 GNOME-b [2] a3\n"
		"b4 GNOME-c [3] a4\n"
		"\n"
		"[1] GNOME-a https://gnome.org/a\n"
		"[2] GNOME-b https://gnome.org/b\n"
		"[3] GNOME-c https://gnome.org/c\n",
		40 },
	/* 3 */ { HTML ("<div>b1 <a href='https://gnome.org/a'>GNOME-a</a> a1</div>"
		"<div>b2 <a href='https://gnome.org'>https://gnome.org/</a> a2</div>"
		"<div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div>"
		"<div>b4 <a href='https://gnome.org/c'>GNOME-c</a> a4</div>"
		"<div>b5 <a href='https://gnome.org/d'>GNOME-d</a> a5</div>"
		"<div>b6 <a href='https://gnome.org/e'>GNOME-e</a> a6</div>"
		"<div>b7 <a href='https://gnome.org/f'>GNOME-f</a> a7</div>"
		"<div>b8 <a href='https://gnome.org/g'>GNOME-g</a> a8</div>"
		"<div>b9 <a href='https://gnome.org/h'>GNOME-h</a> a9</div>"
		"<div>b10 <a href='https://gnome.org/i'>GNOME-i</a> a10</div>"),
		"b1 GNOME-a [1] a1\n"
		"b2 https://gnome.org/ a2\n"
		"b3 GNOME-b [2] a3\n"
		"b4 GNOME-c [3] a4\n"
		"b5 GNOME-d [4] a5\n"
		"b6 GNOME-e [5] a6\n"
		"b7 GNOME-f [6] a7\n"
		"b8 GNOME-g [7] a8\n"
		"b9 GNOME-h [8] a9\n"
		"b10 GNOME-i [9] a10\n"
		"\n"
		"[1] GNOME-a https://gnome.org/a\n"
		"[2] GNOME-b https://gnome.org/b\n"
		"[3] GNOME-c https://gnome.org/c\n"
		"[4] GNOME-d https://gnome.org/d\n"
		"[5] GNOME-e https://gnome.org/e\n"
		"[6] GNOME-f https://gnome.org/f\n"
		"[7] GNOME-g https://gnome.org/g\n"
		"[8] GNOME-h https://gnome.org/h\n"
		"[9] GNOME-i https://gnome.org/i\n",
		40 },
	/* 4 */ { HTML ("<div>b1 <a href='https://gnome.org/a'>GNOME-a<br>next line<br>text</a> a1</div>"
		"<div>b2 <a href='https://gnome.org/'>https://gnome.org</a> a2</div>"
		"<div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div>"
		"<div>b4 <a href='https://gnome.org/c'>GNOME-c</a> a4</div>"
		"<div>b5 <a href='https://gnome.org/d'>GNOME-d</a> a5</div>"
		"<div>b6 <a href='https://gnome.org/e'>GNOME-e</a> a6</div>"
		"<div>b7 <a href='https://gnome.org/f'>GNOME-f</a> a7</div>"
		"<div>b8 <a href='https://gnome.org/g'>GNOME-g</a> a8</div>"
		"<div>b9 <a href='https://gnome.org/h'>GNOME-h</a> a9</div>"
		"<div>b10 <a href='https://gnome.org/i'>GNOME-i</a> a10</div>"
		"<div>b11 <a href='https://gnome.org/j'>GNOME-j long text here</a> a11</div>"),
		"b1 GNOME-a\nnext line\ntext [1] a1\n"
		"b2 https://gnome.org a2\n"
		"b3 GNOME-b [2] a3\n"
		"b4 GNOME-c [3] a4\n"
		"b5 GNOME-d [4] a5\n"
		"b6 GNOME-e [5] a6\n"
		"b7 GNOME-f [6] a7\n"
		"b8 GNOME-g [7] a8\n"
		"b9 GNOME-h [8] a9\n"
		"b10 GNOME-i [9] a10\n"
		"b11 GNOME-j long text here [10] a11\n"
		"\n"
		"[1] GNOME-a next line text\n"
		"    https://gnome.org/a\n"
		"[2] GNOME-b https://gnome.org/b\n"
		"[3] GNOME-c https://gnome.org/c\n"
		"[4] GNOME-d https://gnome.org/d\n"
		"[5] GNOME-e https://gnome.org/e\n"
		"[6] GNOME-f https://gnome.org/f\n"
		"[7] GNOME-g https://gnome.org/g\n"
		"[8] GNOME-h https://gnome.org/h\n"
		"[9] GNOME-i https://gnome.org/i\n"
		"[10] GNOME-j long text here\n"
		"     https://gnome.org/j\n",
		40 },
	/* 5 */ { HTML ("<ul><li>first line</li>"
		"<li>b2 <a href='https://gnome.org/'>GNOME</a> a2</li>"
		"<li><div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div></li>"
		"<li>last line</li></ul>"),
		" * first line\n"
		" * b2 GNOME [1] a2\n"
		" * b3 GNOME-b [2] a3\n"
		" * last line\n"
		"\n"
		"[1] GNOME https://gnome.org/\n"
		"[2] GNOME-b https://gnome.org/b\n",
		40 },
	/* 6 */ { HTML ("<ol><li>first line</li>"
		"<li>b2 <a href='https://gnome.org/'>GNOME</a> a2</li>"
		"<li><div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div></li>"
		"<li>last line</li></ol>"),
		"   1. first line\n"
		"   2. b2 GNOME [1] a2\n"
		"   3. b3 GNOME-b [2] a3\n"
		"   4. last line\n"
		"\n"
		"[1] GNOME https://gnome.org/\n"
		"[2] GNOME-b https://gnome.org/b\n",
		40 },
	/* 7 */ { HTML ("<div>before <a href='http://gnome.org'>GNOME</a> after</div>"
		"<div>before <a href='mailto:user@no.where'>Mail the User</a> after</div>"
		"<div>before <a href='https://gnome.org'>https GNOME</a> after</div>"
		"<div>before <a href='http://gnome.org/'>http GNOME</a> after</div>"),
		"before GNOME [1] after\n"
		"before Mail the User after\n"
		"before https GNOME [2] after\n"
		"before http GNOME [1] after\n"
		"\n"
		"[1] GNOME http://gnome.org/\n"
		"[2] https GNOME https://gnome.org/\n",
		40 },
	/* 8 */ { HTML ("<div>before <a href='https://gnome.org/#%C3%A4bc'>https://gnome.org/#äbc</a> after</div>"),
		"before https://gnome.org/#äbc after\n",
		40 }
	};

	#undef HTML

	gchar *script;
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (tests); ii++) {
		test_utils_load_string (fixture, tests[ii].html);

		script = e_web_view_jsc_printf_script ("EvoConvert.ToPlainText(document.body, %d, %d);", tests[ii].normal_div_width, E_HTML_LINK_TO_TEXT_REFERENCE);

		test_utils_jsc_call_string_and_verify (fixture, script, tests[ii].plain);

		g_free (script);
	}
}

static void
test_convert_to_plain_links_reference_without_label (TestFixture *fixture)
{
	#define HTML(_body) ("<html><head><style><!-- span.Apple-tab-span { white-space:pre; } --></style></head><body style='font-family:monospace;'>" _body "</body></html>")

	#define HTML(_body) ("<html><head><style><!-- span.Apple-tab-span { white-space:pre; } --></style></head><body style='font-family:monospace;'>" _body "</body></html>")

	struct _tests {
		const gchar *html;
		const gchar *plain;
		gint normal_div_width;
	} tests[] = {
	/* 0 */ { HTML ("<div>before <a href='https://gnome.org'>https://gnome.org</a> after</div>"),
		"before https://gnome.org after\n",
		40 },
	/* 1 */ { HTML ("<div>before <a href='https://gnome.org'>GNOME</a> after</div>"),
		"before GNOME [1] after\n"
		"\n"
		"[1] https://gnome.org/\n",
		40 },
	/* 2 */ { HTML ("<div>b1 <a href='https://gnome.org/a'>GNOME-a</a> a1</div>"
		"<div>b2 <a href='https://gnome.org/'>https://gnome.org/</a> a2</div>"
		"<div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div>"
		"<div>b4 <a href='https://gnome.org/c'>GNOME-c</a> a4</div>"),
		"b1 GNOME-a [1] a1\n"
		"b2 https://gnome.org/ a2\n"
		"b3 GNOME-b [2] a3\n"
		"b4 GNOME-c [3] a4\n"
		"\n"
		"[1] https://gnome.org/a\n"
		"[2] https://gnome.org/b\n"
		"[3] https://gnome.org/c\n",
		40 },
	/* 3 */ { HTML ("<div>b1 <a href='https://gnome.org/a'>GNOME-a</a> a1</div>"
		"<div>b2 <a href='https://gnome.org'>https://gnome.org/</a> a2</div>"
		"<div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div>"
		"<div>b4 <a href='https://gnome.org/c'>GNOME-c</a> a4</div>"
		"<div>b5 <a href='https://gnome.org/d'>GNOME-d</a> a5</div>"
		"<div>b6 <a href='https://gnome.org/e'>GNOME-e</a> a6</div>"
		"<div>b7 <a href='https://gnome.org/f'>GNOME-f</a> a7</div>"
		"<div>b8 <a href='https://gnome.org/g'>GNOME-g</a> a8</div>"
		"<div>b9 <a href='https://gnome.org/h'>GNOME-h</a> a9</div>"
		"<div>b10 <a href='https://gnome.org/i'>GNOME-i</a> a10</div>"),
		"b1 GNOME-a [1] a1\n"
		"b2 https://gnome.org/ a2\n"
		"b3 GNOME-b [2] a3\n"
		"b4 GNOME-c [3] a4\n"
		"b5 GNOME-d [4] a5\n"
		"b6 GNOME-e [5] a6\n"
		"b7 GNOME-f [6] a7\n"
		"b8 GNOME-g [7] a8\n"
		"b9 GNOME-h [8] a9\n"
		"b10 GNOME-i [9] a10\n"
		"\n"
		"[1] https://gnome.org/a\n"
		"[2] https://gnome.org/b\n"
		"[3] https://gnome.org/c\n"
		"[4] https://gnome.org/d\n"
		"[5] https://gnome.org/e\n"
		"[6] https://gnome.org/f\n"
		"[7] https://gnome.org/g\n"
		"[8] https://gnome.org/h\n"
		"[9] https://gnome.org/i\n",
		40 },
	/* 4 */ { HTML ("<div>b1 <a href='https://gnome.org/a'>GNOME-a<br>next line<br>text</a> a1</div>"
		"<div>b2 <a href='https://gnome.org/'>https://gnome.org</a> a2</div>"
		"<div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div>"
		"<div>b4 <a href='https://gnome.org/c'>GNOME-c</a> a4</div>"
		"<div>b5 <a href='https://gnome.org/d'>GNOME-d</a> a5</div>"
		"<div>b6 <a href='https://gnome.org/e'>GNOME-e</a> a6</div>"
		"<div>b7 <a href='https://gnome.org/f'>GNOME-f</a> a7</div>"
		"<div>b8 <a href='https://gnome.org/g'>GNOME-g</a> a8</div>"
		"<div>b9 <a href='https://gnome.org/h'>GNOME-h</a> a9</div>"
		"<div>b10 <a href='https://gnome.org/i'>GNOME-i</a> a10</div>"
		"<div>b11 <a href='https://gnome.org/j'>GNOME-j long text here</a> a11</div>"),
		"b1 GNOME-a\nnext line\ntext [1] a1\n"
		"b2 https://gnome.org a2\n"
		"b3 GNOME-b [2] a3\n"
		"b4 GNOME-c [3] a4\n"
		"b5 GNOME-d [4] a5\n"
		"b6 GNOME-e [5] a6\n"
		"b7 GNOME-f [6] a7\n"
		"b8 GNOME-g [7] a8\n"
		"b9 GNOME-h [8] a9\n"
		"b10 GNOME-i [9] a10\n"
		"b11 GNOME-j long text here [10] a11\n"
		"\n"
		"[1] https://gnome.org/a\n"
		"[2] https://gnome.org/b\n"
		"[3] https://gnome.org/c\n"
		"[4] https://gnome.org/d\n"
		"[5] https://gnome.org/e\n"
		"[6] https://gnome.org/f\n"
		"[7] https://gnome.org/g\n"
		"[8] https://gnome.org/h\n"
		"[9] https://gnome.org/i\n"
		"[10] https://gnome.org/j\n",
		40 },
	/* 5 */ { HTML ("<ul><li>first line</li>"
		"<li>b2 <a href='https://gnome.org/'>GNOME</a> a2</li>"
		"<li><div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div></li>"
		"<li>last line</li></ul>"),
		" * first line\n"
		" * b2 GNOME [1] a2\n"
		" * b3 GNOME-b [2] a3\n"
		" * last line\n"
		"\n"
		"[1] https://gnome.org/\n"
		"[2] https://gnome.org/b\n",
		40 },
	/* 6 */ { HTML ("<ol><li>first line</li>"
		"<li>b2 <a href='https://gnome.org/'>GNOME</a> a2</li>"
		"<li><div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div></li>"
		"<li>last line</li></ol>"),
		"   1. first line\n"
		"   2. b2 GNOME [1] a2\n"
		"   3. b3 GNOME-b [2] a3\n"
		"   4. last line\n"
		"\n"
		"[1] https://gnome.org/\n"
		"[2] https://gnome.org/b\n",
		40 },
	/* 7 */ { HTML ("<div>before <a href='http://gnome.org'>GNOME</a> after</div>"
		"<div>before <a href='mailto:user@no.where'>Mail the User</a> after</div>"
		"<div>before <a href='https://gnome.org'>https GNOME</a> after</div>"
		"<div>before <a href='http://gnome.org/'>http GNOME</a> after</div>"),
		"before GNOME [1] after\n"
		"before Mail the User after\n"
		"before https GNOME [2] after\n"
		"before http GNOME [1] after\n"
		"\n"
		"[1] http://gnome.org/\n"
		"[2] https://gnome.org/\n",
		40 },
	/* 8 */ { HTML ("<div>before <a href='https://gnome.org/#%C3%A4bc'>https://gnome.org/#äbc</a> after</div>"),
		"before https://gnome.org/#äbc after\n",
		40 }
	};

	#undef HTML

	gchar *script;
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (tests); ii++) {
		test_utils_load_string (fixture, tests[ii].html);

		script = e_web_view_jsc_printf_script ("EvoConvert.ToPlainText(document.body, %d, %d);", tests[ii].normal_div_width, E_HTML_LINK_TO_TEXT_REFERENCE_WITHOUT_LABEL);

		test_utils_jsc_call_string_and_verify (fixture, script, tests[ii].plain);

		g_free (script);
	}
}

gint
main (gint argc,
      gchar *argv[])
{
	GApplication *application; /* Needed for WebKitGTK sandboxing */
	gint res;

	setlocale (LC_ALL, "");

	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("https://gitlab.gnome.org/GNOME/evolution/issues/");

	g_setenv ("E_WEB_VIEW_TEST_SOURCES", "1", FALSE);
	g_setenv ("EVOLUTION_SOURCE_WEBKITDATADIR", EVOLUTION_SOURCE_WEBKITDATADIR, FALSE);

	gtk_init (&argc, &argv);

	e_util_init_main_thread (NULL);
	e_passwords_init ();

	application = g_application_new ("org.gnome.Evolution.test-web-view-jsc", G_APPLICATION_FLAGS_NONE);

	test_utils_add_test ("/EWebView/JSCObjectProperties", test_jsc_object_properties);
	test_utils_add_test ("/EWebView/SetElementHidden", test_set_element_hidden);
	test_utils_add_test ("/EWebView/SetElementDisabled", test_set_element_disabled);
	test_utils_add_test ("/EWebView/SetElementChecked", test_set_element_checked);
	test_utils_add_test ("/EWebView/SetElementStyleProperty", test_set_element_style_property);
	test_utils_add_test ("/EWebView/SetElementAttribute", test_set_element_attribute);
	test_utils_add_test ("/EWebView/StyleSheets", test_style_sheets);
	test_utils_add_test ("/EWebView/ElementClicked", test_element_clicked);
	test_utils_add_test ("/EWebView/NeedInputChanged", test_need_input_changed);
	test_utils_add_test ("/EWebView/Selection", test_selection);
	test_utils_add_test ("/EWebView/GetContent", test_get_content);
	test_utils_add_test ("/EWebView/GetElementFromPoint", test_get_element_from_point);
	test_utils_add_test ("/EWebView/ConvertToPlain", test_convert_to_plain);
	test_utils_add_test ("/EWebView/ConvertToPlainQuoted", test_convert_to_plain_quoted);
	test_utils_add_test ("/EWebView/ConvertToPlainLinksNone", test_convert_to_plain_links_none);
	test_utils_add_test ("/EWebView/ConvertToPlainLinksInline", test_convert_to_plain_links_inline);
	test_utils_add_test ("/EWebView/ConvertToPlainLinksReference", test_convert_to_plain_links_reference);
	test_utils_add_test ("/EWebView/ConvertToPlainLinksReferenceWithoutLabel", test_convert_to_plain_links_reference_without_label);

	res = g_test_run ();

	g_clear_object (&application);
	e_misc_util_free_global_memory ();

	return res;
}
