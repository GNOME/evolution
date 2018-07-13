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

#include <e-util/e-util.h>

#include "test-html-editor-units-utils.h"

#include "test-html-editor-units-bugs.h"

static void
test_bug_726548 (TestFixture *fixture)
{
	/* This test is known to fail, skip it. */
	printf ("SKIPPED ");
#if 0
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
		HTML_PREFIX "<div style=\"width: 71ch;\">aaa</div>"
		"<ol style=\"width: 65ch;\">"
		"<li>a</li><li>b</li><li>c</li></ol>"
		"<div style=\"width: 71ch;\"><br></div>" HTML_SUFFIX,
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
#endif
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
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">\n"
		"<div>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>\n"
		"<div><br></div>\n"
		"<div>This is the third paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>\n"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">\n"
		"<div>This is the first paragraph of a sub-quoted text which has some long text to test. It has the second sentence as well.</div>\n"
		"<br>\n"
		"</blockquote>\n"
		"<div>This is the fourth paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>\n"
		"</blockquote>\n"
		"<div><br></div>\n"
		"</body></html>",
		E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:uuuSuusD\n",
		HTML_PREFIX
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">\n"
		"<div>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>\n"
		"<div><br></div>\n"
		"<div>This is the third paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>\n"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">\n"
		"<div><br></div>\n"
		"</blockquote>\n"
		"<div>This is the fourth paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>\n"
		"</blockquote>\n"
		"<div><br></div>"
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
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">\n"
		"<div>Single line quoted.</div>\n"
		"</blockquote>\n"
		"</body></html>",
		E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:ChcD\n",
		HTML_PREFIX "<div>One line before quotation</div>\n"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">\n"
		"<div>Single line quoted.</div>\n"
		"</blockquote>" HTML_SUFFIX,
		"One line before quotation\n"
		"> Single line quoted.")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:Cecb\n",
		HTML_PREFIX "<div>One line before quotation</div>\n"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">\n"
		"<div>Single line quoted</div>\n"
		"</blockquote>" HTML_SUFFIX,
		"One line before quotation\n"
		"> Single line quoted")) {
		g_test_fail ();
		return;
	}
}

static void
test_bug_767903 (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:This is the first line:\\n\n"
		"action:style-list-bullet\n"
		"type:First item\\n\n"
		"type:Second item\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">This is the first line:</div>"
		"<ul style=\"width: 68ch;\">"
		"<li>First item</li><li>Second item<br></li></ul>" HTML_SUFFIX,
		"This is the first line:\n"
		" * First item\n"
		" * Second item")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:uhb\n"
		"undo:undo\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">This is the first line:</div>"
		"<ul style=\"width: 68ch;\">"
		"<li>First item</li><li>Second item<br></li></ul>" HTML_SUFFIX,
		"This is the first line:\n"
		" * First item\n"
		" * Second item")) {
		g_test_fail ();
		return;
	}
}

static void
test_bug_769708 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head><style id=\"-x-evo-quote-style\" type=\"text/css\">.-x-evo-quoted { -webkit-user-select: none; }</style>"
		"<style id=\"-x-evo-style-a\" type=\"text/css\">a { cursor: text; }</style></head>"
		"<body data-evo-draft=\"\" data-evo-plain-text=\"\" spellcheck=\"true\">"
		"<div data-evo-paragraph=\"\" id=\"-x-evo-input-start\">aaa</div>"
		"<div class=\"-x-evo-signature-wrapper\"><span class=\"-x-evo-signature\" id=\"autogenerated\"><pre>-- <br></pre>"
		"<div data-evo-paragraph=\"\">user &lt;user@no.where&gt;</div>"
		"</span></div></body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div>aaa</div><div><span><pre>-- <br></pre>"
		"<div>user &lt;user@no.where&gt;</div>"
		"</span></div>" HTML_SUFFIX,
		"aaa\n"
		"-- \n"
		"user <user@no.where>"))
		g_test_fail ();
}

static void
test_bug_769913 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"type:ab\n"
		"seq:ltlD\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:ttllDD\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:ttlDlD\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:tttlllDDD\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:tttlDlDlD\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:tb\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:ttbb\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:ttlbrb\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:tttbbb\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:tttllbrbrb\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab")) {
		g_test_fail ();
		return;
	}
}

