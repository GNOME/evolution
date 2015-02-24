/*
 * Evolution calendar - Event editor dialog
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
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>

#include "event-page.h"
#include "recurrence-page.h"
#include "schedule-page.h"
#include "cancel-comp.h"
#include "event-editor.h"

#define EVENT_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_EVENT_EDITOR, EventEditorPrivate))

struct _EventEditorPrivate {
	EventPage *event_page;
	RecurrencePage *recur_page;
	GtkWidget *recur_window;
	SchedulePage *sched_page;
	GtkWidget *sched_window;

	EMeetingStore *model;
	gboolean meeting_shown;
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
"      <menuitem action='alarms'/>"
"      <menuitem action='show-time-busy'/>"
"      <menuitem action='recurrence'/>"
"      <menuitem action='all-day-event'/>"
"      <menuitem action='free-busy'/>"
"      <menu action='classification-menu'>"
"        <menuitem action='classify-public'/>"
"        <menuitem action='classify-private'/>"
"        <menuitem action='classify-confidential'/>"
"      </menu>"
"    </menu>"
"  </menubar>"
"  <toolbar name='main-toolbar'>"
"    <placeholder name='content'>\n"
"      <toolitem action='alarms'/>\n"
"      <toolitem action='show-time-busy'/>\n"
"      <toolitem action='recurrence'/>\n"
"      <toolitem action='all-day-event'/>\n"
"      <toolitem action='free-busy'/>\n"
"    </placeholder>"
"  </toolbar>"
"</ui>";

static void	event_editor_edit_comp		(CompEditor *editor,
						 ECalComponent *comp);
static gboolean	event_editor_send_comp		(CompEditor *editor,
						 ECalComponentItipMethod method,
						 gboolean strip_alarms);

G_DEFINE_TYPE (EventEditor, event_editor, TYPE_COMP_EDITOR)

static void
create_schedule_page (CompEditor *editor)
{
	EventEditorPrivate *priv;
	ENameSelector *name_selector;
	CompEditorPage *page;
	GtkWidget *content_area;

	priv = EVENT_EDITOR_GET_PRIVATE (editor);

	priv->sched_window = gtk_dialog_new_with_buttons (
		_("Free/Busy"), GTK_WINDOW (editor), GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Close"), GTK_RESPONSE_CLOSE, NULL);

	content_area =
		gtk_dialog_get_content_area (GTK_DIALOG (priv->sched_window));

	g_signal_connect (
		priv->sched_window, "response",
		G_CALLBACK (gtk_widget_hide), NULL);
	g_signal_connect (
		priv->sched_window, "delete-event",
		G_CALLBACK (gtk_widget_hide_on_delete), NULL);

	priv->sched_page = schedule_page_new (priv->model, editor);
	page = COMP_EDITOR_PAGE (priv->sched_page);
	gtk_container_add (
		GTK_CONTAINER (content_area),
		comp_editor_page_get_widget (page));

	name_selector = event_page_get_name_selector (priv->event_page);
	schedule_page_set_name_selector (priv->sched_page, name_selector);

	comp_editor_append_page (editor, page, NULL, FALSE);
	schedule_page_update_free_busy (priv->sched_page);

	gtk_widget_show_all (priv->sched_window);
}

static void
action_alarms_cb (GtkAction *action,
                  EventEditor *editor)
{
	event_page_show_alarm (editor->priv->event_page);
}

static void
action_all_day_event_cb (GtkToggleAction *action,
                         EventEditor *editor)
{
	gboolean active;
	GtkAction *action_show_busy;
	CompEditor *comp_editor = COMP_EDITOR (editor);

	active = gtk_toggle_action_get_active (action);
	event_page_set_all_day_event (editor->priv->event_page, active);

	action_show_busy = comp_editor_get_action (comp_editor, "show-time-busy");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action_show_busy), !active);
	event_page_set_show_time_busy (editor->priv->event_page, !active);
}

static void
action_free_busy_cb (GtkAction *action,
                     EventEditor *editor)
{
	if (editor->priv->sched_window == NULL)
		create_schedule_page (COMP_EDITOR (editor));
	else
		gtk_window_present (GTK_WINDOW (editor->priv->sched_window));
}

static void
action_recurrence_cb (GtkAction *action,
                      EventEditor *editor)
{
	gtk_widget_show (editor->priv->recur_window);
}

static void
action_send_options_cb (GtkAction *action,
                        EventEditor *editor)
{
	event_page_send_options_clicked_cb (editor->priv->event_page);
}

static void
action_show_time_busy_cb (GtkToggleAction *action,
                          EventEditor *editor)
{
	gboolean active;

	active = gtk_toggle_action_get_active (action);
	event_page_set_show_time_busy (editor->priv->event_page, active);
}

static GtkActionEntry editable_entries[] = {

	{ "alarms",
	  "appointment-soon",
	  N_("_Reminders"),
	  NULL,
	  N_("Set or unset reminders for this event"),
	  G_CALLBACK (action_alarms_cb) },
};

static GtkToggleActionEntry editable_toggle_entries[] = {

	{ "show-time-busy",
	  "dialog-error",
	  N_("Show Time as _Busy"),
	  NULL,
	  N_("Toggles whether to show time as busy"),
	  G_CALLBACK (action_show_time_busy_cb),
	  FALSE }
};

static GtkActionEntry event_entries[] = {

	{ "recurrence",
	  "stock_task-recurring",
	  N_("_Recurrence"),
	  NULL,
	  N_("Make this a recurring event"),
	  G_CALLBACK (action_recurrence_cb) },

	{ "send-options",
	  NULL,
	  N_("Send Options"),
	  NULL,
	  N_("Insert advanced send options"),
	  G_CALLBACK (action_send_options_cb) }
};

static GtkToggleActionEntry event_toggle_entries[] = {

	{ "all-day-event",
	  "stock_new-24h-appointment",
	  N_("All _Day Event"),
	  NULL,
	  N_("Toggles whether to have All Day Event"),
	  G_CALLBACK (action_all_day_event_cb),
	  FALSE },
};

static GtkActionEntry meeting_entries[] = {

	{ "free-busy",
	  "query-free-busy",
	  N_("_Free/Busy"),
	  NULL,
	  N_("Query free / busy information for the attendees"),
	  G_CALLBACK (action_free_busy_cb) }
};

static void
event_editor_model_changed_cb (EventEditor *ee)
{
	if (!ee->priv->updating) {
		comp_editor_set_changed (COMP_EDITOR (ee), TRUE);
		comp_editor_set_needs_send (COMP_EDITOR (ee), TRUE);
	}
}

static GObject *
event_editor_constructor (GType type,
                          guint n_construct_properties,
                          GObjectConstructParam *construct_properties)
{
	GObject *object;
	CompEditor *editor;
	CompEditorFlags flags;
	CompEditorPage *page;
	EventEditorPrivate *priv;
	GtkActionGroup *action_group;
	GtkWidget *content_area;
	EShell *shell;
	ECalClient *client;
	gboolean is_meeting;
	GtkWidget *alarm_page;
	GtkWidget *attendee_page;

	/* Chain up to parent's constructor() method. */
	object = G_OBJECT_CLASS (event_editor_parent_class)->constructor (
		type, n_construct_properties, construct_properties);

	editor = COMP_EDITOR (object);
	priv = EVENT_EDITOR_GET_PRIVATE (object);

	shell = comp_editor_get_shell (editor);

	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);
	action_group = comp_editor_get_action_group (editor, "coordinated");

	is_meeting = flags & COMP_EDITOR_MEETING;

	gtk_action_group_set_visible (action_group, is_meeting);

	priv->event_page = event_page_new (priv->model, editor);
	comp_editor_append_page (
		editor, COMP_EDITOR_PAGE (priv->event_page),
		_("Appointment"), TRUE);

	priv->recur_window = gtk_dialog_new_with_buttons (
		_("Recurrence"), GTK_WINDOW (editor), GTK_DIALOG_MODAL,
		_("_Close"), GTK_RESPONSE_CLOSE, NULL);
	g_signal_connect (
		priv->recur_window, "response",
		G_CALLBACK (gtk_widget_hide), NULL);
	g_signal_connect (
		priv->recur_window, "delete-event",
		G_CALLBACK (gtk_widget_hide_on_delete), NULL);

	content_area =
		gtk_dialog_get_content_area (GTK_DIALOG (priv->recur_window));

	priv->recur_page = recurrence_page_new (priv->model, editor);
	page = COMP_EDITOR_PAGE (priv->recur_page);
	if (!e_shell_get_express_mode (shell)) {
		gtk_container_add (
			GTK_CONTAINER (content_area),
			comp_editor_page_get_widget (page));
		gtk_widget_show_all (gtk_bin_get_child (GTK_BIN (priv->recur_window)));
		comp_editor_append_page (editor, page, NULL, FALSE);
	} else {
		comp_editor_append_page (editor, page, _("Recurrence"), TRUE);
	}

	if (e_shell_get_express_mode (shell)) {
		ENameSelector *name_selector;

		priv->sched_page = schedule_page_new (priv->model, editor);
		page = COMP_EDITOR_PAGE (priv->sched_page);

		name_selector = event_page_get_name_selector (priv->event_page);
		schedule_page_set_name_selector (priv->sched_page, name_selector);

		comp_editor_append_page (editor, page, _("Free/Busy"), TRUE);
		schedule_page_update_free_busy (priv->sched_page);

		e_binding_bind_property (
			action_group, "visible",
			comp_editor_page_get_widget (page), "visible",
			G_BINDING_SYNC_CREATE);

		/* Alarm page */
		alarm_page = event_page_get_alarm_page (priv->event_page);
		comp_editor_append_widget (editor, alarm_page, _("Reminder"), TRUE);
		g_object_unref (alarm_page);
	}

	if (is_meeting) {

		if (e_client_check_capability (E_CLIENT (client), CAL_STATIC_CAPABILITY_REQ_SEND_OPTIONS))
			event_page_show_options (priv->event_page);

		comp_editor_set_group_item (editor, TRUE);
		if (!((flags & COMP_EDITOR_USER_ORG) ||
			(flags & COMP_EDITOR_DELEGATE) ||
			(flags & COMP_EDITOR_NEW_ITEM))) {
			GtkAction *action;

			action = comp_editor_get_action (editor, "free-busy");
			gtk_action_set_visible (action, FALSE);
		}

		event_page_set_meeting (priv->event_page, TRUE);
		priv->meeting_shown = TRUE;

		if (e_shell_get_express_mode (shell)) {
			attendee_page = event_page_get_attendee_page (priv->event_page);
			comp_editor_append_widget (editor, attendee_page, _("Attendees"), TRUE);
			g_object_unref (attendee_page);
		}
	}

	return object;
}

