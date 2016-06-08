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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CONTENT_EDITOR_H
#define E_CONTENT_EDITOR_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include <camel/camel.h>

#include <e-util/e-content-editor-enums.h>
#include <e-util/e-emoticon.h>
#include <e-util/e-spell-checker.h>

#define DEFAULT_CONTENT_EDITOR_NAME "WebKit"

G_BEGIN_DECLS

#define E_TYPE_CONTENT_EDITOR e_content_editor_get_type ()
G_DECLARE_INTERFACE (EContentEditor, e_content_editor, E, CONTENT_EDITOR, GtkWidget)

typedef struct {
	const gchar *from_domain;
	GList *images;
} EContentEditorInlineImages;

struct _EContentEditorInterface {
	GTypeInterface parent_interface;

	void		(*insert_content)		(EContentEditor *editor,
							 const gchar *content,
							 EContentEditorInsertContentFlags flags);

	gchar *		(*get_content)			(EContentEditor *editor,
							 EContentEditorGetContentFlags flags,
							 EContentEditorInlineImages **inline_images);

	void		(*insert_image)			(EContentEditor *editor,
							 const gchar *uri);

	void            (*insert_image_from_mime_part)
							(EContentEditor *editor,
							 CamelMimePart *part);

	void		(*insert_emoticon)		(EContentEditor *editor,
							 EEmoticon *emoticon);

	void		(*set_current_content_flags)	(EContentEditor *editor,
							 EContentEditorContentFlags flags);

	EContentEditorContentFlags
			(*get_current_content_flags)	(EContentEditor *editor);

	void		(*move_caret_on_coordinates)	(EContentEditor *editor,
							 gint x,
							 gint y,
							 gboolean cancel_if_not_collapsed);

	void		(*set_changed)			(EContentEditor *editor,
							 gboolean changed);

	gboolean	(*get_changed)			(EContentEditor *editor);

	void		(*cut)				(EContentEditor *editor);

	void		(*copy)				(EContentEditor *editor);

	void		(*paste)			(EContentEditor *editor);

	gboolean	(*paste_prefer_text_html)	(EContentEditor *editor);

	void		(*reconnect_paste_clipboard_signals)
							(EContentEditor *editor);

	gboolean	(*can_undo)			(EContentEditor *editor);

	void		(*undo)				(EContentEditor *editor);

	gboolean	(*can_redo)			(EContentEditor *editor);

	void		(*redo)				(EContentEditor *editor);

	void		(*clear_undo_redo_history)	(EContentEditor *editor);

	void		(*set_html_mode)		(EContentEditor *editor,
							 gboolean html_mode);

	gboolean	(*get_html_mode)		(EContentEditor *editor);

	ESpellChecker *	(*get_spell_checker)		(EContentEditor *editor);

	void		(*set_spell_checking_languages)	(EContentEditor *editor,
							 const gchar **languages);

	void		(*set_spell_check)		(EContentEditor *editor,
							 gboolean enable);

	gboolean	(*get_spell_check)		(EContentEditor *editor);

	gchar *		(*selection_get_text)		(EContentEditor *editor);

	gchar *		(*get_caret_word)		(EContentEditor *editor);

	void		(*replace_caret_word)		(EContentEditor *editor,
							 const gchar *replacement);

	void		(*select_all)			(EContentEditor *editor);

	gboolean	(*selection_is_indented)	(EContentEditor *editor);

	void		(*selection_indent)		(EContentEditor *editor);

	void		(*selection_unindent)		(EContentEditor *editor);

	void		(*selection_create_link)	(EContentEditor *editor,
							 const gchar *uri);

	void		(*selection_unlink)		(EContentEditor *editor);

	void		(*find)				(EContentEditor *editor,
							 guint32 flags,
							 const gchar *text);

	void		(*selection_replace)		(EContentEditor *editor,
							 const gchar *replacement);

	void		(*replace_all)			(EContentEditor *editor,
							 guint32 flags,
							 const gchar *find_text,
							 const gchar *replace_with);

	void		(*selection_save)		(EContentEditor *editor);

	void		(*selection_restore)		(EContentEditor *editor);

	void		(*selection_wrap)		(EContentEditor *editor);

	void		(*show_inspector)		(EContentEditor *editor);

	guint		(*get_caret_position)		(EContentEditor *editor);

