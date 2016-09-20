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

#include <glib.h>
#include <glib/gstdio.h>

#include <locale.h>
#include <e-util/e-util.h>

#include "e-html-editor-private.h"
#include "test-html-editor-units-bugs.h"
#include "test-html-editor-units-utils.h"
#include "test-keyfile-settings-backend.h"

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
		HTML_PREFIX "<div>some <b>bold</b> text</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>some <b>bold</b> text</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>some <i>italic</i> text</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>some <i>italic</i> text</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>some <u>underline</u> text</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>some <u>underline</u> text</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>some <strike>strikethrough</strike> text</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>some <strike>strikethrough</strike> text</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>some <font face=\"monospace\" size=\"3\">monospace</font> text</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>some <font face=\"monospace\" size=\"3\">monospace</font> text</div>" HTML_SUFFIX,
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
			"<div style=\"text-align: center\">center</div>"
			"<div style=\"text-align: right\">right</div>"
			"<div>left</div><div><br></div>"
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
			"<div style=\"text-align: center\">center</div>"
			"<div style=\"text-align: right\">right</div>"
			"<div>left</div><div><br></div>"
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
			"<div>level 0</div>"
			"<div style=\"margin-left: 3ch;\">"
				"<div>level 1</div>"
				"<div style=\"margin-left: 3ch;\"><div>level 2</div></div>"
				"<div>level 1</div>"
			"</div><div><br></div>"
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
			"<div>level 0</div>"
			"<div style=\"margin-left: 3ch;\">"
				"<div>level 1</div>"
				"<div style=\"margin-left: 3ch;\"><div>level 2</div></div>"
				"<div>level 1</div>"
			"</div><div><br></div>"
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
		HTML_PREFIX "<div><font size=\"1\">FontM2</font> <font size=\"2\">FontM1</font> Font0 <font size=\"4\">FontP1</font> "
		"<font size=\"5\">FontP2</font> <font size=\"6\">FontP3</font> <font size=\"7\">FontP4</font></div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div><font size=\"1\">FontM2</font> <font size=\"2\">FontM1</font> Font0 <font size=\"4\">FontP1</font> "
		"<font size=\"5\">FontP2</font> <font size=\"6\">FontP3</font> <font size=\"7\">FontP4</font><br></div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>default <font color=\"#ff0000\">red</font> <font color=\"#00ff00\">green</font> "
		"<font color=\"#0000ff\">blue</font></div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>default <font color=\"#ff0000\">red </font><font color=\"#00ff00\">green </font>"
		"<font color=\"#0000ff\">blue</font></div>" HTML_SUFFIX,
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
			"<div>text</div>"
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
			"<div>text</div>"
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
		HTML_PREFIX "<div>a link example: <a href=\"http://www.gnome.org\">http://www.gnome.org</a></div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>a link example: <a href=\"http://www.gnome.org\">GNOME</a></div>" HTML_SUFFIX,
		"a link example: GNOME"))
		g_test_fail ();
}

static void
test_link_insert_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:www.gnome.org \n",
		HTML_PREFIX "<div><a href=\"http://www.gnome.org\">www.gnome.org</a> </div>" HTML_SUFFIX,
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
		"seq:A\n" /* Alt+D to jump to the Description */
		"type:D\n"
		"seq:a\n"
		"type:GNOME\n"
		"seq:n\n",
		HTML_PREFIX "<div><a href=\"http://www.gnome.org\">GNOME</a> </div>" HTML_SUFFIX,
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
		"seq:A\n" /* Alt+R to press the 'Remove Link' */
		"type:R\n"
		"seq:a\n",
		HTML_PREFIX "<div>www.gnome.org </div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div><a href=\"http://www.gnome.org/about\">www.gnome.org/about</a> </div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div><a href=\"http://www.gnome.org\">www.gnome.o</a></div>" HTML_SUFFIX,
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
		"seq:^\n",  /* Escape key press to close the dialog */
		HTML_PREFIX "<div>text</div><hr align=\"left\" size=\"2\" noshade=\"\">" HTML_SUFFIX,
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
		HTML_PREFIX "<div>above</div><hr align=\"left\" size=\"2\" noshade=\"\"><div>below</div>" HTML_SUFFIX,
		"above\nbelow"))
		g_test_fail ();
}

