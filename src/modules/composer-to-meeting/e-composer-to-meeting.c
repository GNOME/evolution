/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>
#include <libedataserver/libedataserver.h>

#include "e-util/e-util.h"
#include "composer/e-msg-composer.h"
#include "composer/e-composer-from-header.h"
#include "calendar/gui/e-comp-editor.h"
#include "calendar/gui/e-comp-editor-page-attachments.h"

#include "e-composer-to-meeting.h"

/* Standard GObject macros */
#define E_TYPE_COMPOSER_TO_MEETING \
	(e_composer_to_meeting_get_type ())
#define E_COMPOSER_TO_MEETING(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMPOSER_TO_MEETING, EComposerToMeeting))
#define E_COMPOSER_TO_MEETING_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMPOSER_TO_MEETING, EComposerToMeetingClass))
#define E_IS_COMPOSER_TO_MEETING(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMPOSER_TO_MEETING))
#define E_IS_COMPOSER_TO_MEETING_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMPOSER_TO_MEETING))
#define E_COMPOSER_TO_MEETING_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMPOSER_TO_MEETING, EComposerToMeetingClass))

typedef struct _EComposerToMeeting EComposerToMeeting;
typedef struct _EComposerToMeetingClass EComposerToMeetingClass;

struct _EComposerToMeeting {
	EExtension parent;
};

struct _EComposerToMeetingClass {
	EExtensionClass parent_class;
};

GType e_composer_to_meeting_get_type (void) G_GNUC_CONST;

G_DEFINE_DYNAMIC_TYPE (EComposerToMeeting, e_composer_to_meeting, E_TYPE_EXTENSION)

static void
composer_to_meeting_attendees_free (gpointer ptr)
{
	ECalComponentAttendee *attendee = ptr;

	if (attendee) {
		g_free ((gpointer) attendee->value);
		g_free ((gpointer) attendee->cn);
		g_free (attendee);
	}
}

static ECalComponent *
composer_to_meeting_component (EMsgComposer *composer)
{
	ECalComponent *comp;
	EHTMLEditor *html_editor;
	EContentEditor *cnt_editor;
	EComposerHeaderTable *header_table;
	EDestination **destinations_array[3];
	ESource *source;
	gchar *alias_name = NULL, *alias_address = NULL, *uid, *text;
	GSList *attendees = NULL;
	const gchar *subject;
	gint ii;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	comp = e_cal_component_new_from_icalcomponent (e_cal_util_new_component (ICAL_VEVENT_COMPONENT));
	g_return_val_if_fail (comp != NULL, NULL);

	header_table = e_msg_composer_get_header_table (composer);

	/* Summary */
	subject = e_composer_header_table_get_subject (header_table);
	if (subject && *subject) {
		ECalComponentText summary;

		summary.value = subject;
		summary.altrep = NULL;

		e_cal_component_set_summary (comp, &summary);
	}

	/* Organizer */
	uid = e_composer_header_table_dup_identity_uid (header_table, &alias_name, &alias_address);
	source = e_composer_header_table_ref_source (header_table, uid);
	if (source) {
		EComposerHeader *composer_header;
		const gchar *name = NULL, *address = NULL;

		composer_header = e_composer_header_table_get_header (header_table, E_COMPOSER_HEADER_FROM);
		if (e_composer_from_header_get_override_visible (E_COMPOSER_FROM_HEADER (composer_header))) {
			name = e_composer_header_table_get_from_name (header_table);
			address = e_composer_header_table_get_from_address (header_table);

			if (address && !*address)
				address = NULL;
		}

		if (!address) {
			if (alias_name)
				name = alias_name;
			if (alias_address)
				address = alias_address;
		}

		if (!address || !name || !*name) {
			ESourceMailIdentity *mail_identity;

			mail_identity = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_IDENTITY);

			if (!name || !*name)
				name = e_source_mail_identity_get_name (mail_identity);

			if (!address)
				address = e_source_mail_identity_get_address (mail_identity);
		}

		if (address && *address) {
			ECalComponentOrganizer organizer = {NULL, NULL, NULL, NULL};
			gchar *mailto;

			mailto = g_strconcat ("mailto:", address, NULL);
			organizer.value = mailto;
			organizer.cn = name;

			e_cal_component_set_organizer (comp, &organizer);

			g_free (mailto);
		}

		g_object_unref (source);
		g_free (alias_address);
		g_free (alias_name);
		g_free (uid);
	}

	/* Attendees */
	destinations_array[0] = e_composer_header_table_get_destinations_to (header_table);
	destinations_array[1] = e_composer_header_table_get_destinations_cc (header_table);
	destinations_array[2] = e_composer_header_table_get_destinations_bcc (header_table);
	for (ii = 0; ii < 3; ii++) {
		EDestination **destinations = destinations_array[ii];
		CamelInternetAddress *address;
		gchar *textrep;

		if (!destinations)
			continue;

		textrep = e_destination_get_textrepv (destinations);
		address = camel_internet_address_new ();

		if (textrep) {
			gint jj, len;

			len = camel_address_decode (CAMEL_ADDRESS (address), textrep);
			for (jj = 0; jj < len; jj++) {
				const gchar *name = NULL, *mail = NULL;

				if (camel_internet_address_get (address, jj, &name, &mail)) {
					ECalComponentAttendee *attendee;

					attendee = g_new0 (ECalComponentAttendee, 1);
					attendee->value = g_strconcat ("mailto:", mail, NULL);
					attendee->cn = g_strdup (name);
					attendee->cutype = ICAL_CUTYPE_INDIVIDUAL;
					attendee->status = ICAL_PARTSTAT_NEEDSACTION;
					attendee->role = ii == 0 ? ICAL_ROLE_REQPARTICIPANT : ICAL_ROLE_OPTPARTICIPANT;

					attendees = g_slist_append (attendees, attendee);
				}
			}
		}

		g_free (textrep);
		g_object_unref (address);
		e_destination_freev (destinations);
	}

	e_cal_component_set_attendee_list (comp, attendees);

	g_slist_free_full (attendees, composer_to_meeting_attendees_free);

	/* Description */
	html_editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (html_editor);
	text = e_content_editor_get_content (cnt_editor, E_CONTENT_EDITOR_GET_PROCESSED | E_CONTENT_EDITOR_GET_TEXT_PLAIN, NULL, NULL);
	if (text && *text) {
		ECalComponentText *description;
		GSList *descr_list = NULL;

		description = g_new0 (ECalComponentText, 1);
		description->value = text;
		description->altrep = NULL;

		descr_list = g_slist_append (descr_list, description);

		e_cal_component_set_description_list (comp, descr_list);

		g_slist_free_full (descr_list, g_free);
	}
	g_free (text);

	return comp;
}

