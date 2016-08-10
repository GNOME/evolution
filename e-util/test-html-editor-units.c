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
#define HTML_PREFIX_PLAIN "<html><head></head><body style=\"font-family: Monospace;\">"
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
test_style_strikethrough_selection (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some strikethrough text\n"
		"seq:hCrcrCSrsc\n"
		"action:strikethrough\n",
		HTML_PREFIX "<p>some <strike>strikethrough</strike> text</p>" HTML_SUFFIX,
		"some strikethrough text"))
		g_test_fail ();
}

static void
test_style_strikethrough_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some \n"
		"action:strikethrough\n"
		"type:strikethrough\n"
		"action:strikethrough\n"
		"type: text\n",
		HTML_PREFIX "<p>some <strike>strikethrough</strike> text</p>" HTML_SUFFIX,
		"some strikethrough text"))
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
		"type:FontM2 FontM1 Font0 FontP1 FontP2 FontP3 FontP4\n"
		"seq:hCSrsc\n"
		"action:size-minus-two\n"
		"seq:rrCSrcs\n"
		"action:size-minus-one\n"
		"seq:rrCSrcs\n"
		"action:size-plus-zero\n"
		"seq:rrCSrcs\n"
		"action:size-plus-one\n"
		"seq:rrCSrcs\n"
		"action:size-plus-two\n"
		"seq:rrCSrcs\n"
		"action:size-plus-three\n"
		"seq:rrCSrcs\n"
		"action:size-plus-four\n",
		HTML_PREFIX "<p><font size=\"1\">FontM2</font> <font size=\"2\">FontM1</font> Font0 <font size=\"4\">FontP1</font> "
		"<font size=\"5\">FontP2</font> <font size=\"6\">FontP3</font> <font size=\"7\">FontP4</font></p>" HTML_SUFFIX,
		"FontM2 FontM1 Font0 FontP1 FontP2 FontP3 FontP4"))
		g_test_fail ();
}

static void
test_font_size_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"action:size-minus-two\n"
		"type:FontM2\n"
		"action:size-plus-zero\n"
		"type: \n"
		"action:size-minus-one\n"
		"type:FontM1\n"
		"action:size-plus-zero\n"
		"type: \n"
		"type:Font0\n"
		"action:size-plus-zero\n"
		"type: \n"
		"action:size-plus-one\n"
		"type:FontP1\n"
		"action:size-plus-zero\n"
		"type: \n"
		"action:size-plus-two\n"
		"type:FontP2\n"
		"action:size-plus-zero\n"
		"type: \n"
		"action:size-plus-three\n"
		"type:FontP3\n"
		"action:size-plus-zero\n"
		"type: \n"
		"action:size-plus-four\n"
		"type:FontP4\n"
		"action:size-plus-zero\n",
		HTML_PREFIX "<p><font size=\"1\">FontM2</font> <font size=\"2\">FontM1</font> Font0 <font size=\"4\">FontP1</font> "
		"<font size=\"5\">FontP2</font> <font size=\"6\">FontP3</font> <font size=\"7\">FontP4</font></p>" HTML_SUFFIX,
		"FontM2 FontM1 Font0 FontP1 FontP2 FontP3 FontP4"))
		g_test_fail ();
}

static void
test_font_color_selection (TestFixture *fixture)
{
	EContentEditor *cnt_editor;
	GdkRGBA rgba;

	g_return_if_fail (fixture != NULL);
	g_return_if_fail (E_IS_HTML_EDITOR (fixture->editor));

	cnt_editor = e_html_editor_get_content_editor (fixture->editor);
	g_return_if_fail (cnt_editor != NULL);

	if (!test_utils_process_commands (fixture,
		"mode:html\n"
		"type:default red green blue\n"
		"seq:hCrcrCSrsc\n")) {
		g_test_fail ();
		return;
	}

	rgba.red = 1.0;
	rgba.green = 0.0;
	rgba.blue = 0.0;
	rgba.alpha = 1.0;

	e_content_editor_set_font_color (cnt_editor, &rgba);

	if (!test_utils_process_commands (fixture,
		"seq:rrCSrcs\n")) {
		g_test_fail ();
		return;
	}

	rgba.red = 0.0;
	rgba.green = 1.0;
	rgba.blue = 0.0;
	rgba.alpha = 1.0;

	e_content_editor_set_font_color (cnt_editor, &rgba);

	if (!test_utils_process_commands (fixture,
		"seq:rrCSrcs\n")) {
		g_test_fail ();
		return;
	}

	rgba.red = 0.0;
	rgba.green = 0.0;
	rgba.blue = 1.0;
	rgba.alpha = 1.0;

	e_content_editor_set_font_color (cnt_editor, &rgba);

	if (!test_utils_run_simple_test (fixture, "",
		HTML_PREFIX "<p>default <font color=\"#ff0000\">red</font> <font color=\"#00ff00\">green</font> "
		"<font color=\"#0000ff\">blue</font></p>" HTML_SUFFIX,
		"default red green blue"))
		g_test_fail ();
}

static void
test_font_color_typed (TestFixture *fixture)
{
	EContentEditor *cnt_editor;
	GdkRGBA rgba;

	g_return_if_fail (fixture != NULL);
	g_return_if_fail (E_IS_HTML_EDITOR (fixture->editor));

	cnt_editor = e_html_editor_get_content_editor (fixture->editor);
	g_return_if_fail (cnt_editor != NULL);

	if (!test_utils_process_commands (fixture,
		"mode:html\n"
		"type:default \n")) {
		g_test_fail ();
		return;
	}

	rgba.red = 1.0;
	rgba.green = 0.0;
	rgba.blue = 0.0;
	rgba.alpha = 1.0;

	e_content_editor_set_font_color (cnt_editor, &rgba);

	if (!test_utils_process_commands (fixture,
		"type:red \n")) {
		g_test_fail ();
		return;
	}

	rgba.red = 0.0;
	rgba.green = 1.0;
	rgba.blue = 0.0;
	rgba.alpha = 1.0;

	e_content_editor_set_font_color (cnt_editor, &rgba);

	if (!test_utils_process_commands (fixture,
		"type:green \n")) {
		g_test_fail ();
		return;
	}

	rgba.red = 0.0;
	rgba.green = 0.0;
	rgba.blue = 1.0;
	rgba.alpha = 1.0;

	e_content_editor_set_font_color (cnt_editor, &rgba);

	if (!test_utils_process_commands (fixture,
		"type:blue\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture, "",
		HTML_PREFIX "<p>default <font color=\"#ff0000\">red </font><font color=\"#00ff00\">green </font>"
		"<font color=\"#0000ff\">blue</font></p>" HTML_SUFFIX,
		"default red green blue"))
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
test_list_alpha_plain (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"action:style-list-alpha\n"
		"type:item 1\\n\n"
		"action:indent\n"
		"type:item 2\\n\n"
		"action:unindent\n"
		"type:item 3\\n\n"
		"type:\\n\n"
		"type:text\n",
		NULL,
		"   A. item 1\n"
		"      A. item 2\n"
		"   B. item 3\n"
		"text"))
		g_test_fail ();
}

