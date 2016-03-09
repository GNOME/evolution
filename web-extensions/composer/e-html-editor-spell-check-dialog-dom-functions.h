/*
 * e-html-editor-spell-check-dialog-dom-functions.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef E_HTML_SPELL_CHECK_DIALOG_DOM_FUNCTIONS_H
#define E_HTML_SPELL_CHECK_DIALOG_DOM_FUNCTIONS_H

#include <webkitdom/webkitdom.h>

#include "e-html-editor-web-extension.h"

G_BEGIN_DECLS

gchar * 	e_html_editor_spell_check_dialog_prev	(WebKitDOMDocument *document,
							 EHTMLEditorWebExtension *extension,
							 const gchar *from_word,
							 const gchar * const *languages);

gchar * 	e_html_editor_spell_check_dialog_next	(WebKitDOMDocument *document,
							 EHTMLEditorWebExtension *extension,
							 const gchar *from_word,
							 const gchar * const *languages);

G_END_DECLS

#endif /* E_HTML_EDITOR_SPELL_CHECK_DIALOG_DOM_FUNCTIONS_H */
