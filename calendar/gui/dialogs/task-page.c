/*
 * Evolution calendar - Main page of the task editor dialog
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *		Miguel de Icaza <miguel@ximian.com>
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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "e-util/e-util.h"

#include "../e-meeting-attendee.h"
#include "../e-meeting-list-view.h"
#include "../e-meeting-store.h"
#include "../e-timezone-entry.h"

#include "comp-editor.h"
#include "comp-editor-util.h"
#include "e-send-options-utils.h"
#include "task-page.h"

#define TASK_PAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_TASK_PAGE, TaskPagePrivate))

struct _TaskPagePrivate {
	GtkBuilder *builder;

	/* Widgets from the UI file */
	GtkWidget *main;

	gchar **address_strings;
	gchar *fallback_address;
	EMeetingAttendee *ia;
	gchar *user_add;
	ECalComponent *comp;

	/* For meeting/event */
	GtkWidget *calendar_label;
	GtkWidget *org_cal_label;
	GtkWidget *attendee_box;

	/* Lists of attendees */
	GPtrArray *deleted_attendees;

	/* Generic informative messages placeholder */
	GtkWidget *info_hbox;
	GtkWidget *info_icon;
	GtkWidget *info_string;

	GtkWidget *summary;
	GtkWidget *summary_label;

	GtkWidget *due_date;
	GtkWidget *start_date;
	GtkWidget *completed_date;
	GtkWidget *timezone;
	GtkWidget *timezone_label;

	GtkWidget *status_combo;
	GtkWidget *priority_combo;
	GtkWidget *percent_complete;
	GtkWidget *classification_combo;
	GtkWidget *web_page_entry;

	GtkWidget *description;

	GtkWidget *categories_btn;
	GtkWidget *categories;

	GtkWidget *client_combo_box;

	/* Meeting related items */
	GtkWidget *list_box;
	GtkWidget *organizer_table;
	GtkWidget *organizer;
	GtkWidget *add;
	GtkWidget *remove;
	GtkWidget *edit;
	GtkWidget *invite;

	/* ListView stuff */
	ECalClient *client;
	EMeetingStore *meeting_store;
	EMeetingListView *list_view;
	gint row;

	/* For handling who the organizer is */
	gboolean user_org;
	gboolean existing;

	gboolean sendoptions_shown;
	gboolean is_assignment;

	ESendOptionsDialog *sod;

	GCancellable *connect_cancellable;
};

static const gint classification_map[] = {
	E_CAL_COMPONENT_CLASS_PUBLIC,
	E_CAL_COMPONENT_CLASS_PRIVATE,
	E_CAL_COMPONENT_CLASS_CONFIDENTIAL,
	-1
};

/* Note that these two arrays must match. */
static const gint status_map[] = {
	ICAL_STATUS_NONE,
	ICAL_STATUS_INPROCESS,
	ICAL_STATUS_COMPLETED,
	ICAL_STATUS_CANCELLED,
	-1
};

typedef enum {
	PRIORITY_HIGH,
	PRIORITY_NORMAL,
	PRIORITY_LOW,
	PRIORITY_UNDEFINED
} TaskEditorPriority;

static const gint priority_map[] = {
	PRIORITY_HIGH,
	PRIORITY_NORMAL,
	PRIORITY_LOW,
	PRIORITY_UNDEFINED,
	-1
};

static gboolean task_page_fill_timezones (CompEditorPage *page, GHashTable *timezones);
static void task_page_select_organizer (TaskPage *tpage, const gchar *backend_address);
static void set_subscriber_info_string (TaskPage *tpage, const gchar *backend_address);

G_DEFINE_TYPE (TaskPage, task_page, TYPE_COMP_EDITOR_PAGE)

static TaskEditorPriority
priority_value_to_index (gint priority_value)
{
	TaskEditorPriority retval;

	if (priority_value == 0)
		retval = PRIORITY_UNDEFINED;
	else if (priority_value <= 4)
		retval = PRIORITY_HIGH;
	else if (priority_value == 5)
		retval = PRIORITY_NORMAL;
	else
		retval = PRIORITY_LOW;

	return retval;
}

static gint
priority_index_to_value (TaskEditorPriority priority)
{
	gint retval;

	switch (priority) {
	case PRIORITY_UNDEFINED:
		retval = 0;
		break;
	case PRIORITY_HIGH:
		retval = 3;
		break;
	case PRIORITY_NORMAL:
		retval = 5;
		break;
	case PRIORITY_LOW:
		retval = 7;
		break;
	default:
		retval = 0;
		break;
	}

	return retval;
}

static gboolean
get_current_identity (TaskPage *page,
                      gchar **name,
                      gchar **mailto)
{
	EShell *shell;
	CompEditor *editor;
	ESourceRegistry *registry;
	GList *list, *iter;
	GtkWidget *entry;
	const gchar *extension_name;
	const gchar *text;
	gboolean match = FALSE;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (page));
	shell = comp_editor_get_shell (editor);

	entry = gtk_bin_get_child (GTK_BIN (page->priv->organizer));
	text = gtk_entry_get_text (GTK_ENTRY (entry));

	if (text == NULL || *text == '\0')
		return FALSE;

	registry = e_shell_get_registry (shell);
	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;

	list = e_source_registry_list_sources (registry, extension_name);

	for (iter = list; !match && iter != NULL; iter = g_list_next (iter)) {
		ESource *source = E_SOURCE (iter->data);
		ESourceMailIdentity *extension;
		const gchar *id_name;
		const gchar *id_address;
		gchar *identity;

		extension = e_source_get_extension (source, extension_name);

		id_name = e_source_mail_identity_get_name (extension);
		id_address = e_source_mail_identity_get_address (extension);

		if (id_name == NULL || id_address == NULL)
			continue;

		identity = g_strdup_printf ("%s <%s>", id_name, id_address);
		match = (g_ascii_strcasecmp (text, identity) == 0);
		g_free (identity);

		if (match && name != NULL)
			*name = g_strdup (id_name);

		if (match && mailto != NULL)
			*mailto = g_strdup_printf ("MAILTO:%s", id_address);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	return match;
}

/* Fills the widgets with default values */
static void
clear_widgets (TaskPage *tpage)
{
	TaskPagePrivate *priv = tpage->priv;
	CompEditor *editor;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (tpage));

	/* Summary, description */
	gtk_entry_set_text (GTK_ENTRY (priv->summary), "");
	gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description)), "", 0);
	e_buffer_tagger_update_tags (GTK_TEXT_VIEW (priv->description));

	/* Start, due times - both set to None */
	e_date_edit_set_time (E_DATE_EDIT (priv->start_date), -1);
	e_date_edit_set_time (E_DATE_EDIT (priv->due_date), -1);

	/* Classification */
	comp_editor_set_classification (editor, E_CAL_COMPONENT_CLASS_PUBLIC);

	/* Categories */
	gtk_entry_set_text (GTK_ENTRY (priv->categories), "");

	e_date_edit_set_time (E_DATE_EDIT (priv->completed_date), -1);
	e_dialog_combo_box_set (priv->status_combo, ICAL_STATUS_NONE, status_map);
	e_dialog_combo_box_set (priv->priority_combo, PRIORITY_UNDEFINED, priority_map);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->percent_complete), 0);

	gtk_entry_set_text (GTK_ENTRY (priv->web_page_entry), "");
}

static gboolean
date_in_past (TaskPage *tpage,
              EDateEdit *date)
{
	struct icaltimetype tt = icaltime_null_time ();

	if (!e_date_edit_get_date (date, &tt.year, &tt.month, &tt.day))
		return FALSE;

	if (e_date_edit_get_time_of_day (date, &tt.hour, &tt.minute))
		tt.zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (tpage->priv->timezone));
	else
		tt.is_date = TRUE;

	return comp_editor_test_time_in_the_past (tt);
}

/* returns whether changed info text */
static gboolean
check_starts_in_the_past (TaskPage *tpage)
{
	TaskPagePrivate *priv;
	gboolean start_in_past, due_in_past;

	if ((comp_editor_get_flags (comp_editor_page_get_editor (COMP_EDITOR_PAGE (tpage))) & COMP_EDITOR_NEW_ITEM) == 0)
		return FALSE;

	priv = tpage->priv;
	start_in_past = date_in_past (tpage, E_DATE_EDIT (priv->start_date));
	due_in_past = date_in_past (tpage, E_DATE_EDIT (priv->due_date));

	if (start_in_past || due_in_past) {
		gchar *tmp = g_strconcat (
			"<b>", start_in_past ? _("Task's start date is in the past") : "",
			start_in_past && due_in_past ? "\n" : "", due_in_past ? _("Task's due date is in the past") : "", "</b>", NULL);
		task_page_set_info_string (tpage, "dialog-warning", tmp);
		g_free (tmp);
	} else {
		task_page_set_info_string (tpage, NULL, NULL);
	}

	return TRUE;
}