	guint		(*get_caret_offset)		(EContentEditor *editor);

	void		(*update_fonts)			(EContentEditor *editor);

	gboolean	(*is_editable)			(EContentEditor *editor);

	void		(*set_editable)			(EContentEditor *editor,
							 gboolean editable);

	gchar *		(*get_current_signature_uid)	(EContentEditor *editor);

	gboolean	(*is_ready)			(EContentEditor *editor);

	char *		(*insert_signature)		(EContentEditor *editor,
							 const gchar *content,
							 gboolean is_html,
							 const gchar *signature_id,
							 gboolean *set_signature_from_message,
							 gboolean *check_if_signature_is_changed,
							 gboolean *ignore_next_signature_change);

	void		(*set_alignment)		(EContentEditor *editor,
							 EContentEditorAlignment value);

	EContentEditorAlignment
			(*get_alignment)		(EContentEditor *editor);

	void		(*set_block_format)		(EContentEditor *editor,
							 EContentEditorBlockFormat value);

	EContentEditorBlockFormat
			(*get_block_format)		(EContentEditor *editor);

	void		(*set_background_color)		(EContentEditor *editor,
							 const GdkRGBA *value);

	const GdkRGBA *	(*get_background_color)		(EContentEditor *editor);

	void		(*set_font_name)		(EContentEditor *editor,
							 const gchar *value);

	const gchar *	(*get_font_name)		(EContentEditor *editor);

	void		(*set_font_color)		(EContentEditor *editor,
							 const GdkRGBA *value);

	const GdkRGBA *	(*get_font_color)		(EContentEditor *editor);

	void		(*set_font_size)		(EContentEditor *editor,
							 guint value);

	guint		(*get_font_size)		(EContentEditor *editor);

	void		(*set_bold)			(EContentEditor *editor,
							 gboolean bold);

	gboolean	(*is_bold)			(EContentEditor *editor);

	void		(*set_italic)			(EContentEditor *editor,
							 gboolean italic);

	gboolean	(*is_italic)			(EContentEditor *editor);

	void		(*set_monospaced)		(EContentEditor *editor,
							 gboolean monospaced);

	gboolean	(*is_monospaced)		(EContentEditor *editor);

	void		(*set_strikethrough)		(EContentEditor *editor,
							 gboolean strikethrough);

	gboolean	(*is_strikethrough)		(EContentEditor *editor);

	void		(*set_subscript)		(EContentEditor *editor,
							 gboolean subscript);

	gboolean	(*is_subscript)			(EContentEditor *editor);

	void		(*set_superscript)		(EContentEditor *editor,
							 gboolean superscript);

	gboolean	(*is_superscript)		(EContentEditor *editor);

	void		(*set_underline)		(EContentEditor *editor,
							 gboolean underline);

	gboolean	(*is_underline)			(EContentEditor *editor);

	void		(*delete_cell_contents)		(EContentEditor *editor);

	void		(*delete_column)		(EContentEditor *editor);

	void		(*delete_row)			(EContentEditor *editor);

	void		(*delete_table)			(EContentEditor *editor);

	void		(*insert_column_after)		(EContentEditor *editor);

	void		(*insert_column_before)		(EContentEditor *editor);

	void		(*insert_row_above)		(EContentEditor *editor);

	void		(*insert_row_below)		(EContentEditor *editor);

	gboolean	(*on_h_rule_dialog_open)	(EContentEditor *editor);

	void		(*on_h_rule_dialog_close)	(EContentEditor *editor);

	void		(*h_rule_set_align)		(EContentEditor *editor,
							 const gchar *value);

	gchar *		(*h_rule_get_align)		(EContentEditor *editor);

	void		(*h_rule_set_size)		(EContentEditor *editor,
							 gint value);

	gint		(*h_rule_get_size)		(EContentEditor *editor);

	void		(*h_rule_set_width)		(EContentEditor *editor,
							 gint value,
							 EContentEditorUnit unit);

	gint		(*h_rule_get_width)		(EContentEditor *editor,
							 EContentEditorUnit *unit);

	void		(*h_rule_set_no_shade)		(EContentEditor *editor,
							 gboolean value);

	gboolean	(*h_rule_get_no_shade)		(EContentEditor *editor);

	void		(*on_image_dialog_open)		(EContentEditor *editor);

