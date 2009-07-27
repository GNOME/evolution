/*
 * Evolution calendar - Task editor dialog
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
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
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <e-util/e-plugin-ui.h>
#include <e-util/e-util-private.h>
#include <evolution-shell-component-utils.h>

#include "task-page.h"
#include "task-details-page.h"
#include "cancel-comp.h"
#include "task-editor.h"

#define TASK_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_TASK_EDITOR, TaskEditorPrivate))

struct _TaskEditorPrivate {
	TaskPage *task_page;
	TaskDetailsPage *task_details_page;
	GtkWidget *task_details_window;

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
"    <menu action='options-menu'>"
"      <menu action='classification-menu'>"
"        <menuitem action='classify-public'/>"
"        <menuitem action='classify-private'/>"
"        <menuitem action='classify-confidential'/>"
"      </menu>"
"      <menuitem action='option-status'/>"
"    </menu>"
"  </menubar>"
"  <toolbar name='main-toolbar'>"
"    <toolitem action='view-time-zone'/>"
"    <toolitem action='option-status'/>"
"  </toolbar>"
"</ui>";

static void task_editor_edit_comp (CompEditor *editor, ECalComponent *comp);
static gboolean task_editor_send_comp (CompEditor *editor, ECalComponentItipMethod method, gboolean strip_alarms);

G_DEFINE_TYPE (TaskEditor, task_editor, TYPE_COMP_EDITOR)

static void
action_option_status_cb (GtkAction *action,
                         TaskEditor *editor)
{
	gtk_widget_show (editor->priv->task_details_window);
}

static void
action_send_options_cb (GtkAction *action,
                        TaskEditor *editor)
{
	task_page_sendoptions_clicked_cb (editor->priv->task_page);
}

static GtkActionEntry task_entries[] = {

	{ "option-status",
	  "stock_view-details",
	  N_("_Status Details"),
	  "<Control>t",
	  N_("Click to change or view the status details of the task"),
	  G_CALLBACK (action_option_status_cb) }
};

static GtkActionEntry assigned_task_entries[] = {

	{ "send-options",
	  NULL,
	  N_("_Send Options"),
	  NULL,
	  N_("Insert advanced send options"),
	  G_CALLBACK (action_send_options_cb) }
};

static void
task_editor_client_changed_cb (TaskEditor *te)
{
	ECal *client;

	client = comp_editor_get_client (COMP_EDITOR (te));
	e_meeting_store_set_e_cal (te->priv->model, client);
}

static void
task_editor_model_changed_cb (TaskEditor *te)
{
	if (!te->priv->updating) {
		comp_editor_set_changed (COMP_EDITOR (te), TRUE);
		comp_editor_set_needs_send (COMP_EDITOR (te), TRUE);
	}
}

static GObject *
task_editor_constructor (GType type,
                         guint n_construct_properties,
                         GObjectConstructParam *construct_properties)
{
	GObject *object;
	CompEditor *editor;
	CompEditorFlags flags;
	TaskEditorPrivate *priv;
	GtkActionGroup *action_group;
	ECal *client;
	gboolean is_assigned;

	/* Chain up to parent's constructor() method. */
	object = G_OBJECT_CLASS (task_editor_parent_class)->constructor (
		type, n_construct_properties, construct_properties);

	editor = COMP_EDITOR (object);
	priv = TASK_EDITOR_GET_PRIVATE (object);

	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);
	action_group = comp_editor_get_action_group (editor, "coordinated");

	is_assigned = flags & COMP_EDITOR_IS_ASSIGNED;

	task_page_set_assignment (priv->task_page, is_assigned);
	gtk_action_group_set_visible (action_group, is_assigned);

	if (is_assigned) {
		if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_REQ_SEND_OPTIONS))
			task_page_show_options (priv->task_page);
		comp_editor_set_group_item (editor, TRUE);
	}

	return object;
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

	if (priv->task_details_page) {
		g_object_unref (priv->task_details_page);
		priv->task_details_page = NULL;
	}

	if (priv->model) {
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (task_editor_parent_class)->dispose (object);
}