static void
test_list_roman_html (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
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
		HTML_PREFIX "<ol type=\"I\">"
		"<li>1</li><li>2</li><li>3</li><li>4</li><li>5</li><li>6</li>"
		"<li>7</li><li>8</li><li>9</li><li>10</li><li>11</li><li>12</li>"
		"<li>13</li><li>14</li><li>15</li><li>16</li><li>17</li><li>18</li>"
		"</ol>" HTML_SUFFIX,
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
test_list_multi_plain (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"action:style-list-bullet\n"
		"type:item 1\\n\n"
		"type:item 2\\n\n"
		"type:\\n\n"
		"action:style-list-roman\n"
		"type:item 3\\n\n"
		"type:item 4\\n\n",
		NULL,
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
test_list_multi_change_plain (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"action:style-list-bullet\n"
		"type:item 1\\n\n"
		"type:item 2\\n\n"
		"type:\\n\n"
		"action:style-list-roman\n"
		"type:item 3\\n\n"
		"type:item 4\\n\n"
		"action:select-all\n"
		"action:style-list-number\n",
		NULL,
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
test_link_insert_dialog_remove_link (TestFixture *fixture)
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
test_link_insert_typed_remove (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:www.gnome.org \n"
		"seq:bbb\n",
		HTML_PREFIX "<p><a href=\"http://www.gnome.org\">www.gnome.o</a></p>" HTML_SUFFIX,
		"www.gnome.o"))
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

static void
test_emoticon_insert_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:before :)after\n",
		HTML_PREFIX "<p>before <img src=\"data:image/png;base64,"
		"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABHNCSVQICAgIfA"
		"hkiAAAAAlwSFlzAAAN1wAADdcBQiibeAAAABl0RVh0U29mdHdhcmUAd3d3Lmlu"
		"a3NjYXBlLm9yZ5vuPBoAAAAXdEVYdEF1dGhvcgBMYXBvIENhbGFtYW5kcmVp35"
		"EaKgAAACl0RVh0RGVzY3JpcHRpb24AQmFzZWQgb2YgSmFrdWIgU3RlaW5lciBk"
		"ZXNpZ26ghAVzAAADRklEQVQ4jWWTXWiVdQCHn//7ec4+zmrbcZ+usT4wy23ilj"
		"K3cmIRIl10GYGgVCMovOjjMsSrwGEFSXohMchCKBBUWEFqrQ10yTbHNh2bp03n"
		"vrdztvN+/t/330U3C3/XD8/FDx6hlGLrLnzeXq0L/R1bVwfQxB4AYvW3H4kbkY"
		"ouHvuyb24rL7YKej7rOJYsKv7m5Vf22eW1VUZheRqUIr+8yNLsYzk00O/nNzc/"
		"fu9M/4UnBD982nat9oXdnS0HX08I/w7CWgRLAgp8E8JKIrOJvt6rXmZ8+HrXt7"
		"cPA2gA33/S3lXz3K7O1vb9iWj6PFd+vcaVgQxaugyRLoMSxenvfuTu72d47eCh"
		"RE3Diwe632/pAtDOn+io061k9562NxLe3XPE0QK3J/LcHllH2UmwCxAFRfw1km"
		"Po3gze6FlePXQkqZt298mjLXVaKMPjzc177XjmOnHuAQJI2BoJWwcVI5QCFZOw"
		"DCxLIHMZePgH+/d12FEkjxuRDDrLKrbrwUQvQsTg+xxpT6OltyG83H9PbWZ596"
		"16GlPrCG+N4NEtKp49qkcy6DQCpTcWPVXB+uI0q2jUFHrsrHLRyx3ihVkEArWx"
		"wZsvScIFH5mTLMxPUbwjRSC1RiOUwvCyC+ihz9OFNlf716mtlWyvd6moKsQ0BM"
		"r1eTiT5d5Ejrl5l+ZSRaWlEUTKMFzXGc3cH9pba5RgyU0O7ypnaDnBz79lWd2Y"
		"oyChIaRCKEFjXQmdO1OkYsnU2C183xs1vIib45P3W9OFBZrlriCyKzSlK9nd2A"
		"ZFNfjuBpZdjB75+JkB5KNxfLue4fHx2JPcNGSkekamFj+qbtCTOyIF+KhonnC1"
		"F19LEGOioggt2MCWEt2PeSBN+kcyfiC1Hn1gbHHpcs/X1j8rbmupuW6mlESPFF"
		"ocEwcBkesifAc7DFFOQMaBS2Oak3Pj0z/dmL6kAVTkq09lA3Py8ly1M7hmMJ8L"
		"8bMu5qZDgeti5F3WciF3VjUuTpY6yw6TS/rMqf+18EFLi5lPrZxUSp14piiXqE"
		"n6ojLpA/DYsZh1bDWVLfIU4qtyb9sX5wYHwydqBHi7o6FJI/xQINqjWDwPoGtq"
		"UqH6Ysyzv/w5PbyV/xd0ZaEGG/mx/wAAAABJRU5ErkJggg==\" alt=\":-)\">"
		"after</p>" HTML_SUFFIX,
		"before :-)after"))
		g_test_fail ();
}

static void
test_emoticon_insert_typed_dash (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:before :-)after\n",
		HTML_PREFIX "<p>before <img src=\"data:image/png;base64,"
		"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABHNCSVQICAgIfA"
		"hkiAAAAAlwSFlzAAAN1wAADdcBQiibeAAAABl0RVh0U29mdHdhcmUAd3d3Lmlu"
		"a3NjYXBlLm9yZ5vuPBoAAAAXdEVYdEF1dGhvcgBMYXBvIENhbGFtYW5kcmVp35"
		"EaKgAAACl0RVh0RGVzY3JpcHRpb24AQmFzZWQgb2YgSmFrdWIgU3RlaW5lciBk"
		"ZXNpZ26ghAVzAAADRklEQVQ4jWWTXWiVdQCHn//7ec4+zmrbcZ+usT4wy23ilj"
		"K3cmIRIl10GYGgVCMovOjjMsSrwGEFSXohMchCKBBUWEFqrQ10yTbHNh2bp03n"
		"vrdztvN+/t/330U3C3/XD8/FDx6hlGLrLnzeXq0L/R1bVwfQxB4AYvW3H4kbkY"
		"ouHvuyb24rL7YKej7rOJYsKv7m5Vf22eW1VUZheRqUIr+8yNLsYzk00O/nNzc/"
		"fu9M/4UnBD982nat9oXdnS0HX08I/w7CWgRLAgp8E8JKIrOJvt6rXmZ8+HrXt7"
		"cPA2gA33/S3lXz3K7O1vb9iWj6PFd+vcaVgQxaugyRLoMSxenvfuTu72d47eCh"
		"RE3Diwe632/pAtDOn+io061k9562NxLe3XPE0QK3J/LcHllH2UmwCxAFRfw1km"
		"Po3gze6FlePXQkqZt298mjLXVaKMPjzc177XjmOnHuAQJI2BoJWwcVI5QCFZOw"
		"DCxLIHMZePgH+/d12FEkjxuRDDrLKrbrwUQvQsTg+xxpT6OltyG83H9PbWZ596"
		"16GlPrCG+N4NEtKp49qkcy6DQCpTcWPVXB+uI0q2jUFHrsrHLRyx3ihVkEArWx"
		"wZsvScIFH5mTLMxPUbwjRSC1RiOUwvCyC+ihz9OFNlf716mtlWyvd6moKsQ0BM"
		"r1eTiT5d5Ejrl5l+ZSRaWlEUTKMFzXGc3cH9pba5RgyU0O7ypnaDnBz79lWd2Y"
		"oyChIaRCKEFjXQmdO1OkYsnU2C183xs1vIib45P3W9OFBZrlriCyKzSlK9nd2A"
		"ZFNfjuBpZdjB75+JkB5KNxfLue4fHx2JPcNGSkekamFj+qbtCTOyIF+KhonnC1"
		"F19LEGOioggt2MCWEt2PeSBN+kcyfiC1Hn1gbHHpcs/X1j8rbmupuW6mlESPFF"
		"ocEwcBkesifAc7DFFOQMaBS2Oak3Pj0z/dmL6kAVTkq09lA3Py8ly1M7hmMJ8L"
		"8bMu5qZDgeti5F3WciF3VjUuTpY6yw6TS/rMqf+18EFLi5lPrZxUSp14piiXqE"
		"n6ojLpA/DYsZh1bDWVLfIU4qtyb9sX5wYHwydqBHi7o6FJI/xQINqjWDwPoGtq"
		"UqH6Ysyzv/w5PbyV/xd0ZaEGG/mx/wAAAABJRU5ErkJggg==\" alt=\":-)\">"
		"after</p>" HTML_SUFFIX,
		"before :-)after"))
		g_test_fail ();
}