	void		(*on_image_dialog_close)	(EContentEditor *editor);

	void		(*image_set_src)		(EContentEditor *editor,
							 const gchar *value);

	gchar *		(*image_get_src)		(EContentEditor *editor);

	void		(*image_set_alt)		(EContentEditor *editor,
							 const gchar *value);

	gchar *		(*image_get_alt)		(EContentEditor *editor);

	gint32		(*image_get_natural_width)	(EContentEditor *editor);

	gint32		(*image_get_width)		(EContentEditor *editor);

	void		(*image_set_width)		(EContentEditor *editor,
							 gint value);

	void		(*image_set_width_follow)	(EContentEditor *editor,
							 gboolean value);

	gint32		(*image_get_natural_height)	(EContentEditor *editor);

	gint32		(*image_get_height)		(EContentEditor *editor);

	void		(*image_set_height)		(EContentEditor *editor,
							 gint value);

	void		(*image_set_height_follow)	(EContentEditor *editor,
							 gboolean value);

	void		(*image_set_url)		(EContentEditor *editor,
							 const gchar *value);

	gchar *		(*image_get_url)		(EContentEditor *editor);

	void		(*image_set_vspace)		(EContentEditor *editor,
							 gint value);

	gint		(*image_get_vspace)		(EContentEditor *editor);

	void		(*image_set_hspace)		(EContentEditor *editor,
							 gint value);

	gint		(*image_get_hspace)		(EContentEditor *editor);

	void		(*image_set_border)		(EContentEditor *editor,
							 gint border);

	gint		(*image_get_border)		(EContentEditor *editor);

	void		(*image_set_align)		(EContentEditor *editor,
							 const gchar *value);

	gchar *		(*image_get_align)		(EContentEditor *editor);

	void		(*link_get_values)		(EContentEditor *editor,
							 gchar **href,
							 gchar **text);

	void		(*link_set_values)		(EContentEditor *editor,
							 const gchar *href,
							 const gchar *text);

	void		(*on_page_dialog_open)		(EContentEditor *editor);

	void		(*on_page_dialog_close)		(EContentEditor *editor);

	void		(*page_set_text_color)		(EContentEditor *editor,
							 const GdkRGBA *value);

	void 		(*page_get_text_color)		(EContentEditor *editor,
							 GdkRGBA *value);

	void		(*page_set_background_color)	(EContentEditor *editor,
							 const GdkRGBA *value);

	void		(*page_get_background_color)	(EContentEditor *editor,
							 GdkRGBA *value);

	void		(*page_set_link_color)		(EContentEditor *editor,
							 const GdkRGBA *value);

	void		(*page_get_link_color)		(EContentEditor *editor,
							 GdkRGBA *value);

	void		(*page_set_visited_link_color)	(EContentEditor *editor,
							 const GdkRGBA *value);

	void		(*page_get_visited_link_color)	(EContentEditor *editor,
							 GdkRGBA *value);

	void		(*page_set_background_image_uri)
							(EContentEditor *editor,
							 const gchar *uri);

	gchar *		(*page_get_background_image_uri)
							(EContentEditor *editor);

	void		(*on_cell_dialog_open)		(EContentEditor *editor);

	void		(*on_cell_dialog_close)		(EContentEditor *editor);

	void		(*cell_set_v_align)		(EContentEditor *editor,
							 const gchar *value,
							 EContentEditorScope scope);

	gchar *		(*cell_get_v_align)		(EContentEditor *editor);

	void		(*cell_set_align)		(EContentEditor *editor,
							 const gchar *value,
							 EContentEditorScope scope);

	gchar *		(*cell_get_align)		(EContentEditor *editor);

	void		(*cell_set_wrap)		(EContentEditor *editor,
							 gboolean value,
							 EContentEditorScope scope);

	gboolean	(*cell_get_wrap)		(EContentEditor *editor);

	void		(*cell_set_header_style)	(EContentEditor *editor,
							 gboolean value,
							 EContentEditorScope scope);

	gboolean	(*cell_is_header)		(EContentEditor *editor);

	void		(*cell_set_width)		(EContentEditor *editor,
							 gint value,
							 EContentEditorUnit unit,
							 EContentEditorScope scope);

	gint		(*cell_get_width)		(EContentEditor *editor,
							 EContentEditorUnit *unit);

