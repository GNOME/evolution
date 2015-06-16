/*
 * Evolution calendar - Framework for a calendar component editor dialog
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>
#include <libebackend/libebackend.h>

#include <shell/e-shell.h>

#include "../print.h"
#include "../comp-util.h"
#include "save-comp.h"
#include "delete-comp.h"
#include "send-comp.h"
#include "changed-comp.h"
#include "cancel-comp.h"
#include "recur-comp.h"
#include "comp-editor.h"
#include "comp-editor-util.h"
#include "../calendar-config-keys.h"

#define COMP_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_COMP_EDITOR, CompEditorPrivate))

#define d(x)

/* Private part of the CompEditor structure */
struct _CompEditorPrivate {

	gpointer shell;  /* weak pointer */

	GSettings *calendar_settings;

	/* EFocusTracker keeps selection actions up-to-date. */
	EFocusTracker *focus_tracker;

	/* Each CompEditor window gets its own GtkWindowGroup, so it
	 * doesn't block the main window or other CompEditor windows. */
	GtkWindowGroup *window_group;

	/* Client to use */
	ECalClient *cal_client;

	/* Source client (where comp lives currently) */
	ECalClient *source_client;

	/* View to listen for changes */
	ECalClientView *view;
	GCancellable *view_cancellable;

	/* Calendar object/uid we are editing; this is an internal copy */
	ECalComponent *comp;

	/* The pages we have */
	GList *pages;

	/* Notebook to hold the pages */
	GtkNotebook *notebook;

	/* Attachment handling */
	GtkWidget *attachment_view;

	/* Manages menus and toolbars */
	GtkUIManager *ui_manager;

	gchar *summary;

	guint32 attachment_bar_visible : 1;

	/* TODO use this flags for setting all the boolean variables
	 * below */
	CompEditorFlags flags;

	icaltimezone *zone;
	gboolean use_24_hour_format;

	GDateWeekday week_start_day;

	gint work_day_end_hour;
	gint work_day_end_minute;
	gint work_day_start_hour;
	gint work_day_start_minute;
	gint work_day_start_mon;
	gint work_day_end_mon;
	gint work_day_start_tue;
	gint work_day_end_tue;
	gint work_day_start_wed;
	gint work_day_end_wed;
	gint work_day_start_thu;
	gint work_day_end_thu;
	gint work_day_start_fri;
	gint work_day_end_fri;
	gint work_day_start_sat;
	gint work_day_end_sat;
	gint work_day_start_sun;
	gint work_day_end_sun;

	gboolean changed;
	gboolean needs_send;

	gboolean saved;

	ECalObjModType mod;

	gboolean existing_org;
	gboolean user_org;
	gboolean is_group_item;

	gboolean warned;
};

enum {
	PROP_0,
	PROP_CHANGED,
	PROP_CLIENT,
	PROP_FLAGS,
	PROP_FOCUS_TRACKER,
	PROP_SHELL,
	PROP_SUMMARY,
	PROP_TIMEZONE,
	PROP_USE_24_HOUR_FORMAT,
	PROP_WEEK_START_DAY,
	PROP_WORK_DAY_END_HOUR,
	PROP_WORK_DAY_END_MINUTE,
	PROP_WORK_DAY_START_HOUR,
	PROP_WORK_DAY_START_MINUTE,
	PROP_WORK_DAY_START_MON,
	PROP_WORK_DAY_END_MON,
	PROP_WORK_DAY_START_TUE,
	PROP_WORK_DAY_END_TUE,
	PROP_WORK_DAY_START_WED,
	PROP_WORK_DAY_END_WED,
	PROP_WORK_DAY_START_THU,
	PROP_WORK_DAY_END_THU,
	PROP_WORK_DAY_START_FRI,
	PROP_WORK_DAY_END_FRI,
	PROP_WORK_DAY_START_SAT,
	PROP_WORK_DAY_END_SAT,
	PROP_WORK_DAY_START_SUN,
	PROP_WORK_DAY_END_SUN
};

static const gchar *ui =
"<ui>"
"  <menubar action='main-menu'>"
"    <menu action='file-menu'>"
"      <menuitem action='save'/>"
"      <menuitem action='save-and-close'/>"
"      <separator/>"
"      <menuitem action='print-preview'/>"
"      <menuitem action='print'/>"
"      <separator/>"
"      <menuitem action='close'/>"
"    </menu>"
"    <menu action='edit-menu'>"
"      <menuitem action='undo'/>"
"      <menuitem action='redo'/>"
"      <separator/>"
"      <menuitem action='cut-clipboard'/>"
"      <menuitem action='copy-clipboard'/>"
"      <menuitem action='paste-clipboard'/>"
"      <menuitem action='delete-selection'/>"
"      <separator/>"
"      <menuitem action='select-all'/>"
"    </menu>"
"    <menu action='view-menu'/>"
"    <menu action='insert-menu'>"
"      <menuitem action='attach'/>"
"    </menu>"
"    <menu action='options-menu'/>"
"    <menu action='help-menu'>"
"      <menuitem action='help'/>"
"    </menu>"
"  </menubar>"
"  <toolbar name='main-toolbar'>"
"    <toolitem action='save-and-close'/>\n"
"    <toolitem action='save'/>\n"
"    <toolitem action='print'/>\n"
"    <separator/>"
"    <toolitem action='undo'/>"
"    <toolitem action='redo'/>"
"    <separator/>"
"    <placeholder name='content'/>\n"
"  </toolbar>"
"</ui>";

static void	comp_editor_show_help		(CompEditor *editor);

static void	real_edit_comp			(CompEditor *editor,
						 ECalComponent *comp);
static gboolean	real_send_comp			(CompEditor *editor,
						 ECalComponentItipMethod method,
						 gboolean strip_alarms);
static gboolean	prompt_and_save_changes		(CompEditor *editor,
						 gboolean send);
static void	close_dialog			(CompEditor *editor);

static void	page_dates_changed_cb		(CompEditor *editor,
						 CompEditorPageDates *dates,
						 CompEditorPage *page);

static void	obj_modified_cb			(ECalClientView *view,
						 const GSList *objs,
						 CompEditor *editor);
static void	obj_removed_cb			(ECalClientView *view,
						 const GSList *uids,
						 CompEditor *editor);

G_DEFINE_TYPE_WITH_CODE (
	CompEditor, comp_editor, GTK_TYPE_WINDOW,
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

enum {
	OBJECT_CREATED,
	COMP_CLOSED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static GList *active_editors;

static void
comp_editor_weak_notify_cb (gpointer unused,
                            GObject *where_the_object_was)
{
	active_editors = g_list_remove (active_editors, where_the_object_was);
}

static void
attachment_store_changed_cb (CompEditor *editor)
{
	/* Mark the editor as changed so it prompts about unsaved
	 * changes on close */
	comp_editor_set_changed (editor, TRUE);
}

static void
attachment_save_finished (EAttachmentStore *store,
                          GAsyncResult *result,
                          gpointer user_data)
{
	GtkWidget *dialog;
	const gchar *primary_text;
	gchar **uris;
	GError *error = NULL;

	struct {
		gchar **uris;
		gboolean done;
		GtkWindow *parent;
	} *status = user_data;

	uris = e_attachment_store_save_finish (store, result, &error);

	status->uris = uris;
	status->done = TRUE;

	if (uris != NULL)
		goto exit;

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		goto exit;

	primary_text = _("Could not save attachments");

	dialog = gtk_message_dialog_new_with_markup (
		status->parent, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		"<big><b>%s</b></big>", primary_text);

	gtk_message_dialog_format_secondary_text (
		GTK_MESSAGE_DIALOG (dialog), "%s", error->message);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

exit:
	if (error != NULL)
		g_error_free (error);

	g_object_unref (status->parent);
}

static GSList *
get_attachment_list (CompEditor *editor)
{
	EAttachmentStore *store;
	EAttachmentView *view;
	GFile *destination;
	GSList *list = NULL;
	const gchar *comp_uid = NULL;
	const gchar *local_store;
	gchar *filename_prefix, *tmp;
	gint ii;

	struct {
		gchar **uris;
		gboolean done;
		GtkWindow *parent;
	} status;

	e_cal_component_get_uid (editor->priv->comp, &comp_uid);
	g_return_val_if_fail (comp_uid != NULL, NULL);

	status.uris = NULL;
	status.done = FALSE;
	status.parent = g_object_ref (editor);

	view = E_ATTACHMENT_VIEW (editor->priv->attachment_view);
	store = e_attachment_view_get_store (view);

	tmp = g_strdup (comp_uid);
	e_filename_make_safe (tmp);
	filename_prefix = g_strconcat (tmp, "-", NULL);
	g_free (tmp);

	local_store = e_cal_client_get_local_attachment_store (editor->priv->cal_client);
	destination = g_file_new_for_path (local_store);

	e_attachment_store_save_async (
		store, destination, filename_prefix,
		(GAsyncReadyCallback) attachment_save_finished, &status);

	g_object_unref (destination);
	g_free (filename_prefix);

	/* We can't return until we have results, so crank
	 * the main loop until the callback gets triggered. */
	/* coverity[loop_condition] */
	while (!status.done)
		gtk_main_iteration ();

	if (status.uris == NULL)
		return NULL;

	/* Transfer the URI strings to the GSList. */
	for (ii = 0; status.uris[ii] != NULL; ii++) {
		list = g_slist_prepend (list, status.uris[ii]);
		status.uris[ii] = NULL;
	}

	g_free (status.uris);

	return g_slist_reverse (list);
}

/* This sets the focus to the toplevel, so any field being edited is committed.
 * FIXME: In future we may also want to check some of the fields are valid,
 * e.g. the EDateEdit fields. */
static void
commit_all_fields (CompEditor *editor)
{
	gtk_window_set_focus (GTK_WINDOW (editor), NULL);
}

static void
changes_view_ready_cb (GObject *source_object,
                       GAsyncResult *result,
                       gpointer user_data)
{
	CompEditor *editor = user_data;
	ECalClientView *view = NULL;
	gboolean success;
	GError *error = NULL;

	g_return_if_fail (editor != NULL);

	success = e_cal_client_get_view_finish (
		E_CAL_CLIENT (source_object), result, &view, &error);

	if (!success)
		view = NULL;

	if (view) {
		editor->priv->view = view;
		g_signal_connect (
			view, "objects_modified",
			G_CALLBACK (obj_modified_cb), editor);
		g_signal_connect (
			view, "objects_removed",
			G_CALLBACK (obj_removed_cb), editor);

		e_cal_client_view_start (view, &error);

		if (error != NULL) {
			g_warning (
				"%s: Failed to start view: %s",
				G_STRFUNC, error->message);
			g_error_free (error);
		}

	} else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);

	} else if (error != NULL) {
		g_warning (
			"%s: Failed to get view: %s",
			G_STRFUNC, error->message);
		g_error_free (error);
	}
}

static void
listen_for_changes (CompEditor *editor)
{
	CompEditorPrivate *priv;
	const gchar *uid = NULL;

	priv = editor->priv;

	/* Discard change listener */
	if (priv->view_cancellable) {
		g_cancellable_cancel (priv->view_cancellable);
		g_object_unref (priv->view_cancellable);
		priv->view_cancellable = NULL;
	}

	if (priv->view) {
		g_signal_handlers_disconnect_matched (
			priv->view, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, editor);

		g_object_unref (priv->view);
		priv->view = NULL;
	}

	/* Listen for changes */
	if (priv->comp)
		e_cal_component_get_uid (priv->comp, &uid);

	if (uid) {
		gchar *query;

		priv->view_cancellable = g_cancellable_new ();
		query = g_strdup_printf ("(uid? \"%s\")", uid);
		e_cal_client_get_view (
			priv->source_client,
			query, priv->view_cancellable,
			changes_view_ready_cb, editor);
		g_free (query);
	}
}

static void
send_timezone (gpointer key,
               gpointer value,
               gpointer user_data)
{
	icaltimezone *zone = value;
	CompEditor *editor = user_data;
	GError *error = NULL;

	e_cal_client_add_timezone_sync (editor->priv->cal_client, zone, NULL, &error);

	if (error != NULL) {
		g_warning (
			"%s: Failed to add timezone: %s",
			G_STRFUNC, error->message);
		g_error_free (error);
	}
}

