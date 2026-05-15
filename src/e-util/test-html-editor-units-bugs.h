/*
 * SPDX-FileCopyrightText: (C) 2016 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef TEST_HTML_EDITOR_UNITS_BUGS_H
#define TEST_HTML_EDITOR_UNITS_BUGS_H

#define HTML_PREFIX "<html><head></head><body>"
#define HTML_SUFFIX "</body></html>"
#define BLOCKQUOTE_STYLE "style=\"margin:0 0 0 .8ex; border-left:2px #729fcf solid;padding-left:1ex\""
#define QUOTE_SPAN(x) "<span class='-x-evo-quoted'>" x "</span>"
#define QUOTE_CHR "<span class='-x-evo-quote-character'>&gt; </span>"
#define WRAP_BR "<br class=\"-x-evo-wrap-br\">"
#define WRAP_BR_SPC "<br class=\"-x-evo-wrap-br\" x-evo-is-space=\"1\">"

G_BEGIN_DECLS

void test_add_html_editor_bug_tests (void);

G_END_DECLS

#endif /* TEST_HTML_EDITOR_UNITS_BUGS_H */