static void
test_paragraph_normal_selection (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"action:style-preformat\n"
		"type:Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\\n\n"
		"type:Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n"
		"seq:hu\n"
		"action:style-normal\n",
		HTML_PREFIX "<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</p>"
		"<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</p>"
		"<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n",
		HTML_PREFIX "<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</p>"
		"<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}
}

static void
test_paragraph_normal_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"action:style-normal\n"
		"type:Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\\n\n"
		"type:Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n",
		HTML_PREFIX "<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</p>"
		"<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</p>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</p>"
		"<p style=\"width: 71ch;\">Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</p>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n",
		HTML_PREFIX "<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</p>"
		"<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</p>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}
}

static void
test_paragraph_preformatted_selection (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"action:style-normal\n"
		"type:Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\\n\n"
		"type:Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n"
		"seq:Chc\n"
		"action:style-preformat\n",
		HTML_PREFIX "<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>"
		"<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</p>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n",
		HTML_PREFIX_PLAIN "<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>"
		"<p style=\"width: 71ch;\">Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</p>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n",
		HTML_PREFIX "<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>"
		"<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</p>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}
}

static void
test_paragraph_preformatted_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"action:style-preformat\n"
		"type:Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero. \n"
		"type:Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n",
		HTML_PREFIX "<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero."
		" Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero. "
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n",
		HTML_PREFIX_PLAIN "<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero."
		" Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero. "
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n",
		HTML_PREFIX "<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero."
		" Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero. "
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}
}

static void
test_paragraph_address_selection (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"action:style-normal\n"
		"type:normal text\\n\n"
		"type:address line 1\\n\n"
		"type:address line 2\\n\n"
		"type:address line 3\\n\n"
		"type:\\n\n"
		"type:normal text\n"
		"seq:huuuuSddrs\n"
		"action:style-address\n",
		HTML_PREFIX "<p>normal text</p>"
		"<address>address line 1</address>"
		"<address>address line 2</address>"
		"<address>address line 3</address>"
		"<p><br></p>"
		"<p>normal text</p>" HTML_SUFFIX,
		"normal text\n"
		"address line 1\n"
		"address line 2\n"
		"address line 3\n"
		"\n"
		"normal text"))
		g_test_fail ();
}

static void
test_paragraph_address_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"action:style-normal\n"
		"type:normal text\\n\n"
		"action:style-address\n"
		"type:address line 1\\n\n"
		"type:address line 2\\n\n"
		"type:address line 3\\n\n"
		"action:style-normal\n"
		"type:\\n\n"
		"type:normal text\n",
		HTML_PREFIX "<p>normal text</p>"
		"<address>address line 1</address>"
		"<address>address line 2</address>"
		"<address>address line 3</address>"
		"<p><br></p>"
		"<p>normal text</p>" HTML_SUFFIX,
		"normal text\n"
		"address line 1\n"
		"address line 2\n"
		"address line 3\n"
		"normal text"))
		g_test_fail ();
}

static gboolean
test_paragraph_header_n_selection (TestFixture *fixture,
				   gint header_n)
{
	gchar *actions, *expected_html, *expected_plain;
	gboolean success;

	actions = g_strdup_printf (
		"mode:html\n"
		"action:style-normal\n"
		"type:normal text\\n\n"
		"type:header %d\\n\n"
		"type:normal text\n"
		"seq:hu\n"
		"action:style-h%d\n",
		header_n, header_n);

	expected_html = g_strdup_printf (
		HTML_PREFIX "<p>normal text</p>"
		"<h%d>header %d</h%d>"
		"<p>normal text</p>" HTML_SUFFIX,
		header_n, header_n, header_n);

	expected_plain = g_strdup_printf (
		"normal text\n"
		"header %d\n"
		"normal text",
		header_n);

	success = test_utils_run_simple_test (fixture, actions, expected_html, expected_plain);

	g_free (expected_plain);
	g_free (expected_html);
	g_free (actions);

	if (!success)
		return success;

	expected_html = g_strdup_printf (
		HTML_PREFIX "<p>normal text</p>"
		"<h%d>header %d</h%d>"
		"<p><br></p>"
		"<p>normal text</p>" HTML_SUFFIX,
		header_n, header_n, header_n);

	expected_plain = g_strdup_printf (
		"normal text\n"
		"header %d\n"
		"\n"
		"normal text",
		header_n);

	success = test_utils_run_simple_test (fixture,
		"seq:h\n"
		"type:\\n\n",
		expected_html, expected_plain);

	g_free (expected_plain);
	g_free (expected_html);

	return success;
}

static gboolean
test_paragraph_header_n_typed (TestFixture *fixture,
			       gint header_n)
{
	gchar *actions, *expected_html, *expected_plain;
	gboolean success;

	actions = g_strdup_printf (
		"mode:html\n"
		"action:style-normal\n"
		"type:normal text\\n\n"
		"action:style-h%d\n"
		"type:header %d\\n\n"
		"action:style-normal\n"
		"type:normal text\n",
		header_n, header_n);

	expected_html = g_strdup_printf (
		HTML_PREFIX "<p>normal text</p>"
		"<h%d>header %d</h%d>"
		"<p>normal text</p>" HTML_SUFFIX,
		header_n, header_n, header_n);

	expected_plain = g_strdup_printf (
		"normal text\n"
		"header %d\n"
		"normal text",
		header_n);

	success = test_utils_run_simple_test (fixture, actions, expected_html, expected_plain);

	g_free (expected_plain);
	g_free (expected_html);
	g_free (actions);

	if (!success)
		return success;

	expected_html = g_strdup_printf (
		HTML_PREFIX "<p>normal text</p>"
		"<h%d>header %d</h%d>"
		"<p><br></p>"
		"<p>normal text</p>" HTML_SUFFIX,
		header_n, header_n, header_n);

	expected_plain = g_strdup_printf (
		"normal text\n"
		"header %d\n"
		"\n"
		"normal text",
		header_n);

	success = test_utils_run_simple_test (fixture,
		"seq:h\n"
		"type:\\n\n",
		expected_html, expected_plain);

	g_free (expected_plain);
	g_free (expected_html);

	return success;
}