static gboolean
save_comp (CompEditor *editor)
{
	CompEditorPrivate *priv;
	CompEditorFlags flags;
	ECalComponent *clone;
	ESourceRegistry *registry;
	EShell *shell;
	GList *l;
	gboolean result;
	GError *error = NULL;
	GHashTable *timezones;
	const gchar *orig_uid = NULL;
	gchar *orig_uid_copy;
	icalcomponent *icalcomp;

	priv = editor->priv;

	if (!priv->changed)
		return TRUE;

	flags = comp_editor_get_flags (editor);
	shell = comp_editor_get_shell (editor);

	registry = e_shell_get_registry (shell);

	/* Stop listening because we are about to change things */
	if (priv->view) {
		g_signal_handlers_disconnect_matched (
			priv->view,
			G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL,
			editor);

		g_object_unref (priv->view);
		priv->view = NULL;
	}

	/* Update on the server */
	timezones = g_hash_table_new (g_str_hash, g_str_equal);

	clone = e_cal_component_clone (priv->comp);
	comp_editor_copy_new_attendees (clone, priv->comp);
	for (l = priv->pages; l != NULL; l = l->next) {
		if (IS_COMP_EDITOR_PAGE (l->data) &&
			!comp_editor_page_fill_component (l->data, clone)) {
			g_object_unref (clone);
			g_hash_table_destroy (timezones);
			comp_editor_show_page (editor, COMP_EDITOR_PAGE (l->data));
			return FALSE;
		}

		/* retrieve all timezones */
		if (IS_COMP_EDITOR_PAGE (l->data))
			comp_editor_page_fill_timezones (l->data, timezones);
	}

	/* If we are not the organizer, we don't update the sequence number */
	if (!e_cal_component_has_organizer (clone) ||
		itip_organizer_is_user (registry, clone, priv->cal_client) ||
		itip_sentby_is_user (registry, clone, priv->cal_client))
		e_cal_component_commit_sequence (clone);
	else
		e_cal_component_abort_sequence (clone);

	g_object_unref (priv->comp);
	priv->comp = clone;

	e_cal_component_get_uid (priv->comp, &orig_uid);
	/* Make a copy of it, because call of e_cal_create_object()
	 * rewrites the internal uid. */
	orig_uid_copy = g_strdup (orig_uid);

	/* send timezones */
	g_hash_table_foreach (timezones, (GHFunc) send_timezone, editor);
	g_hash_table_destroy (timezones);

	/* Attachments*/

	e_cal_component_set_attachment_list (
		priv->comp, get_attachment_list (editor));
	icalcomp = e_cal_component_get_icalcomponent (priv->comp);
	/* send the component to the server */
	if (!cal_comp_is_on_server_sync (priv->comp, priv->cal_client, NULL, NULL)) {
		gchar *uid = NULL;
		result = e_cal_client_create_object_sync (
			priv->cal_client, icalcomp, &uid, NULL, &error);
		if (result) {
			icalcomponent_set_uid (icalcomp, uid);
			g_free (uid);
			g_signal_emit_by_name (editor, "object_created");
		}
	} else {
		gboolean has_recurrences;

		has_recurrences =
			e_cal_component_has_recurrences (priv->comp);

		if (has_recurrences && priv->mod == E_CAL_OBJ_MOD_ALL)
			comp_util_sanitize_recurrence_master_sync (priv->comp, priv->cal_client, NULL, NULL);

		if (priv->mod == E_CAL_OBJ_MOD_THIS) {
			e_cal_component_set_rdate_list (priv->comp, NULL);
			e_cal_component_set_rrule_list (priv->comp, NULL);
			e_cal_component_set_exdate_list (priv->comp, NULL);
			e_cal_component_set_exrule_list (priv->comp, NULL);
		}
		result = e_cal_client_modify_object_sync (
			priv->cal_client, icalcomp, priv->mod, NULL, &error);

		if (priv->mod == E_CAL_OBJ_MOD_THIS) {
			if (result && ((flags & COMP_EDITOR_DELEGATE) ||
				!e_cal_component_has_organizer (clone) ||
				itip_organizer_is_user (registry, clone, priv->cal_client) ||
				itip_sentby_is_user (registry, clone, priv->cal_client)))
				e_cal_component_commit_sequence (clone);
			else
				e_cal_component_abort_sequence (clone);
		}
	}

	/* If the delay delivery is set, the items will not be created in
	 * the server immediately, so we need not show them in the view.
	 * They will appear as soon as the server creates it after the
	 * delay period. */
	if (result && e_cal_component_has_attendees (priv->comp)) {
		gboolean delay_set = FALSE;
		icalproperty *icalprop;
		icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
		while (icalprop) {
			const gchar *x_name;

			x_name = icalproperty_get_x_name (icalprop);
			if (!strcmp (x_name, "X-EVOLUTION-OPTIONS-DELAY")) {
				delay_set = TRUE;
				break;
			}

			icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
		}
		if (delay_set) {
			g_free (orig_uid_copy);
			return TRUE;
		}
	}

	if (!result) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (
			NULL, 0,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_OK,
			"%s", (error != NULL) ? error->message :
			_("Could not update object"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		if (error != NULL)
			g_error_free (error);

		g_free (orig_uid_copy);

		return FALSE;
	} else {
		if (priv->source_client &&
		    !e_source_equal (e_client_get_source (E_CLIENT (priv->cal_client)),
				     e_client_get_source (E_CLIENT (priv->source_client))) &&
		    cal_comp_is_on_server_sync (priv->comp, priv->source_client, NULL, NULL)) {
			/* Comp found a new home. Remove it from old one. */
			GError *error = NULL;

			if (e_cal_component_is_instance (priv->comp) ||
				e_cal_component_has_recurrences (priv->comp))
				e_cal_client_remove_object_sync (
					priv->source_client, orig_uid_copy,
					NULL, E_CAL_OBJ_MOD_ALL, NULL, &error);
			else
				e_cal_client_remove_object_sync (
					priv->source_client,
					orig_uid_copy, NULL, E_CAL_OBJ_MOD_THIS, NULL, &error);

			if (error != NULL) {
				g_warning (
					"%s: Failed to remove object: %s",
					G_STRFUNC, error->message);
				g_error_free (error);
			}

			/* Let priv->source_client point to new home,
			 * so we can move it again this session. */
			g_object_unref (priv->source_client);
			priv->source_client = g_object_ref (priv->cal_client);

			listen_for_changes (editor);
		}

		comp_editor_set_changed (editor, FALSE);
		priv->saved = TRUE;
	}

	g_free (orig_uid_copy);

	return TRUE;
}

static gboolean
save_comp_with_send (CompEditor *editor)
{
	CompEditorPrivate *priv;
	CompEditorFlags flags;
	ESourceRegistry *registry;
	EShell *shell;
	gboolean send, delegated, only_new_attendees = FALSE;
	gboolean delegate;
	gboolean strip_alarms = TRUE;

	priv = editor->priv;

	flags = comp_editor_get_flags (editor);
	shell = comp_editor_get_shell (editor);

	registry = e_shell_get_registry (shell);

	send = priv->changed && priv->needs_send;
	delegate = flags & COMP_EDITOR_DELEGATE;

	if (delegate) {
		icalcomponent *icalcomp = e_cal_component_get_icalcomponent (priv->comp);
		icalproperty *icalprop;

		icalprop = icalproperty_new_x ("1");
		icalproperty_set_x_name (icalprop, "X-EVOLUTION-DELEGATED");
		icalcomponent_add_property (icalcomp, icalprop);
	}

	if (!save_comp (editor))
		return FALSE;

	delegated = delegate && !e_cal_client_check_save_schedules (priv->cal_client);
	if (delegated || (send && send_component_dialog (
		(GtkWindow *) editor, priv->cal_client, priv->comp,
		!priv->existing_org, &strip_alarms, !priv->existing_org ?
		NULL : &only_new_attendees))) {
		if (delegated)
			only_new_attendees = FALSE;

		comp_editor_set_flags (
			editor, (comp_editor_get_flags (editor) &
			(~COMP_EDITOR_SEND_TO_NEW_ATTENDEES_ONLY)) |
			(only_new_attendees ?
			COMP_EDITOR_SEND_TO_NEW_ATTENDEES_ONLY : 0));

		if ((itip_organizer_is_user (registry, priv->comp, priv->cal_client) ||
			itip_sentby_is_user (registry, priv->comp, priv->cal_client))) {
			if (e_cal_component_get_vtype (priv->comp) == E_CAL_COMPONENT_JOURNAL)
				return comp_editor_send_comp (
					editor, E_CAL_COMPONENT_METHOD_PUBLISH,
					strip_alarms);
			else
				return comp_editor_send_comp (
					editor, E_CAL_COMPONENT_METHOD_REQUEST,
					strip_alarms);
		} else {
			if (!comp_editor_send_comp (
				editor, E_CAL_COMPONENT_METHOD_REQUEST,
				strip_alarms))
				return FALSE;

			if (delegate)
				return comp_editor_send_comp (
					editor, E_CAL_COMPONENT_METHOD_REPLY,
					strip_alarms);
		}
	}

	return TRUE;
}

static void
update_window_border (CompEditor *editor,
                      const gchar *description)
{
	const gchar *icon_name;
	const gchar *format;
	gchar *title;

	if (editor->priv->comp == NULL) {
		title = g_strdup (_("Edit Appointment"));
		icon_name = "x-office-calendar";
		goto exit;

	} else switch (e_cal_component_get_vtype (editor->priv->comp)) {
		case E_CAL_COMPONENT_EVENT:
			if (editor->priv->is_group_item)
				format = _("Meeting - %s");
			else
				format = _("Appointment - %s");
			icon_name = "appointment-new";
			break;

		case E_CAL_COMPONENT_TODO:
			if (editor->priv->is_group_item)
				format = _("Assigned Task - %s");
			else
				format = _("Task - %s");
			icon_name = "stock_task";
			break;

		case E_CAL_COMPONENT_JOURNAL:
			format = _("Memo - %s");
			icon_name = "stock_insert-note";
			break;

		default:
			g_return_if_reached ();
	}

	if (description == NULL || *description == '\0') {
		ECalComponentText text;

		e_cal_component_get_summary (editor->priv->comp, &text);
		description = text.value;
	}

	if (description == NULL || *description == '\0')
		description = _("No Summary");

	title = g_strdup_printf (format, description);

exit:
	gtk_window_set_icon_name (GTK_WINDOW (editor), icon_name);
	gtk_window_set_title (GTK_WINDOW (editor), title);

	g_free (title);
}

static void
action_attach_cb (GtkAction *action,
                  CompEditor *editor)
{
	EAttachmentStore *store;
	EAttachmentView *view;

	view = E_ATTACHMENT_VIEW (editor->priv->attachment_view);
	store = e_attachment_view_get_store (view);

	e_attachment_store_run_load_dialog (store, GTK_WINDOW (editor));
}

static void
action_classification_cb (GtkRadioAction *action,
                          GtkRadioAction *current,
                          CompEditor *editor)
{
	comp_editor_set_changed (editor, TRUE);
}

static void
action_close_cb (GtkAction *action,
                 CompEditor *editor)
{
	commit_all_fields (editor);

	if (prompt_and_save_changes (editor, TRUE))
		close_dialog (editor);
}

static void
action_help_cb (GtkAction *action,
                CompEditor *editor)
{
	comp_editor_show_help (editor);
}

static void
action_print_cb (GtkAction *action,
                 CompEditor *editor)
{
	CompEditorPrivate *priv = editor->priv;
	ECalComponent *comp;
	GList *l;
	icalcomponent *component;
	icalcomponent *clone;
	icaltimezone *zone;
	gboolean use_24_hour_format;

	comp = e_cal_component_new ();
	component = e_cal_component_get_icalcomponent (priv->comp);
	clone = icalcomponent_new_clone (component);
	e_cal_component_set_icalcomponent (comp, clone);

	for (l = priv->pages; l != NULL; l = l->next)
		 comp_editor_page_fill_component (l->data, comp);

	zone = comp_editor_get_timezone (editor);
	use_24_hour_format = comp_editor_get_use_24_hour_format (editor);

	print_comp (
		comp, priv->cal_client, zone, use_24_hour_format,
		GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);

	g_object_unref (comp);
}

static void
action_print_preview_cb (GtkAction *action,
                         CompEditor *editor)
{
	CompEditorPrivate *priv = editor->priv;
	ECalComponent *comp;
	GList *l;
	icalcomponent *component;
	icalcomponent *clone;
	icaltimezone *zone;
	gboolean use_24_hour_format;

	comp = e_cal_component_new ();
	component = e_cal_component_get_icalcomponent (priv->comp);
	clone = icalcomponent_new_clone (component);
	e_cal_component_set_icalcomponent (comp, clone);

	for (l = priv->pages; l != NULL; l = l->next)
		 comp_editor_page_fill_component (l->data, comp);

	zone = comp_editor_get_timezone (editor);
	use_24_hour_format = comp_editor_get_use_24_hour_format (editor);

	print_comp (
		comp, priv->cal_client, zone, use_24_hour_format,
		GTK_PRINT_OPERATION_ACTION_PREVIEW);

	g_object_unref (comp);
}

static gboolean
remove_event_dialog (ECalClient *client,
                     ECalComponent *comp,
                     GtkWindow *parent)
{
	GtkWidget *dialog;
	gboolean ret;

	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), TRUE);

	dialog = gtk_message_dialog_new (
		parent, 0, GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_YES_NO, "%s", _("Keep original item?"));
	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	ret = gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES;
	gtk_widget_destroy (dialog);

	return ret;
}