static void
test_bug_769955 (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines", FALSE);

	/* Use paste action, pretty the same as Ctrl+V */

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"action:paste\n"
		"seq:ll\n"
		"action:style-preformat\n",
		HTML_PREFIX "<pre>"
		"<a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>"
		HTML_SUFFIX,
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"type:[1] \n"
		"action:paste\n"
		"action:style-preformat\n",
		HTML_PREFIX "<pre>"
		"[1] <a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>" HTML_SUFFIX,
		"[1] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"type:[2] \n"
		"action:paste\n"
		"seq:h\n"
		"action:style-preformat\n",
		HTML_PREFIX "<pre>"
		"[2] <a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>" HTML_SUFFIX,
		"[2] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"type:[3] \n"
		"action:paste\n"
		"seq:Chc\n"
		"action:style-preformat\n",
		HTML_PREFIX "<pre>"
		"[3] <a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>" HTML_SUFFIX,
		"[3] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"type:[4] \n"
		"action:paste\n"
		"seq:l\n"
		"action:style-preformat\n",
		HTML_PREFIX "<pre>"
		"[4] <a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>" HTML_SUFFIX,
		"[4] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	/* Use Shift+Insert instead of paste action */

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"seq:Sis\n"
		"seq:ll\n"
		"action:style-preformat\n",
		HTML_PREFIX "<pre>"
		"<a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>"
		HTML_SUFFIX,
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"type:[5] \n"
		"seq:Sis\n"
		"action:style-preformat\n",
		HTML_PREFIX "<pre>"
		"[5] <a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>" HTML_SUFFIX,
		"[5] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"type:[6] \n"
		"seq:Sis\n"
		"seq:h\n"
		"action:style-preformat\n",
		HTML_PREFIX "<pre>"
		"[6] <a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>" HTML_SUFFIX,
		"[6] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"type:[7] \n"
		"seq:Sis\n"
		"seq:Chc\n"
		"action:style-preformat\n",
		HTML_PREFIX "<pre>"
		"[7] <a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>" HTML_SUFFIX,
		"[7] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"action:style-normal\n"
		"seq:Dc\n"
		"type:[8] \n"
		"seq:Sis\n"
		"seq:l\n"
		"action:style-preformat\n",
		HTML_PREFIX "<pre>"
		"[8] <a href=\"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\">"
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines</a></pre>" HTML_SUFFIX,
		"[8] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines")) {
		g_test_fail ();
		return;
	}
}

static void
test_bug_770073 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<!-- text/html -->"
		"<div><span>the 1st line text</span></div>"
		"<br>"
		"<div><span>the 3rd line text</span></div>"
		"<span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span><span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:Chcddbb\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Today, User wrote:</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div style=\"width: 71ch;\">&gt; the 1st line text</div>"
		"<div style=\"width: 71ch;\">&gt; the 3rd line text</div>"
		"</blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> the 1st line text\n"
		"> the 3rd line text")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<!-- text/html -->"
		"<div><span>the first line text</span></div>"
		"<br>"
		"<div><span>the third line text</span></div>"
		"<span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span><span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:Chcddbb\n",
		HTML_PREFIX "<div>On Today, User wrote:</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div><span>the first line text</span></div>"
		"<div><span>the third line text</span></div>"
		"</blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> the first line text\n"
		"> the third line text"))
		g_test_fail ();

}

static void
test_bug_770074 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<!-- text/html -->"
		"<div><span>the 1st line text</span></div>"
		"<br>"
		"<div><span>the 3rd line text</span></div>"
		"<span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span><span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:Chcddbb\n"
		"seq:n\n"
		"undo:undo\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Today, User wrote:</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div style=\"width: 71ch;\">&gt; the 1st line text</div>"
		"<div style=\"width: 71ch;\">&gt; the 3rd line text</div>"
		"</blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> the 1st line text\n"
		"> the 3rd line text"))
		g_test_fail ();
}

static void
test_bug_771044 (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"type:123 456\\n789 abc\\n\n"
		"seq:uuhSdsD\n",
		HTML_PREFIX
		"<div>789 abc</div>"
		"<div><br></div>"
		HTML_SUFFIX,
		"789 abc\n"))
		g_test_fail ();
}

static void
test_bug_771131 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><pre>On &lt;date1&gt;, &lt;name1&gt; wrote:\n"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">\n"
		"Hello\n"
		"\n"
		"Goodbye</blockquote>"
		"<div><span>the 3rd line text</span></div>"
		"</pre><span class=\"-x-evo-to-body\" data-credits=\"On Sat, 2016-09-10 at 20:00 +0000, example@example.com wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Sat, 2016-09-10 at 20:00 +0000, example@example.com wrote:</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div style=\"width: 71ch;\">&gt; On &lt;date1&gt;, &lt;name1&gt; wrote:</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div style=\"width: 71ch;\">&gt; &gt; Hello</div>"
		"<div style=\"width: 71ch;\">&gt; &gt; <br></div>"
		"<div style=\"width: 71ch;\">&gt; &gt; Goodbye</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\">&gt; <br></div>"
		"<div style=\"width: 71ch;\">&gt; the 3rd line text</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"On Sat, 2016-09-10 at 20:00 +0000, example@example.com wrote:\n"
		"> On <date1>, <name1> wrote:\n"
		"> > Hello\n"
		"> > \n"
		"> > Goodbye\n"
		"> \n"
		"> the 3rd line text"))
		g_test_fail ();
}

