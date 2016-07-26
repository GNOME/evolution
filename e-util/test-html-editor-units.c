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

#define HTML_PREFIX "<html><head></head><body>"
#define HTML_PREFIX_PLAIN "<html><head></head><body style=\"font-family: Monospace;\" data-evo-plain-text>"
#define HTML_SUFFIX "</body></html>"

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
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some bold text\n"
		"seq:hCrcrCSrsc\n"
		"action:bold\n",
		HTML_PREFIX "<p>some <b>bold</b> text</p>" HTML_SUFFIX,
		"some bold text"))
		g_test_fail ();
}

static void
test_style_bold_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some \n"
		"action:bold\n"
		"type:bold\n"
		"action:bold\n"
		"type: text\n",
		HTML_PREFIX "<p>some <b>bold</b> text</p>" HTML_SUFFIX,
		"some bold text"))
		g_test_fail ();
}

static void
test_style_italic_selection (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some italic text\n"
		"seq:hCrcrCSrsc\n"
		"action:italic\n",
		HTML_PREFIX "<p>some <i>italic</i> text</p>" HTML_SUFFIX,
		"some italic text"))
		g_test_fail ();
}

static void
test_style_italic_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some \n"
		"action:italic\n"
		"type:italic\n"
		"action:italic\n"
		"type: text\n",
		HTML_PREFIX "<p>some <i>italic</i> text</p>" HTML_SUFFIX,
		"some italic text"))
		g_test_fail ();
}

static void
test_style_underline_selection (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some underline text\n"
		"seq:hCrcrCSrsc\n"
		"action:underline\n",
		HTML_PREFIX "<p>some <u>underline</u> text</p>" HTML_SUFFIX,
		"some underline text"))
		g_test_fail ();
}

static void
test_style_underline_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some \n"
		"action:underline\n"
		"type:underline\n"
		"action:underline\n"
		"type: text\n",
		HTML_PREFIX "<p>some <u>underline</u> text</p>" HTML_SUFFIX,
		"some underline text"))
		g_test_fail ();
}

static void
test_style_monospace_selection (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some monospace text\n"
		"seq:hCrcrCSrsc\n"
		"action:monospaced\n",
		HTML_PREFIX "<p>some <font face=\"monospace\" size=\"3\">monospace</font> text</p>" HTML_SUFFIX,
		"some monospace text"))
		g_test_fail ();
}

static void
test_style_monospace_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some \n"
		"action:monospaced\n"
		"type:monospace\n"
		"action:monospaced\n"
		"type: text\n",
		HTML_PREFIX "<p>some <font face=\"monospace\" size=\"3\">monospace</font> text</p>" HTML_SUFFIX,
		"some monospace text"))
		g_test_fail ();
}

static void
test_undo_text_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some te\n"
		"undo:save\n"	/* 1 */
		"type:tz\n"
		"undo:save\n"	/* 2 */
		"undo:undo\n"
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:undo:2\n"
		"undo:drop\n"
		"type:xt\n",
		HTML_PREFIX "<p>some text</p>" HTML_SUFFIX,
		"some text"))
		g_test_fail ();
}

static void
test_undo_text_forward_delete (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some text to delete\n"
		"seq:hCrcrCDc\n"
		"undo:undo\n"
		"undo:redo\n"
		"undo:undo\n",
		HTML_PREFIX "<p>some text to delete</p>" HTML_SUFFIX,
		"some text to delete"))
		g_test_fail ();
}

static void
test_undo_text_backward_delete (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some text to delete\n"
		"seq:hCrcrCbc\n"
		"undo:undo\n"
		"undo:redo\n"
		"undo:undo\n",
		HTML_PREFIX "<p>some text to delete</p>" HTML_SUFFIX,
		"some text to delete"))
		g_test_fail ();
}

static void
test_justify_selection (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:center\\n\n"
		"type:right\\n\n"
		"type:left\\n\n"
		"seq:uuu\n"
		"action:justify-center\n"
		"seq:d\n"
		"action:justify-right\n"
		"seq:d\n"
		"action:justify-left\n",
		HTML_PREFIX
			"<p style=\"text-align: center\">center</p>"
			"<p style=\"text-align: right\">right</p>"
			"<p>left</p><p><br></p>"
		HTML_SUFFIX,
		"                                center\n"
		"                                                                  right\n"
		"left\n"))
		g_test_fail ();
}