static void
save_and_close_editor (CompEditor *editor,
                       gboolean can_close)
{
	CompEditorPrivate *priv = editor->priv;
	EAttachmentStore *store;
	EAttachmentView *view;
	ECalComponentText text;
	gboolean delegated = FALSE;
	gboolean correct = FALSE;
	ECalComponent *comp;
	const gchar *uid = NULL;

	view = E_ATTACHMENT_VIEW (priv->attachment_view);
	store = e_attachment_view_get_store (view);

	if (e_attachment_store_get_num_loading (store) > 0) {
		gboolean response = 1;
		ECalComponentVType vtype = e_cal_component_get_vtype (editor->priv->comp);

		if (vtype == E_CAL_COMPONENT_EVENT)
			response = e_alert_run_dialog_for_args (
				GTK_WINDOW (editor),
				"calendar:ask-send-event-pending-download",
				NULL);
		else
			response = e_alert_run_dialog_for_args (
				GTK_WINDOW (editor),
				"calendar:ask-send-task-pending-download",
				NULL);

		if (response != GTK_RESPONSE_YES)
			return;
	}

	if (e_client_is_readonly (E_CLIENT (priv->cal_client))) {
		e_alert_submit (
			E_ALERT_SINK (editor),
			"calendar:prompt-read-only-cal-editor",
			e_source_get_display_name (
				e_client_get_source (E_CLIENT (priv->cal_client))),
			NULL);
		return;
	}

	if ((comp_editor_get_flags (editor) & COMP_EDITOR_IS_ASSIGNED) != 0
	    && e_cal_component_get_vtype (priv->comp) == E_CAL_COMPONENT_TODO
	    && e_client_check_capability (E_CLIENT (priv->cal_client), CAL_STATIC_CAPABILITY_NO_TASK_ASSIGNMENT)) {
		e_alert_submit (
			E_ALERT_SINK (editor),
			"calendar:prompt-no-task-assignment-editor",
			e_source_get_display_name (
				e_client_get_source (E_CLIENT (priv->cal_client))),
			NULL);
		return;
	}

	commit_all_fields (editor);
	if (e_cal_component_has_recurrences (priv->comp)) {
		if (!recur_component_dialog (
			priv->cal_client, priv->comp, &priv->mod,
			GTK_WINDOW (editor), delegated))
			return;
	} else if (e_cal_component_is_instance (priv->comp))
		priv->mod = E_CAL_OBJ_MOD_THIS;

	comp = comp_editor_get_current_comp (editor, &correct);
	e_cal_component_get_summary (comp, &text);
	g_object_unref (comp);

	if (!correct)
		return;

	if (!text.value)
		if (!send_component_prompt_subject (
			(GtkWindow *) editor, priv->cal_client, priv->comp))
			return;

	gtk_widget_set_sensitive (GTK_WIDGET (editor), FALSE);

	if (save_comp_with_send (editor)) {
		CompEditorFlags flags;
		gboolean delegate;

		flags = comp_editor_get_flags (editor);
		delegate = flags & COMP_EDITOR_DELEGATE;

		if (delegate && !remove_event_dialog (
			priv->cal_client, priv->comp, GTK_WINDOW (editor))) {
			GError *error = NULL;

			e_cal_component_get_uid (priv->comp, &uid);

			if (e_cal_component_is_instance (priv->comp) ||
				e_cal_component_has_recurrences (priv->comp)) {
				gchar *rid;
				rid = e_cal_component_get_recurid_as_string (priv->comp);
				e_cal_client_remove_object_sync (
					priv->cal_client, uid, rid,
					priv->mod, NULL, &error);
				g_free (rid);
			} else
				e_cal_client_remove_object_sync (
					priv->cal_client, uid, NULL,
					E_CAL_OBJ_MOD_THIS, NULL, &error);

			g_clear_error (&error);
		}
	} else
		correct = FALSE;

	gtk_widget_set_sensitive (GTK_WIDGET (editor), TRUE);

	if (correct) {
		if (can_close)
			close_dialog (editor);
		else {
			ECalComponent *comp;
			ECalClientSourceType source_type;
			icalcomponent *icalcomp = NULL;
			const gchar *uid = NULL;
			gchar *rid = NULL;
			GError *error = NULL;

			comp_editor_set_changed (editor, FALSE);

			/*
			 * A server can modify the event on save. Considering this, it is needed to fetch the updated
			 * version of the event from server, updating the component, then user can keep editing the
			 * event
			 */
			e_cal_component_get_uid (priv->comp, &uid);
			rid = e_cal_component_get_recurid_as_string (priv->comp);

			source_type = e_cal_client_get_source_type (priv->cal_client);
			e_cal_client_get_object_sync (
				priv->cal_client, uid, rid,
				&icalcomp, NULL, &error);
			if (error != NULL) {
				switch (source_type) {
					case (E_CAL_CLIENT_SOURCE_TYPE_TASKS):
						g_warning ("Unable to retrieve saved component from the task list, returned error was: %s", error->message);
						break;
					case (E_CAL_CLIENT_SOURCE_TYPE_MEMOS):
						g_warning ("Unable to retrieve saved component from the memo list, returned error was: %s", error->message);
						break;
					case (E_CAL_CLIENT_SOURCE_TYPE_EVENTS):
					default:
						g_warning ("Unable to retrieve saved component from the calendar, returned error was: %s", error->message);
						break;
				}
				g_clear_error (&error);
				e_notice (
					GTK_WINDOW (editor),
					GTK_MESSAGE_ERROR,
					_("Unable to synchronize with the server"));
			} else {
				comp = e_cal_component_new ();
				if (e_cal_component_set_icalcomponent (comp, icalcomp)) {
					gboolean has_recurrences;

					has_recurrences = e_cal_component_has_recurrences (comp);

					if (has_recurrences && priv->mod == E_CAL_OBJ_MOD_ALL)
						comp_util_sanitize_recurrence_master_sync (comp, priv->cal_client, NULL, NULL);

					comp_editor_edit_comp (editor, comp);
				} else {
					switch (source_type) {
						case (E_CAL_CLIENT_SOURCE_TYPE_TASKS):
							g_warning ("Unable to update the editor with the retrieved component from the task list");
						break;
						case (E_CAL_CLIENT_SOURCE_TYPE_MEMOS):
							g_warning ("Unable to update the editor with the retrieved component from the memo list");
							break;
						case (E_CAL_CLIENT_SOURCE_TYPE_EVENTS):
						default:
							g_warning ("Unable to update the editor with the retrieved component from the calendar");
							break;
					}
					e_notice (
						GTK_WINDOW (editor),
						GTK_MESSAGE_ERROR,
						_("Unable to synchronize with the server"));
					icalcomponent_free (icalcomp);
				}
				g_object_unref (comp);
			}
			g_free (rid);
		}
	}
}

static void
action_save_cb (GtkAction *action,
                CompEditor *editor)
{
	save_and_close_editor (editor, FALSE);
}

static void
action_save_and_close_cb (GtkAction *action,
                          CompEditor *editor)
{
	save_and_close_editor (editor, TRUE);
}

static void
action_view_categories_cb (GtkToggleAction *action,
                           CompEditor *editor)
{
	CompEditorClass *class;
	gboolean active;

	class = COMP_EDITOR_GET_CLASS (editor);
	active = gtk_toggle_action_get_active (action);

	if (class->show_categories != NULL)
		class->show_categories (editor, active);
}

static void
action_view_role_cb (GtkToggleAction *action,
                     CompEditor *editor)
{
	CompEditorClass *class;
	gboolean active;

	class = COMP_EDITOR_GET_CLASS (editor);
	active = gtk_toggle_action_get_active (action);

	if (class->show_role != NULL)
		class->show_role (editor, active);
}

static void
action_view_rsvp_cb (GtkToggleAction *action,
                     CompEditor *editor)
{
	CompEditorClass *class;
	gboolean active;

	class = COMP_EDITOR_GET_CLASS (editor);
	active = gtk_toggle_action_get_active (action);

	if (class->show_rsvp != NULL)
		class->show_rsvp (editor, active);
}

static void
action_view_status_cb (GtkToggleAction *action,
                       CompEditor *editor)
{
	CompEditorClass *class;
	gboolean active;

	class = COMP_EDITOR_GET_CLASS (editor);
	active = gtk_toggle_action_get_active (action);

	if (class->show_status != NULL)
		class->show_status (editor, active);
}

static void
action_view_time_zone_cb (GtkToggleAction *action,
                          CompEditor *editor)
{
	CompEditorClass *class;
	gboolean active;

	class = COMP_EDITOR_GET_CLASS (editor);
	active = gtk_toggle_action_get_active (action);

	if (class->show_time_zone != NULL)
		class->show_time_zone (editor, active);
}

static void
action_view_type_cb (GtkToggleAction *action,
                     CompEditor *editor)
{
	CompEditorClass *class;
	gboolean active;

	class = COMP_EDITOR_GET_CLASS (editor);
	active = gtk_toggle_action_get_active (action);

	if (class->show_type != NULL)
		class->show_type (editor, active);
}

