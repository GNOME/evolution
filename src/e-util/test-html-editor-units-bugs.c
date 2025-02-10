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
test_bug_750657 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body>"
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
		"<div><br></div>"
		"</body></html>",
		E_CONTENT_EDITOR_INSERT_TEXT_HTML | E_CONTENT_EDITOR_INSERT_REPLACE_ALL);

	if (!test_utils_run_simple_test (fixture,
		"seq:CecuuuSuusD\n",
		HTML_PREFIX
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>This is the first paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"<div><br></div>"
		"<div>This is the third paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div><br></div>"
		"</blockquote>"
		"<div>This is the fourth paragraph of a quoted text which has some long text to test. It has the second sentence as well.</div>"
		"</blockquote>"
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
		"> Single line quoted.\n")) {
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
		"> Single line quoted\n")) {
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
		"<li>First item</li><li>Second item</li></ul>" HTML_SUFFIX,
		"This is the first line:\n"
		" * First item\n"
		" * Second item\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:uhb\n"
		"undo:undo\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">This is the first line:</div>"
		"<ul style=\"width: 68ch;\">"
		"<li>First item</li><li>Second item</li></ul>" HTML_SUFFIX,
		"This is the first line:\n"
		" * First item\n"
		" * Second item\n")) {
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
		HTML_PREFIX "<div style=\"width: 71ch;\">aaa</div>"
		"<div style=\"width: 71ch;\"><span><pre>-- <br></pre>"
		"<div>user &lt;<a href=\"mailto:user@no.where\">user@no.where</a>&gt;</div>"
		"</span></div>" HTML_SUFFIX,
		"aaa\n"
		"-- \n"
		"user <user@no.where>\n"))
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
		"ab\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:ttllDD\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:ttlDlD\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:tttlllDDD\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:tttlDlDlD\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:tb\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:ttbb\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:ttlbrb\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:tttbbb\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:tttllbrbrb\n",
		HTML_PREFIX "<div>ab</div>" HTML_SUFFIX,
		"ab\n")) {
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
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\n")) {
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
		"[1] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\n")) {
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
		"[2] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\n")) {
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
		"[3] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\n")) {
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
		"[4] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\n")) {
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
		"http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\n")) {
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
		"[5] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\n")) {
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
		"[6] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\n")) {
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
		"[7] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\n")) {
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
		"[8] http://www.example.com/this-is-a-very-long-link-which-should-not-be-wrapped-into-multiple-lines\n")) {
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
		"seq:Chcddb\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Today, User wrote:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "the 1st line text</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "the 3rd line text</div>"
		"</blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> the 1st line text\n"
		"> the 3rd line text\n")) {
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
		"seq:Chcddb\n",
		HTML_PREFIX "<div>On Today, User wrote:</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div><span>the first line text</span></div>"
		"<div><span>the third line text</span></div>"
		"</blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> the first line text\n"
		"> the third line text\n"))
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
		"seq:Chcddb\n"
		"seq:n\n"
		"undo:undo\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Today, User wrote:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "the 1st line text</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "the 3rd line text</div>"
		"</blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> the 1st line text\n"
		"> the 3rd line text\n"))
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
	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-wrap-quoted-text-in-replies", TRUE);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><pre>On &lt;date1&gt;, &lt;name1&gt; wrote:\n"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"Hello\n"
		"\n"
		"Goodbye</blockquote>"
		"<div><span>the 3rd line text</span></div>"
		"</pre><span class=\"-x-evo-to-body\" data-credits=\"On Sat, 2016-09-10 at 20:00 +0000, example@example.com wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Sat, 2016-09-10 at 20:00 +0000, <a href=\"mailto:example@example.com\">example@example.com</a> wrote:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "On &lt;date1&gt;, &lt;name1&gt; wrote:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "Hello</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "<br></div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "Goodbye</div>"
		"</blockquote>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "the 3rd line text</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"On Sat, 2016-09-10 at 20:00 +0000, example@example.com wrote:\n"
		"> On <date1>, <name1> wrote:\n"
		"> > Hello\n"
		"> > \n"
		"> > Goodbye\n"
		"> the 3rd line text\n"))
		g_test_fail ();

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-wrap-quoted-text-in-replies", FALSE);

	test_utils_insert_content (fixture,
		"<body><pre>On &lt;date1&gt;, &lt;name1&gt; wrote:\n"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"Hello\n"
		"\n"
		"Goodbye</blockquote>"
		"<div><span>the 3rd line text</span></div>"
		"</pre><span class=\"-x-evo-to-body\" data-credits=\"On Sat, 2016-09-10 at 20:00 +0000, example@example.com wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Sat, 2016-09-10 at 20:00 +0000, <a href=\"mailto:example@example.com\">example@example.com</a> wrote:</div>"
		"<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "On &lt;date1&gt;, &lt;name1&gt; wrote:</pre>"
		"<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "Hello</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "<br></pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "Goodbye</pre>"
		"</blockquote>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "the 3rd line text</pre>"
		"</blockquote>"
		HTML_SUFFIX,
		"On Sat, 2016-09-10 at 20:00 +0000, example@example.com wrote:\n"
		"> On <date1>, <name1> wrote:\n"
		"> > Hello\n"
		"> > \n"
		"> > Goodbye\n"
		"> the 3rd line text\n"))
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
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"This week summary:"
		"</blockquote>"
		"</pre><span class=\"-x-evo-to-body\" data-credits=\"On Thu, 2016-09-15 at 08:08 -0400, user wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Thu, 2016-09-15 at 08:08 -0400, user wrote:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "----- Original Message -----</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "This week summary:</div>"
		"</blockquote>"
		"</blockquote>"
		HTML_SUFFIX,
		"On Thu, 2016-09-15 at 08:08 -0400, user wrote:\n"
		"> \n"
		"> ----- Original Message -----\n"
		"> > This week summary:\n"))
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
		"seq:deb",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Thu, 2016-09-15 at 08:08 -0400, user wrote:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "b</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"On Thu, 2016-09-15 at 08:08 -0400, user wrote:\n"
		"> \n"
		"> b\n"))
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
		FALSE,
		"none",
		&set_signature_from_message,
		&check_if_signature_is_changed,
		&ignore_next_signature_change);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\"><span></span></div>" HTML_SUFFIX,
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
		"a b 1 2 3 c d\n"))
		g_test_fail ();
}

static void
test_bug_773164 (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("This is paragraph 1\n\nThis is paragraph 2\n\nThis is a longer paragraph 3", FALSE);

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-paste-plain-prefer-pre", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"undo:save\n"
		"action:paste\n"
		"undo:undo\n"
		"undo:test\n"
		"undo:redo\n"
		"seq:huuue\n" /* Go to the end of the second line */
		"seq:Sddes\n"
		"action:cut\n"
		"seq:dde\n" /* Go to the end of the last line */
		"action:paste\n"
		"undo:undo:3\n"
		"undo:test\n"
		"undo:redo:3\n",
		HTML_PREFIX "<pre>This is paragraph 1</pre>"
		"<pre><br></pre>"
		"<pre>This is a longer paragraph 3</pre>"
		"<pre><br></pre>"
		"<pre>This is paragraph 2</pre>"
		"<pre><br></pre>"
		HTML_SUFFIX,
		"This is paragraph 1\n"
		"\n"
		"This is a longer paragraph 3\n"
		"\n"
		"This is paragraph 2\n"))
		g_test_fail ();
}

static void
test_bug_775042 (TestFixture *fixture)
{
	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-wrap-quoted-text-in-replies", FALSE);

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
		"<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "a</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "b</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "c</pre>"
		"</blockquote>"
		HTML_SUFFIX,
		"On Fri, 2016-11-25 at 08:18 +0000, user wrote:\n"
		"> a\n"
		"> b\n"
		"> c\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><pre>a\n"
		"b\n"
		"c"
		"<span class=\"-x-evo-to-body\" data-credits=\"On Fri, 2016-11-25 at 08:18 +0000, user wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:rl\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Fri, 2016-11-25 at 08:18 +0000, user wrote:</div>"
		"<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "a</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "b</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "c</pre>"
		"</blockquote>"
		HTML_SUFFIX,
		"On Fri, 2016-11-25 at 08:18 +0000, user wrote:\n"
		"> a\n"
		"> b\n"
		"> c\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><div>a</div>"
		"<p>b</p>"
		"<div>c</div>"
		"<span class=\"-x-evo-to-body\" data-credits=\"On Fri, 2016-11-25 at 08:18 +0000, user wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:rl\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">On Fri, 2016-11-25 at 08:18 +0000, user wrote:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "a</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "b</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "c</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"On Fri, 2016-11-25 at 08:18 +0000, user wrote:\n"
		"> a\n"
		"> b\n"
		"> c\n")) {
		g_test_fail ();
		return;
	}
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
		"def\n"))
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
		"seq:ChcddhSesDbnn\n"
		"type:a very long text, which splits into multiple lines when this paragraph is not marked as preformatted, but as normal, as it should be\n"
		"seq:n\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "line 1</pre>"
		"</blockquote>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">a very long text, which splits into multiple lines when this paragraph is not marked as preformatted, but as normal, as it should be</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "line 3</pre>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> line 1\n"
		"\n"
		"a very long text, which splits into multiple lines when this paragraph\n"
		"is not marked as preformatted, but as normal, as it should be\n"
		"\n"
		"> line 3\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<div>line 1</div>"
		"<div>line 2</div>"
		"<div>line 3</div>"
		"<span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:ChcddhSesDbnn\n"
		"type:a very long text, which splits into multiple lines when this paragraph is not marked as preformatted, but as normal, as it should be\n"
		"seq:n\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "line 1</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">a very long text, which splits into multiple lines when this paragraph is not marked as preformatted, but as normal, as it should be</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "line 3</div>"
		"</blockquote>"
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

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-paste-plain-prefer-pre", TRUE);

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
		"undo:redo\n",
		HTML_PREFIX "<div>line 0</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<pre>Xline 1</pre>"
		"<pre>line 2</pre>"
		"</blockquote>"
		"<div>line 4</div>"
		HTML_SUFFIX,
		"line 0\n"
		"> Xline 1\n"
		"> line 2\n"
		"line 4\n"))
		g_test_fail ();
}

