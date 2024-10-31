/*
 * Copyright (C) 2022 Red Hat (www.redhat.com)
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

#include <locale.h>
#include <e-util/e-util.h>

typedef struct _TestFixture {
	gboolean dummy;
} TestFixture;

static void
test_fixture_setup (TestFixture *fixture,
		    gconstpointer user_data)
{
}

static void
test_fixture_tear_down (TestFixture *fixture,
			gconstpointer user_data)
{
}

#define test_markdown_convert(html, expected_markdown, flags) G_STMT_START { \
		gchar *converted; \
		const gchar *expected = expected_markdown; \
		converted = e_markdown_utils_html_to_text (html, -1, flags); \
		g_assert_nonnull (converted); \
		g_assert_cmpstr (converted, ==, expected); \
		g_free (converted); \
	} G_STMT_END

#define HTML_PREFIX "<html><head></head><body>"
#define HTML_SUFFIX "</body></html>"

static void
test_markdown_convert_single_line_with_br (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div style=\"width: 71ch;\">Single line with br<br></div>"
		HTML_SUFFIX,
		"Single line with br\n", 0);
}

static void
test_markdown_convert_single_line_without_br (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div style=\"width: 71ch;\">Single line without br</div>"
		HTML_SUFFIX,
		"Single line without br\n", 0);
}

static void
test_markdown_convert_two_lines_with_br (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div style=\"width: 71ch;\">First and only line<br></div>"
		"<div style=\"width: 71ch;\"><br></div>"
		HTML_SUFFIX,
		"First and only line\n"
		"\n", 0);
}

static void
test_markdown_convert_two_lines_without_br (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div style=\"width: 71ch;\">First and only line</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		HTML_SUFFIX,
		"First and only line\n"
		"\n", 0);
}

static void
test_markdown_convert_soft_line_break (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div style=\"width: 71ch;\">Text1\nText2</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		HTML_SUFFIX,
		"Text1  \n"
		"Text2\n"
		"\n", E_MARKDOWN_HTML_TO_TEXT_FLAG_SIGNIFICANT_NL);

	test_markdown_convert (
		HTML_PREFIX
		"<div style=\"width: 71ch;\">Text1\nText2</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		HTML_SUFFIX,
		"Text1 Text2\n"
		"\n", 0);
}

static void
test_markdown_convert_br_line_break (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div style=\"width: 71ch;\">Text1<br>Text2</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		HTML_SUFFIX,
		"Text1  \n"
		"Text2\n"
		"\n", 0);
}

static void
test_markdown_convert_brnl_line_break (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div style=\"width: 71ch;\">Text1<br>\nText2</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		HTML_SUFFIX,
		"Text1  \n"
		"Text2\n"
		"\n", 0);
}

static void
test_markdown_convert_nlbr_line_break (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div style=\"width: 71ch;\">Text1\n<br>Text2</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		HTML_SUFFIX,
		"Text1  \n"
		"Text2\n"
		"\n", 0);
}

static void
test_markdown_convert_consecutive_paragraphs (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div style=\"width: 71ch;\">Text1\nText2<br></div>"
		"<div style=\"width: 71ch;\">Text3</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">Text4</div>"
		HTML_SUFFIX,
		"Text1  \n"
		"Text2  \n"
		"Text3\n"
		"\n"
		"Text4\n", E_MARKDOWN_HTML_TO_TEXT_FLAG_SIGNIFICANT_NL);

	test_markdown_convert (
		HTML_PREFIX
		"<div style=\"width: 71ch;\">Text1\nText2<br></div>"
		"<div style=\"width: 71ch;\">Text3</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">Text4</div>"
		HTML_SUFFIX,
		"Text1 Text2  \n"
		"Text3\n"
		"\n"
		"Text4\n", 0);
}

static void
test_markdown_convert_trailing_spaces (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div style=\"width: 71ch;\">Text1</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">   Text2   </div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">   Text3 Text   4       \n Text 5      </div>"
		"<div style=\"width: 71ch;\"><br></div>"
		HTML_SUFFIX,
		"Text1\n"
		"\n"
		"Text2\n"
		"\n"
		"Text3 Text   4  \n"
		"Text 5\n"
		"\n", E_MARKDOWN_HTML_TO_TEXT_FLAG_SIGNIFICANT_NL);

	test_markdown_convert (
		HTML_PREFIX
		"<div style=\"width: 71ch;\">Text1</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">   Text2   </div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">   Text3 Text   4       <br> Text 5      </div>"
		"<div style=\"width: 71ch;\"><br></div>"
		HTML_SUFFIX,
		"Text1\n"
		"\n"
		"Text2\n"
		"\n"
		"Text3 Text   4  \n"
		"Text 5\n"
		"\n", 0);
}

static void
test_markdown_convert_complex_plain (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div style=\"width: 71ch;\">The first paragraph with br<br></div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">The second paragraph, single line.<br></div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">The third paragraph, with\na soft line break.</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">The fourth paragraph, with BR<br> and BR<br>\nfollowed with a new line.</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">then with</div>"
		"<div style=\"width: 71ch;\">hard line break<br></div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\">end of message.<br></div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\"><span></span></div>"
		HTML_SUFFIX,
		"The first paragraph with br\n"
		"\n"
		"The second paragraph, single line.\n"
		"\n"
		"The third paragraph, with  \n"
		"a soft line break.\n"
		"\n"
		"The fourth paragraph, with BR  \n"
		"and BR  \n"
		"  \n"
		"followed with a new line.\n"
		"\n"
		"then with  \n"
		"hard line break\n"
		"\n"
		"end of message.\n"
		"\n"
		"\n", E_MARKDOWN_HTML_TO_TEXT_FLAG_SIGNIFICANT_NL);
}

static void
test_markdown_convert_h1 (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<h1>Header 1</h1>"
		"<div><br></div>"
		HTML_SUFFIX,
		"# Header 1\n"
		"\n", 0);
}

static void
test_markdown_convert_h2 (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<h2>Header 2</h2>"
		"<div><br></div>"
		HTML_SUFFIX,
		"## Header 2\n"
		"\n", 0);
}

static void
test_markdown_convert_h3 (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<h3>Header 3</h3>"
		"<div><br></div>"
		HTML_SUFFIX,
		"### Header 3\n"
		"\n", 0);
}

static void
test_markdown_convert_h4 (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<h4>Header 4</h4>"
		"<div><br></div>"
		HTML_SUFFIX,
		"#### Header 4\n"
		"\n", 0);
}

static void
test_markdown_convert_h5 (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<h5>Header 5</h5>"
		"<div><br></div>"
		HTML_SUFFIX,
		"##### Header 5\n"
		"\n", 0);
}

static void
test_markdown_convert_h6 (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<h6>Header 6</h6>"
		"<div><br></div>"
		HTML_SUFFIX,
		"###### Header 6\n"
		"\n", 0);
}

static void
test_markdown_convert_anchor (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div>text 1 <a href='www.gnome.org'>GNOME link</a> text 2</div>"
		"<div><br></div>"
		HTML_SUFFIX,
		"text 1 [GNOME link](www.gnome.org) text 2\n"
		"\n", 0);
}

static void
test_markdown_convert_bold (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<p>before <b>bold text</b> after</p>"
		"<p><br></p>"
		"<p>before <strong>strong text</strong> after</p>"
		"<div><br></div>"
		HTML_SUFFIX,
		"before **bold text** after\n"
		"\n"
		"before **strong text** after\n"
		"\n", 0);
}

static void
test_markdown_convert_italic (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<DIV>before <I>italic text</I> after</DIV>"
		"<P><BR></P>"
		"<DIV>before <EM>caps em text</EM> after</div>"
		"<diV><br></Div>"
		"<DIV>before <em>lows em text</em> after</div>"
		"<diV><br></Div>"
		HTML_SUFFIX,
		"before *italic text* after\n"
		"\n"
		"before *caps em text* after\n"
		"\n"
		"before *lows em text* after\n"
		"\n", 0);
}

static void
test_markdown_convert_code (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div>before <code>code text</code> after</div>"
		"<div><br></div>"
		HTML_SUFFIX,
		"before `code text` after\n"
		"\n", 0);
}

static void
test_markdown_convert_pre (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div>text</div>"
		"<pre>pre text start\n"
		" line with one surrounding space \n"
		"  line with two surrounding spaces \n"
		"   indented line with two end spaces  \n"
		"   line with three surrounding spaces   \n"
		"\ttab\n"
		"pre text end</pre>"
		"<div>text 2</div>"
		"<div><br></div>"
		HTML_SUFFIX,
		"text\n"
		"```\n"
		"pre text start\n"
		" line with one surrounding space \n"
		"  line with two surrounding spaces \n"
		"   indented line with two end spaces  \n"
		"   line with three surrounding spaces   \n"
		"\ttab\n"
		"pre text end\n"
		"```\n"
		"text 2\n"
		"\n", 0);
}

static void
test_markdown_convert_ul (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div>text before</div>"
		"<ul>"
		"<li>item 1</li>"
		"<li>item 2</li>"
		"</ul>"
		"<div>text after</div>"
		"<div><br></div>"
		HTML_SUFFIX,
		"text before\n"
		"\n"
		"- item 1\n"
		"- item 2\n"
		"\n"
		"text after\n"
		"\n", 0);
}

static void
test_markdown_convert_ol (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div>text before</div>"
		"<ol>"
		"<li>item 1</li>"
		"<li>item 2</li>"
		"</ol>"
		"<div>text after</div>"
		"<div><br></div>"
		HTML_SUFFIX,
		"text before\n"
		"\n"
		"1. item 1\n"
		"2. item 2\n"
		"\n"
		"text after\n"
		"\n", 0);
}

/* static void
test_markdown_convert_nested_ulol (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div>text before</div>"
		"<ol>"
		"<li>item 1</li>"
		"<li>item 2</li>"
		"<ul>"
		  "<li>item 2.1</li>"
		  "<li>item 2.2</li>"
		  "<ol>"
		    "<li>item 2.2.1</li>"
		    "<ul>"
		      "<li>item 2.2.1.1</li>"
		      "<li>item 2.2.1.2</li>"
		      "<li>item 2.2.1.3</li>"
		    "</ul>"
		    "<li>item 2.2.2</li>"
		    "<li>item 2.2.3</li>"
		  "</ol>"
		  "<li>item 2.3</li>"
		"</ul>"
		"<li>item 3</li>"
		"<li>item 4</li>"
		"</ol>"
		"<div>text after</div>"
		"<div><br></div>"
		HTML_SUFFIX,
		"text before\n"
		"\n"
		"1. item 1\n"
		"2. item 2\n"
		"\n"
		"   - item 2.1\n"
		"   - item 2.2\n"
		"\n"
		"      1. item 2.2.1\n"
		"\n"
		"         - item 2.2.1.1\n"
		"         - item 2.2.1.2\n"
		"         - item 2.2.1.3\n"
		"\n"
		"      2. item 2.2.2\n"
		"      3. item 2.2.3\n"
		"\n"
		"   - item 2.3\n"
		"\n"
		"3. item 3\n"
		"4. item 4\n"
		"\n"
		"text after\n"
		"\n", 0);
} */