static void
test_bug_771493 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><pre><br>"
		"----- Original Message -----\n"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">\n"
		"This week summary:"
		"</blockquote>"
		"</pre><span class=\"-x-evo-to-body\" data-credits=\"On Thu, 2016-09-15 at 08:08 -0400, user wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Thu, 2016-09-15 at 08:08 -0400, user wrote:</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div style=\"width: 71ch;\">&gt; <br></div>"
		"<div style=\"width: 71ch;\">&gt; ----- Original Message -----</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div style=\"width: 71ch;\">&gt; &gt; This week summary:</div>"
		"</blockquote>"
		"</blockquote>"
		HTML_SUFFIX,
		"On Thu, 2016-09-15 at 08:08 -0400, user wrote:\n"
		"> \n"
		"> ----- Original Message -----\n"
		"> > This week summary:"))
		g_test_fail ();
}

static void
test_bug_772171 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><pre>a\n"
		"b\n"
		"</pre>"
		"<span class=\"-x-evo-to-body\" data-credits=\"On Thu, 2016-09-15 at 08:08 -0400, user wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:ddeb",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Thu, 2016-09-15 at 08:08 -0400, user wrote:</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div style=\"width: 71ch;\">&gt; <br></div>"
		"<div style=\"width: 71ch;\">&gt; b</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"On Thu, 2016-09-15 at 08:08 -0400, user wrote:\n"
		"> \n"
		"> b"))
		g_test_fail ();
}

static void
test_bug_772513 (TestFixture *fixture)
{
	EContentEditor *cnt_editor;
	gboolean set_signature_from_message, check_if_signature_is_changed, ignore_next_signature_change;

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-reply-start-bottom", TRUE);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	cnt_editor = test_utils_get_content_editor (fixture);

	e_content_editor_insert_signature (
		cnt_editor,
		"",
		FALSE,
		"none",
		&set_signature_from_message,
		&check_if_signature_is_changed,
		&ignore_next_signature_change);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 71ch;\"><br></div>" HTML_SUFFIX,
		"\n"))
		g_test_fail ();
}

static void
test_bug_772918 (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:a b c d\n"
		"seq:lll\n"
		"type:1 2 3 \n"
		"undo:undo:6\n"
		"undo:redo:6\n",
		HTML_PREFIX "<div>a b 1 2 3 c d</div>" HTML_SUFFIX,
		"a b 1 2 3 c d"))
		g_test_fail ();
}

static void
test_bug_773164 (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("This is paragraph 1\n\nThis is paragraph 2\n\nThis is a longer paragraph 3", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"undo:save\n"
		"action:paste\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"seq:huuuue\n" /* Go to the end of the first line */
		"seq:Sdds\n"
		"action:cut\n"
		"seq:dde\n" /* Go to the end of the last line */
		"action:paste\n"
		"undo:undo:5\n"
		"undo:test\n"
		"undo:redo:5\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">This is paragraph 1</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">This is a longer paragraph 3</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">This is paragraph 2</div>"
		HTML_SUFFIX,
		"This is paragraph 1\n"
		"\n"
		"This is a longer paragraph 3\n"
		"\n"
		"This is paragraph 2"))
		g_test_fail ();
}

static void
test_bug_775042 (TestFixture *fixture)
{
	test_utils_insert_content (fixture,
		"<body><pre>a\n"
		"b\n"
		"c"
		"<span class=\"-x-evo-to-body\" data-credits=\"On Fri, 2016-11-25 at 08:18 +0000, user wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:rl\n"
		"mode:plain\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Fri, 2016-11-25 at 08:18 +0000, user wrote:</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<pre>&gt; a<br>"
		"&gt; b<br>"
		"&gt; c</pre>"
		"</blockquote>"
		HTML_SUFFIX,
		"On Fri, 2016-11-25 at 08:18 +0000, user wrote:\n"
		"> a\n"
		"> b\n"
		"> c"))
		g_test_fail ();
}

static void
test_bug_775691 (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:abc def ghi\\n\n"
		"seq:urrrrSrrrs\n"
		"action:copy\n"
		"seq:d\n"
		"action:paste\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">abc def ghi</div>"
		"<div style=\"width: 71ch;\">def</div>"
		HTML_SUFFIX,
		"abc def ghi\n"
		"def"))
		g_test_fail ();
}

