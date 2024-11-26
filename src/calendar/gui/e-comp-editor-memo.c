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

#include "evolution-config.h"

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
	ECompEditorPropertyPart *summary;
	ECompEditorPropertyPart *dtstart;
	ECompEditorPropertyPart *classification;
	ECompEditorPropertyPart *status;
	ECompEditorPropertyPart *url;
	ECompEditorPropertyPart *categories;
	ECompEditorPropertyPart *description;
	ECompEditorPage *attachments_page;

	gpointer insensitive_info_alert;
};

G_DEFINE_TYPE_WITH_PRIVATE (ECompEditorMemo, e_comp_editor_memo, E_TYPE_COMP_EDITOR)

/* A rough code to mimic what Nextcloud does, it's not accurate, but it's close
   enough to work similarly. It skips leading whitespaces and uses up to the first
   100 letters of the first non-empty line as the base file name. */
static gchar *
ece_memo_construct_summary (const gchar *description)
{
	GString *base_filename;
	gunichar uchr;
	gboolean add_space = FALSE;

	if (!description || !*description || !g_utf8_validate (description, -1, NULL))
		return g_strdup (_("New note"));

	base_filename = g_string_sized_new (102);

	while (uchr = g_utf8_get_char (description), g_unichar_isspace (uchr))
		description = g_utf8_next_char (description);

	while (uchr = g_utf8_get_char (description), uchr && uchr != '\r' && uchr != '\n') {
		if (g_unichar_isspace (uchr)) {
			add_space = TRUE;
		} else if ((uchr >> 8) != 0 || !strchr ("\"/\\?:*|", (uchr & 0xFF))) {
			if (base_filename->len >= 98)
				break;

			if (add_space) {
				g_string_append_c (base_filename, ' ');
				add_space = FALSE;
			}

			g_string_append_unichar (base_filename, uchr);

			if (base_filename->len >= 100)
				break;
		}

		description = g_utf8_next_char (description);
	}

	if (!base_filename->len)
		g_string_append (base_filename, _("New note"));

	return g_string_free (base_filename, FALSE);
}

static void
ece_memo_description_changed_cb (GtkTextBuffer *text_buffer,
				 gpointer user_data)
{
	ECompEditorMemo *memo_editor = user_data;
	GtkTextIter text_iter_start, text_iter_end;
	GtkWidget *entry;
	gchar *value, *summary;

	g_return_if_fail (GTK_IS_TEXT_BUFFER (text_buffer));
	g_return_if_fail (E_IS_COMP_EDITOR_MEMO (memo_editor));
	g_return_if_fail (!e_comp_editor_property_part_get_visible (memo_editor->priv->summary));

	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter (text_buffer, &text_iter_end);
	value = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	summary = ece_memo_construct_summary (value);
	entry = e_comp_editor_property_part_get_edit_widget (memo_editor->priv->summary);
	gtk_entry_set_text (GTK_ENTRY (entry), summary);

	g_free (summary);
	g_free (value);
}