static void
composer_to_meeting_copy_attachments (EMsgComposer *composer,
				      ECompEditor *comp_editor)
{
	ECompEditorPage *page_attachments;
	EAttachmentView *attachment_view;
	EAttachmentStore *store;
	GList *attachments, *link;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	attachment_view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (attachment_view);
	attachments = e_attachment_store_get_attachments (store);

	if (!attachments)
		return;

	page_attachments = e_comp_editor_get_page (comp_editor, E_TYPE_COMP_EDITOR_PAGE_ATTACHMENTS);
	if (page_attachments) {
		store = e_comp_editor_page_attachments_get_store (E_COMP_EDITOR_PAGE_ATTACHMENTS (page_attachments));

		for (link = attachments; link; link = g_list_next (link)) {
			EAttachment *attachment = link->data;

			e_attachment_store_add_attachment (store, attachment);
		}
	}

	g_list_free_full (attachments, g_object_unref);
}

static void
action_composer_to_meeting_cb (GtkAction *action,
			       EMsgComposer *composer)
{
	ECalComponent *comp;
	ECompEditor *comp_editor;
	ECompEditorFlags flags;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if (!e_util_prompt_user (GTK_WINDOW (composer), NULL, NULL, "mail-composer:prompt-composer-to-meeting", NULL))
		return;

	comp = composer_to_meeting_component (composer);
	if (!comp)
		return;

	flags = E_COMP_EDITOR_FLAG_IS_NEW |
		E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER |
		E_COMP_EDITOR_FLAG_WITH_ATTENDEES;

	comp_editor = e_comp_editor_open_for_component (NULL, e_msg_composer_get_shell (composer),
		NULL, e_cal_component_get_icalcomponent (comp), flags);

	/* Attachments */
	composer_to_meeting_copy_attachments (composer, comp_editor);

	gtk_window_present (GTK_WINDOW (comp_editor));

	g_object_unref (comp);

	gtk_widget_destroy (GTK_WIDGET (composer));
}

static void
e_composer_to_meeting_setup_ui (EMsgComposer *composer)
{
	const gchar *ui =
		"<ui>"
		"  <menubar action='main-menu'>"
		"    <placeholder name='pre-edit-menu'>"
		"      <menu action='file-menu'>"
		"        <placeholder name='custom-actions-placeholder'>"
		"          <menuitem action='composer-to-meeting-action'/>"
		"        </placeholder>"
		"      </menu>"
		"    </placeholder>"
		"  </menubar>"
		"</ui>";

	GtkActionEntry entries[] = {
		{ "composer-to-meeting-action",
		  "stock_people",
		  N_("Convert to M_eeting"),
		  NULL,
		  N_("Convert the message to a meeting request"),
		  G_CALLBACK (action_composer_to_meeting_cb) }
	};

	EHTMLEditor *editor;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GError *error = NULL;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = e_msg_composer_get_editor (composer);
	ui_manager = e_html_editor_get_ui_manager (editor);
	action_group = e_html_editor_get_action_group (editor, "composer");

	gtk_action_group_add_actions (action_group, entries, G_N_ELEMENTS (entries), composer);

	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);

	if (error) {
		g_critical ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}
}

static void
composer_to_meeting_constructed (GObject *object)
{
	EMsgComposer *composer;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_composer_to_meeting_parent_class)->constructed (object);

	composer = E_MSG_COMPOSER (e_extension_get_extensible (E_EXTENSION (object)));

	e_composer_to_meeting_setup_ui (composer);
}

static void
e_composer_to_meeting_class_init (EComposerToMeetingClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = composer_to_meeting_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MSG_COMPOSER;
}

static void
e_composer_to_meeting_class_finalize (EComposerToMeetingClass *class)
{
}

static void
e_composer_to_meeting_init (EComposerToMeeting *extension)
{
}

void
e_composer_to_meeting_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_composer_to_meeting_register_type (type_module);
}