static void
task_editor_show_categories (CompEditor *editor,
                             gboolean visible)
{
	TaskEditorPrivate *priv;

	priv = TASK_EDITOR_GET_PRIVATE (editor);

	task_page_set_show_categories (priv->task_page, visible);
}

static void
task_editor_show_role (CompEditor *editor,
                       gboolean visible)
{
	TaskEditorPrivate *priv;

	priv = TASK_EDITOR_GET_PRIVATE (editor);

	task_page_set_view_role (priv->task_page, visible);
}

static void
task_editor_show_rsvp (CompEditor *editor,
                       gboolean visible)
{
	TaskEditorPrivate *priv;

	priv = TASK_EDITOR_GET_PRIVATE (editor);

	task_page_set_view_rsvp (priv->task_page, visible);
}

static void
task_editor_show_status (CompEditor *editor,
                         gboolean visible)
{
	TaskEditorPrivate *priv;

	priv = TASK_EDITOR_GET_PRIVATE (editor);

	task_page_set_view_status (priv->task_page, visible);
}

static void
task_editor_show_time_zone (CompEditor *editor,
                            gboolean visible)
{
	TaskEditorPrivate *priv;

	priv = TASK_EDITOR_GET_PRIVATE (editor);

	task_page_set_show_timezone (priv->task_page, visible);
}

static void
task_editor_show_type (CompEditor *editor,
                       gboolean visible)
{
	TaskEditorPrivate *priv;

	priv = TASK_EDITOR_GET_PRIVATE (editor);

	task_page_set_view_type (priv->task_page, visible);
}

