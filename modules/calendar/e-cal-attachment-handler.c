/*
 * e-cal-attachment-handler.c
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

#include "e-cal-attachment-handler.h"

#include <glib/gi18n.h>
#include <libical/ical.h>
#include <camel/camel.h>
#include <libecal/libecal.h>

#include <shell/e-shell.h>
#include <shell/e-shell-view.h>
#include <shell/e-shell-window.h>

#define E_CAL_ATTACHMENT_HANDLER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_ATTACHMENT_HANDLER, ECalAttachmentHandlerPrivate))

typedef struct _ImportContext ImportContext;

struct _ECalAttachmentHandlerPrivate {
	gint placeholder;
};

struct _ImportContext {
	ECalClient *client;
	icalcomponent *component;
	ECalClientSourceType source_type;
};

static gpointer parent_class;
static GType cal_attachment_handler_type;

static const gchar *ui =
"<ui>"
"  <popup name='context'>"
"    <placeholder name='custom-actions'>"
"      <menuitem action='import-to-calendar'/>"
"      <menuitem action='import-to-tasks'/>"
"    </placeholder>"
"  </popup>"
"</ui>";

static icalcomponent *
attachment_handler_get_component (EAttachment *attachment)
{
	CamelDataWrapper *wrapper;
	CamelMimePart *mime_part;
	CamelStream *stream;
	GByteArray *buffer;
	icalcomponent *component;
	const gchar *key = "__icalcomponent__";

	component = g_object_get_data (G_OBJECT (attachment), key);
	if (component != NULL)
		return component;

	if (e_attachment_get_loading (attachment) ||
	    e_attachment_get_saving (attachment))
		return NULL;

	mime_part = e_attachment_ref_mime_part (attachment);
	if (mime_part == NULL)
		return NULL;

	buffer = g_byte_array_new ();
	stream = camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (stream), buffer);
	wrapper = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
	camel_data_wrapper_decode_to_stream_sync (wrapper, stream, NULL, NULL);
	g_object_unref (stream);

	g_object_unref (mime_part);

	if (buffer->len > 0) {
		const gchar *str;

		/* ensure string being null-terminated  */
		g_byte_array_append (buffer, (const guint8 *) "", 1);

		str = (const gchar *) buffer->data;
		while (*str && g_ascii_isspace (*str))
			str++;

		if (g_ascii_strncasecmp (str, "BEGIN:", 6) == 0)
			component = e_cal_util_parse_ics_string (str);
	}

	g_byte_array_free (buffer, TRUE);

	if (component == NULL)
		return NULL;

	g_object_set_data_full (
		G_OBJECT (attachment), key, component,
		(GDestroyNotify) icalcomponent_free);

	return component;
}

typedef struct {
	EShell *shell;
	ESource *source;
	icalcomponent *icalcomp;
	const gchar *extension_name;
} ImportComponentData;

static void
import_component_data_free (gpointer ptr)
{
	ImportComponentData *icd = ptr;

	if (icd) {
		g_clear_object (&icd->shell);
		g_clear_object (&icd->source);
		if (icd->icalcomp)
			icalcomponent_free (icd->icalcomp);
		g_free (icd);
	}
}

static void
import_component_thread (EAlertSinkThreadJobData *job_data,
			 gpointer user_data,
			 GCancellable *cancellable,
			 GError **error)
{
	ImportComponentData *icd = user_data;
	icalcomponent_kind need_kind = ICAL_ANY_COMPONENT;
	icalcomponent *subcomp, *vcalendar;
	icalcompiter iter;
	EClient *e_client;
	ECalClient *client = NULL;

	g_return_if_fail (icd != NULL);

	e_client = e_util_open_client_sync (job_data, e_shell_get_client_cache (icd->shell), icd->extension_name, icd->source, 30, cancellable, error);
	if (e_client)
		client = E_CAL_CLIENT (e_client);

	if (!client)
		return;

	if (g_str_equal (icd->extension_name, E_SOURCE_EXTENSION_CALENDAR))
		need_kind = ICAL_VEVENT_COMPONENT;
	else if (g_str_equal (icd->extension_name, E_SOURCE_EXTENSION_MEMO_LIST))
		need_kind = ICAL_VJOURNAL_COMPONENT;
	else if (g_str_equal (icd->extension_name, E_SOURCE_EXTENSION_TASK_LIST))
		need_kind = ICAL_VTODO_COMPONENT;

	if (need_kind == ICAL_ANY_COMPONENT) {
		g_warn_if_reached ();
		goto out;
	}

	iter = icalcomponent_begin_component (icd->icalcomp, ICAL_ANY_COMPONENT);

	while ((subcomp = icalcompiter_deref (&iter)) != NULL) {
		icalcomponent_kind kind;

		kind = icalcomponent_isa (subcomp);
		icalcompiter_next (&iter);

		if (kind == need_kind)
			continue;

		if (kind == ICAL_VTIMEZONE_COMPONENT)
			continue;

		icalcomponent_remove_component (icd->icalcomp, subcomp);
		icalcomponent_free (subcomp);
	}

	switch (icalcomponent_isa (icd->icalcomp)) {
		case ICAL_VEVENT_COMPONENT:
		case ICAL_VJOURNAL_COMPONENT:
		case ICAL_VTODO_COMPONENT:
			vcalendar = e_cal_util_new_top_level ();
			if (icalcomponent_get_method (icd->icalcomp) == ICAL_METHOD_CANCEL)
				icalcomponent_set_method (vcalendar, ICAL_METHOD_CANCEL);
			else
				icalcomponent_set_method (vcalendar, ICAL_METHOD_PUBLISH);
			icalcomponent_add_component (vcalendar, icalcomponent_new_clone (icd->icalcomp));
			break;

		case ICAL_VCALENDAR_COMPONENT:
			vcalendar = icalcomponent_new_clone (icd->icalcomp);
			if (!icalcomponent_get_first_property (vcalendar, ICAL_METHOD_PROPERTY))
				icalcomponent_set_method (vcalendar, ICAL_METHOD_PUBLISH);
			break;

		default:
			goto out;
	}

	e_cal_client_receive_objects_sync (client, vcalendar, cancellable, error);

	icalcomponent_free (vcalendar);
 out:
	g_clear_object (&client);
}

