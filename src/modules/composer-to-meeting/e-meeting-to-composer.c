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
#include "calendar/gui/comp-util.h"
#include "calendar/gui/e-comp-editor.h"
#include "calendar/gui/e-comp-editor-page-attachments.h"
#include "calendar/gui/itip-utils.h"

#include "e-meeting-to-composer.h"

/* Standard GObject macros */
#define E_TYPE_MEETING_TO_COMPOSER \
	(e_meeting_to_composer_get_type ())
#define E_MEETING_TO_COMPOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MEETING_TO_COMPOSER, EMeetingToComposer))
#define E_MEETING_TO_COMPOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MEETING_TO_COMPOSER, EMeetingToComposerClass))
#define E_IS_MEETING_TO_COMPOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MEETING_TO_COMPOSER))
#define E_IS_MEETING_TO_COMPOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MEETING_TO_COMPOSER))
#define E_MEETING_TO_COMPOSER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MEETING_TO_COMPOSER, EMeetingToComposerClass))

typedef struct _EMeetingToComposer EMeetingToComposer;
typedef struct _EMeetingToComposerClass EMeetingToComposerClass;

struct _EMeetingToComposer {
	EExtension parent;
};

struct _EMeetingToComposerClass {
	EExtensionClass parent_class;
};

GType e_meeting_to_composer_get_type (void) G_GNUC_CONST;

G_DEFINE_DYNAMIC_TYPE (EMeetingToComposer, e_meeting_to_composer, E_TYPE_EXTENSION)

static void
meeting_to_composer_unref_nonull_object (gpointer ptr)
{
	if (ptr)
		g_object_unref (ptr);
}

static gboolean
meeting_to_composer_check_identity_source (ESource *source,
					   const gchar *address,
					   gchar **alias_name,
					   gchar **alias_address)
{
	ESourceMailIdentity *identity_extension;
	GHashTable *aliases = NULL;
	const gchar *text;
	gboolean found = FALSE;

	if (!E_IS_SOURCE (source) || !address ||
	    !e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_IDENTITY))
		return FALSE;

	identity_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_IDENTITY);

	text = e_source_mail_identity_get_address (identity_extension);
	found = text && g_ascii_strcasecmp (text, address) == 0;

	if (!found) {
		aliases = e_source_mail_identity_get_aliases_as_hash_table (identity_extension);
		if (aliases) {
			found = g_hash_table_contains (aliases, address);
			if (found) {
				if (alias_name)
					*alias_name = g_strdup (g_hash_table_lookup (aliases, address));
				if (alias_address)
					*alias_address = g_strdup (address);
			}
		}
	}

	if (aliases)
		g_hash_table_destroy (aliases);

	return found;
}

static void
meeting_to_composer_copy_attachments (ECompEditor *comp_editor,
				      EMsgComposer *composer)
{
	ECompEditorPage *page_attachments;
	EAttachmentView *attachment_view;
	EAttachmentStore *store;
	GList *attachments, *link;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	page_attachments = e_comp_editor_get_page (comp_editor, E_TYPE_COMP_EDITOR_PAGE_ATTACHMENTS);
	if (!page_attachments)
		return;

	store = e_comp_editor_page_attachments_get_store (E_COMP_EDITOR_PAGE_ATTACHMENTS (page_attachments));
	attachments = e_attachment_store_get_attachments (store);

	if (!attachments)
		return;

	attachment_view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (attachment_view);

	for (link = attachments; link; link = g_list_next (link)) {
		EAttachment *attachment = link->data;

		e_attachment_store_add_attachment (store, attachment);
	}

	g_list_free_full (attachments, g_object_unref);
}