static void
ece_memo_notify_target_client_cb (GObject *object,
				  GParamSpec *param,
				  gpointer user_data)
{
	ECompEditorMemo *memo_editor;
	ECompEditor *comp_editor;
	ECalClient *cal_client;
	EUIAction *action;
	GtkWidget *description_widget;
	GtkTextBuffer *text_buffer;
	gboolean supports_date;
	gboolean simple_memo, simple_memo_with_summary;

	g_return_if_fail (E_IS_COMP_EDITOR_MEMO (object));

	memo_editor = E_COMP_EDITOR_MEMO (object);
	comp_editor = E_COMP_EDITOR (memo_editor);
	cal_client = e_comp_editor_get_target_client (comp_editor);
	description_widget = e_comp_editor_property_part_string_get_real_edit_widget (E_COMP_EDITOR_PROPERTY_PART_STRING (memo_editor->priv->description));
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (description_widget));

	simple_memo_with_summary = cal_client && e_client_check_capability (E_CLIENT (cal_client), E_CAL_STATIC_CAPABILITY_SIMPLE_MEMO_WITH_SUMMARY);
	simple_memo = simple_memo_with_summary || (cal_client && e_client_check_capability (E_CLIENT (cal_client), E_CAL_STATIC_CAPABILITY_SIMPLE_MEMO));

	if (simple_memo) {
		if (!simple_memo_with_summary &&
		    e_comp_editor_property_part_get_visible (memo_editor->priv->summary)) {
			g_signal_connect (text_buffer, "changed",
				G_CALLBACK (ece_memo_description_changed_cb), memo_editor);

			gtk_widget_grab_focus (description_widget);
		} else if (simple_memo_with_summary &&
			   !e_comp_editor_property_part_get_visible (memo_editor->priv->summary)) {
			g_signal_handlers_disconnect_by_func (text_buffer,
				G_CALLBACK (ece_memo_description_changed_cb), memo_editor);
		}

		e_comp_editor_property_part_set_visible (memo_editor->priv->summary, simple_memo_with_summary);
		e_comp_editor_property_part_set_visible (memo_editor->priv->dtstart, FALSE);
		e_comp_editor_property_part_set_visible (memo_editor->priv->classification, FALSE);
		e_comp_editor_property_part_set_visible (memo_editor->priv->status, FALSE);
		e_comp_editor_property_part_set_visible (memo_editor->priv->url, FALSE);
		e_comp_editor_property_part_set_visible (memo_editor->priv->categories, FALSE);

		gtk_widget_hide (GTK_WIDGET (memo_editor->priv->attachments_page));

		action = e_comp_editor_get_action (comp_editor, "view-categories");
		e_ui_action_set_sensitive (action, FALSE);

		action = e_comp_editor_get_action (comp_editor, "option-attendees");
		e_ui_action_set_visible (action, FALSE);
	} else {
		supports_date = !cal_client || !e_client_check_capability (E_CLIENT (cal_client), E_CAL_STATIC_CAPABILITY_NO_MEMO_START_DATE);

		if (!e_comp_editor_property_part_get_visible (memo_editor->priv->summary)) {
			g_signal_handlers_disconnect_by_func (text_buffer,
				G_CALLBACK (ece_memo_description_changed_cb), memo_editor);
		}

		e_comp_editor_property_part_set_visible (memo_editor->priv->summary, TRUE);
		e_comp_editor_property_part_set_visible (memo_editor->priv->dtstart, supports_date);
		e_comp_editor_property_part_set_visible (memo_editor->priv->classification, TRUE);
		e_comp_editor_property_part_set_visible (memo_editor->priv->status, TRUE);
		e_comp_editor_property_part_set_visible (memo_editor->priv->url, TRUE);
		e_comp_editor_property_part_set_visible (memo_editor->priv->categories, TRUE);

		gtk_widget_show (GTK_WIDGET (memo_editor->priv->attachments_page));

		action = e_comp_editor_get_action (comp_editor, "view-categories");
		e_ui_action_set_sensitive (action, TRUE);

		action = e_comp_editor_get_action (comp_editor, "option-attendees");
		e_ui_action_set_visible (action, TRUE);
	}
}

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

	if (memo_editor->priv->insensitive_info_alert)
		e_alert_response (memo_editor->priv->insensitive_info_alert, GTK_RESPONSE_OK);

	if (force_insensitive || !is_organizer) {
		ECalClient *client;
		const gchar *message = NULL;

		client = e_comp_editor_get_target_client (comp_editor);
		if (!client)
			message = _("Memo cannot be edited, because the selected memo list could not be opened");
		else if (e_client_is_readonly (E_CLIENT (client)))
			message = _("Memo cannot be edited, because the selected memo list is read only");
		else if (!is_organizer)
			message = _("Changes made to the memo will not be sent to the attendees, because you are not the organizer");

		if (message) {
			EAlert *alert;

			alert = e_comp_editor_add_information (comp_editor, message, NULL);

			memo_editor->priv->insensitive_info_alert = alert;

			if (alert)
				g_object_add_weak_pointer (G_OBJECT (alert), &memo_editor->priv->insensitive_info_alert);

			g_clear_object (&alert);
		}
	}
}