static void
test_markdown_convert_blockquote (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div>text before</div>"
		"<blockquote>"
		  "<div>text 1</div>"
		  "<div><br></div>"
		  "<div>text 2</div>"
		"</blockquote>"
		"<div>text after</div>"
		"<div><br></div>"
		HTML_SUFFIX,
		"text before\n"
		"\n"
		"> text 1\n"
		"> \n"
		"> text 2\n"
		"\n"
		"text after\n"
		"\n", 0);
}

static void
test_markdown_convert_nested_blockquote (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div>text before</div>"
		"<blockquote>"
		  "<div>level 1.a<br>level 1.b</div>"
		  "<blockquote>"
		    "<div>level 2.a</div>"
		    "<div>level 2.b</div>"
		    "<blockquote>"
		      "<div>level 3.a</div>"
		      "<div>level 3.b</div>"
		      "<div><br></div>"
		      "<div>level 3.c</div>"
		    "</blockquote>"
		    "<div>level 2.c</div>"
		    "<div>level 2.d</div>"
		    "<blockquote>"
		      "<div>level 3.d</div>"
		      "<div>level 3.e</div>"
		    "</blockquote>"
		  "</blockquote>"
		  "<div>level 1.c</div>"
		"</blockquote>"
		"<div>text after</div>"
		"<div><br></div>"
		HTML_SUFFIX,
		"text before\n"
		"\n"
		"> level 1.a  \n"
		"> level 1.b\n"
		"> \n"
		"> > level 2.a\n"
		"> > level 2.b\n"
		"> > \n"
		"> > > level 3.a\n"
		"> > > level 3.b\n"
		"> > > \n"
		"> > > level 3.c\n"
		"> > \n"
		"> > level 2.c\n"
		"> > level 2.d\n"
		"> > \n"
		"> > > level 3.d\n"
		"> > > level 3.e\n"
		"> > \n"
		"> \n"
		"> level 1.c\n"
		"\n"
		"text after\n"
		"\n", 0);
}