static void
test_image_insert (TestFixture *fixture)
{
	EContentEditor *cnt_editor;
	gchar *expected_html;
	gchar *filename;
	gchar *uri;
	gchar *image_data;
	gchar *image_data_base64;
	gsize image_data_length;
	GError *error = NULL;

	if (!test_utils_process_commands (fixture,
		"mode:html\n"
		"type:before*\n")) {
		g_test_fail ();
		return;
	}

	filename = g_build_filename (EVOLUTION_TESTTOPSRCDIR, "data", "icons", "hicolor_actions_24x24_stock_people.png", NULL);
	uri = g_filename_to_uri (filename, NULL, &error);
	g_assert_no_error (error);

	g_file_get_contents (filename, &image_data, &image_data_length, &error);
	g_assert_no_error (error);

	g_free (filename);

	/* Mimic what the action:insert-image does, without invoking the image chooser dialog */
	cnt_editor = e_html_editor_get_content_editor (fixture->editor);
	e_content_editor_insert_image (cnt_editor, uri);

	g_free (uri);

	image_data_base64 = g_base64_encode ((const guchar *) image_data, image_data_length);
	g_return_if_fail (image_data_base64 != NULL);

	expected_html = g_strconcat (HTML_PREFIX "<div>before*<img src=\"data:image/png;base64,",
		image_data_base64, "\">+after</div>" HTML_SUFFIX, NULL);

	g_free (image_data_base64);
	g_free (image_data);

	if (!test_utils_run_simple_test (fixture,
		"type:+after\n",
		expected_html,
		"before*+after"))
		g_test_fail ();

	g_free (expected_html);
}

static void
test_emoticon_insert_typed (TestFixture *fixture)
{
	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-magic-smileys", TRUE);
	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-unicode-smileys", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:before :)after\n",
		HTML_PREFIX "<div>before <img src=\"data:image/png;base64,"
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
		"after</div>" HTML_SUFFIX,
		"before :-)after"))
		g_test_fail ();
}

