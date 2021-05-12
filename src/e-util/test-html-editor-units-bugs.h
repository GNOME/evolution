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