static void
test_paragraph_header1_selection (TestFixture *fixture)
{
	if (!test_paragraph_header_n_selection (fixture, 1))
		g_test_fail ();
}

static void
test_paragraph_header1_typed (TestFixture *fixture)
{
	if (!test_paragraph_header_n_typed (fixture, 1))
		g_test_fail ();
}

static void
test_paragraph_header2_selection (TestFixture *fixture)
{
	if (!test_paragraph_header_n_selection (fixture, 2))
		g_test_fail ();
}

static void
test_paragraph_header2_typed (TestFixture *fixture)
{
	if (!test_paragraph_header_n_typed (fixture, 2))
		g_test_fail ();
}

static void
test_paragraph_header3_selection (TestFixture *fixture)
{
	if (!test_paragraph_header_n_selection (fixture, 3))
		g_test_fail ();
}

static void
test_paragraph_header3_typed (TestFixture *fixture)
{
	if (!test_paragraph_header_n_typed (fixture, 3))
		g_test_fail ();
}

static void
test_paragraph_header4_selection (TestFixture *fixture)
{
	if (!test_paragraph_header_n_selection (fixture, 4))
		g_test_fail ();
}

static void
test_paragraph_header4_typed (TestFixture *fixture)
{
	if (!test_paragraph_header_n_typed (fixture, 4))
		g_test_fail ();
}

static void
test_paragraph_header5_selection (TestFixture *fixture)
{
	if (!test_paragraph_header_n_selection (fixture, 5))
		g_test_fail ();
}

static void
test_paragraph_header5_typed (TestFixture *fixture)
{
	if (!test_paragraph_header_n_typed (fixture, 5))
		g_test_fail ();
}

static void
test_paragraph_header6_selection (TestFixture *fixture)
{
	if (!test_paragraph_header_n_selection (fixture, 6))
		g_test_fail ();
}

static void
test_paragraph_header6_typed (TestFixture *fixture)
{
	if (!test_paragraph_header_n_typed (fixture, 6))
		g_test_fail ();
}

static void
test_paragraph_wrap_lines (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\\n\n"
		"type:Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n"
		"action:select-all\n"
		"action:wrap-lines\n",
		HTML_PREFIX "<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec<br>odio. Praesent libero.</p>"
		"<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec<br>odio. Praesent libero.</p>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n" "odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n" "odio. Praesent libero."))
		g_test_fail ();
}

static void
test_paste_singleline_html2html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body>some <b>bold</b> text</body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text before \n"
		"action:paste\n"
		"type: text after\n",
		HTML_PREFIX "<p>text before some <b>bold</b> text text after</p>" HTML_SUFFIX,
		"text before some bold text text after"))
		g_test_fail ();
}

static void
test_paste_singleline_html2plain (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body>some <b>bold</b> text</body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:text before \n"
		"action:paste\n"
		"type: text after\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">text before some bold text text after</p>" HTML_SUFFIX,
		"text before some bold text text after"))
		g_test_fail ();
}

static void
test_paste_singleline_plain2html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("some plain text", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text before \n"
		"action:paste\n"
		"type: text after\n",
		HTML_PREFIX "<p>text before some plain text text after</p>" HTML_SUFFIX,
		"text before some plain text text after"))
		g_test_fail ();
}

static void
test_paste_singleline_plain2plain (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("some plain text", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:text before \n"
		"action:paste\n"
		"type: text after\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">text before some plain text text after</p>" HTML_SUFFIX,
		"text before some plain text text after"))
		g_test_fail ();
}

static void
test_paste_multiline_html2html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body><b>bold</b> text<br><i>italic</i> text<br><u>underline</u> text<br></body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text before \n"
		"action:paste\n"
		"type:text after\n",
		HTML_PREFIX "<p>text before <b>bold</b> text</p><p><i>italic</i> text</p><p><u>underline</u> text</p><p>text after</p>" HTML_SUFFIX,
		"text before bold text\nitalic text\nunderline text\ntext after"))
		g_test_fail ();
}

static void
test_paste_multiline_html2plain (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body><b>bold</b> text<br><i>italic</i> text<br><u>underline</u> text</body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:text before \n"
		"action:paste\n"
		"type:\\ntext after\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">text before bold text</p>"
		"<p style=\"width: 71ch;\">italic text</p>"
		"<p style=\"width: 71ch;\">underline text</p>"
		"<p style=\"width: 71ch;\">text after</p>" HTML_SUFFIX,
		"text before bold text\nitalic text\nunderline text\ntext after"))
		g_test_fail ();
}

static void
test_paste_multiline_div_html2html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body><div><b>bold</b> text</div><div><i>italic</i> text</div><div><u>underline</u> text</div><div></div></body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text before \n"
		"action:paste\n"
		"type:text after\n",
		HTML_PREFIX "<p>text before <b>bold</b> text</p><p><i>italic</i> text</p><p><u>underline</u> text</p><p>text after</p>" HTML_SUFFIX,
		"text before bold text\nitalic text\nunderline text\ntext after"))
		g_test_fail ();
}

static void
test_paste_multiline_div_html2plain (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body><div><b>bold</b> text</div><div><i>italic</i> text</div><div><u>underline</u> text</div></body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:text before \n"
		"action:paste\n"
		"type:\\ntext after\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">text before bold text</p>"
		"<p style=\"width: 71ch;\">italic text</p>"
		"<p style=\"width: 71ch;\">underline text</p>"
		"<p style=\"width: 71ch;\">text after</p>" HTML_SUFFIX,
		"text before bold text\nitalic text\nunderline text\ntext after"))
		g_test_fail ();
}

static void
test_paste_multiline_p_html2html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body><p><b>bold</b> text</p><p><i>italic</i> text</p><p><u>underline</u> text</p><p></p></body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text before \n"
		"action:paste\n"
		"type:text after\n",
		HTML_PREFIX "<p>text before <b>bold</b> text</p><p><i>italic</i> text</p><p><u>underline</u> text</p><p>text after</p>" HTML_SUFFIX,
		"text before bold text\nitalic text\nunderline text\ntext after"))
		g_test_fail ();
}

static void
test_paste_multiline_p_html2plain (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body><p><b>bold</b> text</p><p><i>italic</i> text</p><p><u>underline</u> text</p></body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:text before \n"
		"action:paste\n"
		"type:\\ntext after\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">text before bold text</p>"
		"<p style=\"width: 71ch;\">italic text</p>"
		"<p style=\"width: 71ch;\">underline text</p>"
		"<p style=\"width: 71ch;\">text after</p>" HTML_SUFFIX,
		"text before bold text\nitalic text\nunderline text\ntext after"))
		g_test_fail ();
}

static void
test_paste_multiline_plain2html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("line 1\nline 2\nline 3\n", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text before \n"
		"action:paste\n"
		"type:text after\n",
		HTML_PREFIX "<p>text before line 1</p><p>line 2</p><p>line 3</p><p>text after</p>" HTML_SUFFIX,
		"text before line 1\nline 2\nline 3\ntext after"))
		g_test_fail ();
}