static void
test_emoticon_insert_typed_dash (TestFixture *fixture)
{
	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-magic-smileys", TRUE);
	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-unicode-smileys", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:before :-)after\n",
		HTML_PREFIX "<div>before <img src=\"data:image/png;base64,"
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
		"after</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>"
		"<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n",
		HTML_PREFIX_PLAIN "<div style=\"width: 71ch;\">Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>"
		"<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n",
		HTML_PREFIX "<div>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>"
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
		HTML_PREFIX "<div>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>"
		"<div>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n",
		HTML_PREFIX_PLAIN "<div style=\"width: 71ch;\">Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>"
		"<div style=\"width: 71ch;\">Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n",
		HTML_PREFIX "<div>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>"
		"<div>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>" HTML_SUFFIX,
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
		"<div>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n",
		HTML_PREFIX_PLAIN "<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>"
		"<div style=\"width: 71ch;\">Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n",
		HTML_PREFIX "<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>"
		"<div>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>normal text</div>"
		"<address>address line 1</address>"
		"<address>address line 2</address>"
		"<address>address line 3</address>"
		"<div><br></div>"
		"<div>normal text</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>normal text</div>"
		"<address>address line 1</address>"
		"<address>address line 2</address>"
		"<address>address line 3</address>"
		"<div><br></div>"
		"<div>normal text</div>" HTML_SUFFIX,
		"normal text\n"
		"address line 1\n"
		"address line 2\n"
		"address line 3\n"
		"\n"
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
		HTML_PREFIX "<div>normal text</div>"
		"<h%d>header %d</h%d>"
		"<div>normal text</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>normal text</div>"
		"<h%d><br></h%d>"
		"<h%d>header %d</h%d>"
		"<div>normal text</div>" HTML_SUFFIX,
		header_n, header_n, header_n, header_n, header_n);

	expected_plain = g_strdup_printf (
		"normal text\n"
		"\n"
		"header %d\n"
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
		HTML_PREFIX "<div>normal text</div>"
		"<h%d>header %d</h%d>"
		"<div>normal text</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>normal text</div>"
		"<h%d>header %d</h%d>"
		"<div><br></div>"
		"<div>normal text</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec<br>odio. Praesent libero.</div>"
		"<div>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec<br>odio. Praesent libero.</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>text before some <b>bold</b> text text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX_PLAIN "<div style=\"width: 71ch;\">text before some bold text text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>text before some plain text text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX_PLAIN "<div style=\"width: 71ch;\">text before some plain text text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>text before <b>bold</b> text</div><div><i>italic</i> text</div><div><u>underline</u> text</div><div>text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX_PLAIN "<div style=\"width: 71ch;\">text before bold text</div>"
		"<div style=\"width: 71ch;\">italic text</div>"
		"<div style=\"width: 71ch;\">underline text</div>"
		"<div style=\"width: 71ch;\">text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>text before <b>bold</b> text</div><div><i>italic</i> text</div><div><u>underline</u> text</div><div>text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX_PLAIN "<div style=\"width: 71ch;\">text before bold text</div>"
		"<div style=\"width: 71ch;\">italic text</div>"
		"<div style=\"width: 71ch;\">underline text</div>"
		"<div style=\"width: 71ch;\">text after</div>" HTML_SUFFIX,
		"text before bold text\nitalic text\nunderline text\ntext after"))
		g_test_fail ();
}

static void
test_paste_multiline_p_html2html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body><div><b>bold</b> text</div><div><i>italic</i> text</div><div><u>underline</u> text</div><div></div></body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text before \n"
		"action:paste\n"
		"type:text after\n",
		HTML_PREFIX "<div>text before <b>bold</b> text</div><div><i>italic</i> text</div><div><u>underline</u> text</div><div>text after</div>" HTML_SUFFIX,
		"text before bold text\nitalic text\nunderline text\ntext after"))
		g_test_fail ();
}

