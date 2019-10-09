/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
	WebKitJavascriptResult *js_result;
	GError *error = NULL;

	g_return_if_fail (script != NULL);

	js_result = webkit_web_view_run_javascript_finish (WEBKIT_WEB_VIEW (source_object), result, &error);

	if (error) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
		    (!g_error_matches (error, WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED) ||
		     /* WebKit can return empty error message, thus ignore those. */
		     (error->message && *(error->message))))
			g_warning ("Failed to call '%s' function: %s", script, error->message);
		g_clear_error (&error);
	}

	if (js_result) {
		JSCException *exception;
		JSCValue *value;

		value = webkit_javascript_result_get_js_value (js_result);
		exception = jsc_context_get_exception (jsc_value_get_context (value));

		if (exception)
			g_warning ("Failed to call '%s': %s", script, jsc_exception_get_message (exception));

		webkit_javascript_result_unref (js_result);
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
	WebKitJavascriptResult *js_result;
	GError *error = NULL;

	g_return_if_fail (jcd != NULL);

	js_result = webkit_web_view_run_javascript_finish (WEBKIT_WEB_VIEW (source_object), result, &error);

	if (error) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
		    (!g_error_matches (error, WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED) ||
		     /* WebKit can return empty error message, thus ignore those. */
		     (error->message && *(error->message))))
			g_warning ("Failed to call '%s': %s", jcd->script, error->message);
		g_clear_error (&error);
	}

	if (js_result) {
		JSCException *exception;
		JSCValue *value;

		value = webkit_javascript_result_get_js_value (js_result);
		exception = jsc_context_get_exception (jsc_value_get_context (value));

		if (exception)
			g_warning ("Failed to call '%s': %s", jcd->script, jsc_exception_get_message (exception));
		else if (jcd->out_result)
			*(jcd->out_result) = value ? g_object_ref (value) : NULL;

		webkit_javascript_result_unref (js_result);
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

	webkit_web_view_run_javascript (fixture->web_view, script, NULL, test_utils_jsc_call_done_cb, g_strdup (script));
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

	webkit_web_view_run_javascript (fixture->web_view, script, NULL, test_utils_jsc_call_sync_done_cb, &jcd);

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
	g_assert (jsc_value_is_boolean (result));

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
	g_assert (jsc_value_is_number (result));

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
	g_assert (jsc_value_is_null (result) || jsc_value_is_string (result));

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
		"	var arrobj = [];\n"
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
	g_assert (jsc_value_is_object (jsc_object));

	g_assert (e_web_view_jsc_get_object_property_boolean (jsc_object, "btrue", FALSE));
	g_assert (!e_web_view_jsc_get_object_property_boolean (jsc_object, "bfalse", TRUE));
	g_assert (!e_web_view_jsc_get_object_property_boolean (jsc_object, "budenfined", FALSE));
	g_assert (e_web_view_jsc_get_object_property_boolean (jsc_object, "budenfined", TRUE));
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

	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn3\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn2\").hidden"));

	e_web_view_jsc_set_element_hidden (fixture->web_view, "", "btn1", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn3\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn2\").hidden"));

	e_web_view_jsc_set_element_hidden (fixture->web_view, "frm1_1", "btn1", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn3\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1\", \"btn1\").hidden"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn2\").hidden"));

	e_web_view_jsc_set_element_hidden (fixture->web_view, "frm2", "btn2", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn3\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1\", \"btn1\").hidden"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn1\").hidden"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn2\").hidden"));

	e_web_view_jsc_set_element_hidden (fixture->web_view, "", "btn1", FALSE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn3\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1\", \"btn1\").hidden"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn1\").hidden"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn2\").hidden"));

	e_web_view_jsc_set_element_hidden (fixture->web_view, "frm2", "btn1", FALSE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn3\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1\", \"btn1\").hidden"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn1\").hidden"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn2\").hidden"));

	e_web_view_jsc_set_element_hidden (fixture->web_view, "frm1_1", "btn1", FALSE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn3\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").hidden"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn1\").hidden"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn2\").hidden"));
}

