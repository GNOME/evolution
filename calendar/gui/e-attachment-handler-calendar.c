/*
 * e-attachment-handler-calendar.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-attachment-handler-calendar.h"

#include <glib/gi18n.h>
#include <libical/ical.h>
#include <libecal/e-cal.h>
#include <camel/camel-stream-mem.h>
#include <libedataserverui/e-source-selector.h>

#include "calendar/common/authentication.h"

#define E_ATTACHMENT_HANDLER_CALENDAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ATTACHMENT_HANDLER_CALENDAR, EAttachmentHandlerCalendarPrivate))

typedef struct _ImportContext ImportContext;

struct _EAttachmentHandlerCalendarPrivate {
	gint placeholder;
};

struct _ImportContext {
	ECal *client;
	icalcomponent *component;
	ECalSourceType source_type;
};

static gpointer parent_class;

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

	mime_part = e_attachment_get_mime_part (attachment);
	if (!CAMEL_IS_MIME_PART (mime_part))
		return NULL;

	buffer = g_byte_array_new ();
	stream = camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (stream), buffer);
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	camel_data_wrapper_decode_to_stream (wrapper, stream);
	camel_object_unref (stream);

	component = e_cal_util_parse_ics_string ((gchar *) buffer->data);

	g_byte_array_free (buffer, TRUE);

	if (component == NULL)
		return NULL;

	g_object_set_data_full (
		G_OBJECT (attachment), key, component,
		(GDestroyNotify) icalcomponent_free);

	return component;
}

static gboolean
attachment_handler_update_objects (ECal *client,
                                   icalcomponent *component)
{
	icalcomponent_kind kind;
	icalcomponent *vcalendar;
	gboolean success;

	kind = icalcomponent_isa (component);

	switch (kind) {
		case ICAL_VTODO_COMPONENT:
		case ICAL_VEVENT_COMPONENT:
			vcalendar = e_cal_util_new_top_level ();
			if (icalcomponent_get_method (component) == ICAL_METHOD_CANCEL)
				icalcomponent_set_method (vcalendar, ICAL_METHOD_CANCEL);
			else
				icalcomponent_set_method (vcalendar, ICAL_METHOD_PUBLISH);
			icalcomponent_add_component (
				vcalendar, icalcomponent_new_clone (component));
			break;

		case ICAL_VCALENDAR_COMPONENT:
			vcalendar = icalcomponent_new_clone (component);
			if (!icalcomponent_get_first_property (vcalendar, ICAL_METHOD_PROPERTY))
				icalcomponent_set_method (vcalendar, ICAL_METHOD_PUBLISH);
			break;

		default:
			return FALSE;
	}

	success = e_cal_receive_objects (client, vcalendar, NULL);

	icalcomponent_free (vcalendar);

	return success;
}

static void
attachment_handler_import_event (ECal *client,
                                 ECalendarStatus status,
                                 EAttachment *attachment)
{
	icalcomponent *component;
	icalcomponent *subcomponent;
	icalcompiter iter;

	/* FIXME Notify the user somehow. */
	g_return_if_fail (status == E_CALENDAR_STATUS_OK);

	component = attachment_handler_get_component (attachment);
	g_return_if_fail (component != NULL);

	iter = icalcomponent_begin_component (component, ICAL_ANY_COMPONENT);

	while ((subcomponent = icalcompiter_deref (&iter)) != NULL) {
		icalcomponent_kind kind;

		kind = icalcomponent_isa (subcomponent);
		icalcompiter_next (&iter);

		if (kind == ICAL_VEVENT_COMPONENT)
			continue;

		if (kind == ICAL_VTIMEZONE_COMPONENT)
			continue;

		icalcomponent_remove_component (component, subcomponent);
		icalcomponent_free (subcomponent);
	}

	/* XXX Do something with the return value. */
	attachment_handler_update_objects (client, component);

	g_object_unref (attachment);
	g_object_unref (client);
}

static void
attachment_handler_import_todo (ECal *client,
                                ECalendarStatus status,
                                EAttachment *attachment)
{
	icalcomponent *component;
	icalcomponent *subcomponent;
	icalcompiter iter;

	/* FIXME Notify the user somehow. */
	g_return_if_fail (status == E_CALENDAR_STATUS_OK);

	component = attachment_handler_get_component (attachment);
	g_return_if_fail (component != NULL);

	iter = icalcomponent_begin_component (component, ICAL_ANY_COMPONENT);

	while ((subcomponent = icalcompiter_deref (&iter)) != NULL) {
		icalcomponent_kind kind;

		kind = icalcomponent_isa (subcomponent);
		icalcompiter_next (&iter);

		if (kind == ICAL_VTODO_COMPONENT)
			continue;

		if (kind == ICAL_VTIMEZONE_COMPONENT)
			continue;

		icalcomponent_remove_component (component, subcomponent);
		icalcomponent_free (subcomponent);
	}

	/* XXX Do something with the return value. */
	attachment_handler_update_objects (client, component);

	g_object_unref (attachment);
	g_object_unref (client);
}