static void
test_paste_multiline_p_html2plain (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body><div><b>bold</b> text</div><div><i>italic</i> text</div><div><u>underline</u> text</div></body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:text before \n"
		"action:paste\n"
		"type:\\ntext after\n",
		HTML_PREFIX_PLAIN "<div style=\"width: 71ch;\">text before bold text</div>"
		"<div style=\"width: 71ch;\">italic text</div>"
		"<div style=\"width: 71ch;\">underline text</div>"
		"<div style=\"width: 71ch;\">text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>text before line 1</div><div>line 2</div><div>line 3</div><div>text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX_PLAIN "<div style=\"width: 71ch;\">text before line 1</div>"
		"<div style=\"width: 71ch;\">line 2</div>"
		"<div style=\"width: 71ch;\">line 3</div>"
		"<div style=\"width: 71ch;\">text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>text before </div>"
		"<blockquote type=\"cite\"><div>some <b>bold</b> text</div></blockquote>"
		"<div>text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX_PLAIN "<div style=\"width: 71ch;\">text before </div>"
		"<blockquote type=\"cite\"><div style=\"width: 71ch;\">&gt; some <b>bold</b> text</div></blockquote>"
		"<div style=\"width: 71ch;\">text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>text before </div>"
		"<blockquote type=\"cite\"><div>some plain text</div></blockquote>"
		"<div>text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX_PLAIN "<div style=\"width: 71ch;\">text before </div>"
		"<blockquote type=\"cite\"><div style=\"width: 71ch;\">&gt; some plain text</div></blockquote>"
		"<div style=\"width: 71ch;\">text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>text before </div>"
		"<blockquote type=\"cite\">&gt; <b>bold</b> text</div>"
		"<div>&gt; <i>italic</i> text</div>"
		"<div>&gt; <u>underline</u> text</div></blockquote>"
		"<div>text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX_PLAIN "<div style=\"width: 71ch;\">text before </div>"
		"<blockquote type=\"cite\"><div>&gt; bold text</div>"
		"<div style=\"width: 71ch;\">&gt; italic text</div>"
		"<div style=\"width: 71ch;\">&gt; underline text</div></blockquote>"
		"<div style=\"width: 71ch;\">&gt; text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>text before </div>"
		"<blockquote type=\"cite\"><div>line 1</div>"
		"<div>line 2</div>"
		"<div>line 3</div></blockquote>"
		"<div>text after</div>" HTML_SUFFIX,
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
		HTML_PREFIX_PLAIN "<div style=\"width: 71ch;\">text before </div>"
		"<blockquote type=\"cite\"><div style=\"width: 71ch;\">&gt; line 1</div>"
		"<div style=\"width: 71ch;\">&gt; line 2</div>"
		"<div style=\"width: 71ch;\">&gt; line 3</div></blockquote>"
		"<div style=\"width: 71ch;\">text after</div>" HTML_SUFFIX,
		"text before \n"
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
		"<div data-evo-paragraph=\"\">level 1</div>"
		"<div data-evo-paragraph=\"\"><br></div>"
		"<div data-evo-paragraph=\"\">level 1</div>"
		"<blockquote type=\"cite\">"
		"<div data-evo-paragraph=\"\">level 2</div>"
		"</blockquote>"
		"<div data-evo-paragraph=\"\">back in level 1</div>"
		"</blockquote>"
		"<div data-evo-paragraph=\"\"><br></div>"
		"<div data-evo-paragraph=\"\">out of the citation</div>"
		"</body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	/* Just check the content was read properly */
	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\"><div>level 1</div><div><br></div><div>level 1</div>"
		"<blockquote type=\"cite\"><div>level 2</div></blockquote><div>back in level 1</div></blockquote>"
		"<div><br></div><div>out of the citation</div>" HTML_SUFFIX,
		"> level 1\n"
		"> \n"
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
		NULL,
		"> level 1\n"
		"> \n"
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
		"<div>Just one short line.</div>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<div>Just one short line.</div>"
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
		"<div>Just one short line.</div>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<div>Just one short line.</div>"
		"</blockquote>" HTML_SUFFIX,
		"> Just one short line.")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body><blockquote type=\"cite\">"
		"<div>short line 1</div>"
		"<div>short line 2</div>"
		"<div>short line 3</div>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<div>short line 1</div>"
		"<div>short line 2</div>"
		"<div>short line 3</div>"
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
		"<div>short line 1</div>"
		"<div>short line 2</div>"
		"<div>short line 3</div>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<div>short line 1</div>"
		"<div>short line 2</div>"
		"<div>short line 3</div>"
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
		"<div>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<div>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"</blockquote>" HTML_SUFFIX,
		"> This is the first paragraph of a quoted text which has some long text\n"
		"> to test. It has the second sentence as well.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_process_commands (fixture,
		"seq:C\n"
		"type:a\n"
		"seq:cDb\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body><blockquote type=\"cite\">"
		"<div>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_TEXT_HTML | E_CONTENT_EDITOR_INSERT_REPLACE_ALL);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<div>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"</blockquote>" HTML_SUFFIX,
		"> This is the first paragraph of a quoted text which has some long text\n"
		"> to test. It has the second sentence as well.")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body><blockquote type=\"cite\">"
		"<div>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"<div>This is the second paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"<div>This is the third paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"</blockquote><br>after quote</body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<div>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"<div>This is the second paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"<div>This is the third paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"</blockquote><br>after quote" HTML_SUFFIX,
		"> This is the first paragraph of a quoted text which has some long text\n"
		"> to test. It has the second sentence as well.\n"
		"> This is the second paragraph of a quoted text which has some long\n"
		"> text to test. It has the second sentence as well.\n"
		"> This is the third paragraph of a quoted text which has some long text\n"
		"> to test. It has the second sentence as well.\n"
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
		HTML_PREFIX "<div>On Today, User wrote:</div>"
		"<blockquote type=\"cite\"><pre>line 1\n"
		"line 2\n"
		"</pre></blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> line 1\n"
		"> line 2\n"
		"> "))
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
		"line 2\n\n"
		"</pre><span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX_PLAIN "<div style=\"width: 71ch;\">On Today, User wrote:</div>"
		"<blockquote type=\"cite\"><div style=\"width: 71ch;\">&gt; line 1</div>"
		"<div style=\"width: 71ch;\">&gt; line 2</div>"
		"<div style=\"width: 71ch;\">&gt; <br></div></blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
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
		HTML_PREFIX "<div>some text</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>some text to delete</div>" HTML_SUFFIX,
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
		HTML_PREFIX "<div>some text to delete</div>" HTML_SUFFIX,
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
		"undo:undo:5\n"
		"undo:test:2\n"
		"undo:redo:5\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:5\n"
		"type:bold\n"
		"seq:CSlsc\n"
		"action:bold\n"
		"undo:save\n" /* 2 */
		"undo:undo:5\n"
		"undo:test:2\n"
		"undo:redo:5\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:5\n"

		"action:italic\n"
		"type:italic\n"
		"undo:save\n" /* 2 */
		"undo:undo:7\n"
		"undo:test:2\n"
		"undo:redo:7\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:7\n"
		"type:italic\n"
		"seq:CSlsc\n"
		"action:italic\n"
		"undo:save\n" /* 2 */
		"undo:undo:7\n"
		"undo:test:2\n"
		"undo:redo:7\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:7\n"

		"action:underline\n"
		"type:underline\n"
		"undo:save\n" /* 2 */
		"undo:undo:10\n"
		"undo:test:2\n"
		"undo:redo:10\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:10\n"
		"type:underline\n"
		"seq:CSlsc\n"
		"action:underline\n"
		"undo:save\n" /* 2 */
		"undo:undo:10\n"
		"undo:test:2\n"
		"undo:redo:10\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:10\n"

		"action:strikethrough\n"
		"type:strikethrough\n"
		"undo:save\n" /* 2 */
		"undo:undo:14\n"
		"undo:test:2\n"
		"undo:redo:14\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:14\n"
		"type:strikethrough\n"
		"seq:CSlsc\n"
		"action:strikethrough\n"
		"undo:save\n" /* 2 */
		"undo:undo:14\n"
		"undo:test:2\n"
		"undo:redo:14\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:14\n"

		"action:monospaced\n"
		"type:monospaced\n"
		"undo:save\n" /* 2 */
		"undo:undo:11\n"
		"undo:test:2\n"
		"undo:redo:11\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:11\n"
		"type:monospaced\n"
		"seq:CSlsc\n"
		"action:monospaced\n"
		"undo:save\n" /* 2 */
		"undo:undo:11\n"
		"undo:test:2\n"
		"undo:redo:11\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:11\n",
		HTML_PREFIX "<div>The first paragraph text</div><div><br></div>" HTML_SUFFIX,
		"The first paragraph text\n"))
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
		"undo:save\n" /* 2 */
		"undo:undo:4\n"
		"undo:test:2\n"
		"undo:redo:4\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:4\n"

		"action:justify-center\n"
		"type:center\n"
		"undo:save\n" /* 2 */
		"undo:undo:7\n"
		"undo:test:2\n"
		"undo:redo:7\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:6\n"
		"type:center\n"
		"seq:CSlsc\n"
		"action:justify-center\n"
		"undo:save\n" /* 2 */
		"undo:undo:7\n"
		"undo:test:2\n"
		"undo:redo:7\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:7\n"

		"action:justify-right\n"
		"type:right\n"
		"undo:save\n" /* 2 */
		"undo:undo:6\n"
		"undo:test:2\n"
		"undo:redo:6\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:6\n"
		"type:right\n"
		"seq:CSlsc\n"
		"action:justify-right\n"
		"undo:save\n" /* 2 */
		"undo:undo:6\n"
		"undo:test:2\n"
		"undo:redo:6\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:6\n",

		HTML_PREFIX "<div>The first paragraph text</div><div><br></div>" HTML_SUFFIX,
		"The first paragraph text\n"))
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
		"undo:save\n" /* 2 */
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

		HTML_PREFIX "<div>The first paragraph text</div><div><br></div>" HTML_SUFFIX,
		"The first paragraph text\n"))
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
		HTML_PREFIX "<div>URL:</div><div><a href=\"http://www.gnome.org\">http://www.gnome.org</a></div><div><br></div>" HTML_SUFFIX,
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
		HTML_PREFIX_PLAIN "<div style=\"width: 71ch;\">URL:</div>"
		"<div style=\"width: 71ch;\"><a href=\"http://www.gnome.org\">http://www.gnome.org</a></div>"
		"<div style=\"width: 71ch;\"><br></div>" HTML_SUFFIX,
		"URL:\nhttp://www.gnome.org\n"))
		g_test_fail ();
}