static void
test_bug_780275_plain (TestFixture *fixture)
{
	test_utils_set_clipboard_text ("line 1\nline 2\nline 3", FALSE);

	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-paste-plain-prefer-pre", TRUE);

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
		"<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "Xline 1</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "line 2</pre>"
		"</blockquote>"
		"<div style=\"width: 71ch;\">line 4</div>"
		HTML_SUFFIX,
		"line 0\n"
		"> Xline 1\n"
		"> line 2\n"
		"line 4\n"))
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
		"<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "Signed-off-by: User &lt;<a href=\"mailto:user@no.where\">user@no.where</a>&gt;</pre>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> Signed-off-by: User <user@no.where>\n"))
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
		"<span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:dd\n"
		"action:wrap-lines\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "a very long text, which splits into multiple lines when this<br>"
		QUOTE_SPAN (QUOTE_CHR) "paragraph is not marked as preformatted, but as normal, as it should<br>"
		QUOTE_SPAN (QUOTE_CHR) "be</pre>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> a very long text, which splits into multiple lines when this\n"
		"> paragraph is not marked as preformatted, but as normal, as it should\n"
		"> be\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<blockquote type=\"cite\"><div>a very long text, which splits into multiple lines when this paragraph is not marked as preformatted, but as normal, as it should be</div></blockquote>"
		"<span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:dd\n"
		"action:wrap-lines\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "a very long text, which splits into multiple lines when this<br>"
		QUOTE_SPAN (QUOTE_CHR) "paragraph is not marked as preformatted, but as normal, as it should<br>"
		QUOTE_SPAN (QUOTE_CHR) "be</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> a very long text, which splits into multiple lines when this\n"
		"> paragraph is not marked as preformatted, but as normal, as it should\n"
		"> be\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<blockquote type=\"cite\"><div>a very long text, which splits into multiple lines when this paragraph is not marked as preformatted, but as normal, as it should be</div></blockquote>"
		"<span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:dd\n"
		"action:wrap-lines\n",
		HTML_PREFIX "<div>Credits:</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
		"<div>a very long text, which splits into multiple lines when this paragraph<br>"
		"is not marked as preformatted, but as normal, as it should be</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> a very long text, which splits into multiple lines when this\n"
		"> paragraph\n"
		"> is not marked as preformatted, but as normal, as it should be\n"))
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

	test_utils_set_clipboard_text ("Seeing @blah instead of @foo XX'ed on" UNICODE_NBSP "https://example.sub" UNICODE_NBSP "domain.org/page I'd"
		" recommend to XX YY <https://example.subdomain.org/p/user/> , click fjwvne on the left, click skjd sjewncj on the right, and set"
		" wqje wjfdn Xs to something like wqjfnm www.example.com/~user wjfdncj or such.", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"action:paste\n"
		"seq:n",
		HTML_PREFIX "<div style=\"width: 71ch;\">"
		"Seeing @blah instead of @foo XX'ed on&nbsp;<a href=\"https://example.sub\">https://example.sub</a>"
		"&nbsp;domain.org/page I'd recommend to XX YY "
		"&lt;<a href=\"https://example.subdomain.org/p/user/\">https://example.subdomain.org/p/user/</a>&gt; , "
		"click fjwvne on the left, click skjd sjewncj on the right, and set wqje wjfdn Xs to something like "
		"wqjfnm <a href=\"https://www.example.com/~user\">www.example.com/~user</a> wjfdncj or such.</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		HTML_SUFFIX,
		"Seeing @blah instead of @foo XX'ed\n"
		"on" UNICODE_NBSP "https://example.sub" UNICODE_NBSP "domain.org/page I'd recommend to XX YY\n"
		"<https://example.subdomain.org/p/user/> , click fjwvne on the left,\n"
		"click skjd sjewncj on the right, and set wqje wjfdn Xs to something\n"
		"like wqjfnm www.example.com/~user wjfdncj or such.\n"))
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
		HTML_PREFIX "<div style=\"width: 71ch;\">On Today, User wrote:</div><blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "Xxxxx xx xxxxxxxxx xx xxxxxxx xx xxxxx xxxx xxxx xx xxx xxx xxxx xxx" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "xxxçx xôxé \"xxxxx xxxx xxxxxxx xxx\" xx xxxx xxxxé xxx xxx xxxéx xxx" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "x'x xéxxxxé x'xxxxxxxxx xx xxx \"Xxxx XXX Xxxxxx Xxx\". Xx xxxx" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "xxxxxxxx xxx xxxxxxxxxxxxxxxx.xx (xxxxxxx xxxxxxxxxx xx .xxx). Xxxx" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "êxxx xxx xxxxxxxxxxx xxxéxxxxxxxx, xxxx xxxxx xx XXX xx xéxxx à xx" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "xxx \"xxx xxxxxx xxxx xx xxxxxxx\" xx xxxx xx xxxxx xxxxxxxx xxxxxxxx" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "xx $ xx xxxx x'xxxxxx.</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "Xxxx xx xéxxxxxxx, xxxxxxxx xxxxxxx (!), xxxxxxx à xxx, xxxx ooo$ XXX" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "xxxxé:" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://xxxxxxxxxxxxxxxx.xx/xxxxxxx/xxxxx-xxxx-xxxxxxxx-xxxxx-xxxx-xxx-xxxxxxxx-xxx/\">https://"
		"xxxxxxxxxxxxxxxx.xx/xxxxxxx/xxxxx-xxxx-xxxxxxxx-xxxxx-xxxx-xxx-xxxxxxxx-xxx/</a>" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "xx xx xxxx xéxéxxxxxxx x'xxxxxx xxxx xx xxxxxx xx xxxxxxxxxxxx xx xxx" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "(xxxxx Xxxxxx) xxxx xxxx x'xxxxxxx xx xxxxxx:" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://xxxxxxxxxxxxxxxx.xxx/xx-xxxxxxx/xxxxxxx/Xxxxxxxxxxxx-Xxxxx-Xxxx-XXX-Xxxxxx-Xxx.xxx\">https://xxxxxx"
		"xxxxxxxxxx.xxx/xx-xxxxxxx/xxxxxxx/Xxxxxxxxxxxx-Xxxxx-Xxxx-XXX-Xxxxxx-Xxx.xxx</a></div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "Xxxx xxx xxx xxxxxxx xxxxxxxéxx x'xxxêxxxx à xxxxx, xxx xx xxxxé xx" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "oooxooo xxxxx xxxxx xxxx... xxxx x'xxx xxxxxxxxxxxx xxxxx xxx" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "xxxxxxxx xx \"xx xxxxx xxx xxx xxxxxxx xxxxxxx xxxxxxxxxxxxxx xxxx" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "xxxxx xxxxxx xx xx xxxx xx x'xxxxxx\". Xx xxxx-êxxx xxx xx xxxxxxxx xx" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "xxxx \"x'xxxêxx à xxxxx xx oooxooo xxxx xxx xéxxxxxxxx, xxxx\"...</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "Xxxxx xxxxxx'xx xxx x xxxx xxxxxxx xxxxx xx xxèx xxxxxxxxx" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "xxxxxxxxxxxxxxxx à xx xxx x'xx xx xêxx (éxxxxxxxxx xxxx-xx-xxxxxxxx):" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://xxxxxxxxxxxxxxxx.xxx/xx-xxxxxxx/xxxxxxx/Xxxxx-xxxx-xxx-xxxxxxxxxx-xxxxx.xxx\">https://xxxxxx"
		"xxxxxxxxxx.xxx/xx-xxxxxxx/xxxxxxx/Xxxxx-xxxx-xxx-xxxxxxxxxx-xxxxx.xxx</a>" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) ";&nbsp;</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "...x'x xxxxx xx xxxxxx x'xxxxxx xéxxxxxxx, xx xxx xxxx xxxxxx" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "x'xxxxxxxxxxx xxxxxx, xxxx" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://xxxxxxxxxxxxxxxx.xxx/xxxxxxxx-xxxxxxx-xxxx-xxx-o/\">"
		"https://xxxxxxxxxxxxxxxx.xxx/xxxxxxxx-xxxxxxx-xxxx-xxx-o/</a> xxxxx xxx" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://xxxxxxxxxxxxxxxx.xxx/xxxxxxxx-xxxxxxx-xxxx-xxx-o/\">https://xxxxxxxxx"
		"xxxxxxx.xxx/xxxxxxxx-xxxxxxx-xxxx-xxx-o/</a> ...</div></blockquote>" HTML_SUFFIX,
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
		"> xxxxé:\n"
		"> https://xxxxxxxxxxxxxxxx.xx/xxxxxxx/xxxxx-xxxx-xxxxxxxx-xxxxx-xxxx-xxx-xxxxxxxx-xxx/\n"
		"> xx xx xxxx xéxéxxxxxxx x'xxxxxx xxxx xx xxxxxx xx xxxxxxxxxxxx xx xxx\n"
		"> (xxxxx Xxxxxx) xxxx xxxx x'xxxxxxx xx xxxxxx:\n"
		"> https://xxxxxxxxxxxxxxxx.xxx/xx-xxxxxxx/xxxxxxx/Xxxxxxxxxxxx-Xxxxx-Xxxx-XXX-Xxxxxx-Xxx.xxx\n"
		"> \n"
		"> Xxxx xxx xxx xxxxxxx xxxxxxxéxx x'xxxêxxxx à xxxxx, xxx xx xxxxé xx\n"
		"> oooxooo xxxxx xxxxx xxxx... xxxx x'xxx xxxxxxxxxxxx xxxxx xxx\n"
		"> xxxxxxxx xx \"xx xxxxx xxx xxx xxxxxxx xxxxxxx xxxxxxxxxxxxxx xxxx\n"
		"> xxxxx xxxxxx xx xx xxxx xx x'xxxxxx\". Xx xxxx-êxxx xxx xx xxxxxxxx xx\n"
		"> xxxx \"x'xxxêxx à xxxxx xx oooxooo xxxx xxx xéxxxxxxxx, xxxx\"...\n"
		"> \n"
		"> Xxxxx xxxxxx'xx xxx x xxxx xxxxxxx xxxxx xx xxèx xxxxxxxxx\n"
		"> xxxxxxxxxxxxxxxx à xx xxx x'xx xx xêxx (éxxxxxxxxx xxxx-xx-xxxxxxxx):\n"
		"> https://xxxxxxxxxxxxxxxx.xxx/xx-xxxxxxx/xxxxxxx/Xxxxx-xxxx-xxx-xxxxxxxxxx-xxxxx.xxx\n"
		"> ; \n"
		"> \n"
		"> ...x'x xxxxx xx xxxxxx x'xxxxxx xéxxxxxxx, xx xxx xxxx xxxxxx\n"
		"> x'xxxxxxxxxxx xxxxxx, xxxx\n"
		"> https://xxxxxxxxxxxxxxxx.xxx/xxxxxxxx-xxxxxxx-xxxx-xxx-o/ xxxxx xxx\n"
		"> https://xxxxxxxxxxxxxxxx.xxx/xxxxxxxx-xxxxxxx-xxxx-xxx-o/ ...\n"))
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
		"\\n"
		"prefix text https://www.gnome.org/1234567890123456789012345678901234567890123456789012345678901234567890 after text\\n"
		"prefix text https://www.gnome.org/123456789012345678901234567890123 after text\\n"
		"prefix text https://www.gnome.org/12345678901234567890 https://www.gnome.org/12345678901234567890 after text\\n"
		"prefix text https://www.gnome.org/1234567890123456789012345678901234567890123456789012345678901234567890\\n"
		" next line text\\n"
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
		"<div style=\"width: 71ch;\">prefix text <a href=\"https://www.gnome.org/1234567890123456789012345678901234567890123456789012345678901234567890\">"
		"https://www.gnome.org/1234567890123456789012345678901234567890123456789012345678901234567890</a> after text</div>"
		"<div style=\"width: 71ch;\">prefix text <a href=\"https://www.gnome.org/123456789012345678901234567890123\">"
		"https://www.gnome.org/123456789012345678901234567890123</a> after text</div>"
		"<div style=\"width: 71ch;\">prefix text <a href=\"https://www.gnome.org/12345678901234567890\">"
		"https://www.gnome.org/12345678901234567890</a> <a href=\"https://www.gnome.org/12345678901234567890\">"
		"https://www.gnome.org/12345678901234567890</a> after text</div>"
		"<div style=\"width: 71ch;\">prefix text <a href=\"https://www.gnome.org/1234567890123456789012345678901234567890123456789012345678901234567890\">"
		"https://www.gnome.org/1234567890123456789012345678901234567890123456789012345678901234567890</a></div>"
		"<div style=\"width: 71ch;\"> next line text</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		HTML_SUFFIX,
		"12345678901234567890123456789012345678901234567890123456789012345678901\n"
		"12345678901234567890123456789012345678901234567890123456789012345678901\n"
		"A\n\n"
		"1234567890123456789012345678901234567890123456789012345678901234567890\n"
		"B\n\n"
		"12345678901234567890123456789012345678901234567890123456789012345678901\n"
		"C\n\n"
		"1234567890123456789012345678901234567890123456789012345678901234567890\n"
		"D\n\n"
		"12345678901234567890123456789012345678901234567890123456789012345678901\n"
		UNICODE_NBSP UNICODE_NBSP UNICODE_NBSP "E\n\n"
		"1234567890123456789012345678901234567890123456789012345678901234567890" UNICODE_NBSP "\n"
		UNICODE_NBSP UNICODE_NBSP "F\n\n"
		" 1\n"
		"  2\n"
		"   3\n"
		"\n"
		"prefix text\n"
		"https://www.gnome.org/1234567890123456789012345678901234567890123456789012345678901234567890\n"
		"after text\n"
		"prefix text https://www.gnome.org/123456789012345678901234567890123\n"
		"after text\n"
		"prefix text https://www.gnome.org/12345678901234567890\n"
		"https://www.gnome.org/12345678901234567890 after text\n"
		"prefix text\n"
		"https://www.gnome.org/1234567890123456789012345678901234567890123456789012345678901234567890\n"
		" next line text\n"))
		g_test_fail ();
}