static void
meeting_to_composer_composer_created_cb (GObject *source_object,
					 GAsyncResult *result,
					 gpointer user_data)
{
	ECompEditor *comp_editor = user_data;
	EMsgComposer *composer;
	EComposerHeaderTable *header_table;
	gboolean did_updating;
	ICalComponent *icomp;
	ICalProperty *prop;
	const gchar *text;
	GPtrArray *to_recips, *cc_recips;
	GError *error = NULL;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	composer = e_msg_composer_new_finish (result, &error);
	if (!composer) {
		g_warning ("%s: Faild to create message composer: %s", G_STRFUNC, error ? error->message : "Unknown error");
		return;
	}

	header_table = e_msg_composer_get_header_table (composer);

	did_updating = e_comp_editor_get_updating (comp_editor);
	/* Just a trick to not show validation errors when getting the component */
	e_comp_editor_set_updating (comp_editor, TRUE);

	icomp = i_cal_component_clone (e_comp_editor_get_component (comp_editor));
	e_comp_editor_fill_component (comp_editor, icomp);

	e_comp_editor_set_updating (comp_editor, did_updating);

	/* Subject */
	prop = e_cal_util_component_find_property_for_locale (icomp, I_CAL_SUMMARY_PROPERTY, NULL);
	text = prop ? i_cal_property_get_summary (prop) : NULL;
	if (text && *text)
		e_composer_header_table_set_subject (header_table, text);
	g_clear_object (&prop);

	/* From */
	prop = i_cal_component_get_first_property (icomp, I_CAL_ORGANIZER_PROPERTY);
	if (prop) {
		EComposerHeader *from_header;
		const gchar *organizer;

		from_header = e_composer_header_table_get_header (header_table, E_COMPOSER_HEADER_FROM);
		organizer = e_cal_util_get_property_email (prop);

		if (organizer && *organizer && from_header) {
			GtkComboBox *identities_combo;
			GtkTreeModel *model;
			GtkTreeIter iter;
			gint id_column;

			identities_combo = GTK_COMBO_BOX (from_header->input_widget);
			id_column = gtk_combo_box_get_id_column (identities_combo);
			model = gtk_combo_box_get_model (identities_combo);

			if (gtk_tree_model_get_iter_first (model, &iter)) {
				do {
					ESource *source;
					gchar *uid;
					gboolean use_source;
					gchar *alias_name = NULL;
					gchar *alias_address = NULL;

					gtk_tree_model_get (model, &iter, id_column, &uid, -1);
					source = e_composer_header_table_ref_source (header_table, uid);

					use_source = meeting_to_composer_check_identity_source (source, organizer, &alias_name, &alias_address);
					if (use_source)
						e_composer_header_table_set_identity_uid (header_table, uid, alias_name, alias_address);

					g_clear_object (&source);
					g_free (alias_name);
					g_free (alias_address);
					g_free (uid);

					if (use_source)
						break;
				} while (gtk_tree_model_iter_next (model, &iter));
			}
		}

		g_clear_object (&prop);
	}

	/* Recipients */
	to_recips = g_ptr_array_new_with_free_func (meeting_to_composer_unref_nonull_object);
	cc_recips = g_ptr_array_new_with_free_func (meeting_to_composer_unref_nonull_object);

	for (prop = i_cal_component_get_first_property (icomp, I_CAL_ATTENDEE_PROPERTY);
	     prop;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_ATTENDEE_PROPERTY)) {
		ICalParameter *param;
		ICalParameterRole role = I_CAL_ROLE_REQPARTICIPANT;
		const gchar *name = NULL, *address;
		EDestination *dest;

		address = e_cal_util_get_property_email (prop);
		if (!address || !*address)
			continue;

		param = i_cal_property_get_first_parameter (prop, I_CAL_ROLE_PARAMETER);
		if (param) {
			role = i_cal_parameter_get_role (param);
			g_object_unref (param);
		}

		if (role == I_CAL_ROLE_NONPARTICIPANT || role == I_CAL_ROLE_NONE)
			continue;

		param = i_cal_property_get_first_parameter (prop, I_CAL_CN_PARAMETER);
		if (param)
			name = i_cal_parameter_get_cn (param);

		if (name && !*name)
			name = NULL;

		dest = e_destination_new ();
		e_destination_set_name (dest, name);
		e_destination_set_email (dest, address);

		if (role == I_CAL_ROLE_REQPARTICIPANT)
			g_ptr_array_add (to_recips, dest);
		else
			g_ptr_array_add (cc_recips, dest);

		g_clear_object (&param);
	}

	if (to_recips->len > 0) {
		g_ptr_array_add (to_recips, NULL);

		e_composer_header_table_set_destinations_to (header_table, (EDestination **) to_recips->pdata);
	}

	if (cc_recips->len > 0) {
		g_ptr_array_add (cc_recips, NULL);

		e_composer_header_table_set_destinations_cc (header_table, (EDestination **) cc_recips->pdata);
	}

	g_ptr_array_free (to_recips, TRUE);
	g_ptr_array_free (cc_recips, TRUE);

	/* Body */
	prop = e_cal_util_component_find_property_for_locale (icomp, I_CAL_DESCRIPTION_PROPERTY, NULL);
	if (prop) {
		text = i_cal_property_get_description (prop);

		if (text && *text) {
			EHTMLEditor *html_editor;
			EContentEditor *cnt_editor;
			EContentEditorMode mode;
			GSettings *settings;

			settings = e_util_ref_settings ("org.gnome.evolution.mail");
			mode = g_settings_get_enum (settings, "composer-mode");
			g_clear_object (&settings);

			/* Let the markdown be allowed, otherwise use the plain text mode */
			if (mode != E_CONTENT_EDITOR_MODE_MARKDOWN &&
			    mode != E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT)
				mode = E_CONTENT_EDITOR_MODE_PLAIN_TEXT;

			html_editor = e_msg_composer_get_editor (composer);
			cnt_editor = e_html_editor_get_content_editor (html_editor);

			e_html_editor_set_mode (html_editor, mode);
			/* no need to transfer the current content from old editor to the new, the content is set below */
			e_html_editor_cancel_mode_change_content_update (html_editor);
			e_content_editor_insert_content (cnt_editor, text, E_CONTENT_EDITOR_INSERT_REPLACE_ALL | E_CONTENT_EDITOR_INSERT_TEXT_PLAIN);
		}

		g_object_unref (prop);
	}

	/* Attachments */
	meeting_to_composer_copy_attachments (comp_editor, composer);

	gtk_window_present (GTK_WINDOW (composer));

	gtk_widget_destroy (GTK_WIDGET (comp_editor));
	g_object_unref (icomp);
}