static void
test_set_element_disabled (TestFixture *fixture)
{
	test_utils_load_body (fixture, LOAD_ALL);

	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn3\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn2\").disabled"));

	e_web_view_jsc_set_element_disabled (fixture->web_view, "", "btn1", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn3\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn2\").disabled"));

	e_web_view_jsc_set_element_disabled (fixture->web_view, "frm1_1", "btn1", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn3\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1\", \"btn1\").disabled"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn2\").disabled"));

	e_web_view_jsc_set_element_disabled (fixture->web_view, "frm2", "btn2", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn3\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1\", \"btn1\").disabled"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn1\").disabled"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn2\").disabled"));

	e_web_view_jsc_set_element_disabled (fixture->web_view, "", "btn1", FALSE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn3\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1\", \"btn1\").disabled"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn1\").disabled"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn2\").disabled"));

	e_web_view_jsc_set_element_disabled (fixture->web_view, "frm2", "btn1", FALSE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn3\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1\", \"btn1\").disabled"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn1\").disabled"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn2\").disabled"));

	e_web_view_jsc_set_element_disabled (fixture->web_view, "frm1_1", "btn1", FALSE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"btn3\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").disabled"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn1\").disabled"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"btn2\").disabled"));
}

static void
test_set_element_checked (TestFixture *fixture)
{
	test_utils_load_body (fixture, LOAD_ALL);

	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"chk1\").checked"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"chk1\").checked"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"chk2\").checked"));

	e_web_view_jsc_set_element_checked (fixture->web_view, "", "chk1", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"chk1\").checked"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"chk1\").checked"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"chk2\").checked"));

	e_web_view_jsc_set_element_checked (fixture->web_view, "frm1_1", "chk1", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"chk1\").checked"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"chk1\").checked"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"chk2\").checked"));

	e_web_view_jsc_set_element_checked (fixture->web_view, "", "chk1", FALSE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"chk1\").checked"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"chk1\").checked"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"chk2\").checked"));

	e_web_view_jsc_set_element_checked (fixture->web_view, "frm2", "chk2", FALSE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"chk1\").checked"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"chk1\").checked"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"chk2\").checked"));

	e_web_view_jsc_set_element_checked (fixture->web_view, "", "chk1", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"chk1\").checked"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"chk1\").checked"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"chk2\").checked"));

	e_web_view_jsc_set_element_checked (fixture->web_view, "frm1_1", "chk1", FALSE, NULL);
	e_web_view_jsc_set_element_checked (fixture->web_view, "frm2", "chk2", TRUE, NULL);
	test_utils_wait_noop (fixture);

	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"\", \"chk1\").checked"));
	g_assert (!test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm1_1\", \"chk1\").checked"));
	g_assert (test_utils_jsc_call_bool_sync (fixture, "Evo.findElement(\"frm2\", \"chk2\").checked"));
}

static void
test_set_element_style_property (TestFixture *fixture)
{
	test_utils_load_body (fixture, LOAD_ALL);

	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"\", \"btn3\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm1\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm2\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm2\", \"btn2\").style.getPropertyValue(\"color\")", "");

	e_web_view_jsc_set_element_style_property (fixture->web_view, "", "btn1", "color", "blue", NULL);
	test_utils_wait_noop (fixture);

	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"\", \"btn1\").style.getPropertyValue(\"color\")", "blue");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"\", \"btn3\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm1\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm2\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm2\", \"btn2\").style.getPropertyValue(\"color\")", "");

	e_web_view_jsc_set_element_style_property (fixture->web_view, "frm2", "btn1", "color", "green", NULL);
	test_utils_wait_noop (fixture);

	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"\", \"btn1\").style.getPropertyValue(\"color\")", "blue");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"\", \"btn3\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm1\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm2\", \"btn1\").style.getPropertyValue(\"color\")", "green");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm2\", \"btn2\").style.getPropertyValue(\"color\")", "");

	e_web_view_jsc_set_element_style_property (fixture->web_view, "frm2", "btn1", "color", NULL, NULL);
	test_utils_wait_noop (fixture);

	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"\", \"btn1\").style.getPropertyValue(\"color\")", "blue");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"\", \"btn3\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm1\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm2\", \"btn1\").style.getPropertyValue(\"color\")", "");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm2\", \"btn2\").style.getPropertyValue(\"color\")", "");
}