static void
test_issue_86 (TestFixture *fixture)
{
	const gchar *source_text =
		"normal text\n"
		"\n"
		"> level 1\n"
		"> level 1\n"
		"> > level 2\n"
		"> > level 2\n"
		"> >\n"
		"> > level 2\n"
		">\n"
		"> level 1\n"
		"> level 1\n"
		">\n"
		"> > > level 3\n"
		"> > > level 3\n"
		">\n"
		"> > level 2\n"
		"> > level 2\n"
		">\n"
		"> level 1\n"
		"\n"
		"back normal text\n";
	gchar *converted, *to_insert;

	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	converted = camel_text_to_html (source_text,
		CAMEL_MIME_FILTER_TOHTML_PRE |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES |
		CAMEL_MIME_FILTER_TOHTML_QUOTE_CITATION,
		0xDDDDDD);

	g_return_if_fail (converted != NULL);

	to_insert = g_strconcat (converted,
		"<span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span>",
		NULL);

	test_utils_insert_content (fixture, to_insert,
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div>On Today, User wrote:</div>"
		"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
			"<pre>normal text</pre>"
			"<pre><br></pre>"
			"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
				"<pre>level 1</pre>"
				"<pre>level 1</pre>"
				"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
					"<pre>level 2</pre>"
					"<pre>level 2</pre>"
					"<pre><br></pre>"
					"<pre>level 2</pre>"
				"</blockquote>"
				"<pre><br></pre>"
				"<pre>level 1</pre>"
				"<pre>level 1</pre>"
				"<pre><br></pre>"
				"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
					"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
						"<pre>level 3</pre>"
						"<pre>level 3</pre>"
					"</blockquote>"
				"</blockquote>"
				"<pre><br></pre>"
				"<blockquote type=\"cite\" " BLOCKQUOTE_STYLE ">"
					"<pre>level 2</pre>"
					"<pre>level 2</pre>"
				"</blockquote>"
				"<pre><br></pre>"
				"<pre>level 1</pre>"
			"</blockquote>"
			"<pre><br></pre>"
			"<pre>back normal text</pre>"
		"</blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> normal text\n"
		"> \n"
		"> > level 1\n"
		"> > level 1\n"
		"> > > level 2\n"
		"> > > level 2\n"
		"> > > \n"
		"> > > level 2\n"
		"> > \n"
		"> > level 1\n"
		"> > level 1\n"
		"> > \n"
		"> > > > level 3\n"
		"> > > > level 3\n"
		"> > \n"
		"> > > level 2\n"
		"> > > level 2\n"
		"> > \n"
		"> > level 1\n"
		"> \n"
		"> back normal text\n"))
		g_test_fail ();

	g_free (to_insert);
	g_free (converted);
}

