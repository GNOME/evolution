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
	g_assert_true (fixture->editor != NULL);
	g_assert_cmpstr (e_html_editor_get_content_editor_name (fixture->editor), ==, DEFAULT_CONTENT_EDITOR_NAME);

	/* test of the test function */
	g_assert_true (test_utils_html_equal (fixture, "<span>a</span>", "<sPaN>a</spaN>"));
	g_assert_true (!test_utils_html_equal (fixture, "<span>A</span>", "<sPaN>a</spaN>"));
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
		"some bold text\n"))
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
		"some bold text\n"))
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
		"some italic text\n"))
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
		"some italic text\n"))
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
		"some underline text\n"))
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
		"some underline text\n"))
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
		"some strikethrough text\n"))
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
		"some strikethrough text\n"))
		g_test_fail ();
}

static void
test_style_monospace_selection (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some monospace text\n"
		"seq:hCrcrCSrsc\n"
		"font-name:monospace\n",
		HTML_PREFIX "<div>some <font face=\"monospace\">monospace</font> text</div>" HTML_SUFFIX,
		"some monospace text\n"))
		g_test_fail ();
}

static void
test_style_monospace_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:some \n"
		"font-name:monospace\n"
		"type:monospace\n"
		"font-name:\n"
		"type: text\n",
		HTML_PREFIX "<div>some <font face=\"monospace\">monospace</font> text</div>" HTML_SUFFIX,
		"some monospace text\n"))
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
			"<div style=\"text-align: center;\">center</div>"
			"<div style=\"text-align: right;\">right</div>"
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
			"<div style=\"text-align: center;\">center</div>"
			"<div style=\"text-align: right;\">right</div>"
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
			"<div style=\"margin-left: 3ch;\">level 1</div>"
			"<div style=\"margin-left: 6ch;\">level 2</div>"
			"<div style=\"margin-left: 3ch;\">level 1</div>"
			"<div><br></div>"
		HTML_SUFFIX,
		"level 0\n"
		"   level 1\n"
		"      level 2\n"
		"   level 1\n"))
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
			"<div style=\"margin-left: 3ch;\">level 1</div>"
			"<div style=\"margin-left: 6ch;\">level 2</div>"
			"<div style=\"margin-left: 3ch;\">level 1</div>"
			"<div><br></div>"
		HTML_SUFFIX,
		"level 0\n"
		"   level 1\n"
		"      level 2\n"
		"   level 1\n"))
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
		"FontM2 FontM1 Font0 FontP1 FontP2 FontP3 FontP4\n"))
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
		HTML_PREFIX "<div><font size=\"1\">FontM2</font><font size=\"3\"> </font><font size=\"2\">FontM1</font><font size=\"3\"> Font0 </font>"
		"<font size=\"4\">FontP1</font><font size=\"3\"> </font><font size=\"5\">FontP2</font><font size=\"3\"> </font>"
		"<font size=\"6\">FontP3</font><font size=\"3\"> </font><font size=\"7\">FontP4</font></div>" HTML_SUFFIX,
		"FontM2 FontM1 Font0 FontP1 FontP2 FontP3 FontP4\n"))
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
		"default red green blue\n"))
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
		"default red green blue\n"))
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
		"text\n"))
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
		"    - item 2\n"
		" * item 3\n"
		"text\n"))
		g_test_fail ();
}

static void
test_list_bullet_change (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"action:style-list-bullet\n"
		"action:style-list-number\n",
		NULL,
		"   1. \n"))
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
		" * \n"))
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
		"         A. item 2\n"
		"   B. item 3\n"
		"text\n"))
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
		"         A. item 2\n"
		"   B. item 3\n"
		"text\n"))
		g_test_fail ();
}

static void
test_list_number_html (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"action:style-list-number\n"
		"type:item 1\\n\n"
		"action:indent\n"
		"type:item 2\\n\n"
		"action:unindent\n"
		"type:item 3\\n\n"
		"type:\\n\n"
		"type:text\n",
		HTML_PREFIX
			"<ol>"
				"<li>item 1</li>"
				"<ol>"
					"<li>item 2</li>"
				"</ol>"
				"<li>item 3</li>"
			"</ol>"
			"<div>text</div>"
		HTML_SUFFIX,
		"   1. item 1\n"
		"         1. item 2\n"
		"   2. item 3\n"
		"text\n"))
		g_test_fail ();
}

static void
test_list_number_plain (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"action:style-list-number\n"
		"type:item 1\\n\n"
		"action:indent\n"
		"type:item 2\\n\n"
		"action:unindent\n"
		"type:item 3\\n\n"
		"type:\\n\n"
		"type:text\n",
		NULL,
		"   1. item 1\n"
		"         1. item 2\n"
		"   2. item 3\n"
		"text\n"))
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
		"    I. 1\n"
		"   II. 2\n"
		"  III. 3\n"
		"   IV. 4\n"
		"    V. 5\n"
		"   VI. 6\n"
		"  VII. 7\n"
		" VIII. 8\n"
		"   IX. 9\n"
		"    X. 10\n"
		"   XI. 11\n"
		"  XII. 12\n"
		" XIII. 13\n"
		"  XIV. 14\n"
		"   XV. 15\n"
		"  XVI. 16\n"
		" XVII. 17\n"
		"XVIII. 18\n"))
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
		"    I. 1\n"
		"   II. 2\n"
		"  III. 3\n"
		"   IV. 4\n"
		"    V. 5\n"
		"   VI. 6\n"
		"  VII. 7\n"
		" VIII. 8\n"
		"   IX. 9\n"
		"    X. 10\n"
		"   XI. 11\n"
		"  XII. 12\n"
		" XIII. 13\n"
		"  XIV. 14\n"
		"   XV. 15\n"
		"  XVI. 16\n"
		" XVII. 17\n"
		"XVIII. 18\n"))
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
		" III. \n"))
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
		" III. \n"))
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
		"   5. \n"))
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
		"   5. \n"))
		g_test_fail ();
}