	void		(*cell_set_row_span)		(EContentEditor *editor,
							 gint value,
							 EContentEditorScope scope);

	gint		(*cell_get_row_span)		(EContentEditor *editor);

	void		(*cell_set_col_span)		(EContentEditor *editor,
							 gint value,
							 EContentEditorScope scope);

	gint		(*cell_get_col_span)		(EContentEditor *editor);

	gchar *		(*cell_get_background_image_uri)
							(EContentEditor *editor);

	void		(*cell_set_background_image_uri)
							(EContentEditor *editor,
							 const gchar *uri);

	void		(*cell_get_background_color)	(EContentEditor *editor,
							 GdkRGBA *value);

	void		(*cell_set_background_color)	(EContentEditor *editor,
							 const GdkRGBA *value,
							 EContentEditorScope scope);

	void		(*table_set_row_count)		(EContentEditor *editor,
							 guint value);

	guint		(*table_get_row_count)		(EContentEditor *editor);

	void		(*table_set_column_count)	(EContentEditor *editor,
							 guint value);

	guint		(*table_get_column_count)	(EContentEditor *editor);

	void		(*table_set_width)		(EContentEditor *editor,
							 gint value,
							 EContentEditorUnit unit);

	guint		(*table_get_width)		(EContentEditor *editor,
							 EContentEditorUnit *unit);

	void		(*table_set_align)		(EContentEditor *editor,
							 const gchar *value);

	gchar *		(*table_get_align)		(EContentEditor *editor);

	void		(*table_set_padding)		(EContentEditor *editor,
							 gint value);

	gint		(*table_get_padding)		(EContentEditor *editor);

	void		(*table_set_spacing)		(EContentEditor *editor,
							 gint value);

	gint		(*table_get_spacing)		(EContentEditor *editor);

	void		(*table_set_border)		(EContentEditor *editor,
							 gint value);

	gint		(*table_get_border)		(EContentEditor *editor);

	gchar *		(*table_get_background_image_uri)
							(EContentEditor *editor);

	void		(*table_set_background_image_uri)
							(EContentEditor *editor,
							 const gchar *uri);

	void		(*table_get_background_color)	(EContentEditor *editor,
							 GdkRGBA *value);

	void		(*table_set_background_color)	(EContentEditor *editor,
							 const GdkRGBA *value);

	gboolean	(*on_table_dialog_open)		(EContentEditor *editor);

	void		(*on_table_dialog_close)	(EContentEditor *editor);

	void		(*on_spell_check_dialog_open)	(EContentEditor *editor);

	void		(*on_spell_check_dialog_close)	(EContentEditor *editor);

	gchar *		(*spell_check_next_word)	(EContentEditor *editor,
							 const gchar *word);

	gchar *		(*spell_check_prev_word)	(EContentEditor *editor,
							 const gchar *word);

	void		(*on_replace_dialog_open)	(EContentEditor *editor);

	void		(*on_replace_dialog_close)	(EContentEditor *editor);

	void		(*on_find_dialog_open)		(EContentEditor *editor);

	void		(*on_find_dialog_close)		(EContentEditor *editor);

	/* Signals */
	void		(*load_finished)		(EContentEditor *editor);
	gboolean	(*paste_clipboard)		(EContentEditor *editor);
	gboolean	(*paste_primary_clipboard)	(EContentEditor *editor);
	gboolean	(*context_menu_requested)	(EContentEditor *editor,
							 EContentEditorNodeFlags flags,
							 GdkEvent *event);
	void		(*find_done)			(EContentEditor *editor,
							 guint match_count);
	void		(*replace_all_done)		(EContentEditor *editor,
							 guint replaced_count);
};

void		e_content_editor_insert_content	(EContentEditor *editor,
						 const gchar *content,
						 EContentEditorInsertContentFlags flags);

gchar *		e_content_editor_get_content	(EContentEditor *editor,
						 EContentEditorGetContentFlags flags,
						 EContentEditorInlineImages **inline_images);

void            e_content_editor_insert_image_from_mime_part
						(EContentEditor *editor,
						 CamelMimePart *part);

void		e_content_editor_insert_image	(EContentEditor *editor,
						 const gchar *uri);

void		e_content_editor_insert_emoticon
						(EContentEditor *editor,
						 EEmoticon *emoticon);