static void
test_bug_779707 (TestFixture *fixture)
{
	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-reply-start-bottom", TRUE);
	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-wrap-quoted-text-in-replies", FALSE);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<pre>line 1\n"
		"line 2\n"
		"line 3\n"
		"</pre><span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:uuuSesDbnnu\n"
		"type:a very long text, which splits into multiple lines when this paragraph is not marked as preformatted, but as normal, as it should be\n"
		"",
		HTML_PREFIX "<div style=\"width: 71ch;\">Credits:</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<pre>&gt; line 1</pre>"
		"</blockquote>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">a very long text, which splits into multiple lines when this paragraph is not marked as preformatted, but as normal, as it should be</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<pre>&gt; line 3</pre>"
		"</blockquote>"
		"<div style=\"width: 71ch;\"><br></div>"
		HTML_SUFFIX,
		"Credits:\n"
		"> line 1\n"
		"\n"
		"a very long text, which splits into multiple lines when this paragraph\n"
		"is not marked as preformatted, but as normal, as it should be\n"
		"\n"
		"> line 3\n"))
		g_test_fail ();
}

static void
test_bug_780275_html (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("line 1\nline 2\nline 3", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:line 0\n"
		"seq:nn\n"
		"action:paste-quote\n"
		"undo:save\n" /* 1 */
		"seq:huuuD\n"
		"undo:undo\n"
		"undo:test:1\n"
		"undo:redo\n"
		"type:X\n"
		"seq:ddenn\n"
		"type:line 4\n"
		"undo:drop\n"
		"undo:save\n" /* 1 */
		"seq:hSuusD\n"
		"undo:undo\n"
		"undo:test:1\n"
		"undo:redo\n"
		"",
		HTML_PREFIX "<div>line 0</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>Xline 1</div>"
		"<div>line 2</div>"
		"</blockquote>"
		"<div>line 4</div>"
		HTML_SUFFIX,
		"line 0\n"
		"> Xline 1\n"
		"> line 2\n"
		"line 4"))
		g_test_fail ();
}

static void
test_bug_780275_plain (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("line 1\nline 2\nline 3", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:line 0\n"
		"seq:nn\n"
		"action:paste-quote\n"
		"undo:save\n" /* 1 */
		"seq:huuuD\n"
		"undo:undo\n"
		"undo:test:1\n"
		"undo:redo\n"
		"type:X\n"
		"seq:ddenn\n"
		"type:line 4\n"
		"undo:drop\n"
		"undo:save\n" /* 1 */
		"seq:hSuusD\n"
		"undo:undo\n"
		"undo:test:1\n"
		"undo:redo\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">line 0</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div style=\"width: 71ch;\">&gt; Xline 1</div>"
		"<div style=\"width: 71ch;\">&gt; line 2</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\">line 4</div>"
		HTML_SUFFIX,
		"line 0\n"
		"> Xline 1\n"
		"> line 2\n"
		"line 4"))
		g_test_fail ();
}

static void
test_bug_781722 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<pre>Signed-off-by: User &lt;<a href=\"mailto:user@no.where\">user@no.where</a>&gt;\n"
		"</pre><span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:dd\n"
		"action:style-preformat\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">Credits:</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<pre>&gt; Signed-off-by: User &lt;<a href=\"mailto:user@no.where\">user@no.where</a>&gt;</pre>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> Signed-off-by: User <user@no.where>"))
		g_test_fail ();
}

static void
test_bug_781116 (TestFixture *fixture)
{
	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-wrap-quoted-text-in-replies", FALSE);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<pre>a very long text, which splits into multiple lines when this paragraph is not marked as preformatted, but as normal, as it should be</pre>\n"
		"</pre><span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:dd\n"
		"action:wrap-lines\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">Credits:</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<pre>&gt; a very long text, which splits into multiple lines when this<br>"
		"&gt; paragraph is not marked as preformatted, but as normal, as it should<br>"
		"&gt; be</pre>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> a very long text, which splits into multiple lines when this\n"
		"> paragraph is not marked as preformatted, but as normal, as it should\n"
		"> be</pre>"))
		g_test_fail ();
}