static GtkActionEntry core_entries[] = {

	{ "close",
	  "window-close",
	  N_("_Close"),
	  "<Control>w",
	  N_("Close the current window"),
	  G_CALLBACK (action_close_cb) },

	{ "copy-clipboard",
	  "edit-copy",
	  N_("_Copy"),
	  "<Control>c",
	  N_("Copy the selection"),
	  NULL },  /* Handled by EFocusTracker */

	{ "cut-clipboard",
	  "edit-cut",
	  N_("Cu_t"),
	  "<Control>x",
	  N_("Cut the selection"),
	  NULL },  /* Handled by EFocusTracker */

	{ "delete-selection",
	  "edit-delete",
	  N_("_Delete"),
	  NULL,
	  N_("Delete the selection"),
	  NULL },  /* Handled by EFocusTracker */

	{ "help",
	  "help-browser",
	  N_("_Help"),
	  "F1",
	  N_("View help"),
	  G_CALLBACK (action_help_cb) },

	{ "paste-clipboard",
	  "edit-paste",
	  N_("_Paste"),
	  "<Control>v",
	  N_("Paste the clipboard"),
	  NULL },  /* Handled by EFocusTracker */

	{ "print",
	  "document-print",
	  N_("_Print..."),
	  "<Control>p",
	  NULL,
	  G_CALLBACK (action_print_cb) },

	{ "print-preview",
	  "document-print-preview",
	  N_("Pre_view..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_print_preview_cb) },

	{ "save",
	  "document-save",
	  N_("_Save"),
	  "<Control>s",
	  N_("Save current changes"),
	  G_CALLBACK (action_save_cb) },

	{ "save-and-close",
	  NULL,
	  N_("Save and Close"),
	  NULL,
	  N_("Save current changes and close editor"),
	  G_CALLBACK (action_save_and_close_cb) },

	{ "select-all",
	  "edit-select-all",
	  N_("Select _All"),
	  "<Control>a",
	  N_("Select all text"),
	  NULL },  /* Handled by EFocusTracker */

	{ "undo",
	  "edit-undo",
	  N_("_Undo"),
	  "<Control>z",
	  N_("Undo"),
	  NULL },  /* Handled by EFocusTracker */

	{ "redo",
	  "edit-redo",
	  N_("_Redo"),
	  "<Control>y",
	  N_("Redo"),
	  NULL },  /* Handled by EFocusTracker */

	/* Menus */

	{ "classification-menu",
	  NULL,
	  N_("_Classification"),
	  NULL,
	  NULL,
	  NULL },

	{ "edit-menu",
	  NULL,
	  N_("_Edit"),
	  NULL,
	  NULL,
	  NULL },

	{ "file-menu",
	  NULL,
	  N_("_File"),
	  NULL,
	  NULL,
	  NULL },

	{ "help-menu",
	  NULL,
	  N_("_Help"),
	  NULL,
	  NULL,
	  NULL },

	{ "insert-menu",
	  NULL,
	  N_("_Insert"),
	  NULL,
	  NULL,
	  NULL },

	{ "options-menu",
	  NULL,
	  N_("_Options"),
	  NULL,
	  NULL,
	  NULL },

	{ "view-menu",
	  NULL,
	  N_("_View"),
	  NULL,
	  NULL,
	  NULL }
};

static GtkActionEntry individual_entries[] = {

	{ "attach",
	  "mail-attachment",
	  N_("_Attachment..."),
	  "<Control>m",
	  N_("Attach a file"),
	  G_CALLBACK (action_attach_cb) }
};

static GtkToggleActionEntry core_toggle_entries[] = {

	{ "view-categories",
	  NULL,
	  N_("_Categories"),
	  NULL,
	  N_("Toggles whether to display categories"),
	  G_CALLBACK (action_view_categories_cb),
	  FALSE },

	{ "view-time-zone",
	  "stock_timezone",
	  N_("Time _Zone"),
	  NULL,
	  N_("Toggles whether the time zone is displayed"),
	  G_CALLBACK (action_view_time_zone_cb),
	  FALSE }
};

static GtkRadioActionEntry classification_radio_entries[] = {

	{ "classify-public",
	  NULL,
	  N_("Pu_blic"),
	  NULL,
	  N_("Classify as public"),
	  E_CAL_COMPONENT_CLASS_PUBLIC },

	{ "classify-private",
	  NULL,
	  N_("_Private"),
	  NULL,
	  N_("Classify as private"),
	  E_CAL_COMPONENT_CLASS_PRIVATE },

	{ "classify-confidential",
	  NULL,
	  N_("_Confidential"),
	  NULL,
	  N_("Classify as confidential"),
	  E_CAL_COMPONENT_CLASS_CONFIDENTIAL }
};

static GtkToggleActionEntry coordinated_toggle_entries[] = {

	{ "view-role",
	  NULL,
	  N_("R_ole Field"),
	  NULL,
	  N_("Toggles whether the Role field is displayed"),
	  G_CALLBACK (action_view_role_cb),
	  FALSE },

	{ "view-rsvp",
	  NULL,
	  N_("_RSVP"),
	  NULL,
	  N_("Toggles whether the RSVP field is displayed"),
	  G_CALLBACK (action_view_rsvp_cb),
	  FALSE },

	{ "view-status",
	  NULL,
	  N_("_Status Field"),
	  NULL,
	  N_("Toggles whether the Status field is displayed"),
	  G_CALLBACK (action_view_status_cb),
	  FALSE },

	{ "view-type",
	  NULL,
	  N_("_Type Field"),
	  NULL,
	  N_("Toggles whether the Attendee Type is displayed"),
	  G_CALLBACK (action_view_type_cb),
	  FALSE }
};

static void
comp_editor_set_shell (CompEditor *editor,
                       EShell *shell)
{
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (editor->priv->shell == NULL);

	editor->priv->shell = shell;

	g_object_add_weak_pointer (G_OBJECT (shell), &editor->priv->shell);
}

static void
comp_editor_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHANGED:
			comp_editor_set_changed (
				COMP_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_CLIENT:
			comp_editor_set_client (
				COMP_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_FLAGS:
			comp_editor_set_flags (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_SHELL:
			comp_editor_set_shell (
				COMP_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_SUMMARY:
			comp_editor_set_summary (
				COMP_EDITOR (object),
				g_value_get_string (value));
			return;

		case PROP_TIMEZONE:
			comp_editor_set_timezone (
				COMP_EDITOR (object),
				g_value_get_pointer (value));
			return;

		case PROP_USE_24_HOUR_FORMAT:
			comp_editor_set_use_24_hour_format (
				COMP_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_WEEK_START_DAY:
			comp_editor_set_week_start_day (
				COMP_EDITOR (object),
				g_value_get_enum (value));
			return;

		case PROP_WORK_DAY_END_HOUR:
			comp_editor_set_work_day_end_hour (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_END_MINUTE:
			comp_editor_set_work_day_end_minute (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_HOUR:
			comp_editor_set_work_day_start_hour (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_MINUTE:
			comp_editor_set_work_day_start_minute (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_MON:
			comp_editor_set_work_day_start_mon (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_END_MON:
			comp_editor_set_work_day_end_mon (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_TUE:
			comp_editor_set_work_day_start_tue (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_END_TUE:
			comp_editor_set_work_day_end_tue (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_WED:
			comp_editor_set_work_day_start_wed (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_END_WED:
			comp_editor_set_work_day_end_wed (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_THU:
			comp_editor_set_work_day_start_thu (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_END_THU:
			comp_editor_set_work_day_end_thu (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_FRI:
			comp_editor_set_work_day_start_fri (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_END_FRI:
			comp_editor_set_work_day_end_fri (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_SAT:
			comp_editor_set_work_day_start_sat (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_END_SAT:
			comp_editor_set_work_day_end_sat (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_START_SUN:
			comp_editor_set_work_day_start_sun (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_WORK_DAY_END_SUN:
			comp_editor_set_work_day_end_sun (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
comp_editor_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHANGED:
			g_value_set_boolean (
				value, comp_editor_get_changed (
				COMP_EDITOR (object)));
			return;

		case PROP_CLIENT:
			g_value_set_object (
				value, comp_editor_get_client (
				COMP_EDITOR (object)));
			return;

		case PROP_FLAGS:
			g_value_set_int (
				value, comp_editor_get_flags (
				COMP_EDITOR (object)));
			return;

		case PROP_FOCUS_TRACKER:
			g_value_set_object (
				value, comp_editor_get_focus_tracker (
				COMP_EDITOR (object)));
			return;

		case PROP_SHELL:
			g_value_set_object (
				value, comp_editor_get_shell (
				COMP_EDITOR (object)));
			return;

		case PROP_SUMMARY:
			g_value_set_string (
				value, comp_editor_get_summary (
				COMP_EDITOR (object)));
			return;

		case PROP_TIMEZONE:
			g_value_set_pointer (
				value, comp_editor_get_timezone (
				COMP_EDITOR (object)));
			return;

		case PROP_USE_24_HOUR_FORMAT:
			g_value_set_boolean (
				value, comp_editor_get_use_24_hour_format (
				COMP_EDITOR (object)));
			return;

		case PROP_WEEK_START_DAY:
			g_value_set_enum (
				value, comp_editor_get_week_start_day (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_END_HOUR:
			g_value_set_int (
				value, comp_editor_get_work_day_end_hour (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_END_MINUTE:
			g_value_set_int (
				value, comp_editor_get_work_day_end_minute (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_START_HOUR:
			g_value_set_int (
				value, comp_editor_get_work_day_start_hour (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_START_MINUTE:
			g_value_set_int (
				value, comp_editor_get_work_day_start_minute (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_START_MON:
			g_value_set_int (
				value, comp_editor_get_work_day_start_mon (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_END_MON:
			g_value_set_int (
				value, comp_editor_get_work_day_end_mon (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_START_TUE:
			g_value_set_int (
				value, comp_editor_get_work_day_start_tue (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_END_TUE:
			g_value_set_int (
				value, comp_editor_get_work_day_end_tue (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_START_WED:
			g_value_set_int (
				value, comp_editor_get_work_day_start_wed (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_END_WED:
			g_value_set_int (
				value, comp_editor_get_work_day_end_wed (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_START_THU:
			g_value_set_int (
				value, comp_editor_get_work_day_start_thu (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_END_THU:
			g_value_set_int (
				value, comp_editor_get_work_day_end_thu (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_START_FRI:
			g_value_set_int (
				value, comp_editor_get_work_day_start_fri (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_END_FRI:
			g_value_set_int (
				value, comp_editor_get_work_day_end_fri (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_START_SAT:
			g_value_set_int (
				value, comp_editor_get_work_day_start_sat (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_END_SAT:
			g_value_set_int (
				value, comp_editor_get_work_day_end_sat (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_START_SUN:
			g_value_set_int (
				value, comp_editor_get_work_day_start_sun (
				COMP_EDITOR (object)));
			return;

		case PROP_WORK_DAY_END_SUN:
			g_value_set_int (
				value, comp_editor_get_work_day_end_sun (
				COMP_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
unref_page_cb (gpointer editor_page,
               gpointer comp_editor)
{
	if (IS_COMP_EDITOR_PAGE (editor_page)) {
		GtkWidget *page_widget;
		CompEditorPage *page = COMP_EDITOR_PAGE (editor_page);
		CompEditor *editor = COMP_EDITOR (comp_editor);

		g_return_if_fail (page != NULL);
		g_return_if_fail (editor != NULL);

		page_widget = comp_editor_page_get_widget (page);
		g_signal_handlers_disconnect_matched (
			page_widget, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page);
	}

	g_signal_handlers_disconnect_matched (
		editor_page, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, comp_editor);
	g_object_unref (editor_page);
}

static void
comp_editor_dispose (GObject *object)
{
	CompEditorPrivate *priv;

	priv = COMP_EDITOR_GET_PRIVATE (object);

	if (priv->shell != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->shell), &priv->shell);
		priv->shell = NULL;
	}

	if (priv->focus_tracker != NULL) {
		g_object_unref (priv->focus_tracker);
		priv->focus_tracker = NULL;
	}

	if (priv->window_group != NULL) {
		g_object_unref (priv->window_group);
		priv->window_group = NULL;
	}

	if (priv->cal_client) {
		g_object_unref (priv->cal_client);
		priv->cal_client = NULL;
	}

	if (priv->source_client) {
		g_object_unref (priv->source_client);
		priv->source_client = NULL;
	}

	if (priv->view_cancellable) {
		g_cancellable_cancel (priv->view_cancellable);
		g_object_unref (priv->view_cancellable);
		priv->view_cancellable = NULL;
	}

	if (priv->view) {
		g_signal_handlers_disconnect_matched (
			priv->view, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->view);
		priv->view = NULL;
	}

	if (priv->attachment_view) {
		EAttachmentStore *store;

		store = e_attachment_view_get_store (
			E_ATTACHMENT_VIEW (priv->attachment_view));
		g_signal_handlers_disconnect_matched (
			store, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->attachment_view);
		priv->attachment_view = NULL;
	}

	/* We want to destroy the pages after the widgets get destroyed,
	 * since they have lots of signal handlers connected to the widgets
	 * with the pages as the data. */
	g_list_foreach (priv->pages, (GFunc) unref_page_cb, object);
	g_list_free (priv->pages);
	priv->pages = NULL;

	if (priv->comp) {
		g_object_unref (priv->comp);
		priv->comp = NULL;
	}

	if (priv->ui_manager != NULL) {
		g_object_unref (priv->ui_manager);
		priv->ui_manager = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (comp_editor_parent_class)->dispose (object);
}

static void
comp_editor_finalize (GObject *object)
{
	CompEditorPrivate *priv;

	priv = COMP_EDITOR_GET_PRIVATE (object);

	g_object_unref (priv->calendar_settings);
	g_free (priv->summary);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (comp_editor_parent_class)->finalize (object);
}

static void
comp_editor_constructed (GObject *object)
{
	e_extensible_load_extensions (E_EXTENSIBLE (object));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (comp_editor_parent_class)->constructed (object);
}

static void
comp_editor_bind_settings (CompEditor *editor)
{
	GtkAction *action;

	g_return_if_fail (editor != NULL);

	action = comp_editor_get_action (editor, "view-categories");
	g_settings_bind (
		editor->priv->calendar_settings, "editor-show-categories",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);

	action = comp_editor_get_action (editor, "view-role");
	g_settings_bind (
		editor->priv->calendar_settings, "editor-show-role",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);

	action = comp_editor_get_action (editor, "view-rsvp");
	g_settings_bind (
		editor->priv->calendar_settings, "editor-show-rsvp",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);

	action = comp_editor_get_action (editor, "view-status");
	g_settings_bind (
		editor->priv->calendar_settings, "editor-show-status",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);

	action = comp_editor_get_action (editor, "view-time-zone");
	g_settings_bind (
		editor->priv->calendar_settings, "editor-show-timezone",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);

	action = comp_editor_get_action (editor, "view-type");
	g_settings_bind (
		editor->priv->calendar_settings, "editor-show-type",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);
}

static gboolean
comp_editor_delete_event (GtkWidget *widget,
                          GdkEventAny *event)
{
	CompEditor *editor;

	editor = COMP_EDITOR (widget);

	commit_all_fields (editor);

	if (prompt_and_save_changes (editor, TRUE))
		close_dialog (editor);

	return TRUE;
}

static gboolean
comp_editor_key_press_event (GtkWidget *widget,
                             GdkEventKey *event)
{
	CompEditor *editor;

	editor = COMP_EDITOR (widget);

	if (event->keyval == GDK_KEY_Escape) {
		commit_all_fields (editor);

		if (prompt_and_save_changes (editor, TRUE))
			close_dialog (editor);

		return TRUE;
	}

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (comp_editor_parent_class)->
		key_press_event (widget, event);
}

static gboolean
comp_editor_drag_motion (GtkWidget *widget,
                         GdkDragContext *context,
                         gint x,
                         gint y,
                         guint time)
{
	CompEditorPrivate *priv;
	EAttachmentView *view;

	priv = COMP_EDITOR_GET_PRIVATE (widget);
	view = E_ATTACHMENT_VIEW (priv->attachment_view);

	return e_attachment_view_drag_motion (view, context, x, y, time);
}

static void
comp_editor_drag_data_received (GtkWidget *widget,
                                GdkDragContext *context,
                                gint x,
                                gint y,
                                GtkSelectionData *selection,
                                guint info,
                                guint time)
{
	CompEditorPrivate *priv;
	EAttachmentView *view;

	priv = COMP_EDITOR_GET_PRIVATE (widget);
	view = E_ATTACHMENT_VIEW (priv->attachment_view);

	/* Forward the data to the attachment view.  Note that calling
	 * e_attachment_view_drag_data_received() will not work because
	 * that function only handles the case where all the other drag
	 * handlers have failed. */
	e_attachment_paned_drag_data_received (
		E_ATTACHMENT_PANED (view),
		context, x, y, selection, info, time);
}

static void
comp_editor_class_init (CompEditorClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (CompEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = comp_editor_set_property;
	object_class->get_property = comp_editor_get_property;
	object_class->dispose = comp_editor_dispose;
	object_class->finalize = comp_editor_finalize;
	object_class->constructed = comp_editor_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->delete_event = comp_editor_delete_event;
	widget_class->key_press_event = comp_editor_key_press_event;
	widget_class->drag_motion = comp_editor_drag_motion;
	widget_class->drag_data_received = comp_editor_drag_data_received;

	class->help_section = "memos-usage";
	class->edit_comp = real_edit_comp;
	class->send_comp = real_send_comp;
	class->object_created = NULL;

	g_object_class_install_property (
		object_class,
		PROP_CHANGED,
		g_param_spec_boolean (
			"changed",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CLIENT,
		g_param_spec_object (
			"client",
			NULL,
			NULL,
			E_TYPE_CAL_CLIENT,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	/* FIXME: Use a proper flags type instead of int. */
	g_object_class_install_property (
		object_class,
		PROP_FLAGS,
		g_param_spec_int (
			"flags",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_FOCUS_TRACKER,
		g_param_spec_object (
			"focus-tracker",
			NULL,
			NULL,
			E_TYPE_FOCUS_TRACKER,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_SHELL,
		g_param_spec_object (
			"shell",
			NULL,
			NULL,
			E_TYPE_SHELL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_SUMMARY,
		g_param_spec_string (
			"summary",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_TIMEZONE,
		g_param_spec_pointer (
			"timezone",
			"Time Zone",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_USE_24_HOUR_FORMAT,
		g_param_spec_boolean (
			"use-24-hour-format",
			"Use 24-hour Format",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WEEK_START_DAY,
		g_param_spec_enum (
			"week-start-day",
			"Week Start Day",
			NULL,
			E_TYPE_DATE_WEEKDAY,
			G_DATE_MONDAY,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_HOUR,
		g_param_spec_int (
			"work-day-end-hour",
			"Work Day End Hour",
			NULL,
			0,
			23,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_MINUTE,
		g_param_spec_int (
			"work-day-end-minute",
			"Work Day End Minute",
			NULL,
			0,
			59,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_HOUR,
		g_param_spec_int (
			"work-day-start-hour",
			"Work Day Start Hour",
			NULL,
			0,
			23,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_MINUTE,
		g_param_spec_int (
			"work-day-start-minute",
			"Work Day Start Minute",
			NULL,
			0,
			59,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_MON,
		g_param_spec_int (
			"work-day-start-mon",
			"Work Day Start for Monday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_MON,
		g_param_spec_int (
			"work-day-end-mon",
			"Work Day End for Monday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_TUE,
		g_param_spec_int (
			"work-day-start-tue",
			"Work Day Start for Tuesday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_TUE,
		g_param_spec_int (
			"work-day-end-tue",
			"Work Day End for Tuesday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_WED,
		g_param_spec_int (
			"work-day-start-wed",
			"Work Day Start for Wednesday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_WED,
		g_param_spec_int (
			"work-day-end-wed",
			"Work Day End for Wednesday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_THU,
		g_param_spec_int (
			"work-day-start-thu",
			"Work Day Start for Thursday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_THU,
		g_param_spec_int (
			"work-day-end-thu",
			"Work Day End for Thursday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_FRI,
		g_param_spec_int (
			"work-day-start-fri",
			"Work Day Start for Friday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_FRI,
		g_param_spec_int (
			"work-day-end-fri",
			"Work Day End for Friday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_SAT,
		g_param_spec_int (
			"work-day-start-sat",
			"Work Day Start for Saturday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_SAT,
		g_param_spec_int (
			"work-day-end-sat",
			"Work Day End for Saturday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_START_SUN,
		g_param_spec_int (
			"work-day-start-sun",
			"Work Day Start for Sunday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WORK_DAY_END_SUN,
		g_param_spec_int (
			"work-day-end-sun",
			"Work Day End for Sunday",
			NULL,
			-1,
			2359,
			-1,
			G_PARAM_READWRITE));

	signals[OBJECT_CREATED] = g_signal_new (
		"object_created",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (CompEditorClass, object_created),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[COMP_CLOSED] = g_signal_new (
		"comp_closed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (CompEditorClass, comp_closed),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOOLEAN,
		G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static void
comp_editor_init (CompEditor *editor)
{
	CompEditorPrivate *priv;
	EAttachmentView *view;
	EAttachmentStore *store;
	EFocusTracker *focus_tracker;
	GdkDragAction drag_actions;
	GtkTargetList *target_list;
	GtkTargetEntry *targets;
	GtkActionGroup *action_group;
	GtkActionGroup *action_group_2;
	GtkAction *action;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWindow *window;
	EShell *shell;
	gboolean express_mode;
	gint n_targets;
	GError *error = NULL;

	/* FIXME We already have a 'shell' property.  Move stuff
	 *       that depends on it to a constructed() method. */
	shell = e_shell_get_default ();
	express_mode = e_shell_get_express_mode (shell);

	editor->priv = priv = COMP_EDITOR_GET_PRIVATE (editor);

	g_object_weak_ref (
		G_OBJECT (editor), (GWeakNotify)
		comp_editor_weak_notify_cb, NULL);

	active_editors = g_list_prepend (active_editors, editor);

	priv->calendar_settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	/* Each editor window gets its own window group. */
	window = GTK_WINDOW (editor);
	priv->window_group = gtk_window_group_new ();
	gtk_window_group_add_window (priv->window_group, window);

	priv->pages = NULL;
	priv->changed = FALSE;
	priv->needs_send = FALSE;
	priv->mod = E_CAL_OBJ_MOD_ALL;
	priv->existing_org = FALSE;
	priv->user_org = FALSE;
	priv->warned = FALSE;
	priv->is_group_item = FALSE;
	priv->saved = FALSE;

	priv->ui_manager = gtk_ui_manager_new ();

	gtk_window_add_accel_group (
		GTK_WINDOW (editor),
		gtk_ui_manager_get_accel_group (priv->ui_manager));

	/* Setup Action Groups */

	action_group = gtk_action_group_new ("core");
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (
		action_group, core_entries,
		G_N_ELEMENTS (core_entries), editor);
	gtk_action_group_add_toggle_actions (
		action_group, core_toggle_entries,
		G_N_ELEMENTS (core_toggle_entries), editor);
	gtk_ui_manager_insert_action_group (
		priv->ui_manager, action_group, 0);
	g_object_unref (action_group);

	action = gtk_action_group_get_action (action_group, "save-and-close");
	if (action) {
		GtkAction *save_action;
		GIcon *icon;
		GIcon *emblemed_icon;
		GEmblem *emblem;

		icon = g_themed_icon_new ("window-close");
		emblemed_icon = g_themed_icon_new ("document-save");
		emblem = g_emblem_new (emblemed_icon);
		g_object_unref (emblemed_icon);

		emblemed_icon = g_emblemed_icon_new (icon, emblem);
		g_object_unref (emblem);
		g_object_unref (icon);

		gtk_action_set_gicon (action, emblemed_icon);

		g_object_unref (emblemed_icon);

		save_action = gtk_action_group_get_action (action_group, "save");
		e_binding_bind_property (
			save_action, "sensitive",
			action, "sensitive",
			G_BINDING_SYNC_CREATE);
	}

	action_group = gtk_action_group_new ("individual");
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (
		action_group, individual_entries,
		G_N_ELEMENTS (individual_entries), editor);
	gtk_action_group_add_radio_actions (
		action_group, classification_radio_entries,
		G_N_ELEMENTS (classification_radio_entries),
		E_CAL_COMPONENT_CLASS_PUBLIC,
		G_CALLBACK (action_classification_cb), editor);
	gtk_ui_manager_insert_action_group (
		priv->ui_manager, action_group, 0);
	g_object_unref (action_group);

	action_group = gtk_action_group_new ("editable");
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_ui_manager_insert_action_group (
		priv->ui_manager, action_group, 0);
	g_object_unref (action_group);

	action_group = gtk_action_group_new ("coordinated");
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_toggle_actions (
		action_group, coordinated_toggle_entries,
		G_N_ELEMENTS (coordinated_toggle_entries), editor);
	gtk_ui_manager_insert_action_group (
		priv->ui_manager, action_group, 0);
	g_object_unref (action_group);

	/* Configure an EFocusTracker to manage selection actions. */

	focus_tracker = e_focus_tracker_new (GTK_WINDOW (editor));

	action = comp_editor_get_action (editor, "cut-clipboard");
	e_focus_tracker_set_cut_clipboard_action (focus_tracker, action);

	action = comp_editor_get_action (editor, "copy-clipboard");
	e_focus_tracker_set_copy_clipboard_action (focus_tracker, action);

	action = comp_editor_get_action (editor, "paste-clipboard");
	e_focus_tracker_set_paste_clipboard_action (focus_tracker, action);

	action = comp_editor_get_action (editor, "delete-selection");
	e_focus_tracker_set_delete_selection_action (focus_tracker, action);

	action = comp_editor_get_action (editor, "select-all");
	e_focus_tracker_set_select_all_action (focus_tracker, action);

	action = comp_editor_get_action (editor, "undo");
	e_focus_tracker_set_undo_action (focus_tracker, action);

	action = comp_editor_get_action (editor, "redo");
	e_focus_tracker_set_redo_action (focus_tracker, action);

	priv->focus_tracker = focus_tracker;

	/* Fine Tuning */

	action = comp_editor_get_action (editor, "attach");
	g_object_set (action, "short-label", _("Attach"), NULL);

	/* Desensitize the "save" action. */
	action = comp_editor_get_action (editor, "save");
	gtk_action_set_sensitive (action, FALSE);

	gtk_ui_manager_add_ui_from_string (priv->ui_manager, ui, -1, &error);
	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	/* Setup Widgets */

	container = GTK_WIDGET (editor);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = comp_editor_get_managed_widget (editor, "/main-menu");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_set_visible (widget, TRUE);

	widget = comp_editor_get_managed_widget (editor, "/main-toolbar");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gtk_style_context_add_class (
		gtk_widget_get_style_context (widget),
		GTK_STYLE_CLASS_PRIMARY_TOOLBAR);

	widget = e_attachment_paned_new ();
	e_attachment_paned_set_resize_toplevel (
		E_ATTACHMENT_PANED (widget), TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->attachment_view = g_object_ref (widget);
	gtk_widget_show (widget);

	if (express_mode) {
		widget = e_attachment_paned_get_view_combo (
			E_ATTACHMENT_PANED (widget));
		gtk_widget_hide (widget);
	}

	container = e_attachment_paned_get_content_area (
		E_ATTACHMENT_PANED (priv->attachment_view));

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), express_mode);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->notebook = GTK_NOTEBOOK (widget);
	gtk_widget_show (widget);

	/* Drag-and-Drop Support */

	view = E_ATTACHMENT_VIEW (priv->attachment_view);
	target_list = e_attachment_view_get_target_list (view);
	drag_actions = e_attachment_view_get_drag_actions (view);

	targets = gtk_target_table_new_from_list (target_list, &n_targets);

	gtk_drag_dest_set (
		GTK_WIDGET (editor), GTK_DEST_DEFAULT_ALL,
		targets, n_targets, drag_actions);

	gtk_target_table_free (targets, n_targets);

	gtk_window_set_type_hint (
		GTK_WINDOW (editor), GDK_WINDOW_TYPE_HINT_NORMAL);

	action_group = comp_editor_get_action_group (editor, "individual");
	action_group_2 = e_attachment_view_get_action_group (view, "editable");

	e_binding_bind_property (
		action_group, "sensitive",
		action_group_2, "sensitive",
		G_BINDING_SYNC_CREATE);

	/* Listen for attachment store changes. */

	store = e_attachment_view_get_store (view);

	g_signal_connect_swapped (
		store, "row-deleted",
		G_CALLBACK (attachment_store_changed_cb), editor);

	g_signal_connect_swapped (
		store, "row-inserted",
		G_CALLBACK (attachment_store_changed_cb), editor);

	comp_editor_bind_settings (editor);

	gtk_application_add_window (
		GTK_APPLICATION (shell), GTK_WINDOW (editor));
}

static gboolean
prompt_and_save_changes (CompEditor *editor,
                         gboolean send)
{
	CompEditorPrivate *priv;
	gboolean correct = FALSE;
	ECalComponent *comp;
	ECalComponentText text;

	priv = editor->priv;

	if (!priv->changed)
		return TRUE;

	switch (save_component_dialog (GTK_WINDOW (editor), priv->comp)) {
	case GTK_RESPONSE_YES: /* Save */
		if (e_client_is_readonly (E_CLIENT (priv->cal_client))) {
			e_alert_submit (
				E_ALERT_SINK (editor),
				"calendar:prompt-read-only-cal-editor",
				e_source_get_display_name (
					e_client_get_source (E_CLIENT (priv->cal_client))),
				NULL);
			/* don't discard changes when selected readonly calendar */
			return FALSE;
		}

		if ((comp_editor_get_flags (editor) & COMP_EDITOR_IS_ASSIGNED) != 0
		    && e_cal_component_get_vtype (priv->comp) == E_CAL_COMPONENT_TODO
		    && e_client_check_capability (E_CLIENT (priv->cal_client), CAL_STATIC_CAPABILITY_NO_TASK_ASSIGNMENT)) {
			e_alert_submit (
				E_ALERT_SINK (editor),
				"calendar:prompt-no-task-assignment-editor",
				e_source_get_display_name (
					e_client_get_source (E_CLIENT (priv->cal_client))),
				NULL);
			return FALSE;
		}

		comp = comp_editor_get_current_comp (editor, &correct);
		e_cal_component_get_summary (comp, &text);
		g_object_unref (comp);

		if (!correct)
			return FALSE;

		if (!text.value)
			if (!send_component_prompt_subject (
				(GtkWindow *) editor, priv->cal_client, priv->comp))
				return FALSE;

		if (e_cal_component_is_instance (priv->comp))
			if (!recur_component_dialog (
				priv->cal_client, priv->comp, &priv->mod,
				GTK_WINDOW (editor), FALSE))
				return FALSE;

		if (send && save_comp_with_send (editor))
			return TRUE;
		else if (!send && save_comp (editor))
			return TRUE;
		else
			return FALSE;
	case GTK_RESPONSE_NO: /* Discard */
		return TRUE;
	case GTK_RESPONSE_CANCEL: /* Cancel */
	default:
		return FALSE;
	}
}

/* Menu callbacks */

static void
comp_editor_show_help (CompEditor *editor)
{
	CompEditorClass *class;

	class = COMP_EDITOR_GET_CLASS (editor);
	g_return_if_fail (class->help_section != NULL);

	e_display_help (GTK_WINDOW (editor), class->help_section);
}

/* Closes the dialog box and emits the appropriate signals */
static void
close_dialog (CompEditor *editor)
{
	CompEditorPrivate *priv = editor->priv;

	g_signal_emit_by_name (editor, "comp_closed", priv->saved);

	/* FIXME Unfortunately we do this here because otherwise corba
	 * calls happen during destruction and we might get a change
	 * notification back when we are in an inconsistent state */
	if (priv->view)
		g_signal_handlers_disconnect_matched (
			priv->view, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, editor);

	gtk_widget_destroy (GTK_WIDGET (editor));
}

static gint
comp_editor_compare (CompEditor *editor_a,
                     const gchar *uid_b)
{
	const gchar *uid_a = NULL;

	e_cal_component_get_uid (editor_a->priv->comp, &uid_a);

	return g_strcmp0 (uid_a, uid_b);
}

void
comp_editor_set_existing_org (CompEditor *editor,
                              gboolean existing_org)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	editor->priv->existing_org = existing_org;
}

gboolean
comp_editor_get_existing_org (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	return editor->priv->existing_org;
}

void
comp_editor_set_user_org (CompEditor *editor,
                          gboolean user_org)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	editor->priv->user_org = user_org;
}

gboolean
comp_editor_get_user_org (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	return editor->priv->user_org;
}

void
comp_editor_set_group_item (CompEditor *editor,
                            gboolean group_item)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	editor->priv->is_group_item = group_item;
}

gboolean
comp_editor_get_group_item (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	return editor->priv->is_group_item;
}

void
comp_editor_set_classification (CompEditor *editor,
                                ECalComponentClassification classification)
{
	GtkAction *action;

	g_return_if_fail (IS_COMP_EDITOR (editor));

	switch (classification) {
		case E_CAL_COMPONENT_CLASS_PUBLIC:
		case E_CAL_COMPONENT_CLASS_PRIVATE:
		case E_CAL_COMPONENT_CLASS_CONFIDENTIAL:
			break;
		default:
			classification = E_CAL_COMPONENT_CLASS_PUBLIC;
			break;
	}

	action = comp_editor_get_action (editor, "classify-public");
	gtk_radio_action_set_current_value (
		GTK_RADIO_ACTION (action), classification);
}

ECalComponentClassification
comp_editor_get_classification (CompEditor *editor)
{
	GtkAction *action;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), 0);

	action = comp_editor_get_action (editor, "classify-public");
	return gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));
}

EShell *
comp_editor_get_shell (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	return editor->priv->shell;
}

void
comp_editor_set_summary (CompEditor *editor,
                         const gchar *summary)
{
	gboolean show_warning;

	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (g_strcmp0 (editor->priv->summary, summary) == 0)
		return;

	g_free (editor->priv->summary);
	editor->priv->summary = g_strdup (summary);

	show_warning =
		!editor->priv->warned &&
		!(editor->priv->flags & COMP_EDITOR_DELEGATE) &&
		editor->priv->existing_org &&
		!editor->priv->user_org && !(editor->priv->flags & COMP_EDITOR_NEW_ITEM);

	if (show_warning) {
		e_notice (
			editor->priv->notebook, GTK_MESSAGE_INFO,
			_("Changes made to this item may be "
			"discarded if an update arrives"));
		editor->priv->warned = TRUE;
	}

	update_window_border (editor, summary);

	g_object_notify (G_OBJECT (editor), "summary");
}

const gchar *
comp_editor_get_summary (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	return editor->priv->summary;
}

icaltimezone *
comp_editor_get_timezone (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	return editor->priv->zone;
}

void
comp_editor_set_timezone (CompEditor *editor,
                          icaltimezone *zone)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->zone == zone)
		return;

	editor->priv->zone = zone;

	g_object_notify (G_OBJECT (editor), "timezone");
}

gboolean
comp_editor_get_use_24_hour_format (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	return editor->priv->use_24_hour_format;
}

void
comp_editor_set_use_24_hour_format (CompEditor *editor,
                                    gboolean use_24_hour_format)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->use_24_hour_format == use_24_hour_format)
		return;

	editor->priv->use_24_hour_format = use_24_hour_format;

	g_object_notify (G_OBJECT (editor), "use-24-hour-format");
}

GDateWeekday
comp_editor_get_week_start_day (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), 0);

	return editor->priv->week_start_day;
}

void
comp_editor_set_week_start_day (CompEditor *editor,
                                GDateWeekday week_start_day)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (g_date_valid_weekday (week_start_day));

	if (week_start_day == editor->priv->week_start_day)
		return;

	editor->priv->week_start_day = week_start_day;

	g_object_notify (G_OBJECT (editor), "week-start-day");
}

gint
comp_editor_get_work_day_end_hour (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), 0);

	return editor->priv->work_day_end_hour;
}

void
comp_editor_set_work_day_end_hour (CompEditor *editor,
                                   gint work_day_end_hour)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_end_hour == work_day_end_hour)
		return;

	editor->priv->work_day_end_hour = work_day_end_hour;

	g_object_notify (G_OBJECT (editor), "work-day-end-hour");
}

gint
comp_editor_get_work_day_end_minute (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), 0);

	return editor->priv->work_day_end_minute;
}

void
comp_editor_set_work_day_end_minute (CompEditor *editor,
                                     gint work_day_end_minute)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_end_minute == work_day_end_minute)
		return;

	editor->priv->work_day_end_minute = work_day_end_minute;

	g_object_notify (G_OBJECT (editor), "work-day-end-minute");
}

gint
comp_editor_get_work_day_start_hour (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), 0);

	return editor->priv->work_day_start_hour;
}

void
comp_editor_set_work_day_start_hour (CompEditor *editor,
                                     gint work_day_start_hour)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_start_hour == work_day_start_hour)
		return;

	editor->priv->work_day_start_hour = work_day_start_hour;

	g_object_notify (G_OBJECT (editor), "work-day-start-hour");
}

gint
comp_editor_get_work_day_start_minute (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), 0);

	return editor->priv->work_day_start_minute;
}

void
comp_editor_set_work_day_start_minute (CompEditor *editor,
                                       gint work_day_start_minute)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_start_minute == work_day_start_minute)
		return;

	editor->priv->work_day_start_minute = work_day_start_minute;

	g_object_notify (G_OBJECT (editor), "work-day-start-minute");
}

gint
comp_editor_get_work_day_start_mon (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), -1);

	return editor->priv->work_day_start_mon;
}

void
comp_editor_set_work_day_start_mon (CompEditor *editor,
				    gint work_day_start)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_start_mon == work_day_start)
		return;

	editor->priv->work_day_start_mon = work_day_start;

	g_object_notify (G_OBJECT (editor), "work-day-start-mon");
}

gint
comp_editor_get_work_day_end_mon (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), -1);

	return editor->priv->work_day_end_mon;
}

void
comp_editor_set_work_day_end_mon (CompEditor *editor,
				  gint work_day_end)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_end_mon == work_day_end)
		return;

	editor->priv->work_day_end_mon = work_day_end;

	g_object_notify (G_OBJECT (editor), "work-day-end-mon");
}

gint
comp_editor_get_work_day_start_tue (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), -1);

	return editor->priv->work_day_start_tue;
}

void
comp_editor_set_work_day_start_tue (CompEditor *editor,
				    gint work_day_start)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_start_tue == work_day_start)
		return;

	editor->priv->work_day_start_tue = work_day_start;

	g_object_notify (G_OBJECT (editor), "work-day-start-tue");
}

gint
comp_editor_get_work_day_end_tue (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), -1);

	return editor->priv->work_day_end_tue;
}

void
comp_editor_set_work_day_end_tue (CompEditor *editor,
				  gint work_day_end)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_end_tue == work_day_end)
		return;

	editor->priv->work_day_end_tue = work_day_end;

	g_object_notify (G_OBJECT (editor), "work-day-end-tue");
}

gint
comp_editor_get_work_day_start_wed (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), -1);

	return editor->priv->work_day_start_wed;
}

void
comp_editor_set_work_day_start_wed (CompEditor *editor,
				    gint work_day_start)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_start_wed == work_day_start)
		return;

	editor->priv->work_day_start_wed = work_day_start;

	g_object_notify (G_OBJECT (editor), "work-day-start-wed");
}

gint
comp_editor_get_work_day_end_wed (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), -1);

	return editor->priv->work_day_end_wed;
}

void
comp_editor_set_work_day_end_wed (CompEditor *editor,
				  gint work_day_end)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_end_wed == work_day_end)
		return;

	editor->priv->work_day_end_wed = work_day_end;

	g_object_notify (G_OBJECT (editor), "work-day-end-wed");
}

gint
comp_editor_get_work_day_start_thu (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), -1);

	return editor->priv->work_day_start_thu;
}

void
comp_editor_set_work_day_start_thu (CompEditor *editor,
				    gint work_day_start)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_start_thu == work_day_start)
		return;

	editor->priv->work_day_start_thu = work_day_start;

	g_object_notify (G_OBJECT (editor), "work-day-start-thu");
}

gint
comp_editor_get_work_day_end_thu (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), -1);

	return editor->priv->work_day_end_thu;
}

void
comp_editor_set_work_day_end_thu (CompEditor *editor,
				  gint work_day_end)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_end_thu == work_day_end)
		return;

	editor->priv->work_day_end_thu = work_day_end;

	g_object_notify (G_OBJECT (editor), "work-day-end-thu");
}

gint
comp_editor_get_work_day_start_fri (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), -1);

	return editor->priv->work_day_start_fri;
}

void
comp_editor_set_work_day_start_fri (CompEditor *editor,
				    gint work_day_start)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_start_fri == work_day_start)
		return;

	editor->priv->work_day_start_fri = work_day_start;

	g_object_notify (G_OBJECT (editor), "work-day-start-fri");
}

gint
comp_editor_get_work_day_end_fri (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), -1);

	return editor->priv->work_day_end_fri;
}

void
comp_editor_set_work_day_end_fri (CompEditor *editor,
				  gint work_day_end)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_end_fri == work_day_end)
		return;

	editor->priv->work_day_end_fri = work_day_end;

	g_object_notify (G_OBJECT (editor), "work-day-end-fri");
}

gint
comp_editor_get_work_day_start_sat (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), -1);

	return editor->priv->work_day_start_sat;
}

void
comp_editor_set_work_day_start_sat (CompEditor *editor,
				    gint work_day_start)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_start_sat == work_day_start)
		return;

	editor->priv->work_day_start_sat = work_day_start;

	g_object_notify (G_OBJECT (editor), "work-day-start-sat");
}

gint
comp_editor_get_work_day_end_sat (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), -1);

	return editor->priv->work_day_end_sat;
}

void
comp_editor_set_work_day_end_sat (CompEditor *editor,
				  gint work_day_end)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_end_sat == work_day_end)
		return;

	editor->priv->work_day_end_sat = work_day_end;

	g_object_notify (G_OBJECT (editor), "work-day-end-sat");
}

gint
comp_editor_get_work_day_start_sun (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), -1);

	return editor->priv->work_day_start_sun;
}

void
comp_editor_set_work_day_start_sun (CompEditor *editor,
				    gint work_day_start)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_start_sun == work_day_start)
		return;

	editor->priv->work_day_start_sun = work_day_start;

	g_object_notify (G_OBJECT (editor), "work-day-start-sun");
}

gint
comp_editor_get_work_day_end_sun (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), -1);

	return editor->priv->work_day_end_sun;
}

void
comp_editor_set_work_day_end_sun (CompEditor *editor,
				  gint work_day_end)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->work_day_end_sun == work_day_end)
		return;

	editor->priv->work_day_end_sun = work_day_end;

	g_object_notify (G_OBJECT (editor), "work-day-end-sun");
}

void
comp_editor_get_work_day_range_for (CompEditor *editor,
				    GDateWeekday weekday,
				    gint *start_hour,
				    gint *start_minute,
				    gint *end_hour,
				    gint *end_minute)
{
	gint start_adept = -1, end_adept = -1;

	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (start_hour != NULL);
	g_return_if_fail (start_minute != NULL);
	g_return_if_fail (end_hour != NULL);
	g_return_if_fail (end_minute != NULL);

	switch (weekday) {
		case G_DATE_MONDAY:
			start_adept = comp_editor_get_work_day_start_mon (editor);
			end_adept = comp_editor_get_work_day_end_mon (editor);
			break;
		case G_DATE_TUESDAY:
			start_adept = comp_editor_get_work_day_start_tue (editor);
			end_adept = comp_editor_get_work_day_end_tue (editor);
			break;
		case G_DATE_WEDNESDAY:
			start_adept = comp_editor_get_work_day_start_wed (editor);
			end_adept = comp_editor_get_work_day_end_wed (editor);
			break;
		case G_DATE_THURSDAY:
			start_adept = comp_editor_get_work_day_start_thu (editor);
			end_adept = comp_editor_get_work_day_end_thu (editor);
			break;
		case G_DATE_FRIDAY:
			start_adept = comp_editor_get_work_day_start_fri (editor);
			end_adept = comp_editor_get_work_day_end_fri (editor);
			break;
		case G_DATE_SATURDAY:
			start_adept = comp_editor_get_work_day_start_sat (editor);
			end_adept = comp_editor_get_work_day_end_sat (editor);
			break;
		case G_DATE_SUNDAY:
			start_adept = comp_editor_get_work_day_start_sun (editor);
			end_adept = comp_editor_get_work_day_end_sun (editor);
			break;
		default:
			break;
	}

	if (start_adept > 0 && (start_adept / 100) >= 0 && (start_adept / 100) <= 23 &&
	    (start_adept % 100) >= 0 && (start_adept % 100) <= 59) {
		*start_hour = start_adept / 100;
		*start_minute = start_adept % 100;
	} else {
		*start_hour = comp_editor_get_work_day_start_hour (editor);
		*start_minute = comp_editor_get_work_day_start_minute (editor);
	}

	if (end_adept > 0 && (end_adept / 100) >= 0 && (end_adept / 100) <= 23 &&
	    (end_adept % 100) >= 0 && (end_adept % 100) <= 59) {
		*end_hour = end_adept / 100;
		*end_minute = end_adept % 100;
	} else {
		*end_hour = comp_editor_get_work_day_end_hour (editor);
		*end_minute = comp_editor_get_work_day_end_minute (editor);
	}
}

/**
 * comp_editor_set_changed:
 * @editor: A component editor
 * @changed: Value to set the changed state to
 *
 * Set the dialog changed state to the given value
 **/
void
comp_editor_set_changed (CompEditor *editor,
                         gboolean changed)
{
	GtkAction *action;
	gboolean show_warning;

	g_return_if_fail (IS_COMP_EDITOR (editor));

	/* always process below changes, because other parts of
	 * the editor listen for 'changed' notifications to update
	 * its widgets, thus do it even the value actually didn't change
	 */
	if (editor->priv->changed != changed) {
		editor->priv->changed = changed;

		action = comp_editor_get_action (editor, "save");
		g_return_if_fail (action != NULL);
		gtk_action_set_sensitive (action, changed);
	}

	show_warning =
		changed && !editor->priv->warned &&
		!(editor->priv->flags & COMP_EDITOR_DELEGATE) &&
		editor->priv->existing_org && !editor->priv->user_org
		&& !(editor->priv->flags & COMP_EDITOR_NEW_ITEM);

	if (show_warning) {
		e_notice (
			editor->priv->notebook, GTK_MESSAGE_INFO,
			_("Changes made to this item may be "
			"discarded if an update arrives"));
		editor->priv->warned = TRUE;
	}

	g_object_notify (G_OBJECT (editor), "changed");
}

/**
 * comp_editor_get_changed:
 * @editor: A component editor
 *
 * Gets the changed state of the dialog
 *
 * Return value: A boolean indicating if the dialog is in a changed
 * state
 **/
gboolean
comp_editor_get_changed (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	return editor->priv->changed;
}

EFocusTracker *
comp_editor_get_focus_tracker (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	return editor->priv->focus_tracker;
}

void
comp_editor_set_flags (CompEditor *editor,
                       CompEditorFlags flags)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	if (editor->priv->flags == flags)
		return;

	editor->priv->flags = flags;
	editor->priv->user_org = flags & COMP_EDITOR_USER_ORG;

	g_object_notify (G_OBJECT (editor), "flags");
}

CompEditorFlags
comp_editor_get_flags (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), 0);

	return editor->priv->flags;
}

GtkUIManager *
comp_editor_get_ui_manager (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	return editor->priv->ui_manager;
}

GtkAction *
comp_editor_get_action (CompEditor *editor,
                        const gchar *action_name)
{
	GtkUIManager *ui_manager;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	ui_manager = comp_editor_get_ui_manager (editor);

	return e_lookup_action (ui_manager, action_name);
}

GtkActionGroup *
comp_editor_get_action_group (CompEditor *editor,
                              const gchar *group_name)
{
	GtkUIManager *ui_manager;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	ui_manager = comp_editor_get_ui_manager (editor);

	return e_lookup_action_group (ui_manager, group_name);
}

GtkWidget *
comp_editor_get_managed_widget (CompEditor *editor,
                                const gchar *widget_path)
{
	GtkUIManager *ui_manager;
	GtkWidget *widget;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);
	g_return_val_if_fail (widget_path != NULL, NULL);

	ui_manager = comp_editor_get_ui_manager (editor);
	widget = gtk_ui_manager_get_widget (ui_manager, widget_path);
	g_return_val_if_fail (widget != NULL, NULL);

	return widget;
}

