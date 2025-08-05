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
#include <e-util/e-ui-manager.h>
#include <e-util/e-ui-menu.h>
#include <e-util/e-util-enumtypes.h>

#define ACTION(name) (E_HTML_EDITOR_ACTION_##name (editor))

G_BEGIN_DECLS

typedef struct _EHTMLEditorActionMenuPair {
	EUIAction *parent_menu_action; /* to control visibility of the parent menu item (submenu) */
	GMenu *submenu_items; /* actual items of the submenu */
} EHTMLEditorActionMenuPair;

EHTMLEditorActionMenuPair *
		e_html_editor_action_menu_pair_new	(EUIAction *action, /* (transfer full) */
							 GMenu *menu); /* (transfer full) */
void		e_html_editor_action_menu_pair_free	(gpointer ptr);

struct _EHTMLEditorPrivate {
	EContentEditorMode mode;

	GtkWidget *content_editors_box;

	EUIManager *ui_manager;
	EUIActionGroup *core_actions; /* owned by priv->manager */
	EUIActionGroup *core_editor_actions; /* owned by priv->manager */
	EUIActionGroup *html_actions; /* owned by priv->manager */
	EUIActionGroup *context_actions; /* owned by priv->manager */
	EUIActionGroup *html_context_actions; /* owned by priv->manager */
	EUIActionGroup *language_actions; /* owned by priv->manager */
	EUIActionGroup *spell_check_actions; /* owned by priv->manager */
	EUIActionGroup *suggestion_actions; /* owned by priv->manager */

	GMenu *emoticon_menu;
	GMenu *recent_languages_menu;
	GMenu *all_languages_menu;
	GPtrArray *spell_suggest_actions; /* EUIAction; to fill a GMenu */
	GPtrArray *spell_suggest_more_actions; /* EUIAction; to fill a GMenu */
	GPtrArray *spell_add_actions; /* EUIAction, to fill a GMenu */
	GHashTable *spell_suggest_menus_by_code; /* gchar *dictionary-code ~> EHTMLEditorActionMenuPair * */

	EUIMenu *main_menu;
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

	GtkWidget *scrolled_window;

	GtkWidget *emoji_chooser;
	GtkWidget *emoticon_chooser;

	GHashTable *cid_parts; /* gchar *cid: URI ~> CamelMimePart * */
	GHashTable *content_editors; /* gchar *name ~> EContentEditor * */
	GHashTable *content_editors_for_mode; /* EContentEditorMode ~> EContentEditor *; pointers borrowed from content_editors */
	EContentEditor *use_content_editor;
	GCancellable *mode_change_content_cancellable;

	gchar *filename;
	GSList *content_editor_bindings; /* reffed GBinding-s related to the EContentEditor */
	gulong subscript_notify_id;
	gulong superscript_notify_id;

	gint editor_layout_row;

	gboolean paste_plain_prefer_pre;

	gchar *context_hover_uri;
};

void		e_html_editor_actions_add_actions
						(EHTMLEditor *editor);
void		e_html_editor_actions_setup_actions
						(EHTMLEditor *editor);
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
void		e_html_editor_emit_after_mode_changed
						(EHTMLEditor *self);
void		e_html_editor_zoom_in		(EHTMLEditor *editor,
						 EContentEditor *cnt_editor);
void		e_html_editor_zoom_out		(EHTMLEditor *editor,
						 EContentEditor *cnt_editor);

G_END_DECLS

#endif /* E_HTML_EDITOR_PRIVATE_H */