static void
sensitize_widgets (TaskPage *tpage)
{
	TaskPagePrivate *priv = tpage->priv;
	CompEditor *editor;
	CompEditorFlags flags;
	ECalClient *client;
	GtkActionGroup *action_group;
	GtkAction *action;
	gboolean read_only, sens = TRUE, sensitize;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (tpage));
	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);

	read_only = e_client_is_readonly (E_CLIENT (client));

	if (flags & COMP_EDITOR_IS_ASSIGNED)
		sens = flags & COMP_EDITOR_USER_ORG;

	sensitize = (!read_only && sens);

	if (read_only) {
		gchar *tmp = g_strconcat ("<b>", _("Task cannot be edited, because the selected task list is read only"), "</b>", NULL);
		task_page_set_info_string (tpage, "dialog-information", tmp);
		g_free (tmp);
	} else if (!sens) {
		gchar *tmp = g_strconcat ("<b>", _("Task cannot be fully edited, because you are not the organizer"), "</b>", NULL);
		task_page_set_info_string (tpage, "dialog-information", tmp);
		g_free (tmp);
	} else if ((flags & COMP_EDITOR_IS_ASSIGNED) != 0 && e_client_check_capability (E_CLIENT (client), CAL_STATIC_CAPABILITY_NO_TASK_ASSIGNMENT)) {
		gchar *tmp = g_strconcat ("<b>", _("Task cannot be edited, because the selected task list does not support assigned tasks"), "</b>", NULL);
		task_page_set_info_string (tpage, "dialog-information", tmp);
		g_free (tmp);
		sens = FALSE;
		read_only = TRUE;
	} else if (!check_starts_in_the_past (tpage)) {
		task_page_set_info_string (tpage, NULL, NULL);
	}

	/* The list of organizers is set to be non-editable. Otherwise any
	 * change in the displayed list causes an 'Account not found' error.
	 */
	gtk_editable_set_editable (GTK_EDITABLE (gtk_bin_get_child (GTK_BIN (priv->organizer))), FALSE);

	gtk_editable_set_editable (GTK_EDITABLE (priv->summary), !read_only);
	gtk_widget_set_sensitive (priv->due_date, !read_only);
	gtk_widget_set_sensitive (priv->start_date, !read_only);
	gtk_widget_set_sensitive (priv->timezone, !read_only);
	gtk_widget_set_sensitive (priv->description, !read_only);
	gtk_widget_set_sensitive (priv->categories_btn, !read_only);
	gtk_editable_set_editable (GTK_EDITABLE (priv->categories), !read_only);

	gtk_widget_set_sensitive (priv->completed_date, !read_only);
	gtk_widget_set_sensitive (priv->status_combo, !read_only);
	gtk_widget_set_sensitive (priv->priority_combo, !read_only);
	gtk_widget_set_sensitive (priv->percent_complete, !read_only);
	gtk_widget_set_sensitive (priv->classification_combo, !read_only);

	gtk_editable_set_editable (
		GTK_EDITABLE (priv->web_page_entry), !read_only);

	gtk_widget_set_sensitive (priv->organizer, !read_only);
	gtk_widget_set_sensitive (priv->add, (!read_only && sens));
	gtk_widget_set_sensitive (priv->edit, (!read_only && sens));
	e_meeting_list_view_set_editable (priv->list_view, (!read_only && sens));
	gtk_widget_set_sensitive (priv->remove, (!read_only && sens));
	gtk_widget_set_sensitive (priv->invite, (!read_only && sens));
	gtk_widget_set_sensitive (GTK_WIDGET (priv->list_view), !read_only);

	action_group = comp_editor_get_action_group (editor, "editable");
	gtk_action_group_set_sensitive (action_group, !read_only);

	action_group = comp_editor_get_action_group (editor, "individual");
	gtk_action_group_set_sensitive (action_group, sensitize);

	action = comp_editor_get_action (editor, "send-options");
	gtk_action_set_sensitive (action, sensitize);

	if (!priv->is_assignment) {
		gtk_widget_hide (priv->calendar_label);
		gtk_widget_hide (priv->list_box);
		gtk_widget_hide (priv->attendee_box);
		gtk_widget_hide (priv->organizer);
		gtk_widget_hide (priv->invite);
		gtk_label_set_text_with_mnemonic (GTK_LABEL (priv->org_cal_label), _("_List:"));
		gtk_label_set_mnemonic_widget (GTK_LABEL (priv->org_cal_label), priv->client_combo_box);
	} else {
		gtk_widget_show (priv->invite);
		gtk_widget_show (priv->calendar_label);
		gtk_widget_show (priv->list_box);
		gtk_widget_show (priv->attendee_box);
		gtk_widget_show (priv->organizer);
		gtk_label_set_text_with_mnemonic (GTK_LABEL (priv->org_cal_label), _("Organi_zer:"));
		gtk_label_set_mnemonic_widget (GTK_LABEL (priv->org_cal_label), priv->organizer);
	}
}

static void
set_attendees (ECalComponent *comp,
               const GPtrArray *attendees)
{
	GSList *comp_attendees = NULL, *l;
	gint i;

	for (i = 0; i < attendees->len; i++) {
		EMeetingAttendee *ia = g_ptr_array_index (attendees, i);
		ECalComponentAttendee *ca;

		ca = e_meeting_attendee_as_e_cal_component_attendee (ia);

		comp_attendees = g_slist_prepend (comp_attendees, ca);

	}
	comp_attendees = g_slist_reverse (comp_attendees);

	e_cal_component_set_attendee_list (comp, comp_attendees);

	for (l = comp_attendees; l != NULL; l = l->next)
		g_free (l->data);
	g_slist_free (comp_attendees);
}

static void
organizer_changed_cb (GtkEntry *entry,
                      TaskPage *tpage)
{
	gchar *name;
	gchar *mailto;

	g_return_if_fail (GTK_IS_ENTRY (entry));
	g_return_if_fail (IS_TASK_PAGE (tpage));

	if (!tpage->priv->ia)
		return;

	if (!get_current_identity (tpage, &name, &mailto))
		return;

	/* XXX EMeetingAttendee takes ownership of the strings. */
	e_meeting_attendee_set_cn (tpage->priv->ia, name);
	e_meeting_attendee_set_address (tpage->priv->ia, mailto);
}

static void
task_page_dispose (GObject *object)
{
	TaskPagePrivate *priv;

	priv = TASK_PAGE_GET_PRIVATE (object);

	if (priv->connect_cancellable != NULL) {
		g_cancellable_cancel (priv->connect_cancellable);
		g_object_unref (priv->connect_cancellable);
		priv->connect_cancellable = NULL;
	}

	if (priv->main != NULL) {
		g_object_unref (priv->main);
		priv->main = NULL;
	}

	if (priv->builder != NULL) {
		g_object_unref (priv->builder);
		priv->builder = NULL;
	}

	if (priv->sod != NULL) {
		g_object_unref (priv->sod);
		priv->sod = NULL;
	}

	if (priv->comp != NULL) {
		g_object_unref (priv->comp);
		priv->comp = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (task_page_parent_class)->dispose (object);
}

static void
task_page_finalize (GObject *object)
{
	TaskPagePrivate *priv;

	priv = TASK_PAGE_GET_PRIVATE (object);

	g_strfreev (priv->address_strings);
	g_free (priv->fallback_address);
	g_free (priv->user_add);

	g_ptr_array_foreach (
		priv->deleted_attendees, (GFunc) g_object_unref, NULL);
	g_ptr_array_free (priv->deleted_attendees, TRUE);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (task_page_parent_class)->finalize (object);
}

static GtkWidget *
task_page_get_widget (CompEditorPage *page)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;

	return priv->main;
}

static void
task_page_focus_main_widget (CompEditorPage *page)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;

	gtk_widget_grab_focus (priv->summary);
}