static void
attachment_handler_row_activated_cb (GtkDialog *dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static void
attachment_handler_run_dialog (GtkWindow *parent,
                               EAttachment *attachment,
                               ECalSourceType source_type,
                               const gchar *title)
{
	GtkWidget *dialog;
	GtkWidget *container;
	GtkWidget *widget;
	GCallback callback;
	ESourceSelector *selector;
	ESourceList *source_list;
	ESource *source;
	ECal *client;
	icalcomponent *component;
	GError *error = NULL;

	component = attachment_handler_get_component (attachment);
	g_return_if_fail (component != NULL);

	e_cal_get_sources (&source_list, source_type, &error);
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
		return;
	}

	source = e_source_list_peek_source_any (source_list);
	g_return_if_fail (source != NULL);

	dialog = gtk_dialog_new_with_buttons (
		title, parent, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

	widget = gtk_button_new_with_mnemonic (_("I_mport"));
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_icon_name (
		"stock_mail-import", GTK_ICON_SIZE_MENU));
	gtk_dialog_add_action_widget (
		GTK_DIALOG (dialog), widget, GTK_RESPONSE_OK);
	gtk_widget_show (widget);

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
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

	widget = e_source_selector_new (source_list);
	selector = E_SOURCE_SELECTOR (widget);
	e_source_selector_set_primary_selection (selector, source);
	e_source_selector_show_selection (selector, FALSE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "row-activated",
		G_CALLBACK (attachment_handler_row_activated_cb), dialog);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
		goto exit;

	source = e_source_selector_peek_primary_selection (selector);
	if (source == NULL)
		goto exit;

	client = auth_new_cal_from_source (source, source_type);
	if (client == NULL)
		goto exit;

	if (source_type == E_CAL_SOURCE_TYPE_EVENT)
		callback = G_CALLBACK (attachment_handler_import_event);
	else if (source_type == E_CAL_SOURCE_TYPE_TODO)
		callback = G_CALLBACK (attachment_handler_import_todo);
	else
		goto exit;

	g_object_ref (attachment);
	g_signal_connect (client, "cal-opened", callback, attachment);
	e_cal_open_async (client, FALSE);

exit:
	gtk_widget_destroy (dialog);
}

static void
attachment_handler_import_to_calendar (GtkAction *action,
                                       EAttachmentHandler *handler)
{
	EAttachment *attachment;
	EAttachmentView *view;
	GList *selected;
	gpointer parent;

	view = e_attachment_handler_get_view (handler);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	selected = e_attachment_view_get_selected_attachments (view);
	g_return_if_fail (g_list_length (selected) == 1);
	attachment = E_ATTACHMENT (selected->data);

	attachment_handler_run_dialog (
		parent, attachment,
		E_CAL_SOURCE_TYPE_EVENT,
		_("Select a Calendar"));

	g_object_unref (attachment);
	g_list_free (selected);
}

static void
attachment_handler_import_to_tasks (GtkAction *action,
                                    EAttachmentHandler *handler)
{
	EAttachment *attachment;
	EAttachmentView *view;
	GList *selected;
	gpointer parent;

	view = e_attachment_handler_get_view (handler);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (view));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	selected = e_attachment_view_get_selected_attachments (view);
	g_return_if_fail (g_list_length (selected) == 1);
	attachment = E_ATTACHMENT (selected->data);

	attachment_handler_run_dialog (
		parent, attachment,
		E_CAL_SOURCE_TYPE_TODO,
		_("Select a Task List"));

	g_object_unref (attachment);
	g_list_free (selected);
}

static GtkActionEntry standard_entries[] = {

	{ "import-to-calendar",
	  "stock_mail-import",
	  N_("I_mport to Calendar"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (attachment_handler_import_to_calendar) },

	{ "import-to-tasks",
	  "stock_mail-import",
	  N_("I_mport to Tasks"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (attachment_handler_import_to_tasks) }
};

static void
attachment_handler_calendar_update_actions (EAttachmentView *view)
{
	EAttachment *attachment;
	GtkAction *action;
	GList *selected;
	icalcomponent *component;
	icalcomponent *subcomponent;
	icalcomponent_kind kind;
	gboolean is_vevent = FALSE;
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
	is_vtodo = (kind == ICAL_VTODO_COMPONENT);

exit:
	action = e_attachment_view_get_action (view, "import-to-calendar");
	gtk_action_set_visible (action, is_vevent);

	action = e_attachment_view_get_action (view, "import-to-tasks");
	gtk_action_set_visible (action, is_vtodo);

	g_list_foreach (selected, (GFunc) g_object_unref, NULL);
	g_list_free (selected);
}

static void
attachment_handler_calendar_constructed (GObject *object)
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
		G_CALLBACK (attachment_handler_calendar_update_actions),
		NULL);
}

static void
attachment_handler_calendar_class_init (EAttachmentHandlerCalendarClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EAttachmentHandlerCalendarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = attachment_handler_calendar_constructed;
}

static void
attachment_handler_calendar_init (EAttachmentHandlerCalendar *handler)
{
	handler->priv = E_ATTACHMENT_HANDLER_CALENDAR_GET_PRIVATE (handler);
}

GType
e_attachment_handler_calendar_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EAttachmentHandlerCalendarClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) attachment_handler_calendar_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EAttachmentHandlerCalendar),
			0,     /* n_preallocs */
			(GInstanceInitFunc) attachment_handler_calendar_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_ATTACHMENT_HANDLER,
			"EAttachmentHandlerCalendar", &type_info, 0);
	}

	return type;
}
