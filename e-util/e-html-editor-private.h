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

#include <e-action-combo-box.h>
#include <e-color-combo.h>
#include <e-html-editor.h>
#include <e-html-editor-actions.h>
#include <e-html-editor-cell-dialog.h>
#include <e-html-editor-find-dialog.h>
#include <e-html-editor-hrule-dialog.h>
#include <e-html-editor-view.h>
#include <e-editor-replace-dialog.h>
#include <e-editor-link-dialog.h>
#include <e-editor-table-dialog.h>
#include <e-editor-page-dialog.h>
#include <e-editor-image-dialog.h>
#include <e-editor-text-dialog.h>
#include <e-editor-paragraph-dialog.h>
#include <e-editor-spell-check-dialog.h>

#ifdef HAVE_XFREE
#include <X11/XF86keysym.h>
#endif

#define ACTION(name) (E_HTML_EDITOR_ACTION_##name (editor))
#define WIDGET(name) (E_EDITOR_WIDGETS_##name (editor))

G_BEGIN_DECLS

struct _EHTMLEditorPrivate {
	GtkUIManager *manager;
	GtkActionGroup *core_actions;
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

	GtkWidget *find_dialog;
	GtkWidget *replace_dialog;
	GtkWidget *link_dialog;
	GtkWidget *hrule_dialog;
	GtkWidget *table_dialog;
	GtkWidget *page_dialog;
	GtkWidget *image_dialog;
	GtkWidget *text_dialog;
	GtkWidget *paragraph_dialog;
	GtkWidget *cell_dialog;
	GtkWidget *spell_check_dialog;

	GtkWidget *color_combo_box;
	GtkWidget *mode_combo_box;
	GtkWidget *size_combo_box;
	GtkWidget *style_combo_box;
	GtkWidget *scrolled_window;

	EHTMLEditorView *html_editor_view;
	EHTMLEditorSelection *selection;

	gchar *filename;

	guint spell_suggestions_merge_id;

	WebKitDOMNode *image;
	WebKitDOMNode *table_cell;

	gint editor_layout_row;
};

void		editor_actions_init		(EHTMLEditor *editor);

G_END_DECLS

#endif /* E_HTML_EDITOR_PRIVATE_H */