void		e_content_editor_set_current_content_flags
						(EContentEditor *editor,
						 EContentEditorContentFlags flags);

EContentEditorContentFlags
		e_content_editor_get_current_content_flags
						(EContentEditor *editor);

void		e_content_editor_move_caret_on_coordinates
						(EContentEditor *editor,
						 gint x,
						 gint y,
						 gboolean cancel_if_not_collapsed);

void		e_content_editor_set_changed	(EContentEditor *editor,
						 gboolean changed);

gboolean	e_content_editor_get_changed	(EContentEditor *editor);

void		e_content_editor_cut		(EContentEditor *editor);

void		e_content_editor_copy		(EContentEditor *editor);

void		e_content_editor_paste		(EContentEditor *editor);

gboolean	e_content_editor_paste_prefer_text_html
						(EContentEditor *editor);

void		e_content_editor_reconnect_paste_clipboard_signals
						(EContentEditor *editor);

gboolean	e_content_editor_can_undo	(EContentEditor *editor);

void		e_content_editor_undo		(EContentEditor *editor);

gboolean	e_content_editor_can_redo	(EContentEditor *editor);

void		e_content_editor_redo		(EContentEditor *editor);

void		e_content_editor_clear_undo_redo_history
						(EContentEditor *editor);

void		e_content_editor_set_html_mode	(EContentEditor *editor,
						 gboolean html_mode);

gboolean	e_content_editor_get_html_mode	(EContentEditor *editor);

ESpellChecker *	e_content_editor_get_spell_checker
						(EContentEditor *editor);

void		e_content_editor_set_spell_checking_languages
						(EContentEditor *editor,
						 const gchar **languages);

void		e_content_editor_set_spell_check
						(EContentEditor *editor,
						 gboolean enable);

gboolean	e_content_editor_get_spell_check
						(EContentEditor *editor);

void		e_content_editor_select_all	(EContentEditor *editor);

gchar *		e_content_editor_selection_get_text
						(EContentEditor *editor);

gchar *		e_content_editor_get_caret_word	(EContentEditor *editor);

void		e_content_editor_replace_caret_word
						(EContentEditor *editor,
						 const gchar *replacement);

gboolean	e_content_editor_selection_is_indented
						(EContentEditor *editor);

void		e_content_editor_selection_indent
						(EContentEditor *editor);

void		e_content_editor_selection_unindent
						(EContentEditor *editor);

void		e_content_editor_selection_create_link
						(EContentEditor *editor,
						 const gchar *uri);

void		e_content_editor_selection_unlink
						(EContentEditor *editor);

void		e_content_editor_find		(EContentEditor *editor,
						 guint32 flags,
						 const gchar *text);

void		e_content_editor_selection_replace
						(EContentEditor *editor,
						 const gchar *replacement);

void		e_content_editor_replace_all	(EContentEditor *editor,
						 guint32 flags,
						 const gchar *find_text,
						 const gchar *replace_with);

void		e_content_editor_selection_save	(EContentEditor *editor);

void		e_content_editor_selection_restore
						(EContentEditor *editor);

void		e_content_editor_selection_wrap	(EContentEditor *editor);

void		e_content_editor_show_inspector (EContentEditor *editor);

guint		e_content_editor_get_caret_position
						(EContentEditor *editor);

guint		e_content_editor_get_caret_offset
						(EContentEditor *editor);

void		e_content_editor_update_fonts	(EContentEditor *editor);

gboolean	e_content_editor_is_editable	(EContentEditor *editor);

void		e_content_editor_set_editable	(EContentEditor *editor,
						 gboolean editable);

gchar *		e_content_editor_get_current_signature_uid
						(EContentEditor *editor);

gboolean	e_content_editor_is_ready	(EContentEditor *editor);

gchar *		e_content_editor_insert_signature
						(EContentEditor *editor,
						 const gchar *content,
						 gboolean is_html,
						 const gchar *signature_id,
						 gboolean *set_signature_from_message,
						 gboolean *check_if_signature_is_changed,
						 gboolean *ignore_next_signature_change);

void		e_content_editor_set_alignment	(EContentEditor *editor,
						 EContentEditorAlignment value);

EContentEditorAlignment
		e_content_editor_get_alignment	(EContentEditor *editor);

