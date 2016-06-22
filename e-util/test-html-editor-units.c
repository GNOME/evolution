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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <locale.h>
#include <e-util/e-util.h>

#include "e-html-editor-private.h"
#include "test-html-editor-units-utils.h"

#define HTML_PREFIX "<html><head></head><body><p data-evo-paragraph=\"\">"
#define HTML_SUFFIX "</p></body></html>"

/* The tests do not use the 'user_data' argument, thus the functions avoid them and the typecast is needed. */
typedef void (* ETestFixtureFunc) (TestFixture *fixture, gconstpointer user_data);

static void
test_create_editor (TestFixture *fixture)
{
	g_assert (fixture->editor != NULL);
	g_assert_cmpstr (e_html_editor_get_content_editor_name (fixture->editor), ==, DEFAULT_CONTENT_EDITOR_NAME);

	/* test of the test function */
	g_assert (test_utils_html_equal (fixture, "<span>a</span>", "<sPaN>a</spaN>"));
	g_assert (!test_utils_html_equal (fixture, "<span>A</span>", "<sPaN>a</spaN>"));
}

static void
test_style_bold_selection (TestFixture *fixture)
{
	if (!test_utils_run_simple_test	(fixture,
		"mode:html\n"
		"type:some bold text\n"
		"seq:hCrcrCSrsc\n"
		"action:bold\n",
		HTML_PREFIX "some <b>bold</b> text" HTML_SUFFIX,
		"some bold text"))
		g_test_fail ();
}

static void
test_style_bold_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test	(fixture,
		"mode:html\n"
		"type:some \n"
		"action:bold\n"
		"type:bold\n"
		"action:bold\n"
		"type: text\n",
		HTML_PREFIX "some <b>bold</b> text" HTML_SUFFIX,
		"some bold text"))
		g_test_fail ();
}

static void
test_style_italic_selection (TestFixture *fixture)
{
	if (!test_utils_run_simple_test	(fixture,
		"mode:html\n"
		"type:some italic text\n"
		"seq:hCrcrCSrsc\n"
		"action:italic\n",
		HTML_PREFIX "some <i>italic</i> text" HTML_SUFFIX,
		"some italic text"))
		g_test_fail ();
}

static void
test_style_italic_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test	(fixture,
		"mode:html\n"
		"type:some \n"
		"action:italic\n"
		"type:italic\n"
		"action:italic\n"
		"type: text\n",
		HTML_PREFIX "some <i>italic</i> text" HTML_SUFFIX,
		"some italic text"))
		g_test_fail ();
}

static void
test_style_underline_selection (TestFixture *fixture)
{
	if (!test_utils_run_simple_test	(fixture,
		"mode:html\n"
		"type:some underline text\n"
		"seq:hCrcrCSrsc\n"
		"action:underline\n",
		HTML_PREFIX "some <u>underline</u> text" HTML_SUFFIX,
		"some underline text"))
		g_test_fail ();
}

static void
test_style_underline_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test	(fixture,
		"mode:html\n"
		"type:some \n"
		"action:underline\n"
		"type:underline\n"
		"action:underline\n"
		"type: text\n",
		HTML_PREFIX "some <u>underline</u> text" HTML_SUFFIX,
		"some underline text"))
		g_test_fail ();
}

static void
test_style_monospace_selection (TestFixture *fixture)
{
	if (!test_utils_run_simple_test	(fixture,
		"mode:html\n"
		"type:some monospace text\n"
		"seq:hCrcrCSrsc\n"
		"action:monospaced\n",
		HTML_PREFIX "some <font face=\"monospace\" size=\"3\">monospace</font> text" HTML_SUFFIX,
		"some monospace text"))
		g_test_fail ();
}

static void
test_style_monospace_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test	(fixture,
		"mode:html\n"
		"type:some \n"
		"action:monospaced\n"
		"type:monospace\n"
		"action:monospaced\n"
		"type: text\n",
		HTML_PREFIX "some <font face=\"monospace\" size=\"3\">monospace</font> text" HTML_SUFFIX,
		"some monospace text"))
		g_test_fail ();
}

static void
test_undo_text_type (TestFixture *fixture)
{
	if (!test_utils_run_simple_test	(fixture,
		"mode:html\n"
		"undo:save\n"	/* 1 */
		"type:some text corretC\n"
		"undo:save\n"	/* 2 */
		"seq:CSlcsD\n"	/* delete the last word */
		"undo:save\n"	/* 3 */
		"type:broken\n" /* and write 'broken' there */
		"undo:save\n"	/* 4 */
		"undo:undo:2\n"	/* undo the 'broken' word write */
		"undo:test:2\n"
		"undo:redo\n" /* redo 'broken' word write */
		"undo:test\n"
		"undo:undo\n" /* undo 'broken' word write */
		"undo:test:2\n"
		"undo:redo\n" /* redo 'broken' word write */
		"undo:test\n"
		"undo:drop:2\n"	/* 2 */
		"undo:undo:2\n" /* undo 'broken' word write and word delete*/
		"undo:test\n"
		"undo:undo:3\n" /* undo text typing */
		"undo:test\n"
		"undo:drop\n",	/* 1 */
		HTML_PREFIX "" HTML_SUFFIX,
		""))
		g_test_fail ();
}

gint
main (gint argc,
      gchar *argv[])
{
	gint cmd_delay = -1;
	GOptionEntry entries[] = {
		{ "cmd-delay", '\0', 0,
		  G_OPTION_ARG_INT, &cmd_delay,
		  "Specify delay, in milliseconds, to use during processing commands. Default is 5 ms.",
		  NULL },
		{ NULL }
	};
	GOptionContext *context;
	GError *error = NULL;
	GList *modules;
	gint res;

	setlocale (LC_ALL, "");

	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=");

	gtk_init (&argc, &argv);

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_warning ("Failed to parse arguments: %s\n", error ? error->message : "Unknown error");
		g_option_context_free (context);
		g_clear_error (&error);
		return -1;
	}

	g_option_context_free (context);

	if (cmd_delay > 0)
		test_utils_set_event_processing_delay_ms ((guint) cmd_delay);

	e_util_init_main_thread (NULL);
	e_passwords_init ();

	modules = e_module_load_all_in_directory (EVOLUTION_MODULEDIR);
	g_list_free_full (modules, (GDestroyNotify) g_type_module_unuse);

	#define add_test(_name, _func)	\
		g_test_add (_name, TestFixture, NULL, \
			test_utils_fixture_set_up, (ETestFixtureFunc) _func, test_utils_fixture_tear_down)

	add_test ("/create/editor", test_create_editor);
	add_test ("/style/bold-selection", test_style_bold_selection);
	add_test ("/style/bold-typed", test_style_bold_typed);
	add_test ("/style/italic-selection", test_style_italic_selection);
	add_test ("/style/italic-typed", test_style_italic_typed);
	add_test ("/style/underline-selection", test_style_underline_selection);
	add_test ("/style/underline-typed", test_style_underline_typed);
	add_test ("/style/monospace-selection", test_style_monospace_selection);
	add_test ("/style/monospace-typed", test_style_monospace_typed);
	add_test ("/undo/text-type", test_undo_text_type);

	#undef add_test

	res = g_test_run ();

	e_util_cleanup_settings ();
	e_spell_checker_free_global_memory ();

	return res;
}