static void
test_markdown_convert_composer_quirks_to_body (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div>paragraph text</div>"
		HTML_SUFFIX
		"<span class='-x-evo-to-body' data-credits='On February 30 Joe Wrote:'></span>",
		"paragraph text\n", 0);

	test_markdown_convert (
		HTML_PREFIX
		"<div>paragraph text</div>"
		HTML_SUFFIX
		"<span class='-x-evo-to-body' data-credits='On February 30 Joe Wrote:'></span>",
		"On February 30 Joe Wrote:  \n"
		"paragraph text\n", E_MARKDOWN_HTML_TO_TEXT_FLAG_COMPOSER_QUIRKS);
}

static void
test_markdown_convert_composer_quirks_cite_body (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div>paragraph text 1</div>"
		"<div><br></div>"
		"<div>paragraph text 2</div>"
		"<div>paragraph text 3</div>"
		HTML_SUFFIX
		"<span class='-x-evo-cite-body'></span>",
		"paragraph text 1\n"
		"\n"
		"paragraph text 2  \n"
		"paragraph text 3\n", 0);

	test_markdown_convert (
		HTML_PREFIX
		"<div>paragraph text 1</div>"
		"<div><br></div>"
		"<div>paragraph text 2</div>"
		"<div>paragraph text 3</div>"
		HTML_SUFFIX
		"<span class='-x-evo-cite-body'></span>",
		"> paragraph text 1\n"
		"> \n"
		"> paragraph text 2  \n"
		"> paragraph text 3\n", E_MARKDOWN_HTML_TO_TEXT_FLAG_COMPOSER_QUIRKS);
}