static void
test_list_indent_same (TestFixture *fixture,
		       gboolean is_html)
{
	const gchar *unindented_html, *unindented_plain;

	unindented_html =
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX;

	unindented_plain =
		" * a\n"
		" * b\n"
		" * c\n"
		" * d\n";

	if (!test_utils_run_simple_test (fixture,
		"action:style-list-bullet\n"
		"type:a\\n\n"
		"type:b\\n\n"
		"type:c\\n\n"
		"type:d\\n\n"
		"seq:nb\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing all items */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:uuuSddds\n"
		"action:unindent\n"
		"action:style-preformat\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>a</pre>"
			"<pre>b</pre>"
			"<pre>c</pre>"
			"<pre>d</pre>"
		HTML_SUFFIX,
		"a\n"
		"b\n"
		"c\n"
		"d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:drop:1\n" /* 1 */
		"action:indent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<ul>"
					"<li>a</li>"
					"<li>b</li>"
					"<li>c</li>"
					"<li>d</li>"
				"</ul>"
			"</ul>"
		HTML_SUFFIX,
		"    - a\n"
		"    - b\n"
		"    - c\n"
		"    - d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:test:2\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:test:2\n"
		"undo:drop:2\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing first item, single select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:uuu\n"
		"action:unindent\n"
		"action:style-preformat\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>a</pre>"
			"<ul>"
				"<li>b</li>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX,
		"a\n"
		" * b\n"
		" * c\n"
		" * d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:drop:1\n" /* 1 */
		"action:indent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<ul><li>a</li></ul>"
				"<li>b</li>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX,
		"    - a\n"
		" * b\n"
		" * c\n"
		" * d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:test:2\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:test:2\n"
		"undo:drop:2\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing mid item, single select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:dh\n"
		"action:unindent\n"
		"action:style-preformat\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
			"</ul>"
			"<pre>b</pre>"
			"<ul>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX,
		" * a\n"
		"b\n"
		" * c\n"
		" * d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:drop:1\n" /* 1 */
		"action:indent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
				"<ul><li>b</li></ul>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX,
		" * a\n"
		"    - b\n"
		" * c\n"
		" * d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:test:2\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:test:2\n"
		"undo:drop:2\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing last item, single select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:dd\n"
		"action:unindent\n"
		"action:style-preformat\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
				"<li>c</li>"
			"</ul>"
			"<pre>d</pre>"
		HTML_SUFFIX,
		" * a\n"
		" * b\n"
		" * c\n"
		"d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:drop:1\n" /* 1 */
		"action:indent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
				"<li>c</li>"
				"<ul><li>d</li></ul>"
			"</ul>"
		HTML_SUFFIX,
		" * a\n"
		" * b\n"
		" * c\n"
		"    - d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:test:2\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:test:2\n"
		"undo:drop:2\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing first items, multi-select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:uuuhSdes\n"
		"action:unindent\n"
		"action:style-preformat\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>a</pre>"
			"<pre>b</pre>"
			"<ul>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX,
		"a\n"
		"b\n"
		" * c\n"
		" * d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:drop:1\n" /* 1 */
		"action:indent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<ul><li>a</li>"
				"<li>b</li></ul>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX,
		"    - a\n"
		"    - b\n"
		" * c\n"
		" * d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:test:2\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:test:2\n"
		"undo:drop:2\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing mid items, multi-select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:duhSdes\n"
		"action:unindent\n"
		"action:style-preformat\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
			"</ul>"
			"<pre>b</pre>"
			"<pre>c</pre>"
			"<ul>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX,
		" * a\n"
		"b\n"
		"c\n"
		" * d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:drop:1\n" /* 1 */
		"action:indent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
				"<ul><li>b</li>"
				"<li>c</li></ul>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX,
		" * a\n"
		"    - b\n"
		"    - c\n"
		" * d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:test:2\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:test:2\n"
		"undo:drop:2\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing last items, multi-select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:dhSues\n"
		"action:unindent\n"
		"action:style-preformat\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
			"</ul>"
			"<pre>c</pre>"
			"<pre>d</pre>"
		HTML_SUFFIX,
		" * a\n"
		" * b\n"
		"c\n"
		"d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:drop:1\n" /* 1 */
		"action:indent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
				"<ul><li>c</li>"
				"<li>d</li></ul>"
			"</ul>"
		HTML_SUFFIX,
		" * a\n"
		" * b\n"
		"    - c\n"
		"    - d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:test:2\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:test:2\n"
		"undo:drop:2\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* The same tests as above, only with added text above and below the list */

	unindented_html =
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
			"<pre>suffix</pre>"
		HTML_SUFFIX;

	unindented_plain =
		"prefix\n"
		" * a\n"
		" * b\n"
		" * c\n"
		" * d\n"
		"suffix\n";

	if (!test_utils_run_simple_test (fixture,
		"action:select-all\n"
		"seq:D\n"
		"action:style-preformat\n"
		"type:prefix\\n\n"
		"action:style-list-bullet\n"
		"type:a\\n\n"
		"type:b\\n\n"
		"type:c\\n\n"
		"type:d\\n\n"
		"seq:n\n"
		"action:style-preformat\n"
		"type:suffix\n"
		"seq:u\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing all items */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:uuuhSdddes\n"
		"action:unindent\n"
		"action:style-preformat\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<pre>a</pre>"
			"<pre>b</pre>"
			"<pre>c</pre>"
			"<pre>d</pre>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		"a\n"
		"b\n"
		"c\n"
		"d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:drop:1\n" /* 1 */
		"action:indent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ul>"
				"<ul>"
					"<li>a</li>"
					"<li>b</li>"
					"<li>c</li>"
					"<li>d</li>"
				"</ul>"
			"</ul>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		"    - a\n"
		"    - b\n"
		"    - c\n"
		"    - d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:test:2\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:test:2\n"
		"undo:drop:2\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing first item, single select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:uuu\n"
		"action:unindent\n"
		"action:style-preformat\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<pre>a</pre>"
			"<ul>"
				"<li>b</li>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		"a\n"
		" * b\n"
		" * c\n"
		" * d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:drop:1\n" /* 1 */
		"action:indent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ul>"
				"<ul><li>a</li></ul>"
				"<li>b</li>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		"    - a\n"
		" * b\n"
		" * c\n"
		" * d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:test:2\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:test:2\n"
		"undo:drop:2\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing mid item, single select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:dh\n"
		"action:unindent\n"
		"action:style-preformat\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ul>"
				"<li>a</li>"
			"</ul>"
			"<pre>b</pre>"
			"<ul>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		" * a\n"
		"b\n"
		" * c\n"
		" * d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:drop:1\n" /* 1 */
		"action:indent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ul>"
				"<li>a</li>"
				"<ul><li>b</li></ul>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		" * a\n"
		"    - b\n"
		" * c\n"
		" * d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:test:2\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:test:2\n"
		"undo:drop:2\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing last item, single select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:ddh\n"
		"action:unindent\n"
		"action:style-preformat\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
				"<li>c</li>"
			"</ul>"
			"<pre>d</pre>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		" * a\n"
		" * b\n"
		" * c\n"
		"d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:drop:1\n" /* 1 */
		"action:indent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
				"<li>c</li>"
				"<ul><li>d</li></ul>"
			"</ul>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		" * a\n"
		" * b\n"
		" * c\n"
		"    - d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:test:2\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:test:2\n"
		"undo:drop:2\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing first items, multi-select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:uuuhSdes\n"
		"action:unindent\n"
		"action:style-preformat\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<pre>a</pre>"
			"<pre>b</pre>"
			"<ul>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		"a\n"
		"b\n"
		" * c\n"
		" * d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:drop:1\n" /* 1 */
		"action:indent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ul>"
				"<ul><li>a</li>"
				"<li>b</li></ul>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		"    - a\n"
		"    - b\n"
		" * c\n"
		" * d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:test:2\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:test:2\n"
		"undo:drop:2\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing mid items, multi-select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:duhSdes\n"
		"action:unindent\n"
		"action:style-preformat\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ul>"
				"<li>a</li>"
			"</ul>"
			"<pre>b</pre>"
			"<pre>c</pre>"
			"<ul>"
				"<li>d</li>"
			"</ul>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		" * a\n"
		"b\n"
		"c\n"
		" * d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:drop:1\n" /* 1 */
		"action:indent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ul>"
				"<li>a</li>"
				"<ul><li>b</li>"
				"<li>c</li></ul>"
				"<li>d</li>"
			"</ul>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		" * a\n"
		"    - b\n"
		"    - c\n"
		" * d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:test:2\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:test:2\n"
		"undo:drop:2\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing last items, multi-select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:deSuhs\n"
		"action:unindent\n"
		"action:style-preformat\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
			"</ul>"
			"<pre>c</pre>"
			"<pre>d</pre>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		" * a\n"
		" * b\n"
		"c\n"
		"d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:drop:1\n" /* 1 */
		"action:indent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
				"<ul><li>c</li>"
				"<li>d</li></ul>"
			"</ul>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		" * a\n"
		" * b\n"
		"    - c\n"
		"    - d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:test:2\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:test:2\n",
		!is_html ? NULL : unindented_html, unindented_plain))
		g_test_fail ();
}

static void
test_list_indent_same_html (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture, "mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_list_indent_same (fixture, TRUE);
}

static void
test_list_indent_same_plain (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture, "mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_list_indent_same (fixture, FALSE);
}

static void
test_list_indent_different (TestFixture *fixture,
			    gboolean is_html)
{
	const gchar *unindented_html, *unindented_plain;

	unindented_html =
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX;

	unindented_plain =
		" * a\n"
		" * b\n"
		" * c\n"
		" * d\n";

	if (!test_utils_run_simple_test (fixture,
		"action:style-list-bullet\n"
		"type:a\\n\n"
		"type:b\\n\n"
		"type:c\\n\n"
		"type:d\\n\n"
		"seq:nb\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing first item, single select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:uuu\n"
		"action:indent\n"
		"action:style-list-number\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<ol><li>a</li></ol>"
				"<li>b</li>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX,
		"      1. a\n"
		" * b\n"
		" * c\n"
		" * d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ol><li>a</li></ol>"
			"<ul>"
				"<li>b</li>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX,
		"   1. a\n"
		" * b\n"
		" * c\n"
		" * d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 3 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:3\n"
		"undo:undo:3\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing mid item, single select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:d\n"
		"action:indent\n"
		"action:style-list-alpha\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
				"<ol type=\"A\"><li>b</li></ol>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX,
		" * a\n"
		"      A. b\n"
		" * c\n"
		" * d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
			"</ul>"
			"<ol type=\"A\"><li>b</li></ol>"
			"<ul>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX,
		" * a\n"
		"   A. b\n"
		" * c\n"
		" * d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 3 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:3\n"
		"undo:undo:3\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing last item, single select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:dd\n"
		"action:indent\n"
		"action:style-list-roman\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
				"<li>c</li>"
				"<ol type=\"I\"><li>d</li></ol>"
			"</ul>"
		HTML_SUFFIX,
		" * a\n"
		" * b\n"
		" * c\n"
		"      I. d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
				"<li>c</li>"
			"</ul>"
			"<ol type=\"I\"><li>d</li></ol>"
		HTML_SUFFIX,
		" * a\n"
		" * b\n"
		" * c\n"
		"   I. d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 3 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:3\n"
		"undo:undo:3\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing first items, multi-select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:uuuhSdes\n"
		"action:indent\n"
		"action:style-list-number\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<ol>"
					"<li>a</li>"
					"<li>b</li>"
				"</ol>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX,
		"      1. a\n"
		"      2. b\n"
		" * c\n"
		" * d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ol>"
				"<li>a</li>"
				"<li>b</li>"
			"</ol>"
			"<ul>"
				"<li>c</li>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX,
		"   1. a\n"
		"   2. b\n"
		" * c\n"
		" * d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 3 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:3\n"
		"undo:undo:3\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing mid items, multi-select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:duhSdes\n"
		"action:indent\n"
		"action:style-list-alpha\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
				"<ol type=\"A\">"
					"<li>b</li>"
					"<li>c</li>"
				"</ol>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX,
		" * a\n"
		"      A. b\n"
		"      B. c\n"
		" * d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
			"</ul>"
			"<ol type=\"A\">"
				"<li>b</li>"
				"<li>c</li>"
			"</ol>"
			"<ul>"
				"<li>d</li>"
			"</ul>"
		HTML_SUFFIX,
		" * a\n"
		"   A. b\n"
		"   B. c\n"
		" * d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 3 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:3\n"
		"undo:undo:3\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing last items, multi-select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:deSuhs\n"
		"action:indent\n"
		"action:style-list-roman\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
				"<ol type=\"I\">"
					"<li>c</li>"
					"<li>d</li>"
				"</ol>"
			"</ul>"
		HTML_SUFFIX,
		" * a\n"
		" * b\n"
		"      I. c\n"
		"     II. d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
			"</ul>"
			"<ol type=\"I\">"
				"<li>c</li>"
				"<li>d</li>"
			"</ol>"
		HTML_SUFFIX,
		" * a\n"
		" * b\n"
		"   I. c\n"
		"  II. d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 3 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:3\n"
		"undo:undo:3\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* The same tests as above, only with added text above and below the list */

	unindented_html =
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ol>"
				"<li>a</li>"
				"<li>b</li>"
				"<li>c</li>"
				"<li>d</li>"
			"</ol>"
			"<pre>suffix</pre>"
		HTML_SUFFIX;

	unindented_plain =
		"prefix\n"
		"   1. a\n"
		"   2. b\n"
		"   3. c\n"
		"   4. d\n"
		"suffix\n";

	if (!test_utils_run_simple_test (fixture,
		"action:select-all\n"
		"seq:D\n"
		"action:style-preformat\n"
		"type:prefix\\n\n"
		"action:style-list-number\n"
		"type:a\\n\n"
		"type:b\\n\n"
		"type:c\\n\n"
		"type:d\\n\n"
		"seq:n\n"
		"action:style-preformat\n"
		"type:suffix\n"
		"seq:ur\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing first item, single select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:uuu\n"
		"action:indent\n"
		"action:style-list-bullet\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ol>"
				"<ul><li>a</li></ul>"
				"<li>b</li>"
				"<li>c</li>"
				"<li>d</li>"
			"</ol>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		"       - a\n"
		"   1. b\n"
		"   2. c\n"
		"   3. d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ul><li>a</li></ul>"
			"<ol>"
				"<li>b</li>"
				"<li>c</li>"
				"<li>d</li>"
			"</ol>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		" * a\n"
		"   1. b\n"
		"   2. c\n"
		"   3. d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 3 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:3\n"
		"undo:undo:3\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing mid item, single select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:d\n"
		"action:indent\n"
		"action:style-list-alpha\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ol>"
				"<li>a</li>"
				"<ol type=\"A\"><li>b</li></ol>"
				"<li>c</li>"
				"<li>d</li>"
			"</ol>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		"   1. a\n"
		"         A. b\n"
		"   2. c\n"
		"   3. d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ol>"
				"<li>a</li>"
			"</ol>"
			"<ol type=\"A\"><li>b</li></ol>"
			"<ol>"
				"<li>c</li>"
				"<li>d</li>"
			"</ol>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		"   1. a\n"
		"   A. b\n"
		"   1. c\n"
		"   2. d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 3 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:3\n"
		"undo:undo:3\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing last item, single select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:dd\n"
		"action:indent\n"
		"action:style-list-roman\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ol>"
				"<li>a</li>"
				"<li>b</li>"
				"<li>c</li>"
				"<ol type=\"I\"><li>d</li></ol>"
			"</ol>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		"   1. a\n"
		"   2. b\n"
		"   3. c\n"
		"         I. d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ol>"
				"<li>a</li>"
				"<li>b</li>"
				"<li>c</li>"
			"</ol>"
			"<ol type=\"I\"><li>d</li></ol>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		"   1. a\n"
		"   2. b\n"
		"   3. c\n"
		"   I. d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 3 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:3\n"
		"undo:undo:3\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing first items, multi-select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:uuuhSdes\n"
		"action:indent\n"
		"action:style-list-bullet\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ol>"
				"<ul>"
					"<li>a</li>"
					"<li>b</li>"
				"</ul>"
				"<li>c</li>"
				"<li>d</li>"
			"</ol>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		"       - a\n"
		"       - b\n"
		"   1. c\n"
		"   2. d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
			"</ul>"
			"<ol>"
				"<li>c</li>"
				"<li>d</li>"
			"</ol>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		" * a\n"
		" * b\n"
		"   1. c\n"
		"   2. d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 3 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:3\n"
		"undo:undo:3\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing mid items, multi-select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:duhSdes\n"
		"action:indent\n"
		"action:style-list-alpha\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ol>"
				"<li>a</li>"
				"<ol type=\"A\">"
					"<li>b</li>"
					"<li>c</li>"
				"</ol>"
				"<li>d</li>"
			"</ol>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		"   1. a\n"
		"         A. b\n"
		"         B. c\n"
		"   2. d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ol>"
				"<li>a</li>"
			"</ol>"
			"<ol type=\"A\">"
				"<li>b</li>"
				"<li>c</li>"
			"</ol>"
			"<ol>"
				"<li>d</li>"
			"</ol>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		"   1. a\n"
		"   A. b\n"
		"   B. c\n"
		"   1. d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 3 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:3\n"
		"undo:undo:3\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	/* Changing last items, multi-select */

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"seq:deSuhs\n"
		"action:indent\n"
		"action:style-list-roman\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ol>"
				"<li>a</li>"
				"<li>b</li>"
				"<ol type=\"I\">"
					"<li>c</li>"
					"<li>d</li>"
				"</ol>"
			"</ol>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		"   1. a\n"
		"   2. b\n"
		"         I. c\n"
		"        II. d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>prefix</pre>"
			"<ol>"
				"<li>a</li>"
				"<li>b</li>"
			"</ol>"
			"<ol type=\"I\">"
				"<li>c</li>"
				"<li>d</li>"
			"</ol>"
			"<pre>suffix</pre>"
		HTML_SUFFIX,
		"prefix\n"
		"   1. a\n"
		"   2. b\n"
		"   I. c\n"
		"  II. d\n"
		"suffix\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 3 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:3\n"
		"undo:undo:3\n",
		!is_html ? NULL : unindented_html, unindented_plain))
		g_test_fail ();
}

static void
test_list_indent_different_html (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture, "mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_list_indent_different (fixture, TRUE);
}

static void
test_list_indent_different_plain (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture, "mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_list_indent_different (fixture, FALSE);
}

static void
test_list_indent_multi (TestFixture *fixture,
			gboolean is_html)
{
	const gchar *unindented_html, *unindented_plain;

	unindented_html =
		HTML_PREFIX
			"<pre>line 1</pre>"
			"<pre>line 2</pre>"
			"<ul>"
				"<li>a</li>"
				"<li>b</li>"
				"<li>c</li>"
			"</ul>"
			"<pre>line 3</pre>"
			"<ol>"
				"<li>1</li>"
				"<li>2</li>"
			"</ol>"
			"<pre>line 4</pre>"
			"<ol type=\"A\">"
				"<li>A</li>"
				"<li>B</li>"
			"</ol>"
			"<pre>line 5</pre>"
			"<ol type=\"I\">"
				"<li>i</li>"
				"<li>ii</li>"
			"</ol>"
			"<pre>line 6</pre>"
		HTML_SUFFIX;

	unindented_plain =
		"line 1\n"
		"line 2\n"
		" * a\n"
		" * b\n"
		" * c\n"
		"line 3\n"
		"   1. 1\n"
		"   2. 2\n"
		"line 4\n"
		"   A. A\n"
		"   B. B\n"
		"line 5\n"
		"   I. i\n"
		"  II. ii\n"
		"line 6\n";

	if (!test_utils_run_simple_test (fixture,
		"action:style-preformat\n"
		"type:line 1\\n\n"
		"type:line 2\\n\n"
		"action:style-list-bullet\n"
		"type:a\\n\n"
		"type:b\\n\n"
		"type:c\\n\\n\n"
		"action:style-preformat\n"
		"type:line 3\\n\n"
		"action:style-list-number\n"
		"type:1\\n\n"
		"type:2\\n\\n\n"
		"action:style-preformat\n"
		"type:line 4\\n\n"
		"action:style-list-alpha\n"
		"type:A\\n\n"
		"type:B\\n\\n\n"
		"action:style-preformat\n"
		"type:line 5\\n\n"
		"action:style-list-roman\n"
		"type:i\\n\n"
		"type:ii\\n\\n\n"
		"action:style-preformat\n"
		"type:line 6\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"action:select-all\n"
		"undo:save\n" /* 1 */
		"action:indent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre style=\"margin-left: 3ch;\">line 1</pre>"
			"<pre style=\"margin-left: 3ch;\">line 2</pre>"
			"<ul><ul>"
				"<li>a</li>"
				"<li>b</li>"
				"<li>c</li>"
			"</ul></ul>"
			"<pre style=\"margin-left: 3ch;\">line 3</pre>"
			"<ol><ol>"
				"<li>1</li>"
				"<li>2</li>"
			"</ol></ol>"
			"<pre style=\"margin-left: 3ch;\">line 4</pre>"
			"<ol type=\"A\"><ol type=\"A\">"
				"<li>A</li>"
				"<li>B</li>"
			"</ol></ol>"
			"<pre style=\"margin-left: 3ch;\">line 5</pre>"
			"<ol type=\"I\"><ol type=\"I\">"
				"<li>i</li>"
				"<li>ii</li>"
			"</ol></ol>"
			"<pre style=\"margin-left: 3ch;\">line 6</pre>"
		HTML_SUFFIX,
		"   line 1\n"
		"   line 2\n"
		"    - a\n"
		"    - b\n"
		"    - c\n"
		"   line 3\n"
		"         1. 1\n"
		"         2. 2\n"
		"   line 4\n"
		"         A. A\n"
		"         B. B\n"
		"   line 5\n"
		"         I. i\n"
		"        II. ii\n"
		"   line 6\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"action:unindent\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:undo\n"
		"undo:test:1\n"
		"undo:redo\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:Shur\n" /* To be able to unindent the selection should end inside the list */
		"action:unindent\n"
		"action:style-preformat\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>line 1</pre>"
			"<pre>line 2</pre>"
			"<pre>a</pre>"
			"<pre>b</pre>"
			"<pre>c</pre>"
			"<pre>line 3</pre>"
			"<pre>1</pre>"
			"<pre>2</pre>"
			"<pre>line 4</pre>"
			"<pre>A</pre>"
			"<pre>B</pre>"
			"<pre>line 5</pre>"
			"<pre>i</pre>"
			"<pre>ii</pre>"
			"<pre>line 6</pre>"
		HTML_SUFFIX,
		"line 1\n"
		"line 2\n"
		"a\n"
		"b\n"
		"c\n"
		"line 3\n"
		"1\n"
		"2\n"
		"line 4\n"
		"A\n"
		"B\n"
		"line 5\n"
		"i\n"
		"ii\n"
		"line 6\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 3 */
		"undo:undo:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:undo:2\n",
		!is_html ? NULL : unindented_html, unindented_plain))
		g_test_fail ();
}

static void
test_list_indent_multi_html (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture, "mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_list_indent_multi (fixture, TRUE);
}

static void
test_list_indent_multi_plain (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture, "mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_list_indent_multi (fixture, FALSE);
}

static void
test_list_indent_nested (TestFixture *fixture,
			 gboolean is_html)
{
	const gchar *unindented_html, *unindented_plain;

	unindented_html =
		HTML_PREFIX
			"<pre>line 1</pre>"
			"<ul>"
				"<li>a</li>"
				"<ol>"
					"<li>123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789</li>"
					"<li>2</li>"
						"<ul>"
							"<li>x</li>"
							"<li>y</li>"
						"</ul>"
					"<li>3</li>"
				"</ol>"
				"<li>b</li>"
			"</ul>"
			"<pre>line 2</pre>"
			"<ol>"
				"<li>1</li>"
					"<ul>"
						"<li>a</li>"
						"<ol type=\"A\">"
							"<li>A</li>"
							"<li>B</li>"
						"</ol>"
						"<li>b</li>"
					"</ul>"
				"<li>2</li>"
			"</ol>"
			"<pre>line 3</pre>"
		HTML_SUFFIX;

	unindented_plain =
		"line 1\n"
		" * a\n"
		"      1. 123456789 123456789 123456789 123456789 123456789 123456789\n"
		"         123456789 123456789\n"
		"      2. 2\n"
		"          + x\n"
		"          + y\n"
		"      3. 3\n"
		" * b\n"
		"line 2\n"
		"   1. 1\n"
		"       - a\n"
		"            A. A\n"
		"            B. B\n"
		"       - b\n"
		"   2. 2\n"
		"line 3\n";

	if (!test_utils_run_simple_test (fixture,
		"action:style-preformat\n"
		"type:line 1\\n\n"
		"action:style-list-bullet\n"
		"type:a\\n\n"
		"action:indent\n"
		"action:style-list-number\n"
		"type:123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789\\n\n"
		"type:2\\n\n"
		"action:indent\n"
		"action:style-list-bullet\n"
		"type:x\\n\n"
		"type:y\\n\\n\n"
		"type:3\\n\\n\n"
		"type:b\\n\\n\n"
		"action:style-preformat\n"
		"type:line 2\\n\n"
		"action:style-list-number\n"
		"type:1\\n\n"
		"action:indent\n"
		"action:style-list-bullet\n"
		"type:a\\n\n"
		"action:indent\n"
		"action:style-list-alpha\n"
		"type:A\\n\n"
		"type:B\\n\\n\n"
		"type:b\\n\\n\n"
		"type:2\\n\\n\n"
		"action:style-preformat\n"
		"type:line 3\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:hurSChcds\n" /* selects all but the "line 1" and "line 3" */
		"undo:save\n" /* 1 */
		"action:indent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>line 1</pre>"
			"<ul><ul>"
				"<li>a</li>"
				"<ol>"
					"<li>123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789</li>"
					"<li>2</li>"
						"<ul>"
							"<li>x</li>"
							"<li>y</li>"
						"</ul>"
					"<li>3</li>"
				"</ol>"
				"<li>b</li>"
			"</ul></ul>"
			"<pre style=\"margin-left: 3ch;\">line 2</pre>"
			"<ol><ol>"
				"<li>1</li>"
					"<ul>"
						"<li>a</li>"
						"<ol type=\"A\">"
							"<li>A</li>"
							"<li>B</li>"
						"</ol>"
						"<li>b</li>"
					"</ul>"
				"<li>2</li>"
			"</ol></ol>"
			"<pre>line 3</pre>"
		HTML_SUFFIX,
		"line 1\n"
		"    - a\n"
		"         1. 123456789 123456789 123456789 123456789 123456789 123456789\n"
		"            123456789 123456789\n"
		"         2. 2\n"
		"             * x\n"
		"             * y\n"
		"         3. 3\n"
		"    - b\n"
		"   line 2\n"
		"         1. 1\n"
		"             + a\n"
		"                  A. A\n"
		"                  B. B\n"
		"             + b\n"
		"         2. 2\n"
		"line 3\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:2\n" /* 0 */
		"undo:save\n" /* 1 */
		"action:unindent\n",
		!is_html ? NULL : unindented_html, unindented_plain)) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:2\n" /* 0 */
		"undo:save\n" /* 1 */
		"action:unindent\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>line 1</pre>"
			"<div>a</div>"
			"<ol>"
				"<li>123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789</li>"
				"<li>2</li>"
					"<ul>"
						"<li>x</li>"
						"<li>y</li>"
					"</ul>"
				"<li>3</li>"
			"</ol>"
			"<div>b</div>"
			"<pre>line 2</pre>"
			"<div>1</div>"
				"<ul>"
					"<li>a</li>"
					"<ol type=\"A\">"
						"<li>A</li>"
						"<li>B</li>"
					"</ol>"
					"<li>b</li>"
				"</ul>"
			"<div>2</div>"
			"<pre>line 3</pre>"
		HTML_SUFFIX,
		"line 1\n"
		"a\n"
		"   1. 123456789 123456789 123456789 123456789 123456789 123456789\n"
		"      123456789 123456789\n"
		"   2. 2\n"
		"       - x\n"
		"       - y\n"
		"   3. 3\n"
		"b\n"
		"line 2\n"
		"1\n"
		" * a\n"
		"      A. A\n"
		"      B. B\n"
		" * b\n"
		"2\n"
		"line 3\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 2 */
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:2\n" /* 0 */
		"undo:save\n" /* 1 */
		"seq:CecuueSChcdds\n"
		"action:unindent\n"
		"seq:CecuuueSChcddds\n"
		"action:unindent\n"
		"action:select-all\n"
		"action:style-normal\n"
		"action:style-preformat\n",
		!is_html ? NULL :
		HTML_PREFIX
			"<pre>line 1</pre>"
			"<pre>a</pre>"
			"<pre>123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789</pre>"
			"<pre>2</pre>"
			"<pre>x</pre>"
			"<pre>y</pre>"
			"<pre>3</pre>"
			"<pre>b</pre>"
			"<pre>line 2</pre>"
			"<pre>1</pre>"
			"<pre>a</pre>"
			"<pre>A</pre>"
			"<pre>B</pre>"
			"<pre>b</pre>"
			"<pre>2</pre>"
			"<pre>line 3</pre>"
		HTML_SUFFIX,
		"line 1\n"
		"a\n"
		"123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789\n"
		"2\n"
		"x\n"
		"y\n"
		"3\n"
		"b\n"
		"line 2\n"
		"1\n"
		"a\n"
		"A\n"
		"B\n"
		"b\n"
		"2\n"
		"line 3\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_process_commands (fixture,
		"undo:save\n" /* 2 */
		"undo:undo:4\n"
		"undo:test:2\n"
		"undo:redo:4\n"
		"undo:test\n"))
		g_test_fail ();
}

static void
test_list_indent_nested_html (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture, "mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_list_indent_nested (fixture, TRUE);
}

static void
test_list_indent_nested_plain (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture, "mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_list_indent_nested (fixture, FALSE);
}

static void
test_link_insert_dialog (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:a link example: \n"
		"action:insert-link\n"
		"type:http://www.gnome.org\n"
		"seq:A\n" /* Alt+A to press the "Add" button in the popover */
		"type:a\n"
		"seq:a\n",
		HTML_PREFIX "<div>a link example: <a href=\"http://www.gnome.org\">http://www.gnome.org</a></div>" HTML_SUFFIX,
		"a link example: http://www.gnome.org\n"))
		g_test_fail ();
}

