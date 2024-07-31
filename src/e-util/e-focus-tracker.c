/*
 * e-focus-tracker.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-content-editor.h"
#include "e-selectable.h"
#include "e-ui-action.h"
#include "e-widget-undo.h"

#include "e-focus-tracker.h"

#define disconnect_and_unref(_x) G_STMT_START { \
	if (_x != NULL) { \
		g_signal_handlers_disconnect_matched ( \
			_x, G_SIGNAL_MATCH_DATA, \
			0, 0, NULL, NULL, focus_tracker); \
		g_clear_object (&(_x)); \
	} \
} G_STMT_END

#define replace_action(_priv_member, _arg, _cb, _prop_name) G_STMT_START { \
	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker)); \
	if (_arg != NULL) { \
		g_return_if_fail (E_IS_UI_ACTION (_arg)); \
		g_object_ref (_arg); \
	} \
	disconnect_and_unref (_priv_member); \
	_priv_member = _arg; \
	if (_arg != NULL) { \
		g_signal_connect_swapped ( \
			_arg, "activate", \
			G_CALLBACK (_cb), \
			focus_tracker); \
	} \
	g_object_notify (G_OBJECT (focus_tracker), _prop_name); \
} G_STMT_END

struct _EFocusTrackerPrivate {
	GtkWidget *focus;  /* not referenced */
	GtkWindow *window;

	EUIAction *cut_clipboard;
	EUIAction *copy_clipboard;
	EUIAction *paste_clipboard;
	EUIAction *delete_selection;
	EUIAction *select_all;
	EUIAction *undo;
	EUIAction *redo;
};

enum {
	PROP_0,
	PROP_FOCUS,
	PROP_WINDOW,
	PROP_CUT_CLIPBOARD_ACTION,
	PROP_COPY_CLIPBOARD_ACTION,
	PROP_PASTE_CLIPBOARD_ACTION,
	PROP_DELETE_SELECTION_ACTION,
	PROP_SELECT_ALL_ACTION,
	PROP_UNDO_ACTION,
	PROP_REDO_ACTION
};

G_DEFINE_TYPE_WITH_PRIVATE (EFocusTracker, e_focus_tracker, G_TYPE_OBJECT)

static void
focus_tracker_disable_actions (EFocusTracker *focus_tracker)
{
	EUIAction *action;

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	if (action != NULL)
		e_ui_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	if (action != NULL)
		e_ui_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	if (action != NULL)
		e_ui_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_delete_selection_action (focus_tracker);
	if (action != NULL)
		e_ui_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	if (action != NULL)
		e_ui_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_undo_action (focus_tracker);
	if (action != NULL)
		e_ui_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_redo_action (focus_tracker);
	if (action != NULL)
		e_ui_action_set_sensitive (action, FALSE);
}

static gboolean
focus_tracker_get_has_undo (GtkWidget *widget)
{
	if (!widget)
		return FALSE;

	if (e_widget_undo_is_attached (widget))
		return e_widget_undo_has_undo (widget);

	if (E_IS_CONTENT_EDITOR (widget))
		return e_content_editor_can_undo (E_CONTENT_EDITOR (widget));

	return FALSE;
}

static gboolean
focus_tracker_get_has_redo (GtkWidget *widget)
{
	if (!widget)
		return FALSE;

	if (e_widget_undo_is_attached (widget))
		return e_widget_undo_has_redo (widget);

	if (E_IS_CONTENT_EDITOR (widget))
		return e_content_editor_can_redo (E_CONTENT_EDITOR (widget));

	return FALSE;
}

