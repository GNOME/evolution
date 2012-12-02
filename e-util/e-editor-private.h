/*
 * e-editor-private.h
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

#ifndef E_EDITOR_PRIVATE_H
#define E_EDITOR_PRIVATE_H

#include <e-editor.h>
#include <e-action-combo-box.h>
#include <e-color-combo.h>
#include <e-editor-actions.h>
#include <e-editor-widget.h>
#include <e-editor-find-dialog.h>
#include <e-editor-replace-dialog.h>
#include <e-editor-link-dialog.h>
#include <e-editor-hrule-dialog.h>
#include <e-editor-table-dialog.h>
#include <e-editor-page-dialog.h>
#include <e-editor-image-dialog.h>
#include <e-editor-text-dialog.h>
#include <e-editor-paragraph-dialog.h>
#include <e-editor-cell-dialog.h>
#include <e-editor-spell-check-dialog.h>

#ifdef HAVE_XFREE
#include <X11/XF86keysym.h>
#endif


#define ACTION(name) (E_EDITOR_ACTION_##name (editor))
#define WIDGET(name) (E_EDITOR_WIDGETS_##name (editor))

G_BEGIN_DECLS

struct _EEditorPrivate {
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

	EEditorWidget *editor_widget;
	EEditorSelection *selection;

	gchar *filename;

	guint spell_suggestions_merge_id;
	GList *active_dictionaries;

	WebKitDOMNode *image;
	WebKitDOMNode *table_cell;
};

void		editor_actions_init		(EEditor *editor);

G_END_DECLS

#endif /* E_EDITOR_PRIVATE_H */