static gboolean
task_page_fill_widgets (CompEditorPage *page,
                        ECalComponent *comp)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	ECalComponentText text;
	ECalComponentDateTime d;
	ECalComponentClassification cl;
	ESourceRegistry *registry;
	CompEditor *editor;
	CompEditorFlags flags;
	GtkAction *action;
	ECalClient *client;
	EShell *shell;
	GSList *l;
	icalcomponent *icalcomp = NULL;
	const gchar *categories, *uid;
	icaltimezone *zone, *default_zone;
	gchar *backend_addr = NULL;
	gboolean active;
	gint *priority_value, *percent = NULL;
	TaskEditorPriority priority;
	icalproperty_status status;
	const gchar *url;
	struct icaltimetype *completed = NULL;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;

	editor = comp_editor_page_get_editor (page);
	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);
	shell = comp_editor_get_shell (editor);

	registry = e_shell_get_registry (shell);

	/* Clean out old data */
	if (priv->comp != NULL)
		g_object_unref (priv->comp);
	priv->comp = NULL;

	g_ptr_array_foreach (
		priv->deleted_attendees, (GFunc) g_object_unref, NULL);
	g_ptr_array_set_size (priv->deleted_attendees, 0);

	/* Component for cancellation */
	priv->comp = e_cal_component_clone (comp);
	comp_editor_copy_new_attendees (priv->comp, comp);

	/* Clean the screen */
	clear_widgets (tpage);

	priv->user_add = itip_get_comp_attendee (
		registry, comp, client);

        /* Summary, description(s) */
	e_cal_component_get_summary (comp, &text);
	if (text.value != NULL)
		gtk_entry_set_text (GTK_ENTRY (priv->summary), text.value);
	else
		gtk_entry_set_text (GTK_ENTRY (priv->summary), "");

	e_cal_component_get_description_list (comp, &l);
	if (l && l->data) {
		ECalComponentText *dtext;

		dtext = l->data;
		gtk_text_buffer_set_text (
			gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description)),
			dtext->value ? dtext->value : "", -1);
	} else {
		gtk_text_buffer_set_text (
			gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description)),
			"", 0);
	}
	e_cal_component_free_text_list (l);
	e_buffer_tagger_update_tags (GTK_TEXT_VIEW (priv->description));

	default_zone = comp_editor_get_timezone (editor);

	/* Due Date. */
	e_cal_component_get_due (comp, &d);
	zone = NULL;
	if (d.value) {
		struct icaltimetype *due_tt = d.value;
		e_date_edit_set_date (
			E_DATE_EDIT (priv->due_date),
			due_tt->year, due_tt->month,
			due_tt->day);
		e_date_edit_set_time_of_day (E_DATE_EDIT (priv->due_date), -1, -1);
	} else {
		e_date_edit_set_time (E_DATE_EDIT (priv->due_date), -1);

		/* If no time is set, we use the default timezone, so the
		 * user usually doesn't have to set this when they set the
		 * date. */
		zone = NULL;
	}

	/* Note that if we are creating a new task, the timezones may not be
	 * on the server, so we try to get the builtin timezone with the TZID
	 * first. */
	if (!zone && d.tzid) {
		GError *error = NULL;

		e_cal_client_get_timezone_sync (
			client, d.tzid, &zone, NULL, &error);

		if (error != NULL) {
			/* FIXME: Handle error better. */
			g_warning (
				"Couldn't get timezone '%s' from server: %s",
				d.tzid ? d.tzid : "", error->message);
			g_error_free (error);
		}
	}

	e_timezone_entry_set_timezone (
		E_TIMEZONE_ENTRY (priv->timezone),
		zone ? zone : default_zone);

	action = comp_editor_get_action (editor, "view-time-zone");
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	task_page_set_show_timezone (tpage, active);

	if (!(flags & COMP_EDITOR_NEW_ITEM) && !zone) {
		GtkAction *action;

		task_page_set_show_timezone (tpage, FALSE);
		action = comp_editor_get_action (editor, "view-time-zone");
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
	}

	e_cal_component_free_datetime (&d);

	/* Start Date. */
	e_cal_component_get_dtstart (comp, &d);
	zone = NULL;
	if (d.value) {
		struct icaltimetype *start_tt = d.value;
		e_date_edit_set_date (
			E_DATE_EDIT (priv->start_date),
			start_tt->year, start_tt->month,
			start_tt->day);
		e_date_edit_set_time_of_day (E_DATE_EDIT (priv->start_date), -1, -1);
	} else {
		e_date_edit_set_time (E_DATE_EDIT (priv->start_date), -1);
	}

	e_cal_component_free_datetime (&d);

	/* Classification. */
	e_cal_component_get_classification (comp, &cl);
	comp_editor_set_classification (editor, cl);

	e_cal_component_get_uid (comp, &uid);
	e_cal_client_get_object_sync (
		client, uid, NULL, &icalcomp, NULL, NULL);
	if (icalcomp != NULL) {
		icalcomponent_free (icalcomp);
		task_page_hide_options (tpage);
	}

	/* Categories */
	e_cal_component_get_categories (comp, &categories);
	if (categories != NULL)
		gtk_entry_set_text (GTK_ENTRY (priv->categories), categories);
	else
		gtk_entry_set_text (GTK_ENTRY (priv->categories), "");

	/* Source */
	e_source_combo_box_set_active (
		E_SOURCE_COMBO_BOX (priv->client_combo_box),
		e_client_get_source (E_CLIENT (client)));

	e_client_get_backend_property_sync (E_CLIENT (client), CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS, &backend_addr, NULL, NULL);
	set_subscriber_info_string (tpage, backend_addr);

	if (priv->is_assignment) {
		ECalComponentOrganizer organizer;
		gchar *name = NULL;
		gchar *mailto = NULL;

		g_free (priv->user_add);
		priv->user_add = itip_get_comp_attendee (
			registry, comp, client);

		/* Organizer strings */
		task_page_select_organizer (tpage, backend_addr);

		/* If there is an existing organizer show it properly */
		if (e_cal_component_has_organizer (comp)) {
			e_cal_component_get_organizer (comp, &organizer);
			if (organizer.value != NULL) {
				const gchar *strip = itip_strip_mailto (organizer.value);
				gchar *string;

				if (itip_organizer_is_user (registry, comp, client) ||
				    itip_sentby_is_user (registry, comp, client)) {
					priv->user_org = TRUE;
				} else {
					if (e_client_check_capability (E_CLIENT (client), CAL_STATIC_CAPABILITY_ORGANIZER_NOT_EMAIL_ADDRESS))
						gtk_widget_set_sensitive (priv->invite, FALSE);
					gtk_widget_set_sensitive (priv->add, FALSE);
					gtk_widget_set_sensitive (priv->edit, FALSE);
					gtk_widget_set_sensitive (priv->remove, FALSE);
					priv->user_org = FALSE;
				}

				if (e_client_check_capability (E_CLIENT (client), CAL_STATIC_CAPABILITY_NO_ORGANIZER) && (flags & COMP_EDITOR_DELEGATE))
					string = g_strdup (priv->user_add);
				else if (organizer.cn != NULL)
					string = g_strdup_printf ("%s <%s>", organizer.cn, strip);
				else
					string = g_strdup (strip);

				g_signal_handlers_block_by_func (gtk_bin_get_child (GTK_BIN (priv->organizer)), organizer_changed_cb, tpage);

				if (!priv->user_org) {
					GtkComboBox *combo_box;
					GtkListStore *list_store;
					GtkTreeModel *model;
					GtkTreeIter iter;

					combo_box = GTK_COMBO_BOX (priv->organizer);
					model = gtk_combo_box_get_model (combo_box);
					list_store = GTK_LIST_STORE (model);

					gtk_list_store_clear (list_store);
					gtk_list_store_append (list_store, &iter);
					gtk_list_store_set (list_store, &iter, 0, string, -1);
					gtk_combo_box_set_active (combo_box, 0);
					gtk_editable_set_editable (GTK_EDITABLE (gtk_bin_get_child (GTK_BIN (priv->organizer))), FALSE);
				} else {
					gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->organizer))), string);
				}

				g_signal_handlers_unblock_by_func (gtk_bin_get_child (GTK_BIN (priv->organizer)), organizer_changed_cb, tpage);

				g_free (string);
				priv->existing = TRUE;
			}
		} else if (get_current_identity (tpage, &name, &mailto)) {
			EMeetingAttendee *attendee;
			gchar *backend_mailto = NULL;

			if (backend_addr != NULL && *backend_addr != '\0') {
				backend_mailto = g_strdup_printf (
					"MAILTO:%s", backend_addr);
				if (g_ascii_strcasecmp (backend_mailto, mailto) == 0) {
					g_free (backend_mailto);
					backend_mailto = NULL;
				}
			}

			attendee =
				e_meeting_store_add_attendee_with_defaults (
				priv->meeting_store);
			priv->ia = g_object_ref (attendee);

			if (backend_mailto == NULL) {
				e_meeting_attendee_set_cn (attendee, name);
				e_meeting_attendee_set_address (attendee, mailto);
				name = mailto = NULL;
			} else {
				e_meeting_attendee_set_address (attendee, backend_mailto);
				e_meeting_attendee_set_sentby (attendee, mailto);
				backend_mailto = mailto = NULL;
			}

			if (client && e_cal_client_check_organizer_must_accept (client))
				e_meeting_attendee_set_status (
					attendee, ICAL_PARTSTAT_NEEDSACTION);
			else
				e_meeting_attendee_set_status (
					attendee, ICAL_PARTSTAT_ACCEPTED);

			e_meeting_list_view_add_attendee_to_name_selector (
				E_MEETING_LIST_VIEW (priv->list_view), attendee);

			g_free (backend_mailto);
		}

		g_free (mailto);
		g_free (name);
	}

	g_free (backend_addr);

	/* Percent Complete. */
	e_cal_component_get_percent (comp, &percent);
	if (percent) {
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (priv->percent_complete), *percent);
	} else {
		/* FIXME: Could check if task is completed and set 100%. */
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (priv->percent_complete), 0);
	}

	/* Status. */
	e_cal_component_get_status (comp, &status);
	if (status == ICAL_STATUS_NONE || status == ICAL_STATUS_NEEDSACTION) {
		/* Try to use the percent value. */
		if (percent) {
			if (*percent == 100)
				status = ICAL_STATUS_COMPLETED;
			else if (*percent > 0)
				status = ICAL_STATUS_INPROCESS;
			else
				status = ICAL_STATUS_NONE;
		} else
			status = ICAL_STATUS_NONE;
	}
	e_dialog_combo_box_set (priv->status_combo, status, status_map);

	if (percent)
		e_cal_component_free_percent (percent);

	/* Completed Date. */
	e_cal_component_get_completed (comp, &completed);
	if (completed) {
		icaltimezone *utc_zone, *zone;

		/* Completed is in UTC, but that would confuse the user, so
		 * we convert it to local time. */
		utc_zone = icaltimezone_get_utc_timezone ();
		zone = comp_editor_get_timezone (editor);

		icaltimezone_convert_time (completed, utc_zone, zone);

		e_date_edit_set_date (
			E_DATE_EDIT (priv->completed_date),
			completed->year, completed->month,
			completed->day);
		e_date_edit_set_time_of_day (
			E_DATE_EDIT (priv->completed_date),
			completed->hour,
			completed->minute);

		e_cal_component_free_icaltimetype (completed);
	}

	/* Priority. */
	e_cal_component_get_priority (comp, &priority_value);
	if (priority_value) {
		priority = priority_value_to_index (*priority_value);
		e_cal_component_free_priority (priority_value);
	} else {
		priority = PRIORITY_UNDEFINED;
	}
	e_dialog_combo_box_set (priv->priority_combo, priority, priority_map);

	/* URL */
	e_cal_component_get_url (comp, &url);
	gtk_entry_set_text (GTK_ENTRY (priv->web_page_entry), url ? url : "");

	sensitize_widgets (tpage);

	e_widget_undo_reset (priv->summary);
	e_widget_undo_reset (priv->categories);
	e_widget_undo_reset (priv->web_page_entry);
	e_widget_undo_reset (priv->description);

	return TRUE;
}