static void
test_link_insert_dialog_selection (TestFixture *fixture)
{
	test_utils_fixture_change_setting_string (fixture, "org.gnome.evolution.mail", "html-link-to-text", "none");

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:a link example: GNOME\n"
		"seq:CSlsc\n"
		"action:insert-link\n"
		"type:http://www.gnome.org\n"
		"seq:A\n" /* Alt+A to press the "Add" button in the popover */
		"type:a\n"
		"seq:a\n",
		HTML_PREFIX "<div>a link example: <a href=\"http://www.gnome.org\">GNOME</a></div>" HTML_SUFFIX,
		"a link example: GNOME\n"))
		g_test_fail ();
}

static void
test_link_insert_typed (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:www.gnome.org \n",
		HTML_PREFIX "<div><a href=\"https://www.gnome.org\">www.gnome.org</a> </div>" HTML_SUFFIX,
		"www.gnome.org \n"))
		g_test_fail ();
}

static void
test_link_insert_typed_change_description (TestFixture *fixture)
{
	test_utils_fixture_change_setting_string (fixture, "org.gnome.evolution.mail", "html-link-to-text", "none");

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:www.gnome.org \n"
		"seq:ll\n"
		"action:insert-link\n"
		"seq:A\n" /* Alt+D to jump to the Description */
		"type:D\n"
		"seq:a\n"
		"type:GNOME\n"
		"seq:A\n" /* Alt+A to press the "Update" button in the popover */
		"type:a\n"
		"seq:a\n",
		HTML_PREFIX "<div><a href=\"https://www.gnome.org\">GNOME</a> </div>" HTML_SUFFIX,
		"GNOME \n"))
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
		"www.gnome.org \n"))
		g_test_fail ();
}

static void
test_link_insert_typed_append (TestFixture *fixture)
{
	test_utils_fixture_change_setting_string (fixture, "org.gnome.evolution.mail", "html-link-to-text", "none");

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:www.gnome.org \n"
		"seq:l\n"
		"type:/about\n",
		HTML_PREFIX "<div><a href=\"https://www.gnome.org\">www.gnome.org/about</a> </div>" HTML_SUFFIX,
		"www.gnome.org/about \n"))
		g_test_fail ();
}

static void
test_link_insert_typed_remove (TestFixture *fixture)
{
	test_utils_fixture_change_setting_string (fixture, "org.gnome.evolution.mail", "html-link-to-text", "none");

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:www.gnome.org \n"
		"seq:bbb\n",
		HTML_PREFIX "<div><a href=\"https://www.gnome.org\">www.gnome.o</a></div>" HTML_SUFFIX,
		"www.gnome.o\n"))
		g_test_fail ();
}

static void
test_h_rule_insert (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text\n"
		"action:insert-rule\n"
		"seq:^\n", /* Escape key press to close the dialog */
		HTML_PREFIX "<div>text</div><hr align=\"center\">" HTML_SUFFIX,
		"text\n"))
		g_test_fail ();
}

static void
test_h_rule_insert_text_after (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:above\n"
		"action:insert-rule\n"
		"seq:^\n" /* Escape key press to close the dialog */
		"seq:drn\n" /* Press the right key instead of End key as the End key won't move caret after the HR element */
		"type:below\n",
		HTML_PREFIX "<div>above</div><hr align=\"center\"><div>below</div>" HTML_SUFFIX,
		"above\n"
		"\n"
		"below\n"))
		g_test_fail ();
}

static void
test_image_insert (TestFixture *fixture)
{
	EContentEditor *cnt_editor;
	gchar *expected_html;
	gchar *filename;
	gchar *uri;
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

	/* Mimic what the action:insert-image does, without invoking the image chooser dialog */
	cnt_editor = e_html_editor_get_content_editor (fixture->editor);
	e_content_editor_insert_image (cnt_editor, uri);
	/* Wait some time until the operation is finished */
	test_utils_wait_milliseconds (500);

	expected_html = g_strconcat (HTML_PREFIX "<div>before*<img src=\"evo-", uri, "\" width=\"24px\" height=\"24px\">+after</div>" HTML_SUFFIX, NULL);

	g_free (uri);
	g_free (filename);

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n" /* 1 */
		"undo:undo\n"
		"undo:redo\n"
		"undo:test:1\n"
		"type:+after\n",
		expected_html,
		"before*+after\n"))
		g_test_fail ();

	g_free (expected_html);
}

static void
test_emoticon_insert_typed (TestFixture *fixture)
{
	gchar *image_uri;
	gchar *expected_html;

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-magic-smileys", TRUE);
	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-unicode-smileys", FALSE);

	image_uri = test_utils_dup_image_uri ("face-smile");

	expected_html = g_strconcat (HTML_PREFIX "<div>before <img src=\"", image_uri, "\" alt=\":-)\" width=\"16px\" height=\"16px\">after</div>" HTML_SUFFIX, NULL);

	g_free (image_uri);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:before :)after\n",
		expected_html,
		"before :-)after\n"))
		g_test_fail ();

	g_free (expected_html);
}

static void
test_emoticon_insert_typed_dash (TestFixture *fixture)
{
	gchar *image_uri;
	gchar *expected_html;

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-magic-smileys", TRUE);
	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-unicode-smileys", FALSE);

	image_uri = test_utils_dup_image_uri ("face-smile");

	expected_html = g_strconcat (HTML_PREFIX "<div>before <img src=\"", image_uri, "\" alt=\":-)\" width=\"16px\" height=\"16px\">after</div>" HTML_SUFFIX, NULL);

	g_free (image_uri);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:before :-)after\n",
		expected_html,
		"before :-)after\n"))
		g_test_fail ();

	g_free (expected_html);
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
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>"
		"<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n",
		HTML_PREFIX "<div>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>"
		"<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n")) {
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
		"odio. Praesent libero.\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>"
		"<div style=\"width: 71ch;\">Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.\n")) {
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
		"odio. Praesent libero.\n")) {
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
		"odio. Praesent libero.\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n",
		HTML_PREFIX "<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>"
		"<div style=\"width: 71ch;\">Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n",
		HTML_PREFIX "<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>"
		"<div>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</div>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n"
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero.\n")) {
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
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n",
		HTML_PREFIX "<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero."
		" Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero. "
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n",
		HTML_PREFIX "<pre>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero."
		" Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.</pre>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero. "
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec odio. Praesent libero.\n")) {
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
		"normal text\n"))
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
		"normal text\n"))
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
		"normal text\n",
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
		"normal text\n",
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
		"normal text\n",
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
		"normal text\n",
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
		HTML_PREFIX "<div>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec<br>"
		"odio. Praesent libero. Lorem ipsum dolor sit amet, consectetur<br>"
		"adipiscing elit. Integer nec odio. Praesent libero.</div>" HTML_SUFFIX,
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer nec\n"
		"odio. Praesent libero. Lorem ipsum dolor sit amet, consectetur\n"
		"adipiscing elit. Integer nec odio. Praesent libero.\n"))
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
		"text before some bold text text after\n"))
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
		HTML_PREFIX "<div style=\"width: 71ch;\">text before some bold text text after</div>" HTML_SUFFIX,
		"text before some bold text text after\n"))
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
		"text before some plain text text after\n"))
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
		HTML_PREFIX "<div style=\"width: 71ch;\">text before some plain text text after</div>" HTML_SUFFIX,
		"text before some plain text text after\n"))
		g_test_fail ();
}

static void
test_paste_multiline_html2html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body><b>bold</b> text<br><i>italic</i> text<br><u>underline</u> text<br>.</body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text before \n"
		"action:paste\n"
		"type:text after\n",
		HTML_PREFIX "<div>text before <b>bold</b> text</div><div><i>italic</i> text<br><u>underline</u> text<br>.text after</div>" HTML_SUFFIX,
		"text before bold text\nitalic text\nunderline text\n.text after\n"))
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
		HTML_PREFIX "<div style=\"width: 71ch;\">text before bold text</div>"
		"<div style=\"width: 71ch;\">italic text<br>underline text</div>"
		"<div style=\"width: 71ch;\">text after</div>" HTML_SUFFIX,
		"text before bold text\nitalic text\nunderline text\ntext after\n"))
		g_test_fail ();
}

static void
test_paste_multiline_div_html2html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body><div><b>bold</b> text</div><div><i>italic</i> text</div><div><u>underline</u> text</div><div>.</div></body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text before \n"
		"action:paste\n"
		"type:text after\n",
		HTML_PREFIX "<div>text before <b>bold</b> text</div><div><i>italic</i> text</div><div><u>underline</u> text</div><div>.text after</div>" HTML_SUFFIX,
		"text before bold text\nitalic text\nunderline text\n.text after\n"))
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
		HTML_PREFIX "<div style=\"width: 71ch;\">text before bold text</div>"
		"<div style=\"width: 71ch;\">italic text<br>underline text<br></div>"
		"<div style=\"width: 71ch;\">text after</div>" HTML_SUFFIX,
		"text before bold text\nitalic text\nunderline text\ntext after\n"))
		g_test_fail ();
}

static void
test_paste_multiline_p_html2html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("<html><body><p><b>bold</b> text</p><p><i>italic</i> text</p><p><u>underline</u> text</p><p><br></p></body></html>", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text before \n"
		"action:paste\n"
		"type:text after\n",
		HTML_PREFIX "<div>text before <b>bold</b> text</div><div><i>italic</i> text</div><div><u>underline</u> text</div><div>text after</div>" HTML_SUFFIX,
		"text before bold text\nitalic text\nunderline text\ntext after\n"))
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
		HTML_PREFIX "<div style=\"width: 71ch;\">text before bold text</div>"
		"<div style=\"width: 71ch;\">italic text<br>underline text<br></div>"
		"<div style=\"width: 71ch;\">text after</div>" HTML_SUFFIX,
		"text before bold text\nitalic text\nunderline text\ntext after\n"))
		g_test_fail ();
}