CompEditor *
comp_editor_find_instance (const gchar *uid)
{
	GList *link;

	g_return_val_if_fail (uid != NULL, NULL);

	link = g_list_find_custom (
		active_editors, uid,
		(GCompareFunc) comp_editor_compare);

	return (link != NULL) ? link->data : NULL;
}

/**
 * comp_editor_set_needs_send:
 * @editor: A component editor
 * @needs_send: Value to set the needs send state to
 *
 * Set the dialog needs send state to the given value
 **/
void
comp_editor_set_needs_send (CompEditor *editor,
                            gboolean needs_send)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	editor->priv->needs_send = needs_send;
}

/**
 * comp_editor_get_needs_send:
 * @editor: A component editor
 *
 * Gets the needs send state of the dialog
 *
 * Return value: A boolean indicating if the dialog is in a needs send
 * state
 **/
gboolean
comp_editor_get_needs_send (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	return editor->priv->needs_send;
}

static void
page_mapped_cb (GtkWidget *page_widget,
                CompEditorPage *page)
{
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (page_widget);
	if (!GTK_IS_WINDOW (toplevel))
		return;

	if (page->accel_group) {
		gtk_window_add_accel_group (
			GTK_WINDOW (toplevel),
			page->accel_group);
	}
}

static void
page_unmapped_cb (GtkWidget *page_widget,
                  CompEditorPage *page)
{
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (page_widget);
	if (!GTK_IS_WINDOW (toplevel))
		return;

	if (page->accel_group) {
		gtk_window_remove_accel_group (GTK_WINDOW (toplevel), page->accel_group);
	}
}

