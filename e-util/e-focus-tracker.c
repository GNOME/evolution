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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-focus-tracker.h"

#include <glib/gi18n-lib.h>

#include "e-selectable.h"
#include "e-widget-undo.h"
#include "e-content-editor.h"

#define E_FOCUS_TRACKER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_FOCUS_TRACKER, EFocusTrackerPrivate))

struct _EFocusTrackerPrivate {
	GtkWidget *focus;  /* not referenced */
	GtkWindow *window;

	GtkAction *cut_clipboard;
	GtkAction *copy_clipboard;
	GtkAction *paste_clipboard;
	GtkAction *delete_selection;
	GtkAction *select_all;
	GtkAction *undo;
	GtkAction *redo;
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

G_DEFINE_TYPE (
	EFocusTracker,
	e_focus_tracker,
	G_TYPE_OBJECT)

static void
focus_tracker_disable_actions (EFocusTracker *focus_tracker)
{
	GtkAction *action;

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	if (action != NULL)
		gtk_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	if (action != NULL)
		gtk_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	if (action != NULL)
		gtk_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_delete_selection_action (focus_tracker);
	if (action != NULL)
		gtk_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	if (action != NULL)
		gtk_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_undo_action (focus_tracker);
	if (action != NULL)
		gtk_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_redo_action (focus_tracker);
	if (action != NULL)
		gtk_action_set_sensitive (action, FALSE);
}

static void
focus_tracker_update_undo_redo (EFocusTracker *focus_tracker,
                                GtkWidget *widget,
                                gboolean can_edit_text)
{
	GtkAction *action;
	gboolean sensitive;

	action = e_focus_tracker_get_undo_action (focus_tracker);
	if (action != NULL) {
		sensitive = can_edit_text && widget && e_widget_undo_has_undo (widget);
		gtk_action_set_sensitive (action, sensitive);

		if (sensitive) {
			gchar *description;

			description = e_widget_undo_describe_undo (widget);
			gtk_action_set_tooltip (action, description);
			g_free (description);
		} else {
			gtk_action_set_tooltip (action, _("Undo"));
		}
	}

	action = e_focus_tracker_get_redo_action (focus_tracker);
	if (action != NULL) {
		sensitive = can_edit_text && widget && e_widget_undo_has_redo (widget);
		gtk_action_set_sensitive (action, sensitive);

		if (sensitive) {
			gchar *description;

			description = e_widget_undo_describe_redo (widget);
			gtk_action_set_tooltip (action, description);
			g_free (description);
		} else {
			gtk_action_set_tooltip (action, _("Redo"));
		}
	}
}

static void
focus_tracker_editable_update_actions (EFocusTracker *focus_tracker,
                                       GtkEditable *editable,
                                       GdkAtom *targets,
                                       gint n_targets)
{
	GtkAction *action;
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
		gtk_action_set_sensitive (action, sensitive);
		gtk_action_set_tooltip (action, _("Cut the selection"));
	}

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	if (action != NULL) {
		sensitive = text_is_selected;
		gtk_action_set_sensitive (action, sensitive);
		gtk_action_set_tooltip (action, _("Copy the selection"));
	}

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	if (action != NULL) {
		sensitive = can_edit_text && clipboard_has_text;
		gtk_action_set_sensitive (action, sensitive);
		gtk_action_set_tooltip (action, _("Paste the clipboard"));
	}

	action = e_focus_tracker_get_delete_selection_action (focus_tracker);
	if (action != NULL) {
		sensitive = can_edit_text && text_is_selected;
		gtk_action_set_sensitive (action, sensitive);
		gtk_action_set_tooltip (action, _("Delete the selection"));
	}

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	if (action != NULL) {
		sensitive = TRUE;  /* always enabled */
		gtk_action_set_sensitive (action, sensitive);
		gtk_action_set_tooltip (action, _("Select all text"));
	}

	focus_tracker_update_undo_redo (focus_tracker, GTK_WIDGET (editable), can_edit_text);
}

static void
focus_tracker_text_view_update_actions (EFocusTracker *focus_tracker,
                                        GtkTextView *text_view,
                                        GdkAtom *targets,
                                        gint n_targets)
{
	GtkAction *action;
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
		gtk_action_set_sensitive (action, sensitive);
		gtk_action_set_tooltip (action, _("Cut the selection"));
	}

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	if (action != NULL) {
		sensitive = text_is_selected;
		gtk_action_set_sensitive (action, sensitive);
		gtk_action_set_tooltip (action, _("Copy the selection"));
	}

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	if (action != NULL) {
		sensitive = can_edit_text && clipboard_has_text;
		gtk_action_set_sensitive (action, sensitive);
		gtk_action_set_tooltip (action, _("Paste the clipboard"));
	}

	action = e_focus_tracker_get_delete_selection_action (focus_tracker);
	if (action != NULL) {
		sensitive = can_edit_text && text_is_selected;
		gtk_action_set_sensitive (action, sensitive);
		gtk_action_set_tooltip (action, _("Delete the selection"));
	}

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	if (action != NULL) {
		sensitive = TRUE;  /* always enabled */
		gtk_action_set_sensitive (action, sensitive);
		gtk_action_set_tooltip (action, _("Select all text"));
	}

	focus_tracker_update_undo_redo (focus_tracker, GTK_WIDGET (text_view), can_edit_text);
}