static void
test_paste_multiline_plain2html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("line 1\nline 2\nline 3\n", FALSE);

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-paste-plain-prefer-pre", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text before \n"
		"action:paste\n"
		"type:text after\n",
		HTML_PREFIX "<div>text before line 1</div><div>line 2</div><div>line 3</div><div>text after</div>" HTML_SUFFIX,
		"text before line 1\nline 2\nline 3\ntext after\n")) {
		g_test_fail ();
		return;
	}

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-paste-plain-prefer-pre", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"seq:cD\n"
		"type:text before \n"
		"action:paste\n"
		"type:text after\n",
		HTML_PREFIX "<div>text before line 1</div><pre>line 2</pre><pre>line 3</pre><pre>text after</pre>" HTML_SUFFIX,
		"text before line 1\nline 2\nline 3\ntext after\n"))
		g_test_fail ();
}

static void
test_paste_multiline_plain2plain (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("line 1\nline 2\nline 3", FALSE);

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-paste-plain-prefer-pre", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:text before \n"
		"action:paste\n"
		"type:\\ntext after\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">text before line 1</div>"
		"<div style=\"width: 71ch;\">line 2</div>"
		"<div style=\"width: 71ch;\">line 3</div>"
		"<div style=\"width: 71ch;\">text after</div>" HTML_SUFFIX,
		"text before line 1\nline 2\nline 3\ntext after\n")) {
		g_test_fail ();
		return;
	}

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-paste-plain-prefer-pre", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"seq:cD\n"
		"type:text before \n"
		"action:paste\n"
		"type:\\ntext after\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">text before line 1</div>"
		"<pre>line 2</pre>"
		"<pre>line 3</pre>"
		"<pre>text after</pre>" HTML_SUFFIX,
		"text before line 1\nline 2\nline 3\ntext after\n"))
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
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE "><div>some <b>bold</b> text</div></blockquote>"
		"<div>text after</div>" HTML_SUFFIX,
		"text before \n"
		"> some bold text\n"
		"text after\n"))
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
		HTML_PREFIX "<div style=\"width: 71ch;\">text before </div>"
		"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "some bold text</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\">text after</div>" HTML_SUFFIX,
		"text before \n"
		"> some bold text\n"
		"text after\n"))
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
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE "><div>some plain text</div></blockquote>"
		"<div>text after</div>" HTML_SUFFIX,
		"text before \n"
		"> some plain text\n"
		"text after\n"))
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
		HTML_PREFIX "<div style=\"width: 71ch;\">text before </div>"
		"<blockquote type=\"cite\"><div>" QUOTE_SPAN (QUOTE_CHR) "some plain text</div></blockquote>"
		"<div style=\"width: 71ch;\">text after</div>" HTML_SUFFIX,
		"text before \n"
		"> some plain text\n"
		"text after\n"))
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
		"type:\\n\n" /* stop quotting */
		"type:text after\n",
		HTML_PREFIX "<div>text before </div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE "><div><b>bold</b> text<br>"
		"<i>italic</i> text<br>"
		"<u>underline</u> text<br></div></blockquote>"
		"<div>text after</div>" HTML_SUFFIX,
		"text before \n"
		"> bold text\n"
		"> italic text\n"
		"> underline text\n"
		"text after\n"))
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
		HTML_PREFIX "<div style=\"width: 71ch;\">text before </div>"
		"<blockquote type=\"cite\"><div>" QUOTE_SPAN (QUOTE_CHR) "bold text<br>"
		QUOTE_SPAN (QUOTE_CHR) "italic text<br>"
		QUOTE_SPAN (QUOTE_CHR) "underline text</div></blockquote>"
		"<div style=\"width: 71ch;\">text after</div>" HTML_SUFFIX,
		"text before \n"
		"> bold text\n"
		"> italic text\n"
		"> underline text\n"
		"text after\n"))
		g_test_fail ();
}

static void
test_paste_quoted_multiline_plain2html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("line 1\nline 2\nline 3\n", FALSE);

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-paste-plain-prefer-pre", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:text before \n"
		"action:paste-quote\n"
		"type:\\n\n" /* stop quotting */
		"type:text after\n",
		HTML_PREFIX "<div>text before </div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE "><div>line 1</div>"
		"<div>line 2</div>"
		"<div>line 3</div>"
		"<div><br></div></blockquote>"
		"<div>text after</div>" HTML_SUFFIX,
		"text before \n"
		"> line 1\n"
		"> line 2\n"
		"> line 3\n"
		"> \n"
		"text after\n")) {
		g_test_fail ();
		return;
	}

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-paste-plain-prefer-pre", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"seq:cD\n"
		"type:text before \n"
		"action:paste-quote\n"
		"type:\\n\n" /* stop quotting */
		"type:text after\n",
		HTML_PREFIX "<div>text before </div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE "><pre>line 1</pre>"
		"<pre>line 2</pre>"
		"<pre>line 3</pre>"
		"<pre><br></pre></blockquote>"
		"<div>text after</div>" HTML_SUFFIX,
		"text before \n"
		"> line 1\n"
		"> line 2\n"
		"> line 3\n"
		"> \n"
		"text after\n"))
		g_test_fail ();
}

static void
test_paste_quoted_multiline_plain2plain (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("line 1\nline 2\nline 3", FALSE);

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-paste-plain-prefer-pre", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:text before \n"
		"action:paste-quote\n"
		"type:\\n\n" /* stop quotting */
		"type:text after\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">text before </div>"
		"<blockquote type=\"cite\"><div>" QUOTE_SPAN (QUOTE_CHR) "line 1</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "line 2</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "line 3</div></blockquote>"
		"<div style=\"width: 71ch;\">text after</div>" HTML_SUFFIX,
		"text before \n"
		"> line 1\n"
		"> line 2\n"
		"> line 3\n"
		"text after\n")) {
		g_test_fail ();
		return;
	}

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-paste-plain-prefer-pre", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"seq:cD\n"
		"type:text before \n"
		"action:paste-quote\n"
		"type:\\n\n" /* stop quotting */
		"type:text after\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">text before </div>"
		"<blockquote type=\"cite\"><pre>" QUOTE_SPAN (QUOTE_CHR) "line 1</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "line 2</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "line 3</pre></blockquote>"
		"<div style=\"width: 71ch;\">text after</div>" HTML_SUFFIX,
		"text before \n"
		"> line 1\n"
		"> line 2\n"
		"> line 3\n"
		"text after\n"))
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
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>level 1</div>"
		"<div><br></div>"
		"<div>level 1</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>level 2</div>"
		"</blockquote>"
		"<div>back in level 1</div>"
		"</blockquote>"
		"<div><br></div>"
		"<div>out of the citation</div>"
		"</body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	/* Just check the content was read properly */
	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\" " BLOCKQUOTE_STYLE "><div>level 1</div><div><br></div><div>level 1</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE "><div>level 2</div></blockquote><div>back in level 1</div></blockquote>"
		"<div><br></div><div>out of the citation</div>" HTML_SUFFIX,
		"> level 1\n"
		"> \n"
		"> level 1\n"
		"> > level 2\n"
		"> back in level 1\n"
		"\n"
		"out of the citation\n")) {
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
		"out of the citation\n"))
		g_test_fail ();
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
		"<html><head></head><body><blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>Just one short line.</div>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>Just one short line.</div>"
		"</blockquote>" HTML_SUFFIX,
		"> Just one short line.\n")) {
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
		"<html><head></head><body><blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>Just one short line.</div>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>Just one short line.</div>"
		"</blockquote>" HTML_SUFFIX,
		"> Just one short line.\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body><blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>short line 1</div>"
		"<div>short line 2</div>"
		"<div>short line 3</div>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>short line 1</div>"
		"<div>short line 2</div>"
		"<div>short line 3</div>"
		"</blockquote>" HTML_SUFFIX,
		"> short line 1\n"
		"> short line 2\n"
		"> short line 3\n")) {
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
		"<html><head></head><body><blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>short line 1</div>"
		"<div>short line 2</div>"
		"<div>short line 3</div>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>short line 1</div>"
		"<div>short line 2</div>"
		"<div>short line 3</div>"
		"</blockquote>" HTML_SUFFIX,
		"> short line 1\n"
		"> short line 2\n"
		"> short line 3\n")) {
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
		"<html><head></head><body><blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"</blockquote>" HTML_SUFFIX,
		"> This is the first paragraph of a quoted text which has some long text\n"
		"> to test. It has the second sentence as well.\n")) {
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
		"<html><head></head><body><blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"</blockquote></body></html>",
		E_CONTENT_EDITOR_INSERT_TEXT_HTML | E_CONTENT_EDITOR_INSERT_REPLACE_ALL);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"</blockquote>" HTML_SUFFIX,
		"> This is the first paragraph of a quoted text which has some long text\n"
		"> to test. It has the second sentence as well.\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body><blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"<div>This is the second paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"<div>This is the third paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"</blockquote><br>after quote</body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
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
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE "><pre>line 1</pre>"
		"<pre>line 2</pre></blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> line 1\n"
		"> line 2\n"))
		g_test_fail ();
}

static void
test_cite_reply_html_to_plain (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<pre>line 1\n"
		"line 2\n\n"
		"</pre><span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Today, User wrote:</div>"
		"<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "line 1</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "line 2</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "<br></pre></blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> line 1\n"
		"> line 2\n"
		"> \n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<div>line 1</div>"
		"<div>line 2</div><br>"
		"<span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Today, User wrote:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "line 1</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "line 2</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div></blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> line 1\n"
		"> line 2\n"
		"> \n"))
		g_test_fail ();
}

static void
test_cite_reply_plain (TestFixture *fixture)
{
	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-wrap-quoted-text-in-replies", TRUE);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<pre>line 1\n"
		"\n"
		"line 2\n"
		"   \n"
		"line 3\n"
		" \n"
		"</pre><span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Today, User wrote:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "line 1</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "line 2</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "&nbsp;&nbsp;&nbsp;</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "line 3</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "&nbsp;</div>"
		"</blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> line 1\n"
		"> \n"
		"> line 2\n"
		">    \n"
		"> line 3\n"
		">  \n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body><div><br></div></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-wrap-quoted-text-in-replies", FALSE);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<pre>line 1\n"
		"\n"
		"line 2\n"
		"   \n"
		"line 3\n"
		" \n"
		"</pre><span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Today, User wrote:</div>"
		"<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "line 1</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "<br></pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "line 2</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "   </pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "line 3</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) " </pre>"
		"</blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> line 1\n"
		"> \n"
		"> line 2\n"
		">    \n"
		"> line 3\n"
		">  \n"))
		g_test_fail ();
}

static void
test_cite_reply_link (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body><div><span>123 (here <a href=\"https://www.example.com\">\n"
		"https://www.example.com/1234567890/1234567890/1234567890/1234567890/1234567890/</a>"
		") and </span>here ěščřžýáíé <a href=\"https://www.example.com\">www.example.com</a>"
		" with closing text after.</div>"
		"<div>www.example1.com</div>"
		"<div>before www.example2.com</div>"
		"<div>www.example3.com after</div>"
		"<div>😏😉🙂 user@no.where line with Emoji</div></body></html>"
		"<span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Today, User wrote:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "123 (here" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://www.example.com/1234567890/1234567890/1234567890/1234567890/1234567890/\">"
			"https://www.example.com/1234567890/1234567890/1234567890/1234567890/1234567890/</a>)" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "and here ěščřžýáíé <a href=\"https://www.example.com\">www.example.com</a> with closing text after.</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://www.example1.com\">www.example1.com</a></div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "before <a href=\"https://www.example2.com\">www.example2.com</a></div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://www.example3.com\">www.example3.com</a> after</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "😏😉🙂 <a href=\"mailto:user@no.where\">user@no.where</a> line with Emoji</div>"
		"</blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> 123 (here\n"
		"> https://www.example.com/1234567890/1234567890/1234567890/1234567890/1234567890/\n"
		"> ) and here ěščřžýáíé www.example.com with closing text after.\n"
		"> www.example1.com\n"
		"> before www.example2.com\n"
		"> www.example3.com after\n"
		"> 😏😉🙂 user@no.where line with Emoji\n"))
		g_test_fail ();
}

static void
test_cite_editing_html (TestFixture *fixture)
{
	const gchar *html[5], *plain[5];

	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<div>before citation</div>"
		"<blockquote type='cite'>"
			"<div>cite level 1a</div>"
			"<blockquote type='cite'>"
				"<div>cite level 2</div>"
			"</blockquote>"
			"<div>cite level 1b</div>"
		"</blockquote>"
		"<div>after citation</div>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	html[0] = HTML_PREFIX "<div>before citation</div>"
		"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
			"<div>cite level 1a</div>"
			"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
				"<div>cite level 2</div>"
			"</blockquote>"
			"<div>cite level 1b</div>"
		"</blockquote>"
		"<div>after citation</div>" HTML_SUFFIX;

	plain[0] = "before citation\n"
		"> cite level 1a\n"
		"> > cite level 2\n"
		"> cite level 1b\n"
		"after citation\n";

	if (!test_utils_run_simple_test (fixture, "", html[0], plain[0])) {
		g_test_fail ();
		return;
	}

	html[1] = HTML_PREFIX "<div>before citation</div>"
		"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
			"<div>cite level 1a</div>"
			"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
				"<div>ciXte level 2</div>"
			"</blockquote>"
			"<div>cite level 1b</div>"
		"</blockquote>"
		"<div>after citation</div>" HTML_SUFFIX;

	plain[1] = "before citation\n"
		"> cite level 1a\n"
		"> > ciXte level 2\n"
		"> cite level 1b\n"
		"after citation\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:Chc\n" /* Ctrl+Home to get to the beginning of the document */
		"seq:ddrr\n" /* on the third line, after the second character */
		"type:X\n",
		html[1], plain[1])) {
		g_test_fail ();
		return;
	}

	html[2] = HTML_PREFIX "<div>before citation</div>"
		"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
			"<div>cite level 1a</div>"
			"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
				"<div>ciX</div>"
			"</blockquote>"
		"</blockquote>"
		"<div>Y</div>"
		"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
			"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
				"<div>te level 2</div>"
			"</blockquote>"
			"<div>cite level 1b</div>"
		"</blockquote>"
		"<div>after citation</div>" HTML_SUFFIX;

	plain[2] = "before citation\n"
		"> cite level 1a\n"
		"> > ciX\n"
		"Y\n"
		"> > te level 2\n"
		"> cite level 1b\n"
		"after citation\n";

	if (!test_utils_run_simple_test (fixture,
		"type:\\nY\n",
		html[2], plain[2])) {
		g_test_fail ();
		return;
	}

	html[3] = HTML_PREFIX "<div>before citation</div>"
		"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
			"<div>cite level 1a</div>"
			"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
				"<div>ciX</div>"
			"</blockquote>"
		"</blockquote>"
		"<div>Y</div>"
		"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
			"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
				"<div>tZ<br>e level 2</div>"
			"</blockquote>"
			"<div>cite level 1b</div>"
		"</blockquote>"
		"<div>after citation</div>" HTML_SUFFIX;

	plain[3] = "before citation\n"
		"> cite level 1a\n"
		"> > ciX\n"
		"Y\n"
		"> > tZ\n"
		"> > e level 2\n"
		"> cite level 1b\n"
		"after citation\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:dr\n"
		"type:Z\n"
		"seq:Sns\n", /* soft Enter */
		html[3], plain[3])) {
		g_test_fail ();
		return;
	}

	html[4] = HTML_PREFIX "<div>before citation</div>"
		"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
			"<div>cite level 1a</div>"
			"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
				"<div>ciX</div>"
			"</blockquote>"
		"</blockquote>"
		"<div>Y</div>"
		"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
			"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
				"<div>tZ<br>e level 2</div>"
			"</blockquote>"
		"</blockquote>"
		"<div><br></div>"
		"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
			"<div><br></div>"
		"</blockquote>"
		"<div><br></div>"
		"<blockquote type='cite' " BLOCKQUOTE_STYLE ">"
			"<div>cite level 1b</div>"
		"</blockquote>"
		"<div><br></div>"
		"<div>after citation</div>" HTML_SUFFIX;

	plain[4] = "before citation\n"
		"> cite level 1a\n"
		"> > ciX\n"
		"Y\n"
		"> > tZ\n"
		"> > e level 2\n"
		"\n"
		"> \n"
		"\n"
		"> cite level 1b\n"
		"\n"
		"after citation\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:endhnden\n",
		html[4], plain[4])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:undo:3\n",
		html[3], plain[3])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:undo:2\n",
		html[2], plain[2])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:undo:2\n",
		html[1], plain[1])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:undo:1\n",
		html[0], plain[0])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:redo:1\n",
		html[1], plain[1])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:redo:2\n",
		html[2], plain[2])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:redo:2\n",
		html[3], plain[3])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:redo:3\n",
		html[4], plain[4])) {
		g_test_fail ();
		return;
	}
}