static void
ece_memo_setup_ui (ECompEditorMemo *memo_editor)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<submenu action='view-menu'>"
		      "<placeholder id='parts'>"
			"<item action='view-categories' text_only='true'/>"
		      "</placeholder>"
		    "</submenu>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry view_actions[] = {
		{ "view-categories",
		  NULL,
		  N_("_Categories"),
		  NULL,
		  N_("Toggles whether to display categories"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state }
	};

	ECompEditor *comp_editor;
	GSettings *settings;
	EUIManager *ui_manager;
	EUIAction *action;

	g_return_if_fail (E_IS_COMP_EDITOR_MEMO (memo_editor));

	comp_editor = E_COMP_EDITOR (memo_editor);
	settings = e_comp_editor_get_settings (comp_editor);
	ui_manager = e_comp_editor_get_ui_manager (comp_editor);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "individual", GETTEXT_PACKAGE,
		view_actions, G_N_ELEMENTS (view_actions), memo_editor, eui);

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
	ECompEditorPropertyPart *part;
	EFocusTracker *focus_tracker;
	EUIManager *ui_manager;
	GtkWidget *edit_widget;

	G_OBJECT_CLASS (e_comp_editor_memo_parent_class)->constructed (object);

	memo_editor = E_COMP_EDITOR_MEMO (object);
	comp_editor = E_COMP_EDITOR (memo_editor);
	ui_manager = e_comp_editor_get_ui_manager (comp_editor);
	focus_tracker = e_comp_editor_get_focus_tracker (comp_editor);

	e_ui_manager_freeze (ui_manager);

	page = e_comp_editor_page_general_new (comp_editor,
		_("_List:"), E_SOURCE_EXTENSION_MEMO_LIST,
		NULL, FALSE, 1);

	part = e_comp_editor_property_part_summary_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 2, 2, 1);
	memo_editor->priv->summary = part;

	part = e_comp_editor_property_part_dtstart_new (C_("ECompEditor", "Sta_rt date:"), TRUE, TRUE, FALSE);
	e_comp_editor_page_add_property_part (page, part, 0, 3, 2, 1);
	memo_editor->priv->dtstart = part;

	part = e_comp_editor_property_part_classification_new ();
	e_comp_editor_page_add_property_part (page, part, 0, 4, 2, 1);
	memo_editor->priv->classification = part;

	edit_widget = e_comp_editor_property_part_get_edit_widget (part);
	gtk_widget_set_halign (edit_widget, GTK_ALIGN_START);
	gtk_widget_set_hexpand (edit_widget, FALSE);

	part = e_comp_editor_property_part_status_new (I_CAL_VJOURNAL_COMPONENT);
	e_comp_editor_page_add_property_part (page, part, 0, 5, 2, 1);
	memo_editor->priv->status = part;

	edit_widget = e_comp_editor_property_part_get_edit_widget (part);
	gtk_widget_set_halign (edit_widget, GTK_ALIGN_START);
	gtk_widget_set_hexpand (edit_widget, FALSE);

	part = e_comp_editor_property_part_url_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 6, 2, 1);
	memo_editor->priv->url = part;

	part = e_comp_editor_property_part_categories_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 7, 2, 1);
	memo_editor->priv->categories = part;

	part = e_comp_editor_property_part_description_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 8, 2, 1);
	memo_editor->priv->description = part;

	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "General"), page);

	page = e_comp_editor_page_attachments_new (comp_editor);
	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "Attachments"), page);
	memo_editor->priv->attachments_page = page;

	ece_memo_setup_ui (memo_editor);

	edit_widget = e_comp_editor_property_part_get_edit_widget (memo_editor->priv->summary);
	e_binding_bind_property (edit_widget, "text", comp_editor, "title-suffix", 0);
	gtk_widget_grab_focus (edit_widget);

	g_signal_connect (comp_editor, "notify::target-client",
		G_CALLBACK (ece_memo_notify_target_client_cb), NULL);

	e_extensible_load_extensions (E_EXTENSIBLE (comp_editor));

	e_ui_manager_thaw (ui_manager);
}

static void
e_comp_editor_memo_init (ECompEditorMemo *memo_editor)
{
	memo_editor->priv = e_comp_editor_memo_get_instance_private (memo_editor);
}

static void
e_comp_editor_memo_class_init (ECompEditorMemoClass *klass)
{
	GObjectClass *object_class;
	ECompEditorClass *comp_editor_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_comp_editor_memo_constructed;

	comp_editor_class = E_COMP_EDITOR_CLASS (klass);
	comp_editor_class->help_section = "memos-usage";
	comp_editor_class->title_format_with_attendees = _("Assigned Memo — %s");
	comp_editor_class->title_format_without_attendees = _("Memo — %s");
	comp_editor_class->icon_name = "stock_insert-note";
	comp_editor_class->sensitize_widgets = ece_memo_sensitize_widgets;
}
