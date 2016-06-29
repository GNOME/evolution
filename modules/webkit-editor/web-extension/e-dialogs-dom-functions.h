/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#ifndef E_DIALOGS_DOM_FUNCTIONS_H
#define E_DIALOGS_DOM_FUNCTIONS_H

#include <webkit2/webkit-web-extension.h>

#include "e-editor-page.h"

G_BEGIN_DECLS

/* ******************** Cell Dialog ***************** */

void		e_dialogs_dom_cell_mark_current_cell_element
						(EEditorPage *editor_page,
						 const gchar *id);
void		e_dialogs_dom_cell_save_history_on_exit
						(EEditorPage *editor_page);
void		e_dialogs_dom_cell_set_element_v_align
						(EEditorPage *editor_page,
						 const gchar *v_align,
						 guint scope);
void		e_dialogs_dom_cell_set_element_align
						(EEditorPage *editor_page,
						 const gchar *align,
						 guint scope);
void		e_dialogs_dom_cell_set_element_no_wrap
						(EEditorPage *editor_page,
						 gboolean wrap_text,
						 guint scope);
void		e_dialogs_dom_cell_set_element_header_style
						(EEditorPage *editor_page,
						 gboolean header_style,
						 guint scope);
void		e_dialogs_dom_cell_set_element_width
						(EEditorPage *editor_page,
						 const gchar *width,
						 guint scope);
void		e_dialogs_dom_cell_set_element_col_span
						(EEditorPage *editor_page,
						 glong span,
						 guint scope);
void		e_dialogs_dom_cell_set_element_row_span
						(EEditorPage *editor_page,
						 glong span,
						 guint scope);
void		e_dialogs_dom_cell_set_element_bg_color
						(EEditorPage *editor_page,
						 const gchar *color,
						 guint scope);

/* ******************** HRule Dialog ***************** */

gboolean	e_dialogs_dom_hrule_find_hrule	(EEditorPage *editor_page,
						 WebKitDOMNode *node_under_mouse_click);
void		e_dialogs_dom_save_history_on_exit
						(EEditorPage *editor_page);

/* ******************** Image Dialog ***************** */

void		e_dialogs_dom_image_mark_image	(EEditorPage *editor_page,
						 WebKitDOMNode *node_under_mouse_click);
void		e_dialogs_dom_image_save_history_on_exit
						(EEditorPage *editor_page);
void		e_dialogs_dom_image_set_element_url
						(EEditorPage *editor_page,
						 const gchar *url);
gchar *		e_dialogs_dom_image_get_element_url
						(EEditorPage *editor_page);

/* ******************** Link Dialog ***************** */

void		e_dialogs_dom_link_commit	(EEditorPage *editor_page,
						 const gchar *url,
						 const gchar *inner_text);
GVariant *	e_dialogs_dom_link_show		(EEditorPage *editor_page);

/* ******************** Page Dialog ***************** */

void		e_dialogs_dom_page_save_history	(EEditorPage *editor_page);
void		e_dialogs_dom_page_save_history_on_exit
						(EEditorPage *editor_page);

/* ******************** Spell Check Dialog ***************** */

gchar * 	e_dialogs_dom_spell_check_prev	(EEditorPage *editor_page,
						 const gchar *from_word,
						 const gchar * const *languages);

gchar * 	e_dialogs_dom_spell_check_next	(EEditorPage *editor_page,
						 const gchar *from_word,
						 const gchar * const *languages);

/* ******************** Table Dialog ***************** */

void		e_dialogs_dom_table_set_row_count
						(EEditorPage *editor_page,
						 gulong expected_count);

gulong		e_dialogs_dom_table_get_row_count
						(EEditorPage *editor_page);

void		e_dialogs_dom_table_set_column_count
						(EEditorPage *editor_page,
						 gulong expected_columns);

gulong		e_dialogs_dom_table_get_column_count
						(EEditorPage *editor_page);

gboolean	e_dialogs_dom_table_show	(EEditorPage *editor_page);

void		e_dialogs_dom_table_save_history_on_exit
						(EEditorPage *editor_page);

G_END_DECLS

#endif /* E_DIALOGS_DOM_FUNCTIONS_H */