static void
test_cite_editing_plain (TestFixture *fixture)
{
	const gchar *html[5], *plain[5];

	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<div>before citation</div>"
		"<blockquote type='cite'>"
			"<div>cite level 1a</div>"
			"<blockquote type='cite'>"
				"<div>cite level 2</div>"
			"</blockquote>"
			"<div>cite level 1b</div>"
		"</blockquote>"
		"<div>after citation</div>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	html[0] = HTML_PREFIX "<div style='width: 71ch;'>before citation</div>"
		"<blockquote type='cite'>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "cite level 1a</div>"
			"<blockquote type='cite'>"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "cite level 2</div>"
			"</blockquote>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "cite level 1b</div>"
		"</blockquote>"
		"<div style='width: 71ch;'>after citation</div>" HTML_SUFFIX;

	plain[0] = "before citation\n"
		"> cite level 1a\n"
		"> > cite level 2\n"
		"> cite level 1b\n"
		"after citation\n";

	if (!test_utils_run_simple_test (fixture, "mode:plain", html[0], plain[0])) {
		g_test_fail ();
		return;
	}

	html[1] = HTML_PREFIX "<div style='width: 71ch;'>before citation</div>"
		"<blockquote type='cite'>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "cite level 1a</div>"
			"<blockquote type='cite'>"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "ciXte level 2</div>"
			"</blockquote>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "cite level 1b</div>"
		"</blockquote>"
		"<div style='width: 71ch;'>after citation</div>" HTML_SUFFIX;

	plain[1] = "before citation\n"
		"> cite level 1a\n"
		"> > ciXte level 2\n"
		"> cite level 1b\n"
		"after citation\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:Chc\n" /* Ctrl+Home to get to the beginning of the document */
		"seq:ddrr\n" /* on the third line, after the second character */
		"type:X\n",
		html[1], plain[1])) {
		g_test_fail ();
		return;
	}

	html[2] = HTML_PREFIX "<div style='width: 71ch;'>before citation</div>"
		"<blockquote type='cite'>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "cite level 1a</div>"
			"<blockquote type='cite'>"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "ciX</div>"
			"</blockquote>"
		"</blockquote>"
		"<div style='width: 71ch;'>Y</div>"
		"<blockquote type='cite'>"
			"<blockquote type='cite'>"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "te level 2</div>"
			"</blockquote>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "cite level 1b</div>"
		"</blockquote>"
		"<div style='width: 71ch;'>after citation</div>" HTML_SUFFIX;

	plain[2] = "before citation\n"
		"> cite level 1a\n"
		"> > ciX\n"
		"Y\n"
		"> > te level 2\n"
		"> cite level 1b\n"
		"after citation\n";

	if (!test_utils_run_simple_test (fixture,
		"type:\\nY\n",
		html[2], plain[2])) {
		g_test_fail ();
		return;
	}

	html[3] = HTML_PREFIX "<div style='width: 71ch;'>before citation</div>"
		"<blockquote type='cite'>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "cite level 1a</div>"
			"<blockquote type='cite'>"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "ciX</div>"
			"</blockquote>"
		"</blockquote>"
		"<div style='width: 71ch;'>Y</div>"
		"<blockquote type='cite'>"
			"<blockquote type='cite'>"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "tZ</div>"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "e level 2</div>"
			"</blockquote>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "cite level 1b</div>"
		"</blockquote>"
		"<div style='width: 71ch;'>after citation</div>" HTML_SUFFIX;

	plain[3] = "before citation\n"
		"> cite level 1a\n"
		"> > ciX\n"
		"Y\n"
		"> > tZ\n"
		"> > e level 2\n"
		"> cite level 1b\n"
		"after citation\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:dr\n"
		"type:Z\n"
		"seq:Sns\n", /* soft Enter */
		html[3], plain[3])) {
		g_test_fail ();
		return;
	}

	html[4] = HTML_PREFIX "<div style='width: 71ch;'>before citation</div>"
		"<blockquote type='cite'>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "cite level 1a</div>"
			"<blockquote type='cite'>"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "ciX</div>"
			"</blockquote>"
		"</blockquote>"
		"<div style='width: 71ch;'>Y</div>"
		"<blockquote type='cite'>"
			"<blockquote type='cite'>"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "tZ</div>"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "e level 2</div>"
			"</blockquote>"
		"</blockquote>"
		"<div style='width: 71ch;'><br></div>"
		"<blockquote type='cite'>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div>"
		"</blockquote>"
		"<div style='width: 71ch;'><br></div>"
		"<blockquote type='cite'>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "cite level 1b</div>"
		"</blockquote>"
		"<div style='width: 71ch;'><br></div>"
		"<div style='width: 71ch;'>after citation</div>" HTML_SUFFIX;

	plain[4] = "before citation\n"
		"> cite level 1a\n"
		"> > ciX\n"
		"Y\n"
		"> > tZ\n"
		"> > e level 2\n"
		"\n"
		"> \n"
		"\n"
		"> cite level 1b\n"
		"\n"
		"after citation\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:endhnden\n",
		html[4], plain[4])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:undo:3\n",
		html[3], plain[3])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:undo:2\n",
		html[2], plain[2])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:undo:2\n",
		html[1], plain[1])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:undo:1\n",
		html[0], plain[0])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:redo:1\n",
		html[1], plain[1])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:redo:2\n",
		html[2], plain[2])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:redo:2\n",
		html[3], plain[3])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:redo:3\n",
		html[4], plain[4])) {
		g_test_fail ();
		return;
	}
}

static void
test_cite_editing_outlook_html (TestFixture *fixture)
{
	const gchar *html[8], *plain[8];
	gint ii;

	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	html[0] = HTML_PREFIX "<div>www</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>xxx</span></p>"
				"<p class=\"MsoNormal\"><span>yyy</span></p>"
				"<ul type=\"disc\">"
					"<li class=\"MsoListParagraph\"><span>Item 1<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Item 2<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Item 3<o:p></o:p></span></li>"
				"</ul>"
				"<p class=\"MsoNormal\"><span>zzz<o:p></o:p></span></p>"
				"<p class=\"MsoNormal\"><span><o:p>&nbsp;</o:p></span></p>"
			"</div>"
		"</blockquote>"
		"<div><br></div>" HTML_SUFFIX;

	plain[0] = "www\n"
		"> xxx\n"
		"> yyy\n"
		"> * Item 1\n"
		"> * Item 2\n"
		"> * Item 3\n"
		"> zzz\n"
		"> " UNICODE_NBSP "\n";

	test_utils_insert_content (fixture,
		"<div>www</div>"
		"<blockquote type=\"cite\">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>xxx</span></p>"
				"<p class=\"MsoNormal\"><span>yyy</span></p>"
				"<ul type=\"disc\">"
					"<li class=\"MsoListParagraph\"><span>Item 1<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Item 2<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Item 3<o:p></o:p></span></li>"
				"</ul>"
				"<p class=\"MsoNormal\"><span>zzz<o:p></o:p></span></p>"
				"<p class=\"MsoNormal\"><span><o:p>&nbsp;</o:p></span></p>"
			"</div>"
		"</blockquote>"
		"<div><br></div>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		html[0], plain[0])) {
		g_test_fail ();
		return;
	}

	html[1] = HTML_PREFIX "<div>w</div>"
		"<div>Aww</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>xxx</span></p>"
				"<p class=\"MsoNormal\"><span>yyy</span></p>"
				"<ul type=\"disc\">"
					"<li class=\"MsoListParagraph\"><span>Item 1<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Item 2<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Item 3<o:p></o:p></span></li>"
				"</ul>"
				"<p class=\"MsoNormal\"><span>zzz<o:p></o:p></span></p>"
				"<p class=\"MsoNormal\"><span><o:p>&nbsp;</o:p></span></p>"
			"</div>"
		"</blockquote>"
		"<div><br></div>" HTML_SUFFIX;

	plain[1] = "w\n"
		"Aww\n"
		"> xxx\n"
		"> yyy\n"
		"> * Item 1\n"
		"> * Item 2\n"
		"> * Item 3\n"
		"> zzz\n"
		"> " UNICODE_NBSP "\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:Chcrn\n"
		"type:A",
		html[1], plain[1])) {
		g_test_fail ();
		return;
	}

	html[2] = HTML_PREFIX "<div>w</div>"
		"<div>Aww</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>x</span></p>"
				"<p class=\"MsoNormal\"><span></span></p>"
			"</div>"
		"</blockquote>"
		"<div>B</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>xx</span></p>"
				"<p class=\"MsoNormal\"><span>yyy</span></p>"
				"<ul type=\"disc\">"
					"<li class=\"MsoListParagraph\"><span>Item 1<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Item 2<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Item 3<o:p></o:p></span></li>"
				"</ul>"
				"<p class=\"MsoNormal\"><span>zzz<o:p></o:p></span></p>"
				"<p class=\"MsoNormal\"><span><o:p>&nbsp;</o:p></span></p>"
			"</div>"
		"</blockquote>"
		"<div><br></div>" HTML_SUFFIX;

	plain[2] = "w\n"
		"Aww\n"
		"> x\n"
		"B\n"
		"> xx\n"
		"> yyy\n"
		"> * Item 1\n"
		"> * Item 2\n"
		"> * Item 3\n"
		"> zzz\n"
		"> " UNICODE_NBSP "\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:drn\n"
		"type:B",
		html[2], plain[2])) {
		g_test_fail ();
		return;
	}

	html[3] = HTML_PREFIX "<div>w</div>"
		"<div>Aww</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>x</span></p>"
				"<p class=\"MsoNormal\"><span></span></p>"
			"</div>"
		"</blockquote>"
		"<div>B</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>xx</span></p>"
				"<p class=\"MsoNormal\"><span>yyy</span></p>"
				"<ul type=\"disc\">"
					"<li class=\"MsoListParagraph\"><span>Item 1<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Ite</span></li>"
					"<li class=\"MsoListParagraph\"><span>Cm 2<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Item 3<o:p></o:p></span></li>"
				"</ul>"
				"<p class=\"MsoNormal\"><span>zzz<o:p></o:p></span></p>"
				"<p class=\"MsoNormal\"><span><o:p>&nbsp;</o:p></span></p>"
			"</div>"
		"</blockquote>"
		"<div><br></div>" HTML_SUFFIX;

	plain[3] = "w\n"
		"Aww\n"
		"> x\n"
		"B\n"
		"> xx\n"
		"> yyy\n"
		"> * Item 1\n"
		"> * Ite\n"
		"> * Cm 2\n"
		"> * Item 3\n"
		"> zzz\n"
		"> " UNICODE_NBSP "\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:ddddrrrn\n"
		"type:C",
		html[3], plain[3])) {
		g_test_fail ();
		return;
	}

	html[4] = HTML_PREFIX "<div>w</div>"
		"<div>Aww</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>x</span></p>"
				"<p class=\"MsoNormal\"><span></span></p>"
			"</div>"
		"</blockquote>"
		"<div>B</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>xx</span></p>"
				"<p class=\"MsoNormal\"><span>yyy</span></p>"
				"<ul type=\"disc\">"
					"<li class=\"MsoListParagraph\"><span>Item 1<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Ite</span></li>"
					"<li class=\"MsoListParagraph\"><span><br></span></li>"
					"<li class=\"MsoListParagraph\"><span>DCm 2<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Item 3<o:p></o:p></span></li>"
				"</ul>"
				"<p class=\"MsoNormal\"><span>zzz<o:p></o:p></span></p>"
				"<p class=\"MsoNormal\"><span><o:p>&nbsp;</o:p></span></p>"
			"</div>"
		"</blockquote>"
		"<div><br></div>" HTML_SUFFIX;

	plain[4] = "w\n"
		"Aww\n"
		"> x\n"
		"B\n"
		"> xx\n"
		"> yyy\n"
		"> * Item 1\n"
		"> * Ite\n"
		"> * \n"
		"> * DCm 2\n"
		"> * Item 3\n"
		"> zzz\n"
		"> " UNICODE_NBSP "\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:hn\n"
		"type:D",
		html[4], plain[4])) {
		g_test_fail ();
		return;
	}

	html[5] = HTML_PREFIX "<div>w</div>"
		"<div>Aww</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>x</span></p>"
				"<p class=\"MsoNormal\"><span></span></p>"
			"</div>"
		"</blockquote>"
		"<div>B</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>xx</span></p>"
				"<p class=\"MsoNormal\"><span>yyy</span></p>"
				"<ul type=\"disc\">"
					"<li class=\"MsoListParagraph\"><span>Item 1<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Ite</span></li>"
				"</ul>"
			"</div>"
		"</blockquote>"
		"<div>E</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<ul type=\"disc\">"
					"<li class=\"MsoListParagraph\"><span>DCm 2<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Item 3<o:p></o:p></span></li>"
				"</ul>"
				"<p class=\"MsoNormal\"><span>zzz<o:p></o:p></span></p>"
				"<p class=\"MsoNormal\"><span><o:p>&nbsp;</o:p></span></p>"
			"</div>"
		"</blockquote>"
		"<div><br></div>" HTML_SUFFIX;

	plain[5] = "w\n"
		"Aww\n"
		"> x\n"
		"B\n"
		"> xx\n"
		"> yyy\n"
		"> * Item 1\n"
		"> * Ite\n"
		"E\n"
		"> * DCm 2\n"
		"> * Item 3\n"
		"> zzz\n"
		"> " UNICODE_NBSP "\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:un\n"
		"type:E",
		html[5], plain[5])) {
		g_test_fail ();
		return;
	}

	html[6] = HTML_PREFIX "<div>w</div>"
		"<div>Aww</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>x</span></p>"
				"<p class=\"MsoNormal\"><span></span></p>"
			"</div>"
		"</blockquote>"
		"<div>B</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>xx</span></p>"
				"<p class=\"MsoNormal\"><span>yyy</span></p>"
				"<ul type=\"disc\">"
					"<li class=\"MsoListParagraph\"><span>Item 1<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Ite</span></li>"
				"</ul>"
			"</div>"
		"</blockquote>"
		"<div>E</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<ul type=\"disc\">"
					"<li class=\"MsoListParagraph\"><span>DCm 2<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Item 3<o:p></o:p></span></li>"
				"</ul>"
				"<p class=\"MsoNormal\"><span>zz</span></p>"
				"<p class=\"MsoNormal\"><span></span></p></div>"
			"</div>"
		"</blockquote>"
		"<div>F</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>z<o:p></o:p></span></p>"
				"<p class=\"MsoNormal\"><span><o:p>&nbsp;</o:p></span></p>"
			"</div>"
		"</blockquote>"
		"<div><br></div>" HTML_SUFFIX;

	plain[6] = "w\n"
		"Aww\n"
		"> x\n"
		"B\n"
		"> xx\n"
		"> yyy\n"
		"> * Item 1\n"
		"> * Ite\n"
		"E\n"
		"> * DCm 2\n"
		"> * Item 3\n"
		"> zz\n"
		"F\n"
		"> z\n"
		"> " UNICODE_NBSP "\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:dddrrn\n"
		"type:F",
		html[6], plain[6])) {
		g_test_fail ();
		return;
	}

	html[7] = HTML_PREFIX "<div>w</div>"
		"<div>Aww</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>x</span></p>"
				"<p class=\"MsoNormal\"><span></span></p>"
			"</div>"
		"</blockquote>"
		"<div>B</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>xx</span></p>"
				"<p class=\"MsoNormal\"><span>yyy</span></p>"
				"<ul type=\"disc\">"
					"<li class=\"MsoListParagraph\"><span>Item 1<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Ite</span></li>"
				"</ul>"
			"</div>"
		"</blockquote>"
		"<div>E</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<ul type=\"disc\">"
					"<li class=\"MsoListParagraph\"><span>DCm 2<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Item 3<o:p></o:p></span></li>"
				"</ul>"
				"<p class=\"MsoNormal\"><span>zz</span></p>"
				"<p class=\"MsoNormal\"><span></span></p></div>"
			"</div>"
		"</blockquote>"
		"<div>F</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>z<o:p></o:p></span></p>"
				"<p class=\"MsoNormal\"><span><o:p>&nbsp;</o:p></span></p>"
			"</div>"
		"</blockquote>"
		"<div><br></div>"
		"<div>G</div>" HTML_SUFFIX;

	plain[7] = "w\n"
		"Aww\n"
		"> x\n"
		"B\n"
		"> xx\n"
		"> yyy\n"
		"> * Item 1\n"
		"> * Ite\n"
		"E\n"
		"> * DCm 2\n"
		"> * Item 3\n"
		"> zz\n"
		"F\n"
		"> z\n"
		"> " UNICODE_NBSP "\n"
		"\n"
		"G\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:dddn\n"
		"type:G",
		html[7], plain[7])) {
		g_test_fail ();
		return;
	}

	for (ii = 6; ii >= 0; ii--) {
		if (!test_utils_run_simple_test (fixture, "undo:undo:2", html[ii], plain[ii])) {
			g_test_fail ();
			return;
		}
	}

	for (ii = 1; ii <= 7; ii++) {
		if (!test_utils_run_simple_test (fixture, "undo:redo:2", html[ii], plain[ii])) {
			g_test_fail ();
			return;
		}
	}
}