static void
action_meeting_to_composer_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	ECompEditor *comp_editor = user_data;
	ICalComponent *icomp;
	ICalComponentKind kind;
	const gchar *prompt_key;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	icomp = e_comp_editor_get_component (comp_editor);
	kind = icomp ? i_cal_component_isa (icomp) : I_CAL_VEVENT_COMPONENT;

	if (kind == I_CAL_VTODO_COMPONENT)
		prompt_key = "mail-composer:prompt-task-to-composer";
	else if (kind == I_CAL_VJOURNAL_COMPONENT)
		prompt_key = "mail-composer:prompt-memo-to-composer";
	else
		prompt_key = "mail-composer:prompt-event-to-composer";

	if (!e_util_prompt_user (GTK_WINDOW (comp_editor), NULL, NULL, prompt_key, NULL))
		return;

	e_msg_composer_new (e_comp_editor_get_shell (comp_editor),
		meeting_to_composer_composer_created_cb, comp_editor);
}

static void
e_meeting_to_composer_setup_ui (ECompEditor *comp_editor)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<submenu action='file-menu'>"
		      "<placeholder id='custom-actions-placeholder'>"
			"<item action='meeting-to-composer-action'/>"
		      "</placeholder>"
		    "</submenu>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry entries[] = {
		{ "meeting-to-composer-action",
		  "mail-message-new",
		  N_("Convert to M_essage"),
		  NULL,
		  N_("Convert to the mail message"),
		  action_meeting_to_composer_cb, NULL, NULL, NULL }
	};

	EUIManager *ui_manager;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	ui_manager = e_comp_editor_get_ui_manager (comp_editor);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "individual", GETTEXT_PACKAGE,
		entries, G_N_ELEMENTS (entries), comp_editor, eui);
}

static void
meeting_to_composer_constructed (GObject *object)
{
	ECompEditor *comp_editor;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_meeting_to_composer_parent_class)->constructed (object);

	comp_editor = E_COMP_EDITOR (e_extension_get_extensible (E_EXTENSION (object)));

	e_meeting_to_composer_setup_ui (comp_editor);
}

static void
e_meeting_to_composer_class_init (EMeetingToComposerClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = meeting_to_composer_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_COMP_EDITOR;
}

static void
e_meeting_to_composer_class_finalize (EMeetingToComposerClass *class)
{
}

static void
e_meeting_to_composer_init (EMeetingToComposer *extension)
{
}

void
e_meeting_to_composer_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_meeting_to_composer_register_type (type_module);
}
