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
#include <libedataserver/libedataserver.h>

#include <e-util/e-emoticon.h>
#include <e-util/e-spell-checker.h>
#include <e-util/e-util-enums.h>

#define DEFAULT_CONTENT_EDITOR_NAME "WebKit"

#define E_CONTENT_EDITOR_DIALOG_HRULE		"hrule"
#define E_CONTENT_EDITOR_DIALOG_IMAGE		"image"
#define E_CONTENT_EDITOR_DIALOG_LINK		"link"
#define E_CONTENT_EDITOR_DIALOG_PAGE		"page"
#define E_CONTENT_EDITOR_DIALOG_CELL		"cell"
#define E_CONTENT_EDITOR_DIALOG_TABLE		"table"
#define E_CONTENT_EDITOR_DIALOG_SPELLCHECK	"spellcheck"
#define E_CONTENT_EDITOR_DIALOG_FIND		"find"
#define E_CONTENT_EDITOR_DIALOG_REPLACE		"replace"

G_BEGIN_DECLS

struct _EHTMLEditor;

#define E_TYPE_CONTENT_EDITOR e_content_editor_get_type ()
G_DECLARE_INTERFACE (EContentEditor, e_content_editor, E, CONTENT_EDITOR, GtkWidget)

typedef GHashTable EContentEditorContentHash;

typedef void (*EContentEditorInitializedCallback)	(EContentEditor *content_editor,
							 gpointer user_data);

struct _EContentEditorInterface {
	GTypeInterface parent_interface;

	void		(*initialize)			(EContentEditor *content_editor,
							 EContentEditorInitializedCallback callback,
							 gpointer user_data);
	void		(*setup_editor)			(EContentEditor *content_editor,
							 struct _EHTMLEditor *html_editor);
	void		(*update_styles)		(EContentEditor *editor);
	void		(*insert_content)		(EContentEditor *editor,
							 const gchar *content,
							 EContentEditorInsertContentFlags flags);

	void		(*get_content)			(EContentEditor *editor,
							 guint32 flags, /* bit-or of EContentEditorGetContentFlags */
							 const gchar *inline_images_from_domain,
							 GCancellable *cancellable,
							 GAsyncReadyCallback callback,
							 gpointer user_data);
	EContentEditorContentHash *
			(*get_content_finish)		(EContentEditor *editor,
							 GAsyncResult *result,
							 GError **error);

	void		(*insert_image)			(EContentEditor *editor,
							 const gchar *uri);

	void		(*insert_emoticon)		(EContentEditor *editor,
							 const EEmoticon *emoticon);

	void		(*move_caret_on_coordinates)	(EContentEditor *editor,
							 gint x,
							 gint y,
							 gboolean cancel_if_not_collapsed);

	void		(*cut)				(EContentEditor *editor);

	void		(*copy)				(EContentEditor *editor);

	void		(*paste)			(EContentEditor *editor);

	void		(*paste_primary)		(EContentEditor *editor);

	void		(*undo)				(EContentEditor *editor);

	void		(*redo)				(EContentEditor *editor);

	void		(*clear_undo_redo_history)	(EContentEditor *editor);

	void		(*set_spell_checking_languages)	(EContentEditor *editor,
							 const gchar **languages);

	gchar *		(*get_caret_word)		(EContentEditor *editor);

	void		(*replace_caret_word)		(EContentEditor *editor,
							 const gchar *replacement);

	void		(*select_all)			(EContentEditor *editor);

	void		(*selection_indent)		(EContentEditor *editor);

	void		(*selection_unindent)		(EContentEditor *editor);

	void		(*selection_unlink)		(EContentEditor *editor);

	void		(*find)				(EContentEditor *editor,
							 guint32 flags,
							 const gchar *text);

	void		(*replace)			(EContentEditor *editor,
							 const gchar *replacement);

	void		(*replace_all)			(EContentEditor *editor,
							 guint32 flags,
							 const gchar *find_text,
							 const gchar *replace_with);

	void		(*selection_save)		(EContentEditor *editor);

	void		(*selection_restore)		(EContentEditor *editor);

	void		(*selection_wrap)		(EContentEditor *editor);

	gchar *		(*get_current_signature_uid)	(EContentEditor *editor);

	gboolean	(*is_ready)			(EContentEditor *editor);

	gchar *		(*insert_signature)		(EContentEditor *editor,
							 const gchar *content,
							 EContentEditorMode editor_mode,
							 gboolean can_reposition_caret,
							 const gchar *signature_id,
							 gboolean *set_signature_from_message,
							 gboolean *check_if_signature_is_changed,
							 gboolean *ignore_next_signature_change);