gint
main (gint argc,
      gchar *argv[])
{
	gchar *test_keyfile_filename;
	gint cmd_delay = -1;
	gboolean background = FALSE;
	GOptionEntry entries[] = {
		{ "cmd-delay", '\0', 0,
		  G_OPTION_ARG_INT, &cmd_delay,
		  "Specify delay, in milliseconds, to use during processing commands. Default is 25 ms.",
		  NULL },
		{ "background", '\0', 0,
		  G_OPTION_ARG_NONE, &background,
		  "Use to run tests in the background, not stealing focus and such.",
		  NULL },
		{ NULL }
	};
	GOptionContext *context;
	GError *error = NULL;
	GList *modules;
	gint res;

	setlocale (LC_ALL, "");

	test_keyfile_filename = e_mktemp ("evolution-XXXXXX.settings");
	g_return_val_if_fail (test_keyfile_filename != NULL, -1);

	/* Start with clean settings file, to run with default settings. */
	g_unlink (test_keyfile_filename);

	/* Force the Evolution's test-keyfile GSettings backend, to not overwrite
	   user settings when playing with them. */
	g_setenv ("GIO_EXTRA_MODULES", EVOLUTION_TESTGIOMODULESDIR, TRUE);
	g_setenv ("GSETTINGS_BACKEND", TEST_KEYFILE_SETTINGS_BACKEND_NAME, TRUE);
	g_setenv (TEST_KEYFILE_SETTINGS_FILENAME_ENVVAR, test_keyfile_filename, TRUE);

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
	test_utils_set_background (background);

	e_util_init_main_thread (NULL);
	e_passwords_init ();

	modules = e_module_load_all_in_directory (EVOLUTION_MODULEDIR);
	g_list_free_full (modules, (GDestroyNotify) g_type_module_unuse);

	test_utils_add_test ("/create/editor", test_create_editor);
	test_utils_add_test ("/style/bold/selection", test_style_bold_selection);
	test_utils_add_test ("/style/bold/typed", test_style_bold_typed);
	test_utils_add_test ("/style/italic/selection", test_style_italic_selection);
	test_utils_add_test ("/style/italic/typed", test_style_italic_typed);
	test_utils_add_test ("/style/underline/selection", test_style_underline_selection);
	test_utils_add_test ("/style/underline/typed", test_style_underline_typed);
	test_utils_add_test ("/style/strikethrough/selection", test_style_strikethrough_selection);
	test_utils_add_test ("/style/strikethrough/typed", test_style_strikethrough_typed);
	test_utils_add_test ("/style/monospace/selection", test_style_monospace_selection);
	test_utils_add_test ("/style/monospace/typed", test_style_monospace_typed);
	test_utils_add_test ("/justify/selection", test_justify_selection);
	test_utils_add_test ("/justify/typed", test_justify_typed);
	test_utils_add_test ("/indent/selection", test_indent_selection);
	test_utils_add_test ("/indent/typed", test_indent_typed);
	test_utils_add_test ("/font/size/selection", test_font_size_selection);
	test_utils_add_test ("/font/size/typed", test_font_size_typed);
	test_utils_add_test ("/font/color/selection", test_font_color_selection);
	test_utils_add_test ("/font/color/typed", test_font_color_typed);
	test_utils_add_test ("/list/bullet/plain", test_list_bullet_plain);
	test_utils_add_test ("/list/bullet/html", test_list_bullet_html);
	test_utils_add_test ("/list/bullet/html/from-block", test_list_bullet_html_from_block);
	test_utils_add_test ("/list/alpha/html", test_list_alpha_html);
	test_utils_add_test ("/list/alpha/plain", test_list_alpha_plain);
	test_utils_add_test ("/list/roman/html", test_list_roman_html);
	test_utils_add_test ("/list/roman/plain", test_list_roman_plain);
	test_utils_add_test ("/list/multi/html", test_list_multi_html);
	test_utils_add_test ("/list/multi/plain", test_list_multi_plain);
	test_utils_add_test ("/list/multi/change/html", test_list_multi_change_html);
	test_utils_add_test ("/list/multi/change/plain", test_list_multi_change_plain);
	test_utils_add_test ("/link/insert/dialog", test_link_insert_dialog);
	test_utils_add_test ("/link/insert/dialog/selection", test_link_insert_dialog_selection);
	test_utils_add_test ("/link/insert/dialog/remove-link", test_link_insert_dialog_remove_link);
	test_utils_add_test ("/link/insert/typed", test_link_insert_typed);
	test_utils_add_test ("/link/insert/typed/change-description", test_link_insert_typed_change_description);
	test_utils_add_test ("/link/insert/typed/append", test_link_insert_typed_append);
	test_utils_add_test ("/link/insert/typed/remove", test_link_insert_typed_remove);
	test_utils_add_test ("/h-rule/insert", test_h_rule_insert);
	test_utils_add_test ("/h-rule/insert-text-after", test_h_rule_insert_text_after);
	test_utils_add_test ("/image/insert", test_image_insert);
	test_utils_add_test ("/emoticon/insert/typed", test_emoticon_insert_typed);
	test_utils_add_test ("/emoticon/insert/typed-dash", test_emoticon_insert_typed_dash);
	test_utils_add_test ("/paragraph/normal/selection", test_paragraph_normal_selection);
	test_utils_add_test ("/paragraph/normal/typed", test_paragraph_normal_typed);
	test_utils_add_test ("/paragraph/preformatted/selection", test_paragraph_preformatted_selection);
	test_utils_add_test ("/paragraph/preformatted/typed", test_paragraph_preformatted_typed);
	test_utils_add_test ("/paragraph/address/selection", test_paragraph_address_selection);
	test_utils_add_test ("/paragraph/address/typed", test_paragraph_address_typed);
	test_utils_add_test ("/paragraph/header1/selection", test_paragraph_header1_selection);
	test_utils_add_test ("/paragraph/header1/typed", test_paragraph_header1_typed);
	test_utils_add_test ("/paragraph/header2/selection", test_paragraph_header2_selection);
	test_utils_add_test ("/paragraph/header2/typed", test_paragraph_header2_typed);
	test_utils_add_test ("/paragraph/header3/selection", test_paragraph_header3_selection);
	test_utils_add_test ("/paragraph/header3/typed", test_paragraph_header3_typed);
	test_utils_add_test ("/paragraph/header4/selection", test_paragraph_header4_selection);
	test_utils_add_test ("/paragraph/header4/typed", test_paragraph_header4_typed);
	test_utils_add_test ("/paragraph/header5/selection", test_paragraph_header5_selection);
	test_utils_add_test ("/paragraph/header5/typed", test_paragraph_header5_typed);
	test_utils_add_test ("/paragraph/header6/selection", test_paragraph_header6_selection);
	test_utils_add_test ("/paragraph/header6/typed", test_paragraph_header6_typed);
	test_utils_add_test ("/paragraph/wrap-lines", test_paragraph_wrap_lines);
	test_utils_add_test ("/paste/singleline/html2html", test_paste_singleline_html2html);
	test_utils_add_test ("/paste/singleline/html2plain", test_paste_singleline_html2plain);
	test_utils_add_test ("/paste/singleline/plain2html", test_paste_singleline_plain2html);
	test_utils_add_test ("/paste/singleline/plain2plain", test_paste_singleline_plain2plain);
	test_utils_add_test ("/paste/multiline/html2html", test_paste_multiline_html2html);
	test_utils_add_test ("/paste/multiline/html2plain", test_paste_multiline_html2plain);
	test_utils_add_test ("/paste/multiline/div/html2html", test_paste_multiline_div_html2html);
	test_utils_add_test ("/paste/multiline/div/html2plain", test_paste_multiline_div_html2plain);
	test_utils_add_test ("/paste/multiline/p/html2html", test_paste_multiline_p_html2html);
	test_utils_add_test ("/paste/multiline/p/html2plain", test_paste_multiline_p_html2plain);
	test_utils_add_test ("/paste/multiline/plain2html", test_paste_multiline_plain2html);
	test_utils_add_test ("/paste/multiline/plain2plain", test_paste_multiline_plain2plain);
	test_utils_add_test ("/paste/quoted/singleline/html2html", test_paste_quoted_singleline_html2html);
	test_utils_add_test ("/paste/quoted/singleline/html2plain", test_paste_quoted_singleline_html2plain);
	test_utils_add_test ("/paste/quoted/singleline/plain2html", test_paste_quoted_singleline_plain2html);
	test_utils_add_test ("/paste/quoted/singleline/plain2plain", test_paste_quoted_singleline_plain2plain);
	test_utils_add_test ("/paste/quoted/multiline/html2html", test_paste_quoted_multiline_html2html);
	test_utils_add_test ("/paste/quoted/multiline/html2plain", test_paste_quoted_multiline_html2plain);
	test_utils_add_test ("/paste/quoted/multiline/plain2html", test_paste_quoted_multiline_plain2html);
	test_utils_add_test ("/paste/quoted/multiline/plain2plain", test_paste_quoted_multiline_plain2plain);
	test_utils_add_test ("/cite/html2plain", test_cite_html2plain);
	test_utils_add_test ("/cite/shortline", test_cite_shortline);
	test_utils_add_test ("/cite/longline", test_cite_longline);
	test_utils_add_test ("/cite/reply/html", test_cite_reply_html);
	test_utils_add_test ("/cite/reply/plain", test_cite_reply_plain);
	test_utils_add_test ("/undo/text/typed", test_undo_text_typed);
	test_utils_add_test ("/undo/text/forward-delete", test_undo_text_forward_delete);
	test_utils_add_test ("/undo/text/backward-delete", test_undo_text_backward_delete);
	test_utils_add_test ("/undo/text/cut", test_undo_text_cut);
	test_utils_add_test ("/undo/style", test_undo_style);
	test_utils_add_test ("/undo/justify", test_undo_justify);
	test_utils_add_test ("/undo/indent", test_undo_indent);
	test_utils_add_test ("/undo/link-paste/html", test_undo_link_paste_html);
	test_utils_add_test ("/undo/link-paste/plain", test_undo_link_paste_plain);

	test_add_html_editor_bug_tests ();

	res = g_test_run ();

	e_util_cleanup_settings ();
	e_spell_checker_free_global_memory ();

	g_unlink (test_keyfile_filename);
	g_free (test_keyfile_filename);

	return res;
}
