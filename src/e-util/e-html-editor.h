/*
 * e-html-editor.h
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_HTML_EDITOR_H
#define E_HTML_EDITOR_H

#include <gtk/gtk.h>
#include <e-util/e-action-combo-box.h>
#include <e-util/e-activity.h>
#include <e-util/e-activity-bar.h>
#include <e-util/e-alert-bar.h>
#include <e-util/e-content-editor.h>
#include <e-util/e-focus-tracker.h>
#include <e-util/e-ui-action.h>
#include <e-util/e-ui-manager.h>
#include <e-util/e-ui-menu.h>

/* Standard GObject macros */
#define E_TYPE_HTML_EDITOR \
	(e_html_editor_get_type ())
#define E_HTML_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_HTML_EDITOR, EHTMLEditor))
#define E_HTML_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_HTML_EDITOR, EHTMLEditorClass))
#define E_IS_HTML_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_HTML_EDITOR))
#define E_IS_HTML_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_HTML_EDITOR))
#define E_HTML_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_HTML_EDITOR, EHTMLEditorClass))

#define E_HTML_EDITOR_MAX_INDENT_LEVEL 10

#define E_HTML_EDITOR_UI_OBJECT_MAIN_MENU "main-menu"
#define E_HTML_EDITOR_UI_OBJECT_MAIN_TOOLBAR "main-toolbar"

G_BEGIN_DECLS

typedef struct _EHTMLEditor EHTMLEditor;
typedef struct _EHTMLEditorClass EHTMLEditorClass;
typedef struct _EHTMLEditorPrivate EHTMLEditorPrivate;

struct _EHTMLEditor {
	GtkGrid parent;
	EHTMLEditorPrivate *priv;
};

struct _EHTMLEditorClass {
	GtkGridClass parent_class;

	void		(*update_actions)	(EHTMLEditor *editor,
						 EContentEditorNodeFlags flags,
						 const gchar *caret_word,
						 const gchar *hover_uri);

	void		(*spell_languages_changed)
						(EHTMLEditor *editor);
};

GType		e_html_editor_get_type		(void) G_GNUC_CONST;
void		e_html_editor_new		(GAsyncReadyCallback callback,
						 gpointer user_data);
GtkWidget *	e_html_editor_new_finish	(GAsyncResult *result,
						 GError **error);
void		e_html_editor_connect_focus_tracker
						(EHTMLEditor *editor,
						 EFocusTracker *focus_tracker);
GtkWidget *	e_html_editor_get_content_box	(EHTMLEditor *editor);
EContentEditor *
		e_html_editor_get_content_editor
						(EHTMLEditor *editor);
void		e_html_editor_register_content_editor
						(EHTMLEditor *editor,
						 const gchar *name,
						 EContentEditor *cnt_editor);
EContentEditorMode
		e_html_editor_get_mode		(EHTMLEditor *editor);
void		e_html_editor_set_mode		(EHTMLEditor *editor,
						 EContentEditorMode mode);
void		e_html_editor_cancel_mode_change_content_update
						(EHTMLEditor *editor);
EUIManager *	e_html_editor_get_ui_manager	(EHTMLEditor *editor);
EUIAction *	e_html_editor_get_action	(EHTMLEditor *editor,
						 const gchar *action_name);
gpointer	e_html_editor_get_ui_object	(EHTMLEditor *editor,
						 const gchar *object_name);
const gchar *	e_html_editor_get_filename	(EHTMLEditor *editor);
void		e_html_editor_set_filename	(EHTMLEditor *editor,
						 const gchar *filename);
EActivityBar *	e_html_editor_get_activity_bar	(EHTMLEditor *editor);
EActivity *	e_html_editor_new_activity	(EHTMLEditor *editor);
void		e_html_editor_pack_above	(EHTMLEditor *editor,
						 GtkWidget *child);
void		e_html_editor_update_spell_actions
						(EHTMLEditor *editor);
void		e_html_editor_save		(EHTMLEditor *editor,
						 const gchar *filename,
						 gboolean as_html,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_html_editor_save_finish	(EHTMLEditor *editor,
						 GAsyncResult *result,
						 GError **error);
void		e_html_editor_add_cid_part	(EHTMLEditor *editor,
						 CamelMimePart *mime_part);
void		e_html_editor_remove_cid_part	(EHTMLEditor *editor,
						 const gchar *cid_uri);
void		e_html_editor_remove_unused_cid_parts
						(EHTMLEditor *editor,
						 GSList *used_mime_parts, /*  CamelMimePart * */
						 GSList **out_removed_mime_parts); /*  CamelMimePart * */
void		e_html_editor_remove_all_cid_parts
						(EHTMLEditor *editor);
CamelMimePart * e_html_editor_ref_cid_part	(EHTMLEditor *editor,
						 const gchar *cid_uri);
void		e_html_editor_clear_alerts	(EHTMLEditor *editor);
EAlertBar *	e_html_editor_get_alert_bar	(EHTMLEditor *editor);
EActionComboBox *
		e_html_editor_util_new_mode_combobox
						(void);
G_END_DECLS

#endif /* E_HTML_EDITOR_H */