/**
 * comp_editor_append_widget:
 * @editor: A component editor
 * @page: A component editor page
 * @label: Label of the page. Should be NULL if add is FALSE.
 * @add: Add's the page into the notebook if TRUE
 *
 * Appends a page to the notebook if add is TRUE else
 * just adds it to the list of pages.
 **/
void
comp_editor_append_widget (CompEditor *editor,
                         GtkWidget *page,
                         const gchar *label,
                         gboolean add)
{
	CompEditorPrivate *priv;
	GtkWidget *label_widget = NULL;

	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = editor->priv;

	g_object_ref (page);

	if (label)
		label_widget = gtk_label_new_with_mnemonic (label);

	priv->pages = g_list_append (priv->pages, page);

	if (add) {
		gtk_notebook_append_page (priv->notebook, page, label_widget);
		gtk_container_child_set (
			GTK_CONTAINER (priv->notebook), page,
			"tab-fill", FALSE, "tab-expand", FALSE, NULL);
	}

	/* Listen for when the page is mapped/unmapped so we can
	 * install/uninstall the appropriate GtkAccelGroup.
	g_signal_connect (
		page, "map",
		G_CALLBACK (page_mapped_cb), page);
	g_signal_connect (
		page, "unmap",
		G_CALLBACK (page_unmapped_cb), page);
		*/

}