static void
test_issue_103 (TestFixture *fixture)
{
	#define LONG_URL "https://www.example.com/123456789012345678901234567890123456789012345678901234567890"
	#define SHORTER_URL "https://www.example.com/1234567890123456789012345678901234567890"
	#define SHORT_URL "https://www.example.com/"

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:before\\n"
		LONG_URL "\\n"
		"after\\n"
		"prefix text " SHORTER_URL " suffix\\n"
		"prefix " SHORT_URL " suffix\\n"
		"end\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">before</div>"
		"<div style=\"width: 71ch;\"><a href=\"" LONG_URL "\">" LONG_URL "</a></div>"
		"<div style=\"width: 71ch;\">after</div>"
		"<div style=\"width: 71ch;\">prefix text <a href=\"" SHORTER_URL "\">" SHORTER_URL "</a> suffix</div>"
		"<div style=\"width: 71ch;\">prefix <a href=\"" SHORT_URL "\">" SHORT_URL "</a> suffix</div>"
		"<div style=\"width: 71ch;\">end</div>"
		HTML_SUFFIX,
		"before\n"
		LONG_URL "\n"
		"after\n"
		"prefix text\n"
		SHORTER_URL " suffix\n"
		"prefix " SHORT_URL " suffix\n"
		"end\n")) {
		g_test_fail ();
		return;
	}

	#undef SHORT_URL
	#undef SHORTER_URL
	#undef LONG_URL
}

static void
test_issue_104 (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:text to replace\n"
		"undo:save\n"	/* 1 */
		"seq:h\n"
		"action:show-replace\n"
		"type:e\t\n"
		"seq:A\n" /* Press 'Alt+A' to press 'Replace All' button */
		"type:a\n"
		"seq:a\n"
		"seq:^\n" /* Close the dialog */
		"undo:undo\n"
		"undo:test:1\n"
		"undo:redo\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">txt to rplac</div>" HTML_SUFFIX,
		"txt to rplac\n"))
		g_test_fail ();
}

static void
test_issue_107 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<pre>text\n"
		"<a href=\"https://www.01.org/\">https://www.01.org/</a>&#160;?\n"
		"<a href=\"https://www.02.org/\">https://www.02.org/</a>&#160;A\n"
		"<a href=\"https://www.03.org/\">https://www.03.org/</a>&#160;ěšč\n"
		"<a href=\"https://www.04.org/\">https://www.04.org/</a> ?\n"
		"<a href=\"https://www.05.org/\">https://www.05.org/</a>\n"
		"<a href=\"https://www.06.org/\">https://www.06.org/</a>&#160;\n"
		"<a href=\"https://www.07.org/\">https://www.07.org/</a>&#160;&#160;\n"
		"<a href=\"https://www.08.org/\">https://www.08.org/</a>&#160;&gt;&#160;&lt;&#160;\n"
		"&lt;<a href=\"https://www.09.org/\">https://www.09.org/</a>&gt;\n"
		"&lt;<a href=\"https://www.10.org/\">https://www.10.org/</a>&#160;?&gt;\n"
		"&#160;<a href=\"https://www.11.org/\">https://www.11.org/</a>&#160;\n"
		"&lt;&#160;<a href=\"https://www.12.org/\">https://www.12.org/</a>&#160;&gt;\n"
		"&#160;&lt;<a href=\"https://www.13.org/\">https://www.13.org/</a>&gt;&#160;\n"
		"Text https://www.14.org/\temail: user@no.where\n"
		"</pre>"
		"<span class=\"-x-evo-to-body\" data-credits=\"On Today, User wrote:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX
		"<div style=\"width: 71ch;\">On Today, User wrote:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "text</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://www.01.org/\">https://www.01.org/</a>&nbsp;?</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://www.02.org/\">https://www.02.org/</a>&nbsp;A</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://www.03.org/\">https://www.03.org/</a>&nbsp;ěšč</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://www.04.org/\">https://www.04.org/</a> ?</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://www.05.org/\">https://www.05.org/</a></div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://www.06.org/\">https://www.06.org/</a>&nbsp;</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://www.07.org/\">https://www.07.org/</a>&nbsp;&nbsp;</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://www.08.org/\">https://www.08.org/</a>&nbsp;&gt;&nbsp;&lt;&nbsp;</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "&lt;<a href=\"https://www.09.org/\">https://www.09.org/</a>&gt;</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "&lt;<a href=\"https://www.10.org/\">https://www.10.org/</a>&nbsp;?&gt;</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "&nbsp;<a href=\"https://www.11.org/\">https://www.11.org/</a>&nbsp;</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "&lt;&nbsp;<a href=\"https://www.12.org/\">https://www.12.org/</a>&nbsp;&gt;</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "&nbsp;&lt;<a href=\"https://www.13.org/\">https://www.13.org/</a>&gt;&nbsp;</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "Text <a href=\"https://www.14.org/\">https://www.14.org/</a>\temail: <a href=\"mailto:user@no.where\">user@no.where</a></div>"
		"</blockquote>" HTML_SUFFIX,
		"On Today, User wrote:\n"
		"> text\n"
		"> https://www.01.org/" UNICODE_NBSP "?\n"
		"> https://www.02.org/" UNICODE_NBSP "A\n"
		"> https://www.03.org/" UNICODE_NBSP "ěšč\n"
		"> https://www.04.org/ ?\n"
		"> https://www.05.org/\n"
		"> https://www.06.org/" UNICODE_NBSP "\n"
		"> https://www.07.org/" UNICODE_NBSP UNICODE_NBSP "\n"
		"> https://www.08.org/" UNICODE_NBSP ">" UNICODE_NBSP "<" UNICODE_NBSP "\n"
		"> <https://www.09.org/>\n"
		"> <https://www.10.org/" UNICODE_NBSP "?>\n"
		"> " UNICODE_NBSP "https://www.11.org/" UNICODE_NBSP "\n"
		"> <" UNICODE_NBSP "https://www.12.org/" UNICODE_NBSP ">\n"
		"> " UNICODE_NBSP "<https://www.13.org/>" UNICODE_NBSP "\n"
		"> Text https://www.14.org/\temail: user@no.where\n")) {
		g_test_fail ();
	}
}

static void
test_issue_884 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<div>Xxxxx'x \"Xxxx 🡒 Xxxxxxxx 🡒 Xxxx Xxxxxxxxxx 🡒 Xxxxxxxx xxx xxxxxxxxxx xxxxxxxx\" xxxxxxx xxxxx xxxx? Xx xxx, xxxx xx xxxxxxx?</div>"
		"<div><br></div>"
		"<div>123456789 123456789 123456789 123456789 123456789 123456789 123456789 123</div>"
		"<div><br></div>"
		"<div>🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈"
		"🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈</div>"
		"<div><br></div>"
		"<div>a🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈</div>"
		"<div><br></div>"
		"<div>ab🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈</div>"
		"<div><br></div>"
		"<div>abc🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈</div>"
		/*"<div><br></div>"
		"<div>abcd🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒</div>"*/,
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX
		"<div style=\"width: 71ch;\">Xxxxx'x \"Xxxx 🡒 Xxxxxxxx 🡒 Xxxx Xxxxxxxxxx 🡒 Xxxxxxxx xxx xxxxxxxxxx xxxxxxxx\" xxxxxxx xxxxx xxxx? Xx xxx, xxxx xx xxxxxxx?</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">123456789 123456789 123456789 123456789 123456789 123456789 123456789 123</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈"
		"🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">a🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">ab🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">abc🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈</div>"
		/*"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">abcd🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒</div>"*/
		HTML_SUFFIX,
		"Xxxxx'x \"Xxxx 🡒 Xxxxxxxx 🡒 Xxxx Xxxxxxxxxx 🡒 Xxxxxxxx xxx xxxxxxxxxx\n"
		"xxxxxxxx\" xxxxxxx xxxxx xxxx? Xx xxx, xxxx xx xxxxxxx?\n"
		"\n"
		"123456789 123456789 123456789 123456789 123456789 123456789 123456789\n"
		"123\n"
		"\n"
		"🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈\n"
		"🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈\n"
		"🐈🐈\n"
		"\n"
		"a🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈\n"
		"🐈🐈🐈\n"
		"\n"
		"ab🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈\n"
		"🐈🐈🐈\n"
		"\n"
		"abc🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈🐈\n"
		"🐈🐈🐈🐈\n"
		/*"\n"
		"abcd🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒🡒\n"
		"🡒🡒🡒\n"*/)) {
		g_test_fail ();
	}
}

