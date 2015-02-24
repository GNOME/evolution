/*
 * Evolution calendar - Task editor dialog
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
 * Authors:
 *		Miguel de Icaza <miguel@ximian.com>
 *      Federico Mena-Quintero <federico@ximian.com>
 *      Seth Alves <alves@hungry.com>
 *      JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>

#include "task-page.h"
#include "cancel-comp.h"
#include "task-editor.h"

#define TASK_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_TASK_EDITOR, TaskEditorPrivate))

struct _TaskEditorPrivate {
	TaskPage *task_page;

	EMeetingStore *model;
	gboolean assignment_shown;
	gboolean updating;
};

/* Extends the UI definition in CompEditor */
static const gchar *ui =
"<ui>"
"  <menubar action='main-menu'>"
"    <menu action='view-menu'>"
"      <menuitem action='view-type'/>"
"      <menuitem action='view-status'/>"
"      <menuitem action='view-role'/>"
"      <menuitem action='view-rsvp'/>"
"      <separator/>"
"      <menuitem action='view-time-zone'/>"
"      <menuitem action='view-categories'/>"
"    </menu>"
"    <menu action='insert-menu'>"
"      <menuitem action='send-options'/>"
"    </menu>"
"  </menubar>"
"  <toolbar name='main-toolbar'>"
"    <placeholder name='content'>"
"      <toolitem action='view-time-zone'/>"
"    </placeholder>"
"  </toolbar>"
"</ui>";

static void	task_editor_edit_comp		(CompEditor *editor,
						 ECalComponent *comp);
static gboolean	task_editor_send_comp		(CompEditor *editor,
						 ECalComponentItipMethod method,
						 gboolean strip_alarms);

G_DEFINE_TYPE (TaskEditor, task_editor, TYPE_COMP_EDITOR)

static void
action_send_options_cb (GtkAction *action,
                        TaskEditor *editor)
{
	task_page_send_options_clicked_cb (editor->priv->task_page);
}

static GtkActionEntry assigned_task_entries[] = {

	{ "send-options",
	  NULL,
	  N_("_Send Options"),
	  NULL,
	  N_("Insert advanced send options"),
	  G_CALLBACK (action_send_options_cb) }
};

static void
task_editor_model_changed_cb (TaskEditor *te)
{
	if (!te->priv->updating) {
		comp_editor_set_changed (COMP_EDITOR (te), TRUE);
		comp_editor_set_needs_send (COMP_EDITOR (te), TRUE);
	}
}