static gboolean
task_page_fill_component (CompEditorPage *page,
                          ECalComponent *comp)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	ECalComponentClassification classification;
	ECalComponentDateTime date;
	CompEditor *editor;
	CompEditorFlags flags;
	ECalClient *client;
	struct icaltimetype start_tt, due_tt;
	gchar *cat, *str;
	gboolean start_date_set, due_date_set;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;
	struct icaltimetype icalcomplete, icaltoday;
	icalproperty_status status;
	TaskEditorPriority priority;
	gint priority_value, percent;
	const gchar *text;
	gboolean date_set;
	icaltimezone *zone;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description));

	editor = comp_editor_page_get_editor (page);
	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);
	zone = comp_editor_get_timezone (editor);

	/* Summary. */

	str = gtk_editable_get_chars (GTK_EDITABLE (priv->summary), 0, -1);
	if (str == NULL || *str == '\0')
		e_cal_component_set_summary (comp, NULL);
	else {
		ECalComponentText text;

		text.value = str;
		text.altrep = NULL;

		e_cal_component_set_summary (comp, &text);
	}

	g_free (str);

	/* Description */

	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	if (!str || strlen (str) == 0)
		e_cal_component_set_description_list (comp, NULL);
	else {
		GSList l;
		ECalComponentText text;

		text.value = str;
		text.altrep = NULL;
		l.data = &text;
		l.next = NULL;

		e_cal_component_set_description_list (comp, &l);
	}

	g_free (str);

	/* Dates */

	due_tt = icaltime_null_time ();

	date.value = &due_tt;
	date.tzid = NULL;

	/* Due Date. */
	if (!e_date_edit_date_is_valid (E_DATE_EDIT (priv->due_date))) {
		comp_editor_page_display_validation_error (page, _("Due date is wrong"), priv->due_date);
		return FALSE;
	}

	due_date_set = e_date_edit_get_date (
		E_DATE_EDIT (priv->due_date),
		&due_tt.year,
		&due_tt.month,
		&due_tt.day);
	if (due_date_set) {
		due_tt.is_date = TRUE;
		date.tzid = NULL;
		e_cal_component_set_due (comp, &date);
	} else {
		e_cal_component_set_due (comp, NULL);
	}

	/* Start Date. */
	if (!e_date_edit_date_is_valid (E_DATE_EDIT (priv->start_date))) {
		comp_editor_page_display_validation_error (page, _("Start date is wrong"), priv->start_date);
		return FALSE;
	}

	start_tt = icaltime_null_time ();
	date.value = &start_tt;
	start_date_set = e_date_edit_get_date (
		E_DATE_EDIT (priv->start_date),
		&start_tt.year,
		&start_tt.month,
		&start_tt.day);
	if (start_date_set) {
		start_tt.is_date = TRUE;
		date.tzid = NULL;
		e_cal_component_set_dtstart (comp, &date);
	} else {
		e_cal_component_set_dtstart (comp, NULL);
	}

	/* Classification. */
	classification = comp_editor_get_classification (editor);
	e_cal_component_set_classification (comp, classification);

	/* send options */
	if (priv->sendoptions_shown && priv->sod) {
		icaltimezone *zone = comp_editor_get_timezone (editor);
		e_send_options_utils_fill_component (priv->sod, comp, zone);
	}

	/* Categories */
	cat = gtk_editable_get_chars (GTK_EDITABLE (priv->categories), 0, -1);
	str = comp_editor_strip_categories (cat);
	g_free (cat);

	e_cal_component_set_categories (comp, str);

	if (str)
		g_free (str);

	if (priv->is_assignment) {
		ECalComponentOrganizer organizer = {NULL, NULL, NULL, NULL};

		if (!priv->existing) {
			gchar *backend_addr = NULL;
			gchar *backend_mailto = NULL;
			gchar *name;
			gchar *mailto;

			e_client_get_backend_property_sync (E_CLIENT (client), CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS, &backend_addr, NULL, NULL);

			/* Find the identity for the organizer or sentby field */
			if (!get_current_identity (tpage, &name, &mailto)) {
				e_notice (
					priv->main, GTK_MESSAGE_ERROR,
					_("An organizer is required."));
				return FALSE;
			}

			/* Prefer the backend address if we have one. */
			if (backend_addr != NULL && *backend_addr != '\0') {
				backend_mailto = g_strdup_printf (
					"MAILTO:%s", backend_addr);
				if (g_ascii_strcasecmp (backend_mailto, mailto) == 0) {
					g_free (backend_mailto);
					backend_mailto = NULL;
				}
			}

			if (backend_mailto == NULL) {
				organizer.cn = name;
				organizer.value = mailto;
				name = mailto = NULL;
			} else {
				organizer.value = backend_mailto;
				organizer.sentby = mailto;
				backend_mailto = mailto = NULL;
			}

			e_cal_component_set_organizer (comp, &organizer);

			g_free (backend_addr);
			g_free (backend_mailto);
			g_free (name);
			g_free (mailto);
		}

		if (e_meeting_store_count_actual_attendees (priv->meeting_store) < 1) {
			e_notice (
				priv->main, GTK_MESSAGE_ERROR,
				_("At least one attendee is required."));
			return FALSE;
		}

		if (flags & COMP_EDITOR_DELEGATE) {
			GSList *attendee_list, *l;
			gint i;
			const GPtrArray *attendees = e_meeting_store_get_attendees (priv->meeting_store);

			e_cal_component_get_attendee_list (priv->comp, &attendee_list);

			for (i = 0; i < attendees->len; i++) {
				EMeetingAttendee *ia = g_ptr_array_index (attendees, i);
				ECalComponentAttendee *ca;

				/* Remove the duplicate user from the component if present */
				if (e_meeting_attendee_is_set_delfrom (ia) || e_meeting_attendee_is_set_delto (ia)) {
					for (l = attendee_list; l; l = l->next) {
						ECalComponentAttendee *a = l->data;

						if (g_str_equal (a->value, e_meeting_attendee_get_address (ia))) {
							attendee_list = g_slist_remove (attendee_list, l->data);
							break;
						}
					}
				}

				ca = e_meeting_attendee_as_e_cal_component_attendee (ia);

				attendee_list = g_slist_append (attendee_list, ca);
			}
			e_cal_component_set_attendee_list (comp, attendee_list);
			e_cal_component_free_attendee_list (attendee_list);
		} else
			set_attendees (comp, e_meeting_store_get_attendees (priv->meeting_store));
	}

	/* Percent Complete. */
	percent = gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON (priv->percent_complete));
	e_cal_component_set_percent (comp, &percent);

	/* Status. */
	status = e_dialog_combo_box_get (priv->status_combo, status_map);
	e_cal_component_set_status (comp, status);

	/* Priority. */
	priority = e_dialog_combo_box_get (priv->priority_combo, priority_map);
	priority_value = priority_index_to_value (priority);
	e_cal_component_set_priority (comp, &priority_value);

	icalcomplete = icaltime_null_time ();

	/* COMPLETED must be in UTC. */
	icalcomplete.is_utc = 1;

	/* Completed Date. */
	if (!e_date_edit_date_is_valid (E_DATE_EDIT (priv->completed_date)) ||
	    !e_date_edit_time_is_valid (E_DATE_EDIT (priv->completed_date))) {
		comp_editor_page_display_validation_error (
			page, _("Completed date is wrong"),
			priv->completed_date);
		return FALSE;
	}

	date_set = e_date_edit_get_date (
		E_DATE_EDIT (priv->completed_date),
		&icalcomplete.year,
		&icalcomplete.month,
		&icalcomplete.day);

	if (date_set) {
		e_date_edit_get_time_of_day (
			E_DATE_EDIT (priv->completed_date),
			&icalcomplete.hour,
			&icalcomplete.minute);

		/* COMPLETED today or before */
		icaltoday = icaltime_current_time_with_zone (zone);
		icaltimezone_convert_time (
			&icaltoday, zone,
			icaltimezone_get_utc_timezone ());

		if (icaltime_compare_date_only (icalcomplete, icaltoday) > 0) {
			comp_editor_page_display_validation_error (
				page, _("Completed date is wrong"),
				priv->completed_date);
			return FALSE;
		}

		/* COMPLETED must be in UTC, so we assume that the date in the
		 * dialog is in the current timezone, and we now convert it
		 * to UTC. FIXME: We should really use one timezone for the
		 * entire time the dialog is shown. Otherwise if the user
		 * changes the timezone, the COMPLETED date may get changed
		 * as well. */
		icaltimezone_convert_time (
			&icalcomplete, zone,
				icaltimezone_get_utc_timezone ());
		e_cal_component_set_completed (comp, &icalcomplete);
	} else {
		e_cal_component_set_completed (comp, NULL);
	}

	/* URL. */
	text = gtk_entry_get_text (GTK_ENTRY (priv->web_page_entry));
	e_cal_component_set_url (comp, text);

	return TRUE;
}

static gboolean
task_page_fill_timezones (CompEditorPage *page,
                          GHashTable *timezones)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	icaltimezone *zone;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;

	/* add start date timezone */
	zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->timezone));
	if (zone) {
		if (!g_hash_table_lookup (timezones, icaltimezone_get_tzid (zone)))
			g_hash_table_insert (timezones, (gpointer) icaltimezone_get_tzid (zone), zone);
	}

	/* Add UTC timezone, which is the one
	 * used for the DATE-COMPLETED property. */
	zone = icaltimezone_get_utc_timezone ();
	if (zone != NULL) {
		gconstpointer tzid = icaltimezone_get_tzid (zone);

		if (!g_hash_table_lookup (timezones, tzid))
			g_hash_table_insert (timezones, (gpointer) tzid, zone);
	}

	return TRUE;
}

static void
task_page_add_attendee (CompEditorPage *page,
                        EMeetingAttendee *attendee)
{
	CompEditor *editor;
	TaskPagePrivate *priv;

	priv = TASK_PAGE_GET_PRIVATE (page);
	editor = comp_editor_page_get_editor (page);

	if ((comp_editor_get_flags (editor) & COMP_EDITOR_DELEGATE) != 0) {
		gchar *delfrom;

		/* EMeetingAttendee takes ownership of the string. */
		delfrom = g_strdup_printf ("MAILTO:%s", priv->user_add);
		e_meeting_attendee_set_delfrom (attendee, delfrom);
	}

	e_meeting_store_add_attendee (priv->meeting_store, attendee);
	e_meeting_list_view_add_attendee_to_name_selector (
		E_MEETING_LIST_VIEW (priv->list_view), attendee);
}

static void
task_page_class_init (TaskPageClass *class)
{
	GObjectClass *object_class;
	CompEditorPageClass *editor_page_class;

	g_type_class_add_private (class, sizeof (TaskPagePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = task_page_dispose;
	object_class->finalize = task_page_finalize;

	editor_page_class = COMP_EDITOR_PAGE_CLASS (class);
	editor_page_class->get_widget = task_page_get_widget;
	editor_page_class->focus_main_widget = task_page_focus_main_widget;
	editor_page_class->fill_widgets = task_page_fill_widgets;
	editor_page_class->fill_component = task_page_fill_component;
	editor_page_class->fill_timezones = task_page_fill_timezones;
	editor_page_class->add_attendee = task_page_add_attendee;
}

static void
task_page_init (TaskPage *tpage)
{
	tpage->priv = TASK_PAGE_GET_PRIVATE (tpage);
	tpage->priv->deleted_attendees = g_ptr_array_new ();
}

void
task_page_set_view_role (TaskPage *page,
                         gboolean state)
{
	TaskPagePrivate *priv = page->priv;

	e_meeting_list_view_column_set_visible (priv->list_view, E_MEETING_STORE_ROLE_COL, state);
}

void
task_page_set_view_status (TaskPage *page,
                           gboolean state)
{
	TaskPagePrivate *priv = page->priv;

	e_meeting_list_view_column_set_visible (priv->list_view, E_MEETING_STORE_STATUS_COL, state);
}

void
task_page_set_view_type (TaskPage *page,
                         gboolean state)
{
	TaskPagePrivate *priv = page->priv;

	e_meeting_list_view_column_set_visible (priv->list_view, E_MEETING_STORE_TYPE_COL, state);
}

void
task_page_set_view_rsvp (TaskPage *page,
                         gboolean state)
{
	TaskPagePrivate *priv = page->priv;

	e_meeting_list_view_column_set_visible (priv->list_view, E_MEETING_STORE_RSVP_COL, state);
}

void
task_page_hide_options (TaskPage *page)
{
	CompEditor *editor;
	GtkAction *action;

	g_return_if_fail (IS_TASK_PAGE (page));

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (page));
	action = comp_editor_get_action (editor, "send-options");
	gtk_action_set_visible (action, FALSE);
}

void
task_page_show_options (TaskPage *page)
{
	CompEditor *editor;
	GtkAction *action;

	g_return_if_fail (IS_TASK_PAGE (page));

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (page));
	action = comp_editor_get_action (editor, "send-options");
	gtk_action_set_visible (action, TRUE);
}

void
task_page_set_assignment (TaskPage *page,
                          gboolean set)
{
	g_return_if_fail (IS_TASK_PAGE (page));

	page->priv->is_assignment = set;
	sensitize_widgets (page);
}

static void
add_clicked_cb (GtkButton *btn,
                TaskPage *page)
{
	EMeetingAttendee *attendee;
	CompEditor *editor;
	CompEditorFlags flags;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (page));
	flags = comp_editor_get_flags (editor);

	attendee = e_meeting_store_add_attendee_with_defaults (page->priv->meeting_store);

	if (flags & COMP_EDITOR_DELEGATE) {
		e_meeting_attendee_set_delfrom (attendee, g_strdup_printf ("MAILTO:%s", page->priv->user_add));
	}

	e_meeting_list_view_edit (page->priv->list_view, attendee);
}

static void edit_clicked_cb (GtkButton *btn, TaskPage *tpage)
{
	TaskPagePrivate *priv;
	GtkTreePath *path = NULL;
	GtkTreeViewColumn *focus_col;

	priv = tpage->priv;

	gtk_tree_view_get_cursor (GTK_TREE_VIEW (priv->list_view), &path, NULL);
	g_return_if_fail (path != NULL);

	gtk_tree_view_get_cursor (GTK_TREE_VIEW (priv->list_view), &path, &focus_col);
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->list_view), path, focus_col, TRUE);
	gtk_tree_path_free (path);
}

