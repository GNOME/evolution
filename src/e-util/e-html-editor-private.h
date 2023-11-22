/*
 * e-html-editor-private.h
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef E_HTML_EDITOR_PRIVATE_H
#define E_HTML_EDITOR_PRIVATE_H

#include <e-util/e-action-combo-box.h>
#include <e-util/e-color-combo.h>
#include <e-util/e-html-editor.h>
#include <e-util/e-html-editor-actions.h>
#include <e-util/e-html-editor-cell-dialog.h>
#include <e-util/e-html-editor-find-dialog.h>
#include <e-util/e-html-editor-hrule-dialog.h>
#include <e-util/e-html-editor-image-dialog.h>
#include <e-util/e-html-editor-link-popover.h>
#include <e-util/e-html-editor-page-dialog.h>
#include <e-util/e-html-editor-paragraph-dialog.h>
#include <e-util/e-html-editor-replace-dialog.h>
#include <e-util/e-html-editor-spell-check-dialog.h>
#include <e-util/e-html-editor-table-dialog.h>
#include <e-util/e-html-editor-text-dialog.h>
#include <e-util/e-util-enumtypes.h>

#ifdef HAVE_XFREE
#include <X11/XF86keysym.h>
#endif

#define ACTION(name) (E_HTML_EDITOR_ACTION_##name (editor))
#define WIDGET(name) (E_HTML_EDITOR_WIDGETS_##name (editor))

G_BEGIN_DECLS

struct _EHTMLEditorPrivate {
	EContentEditorMode mode;

	GtkWidget *content_editors_box;

	GtkUIManager *manager;
	GtkActionGroup *core_actions;
	GtkActionGroup *core_editor_actions;
	GtkActionGroup *html_actions;
	GtkActionGroup *context_actions;
	GtkActionGroup *html_context_actions;
	GtkActionGroup *language_actions;
	GtkActionGroup *spell_check_actions;
	GtkActionGroup *suggestion_actions;

	GtkWidget *main_menu;
	GtkWidget *main_toolbar;
	GtkWidget *edit_toolbar;
	GtkWidget *html_toolbar;
	GtkWidget *activity_bar;
	GtkWidget *alert_bar;
	GtkWidget *edit_area;
	GtkWidget *markdown_editor;

	GtkWidget *find_dialog;
	GtkWidget *replace_dialog;
	GtkWidget *link_popover;
	GtkWidget *hrule_dialog;
	GtkWidget *table_dialog;
	GtkWidget *page_dialog;
	GtkWidget *image_dialog;
	GtkWidget *text_dialog;
	GtkWidget *paragraph_dialog;
	GtkWidget *cell_dialog;
	GtkWidget *spell_check_dialog;

	GtkWidget *fg_color_combo_box;
	GtkWidget *bg_color_combo_box;
	GtkWidget *mode_combo_box;
	GtkToolItem *mode_tool_item;
	GtkWidget *size_combo_box;
	GtkWidget *style_combo_box;
	GtkWidget *font_name_combo_box;
	GtkWidget *scrolled_window;

	GtkWidget *emoji_chooser;

	GHashTable *cid_parts; /* gchar *cid: URI ~> CamelMimePart * */
	GHashTable *content_editors; /* gchar *name ~> EContentEditor * */
	GHashTable *content_editors_for_mode; /* EContentEditorMode ~> EContentEditor *; pointers borrowed from content_editors */
	EContentEditor *use_content_editor;
	GCancellable *mode_change_content_cancellable;

	gchar *filename;
	GSList *content_editor_bindings; /* reffed GBinding-s related to the EContentEditor */
	gulong subscript_notify_id;
	gulong superscript_notify_id;

	guint spell_suggestions_merge_id;
	guint recent_spell_languages_merge_id;

	gint editor_layout_row;

	gboolean paste_plain_prefer_pre;

	gchar *context_hover_uri;
};

void		e_html_editor_actions_init	(EHTMLEditor *editor);
void		e_html_editor_actions_bind	(EHTMLEditor *editor);
void		e_html_editor_actions_unbind	(EHTMLEditor *editor);
void		e_html_editor_actions_update_spellcheck_languages_menu
						(EHTMLEditor *editor,
						 const gchar * const *languages);
const gchar *	e_html_editor_get_content_editor_name
						(EHTMLEditor *editor);
GtkWidget *	e_html_editor_util_create_font_name_combo
						(void);
gchar *		e_html_editor_util_dup_font_id	(GtkComboBox *combo_box,
						 const gchar *font_name);
gboolean	e_html_editor_has_editor_for_mode
						(EHTMLEditor *editor,
						 EContentEditorMode mode);

G_END_DECLS

#endif /* E_HTML_EDITOR_PRIVATE_H */