void		e_content_editor_set_block_format
						(EContentEditor *editor,
						 EContentEditorBlockFormat value);

EContentEditorBlockFormat
		e_content_editor_get_block_format
						(EContentEditor *editor);

void		e_content_editor_set_background_color
						(EContentEditor *editor,
						 const GdkRGBA *value);

const GdkRGBA *	e_content_editor_get_background_color
						(EContentEditor *editor);

void		e_content_editor_set_font_name	(EContentEditor *editor,
						 const gchar *value);

const gchar *	e_content_editor_get_font_name	(EContentEditor *editor);

void		e_content_editor_set_font_color	(EContentEditor *editor,
						 const GdkRGBA *value);

const GdkRGBA *	e_content_editor_get_font_color	(EContentEditor *editor);

void		e_content_editor_set_font_size	(EContentEditor *editor,
						 guint value);

guint		e_content_editor_get_font_size	(EContentEditor *editor);

void		e_content_editor_set_bold	(EContentEditor *editor,
						 gboolean bold);

gboolean	e_content_editor_is_bold	(EContentEditor *editor);

void		e_content_editor_set_italic	(EContentEditor *editor,
						 gboolean italic);

gboolean	e_content_editor_is_italic	(EContentEditor *editor);

void		e_content_editor_set_monospaced	(EContentEditor *editor,
						 gboolean monospaced);

gboolean	e_content_editor_is_monospaced (EContentEditor *editor);

void		e_content_editor_set_strikethrough
						(EContentEditor *editor,
						 gboolean strikethrough);

gboolean	e_content_editor_is_strikethrough
						(EContentEditor *editor);

void		e_content_editor_set_subscript	(EContentEditor *editor,
						 gboolean subscript);

gboolean	e_content_editor_is_subscript	(EContentEditor *editor);

void		e_content_editor_set_superscript
						(EContentEditor *editor,
						 gboolean superscript);

gboolean	e_content_editor_is_superscript
						(EContentEditor *editor);

void		e_content_editor_set_underline	(EContentEditor *editor,
						 gboolean underline);

gboolean	e_content_editor_is_underline	(EContentEditor *editor);

void		e_content_editor_delete_cell_contents
						(EContentEditor *editor);
void
		e_content_editor_delete_column	(EContentEditor *editor);

void		e_content_editor_delete_row	(EContentEditor *editor);

void		e_content_editor_delete_table	(EContentEditor *editor);

void		e_content_editor_insert_column_after
						(EContentEditor *editor);

void		e_content_editor_insert_column_before
						(EContentEditor *editor);

void		e_content_editor_insert_row_above
						(EContentEditor *editor);

void		e_content_editor_insert_row_below
						(EContentEditor *editor);

gboolean	e_content_editor_on_h_rule_dialog_open
						(EContentEditor *editor);

void		e_content_editor_on_h_rule_dialog_close
						(EContentEditor *editor);

void		e_content_editor_h_rule_set_align
						(EContentEditor *editor,
						 const gchar *value);

gchar *		e_content_editor_h_rule_get_align
						(EContentEditor *editor);

void		e_content_editor_h_rule_set_size
						(EContentEditor *editor,
						 gint value);

gint		e_content_editor_h_rule_get_size
						(EContentEditor *editor);

void		e_content_editor_h_rule_set_width
						(EContentEditor *editor,
						 gint value,
						 EContentEditorUnit unit);

gint		e_content_editor_h_rule_get_width
						(EContentEditor *editor,
						 EContentEditorUnit *unit);

void		e_content_editor_h_rule_set_no_shade
						(EContentEditor *editor,
						 gboolean value);

gboolean	e_content_editor_h_rule_get_no_shade
						(EContentEditor *editor);

void		e_content_editor_on_image_dialog_open
						(EContentEditor *editor);

void		e_content_editor_on_image_dialog_close
						(EContentEditor *editor);

void		e_content_editor_image_set_src	(EContentEditor *editor,
						 const gchar *value);

gchar *		e_content_editor_image_get_src	(EContentEditor *editor);

void		e_content_editor_image_set_alt	(EContentEditor *editor,
						 const gchar *value);

gchar *		e_content_editor_image_get_alt	(EContentEditor *editor);

void		e_content_editor_image_set_url	(EContentEditor *editor,
						 const gchar *value);

