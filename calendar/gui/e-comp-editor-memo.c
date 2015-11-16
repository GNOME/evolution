/*
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <e-util/e-util.h>

#include "e-comp-editor.h"
#include "e-comp-editor-page-attachments.h"
#include "e-comp-editor-page-general.h"
#include "e-comp-editor-property-part.h"
#include "e-comp-editor-property-parts.h"

#include "e-comp-editor-memo.h"

struct _ECompEditorMemoPrivate {
	ECompEditorPropertyPart *categories;

	gpointer insensitive_info_alert;
};

G_DEFINE_TYPE (ECompEditorMemo, e_comp_editor_memo, E_TYPE_COMP_EDITOR)

static void
ece_memo_sensitize_widgets (ECompEditor *comp_editor,
			    gboolean force_insensitive)
{
	ECompEditorMemo *memo_editor;
	gboolean is_organizer;
	guint32 flags;

	g_return_if_fail (E_IS_COMP_EDITOR_MEMO (comp_editor));

	E_COMP_EDITOR_CLASS (e_comp_editor_memo_parent_class)->sensitize_widgets (comp_editor, force_insensitive);

	flags = e_comp_editor_get_flags (comp_editor);
	is_organizer = (flags & (E_COMP_EDITOR_FLAG_IS_NEW | E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER)) != 0;
	memo_editor = E_COMP_EDITOR_MEMO (comp_editor);

	if (force_insensitive || !is_organizer) {
		ECalClient *client;
		const gchar *message = NULL;

		client = e_comp_editor_get_target_client (comp_editor);
		if (!client)
			message = _("Memo cannot be edited, because the selected memo list could not be opened");
		else if (e_client_is_readonly (E_CLIENT (client)))
			message = _("Memo cannot be edited, because the selected memo list is read only");
		else if (!is_organizer)
			message = _("Memo cannot be fully edited, because you are not the organizer");

		if (message) {
			EAlert *alert;

			alert = e_comp_editor_add_information (comp_editor, message, NULL);

			if (memo_editor->priv->insensitive_info_alert)
				e_alert_response (memo_editor->priv->insensitive_info_alert, GTK_RESPONSE_OK);

			memo_editor->priv->insensitive_info_alert = alert;

			if (alert)
				g_object_add_weak_pointer (G_OBJECT (alert), &memo_editor->priv->insensitive_info_alert);

			g_clear_object (&alert);
		} else 	if (memo_editor->priv->insensitive_info_alert) {
			e_alert_response (memo_editor->priv->insensitive_info_alert, GTK_RESPONSE_OK);
		}

	} else if (memo_editor->priv->insensitive_info_alert) {
		e_alert_response (memo_editor->priv->insensitive_info_alert, GTK_RESPONSE_OK);
	}
}

static void
ece_memo_setup_ui (ECompEditorMemo *memo_editor)
{
	const gchar *ui =
		"<ui>"
		"  <menubar action='main-menu'>"
		"    <menu action='view-menu'>"
		"      <placeholder name='parts'>"
		"        <menuitem action='view-categories'/>"
		"      </placeholder>"
		"    </menu>"
		"  </menubar>"
		"</ui>";

	const GtkToggleActionEntry view_actions[] = {
		{ "view-categories",
		  NULL,
		  N_("_Categories"),
		  NULL,
		  N_("Toggles whether to display categories"),
		  NULL,
		  FALSE }
	};

	ECompEditor *comp_editor;
	GSettings *settings;
	GtkUIManager *ui_manager;
	GtkAction *action;
	GtkActionGroup *action_group;
	GError *error = NULL;

	g_return_if_fail (E_IS_COMP_EDITOR_MEMO (memo_editor));

	comp_editor = E_COMP_EDITOR (memo_editor);
	settings = e_comp_editor_get_settings (comp_editor);
	ui_manager = e_comp_editor_get_ui_manager (comp_editor);
	action_group = e_comp_editor_get_action_group (comp_editor, "individual");

	gtk_action_group_add_toggle_actions (action_group,
		view_actions, G_N_ELEMENTS (view_actions), memo_editor);

	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);

	e_plugin_ui_register_manager (ui_manager, "org.gnome.evolution.memo-editor", memo_editor);
	e_plugin_ui_enable_manager (ui_manager, "org.gnome.evolution.memo-editor");

	if (error) {
		g_critical ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	action = e_comp_editor_get_action (comp_editor, "view-categories");
	e_binding_bind_property (
		memo_editor->priv->categories, "visible",
		action, "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	g_settings_bind (
		settings, "editor-show-categories",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);
}

static void
e_comp_editor_memo_constructed (GObject *object)
{
	ECompEditorMemo *memo_editor;
	ECompEditor *comp_editor;
	ECompEditorPage *page;
	ECompEditorPropertyPart *part, *summary;
	EFocusTracker *focus_tracker;
	GtkWidget *edit_widget;

	G_OBJECT_CLASS (e_comp_editor_memo_parent_class)->constructed (object);

	memo_editor = E_COMP_EDITOR_MEMO (object);
	comp_editor = E_COMP_EDITOR (memo_editor);
	focus_tracker = e_comp_editor_get_focus_tracker (comp_editor);

	page = e_comp_editor_page_general_new (comp_editor,
		_("_List:"), E_SOURCE_EXTENSION_MEMO_LIST,
		NULL, FALSE, 1);

	part = e_comp_editor_property_part_summary_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 2, 2, 1);
	summary = part;

	part = e_comp_editor_property_part_dtstart_new (C_("ECompEditor", "Sta_rt date:"), TRUE, TRUE);
	e_comp_editor_page_add_property_part (page, part, 0, 3, 2, 1);

	part = e_comp_editor_property_part_classification_new ();
	e_comp_editor_page_add_property_part (page, part, 0, 4, 2, 1);

	edit_widget = e_comp_editor_property_part_get_edit_widget (part);
	gtk_widget_set_halign (edit_widget, GTK_ALIGN_START);
	gtk_widget_set_hexpand (edit_widget, FALSE);

	part = e_comp_editor_property_part_categories_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 5, 2, 1);
	memo_editor->priv->categories = part;

	part = e_comp_editor_property_part_description_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 6, 2, 1);

	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "General"), page);

	page = e_comp_editor_page_attachments_new (comp_editor);
	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "Attachments"), page);

	ece_memo_setup_ui (memo_editor);

	edit_widget = e_comp_editor_property_part_get_edit_widget (summary);
	e_binding_bind_property (edit_widget, "text", comp_editor, "title-suffix", 0);
	gtk_widget_grab_focus (edit_widget);
}

static void
e_comp_editor_memo_init (ECompEditorMemo *memo_editor)
{
	memo_editor->priv = G_TYPE_INSTANCE_GET_PRIVATE (memo_editor, E_TYPE_COMP_EDITOR_MEMO, ECompEditorMemoPrivate);
}

static void
e_comp_editor_memo_class_init (ECompEditorMemoClass *klass)
{
	GObjectClass *object_class;
	ECompEditorClass *comp_editor_class;

	g_type_class_add_private (klass, sizeof (ECompEditorMemoPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_comp_editor_memo_constructed;

	comp_editor_class = E_COMP_EDITOR_CLASS (klass);
	comp_editor_class->help_section = "memos-usage";
	comp_editor_class->title_format_with_attendees = _("Assigned Memo - %s");
	comp_editor_class->title_format_without_attendees = _("Memo - %s");
	comp_editor_class->icon_name = "stock_insert-note";
	comp_editor_class->sensitize_widgets = ece_memo_sensitize_widgets;
}