static void
test_issue_783 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head></head><body leftmargin=\"0\" topmargin=\"0\" marginwidth=\"0\" marginheight=\"0\" style=\"margin:0;padding:0;background-color:#c8c8c8\">"
		"<div>Mailpoet</div>"
		"</body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX
		"<div>Mailpoet</div>"
		HTML_SUFFIX,
		"Mailpoet\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head><style type=\"text/css\">"
		"body {\n"
		"    margin:0;\n"
		"    font:12px/16px Arial, sans-serif;\n"
		"}\n"
		"</style></head><body style=\"margin: 0; font: 12px/ 16px Arial, sans-serif\">"
		"<div>Amazon</div>"
		"</body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	/* WebKit "normalizes" the 'font' rule; the important part is that the margin is gone from the HTML */
	if (!test_utils_run_simple_test (fixture,
		"",
		"<html><head><style type=\"text/css\">"
		"body { font-style: normal; font-variant-caps: normal; font-weight: normal; font-stretch: normal; "
		"font-size: 12px; line-height: 16px; font-family: Arial, sans-serif; }"
		"</style></head><body style=\"font-style: normal; font-variant-caps: normal; font-weight: normal; "
		"font-stretch: normal; font-size: 12px; line-height: 16px; font-family: Arial, sans-serif;\">"
		"<div>Amazon</div>"
		HTML_SUFFIX,
		"Amazon\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head><style text=\"text/css\">"
		"body { width: 100% !important; -webkit-text-size-adjust: 100% !important; "
		"-ms-text-size-adjust: 100% !important; -webkit-font-smoothing: antialiased "
		"!important; margin: 0 !important; padding: 0 8px 100px 8px; font-family: "
		"'Market Sans', Helvetica, Arial, sans-serif !important; background-color:#ffffff}"
		"</style></head><body yahoo=\"fix\">"
		"<div>eBay</div>"
		"</body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		"<html><head><style text=\"text/css\">"
		"body { background-color: rgb(255, 255, 255); width: 100% !important; -webkit-font-smoothing: antialiased !important;"
		" font-family: \"Market Sans\", Helvetica, Arial, sans-serif !important; }"
		"</style></head><body yahoo=\"fix\">"
		"<div>eBay</div>"
		HTML_SUFFIX,
		"eBay\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<html><head><style text=\"text/css\">"
		"table { color: blue; }\n"
		"body { color: yellow; }\n"
		"body { padding: 10px; }\n"
		"div { color: orange; }"
		"</style></head><body>"
		"<div>Custom</div>"
		"</body></html>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		"<html><head><style text=\"text/css\">"
		"table { color: blue; }\n"
		"body { color: yellow; }\n"
		"div { color: orange; }"
		"</style></head><body yahoo=\"fix\">"
		"<div>Custom</div>"
		HTML_SUFFIX,
		"Custom\n")) {
		g_test_fail ();
		return;
	}
}

static void
test_issue_1197 (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:a\\n\\n\\n\\nb\n"
		"seq:uub\n"
		"type:c\n",
		HTML_PREFIX
		"<div style=\"width: 71ch;\">a</div>"
		"<div style=\"width: 71ch;\">c</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">b</div>"
		HTML_SUFFIX,
		"a\n"
		"c\n"
		"\n"
		"b\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:b\n",
		HTML_PREFIX
		"<div style=\"width: 71ch;\">a</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">b</div>"
		HTML_SUFFIX,
		"a\n"
		"\n"
		"\n"
		"b\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:b\n"
		"type:d\n",
		HTML_PREFIX
		"<div style=\"width: 71ch;\">ad</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">b</div>"
		HTML_SUFFIX,
		"ad\n"
		"\n"
		"b\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:bnn\n",
		HTML_PREFIX
		"<div style=\"width: 71ch;\">a</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">b</div>"
		HTML_SUFFIX,
		"a\n"
		"\n"
		"\n"
		"\n"
		"b\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"type:e\n",
		HTML_PREFIX
		"<div style=\"width: 71ch;\">a</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">e</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">b</div>"
		HTML_SUFFIX,
		"a\n"
		"\n"
		"e\n"
		"\n"
		"b\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:lDD\n"
		"type:f\n",
		HTML_PREFIX
		"<div style=\"width: 71ch;\">a</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">f</div>"
		"<div style=\"width: 71ch;\">b</div>"
		HTML_SUFFIX,
		"a\n"
		"\n"
		"f\n"
		"b\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:bD\n"
		"type:g\n",
		HTML_PREFIX
		"<div style=\"width: 71ch;\">a</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">gb</div>"
		HTML_SUFFIX,
		"a\n"
		"\n"
		"gb\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:bunnuuue\n"
		"mode:html\n"
		"type: \n"
		"action:bold\n"
		"type:bold\n"
		"action:bold\n"
		"type: hh\n"
		"seq:dd",
		HTML_PREFIX
		"<div>a <b>bold</b> hh</div>"
		"<div><br></div>"
		"<div><br></div>"
		"<div><br></div>"
		"<div>b</div>"
		HTML_SUFFIX,
		"a bold hh\n"
		"\n"
		"\n"
		"\n"
		"b\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"type:i\n",
		HTML_PREFIX
		"<div>a <b>bold</b> hh</div>"
		"<div><br></div>"
		"<div>i</div>"
		"<div><br></div>"
		"<div>b</div>"
		HTML_SUFFIX,
		"a bold hh\n"
		"\n"
		"i\n"
		"\n"
		"b\n")) {
		g_test_fail ();
		return;
	}


	if (!test_utils_run_simple_test (fixture,
		"seq:bbb\n"
		"type:j\n",
		HTML_PREFIX
		"<div>a <b>bold</b> hhj</div>"
		"<div><br></div>"
		"<div>b</div>"
		HTML_SUFFIX,
		"a bold hhj\n"
		"\n"
		"b\n")) {
		g_test_fail ();
		return;
	}
}

static void
test_issue_913 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"type:aa \n"
		"action:size-plus-two\n"
		"type:bb \n"
		"action:bold\n"
		"type:cc\n"
		"action:bold\n"
		"type: dd\n"
		"action:size-plus-zero\n"
		"type:\\nee\n",
		HTML_PREFIX
		"<div>aa <font size=\"5\">bb <b>cc</b> dd</font></div>"
		"<div><font size=\"3\">ee</font></div>"
		HTML_SUFFIX,
		"aa bb cc dd\n"
		"ee\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_signature (fixture,
		"<div>tt <b>uu <i>vv</i> <font size=\"2\">ww</font> xx</b> yy</div>",
		TRUE, "UID", FALSE, FALSE, TRUE);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX
		"<div>aa <font size=\"5\">bb <b>cc</b> dd</font></div>"
		"<div><font size=\"3\">ee</font></div>"
		"<div><br></div>"
		"<div><span>"
		"<pre>-- <br></pre>"
		"<div>tt <b>uu <i>vv</i> <font size=\"2\">ww</font> xx</b> yy</div>"
		"</span></div>"
		HTML_SUFFIX,
		"aa bb cc dd\n"
		"ee\n"
		"\n"
		"-- \n"
		"tt uu vv ww xx yy\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n",
		HTML_PREFIX
		"<div style=\"width: 71ch;\">aa bb cc dd</div>"
		"<div style=\"width: 71ch;\">ee</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\"><span>"
		"<pre>-- <br></pre>"
		"<div>tt uu vv ww xx yy</div>"
		"</span></div>"
		HTML_SUFFIX,
		"aa bb cc dd\n"
		"ee\n"
		"\n"
		"-- \n"
		"tt uu vv ww xx yy\n")) {
		g_test_fail ();
		return;
	}
}

static void
test_issue_1214 (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:html\n"
		"type:a\\nb\\nc\\nd\\n\n"
		"seq:ChcSddds\n"
		"action:style-list-bullet\n"
		"seq:Chcden\n"
		"type:ee\n"
		"seq:Sdds\n"
		"action:style-normal\n",
		HTML_PREFIX
		"<ul>"
		"<li>a</li>"
		"<li>b</li>"
		"</ul>"
		"<div>ee</div>"
		"<div>c</div>"
		"<div>d</div>"
		"<div><br></div>"
		HTML_SUFFIX,
		" * a\n"
		" * b\n"
		"ee\n"
		"c\n"
		"d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"action:select-all\n"
		"seq:Db\n"
		"type:a\\nb\\nc\\nd\\n\n"
		"seq:ChcSddds\n"
		"action:style-list-bullet\n"
		"seq:Chcden\n"
		"type:ee\n"
		"seq:ddeSuus\n"
		"action:style-normal\n",
		HTML_PREFIX
		"<ul>"
		"<li>a</li>"
		"<li>b</li>"
		"</ul>"
		"<div>ee</div>"
		"<div>c</div>"
		"<div>d</div>"
		"<div><br></div>"
		HTML_SUFFIX,
		" * a\n"
		" * b\n"
		"ee\n"
		"c\n"
		"d\n")) {
		g_test_fail ();
		return;
	}
}