/**
 * comp_editor_append_page:
 * @editor: A component editor
 * @page: A component editor page
 * @label: Label of the page. Should be NULL if add is FALSE.
 * @add: Add's the page into the notebook if TRUE
 *
 * Appends a page to the notebook if add is TRUE else
 * just adds it to the list of pages.
 **/
void
comp_editor_append_page (CompEditor *editor,
                         CompEditorPage *page,
                         const gchar *label,
                         gboolean add)
{
	CompEditorPrivate *priv;
	GtkWidget *page_widget;
	GtkWidget *label_widget = NULL;
	gboolean is_first_page;

	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	priv = editor->priv;

	g_object_ref (page);

	/* If we are editing something, fill the widgets with current info */
	if (priv->comp != NULL) {
		ECalComponent *comp;

		comp = comp_editor_get_current_comp (editor, NULL);
		comp_editor_page_fill_widgets (page, comp);
		g_object_unref (comp);
	}

	page_widget = comp_editor_page_get_widget (page);
	g_return_if_fail (page_widget != NULL);

	if (label)
		label_widget = gtk_label_new_with_mnemonic (label);

	is_first_page = (priv->pages == NULL);

	priv->pages = g_list_append (priv->pages, page);

	if (add) {
		gtk_notebook_append_page (
			priv->notebook, page_widget, label_widget);
		gtk_container_child_set (
			GTK_CONTAINER (priv->notebook), page_widget,
			"tab-fill", FALSE, "tab-expand", FALSE, NULL);
	}

	/* Listen for things happening on the page */
	g_signal_connect_swapped (
		page, "dates_changed",
		G_CALLBACK (page_dates_changed_cb), editor);

	/* Listen for when the page is mapped/unmapped so we can
	 * install/uninstall the appropriate GtkAccelGroup. */
	g_signal_connect (
		page_widget, "map",
		G_CALLBACK (page_mapped_cb), page);
	g_signal_connect (
		page_widget, "unmap",
		G_CALLBACK (page_unmapped_cb), page);

	/* The first page is the main page of the editor, so we ask it to focus
	 * its main widget.
	 */
	if (is_first_page)
		comp_editor_page_focus_main_widget (page);
}

/**
 * comp_editor_remove_page:
 * @editor: A component editor
 * @page: A component editor page
 *
 * Removes the page from the component editor
 **/
void
comp_editor_remove_page (CompEditor *editor,
                         CompEditorPage *page)
{
	CompEditorPrivate *priv;
	GtkWidget *page_widget;
	gint page_num;

	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	priv = editor->priv;

	page_widget = comp_editor_page_get_widget (page);
	page_num = gtk_notebook_page_num (priv->notebook, page_widget);
	if (page_num == -1)
		return;

	/* Disconnect all the signals added in append_page(). */
	g_signal_handlers_disconnect_matched (
		page, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_disconnect_matched (
		page_widget, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page);

	gtk_notebook_remove_page (priv->notebook, page_num);

	priv->pages = g_list_remove (priv->pages, page);
	g_object_unref (page);
}

/**
 * comp_editor_show_page:
 * @editor:
 * @page:
 *
 *
 **/
void
comp_editor_show_page (CompEditor *editor,
                       CompEditorPage *page)
{
	CompEditorPrivate *priv;
	GtkWidget *page_widget;
	gint page_num;

	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	priv = editor->priv;

	page_widget = comp_editor_page_get_widget (page);
	page_num = gtk_notebook_page_num (priv->notebook, page_widget);
	gtk_notebook_set_current_page (priv->notebook, page_num);
}

/**
 * comp_editor_set_client:
 * @editor: A component editor
 * @cal_client: The calendar client to use
 *
 * Sets the calendar client used by the editor to update components
 **/
void
comp_editor_set_client (CompEditor *editor,
                        ECalClient *cal_client)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (cal_client == NULL || E_IS_CAL_CLIENT (cal_client));

	if (editor->priv->cal_client == cal_client)
		return;

	if (cal_client != NULL)
		g_object_ref (cal_client);

	if (editor->priv->cal_client != NULL)
		g_object_unref (editor->priv->cal_client);

	editor->priv->cal_client = cal_client;

	if (editor->priv->source_client == NULL && cal_client != NULL)
		editor->priv->source_client = g_object_ref (cal_client);

	g_object_notify (G_OBJECT (editor), "client");
}

/**
 * comp_editor_get_client:
 * @editor: A component editor
 *
 * Returns the calendar client of the editor
 *
 * Return value: The calendar client of the editor
 **/
ECalClient *
comp_editor_get_client (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	return editor->priv->cal_client;
}

static void
attachment_loaded_cb (EAttachment *attachment,
                      GAsyncResult *result,
                      GtkWindow *parent)
{
	GFileInfo *file_info;
	const gchar *display_name;
	const gchar *uid;
	gchar *new_name;

	/* Prior to 2.27.2, attachment files were named:
	 *
	 *     <component-uid> '-' <actual-filename>
	 *     -------------------------------------
	 *              (one long filename)
	 *
	 * Here we fix the display name if this form is detected so we
	 * don't show the component UID in the user interface.  If the
	 * user saves changes in the editor, the attachment will be
	 * written to disk as:
	 *
	 *     <component-uid> / <actual-filename>
	 *     ---------------   -----------------
	 *       (directory)      (original name)
	 *
	 * So this is a lazy migration from the old form to the new.
	 */

	file_info = e_attachment_ref_file_info (attachment);
	if (file_info == NULL) {
		/* failed to load an attachment file */
		e_attachment_load_handle_error (attachment, result, parent);
		return;
	}

	display_name = g_file_info_get_display_name (file_info);
	uid = g_object_get_data (G_OBJECT (attachment), "uid");

	if (g_str_has_prefix (display_name, uid)) {
		new_name = g_strdup (display_name + strlen (uid) + 1);
		g_file_info_set_display_name (file_info, new_name);
		g_object_notify (G_OBJECT (attachment), "file-info");
		g_free (new_name);
	}

	g_object_unref (file_info);

	e_attachment_load_handle_error (attachment, result, parent);
}

static void
set_attachment_list (CompEditor *editor,
                     GSList *uri_list)
{
	EAttachmentStore *store;
	EAttachmentView *view;
	const gchar *uid = NULL;
	GSList *iter;

	view = E_ATTACHMENT_VIEW (editor->priv->attachment_view);
	store = e_attachment_view_get_store (view);

	if (e_attachment_store_get_num_attachments (store) > 0) {
		/* To prevent repopulating the
		 * bar due to redraw functions in fill_widget.
		 * Assumes it can be set only once.
		 */
		return;
	}

	/* XXX What an awkward API this is.  Takes a return location
	 *     for a constant string instead of just returning it. */
	e_cal_component_get_uid (editor->priv->comp, &uid);

	for (iter = uri_list; iter != NULL; iter = iter->next) {
		EAttachment *attachment;

		attachment = e_attachment_new_for_uri (iter->data);
		e_attachment_store_add_attachment (store, attachment);
		g_object_set_data_full (
			G_OBJECT (attachment),
			"uid", g_strdup (uid),
			(GDestroyNotify) g_free);
		e_attachment_load_async (
			attachment, (GAsyncReadyCallback)
			attachment_loaded_cb, editor);
		g_object_unref (attachment);
	}
}

static void
fill_widgets (CompEditor *editor)
{
	EAttachmentStore *store;
	EAttachmentView *view;
	CompEditorPrivate *priv;
	GtkAction *action;
	GList *iter;

	view = E_ATTACHMENT_VIEW (editor->priv->attachment_view);
	store = e_attachment_view_get_store (view);

	priv = editor->priv;

	/*Check if attachments are available here and set them*/
	if (e_cal_component_has_attachments (priv->comp)) {
		GSList *attachment_list = NULL;
		e_cal_component_get_attachment_list (priv->comp, &attachment_list);
		g_signal_handlers_block_by_func (
			store, G_CALLBACK (attachment_store_changed_cb),
			editor);
		set_attachment_list (editor, attachment_list);
		g_signal_handlers_unblock_by_func (
			store, G_CALLBACK (attachment_store_changed_cb),
			editor);
		g_slist_foreach (attachment_list, (GFunc) g_free, NULL);
		g_slist_free (attachment_list);
	}

	action = comp_editor_get_action (editor, "classify-public");
	g_signal_handlers_block_by_func (
		action, G_CALLBACK (action_classification_cb), editor);

	for (iter = priv->pages; iter != NULL; iter = iter->next) {
		if (IS_COMP_EDITOR_PAGE (iter->data))
			comp_editor_page_fill_widgets (iter->data, priv->comp);
	}

	g_signal_handlers_unblock_by_func (
		action, G_CALLBACK (action_classification_cb), editor);
}