	void		(*delete_cell_contents)		(EContentEditor *editor);

	void		(*delete_column)		(EContentEditor *editor);

	void		(*delete_row)			(EContentEditor *editor);

	void		(*delete_table)			(EContentEditor *editor);

	void		(*insert_column_after)		(EContentEditor *editor);

	void		(*insert_column_before)		(EContentEditor *editor);

	void		(*insert_row_above)		(EContentEditor *editor);

	void		(*insert_row_below)		(EContentEditor *editor);

	void		(*on_dialog_open)		(EContentEditor *editor,
							 const gchar *name);

	void		(*on_dialog_close)		(EContentEditor *editor,
							 const gchar *name);

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

	void		(*link_get_properties)		(EContentEditor *editor,
							 gchar **out_href,
							 gchar **out_text,
							 gchar **out_name);

	void		(*link_set_properties)		(EContentEditor *editor,
							 const gchar *href,
							 const gchar *text,
							 const gchar *name);

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
	void		(*page_set_font_name)		(EContentEditor *editor,
							 const gchar *value);
	const gchar *	(*page_get_font_name)		(EContentEditor *editor);

	void		(*page_set_background_image_uri)
							(EContentEditor *editor,
							 const gchar *uri);

	gchar *		(*page_get_background_image_uri)
							(EContentEditor *editor);

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

	gchar *		(*spell_check_next_word)	(EContentEditor *editor,
							 const gchar *word);

	gchar *		(*spell_check_prev_word)	(EContentEditor *editor,
							 const gchar *word);

	/* Signals */
	void		(*load_finished)		(EContentEditor *editor);
	gboolean	(*paste_clipboard)		(EContentEditor *editor);
	gboolean	(*paste_primary_clipboard)	(EContentEditor *editor);
	void		(*context_menu_requested)	(EContentEditor *editor,
							 EContentEditorNodeFlags flags,
							 const gchar *caret_word,
							 GdkEvent *event);
	void		(*find_done)			(EContentEditor *editor,
							 guint match_count);
	void		(*replace_all_done)		(EContentEditor *editor,
							 guint replaced_count);
	void		(*drop_handled)			(EContentEditor *editor);
	void		(*content_changed)		(EContentEditor *editor);
	CamelMimePart *	(*ref_mime_part)		(EContentEditor *editor,
							 const gchar *uri);

	void		(*delete_h_rule)		(EContentEditor *editor);
	void		(*delete_image)			(EContentEditor *editor);

	gboolean	(*supports_mode)		(EContentEditor *editor,
							 EContentEditorMode mode);
	void		(*grab_focus)			(EContentEditor *editor);
	gboolean	(*is_focus)			(EContentEditor *editor);
	const gchar *	(*get_hover_uri)		(EContentEditor *editor);
	void		(*get_caret_client_rect)	(EContentEditor *editor,
							 GdkRectangle *out_rect);
	gdouble		(*get_zoom_level)		(EContentEditor *editor);
	void		(*set_zoom_level)		(EContentEditor *editor,
							 gdouble level);
	/* padding for future expansion */
	gpointer reserved[13];
};

/* Properties */

ESpellChecker *	e_content_editor_ref_spell_checker
						(EContentEditor *editor);
gboolean	e_content_editor_supports_mode	(EContentEditor *editor,
						 EContentEditorMode mode);
void		e_content_editor_grab_focus	(EContentEditor *editor);
gboolean	e_content_editor_is_focus	(EContentEditor *editor);
gboolean	e_content_editor_is_malfunction	(EContentEditor *editor);
gboolean	e_content_editor_can_cut	(EContentEditor *editor);
gboolean	e_content_editor_can_copy	(EContentEditor *editor);
gboolean	e_content_editor_can_paste	(EContentEditor *editor);
gboolean	e_content_editor_can_undo	(EContentEditor *editor);
gboolean	e_content_editor_can_redo	(EContentEditor *editor);
gint		e_content_editor_indent_level	(EContentEditor *editor);
gboolean	e_content_editor_get_spell_check_enabled
						(EContentEditor *editor);
void		e_content_editor_set_spell_check_enabled
						(EContentEditor *editor,
						 gboolean enable);
gboolean	e_content_editor_is_editable	(EContentEditor *editor);
void		e_content_editor_set_editable	(EContentEditor *editor,
						 gboolean editable);
gboolean	e_content_editor_get_changed	(EContentEditor *editor);
void		e_content_editor_set_changed	(EContentEditor *editor,
						 gboolean changed);
void		e_content_editor_set_alignment	(EContentEditor *editor,
						 EContentEditorAlignment value);
EContentEditorAlignment
		e_content_editor_get_alignment	(EContentEditor *editor);