static void
task_editor_dispose (GObject *object)
{
	TaskEditorPrivate *priv;

	priv = TASK_EDITOR_GET_PRIVATE (object);

	if (priv->task_page) {
		g_object_unref (priv->task_page);
		priv->task_page = NULL;
	}

	if (priv->model) {
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (task_editor_parent_class)->dispose (object);
}

static void
task_editor_constructed (GObject *object)
{
	TaskEditorPrivate *priv;
	CompEditor *editor;
	CompEditorFlags flags;
	GtkActionGroup *action_group;
	gboolean is_assigned;

	priv = TASK_EDITOR_GET_PRIVATE (object);
	editor = COMP_EDITOR (object);

	flags = comp_editor_get_flags (editor);
	is_assigned = flags & COMP_EDITOR_IS_ASSIGNED;

	priv->task_page = task_page_new (priv->model, editor);
	task_page_set_assignment (priv->task_page, is_assigned);
	comp_editor_append_page (
		editor, COMP_EDITOR_PAGE (priv->task_page),
		_("Task"), TRUE);

	action_group = comp_editor_get_action_group (editor, "coordinated");
	gtk_action_group_set_visible (action_group, is_assigned);

	if (is_assigned) {
		ECalClient *client;

		client = comp_editor_get_client (editor);

		if (e_client_check_capability (
				E_CLIENT (client),
				CAL_STATIC_CAPABILITY_REQ_SEND_OPTIONS))
			task_page_show_options (priv->task_page);
		comp_editor_set_group_item (editor, TRUE);
	}

	e_binding_bind_property (
		object, "client",
		priv->model, "client",
		G_BINDING_SYNC_CREATE);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (task_editor_parent_class)->constructed (object);
}

static void
task_editor_show_categories (CompEditor *editor,
                             gboolean visible)
{
	TaskEditor *task_editor = TASK_EDITOR (editor);

	task_page_set_show_categories (task_editor->priv->task_page, visible);
}

static void
task_editor_show_role (CompEditor *editor,
                       gboolean visible)
{
	TaskEditor *task_editor = TASK_EDITOR (editor);

	task_page_set_view_role (task_editor->priv->task_page, visible);
}

static void
task_editor_show_rsvp (CompEditor *editor,
                       gboolean visible)
{
	TaskEditor *task_editor = TASK_EDITOR (editor);

	task_page_set_view_rsvp (task_editor->priv->task_page, visible);
}

static void
task_editor_show_status (CompEditor *editor,
                         gboolean visible)
{
	TaskEditor *task_editor = TASK_EDITOR (editor);

	task_page_set_view_status (task_editor->priv->task_page, visible);
}

static void
task_editor_show_time_zone (CompEditor *editor,
                            gboolean visible)
{
	TaskEditor *task_editor = TASK_EDITOR (editor);

	task_page_set_show_timezone (task_editor->priv->task_page, visible);
}

static void
task_editor_show_type (CompEditor *editor,
                       gboolean visible)
{
	TaskEditor *task_editor = TASK_EDITOR (editor);

	task_page_set_view_type (task_editor->priv->task_page, visible);
}

static void
task_editor_class_init (TaskEditorClass *class)
{
	GObjectClass *object_class;
	CompEditorClass *editor_class;

	g_type_class_add_private (class, sizeof (TaskEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = task_editor_dispose;
	object_class->constructed = task_editor_constructed;

	editor_class = COMP_EDITOR_CLASS (class);
	editor_class->help_section = "tasks-usage";
	editor_class->edit_comp = task_editor_edit_comp;
	editor_class->send_comp = task_editor_send_comp;
	editor_class->show_categories = task_editor_show_categories;
	editor_class->show_role = task_editor_show_role;
	editor_class->show_rsvp = task_editor_show_rsvp;
	editor_class->show_status = task_editor_show_status;
	editor_class->show_time_zone = task_editor_show_time_zone;
	editor_class->show_type = task_editor_show_type;
}

static void
task_editor_init (TaskEditor *te)
{
	CompEditor *editor = COMP_EDITOR (te);
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GtkAction *action;
	const gchar *id;
	GError *error = NULL;

	te->priv = TASK_EDITOR_GET_PRIVATE (te);
	te->priv->model = E_MEETING_STORE (e_meeting_store_new ());
	te->priv->assignment_shown = TRUE;
	te->priv->updating = FALSE;

	action_group = comp_editor_get_action_group (editor, "coordinated");
	gtk_action_group_add_actions (
		action_group, assigned_task_entries,
		G_N_ELEMENTS (assigned_task_entries), te);

	ui_manager = comp_editor_get_ui_manager (editor);
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);

	id = "org.gnome.evolution.task-editor";
	e_plugin_ui_register_manager (ui_manager, id, te);
	e_plugin_ui_enable_manager (ui_manager, id);

	if (error != NULL) {
		g_critical ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	action = comp_editor_get_action (editor, "print");
	gtk_action_set_tooltip (action, _("Print this task"));

	g_signal_connect_swapped (
		te->priv->model, "row_changed",
		G_CALLBACK (task_editor_model_changed_cb), te);
	g_signal_connect_swapped (
		te->priv->model, "row_inserted",
		G_CALLBACK (task_editor_model_changed_cb), te);
	g_signal_connect_swapped (
		te->priv->model, "row_deleted",
		G_CALLBACK (task_editor_model_changed_cb), te);
}

static void
task_editor_edit_comp (CompEditor *editor,
                       ECalComponent *comp)
{
	TaskEditorPrivate *priv;
	ECalComponentOrganizer organizer;
	ECalClient *client;
	GSList *attendees = NULL;
	ESourceRegistry *registry;
	EShell *shell;

	priv = TASK_EDITOR_GET_PRIVATE (editor);

	priv->updating = TRUE;

	if (COMP_EDITOR_CLASS (task_editor_parent_class)->edit_comp)
		COMP_EDITOR_CLASS (task_editor_parent_class)->edit_comp (editor, comp);

	shell = comp_editor_get_shell (editor);
	client = comp_editor_get_client (editor);

	registry = e_shell_get_registry (shell);

	/* Get meeting related stuff */
	e_cal_component_get_organizer (comp, &organizer);
	e_cal_component_get_attendee_list (comp, &attendees);

	if (attendees != NULL) {
		GSList *l;
		gint row;

		task_page_hide_options (priv->task_page);
		task_page_set_assignment (priv->task_page, TRUE);

		for (l = attendees; l != NULL; l = l->next) {
			ECalComponentAttendee *ca = l->data;
			EMeetingAttendee *ia;

			ia = E_MEETING_ATTENDEE (
				e_meeting_attendee_new_from_e_cal_component_attendee (ca));
			/* If we aren't the organizer or the attendee is just
			 * delegating, don't allow editing. */
			if (!comp_editor_get_user_org (editor) ||
				e_meeting_attendee_is_set_delto (ia))
				e_meeting_attendee_set_edit_level (
					ia,  E_MEETING_ATTENDEE_EDIT_NONE);
			comp_editor_page_add_attendee (
				COMP_EDITOR_PAGE (priv->task_page), ia);

			g_object_unref (ia);
		}

		/* If we aren't the organizer we can still change our own status */
		if (!comp_editor_get_user_org (editor)) {
			EMeetingAttendee *ia;

			ia = e_meeting_store_find_self (priv->model, &row);

			if (ia != NULL)
				e_meeting_attendee_set_edit_level (
					ia, E_MEETING_ATTENDEE_EDIT_STATUS);
		} else if (e_cal_client_check_organizer_must_attend (client)) {
			EMeetingAttendee *ia;

			ia = e_meeting_store_find_attendee (priv->model, organizer.value, &row);
			if (ia != NULL)
				e_meeting_attendee_set_edit_level (
					ia, E_MEETING_ATTENDEE_EDIT_NONE);
		}

		comp_editor_set_group_item (editor, TRUE);
		priv->assignment_shown = TRUE;
	}
	e_cal_component_free_attendee_list (attendees);

	comp_editor_set_needs_send (
		editor, priv->assignment_shown &&
		itip_organizer_is_user (registry, comp, client));

	priv->updating = FALSE;
}

static gboolean
task_editor_send_comp (CompEditor *editor,
                       ECalComponentItipMethod method,
                       gboolean strip_alarms)
{
	TaskEditorPrivate *priv;
	EShell *shell;
	ESourceRegistry *registry;
	ECalComponent *comp = NULL;

	priv = TASK_EDITOR_GET_PRIVATE (editor);

	/* Don't cancel more than once or when just publishing */
	if (method == E_CAL_COMPONENT_METHOD_PUBLISH ||
	    method == E_CAL_COMPONENT_METHOD_CANCEL)
		goto parent;

	shell = comp_editor_get_shell (editor);
	registry = e_shell_get_registry (shell);

	comp = task_page_get_cancel_comp (priv->task_page);
	if (comp != NULL) {
		ECalClient *client;
		gboolean result;

		client = e_meeting_store_get_client (priv->model);
		result = itip_send_comp_sync (
			registry, E_CAL_COMPONENT_METHOD_CANCEL, comp,
			client, NULL, NULL, NULL, strip_alarms, FALSE, NULL, NULL);
		g_object_unref (comp);

		if (!result)
			return FALSE;
	}

 parent:
	if (COMP_EDITOR_CLASS (task_editor_parent_class)->send_comp)
		return COMP_EDITOR_CLASS (task_editor_parent_class)->
			send_comp (editor, method, strip_alarms);

	return FALSE;
}

/**
 * task_editor_new:
 * @client: a ECalClient
 *
 * Creates a new event editor dialog.
 *
 * Return value: A newly-created event editor dialog, or NULL if the event
 * editor could not be created.
 **/
CompEditor *
task_editor_new (ECalClient *client,
                 EShell *shell,
                 CompEditorFlags flags)
{
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return g_object_new (
		TYPE_TASK_EDITOR,
		"client", client, "flags", flags, "shell", shell, NULL);
}

void
task_editor_show_assignment (TaskEditor *te)
{
	CompEditor *editor;

	g_return_if_fail (IS_TASK_EDITOR (te));

	editor = COMP_EDITOR (te);

	task_page_set_assignment (te->priv->task_page, TRUE);
	if (!te->priv->assignment_shown) {
		te->priv->assignment_shown = TRUE;
		comp_editor_set_needs_send (editor, TRUE);
		comp_editor_set_changed (editor, FALSE);
	}

}