static void
test_markdown_convert_composer_quirks_cite_body_complex (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div>paragraph <b>bold</b> <i>text</i> 1</div>"
		"<pre>pre-line\n"
		"\n"
		"   indented pre-line\n"
		"last pre-line</pre>"
		"<div>paragraph text 2</div>"
		"<blockquote type='cite'>"
		  "<div>quoted text...</div>"
		  "<div><br></div>"
		  "<div>...is here</div>"
		"</blockquote>"
		"<div>Click <a href='www.gnome.org'>here</a> to visit <a href='www.gnome.org'>the GNOME site</a>.</div>"
		"<div><br></div>"
		HTML_SUFFIX
		"<span class='-x-evo-cite-body'></span>",
		"> paragraph **bold** *text* 1\n"
		"> ```\n"
		"> pre-line\n"
		"> \n"
		">    indented pre-line\n"
		"> last pre-line\n"
		"> ```\n"
		"> paragraph text 2\n"
		"> \n"
		"> > quoted text...\n"
		"> > \n"
		"> > ...is here\n"
		"> \n"
		"> Click [here](www.gnome.org) to visit [the GNOME site](www.gnome.org).\n"
		"> \n", E_MARKDOWN_HTML_TO_TEXT_FLAG_COMPOSER_QUIRKS);
}

static void
test_markdown_convert_composer_quirks_to_body_and_cite_body (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div>paragraph text 1</div>"
		"<div><br></div>"
		"<div>paragraph text 2</div>"
		"<div>paragraph text 3</div>"
		HTML_SUFFIX
		"<span class='-x-evo-cite-body'></span>"
		"<span class='-x-evo-to-body' data-credits='On February 30 Joe Wrote:'></span>",
		"On February 30 Joe Wrote:\n"
		"> paragraph text 1\n"
		"> \n"
		"> paragraph text 2  \n"
		"> paragraph text 3\n", E_MARKDOWN_HTML_TO_TEXT_FLAG_COMPOSER_QUIRKS);
}

static void
test_markdown_convert_to_plain_text (void)
{
	const gchar *html =
		HTML_PREFIX
		"<div>paragraph <b>bold</b> <i>text</i> 1</div>"
		"<pre>pre-line\n"
		"\n"
		"   indented pre-line\n"
		"last pre-line</pre>"
		"<div>paragraph text 2</div>"
		"<blockquote type='cite'>"
		  "<div>quoted text...</div>"
		  "<div><br></div>"
		  "<div>...is here</div>"
		"</blockquote>"
		"<div>Click <a href='www.gnome.org'>here</a> to visit <a href='www.gnome.org'>the GNOME site</a>.</div>"
		"<div><br></div>"
		HTML_SUFFIX;

	test_markdown_convert (
		html,
		"paragraph **bold** *text* 1\n"
		"```\n"
		"pre-line\n"
		"\n"
		"   indented pre-line\n"
		"last pre-line\n"
		"```\n"
		"paragraph text 2\n"
		"\n"
		"> quoted text...\n"
		"> \n"
		"> ...is here\n"
		"\n"
		"Click [here](www.gnome.org) to visit [the GNOME site](www.gnome.org).\n"
		"\n", 0);

	test_markdown_convert (
		html,
		"paragraph bold text 1\n"
		"pre-line\n"
		"\n"
		"   indented pre-line\n"
		"last pre-line\n"
		"paragraph text 2\n"
		"\n"
		"> quoted text...\n"
		"> \n"
		"> ...is here\n"
		"\n"
		"Click here to visit the GNOME site.\n"
		"\n", E_MARKDOWN_HTML_TO_TEXT_FLAG_PLAIN_TEXT);
}