gchar *		e_content_editor_image_get_url	(EContentEditor *editor);

void		e_content_editor_image_set_vspace
						(EContentEditor *editor,
						 gint value);

gint		e_content_editor_image_get_vspace
						(EContentEditor *editor);

void		e_content_editor_image_set_hspace
						(EContentEditor *editor,
						 gint value);

gint		e_content_editor_image_get_hspace
						(EContentEditor *editor);

void		e_content_editor_image_set_border
						(EContentEditor *editor,
						 gint value);

gint		e_content_editor_image_get_border
						(EContentEditor *editor);

void		e_content_editor_image_set_align
						(EContentEditor *editor,
						 const gchar *value);

gchar *		e_content_editor_image_get_align
						(EContentEditor *editor);

void		e_content_editor_image_set_width
						(EContentEditor *editor,
						 gint value);

gint32		e_content_editor_image_get_width
						(EContentEditor *editor);

gint32		e_content_editor_image_get_natural_width
						(EContentEditor *editor);

void		e_content_editor_image_set_width_follow
						(EContentEditor *editor,
						 gboolean value);
void		e_content_editor_image_set_height
						(EContentEditor *editor,
						 gint value);

gint32		e_content_editor_image_get_height
						(EContentEditor *editor);

gint32		e_content_editor_image_get_natural_height
						(EContentEditor *editor);

void		e_content_editor_image_set_height_follow
						(EContentEditor *editor,
						 gboolean value);

void		e_content_editor_link_get_values
						(EContentEditor *editor,
						 gchar **href,
						 gchar **text);

void		e_content_editor_link_set_values
						(EContentEditor *editor,
						 const gchar *href,
						 const gchar *text);

void		e_content_editor_page_set_text_color
						(EContentEditor *editor,
						 const GdkRGBA *value);

void		e_content_editor_on_page_dialog_open
						(EContentEditor *editor);

void		e_content_editor_on_page_dialog_close
						(EContentEditor *editor);

void		e_content_editor_page_get_text_color
						(EContentEditor *editor,
						 GdkRGBA *value);

void		e_content_editor_page_set_background_color
						(EContentEditor *editor,
						 const GdkRGBA *value);

void		e_content_editor_page_get_background_color
						(EContentEditor *editor,
						 GdkRGBA *value);

void		e_content_editor_page_set_link_color
						(EContentEditor *editor,
						 const GdkRGBA *value);

void		e_content_editor_page_get_link_color
						(EContentEditor *editor,
						 GdkRGBA *value);

void		e_content_editor_page_set_visited_link_color
						(EContentEditor *editor,
						 const GdkRGBA *value);

void		e_content_editor_page_get_visited_link_color
						(EContentEditor *editor,
						 GdkRGBA *value);

void		e_content_editor_page_set_background_image_uri
						(EContentEditor *editor,
						 const gchar *uri);

gchar *		e_content_editor_page_get_background_image_uri
						(EContentEditor *editor);

void		e_content_editor_on_cell_dialog_open
						(EContentEditor *editor);

void		e_content_editor_on_cell_dialog_close
						(EContentEditor *editor);

void		e_content_editor_cell_set_v_align
						(EContentEditor *editor,
						 const gchar *value,
						 EContentEditorScope scope);

gchar *		e_content_editor_cell_get_v_align
						(EContentEditor *editor);

void		e_content_editor_cell_set_align	(EContentEditor *editor,
						 const gchar *value,
						 EContentEditorScope scope);

gchar *		e_content_editor_cell_get_align	(EContentEditor *editor);

void		e_content_editor_cell_set_wrap	(EContentEditor *editor,
						 gboolean value,
						 EContentEditorScope scope);

gboolean	e_content_editor_cell_get_wrap	(EContentEditor *editor);

void		e_content_editor_cell_set_header_style
						(EContentEditor *editor,
						 gboolean value,
						 EContentEditorScope scope);

gboolean	e_content_editor_cell_is_header	(EContentEditor *editor);

void		e_content_editor_cell_set_width	(EContentEditor *editor,
						 gint value,
						 EContentEditorUnit unit,
						 EContentEditorScope scope);

gint		e_content_editor_cell_get_width	(EContentEditor *editor,
						 EContentEditorUnit *unit);