static void
test_cite_editing_outlook_plain (TestFixture *fixture)
{
	const gchar *html[8], *plain[8];
	gint ii;

	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	html[0] = HTML_PREFIX "<div style=\"width: 71ch;\">www</div>"
		"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "xxx</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "yyy</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Item 1</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Item 2</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Item 3</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "zzz</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "&nbsp;</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\"><br></div>" HTML_SUFFIX;

	plain[0] = "www\n"
		"> xxx\n"
		"> yyy\n"
		"> " UNICODE_NBSP "* Item 1\n"
		"> " UNICODE_NBSP "* Item 2\n"
		"> " UNICODE_NBSP "* Item 3\n"
		"> zzz\n"
		"> " UNICODE_NBSP "\n";

	test_utils_insert_content (fixture,
		"<div>www</div>"
		"<blockquote type=\"cite\">"
			"<div class=\"WordSection1\">"
				"<p class=\"MsoNormal\"><span>xxx</span></p>"
				"<p class=\"MsoNormal\"><span>yyy</span></p>"
				"<ul type=\"disc\">"
					"<li class=\"MsoListParagraph\"><span>Item 1<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Item 2<o:p></o:p></span></li>"
					"<li class=\"MsoListParagraph\"><span>Item 3<o:p></o:p></span></li>"
				"</ul>"
				"<p class=\"MsoNormal\"><span>zzz<o:p></o:p></span></p>"
				"<p class=\"MsoNormal\"><span><o:p>&nbsp;</o:p></span></p>"
			"</div>"
		"</blockquote>"
		"<div><br></div>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain",
		html[0], plain[0])) {
		g_test_fail ();
		return;
	}

	html[1] = HTML_PREFIX "<div style=\"width: 71ch;\">w</div>"
		"<div style=\"width: 71ch;\">Aww</div>"
		"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "xxx</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "yyy</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Item 1</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Item 2</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Item 3</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "zzz</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "&nbsp;</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\"><br></div>" HTML_SUFFIX;

	plain[1] = "w\n"
		"Aww\n"
		"> xxx\n"
		"> yyy\n"
		"> " UNICODE_NBSP "* Item 1\n"
		"> " UNICODE_NBSP "* Item 2\n"
		"> " UNICODE_NBSP "* Item 3\n"
		"> zzz\n"
		"> " UNICODE_NBSP "\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:Chcrn\n"
		"type:A",
		html[1], plain[1])) {
		g_test_fail ();
		return;
	}

	html[2] = HTML_PREFIX "<div style=\"width: 71ch;\">w</div>"
		"<div style=\"width: 71ch;\">Aww</div>"
		"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "x</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\">B</div>"
		"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "xx</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "yyy</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Item 1</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Item 2</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Item 3</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "zzz</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "&nbsp;</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\"><br></div>" HTML_SUFFIX;

	plain[2] = "w\n"
		"Aww\n"
		"> x\n"
		"B\n"
		"> xx\n"
		"> yyy\n"
		"> " UNICODE_NBSP "* Item 1\n"
		"> " UNICODE_NBSP "* Item 2\n"
		"> " UNICODE_NBSP "* Item 3\n"
		"> zzz\n"
		"> " UNICODE_NBSP "\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:drn\n"
		"type:B",
		html[2], plain[2])) {
		g_test_fail ();
		return;
	}

	html[3] = HTML_PREFIX "<div style=\"width: 71ch;\">w</div>"
		"<div style=\"width: 71ch;\">Aww</div>"
		"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "x</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\">B</div>"
		"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "xx</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "yyy</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Item 1</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Ite</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\">C</div>"
		"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "m 2</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Item 3</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "zzz</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "&nbsp;</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\"><br></div>" HTML_SUFFIX;

	plain[3] = "w\n"
		"Aww\n"
		"> x\n"
		"B\n"
		"> xx\n"
		"> yyy\n"
		"> " UNICODE_NBSP "* Item 1\n"
		"> " UNICODE_NBSP "* Ite\n"
		"C\n"
		"> m 2\n"
		"> " UNICODE_NBSP "* Item 3\n"
		"> zzz\n"
		"> " UNICODE_NBSP "\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:ddddrrrrrrn\n"
		"type:C",
		html[3], plain[3])) {
		g_test_fail ();
		return;
	}

	html[4] = HTML_PREFIX "<div style=\"width: 71ch;\">w</div>"
		"<div style=\"width: 71ch;\">Aww</div>"
		"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "x</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\">B</div>"
		"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "xx</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "yyy</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Item 1</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Ite</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\">C</div>"
		"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "m 2</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Item 3</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "zz</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\">D</div>"
		"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "z</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "&nbsp;</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\"><br></div>" HTML_SUFFIX;

	plain[4] = "w\n"
		"Aww\n"
		"> x\n"
		"B\n"
		"> xx\n"
		"> yyy\n"
		"> " UNICODE_NBSP "* Item 1\n"
		"> " UNICODE_NBSP "* Ite\n"
		"C\n"
		"> m 2\n"
		"> " UNICODE_NBSP "* Item 3\n"
		"> zz\n"
		"D\n"
		"> z\n"
		"> " UNICODE_NBSP "\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:dddrrn\n"
		"type:D",
		html[4], plain[4])) {
		g_test_fail ();
		return;
	}

	html[5] = HTML_PREFIX "<div style=\"width: 71ch;\">w</div>"
		"<div style=\"width: 71ch;\">Aww</div>"
		"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "x</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\">B</div>"
		"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "xx</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "yyy</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Item 1</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Ite</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\">C</div>"
		"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "m 2</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) UNICODE_NBSP "* Item 3</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "zz</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\">D</div>"
		"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "z</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "&nbsp;</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">E</div>" HTML_SUFFIX;

	plain[5] = "w\n"
		"Aww\n"
		"> x\n"
		"B\n"
		"> xx\n"
		"> yyy\n"
		"> " UNICODE_NBSP "* Item 1\n"
		"> " UNICODE_NBSP "* Ite\n"
		"C\n"
		"> m 2\n"
		"> " UNICODE_NBSP "* Item 3\n"
		"> zz\n"
		"D\n"
		"> z\n"
		"> " UNICODE_NBSP "\n"
		"\n"
		"E\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:dddn\n"
		"type:E",
		html[5], plain[5])) {
		g_test_fail ();
		return;
	}

	for (ii = 4; ii >= 0; ii--) {
		if (!test_utils_run_simple_test (fixture, "undo:undo:2", html[ii], plain[ii])) {
			g_test_fail ();
			return;
		}
	}

	for (ii = 1; ii <= 4; ii++) {
		if (!test_utils_run_simple_test (fixture, "undo:redo:2", html[ii], plain[ii])) {
			g_test_fail ();
			return;
		}
	}
}

static void
test_cite_nested_html (TestFixture *fixture)
{
	const gchar *html[2], *plain[2];

	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<blockquote type=\"cite\">"
			"<div>a</div>"
			"<div>b</div>"
			"<blockquote type=\"cite\">"
				"<div>aa</div>"
				"<div>bb</div>"
				"<div>cc</div>"
				"<div>dd</div>"
			"</blockquote>"
			"<div>c</div>"
			"<div>d</div>"
		"</blockquote>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	html[0] = HTML_PREFIX "<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div>a</div>"
			"<div>b</div>"
			"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
				"<div>aa</div>"
				"<div>bb</div>"
				"<div>cc</div>"
				"<div>dd</div>"
			"</blockquote>"
			"<div>c</div>"
			"<div>d</div>"
		"</blockquote>" HTML_SUFFIX;

	plain[0] = "> a\n"
		"> b\n"
		"> > aa\n"
		"> > bb\n"
		"> > cc\n"
		"> > dd\n"
		"> c\n"
		"> d\n";

	if (!test_utils_run_simple_test (fixture, "", html[0], plain[0])) {
		g_test_fail ();
		return;
	}

	html[1] = HTML_PREFIX "<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<div>a</div>"
			"<div>b</div>"
			"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
				"<div>aa</div>"
				"<div>bb</div>"
				"<div><br></div>"
			"</blockquote>"
		"</blockquote>"
		"<div>X</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
				"<div>cc</div>"
				"<div>dd</div>"
			"</blockquote>"
			"<div>c</div>"
			"<div>d</div>"
		"</blockquote>" HTML_SUFFIX;

	plain[1] = "> a\n"
		"> b\n"
		"> > aa\n"
		"> > bb\n"
		"> > \n"
		"X\n"
		"> > cc\n"
		"> > dd\n"
		"> c\n"
		"> d\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:ddddn\n"
		"type:X",
		html[1], plain[1])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture, "undo:undo:2", html[0], plain[0])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture, "undo:redo:2", html[1], plain[1]))
		g_test_fail ();
}

static void
test_cite_nested_plain (TestFixture *fixture)
{
	const gchar *html[2], *plain[2];

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<blockquote type=\"cite\">"
			"<div>a</div>"
			"<div>b</div>"
			"<blockquote type=\"cite\">"
				"<div>aa</div>"
				"<div>bb</div>"
				"<div>cc</div>"
				"<div>dd</div>"
			"</blockquote>"
			"<div>c</div>"
			"<div>d</div>"
		"</blockquote>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	html[0] = HTML_PREFIX "<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "a</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "b</div>"
			"<blockquote type=\"cite\">"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "aa</div>"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "bb</div>"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "cc</div>"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "dd</div>"
			"</blockquote>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "c</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "d</div>"
		"</blockquote>" HTML_SUFFIX;

	plain[0] = "> a\n"
		"> b\n"
		"> > aa\n"
		"> > bb\n"
		"> > cc\n"
		"> > dd\n"
		"> c\n"
		"> d\n";

	if (!test_utils_run_simple_test (fixture, "", html[0], plain[0])) {
		g_test_fail ();
		return;
	}

	html[1] = HTML_PREFIX "<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "a</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "b</div>"
			"<blockquote type=\"cite\">"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "aa</div>"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "bb</div>"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "<br></div>"
			"</blockquote>"
		"</blockquote>"
		"<div style=\"width: 71ch;\">X</div>"
		"<blockquote type=\"cite\">"
			"<blockquote type=\"cite\">"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "cc</div>"
				"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "dd</div>"
			"</blockquote>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "c</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR) "d</div>"
		"</blockquote>" HTML_SUFFIX;

	plain[1] = "> a\n"
		"> b\n"
		"> > aa\n"
		"> > bb\n"
		"> > \n"
		"X\n"
		"> > cc\n"
		"> > dd\n"
		"> c\n"
		"> d\n";

	if (!test_utils_run_simple_test (fixture,
		"seq:ddddn\n"
		"type:X",
		html[1], plain[1])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture, "undo:undo:2", html[0], plain[0])) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture, "undo:redo:2", html[1], plain[1]))
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
		"some text\n"))
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
		"some text to delete\n"))
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
		"some text to delete\n"))
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
		"some text to delete\n"))
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
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:2\n"

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
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:2\n"

		"action:underline\n"
		"type:underline\n"
		"undo:save\n" /* 2 */
		"undo:undo:9\n"
		"undo:test:2\n"
		"undo:redo:9\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:9\n"
		"type:underline\n"
		"seq:CSlsc\n"
		"action:underline\n"
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:2\n"

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
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:2\n"

		"font-name:monospace\n"
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
		"font-name:monospace\n"
		"undo:save\n" /* 2 */
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:2\n",
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
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:2\n",

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
		"undo:undo:2\n"
		"undo:test:2\n"
		"undo:redo:2\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:2\n"

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
		"undo:undo:1\n"
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
		"undo:undo:18\n" /* 6x action:indent, 3x type "level X\\n" (= 4 undo steps) */
		"undo:test:2\n"
		"undo:redo:18\n"
		"undo:test\n"
		"undo:drop\n" /* drop the save 2 */
		"undo:undo:18\n",

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
		"undo:undo:2\n"
		"undo:redo:4\n"
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
		HTML_PREFIX "<div style=\"width: 71ch;\">URL:</div>"
		"<div style=\"width: 71ch;\"><a href=\"http://www.gnome.org\">http://www.gnome.org</a></div>"
		"<div style=\"width: 71ch;\"><br></div>" HTML_SUFFIX,
		"URL:\nhttp://www.gnome.org\n"))
		g_test_fail ();
}