static void
test_markdown_convert_to_plain_text_links_none (void)
{
	#define HTML(_body) HTML_PREFIX _body HTML_SUFFIX

	struct _tests {
		const gchar *html;
		const gchar *plain;
	} tests[] = {
	/* 0 */ { HTML ("<div>before <a href='https://gnome.org'>https://gnome.org</a> after</div>"),
		"before https://gnome.org after\n"},
	/* 1 */ { HTML ("<div>before <a href='https://gnome.org'>GNOME</a> after</div>"),
		"before GNOME after\n" },
	/* 2 */ { HTML ("<div>b1 <a href='https://gnome.org/a'>GNOME-a</a> a1</div>"
		"<div>b2 <a href='https://gnome.org/'>https://gnome.org/</a> a2</div>"
		"<div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div>"
		"<div>b4 <a href='https://gnome.org/c'>GNOME-c</a> a4</div>"),
		"b1 GNOME-a a1\n"
		"b2 https://gnome.org/ a2\n"
		"b3 GNOME-b a3\n"
		"b4 GNOME-c a4\n" },
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
		"b10 GNOME-i a10\n" },
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
		"b11 GNOME-j a11\n" },
	/* 5 */ { HTML ("<ul><li>first line</li>"
		"<li>b2 <a href='https://gnome.org/'>GNOME</a> a2</li>"
		"<li><div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div></li>"
		"<li>last line</li></ul>"),
		"\n"
		"- first line\n"
		"- b2 GNOME a2\n"
		"- b3 GNOME-b a3\n\n"
		"- last line\n"
		"\n" },
	/* 6 */ { HTML ("<ol><li>first line</li>"
		"<li>b2 <a href='https://gnome.org/'>GNOME</a> a2</li>"
		"<li><div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div></li>"
		"<li>last line</li></ol>"),
		"\n"
		"1. first line\n"
		"2. b2 GNOME a2\n"
		"3. b3 GNOME-b a3\n\n"
		"4. last line\n"
		"\n" },
	/* 7 */ { HTML ("<div>before <a href='gnome.org'>GNOME</a> after</div>"
		"<div>before <a href='mailto:user@no.where'>Mail the User</a> after</div>"
		"<div>before <a href='https://gnome.org'>https GNOME</a> after</div>"
		"<div>before <a href='http://gnome.org'>http GNOME</a> after</div>"),
		"before GNOME after\n"
		"before Mail the User after\n"
		"before https GNOME after\n"
		"before http GNOME after\n" },
	/* 8 */ { HTML ("<div>before <a href='https://gnome.org/#%C3%A4bc'>https://gnome.org/#äbc</a> after</div>"),
		"before https://gnome.org/#äbc after\n" }
	};

	#undef HTML

	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (tests); ii++) {
		test_markdown_convert (tests[ii].html, tests[ii].plain, E_MARKDOWN_HTML_TO_TEXT_FLAG_PLAIN_TEXT);
	}
}

static void
test_markdown_convert_to_plain_text_links_inline (void)
{
	#define HTML(_body) HTML_PREFIX _body HTML_SUFFIX

	struct _tests {
		const gchar *html;
		const gchar *plain;
	} tests[] = {
	/* 0 */ { HTML ("<div>before <a href='https://gnome.org'>https://gnome.org</a> after</div>"),
		"before https://gnome.org after\n" },
	/* 1 */ { HTML ("<div>before <a href='https://gnome.org'>GNOME</a> after</div>"),
		"before GNOME <https://gnome.org> after\n" },
	/* 2 */ { HTML ("<div>b1 <a href='https://gnome.org/a'>GNOME-a</a> a1</div>"
		"<div>b2 <a href='https://gnome.org/'>https://gnome.org/</a> a2</div>"
		"<div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div>"
		"<div>b4 <a href='https://gnome.org/c'>GNOME-c</a> a4</div>"),
		"b1 GNOME-a <https://gnome.org/a> a1\n"
		"b2 https://gnome.org/ a2\n"
		"b3 GNOME-b <https://gnome.org/b> a3\n"
		"b4 GNOME-c <https://gnome.org/c> a4\n" },
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
		"b10 GNOME-i <https://gnome.org/i> a10\n" },
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
		"b11 GNOME-j <https://gnome.org/j> a11\n" },
	/* 5 */ { HTML ("<ul><li>first line</li>"
		"<li>b2 <a href='https://gnome.org/'>GNOME</a> a2</li>"
		"<li><div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div></li>"
		"<li>last line</li></ul>"),
		"\n"
		"- first line\n"
		"- b2 GNOME <https://gnome.org/> a2\n"
		"- b3 GNOME-b <https://gnome.org/b> a3\n\n"
		"- last line\n"
		"\n" },
	/* 6 */ { HTML ("<ol><li>first line</li>"
		"<li>b2 <a href='https://gnome.org/'>GNOME</a> a2</li>"
		"<li><div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div></li>"
		"<li>last line</li></ol>"),
		"\n"
		"1. first line\n"
		"2. b2 GNOME <https://gnome.org/> a2\n"
		"3. b3 GNOME-b <https://gnome.org/b> a3\n\n"
		"4. last line\n"
		"\n" },
	/* 7 */ { HTML ("<div>before <a href='http://gnome.org/'>GNOME</a> after</div>"
		"<div>before <a href='mailto:user@no.where'>Mail the User</a> after</div>"
		"<div>before <a href='https://gnome.org'>https GNOME</a> after</div>"
		"<div>before <a href='http://gnome.org/'>http GNOME</a> after</div>"),
		"before GNOME <http://gnome.org/> after\n"
		"before Mail the User after\n"
		"before https GNOME <https://gnome.org> after\n"
		"before http GNOME <http://gnome.org/> after\n" },
	/* 8 */ { HTML ("<div>before <a href='https://gnome.org/#%C3%A4bc'>https://gnome.org/#äbc</a> after</div>"),
		"before https://gnome.org/#äbc after\n" }
	};

	#undef HTML

	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (tests); ii++) {
		test_markdown_convert (tests[ii].html, tests[ii].plain, E_MARKDOWN_HTML_TO_TEXT_FLAG_PLAIN_TEXT | E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_INLINE);
	}
}