static void
test_bug_780088 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_set_clipboard_text ("Seeing @blah instead of @foo XX'ed on" UNICODE_NBSP "https://example.sub" UNICODE_NBSP "domain.org/page I'd recommend to XX YY <https://example.subdomain.org/p/user/> , click fjwvne on the left, click skjd sjewncj on the right, and set wqje wjfdn Xs to something like wqjfnm www.example.com/~user wjfdncj or such.", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"action:paste\n"
		"seq:n",
		HTML_PREFIX "<div style=\"width: 71ch;\">"
		"Seeing @blah instead of @foo XX'ed on&nbsp;<a href=\"https://example.sub\">https://example.sub</a>"
		"&nbsp;domain.org/page I'd recommend to XX YY "
		"&lt;<a href=\"https://example.subdomain.org/p/user/\">https://example.subdomain.org/p/user/</a>&gt; , "
		"click fjwvne on the left, click skjd sjewncj on the right, and set wqje wjfdn Xs to something like "
		"wqjfnm <a href=\"www.example.com/~user\">www.example.com/~user</a> wjfdncj or such.</div>"
		"</div><div style=\"width: 71ch;\"><br></div>"
		HTML_SUFFIX,
		"Seeing @blah instead of @foo XX'ed on" UNICODE_NBSP "https://example.sub" UNICODE_NBSP "domain.org/pa\n"
		"ge I'd recommend to XX YY <https://example.subdomain.org/p/user/> ,\n"
		"click fjwvne on the left, click skjd sjewncj on the right, and set wqje\n"
		"wjfdn Xs to something like wqjfnm www.example.com/~user wjfdncj or\n"
		"such.\n"))
		g_test_fail ();
}