static void
test_delete_quoted (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><pre>a\n"
		"b\n"
		"c\n"
		"</pre>"
		"<span class=\"-x-evo-to-body\" data-credits=\"On Thu, 2016-09-15 at 08:08 -0400, user wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:ddddd\n"
		"undo:save\n" /* 1 */
		"seq:SusDdd\n"
		"type:b\n"
		"undo:undo:2\n"
		"undo:test\n"
		"undo:redo:2",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Thu, 2016-09-15 at 08:08 -0400, user wrote:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "a</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "b</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"On Thu, 2016-09-15 at 08:08 -0400, user wrote:\n"
		"> a\n"
		"> b\n"))
		g_test_fail ();
}

static void
test_delete_after_quoted (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-wrap-quoted-text-in-replies", FALSE);

	test_utils_insert_content (fixture,
		"<body><pre>a\n"
		"b\n"
		"\n</pre>"
		"<span class=\"-x-evo-to-body\" data-credits=\"On Thu, 2016-09-15 at 08:08 -0400, user wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:dddb\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Thu, 2016-09-15 at 08:08 -0400, user wrote:</div>"
		"<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "a</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "b</pre>"
		"</blockquote>"
		HTML_SUFFIX,
		"On Thu, 2016-09-15 at 08:08 -0400, user wrote:\n"
		"> a\n"
		"> b\n"))
		g_test_fail ();
}

static void
test_delete_quoted_selection (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("line 1\n\nline 2\n", FALSE);

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-paste-plain-prefer-pre", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:line 0\n"
		"seq:n\n"
		"action:paste-quote\n"
		"undo:save\n" /* 1 */
		"seq:SuusD\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:undo\n"
		"seq:r\n"
		"type:X\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">line 0</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "line 1</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "line 2</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "X</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"line 0\n"
		"> line 1\n"
		"> \n"
		"> line 2\n"
		"> X\n")) {
		g_test_fail ();
		return;
	}

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-paste-plain-prefer-pre", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"seq:cD\n"
		"type:line 0\n"
		"seq:n\n"
		"action:paste-quote\n"
		"undo:save\n" /* 1 */
		"seq:SuusD\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:undo\n"
		"seq:r\n"
		"type:X\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">line 0</div>"
		"<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "line 1</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "<br></pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "line 2</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "X</pre>"
		"</blockquote>"
		HTML_SUFFIX,
		"line 0\n"
		"> line 1\n"
		"> \n"
		"> line 2\n"
		"> X\n"))
		g_test_fail ();
}

static void
test_delete_quoted_multiselect_pre (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("line 1\nline 2\nline 3", FALSE);

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-paste-plain-prefer-pre", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"action:paste-quote\n"
		"type:X\n"
		"undo:save\n" /* 1 */
		"seq:ChcrrSdsD\n",
		HTML_PREFIX "<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<pre>line 2</pre>"
		"<pre>line 3X</pre>"
		"</blockquote>"
		HTML_SUFFIX,
		"> line 2\n"
		"> line 3X\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:drop:1\n"
		"seq:Cec\n" /* Go to the end of the document (Ctrl+End) */
		"type:\\nY\n",
		HTML_PREFIX "<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<pre>line 2</pre>"
		"<pre>line 3X</pre>"
		"</blockquote>"
		"<div>Y</div>"
		HTML_SUFFIX,
		"> line 2\n"
		"> line 3X\n"
		"Y\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture, "<body></body>", E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"action:paste-quote\n"
		"type:X\n"
		"undo:save\n" /* 1 */
		"seq:ChcrrSdsD\n",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "line 2</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "line 3X</pre>"
		"</blockquote>"
		HTML_SUFFIX,
		"> line 2\n"
		"> line 3X\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"seq:Cec\n" /* Go to the end of the document (Ctrl+End) */
		"type:\\nY\n",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "line 2</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "line 3X</pre>"
		"</blockquote>"
		"<div style=\"width: 71ch;\">Y</div>"
		HTML_SUFFIX,
		"> line 2\n"
		"> line 3X\n"
		"Y\n"))
		g_test_fail ();
}

static void
test_delete_quoted_multiselect_div (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("line 1\nline 2\nline 3", FALSE);

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-paste-plain-prefer-pre", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"action:paste-quote\n"
		"type:X\n"
		"undo:save\n" /* 1 */
		"seq:ChcrrSdsD\n",
		HTML_PREFIX "<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>line 2</div>"
		"<div>line 3X</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"> line 2\n"
		"> line 3X\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"undo:drop:1\n"
		"seq:Cec\n" /* Go to the end of the document (Ctrl+End) */
		"type:\\nY\n",
		HTML_PREFIX "<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>line 2</div>"
		"<div>line 3X</div>"
		"</blockquote>"
		"<div>Y</div>"
		HTML_SUFFIX,
		"> line 2\n"
		"> line 3X\n"
		"Y\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture, "<body></body>", E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"action:paste-quote\n"
		"type:X\n"
		"undo:save\n" /* 1 */
		"seq:ChcrrSdsD\n",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "line 2</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "line 3X</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"> line 2\n"
		"> line 3X\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"seq:Cec\n" /* Go to the end of the document (Ctrl+End) */
		"type:\\nY\n",
		HTML_PREFIX "<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "line 2</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "line 3X</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\">Y</div>"
		HTML_SUFFIX,
		"> line 2\n"
		"> line 3X\n"
		"Y\n"))
		g_test_fail ();
}

static void
test_replace_dialog (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:text to replace\n"
		"undo:save\n"	/* 1 */
		"seq:h\n"
		"action:show-replace\n"
		"type:to\t2\n"
		"seq:A\n" /* Press 'Alt+R' to press 'Replace' button */
		"type:r\n"
		"seq:a\n"
		"seq:^\n" /* Close the dialog */
		"undo:undo\n"
		"undo:test:1\n"
		"undo:redo\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">text 2 replace</div>" HTML_SUFFIX,
		"text 2 replace\n"))
		g_test_fail ();
}

static void
test_replace_dialog_all (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:text to replace\n"
		"undo:save\n"	/* 1 */
		"seq:h\n"
		"action:show-replace\n"
		"type:e\t3\n"
		"seq:A\n" /* Press 'Alt+A' to press 'Replace All' button */
		"type:a\n"
		"seq:a\n"
		"seq:^\n" /* Close the dialog */
		"undo:undo\n"
		"undo:test:1\n"
		"undo:redo\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">t3xt to r3plac3</div>" HTML_SUFFIX,
		"t3xt to r3plac3\n"))
		g_test_fail ();
}

static void
test_wrap_basic (TestFixture *fixture)
{
	test_utils_fixture_change_setting_int32 (fixture, "org.gnome.evolution.mail", "composer-word-wrap-length", 10);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:123 456 789 123 456\\n\n"
		"type:a b\\n\n"
		"type:c \\n\n"
		"type:d\\n\n"
		"type: e f\\n\n"
		"type:\\n\n"
		"type:123 456 7 8 9 12345 1 2 3 456 789\n"
		"action:select-all\n"
		"action:wrap-lines\n",
		HTML_PREFIX "<div>123 456<br>"
		"789 123<br>"
		"456 a b c <br>"
		"d  e f</div>"
		"<div><br></div>"
		"<div>123 456 7<br>"
		"8 9 12345<br>"
		"1 2 3 456<br>"
		"789</div>" HTML_SUFFIX,
		"123 456\n"
		"789 123\n"
		"456 a b c \n"
		"d  e f\n"
		"\n"
		"123 456 7\n"
		"8 9 12345\n"
		"1 2 3 456\n"
		"789\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"action:select-all\n"
		"seq:D\n"
		"type:123 456 7 8 901234567890123456 1 2 3 4 5 6 7\\n\n"
		"type:1234567890123456 12345678901234567890 1 2 3 4 5 6 7 8\\n\n"
		"type:12345678 123456789 1234567890 123 456 78\n"
		"action:select-all\n"
		"action:wrap-lines\n",
		HTML_PREFIX "<div>123 456 7<br>"
		"8<br>"
		"901234567890123456<br>"
		"1 2 3 4 5<br>"
		"6 7<br>"
		"1234567890123456<br>"
		"12345678901234567890<br>"
		"1 2 3 4 5<br>"
		"6 7 8<br>"
		"12345678<br>"
		"123456789<br>"
		"1234567890<br>"
		"123 456 78</div>" HTML_SUFFIX,
		"123 456 7\n"
		"8\n"
		"9012345678\n"
		"90123456\n"
		"1 2 3 4 5\n"
		"6 7\n"
		"1234567890\n"
		"123456\n"
		"1234567890\n"
		"1234567890\n"
		"1 2 3 4 5\n"
		"6 7 8\n"
		"12345678\n"
		"123456789\n"
		"1234567890\n"
		"123 456 78\n")) {
		g_test_fail ();
		return;
	}
}

static void
test_wrap_nested (TestFixture *fixture)
{
	test_utils_fixture_change_setting_int32 (fixture, "org.gnome.evolution.mail", "composer-word-wrap-length", 10);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:123 4 \n"
		"action:bold\n"
		"type:b\n"
		"action:bold\n"
		"type: 5 67 89 \n"
		"action:bold\n"
		"type:bold text\n"
		"action:bold\n"
		"type: 123 456 \n"
		"action:italic\n"
		"type:italic text \n"
		"action:underline\n"
		"type:and underline text\n"
		"action:underline\n"
		"type: xyz\n"
		"action:italic\n"
		"type: 7 8 9 1 2 3 4 5\n"
		"action:select-all\n"
		"action:wrap-lines\n",
		HTML_PREFIX "<div>123 4 <b>b</b> 5<br>"
		"67 89 <b>bold<br>"
		"text</b> 123<br>"
		"456 <i>italic<br>"
		"text <u>and<br>"
		"underline<br>"
		"text</u> xyz</i> 7<br>"
		"8 9 1 2 3<br>"
		"4 5</div>" HTML_SUFFIX,
		"123 4 b 5\n"
		"67 89 bold\n"
		"text 123\n"
		"456 italic\n"
		"text and\n"
		"underline\n"
		"text xyz 7\n"
		"8 9 1 2 3\n"
		"4 5\n"))
		g_test_fail ();
}

static void
test_pre_split_simple_html (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<pre>line 1\n"
		"line 2</pre>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<pre>line 1</pre>"
		"<pre>line 2</pre>"
		HTML_SUFFIX,
		"line 1\n"
		"line 2\n"))
		g_test_fail ();
}

static void
test_pre_split_simple_plain (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<pre>line 1\n"
		"line 2</pre>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<pre>line 1</pre>"
		"<pre>line 2</pre>"
		HTML_SUFFIX,
		"line 1\n"
		"line 2\n"))
		g_test_fail ();
}

static void
test_pre_split_complex_html (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<div>leading text</div>"
		"<pre><b>bold1</b><blockquote type=\"cite\">text 1\n"
		"text 2\n"
		"text 3</blockquote>"
		"text A<i>italic</i>text B\n"
		"<b>bold2</b>"
		"</pre>"
		"<div>mid text</div>"
		"<pre><blockquote type=\"cite\">level 1\n"
		"E-mail: &lt;<a href=\"mailto:user@no.where\">user@no.where</a>&gt; line\n"
		"Phone: 1234567890\n"
		"<div>div in\npre</div>"
		"<blockquote type=\"cite\">level 2\n"
		"\n"
		"level 2\n</blockquote>"
		"</blockquote></pre>"
		"<pre>text\n"
		"text 2<i>italic 1</i>\n"
		"<i>italic 2</i> text 3\n"
		"pre <i>imid</i> pos\n"
		"<i>ipre</i> mid <i>ipos</i>\n"
		"<i>ipre2</i> mid2 <i>i<b>pos</b>2</i> pos2\n</pre>"
		"<div>closing text</div>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div>leading text</div>"
		"<pre><b>bold1</b></pre>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<pre>text 1</pre>"
			"<pre>text 2</pre>"
			"<pre>text 3</pre>"
		"</blockquote>"
		"<pre>text A<i>italic</i>text B</pre>"
		"<pre><b>bold2</b></pre>"
		"<div>mid text</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<pre>level 1</pre>"
			"<pre>E-mail: &lt;<a href=\"mailto:user@no.where\">user@no.where</a>&gt; line</pre>"
			"<pre>Phone: 1234567890</pre>"
			"<pre><div>div in pre</div></pre>"
			"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
				"<pre>level 2</pre>"
				"<pre><br></pre>"
				"<pre>level 2</pre>"
			"</blockquote>"
		"</blockquote>"
		"<pre>text</pre>"
		"<pre>text 2<i>italic 1</i></pre>"
		"<pre><i>italic 2</i> text 3</pre>"
		"<pre>pre <i>imid</i> pos</pre>"
		"<pre><i>ipre</i> mid <i>ipos</i></pre>"
		"<pre><i>ipre2</i> mid2 <i>i<b>pos</b>2</i> pos2</pre>"
		"<div>closing text</div>"
		HTML_SUFFIX,
		"leading text\n"
		"bold1\n"
		"> text 1\n"
		"> text 2\n"
		"> text 3\n"
		"text Aitalictext B\n"
		"bold2\n"
		"mid text\n"
		"> level 1\n"
		"> E-mail: <user@no.where> line\n"
		"> Phone: 1234567890\n"
		"> div in pre\n"
		"> > level 2\n"
		"> > \n"
		"> > level 2\n"
		"text\n"
		"text 2italic 1\n"
		"italic 2 text 3\n"
		"pre imid pos\n"
		"ipre mid ipos\n"
		"ipre2 mid2 ipos2 pos2\n"
		"closing text\n"))
		g_test_fail ();
}