static void
test_issue_1330 (TestFixture *fixture)
{
	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-wrap-quoted-text-in-replies", TRUE);
	test_utils_fixture_change_setting_int32 (fixture, "org.gnome.evolution.mail", "composer-word-wrap-length", 12);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><div>123 567 90 <br></div>"
		"<div>2345678901 <br></div>"
		"<div>34 67 9012 <br></div>"
		"<div>45678 0 2 4 6 8 0</div>"
		"<span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 12ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "123 567 90</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "2345678901</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "34 67 9012</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "45678 0 2" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "4 6 8 0</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> 123 567 90\n"
		"> 2345678901\n"
		"> 34 67 9012\n"
		"> 45678 0 2\n"
		"> 4 6 8 0\n"))
		g_test_fail ();
}

static void
test_issue_1157 (TestFixture *fixture)
{
	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-wrap-quoted-text-in-replies", TRUE);
	test_utils_fixture_change_setting_int32 (fixture, "org.gnome.evolution.mail", "composer-word-wrap-length", 12);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><div>123 567 90 <br></div>"
		"<div>2345678901 <br></div>"
		"<span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	test_utils_set_clipboard_text ("http://e.c/", FALSE);

	if (!test_utils_run_simple_test (fixture,
		"seq:Cecn\n"
		"action:paste\n",
		HTML_PREFIX "<div style=\"width: 12ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "123 567 90</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "2345678901</div>"
		"</blockquote>"
		"<div style=\"width: 12ch;\"><a href=\"http://e.c/\">http://e.c/</a></div>"
		HTML_SUFFIX,
		"Credits:\n"
		"> 123 567 90\n"
		"> 2345678901\n"
		"http://e.c/\n"))
		g_test_fail ();
}

static void
test_issue_1365 (TestFixture *fixture)
{
	test_utils_fixture_change_setting_boolean (fixture, "org.gnome.evolution.mail", "composer-wrap-quoted-text-in-replies", TRUE);
	test_utils_fixture_change_setting_int32 (fixture, "org.gnome.evolution.mail", "composer-word-wrap-length", 25);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><div>aa bb,</div>"
		"<div><br></div>"
		"<div>cc dd. &nbsp;ee ff.</div>"
		"<div><br></div>"
		"<div>xxxx:</div>"
		"<div>123456789 1234 6789 123 56 89.<br><br></div>"
		"<div class=\"gmail_quote\">yyy:</div>"
		"<blockquote type=\"cite\" style=\"margin:0 0 0 .8ex; border-left:2px #729fcf solid;padding-left:1ex\">"
		"<pre class=\"k9mail\">zz yy,<br>"
		"<br>"
		"xxx,<br>"
		"www 123456789 12 456 89 123 567 9 12 456 89.<br>"
		"<br>"
		"vv,<br>"
		" uu<br></pre>"
		"<blockquote type=\"cite\" style=\"margin:0 0 0 .8ex; border-left:2px #729fcf solid;padding-left:1ex\">"
		"<pre class=\"k9mail\">ZZ YY,<br>"
		"<br>"
		"XXX,<br>"
		"WWW 987654321 98 654 21 987 543 1 98 654 21.<br>"
		"<br>"
		"VV,<br>"
		" UU<br></pre>"
		"</blockquote>"
		"</blockquote>"
		"<div><br></div>"
		"<span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 25ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "aa bb,</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "cc dd. &nbsp;ee ff.</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "xxxx:</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "123456789 1234 6789 123" WRAP_BR_SPC
			QUOTE_SPAN (QUOTE_CHR) "56 89.</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "yyy:</div>"
			"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "zz yy,</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "<br></div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "xxx,</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "www 123456789 12 456" WRAP_BR_SPC
				QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "89 123 567 9 12 456" WRAP_BR_SPC
				QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "89.</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "<br></div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "vv,</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "&nbsp;uu</div>"
			"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR QUOTE_CHR) "ZZ YY,</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR QUOTE_CHR) "<br></div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR QUOTE_CHR) "XXX,</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR QUOTE_CHR) "WWW 987654321 98" WRAP_BR_SPC
				QUOTE_SPAN (QUOTE_CHR QUOTE_CHR QUOTE_CHR) "654 21 987 543 1 98" WRAP_BR_SPC
				QUOTE_SPAN (QUOTE_CHR QUOTE_CHR QUOTE_CHR) "654 21.</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR QUOTE_CHR) "<br></div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR QUOTE_CHR) "VV,</div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR QUOTE_CHR) "&nbsp;UU</div>"
			"</blockquote>"
			"</blockquote>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> aa bb,\n"
		"> \n"
		"> cc dd. " UNICODE_NBSP "ee ff.\n"
		"> \n"
		"> xxxx:\n"
		"> 123456789 1234 6789 123\n"
		"> 56 89.\n"
		"> \n"
		"> yyy:\n"
		"> > zz yy,\n"
		"> > \n"
		"> > xxx,\n"
		"> > www 123456789 12 456\n"
		"> > 89 123 567 9 12 456\n"
		"> > 89.\n"
		"> > \n"
		"> > vv,\n"
		"> > " UNICODE_NBSP "uu\n"
		"> > > ZZ YY,\n"
		"> > > \n"
		"> > > XXX,\n"
		"> > > WWW 987654321 98\n"
		"> > > 654 21 987 543 1 98\n"
		"> > > 654 21.\n"
		"> > > \n"
		"> > > VV,\n"
		"> > > " UNICODE_NBSP "UU\n"
		"> \n"))
		g_test_fail ();
}

static void
test_issue_1344 (TestFixture *fixture)
{
	if (!test_utils_run_simple_test (fixture,
		"mode:plain\n"
		"type:\\n\\na\\nb\\n\n"
		"seq:Chcd\n"
		"action:style-list-bullet\n"
		"type:c\n",
		HTML_PREFIX
		"<div style=\"width: 71ch;\"><br></div>"
		"<ul style=\"width: 68ch;\"><li>c</li></ul>"
		"<div style=\"width: 71ch;\">a</div>"
		"<div style=\"width: 71ch;\">b</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		HTML_SUFFIX,
		"\n"
		" * c\n"
		"a\n"
		"b\n"
		"\n"))
		g_test_fail ();
}

static void
test_issue_1391 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><div>a</div>"
		"<div>b</div>"
		"<div>c</div>"
		"<blockquote type=\"cite\" style=\"margin:0 0 0 .8ex; border-left:2px #729fcf solid;padding-left:1ex\">"
		"<div>d</div>"
		"<div>ee</div>"
		"<div>f</div>"
		"<blockquote type=\"cite\" style=\"margin:0 0 0 .8ex; border-left:2px #729fcf solid;padding-left:1ex\">"
		"<div>g</div>"
		"<div>h</div>"
		"<pre>i</pre>"
		"</blockquote>"
		"</blockquote>"
		"<div><br></div>"
		"<span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:Chcdde\n"
		"type:1\n"
		"action:style-preformat\n"
		"seq:dddhr\n"
		"type:2\n"
		"action:style-preformat\n"
		"seq:dddh\n"
		"type:3\n"
		"action:style-preformat\n"
		"seq:de\n"
		"type:4\n"
		"action:style-normal\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "a</div>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "b1</pre>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "c</div>"
			"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "d</div>"
			"<pre>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "e2e</pre>"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "f</div>"
			"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR QUOTE_CHR) "g</div>"
			"<pre>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR QUOTE_CHR) "3h</pre>"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR QUOTE_CHR) "i4</div>"
			"</blockquote>"
			"</blockquote>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> a\n"
		"> b1\n"
		"> c\n"
		"> > d\n"
		"> > e2e\n"
		"> > f\n"
		"> > > g\n"
		"> > > 3h\n"
		"> > > i4\n"
		"> \n"))
		g_test_fail ();
}