static void
test_markdown_convert_to_plain_text_links_reference (void)
{
	#define HTML(_body) HTML_PREFIX _body HTML_SUFFIX

	struct _tests {
		const gchar *html;
		const gchar *plain;
	} tests[] = {
	/* 0 */ { HTML ("<div>before <a href='https://gnome.org'>https://gnome.org</a> after</div>"),
		"before https://gnome.org after\n" },
	/* 1 */ { HTML ("<div>before <a href='https://gnome.org'>GNOME</a> after</div>"),
		"before GNOME [1] after\n"
		"\n"
		"[1] GNOME https://gnome.org\n" },
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
		"[3] GNOME-c https://gnome.org/c\n" },
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
		"[9] GNOME-i https://gnome.org/i\n" },
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
		"[1] GNOME-a next line text https://gnome.org/a\n"
		"[2] GNOME-b https://gnome.org/b\n"
		"[3] GNOME-c https://gnome.org/c\n"
		"[4] GNOME-d https://gnome.org/d\n"
		"[5] GNOME-e https://gnome.org/e\n"
		"[6] GNOME-f https://gnome.org/f\n"
		"[7] GNOME-g https://gnome.org/g\n"
		"[8] GNOME-h https://gnome.org/h\n"
		"[9] GNOME-i https://gnome.org/i\n"
		"[10] GNOME-j long text here https://gnome.org/j\n" },
	/* 5 */ { HTML ("<ul><li>first line</li>"
		"<li>b2 <a href='https://gnome.org/'>GNOME</a> a2</li>"
		"<li><div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div></li>"
		"<li>last line</li></ul>"),
		"\n"
		"- first line\n"
		"- b2 GNOME [1] a2\n"
		"- b3 GNOME-b [2] a3\n\n"
		"- last line\n"
		"\n"
		"\n"
		"[1] GNOME https://gnome.org/\n"
		"[2] GNOME-b https://gnome.org/b\n" },
	/* 6 */ { HTML ("<ol><li>first line</li>"
		"<li>b2 <a href='https://gnome.org/'>GNOME</a> a2</li>"
		"<li><div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div></li>"
		"<li>last line</li></ol>"),
		"\n"
		"1. first line\n"
		"2. b2 GNOME [1] a2\n"
		"3. b3 GNOME-b [2] a3\n\n"
		"4. last line\n"
		"\n"
		"\n"
		"[1] GNOME https://gnome.org/\n"
		"[2] GNOME-b https://gnome.org/b\n" },
	/* 7 */ { HTML ("<div>before <a href='http://gnome.org/'>GNOME</a> after</div>"
		"<div>before <a href='mailto:user@no.where'>Mail the User</a> after</div>"
		"<div>before <a href='https://gnome.org'>https GNOME</a> after</div>"
		"<div>before <a href='http://gnome.org/'>http GNOME</a> after</div>"),
		"before GNOME [1] after\n"
		"before Mail the User after\n"
		"before https GNOME [2] after\n"
		"before http GNOME [1] after\n"
		"\n"
		"[1] GNOME http://gnome.org/\n"
		"[2] https GNOME https://gnome.org\n" },
	/* 8 */ { HTML ("<div>before <a href='https://gnome.org/#%C3%A4bc'>https://gnome.org/#äbc</a> after</div>"),
		"before https://gnome.org/#äbc after\n" }
	};

	#undef HTML

	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (tests); ii++) {
		test_markdown_convert (tests[ii].html, tests[ii].plain, E_MARKDOWN_HTML_TO_TEXT_FLAG_PLAIN_TEXT | E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_REFERENCE);
	}
}