static void
test_pre_split_complex_plain (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<div>leading text</div>"
		"<pre><b>bold1</b><blockquote type=\"cite\">text 1\n"
		"text 2\n"
		"text 3</blockquote>"
		"text A<i>italic</i>text B\n"
		"<b>bold2</b>"
		"</pre>"
		"<div>mid text</div>"
		"<pre><blockquote type=\"cite\">level 1\n"
		"E-mail: &lt;<a href=\"mailto:user@no.where\">user@no.where</a>&gt; line\n"
		"Phone: 1234567890\n"
		"<div>div in\npre</div>"
		"<blockquote type=\"cite\">level 2\n"
		"\n"
		"level 2\n</blockquote>"
		"</blockquote></pre>"
		"<pre>text\n"
		"text 2<i>italic 1</i>\n"
		"<i>italic 2</i> text 3\n"
		"pre <i>imid</i> pos\n"
		"<i>ipre</i> mid <i>ipos</i>\n"
		"<i>ipre2</i> mid2 <i>i<b>pos</b>2</i> pos2\n</pre>"
		"<div>closing text</div>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style='width: 71ch;'>leading text</div>"
		"<pre><b>bold1</b></pre>"
		"<blockquote type=\"cite\">"
			"<pre>" QUOTE_SPAN (QUOTE_CHR) "text 1</pre>"
			"<pre>" QUOTE_SPAN (QUOTE_CHR) "text 2</pre>"
			"<pre>" QUOTE_SPAN (QUOTE_CHR) "text 3</pre>"
		"</blockquote>"
		"<pre>text A<i>italic</i>text B</pre>"
		"<pre><b>bold2</b></pre>"
		"<div style='width: 71ch;'>mid text</div>"
		"<blockquote type=\"cite\">"
			"<pre>" QUOTE_SPAN (QUOTE_CHR) "level 1</pre>"
			"<pre>" QUOTE_SPAN (QUOTE_CHR) "E-mail: &lt;<a href=\"mailto:user@no.where\">user@no.where</a>&gt; line</pre>"
			"<pre>" QUOTE_SPAN (QUOTE_CHR) "Phone: 1234567890</pre>"
			"<pre>" QUOTE_SPAN (QUOTE_CHR) "div in pre</pre>"
			"<blockquote type=\"cite\">"
				"<pre>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "level 2</pre>"
				"<pre>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "<br></pre>"
				"<pre>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "level 2</pre>"
			"</blockquote>"
		"</blockquote>"
		"<pre>text</pre>"
		"<pre>text 2<i>italic 1</i></pre>"
		"<pre><i>italic 2</i> text 3</pre>"
		"<pre>pre <i>imid</i> pos</pre>"
		"<pre><i>ipre</i> mid <i>ipos</i></pre>"
		"<pre><i>ipre2</i> mid2 <i>i<b>pos</b>2</i> pos2</pre>"
		"<div style='width: 71ch;'>closing text</div>"
		HTML_SUFFIX,
		"leading text\n"
		"bold1\n"
		"> text 1\n"
		"> text 2\n"
		"> text 3\n"
		"text Aitalictext B\n"
		"bold2\n"
		"mid text\n"
		"> level 1\n"
		"> E-mail: <user@no.where> line\n"
		"> Phone: 1234567890\n"
		"> div in pre\n"
		"> > level 2\n"
		"> > \n"
		"> > level 2\n"
		"text\n"
		"text 2italic 1\n"
		"italic 2 text 3\n"
		"pre imid pos\n"
		"ipre mid ipos\n"
		"ipre2 mid2 ipos2 pos2\n"
		"closing text\n"))
		g_test_fail ();
}

gint
main (gint argc,
      gchar *argv[])
{
	gchar *test_keyfile_filename;
	gint cmd_delay = -1;
	gboolean background = FALSE;
	gboolean multiple_web_processes = FALSE;
	gboolean keep_going = FALSE;
	GOptionEntry entries[] = {
		{ "cmd-delay", '\0', 0,
		  G_OPTION_ARG_INT, &cmd_delay,
		  "Specify delay, in milliseconds, to use during processing commands. Default is 25 ms.",
		  NULL },
		{ "background", '\0', 0,
		  G_OPTION_ARG_NONE, &background,
		  "Use to run tests in the background, not stealing focus and such.",
		  NULL },
		{ "multiple-web-processes", '\0', 0,
		  G_OPTION_ARG_NONE, &multiple_web_processes,
		  "Use multiple web processes for each test being run. Default is to use single web process.",
		  NULL },
		/* Cannot use --keep-going, it's taken by glib */
		{ "e-keep-going", '\0', 0,
		  G_OPTION_ARG_NONE, &keep_going,
		  "Use to not abort failed tests, but keep going through all tests.",
		  NULL },
		{ NULL }
	};
	GApplication *application; /* Needed for WebKitGTK sandboxing */
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
	g_setenv ("E_HTML_EDITOR_TEST_SOURCES", "1", FALSE);
	g_setenv ("EVOLUTION_SOURCE_WEBKITDATADIR", EVOLUTION_SOURCE_WEBKITDATADIR, FALSE);
	g_setenv (TEST_KEYFILE_SETTINGS_FILENAME_ENVVAR, test_keyfile_filename, TRUE);

	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=");

	if (keep_going)
		g_test_set_nonfatal_assertions ();

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
	test_utils_set_multiple_web_processes (multiple_web_processes);
	test_utils_set_keep_going (keep_going);

	e_util_init_main_thread (NULL);
	e_passwords_init ();

	application = g_application_new ("org.gnome.Evolution.test-html-editor-units", G_APPLICATION_FLAGS_NONE);

	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (), EVOLUTION_ICONDIR);
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (), E_DATA_SERVER_ICONDIR);

	modules = e_module_load_all_in_directory (EVOLUTION_MODULEDIR);
	g_list_free_full (modules, (GDestroyNotify) g_type_module_unuse);

	test_utils_add_test ("/create/editor", test_create_editor);
	test_utils_add_test ("/style/bold-selection", test_style_bold_selection);
	test_utils_add_test ("/style/bold-typed", test_style_bold_typed);
	test_utils_add_test ("/style/italic-selection", test_style_italic_selection);
	test_utils_add_test ("/style/italic-typed", test_style_italic_typed);
	test_utils_add_test ("/style/underline-selection", test_style_underline_selection);
	test_utils_add_test ("/style/underline-typed", test_style_underline_typed);
	test_utils_add_test ("/style/strikethrough-selection", test_style_strikethrough_selection);
	test_utils_add_test ("/style/strikethrough-typed", test_style_strikethrough_typed);
	test_utils_add_test ("/style/monospace-selection", test_style_monospace_selection);
	test_utils_add_test ("/style/monospace-typed", test_style_monospace_typed);
	test_utils_add_test ("/justify/selection", test_justify_selection);
	test_utils_add_test ("/justify/typed", test_justify_typed);
	test_utils_add_test ("/indent/selection", test_indent_selection);
	test_utils_add_test ("/indent/typed", test_indent_typed);
	test_utils_add_test ("/font/size-selection", test_font_size_selection);
	test_utils_add_test ("/font/size-typed", test_font_size_typed);
	test_utils_add_test ("/font/color-selection", test_font_color_selection);
	test_utils_add_test ("/font/color-typed", test_font_color_typed);
	test_utils_add_test ("/list/bullet-plain", test_list_bullet_plain);
	test_utils_add_test ("/list/bullet-html", test_list_bullet_html);
	test_utils_add_test ("/list/bullet-change", test_list_bullet_change);
	test_utils_add_test ("/list/bullet-html-from-block", test_list_bullet_html_from_block);
	test_utils_add_test ("/list/alpha-html", test_list_alpha_html);
	test_utils_add_test ("/list/alpha-plain", test_list_alpha_plain);
	test_utils_add_test ("/list/number-html", test_list_number_html);
	test_utils_add_test ("/list/number-plain", test_list_number_plain);
	test_utils_add_test ("/list/roman-html", test_list_roman_html);
	test_utils_add_test ("/list/roman-plain", test_list_roman_plain);
	test_utils_add_test ("/list/multi-html", test_list_multi_html);
	test_utils_add_test ("/list/multi-plain", test_list_multi_plain);
	test_utils_add_test ("/list/multi-change-html", test_list_multi_change_html);
	test_utils_add_test ("/list/multi-change-plain", test_list_multi_change_plain);
	test_utils_add_test ("/list/indent-same-html", test_list_indent_same_html);
	test_utils_add_test ("/list/indent-same-plain", test_list_indent_same_plain);
	test_utils_add_test ("/list/indent-different-html", test_list_indent_different_html);
	test_utils_add_test ("/list/indent-different-plain", test_list_indent_different_plain);
	test_utils_add_test ("/list/indent-multi-html", test_list_indent_multi_html);
	test_utils_add_test ("/list/indent-multi-plain", test_list_indent_multi_plain);
	test_utils_add_test ("/list/indent-nested-html", test_list_indent_nested_html);
	test_utils_add_test ("/list/indent-nested-plain", test_list_indent_nested_plain);
	test_utils_add_test ("/link/insert-dialog", test_link_insert_dialog);
	test_utils_add_test ("/link/insert-dialog-selection", test_link_insert_dialog_selection);
	test_utils_add_test ("/link/insert-dialog-remove-link", test_link_insert_dialog_remove_link);
	test_utils_add_test ("/link/insert-typed", test_link_insert_typed);
	test_utils_add_test ("/link/insert-typed-change-description", test_link_insert_typed_change_description);
	test_utils_add_test ("/link/insert-typed-append", test_link_insert_typed_append);
	test_utils_add_test ("/link/insert-typed-remove", test_link_insert_typed_remove);
	test_utils_add_test ("/h-rule/insert", test_h_rule_insert);
	test_utils_add_test ("/h-rule/insert-text-after", test_h_rule_insert_text_after);
	test_utils_add_test ("/image/insert", test_image_insert);
	test_utils_add_test ("/emoticon/insert-typed", test_emoticon_insert_typed);
	test_utils_add_test ("/emoticon/insert-typed-dash", test_emoticon_insert_typed_dash);
	test_utils_add_test ("/paragraph/normal-selection", test_paragraph_normal_selection);
	test_utils_add_test ("/paragraph/normal-typed", test_paragraph_normal_typed);
	test_utils_add_test ("/paragraph/preformatted-selection", test_paragraph_preformatted_selection);
	test_utils_add_test ("/paragraph/preformatted-typed", test_paragraph_preformatted_typed);
	test_utils_add_test ("/paragraph/address-selection", test_paragraph_address_selection);
	test_utils_add_test ("/paragraph/address-typed", test_paragraph_address_typed);
	test_utils_add_test ("/paragraph/header1-selection", test_paragraph_header1_selection);
	test_utils_add_test ("/paragraph/header1-typed", test_paragraph_header1_typed);
	test_utils_add_test ("/paragraph/header2-selection", test_paragraph_header2_selection);
	test_utils_add_test ("/paragraph/header2-typed", test_paragraph_header2_typed);
	test_utils_add_test ("/paragraph/header3-selection", test_paragraph_header3_selection);
	test_utils_add_test ("/paragraph/header3-typed", test_paragraph_header3_typed);
	test_utils_add_test ("/paragraph/header4-selection", test_paragraph_header4_selection);
	test_utils_add_test ("/paragraph/header4-typed", test_paragraph_header4_typed);
	test_utils_add_test ("/paragraph/header5-selection", test_paragraph_header5_selection);
	test_utils_add_test ("/paragraph/header5-typed", test_paragraph_header5_typed);
	test_utils_add_test ("/paragraph/header6-selection", test_paragraph_header6_selection);
	test_utils_add_test ("/paragraph/header6-typed", test_paragraph_header6_typed);
	test_utils_add_test ("/paragraph/wrap-lines", test_paragraph_wrap_lines);
	test_utils_add_test ("/paste/singleline-html2html", test_paste_singleline_html2html);
	test_utils_add_test ("/paste/singleline-html2plain", test_paste_singleline_html2plain);
	test_utils_add_test ("/paste/singleline-plain2html", test_paste_singleline_plain2html);
	test_utils_add_test ("/paste/singleline-plain2plain", test_paste_singleline_plain2plain);
	test_utils_add_test ("/paste/multiline-html2html", test_paste_multiline_html2html);
	test_utils_add_test ("/paste/multiline-html2plain", test_paste_multiline_html2plain);
	test_utils_add_test ("/paste/multiline-div-html2html", test_paste_multiline_div_html2html);
	test_utils_add_test ("/paste/multiline-div-html2plain", test_paste_multiline_div_html2plain);
	test_utils_add_test ("/paste/multiline-p-html2html", test_paste_multiline_p_html2html);
	test_utils_add_test ("/paste/multiline-p-html2plain", test_paste_multiline_p_html2plain);
	test_utils_add_test ("/paste/multiline-plain2html", test_paste_multiline_plain2html);
	test_utils_add_test ("/paste/multiline-plain2plain", test_paste_multiline_plain2plain);
	test_utils_add_test ("/paste/quoted-singleline-html2html", test_paste_quoted_singleline_html2html);
	test_utils_add_test ("/paste/quoted-singleline-html2plain", test_paste_quoted_singleline_html2plain);
	test_utils_add_test ("/paste/quoted-singleline-plain2html", test_paste_quoted_singleline_plain2html);
	test_utils_add_test ("/paste/quoted-singleline-plain2plain", test_paste_quoted_singleline_plain2plain);
	test_utils_add_test ("/paste/quoted-multiline-html2html", test_paste_quoted_multiline_html2html);
	test_utils_add_test ("/paste/quoted-multiline-html2plain", test_paste_quoted_multiline_html2plain);
	test_utils_add_test ("/paste/quoted-multiline-plain2html", test_paste_quoted_multiline_plain2html);
	test_utils_add_test ("/paste/quoted-multiline-plain2plain", test_paste_quoted_multiline_plain2plain);
	test_utils_add_test ("/cite/html2plain", test_cite_html2plain);
	test_utils_add_test ("/cite/shortline", test_cite_shortline);
	test_utils_add_test ("/cite/longline", test_cite_longline);
	test_utils_add_test ("/cite/reply-html", test_cite_reply_html);
	test_utils_add_test ("/cite/reply-html-to-plain", test_cite_reply_html_to_plain);
	test_utils_add_test ("/cite/reply-plain", test_cite_reply_plain);
	test_utils_add_test ("/cite/reply-link", test_cite_reply_link);
	test_utils_add_test ("/cite/editing-html", test_cite_editing_html);
	test_utils_add_test ("/cite/editing-plain", test_cite_editing_plain);
	test_utils_add_test ("/cite/editing-outlook-html", test_cite_editing_outlook_html);
	test_utils_add_test ("/cite/editing-outlook-plain", test_cite_editing_outlook_plain);
	test_utils_add_test ("/cite/nested-html", test_cite_nested_html);
	test_utils_add_test ("/cite/nested-plain", test_cite_nested_plain);
	test_utils_add_test ("/undo/text-typed", test_undo_text_typed);
	test_utils_add_test ("/undo/text-forward-delete", test_undo_text_forward_delete);
	test_utils_add_test ("/undo/text-backward-delete", test_undo_text_backward_delete);
	test_utils_add_test ("/undo/text-cut", test_undo_text_cut);
	test_utils_add_test ("/undo/style", test_undo_style);
	test_utils_add_test ("/undo/justify", test_undo_justify);
	test_utils_add_test ("/undo/indent", test_undo_indent);
	test_utils_add_test ("/undo/link-paste-html", test_undo_link_paste_html);
	test_utils_add_test ("/undo/link-paste-plain", test_undo_link_paste_plain);
	test_utils_add_test ("/delete/quoted", test_delete_quoted);
	test_utils_add_test ("/delete/after-quoted", test_delete_after_quoted);
	test_utils_add_test ("/delete/quoted-selection", test_delete_quoted_selection);
	test_utils_add_test ("/delete/quoted-multiselect-pre", test_delete_quoted_multiselect_pre);
	test_utils_add_test ("/delete/quoted-multiselect-div", test_delete_quoted_multiselect_div);
	test_utils_add_test ("/replace/dialog", test_replace_dialog);
	test_utils_add_test ("/replace/dialog-all", test_replace_dialog_all);
	test_utils_add_test ("/wrap/basic", test_wrap_basic);
	test_utils_add_test ("/wrap/nested", test_wrap_nested);
	test_utils_add_test ("/pre-split/simple-html", test_pre_split_simple_html);
	test_utils_add_test ("/pre-split/simple-plain", test_pre_split_simple_plain);
	test_utils_add_test ("/pre-split/complex-html", test_pre_split_complex_html);
	test_utils_add_test ("/pre-split/complex-plain", test_pre_split_complex_plain);

	test_add_html_editor_bug_tests ();

	res = g_test_run ();

	g_clear_object (&application);
	e_misc_util_free_global_memory ();
	test_utils_free_global_memory ();

	g_unlink (test_keyfile_filename);
	g_free (test_keyfile_filename);

	return res;
}