static void
test_paste_multiline_plain2plain (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("line 1\nline 2\nline 3", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:text before \n"
		"action:paste\n"
		"type:\\ntext after\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">text before line 1</p>"
		"<p style=\"width: 71ch;\">line 2</p>"
		"<p style=\"width: 71ch;\">line 3</p>"
		"<p style=\"width: 71ch;\">text after</p>" HTML_SUFFIX,
		"text before line 1\nline 2\nline 3\ntext after"))
		g_test_fail ();
}

static void
test_paste_quoted_singleline_html2html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body>some <b>bold</b> text</body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text before \n"
		"action:paste-quote\n"
		"type:\\n\n" /* stop quotting */
		"type:text after\n",
		HTML_PREFIX "<p>text before </p>"
		"<blockquote type=\"cite\"><p>some <b>bold</b> text</p></blockquote>"
		"<p>text after</p>" HTML_SUFFIX,
		"text before \n"
		"> some bold text\n"
		"text after"))
		g_test_fail ();
}

static void
test_paste_quoted_singleline_html2plain (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body>some <b>bold</b> text</body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:text before \n"
		"action:paste-quote\n"
		"type:\\n\n" /* stop quotting */
		"type:text after\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">text before </p>"
		"<blockquote type=\"cite\"><p style=\"width: 71ch;\">&gt; some <b>bold</b> text</p></blockquote>"
		"<p style=\"width: 71ch;\">text after</p>" HTML_SUFFIX,
		"text before \n"
		"> some bold text\n"
		"text after"))
		g_test_fail ();
}

static void
test_paste_quoted_singleline_plain2html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("some plain text", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text before \n"
		"action:paste-quote\n"
		"type:\\n\n" /* stop quotting */
		"type:text after\n",
		HTML_PREFIX "<p>text before </p>"
		"<blockquote type=\"cite\"><p>some plain text</p></blockquote>"
		"<p>text after</p>" HTML_SUFFIX,
		"text before \n"
		"> some plain text\n"
		"text after"))
		g_test_fail ();
}

static void
test_paste_quoted_singleline_plain2plain (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("some plain text", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:text before \n"
		"action:paste-quote\n"
		"type:\\n\n" /* stop quotting */
		"type:text after\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">text before </p>"
		"<blockquote type=\"cite\"><p style=\"width: 71ch;\">&gt; some plain text</p></blockquote>"
		"<p style=\"width: 71ch;\">text after</p>" HTML_SUFFIX,
		"text before \n"
		"> some plain text\n"
		"text after"))
		g_test_fail ();
}

static void
test_paste_quoted_multiline_html2html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body><b>bold</b> text<br><i>italic</i> text<br><u>underline</u> text<br></body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text before \n"
		"action:paste-quote\n"
		"seq:b\n" /* stop quotting */
		"type:text after\n",
		HTML_PREFIX "<p>text before </p>"
		"<blockquote type=\"cite\">&gt; <b>bold</b> text</p>"
		"<p>&gt; <i>italic</i> text</p>"
		"<p>&gt; <u>underline</u> text</p></blockquote>"
		"<p>text after</p>" HTML_SUFFIX,
		"text before \n"
		"> bold text\n"
		"> italic text\n"
		"> underline text\n"
		"text after"))
		g_test_fail ();
}

static void
test_paste_quoted_multiline_html2plain (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body><b>bold</b> text<br><i>italic</i> text<br><u>underline</u> text</body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:text before \n"
		"action:paste-quote\n"
		"type:\\n\n" /* stop quotting */
		"type:text after\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">text before </p>"
		"<blockquote type=\"cite\"><p>&gt; bold text</p>"
		"<p style=\"width: 71ch;\">&gt; italic text</p>"
		"<p style=\"width: 71ch;\">&gt; underline text</p></blockquote>"
		"<p style=\"width: 71ch;\">&gt; text after</p>" HTML_SUFFIX,
		"text before \n"
		"> bold text\n"
		"> italic text\n"
		"> underline text\n"
		"text after"))
		g_test_fail ();
}

static void
test_paste_quoted_multiline_plain2html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("line 1\nline 2\nline 3\n", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text before \n"
		"action:paste-quote\n"
		"seq:b\n" /* stop quotting */
		"type:text after\n",
		HTML_PREFIX "<p>text before </p>"
		"<blockquote type=\"cite\"><p>line 1</p>"
		"<p>line 2</p>"
		"<p>line 3</p></blockquote>"
		"<p>text after</p>" HTML_SUFFIX,
		"text before \n"
		"> line 1\n"
		"> line 2\n"
		"> line 3\n"
		"text after"))
		g_test_fail ();
}

static void
test_paste_quoted_multiline_plain2plain (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("line 1\nline 2\nline 3", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:text before \n"
		"action:paste-quote\n"
		"type:\\n\n" /* stop quotting */
		"type:text after\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">text before </p>"
		"<blockquote type=\"cite\"><p style=\"width: 71ch;\">&gt; line 1</p>"
		"<p style=\"width: 71ch;\">&gt; line 2</p>"
		"<p style=\"width: 71ch;\">&gt; line 3</p></blockquote>"
		"<p style=\"width: 71ch;\">text after</p>" HTML_SUFFIX,
		"text before\n"
		"> line 1\n"
		"> line 2\n"
		"> line 3\n"
		"text after"))
		g_test_fail ();
}

static void
test_cite_html2plain (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body>"
		"<blockquote type=\"cite\">"
		"<p>level 1</p>"
		"<p><br></p>"
		"<p>level 1</p>"
		"<blockquote type=\"cite\">"
		"<p>level 2</p>"
		"</blockquote>"
		"<p>back in level 1</p>"
		"</blockquote>"
		"<p><br></p>"
		"<p>out of the citation</p>"
		"</body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	/* Just check the content was read properly */
	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\"><p>level 1</p><p><br></p><p>level 1</p>"
		"<blockquote type=\"cite\"><p>level 2</p></blockquote><p>back in level 1</p></blockquote>"
		"<p><br></p><p>out of the citation</p>" HTML_SUFFIX,
		"> level 1\n"
		">\n"
		"> level 1\n"
		"> > level 2\n"
		"> back in level 1\n"
		"\n"
		"out of the citation")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n",
		HTML_PREFIX_PLAIN ,
		"> level 1\n"
		">\n"
		"> level 1\n"
		"> > level 2\n"
		"> back in level 1\n"
		"\n"
		"out of the citation")) {
		g_test_fail ();
	}
}