static void
test_justify_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"action:justify-center\n"
		"type:center\\n\n"
		"action:justify-right\n"
		"type:right\\n\n"
		"action:justify-left\n"
		"type:left\\n\n",
		HTML_PREFIX
			"<p style=\"text-align: center\">center</p>"
			"<p style=\"text-align: right\">right</p>"
			"<p>left</p><p><br></p>"
		HTML_SUFFIX,
		"                                center\n"
		"                                                                  right\n"
		"left\n"))
		g_test_fail ();
}

static void
test_indent_selection (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:level 0\\n\n"
		"type:level 1\\n\n"
		"type:level 2\\n\n"
		"type:level 1\\n\n"
		"seq:uuu\n"
		"action:indent\n"
		"seq:d\n"
		"action:indent\n"
		"action:indent\n"
		"seq:d\n"
		"action:indent\n"
		"action:indent\n" /* just to try whether the unindent will work too */
		"action:unindent\n",
		HTML_PREFIX
			"<p>level 0</p>"
			"<div style=\"margin-left: 3ch;\">"
				"<p>level 1</p>"
				"<div style=\"margin-left: 3ch;\"><p>level 2</p></div>"
				"<p>level 1</p>"
			"</div><p><br></p>"
		HTML_SUFFIX,
		"level 0\n"
		"    level 1\n"
		"        level 2\n"
		"    level 1\n"))
		g_test_fail ();
}

static void
test_indent_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:level 0\\n\n"
		"action:indent\n"
		"type:level 1\\n\n"
		"action:indent\n"
		"type:level 2\\n\n"
		"action:unindent\n"
		"type:level 1\\n\n"
		"action:unindent\n",
		HTML_PREFIX
			"<p>level 0</p>"
			"<div style=\"margin-left: 3ch;\">"
				"<p>level 1</p>"
				"<div style=\"margin-left: 3ch;\"><p>level 2</p></div>"
				"<p>level 1</p>"
			"</div><p><br></p>"
		HTML_SUFFIX,
		"level 0\n"
		"    level 1\n"
		"        level 2\n"
		"    level 1\n"))
		g_test_fail ();
}

static void
test_font_size_selection (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some monospace text\n"
		"seq:hCrcrCSrsc\n"
		"action:monospaced\n",
		HTML_PREFIX "<p>some <font face=\"monospace\" size=\"3\">monospace</font> text</p>" HTML_SUFFIX,
		"some monospace text"))
		g_test_fail ();
}

static void
test_font_size_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some \n"
		"action:monospaced\n"
		"type:monospace\n"
		"action:monospaced\n"
		"type: text\n",
		HTML_PREFIX "<p>some <font face=\"monospace\" size=\"3\">monospace</font> text</p>" HTML_SUFFIX,
		"some monospace text"))
		g_test_fail ();
}

static void
test_list_bullet_plain (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"action:style-list-bullet\n"
		"type:item 1\\n\n"
		"type:item 2\\n\n"
		"type:item 3\\n\n"
		"type:\\n\n"
		"type:text\n",
		NULL,
		" * item 1\n"
		" * item 2\n"
		" * item 3\n"
		"text"))
		g_test_fail ();
}

static void
test_list_bullet_html (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"action:style-list-bullet\n"
		"type:item 1\\n\n"
		"action:indent\n"
		"type:item 2\\n\n"
		"action:unindent\n"
		"type:item 3\\n\n"
		"type:\\n\n"
		"type:text\n",
		HTML_PREFIX
			"<ul>"
				"<li>item 1</li>"
				"<ul>"
					"<li>item 2</li>"
				"</ul>"
				"<li>item 3</li>"
			"</ul>"
			"<p>text</p>"
		HTML_SUFFIX,
		" * item 1\n"
		"    * item 2\n"
		" * item 3\n"
		"text"))
		g_test_fail ();
}

static void
test_list_bullet_html_from_block (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:item 1\\n\n"
		"type:item 2\n"
		"action:style-list-roman\n"
		"type:\\n\n"
		"action:style-preformat\n"
		"type:item 3\\n\n"
		"action:select-all\n"
		"action:style-list-bullet\n",
		HTML_PREFIX
			"<ul>"
				"<li>item 1</li>"
				"<li>item 2</li>"
				"<li>item 3</li>"
				"<li><br></li>"
			"</ul>"
		HTML_SUFFIX,
		" * item 1\n"
		" * item 2\n"
		" * item 3\n"
		" * "))
		g_test_fail ();
}