void		e_content_editor_cell_set_row_span
						(EContentEditor *editor,
						 gint value,
						 EContentEditorScope scope);

gint		e_content_editor_cell_get_row_span
						(EContentEditor *editor);

void		e_content_editor_cell_set_col_span
						(EContentEditor *editor,
						 gint value,
						 EContentEditorScope scope);

gint		e_content_editor_cell_get_col_span
						(EContentEditor *editor);

void		e_content_editor_cell_set_background_image_uri
						(EContentEditor *editor,
						 const gchar *uri);

gchar *		e_content_editor_cell_get_background_image_uri
						(EContentEditor *editor);

void		e_content_editor_cell_set_background_color
						(EContentEditor *editor,
						 const GdkRGBA *value,
						 EContentEditorScope scope);

void		e_content_editor_cell_get_background_color
						(EContentEditor *editor,
						 GdkRGBA *value);

void		e_content_editor_table_set_row_count
						(EContentEditor *editor,
						 guint value);

guint		e_content_editor_table_get_row_count
						(EContentEditor *editor);

void		e_content_editor_table_set_column_count
						(EContentEditor *editor,
						 guint value);

guint		e_content_editor_table_get_column_count
						(EContentEditor *editor);

void		e_content_editor_table_set_width
						(EContentEditor *editor,
						 gint value,
						 EContentEditorUnit unit);

guint		e_content_editor_table_get_width
						(EContentEditor *editor,
						 EContentEditorUnit *unit);

void		e_content_editor_table_set_align
						(EContentEditor *editor,
						 const gchar *value);

gchar *		e_content_editor_table_get_align
						(EContentEditor *editor);

void		e_content_editor_table_set_padding
						(EContentEditor *editor,
						 gint value);

gint		e_content_editor_table_get_padding
						(EContentEditor *editor);

void		e_content_editor_table_set_spacing
						(EContentEditor *editor,
						 gint value);

gint		e_content_editor_table_get_spacing
						(EContentEditor *editor);

void		e_content_editor_table_set_border
						(EContentEditor *editor,
						 gint value);

gint		e_content_editor_table_get_border
						(EContentEditor *editor);

gchar *		e_content_editor_table_get_background_image_uri
						(EContentEditor *editor);

void		e_content_editor_table_set_background_image_uri
						(EContentEditor *editor,
						 const gchar *uri);

void		e_content_editor_table_get_background_color
						(EContentEditor *editor,
						 GdkRGBA *value);

void		e_content_editor_table_set_background_color
						(EContentEditor *editor,
						 const GdkRGBA *value);

gboolean	e_content_editor_on_table_dialog_open
						(EContentEditor *editor);

void		e_content_editor_on_table_dialog_close
						(EContentEditor *editor);

void		e_content_editor_on_spell_check_dialog_open
						(EContentEditor *editor);

void		e_content_editor_on_spell_check_dialog_close
						(EContentEditor *editor);

gchar *		e_content_editor_spell_check_next_word
						(EContentEditor *editor,
						 const gchar *word);

gchar *		e_content_editor_spell_check_prev_word
						(EContentEditor *editor,
						 const gchar *word);

void		e_content_editor_spell_check_replace_all
						(EContentEditor *editor,
						 const gchar *word,
						 const gchar *replacement);

void		e_content_editor_on_replace_dialog_open
						(EContentEditor *editor);

void		e_content_editor_on_replace_dialog_close
						(EContentEditor *editor);

void		e_content_editor_on_find_dialog_open
						(EContentEditor *editor);

void		e_content_editor_on_find_dialog_close
						(EContentEditor *editor);

void		e_content_editor_emit_load_finished
						(EContentEditor *editor);
gboolean	e_content_editor_emit_paste_clipboard
						(EContentEditor *editor);
gboolean	e_content_editor_emit_paste_primary_clipboard
						(EContentEditor *editor);
gboolean	e_content_editor_emit_context_menu_requested
						(EContentEditor *editor,
						 EContentEditorNodeFlags flags,
						 GdkEvent *event);
void		e_content_editor_emit_find_done	(EContentEditor *editor,
						 guint match_count);
void		e_content_editor_emit_replace_all_done
						(EContentEditor *editor,
						 guint replaced_count);

G_END_DECLS

#endif /* E_CONTENT_EDITOR_H */