static gboolean
existing_attendee (EMeetingAttendee *ia,
                   ECalComponent *comp)
{
	GSList *attendees, *l;
	const gchar *ia_address;
	const gchar *ia_sentby = NULL;

	ia_address = itip_strip_mailto (e_meeting_attendee_get_address (ia));
	if (!ia_address)
		return FALSE;

	if (e_meeting_attendee_is_set_sentby (ia))
		ia_sentby = itip_strip_mailto (e_meeting_attendee_get_sentby (ia));

	e_cal_component_get_attendee_list (comp, &attendees);

	for (l = attendees; l; l = l->next) {
		ECalComponentAttendee *attendee = l->data;
		const gchar *address;
		const gchar *sentby = NULL;

		address = itip_strip_mailto (attendee->value);
		if (attendee->sentby)
			sentby = itip_strip_mailto (attendee->sentby);

		if ((address && !g_ascii_strcasecmp (ia_address, address)) || (sentby && ia_sentby && !g_ascii_strcasecmp (ia_sentby, sentby))) {
			e_cal_component_free_attendee_list (attendees);
			return TRUE;
		}
	}

	e_cal_component_free_attendee_list (attendees);

	return FALSE;
}

static void
remove_attendee (TaskPage *page,
                 EMeetingAttendee *ia)
{
	TaskPagePrivate *priv = page->priv;
	CompEditor *editor;
	CompEditorFlags flags;
	gint pos = 0;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (page));
	flags = comp_editor_get_flags (editor);

	/* If the user deletes the organizer attendee explicitly,
	 * assume they no longer want the organizer showing up */
	if (ia == priv->ia) {
		g_object_unref (priv->ia);
		priv->ia = NULL;
	}

	/* If this was a delegatee, no longer delegate */
	if (e_meeting_attendee_is_set_delfrom (ia)) {
		EMeetingAttendee *ib;

		ib = e_meeting_store_find_attendee (priv->meeting_store, e_meeting_attendee_get_delfrom (ia), &pos);
		if (ib != NULL) {
			e_meeting_attendee_set_delto (ib, NULL);

			if (!(flags & COMP_EDITOR_DELEGATE))
				e_meeting_attendee_set_edit_level (ib,  E_MEETING_ATTENDEE_EDIT_FULL);
		}
	}

	/* Handle deleting all attendees in the delegation chain */
	while (ia != NULL) {
		EMeetingAttendee *ib = NULL;

		if (existing_attendee (ia, priv->comp) && !comp_editor_have_in_new_attendees (priv->comp, ia)) {
			g_object_ref (ia);
			g_ptr_array_add (priv->deleted_attendees, ia);
		}

		if (e_meeting_attendee_get_delto (ia) != NULL)
			ib = e_meeting_store_find_attendee (priv->meeting_store, e_meeting_attendee_get_delto (ia), NULL);

		comp_editor_manage_new_attendees (priv->comp, ia, FALSE);
		e_meeting_list_view_remove_attendee_from_name_selector (priv->list_view, ia);
		e_meeting_store_remove_attendee (priv->meeting_store, ia);

		ia = ib;
	}

	sensitize_widgets (page);
}

static void
remove_clicked_cb (GtkButton *btn,
                   TaskPage *page)
{
	TaskPagePrivate *priv;
	EMeetingAttendee *ia;
	GtkTreeSelection *selection;
	GList *paths = NULL, *tmp;
	GtkTreeIter iter;
	GtkTreePath *path = NULL;
	GtkTreeModel *model = NULL;
	gboolean valid_iter;
	gchar *address;

	priv = page->priv;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->list_view));
	model = GTK_TREE_MODEL (priv->meeting_store);
	if (!(paths = gtk_tree_selection_get_selected_rows (selection, &model))) {
		g_warning ("Could not get a selection to delete.");
		return;
	}
	paths = g_list_reverse (paths);

	for (tmp = paths; tmp; tmp = tmp->next) {
		path = tmp->data;

		gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->meeting_store), &iter, path);

		gtk_tree_model_get (GTK_TREE_MODEL (priv->meeting_store), &iter, E_MEETING_STORE_ADDRESS_COL, &address, -1);
		ia = e_meeting_store_find_attendee (priv->meeting_store, address, NULL);
		g_free (address);
		if (!ia) {
			g_warning ("Cannot delete attendee\n");
			continue;
		} else if (e_meeting_attendee_get_edit_level (ia) != E_MEETING_ATTENDEE_EDIT_FULL) {
			g_warning ("Not enough rights to delete attendee: %s\n", e_meeting_attendee_get_address (ia));
			continue;
		}

		remove_attendee (page, ia);
	}

	/* Select closest item after removal */
	valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->meeting_store), &iter, path);
	if (!valid_iter) {
		gtk_tree_path_prev (path);
		valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->meeting_store), &iter, path);
	}

	if (valid_iter) {
		gtk_tree_selection_unselect_all (selection);
		gtk_tree_selection_select_iter (selection, &iter);
	}

	g_list_foreach (paths, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (paths);
}

static void
invite_cb (GtkWidget *widget,
           TaskPage *page)
{
	e_meeting_list_view_invite_others_dialog (page->priv->list_view);
}

static void
attendee_added_cb (EMeetingListView *emlv,
                   EMeetingAttendee *ia,
                   TaskPage *page)
{
	TaskPagePrivate *priv = page->priv;
	CompEditor *editor;
	CompEditorFlags flags;
	ECalClient *client;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (page));
	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);

	if (!(flags & COMP_EDITOR_DELEGATE)) {
		comp_editor_manage_new_attendees (priv->comp, ia, TRUE);
		return;
	}

	/* do not remove here, it did EMeetingListView already */
	e_meeting_attendee_set_delfrom (ia, g_strdup_printf ("MAILTO:%s", priv->user_add ? priv->user_add : ""));

	if (!e_client_check_capability (E_CLIENT (client), CAL_STATIC_CAPABILITY_DELEGATE_TO_MANY)) {
		EMeetingAttendee *delegator;

		gtk_widget_set_sensitive (priv->invite, FALSE);
		gtk_widget_set_sensitive (priv->add, FALSE);
		gtk_widget_set_sensitive (priv->edit, FALSE);

		delegator = e_meeting_store_find_attendee (priv->meeting_store, priv->user_add, NULL);
		g_return_if_fail (delegator != NULL);

		e_meeting_attendee_set_delto (delegator, g_strdup (e_meeting_attendee_get_address (ia)));
	}
}

static gboolean
list_view_event (EMeetingListView *list_view,
                 GdkEvent *event,
                 TaskPage *page)
{
	TaskPagePrivate *priv= page->priv;
	CompEditor *editor;
	CompEditorFlags flags;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (page));
	flags = comp_editor_get_flags (editor);

	if (event->type == GDK_2BUTTON_PRESS && flags & COMP_EDITOR_USER_ORG) {
		EMeetingAttendee *attendee;

		attendee = e_meeting_store_add_attendee_with_defaults (priv->meeting_store);

		if (flags & COMP_EDITOR_DELEGATE) {
			e_meeting_attendee_set_delfrom (attendee, g_strdup_printf ("MAILTO:%s", page->priv->user_add));
		}

		e_meeting_list_view_edit (page->priv->list_view, attendee);
		return TRUE;
	}

	return FALSE;
}

static gboolean
list_key_press (EMeetingListView *list_view,
                GdkEventKey *event,
                TaskPage *page)
{
	if (event->keyval == GDK_KEY_Delete) {

		remove_clicked_cb (NULL, page);

		return TRUE;
	} else if (event->keyval == GDK_KEY_Insert) {
		add_clicked_cb (NULL, page);

		return TRUE;
	}

	return FALSE;
}

void
task_page_set_show_timezone (TaskPage *page,
                             gboolean state)
{
	if (state) {
		gtk_widget_show_all (page->priv->timezone);
		gtk_widget_show (page->priv->timezone_label);
	} else {
		gtk_widget_hide (page->priv->timezone);
		gtk_widget_hide (page->priv->timezone_label);
	}

}

void
task_page_set_show_categories (TaskPage *page,
                               gboolean state)
{
	if (state) {
		gtk_widget_show (page->priv->categories_btn);
		gtk_widget_show (page->priv->categories);
	} else {
		gtk_widget_hide (page->priv->categories_btn);
		gtk_widget_hide (page->priv->categories);
	}
}

/*If the msg has some value set, the icon should always be set */
void
task_page_set_info_string (TaskPage *tpage,
                           const gchar *icon,
                           const gchar *msg)
{
	TaskPagePrivate *priv;

	priv = tpage->priv;

	gtk_image_set_from_icon_name (GTK_IMAGE (priv->info_icon), icon, GTK_ICON_SIZE_BUTTON);
	gtk_label_set_markup (GTK_LABEL (priv->info_string), msg);

	if (msg && icon)
		gtk_widget_show (priv->info_hbox);
	else
		gtk_widget_hide (priv->info_hbox);
}