static void
focus_tracker_editor_update_actions (EFocusTracker *focus_tracker,
                                     EContentEditor *cnt_editor,
                                     GdkAtom *targets,
                                     gint n_targets)
{
	GtkAction *action;
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
		gtk_action_set_sensitive (action, can_cut);
		gtk_action_set_tooltip (action, _("Cut the selection"));
	}

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	if (action != NULL) {
		gtk_action_set_sensitive (action, can_copy);
		gtk_action_set_tooltip (action, _("Copy the selection"));
	}

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	if (action != NULL) {
		gtk_action_set_sensitive (action, can_paste);
		gtk_action_set_tooltip (action, _("Paste the clipboard"));
	}
}

static void
focus_tracker_selectable_update_actions (EFocusTracker *focus_tracker,
                                         ESelectable *selectable,
                                         GdkAtom *targets,
                                         gint n_targets)
{
	ESelectableInterface *iface;
	GtkAction *action;

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
		gtk_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	if (action != NULL && iface->copy_clipboard == NULL)
		gtk_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	if (action != NULL && iface->paste_clipboard == NULL)
		gtk_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_delete_selection_action (focus_tracker);
	if (action != NULL && iface->delete_selection == NULL)
		gtk_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	if (action != NULL && iface->select_all == NULL)
		gtk_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_undo_action (focus_tracker);
	if (action != NULL && iface->undo == NULL)
		gtk_action_set_sensitive (action, FALSE);

	action = e_focus_tracker_get_redo_action (focus_tracker);
	if (action != NULL && iface->redo == NULL)
		gtk_action_set_sensitive (action, FALSE);
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

	else if (GTK_IS_TEXT_VIEW (focus))
		focus_tracker_text_view_update_actions (
			focus_tracker, GTK_TEXT_VIEW (focus),
			targets, n_targets);

	else if (E_IS_CONTENT_EDITOR (focus))
		focus_tracker_editor_update_actions (
			focus_tracker, E_CONTENT_EDITOR (focus),
			targets, n_targets);

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
	EFocusTrackerPrivate *priv;

	priv = E_FOCUS_TRACKER_GET_PRIVATE (object);

	g_signal_handlers_disconnect_matched (
		gtk_clipboard_get (GDK_SELECTION_PRIMARY),
		G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, object);

	g_signal_handlers_disconnect_matched (
		gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
		G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, object);

	if (priv->window != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->window, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->window);
		priv->window = NULL;
	}

	if (priv->cut_clipboard != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->cut_clipboard, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->cut_clipboard);
		priv->cut_clipboard = NULL;
	}

	if (priv->copy_clipboard != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->copy_clipboard, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->copy_clipboard);
		priv->copy_clipboard = NULL;
	}

	if (priv->paste_clipboard != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->paste_clipboard, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->paste_clipboard);
		priv->paste_clipboard = NULL;
	}

	if (priv->delete_selection != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->delete_selection, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->delete_selection);
		priv->delete_selection = NULL;
	}

	if (priv->select_all != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->select_all, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->select_all);
		priv->select_all = NULL;
	}

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

	g_type_class_add_private (class, sizeof (EFocusTrackerPrivate));

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
			GTK_TYPE_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_COPY_CLIPBOARD_ACTION,
		g_param_spec_object (
			"copy-clipboard-action",
			"Copy Clipboard Action",
			NULL,
			GTK_TYPE_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_PASTE_CLIPBOARD_ACTION,
		g_param_spec_object (
			"paste-clipboard-action",
			"Paste Clipboard Action",
			NULL,
			GTK_TYPE_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DELETE_SELECTION_ACTION,
		g_param_spec_object (
			"delete-selection-action",
			"Delete Selection Action",
			NULL,
			GTK_TYPE_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SELECT_ALL_ACTION,
		g_param_spec_object (
			"select-all-action",
			"Select All Action",
			NULL,
			GTK_TYPE_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_UNDO_ACTION,
		g_param_spec_object (
			"undo-action",
			"Undo Action",
			NULL,
			GTK_TYPE_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_REDO_ACTION,
		g_param_spec_object (
			"redo-action",
			"Redo Action",
			NULL,
			GTK_TYPE_ACTION,
			G_PARAM_READWRITE));
}

static void
e_focus_tracker_init (EFocusTracker *focus_tracker)
{
	GtkAction *action;

	focus_tracker->priv = E_FOCUS_TRACKER_GET_PRIVATE (focus_tracker);

	/* Define dummy actions.  These will most likely be overridden,
	 * but for cases where they're not it ensures ESelectable objects
	 * will always get a valid GtkAction when they ask us for one. */

	action = gtk_action_new (
		"cut-clipboard", _("Cu_t"),
		_("Cut the selection"), "edit-cut");
	focus_tracker->priv->cut_clipboard = action;

	action = gtk_action_new (
		"copy-clipboard", _("_Copy"),
		_("Copy the selection"), "edit-copy");
	focus_tracker->priv->copy_clipboard = action;

	action = gtk_action_new (
		"paste-clipboard", _("_Paste"),
		_("Paste the clipboard"), "edit-paste");
	focus_tracker->priv->paste_clipboard = action;

	action = gtk_action_new (
		"delete-selection", _("_Delete"),
		_("Delete the selection"), "edit-delete");
	focus_tracker->priv->delete_selection = action;

	action = gtk_action_new (
		"select-all", _("Select _All"),
		_("Select all text"), "edit-select-all");
	focus_tracker->priv->select_all = action;
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

GtkAction *
e_focus_tracker_get_cut_clipboard_action (EFocusTracker *focus_tracker)
{
	g_return_val_if_fail (E_IS_FOCUS_TRACKER (focus_tracker), NULL);

	return focus_tracker->priv->cut_clipboard;
}

void
e_focus_tracker_set_cut_clipboard_action (EFocusTracker *focus_tracker,
                                          GtkAction *cut_clipboard)
{
	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	if (cut_clipboard != NULL) {
		g_return_if_fail (GTK_IS_ACTION (cut_clipboard));
		g_object_ref (cut_clipboard);
	}

	if (focus_tracker->priv->cut_clipboard != NULL) {
		g_signal_handlers_disconnect_matched (
			focus_tracker->priv->cut_clipboard,
			G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
			focus_tracker);
		g_object_unref (focus_tracker->priv->cut_clipboard);
	}

	focus_tracker->priv->cut_clipboard = cut_clipboard;

	if (cut_clipboard != NULL)
		g_signal_connect_swapped (
			cut_clipboard, "activate",
			G_CALLBACK (e_focus_tracker_cut_clipboard),
			focus_tracker);

	g_object_notify (G_OBJECT (focus_tracker), "cut-clipboard-action");
}

GtkAction *
e_focus_tracker_get_copy_clipboard_action (EFocusTracker *focus_tracker)
{
	g_return_val_if_fail (E_IS_FOCUS_TRACKER (focus_tracker), NULL);

	return focus_tracker->priv->copy_clipboard;
}

void
e_focus_tracker_set_copy_clipboard_action (EFocusTracker *focus_tracker,
                                           GtkAction *copy_clipboard)
{
	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	if (copy_clipboard != NULL) {
		g_return_if_fail (GTK_IS_ACTION (copy_clipboard));
		g_object_ref (copy_clipboard);
	}

	if (focus_tracker->priv->copy_clipboard != NULL) {
		g_signal_handlers_disconnect_matched (
			focus_tracker->priv->copy_clipboard,
			G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
			focus_tracker);
		g_object_unref (focus_tracker->priv->copy_clipboard);
	}

	focus_tracker->priv->copy_clipboard = copy_clipboard;

	if (copy_clipboard != NULL)
		g_signal_connect_swapped (
			copy_clipboard, "activate",
			G_CALLBACK (e_focus_tracker_copy_clipboard),
			focus_tracker);

	g_object_notify (G_OBJECT (focus_tracker), "copy-clipboard-action");
}

GtkAction *
e_focus_tracker_get_paste_clipboard_action (EFocusTracker *focus_tracker)
{
	g_return_val_if_fail (E_IS_FOCUS_TRACKER (focus_tracker), NULL);

	return focus_tracker->priv->paste_clipboard;
}

void
e_focus_tracker_set_paste_clipboard_action (EFocusTracker *focus_tracker,
                                            GtkAction *paste_clipboard)
{
	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	if (paste_clipboard != NULL) {
		g_return_if_fail (GTK_IS_ACTION (paste_clipboard));
		g_object_ref (paste_clipboard);
	}

	if (focus_tracker->priv->paste_clipboard != NULL) {
		g_signal_handlers_disconnect_matched (
			focus_tracker->priv->paste_clipboard,
			G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
			focus_tracker);
		g_object_unref (focus_tracker->priv->paste_clipboard);
	}

	focus_tracker->priv->paste_clipboard = paste_clipboard;

	if (paste_clipboard != NULL)
		g_signal_connect_swapped (
			paste_clipboard, "activate",
			G_CALLBACK (e_focus_tracker_paste_clipboard),
			focus_tracker);

	g_object_notify (G_OBJECT (focus_tracker), "paste-clipboard-action");
}

GtkAction *
e_focus_tracker_get_delete_selection_action (EFocusTracker *focus_tracker)
{
	g_return_val_if_fail (E_IS_FOCUS_TRACKER (focus_tracker), NULL);

	return focus_tracker->priv->delete_selection;
}

void
e_focus_tracker_set_delete_selection_action (EFocusTracker *focus_tracker,
                                             GtkAction *delete_selection)
{
	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	if (delete_selection != NULL) {
		g_return_if_fail (GTK_IS_ACTION (delete_selection));
		g_object_ref (delete_selection);
	}

	if (focus_tracker->priv->delete_selection != NULL) {
		g_signal_handlers_disconnect_matched (
			focus_tracker->priv->delete_selection,
			G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
			focus_tracker);
		g_object_unref (focus_tracker->priv->delete_selection);
	}

	focus_tracker->priv->delete_selection = delete_selection;

	if (delete_selection != NULL)
		g_signal_connect_swapped (
			delete_selection, "activate",
			G_CALLBACK (e_focus_tracker_delete_selection),
			focus_tracker);

	g_object_notify (G_OBJECT (focus_tracker), "delete-selection-action");
}

GtkAction *
e_focus_tracker_get_select_all_action (EFocusTracker *focus_tracker)
{
	g_return_val_if_fail (E_IS_FOCUS_TRACKER (focus_tracker), NULL);

	return focus_tracker->priv->select_all;
}

void
e_focus_tracker_set_select_all_action (EFocusTracker *focus_tracker,
                                       GtkAction *select_all)
{
	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	if (select_all != NULL) {
		g_return_if_fail (GTK_IS_ACTION (select_all));
		g_object_ref (select_all);
	}

	if (focus_tracker->priv->select_all != NULL) {
		g_signal_handlers_disconnect_matched (
			focus_tracker->priv->select_all,
			G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
			focus_tracker);
		g_object_unref (focus_tracker->priv->select_all);
	}

	focus_tracker->priv->select_all = select_all;

	if (select_all != NULL)
		g_signal_connect_swapped (
			select_all, "activate",
			G_CALLBACK (e_focus_tracker_select_all),
			focus_tracker);

	g_object_notify (G_OBJECT (focus_tracker), "select-all-action");
}

GtkAction *
e_focus_tracker_get_undo_action (EFocusTracker *focus_tracker)
{
	g_return_val_if_fail (E_IS_FOCUS_TRACKER (focus_tracker), NULL);

	return focus_tracker->priv->undo;
}

void
e_focus_tracker_set_undo_action (EFocusTracker *focus_tracker,
                                 GtkAction *undo)
{
	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	if (undo != NULL) {
		g_return_if_fail (GTK_IS_ACTION (undo));
		g_object_ref (undo);
	}

	if (focus_tracker->priv->undo != NULL) {
		g_signal_handlers_disconnect_matched (
			focus_tracker->priv->undo,
			G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
			focus_tracker);
		g_object_unref (focus_tracker->priv->undo);
	}

	focus_tracker->priv->undo = undo;

	if (undo != NULL)
		g_signal_connect_swapped (
			undo, "activate",
			G_CALLBACK (e_focus_tracker_undo),
			focus_tracker);

	g_object_notify (G_OBJECT (focus_tracker), "undo-action");
}

GtkAction *
e_focus_tracker_get_redo_action (EFocusTracker *focus_tracker)
{
	g_return_val_if_fail (E_IS_FOCUS_TRACKER (focus_tracker), NULL);

	return focus_tracker->priv->redo;
}

void
e_focus_tracker_set_redo_action (EFocusTracker *focus_tracker,
                                 GtkAction *redo)
{
	g_return_if_fail (E_IS_FOCUS_TRACKER (focus_tracker));

	if (redo != NULL) {
		g_return_if_fail (GTK_IS_ACTION (redo));
		g_object_ref (redo);
	}

	if (focus_tracker->priv->redo != NULL) {
		g_signal_handlers_disconnect_matched (
			focus_tracker->priv->redo,
			G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
			focus_tracker);
		g_object_unref (focus_tracker->priv->redo);
	}

	focus_tracker->priv->redo = redo;

	if (redo != NULL)
		g_signal_connect_swapped (
			redo, "activate",
			G_CALLBACK (e_focus_tracker_redo),
			focus_tracker);

	g_object_notify (G_OBJECT (focus_tracker), "redo-action");
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