static void
test_bug_788829 (TestFixture *fixture)
{
	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-wrap-quoted-text-in-replies", TRUE);
	test_utils_fixture_change_setting_int32 (fixture, "org.gnome.evolution.mail", "composer-word-wrap-length", 71);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<div>Xxxxx xx xxxxxxxxx xx xxxxxxx xx xxxxx xxxx xxxx xx xxx xxx xxxx xxx xxxçx xôxé "
		"\"xxxxx xxxx xxxxxxx xxx\" xx xxxx xxxxé xxx xxx xxxéx xxx x'x xéxxxxé x'xxxxxxxxx xx "
		"xxx \"<a href=\"https://gnome.org\">Xxxx XXX Xxxxxx Xxx</a>\". Xx xxxx xxxxxxxx xxx <a"
		" href=\"https://gnome.org/\">xxxxxxxxxxxxxxxx.xx</a> (xxxxxxx xxxxxxxxxx xx .xxx). Xxxx "
		"êxxx xxx xxxxxxxxxxx xxxéxxxxxxxx, xxxx xxxxx xx XXX xx xéxxx à xx xxx \"xxx xxxxxx xxxx "
		"xx xxxxxxx\" xx xxxx xx xxxxx xxxxxxxx xxxxxxxx xx $ xx xxxx x'xxxxxx.</div><div><br>"
		"</div><div>Xxxx xx xéxxxxxxx, xxxxxxxx xxxxxxx (!), xxxxxxx à xxx, xxxx ooo$ XXX xxxxé: "
		"<a href=\"https://gnome.org\">https://xxxxxxxxxxxxxxxx.xx/xxxxxxx/xxxxx-xxxx-xxxxxxxx-x"
		"xxxx-xxxx-xxx-xxxxxxxx-xxx/</a> xx xx xxxx xéxéxxxxxxx x'xxxxxx xxxx xx xxxxxx xx xxxxxx"
		"xxxxxx xx xxx (xxxxx Xxxxxx) xxxx xxxx x'xxxxxxx xx xxxxxx: <a href=\"https://gnome.org\">"
		"https://xxxxxxxxxxxxxxxx.xxx/xx-xxxxxxx/xxxxxxx/Xxxxxxxxxxxx-Xxxxx-Xxxx-XXX-Xxxxxx-Xxx.xxx"
		"</a></div><div><br></div><div>Xxxx xxx xxx xxxxxxx xxxxxxxéxx x'xxxêxxxx à xxxxx, xxx xx x"
		"xxxé xx oooxooo xxxxx xxxxx xxxx... xxxx x'xxx xxxxxxxxxxxx xxxxx xxx xxxxxxxx xx \"xx xxx"
		"xx xxx xxx xxxxxxx xxxxxxx xxxxxxxxxxxxxx xxxx xxxxx xxxxxx xx xx xxxx xx x'xxxxxx\". Xx "
		"xxxx-êxxx xxx xx xxxxxxxx xx xxxx \"x'xxxêxx à xxxxx xx oooxooo xxxx xxx xéxxxxxxxx, xxxx"
		"\"...</div><div><br></div><div>Xxxxx xxxxxx'xx xxx x xxxx xxxxxxx xxxxx xx xxèx xxxxxxxxx "
		"xxxxxxxxxxxxxxxx à xx xxx x'xx xx xêxx (éxxxxxxxxx xxxx-xx-xxxxxxxx): <a href=\"https://"
		"gnome.org\">https://xxxxxxxxxxxxxxxx.xxx/xx-xxxxxxx/xxxxxxx/Xxxxx-xxxx-xxx-xxxxxxxxxx-xx"
		"xxx.xxx</a> ;&nbsp;</div><div><br></div><div>...x'x xxxxx xx xxxxxx x'xxxxxx xéxxxxxxx, "
		"xx xxx xxxx xxxxxx x'xxxxxxxxxxx xxxxxx, xxxx <a href=\"https://gnome.org\">https://xxxx"
		"xxxxxxxxxxxx.xxx/xxxxxxxx-xxxxxxx-xxxx-xxx-o/</a> xxxxx xxx <a href=\"https://gnome.org/\""
		">https://xxxxxxxxxxxxxxxx.xxx/xxxxxxxx-xxxxxxx-xxxx-xxx-o/</a> ...</div>"
		"<span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Today, User wrote:</div><blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div style=\"width: 71ch;\">&gt; Xxxxx xx xxxxxxxxx xx xxxxxxx xx xxxxx xxxx "
		"xxxx xx xxx xxx xxxx xxx<br>&gt; xxxçx xôxé \"xxxxx xxxx xxxxxxx xxx\" xx xxxx "
		"xxxxé xxx xxx xxxéx xxx<br>&gt; x'x xéxxxxé x'xxxxxxxxx xx xxx \"Xxxx XXX Xxxxxx "
		"Xxx\". Xx xxxx<br>&gt; xxxxxxxx xxx xxxxxxxxxxxxxxxx.xx (xxxxxxx xxxxxxxxxx xx .xx"
		"x). Xxxx<br>&gt; êxxx xxx xxxxxxxxxxx xxxéxxxxxxxx, xxxx xxxxx xx XXX xx xéxxx à "
		"xx<br>&gt; xxx \"xxx xxxxxx xxxx xx xxxxxxx\" xx xxxx xx xxxxx xxxxxxxx xxxxxxxx"
		"<br>&gt; xx $ xx xxxx x'xxxxxx.</div><div style=\"width: 71ch;\">&gt; <br></div>"
		"<div style=\"width: 71ch;\">&gt; Xxxx xx xéxxxxxxx, xxxxxxxx xxxxxxx (!), "
		"xxxxxxx à xxx, xxxx ooo$ XXX<br>&gt; xxxxé: https://xxxxxxxxxxxxxxxx.xx/xxx"
		"xxxx/xxxxx-xxxx-xxxxxxxx-xxxxx-<br>&gt; xxxx-xxx-xxxxxxxx-xxx/ xx xx xxxx "
		"xéxéxxxxxxx x'xxxxxx xxxx xx xxxxxx<br>&gt; xx xxxxxxxxxxxx xx xxx (xxxxx "
		"Xxxxxx) xxxx xxxx x'xxxxxxx xx xxxxxx: <br>&gt; https://xxxxxxxxxxxxxxxx.xx"
		"x/xx-xxxxxxx/xxxxxxx/Xxxxxxxxxxxx-Xxxxx-Xx<br>&gt; xx-XXX-Xxxxxx-Xxx.xxx</div>"
		"<div style=\"width: 71ch;\">&gt; <br></div><div style=\"width: 71ch;\">&gt; Xx"
		"xx xxx xxx xxxxxxx xxxxxxxéxx x'xxxêxxxx à xxxxx, xxx xx xxxxé xx<br>&gt; oooxo"
		"oo xxxxx xxxxx xxxx... xxxx x'xxx xxxxxxxxxxxx xxxxx xxx<br>&gt; xxxxxxxx xx \""
		"xx xxxxx xxx xxx xxxxxxx xxxxxxx xxxxxxxxxxxxxx xxxx<br>&gt; xxxxx xxxxxx xx xx "
		"xxxx xx x'xxxxxx\". Xx xxxx-êxxx xxx xx xxxxxxxx xx<br>&gt; xxxx \"x'xxxêxx à "
		"xxxxx xx oooxooo xxxx xxx xéxxxxxxxx, xxxx\"...</div><div style=\"width: 71ch;\">"
		"&gt; <br></div><div style=\"width: 71ch;\">&gt; Xxxxx xxxxxx'xx xxx x xxxx xxxxxxx "
		"xxxxx xx xxèx xxxxxxxxx<br>&gt; <br>&gt; xxxxxxxxxxxxxxxx à xx xxx x'xx xx xêxx "
		"(éxxxxxxxxx xxxx-xx-xxxxxxxx): <a href=\"https://xxxxxxxxxxxxxxxx.xxx/xx-xxxxxxx/"
		"xxxxxxx/Xxxxx-xxxx-xxx-xxxxxxxxxx-xxxxx.xxx\">https://xxxxxxxxxxxxxxxx.xxx/xx-xxx"
		"xxxx/xxxxxxx/Xxxxx-xxxx-xxx-<br>&gt; xxxxxxxxxx-xxxxx.xxx</a> ;&nbsp;</div><div "
		"style=\"width: 71ch;\">&gt; <br></div><div style=\"width: 71ch;\">&gt; ...x'x "
		"xxxxx xx xxxxxx x'xxxxxx xéxxxxxxx, xx xxx xxxx xxxxxx<br>&gt; x'xxxxxxxxxxx "
		"xxxxxx, xxxx https://xxxxxxxxxxxxxxxx.xxx/xxxxxxxx-xxxx<br>&gt; xxx-xxxx-xxx-o/ "
		"xxxxx xxx https://xxxxxxxxxxxxxxxx.xxx/xxxxxxxx-xxxxx<br>&gt; xx-xxxx-xxx-o/ ...</div>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> Xxxxx xx xxxxxxxxx xx xxxxxxx xx xxxxx xxxx xxxx xx xxx xxx xxxx xxx\n"
		"> xxxçx xôxé \"xxxxx xxxx xxxxxxx xxx\" xx xxxx xxxxé xxx xxx xxxéx xxx\n"
		"> x'x xéxxxxé x'xxxxxxxxx xx xxx \"Xxxx XXX Xxxxxx Xxx\". Xx xxxx\n"
		"> xxxxxxxx xxx xxxxxxxxxxxxxxxx.xx (xxxxxxx xxxxxxxxxx xx .xxx). Xxxx\n"
		"> êxxx xxx xxxxxxxxxxx xxxéxxxxxxxx, xxxx xxxxx xx XXX xx xéxxx à xx\n"
		"> xxx \"xxx xxxxxx xxxx xx xxxxxxx\" xx xxxx xx xxxxx xxxxxxxx xxxxxxxx\n"
		"> xx $ xx xxxx x'xxxxxx.\n"
		"> \n"
		"> Xxxx xx xéxxxxxxx, xxxxxxxx xxxxxxx (!), xxxxxxx à xxx, xxxx ooo$ XXX\n"
		"> xxxxé: https://xxxxxxxxxxxxxxxx.xx/xxxxxxx/xxxxx-xxxx-xxxxxxxx-xxxxx-\n"
		"> xxxx-xxx-xxxxxxxx-xxx/ xx xx xxxx xéxéxxxxxxx x'xxxxxx xxxx xx xxxxxx\n"
		"> xx xxxxxxxxxxxx xx xxx (xxxxx Xxxxxx) xxxx xxxx x'xxxxxxx xx xxxxxx: \n"
		"> https://xxxxxxxxxxxxxxxx.xxx/xx-xxxxxxx/xxxxxxx/Xxxxxxxxxxxx-Xxxxx-Xx\n"
		"> xx-XXX-Xxxxxx-Xxx.xxx\n"
		"> \n"
		"> Xxxx xxx xxx xxxxxxx xxxxxxxéxx x'xxxêxxxx à xxxxx, xxx xx xxxxé xx\n"
		"> oooxooo xxxxx xxxxx xxxx... xxxx x'xxx xxxxxxxxxxxx xxxxx xxx\n"
		"> xxxxxxxx xx \"xx xxxxx xxx xxx xxxxxxx xxxxxxx xxxxxxxxxxxxxx xxxx\n"
		"> xxxxx xxxxxx xx xx xxxx xx x'xxxxxx\". Xx xxxx-êxxx xxx xx xxxxxxxx xx\n"
		"> xxxx \"x'xxxêxx à xxxxx xx oooxooo xxxx xxx xéxxxxxxxx, xxxx\"...\n"
		"> \n"
		"> Xxxxx xxxxxx'xx xxx x xxxx xxxxxxx xxxxx xx xxèx xxxxxxxxx\n"
		"> \n"
		"> xxxxxxxxxxxxxxxx à xx xxx x'xx xx xêxx (éxxxxxxxxx xxxx-xx-xxxxxxxx): https://xxxxxxxxxxxxxxxx.xxx/xx-xxxxxxx/xxxxxxx/Xxxxx-xxxx-xxx-\n"
		"> xxxxxxxxxx-xxxxx.xxx ; \n"
		"> \n"
		"> ...x'x xxxxx xx xxxxxx x'xxxxxx xéxxxxxxx, xx xxx xxxx xxxxxx\n"
		"> x'xxxxxxxxxxx xxxxxx, xxxx https://xxxxxxxxxxxxxxxx.xxx/xxxxxxxx-xxxx\n"
		"> xxx-xxxx-xxx-o/ xxxxx xxx https://xxxxxxxxxxxxxxxx.xxx/xxxxxxxx-xxxxx\n"
		"> xx-xxxx-xxx-o/ ..."))
		g_test_fail ();
}