static void
test_list_alpha_html (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"action:style-list-alpha\n"
		"type:item 1\\n\n"
		"action:indent\n"
		"type:item 2\\n\n"
		"action:unindent\n"
		"type:item 3\\n\n"
		"type:\\n\n"
		"type:text\n",
		HTML_PREFIX
			"<ol type=\"A\">"
				"<li>item 1</li>"
				"<ol type=\"A\">"
					"<li>item 2</li>"
				"</ol>"
				"<li>item 3</li>"
			"</ol>"
			"<p>text</p>"
		HTML_SUFFIX,
		"   A. item 1\n"
		"      A. item 2\n"
		"   B. item 3\n"
		"text"))
		g_test_fail ();
}

static void
test_list_roman_plain (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"action:style-list-roman\n"
		"type:1\\n\n"
		"type:2\\n\n"
		"type:3\\n\n"
		"type:4\\n\n"
		"type:5\\n\n"
		"type:6\\n\n"
		"type:7\\n\n"
		"type:8\\n\n"
		"type:9\\n\n"
		"type:10\\n\n"
		"type:11\\n\n"
		"type:12\\n\n"
		"type:13\\n\n"
		"type:14\\n\n"
		"type:15\\n\n"
		"type:16\\n\n"
		"type:17\\n\n"
		"type:18\n",
		NULL,
		"   I. 1\n"
		"  II. 2\n"
		" III. 3\n"
		"  IV. 4\n"
		"   V. 5\n"
		"  VI. 6\n"
		" VII. 7\n"
		"VIII. 8\n"
		"  IX. 9\n"
		"   X. 10\n"
		"  XI. 11\n"
		" XII. 12\n"
		"XIII. 13\n"
		" XIV. 14\n"
		"  XV. 15\n"
		" XVI. 16\n"
		"XVII. 17\n"
		"XVIII. 18"))
		g_test_fail ();
}

static void
test_list_multi_html (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"action:style-list-bullet\n"
		"type:item 1\\n\n"
		"type:item 2\\n\n"
		"type:\\n\n"
		"action:style-list-roman\n"
		"type:item 3\\n\n"
		"type:item 4\\n\n",
		HTML_PREFIX
			"<ul>"
				"<li>item 1</li>"
				"<li>item 2</li>"
			"</ul>"
			"<ol type=\"I\">"
				"<li>item 3</li>"
				"<li>item 4</li>"
				"<li><br></li>"
			"</ol>"
		HTML_SUFFIX,
		" * item 1\n"
		" * item 2\n"
		"   I. item 3\n"
		"  II. item 4\n"
		" III. "))
		g_test_fail ();
}

static void
test_list_multi_change_html (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"action:style-list-bullet\n"
		"type:item 1\\n\n"
		"type:item 2\\n\n"
		"type:\\n\n"
		"action:style-list-roman\n"
		"type:item 3\\n\n"
		"type:item 4\\n\n"
		"action:select-all\n"
		"action:style-list-number\n",
		HTML_PREFIX
			"<ol>"
				"<li>item 1</li>"
				"<li>item 2</li>"
				"<li>item 3</li>"
				"<li>item 4</li>"
				"<li><br></li>"
			"</ol>"
		HTML_SUFFIX,
		"   1. item 1\n"
		"   2. item 2\n"
		"   3. item 3\n"
		"   4. item 4\n"
		"   5. "))
		g_test_fail ();
}

static void
test_link_insert_dialog (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:a link example: \n"
		"action:insert-link\n"
		"type:http://www.gnome.org\n"
		"seq:n\n",
		HTML_PREFIX "<p>a link example: <a href=\"http://www.gnome.org\">http://www.gnome.org</a></p>" HTML_SUFFIX,
		"a link example: http://www.gnome.org"))
		g_test_fail ();
}

static void
test_link_insert_dialog_selection (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:a link example: GNOME\n"
		"seq:CSlsc\n"
		"action:insert-link\n"
		"type:http://www.gnome.org\n"
		"seq:n\n",
		HTML_PREFIX "<p>a link example: <a href=\"http://www.gnome.org\">GNOME</a></p>" HTML_SUFFIX,
		"a link example: GNOME"))
		g_test_fail ();
}

static void
test_link_insert_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:www.gnome.org \n",
		HTML_PREFIX "<p><a href=\"http://www.gnome.org\">www.gnome.org</a> </p>" HTML_SUFFIX,
		"www.gnome.org "))
		g_test_fail ();
}