static void
test_cite_shortline (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body><blockquote type=\"cite\">"
		"<p>Just one short line.</p>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<p>Just one short line.</p>"
		"</blockquote>" HTML_SUFFIX,
		"> Just one short line.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_process_commands (fixture,
		"seq:C\n"
		"type:a\n"
		"seq:cD\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body><blockquote type=\"cite\">"
		"<p>Just one short line.</p>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<p>Just one short line.</p>"
		"</blockquote>" HTML_SUFFIX,
		"> Just one short line.")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body><blockquote type=\"cite\">"
		"<p>short line 1</p>"
		"<p>short line 2</p>"
		"<p>short line 3</p>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<p>short line 1</p>"
		"<p>short line 2</p>"
		"<p>short line 3</p>"
		"</blockquote>" HTML_SUFFIX,
		"> short line 1\n"
		"> short line 2\n"
		"> short line 3")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_process_commands (fixture,
		"seq:C\n"
		"type:a\n"
		"seq:cD\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body><blockquote type=\"cite\">"
		"<p>short line 1</p>"
		"<p>short line 2</p>"
		"<p>short line 3</p>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<p>short line 1</p>"
		"<p>short line 2</p>"
		"<p>short line 3</p>"
		"</blockquote>" HTML_SUFFIX,
		"> short line 1\n"
		"> short line 2\n"
		"> short line 3")) {
		g_test_fail ();
		return;
	}
}

static void
test_cite_longline (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body><blockquote type=\"cite\">"
		"<p>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<p>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>"
		"</blockquote>" HTML_SUFFIX,
		"> This is the first paragraph of a quoted text which has some long\n"
		"> text to test. It has the second sentence as well.\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_process_commands (fixture,
		"seq:C\n"
		"type:a\n"
		"seq:cD\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body><blockquote type=\"cite\">"
		"<p>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<p>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>"
		"</blockquote>" HTML_SUFFIX,
		"> This is the first paragraph of a quoted text which has some long\n"
		"> text to test. It has the second sentence as well.\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body><blockquote type=\"cite\">"
		"<p>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>"
		"<p>This is the second paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>"
		"<p>This is the third paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>"
		"</blockquote><br>after quote</body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<p>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>"
		"<p>This is the second paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>"
		"<p>This is the third paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>"
		"</blockquote><br>after quote" HTML_SUFFIX,
		"> This is the first paragraph of a quoted text which has some long\n"
		"> text to test. It has the second sentence as well.\n"
		"> This is the econd paragraph of a quoted text which has some long\n"
		"> text to test. It has the second sentence as well.\n"
		"> This is the third paragraph of a quoted text which has some long\n"
		"> text to test. It has the second sentence as well.\n"
		"\nafter quote")) {
		g_test_fail ();
		return;
	}
}

static void
test_cite_reply_html (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<pre>line 1\n"
		"line 2\n"
		"</pre><span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<p>On Today, User wrote:</p>"
		"<blockquote type=\"cite\"><pre>line 1\n"
		"line 2\n"
		"</pre></blockquote>" HTML_SUFFIX,
		"On Today, Use wrote:\n"
		"> line 1\n"
		"> line 2\n"))
		g_test_fail ();

}

static void
test_cite_reply_plain (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<pre>line 1\n"
		"line 2\n"
		"</pre><span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">On Today, User wrote:</p>"
		"<blockquote type=\"cite\"><p style=\"width: 71ch;\">&gt; line 1</p>"
		"<p style=\"width: 71ch;\">&gt; line 2</p>"
		"<p style=\"width: 71ch;\">&gt; <br></p></blockquote>" HTML_SUFFIX,
		"On Today, Use wrote:\n"
		"> line 1\n"
		"> line 2\n"
		"> "))
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
test_undo_text_cut (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:some text to delete\n"
		"seq:CSllsc\n"
		"action:cut\n"
		"undo:undo\n",
		NULL,
		"some text to delete"))
		g_test_fail ();
}

static void
test_undo_style (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:The first paragraph text\\n\n"
		"undo:save\n" /* 1 */

		"action:bold\n"
		"type:bold\n"
		"undo:save\n" /* 2 */
		"undo:undo:4\n"
		"undo:test:2\n"
		"undo:redo:4\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:4\n"
		"type:bold\n"
		"seq:CSlsc\n"
		"action:bold\n"
		"undo:save:\n" /* 2 */
		"undo:undo:4\n"
		"undo:test:2\n"
		"undo:redo:4\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:4\n"

		"action:italic\n"
		"type:italic\n"
		"undo:save\n" /* 2 */
		"undo:undo:6\n"
		"undo:test:2\n"
		"undo:redo:6\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:6\n"
		"type:italic\n"
		"seq:CSlsc\n"
		"action:italic\n"
		"undo:save:\n" /* 2 */
		"undo:undo:6\n"
		"undo:test:2\n"
		"undo:redo:6\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:6\n"

		"action:underline\n"
		"type:underline\n"
		"undo:save\n" /* 2 */
		"undo:undo:9\n"
		"undo:test:2\n"
		"undo:redo:9\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:9\n"
		"type:italic\n"
		"seq:CSlsc\n"
		"action:underline\n"
		"undo:save:\n" /* 2 */
		"undo:undo:9\n"
		"undo:test:2\n"
		"undo:redo:9\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:9\n"

		"action:strikethrough\n"
		"type:strikethrough\n"
		"undo:save\n" /* 2 */
		"undo:undo:13\n"
		"undo:test:2\n"
		"undo:redo:13\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:13\n"
		"type:strikethrough\n"
		"seq:CSlsc\n"
		"action:strikethrough\n"
		"undo:save:\n" /* 2 */
		"undo:undo:13\n"
		"undo:test:2\n"
		"undo:redo:13\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:13\n"

		"action:monospaced\n"
		"type:monospaced\n"
		"undo:save\n" /* 2 */
		"undo:undo:10\n"
		"undo:test:2\n"
		"undo:redo:10\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:10\n"
		"type:monospaced\n"
		"seq:CSlsc\n"
		"action:monospaced\n"
		"undo:save:\n" /* 2 */
		"undo:undo:10\n"
		"undo:test:2\n"
		"undo:redo:10\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:10\n",
		HTML_PREFIX "<p>The first paragraph</p><p><br></p>" HTML_SUFFIX,
		"The first paragraph\n"))
		g_test_fail ();
}

static void
test_undo_justify (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:The first paragraph text\\n\n"
		"undo:save\n" /* 1 */

		"action:justify-left\n"
		"type:left\n"
		"undo:save\n" /* 2 */
		"undo:undo:4\n"
		"undo:test:2\n"
		"undo:redo:4\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:4\n"
		"type:left\n"
		"seq:CSlsc\n"
		"action:justify-left\n"
		"undo:save:\n" /* 2 */
		"undo:undo:4\n"
		"undo:test:2\n"
		"undo:redo:4\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:4\n"

		"action:justify-center\n"
		"type:center\n"
		"undo:save\n" /* 2 */
		"undo:undo:6\n"
		"undo:test:2\n"
		"undo:redo:6\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:6\n"
		"type:center\n"
		"seq:CSlsc\n"
		"action:justify-center\n"
		"undo:save:\n" /* 2 */
		"undo:undo:6\n"
		"undo:test:2\n"
		"undo:redo:6\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:6\n"

		"action:justify-right\n"
		"type:right\n"
		"undo:save\n" /* 2 */
		"undo:undo:5\n"
		"undo:test:2\n"
		"undo:redo:5\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:5\n"
		"type:right\n"
		"seq:CSlsc\n"
		"action:justify-right\n"
		"undo:save:\n" /* 2 */
		"undo:undo:5\n"
		"undo:test:2\n"
		"undo:redo:5\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:5\n",

		HTML_PREFIX "<p>The first paragraph</p><p><br></p>" HTML_SUFFIX,
		"The first paragraph\n"))
		g_test_fail ();
}