/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (TaskPage *tpage)
{
	EShell *shell;
	EClientCache *client_cache;
	CompEditor *editor;
	CompEditorPage *page = COMP_EDITOR_PAGE (tpage);
	GtkEntryCompletion *completion;
	TaskPagePrivate *priv;
	GSList *accel_groups;
	GtkWidget *toplevel;
	GtkWidget *parent;
	GtkWidget *sw;
	GtkTreeSelection *selection;

	priv = tpage->priv;

	editor = comp_editor_page_get_editor (page);
	shell = comp_editor_get_shell (editor);
	client_cache = e_shell_get_client_cache (shell);

	priv->main = e_builder_get_widget (priv->builder, "task-page");
	if (!priv->main)
		return FALSE;

	/* Get the GtkAccelGroup from the toplevel window, so we can install
	 * it when the notebook page is mapped. */
	toplevel = gtk_widget_get_toplevel (priv->main);
	accel_groups = gtk_accel_groups_from_object (G_OBJECT (toplevel));
	if (accel_groups)
		page->accel_group = g_object_ref (accel_groups->data);

	g_object_ref (priv->main);
	parent = gtk_widget_get_parent (priv->main);
	gtk_container_remove (GTK_CONTAINER (parent), priv->main);

	priv->info_hbox = e_builder_get_widget (priv->builder, "generic-info");
	priv->info_icon = e_builder_get_widget (priv->builder, "generic-info-image");
	priv->info_string = e_builder_get_widget (priv->builder, "generic-info-msgs");

	priv->summary = e_builder_get_widget (priv->builder, "summary");
	priv->summary_label = e_builder_get_widget (priv->builder, "summary-label");

	/* Glade's visibility flag doesn't seem to work for custom widgets */
	priv->due_date = e_builder_get_widget (priv->builder, "due-date");
	gtk_widget_show (priv->due_date);
	priv->start_date = e_builder_get_widget (priv->builder, "start-date");
	gtk_widget_show (priv->start_date);

	priv->completed_date = e_builder_get_widget (priv->builder, "completed-date");
	priv->status_combo = e_builder_get_widget (priv->builder, "status-combobox");
	priv->priority_combo = e_builder_get_widget (priv->builder, "priority-combobox");
	priv->percent_complete = e_builder_get_widget (priv->builder, "percent-complete");
	priv->classification_combo = e_builder_get_widget (priv->builder, "classification-combobox");
	priv->web_page_entry = e_builder_get_widget (priv->builder, "web-page-entry");

	priv->timezone = e_builder_get_widget (priv->builder, "timezone");
	priv->timezone_label = e_builder_get_widget (priv->builder, "timezone-label");
	priv->description = e_builder_get_widget (priv->builder, "description");
	priv->categories_btn = e_builder_get_widget (priv->builder, "categories-button");
	priv->categories = e_builder_get_widget (priv->builder, "categories");

	priv->organizer = e_builder_get_widget (priv->builder, "organizer");
	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (priv->organizer))));
	gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (priv->organizer), 0);

	priv->invite = e_builder_get_widget (priv->builder, "invite");
	priv->add = e_builder_get_widget (priv->builder, "add-attendee");
	priv->edit = e_builder_get_widget (priv->builder, "edit-attendee");
	priv->remove = e_builder_get_widget (priv->builder, "remove-attendee");
	priv->list_box = e_builder_get_widget (priv->builder, "list-box");
	priv->calendar_label = e_builder_get_widget (priv->builder, "group-label");
	priv->attendee_box = e_builder_get_widget (priv->builder, "attendee-box");
	priv->org_cal_label = e_builder_get_widget (priv->builder, "org-task-label");

	priv->list_view = e_meeting_list_view_new (priv->meeting_store);

	selection = gtk_tree_view_get_selection ((GtkTreeView *) priv->list_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	gtk_widget_show (GTK_WIDGET (priv->list_view));

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_widget_show (sw);
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (priv->list_view));
	gtk_box_pack_start (GTK_BOX (priv->list_box), sw, TRUE, TRUE, 0);

	priv->client_combo_box = e_builder_get_widget (
		priv->builder, "client-combo-box");
	e_client_combo_box_set_client_cache (
		E_CLIENT_COMBO_BOX (priv->client_combo_box), client_cache);

	gtk_label_set_mnemonic_widget (GTK_LABEL (priv->calendar_label), priv->client_combo_box);

	completion = e_category_completion_new ();
	gtk_entry_set_completion (GTK_ENTRY (priv->categories), completion);
	g_object_unref (completion);

	return (priv->summary
		&& priv->summary_label
		&& priv->due_date
		&& priv->start_date
		&& priv->timezone
		&& priv->description
		&& priv->categories_btn
		&& priv->categories
		&& priv->organizer
		&& priv->completed_date
		&& priv->status_combo
		&& priv->priority_combo
		&& priv->percent_complete
		&& priv->classification_combo
		&& priv->web_page_entry
		);
}

static void
summary_changed_cb (GtkEntry *entry,
                    CompEditorPage *page)
{
	CompEditor *editor;
	const gchar *text;

	if (comp_editor_page_get_updating (page))
		return;

	editor = comp_editor_page_get_editor (page);
	text = gtk_entry_get_text (entry);
	comp_editor_set_summary (editor, text);
}

/* Callback used when the start or due date widgets change.  We notify the
 * other pages in the task editor, so they can update any labels.
 */
static void
date_changed_cb (EDateEdit *dedit,
                 TaskPage *tpage)
{
	TaskPagePrivate *priv = tpage->priv;
	CompEditorPageDates dates;
	gboolean date_set;
	ECalComponentDateTime start_dt, due_dt;
	struct icaltimetype start_tt = icaltime_null_time ();
	struct icaltimetype due_tt = icaltime_null_time ();

	if (comp_editor_page_get_updating (COMP_EDITOR_PAGE (tpage)))
		return;

	date_set = e_date_edit_get_date (
		E_DATE_EDIT (priv->start_date),
		&start_tt.year,
		&start_tt.month,
		&start_tt.day);
	if (date_set) {
		start_tt.is_date = TRUE;
		start_dt.tzid = NULL;
	} else {
		start_tt = icaltime_null_time ();
		start_dt.tzid = NULL;
	}

	date_set = e_date_edit_get_date (
		E_DATE_EDIT (priv->due_date),
		&due_tt.year,
		&due_tt.month,
		&due_tt.day);
	if (date_set) {
		due_tt.is_date = TRUE;
		due_dt.tzid = NULL;
	} else {
		due_tt = icaltime_null_time ();
		due_dt.tzid = NULL;
	}

	start_dt.value = &start_tt;
	dates.start = &start_dt;
	dates.end = NULL;
	due_dt.value = &due_tt;
	dates.due = &due_dt;
	dates.complete = NULL;

	/* Notify upstream */
	comp_editor_page_notify_dates_changed (COMP_EDITOR_PAGE (tpage),
					       &dates);

	check_starts_in_the_past (tpage);
}

static void
timezone_changed_cb (EDateEdit *dedit,
                     TaskPage *tpage)
{
	date_changed_cb ((EDateEdit *) tpage->priv->start_date, tpage);
	date_changed_cb ((EDateEdit *) tpage->priv->due_date, tpage);
}

/* Callback used when the categories button is clicked; we must bring up the
 * category list dialog.
 */
static void
categories_clicked_cb (GtkWidget *button,
                       TaskPage *tpage)
{
	GtkEntry *entry;

	entry = GTK_ENTRY (tpage->priv->categories);
	e_categories_config_open_dialog_for_entry (entry);
}

static gboolean
check_start_before_end (struct icaltimetype *start_tt,
                        icaltimezone *start_zone,
                        struct icaltimetype *end_tt,
                        icaltimezone *end_zone,
                        gboolean adjust_end_time,
                        gboolean adjust_by_hour)
{
	struct icaltimetype end_tt_copy;
	gint cmp;

	/* Convert the end time to the same timezone as the start time. */
	end_tt_copy = *end_tt;
	icaltimezone_convert_time (&end_tt_copy, end_zone, start_zone);

	/* Now check if the start time is after the end time. If it is,
	 * we need to modify one of the times. */
	cmp = icaltime_compare (*start_tt, end_tt_copy);
	if (cmp > 0) {
		if (adjust_end_time) {
			/* Modify the end time, to be the start + 1 hour/day. */
			*end_tt = *start_tt;
			icaltime_adjust (end_tt, 0, adjust_by_hour ? 1 : 24, 0, 0);
			icaltimezone_convert_time (
				end_tt, start_zone,
				end_zone);
		} else {
			/* Modify the start time, to be the end - 1 hour/day. */
			*start_tt = *end_tt;
			icaltime_adjust (start_tt, 0, adjust_by_hour ? -1 : -24, 0, 0);
			icaltimezone_convert_time (
				start_tt, end_zone,
				start_zone);
		}
		return TRUE;
	}

	return FALSE;
}

/*
 * This is called whenever the start or due dates.
 * It makes sure that the start date < end date. It also emits the notification
 * signals so the other event editor pages update their labels etc.
 *
 * If adjust_end_time is TRUE, if the start time < end time it will adjust
 * the end time. If FALSE it will adjust the start time. If the user sets the
 * start or end time, the other time is adjusted to make it valid.
 *
 * Time part of the value is changed only when both edits have time set,
 * otherwise times will differ one hour.
 */
static void
times_updated (TaskPage *tpage,
               gboolean adjust_end_time)
{
	TaskPagePrivate *priv;
	struct icaltimetype start_tt = icaltime_null_time ();
	struct icaltimetype end_tt = icaltime_null_time ();
	gboolean date_set;
	gboolean set_start_date = FALSE, set_end_date = FALSE;
	icaltimezone *zone;

	priv = tpage->priv;

	if (comp_editor_page_get_updating (COMP_EDITOR_PAGE (tpage)))
		return;

	date_set = e_date_edit_get_date (
		E_DATE_EDIT (priv->start_date),
		&start_tt.year,
		&start_tt.month,
		&start_tt.day);
	if (!date_set)
		return;

	date_set = e_date_edit_get_date (
		E_DATE_EDIT (priv->due_date),
		&end_tt.year,
		&end_tt.month,
		&end_tt.day);
	if (!date_set)
		return;

	zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->timezone));

	if (check_start_before_end (&start_tt, zone,
				    &end_tt, zone,
				    adjust_end_time,
				    FALSE)) {
		if (adjust_end_time)
			set_end_date = TRUE;
		else
			set_start_date = TRUE;
	}

	if (set_start_date) {
		g_signal_handlers_block_matched (priv->start_date, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, tpage);
		e_date_edit_set_date (E_DATE_EDIT (priv->start_date), start_tt.year, start_tt.month, start_tt.day);
		g_signal_handlers_unblock_matched (priv->start_date, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, tpage);
	}

	if (set_end_date) {
		g_signal_handlers_block_matched (priv->due_date, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, tpage);
		e_date_edit_set_date (E_DATE_EDIT (priv->due_date), end_tt.year, end_tt.month, end_tt.day);
		g_signal_handlers_unblock_matched (priv->due_date, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, tpage);
	}

	/* Notify upstream */
	date_changed_cb ((EDateEdit *) priv->start_date, tpage);
	date_changed_cb ((EDateEdit *) priv->due_date, tpage);
}

static void
start_date_changed_cb (TaskPage *tpage)
{
	times_updated (tpage, TRUE);
}

static void
due_date_changed_cb (TaskPage *tpage)
{
	times_updated (tpage, FALSE);
}

static void
tpage_get_client_cb (GObject *source_object,
                     GAsyncResult *result,
                     gpointer user_data)
{
	EClient *client;
	EClientComboBox *combo_box;
	TaskPage *tpage = user_data;
	TaskPagePrivate *priv;
	CompEditor *editor;
	GError *error = NULL;

	combo_box = E_CLIENT_COMBO_BOX (source_object);

	client = e_client_combo_box_get_client_finish (
		combo_box, result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_clear_error (&error);
		return;
	}

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (tpage));
	priv = tpage->priv;

	if (error != NULL) {
		GtkWidget *dialog;
		ECalClient *old_client;

		old_client = comp_editor_get_client (editor);

		e_source_combo_box_set_active (
			E_SOURCE_COMBO_BOX (combo_box),
			e_client_get_source (E_CLIENT (old_client)));

		dialog = gtk_message_dialog_new (
			NULL, GTK_DIALOG_MODAL,
			GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
			"%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_clear_error (&error);
	} else {
		icaltimezone *zone;
		ECalClient *cal_client = E_CAL_CLIENT (client);

		g_return_if_fail (cal_client != NULL);

		zone = comp_editor_get_timezone (editor);
		e_cal_client_set_default_timezone (cal_client, zone);

		comp_editor_set_client (editor, cal_client);
		comp_editor_page_changed (COMP_EDITOR_PAGE (tpage));
		if (e_client_check_capability (client, CAL_STATIC_CAPABILITY_REQ_SEND_OPTIONS) && priv->is_assignment)
			task_page_show_options (tpage);
		else
			task_page_hide_options (tpage);

		if (client) {
			gchar *backend_addr = NULL;

			e_client_get_backend_property_sync (client, CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS, &backend_addr, NULL, NULL);

			if (priv->is_assignment)
				task_page_select_organizer (tpage, backend_addr);

			set_subscriber_info_string (tpage, backend_addr);
			g_free (backend_addr);
		}

		sensitize_widgets (tpage);
	}
}