static void
test_link_insert_typed_change_description (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:www.gnome.org \n"
		"seq:ll\n"
		"action:insert-link\n"
		"seq:tt\n" /* Jump to the description */
		"type:GNOME\n"
		"seq:n\n",
		HTML_PREFIX "<p><a href=\"http://www.gnome.org\">GNOME</a> </p>" HTML_SUFFIX,
		"GNOME "))
		g_test_fail ();
}

static void
test_link_insert_dialog_remove (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:www.gnome.org \n"
		"seq:ll\n"
		"action:insert-link\n"
		"seq:tttt\n" /* Jump to 'Remove Link' */
		"seq:n\n", /* Press the button */
		HTML_PREFIX "<p>www.gnome.org </p>" HTML_SUFFIX,
		"www.gnome.org "))
		g_test_fail ();
}

static void
test_link_insert_typed_append (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:www.gnome.org \n"
		"seq:l\n"
		"type:/about\n",
		HTML_PREFIX "<p><a href=\"http://www.gnome.org/about\">www.gnome.org/about</a> </p>" HTML_SUFFIX,
		"www.gnome.org/about "))
		g_test_fail ();
}

static void
test_h_rule_insert (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text\n"
		"action:insert-rule\n"
		"seq:tttttn\n", /* Move to the Close button and press it */
		HTML_PREFIX "<p>text</p><hr align=\"left\" size=\"2\" noshade=\"\">" HTML_SUFFIX,
		"text"))
		g_test_fail ();
}

static void
test_h_rule_insert_text_after (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:above\n"
		"action:insert-rule\n"
		"seq:tttttn\n" /* Move to the Close button and press it */
		"seq:den\n"
		"type:below\n",
		HTML_PREFIX "<p>above</p><hr align=\"left\" size=\"2\" noshade=\"\"><p>below</p>" HTML_SUFFIX,
		"above\nbelow"))
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

	/* Force the memory GSettings backend, to not overwrite user settings
	   when playing with them. It also ensures that the test will run with
	   default settings, until changed. */
	g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

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
	add_test ("/style/bold/selection", test_style_bold_selection);
	add_test ("/style/bold/typed", test_style_bold_typed);
	add_test ("/style/italic/selection", test_style_italic_selection);
	add_test ("/style/italic/typed", test_style_italic_typed);
	add_test ("/style/underline/selection", test_style_underline_selection);
	add_test ("/style/underline/typed", test_style_underline_typed);
	add_test ("/style/monospace/selection", test_style_monospace_selection);
	add_test ("/style/monospace/typed", test_style_monospace_typed);
	add_test ("/undo/text-typed", test_undo_text_typed);
	add_test ("/undo/text/forward-delete", test_undo_text_forward_delete);
	add_test ("/undo/text/backward-delete", test_undo_text_backward_delete);
	add_test ("/justify/selection", test_justify_selection);
	add_test ("/justify/typed", test_justify_typed);
	add_test ("/indent/selection", test_indent_selection);
	add_test ("/indent/typed", test_indent_typed);
	add_test ("/font/size/selection", test_font_size_selection);
	add_test ("/font/size/typed", test_font_size_typed);
	add_test ("/list/bullet/plain", test_list_bullet_plain);
	add_test ("/list/bullet/html", test_list_bullet_html);
	add_test ("/list/bullet/html/from-block", test_list_bullet_html_from_block);
	add_test ("/list/alpha/html", test_list_alpha_html);
	add_test ("/list/roman/plain", test_list_roman_plain);
	add_test ("/list/multi/html", test_list_multi_html);
	add_test ("/list/multi/change/html", test_list_multi_change_html);
	add_test ("/link/insert/dialog", test_link_insert_dialog);
	add_test ("/link/insert/dialog/selection", test_link_insert_dialog_selection);
	add_test ("/link/insert/dialog/remove", test_link_insert_dialog_remove);
	add_test ("/link/insert/typed", test_link_insert_typed);
	add_test ("/link/insert/typed/change-description", test_link_insert_typed_change_description);
	add_test ("/link/insert/typed/append", test_link_insert_typed_append);
	add_test ("/h-rule/insert", test_h_rule_insert);
	add_test ("/h-rule/insert-text-after", test_h_rule_insert_text_after);

	#undef add_test

	res = g_test_run ();

	e_util_cleanup_settings ();
	e_spell_checker_free_global_memory ();

	return res;
}
