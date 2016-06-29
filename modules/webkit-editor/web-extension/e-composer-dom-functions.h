/*
 * e-composer-dom-functions.h
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

#ifndef E_COMPOSER_DOM_FUNCTIONS_H
#define E_COMPOSER_DOM_FUNCTIONS_H

#include <webkitdom/webkitdom.h>

#include "e-editor-page.h"

G_BEGIN_DECLS

gchar *		e_composer_dom_remove_signatures
						(EEditorPage *editor_page,
						 gboolean top_signature);
gchar *		e_composer_dom_insert_signature	(EEditorPage *editor_page,
						 const gchar *content,
						 gboolean is_html,
						 const gchar *id,
						 gboolean *set_signature_from_message,
						 gboolean *check_if_signature_is_changed,
						 gboolean *ignore_next_signature_change);
gchar *		e_composer_dom_get_active_signature_uid
						(EEditorPage *editor_page);
gchar *		e_composer_dom_get_raw_body_content_without_signature
						(EEditorPage *editor_page);
gchar *		e_composer_dom_get_raw_body_content
						(EEditorPage *editor_page);
void		e_composer_dom_save_drag_and_drop_history
						(EEditorPage *editor_page);
void		e_composer_dom_clean_after_drag_and_drop
						(EEditorPage *editor_page);

G_END_DECLS

#endif /* E_COMPOSER_DOM_FUNCTIONS_H */