static void
source_changed_cb (ESourceComboBox *combo_box,
                   TaskPage *tpage)
{
	TaskPagePrivate *priv = tpage->priv;
	ESource *source;

	if (comp_editor_page_get_updating (COMP_EDITOR_PAGE (tpage)))
		return;

	source = e_source_combo_box_ref_active (combo_box);
	/* This is valid when the 'combo_box' is rebuilding its content. */
	if (!source)
		return;

	if (priv->connect_cancellable != NULL) {
		g_cancellable_cancel (priv->connect_cancellable);
		g_object_unref (priv->connect_cancellable);
	}
	priv->connect_cancellable = g_cancellable_new ();

	e_client_combo_box_get_client (
		E_CLIENT_COMBO_BOX (combo_box),
		source, priv->connect_cancellable,
		tpage_get_client_cb, tpage);

	g_object_unref (source);
}

static void
set_subscriber_info_string (TaskPage *tpage,
                            const gchar *backend_address)
{
	if (!check_starts_in_the_past (tpage))
		task_page_set_info_string (tpage, NULL, NULL);
}

void
task_page_send_options_clicked_cb (TaskPage *tpage)
{
	TaskPagePrivate *priv = tpage->priv;
	CompEditor *editor;
	GtkWidget *toplevel;
	ESource *source;
	ECalClient *client;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (tpage));
	client = comp_editor_get_client (editor);

	if (!priv->sod) {
		priv->sod = e_send_options_dialog_new ();
		priv->sod->data->initialized = TRUE;
		source = e_source_combo_box_ref_active (
			E_SOURCE_COMBO_BOX (priv->client_combo_box));
		e_send_options_utils_set_default_data (
			priv->sod, source, "task");
		g_object_unref (source);
	}

	if (e_client_check_capability (E_CLIENT (client), CAL_STATIC_CAPABILITY_NO_GEN_OPTIONS)) {
		e_send_options_set_need_general_options (priv->sod, FALSE);
	}

	toplevel = gtk_widget_get_toplevel (priv->main);
	e_send_options_dialog_run (priv->sod, toplevel, E_ITEM_TASK);
}

static void
complete_date_changed (TaskPage *tpage,
                       time_t ctime,
                       gboolean complete)
{
	CompEditorPageDates dates = {NULL, NULL, NULL, NULL};
	icaltimezone *zone;
	struct icaltimetype completed_tt = icaltime_null_time ();

	/* Get the current time in UTC. */
	zone = icaltimezone_get_utc_timezone ();
	completed_tt = icaltime_from_timet_with_zone (ctime, FALSE, zone);
	completed_tt.is_utc = TRUE;

	dates.start = NULL;
	dates.end = NULL;
	dates.due = NULL;
	if (complete)
		dates.complete = &completed_tt;

	/* Notify upstream */
	comp_editor_page_notify_dates_changed (COMP_EDITOR_PAGE (tpage),
					       &dates);
}

static void
completed_date_changed_cb (EDateEdit *dedit,
                           TaskPage *tpage)
{
	TaskPagePrivate *priv = tpage->priv;
	CompEditorPageDates dates = {NULL, NULL, NULL, NULL};
	struct icaltimetype completed_tt = icaltime_null_time ();
	icalproperty_status status;
	gboolean date_set;

	if (comp_editor_page_get_updating (COMP_EDITOR_PAGE (tpage)))
		return;

	comp_editor_page_set_updating (COMP_EDITOR_PAGE (tpage), TRUE);

	date_set = e_date_edit_get_date (
		E_DATE_EDIT (priv->completed_date),
		&completed_tt.year,
		&completed_tt.month,
		&completed_tt.day);
	e_date_edit_get_time_of_day (
		E_DATE_EDIT (priv->completed_date),
		&completed_tt.hour,
		&completed_tt.minute);

	status = e_dialog_combo_box_get (priv->status_combo, status_map);

	if (!date_set) {
		completed_tt = icaltime_null_time ();
		if (status == ICAL_STATUS_COMPLETED) {
			e_dialog_combo_box_set (
				priv->status_combo,
				ICAL_STATUS_NONE,
				status_map);
			gtk_spin_button_set_value (
				GTK_SPIN_BUTTON (priv->percent_complete), 0);
		}
	} else {
		if (status != ICAL_STATUS_COMPLETED) {
			e_dialog_combo_box_set (
				priv->status_combo,
				ICAL_STATUS_COMPLETED,
				status_map);
		}
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (priv->percent_complete), 100);
	}

	comp_editor_page_set_updating (COMP_EDITOR_PAGE (tpage), FALSE);

	/* Notify upstream */
	dates.complete = &completed_tt;
	comp_editor_page_notify_dates_changed (COMP_EDITOR_PAGE (tpage), &dates);
}

static void
status_changed (GtkWidget *combo,
                TaskPage *tpage)
{
	TaskPagePrivate *priv;
	icalproperty_status status;
	CompEditor *editor;
	time_t ctime = -1;

	priv = tpage->priv;

	if (comp_editor_page_get_updating (COMP_EDITOR_PAGE (tpage)))
		return;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (tpage));

	comp_editor_page_set_updating (COMP_EDITOR_PAGE (tpage), TRUE);

	status = e_dialog_combo_box_get (priv->status_combo, status_map);
	if (status == ICAL_STATUS_NONE) {
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (priv->percent_complete), 0);
		e_date_edit_set_time (E_DATE_EDIT (priv->completed_date), ctime);
		complete_date_changed (tpage, 0, FALSE);
	} else if (status == ICAL_STATUS_INPROCESS) {
		gint percent_complete = gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (priv->percent_complete));
		if (percent_complete <= 0 || percent_complete >= 100)
			gtk_spin_button_set_value (
				GTK_SPIN_BUTTON (priv->percent_complete), 50);

		e_date_edit_set_time (E_DATE_EDIT (priv->completed_date), ctime);
		complete_date_changed (tpage, 0, FALSE);
	} else if (status == ICAL_STATUS_COMPLETED) {
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (priv->percent_complete), 100);
		ctime = time (NULL);
		e_date_edit_set_time (E_DATE_EDIT (priv->completed_date), ctime);
		complete_date_changed (tpage, ctime, TRUE);
	}

	comp_editor_page_set_updating (COMP_EDITOR_PAGE (tpage), FALSE);

	comp_editor_set_changed (editor, TRUE);
}

static void
percent_complete_changed (GtkAdjustment *adj,
                          TaskPage *tpage)
{
	TaskPagePrivate *priv;
	gint percent;
	icalproperty_status status;
	CompEditor *editor;
	gboolean complete;
	time_t ctime = -1;

	priv = tpage->priv;

	if (comp_editor_page_get_updating (COMP_EDITOR_PAGE (tpage)))
		return;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (tpage));

	comp_editor_page_set_updating (COMP_EDITOR_PAGE (tpage), TRUE);

	percent = gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON (priv->percent_complete));
	if (percent == 100) {
		complete = TRUE;
		ctime = time (NULL);
		status = ICAL_STATUS_COMPLETED;
	} else {
		complete = FALSE;

		if (percent == 0)
			status = ICAL_STATUS_NONE;
		else
			status = ICAL_STATUS_INPROCESS;
	}

	e_dialog_combo_box_set (priv->status_combo, status, status_map);
	e_date_edit_set_time (E_DATE_EDIT (priv->completed_date), ctime);
	complete_date_changed (tpage, ctime, complete);

	comp_editor_page_set_updating (COMP_EDITOR_PAGE (tpage), FALSE);

	comp_editor_set_changed (editor, TRUE);
}

static gboolean
task_page_transform_classification_to_combo (GBinding *binding,
                                             const GValue *source_value,
                                             GValue *target_value,
                                             gpointer user_data)
{
	gint action_value;

	g_return_val_if_fail (source_value != NULL, FALSE);
	g_return_val_if_fail (target_value != NULL, FALSE);

	action_value = g_value_get_int (source_value);
	g_value_set_int (target_value, action_value - 1);

	return TRUE;
}

static gboolean
task_page_transform_classification_from_combo (GBinding *binding,
                                               const GValue *source_value,
                                               GValue *target_value,
                                               gpointer user_data)
{
	gint combo_value;

	g_return_val_if_fail (source_value != NULL, FALSE);
	g_return_val_if_fail (target_value != NULL, FALSE);

	combo_value = g_value_get_int (source_value);
	g_value_set_int (target_value, combo_value + 1);

	return TRUE;
}