void		e_content_editor_set_background_color
						(EContentEditor *editor,
						 const GdkRGBA *value);
GdkRGBA *	e_content_editor_dup_background_color
						(EContentEditor *editor);
void		e_content_editor_set_font_color	(EContentEditor *editor,
						 const GdkRGBA *value);
GdkRGBA *	e_content_editor_dup_font_color	(EContentEditor *editor);
void		e_content_editor_set_font_name	(EContentEditor *editor,
						 const gchar *value);
gchar *		e_content_editor_dup_font_name	(EContentEditor *editor);
void		e_content_editor_set_font_size	(EContentEditor *editor,
						 gint value);
gint		e_content_editor_get_font_size	(EContentEditor *editor);
void		e_content_editor_set_block_format
						(EContentEditor *editor,
						 EContentEditorBlockFormat value);
EContentEditorBlockFormat
		e_content_editor_get_block_format
						(EContentEditor *editor);
void		e_content_editor_set_bold	(EContentEditor *editor,
						 gboolean bold);
gboolean	e_content_editor_is_bold	(EContentEditor *editor);
void		e_content_editor_set_italic	(EContentEditor *editor,
						 gboolean italic);
gboolean	e_content_editor_is_italic	(EContentEditor *editor);
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
void		e_content_editor_set_start_bottom
						(EContentEditor *editor,
						 EThreeState value);
EThreeState	e_content_editor_get_start_bottom
						(EContentEditor *editor);
void		e_content_editor_set_top_signature
						(EContentEditor *editor,
						 EThreeState value);
EThreeState	e_content_editor_get_top_signature
						(EContentEditor *editor);
void		e_content_editor_set_visually_wrap_long_lines
						(EContentEditor *editor,
						 gboolean value);
gboolean	e_content_editor_get_visually_wrap_long_lines
						(EContentEditor *editor);

/* Methods */
void		e_content_editor_initialize	(EContentEditor *content_editor,
						 EContentEditorInitializedCallback callback,
						 gpointer user_data);
void		e_content_editor_setup_editor	(EContentEditor *content_editor,
						 struct _EHTMLEditor *html_editor);
void		e_content_editor_update_styles	(EContentEditor *editor);
void		e_content_editor_insert_content	(EContentEditor *editor,
						 const gchar *content,
						 EContentEditorInsertContentFlags flags);