static void
test_undo_indent (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:The first paragraph text\\n\n"
		"undo:save\n" /* 1 */

		"action:indent\n"
		"type:text\n"
		"undo:save\n" /* 2 */
		"undo:undo:5\n"
		"undo:test:2\n"
		"undo:redo:5\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:5\n"
		"type:text\n"
		"seq:CSlsc\n"
		"action:indent\n"
		"undo:save:\n" /* 2 */
		"undo:undo:5\n"
		"undo:test:2\n"
		"undo:redo:5\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:5\n"

		"type:text\n"
		"undo:save\n" /* 2 */
		"action:indent\n"
		"undo:save\n" /* 3 */
		"action:unindent\n"
		"undo:test:2\n"
		"action:indent\n"
		"undo:test\n"
		"undo:save\n" /* 4 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:drop:2\n" /* drop the save 4 and 3 */
		"undo:undo:3\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:4\n"
		"undo:test\n"

		"type:level 1\\n\n"
		"type:level 2\\n\n"
		"type:level 3\\n\n"
		"seq:uuu\n"
		"action:indent\n"
		"undo:save\n" /* 2 */
		"seq:d\n"
		"action:indent\n"
		"action:indent\n"
		"undo:save\n" /* 3 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:drop:2\n" /* drop the save 3 and 2 */
		"seq:d\n"

		"action:indent\n"
		"undo:save\n" /* 2 */
		"action:indent\n"
		"action:indent\n"
		"undo:save\n" /* 3 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:drop:2\n" /* drop the save 3 and 2 */

		"undo:save\n" /* 2 */
		"undo:undo:30\n" /* 6x action:indent, 24x type "level X\\n" */
		"undo:test:2\n"
		"undo:redo:30\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:30\n",

		HTML_PREFIX "<p>The first paragraph</p><p><br></p>" HTML_SUFFIX,
		"The first paragraph\n"))
		g_test_fail ();
}

static void
test_undo_link_paste_html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("http://www.gnome.org", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:URL:\\n\n"
		"undo:save\n" /* 1 */
		"action:paste\n"
		"type:\\n\n"
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:undo:5\n"
		"undo:redo:7\n"
		"undo:test\n",
		HTML_PREFIX "<p>URL:</p><p><a href=\"http://www.gnome.org\">http://www.gnome.org</a></p><p><br></p>" HTML_SUFFIX,
		"URL:\nhttp://www.gnome.org\n"))
		g_test_fail ();
}

static void
test_undo_link_paste_plain (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("http://www.gnome.org", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:URL:\\n\n"
		"undo:save\n" /* 1 */
		"action:paste\n"
		"type:\\n\n"
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:undo:5\n"
		"undo:redo:7\n"
		"undo:test\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">URL:</p>"
		"<p style=\"width: 71ch;\"><a href=\"http://www.gnome.org\">http://www.gnome.org</a></p>"
		"<p style=\"width: 71ch;\"><br></p>" HTML_SUFFIX,
		"URL:\nhttp://www.gnome.org\n"))
		g_test_fail ();
}

static void
test_bug_726548 (TestFixture *fixture)
{
	gboolean success;
	gchar *text;
	const gchar *expected_plain =
		"aaa\n"
		"   1. a\n"
		"   2. b\n"
		"   3. c\n";

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:aaa\\n\n"
		"action:style-list-number\n"
		"type:a\\nb\\nc\\n\\n\n"
		"seq:C\n"
		"type:ac\n"
		"seq:c\n",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">aaa</p>"
		"<ol data-evo-paragraph=\"\" style=\"width: 65ch;\">"
		"<li>a</li><li>b</li><li>c</li></ol>"
		"<p style=\"width: 71ch;\"><br></p>" HTML_SUFFIX,
		expected_plain)) {
		g_test_fail ();
		return;
	}

	text = test_utils_get_clipboard_text (FALSE);
	success = test_utils_html_equal (fixture, text, expected_plain);

	if (!success) {
		g_warning ("%s: clipboard Plain text \n---%s---\n does not match expected Plain\n---%s---",
			G_STRFUNC, text, expected_plain);
		g_free (text);
		g_test_fail ();
	} else {
		g_free (text);
	}
}

static void
test_bug_750657 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body>\n"
		"<blockquote type=\"cite\">\n"
		"<p>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>\n"
		"<p><br></p>\n"
		"<p>This is the third paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>\n"
		"<blockquote type=\"cite\">\n"
		"<p>This is the first paragraph of a sub-quoted text which has some long text to test. It has the second sentence as well.</p>\n"
		"<br>\n"
		"</blockquote>\n"
		"<p>This is the fourth paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>\n"
		"</blockquote>\n"
		"<p><br></p>\n"
		"</body></html>\n",
		E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:uuuSuusD\n",
		HTML_PREFIX "\n"
		"<blockquote type=\"cite\">\n"
		"<p>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>\n"
		"<p><br></p>\n"
		"<p>This is the third paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>\n"
		"<p>This is the fourth paragraph of a quoted text which has some long text to test. It has the second sentence as well.</p>\n"
		"</blockquote>\n"
		"<p><br></p>\n"
		HTML_SUFFIX,
		NULL)) {
		g_test_fail ();
		return;
	}
}

static void
test_bug_760989 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n"
		"type:a\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body>\n"
		"One line before quotation<br>\n"
		"<blockquote type=\"cite\">\n"
		"<p>Single line quoted.</p>\n"
		"</blockquote>\n"
		"</body></html>",
		E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:ChcD\n",
		HTML_PREFIX "<p>One line before quotation</p>\n"
		"<blockquote type=\"cite\">\n"
		"<p>Single line quoted.</p>\n"
		"</blockquote>" HTML_SUFFIX,
		"One line before quotation\n"
		"> Single line quoted.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:Cecb\n",
		HTML_PREFIX "<p>One line before quotation</p>\n"
		"<blockquote type=\"cite\">\n"
		"<p>Single line quoted</p>\n"
		"</blockquote>" HTML_SUFFIX,
		"One line before quotation\n"
		"> Single line quoted")) {
		g_test_fail ();
		return;
	}
}