static gboolean
test_issue_1394_with_wrap_length (TestFixture *fixture,
				  gint wrap_length)
{
	gchar *html;

	test_utils_fixture_change_setting_int32 (fixture, "org.gnome.evolution.mail", "composer-word-wrap-length", wrap_length);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n"))
		return FALSE;

	test_utils_insert_content (fixture,
		"<body><div>a</div>"
		"<blockquote type=\"cite\">"
		"<div>b</div>"
		"<div><a href=\"https://www.example.com/\">https://www.example.com/</a></div>"
		"<div>c</div>"
		"</blockquote>"
		"<div><br></div>"
		"<span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	html = g_strdup_printf (
		HTML_PREFIX "<div style=\"width: %dch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
			"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "<a href=\"https://www.example.com/\">https://www.example.com/</a></div>"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "c</div>"
			"</blockquote>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div>"
		"</blockquote>"
		HTML_SUFFIX,
		wrap_length);

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n"
		"seq:dddSllllsb\n",
		html,
		"Credits:\n"
		"> > https://www.example.com/\n"
		"> > c\n"
		"> \n")) {
		g_free (html);
		return FALSE;
	}

	g_free (html);

	if (!test_utils_process_commands (fixture,
		"undo:save\n"
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:2"))
		return FALSE;

	html = g_strdup_printf (
		HTML_PREFIX "<div style=\"width: %dch;\">Credits:<a href=\"https://www.example.com/\">https://www.example.com/</a></div>"
		"<blockquote type=\"cite\">"
			"<blockquote type=\"cite\">"
			"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "c</div>"
			"</blockquote>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "<br></div>"
		"</blockquote>"
		HTML_SUFFIX,
		wrap_length);

	if (!test_utils_run_simple_test (fixture,
		"undo:save\n"
		"seq:b\n",
		html,
		"Credits:https://www.example.com/\n"
		"> > c\n"
		"> \n")) {
		g_free (html);
		return FALSE;
	}

	g_free (html);

	if (!test_utils_process_commands (fixture,
		"undo:save\n"
		"undo:undo\n"
		"undo:test:2\n"
		"undo:redo\n"
		"undo:test\n"
		"undo:drop:2"))
		return FALSE;

	return TRUE;
}

static void
test_issue_1394 (TestFixture *fixture)
{
	if (!test_issue_1394_with_wrap_length (fixture, 50) ||
	    !test_issue_1394_with_wrap_length (fixture, 10))
		g_test_fail ();
}

static void
test_issue_1159 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><div>a</div>"
		"<div>b</div>"
		"<div>c</div>"
		"<span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:Cecn\n"
		"seq:ChcddSdds\n"
		"seq:n\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "a</div>"
		"</blockquote>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\"><br></div>"
		HTML_SUFFIX,
		"Credits:\n"
		"> a\n"
		"\n"))
		g_test_fail ();
}

static void
test_issue_1424_level1 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><div>a</div>"
		"<div>b</div>"
		"<div>c</div>"
		"<span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"seq:c\n"
		"action:style-preformat\n",
		HTML_PREFIX "<pre>Credits:</pre>"
		"<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "a</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "b</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "c</pre>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> a\n"
		"> b\n"
		"> c\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"action:style-normal\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "a</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "b</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "c</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> a\n"
		"> b\n"
		"> c\n")) {
		g_test_fail ();
		return;
	}
}

static void
test_issue_1424_level2 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><div>a</div>"
		"<blockquote type=\"cite\">"
		"<div>b</div>"
		"<div>c</div>"
		"</blockquote>"
		"<div>d</div>"
		"<span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:C\n"
		"type:a\n"
		"seq:c\n"
		"action:style-preformat\n",
		HTML_PREFIX "<pre>Credits:</pre>"
		"<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "a</pre>"
		"<blockquote type=\"cite\">"
		"<pre>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "b</pre>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "c</pre>"
		"</blockquote>"
		"<pre>" QUOTE_SPAN (QUOTE_CHR) "d</pre>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> a\n"
		"> > b\n"
		"> > c\n"
		"> d\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"action:style-normal\n",
		HTML_PREFIX "<div style=\"width: 71ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "a</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "b</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "c</div>"
		"</blockquote>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "d</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> a\n"
		"> > b\n"
		"> > c\n"
		"> d\n")) {
		g_test_fail ();
		return;
	}
}

static void
test_issue_1439 (TestFixture *fixture)
{
	if (!test_utils_process_commands (fixture,
		"mode:html\n")) {
		g_test_fail ();
		return;
	}

	test_utils_set_clipboard_text ("The <a href='http://www.example.com/'>example com</a> site", TRUE);

	if (!test_utils_run_simple_test (fixture,
		"action:style-list-bullet\n"
		"type:l1\\nl2\\n\n"
		"action:paste\n"
		"type:\\nl4\n",
		HTML_PREFIX "<ul>"
		"<li>l1</li>"
		"<li>l2</li>"
		"<li>The <a href='http://www.example.com/'>example com</a> site</li>"
		"<li>l4</li>"
		"</ul>"
		HTML_SUFFIX,
		" * l1\n"
		" * l2\n"
		" * The example com [1] site\n"
		" * l4\n"
		"\n"
		"[1] example com http://www.example.com/\n"))
		g_test_fail ();
}

static void
test_issue_1392 (TestFixture *fixture)
{
	test_utils_fixture_change_setting_int32 (fixture, "org.gnome.evolution.mail", "composer-word-wrap-length", 28);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><div>aaaa <a href=\"https://www.example.com/\">www.example.com/aaabbbccc</a> bbbb</div>"
		"<div>ccc</div>"
		"<body><div>dddd <a href=\"https://www.example.com/\">www.example.com/eeefffggg</a> hhh</div>"
		"<div>iii</div>"
		"<span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"seq:ddelbbbblbb\n"
		"type:1\n",
		HTML_PREFIX "<div style=\"width: 28ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "aaaa <a href=\"https://www.example.com/aaabbbccc\">www.example.com/a1bc</a>" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "bbbb</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "ccc</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "dddd" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://www.example.com/eeefffggg\">www.example.com/eeefffggg</a>" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "hhh</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "iii</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> aaaa www.example.com/a1bc\n"
		"> bbbb\n"
		"> ccc\n"
		"> dddd\n"
		"> www.example.com/eeefffggg\n"
		"> hhh\n"
		"> iii\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:hddddbdelbbbblbb\n"
		"type:2\n",
		HTML_PREFIX "<div style=\"width: 28ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "aaaa <a href=\"https://www.example.com/aaabbbccc\">www.example.com/a1bc</a>" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "bbbb</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "ccc</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "dddd<a href=\"https://www.example.com/eeefffggg\">www.example.com/e2fg</a>" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "hhh</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "iii</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> aaaa www.example.com/a1bc\n"
		"> bbbb\n"
		"> ccc\n"
		"> ddddwww.example.com/e2fg\n"
		"> hhh\n"
		"> iii\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:hr\n"
		"type:333\n",
		HTML_PREFIX "<div style=\"width: 28ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "aaaa <a href=\"https://www.example.com/aaabbbccc\">www.example.com/a1bc</a>" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "bbbb</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "ccc</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "d333ddd" WRAP_BR
		QUOTE_SPAN (QUOTE_CHR) "<a href=\"https://www.example.com/eeefffggg\">www.example.com/e2fg</a> hhh</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "iii</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> aaaa www.example.com/a1bc\n"
		"> bbbb\n"
		"> ccc\n"
		"> d333ddd\n"
		"> www.example.com/e2fg hhh\n"
		"> iii\n"))
		g_test_fail ();
}

static void
test_issue_1708 (TestFixture *fixture)
{
	test_utils_fixture_change_setting_int32 (fixture, "org.gnome.evolution.mail", "composer-word-wrap-length", 10);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><div>aaa bbb ccc</div>"
		"<div>1234567</div>"
		"<div>dd 1234567</div>"
		"<div>1234567 ee</div>"
		"<div>ff 1234567 gg</div>"
		"<div>12345678</div>"
		"<div>12345678 hh</div>"
		"<div>ii 12345678</div>"
		"<div>jj 12345678 kk</div>"
		"<div>1234567890123456</div>"
		"<div>ll 1234567890123456</div>"
		"<div>1234567890123456 mm</div>"
		"<div>nn 1234567890123456 oo</div>"
		"<div>123456789012345678901</div>"
		"<div>pp 123456789012345678901</div>"
		"<div>123456789012345678901 qq</div>"
		"<div>rr 123456789012345678901 tt</div>"
		"<div>uuu 123456789012345678901 vvv</div>"
		"<span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 10ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "aaa bbb" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "ccc</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "1234567</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "dd" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "1234567</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "1234567" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "ee</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "ff" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "1234567" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "gg</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "12345678</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "12345678" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "hh</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "ii" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "12345678</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "jj" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "12345678" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "kk</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "12345678" WRAP_BR
		QUOTE_SPAN (QUOTE_CHR) "90123456</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "ll" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "12345678" WRAP_BR
		QUOTE_SPAN (QUOTE_CHR) "90123456</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "12345678" WRAP_BR
		QUOTE_SPAN (QUOTE_CHR) "90123456" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "mm</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "nn" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "12345678" WRAP_BR
		QUOTE_SPAN (QUOTE_CHR) "90123456" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "oo</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "12345678" WRAP_BR
		QUOTE_SPAN (QUOTE_CHR) "90123456" WRAP_BR
		QUOTE_SPAN (QUOTE_CHR) "78901</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "pp" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "12345678" WRAP_BR
		QUOTE_SPAN (QUOTE_CHR) "90123456" WRAP_BR
		QUOTE_SPAN (QUOTE_CHR) "78901</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "12345678" WRAP_BR
		QUOTE_SPAN (QUOTE_CHR) "90123456" WRAP_BR
		QUOTE_SPAN (QUOTE_CHR) "78901 qq</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "rr" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "12345678" WRAP_BR
		QUOTE_SPAN (QUOTE_CHR) "90123456" WRAP_BR
		QUOTE_SPAN (QUOTE_CHR) "78901 tt</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "uuu" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "12345678" WRAP_BR
		QUOTE_SPAN (QUOTE_CHR) "90123456" WRAP_BR
		QUOTE_SPAN (QUOTE_CHR) "78901" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "vvv</div>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> aaa bbb\n"
		"> ccc\n"
		"> 1234567\n"
		"> dd\n"
		"> 1234567\n"
		"> 1234567\n"
		"> ee\n"
		"> ff\n"
		"> 1234567\n"
		"> gg\n"
		"> 12345678\n"
		"> 12345678\n"
		"> hh\n"
		"> ii\n"
		"> 12345678\n"
		"> jj\n"
		"> 12345678\n"
		"> kk\n"
		"> 12345678\n"
		"> 90123456\n"
		"> ll\n"
		"> 12345678\n"
		"> 90123456\n"
		"> 12345678\n"
		"> 90123456\n"
		"> mm\n"
		"> nn\n"
		"> 12345678\n"
		"> 90123456\n"
		"> oo\n"
		"> 12345678\n"
		"> 90123456\n"
		"> 78901\n"
		"> pp\n"
		"> 12345678\n"
		"> 90123456\n"
		"> 78901\n"
		"> 12345678\n"
		"> 90123456\n"
		"> 78901 qq\n"
		"> rr\n"
		"> 12345678\n"
		"> 90123456\n"
		"> 78901 tt\n"
		"> uuu\n"
		"> 12345678\n"
		"> 90123456\n"
		"> 78901\n"
		"> vvv\n"))
		g_test_fail ();
}