static void
test_markdown_convert_to_plain_text_links_reference_without_label (void)
{
	#define HTML(_body) HTML_PREFIX _body HTML_SUFFIX

	struct _tests {
		const gchar *html;
		const gchar *plain;
	} tests[] = {
	/* 0 */ { HTML ("<div>before <a href='https://gnome.org'>https://gnome.org</a> after</div>"),
		"before https://gnome.org after\n" },
	/* 1 */ { HTML ("<div>before <a href='https://gnome.org'>GNOME</a> after</div>"),
		"before GNOME [1] after\n"
		"\n"
		"[1] https://gnome.org\n" },
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
		"[3] https://gnome.org/c\n" },
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
		"[9] https://gnome.org/i\n" },
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
		"[10] https://gnome.org/j\n" },
	/* 5 */ { HTML ("<ul><li>first line</li>"
		"<li>b2 <a href='https://gnome.org/'>GNOME</a> a2</li>"
		"<li><div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div></li>"
		"<li>last line</li></ul>"),
		"\n"
		"- first line\n"
		"- b2 GNOME [1] a2\n"
		"- b3 GNOME-b [2] a3\n\n"
		"- last line\n"
		"\n"
		"\n"
		"[1] https://gnome.org/\n"
		"[2] https://gnome.org/b\n" },
	/* 6 */ { HTML ("<ol><li>first line</li>"
		"<li>b2 <a href='https://gnome.org/'>GNOME</a> a2</li>"
		"<li><div>b3 <a href='https://gnome.org/b'>GNOME-b</a> a3</div></li>"
		"<li>last line</li></ol>"),
		"\n"
		"1. first line\n"
		"2. b2 GNOME [1] a2\n"
		"3. b3 GNOME-b [2] a3\n\n"
		"4. last line\n"
		"\n"
		"\n"
		"[1] https://gnome.org/\n"
		"[2] https://gnome.org/b\n" },
	/* 7 */ { HTML ("<div>before <a href='http://gnome.org/'>GNOME</a> after</div>"
		"<div>before <a href='mailto:user@no.where'>Mail the User</a> after</div>"
		"<div>before <a href='https://gnome.org'>https GNOME</a> after</div>"
		"<div>before <a href='http://gnome.org/'>http GNOME</a> after</div>"),
		"before GNOME [1] after\n"
		"before Mail the User after\n"
		"before https GNOME [2] after\n"
		"before http GNOME [1] after\n"
		"\n"
		"[1] http://gnome.org/\n"
		"[2] https://gnome.org\n" },
	/* 8 */ { HTML ("<div>before <a href='https://gnome.org/#%C3%A4bc'>https://gnome.org/#äbc</a> after</div>"),
		"before https://gnome.org/#äbc after\n" }
	};

	#undef HTML

	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (tests); ii++) {
		test_markdown_convert (tests[ii].html, tests[ii].plain, E_MARKDOWN_HTML_TO_TEXT_FLAG_PLAIN_TEXT | E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_REFERENCE_WITHOUT_LABEL);
	}
}

static void
test_markdown_convert_signature (void)
{
	test_markdown_convert (
		HTML_PREFIX
		"<div style=\"width: 71ch;\">text</div>"
		"<div style=\"width: 71ch;\"><br></div>"
		"<div style=\"width: 71ch;\"><span>"
		"<pre>-- <br></pre>"
		"<pre>User &lt;user@no.where&gt;</pre>"
		"</span></div>"
		HTML_SUFFIX,
		"text\n"
		"\n"
		"```\n"
		"-- \n"
		"User <user@no.where>\n"
		"```\n"
		"\n", E_MARKDOWN_HTML_TO_TEXT_FLAG_COMPOSER_QUIRKS | E_MARKDOWN_HTML_TO_TEXT_FLAG_SIGNIFICANT_NL);
}

typedef void (* ETestFixtureFunc) (TestFixture *fixture, gconstpointer user_data);