static void
test_bug_750636 (TestFixture *fixture)
{
	test_utils_fixture_change_setting_int32 (fixture, "org.gnome.evolution.mail", "composer-word-wrap-length", 71);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:"
		"12345678901234567890123456789012345678901234567890123456789012345678901"
		"12345678901234567890123456789012345678901234567890123456789012345678901A\\n\\n"
		"1234567890123456789012345678901234567890123456789012345678901234567890 B\\n\\n"
		"12345678901234567890123456789012345678901234567890123456789012345678901     C\\n\\n"
		"1234567890123456789012345678901234567890123456789012345678901234567890     D\\n\\n"
		"12345678901234567890123456789012345678901234567890123456789012345678901" UNICODE_NBSP UNICODE_NBSP UNICODE_NBSP "E\\n\\n"
		"1234567890123456789012345678901234567890123456789012345678901234567890" UNICODE_NBSP UNICODE_NBSP UNICODE_NBSP "F\\n\\n"
		" 1\\n"
		"  2\\n"
		"   3\\n"
		"\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">"
		"12345678901234567890123456789012345678901234567890123456789012345678901"
		"12345678901234567890123456789012345678901234567890123456789012345678901A</div>"
		"<div style=\"width: 71ch;\"><br></div><div style=\"width: 71ch;\">"
		"1234567890123456789012345678901234567890123456789012345678901234567890 B</div>"
		"<div style=\"width: 71ch;\"><br></div><div style=\"width: 71ch;\">"
		"12345678901234567890123456789012345678901234567890123456789012345678901     C</div>"
		"<div style=\"width: 71ch;\"><br></div><div style=\"width: 71ch;\">"
		"1234567890123456789012345678901234567890123456789012345678901234567890     D</div>"
		"<div style=\"width: 71ch;\"><br></div><div style=\"width: 71ch;\">"
		"12345678901234567890123456789012345678901234567890123456789012345678901&nbsp;&nbsp;&nbsp;E</div>"
		"<div style=\"width: 71ch;\"><br></div><div style=\"width: 71ch;\">"
		"1234567890123456789012345678901234567890123456789012345678901234567890&nbsp;&nbsp;&nbsp;F</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\"> 1</div>"
		"<div style=\"width: 71ch;\">  2</div>"
		"<div style=\"width: 71ch;\">   3</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		HTML_SUFFIX,
		"12345678901234567890123456789012345678901234567890123456789012345678901\n"
		"12345678901234567890123456789012345678901234567890123456789012345678901\n"
		"A\n\n"
		"1234567890123456789012345678901234567890123456789012345678901234567890\n"
		"B\n\n"
		"12345678901234567890123456789012345678901234567890123456789012345678901\n"
		"C\n\n"
		"1234567890123456789012345678901234567890123456789012345678901234567890 \n"
		"D\n\n"
		"12345678901234567890123456789012345678901234567890123456789012345678901\n"
		"   E\n\n"
		"1234567890123456789012345678901234567890123456789012345678901234567890 \n"
		"  F\n\n"
		" 1\n"
		"  2\n"
		"   3\n"))
		g_test_fail ();
}