static void
attachment_handler_row_activated_cb (GtkDialog *dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static void
attachment_handler_run_dialog (GtkWindow *parent,
                               EAttachment *attachment,
                               ECalClientSourceType source_type,
                               const gchar *title)
{
	EShell *shell;
	EShellWindow *shell_window = NULL;
	GtkWidget *dialog;
	GtkWidget *container;
	GtkWidget *widget;
	ESourceRegistry *registry;
	ESourceSelector *selector;
	ESource *source;
	const gchar *extension_name;
	icalcomponent *component;

	switch (source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			break;
		default:
			g_return_if_reached ();
	}

	if (E_IS_SHELL_WINDOW (parent)) {
		shell_window = E_SHELL_WINDOW (parent);
		shell = e_shell_window_get_shell (shell_window);
	} else {
		GList *windows, *wlink;

		shell = e_shell_get_default ();

		windows = gtk_application_get_windows (GTK_APPLICATION (shell));
		for (wlink = windows; wlink; wlink = g_list_next (wlink)) {
			if (E_IS_SHELL_WINDOW (wlink->data)) {
				shell_window = E_SHELL_WINDOW (wlink->data);
				break;
			}
		}
	}

	g_return_if_fail (shell_window != NULL);

	component = attachment_handler_get_component (attachment);
	g_return_if_fail (component != NULL);

	dialog = gtk_dialog_new_with_buttons (
		title, parent, GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Cancel"), GTK_RESPONSE_CANCEL, NULL);

	widget = gtk_button_new_with_mnemonic (_("I_mport"));
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_icon_name (
		"stock_mail-import", GTK_ICON_SIZE_MENU));
	gtk_dialog_add_action_widget (
		GTK_DIALOG (dialog), widget, GTK_RESPONSE_OK);
	gtk_widget_show (widget);

	gtk_window_set_default_size (GTK_WINDOW (dialog), 300, 400);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	registry = e_shell_get_registry (shell);
	widget = e_source_selector_new (registry, extension_name);
	selector = E_SOURCE_SELECTOR (widget);
	e_source_selector_set_show_toggles (selector, FALSE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "row-activated",
		G_CALLBACK (attachment_handler_row_activated_cb), dialog);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
		goto exit;

	source = e_source_selector_ref_primary_selection (selector);
	if (source != NULL) {
		EShellView *shell_view;
		EActivity *activity;
		icalcomponent *icalcomp;
		ImportComponentData *icd;
		const gchar *description;
		const gchar *alert_ident;

		icalcomp = attachment_handler_get_component (attachment);

		switch (source_type) {
			case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
				description = _("Importing an event");
				alert_ident = "calendar:failed-create-event";
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
				description = _("Importing a memo");
				alert_ident = "calendar:failed-create-memo";
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
				description = _("Importing a task");
				alert_ident = "calendar:failed-create-task";
				break;
			default:
				g_warn_if_reached ();
				return;
		}

		shell_view = e_shell_window_get_shell_view (shell_window,
			e_shell_window_get_active_view (shell_window));

		icd = g_new0 (ImportComponentData, 1);
		icd->shell = g_object_ref (shell);
		icd->source = g_object_ref (source);
		icd->icalcomp = icalcomponent_new_clone (icalcomp);
		icd->extension_name = extension_name;

		activity = e_shell_view_submit_thread_job (shell_view, description, alert_ident,
			e_source_get_display_name (source), import_component_thread, icd,
			import_component_data_free);

		g_clear_object (&activity);
		g_object_unref (source);
	}

 exit:
	gtk_widget_destroy (dialog);
}

static void
attachment_handler_import_ical (EAttachmentHandler *handler,
				ECalClientSourceType source_type,
				const gchar *title)
{
	EAttachment *attachment;
	EAttachmentView *view;
	GList *selected;
	gpointer parent;

	view = e_attachment_handler_get_view (handler);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	selected = e_attachment_view_get_selected_attachments (view);
	g_return_if_fail (g_list_length (selected) == 1);
	attachment = E_ATTACHMENT (selected->data);

	attachment_handler_run_dialog (parent, attachment, source_type, title);

	g_object_unref (attachment);
	g_list_free (selected);
}

static void
attachment_handler_import_to_calendar (GtkAction *action,
                                       EAttachmentHandler *handler)
{
	attachment_handler_import_ical (handler, E_CAL_CLIENT_SOURCE_TYPE_EVENTS, _("Select a Calendar"));
}

static void
attachment_handler_import_to_memos (GtkAction *action,
                                    EAttachmentHandler *handler)
{
	attachment_handler_import_ical (handler, E_CAL_CLIENT_SOURCE_TYPE_MEMOS, _("Select a Memo List"));
}

static void
attachment_handler_import_to_tasks (GtkAction *action,
                                    EAttachmentHandler *handler)
{
	attachment_handler_import_ical (handler, E_CAL_CLIENT_SOURCE_TYPE_TASKS, _("Select a Task List"));
}

static GtkActionEntry standard_entries[] = {

	{ "import-to-calendar",
	  "stock_mail-import",
	  N_("I_mport to Calendar"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (attachment_handler_import_to_calendar) },

	{ "import-to-memos",
	  "stock_mail-import",
	  N_("I_mport to Memo List"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (attachment_handler_import_to_memos) },

	{ "import-to-tasks",
	  "stock_mail-import",
	  N_("I_mport to Task List"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (attachment_handler_import_to_tasks) }
};

static void
cal_attachment_handler_update_actions (EAttachmentView *view)
{
	EAttachment *attachment;
	GtkAction *action;
	GList *selected;
	icalcomponent *component;
	icalcomponent *subcomponent;
	icalcomponent_kind kind;
	gboolean is_vevent = FALSE;
	gboolean is_vjournal = FALSE;
	gboolean is_vtodo = FALSE;

	selected = e_attachment_view_get_selected_attachments (view);

	if (g_list_length (selected) != 1)
		goto exit;

	attachment = E_ATTACHMENT (selected->data);
	component = attachment_handler_get_component (attachment);

	if (component == NULL)
		goto exit;

	subcomponent = icalcomponent_get_inner (component);

	if (subcomponent == NULL)
		goto exit;

	kind = icalcomponent_isa (subcomponent);
	is_vevent = (kind == ICAL_VEVENT_COMPONENT);
	is_vjournal = (kind == ICAL_VJOURNAL_COMPONENT);
	is_vtodo = (kind == ICAL_VTODO_COMPONENT);

exit:
	action = e_attachment_view_get_action (view, "import-to-calendar");
	gtk_action_set_visible (action, is_vevent);

	action = e_attachment_view_get_action (view, "import-to-memos");
	gtk_action_set_visible (action, is_vjournal);

	action = e_attachment_view_get_action (view, "import-to-tasks");
	gtk_action_set_visible (action, is_vtodo);

	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);
}

static void
cal_attachment_handler_constructed (GObject *object)
{
	EAttachmentHandler *handler;
	EAttachmentView *view;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GError *error = NULL;

	handler = E_ATTACHMENT_HANDLER (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	view = e_attachment_handler_get_view (handler);

	action_group = e_attachment_view_add_action_group (view, "calendar");
	gtk_action_group_add_actions (
		action_group, standard_entries,
		G_N_ELEMENTS (standard_entries), handler);

	ui_manager = e_attachment_view_get_ui_manager (view);
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_signal_connect (
		view, "update_actions",
		G_CALLBACK (cal_attachment_handler_update_actions),
		NULL);
}

static void
cal_attachment_handler_class_init (ECalAttachmentHandlerClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ECalAttachmentHandlerPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = cal_attachment_handler_constructed;
}

static void
cal_attachment_handler_init (ECalAttachmentHandler *handler)
{
	handler->priv = E_CAL_ATTACHMENT_HANDLER_GET_PRIVATE (handler);
}

GType
e_cal_attachment_handler_get_type (void)
{
	return cal_attachment_handler_type;
}

void
e_cal_attachment_handler_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (ECalAttachmentHandlerClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) cal_attachment_handler_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (ECalAttachmentHandler),
		0,     /* n_preallocs */
		(GInstanceInitFunc) cal_attachment_handler_init,
		NULL   /* value_table */
	};

	cal_attachment_handler_type = g_type_module_register_type (
		type_module, E_TYPE_ATTACHMENT_HANDLER,
		"ECalAttachmentHandler", &type_info, 0);
}