static void
test_issue_1763 (TestFixture *fixture)
{
	test_utils_fixture_change_setting_int32 (fixture, "org.gnome.evolution.mail", "composer-word-wrap-length", 20);

	if (!test_utils_process_commands (fixture,
		"mode:plain\n")) {
		g_test_fail ();
		return;
	}

	test_utils_insert_content (fixture,
		"<body><div>aaa &lt;u@no.where&gt; bbb ccc</div>"
		"<div>12345678 0123 5678</div>"
		"<blockquote type=\"cite\">"
		"<div>ddd www.no.whr eee fff ggg hhh iii jjj</div>"
		"<div>123 4567890 123456 789012 34567</div>"
		"</blockquote>"
		"<span class=\"-x-evo-to-body\" data-credits=\"Credits:\"></span>"
		"<span class=\"-x-evo-cite-body\"></span></body>",
		E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	if (!test_utils_run_simple_test (fixture,
		"",
		HTML_PREFIX "<div style=\"width: 20ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "aaa &lt;<a href=\"mailto:u@no.where\">u@no.where</a>&gt;" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "bbb ccc</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "12345678 0123 5678</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "ddd <a href=\"https://www.no.whr\">www.no.whr</a>" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "eee fff ggg hhh" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "iii jjj</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "123 4567890" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "123456 789012" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "34567</div>"
		"</blockquote>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> aaa <u@no.where>\n"
		"> bbb ccc\n"
		"> 12345678 0123 5678\n"
		"> > ddd www.no.whr\n"
		"> > eee fff ggg hhh\n"
		"> > iii jjj\n"
		"> > 123 4567890\n"
		"> > 123456 789012\n"
		"> > 34567\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"seq:d\n"
		"type:1",
		HTML_PREFIX "<div style=\"width: 20ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "1aaa &lt;<a href=\"mailto:u@no.where\">u@no.where</a>&gt;" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "bbb ccc</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "12345678 0123 5678</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "ddd <a href=\"https://www.no.whr\">www.no.whr</a>" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "eee fff ggg hhh" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "iii jjj</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "123 4567890" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "123456 789012" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "34567</div>"
		"</blockquote>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> 1aaa <u@no.where>\n"
		"> bbb ccc\n"
		"> 12345678 0123 5678\n"
		"> > ddd www.no.whr\n"
		"> > eee fff ggg hhh\n"
		"> > iii jjj\n"
		"> > 123 4567890\n"
		"> > 123456 789012\n"
		"> > 34567\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"type:2",
		HTML_PREFIX "<div style=\"width: 20ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "12aaa &lt;<a href=\"mailto:u@no.where\">u@no.where</a>&gt;" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "bbb ccc</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "12345678 0123 5678</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "ddd <a href=\"https://www.no.whr\">www.no.whr</a>" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "eee fff ggg hhh" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "iii jjj</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "123 4567890" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "123456 789012" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "34567</div>"
		"</blockquote>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> 12aaa <u@no.where>\n"
		"> bbb ccc\n"
		"> 12345678 0123 5678\n"
		"> > ddd www.no.whr\n"
		"> > eee fff ggg hhh\n"
		"> > iii jjj\n"
		"> > 123 4567890\n"
		"> > 123456 789012\n"
		"> > 34567\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"type:3",
		HTML_PREFIX "<div style=\"width: 20ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "123aaa" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "&lt;<a href=\"mailto:u@no.where\">u@no.where</a>&gt; bbb" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "ccc</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "12345678 0123 5678</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "ddd <a href=\"https://www.no.whr\">www.no.whr</a>" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "eee fff ggg hhh" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "iii jjj</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "123 4567890" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "123456 789012" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "34567</div>"
		"</blockquote>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> 123aaa\n"
		"> <u@no.where> bbb\n"
		"> ccc\n"
		"> 12345678 0123 5678\n"
		"> > ddd www.no.whr\n"
		"> > eee fff ggg hhh\n"
		"> > iii jjj\n"
		"> > 123 4567890\n"
		"> > 123456 789012\n"
		"> > 34567\n")) {
		g_test_fail ();
		return;
	}

	if (!test_utils_run_simple_test (fixture,
		"type:4",
		HTML_PREFIX "<div style=\"width: 20ch;\">Credits:</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "1234aaa" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "&lt;<a href=\"mailto:u@no.where\">u@no.where</a>&gt; bbb" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR) "ccc</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR) "12345678 0123 5678</div>"
		"<blockquote type=\"cite\">"
		"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "ddd <a href=\"https://www.no.whr\">www.no.whr</a>" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "eee fff ggg hhh" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "iii jjj</div>"
		"<div>" QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "123 4567890" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "123456 789012" WRAP_BR_SPC
		QUOTE_SPAN (QUOTE_CHR QUOTE_CHR) "34567</div>"
		"</blockquote>"
		"</blockquote>"
		HTML_SUFFIX,
		"Credits:\n"
		"> 1234aaa\n"
		"> <u@no.where> bbb\n"
		"> ccc\n"
		"> 12345678 0123 5678\n"
		"> > ddd www.no.whr\n"
		"> > eee fff ggg hhh\n"
		"> > iii jjj\n"
		"> > 123 4567890\n"
		"> > 123456 789012\n"
		"> > 34567\n")) {
		g_test_fail ();
		return;
	}
}

void
test_add_html_editor_bug_tests (void)
{
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
	test_utils_add_test ("/bug/780275-html", test_bug_780275_html);
	test_utils_add_test ("/bug/780275-plain", test_bug_780275_plain);
	test_utils_add_test ("/bug/781722", test_bug_781722);
	test_utils_add_test ("/bug/781116", test_bug_781116);
	test_utils_add_test ("/bug/780088", test_bug_780088);
	test_utils_add_test ("/bug/788829", test_bug_788829);
	test_utils_add_test ("/bug/750636", test_bug_750636);
	test_utils_add_test ("/issue/86", test_issue_86);
	test_utils_add_test ("/issue/103", test_issue_103);
	test_utils_add_test ("/issue/104", test_issue_104);
	test_utils_add_test ("/issue/107", test_issue_107);
	test_utils_add_test ("/issue/884", test_issue_884);
	test_utils_add_test ("/issue/783", test_issue_783);
	test_utils_add_test ("/issue/1197", test_issue_1197);
	test_utils_add_test ("/issue/913", test_issue_913);
	test_utils_add_test ("/issue/1214", test_issue_1214);
	test_utils_add_test ("/issue/1330", test_issue_1330);
	test_utils_add_test ("/issue/1157", test_issue_1157);
	test_utils_add_test ("/issue/1365", test_issue_1365);
	test_utils_add_test ("/issue/1344", test_issue_1344);
	test_utils_add_test ("/issue/1391", test_issue_1391);
	test_utils_add_test ("/issue/1394", test_issue_1394);
	test_utils_add_test ("/issue/1159", test_issue_1159);
	test_utils_add_test ("/issue/1424-level1", test_issue_1424_level1);
	test_utils_add_test ("/issue/1424-level2", test_issue_1424_level2);
	test_utils_add_test ("/issue/1439", test_issue_1439);
	test_utils_add_test ("/issue/1392", test_issue_1392);
	test_utils_add_test ("/issue/1708", test_issue_1708);
	test_utils_add_test ("/issue/1763", test_issue_1763);
}