static void
focus_tracker_update_undo_redo (EFocusTracker *focus_tracker,
                                GtkWidget *widget,
                                gboolean can_edit_text)
{
	EUIAction *action;
	gboolean sensitive;

	action = e_focus_tracker_get_undo_action (focus_tracker);
	if (action != NULL) {
		sensitive = can_edit_text && focus_tracker_get_has_undo (widget);
		e_ui_action_set_sensitive (action, sensitive);

		if (sensitive) {
			gchar *description;

			description = e_widget_undo_describe_undo (widget);
			e_ui_action_set_tooltip (action, description && *description ? description : _("Undo"));
			g_free (description);
		} else {
			e_ui_action_set_tooltip (action, _("Undo"));
		}
	}

	action = e_focus_tracker_get_redo_action (focus_tracker);
	if (action != NULL) {
		sensitive = can_edit_text && focus_tracker_get_has_redo (widget);
		e_ui_action_set_sensitive (action, sensitive);

		if (sensitive) {
			gchar *description;

			description = e_widget_undo_describe_redo (widget);
			e_ui_action_set_tooltip (action, description && *description ? description : _("Redo"));
			g_free (description);
		} else {
			e_ui_action_set_tooltip (action, _("Redo"));
		}
	}
}

static void
focus_tracker_editable_update_actions (EFocusTracker *focus_tracker,
                                       GtkEditable *editable,
                                       GdkAtom *targets,
                                       gint n_targets)
{
	EUIAction *action;
	gboolean can_edit_text;
	gboolean clipboard_has_text;
	gboolean text_is_selected;
	gboolean sensitive;

	can_edit_text =
		gtk_editable_get_editable (editable);

	clipboard_has_text = (targets != NULL) &&
		gtk_targets_include_text (targets, n_targets);

	text_is_selected =
		gtk_editable_get_selection_bounds (editable, NULL, NULL);

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	if (action != NULL) {
		sensitive = can_edit_text && text_is_selected;
		e_ui_action_set_sensitive (action, sensitive);
		e_ui_action_set_tooltip (action, _("Cut the selection"));
	}

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	if (action != NULL) {
		sensitive = text_is_selected;
		e_ui_action_set_sensitive (action, sensitive);
		e_ui_action_set_tooltip (action, _("Copy the selection"));
	}

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	if (action != NULL) {
		sensitive = can_edit_text && clipboard_has_text;
		e_ui_action_set_sensitive (action, sensitive);
		e_ui_action_set_tooltip (action, _("Paste the clipboard"));
	}

	action = e_focus_tracker_get_delete_selection_action (focus_tracker);
	if (action != NULL) {
		sensitive = can_edit_text && text_is_selected;
		e_ui_action_set_sensitive (action, sensitive);
		e_ui_action_set_tooltip (action, _("Delete the selection"));
	}

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	if (action != NULL) {
		sensitive = TRUE;  /* always enabled */
		e_ui_action_set_sensitive (action, sensitive);
		e_ui_action_set_tooltip (action, _("Select all text"));
	}

	focus_tracker_update_undo_redo (focus_tracker, GTK_WIDGET (editable), can_edit_text);
}

static void
focus_tracker_text_view_update_actions (EFocusTracker *focus_tracker,
                                        GtkTextView *text_view,
                                        GdkAtom *targets,
                                        gint n_targets)
{
	EUIAction *action;
	GtkTextBuffer *buffer;
	gboolean can_edit_text;
	gboolean clipboard_has_text;
	gboolean text_is_selected;
	gboolean sensitive;

	buffer = gtk_text_view_get_buffer (text_view);
	can_edit_text = gtk_text_view_get_editable (text_view);
	clipboard_has_text = (targets != NULL) && gtk_targets_include_text (targets, n_targets);
	text_is_selected = gtk_text_buffer_get_selection_bounds (buffer, NULL, NULL);

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	if (action != NULL) {
		sensitive = can_edit_text && text_is_selected;
		e_ui_action_set_sensitive (action, sensitive);
		e_ui_action_set_tooltip (action, _("Cut the selection"));
	}

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	if (action != NULL) {
		sensitive = text_is_selected;
		e_ui_action_set_sensitive (action, sensitive);
		e_ui_action_set_tooltip (action, _("Copy the selection"));
	}

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	if (action != NULL) {
		sensitive = can_edit_text && clipboard_has_text;
		e_ui_action_set_sensitive (action, sensitive);
		e_ui_action_set_tooltip (action, _("Paste the clipboard"));
	}

	action = e_focus_tracker_get_delete_selection_action (focus_tracker);
	if (action != NULL) {
		sensitive = can_edit_text && text_is_selected;
		e_ui_action_set_sensitive (action, sensitive);
		e_ui_action_set_tooltip (action, _("Delete the selection"));
	}

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	if (action != NULL) {
		sensitive = TRUE;  /* always enabled */
		e_ui_action_set_sensitive (action, sensitive);
		e_ui_action_set_tooltip (action, _("Select all text"));
	}

	focus_tracker_update_undo_redo (focus_tracker, GTK_WIDGET (text_view), can_edit_text);
}