void
test_add_html_editor_bug_tests (void)
{
	test_utils_add_test ("/bug/726548", test_bug_726548);
	test_utils_add_test ("/bug/750657", test_bug_750657);
	test_utils_add_test ("/bug/760989", test_bug_760989);
	test_utils_add_test ("/bug/767903", test_bug_767903);
	test_utils_add_test ("/bug/769708", test_bug_769708);
	test_utils_add_test ("/bug/769913", test_bug_769913);
	test_utils_add_test ("/bug/769955", test_bug_769955);
	test_utils_add_test ("/bug/770073", test_bug_770073);
	test_utils_add_test ("/bug/770074", test_bug_770074);
	test_utils_add_test ("/bug/771044", test_bug_771044);
	test_utils_add_test ("/bug/771131", test_bug_771131);
	test_utils_add_test ("/bug/771493", test_bug_771493);
	test_utils_add_test ("/bug/772171", test_bug_772171);
	test_utils_add_test ("/bug/772513", test_bug_772513);
	test_utils_add_test ("/bug/772918", test_bug_772918);
	test_utils_add_test ("/bug/773164", test_bug_773164);
	test_utils_add_test ("/bug/775042", test_bug_775042);
	test_utils_add_test ("/bug/775691", test_bug_775691);
	test_utils_add_test ("/bug/779707", test_bug_779707);
	test_utils_add_test ("/bug/780275/html", test_bug_780275_html);
	test_utils_add_test ("/bug/780275/plain", test_bug_780275_plain);
	test_utils_add_test ("/bug/781722", test_bug_781722);
	test_utils_add_test ("/bug/781116", test_bug_781116);
	test_utils_add_test ("/bug/780088", test_bug_780088);
	test_utils_add_test ("/bug/788829", test_bug_788829);
	test_utils_add_test ("/bug/750636", test_bug_750636);
}