static void
test_bug_769708 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head><style id=\"-x-evo-quote-style\" type=\"text/css\">.-x-evo-quoted { -webkit-user-select: none; }</style>"
		"<style id=\"-x-evo-style-a\" type=\"text/css\">a { cursor: text; }</style></head>"
		"<body data-evo-plain-text=\"\" spellcheck=\"true\">"
		"<p data-evo-paragraph=\"\" class=\"\" id=\"-x-evo-input-start\">aaa</p>"
		"<div class=\"-x-evo-signature-wrapper\"><span class=\"-x-evo-signature\" id=\"autogenerated\"><pre>-- <br></pre>"
		"<p data-evo-paragraph=\"\" class=\"\">user &lt;user@no.where&gt;</p>"
		"</span></div></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX_PLAIN "<p style=\"width: 71ch;\">aaa</p><div><span><p style=\"width: 71ch;\">--&nbsp;</p>"
		"<p style=\"width: 71ch;\"<br></p><p style=\"width: 71ch;\">user &lt;user@no.where&gt;</p></span></div>" HTML_SUFFIX,
		"aaa\n"
		"-- \n"
		"user <user@no.where>"))
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
	add_test ("/style/strikethrough/selection", test_style_strikethrough_selection);
	add_test ("/style/strikethrough/typed", test_style_strikethrough_typed);
	add_test ("/style/monospace/selection", test_style_monospace_selection);
	add_test ("/style/monospace/typed", test_style_monospace_typed);
	add_test ("/justify/selection", test_justify_selection);
	add_test ("/justify/typed", test_justify_typed);
	add_test ("/indent/selection", test_indent_selection);
	add_test ("/indent/typed", test_indent_typed);
	add_test ("/font/size/selection", test_font_size_selection);
	add_test ("/font/size/typed", test_font_size_typed);
	add_test ("/font/color/selection", test_font_color_selection);
	add_test ("/font/color/typed", test_font_color_typed);
	add_test ("/list/bullet/plain", test_list_bullet_plain);
	add_test ("/list/bullet/html", test_list_bullet_html);
	add_test ("/list/bullet/html/from-block", test_list_bullet_html_from_block);
	add_test ("/list/alpha/html", test_list_alpha_html);
	add_test ("/list/alpha/plain", test_list_alpha_plain);
	add_test ("/list/roman/html", test_list_roman_html);
	add_test ("/list/roman/plain", test_list_roman_plain);
	add_test ("/list/multi/html", test_list_multi_html);
	add_test ("/list/multi/plain", test_list_multi_plain);
	add_test ("/list/multi/change/html", test_list_multi_change_html);
	add_test ("/list/multi/change/plain", test_list_multi_change_plain);
	add_test ("/link/insert/dialog", test_link_insert_dialog);
	add_test ("/link/insert/dialog/selection", test_link_insert_dialog_selection);
	add_test ("/link/insert/dialog/remove-link", test_link_insert_dialog_remove_link);
	add_test ("/link/insert/typed", test_link_insert_typed);
	add_test ("/link/insert/typed/change-description", test_link_insert_typed_change_description);
	add_test ("/link/insert/typed/append", test_link_insert_typed_append);
	add_test ("/link/insert/typed/remove", test_link_insert_typed_remove);
	add_test ("/h-rule/insert", test_h_rule_insert);
	add_test ("/h-rule/insert-text-after", test_h_rule_insert_text_after);
	add_test ("/emoticon/insert/typed", test_emoticon_insert_typed);
	add_test ("/emoticon/insert/typed-dash", test_emoticon_insert_typed_dash);
	add_test ("/paragraph/normal/selection", test_paragraph_normal_selection);
	add_test ("/paragraph/normal/typed", test_paragraph_normal_typed);
	add_test ("/paragraph/preformatted/selection", test_paragraph_preformatted_selection);
	add_test ("/paragraph/preformatted/typed", test_paragraph_preformatted_typed);
	add_test ("/paragraph/address/selection", test_paragraph_address_selection);
	add_test ("/paragraph/address/typed", test_paragraph_address_typed);
	add_test ("/paragraph/header1/selection", test_paragraph_header1_selection);
	add_test ("/paragraph/header1/typed", test_paragraph_header1_typed);
	add_test ("/paragraph/header2/selection", test_paragraph_header2_selection);
	add_test ("/paragraph/header2/typed", test_paragraph_header2_typed);
	add_test ("/paragraph/header3/selection", test_paragraph_header3_selection);
	add_test ("/paragraph/header3/typed", test_paragraph_header3_typed);
	add_test ("/paragraph/header4/selection", test_paragraph_header4_selection);
	add_test ("/paragraph/header4/typed", test_paragraph_header4_typed);
	add_test ("/paragraph/header5/selection", test_paragraph_header5_selection);
	add_test ("/paragraph/header5/typed", test_paragraph_header5_typed);
	add_test ("/paragraph/header6/selection", test_paragraph_header6_selection);
	add_test ("/paragraph/header6/typed", test_paragraph_header6_typed);
	add_test ("/paragraph/wrap-lines", test_paragraph_wrap_lines);
	add_test ("/paste/singleline/html2html", test_paste_singleline_html2html);
	add_test ("/paste/singleline/html2plain", test_paste_singleline_html2plain);
	add_test ("/paste/singleline/plain2html", test_paste_singleline_plain2html);
	add_test ("/paste/singleline/plain2plain", test_paste_singleline_plain2plain);
	add_test ("/paste/multiline/html2html", test_paste_multiline_html2html);
	add_test ("/paste/multiline/html2plain", test_paste_multiline_html2plain);
	add_test ("/paste/multiline/div/html2html", test_paste_multiline_div_html2html);
	add_test ("/paste/multiline/div/html2plain", test_paste_multiline_div_html2plain);
	add_test ("/paste/multiline/p/html2html", test_paste_multiline_p_html2html);
	add_test ("/paste/multiline/p/html2plain", test_paste_multiline_p_html2plain);
	add_test ("/paste/multiline/plain2html", test_paste_multiline_plain2html);
	add_test ("/paste/multiline/plain2plain", test_paste_multiline_plain2plain);
	add_test ("/paste/quoted/singleline/html2html", test_paste_quoted_singleline_html2html);
	add_test ("/paste/quoted/singleline/html2plain", test_paste_quoted_singleline_html2plain);
	add_test ("/paste/quoted/singleline/plain2html", test_paste_quoted_singleline_plain2html);
	add_test ("/paste/quoted/singleline/plain2plain", test_paste_quoted_singleline_plain2plain);
	add_test ("/paste/quoted/multiline/html2html", test_paste_quoted_multiline_html2html);
	add_test ("/paste/quoted/multiline/html2plain", test_paste_quoted_multiline_html2plain);
	add_test ("/paste/quoted/multiline/plain2html", test_paste_quoted_multiline_plain2html);
	add_test ("/paste/quoted/multiline/plain2plain", test_paste_quoted_multiline_plain2plain);
	add_test ("/cite/html2plain", test_cite_html2plain);
	add_test ("/cite/shortline", test_cite_shortline);
	add_test ("/cite/longline", test_cite_longline);
	add_test ("/cite/reply/html", test_cite_reply_html);
	add_test ("/cite/reply/plain", test_cite_reply_plain);
	add_test ("/undo/text/typed", test_undo_text_typed);
	add_test ("/undo/text/forward-delete", test_undo_text_forward_delete);
	add_test ("/undo/text/backward-delete", test_undo_text_backward_delete);
	add_test ("/undo/text/cut", test_undo_text_cut);
	add_test ("/undo/style", test_undo_style);
	add_test ("/undo/justify", test_undo_justify);
	add_test ("/undo/indent", test_undo_indent);
	add_test ("/undo/link-paste/html", test_undo_link_paste_html);
	add_test ("/undo/link-paste/plain", test_undo_link_paste_plain);
	add_test ("/bug/726548", test_bug_726548);
	add_test ("/bug/750657", test_bug_750657);
	add_test ("/bug/760989", test_bug_760989);
	add_test ("/bug/769708", test_bug_769708);

	#undef add_test

	res = g_test_run ();

	e_util_cleanup_settings ();
	e_spell_checker_free_global_memory ();

	return res;
}