static void
test_set_element_attribute (TestFixture *fixture)
{
	test_utils_load_body (fixture, LOAD_ALL);

	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"\", \"btn3\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm1\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm2\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm2\", \"btn2\").getAttributeNS(\"\", \"myattr\")", NULL);

	e_web_view_jsc_set_element_attribute (fixture->web_view, "", "btn1", NULL, "myattr", "val1", NULL);
	test_utils_wait_noop (fixture);

	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"\", \"btn1\").getAttributeNS(\"\", \"myattr\")", "val1");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"\", \"btn3\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm1\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm2\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm2\", \"btn2\").getAttributeNS(\"\", \"myattr\")", NULL);

	e_web_view_jsc_set_element_attribute (fixture->web_view, "frm2", "btn1", NULL, "myattr", "val2", NULL);
	test_utils_wait_noop (fixture);

	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"\", \"btn1\").getAttributeNS(\"\", \"myattr\")", "val1");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"\", \"btn3\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm1\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm2\", \"btn1\").getAttributeNS(\"\", \"myattr\")", "val2");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm2\", \"btn2\").getAttributeNS(\"\", \"myattr\")", NULL);

	e_web_view_jsc_set_element_attribute (fixture->web_view, "frm2", "btn1", NULL, "myattr", NULL, NULL);
	test_utils_wait_noop (fixture);

	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"\", \"btn1\").getAttributeNS(\"\", \"myattr\")", "val1");
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"\", \"btn3\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm1\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm1_1\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm2\", \"btn1\").getAttributeNS(\"\", \"myattr\")", NULL);
	test_utils_jsc_call_string_and_verify (fixture, "Evo.findElement(\"frm2\", \"btn2\").getAttributeNS(\"\", \"myattr\")", NULL);
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

	g_assert (!e_web_view_get_need_input (E_WEB_VIEW (fixture->web_view)));

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

	g_assert (!e_web_view_get_need_input (E_WEB_VIEW (fixture->web_view)));
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

	g_assert (WEBKIT_IS_WEB_VIEW (source_object));
	g_assert_nonnull (gcd);

	success = e_web_view_jsc_get_selection_finish (WEBKIT_WEB_VIEW (source_object), result, &texts, &error);

	g_assert_no_error (error);
	g_assert (success);

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
	test_selection_verify (fixture, NULL, "<pre>er text\nin PR</pre>");
	test_selection_verify (fixture, "er text\nin PR", "<pre>er text\nin PR</pre>");

	test_selection_select_in_iframe (fixture, "", "br1", "br2");

	g_assert_cmpint (e_web_view_has_selection (E_WEB_VIEW (fixture->web_view)) ? 1 : 0, ==, 1);
	test_selection_verify (fixture, "\norange; bolditalic", NULL);
	test_selection_verify (fixture, NULL, "<br id=\"br1\">o<font color=\"orange\">rang</font>e; <b>bold</b><i>italic</i>");
	test_selection_verify (fixture, "\norange; bolditalic", "<br id=\"br1\">o<font color=\"orange\">rang</font>e; <b>bold</b><i>italic</i>");

	test_selection_select_in_iframe (fixture, "frm1", "plain", "rgb");

	g_assert_cmpint (e_web_view_has_selection (E_WEB_VIEW (fixture->web_view)) ? 1 : 0, ==, 1);
	test_selection_verify (fixture, "unformatted text\n", NULL);
	test_selection_verify (fixture, NULL, "<div id=\"plain\">unformatted text</div><br><div id=\"rgb\"></div>");
	test_selection_verify (fixture, "unformatted text\n", "<div id=\"plain\">unformatted text</div><br><div id=\"rgb\"></div>");

	test_selection_select_in_iframe (fixture, "frm1", "rgb", "styled");

	g_assert_cmpint (e_web_view_has_selection (E_WEB_VIEW (fixture->web_view)) ? 1 : 0, ==, 1);
	test_selection_verify (fixture, "RGB", NULL);
	test_selection_verify (fixture, NULL, "<div id=\"rgb\"><font color=\"red\">R</font><font color=\"green\">G</font><font color=\"blue\">B</font></div><div id=\"styled\"></div>");
	test_selection_verify (fixture, "RGB", "<div id=\"rgb\"><font color=\"red\">R</font><font color=\"green\">G</font><font color=\"blue\">B</font></div><div id=\"styled\"></div>");

	test_selection_select_in_iframe (fixture, "frm1", "styled", "end");

	g_assert_cmpint (e_web_view_has_selection (E_WEB_VIEW (fixture->web_view)) ? 1 : 0, ==, 1);
	test_selection_verify (fixture, "bbggrr", NULL);
	test_selection_verify (fixture, NULL, "<span style=\"color:blue;\">bb</span><span style=\"color:green;\">gg</span><span style=\"color:red;\">rr</span>");
	test_selection_verify (fixture, "bbggrr", "<span style=\"color:blue;\">bb</span><span style=\"color:green;\">gg</span><span style=\"color:red;\">rr</span>");

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

	g_assert (WEBKIT_IS_WEB_VIEW (source_object));
	g_assert_nonnull (gcd);

	success = e_web_view_jsc_get_document_content_finish (WEBKIT_WEB_VIEW (source_object), result, &texts, &error);

	g_assert_no_error (error);
	g_assert (success);

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

	g_assert (WEBKIT_IS_WEB_VIEW (source_object));
	g_assert_nonnull (gcd);

	success = e_web_view_jsc_get_element_content_finish (WEBKIT_WEB_VIEW (source_object), result, &texts, &error);

	g_assert_no_error (error);
	g_assert (success);

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
		"<html style=\"\"><head><meta charset=\"utf-8\"></head><body>"
		"<div id=\"frst\">first div</div>"
		"<div id=\"scnd\">second div</div>"
		"<iframe id=\"frm1\" src=\"empty:///\"></iframe>"
		"</body></html>";
	const gchar *html_frm1 =
		"<html style=\"\"><head><meta name=\"keywords\" value=\"test\"></head><body>"
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

	expect_plain = "frm1 div";
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

	expect_plain = "frm1 div";
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

	g_assert (WEBKIT_IS_WEB_VIEW (source_object));
	g_assert_nonnull (gefp);

	success = e_web_view_jsc_get_element_from_point_finish (WEBKIT_WEB_VIEW (source_object), result, &iframe_src, &iframe_id, &element_id, &error);

	g_assert_no_error (error);
	g_assert (success);

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
		"	var elem = Evo.findElement(iframe_id, elem_id);\n"
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
		gchar *script;
		JSCValue *value;
		gint xx, yy;

		script = e_web_view_jsc_printf_script ("TestGetPosition(%s, %s);", elems[ii].iframe_id, elems[ii].elem_id);

		test_utils_jsc_call_sync (fixture, script, &value);

		g_assert_nonnull (value);
		g_assert (jsc_value_is_object (value));

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

	scroll_x = test_utils_jsc_call_int32_sync (fixture, "window.scrollX;");
	scroll_y = test_utils_jsc_call_int32_sync (fixture, "window.scrollY;");
	client_width = test_utils_jsc_call_int32_sync (fixture, "document.body.clientWidth;");
	client_height = test_utils_jsc_call_int32_sync (fixture, "document.body.clientHeight;");

	tested = 0;

	for (ii = 0; ii < G_N_ELEMENTS (elems); ii++) {
		const gchar *iframe_src;
		gchar *script;
		JSCValue *value;
		gint xx, yy;

		script = e_web_view_jsc_printf_script ("TestGetPosition(%s, %s);", elems[ii].iframe_id, elems[ii].elem_id);

		test_utils_jsc_call_sync (fixture, script, &value);

		g_assert_nonnull (value);
		g_assert (jsc_value_is_object (value));

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

gint
main (gint argc,
      gchar *argv[])
{
	gint res;

	setlocale (LC_ALL, "");

	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("https://gitlab.gnome.org/GNOME/evolution/issues/");

	gtk_init (&argc, &argv);

	e_util_init_main_thread (NULL);
	e_passwords_init ();

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

	res = g_test_run ();

	e_misc_util_free_global_memory ();

	return res;
}