static void
focus_tracker_editor_update_actions (EFocusTracker *focus_tracker,
                                     EContentEditor *cnt_editor,
                                     GdkAtom *targets,
                                     gint n_targets)
{
	EUIAction *action;
	gboolean can_copy;
	gboolean can_cut;
	gboolean can_paste;

	g_object_get (cnt_editor,
		      "can-copy", &can_copy,
		      "can-cut", &can_cut,
		      "can-paste", &can_paste,
		      NULL);

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	if (action != NULL) {
		e_ui_action_set_sensitive (action, can_cut);
		e_ui_action_set_tooltip (action, _("Cut the selection"));
	}

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	if (action != NULL) {
		e_ui_action_set_sensitive (action, can_copy);
		e_ui_action_set_tooltip (action, _("Copy the selection"));
	}

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	if (action != NULL) {
		e_ui_action_set_sensitive (action, can_paste);
		e_ui_action_set_tooltip (action, _("Paste the clipboard"));
	}

	focus_tracker_update_undo_redo (focus_tracker, GTK_WIDGET (cnt_editor),
		e_content_editor_is_editable (cnt_editor));
}

static void
focus_tracker_selectable_update_actions (EFocusTracker *focus_tracker,
                                         ESelectable *selectable,
                                         GdkAtom *targets,
                                         gint n_targets)
{
	ESelectableInterface *iface;
	EUIAction *action;

	iface = E_SELECTABLE_GET_INTERFACE (selectable);

	e_selectable_update_actions (
		selectable, focus_tracker, targets, n_targets);

	/* Disable actions for which the corresponding method is not
	 * implemented.  This allows update_actions() implementations
	 * to simply skip the actions they don't support, which in turn
	 * allows us to add new actions without disturbing the existing
	 * ESelectable implementations. */

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	if (action != NULL && iface->cut_clipboard == NULL)
		e_ui_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	if (action != NULL && iface->copy_clipboard == NULL)
		e_ui_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	if (action != NULL && iface->paste_clipboard == NULL)
		e_ui_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_delete_selection_action (focus_tracker);
	if (action != NULL && iface->delete_selection == NULL)
		e_ui_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	if (action != NULL && iface->select_all == NULL)
		e_ui_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_undo_action (focus_tracker);
	if (action != NULL && iface->undo == NULL)
		e_ui_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_redo_action (focus_tracker);
	if (action != NULL && iface->redo == NULL)
		e_ui_action_set_sensitive (action, FALSE);
}

static void
focus_tracker_targets_received_cb (GtkClipboard *clipboard,
                                   GdkAtom *targets,
                                   gint n_targets,
                                   EFocusTracker *focus_tracker)
{
	GtkWidget *focus;

	focus = e_focus_tracker_get_focus (focus_tracker);

	if (focus == NULL)
		focus_tracker_disable_actions (focus_tracker);

	else if (E_IS_SELECTABLE (focus))
		focus_tracker_selectable_update_actions (
			focus_tracker, E_SELECTABLE (focus),
			targets, n_targets);

	else if (GTK_IS_EDITABLE (focus))
		focus_tracker_editable_update_actions (
			focus_tracker, GTK_EDITABLE (focus),
			targets, n_targets);

	else {
		GtkWidget *ancestor = gtk_widget_get_ancestor (focus, E_TYPE_CONTENT_EDITOR);

		if (E_IS_CONTENT_EDITOR (ancestor))
			focus_tracker_editor_update_actions (
				focus_tracker, E_CONTENT_EDITOR (ancestor),
				targets, n_targets);

		else if (GTK_IS_TEXT_VIEW (focus))
			focus_tracker_text_view_update_actions (
				focus_tracker, GTK_TEXT_VIEW (focus),
				targets, n_targets);

		else if (E_IS_CONTENT_EDITOR (focus))
			focus_tracker_editor_update_actions (
				focus_tracker, E_CONTENT_EDITOR (focus),
				targets, n_targets);
	}

	g_object_unref (focus_tracker);
}