void		e_content_editor_get_content	(EContentEditor *editor,
						 guint32 flags, /* bit-or of EContentEditorGetContentFlags */
						 const gchar *inline_images_from_domain,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
EContentEditorContentHash *
		e_content_editor_get_content_finish
						(EContentEditor *editor,
						 GAsyncResult *result,
						 GError **error);
EContentEditorContentHash *
		e_content_editor_util_new_content_hash
						(void);
void		e_content_editor_util_free_content_hash
						(EContentEditorContentHash *content_hash);
void		e_content_editor_util_put_content_data
						(EContentEditorContentHash *content_hash,
						 EContentEditorGetContentFlags flag,
						 const gchar *data);
void		e_content_editor_util_take_content_data
						(EContentEditorContentHash *content_hash,
						 EContentEditorGetContentFlags flag,
						 gpointer data,
						 GDestroyNotify destroy_data);
void		e_content_editor_util_take_content_data_images
						(EContentEditorContentHash *content_hash,
						 GSList *image_parts); /* CamelMimePart * */
gpointer	e_content_editor_util_get_content_data
						(EContentEditorContentHash *content_hash,
						 EContentEditorGetContentFlags flag);
gpointer	e_content_editor_util_steal_content_data
						(EContentEditorContentHash *content_hash,
						 EContentEditorGetContentFlags flag,
						 GDestroyNotify *out_destroy_data);
CamelMimePart *	e_content_editor_util_create_data_mimepart
						(const gchar *uri,
						 const gchar *cid,
						 gboolean as_inline,
						 const gchar *prefer_filename,
						 const gchar *prefer_mime_type,
						 GCancellable *cancellable);

void		e_content_editor_insert_image	(EContentEditor *editor,
						 const gchar *uri);
void		e_content_editor_insert_emoticon
						(EContentEditor *editor,
						 const EEmoticon *emoticon);

void		e_content_editor_move_caret_on_coordinates
						(EContentEditor *editor,
						 gint x,
						 gint y,
						 gboolean cancel_if_not_collapsed);

void		e_content_editor_cut		(EContentEditor *editor);

void		e_content_editor_copy		(EContentEditor *editor);

void		e_content_editor_paste		(EContentEditor *editor);

void		e_content_editor_paste_primary	(EContentEditor *editor);

void		e_content_editor_undo		(EContentEditor *editor);

void		e_content_editor_redo		(EContentEditor *editor);

void		e_content_editor_clear_undo_redo_history
						(EContentEditor *editor);

void		e_content_editor_set_spell_checking_languages
						(EContentEditor *editor,
						 const gchar **languages);

void		e_content_editor_select_all	(EContentEditor *editor);

gchar *		e_content_editor_get_caret_word	(EContentEditor *editor);

void		e_content_editor_replace_caret_word
						(EContentEditor *editor,
						 const gchar *replacement);

void		e_content_editor_selection_indent
						(EContentEditor *editor);

void		e_content_editor_selection_unindent
						(EContentEditor *editor);

void		e_content_editor_selection_unlink
						(EContentEditor *editor);

void		e_content_editor_find		(EContentEditor *editor,
						 guint32 flags,
						 const gchar *text);

void		e_content_editor_replace	(EContentEditor *editor,
						 const gchar *replacement);

void		e_content_editor_replace_all	(EContentEditor *editor,
						 guint32 flags,
						 const gchar *find_text,
						 const gchar *replace_with);

void		e_content_editor_selection_save	(EContentEditor *editor);

void		e_content_editor_selection_restore
						(EContentEditor *editor);

void		e_content_editor_selection_wrap	(EContentEditor *editor);

gchar *		e_content_editor_get_current_signature_uid
						(EContentEditor *editor);

gboolean	e_content_editor_is_ready	(EContentEditor *editor);
GError *	e_content_editor_dup_last_error	(EContentEditor *editor);
void		e_content_editor_take_last_error(EContentEditor *editor,
						 GError *error);

gchar *		e_content_editor_insert_signature
						(EContentEditor *editor,
						 const gchar *content,
						 EContentEditorMode editor_mode,
						 gboolean can_reposition_caret,
						 const gchar *signature_id,
						 gboolean *set_signature_from_message,
						 gboolean *check_if_signature_is_changed,
						 gboolean *ignore_next_signature_change);

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

void		e_content_editor_on_dialog_open	(EContentEditor *editor,
						 const gchar *name);
void		e_content_editor_on_dialog_close(EContentEditor *editor,
						 const gchar *name);

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

void		e_content_editor_link_get_properties
						(EContentEditor *editor,
						 gchar **out_href,
						 gchar **out_text,
						 gchar **out_name);

void		e_content_editor_link_set_properties
						(EContentEditor *editor,
						 const gchar *href,
						 const gchar *text,
						 const gchar *name);

void		e_content_editor_page_set_text_color
						(EContentEditor *editor,
						 const GdkRGBA *value);

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
void		e_content_editor_page_set_font_name
						(EContentEditor *editor,
						 const gchar *value);

const gchar *	e_content_editor_page_get_font_name
						(EContentEditor *editor);

void		e_content_editor_page_set_background_image_uri
						(EContentEditor *editor,
						 const gchar *uri);

gchar *		e_content_editor_page_get_background_image_uri
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
void		e_content_editor_delete_h_rule	(EContentEditor *editor);
void		e_content_editor_delete_image	(EContentEditor *editor);

const gchar *	e_content_editor_get_hover_uri	(EContentEditor *editor);
void		e_content_editor_get_caret_client_rect
						(EContentEditor *editor,
						 GdkRectangle *out_rect);
gdouble		e_content_editor_get_zoom_level	(EContentEditor *editor);
void		e_content_editor_set_zoom_level	(EContentEditor *editor,
						 gdouble level);

/* Signal helpers */

void		e_content_editor_emit_load_finished
						(EContentEditor *editor);
gboolean	e_content_editor_emit_paste_clipboard
						(EContentEditor *editor);
gboolean	e_content_editor_emit_paste_primary_clipboard
						(EContentEditor *editor);
void		e_content_editor_emit_context_menu_requested
						(EContentEditor *editor,
						 EContentEditorNodeFlags flags,
						 const gchar *caret_word,
						 GdkEvent *event);
void		e_content_editor_emit_find_done	(EContentEditor *editor,
						 guint match_count);
void		e_content_editor_emit_replace_all_done
						(EContentEditor *editor,
						 guint replaced_count);
void		e_content_editor_emit_drop_handled
						(EContentEditor *editor);
void		e_content_editor_emit_content_changed
						(EContentEditor *editor);
CamelMimePart *	e_content_editor_emit_ref_mime_part
						(EContentEditor *editor,
						 const gchar *uri);

gboolean	e_content_editor_util_three_state_to_bool
						(EThreeState value,
						 const gchar *mail_key);

G_END_DECLS

#endif /* E_CONTENT_EDITOR_H */