gint
main (gint argc,
      gchar *argv[])
{
	gboolean keep_going = FALSE;
	GOptionEntry entries[] = {
		/* Cannot use --keep-going, it's taken by glib */
		{ "e-keep-going", '\0', 0,
		  G_OPTION_ARG_NONE, &keep_going,
		  "Use to not abort failed tests, but keep going through all tests.",
		  NULL },
		{ NULL }
	};
	GOptionContext *context;
	GError *error = NULL;
	gint res;

	setlocale (LC_ALL, "");

	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("https://gitlab.gnome.org/GNOME/evolution/issues/");

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_warning ("Failed to parse arguments: %s\n", error ? error->message : "Unknown error");
		g_option_context_free (context);
		g_clear_error (&error);
		return -1;
	}
	g_option_context_free (context);

	if (keep_going)
		g_test_set_nonfatal_assertions ();

	e_util_init_main_thread (NULL);

	#define add_test(_nm, _func) \
		g_test_add (_nm, TestFixture, NULL, \
			test_fixture_setup, (ETestFixtureFunc) _func, test_fixture_tear_down);

	add_test ("/MarkdownConvert/SingleLineWithBR", test_markdown_convert_single_line_with_br);
	add_test ("/MarkdownConvert/SingleLineWithoutBR", test_markdown_convert_single_line_without_br);
	add_test ("/MarkdownConvert/TwoLinesWithBR", test_markdown_convert_two_lines_with_br);
	add_test ("/MarkdownConvert/TwoLinesWithoutBR", test_markdown_convert_two_lines_without_br);
	add_test ("/MarkdownConvert/SoftLineBreak", test_markdown_convert_soft_line_break);
	add_test ("/MarkdownConvert/BRLineBreak", test_markdown_convert_br_line_break);
	add_test ("/MarkdownConvert/BRNLLineBreak", test_markdown_convert_brnl_line_break);
	add_test ("/MarkdownConvert/NLBRLineBreak", test_markdown_convert_nlbr_line_break);
	add_test ("/MarkdownConvert/ConsecutiveParagraphs", test_markdown_convert_consecutive_paragraphs);
	add_test ("/MarkdownConvert/TrailingSpaces", test_markdown_convert_trailing_spaces);
	add_test ("/MarkdownConvert/ComplexPlain", test_markdown_convert_complex_plain);
	add_test ("/MarkdownConvert/H1", test_markdown_convert_h1);
	add_test ("/MarkdownConvert/H2", test_markdown_convert_h2);
	add_test ("/MarkdownConvert/H3", test_markdown_convert_h3);
	add_test ("/MarkdownConvert/H4", test_markdown_convert_h4);
	add_test ("/MarkdownConvert/H5", test_markdown_convert_h5);
	add_test ("/MarkdownConvert/H6", test_markdown_convert_h6);
	add_test ("/MarkdownConvert/Anchor", test_markdown_convert_anchor);
	add_test ("/MarkdownConvert/Bold", test_markdown_convert_bold);
	add_test ("/MarkdownConvert/Italic", test_markdown_convert_italic);
	add_test ("/MarkdownConvert/Code", test_markdown_convert_code);
	add_test ("/MarkdownConvert/Pre", test_markdown_convert_pre);
	add_test ("/MarkdownConvert/Ul", test_markdown_convert_ul);
	add_test ("/MarkdownConvert/Ol", test_markdown_convert_ol);
	/* libxml2 2.9.14 reports end of ul/ol too early when they are nested;
	   see https://gitlab.gnome.org/GNOME/libxml2/-/issues/447
	add_test ("/MarkdownConvert/NestedUlOl", test_markdown_convert_nested_ulol); */
	add_test ("/MarkdownConvert/Blockquote", test_markdown_convert_blockquote);
	add_test ("/MarkdownConvert/NestedBlockquote", test_markdown_convert_nested_blockquote);
	add_test ("/MarkdownConvert/ComposerQuirksToBody", test_markdown_convert_composer_quirks_to_body);
	add_test ("/MarkdownConvert/ComposerQuirksCiteBody", test_markdown_convert_composer_quirks_cite_body);
	add_test ("/MarkdownConvert/ComposerQuirksCiteBodyComplex", test_markdown_convert_composer_quirks_cite_body_complex);
	add_test ("/MarkdownConvert/ComposerQuirksToBodyAndCiteBody", test_markdown_convert_composer_quirks_to_body_and_cite_body);
	add_test ("/MarkdownConvert/ToPlainText", test_markdown_convert_to_plain_text);
	add_test ("/MarkdownConvert/ToPlainTextLinksNone", test_markdown_convert_to_plain_text_links_none);
	add_test ("/MarkdownConvert/ToPlainTextLinksInline", test_markdown_convert_to_plain_text_links_inline);
	add_test ("/MarkdownConvert/ToPlainTextLinksReference", test_markdown_convert_to_plain_text_links_reference);
	add_test ("/MarkdownConvert/ToPlainTextLinksReferenceWithoutLabel", test_markdown_convert_to_plain_text_links_reference_without_label);
	add_test ("/MarkdownConvert/Signature", test_markdown_convert_signature);

	#undef add_test

	res = g_test_run ();

	e_misc_util_free_global_memory ();

	return res;
}