static void
task_editor_class_init (TaskEditorClass *class)
{
	GObjectClass *object_class;
	CompEditorClass *editor_class;

	g_type_class_add_private (class, sizeof (TaskEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = task_editor_constructor;
	object_class->dispose = task_editor_dispose;

	editor_class = COMP_EDITOR_CLASS (class);
	editor_class->help_section = "usage-calendar-todo";
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
	GError *error = NULL;

	te->priv = TASK_EDITOR_GET_PRIVATE (te);
	te->priv->model = E_MEETING_STORE (e_meeting_store_new ());
	te->priv->assignment_shown = TRUE;
	te->priv->updating = FALSE;

	te->priv->task_page = task_page_new (te->priv->model, editor);
	comp_editor_append_page (
		editor, COMP_EDITOR_PAGE (te->priv->task_page),
		_("_Task"), TRUE);

	te->priv->task_details_window = gtk_dialog_new_with_buttons (
		_("Task Details"), GTK_WINDOW (te), GTK_DIALOG_MODAL,
		GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	g_signal_connect (
		te->priv->task_details_window, "response",
		G_CALLBACK (gtk_widget_hide), NULL);
	g_signal_connect (
		te->priv->task_details_window, "delete-event",
		G_CALLBACK(gtk_widget_hide), NULL);

	te->priv->task_details_page = task_details_page_new (editor);
	gtk_container_add (
		GTK_CONTAINER (GTK_DIALOG (te->priv->task_details_window)->vbox),
		comp_editor_page_get_widget ((CompEditorPage *) te->priv->task_details_page));
	gtk_widget_show_all (gtk_bin_get_child (GTK_BIN (te->priv->task_details_window)));
	comp_editor_append_page (
		editor, COMP_EDITOR_PAGE (te->priv->task_details_page), NULL, FALSE);

	action_group = comp_editor_get_action_group (editor, "individual");
	gtk_action_group_add_actions (
		action_group, task_entries,
		G_N_ELEMENTS (task_entries), te);

	action_group = comp_editor_get_action_group (editor, "coordinated");
	gtk_action_group_add_actions (
		action_group, assigned_task_entries,
		G_N_ELEMENTS (assigned_task_entries), te);

	ui_manager = comp_editor_get_ui_manager (editor);
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);
	e_plugin_ui_register_manager ("task-editor", ui_manager, te);

	if (error != NULL) {
		g_critical ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	g_signal_connect (
		te, "notify::client",
		G_CALLBACK (task_editor_client_changed_cb), NULL);

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
task_editor_edit_comp (CompEditor *editor, ECalComponent *comp)
{
	TaskEditorPrivate *priv;
	ECalComponentOrganizer organizer;
	ECal *client;
	GSList *attendees = NULL;

	priv = TASK_EDITOR_GET_PRIVATE (editor);

	priv->updating = TRUE;

	if (COMP_EDITOR_CLASS (task_editor_parent_class)->edit_comp)
		COMP_EDITOR_CLASS (task_editor_parent_class)->edit_comp (editor, comp);

	client = comp_editor_get_client (editor);

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

			ia = E_MEETING_ATTENDEE (e_meeting_attendee_new_from_e_cal_component_attendee (ca));
			/* If we aren't the organizer or the attendee is just delegating, don't allow editing */
			if (!comp_editor_get_user_org (editor) || e_meeting_attendee_is_set_delto (ia))
				e_meeting_attendee_set_edit_level (ia,  E_MEETING_ATTENDEE_EDIT_NONE);
			task_page_add_attendee (priv->task_page, ia);

			g_object_unref (ia);
		}

		/* If we aren't the organizer we can still change our own status */
		if (!comp_editor_get_user_org (editor)) {
			EAccountList *accounts;
			EAccount *account;
			EIterator *it;

			accounts = itip_addresses_get ();
			for (it = e_list_get_iterator((EList *)accounts);e_iterator_is_valid(it);e_iterator_next(it)) {
				EMeetingAttendee *ia;

				account = (EAccount*)e_iterator_get(it);

				ia = e_meeting_store_find_attendee (priv->model, account->id->address, &row);
				if (ia != NULL)
					e_meeting_attendee_set_edit_level (ia, E_MEETING_ATTENDEE_EDIT_STATUS);
			}
			g_object_unref(it);
		} else if (e_cal_get_organizer_must_attend (client)) {
			EMeetingAttendee *ia;

			ia = e_meeting_store_find_attendee (priv->model, organizer.value, &row);
			if (ia != NULL)
				e_meeting_attendee_set_edit_level (ia, E_MEETING_ATTENDEE_EDIT_NONE);
		}

		comp_editor_set_group_item (editor, TRUE);
		priv->assignment_shown = TRUE;
	}
	e_cal_component_free_attendee_list (attendees);

	comp_editor_set_needs_send (editor, priv->assignment_shown && itip_organizer_is_user (comp, client));

	priv->updating = FALSE;
}

static gboolean
task_editor_send_comp (CompEditor *editor, ECalComponentItipMethod method, gboolean strip_alarms)
{
	TaskEditorPrivate *priv;
	ECalComponent *comp = NULL;

	priv = TASK_EDITOR_GET_PRIVATE (editor);

	/* Don't cancel more than once or when just publishing */
	if (method == E_CAL_COMPONENT_METHOD_PUBLISH ||
	    method == E_CAL_COMPONENT_METHOD_CANCEL)
		goto parent;

	comp = task_page_get_cancel_comp (priv->task_page);
	if (comp != NULL) {
		ECal *client;
		gboolean result;

		client = e_meeting_store_get_e_cal (priv->model);
		result = itip_send_comp (E_CAL_COMPONENT_METHOD_CANCEL, comp,
				client, NULL, NULL, NULL, strip_alarms, FALSE);
		g_object_unref (comp);

		if (!result)
			return FALSE;
	}

 parent:
	if (COMP_EDITOR_CLASS (task_editor_parent_class)->send_comp)
		return COMP_EDITOR_CLASS (task_editor_parent_class)->send_comp (editor, method, strip_alarms);

	return FALSE;
}

/**
 * task_editor_new:
 * @client: a ECal
 *
 * Creates a new event editor dialog.
 *
 * Return value: A newly-created event editor dialog, or NULL if the event
 * editor could not be created.
 **/
CompEditor *
task_editor_new (ECal *client, CompEditorFlags flags)
{
	g_return_val_if_fail (E_IS_CAL (client), NULL);

	return g_object_new (
		TYPE_TASK_EDITOR,
		"flags", flags, "client", client, NULL);
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