static void
focus_tracker_set_focus_cb (GtkWindow *window,
                            GtkWidget *focus,
                            EFocusTracker *focus_tracker)
{
	while (focus != NULL) {
		if (E_IS_SELECTABLE (focus))
			break;

		if (GTK_IS_EDITABLE (focus))
			break;

		if (GTK_IS_TEXT_VIEW (focus))
			break;

		if (E_IS_CONTENT_EDITOR (focus))
			break;

		focus = gtk_widget_get_parent (focus);
	}

	if (focus == focus_tracker->priv->focus)
		return;

	focus_tracker->priv->focus = focus;
	g_object_notify (G_OBJECT (focus_tracker), "focus");

	e_focus_tracker_update_actions (focus_tracker);
}

static void
focus_tracker_set_window (EFocusTracker *focus_tracker,
                          GtkWindow *window)
{
	g_return_if_fail (GTK_IS_WINDOW (window));
	g_return_if_fail (focus_tracker->priv->window == NULL);

	focus_tracker->priv->window = g_object_ref (window);

	g_signal_connect (
		window, "set-focus",
		G_CALLBACK (focus_tracker_set_focus_cb), focus_tracker);
}

static void
focus_tracker_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WINDOW:
			focus_tracker_set_window (
				E_FOCUS_TRACKER (object),
				g_value_get_object (value));
			return;

		case PROP_CUT_CLIPBOARD_ACTION:
			e_focus_tracker_set_cut_clipboard_action (
				E_FOCUS_TRACKER (object),
				g_value_get_object (value));
			return;

		case PROP_COPY_CLIPBOARD_ACTION:
			e_focus_tracker_set_copy_clipboard_action (
				E_FOCUS_TRACKER (object),
				g_value_get_object (value));
			return;

		case PROP_PASTE_CLIPBOARD_ACTION:
			e_focus_tracker_set_paste_clipboard_action (
				E_FOCUS_TRACKER (object),
				g_value_get_object (value));
			return;

		case PROP_DELETE_SELECTION_ACTION:
			e_focus_tracker_set_delete_selection_action (
				E_FOCUS_TRACKER (object),
				g_value_get_object (value));
			return;

		case PROP_SELECT_ALL_ACTION:
			e_focus_tracker_set_select_all_action (
				E_FOCUS_TRACKER (object),
				g_value_get_object (value));
			return;

		case PROP_UNDO_ACTION:
			e_focus_tracker_set_undo_action (
				E_FOCUS_TRACKER (object),
				g_value_get_object (value));
			return;

		case PROP_REDO_ACTION:
			e_focus_tracker_set_redo_action (
				E_FOCUS_TRACKER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
focus_tracker_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FOCUS:
			g_value_set_object (
				value,
				e_focus_tracker_get_focus (
				E_FOCUS_TRACKER (object)));
			return;

		case PROP_WINDOW:
			g_value_set_object (
				value,
				e_focus_tracker_get_window (
				E_FOCUS_TRACKER (object)));
			return;

		case PROP_CUT_CLIPBOARD_ACTION:
			g_value_set_object (
				value,
				e_focus_tracker_get_cut_clipboard_action (
				E_FOCUS_TRACKER (object)));
			return;

		case PROP_COPY_CLIPBOARD_ACTION:
			g_value_set_object (
				value,
				e_focus_tracker_get_copy_clipboard_action (
				E_FOCUS_TRACKER (object)));
			return;

		case PROP_PASTE_CLIPBOARD_ACTION:
			g_value_set_object (
				value,
				e_focus_tracker_get_paste_clipboard_action (
				E_FOCUS_TRACKER (object)));
			return;

		case PROP_DELETE_SELECTION_ACTION:
			g_value_set_object (
				value,
				e_focus_tracker_get_delete_selection_action (
				E_FOCUS_TRACKER (object)));
			return;

		case PROP_SELECT_ALL_ACTION:
			g_value_set_object (
				value,
				e_focus_tracker_get_select_all_action (
				E_FOCUS_TRACKER (object)));
			return;

		case PROP_UNDO_ACTION:
			g_value_set_object (
				value,
				e_focus_tracker_get_undo_action (
				E_FOCUS_TRACKER (object)));
			return;

		case PROP_REDO_ACTION:
			g_value_set_object (
				value,
				e_focus_tracker_get_redo_action (
				E_FOCUS_TRACKER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
focus_tracker_dispose (GObject *object)
{
	EFocusTracker *focus_tracker = E_FOCUS_TRACKER (object);

	g_signal_handlers_disconnect_matched (
		gtk_clipboard_get (GDK_SELECTION_PRIMARY),
		G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, focus_tracker);

	g_signal_handlers_disconnect_matched (
		gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
		G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, focus_tracker);

	disconnect_and_unref (focus_tracker->priv->window);
	disconnect_and_unref (focus_tracker->priv->cut_clipboard);
	disconnect_and_unref (focus_tracker->priv->copy_clipboard);
	disconnect_and_unref (focus_tracker->priv->paste_clipboard);
	disconnect_and_unref (focus_tracker->priv->delete_selection);
	disconnect_and_unref (focus_tracker->priv->select_all);
	disconnect_and_unref (focus_tracker->priv->undo);
	disconnect_and_unref (focus_tracker->priv->redo);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_focus_tracker_parent_class)->dispose (object);
}

static void
focus_tracker_constructed (GObject *object)
{
	GtkClipboard *clipboard;

	/* Listen for "owner-change" signals from the primary selection
	 * clipboard to learn when text selections change in GtkEditable
	 * widgets.  It's a bit of an overkill, but I don't know of any
	 * other notification mechanism. */

	clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);

	g_signal_connect_swapped (
		clipboard, "owner-change",
		G_CALLBACK (e_focus_tracker_update_actions), object);

	/* Listen for "owner-change" signals from the default clipboard
	 * so we can update the paste action when the user cuts or copies
	 * something.  This is how GEdit does it. */

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	g_signal_connect_swapped (
		clipboard, "owner-change",
		G_CALLBACK (e_focus_tracker_update_actions), object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_focus_tracker_parent_class)->constructed (object);
}

static void
e_focus_tracker_class_init (EFocusTrackerClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = focus_tracker_set_property;
	object_class->get_property = focus_tracker_get_property;
	object_class->dispose = focus_tracker_dispose;
	object_class->constructed = focus_tracker_constructed;

	g_object_class_install_property (
		object_class,
		PROP_FOCUS,
		g_param_spec_object (
			"focus",
			"Focus",
			NULL,
			GTK_TYPE_WIDGET,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_WINDOW,
		g_param_spec_object (
			"window",
			"Window",
			NULL,
			GTK_TYPE_WINDOW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_CUT_CLIPBOARD_ACTION,
		g_param_spec_object (
			"cut-clipboard-action",
			"Cut Clipboard Action",
			NULL,
			E_TYPE_UI_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_COPY_CLIPBOARD_ACTION,
		g_param_spec_object (
			"copy-clipboard-action",
			"Copy Clipboard Action",
			NULL,
			E_TYPE_UI_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_PASTE_CLIPBOARD_ACTION,
		g_param_spec_object (
			"paste-clipboard-action",
			"Paste Clipboard Action",
			NULL,
			E_TYPE_UI_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DELETE_SELECTION_ACTION,
		g_param_spec_object (
			"delete-selection-action",
			"Delete Selection Action",
			NULL,
			E_TYPE_UI_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SELECT_ALL_ACTION,
		g_param_spec_object (
			"select-all-action",
			"Select All Action",
			NULL,
			E_TYPE_UI_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_UNDO_ACTION,
		g_param_spec_object (
			"undo-action",
			"Undo Action",
			NULL,
			E_TYPE_UI_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_REDO_ACTION,
		g_param_spec_object (
			"redo-action",
			"Redo Action",
			NULL,
			E_TYPE_UI_ACTION,
			G_PARAM_READWRITE));
}

static void
e_focus_tracker_init (EFocusTracker *focus_tracker)
{
	focus_tracker->priv = e_focus_tracker_get_instance_private (focus_tracker);
}

EFocusTracker *
e_focus_tracker_new (GtkWindow *window)
{
	g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

	return g_object_new (E_TYPE_FOCUS_TRACKER, "window", window, NULL);
}

GtkWidget *
e_focus_tracker_get_focus (EFocusTracker *focus_tracker)
{
	g_return_val_if_fail (E_IS_FOCUS_TRACKER (focus_tracker), NULL);

	return focus_tracker->priv->focus;
}

GtkWindow *
e_focus_tracker_get_window (EFocusTracker *focus_tracker)
{
	g_return_val_if_fail (E_IS_FOCUS_TRACKER (focus_tracker), NULL);

	return focus_tracker->priv->window;
}

EUIAction *
e_focus_tracker_get_cut_clipboard_action (EFocusTracker *focus_tracker)
{
	g_return_val_if_fail (E_IS_FOCUS_TRACKER (focus_tracker), NULL);

	return focus_tracker->priv->cut_clipboard;
}

void
e_focus_tracker_set_cut_clipboard_action (EFocusTracker *focus_tracker,
                                          EUIAction *cut_clipboard)
{
	replace_action (focus_tracker->priv->cut_clipboard, cut_clipboard, e_focus_tracker_cut_clipboard, "cut-clipboard-action");
}

EUIAction *
e_focus_tracker_get_copy_clipboard_action (EFocusTracker *focus_tracker)
{
	g_return_val_if_fail (E_IS_FOCUS_TRACKER (focus_tracker), NULL);

	return focus_tracker->priv->copy_clipboard;
}

void
e_focus_tracker_set_copy_clipboard_action (EFocusTracker *focus_tracker,
                                           EUIAction *copy_clipboard)
{
	replace_action (focus_tracker->priv->copy_clipboard, copy_clipboard, e_focus_tracker_copy_clipboard, "copy-clipboard-action");
}

EUIAction *
e_focus_tracker_get_paste_clipboard_action (EFocusTracker *focus_tracker)
{
	g_return_val_if_fail (E_IS_FOCUS_TRACKER (focus_tracker), NULL);

	return focus_tracker->priv->paste_clipboard;
}

void
e_focus_tracker_set_paste_clipboard_action (EFocusTracker *focus_tracker,
                                            EUIAction *paste_clipboard)
{
	replace_action (focus_tracker->priv->paste_clipboard, paste_clipboard, e_focus_tracker_paste_clipboard, "paste-clipboard-action");
}

EUIAction *
e_focus_tracker_get_delete_selection_action (EFocusTracker *focus_tracker)
{
	g_return_val_if_fail (E_IS_FOCUS_TRACKER (focus_tracker), NULL);

	return focus_tracker->priv->delete_selection;
}

void
e_focus_tracker_set_delete_selection_action (EFocusTracker *focus_tracker,
                                             EUIAction *delete_selection)
{
	replace_action (focus_tracker->priv->delete_selection, delete_selection, e_focus_tracker_delete_selection, "delete-selection-action");
}

EUIAction *
e_focus_tracker_get_select_all_action (EFocusTracker *focus_tracker)
{
	g_return_val_if_fail (E_IS_FOCUS_TRACKER (focus_tracker), NULL);

	return focus_tracker->priv->select_all;
}

void
e_focus_tracker_set_select_all_action (EFocusTracker *focus_tracker,
                                       EUIAction *select_all)
{
	replace_action (focus_tracker->priv->select_all, select_all, e_focus_tracker_select_all, "select-all-action");
}

EUIAction *
e_focus_tracker_get_undo_action (EFocusTracker *focus_tracker)
{
	g_return_val_if_fail (E_IS_FOCUS_TRACKER (focus_tracker), NULL);

	return focus_tracker->priv->undo;
}

void
e_focus_tracker_set_undo_action (EFocusTracker *focus_tracker,
                                 EUIAction *undo)
{
	replace_action (focus_tracker->priv->undo, undo, e_focus_tracker_undo, "undo-action");
}

EUIAction *
e_focus_tracker_get_redo_action (EFocusTracker *focus_tracker)
{
	g_return_val_if_fail (E_IS_FOCUS_TRACKER (focus_tracker), NULL);

	return focus_tracker->priv->redo;
}

void
e_focus_tracker_set_redo_action (EFocusTracker *focus_tracker,
                                 EUIAction *redo)
{
	replace_action (focus_tracker->priv->redo, redo, e_focus_tracker_redo, "redo-action");
}

void
e_focus_tracker_update_actions (EFocusTracker *focus_tracker)
{
	GtkClipboard *clipboard;

	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	/* Request clipboard targets asynchronously. */

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	gtk_clipboard_request_targets (
		clipboard, (GtkClipboardTargetsReceivedFunc)
		focus_tracker_targets_received_cb,
		g_object_ref (focus_tracker));
}

void
e_focus_tracker_cut_clipboard (EFocusTracker *focus_tracker)
{
	GtkWidget *focus;

	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	focus = e_focus_tracker_get_focus (focus_tracker);

	if (E_IS_SELECTABLE (focus)) {
		e_selectable_cut_clipboard (E_SELECTABLE (focus));

	} else if (GTK_IS_EDITABLE (focus)) {
		gtk_editable_cut_clipboard (GTK_EDITABLE (focus));

	} else if (GTK_IS_TEXT_VIEW (focus)) {
		GtkClipboard *clipboard;
		GtkTextView *text_view;
		GtkTextBuffer *buffer;
		gboolean is_editable;

		clipboard = gtk_widget_get_clipboard (
			focus, GDK_SELECTION_CLIPBOARD);

		text_view = GTK_TEXT_VIEW (focus);
		buffer = gtk_text_view_get_buffer (text_view);
		is_editable = gtk_text_view_get_editable (text_view);

		gtk_text_buffer_cut_clipboard (buffer, clipboard, is_editable);

	} else if (E_IS_CONTENT_EDITOR (focus)) {
		e_content_editor_cut (E_CONTENT_EDITOR (focus));
	}
}

void
e_focus_tracker_copy_clipboard (EFocusTracker *focus_tracker)
{
	GtkWidget *focus;

	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	focus = e_focus_tracker_get_focus (focus_tracker);

	if (E_IS_SELECTABLE (focus)) {
		e_selectable_copy_clipboard (E_SELECTABLE (focus));

	} else if (GTK_IS_EDITABLE (focus)) {
		gtk_editable_copy_clipboard (GTK_EDITABLE (focus));

	} else if (GTK_IS_TEXT_VIEW (focus)) {
		GtkClipboard *clipboard;
		GtkTextView *text_view;
		GtkTextBuffer *buffer;

		clipboard = gtk_widget_get_clipboard (
			focus, GDK_SELECTION_CLIPBOARD);

		text_view = GTK_TEXT_VIEW (focus);
		buffer = gtk_text_view_get_buffer (text_view);

		gtk_text_buffer_copy_clipboard (buffer, clipboard);

	} else if (E_IS_CONTENT_EDITOR (focus)) {
		e_content_editor_copy (E_CONTENT_EDITOR (focus));
	}
}

void
e_focus_tracker_paste_clipboard (EFocusTracker *focus_tracker)
{
	GtkWidget *focus;

	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	focus = e_focus_tracker_get_focus (focus_tracker);

	if (E_IS_SELECTABLE (focus)) {
		e_selectable_paste_clipboard (E_SELECTABLE (focus));

	} else if (GTK_IS_EDITABLE (focus)) {
		gtk_editable_paste_clipboard (GTK_EDITABLE (focus));

	} else if (GTK_IS_TEXT_VIEW (focus)) {
		GtkClipboard *clipboard;
		GtkTextView *text_view;
		GtkTextBuffer *buffer;
		gboolean is_editable;

		clipboard = gtk_widget_get_clipboard (
			focus, GDK_SELECTION_CLIPBOARD);

		text_view = GTK_TEXT_VIEW (focus);
		buffer = gtk_text_view_get_buffer (text_view);
		is_editable = gtk_text_view_get_editable (text_view);

		gtk_text_buffer_paste_clipboard (
			buffer, clipboard, NULL, is_editable);

	} else if (E_IS_CONTENT_EDITOR (focus)) {
		e_content_editor_paste (E_CONTENT_EDITOR (focus));
	}
}

void
e_focus_tracker_delete_selection (EFocusTracker *focus_tracker)
{
	GtkWidget *focus;

	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	focus = e_focus_tracker_get_focus (focus_tracker);

	if (E_IS_SELECTABLE (focus)) {
		e_selectable_delete_selection (E_SELECTABLE (focus));

	} else if (GTK_IS_EDITABLE (focus)) {
		gtk_editable_delete_selection (GTK_EDITABLE (focus));

	} else if (GTK_IS_TEXT_VIEW (focus)) {
		GtkTextView *text_view;
		GtkTextBuffer *buffer;
		gboolean is_editable;

		text_view = GTK_TEXT_VIEW (focus);
		buffer = gtk_text_view_get_buffer (text_view);
		is_editable = gtk_text_view_get_editable (text_view);

		gtk_text_buffer_delete_selection (buffer, TRUE, is_editable);
	}
}

void
e_focus_tracker_select_all (EFocusTracker *focus_tracker)
{
	GtkWidget *focus;

	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	focus = e_focus_tracker_get_focus (focus_tracker);

	if (E_IS_SELECTABLE (focus)) {
		e_selectable_select_all (E_SELECTABLE (focus));

	} else if (GTK_IS_EDITABLE (focus)) {
		gtk_editable_select_region (GTK_EDITABLE (focus), 0, -1);

	} else if (GTK_IS_TEXT_VIEW (focus)) {
		GtkTextView *text_view;
		GtkTextBuffer *buffer;
		GtkTextIter start, end;

		text_view = GTK_TEXT_VIEW (focus);
		buffer = gtk_text_view_get_buffer (text_view);

		gtk_text_buffer_get_bounds (buffer, &start, &end);
		gtk_text_buffer_select_range (buffer, &start, &end);

	} else if (E_IS_CONTENT_EDITOR (focus)) {
		e_content_editor_select_all (E_CONTENT_EDITOR (focus));
	}
}

void
e_focus_tracker_undo (EFocusTracker *focus_tracker)
{
	GtkWidget *focus;

	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	focus = e_focus_tracker_get_focus (focus_tracker);

	if (E_IS_SELECTABLE (focus))
		e_selectable_undo (E_SELECTABLE (focus));
	else
		e_widget_undo_do_undo (focus);
}

void
e_focus_tracker_redo (EFocusTracker *focus_tracker)
{
	GtkWidget *focus;

	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	focus = e_focus_tracker_get_focus (focus_tracker);

	if (E_IS_SELECTABLE (focus))
		e_selectable_redo (E_SELECTABLE (focus));
	else
		e_widget_undo_do_redo (focus);
}