static void
real_edit_comp (CompEditor *editor,
                ECalComponent *comp)
{
	CompEditorPrivate *priv;

	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = editor->priv;

	if (priv->comp) {
		g_object_unref (priv->comp);
		priv->comp = NULL;
	}

	if (comp) {
		priv->comp = e_cal_component_clone (comp);
		comp_editor_copy_new_attendees (priv->comp, comp);
	}

	priv->existing_org = e_cal_component_has_organizer (comp);
	priv->warned = FALSE;

	update_window_border (editor, NULL);

	fill_widgets (editor);

	comp_editor_set_changed (editor, FALSE);

	listen_for_changes (editor);
}

/* TODO These functions should be available in e-cal-component.c */
static void
set_attendees_for_delegation (ECalComponent *comp,
                              const gchar *address,
                              ECalComponentItipMethod method)
{
	icalproperty *prop;
	icalparameter *param;
	icalcomponent *icalcomp;

	icalcomp = e_cal_component_get_icalcomponent (comp);

	for (prop = icalcomponent_get_first_property (icalcomp, ICAL_ATTENDEE_PROPERTY);
	     prop;
	     prop = icalcomponent_get_next_property (icalcomp, ICAL_ATTENDEE_PROPERTY)) {
		const gchar *attendee = icalproperty_get_attendee (prop);
		const gchar *delfrom = NULL;

		param = icalproperty_get_first_parameter (prop, ICAL_DELEGATEDFROM_PARAMETER);
		if (param)
			delfrom = icalparameter_get_delegatedfrom (param);
		if (!(g_str_equal (itip_strip_mailto (attendee), address) ||
				((delfrom && *delfrom) &&
				 g_str_equal (itip_strip_mailto (delfrom), address)))) {
			icalcomponent_remove_property (icalcomp, prop);
		}

	}

}

static void
get_users_from_memo_comp (ECalComponent *comp,
                          GSList **users)
{
	icalcomponent *icalcomp;
	icalproperty *icalprop;
	const gchar *attendees = NULL;
	gchar **emails, **iter;

	icalcomp = e_cal_component_get_icalcomponent (comp);

	for (icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
	     icalprop != NULL;
	     icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY)) {
		const gchar *x_name;

		x_name = icalproperty_get_x_name (icalprop);

		if (g_str_equal (x_name, "X-EVOLUTION-RECIPIENTS"))
			break;
	}

	if (icalprop) {
		attendees = icalproperty_get_x (icalprop);
		emails = g_strsplit (attendees, ";", -1);

		iter = emails;
		while (*iter) {
			*users = g_slist_append (*users, g_strdup (*iter));
			iter++;
		}
		g_strfreev (emails);
	}
}

static gboolean
real_send_comp (CompEditor *editor,
                ECalComponentItipMethod method,
                gboolean strip_alarms)
{
	CompEditorPrivate *priv;
	CompEditorFlags flags;
	EShell *shell;
	ESourceRegistry *registry;
	ECalComponent *send_comp = NULL;
	gchar *address = NULL;
	GSList *users = NULL;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	priv = editor->priv;

	flags = comp_editor_get_flags (editor);
	shell = comp_editor_get_shell (editor);

	registry = e_shell_get_registry (shell);

	if (priv->mod == E_CAL_OBJ_MOD_ALL && e_cal_component_is_instance (priv->comp)) {
		/* Ensure we send the master object, not the instance only */
		icalcomponent *icalcomp = NULL;
		const gchar *uid = NULL;

		e_cal_component_get_uid (priv->comp, &uid);
		e_cal_client_get_object_sync (
			priv->cal_client, uid, NULL, &icalcomp, NULL, NULL);
		if (icalcomp != NULL) {
			send_comp = e_cal_component_new ();
			if (!e_cal_component_set_icalcomponent (send_comp, icalcomp)) {
				icalcomponent_free (icalcomp);
				g_object_unref (send_comp);
				send_comp = NULL;
			}
		}
	}

	if (!send_comp)
		send_comp = e_cal_component_clone (priv->comp);

	comp_editor_copy_new_attendees (send_comp, priv->comp);

	if (e_cal_component_get_vtype (send_comp) == E_CAL_COMPONENT_JOURNAL)
		get_users_from_memo_comp (send_comp, &users);

	/* The user updates the delegated status to the Organizer,
	 * so remove all other attendees. */
	if (flags & COMP_EDITOR_DELEGATE) {
		address = itip_get_comp_attendee (
			registry, send_comp, priv->cal_client);

		if (address)
			set_attendees_for_delegation (send_comp, address, method);
	}

	if (!e_cal_component_has_attachments (priv->comp) ||
		e_client_check_capability (
			E_CLIENT (priv->cal_client),
			CAL_STATIC_CAPABILITY_CREATE_MESSAGES)) {
		if (itip_send_comp_sync (
			registry, method, send_comp, priv->cal_client,
			NULL, NULL, users, strip_alarms,
			priv->flags & COMP_EDITOR_SEND_TO_NEW_ATTENDEES_ONLY, NULL, NULL)) {
			g_object_unref (send_comp);
			return TRUE;
		}
	} else {
		/* Clone the component with attachments set to CID:...  */
		GSList *attach_list = NULL;
		GSList *mime_attach_list, *attach;

		/* mime_attach_list is freed by itip_send_comp_sync */
		mime_attach_list = comp_editor_get_mime_attach_list (editor);

		for (attach = mime_attach_list; attach; attach = attach->next) {
			struct CalMimeAttach *cma = (struct CalMimeAttach *) attach->data;

			attach_list = g_slist_append (
				attach_list, g_strconcat (
				"cid:", cma->content_id, NULL));
		}

		if (attach_list) {
			e_cal_component_set_attachment_list (send_comp, attach_list);

			g_slist_foreach (attach_list, (GFunc) g_free, NULL);
			g_slist_free (attach_list);
		}

		if (itip_send_comp_sync (
			registry, method, send_comp, priv->cal_client,
			NULL, mime_attach_list, users, strip_alarms,
			priv->flags & COMP_EDITOR_SEND_TO_NEW_ATTENDEES_ONLY, NULL, NULL)) {
			gboolean saved = save_comp (editor);

			g_object_unref (send_comp);

			if (!saved)
				comp_editor_set_changed (editor, TRUE);

			return saved;
		}
	}

	g_object_unref (send_comp);
	g_free (address);
	comp_editor_set_changed (editor, TRUE);

	return FALSE;

}

/**
 * comp_editor_edit_comp:
 * @editor: A component editor
 * @comp: A calendar component
 *
 * Starts the editor editing the given component
 **/
void
comp_editor_edit_comp (CompEditor *editor,
                       ECalComponent *comp)
{
	CompEditorClass *class;

	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	class = COMP_EDITOR_GET_CLASS (editor);

	if (class->edit_comp)
		class->edit_comp (editor, comp);
}

ECalComponent *
comp_editor_get_comp (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	return editor->priv->comp;
}

/**
 * comp_editor_get_current_comp
 * @editor: a #CompEditor
 * @correct: Set this no non-%NULL if you are interested to know if all
 * pages reported success when filling component.
 *
 * Returns: Newly allocated component, should be unref-ed by g_object_unref().
 **/
ECalComponent *
comp_editor_get_current_comp (CompEditor *editor,
                              gboolean *correct)
{
	CompEditorPrivate *priv;
	ECalComponent *comp;
	GList *l;
	gboolean all_ok = TRUE;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	priv = editor->priv;

	comp = e_cal_component_clone (priv->comp);
	comp_editor_copy_new_attendees (comp, priv->comp);
	if (priv->changed) {
		for (l = priv->pages; l != NULL; l = l->next) {
			if (IS_COMP_EDITOR_PAGE (l->data))
				all_ok = comp_editor_page_fill_component (l->data, comp) && all_ok;
		}
	}

	if (correct)
		*correct = all_ok;

	return comp;
}

/**
 * comp_editor_save_comp:
 * @editor:
 *
 *
 **/
gboolean
comp_editor_save_comp (CompEditor *editor,
                       gboolean send)
{
	return prompt_and_save_changes (editor, send);
}

/**
 * comp_editor_delete_comp:
 * @editor:
 *
 *
 **/
void
comp_editor_delete_comp (CompEditor *editor)
{
	CompEditorPrivate *priv;
	const gchar *uid;

	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = editor->priv;

	e_cal_component_get_uid (priv->comp, &uid);
	if (e_cal_component_is_instance (priv->comp) ||
		e_cal_component_has_recurrences (priv->comp))
		e_cal_client_remove_object_sync (
			priv->cal_client, uid, NULL,
			E_CAL_OBJ_MOD_ALL, NULL, NULL);
	else
		e_cal_client_remove_object_sync (
			priv->cal_client, uid, NULL,
			E_CAL_OBJ_MOD_THIS, NULL, NULL);
	close_dialog (editor);
}

/**
 * comp_editor_send_comp:
 * @editor:
 * @method:
 *
 *
 **/
gboolean
comp_editor_send_comp (CompEditor *editor,
                       ECalComponentItipMethod method,
                       gboolean strip_alarms)
{
	CompEditorClass *class;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	class = COMP_EDITOR_GET_CLASS (editor);

	if (class->send_comp)
		return class->send_comp (editor, method, strip_alarms);

	return FALSE;
}

gboolean
comp_editor_close (CompEditor *editor)
{
	gboolean close;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	commit_all_fields (editor);

	close = prompt_and_save_changes (editor, TRUE);
	if (close)
		close_dialog (editor);

	return close;
}

/* Utility function to get the mime-attachment list from the attachment
 * bar for sending the comp via itip. The list and its contents must
 * be freed by the caller.
 */
GSList *
comp_editor_get_mime_attach_list (CompEditor *editor)
{
	EAttachmentStore *store;
	EAttachmentView *view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	struct CalMimeAttach *cal_mime_attach;
	GSList *attach_list = NULL;
	gboolean valid;

	view = E_ATTACHMENT_VIEW (editor->priv->attachment_view);
	store = e_attachment_view_get_store (view);

	model = GTK_TREE_MODEL (store);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		EAttachment *attachment;
		CamelDataWrapper *wrapper;
		CamelMimePart *mime_part;
		CamelStream *stream;
		GByteArray *byte_array;
		guchar *buffer = NULL;
		const gchar *description;
		const gchar *disposition;
		gint column_id;

		column_id = E_ATTACHMENT_STORE_COLUMN_ATTACHMENT;
		gtk_tree_model_get (model, &iter, column_id, &attachment, -1);
		mime_part = e_attachment_ref_mime_part (attachment);
		g_object_unref (attachment);

		valid = gtk_tree_model_iter_next (model, &iter);

		if (mime_part == NULL)
			continue;

		cal_mime_attach = g_malloc0 (sizeof (struct CalMimeAttach));
		wrapper = camel_medium_get_content (CAMEL_MEDIUM (mime_part));

		byte_array = g_byte_array_new ();
		stream = camel_stream_mem_new_with_byte_array (byte_array);

		camel_data_wrapper_decode_to_stream_sync (
			wrapper, stream, NULL, NULL);
		buffer = g_memdup (byte_array->data, byte_array->len);

		camel_mime_part_set_content_id (mime_part, NULL);

		cal_mime_attach->encoded_data = (gchar *) buffer;
		cal_mime_attach->length = byte_array->len;
		cal_mime_attach->filename =
			g_strdup (camel_mime_part_get_filename (mime_part));
		description = camel_mime_part_get_description (mime_part);
		if (description == NULL || *description == '\0')
			description = _("attachment");
		cal_mime_attach->description = g_strdup (description);
		cal_mime_attach->content_type = g_strdup (
			camel_data_wrapper_get_mime_type (wrapper));
		cal_mime_attach->content_id = g_strdup (
			camel_mime_part_get_content_id (mime_part));

		disposition = camel_mime_part_get_disposition (mime_part);
		cal_mime_attach->disposition =
			(disposition != NULL) &&
			(g_ascii_strcasecmp (disposition, "inline") == 0);

		attach_list = g_slist_append (attach_list, cal_mime_attach);

		g_object_unref (mime_part);
		g_object_unref (stream);

	}

	return attach_list;
}

static void
page_dates_changed_cb (CompEditor *editor,
                       CompEditorPageDates *dates,
                       CompEditorPage *page)
{
	CompEditorPrivate *priv = editor->priv;
	GList *l;

	for (l = priv->pages; l != NULL; l = l->next)
		if (page != (CompEditorPage *) l->data && IS_COMP_EDITOR_PAGE (l->data))
			comp_editor_page_set_dates (l->data, dates);

	if (!priv->warned && priv->existing_org && !priv->user_org &&
		!(editor->priv->flags & COMP_EDITOR_NEW_ITEM)) {
		e_notice (
			priv->notebook, GTK_MESSAGE_INFO,
			_("Changes made to this item may be discarded "
			"if an update arrives"));
		priv->warned = TRUE;
	}
}

static void
obj_modified_cb (ECalClientView *view,
                 const GSList *objects,
                 CompEditor *editor)
{
	CompEditorPrivate *priv;
	ECalComponent *comp = NULL;

	priv = editor->priv;

	/* We queried based on a specific UID so we definitely changed */
	if (changed_component_dialog (
		(GtkWindow *) editor, priv->comp, FALSE, priv->changed)) {
		icalcomponent *icalcomp = icalcomponent_new_clone (objects->data);

		comp = e_cal_component_new ();
		if (e_cal_component_set_icalcomponent (comp, icalcomp)) {
			comp_editor_edit_comp (editor, comp);
		} else {
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (
				NULL, 0,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"%s",
				_("Unable to use current version!"));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			icalcomponent_free (icalcomp);
		}

		g_object_unref (comp);
	}
}

static void
obj_removed_cb (ECalClientView *view,
                const GSList *uids,
                CompEditor *editor)
{
	CompEditorPrivate *priv = editor->priv;

	if (changed_component_dialog (
		GTK_WINDOW (editor), priv->comp, TRUE, priv->changed))
		close_dialog (editor);
}
