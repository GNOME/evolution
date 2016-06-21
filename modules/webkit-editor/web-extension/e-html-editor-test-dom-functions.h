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

#ifndef E_HTML_EDITOR_TEST_DOM_FUNCTIONS_H
#define E_HTML_EDITOR_TEST_DOM_FUNCTIONS_H

#include <webkitdom/webkitdom.h>

gboolean	dom_test_html_equal		(WebKitDOMDocument *document,
						 const gchar *html1,
						 const gchar *html2);

#endif /* E_HTML_EDITOR_TEST_DOM_FUNCTIONS_H */