/* Hooks the widget signals */
static gboolean
init_widgets (TaskPage *tpage)
{
	CompEditor *editor;
	TaskPagePrivate *priv;
	GtkAction *action;
	GtkTextBuffer *text_buffer;
	icaltimezone *zone;
	gboolean active;
	GtkAdjustment *adjustment;

	priv = tpage->priv;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (tpage));

	/* Make sure the EDateEdit widgets use our timezones to get the
	 * current time. */
	e_date_edit_set_get_time_callback (
		E_DATE_EDIT (priv->start_date),
		(EDateEditGetTimeCallback) comp_editor_get_current_time,
		g_object_ref (editor),
		(GDestroyNotify) g_object_unref);
	e_date_edit_set_get_time_callback (
		E_DATE_EDIT (priv->due_date),
		(EDateEditGetTimeCallback) comp_editor_get_current_time,
		g_object_ref (editor),
		(GDestroyNotify) g_object_unref);

	/* Generic informative messages */
	gtk_widget_hide (priv->info_hbox);

	/* Summary */
	g_signal_connect (
		priv->summary, "changed",
		G_CALLBACK (summary_changed_cb), tpage);

	/* Description */
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description));

	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (priv->description), GTK_WRAP_WORD);

	e_buffer_tagger_connect (GTK_TEXT_VIEW (priv->description));

	/* Dates */
	g_signal_connect (
		priv->start_date, "changed",
		G_CALLBACK (date_changed_cb), tpage);
	g_signal_connect (
		priv->due_date, "changed",
		G_CALLBACK (date_changed_cb), tpage);

	/* time zone changed */
	g_signal_connect (
		priv->timezone, "changed",
		G_CALLBACK (timezone_changed_cb), tpage);

	/* Categories button */
	g_signal_connect (
		priv->categories_btn, "clicked",
		G_CALLBACK (categories_clicked_cb), tpage);

	/* Source selector */
	g_signal_connect (
		priv->client_combo_box, "changed",
		G_CALLBACK (source_changed_cb), tpage);

	/* Connect the default signal handler to use to make sure the "changed"
	 * field gets set whenever a field is changed. */

	/* Belongs to priv->description */
	g_signal_connect_swapped (
		text_buffer, "changed",
		G_CALLBACK (comp_editor_page_changed), tpage);
	g_signal_connect_swapped (
		priv->summary, "changed",
		G_CALLBACK (comp_editor_page_changed), tpage);
	g_signal_connect_swapped (
		priv->start_date, "changed",
		G_CALLBACK (start_date_changed_cb), tpage);
	g_signal_connect_swapped (
		priv->start_date, "changed",
		G_CALLBACK (comp_editor_page_changed), tpage);
	g_signal_connect_swapped (
		priv->due_date, "changed",
		G_CALLBACK (due_date_changed_cb), tpage);
	g_signal_connect_swapped (
		priv->due_date, "changed",
		G_CALLBACK (comp_editor_page_changed), tpage);
	g_signal_connect_swapped (
		priv->timezone, "changed",
		G_CALLBACK (comp_editor_page_changed), tpage);
	g_signal_connect_swapped (
		priv->categories, "changed",
		G_CALLBACK (comp_editor_page_changed), tpage);

	g_signal_connect (
		priv->list_view, "event",
		G_CALLBACK (list_view_event), tpage);
	g_signal_connect (
		priv->list_view, "key_press_event",
		G_CALLBACK (list_key_press), tpage);

	/* Add attendee button */
	g_signal_connect (
		priv->add, "clicked",
		G_CALLBACK (add_clicked_cb), tpage);

	/* Edit attendee button */
	g_signal_connect (
		priv->edit, "clicked",
		G_CALLBACK (edit_clicked_cb), tpage);

	/* Remove attendee button */
	g_signal_connect (
		priv->remove, "clicked",
		G_CALLBACK (remove_clicked_cb), tpage);

	/* Contacts button */
	g_signal_connect (
		priv->invite, "clicked",
		G_CALLBACK (invite_cb), tpage);

	/* Meeting List View */
	g_signal_connect (
		priv->list_view, "attendee_added",
		G_CALLBACK (attendee_added_cb), tpage);

	/* Set the default timezone, so the timezone entry may be hidden. */
	zone = comp_editor_get_timezone (editor);
	e_timezone_entry_set_default_timezone (E_TIMEZONE_ENTRY (priv->timezone), zone);

	/* Make sure the EDateEdit widgets use our timezones to get the
	 * current time. */
	e_date_edit_set_get_time_callback (
		E_DATE_EDIT (priv->completed_date),
		(EDateEditGetTimeCallback) comp_editor_get_current_time,
		g_object_ref (editor),
		(GDestroyNotify) g_object_unref);

	/* Connect signals. The Status, Percent Complete & Date Completed
	 * properties are closely related so whenever one changes we may need
	 * to update the other 2. */
	g_signal_connect (
		GTK_COMBO_BOX (priv->status_combo), "changed",
		G_CALLBACK (status_changed), tpage);

	adjustment = gtk_spin_button_get_adjustment (
		GTK_SPIN_BUTTON (priv->percent_complete));
	g_signal_connect (
		adjustment, "value_changed",
		G_CALLBACK (percent_complete_changed), tpage);

	/* Priority */
	g_signal_connect_swapped (
		GTK_COMBO_BOX (priv->priority_combo), "changed",
		G_CALLBACK (comp_editor_page_changed), tpage);

	/* Completed Date */
	g_signal_connect (
		priv->completed_date, "changed",
		G_CALLBACK (completed_date_changed_cb), tpage);
	g_signal_connect_swapped (
		priv->completed_date, "changed",
		G_CALLBACK (comp_editor_page_changed), tpage);

	/* URL */
	g_signal_connect_swapped (
		priv->web_page_entry, "changed",
		G_CALLBACK (comp_editor_page_changed), tpage);

	action = comp_editor_get_action (editor, "view-time-zone");
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	task_page_set_show_timezone (tpage, active);

	e_meeting_list_view_column_set_visible (
		priv->list_view, E_MEETING_STORE_ATTENDEE_COL, TRUE);

	action = comp_editor_get_action (editor, "view-role");
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	e_meeting_list_view_column_set_visible (
		priv->list_view, E_MEETING_STORE_ROLE_COL, active);

	action = comp_editor_get_action (editor, "view-rsvp");
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	e_meeting_list_view_column_set_visible (
		priv->list_view, E_MEETING_STORE_RSVP_COL, active);

	action = comp_editor_get_action (editor, "view-status");
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	e_meeting_list_view_column_set_visible (
		priv->list_view, E_MEETING_STORE_STATUS_COL, active);

	action = comp_editor_get_action (editor, "view-type");
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	e_meeting_list_view_column_set_visible (
		priv->list_view, E_MEETING_STORE_TYPE_COL, active);

	action = comp_editor_get_action (editor, "view-categories");
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	task_page_set_show_categories (tpage, active);

	/* Classification */
	action = comp_editor_get_action (editor, "classify-public");
	e_binding_bind_property_full (
		action, "current-value",
		priv->classification_combo, "active",
		G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
		task_page_transform_classification_to_combo,
		task_page_transform_classification_from_combo,
		NULL, NULL);

	return TRUE;
}

static void
task_page_select_organizer (TaskPage *tpage,
                            const gchar *backend_address)
{
	TaskPagePrivate *priv = tpage->priv;
	const gchar *default_address;
	gint ii;

	/* Treat an empty backend address as NULL. */
	if (backend_address != NULL && *backend_address == '\0')
		backend_address = NULL;

	default_address = priv->fallback_address;

	if (backend_address != NULL) {
		for (ii = 0; priv->address_strings[ii] != NULL; ii++) {
			if (g_strrstr (priv->address_strings[ii], backend_address) != NULL) {
				default_address = priv->address_strings[ii];
				break;
			}
		}
	}

	if (default_address != NULL) {
		if (!priv->comp || !e_cal_component_has_organizer (priv->comp)) {
			GtkEntry *entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->organizer)));

			g_signal_handlers_block_by_func (entry, organizer_changed_cb, tpage);
			gtk_entry_set_text (entry, default_address);
			g_signal_handlers_unblock_by_func (entry, organizer_changed_cb, tpage);
		}
	} else
		g_warning ("No potential organizers!");
}

/**
 * task_page_construct:
 * @tpage: An task page.
 *
 * Constructs an task page by loading its Glade data.
 *
 * Return value: The same object as @tpage, or NULL if the widgets could not be
 * created.
 **/
TaskPage *
task_page_construct (TaskPage *tpage,
                     EMeetingStore *meeting_store,
                     ECalClient *client)
{
	EShell *shell;
	CompEditor *editor;
	ESourceRegistry *registry;
	EFocusTracker *focus_tracker;
	TaskPagePrivate *priv;
	GtkComboBox *combo_box;
	GtkListStore *list_store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint ii;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (tpage));
	shell = comp_editor_get_shell (editor);
	focus_tracker = comp_editor_get_focus_tracker (editor);

	priv = tpage->priv;
	priv->meeting_store = g_object_ref (meeting_store);
	priv->client = client;

	/* Make sure our custom widget classes are registered with
	 * GType before we load the GtkBuilder definition file. */
	g_type_ensure (E_TYPE_CLIENT_COMBO_BOX);
	g_type_ensure (E_TYPE_DATE_EDIT);
	g_type_ensure (E_TYPE_TIMEZONE_ENTRY);
	g_type_ensure (E_TYPE_SOURCE_COMBO_BOX);
/* FIXME WK2
	g_type_ensure (E_TYPE_SPELL_ENTRY);*/
	g_type_ensure (E_TYPE_URL_ENTRY);

	priv->builder = gtk_builder_new ();
	e_load_ui_builder_definition (priv->builder, "task-page.ui");

	if (!get_widgets (tpage)) {
		g_message (
			"task_page_construct(): "
			"Could not find all widgets in the XML file!");
		return NULL;
	}

	e_spell_text_view_attach (GTK_TEXT_VIEW (priv->description));
	e_widget_undo_attach (priv->summary, focus_tracker);
	e_widget_undo_attach (priv->categories, focus_tracker);
	e_widget_undo_attach (priv->web_page_entry, focus_tracker);
	e_widget_undo_attach (priv->description, focus_tracker);

	combo_box = GTK_COMBO_BOX (priv->organizer);
	model = gtk_combo_box_get_model (combo_box);
	list_store = GTK_LIST_STORE (model);

	registry = e_shell_get_registry (shell);
	priv->address_strings = itip_get_user_identities (registry);
	priv->fallback_address = itip_get_fallback_identity (registry);

	/* FIXME Could we just use a GtkComboBoxText? */
	for (ii = 0; priv->address_strings[ii] != NULL; ii++) {
		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (
			list_store, &iter,
			0, priv->address_strings[ii], -1);
	}

	gtk_combo_box_set_active (combo_box, 0);

	g_signal_connect (
		gtk_bin_get_child (GTK_BIN (priv->organizer)), "changed",
		G_CALLBACK (organizer_changed_cb), tpage);

	if (!init_widgets (tpage)) {
		g_message (
			"task_page_construct(): "
			"Could not initialize the widgets!");
		return NULL;
	}

	return tpage;
}

/**
 * task_page_new:
 *
 * Creates a new task page.
 *
 * Return value: A newly-created task page, or NULL if the page could
 * not be created.
 **/
TaskPage *
task_page_new (EMeetingStore *model,
               CompEditor *editor)
{
	TaskPage *tpage;
	ECalClient *client;

	tpage = g_object_new (TYPE_TASK_PAGE, "editor", editor, NULL);
	client = comp_editor_get_client (editor);
	if (!task_page_construct (tpage, model, client)) {
		g_object_unref (tpage);
		g_return_val_if_reached (NULL);
	}

	return tpage;
}

ECalComponent *
task_page_get_cancel_comp (TaskPage *page)
{
	TaskPagePrivate *priv;

	g_return_val_if_fail (page != NULL, NULL);
	g_return_val_if_fail (IS_TASK_PAGE (page), NULL);

	priv = page->priv;

	if (priv->deleted_attendees->len == 0)
		return NULL;

	set_attendees (priv->comp, priv->deleted_attendees);

	return e_cal_component_clone (priv->comp);
}