static void
event_editor_dispose (GObject *object)
{
	EventEditorPrivate *priv;

	priv = EVENT_EDITOR_GET_PRIVATE (object);

	if (priv->event_page) {
		g_object_unref (priv->event_page);
		priv->event_page = NULL;
	}

	if (priv->recur_page) {
		g_object_unref (priv->recur_page);
		priv->recur_page = NULL;
	}

	if (priv->sched_page) {
		g_object_unref (priv->sched_page);
		priv->sched_page = NULL;
	}

	if (priv->model) {
		g_signal_handlers_disconnect_by_func (
			priv->model, event_editor_model_changed_cb, object);
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (event_editor_parent_class)->dispose (object);
}

static void
event_editor_constructed (GObject *object)
{
	EventEditorPrivate *priv;

	priv = EVENT_EDITOR_GET_PRIVATE (object);

	e_binding_bind_property (
		object, "client",
		priv->model, "client",
		G_BINDING_SYNC_CREATE);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (event_editor_parent_class)->constructed (object);
}

static void
event_editor_show_categories (CompEditor *editor,
                              gboolean visible)
{
	EventEditorPrivate *priv;

	priv = EVENT_EDITOR_GET_PRIVATE (editor);

	event_page_set_show_categories (priv->event_page, visible);
}

static void
event_editor_show_role (CompEditor *editor,
                        gboolean visible)
{
	EventEditorPrivate *priv;

	priv = EVENT_EDITOR_GET_PRIVATE (editor);

	event_page_set_view_role (priv->event_page, visible);
}

static void
event_editor_show_rsvp (CompEditor *editor,
                        gboolean visible)
{
	EventEditorPrivate *priv;

	priv = EVENT_EDITOR_GET_PRIVATE (editor);

	event_page_set_view_rsvp (priv->event_page, visible);
}

static void
event_editor_show_status (CompEditor *editor,
                          gboolean visible)
{
	EventEditorPrivate *priv;

	priv = EVENT_EDITOR_GET_PRIVATE (editor);

	event_page_set_view_status (priv->event_page, visible);
}

static void
event_editor_show_time_zone (CompEditor *editor,
                             gboolean visible)
{
	EventEditorPrivate *priv;

	priv = EVENT_EDITOR_GET_PRIVATE (editor);

	event_page_set_show_timezone (priv->event_page, visible);
}

static void
event_editor_show_type (CompEditor *editor,
                        gboolean visible)
{
	EventEditorPrivate *priv;

	priv = EVENT_EDITOR_GET_PRIVATE (editor);

	event_page_set_view_type (priv->event_page, visible);
}

static void
event_editor_class_init (EventEditorClass *class)
{
	GObjectClass *object_class;
	CompEditorClass *editor_class;

	g_type_class_add_private (class, sizeof (EventEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = event_editor_constructor;
	object_class->dispose = event_editor_dispose;
	object_class->constructed = event_editor_constructed;

	editor_class = COMP_EDITOR_CLASS (class);
	editor_class->help_section = "calendar-usage-add-appointment";
	editor_class->edit_comp = event_editor_edit_comp;
	editor_class->send_comp = event_editor_send_comp;
	editor_class->show_categories = event_editor_show_categories;
	editor_class->show_role = event_editor_show_role;
	editor_class->show_rsvp = event_editor_show_rsvp;
	editor_class->show_status = event_editor_show_status;;
	editor_class->show_time_zone = event_editor_show_time_zone;
	editor_class->show_type = event_editor_show_type;
}

static void
event_editor_init (EventEditor *ee)
{
	CompEditor *editor = COMP_EDITOR (ee);
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GtkAction *action;
	const gchar *id;
	GError *error = NULL;

	ee->priv = EVENT_EDITOR_GET_PRIVATE (ee);
	ee->priv->model = E_MEETING_STORE (e_meeting_store_new ());
	ee->priv->meeting_shown = TRUE;
	ee->priv->updating = FALSE;

	action_group = comp_editor_get_action_group (editor, "individual");
	gtk_action_group_add_actions (
		action_group, event_entries,
		G_N_ELEMENTS (event_entries), ee);
	gtk_action_group_add_toggle_actions (
		action_group, event_toggle_entries,
		G_N_ELEMENTS (event_toggle_entries), ee);

	action_group = comp_editor_get_action_group (editor, "editable");
	gtk_action_group_add_actions (
		action_group, editable_entries,
		G_N_ELEMENTS (editable_entries), ee);
	gtk_action_group_add_toggle_actions (
		action_group, editable_toggle_entries,
		G_N_ELEMENTS (editable_toggle_entries), ee);

	action_group = comp_editor_get_action_group (editor, "coordinated");
	gtk_action_group_add_actions (
		action_group, meeting_entries,
		G_N_ELEMENTS (meeting_entries), ee);

	ui_manager = comp_editor_get_ui_manager (editor);
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);

	id = "org.gnome.evolution.event-editor";
	e_plugin_ui_register_manager (ui_manager, id, ee);
	e_plugin_ui_enable_manager (ui_manager, id);

	if (error != NULL) {
		g_critical ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	action = comp_editor_get_action (editor, "print");
	gtk_action_set_tooltip (action, _("Print this event"));

	/* Hide send options. */
	action = comp_editor_get_action (editor, "send-options");
	gtk_action_set_visible (action, FALSE);

	g_signal_connect_swapped (
		ee->priv->model, "row_changed",
		G_CALLBACK (event_editor_model_changed_cb), ee);
	g_signal_connect_swapped (
		ee->priv->model, "row_inserted",
		G_CALLBACK (event_editor_model_changed_cb), ee);
	g_signal_connect_swapped (
		ee->priv->model, "row_deleted",
		G_CALLBACK (event_editor_model_changed_cb), ee);
}

static void
event_editor_edit_comp (CompEditor *editor,
                        ECalComponent *comp)
{
	EventEditorPrivate *priv;
	ECalComponentOrganizer organizer;
	gboolean delegate;
	ECalComponentDateTime dtstart, dtend;
	ECalClient *client;
	GSList *attendees = NULL;
	ESourceRegistry *registry;
	EShell *shell;

	priv = EVENT_EDITOR_GET_PRIVATE (editor);

	priv->updating = TRUE;
	delegate = (comp_editor_get_flags (COMP_EDITOR (editor)) & COMP_EDITOR_DELEGATE);

	if (priv->sched_page) {
		e_cal_component_get_dtstart (comp, &dtstart);
		e_cal_component_get_dtend (comp, &dtend);

		schedule_page_set_meeting_time (priv->sched_page, dtstart.value, dtend.value);

		e_cal_component_free_datetime (&dtstart);
		e_cal_component_free_datetime (&dtend);
	}

	if (COMP_EDITOR_CLASS (event_editor_parent_class)->edit_comp)
		COMP_EDITOR_CLASS (event_editor_parent_class)->edit_comp (editor, comp);

	shell = comp_editor_get_shell (editor);
	client = comp_editor_get_client (editor);

	registry = e_shell_get_registry (shell);

	/* Get meeting related stuff */
	e_cal_component_get_organizer (comp, &organizer);
	e_cal_component_get_attendee_list (comp, &attendees);

	/* Set up the attendees */
	if (attendees != NULL) {
		GSList *l;
		gint row;
		gchar *user_email;

		user_email = itip_get_comp_attendee (
			registry, comp, client);

		if (!priv->meeting_shown) {
			GtkAction *action;

			action = comp_editor_get_action (editor, "free-busy");
			gtk_action_set_visible (action, TRUE);
		}

		if (!(delegate && e_client_check_capability (
			E_CLIENT (client), CAL_STATIC_CAPABILITY_DELEGATE_TO_MANY))) {
			event_page_remove_all_attendees (priv->event_page);

			for (l = attendees; l != NULL; l = l->next) {
				ECalComponentAttendee *ca = l->data;
				EMeetingAttendee *ia;
				gboolean addresses_match;

				addresses_match = g_str_equal (
					user_email,
					itip_strip_mailto (ca->value));

				if (delegate && !addresses_match)
					continue;

				ia = E_MEETING_ATTENDEE (
					e_meeting_attendee_new_from_e_cal_component_attendee (ca));

				/* If we aren't the organizer or the attendee
				 * is just delegated, don't allow editing. */
				if (!comp_editor_get_user_org (editor) ||
					e_meeting_attendee_is_set_delto (ia))
					e_meeting_attendee_set_edit_level (
						ia,  E_MEETING_ATTENDEE_EDIT_NONE);

				comp_editor_page_add_attendee (
					COMP_EDITOR_PAGE (priv->event_page), ia);

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

				ia = e_meeting_store_find_attendee (
					priv->model, organizer.value, &row);
				if (ia != NULL)
					e_meeting_attendee_set_edit_level (
						ia, E_MEETING_ATTENDEE_EDIT_NONE);
			}
		}

		event_page_set_meeting (priv->event_page, TRUE);
		priv->meeting_shown = TRUE;
		g_free (user_email);
	}
	e_cal_component_free_attendee_list (attendees);

	comp_editor_set_needs_send (
		editor, priv->meeting_shown && (itip_organizer_is_user (
		registry, comp, client) || itip_sentby_is_user (registry,
		comp, client)));

	priv->updating = FALSE;
}

static gboolean
event_editor_send_comp (CompEditor *editor,
                        ECalComponentItipMethod method,
                        gboolean strip_alarms)
{
	EventEditorPrivate *priv;
	EShell *shell;
	ESourceRegistry *registry;
	ECalComponent *comp = NULL;

	priv = EVENT_EDITOR_GET_PRIVATE (editor);

	/* Don't cancel more than once or when just publishing */
	if (method == E_CAL_COMPONENT_METHOD_PUBLISH ||
	    method == E_CAL_COMPONENT_METHOD_CANCEL)
		goto parent;

	shell = comp_editor_get_shell (editor);
	registry = e_shell_get_registry (shell);

	comp = event_page_get_cancel_comp (priv->event_page);
	if (comp != NULL) {
		ECalClient *client;
		gboolean result;

		client = e_meeting_store_get_client (priv->model);
		result = itip_send_comp_sync (
			registry, E_CAL_COMPONENT_METHOD_CANCEL, comp,
			client, NULL, NULL, NULL, strip_alarms, FALSE, NULL, NULL);
		g_object_unref (comp);

		if (!result)
			return result;
	}

 parent:
	if (COMP_EDITOR_CLASS (event_editor_parent_class)->send_comp)
		return COMP_EDITOR_CLASS (event_editor_parent_class)->
			send_comp (editor, method, strip_alarms);

	return FALSE;
}

/**
 * event_editor_new:
 * @client: a ECalClient
 *
 * Creates a new event editor dialog.
 *
 * Return value: A newly-created event editor dialog, or NULL if the event
 * editor could not be created.
 **/
CompEditor *
event_editor_new (ECalClient *client,
                  EShell *shell,
                  CompEditorFlags flags)
{
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return g_object_new (
		TYPE_EVENT_EDITOR,
		"client", client, "flags", flags, "shell", shell, NULL);
}

void
event_editor_show_meeting (EventEditor *ee)
{
	CompEditor *editor;
	CompEditorFlags flags;

	g_return_if_fail (IS_EVENT_EDITOR (ee));

	editor = COMP_EDITOR (ee);
	flags = comp_editor_get_flags (editor);

	event_page_set_meeting (ee->priv->event_page, TRUE);
	if (!ee->priv->meeting_shown) {
		GtkAction *action;

		action = comp_editor_get_action (editor, "free-busy");
		gtk_action_set_visible (action, TRUE);

		ee->priv->meeting_shown = TRUE;

		comp_editor_set_changed (editor, FALSE);
		comp_editor_set_needs_send (editor, TRUE);
	}

	if (!(flags & COMP_EDITOR_NEW_ITEM) && !(flags & COMP_EDITOR_USER_ORG))
		gtk_drag_dest_unset (GTK_WIDGET (editor));
}
