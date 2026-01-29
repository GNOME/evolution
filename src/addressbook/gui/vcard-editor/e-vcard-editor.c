/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <libebook/libebook.h>
#include <libedataserver/libedataserver.h>

#define LIBICAL_GLIB_UNSTABLE_API 1
#include <libical-glib/libical-glib.h>
#undef LIBICAL_GLIB_UNSTABLE_API

#include "addressbook/gui/widgets/eab-contact-merging.h"
#include "addressbook/util/eab-book-util.h"
#include "e-util/e-util.h"

#include "e-section-box.h"
#include "e-vcard-editor-item.h"
#include "e-vcard-editor-address.h"
#include "e-vcard-editor-certificate.h"
#include "e-vcard-editor-notes.h"
#include "e-vcard-editor-section.h"
#include "e-vcard-editor.h"

#define MAX_PHOTO_SIZE_PX 1024

#define FAKE_CONTACT_NAME_PREFIX	(E_CONTACT_FIELD_LAST + 1)
#define FAKE_CONTACT_NAME_FIRST		(E_CONTACT_FIELD_LAST + 2)
#define FAKE_CONTACT_NAME_MIDDLE	(E_CONTACT_FIELD_LAST + 3)
#define FAKE_CONTACT_NAME_LAST		(E_CONTACT_FIELD_LAST + 4)
#define FAKE_CONTACT_NAME_SUFFIX	(E_CONTACT_FIELD_LAST + 5)

struct _EVCardEditor {
	GtkDialog parent_object;

	EVCardEditorFlags flags;
	EVCardEditorContactKind contact_kind;
	gboolean changed;
	gboolean updating;
	gboolean add_menu_needs_update;
	EContact *contact;
	ESourceRegistry *registry;
	EClientCache *client_cache;
	EBookClient *source_client;
	EBookClient *target_client;
	GCancellable *opening_client;
	GCancellable *saving;

	GtkWidget *top_box; /* not owned */
	GtkWidget *section_box; /* not owned */
	GtkWidget *button_save; /* not owned */
	GtkWidget *logo_photo_widget; /* not owned */
	GtkWidget *client_combo; /* not owned */
	GtkComboBoxText *file_under_combo; /* not owned */
	GtkEntry *full_name_entry; /* not owned */
	GtkEntry *name_parts[5]; /* not owned */
	GtkEntry *company_entry; /* not owned */
	GtkWidget *add_menu_button; /* not owned */
	GtkWidget *top_separator; /* not owned */
	GtkWidget *info_bar; /* not owned */

	GPtrArray *items; /* EVCardEditorItem *, those without section */
	GHashTable *sections; /* SectionKind ~> EVCardEditorSection * (not owned) */
	GHashTable *supported_fields; /* GINT_TO_POINTER(EContactField) ~> NULL */
	GCancellable *supported_fields_cancellable;
};

enum {
	PROP_0,
	PROP_CHANGED,
	PROP_CLIENT_CACHE,
	PROP_CONTACT,
	PROP_FLAGS,
	PROP_REGISTRY,
	PROP_SOURCE_CLIENT,
	PROP_TARGET_CLIENT,
	N_PROPS
};

enum {
	CUSTOM_SAVE,
	AFTER_SAVE,
	LAST_SIGNAL
};

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (EVCardEditor, e_vcard_editor, GTK_TYPE_DIALOG)

static void
eve_convert_contact_for_client (EContact **inout_contact,
				EBookClient *book_client)
{
	EVCardVersion vcard_version;

	vcard_version = e_book_client_get_prefer_vcard_version (book_client);

	if (vcard_version != E_VCARD_VERSION_UNKNOWN && vcard_version != e_vcard_get_version (E_VCARD (*inout_contact))) {
		EContact *converted;

		converted = e_contact_convert (*inout_contact, vcard_version);
		if (converted) {
			g_object_unref (*inout_contact);
			*inout_contact = converted;
		}
	}
}

static gboolean
eve_is_supported_field (EVCardEditor *self,
			EContactField field_id)
{
	if (!self->supported_fields || !g_hash_table_size (self->supported_fields))
		return TRUE;

	#define is_supported(_fldid) g_hash_table_contains (self->supported_fields, GINT_TO_POINTER (_fldid))

	if (field_id == FAKE_CONTACT_NAME_PREFIX ||
	    field_id == FAKE_CONTACT_NAME_FIRST ||
	    field_id == FAKE_CONTACT_NAME_MIDDLE ||
	    field_id == FAKE_CONTACT_NAME_LAST ||
	    field_id == FAKE_CONTACT_NAME_SUFFIX ||
	    field_id == E_CONTACT_NAME ||
	    field_id == E_CONTACT_EMAIL ||
	    field_id == E_CONTACT_TEL ||
	    is_supported (field_id))
		return TRUE;

	if (field_id == E_CONTACT_IMPP) {
		return is_supported (E_CONTACT_IM_AIM_HOME_1) ||
			is_supported (E_CONTACT_IM_AIM_HOME_2) ||
			is_supported (E_CONTACT_IM_AIM_HOME_3) ||
			is_supported (E_CONTACT_IM_AIM_WORK_1) ||
			is_supported (E_CONTACT_IM_AIM_WORK_2) ||
			is_supported (E_CONTACT_IM_AIM_WORK_3) ||
			is_supported (E_CONTACT_IM_GROUPWISE_HOME_1) ||
			is_supported (E_CONTACT_IM_GROUPWISE_HOME_2) ||
			is_supported (E_CONTACT_IM_GROUPWISE_HOME_3) ||
			is_supported (E_CONTACT_IM_GROUPWISE_WORK_1) ||
			is_supported (E_CONTACT_IM_GROUPWISE_WORK_2) ||
			is_supported (E_CONTACT_IM_GROUPWISE_WORK_3) ||
			is_supported (E_CONTACT_IM_JABBER_HOME_1) ||
			is_supported (E_CONTACT_IM_JABBER_HOME_2) ||
			is_supported (E_CONTACT_IM_JABBER_HOME_3) ||
			is_supported (E_CONTACT_IM_JABBER_WORK_1) ||
			is_supported (E_CONTACT_IM_JABBER_WORK_2) ||
			is_supported (E_CONTACT_IM_JABBER_WORK_3) ||
			is_supported (E_CONTACT_IM_YAHOO_HOME_1) ||
			is_supported (E_CONTACT_IM_YAHOO_HOME_2) ||
			is_supported (E_CONTACT_IM_YAHOO_HOME_3) ||
			is_supported (E_CONTACT_IM_YAHOO_WORK_1) ||
			is_supported (E_CONTACT_IM_YAHOO_WORK_2) ||
			is_supported (E_CONTACT_IM_YAHOO_WORK_3) ||
			is_supported (E_CONTACT_IM_MSN_HOME_1) ||
			is_supported (E_CONTACT_IM_MSN_HOME_2) ||
			is_supported (E_CONTACT_IM_MSN_HOME_3) ||
			is_supported (E_CONTACT_IM_MSN_WORK_1) ||
			is_supported (E_CONTACT_IM_MSN_WORK_2) ||
			is_supported (E_CONTACT_IM_MSN_WORK_3) ||
			is_supported (E_CONTACT_IM_ICQ_HOME_1) ||
			is_supported (E_CONTACT_IM_ICQ_HOME_2) ||
			is_supported (E_CONTACT_IM_ICQ_HOME_3) ||
			is_supported (E_CONTACT_IM_ICQ_WORK_1) ||
			is_supported (E_CONTACT_IM_ICQ_WORK_2) ||
			is_supported (E_CONTACT_IM_ICQ_WORK_3) ||
			is_supported (E_CONTACT_IM_AIM) ||
			is_supported (E_CONTACT_IM_GROUPWISE) ||
			is_supported (E_CONTACT_IM_JABBER) ||
			is_supported (E_CONTACT_IM_YAHOO) ||
			is_supported (E_CONTACT_IM_MSN) ||
			is_supported (E_CONTACT_IM_ICQ) ||
			is_supported (E_CONTACT_IM_SKYPE_HOME_1) ||
			is_supported (E_CONTACT_IM_SKYPE_HOME_2) ||
			is_supported (E_CONTACT_IM_SKYPE_HOME_3) ||
			is_supported (E_CONTACT_IM_SKYPE_WORK_1) ||
			is_supported (E_CONTACT_IM_SKYPE_WORK_2) ||
			is_supported (E_CONTACT_IM_SKYPE_WORK_3) ||
			is_supported (E_CONTACT_IM_SKYPE) ||
			is_supported (E_CONTACT_IM_GOOGLE_TALK_HOME_1) ||
			is_supported (E_CONTACT_IM_GOOGLE_TALK_HOME_2) ||
			is_supported (E_CONTACT_IM_GOOGLE_TALK_HOME_3) ||
			is_supported (E_CONTACT_IM_GOOGLE_TALK_WORK_1) ||
			is_supported (E_CONTACT_IM_GOOGLE_TALK_WORK_2) ||
			is_supported (E_CONTACT_IM_GOOGLE_TALK_WORK_3) ||
			is_supported (E_CONTACT_IM_GOOGLE_TALK) ||
			is_supported (E_CONTACT_IM_TWITTER) ||
			is_supported (E_CONTACT_IM_MATRIX_HOME_1) ||
			is_supported (E_CONTACT_IM_MATRIX_HOME_2) ||
			is_supported (E_CONTACT_IM_MATRIX_HOME_3) ||
			is_supported (E_CONTACT_IM_MATRIX_WORK_1) ||
			is_supported (E_CONTACT_IM_MATRIX_WORK_2) ||
			is_supported (E_CONTACT_IM_MATRIX_WORK_3) ||
			is_supported (E_CONTACT_IM_MATRIX);
	}

	if (field_id == E_CONTACT_TEL) {
		return is_supported (E_CONTACT_PHONE_ASSISTANT) ||
			is_supported (E_CONTACT_PHONE_BUSINESS) ||
			is_supported (E_CONTACT_PHONE_BUSINESS_2) ||
			is_supported (E_CONTACT_PHONE_BUSINESS_FAX) ||
			is_supported (E_CONTACT_PHONE_CALLBACK) ||
			is_supported (E_CONTACT_PHONE_CAR) ||
			is_supported (E_CONTACT_PHONE_COMPANY) ||
			is_supported (E_CONTACT_PHONE_HOME) ||
			is_supported (E_CONTACT_PHONE_HOME_2) ||
			is_supported (E_CONTACT_PHONE_HOME_FAX) ||
			is_supported (E_CONTACT_PHONE_ISDN) ||
			is_supported (E_CONTACT_PHONE_MOBILE) ||
			is_supported (E_CONTACT_PHONE_OTHER) ||
			is_supported (E_CONTACT_PHONE_OTHER_FAX) ||
			is_supported (E_CONTACT_PHONE_PAGER) ||
			is_supported (E_CONTACT_PHONE_PRIMARY) ||
			is_supported (E_CONTACT_PHONE_RADIO) ||
			is_supported (E_CONTACT_PHONE_TELEX) ||
			is_supported (E_CONTACT_PHONE_TTYTDD);
	}

	if (field_id == E_CONTACT_ADDRESS) {
		return is_supported (E_CONTACT_ADDRESS_HOME) ||
			is_supported (E_CONTACT_ADDRESS_WORK) ||
			is_supported (E_CONTACT_ADDRESS_OTHER);
	}

	#undef is_supported

	return FALSE;
}

static void
eve_update_file_under_choices (EVCardEditor *self)
{
	EContactName *name;
	const gchar *company, *current_value;
	gint index = -1, select = -1, previous_active;

	if (self->updating || !self->file_under_combo || !self->full_name_entry)
		return;

	current_value = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (self->file_under_combo))));

	if (current_value && *current_value)
		previous_active = gtk_combo_box_get_active (GTK_COMBO_BOX (self->file_under_combo));
	else
		previous_active = 0;

	gtk_combo_box_text_remove_all (self->file_under_combo);

	name = e_contact_name_from_string (gtk_entry_get_text (self->full_name_entry));
	company = self->company_entry ? gtk_entry_get_text (self->company_entry) : NULL;

	#define add_choice_direct(_choice) G_STMT_START { \
		gtk_combo_box_text_append_text (self->file_under_combo, _choice); \
		index++; \
		if (select == -1 && g_strcmp0 (current_value, _choice) == 0) \
			select = index; \
	} G_STMT_END
	#define add_choice(_choice) G_STMT_START { \
		gchar *str = _choice; \
		add_choice_direct (str); \
		g_free (str); \
	} G_STMT_END

	if (name) {
		if (name->family && *name->family) {
			if (name->given && *name->given) {
				add_choice (g_strdup_printf ("%s, %s", name->family, name->given));
				add_choice (g_strdup_printf ("%s %s", name->given, name->family));

				if (name->additional && *name->additional)
					add_choice (g_strdup_printf ("%s, %s %s", name->family, name->given, name->additional));

				if (company && *company) {
					add_choice_direct (company);
					add_choice (g_strdup_printf ("%s, %s (%s)", name->family, name->given, company));
					add_choice (g_strdup_printf ("%s (%s, %s)", company, name->family, name->given));
				}
			} else {
				add_choice_direct (name->family);

				if (company && *company) {
					add_choice (g_strdup_printf ("%s (%s)", name->family, company));
					add_choice (g_strdup_printf ("%s (%s)", company, name->family));
				}
			}
		} else if (name->given && *name->given) {
			add_choice_direct (name->given);

			if (company && *company) {
				add_choice (g_strdup_printf ("%s (%s)", name->given, company));
				add_choice (g_strdup_printf ("%s (%s)", company, name->given));
			}
		} else if (company && *company) {
			add_choice_direct (company);
		}
	} else if (company && *company) {
		add_choice_direct (company);
	}

	#undef add_choice
	#undef add_choice_direct

	e_contact_name_free (name);

	if (index == -1 && previous_active >= 0) {
		gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (self->file_under_combo))), "");
	} else if (select != -1) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->file_under_combo), select);
	} else if (previous_active >= 0) {
		GtkComboBox *combo = GTK_COMBO_BOX (self->file_under_combo);
		GtkTreeModel *model;
		gint n_available;

		model = gtk_combo_box_get_model (combo);
		n_available = gtk_tree_model_iter_n_children (model, NULL);

		if (previous_active < n_available) {
			gtk_combo_box_set_active (combo, previous_active);
		} else if (n_available > 0) {
			gtk_combo_box_set_active (combo, 0);
		}
	}
}

static void
eve_report_error (EVCardEditor *self,
		  const gchar *error_message,
		  GtkWidget *error_widget)
{
	EMessagePopover *message_popover;

	if (!error_message || !*error_message)
		return;

	if (error_widget)
		gtk_widget_grab_focus (error_widget);

	message_popover = e_message_popover_new (error_widget ? error_widget : self->top_separator,
		E_MESSAGE_POPOVER_FLAG_ERROR | E_MESSAGE_POPOVER_FLAG_HIGHLIGHT_POPOVER |
		(error_widget ? E_MESSAGE_POPOVER_FLAG_HIGHLIGHT_WIDGET : 0));

	e_message_popover_set_text_literal (message_popover, error_message);

	e_message_popover_show (message_popover);
}

static void
eve_report_save_error (EVCardEditor *self,
		       const GError *error,
		       GtkWidget *error_widget)
{
	if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		gchar *msg;

		msg = g_strdup_printf (_("Failed to save changes: %s"), error->message);
		eve_report_error (self, msg, NULL);
		g_free (msg);
	}
}

static GtkWidget *
eve_create_type_combo_work_home_other (EVCardAttribute *attr)
{
	const struct _attr_types {
		const gchar *value;
		const gchar *display_name;
	} work_home_other[] = {
		{ "work", NC_("addressbook-label", "Work") },
		{ "home", NC_("addressbook-label", "Home") },
		{ "other", NC_("addressbook-label", "Other") }
	};
	GtkWidget *combo;
	GtkComboBoxText *text_combo;
	const gchar *select_id = NULL;
	guint ii;

	combo = gtk_combo_box_text_new ();
	text_combo = GTK_COMBO_BOX_TEXT (combo);

	for (ii = 0; ii < G_N_ELEMENTS (work_home_other); ii++) {
		gtk_combo_box_text_append (text_combo, work_home_other[ii].value, _(work_home_other[ii].display_name));

		if (!select_id && attr && e_vcard_attribute_has_type (attr, work_home_other[ii].value))
			select_id = work_home_other[ii].value;
	}

	if (attr)
		gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), select_id ? select_id : "other");
	else
		gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), "work");

	return combo;
}

static void
eve_add_attr_type_generic (EVCardAttribute *attr,
			   const gchar *type)
{
	const gchar *comma;

	if (type == NULL)
		return;

	comma = strchr (type, ',');

	if (comma) {
		gchar *first_type = g_strndup (type, comma - type);

		e_vcard_attribute_add_param_with_value (attr, e_vcard_attribute_param_new (EVC_TYPE), first_type);
		e_vcard_attribute_add_param_with_value (attr, e_vcard_attribute_param_new (EVC_TYPE), comma + 1);

		g_free (first_type);
	} else {
		e_vcard_attribute_add_param_with_value (attr, e_vcard_attribute_param_new (EVC_TYPE), type);
	}
}

static EVCardEditorItem *
eve_add_item_attrs_generic (EVCardEditorSection *section,
			    EVCardAttribute *attr,
			    const gchar *value,
			    GtkWidget *(* create_type_combo_func) (EVCardAttribute *attr),
			    const gchar **preselect_items,
			    guint n_preselect_items,
			    const gchar *fallback_id)
{
	GtkWidget *combo, *entry;
	EVCardEditorItem *item;

	combo = create_type_combo_func (attr);

	entry = gtk_entry_new ();
	g_object_set (entry,
		"hexpand", TRUE,
		"width-chars", 25,
		"text", value ? value : "",
		NULL);

	if (n_preselect_items > 0) {
		guint n_items;

		n_items = e_vcard_editor_section_get_n_items (section);

		if (GTK_IS_COMBO_BOX_TEXT (combo)) {
			GtkTreeModel *model;
			gboolean any_set = FALSE;

			model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

			if (gtk_tree_model_iter_n_children (model, NULL) > 1) {
				GHashTable *used_values;
				guint ii;

				used_values = g_hash_table_new (g_str_hash, g_str_equal);

				for (ii = 0; ii < n_items; ii++) {
					GtkWidget *widget;

					item = e_vcard_editor_section_get_item (section, ii);
					widget = e_vcard_editor_item_get_data_widget (item, 0);

					if (GTK_IS_COMBO_BOX_TEXT (widget)) {
						const gchar *active_id;

						active_id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (widget));

						if (active_id)
							g_hash_table_add (used_values, (gpointer) active_id);
					}
				}

				for (ii = 0; ii < n_preselect_items; ii++) {
					const gchar *presel = preselect_items[ii];

					if (!g_hash_table_contains (used_values, presel)) {
						gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), presel);
						any_set = TRUE;
						break;
					}
				}

				g_hash_table_destroy (used_values);
			}

			if (!any_set && fallback_id)
				gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), fallback_id);
		}
	}

	item = e_vcard_editor_item_new (NULL, combo, FALSE, NULL, NULL);
	e_vcard_editor_item_add_data_widget (item, entry, TRUE);
	e_vcard_editor_item_add_remove_button (item, section);
	e_vcard_editor_section_take_item (section, item);

	gtk_widget_show_all (e_vcard_editor_item_get_data_box (item));

	g_signal_connect_object (combo, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);
	g_signal_connect_object (entry, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);

	return item;
}

static void
eve_read_attrs_generic (EVCardEditorSection *section,
			EContact *contact,
			EContactField field_id,
			GtkWidget *(* create_type_combo_func) (EVCardAttribute *attr))
{
	GList *attrs, *link;
	const gchar *vcard_attr_name;

	e_vcard_editor_section_remove_dynamic (section);

	vcard_attr_name = e_contact_vcard_attribute (field_id);
	g_return_if_fail (vcard_attr_name != NULL);

	attrs = e_vcard_get_attributes_by_name (E_VCARD (contact), vcard_attr_name);

	for (link = attrs; link; link = g_list_next (link)) {
		EVCardAttribute *attr = link->data;
		gchar *value;

		value = e_vcard_attribute_get_value (attr);

		if (value && *value) {
			EVCardEditorItem *item;

			item = eve_add_item_attrs_generic (section, attr, value, create_type_combo_func, NULL, 0, NULL);
			e_vcard_editor_item_set_field_id (item, field_id);
		}

		g_free (value);
	}

	g_list_free (attrs);
}

static void
eve_write_attrs_generic (EVCardEditorSection *section,
			 EContact *contact,
			 const gchar *vcard_attr_name,
			 void (* add_attr_type_func) (EVCardAttribute *attr,
						      const gchar *type))
{
	GList *attr_list = NULL;
	guint ii, n_items;

	if (!e_vcard_editor_section_get_changed (section))
		return;

	n_items = e_vcard_editor_section_get_n_items (section);

	for (ii = 0; ii < n_items; ii++) {
		EVCardEditorItem *item = e_vcard_editor_section_get_item (section, ii);
		GtkWidget *combo;
		GtkWidget *entry;
		gchar *value;

		combo = e_vcard_editor_item_get_data_widget (item, 0);
		entry = e_vcard_editor_item_get_data_widget (item, 1);
		value = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (entry))));

		if (value && *value) {
			EVCardAttribute *attr;
			const gchar *type;

			attr = e_vcard_attribute_new (NULL, vcard_attr_name);
			e_vcard_attribute_add_value_take (attr, g_steal_pointer (&value));

			type = gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo));
			if (type && *type)
				add_attr_type_func (attr, type);

			attr_list = g_list_prepend (attr_list, attr);
		}

		g_free (value);
	}

	attr_list = g_list_reverse (attr_list);

	e_vcard_remove_attributes (E_VCARD (contact), NULL, vcard_attr_name);
	e_vcard_append_attributes_take (E_VCARD (contact), g_steal_pointer (&attr_list));
}

static gboolean
eve_simple_is_multivalue_field_id (EContactField field_id)
{
	switch (field_id) {
	case E_CONTACT_ORG_DIRECTORY:
	case E_CONTACT_EXPERTISE:
	case E_CONTACT_HOBBY:
	case E_CONTACT_INTEREST:
		return TRUE;
	default:
		break;
	}

	return FALSE;
}

static void
eve_read_item_simple (EVCardEditorItem *item,
		      EVCardEditor *editor,
		      EContact *contact)
{
	GtkWidget *widget;
	gchar *text = NULL;

	if (eve_simple_is_multivalue_field_id (e_vcard_editor_item_get_field_id (item))) {
		GList *values, *link;
		GString *tmp = NULL;

		values = e_contact_get (contact, e_vcard_editor_item_get_field_id (item));
		for (link = values; link; link = g_list_next (link)) {
			gchar *value = link->data;

			if (!value)
				continue;

			value = g_strstrip (value);

			if (*value) {
				if (!tmp) {
					tmp = g_string_new (value);
				} else {
					g_string_append_c (tmp, ',');
					g_string_append (tmp, value);
				}
			}
		}

		g_list_free_full (values, g_free);

		if (tmp)
			text = g_string_free (tmp, FALSE);
	} else {
		text = e_contact_get (contact, e_vcard_editor_item_get_field_id (item));
	}

	widget = e_vcard_editor_item_get_data_widget (item, 0);

	if (GTK_IS_ENTRY (widget)) {
		gtk_entry_set_text (GTK_ENTRY (widget), text ? text : "");
	} else if (E_IS_VCARD_EDITOR_NOTES (widget)) {
		e_vcard_editor_notes_set_text (E_VCARD_EDITOR_NOTES (widget), text);
	} else {
		g_warning ("%s: Unexpected widget %s", G_STRFUNC, G_OBJECT_TYPE_NAME (widget));
	}

	g_free (text);
}

static gboolean
eve_write_item_simple (EVCardEditorItem *item,
		       EVCardEditor *editor,
		       EContact *contact,
		       gchar **out_error_message,
		       GtkWidget **out_error_widget)
{
	GtkWidget *widget;
	gchar *value = NULL;

	if (e_vcard_editor_item_get_field_id (item) == E_CONTACT_FULL_NAME &&
	    !editor->name_parts[0] && !editor->name_parts[1] && !editor->name_parts[2] &&
	    !editor->name_parts[3] && !editor->name_parts[4]) {
		/* the name is "recalculated" on full-name set, when it does not exist
		   and because there is no special editing for it, then drop the name */
		e_contact_set (contact, E_CONTACT_NAME, NULL);
	}

	widget = e_vcard_editor_item_get_data_widget (item, 0);

	if (GTK_IS_ENTRY (widget)) {
		value = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (widget))));
	} else if (E_IS_VCARD_EDITOR_NOTES (widget)) {
		value = e_vcard_editor_notes_get_text (E_VCARD_EDITOR_NOTES (widget));

		if (value)
			value = g_strstrip (value);
	} else {
		g_warning ("%s: Unexpected widget %s", G_STRFUNC, G_OBJECT_TYPE_NAME (widget));
	}

	if (value && *value) {
		if (eve_simple_is_multivalue_field_id (e_vcard_editor_item_get_field_id (item))) {
			gchar **split_text = g_strsplit (value, ",", -1);
			GList *values = NULL;
			guint ii;

			for (ii = 0; split_text && split_text[ii]; ii++) {
				gchar *value_stripped = g_strstrip (split_text[ii]);

				if (*value_stripped)
					values = g_list_prepend (values, value_stripped);
			}

			values = g_list_reverse (values);

			e_contact_set (contact, e_vcard_editor_item_get_field_id (item), values);

			g_list_free (values);
			g_strfreev (split_text);
		} else {
			e_contact_set (contact, e_vcard_editor_item_get_field_id (item), value);
		}
	} else {
		e_contact_set (contact, e_vcard_editor_item_get_field_id (item), NULL);
	}

	g_free (value);

	return TRUE;
}

static EVCardEditorItem *
eve_add_item_simple (EVCardEditorSection *section,
		     EContactField field_id,
		     const gchar *field_label)
{
	EVCardEditor *editor = e_vcard_editor_section_get_editor (section);
	EVCardEditorItem *item;
	GtkWidget *label;
	GtkWidget *entry;
	gchar *text;

	if (gtk_widget_get_direction (GTK_WIDGET (section)) == GTK_TEXT_DIR_RTL)
		text = g_strconcat (":", field_label, NULL);
	else
		text = g_strconcat (field_label, ":", NULL);

	label = gtk_label_new_with_mnemonic (text);
	g_object_set (label,
		"halign", GTK_ALIGN_END,
		NULL);

	g_free (text);

	entry = gtk_entry_new ();
	g_object_set (entry,
		"hexpand", TRUE,
		NULL);

	item = e_vcard_editor_item_new (label, entry, TRUE,
		eve_read_item_simple, eve_write_item_simple);
	e_vcard_editor_item_set_field_id (item, field_id);

	if (field_id != E_CONTACT_FULL_NAME)
		e_vcard_editor_item_add_remove_button (item, section);

	gtk_widget_show_all (e_vcard_editor_item_get_data_box (item));

	g_signal_connect_object (entry, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);

	e_vcard_editor_section_take_item (section, item);

	if (field_id == E_CONTACT_ORG) {
		editor->company_entry = GTK_ENTRY (entry);

		g_signal_connect_object (editor->company_entry, "changed",
			G_CALLBACK (eve_update_file_under_choices), editor, G_CONNECT_SWAPPED);
	}

	return item;
}

static gboolean
eve_check_exists_simple (EVCardEditor *editor,
			 EContact *contact,
			 EContactField field_id)
{
	gboolean exists = FALSE;

	if (!contact)
		return exists;

	if (eve_simple_is_multivalue_field_id (field_id)) {
		GList *values;

		values = e_contact_get (contact, field_id);
		exists = values != NULL;
		g_list_free_full (values, g_free);
	} else {
		gchar *value;

		value = e_contact_get (contact, field_id);
		exists = value != NULL && *value != '\0';
		g_free (value);
	}

	return exists;
}

static void
eve_remove_field_simple (EVCardEditor *editor,
			 EContact *contact,
			 EContactField field_id)
{
	e_contact_set (contact, field_id, NULL);
}

static void
eve_read_item_date (EVCardEditorItem *item,
		    EVCardEditor *editor,
		    EContact *contact)
{
	ESplitDateEdit *date_edit;
	EContactDate *date;

	date = e_contact_get (contact, e_vcard_editor_item_get_field_id (item));

	date_edit = E_SPLIT_DATE_EDIT (e_vcard_editor_item_get_data_widget (item, 0));

	e_split_date_edit_set_ymd (date_edit, date ? date->year : 0, date ? date->month : 0, date ? date->day : 0);
	if (date)
		e_contact_date_free (date);
}

static gboolean
eve_write_item_date (EVCardEditorItem *item,
		     EVCardEditor *editor,
		     EContact *contact,
		     gchar **out_error_message,
		     GtkWidget **out_error_widget)
{
	EContactDate date = { 0, 0, 0 };
	EContactField field_id = e_vcard_editor_item_get_field_id (item);
	ESplitDateEdit *date_edit;
	EVCardVersion vcard_version;

	date_edit = E_SPLIT_DATE_EDIT (e_vcard_editor_item_get_data_widget (item, 0));
	e_split_date_edit_get_ymd (date_edit, &date.year, &date.month, &date.day);

	if (field_id == E_CONTACT_BIRTH_DATE &&
	    date.year > 0 && date.month > 0 && date.day > 0) {
		GDateTime *dt = g_date_time_new_utc (date.year, date.month, date.day, 0, 0, 0.0);

		if (dt) {
			GDateTime *now = g_date_time_new_now_utc ();
			gboolean bad = FALSE;

			if (now && g_date_time_compare (dt, now) > 0) {
				*out_error_message = g_strdup_printf (_("“%s” cannot be a future date"),
					e_contact_pretty_name (field_id));
				*out_error_widget = GTK_WIDGET (date_edit);
				bad = TRUE;
			}

			g_clear_pointer (&now, g_date_time_unref);
			g_clear_pointer (&dt, g_date_time_unref);

			if (bad)
				return FALSE;
		}
	}

	vcard_version = e_vcard_get_version (E_VCARD (contact));
	/* pre-vCard 4.0 cannot save partial dates */
	if (vcard_version != E_VCARD_VERSION_UNKNOWN && vcard_version <= E_VCARD_VERSION_30) {
		if ((date.year > 0 || date.month > 0 || date.day > 0) && (!date.year || !date.month || !date.day)) {
			*out_error_message = g_strdup_printf (_("“%s” cannot be a partial date"),
				e_contact_pretty_name (field_id));
			*out_error_widget = GTK_WIDGET (date_edit);

			return FALSE;
		}
	}

	if (date.year != 0 || date.month != 0 || date.day != 0)
		e_contact_set (contact, field_id, &date);
	else
		e_contact_set (contact, field_id, NULL);

	return TRUE;
}

static EVCardEditorItem *
eve_add_item_date (EVCardEditorSection *section,
		   EContactField field_id,
		   const gchar *field_label)
{
	EVCardEditorItem *item;
	GtkWidget *label;
	GtkWidget *date_edit;
	gchar *text;

	if (gtk_widget_get_direction (GTK_WIDGET (section)) == GTK_TEXT_DIR_RTL)
		text = g_strconcat (":", field_label, NULL);
	else
		text = g_strconcat (field_label, ":", NULL);

	label = gtk_label_new_with_mnemonic (text);
	g_object_set (label,
		"halign", GTK_ALIGN_END,
		NULL);

	g_free (text);

	date_edit = e_split_date_edit_new ();
	g_object_set (date_edit,
		"hexpand", FALSE,
		NULL);

	item = e_vcard_editor_item_new (label, date_edit, FALSE, eve_read_item_date, eve_write_item_date);
	e_vcard_editor_item_set_field_id (item, field_id);
	e_vcard_editor_item_add_remove_button (item, section);

	gtk_widget_show_all (e_vcard_editor_item_get_data_box (item));

	g_signal_connect_object (date_edit, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);

	e_vcard_editor_section_take_item (section, item);

	return item;
}

static void
eve_read_item_geo (EVCardEditorItem *item,
		   EVCardEditor *editor,
		   EContact *contact)
{
	EContactGeo *geo;
	GtkEntry *lat, *lon;
	gchar *lat_str = NULL, *lot_str = NULL;

	geo = e_contact_get (contact, e_vcard_editor_item_get_field_id (item));

	lat = GTK_ENTRY (e_vcard_editor_item_get_data_widget (item, 0));
	lon = GTK_ENTRY (e_vcard_editor_item_get_data_widget (item, 1));

	if (geo && e_contact_geo_to_string (geo, &lat_str, &lot_str)) {
		gtk_entry_set_text (lat, lat_str);
		gtk_entry_set_text (lon, lot_str);

		g_free (lat_str);
		g_free (lot_str);
	} else {
		gtk_entry_set_text (lat, "");
		gtk_entry_set_text (lon, "");
	}

	e_contact_geo_free (geo);
}

static gboolean
eve_extract_item_geo (EVCardEditorItem *item,
		      EContactGeo *inout_geo,
		      gboolean *out_is_empty,
		      gchar **out_error_message,
		      GtkWidget **out_error_widget)
{
	GtkWidget *lat_widget, *lon_widget;
	gchar *lat, *lon, *ptr;

	if (out_is_empty)
		*out_is_empty = FALSE;

	lat_widget = e_vcard_editor_item_get_data_widget (item, 0);
	lon_widget = e_vcard_editor_item_get_data_widget (item, 1);

	lat = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (lat_widget))));
	lon = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (lon_widget))));

	if (!lat || !*lat) {
		if (!lon || !*lon) {
			if (out_is_empty)
				*out_is_empty = TRUE;

			g_free (lat);
			g_free (lon);

			return FALSE;
		}

		if (out_error_message)
			*out_error_message = g_strdup ("Both latitude and longitude should be filled");
		if (out_error_widget)
			*out_error_widget = lat_widget;

		g_free (lat);
		g_free (lon);

		return FALSE;
	} else if (!lon || !*lon) {
		if (out_error_message)
			*out_error_message = g_strdup ("Both latitude and longitude should be filled");
		if (out_error_widget)
			*out_error_widget = lon_widget;

		g_free (lat);
		g_free (lon);

		return FALSE;
	}

	ptr = strchr (lat, ',');
	if (ptr)
		*ptr = '.';

	ptr = strchr (lon, ',');
	if (ptr)
		*ptr = '.';

	ptr = NULL;
	inout_geo->latitude = g_ascii_strtod (lat, &ptr);

	if (ptr && *ptr != '\0') {
		if (out_error_message)
			*out_error_message = g_strdup ("Latitude is not a valid number");
		if (out_error_widget)
			*out_error_widget = lat_widget;

		g_free (lat);
		g_free (lon);

		return FALSE;
	}

	ptr = NULL;
	inout_geo->longitude = g_ascii_strtod (lon, &ptr);

	if (ptr && *ptr != '\0') {
		if (out_error_message)
			*out_error_message = g_strdup ("Longitude is not a valid number");
		if (out_error_widget)
			*out_error_widget = lon_widget;

		g_free (lat);
		g_free (lon);

		return FALSE;
	}

	g_free (lat);
	g_free (lon);

	return TRUE;
}

static gboolean
eve_write_item_geo (EVCardEditorItem *item,
		    EVCardEditor *editor,
		    EContact *contact,
		    gchar **out_error_message,
		    GtkWidget **out_error_widget)
{
	EContactGeo geo = { 0.0, 0.0 };
	EContactField field_id = e_vcard_editor_item_get_field_id (item);
	gboolean is_empty = FALSE;

	if (!eve_extract_item_geo (item, &geo, &is_empty, out_error_message, out_error_widget)) {
		if (is_empty)
			e_contact_set (contact, field_id, NULL);
		return is_empty;
	}

	e_contact_set (contact, field_id, &geo);

	return TRUE;
}

static void
eve_geo_open_clicked_cb (GtkWidget *button,
			 gpointer user_data)
{
	EVCardEditorItem *item = user_data;
	EContactGeo geo = { 0, 0 };

	if (eve_extract_item_geo (item, &geo, NULL, NULL, NULL)) {
		GtkWidget *parent;
		gchar buff[256];
		guint ii;

		g_warn_if_fail (g_snprintf (buff, sizeof (buff), "%.6f/%.6f", geo.latitude, geo.longitude) < sizeof (buff));

		for (ii = 0; buff[ii]; ii++) {
			if (buff[ii] == ',')
				buff[ii] = '.';
			else if (buff[ii] == '/')
				buff[ii] = ',';
		}

		parent = gtk_widget_get_ancestor (button, GTK_TYPE_WINDOW);

		e_open_map_uri (parent ? GTK_WINDOW (parent) : NULL, buff);
	}
}

static void
eve_geo_item_changed_cb (EVCardEditorItem *item,
			 gpointer user_data)
{
	GtkWidget *open_button = user_data;
	EContactGeo geo = { 0, 0 };

	gtk_widget_set_sensitive (open_button, eve_extract_item_geo (item, &geo, NULL, NULL, NULL));
}

static EVCardEditorItem *
eve_add_item_geo (EVCardEditorSection *section,
		  EContactField field_id,
		  const gchar *field_label)
{
	EVCardEditorItem *item;
	GtkWidget *label;
	GtkWidget *widget;

	label = gtk_label_new_with_mnemonic (_("_Geo:"));
	g_object_set (label,
		"halign", GTK_ALIGN_END,
		NULL);

	widget = gtk_entry_new ();
	g_object_set (widget,
		"hexpand", FALSE,
		"placeholder-text", _("Latitude"),
		"tooltip-text", _("Latitude"),
		NULL);

	item = e_vcard_editor_item_new (label, widget, FALSE, eve_read_item_geo, eve_write_item_geo);

	g_signal_connect_object (widget, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);

	widget = gtk_entry_new ();
	g_object_set (widget,
		"hexpand", FALSE,
		"placeholder-text", _("Longitude"),
		"tooltip-text", _("Longitude"),
		NULL);
	e_vcard_editor_item_add_data_widget (item, widget, FALSE);

	g_signal_connect_object (widget, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);

	widget = gtk_button_new_from_icon_name ("emblem-web", GTK_ICON_SIZE_BUTTON);
	g_object_set (widget,
		"halign", GTK_ALIGN_CENTER,
		"valign", GTK_ALIGN_CENTER,
		"hexpand", FALSE,
		"sensitive", FALSE,
		"tooltip-text", _("Open in map"),
		"always-show-image", TRUE,
		NULL);

	e_vcard_editor_item_add_data_widget (item, widget, FALSE);

	g_signal_connect_object (widget, "clicked",
		G_CALLBACK (eve_geo_open_clicked_cb), item, 0);

	g_signal_connect_object (item, "changed",
		G_CALLBACK (eve_geo_item_changed_cb), widget, 0);

	e_vcard_editor_item_set_field_id (item, field_id);
	e_vcard_editor_item_add_remove_button (item, section);

	gtk_widget_show_all (e_vcard_editor_item_get_data_box (item));

	e_vcard_editor_section_take_item (section, item);

	return item;
}

static gboolean
eve_find_tz_completion_match (GtkEntry *tz_entry,
			      GtkTreeModel **out_model,
			      GtkTreeIter *out_iter,
			      const gchar *value)
{
	GtkEntryCompletion *completion;
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (!value || !*value)
		return FALSE;

	completion = gtk_entry_get_completion (tz_entry);
	if (!completion)
		return FALSE;

	model = gtk_entry_completion_get_model (completion);
	if (!model)
		return FALSE;

	if (!gtk_tree_model_get_iter_first (model, &iter))
		return FALSE;

	do {
		gchar *display_name = NULL, *location = NULL;
		gboolean match;

		gtk_tree_model_get (model, &iter, 0, &display_name, 1, &location, -1);

		match = (display_name && g_utf8_collate (display_name, value) == 0) ||
			(location && g_ascii_strcasecmp (location, value) == 0);

		g_free (display_name);
		g_free (location);

		if (match) {
			*out_model = model;
			*out_iter = iter;

			return TRUE;
		}
	} while (gtk_tree_model_iter_next (model, &iter));

	return FALSE;
}

static void
eve_read_item_tz (EVCardEditorItem *item,
		  EVCardEditor *editor,
		  EContact *contact)
{
	EVCardAttribute *attr;
	GtkEntry *entry;

	entry = GTK_ENTRY (e_vcard_editor_item_get_data_widget (item, 0));

	attr = e_vcard_get_attribute (E_VCARD (contact), EVC_TZ);

	if (attr && e_vcard_attribute_get_n_values (attr) == 1) {
		GtkTreeModel *model = NULL;
		GtkTreeIter iter;
		const gchar *value;

		value = e_vcard_attribute_get_nth_value (attr, 0);
		if (value && *value && eve_find_tz_completion_match (entry, &model, &iter, value)) {
			gchar *display_name = NULL;

			gtk_tree_model_get (model, &iter, 0, &display_name, -1);

			gtk_entry_set_text (entry, (display_name && *display_name) ? display_name : value);

			g_free (display_name);
		} else {
			gtk_entry_set_text (entry, (value && *value) ? value : "");
		}
	} else {
		gtk_entry_set_text (entry, "");
	}
}

static gboolean
eve_write_item_tz (EVCardEditorItem *item,
		   EVCardEditor *editor,
		   EContact *contact,
		   gchar **out_error_message,
		   GtkWidget **out_error_widget)
{
	EVCard *vcard = E_VCARD (contact);
	GtkEntry *entry;
	gchar *value;

	e_vcard_remove_attributes (vcard, NULL, EVC_TZ);

	entry = GTK_ENTRY (e_vcard_editor_item_get_data_widget (item, 0));

	value = g_strstrip (g_strdup (gtk_entry_get_text (entry)));
	if (value && *value) {
		GtkTreeModel *model = NULL;
		GtkTreeIter iter;

		if (eve_find_tz_completion_match (entry, &model, &iter, value)) {
			gchar *location = NULL;

			gtk_tree_model_get (model, &iter, 1, &location, -1);

			if (location && *location) {
				g_free (value);
				value = g_steal_pointer (&location);
			}

			g_free (location);
		}

		e_vcard_add_attribute_with_value_take (vcard, e_vcard_attribute_new (NULL, EVC_TZ), g_steal_pointer (&value));
	}

	g_free (value);

	return TRUE;
}

static gboolean
eve_timezone_entry_completion_match_cb (GtkEntryCompletion *completion,
					const gchar *key,
					GtkTreeIter *iter,
					gpointer user_data)
{
	GtkTreeModel *model = gtk_entry_completion_get_model (completion);
	gchar *value = NULL;
	gboolean match;

	if (!model || !key || !*key)
		return FALSE;

	gtk_tree_model_get (model, iter, 0, &value, -1);

	if (!value)
		return FALSE;

	match = e_util_utf8_strstrcase (value, key) != NULL;

	g_free (value);

	return match;
}

static gchar *
eve_tz_dup_display_name (ICalTimezone *zone,
			 ICalTime *today,
			 gint *out_offset)
{
	const gchar *localized_location;
	const gchar *sign = "+";
	gint hours, minutes, seconds;
	gint is_daylight = 0; /* Its value is ignored, but libical-glib 3.0.5 API requires it */
	gint offset;

	offset = i_cal_timezone_get_utc_offset (zone, today, &is_daylight);

	*out_offset = offset;

	/* The localized_location() can reload the zone members, effectively freeing
	   the location, thus localize it only after the offset is known */
	localized_location = _(i_cal_timezone_get_location (zone));

	if (offset == 0)
		return g_strdup (localized_location);

	if (offset < 0) {
		offset = -offset;
		sign = "-";
	}

	hours = offset / 3600;
	minutes = (offset % 3600) / 60;
	seconds = offset % 60;

	if (seconds == 0)
		return g_strdup_printf ("%s (%s %s%02i:%02i)", localized_location, _("UTC"), sign, hours, minutes);
	else
		return g_strdup_printf ("%s (%s %s%02i:%02i:%02i)", localized_location, _("UTC"), sign, hours, minutes, seconds);
}

typedef struct _TzInfoData {
	ICalTimezone *zone;
	gchar *display_name;
	gchar *display_name_key;
	gint offset;
} TzInfoData;

static void
tz_info_data_clear (gpointer ptr)
{
	TzInfoData *data = ptr;

	if (data) {
		g_free (data->display_name);
		g_free (data->display_name_key);
	}
}

static gint
eve_sort_tz_infos_cb (gconstpointer aa,
		      gconstpointer bb)
{
	const TzInfoData *data1 = aa;
	const TzInfoData *data2 = bb;

	if (data1->offset != data2->offset)
		return data1->offset - data2->offset;

	return g_strcmp0 (data1->display_name_key, data2->display_name_key);
}

static EVCardEditorItem *
eve_add_item_tz (EVCardEditorSection *section,
		 EContactField field_id,
		 const gchar *field_label)
{
	EVCardEditorItem *item;
	GtkWidget *label;
	GtkWidget *entry;
	ICalArray *zones;

	label = gtk_label_new_with_mnemonic (_("Time _zone:"));
	g_object_set (label,
		"halign", GTK_ALIGN_END,
		NULL);

	entry = gtk_entry_new ();
	g_object_set (entry,
		"hexpand", TRUE,
		NULL);

	zones = i_cal_timezone_get_builtin_timezones ();
	if (zones && i_cal_array_size (zones) > 0) {
		GtkEntryCompletion *completion;
		GtkListStore *list_store;
		GtkTreeIter iter;
		GArray *tz_infos;
		ICalTime *today;
		gint ii, sz;

		today = i_cal_time_new_today ();

		list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
		sz = i_cal_array_size (zones);

		tz_infos = g_array_sized_new (FALSE, TRUE, sizeof (TzInfoData), sz);
		g_array_set_clear_func (tz_infos, tz_info_data_clear);

		for (ii = 0; ii < sz; ii++) {
			TzInfoData data = { NULL, };
			ICalTimezone *zone;
			const gchar *location;

			zone = i_cal_timezone_array_element_at (zones, ii);
			if (!zone)
				continue;

			location = i_cal_timezone_get_location (zone);
			if (!location || !*location)
				continue;

			data.zone = zone;
			data.display_name = eve_tz_dup_display_name (zone, today, &data.offset);
			data.display_name_key = g_utf8_collate_key (data.display_name, -1);

			g_array_append_val (tz_infos, data);
		}

		g_clear_object (&today);

		g_array_sort (tz_infos, eve_sort_tz_infos_cb);

		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter, 0, _("UTC"), 1, "UTC", -1);

		for (ii = 0; ii < tz_infos->len; ii++) {
			TzInfoData *data = &g_array_index (tz_infos, TzInfoData, ii);

			gtk_list_store_append (list_store, &iter);
			gtk_list_store_set (list_store, &iter, 0, data->display_name, 1, i_cal_timezone_get_location (data->zone), -1);
		}

		completion = gtk_entry_completion_new ();
		gtk_entry_completion_set_text_column (completion, 0);
		gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (list_store));
		gtk_entry_completion_set_match_func (completion, eve_timezone_entry_completion_match_cb, NULL, NULL);

		gtk_entry_set_completion (GTK_ENTRY (entry), completion);

		g_clear_object (&completion);
		g_clear_object (&list_store);
		g_clear_pointer (&tz_infos, g_array_unref);
	}

	item = e_vcard_editor_item_new (label, entry, TRUE, eve_read_item_tz, eve_write_item_tz);
	e_vcard_editor_item_set_field_id (item, field_id);

	g_signal_connect_object (entry, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);

	e_vcard_editor_item_add_remove_button (item, section);

	gtk_widget_show_all (e_vcard_editor_item_get_data_box (item));

	e_vcard_editor_section_take_item (section, item);

	return item;
}

static const gchar *
eve_name_part_get_value (EVCardEditor *self,
			 gint part_id,
			 const EContactName *fallback)
{
	GtkEntry *entry;

	entry = self->name_parts[part_id - FAKE_CONTACT_NAME_PREFIX];
	if (entry)
		return gtk_entry_get_text (entry);

	if (fallback) {
		switch (part_id) {
		case FAKE_CONTACT_NAME_PREFIX:
			return fallback->prefixes;
			break;
		case FAKE_CONTACT_NAME_FIRST:
			return fallback->given;
			break;
		case FAKE_CONTACT_NAME_MIDDLE:
			return fallback->additional;
			break;
		case FAKE_CONTACT_NAME_LAST:
			return fallback->family;
			break;
		case FAKE_CONTACT_NAME_SUFFIX:
			return fallback->suffixes;
			break;
		default:
			g_warn_if_reached ();
			break;
		}
	}

	return "";
}

static void
eve_name_part_set_value (EVCardEditor *self,
			 gint part_id,
			 const gchar *value)
{
	GtkEntry *entry;

	entry = self->name_parts[part_id - FAKE_CONTACT_NAME_PREFIX];
	if (!entry)
		return;

	gtk_entry_set_text (entry, value ? value : "");
}

static void
eve_full_name_changed_cb (EVCardEditor *self)
{
	EContactName *name;

	if (!self->full_name_entry || self->updating)
		return;

	if (!self->name_parts[0] && !self->name_parts[1] && !self->name_parts[2] &&
	    !self->name_parts[3] && !self->name_parts[4]) {
		eve_update_file_under_choices (self);
		return;
	}

	name = e_contact_name_from_string (gtk_entry_get_text (self->full_name_entry));

	self->updating = TRUE;
	eve_name_part_set_value (self, FAKE_CONTACT_NAME_PREFIX, name ? name->prefixes : NULL);
	eve_name_part_set_value (self, FAKE_CONTACT_NAME_FIRST, name ? name->given : NULL);
	eve_name_part_set_value (self, FAKE_CONTACT_NAME_MIDDLE, name ? name->additional : NULL);
	eve_name_part_set_value (self, FAKE_CONTACT_NAME_LAST, name ? name->family : NULL);
	eve_name_part_set_value (self, FAKE_CONTACT_NAME_SUFFIX, name ? name->suffixes : NULL);
	self->updating = FALSE;

	e_contact_name_free (name);

	eve_update_file_under_choices (self);
}

static void
eve_name_part_changed_cb (EVCardEditor *self)
{
	EContactName *name, stack_name;
	gchar *full_name;

	if (!self->full_name_entry || self->updating)
		return;

	memset (&stack_name, 0, sizeof (stack_name));

	name = e_contact_name_from_string (gtk_entry_get_text (self->full_name_entry));

	stack_name.prefixes = (gchar *) eve_name_part_get_value (self, FAKE_CONTACT_NAME_PREFIX, name);
	stack_name.given = (gchar *) eve_name_part_get_value (self, FAKE_CONTACT_NAME_FIRST, name);
	stack_name.additional = (gchar *) eve_name_part_get_value (self, FAKE_CONTACT_NAME_MIDDLE, name);
	stack_name.family = (gchar *) eve_name_part_get_value (self, FAKE_CONTACT_NAME_LAST, name);
	stack_name.suffixes = (gchar *) eve_name_part_get_value (self, FAKE_CONTACT_NAME_SUFFIX, name);

	self->updating = TRUE;
	full_name = e_contact_name_to_string (&stack_name);
	gtk_entry_set_text (self->full_name_entry, full_name);
	self->updating = FALSE;

	e_contact_name_free (name);
	g_free (full_name);

	eve_update_file_under_choices (self);
}

static void
eve_read_item_name (EVCardEditorItem *item,
		    EVCardEditor *editor,
		    EContact *contact)
{
	GtkWidget *widget;
	GtkEntry *entry;
	EContactName *name;
	const gchar *text = NULL;

	name = e_contact_get (contact, E_CONTACT_NAME);

	if (name) {
		switch ((gint) e_vcard_editor_item_get_field_id (item)) {
		case FAKE_CONTACT_NAME_PREFIX:
			text = name->prefixes;
			break;
		case FAKE_CONTACT_NAME_FIRST:
			text = name->given;
			break;
		case FAKE_CONTACT_NAME_MIDDLE:
			text = name->additional;
			break;
		case FAKE_CONTACT_NAME_LAST:
			text = name->family;
			break;
		case FAKE_CONTACT_NAME_SUFFIX:
			text = name->suffixes;
			break;
		default:
			g_warn_if_reached ();
			break;
		}
	}

	widget = e_vcard_editor_item_get_data_widget (item, 0);
	if (GTK_IS_COMBO_BOX_TEXT (widget))
		entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget)));
	else
		entry = GTK_ENTRY (widget);

	gtk_entry_set_text (entry, text ? text : "");

	if (name)
		e_contact_name_free (name);
}

static gboolean
eve_write_item_name (EVCardEditorItem *item,
		     EVCardEditor *editor,
		     EContact *contact,
		     gchar **out_error_message,
		     GtkWidget **out_error_widget)
{
	EContactName *name, stack_name;
	GtkWidget *widget;
	GtkEntry *entry;
	gchar *value;
	const gchar *full_name_text;

	memset (&stack_name, 0, sizeof (EContactName));

	widget = e_vcard_editor_item_get_data_widget (item, 0);
	if (GTK_IS_COMBO_BOX_TEXT (widget))
		entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget)));
	else
		entry = GTK_ENTRY (widget);
	value = g_strstrip (g_strdup (gtk_entry_get_text (entry)));

	full_name_text = editor->full_name_entry ? gtk_entry_get_text (editor->full_name_entry) : NULL;
	name = full_name_text ? e_contact_name_from_string (full_name_text) : NULL;

	stack_name.prefixes = name ? name->prefixes : NULL;
	stack_name.given = name ? name->given : NULL;
	stack_name.additional = name ? name->additional : NULL;
	stack_name.family = name ? name->family : NULL;
	stack_name.suffixes = name ? name->suffixes : NULL;

	switch ((gint) e_vcard_editor_item_get_field_id (item)) {
	case FAKE_CONTACT_NAME_PREFIX:
		stack_name.prefixes = value;
		break;
	case FAKE_CONTACT_NAME_FIRST:
		stack_name.given = value;
		break;
	case FAKE_CONTACT_NAME_MIDDLE:
		stack_name.additional = value;
		break;
	case FAKE_CONTACT_NAME_LAST:
		stack_name.family = value;
		break;
	case FAKE_CONTACT_NAME_SUFFIX:
		stack_name.suffixes = value;
		break;
	default:
		g_warn_if_reached ();
		break;
	}

	if ((!stack_name.prefixes || !*stack_name.prefixes) &&
	    (!stack_name.given || !*stack_name.given) &&
	    (!stack_name.additional || !*stack_name.additional) &&
	    (!stack_name.family || !*stack_name.family) &&
	    (!stack_name.suffixes || !*stack_name.suffixes)) {
		e_contact_set (contact, E_CONTACT_NAME, NULL);

		/* Set the NAME from the full name text */
		if (full_name_text)
			e_contact_set (contact, E_CONTACT_FULL_NAME, full_name_text);
	} else {
		e_contact_set (contact, E_CONTACT_NAME, &stack_name);
	}

	if (name)
		e_contact_name_free (name);

	g_free (value);

	return TRUE;
}

static EVCardEditorItem *
eve_add_item_name (EVCardEditorSection *section,
		   EContactField field_id,
		   const gchar *field_label)
{
	EVCardEditor *editor = e_vcard_editor_section_get_editor (section);
	EVCardEditorItem *item;
	GtkWidget *label;
	GtkWidget *widget;
	GtkEntry *entry;
	gchar *text;

	if (gtk_widget_get_direction (GTK_WIDGET (section)) == GTK_TEXT_DIR_RTL)
		text = g_strconcat (":", field_label, NULL);
	else
		text = g_strconcat (field_label, ":", NULL);

	label = gtk_label_new_with_mnemonic (text);
	g_object_set (label,
		"halign", GTK_ALIGN_END,
		NULL);

	g_free (text);

	if (field_id == FAKE_CONTACT_NAME_PREFIX || field_id == FAKE_CONTACT_NAME_SUFFIX) {
		const gchar *prefixes[] = { N_("Mr."), N_("Mrs."), N_("Ms."), N_("Miss"), N_("Dr."), NULL };
		const gchar *suffixes[] = { N_("Sr."), N_("Jr."), N_("I"), N_("II"), N_("III"), N_("Esq."), NULL };
		const gchar **items;
		GtkComboBoxText *text_combo;
		guint ii;

		widget = gtk_combo_box_text_new_with_entry ();
		entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget)));

		text_combo = GTK_COMBO_BOX_TEXT (widget);
		gtk_combo_box_text_append_text (text_combo, "");

		if (field_id == FAKE_CONTACT_NAME_PREFIX)
			items = prefixes;
		else
			items = suffixes;

		for (ii = 0; items[ii]; ii++) {
			gtk_combo_box_text_append_text (text_combo, _(items[ii]));
		}
	} else {
		widget = gtk_entry_new ();
		entry = GTK_ENTRY (widget);
	}
	g_object_set (widget,
		"hexpand", TRUE,
		NULL);

	item = e_vcard_editor_item_new (label, widget, TRUE,
		eve_read_item_name, eve_write_item_name);
	e_vcard_editor_item_set_field_id (item, field_id);

	gtk_widget_show_all (e_vcard_editor_item_get_data_box (item));

	g_signal_connect_object (entry, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);

	e_vcard_editor_section_take_item (section, item);

	if (editor) {
		if (editor->full_name_entry) {
			EContactName *name;
			const gchar *value;

			name = e_contact_name_from_string (gtk_entry_get_text (editor->full_name_entry));
			value = eve_name_part_get_value (editor, field_id, name);

			gtk_entry_set_text (entry, value ? value : "");

			e_contact_name_free (name);
		}

		editor->name_parts[field_id - FAKE_CONTACT_NAME_PREFIX] = entry;

		g_signal_connect_object (entry, "changed",
			G_CALLBACK (eve_name_part_changed_cb), editor, G_CONNECT_SWAPPED);
	}

	return item;
}

static gboolean
eve_check_exists_name (EVCardEditor *editor,
		       EContact *contact,
		       EContactField field_id)
{
	EContactName *name;
	gboolean exists = FALSE;

	if (!contact)
		return exists;

	name = e_contact_get (contact, E_CONTACT_NAME);
	if (name != NULL) {
		EContactName *name_from_full_name = NULL;
		gchar *full_name;

		full_name = e_contact_get (contact, E_CONTACT_FULL_NAME);
		name_from_full_name = full_name ? e_contact_name_from_string (full_name) : NULL;

		#define IS_FILLED_AND_DIFFERENT(_member) name->_member && *(name->_member) && (!name_from_full_name || e_util_strcmp0 (name->_member, name_from_full_name->_member) != 0)

		switch ((gint) field_id) {
		case FAKE_CONTACT_NAME_PREFIX:
			exists = IS_FILLED_AND_DIFFERENT (prefixes);
			break;
		case FAKE_CONTACT_NAME_FIRST:
			exists = IS_FILLED_AND_DIFFERENT (given);
			break;
		case FAKE_CONTACT_NAME_MIDDLE:
			exists = IS_FILLED_AND_DIFFERENT (additional);
			break;
		case FAKE_CONTACT_NAME_LAST:
			exists = IS_FILLED_AND_DIFFERENT (family);
			break;
		case FAKE_CONTACT_NAME_SUFFIX:
			exists = IS_FILLED_AND_DIFFERENT (suffixes);
			break;
		default:
			g_warn_if_reached ();
			break;
		}

		#undef IS_FILLED_AND_DIFFERENT

		g_free (full_name);
		e_contact_name_free (name_from_full_name);
		e_contact_name_free (name);
	}

	return exists;
}

static void
eve_read_item_gender (EVCardEditorItem *item,
		      EVCardEditor *editor,
		      EContact *contact)
{
	GtkComboBox *combo;
	GtkEntry *entry;
	EContactGender *gender;

	gender = e_contact_get (contact, E_CONTACT_GENDER);

	combo = GTK_COMBO_BOX (e_vcard_editor_item_get_data_widget (item, 0));
	entry = GTK_ENTRY (e_vcard_editor_item_get_data_widget (item, 1));

	if (gender) {
		gtk_combo_box_set_active_id (combo, e_contact_gender_sex_to_string (gender->sex));

		if (gtk_combo_box_get_active (combo) == -1)
			gtk_combo_box_set_active_id (combo, "");

		gtk_entry_set_text (entry, gender->identity ? gender->identity : "");
	} else {
		gtk_combo_box_set_active_id (combo, "");
		gtk_entry_set_text (entry, "");
	}

	e_contact_gender_free (gender);
}

static gboolean
eve_write_item_gender (EVCardEditorItem *item,
		       EVCardEditor *editor,
		       EContact *contact,
		       gchar **out_error_message,
		       GtkWidget **out_error_widget)
{
	EContactGender gender = { 0, };
	GtkComboBox *combo;
	GtkEntry *entry;
	const gchar *active_id;

	combo = GTK_COMBO_BOX (e_vcard_editor_item_get_data_widget (item, 0));
	entry = GTK_ENTRY (e_vcard_editor_item_get_data_widget (item, 1));

	active_id = gtk_combo_box_get_active_id (combo);
	if (!active_id)
		gender.sex = E_CONTACT_GENDER_SEX_NOT_SET;
	else
		gender.sex = e_contact_gender_sex_from_string (active_id);

	gender.identity = g_strstrip (g_strdup (gtk_entry_get_text (entry)));
	if (gender.identity && !*gender.identity)
		g_clear_pointer (&gender.identity, g_free);

	if (gender.identity != NULL || gender.sex != E_CONTACT_GENDER_SEX_NOT_SET)
		e_contact_set (contact, E_CONTACT_GENDER, &gender);
	else
		e_contact_set (contact, E_CONTACT_GENDER, NULL);

	g_free (gender.identity);

	return TRUE;
}

static EVCardEditorItem *
eve_add_item_gender (EVCardEditorSection *section,
		     EContactField field_id,
		     const gchar *field_label)
{
	EVCardEditorItem *item;
	GtkWidget *label;
	GtkWidget *combo;
	GtkWidget *entry;
	GtkComboBoxText *combo_text;
	gchar *text;

	if (gtk_widget_get_direction (GTK_WIDGET (section)) == GTK_TEXT_DIR_RTL)
		text = g_strconcat (":", field_label, NULL);
	else
		text = g_strconcat (field_label, ":", NULL);

	label = gtk_label_new_with_mnemonic (text);
	g_object_set (label,
		"halign", GTK_ALIGN_END,
		NULL);

	g_free (text);

	combo = gtk_combo_box_text_new ();
	g_object_set (combo,
		"hexpand", FALSE,
		NULL);
	combo_text = GTK_COMBO_BOX_TEXT (combo);
	gtk_combo_box_text_append (combo_text, "", "");
	gtk_combo_box_text_append (combo_text, "M", C_("gender-sex", "Male"));
	gtk_combo_box_text_append (combo_text, "F", C_("gender-sex", "Female"));
	gtk_combo_box_text_append (combo_text, "O", C_("gender-sex", "Other"));
	gtk_combo_box_text_append (combo_text, "U", C_("gender-sex", "Unknown"));
	gtk_combo_box_text_append (combo_text, "N", C_("gender-sex", "Not Applicable"));
	gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), "");

	entry = gtk_entry_new ();
	g_object_set (entry,
		"hexpand", TRUE,
		NULL);

	item = e_vcard_editor_item_new (label, combo, FALSE,
		eve_read_item_gender, eve_write_item_gender);
	e_vcard_editor_item_set_field_id (item, field_id);
	e_vcard_editor_item_add_data_widget (item, entry, TRUE);
	e_vcard_editor_item_add_remove_button (item, section);

	gtk_widget_show_all (e_vcard_editor_item_get_data_box (item));

	g_signal_connect_object (combo, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);
	g_signal_connect_object (entry, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);

	e_vcard_editor_section_take_item (section, item);

	return item;
}

static gboolean
eve_check_exists_gender (EVCardEditor *editor,
			 EContact *contact,
			 EContactField field_id)
{
	EContactGender *gender;
	gboolean exists = FALSE;

	if (!contact)
		return exists;

	gender = e_contact_get (contact, field_id);
	exists = gender != NULL;
	e_contact_gender_free (gender);

	return exists;
}

static EVCardEditorItem *
eve_add_item_email (EVCardEditorSection *section,
		    EContactField field_id,
		    const gchar *field_label)
{
	EVCardEditorItem *item;
	const gchar *preselect_items[] = { "work", "home", "other" };

	item = eve_add_item_attrs_generic (section, NULL, NULL, eve_create_type_combo_work_home_other,
		preselect_items, G_N_ELEMENTS (preselect_items), "other");
	e_vcard_editor_item_set_field_id (item, E_CONTACT_EMAIL);

	return item;
}

static void
eve_read_emails (EVCardEditorSection *section,
		 EContact *contact)
{
	GtkWidget *widget;

	eve_read_attrs_generic (section, contact, E_CONTACT_EMAIL, eve_create_type_combo_work_home_other);

	widget = e_vcard_editor_section_get_widget (section, 0);
	if (widget) {
		const gchar *value;

		value = e_contact_get (contact, E_CONTACT_WANTS_HTML);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value != NULL && (g_ascii_strcasecmp (value, "TRUE") == 0 || g_strcmp0 (value, "1") == 0));
	}
}

static gboolean
eve_write_emails (EVCardEditorSection *section,
		  EContact *contact,
		  gchar **out_error_message,
		  GtkWidget **out_error_widget)
{
	GtkWidget *widget;

	eve_write_attrs_generic (section, contact, EVC_EMAIL, eve_add_attr_type_generic);

	widget = e_vcard_editor_section_get_widget (section, 0);

	if (widget && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		e_contact_set (contact, E_CONTACT_WANTS_HTML, GINT_TO_POINTER (TRUE));
	else
		e_vcard_remove_attributes (E_VCARD (contact), NULL, e_contact_vcard_attribute (E_CONTACT_WANTS_HTML));

	return TRUE;
}

static GtkWidget *
eve_create_type_combo_phone (EVCardAttribute *attr)
{
	const struct _tel_types {
		EContactField field_id;
		const gchar *type_1;
		const gchar *type_2;
	} tel_types[] = {
		{ E_CONTACT_PHONE_ASSISTANT,    EVC_X_ASSISTANT,       NULL    },
		{ E_CONTACT_PHONE_BUSINESS,     "WORK",                "VOICE" },
		{ E_CONTACT_PHONE_BUSINESS_FAX, "WORK",                "FAX"   },
		{ E_CONTACT_PHONE_CALLBACK,     EVC_X_CALLBACK,        NULL    },
		{ E_CONTACT_PHONE_CAR,          "CAR",                 NULL    },
		{ E_CONTACT_PHONE_COMPANY,      "X-EVOLUTION-COMPANY", NULL,   },
		{ E_CONTACT_PHONE_HOME,         "HOME",                "VOICE" },
		{ E_CONTACT_PHONE_HOME_FAX,     "HOME",                "FAX"   },
		{ E_CONTACT_PHONE_ISDN,         "ISDN",                NULL    },
		{ E_CONTACT_PHONE_MOBILE,       "CELL",                NULL    },
		{ E_CONTACT_PHONE_OTHER,        "VOICE",               NULL    },
		{ E_CONTACT_PHONE_OTHER_FAX,    "FAX",                 NULL    },
		{ E_CONTACT_PHONE_PAGER,        "PAGER",               NULL    },
		{ E_CONTACT_PHONE_PRIMARY,      "PREF",                NULL    },
		{ E_CONTACT_PHONE_RADIO,        EVC_X_RADIO,           NULL    },
		{ E_CONTACT_PHONE_TELEX,        EVC_X_TELEX,           NULL    },
		{ E_CONTACT_PHONE_TTYTDD,       EVC_X_TTYTDD,          NULL    }
	};
	GtkWidget *combo;
	GtkComboBoxText *text_combo;
	gint select_index = -1, ii;

	combo = gtk_combo_box_text_new ();
	text_combo = GTK_COMBO_BOX_TEXT (combo);

	for (ii = 0; ii < G_N_ELEMENTS (tel_types); ii++) {
		if (tel_types[ii].type_2) {
			gchar *tmp;

			tmp = g_strconcat (tel_types[ii].type_1, ",", tel_types[ii].type_2, NULL);
			gtk_combo_box_text_append (text_combo, tmp, e_contact_pretty_name (tel_types[ii].field_id));
			g_free (tmp);
		} else {
			gtk_combo_box_text_append (text_combo, tel_types[ii].type_1, e_contact_pretty_name (tel_types[ii].field_id));
		}

		if (select_index == -1 && attr && e_vcard_attribute_has_type (attr, tel_types[ii].type_1) && (
		    !tel_types[ii].type_2 || e_vcard_attribute_has_type (attr, tel_types[ii].type_2))) {
			select_index = ii;
		}
	}

	if (select_index >= 0)
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo), select_index);
	else if (attr)
		gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), "VOICE");
	else
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 1);

	return combo;
}

static EVCardEditorItem *
eve_add_item_phone (EVCardEditorSection *section,
		    EContactField field_id,
		    const gchar *field_label)
{
	EVCardEditorItem *item;
	const gchar *preselect_items[] = { "WORK,VOICE", "CELL", "HOME,VOICE", "HOME,FAX", "PAGER", "VOICE" };

	item = eve_add_item_attrs_generic (section, NULL, NULL, eve_create_type_combo_phone,
		preselect_items, G_N_ELEMENTS (preselect_items), "VOICE");
	e_vcard_editor_item_set_field_id (item, E_CONTACT_TEL);

	return item;
}

static void
eve_read_phones (EVCardEditorSection *section,
		 EContact *contact)
{
	eve_read_attrs_generic (section, contact, E_CONTACT_TEL, eve_create_type_combo_phone);
}

static gboolean
eve_write_phones (EVCardEditorSection *section,
		  EContact *contact,
		  gchar **out_error_message,
		  GtkWidget **out_error_widget)
{
	eve_write_attrs_generic (section, contact, EVC_TEL, eve_add_attr_type_generic);

	return TRUE;
}

static EVCardEditorItem *
eve_add_item_sip (EVCardEditorSection *section,
		  EContactField field_id,
		  const gchar *field_label)
{
	EVCardEditorItem *item;
	const gchar *preselect_items[] = { "work", "home", "other" };

	item = eve_add_item_attrs_generic (section, NULL, NULL, eve_create_type_combo_work_home_other,
		preselect_items, G_N_ELEMENTS (preselect_items), "other");
	e_vcard_editor_item_set_field_id (item, E_CONTACT_SIP);

	return item;
}

static void
eve_read_sips (EVCardEditorSection *section,
	       EContact *contact)
{
	eve_read_attrs_generic (section, contact, E_CONTACT_SIP, eve_create_type_combo_work_home_other);
}

static gboolean
eve_write_sips (EVCardEditorSection *section,
		EContact *contact,
		gchar **out_error_message,
		GtkWidget **out_error_widget)
{
	eve_write_attrs_generic (section, contact, EVC_X_SIP, eve_add_attr_type_generic);

	return TRUE;
}

static GtkWidget *
eve_create_type_combo_im (EVCardAttribute *attr)
{
	gint n_im_services = -1;
	const EABTypeLabel *im_services = eab_get_im_type_labels (&n_im_services);
	GtkWidget *combo;
	GtkComboBoxText *text_combo;
	guint ii;

	combo = gtk_combo_box_text_new_with_entry ();
	text_combo = GTK_COMBO_BOX_TEXT (combo);

	for (ii = 0; ii < n_im_services; ii++) {
		const gchar *scheme;

		scheme = e_contact_impp_field_to_scheme (im_services[ii].field_id);

		gtk_combo_box_text_append (text_combo, scheme, _(im_services[ii].text));
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

	return combo;
}

static EVCardEditorItem *
eve_add_item_im (EVCardEditorSection *section,
		 EContactField field_id,
		 const gchar *field_label)
{
	EVCardEditorItem *item;
	const gchar *preselect_items[4];

	preselect_items[0] = e_contact_impp_field_to_scheme (E_CONTACT_IM_MATRIX);
	preselect_items[1] = e_contact_impp_field_to_scheme (E_CONTACT_IM_AIM);
	preselect_items[2] = e_contact_impp_field_to_scheme (E_CONTACT_IM_YAHOO);
	preselect_items[3] = e_contact_impp_field_to_scheme (E_CONTACT_IM_MSN);

	item = eve_add_item_attrs_generic (section, NULL, NULL, eve_create_type_combo_im,
		preselect_items, G_N_ELEMENTS (preselect_items), preselect_items[0]);
	e_vcard_editor_item_set_field_id (item, E_CONTACT_IMPP);

	return item;
}

static void
eve_read_ims (EVCardEditorSection *section,
	      EContact *contact)
{
	gint n_im_services = -1;
	const EABTypeLabel *im_services = eab_get_im_type_labels (&n_im_services);
	GList *values, *link;

	e_vcard_editor_section_remove_dynamic (section);

	values = e_contact_get (contact, E_CONTACT_IMPP);

	for (link = values; link; link = g_list_next (link)) {
		const gchar *value = link->data;
		gchar cut_scheme[128] = { 0, }; /* if anyone uses scheme longer than this, then... well... */
		EVCardEditorItem *item;
		GtkWidget *widget;
		EContactField field;
		guint scheme_len = 0;
		gint service_type = -1;

		if (!value || !*value)
			continue;

		field = e_contact_impp_scheme_to_field (value, &scheme_len);

		if (field == E_CONTACT_FIELD_LAST) {
			const gchar *ptr = strchr (value, ':');
			if (ptr)
				scheme_len = ptr - value + 1;
		} else {
			for (service_type = 0; service_type < n_im_services; service_type++) {
				if (field == im_services[service_type].field_id)
					break;
			}

			if (service_type >= n_im_services)
				service_type = -1;
		}

		if (scheme_len > 0 && scheme_len < sizeof (cut_scheme))
			strncpy (cut_scheme, value, scheme_len - 1);

		item = eve_add_item_attrs_generic (section, NULL, scheme_len ? value + scheme_len : value, eve_create_type_combo_im, NULL, 0, NULL);
		e_vcard_editor_item_set_field_id (item, E_CONTACT_IMPP);

		widget = e_vcard_editor_item_get_data_widget (item, 0);

		if (service_type != -1) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), service_type);
		} else if (service_type == -1 && cut_scheme[0]) {
			GtkWidget *entry;

			entry = gtk_bin_get_child (GTK_BIN (widget));
			gtk_entry_set_text (GTK_ENTRY (entry), cut_scheme);
		}

		widget = e_vcard_editor_item_get_data_widget (item, 1);
		gtk_entry_set_text (GTK_ENTRY (widget), scheme_len ? value + scheme_len : value);
	}

	g_list_free_full (values, g_free);
}

static gboolean
eve_write_ims (EVCardEditorSection *section,
	       EContact *contact,
	       gchar **out_error_message,
	       GtkWidget **out_error_widget)
{
	GList *impps = NULL; /* gchar * */
	guint ii, n_items;

	if (!e_vcard_editor_section_get_changed (section))
		return TRUE;

	n_items = e_vcard_editor_section_get_n_items (section);

	for (ii = 0; ii < n_items; ii++) {
		EVCardEditorItem *item = e_vcard_editor_section_get_item (section, ii);
		GtkWidget *combo;
		GtkWidget *entry;
		gchar *value;

		combo = e_vcard_editor_item_get_data_widget (item, 0);
		entry = e_vcard_editor_item_get_data_widget (item, 1);
		value = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (entry))));

		if (value && *value) {
			const gchar *scheme;
			gchar *impp_value;

			if (gtk_combo_box_get_active (GTK_COMBO_BOX (combo)) >= 0) {
				scheme = gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo));
			} else {
				scheme = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (combo))));
			}

			if (scheme && *scheme) {
				impp_value = g_strconcat (scheme, scheme[strlen (scheme) - 1] == ':' ? "" : ":", value, NULL);
			} else {
				impp_value = g_strconcat ("impp:", value, NULL);
			}

			impps = g_list_prepend (impps, impp_value);
		}

		g_free (value);
	}

	impps = g_list_reverse (impps);

	e_contact_set (contact, E_CONTACT_IMPP, impps);

	g_list_free_full (impps, g_free);

	return TRUE;
}

static EVCardEditorItem *
eve_add_item_web (EVCardEditorSection *section,
		  EContactField field_id,
		  const gchar *field_label)
{
	GtkWidget *entry;
	GtkWidget *label;
	EVCardEditorItem *item;
	gchar *text;

	if (gtk_widget_get_direction (GTK_WIDGET (section)) == GTK_TEXT_DIR_RTL)
		text = g_strconcat (":", field_label, NULL);
	else
		text = g_strconcat (field_label, ":", NULL);

	label = gtk_label_new_with_mnemonic (text);
	g_object_set (label,
		"halign", GTK_ALIGN_END,
		NULL);

	g_free (text);

	entry = e_url_entry_new ();
	g_object_set (entry,
		"hexpand", TRUE,
		"width-chars", 25,
		NULL);

	item = e_vcard_editor_item_new (label, entry, TRUE, NULL, NULL);
	e_vcard_editor_item_set_field_id (item, field_id);
	e_vcard_editor_item_add_remove_button (item, section);
	e_vcard_editor_section_take_item (section, item);

	gtk_widget_show_all (e_vcard_editor_item_get_data_box (item));

	g_signal_connect_object (entry, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);

	return item;
}

static const struct _webs {
	const gchar *label;
	EContactField field_id;
} webs[] = {
	{ N_("_Blog"),		E_CONTACT_BLOG_URL },
	{ N_("_Calendar"),	E_CONTACT_CALENDAR_URI },
	{ N_("_Free/Busy"),	E_CONTACT_FREEBUSY_URL },
	{ N_("_Video Chat"),	E_CONTACT_VIDEO_URL },
	{ N_("_URL"),		E_CONTACT_HOMEPAGE_URL }
};

static void
eve_read_webs (EVCardEditorSection *section,
	       EContact *contact)
{
	EVCard *vcard;
	guint ii;

	e_vcard_editor_section_remove_dynamic (section);

	if (!contact)
		return;

	vcard = E_VCARD (contact);

	for (ii = 0; ii < G_N_ELEMENTS (webs); ii++) {
		GList *attrs, *link;

		attrs = e_vcard_get_attributes_by_name (vcard, e_contact_vcard_attribute (webs[ii].field_id));

		for (link = attrs; link; link = g_list_next (link)) {
			EVCardAttribute *attr = link->data;
			gchar *value;

			value = e_vcard_attribute_get_value (attr);

			if (value && *value) {
				EVCardEditorItem *item;
				GtkWidget *entry;

				item = eve_add_item_web (section, webs[ii].field_id, webs[ii].label);
				entry = e_vcard_editor_item_get_data_widget (item, 0);
				gtk_entry_set_text (GTK_ENTRY (entry), value);
			}

			g_free (value);
		}

		g_list_free (attrs);
	}
}

static gboolean
eve_write_webs (EVCardEditorSection *section,
		EContact *contact,
		gchar **out_error_message,
		GtkWidget **out_error_widget)
{
	EVCard *vcard;
	GList *attr_list = NULL;
	const gchar *vcard_attr_name;
	EContactField last_field_id = E_CONTACT_FIELD_LAST;
	guint ii, n_items;

	if (!e_vcard_editor_section_get_changed (section))
		return TRUE;

	vcard = E_VCARD (contact);

	for (ii = 0; ii < G_N_ELEMENTS (webs); ii++) {
		vcard_attr_name = e_contact_vcard_attribute (webs[ii].field_id);

		e_vcard_remove_attributes (vcard, NULL, vcard_attr_name);
	}

	vcard_attr_name = NULL;
	n_items = e_vcard_editor_section_get_n_items (section);

	for (ii = 0; ii < n_items; ii++) {
		EVCardEditorItem *item = e_vcard_editor_section_get_item (section, ii);
		GtkWidget *entry;
		gchar *value;

		if (e_vcard_editor_item_get_field_id (item) != last_field_id) {
			last_field_id = e_vcard_editor_item_get_field_id (item);
			vcard_attr_name = e_contact_vcard_attribute (last_field_id);
		}

		entry = e_vcard_editor_item_get_data_widget (item, 0);
		value = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (entry))));

		if (value && *value) {
			EVCardAttribute *attr;

			attr = e_vcard_attribute_new (NULL, vcard_attr_name);
			e_vcard_attribute_add_value_take (attr, g_steal_pointer (&value));

			attr_list = g_list_prepend (attr_list, attr);
		}

		g_free (value);
	}

	attr_list = g_list_reverse (attr_list);

	e_vcard_append_attributes_take (vcard, g_steal_pointer (&attr_list));

	return TRUE;
}

static EVCardEditorItem *
eve_add_item_addr (EVCardEditorSection *section,
		   EContactField field_id,
		   const gchar *field_label)
{
	GtkWidget *addr_widget;
	EVCardEditorItem *item;
	guint n_items;

	addr_widget = e_vcard_editor_address_new ();
	g_object_set (addr_widget,
		"hexpand", TRUE,
		NULL);

	item = e_vcard_editor_item_new (NULL, addr_widget, TRUE, NULL, NULL);
	e_vcard_editor_item_set_field_id (item, field_id);
	e_vcard_editor_item_add_remove_button (item, section);

	n_items = e_vcard_editor_section_get_n_items (section);
	if (n_items > 0) {
		gboolean work_used = FALSE, home_used = FALSE;
		guint ii;

		for (ii = 0; ii < n_items && (!work_used || !home_used); ii++) {
			EVCardEditorItem *existing = e_vcard_editor_section_get_item (section, ii);
			GtkWidget *widget;

			widget = e_vcard_editor_item_get_data_widget (existing, 0);

			if (E_IS_VCARD_EDITOR_ADDRESS (widget)) {
				const gchar *addr_type;

				addr_type = e_vcard_editor_address_get_address_type (E_VCARD_EDITOR_ADDRESS (widget));

				work_used = work_used || g_strcmp0 (addr_type, "work") == 0;
				home_used = home_used || g_strcmp0 (addr_type, "home") == 0;
			}
		}

		e_vcard_editor_address_fill_widgets (E_VCARD_EDITOR_ADDRESS (addr_widget),
			(!work_used) ? "work" : (!home_used) ? "home" : "other", NULL);
	}

	e_vcard_editor_section_take_item (section, item);

	gtk_widget_show_all (e_vcard_editor_item_get_data_box (item));

	g_signal_connect_object (addr_widget, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);

	return item;
}

static void
eve_read_addrs (EVCardEditorSection *section,
		EContact *contact)
{
	EVCard *vcard;
	GList *attrs, *link;

	e_vcard_editor_section_remove_dynamic (section);

	if (!contact)
		return;

	vcard = E_VCARD (contact);

	attrs = e_vcard_get_attributes_by_name (vcard, EVC_ADR);

	for (link = attrs; link; link = g_list_next (link)) {
		EVCardAttribute *attr = link->data;
		EContactAddress addr;

		memset (&addr, 0, sizeof (EContactAddress));

		e_contact_address_read_attr (&addr, attr);

		if ((addr.street && *addr.street) || (addr.ext && *addr.ext) || (addr.po && *addr.po) || (addr.locality && *addr.locality) ||
		    (addr.code && *addr.code) || (addr.region && *addr.region) || (addr.country && *addr.country)) {
			EVCardEditorItem *item;
			GtkWidget *addr_widget;
			const gchar *type;

			if (e_vcard_attribute_has_type (attr, "work"))
				type = "work";
			else if (e_vcard_attribute_has_type (attr, "home"))
				type = "home";
			else
				type = "other";

			item = eve_add_item_addr (section, E_CONTACT_ADDRESS, NULL);
			addr_widget = e_vcard_editor_item_get_data_widget (item, 0);
			e_vcard_editor_address_fill_widgets (E_VCARD_EDITOR_ADDRESS (addr_widget), type, &addr);
		}

		e_contact_address_clear (&addr);
	}

	g_list_free (attrs);
}

static gboolean
eve_write_addrs (EVCardEditorSection *section,
		 EContact *contact,
		 gchar **out_error_message,
		 GtkWidget **out_error_widget)
{
	EVCard *vcard;
	EVCardVersion version;
	GList *attr_list = NULL;
	guint n_work = 0, n_home = 0, n_other = 0;
	guint ii, n_items;

	if (!e_vcard_editor_section_get_changed (section))
		return TRUE;

	vcard = E_VCARD (contact);
	version = e_vcard_get_version (vcard);

	e_vcard_remove_attributes (vcard, NULL, EVC_ADR);
	e_vcard_remove_attributes (vcard, NULL, EVC_LABEL);

	n_items = e_vcard_editor_section_get_n_items (section);

	for (ii = 0; ii < n_items; ii++) {
		EVCardEditorItem *item = e_vcard_editor_section_get_item (section, ii);
		GtkWidget *addr_widget;
		EContactAddress *addr = NULL;
		gchar *type = NULL;

		addr_widget = e_vcard_editor_item_get_data_widget (item, 0);

		if (!e_vcard_editor_address_fill_addr (E_VCARD_EDITOR_ADDRESS (addr_widget), &type, &addr, out_error_message, out_error_widget))
			return FALSE;

		if (addr) {
			EVCardAttribute *attr;
			EContactField address_type = E_CONTACT_ADDRESS_OTHER;
			gchar *label;
			guint n_attrs;

			if (type && *type) {
				if (g_ascii_strcasecmp (type, "work") == 0) {
					n_work++;
					n_attrs = n_work;
					address_type = E_CONTACT_ADDRESS_WORK;
				} else if (g_ascii_strcasecmp (type, "home") == 0) {
					n_home++;
					n_attrs = n_home;
					address_type = E_CONTACT_ADDRESS_HOME;
				} else {
					n_other++;
					n_attrs = n_other;
				}
			} else {
				n_other++;
				n_attrs = n_other;
			}

			label = eab_format_address_label (addr, address_type, e_contact_get_const (contact, E_CONTACT_ORG));

			if (n_attrs > 1) {
				gchar *group;

				group = g_strdup_printf ("adr%u", n_attrs);
				attr = e_vcard_attribute_new (group, EVC_ADR);
				g_free (group);
			} else {
				attr = e_vcard_attribute_new (NULL, EVC_ADR);
			}
			if (type && *type)
				e_vcard_attribute_add_param_with_value (attr, e_vcard_attribute_param_new (EVC_TYPE), type);

			e_contact_address_write_attr (addr, attr);

			attr_list = g_list_prepend (attr_list, attr);

			if (label && *label) {
				if (version == E_VCARD_VERSION_40) {
					e_vcard_attribute_add_param_with_value (attr, e_vcard_attribute_param_new (EVC_LABEL), label);
				} else {
					attr = e_vcard_attribute_new (e_vcard_attribute_get_group (attr), EVC_LABEL);
					if (type != NULL)
						e_vcard_attribute_add_param_with_value (attr, e_vcard_attribute_param_new (EVC_TYPE), type);
					e_vcard_attribute_add_value_take (attr, g_steal_pointer (&label));
					attr_list = g_list_prepend (attr_list, attr);
				}
			}

			g_free (label);
		}

		e_contact_address_free (addr);
		g_free (type);
	}

	attr_list = g_list_reverse (attr_list);

	e_vcard_append_attributes_take (vcard, g_steal_pointer (&attr_list));

	return TRUE;
}

static void
eve_focus_on_size_allocate_cb (GtkWidget *widget,
			       GdkRectangle *allocation,
			       gpointer user_data)
{
	g_signal_handlers_disconnect_by_func (widget, G_CALLBACK (eve_focus_on_size_allocate_cb), NULL);

	gtk_widget_grab_focus (widget);
}

static void
eve_focus_added_item (EVCardEditorItem *item)
{
	if (item) {
		GtkWidget *widget;

		widget = e_vcard_editor_item_get_data_widget (item, 0);
		g_signal_connect (widget, "size-allocate",
			G_CALLBACK (eve_focus_on_size_allocate_cb), NULL);
	}
}

static void
eve_cert_widget_add_clicked_cb (EVCardEditorSection *section);
static void
eve_cert_widget_remove_clicked_cb (GtkWidget *cert_widget,
				   gpointer user_data);

static EVCardEditorItem *
eve_add_item_cert_to_section (EVCardEditorSection *section,
			      GtkWidget *cert_widget) /* transfer full, nullable */
{
	EVCardEditorItem *item;
	GtkWidget *widget;

	widget = cert_widget ? cert_widget : e_vcard_editor_certificate_new ();

	g_signal_connect_object (widget, "add-clicked",
		G_CALLBACK (eve_cert_widget_add_clicked_cb), section, G_CONNECT_SWAPPED);
	g_signal_connect_object (widget, "remove-clicked",
		G_CALLBACK (eve_cert_widget_remove_clicked_cb), section, 0);

	item = e_vcard_editor_item_new (NULL, widget, TRUE, NULL, NULL);

	gtk_widget_show_all (e_vcard_editor_item_get_data_box (item));

	g_signal_connect_object (widget, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);

	e_vcard_editor_section_take_item (section, item);

	return item;
}

static EVCardEditorItem *
eve_add_item_cert_choose_file (EVCardEditorSection *section,
			       EContactField prefer_type)
{
	EVCardEditor *self;
	EVCardEditorItem *item = NULL;
	GtkWidget *cert_widget;
	GtkWindow *parent;
	GError *local_error = NULL;

	self = e_vcard_editor_section_get_editor (section);
	parent = GTK_WINDOW (self);

	cert_widget = e_vcard_editor_certificate_new_from_chooser (GTK_IS_WINDOW (parent) ? GTK_WINDOW (parent) : NULL, prefer_type,
		eve_is_supported_field (self, E_CONTACT_PGP_CERT), &local_error);

	if (cert_widget)
		item = eve_add_item_cert_to_section (section, g_steal_pointer (&cert_widget));
	else if (local_error)
		eve_report_error (self, local_error->message, NULL);

	g_clear_error (&local_error);

	return item;
}

static void
eve_cert_widget_add_clicked_cb (EVCardEditorSection *section)
{
	EVCardEditorItem *item;

	item = eve_add_item_cert_choose_file (section, E_CONTACT_FIELD_LAST);

	eve_focus_added_item (item);
}

static void
eve_cert_widget_remove_clicked_cb (GtkWidget *cert_widget,
				   gpointer user_data)
{
	EVCardEditorSection *section = user_data;
	guint ii, n_items;

	n_items = e_vcard_editor_section_get_n_items (section);

	for (ii = 0; ii < n_items; ii++) {
		EVCardEditorItem *item = e_vcard_editor_section_get_item (section, ii);
		GtkWidget *data_widget;

		data_widget = e_vcard_editor_item_get_data_widget (item, 0);

		if (data_widget == cert_widget) {
			e_vcard_editor_section_remove_item (section, ii);
			return;
		}
	}

	g_warn_if_reached ();
}

static EVCardEditorItem *
eve_add_item_cert (EVCardEditorSection *section,
		   EContactField field_id,
		   const gchar *field_label)
{
	return eve_add_item_cert_choose_file (section, field_id);
}

static void
eve_read_certs (EVCardEditorSection *section,
		EContact *contact)
{
	EVCard *vcard;
	GList *attrs, *link;

	e_vcard_editor_section_remove_dynamic (section);

	if (!contact)
		return;

	vcard = E_VCARD (contact);

	attrs = e_vcard_get_attributes_by_name (vcard, EVC_KEY);

	for (link = attrs; link; link = g_list_next (link)) {
		EVCardAttribute *attr = link->data;
		EVCardEditorItem *item;
		GtkWidget *widget;

		if (!attr)
			continue;

		item = eve_add_item_cert_to_section (section, NULL);
		widget = e_vcard_editor_item_get_data_widget (item, 0);
		e_vcard_editor_certificate_fill_widgets (E_VCARD_EDITOR_CERTIFICATE (widget), attr);
	}

	g_list_free (attrs);
}

static gboolean
eve_write_certs (EVCardEditorSection *section,
		 EContact *contact,
		 gchar **out_error_message,
		 GtkWidget **out_error_widget)
{
	EVCard *vcard;
	EVCardVersion to_version;
	GList *attr_list = NULL;
	guint ii, n_items;

	if (!e_vcard_editor_section_get_changed (section))
		return TRUE;

	vcard = E_VCARD (contact);
	to_version = e_vcard_get_version (vcard);

	e_vcard_remove_attributes (vcard, NULL, EVC_KEY);

	n_items = e_vcard_editor_section_get_n_items (section);

	for (ii = 0; ii < n_items; ii++) {
		EVCardEditorItem *item = e_vcard_editor_section_get_item (section, ii);
		EVCardAttribute *attr = NULL;
		GtkWidget *widget;

		widget = e_vcard_editor_item_get_data_widget (item, 0);

		if (!e_vcard_editor_certificate_fill_attr (E_VCARD_EDITOR_CERTIFICATE (widget), to_version, &attr, out_error_message, out_error_widget))
			return FALSE;

		if (attr)
			attr_list = g_list_prepend (attr_list, attr);
	}

	attr_list = g_list_reverse (attr_list);

	e_vcard_append_attributes_take (vcard, g_steal_pointer (&attr_list));

	return TRUE;
}

static void
eve_certs_section_changed_cb (EVCardEditorSection *section,
			      gpointer user_data)
{
	guint ii, n_items;

	n_items = e_vcard_editor_section_get_n_items (section);

	for (ii = 0; ii < n_items; ii++) {
		EVCardEditorItem *item;
		GtkWidget *widget;

		item = e_vcard_editor_section_get_item (section, ii);
		widget = e_vcard_editor_item_get_data_widget (item, 0);

		e_vcard_editor_certificate_set_add_button_visible (E_VCARD_EDITOR_CERTIFICATE (widget), ii + 1 == n_items);
	}
}

static EVCardEditorItem *
eve_add_item_notes (EVCardEditorSection *section,
		    EContactField field_id,
		    const gchar *field_label)
{
	EVCardEditorItem *item;
	GtkWidget *widget;

	widget = e_vcard_editor_notes_new ();

	item = e_vcard_editor_item_new (NULL, widget, TRUE,
		eve_read_item_simple, eve_write_item_simple);
	e_vcard_editor_item_set_field_id (item, field_id);
	e_vcard_editor_item_add_remove_button (item, section);

	gtk_widget_show_all (e_vcard_editor_item_get_data_box (item));

	g_signal_connect_object (widget, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);

	e_vcard_editor_section_take_item (section, item);

	return item;
}

/* order matches order in the dialog */
typedef enum {
	SECTION_KIND_SUBMENU_START = -3,
	SECTION_KIND_SUBMENU_END = -2,
	SECTION_KIND_SEPARATOR = -1,
	SECTION_KIND_IDENTITY,
	SECTION_KIND_IDENTITY_DETAILS,
	SECTION_KIND_ORGANIZATION,
	SECTION_KIND_JOB,
	SECTION_KIND_EMAIL,
	SECTION_KIND_PHONE,
	SECTION_KIND_SIP,
	SECTION_KIND_IM,
	SECTION_KIND_WEB,
	SECTION_KIND_ADDRESS,
	SECTION_KIND_LOCATION,
	SECTION_KIND_LIFE_EVENTS,
	SECTION_KIND_CERTS,
	SECTION_KIND_NOTES
} SectionKind;

typedef EVCardEditorItem * (* EVCardEditorAddItemFunc) (EVCardEditorSection *section,
							EContactField field_id,
							const gchar *field_label);
typedef void		(* EVCardEditorRemoveFieldFunc)	(EVCardEditor *editor,
							 EContact *contact,
							 EContactField field_id);
typedef gboolean	(* EVCardEditorCheckExistsFunc) (EVCardEditor *editor,
							 EContact *contact,
							 EContactField field_id);

static const struct _sections {
	SectionKind kind;
	const gchar *label;
	EVCardEditorSectionReadFunc fill_section_func;
	EVCardEditorSectionWriteFunc fill_contact_func;
	EVCardEditorAddItemFunc add_item_func;
} sections[] = {
	{ SECTION_KIND_IDENTITY,	N_("Identity"),		NULL, NULL, NULL },
	{ SECTION_KIND_IDENTITY_DETAILS,N_("Identity Details"),	NULL, NULL, NULL },
	{ SECTION_KIND_ORGANIZATION,	N_("Organization"),	NULL, NULL, NULL },
	{ SECTION_KIND_JOB,		N_("Job"),		NULL, NULL, NULL },
	{ SECTION_KIND_EMAIL,		N_("Email"),		eve_read_emails, eve_write_emails, eve_add_item_email },
	{ SECTION_KIND_PHONE,		N_("Phone"),		eve_read_phones, eve_write_phones, eve_add_item_phone },
	{ SECTION_KIND_SIP,		N_("SIP"),		eve_read_sips, eve_write_sips, eve_add_item_sip },
	{ SECTION_KIND_IM,		N_("Instant Messaging"), eve_read_ims, eve_write_ims, eve_add_item_im },
	{ SECTION_KIND_WEB,		N_("Web Address"),	eve_read_webs, eve_write_webs, NULL },
	{ SECTION_KIND_ADDRESS,		N_("Mailing Address"),	eve_read_addrs, eve_write_addrs, eve_add_item_addr },
	{ SECTION_KIND_LOCATION,	N_("Location"),		NULL, NULL, NULL },
	{ SECTION_KIND_LIFE_EVENTS,	N_("Life Events"),	NULL, NULL, NULL },
	{ SECTION_KIND_CERTS,		N_("Certificates"),	eve_read_certs, eve_write_certs, NULL },
	{ SECTION_KIND_NOTES,		N_("Notes"),		NULL, NULL, NULL }
};

static const struct _add_items {
	SectionKind kind;
	const gchar *label;
	EContactField field_id;
	guint cardinality;
	EVCardEditorAddItemFunc add_item_func;
	EVCardEditorCheckExistsFunc check_exists_func;
	EVCardEditorRemoveFieldFunc remove_field_func;
} add_items[] = {
	{ SECTION_KIND_SUBMENU_START, N_("_Identity"), E_CONTACT_FIELD_LAST, 0, NULL, NULL, NULL },
	{ SECTION_KIND_IDENTITY,	N_("Name _Prefix"),	FAKE_CONTACT_NAME_PREFIX, 1, eve_add_item_name, eve_check_exists_name, NULL },
	{ SECTION_KIND_IDENTITY,	N_("_First Name"),	FAKE_CONTACT_NAME_FIRST, 1, eve_add_item_name, eve_check_exists_name, NULL },
	{ SECTION_KIND_IDENTITY,	N_("_Middle Name"),	FAKE_CONTACT_NAME_MIDDLE, 1, eve_add_item_name, eve_check_exists_name, NULL },
	{ SECTION_KIND_IDENTITY,	N_("S_urname"),		FAKE_CONTACT_NAME_LAST, 1, eve_add_item_name, eve_check_exists_name, NULL },
	{ SECTION_KIND_IDENTITY,	N_("Name Suffi_x"),	FAKE_CONTACT_NAME_SUFFIX, 1, eve_add_item_name, eve_check_exists_name, NULL },
	{ SECTION_KIND_SUBMENU_END, NULL, E_CONTACT_FIELD_LAST, 0, NULL, NULL, NULL },

	{ SECTION_KIND_SUBMENU_START, N_("Identity _Details"), E_CONTACT_FIELD_LAST, 0, NULL, NULL, NULL },
	{ SECTION_KIND_SEPARATOR, NULL, E_CONTACT_FIELD_LAST, 0, NULL, NULL, NULL },
	{ SECTION_KIND_IDENTITY_DETAILS,N_("_Nickname"),	E_CONTACT_NICKNAME, 1, eve_add_item_simple, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_IDENTITY_DETAILS,N_("_Gender"),		E_CONTACT_GENDER, 1, eve_add_item_gender, eve_check_exists_gender, eve_remove_field_simple },
	{ SECTION_KIND_IDENTITY_DETAILS,N_("Sp_ouse"),		E_CONTACT_SPOUSE, 1, eve_add_item_simple, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_IDENTITY_DETAILS,N_("Ex_pertise"),	E_CONTACT_EXPERTISE, 1, eve_add_item_simple, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_IDENTITY_DETAILS,N_("Inte_rest"),	E_CONTACT_INTEREST, 1, eve_add_item_simple, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_IDENTITY_DETAILS,N_("Ho_bby"),		E_CONTACT_HOBBY, 1, eve_add_item_simple, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_SUBMENU_END, NULL, E_CONTACT_FIELD_LAST, 0, NULL, NULL, NULL },

	{ SECTION_KIND_SUBMENU_START, N_("_Organization"), E_CONTACT_FIELD_LAST, 0, NULL, NULL, NULL },
	{ SECTION_KIND_ORGANIZATION,	N_("_Company"),		E_CONTACT_ORG, 1, eve_add_item_simple, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_ORGANIZATION,	N_("_Department"),	E_CONTACT_ORG_UNIT, 1, eve_add_item_simple, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_ORGANIZATION,	N_("_Office"),		E_CONTACT_OFFICE, 1, eve_add_item_simple, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_ORGANIZATION,	N_("Or_g. Directory"),	E_CONTACT_ORG_DIRECTORY, 1, eve_add_item_simple, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_SUBMENU_END, NULL, E_CONTACT_FIELD_LAST, 0, NULL, NULL, NULL },

	{ SECTION_KIND_SUBMENU_START, N_("_Job"), E_CONTACT_FIELD_LAST, 0, NULL, NULL, NULL },
	{ SECTION_KIND_JOB,		N_("_Title"),		E_CONTACT_TITLE, 1, eve_add_item_simple, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_JOB,		N_("_Profession"),	E_CONTACT_ROLE, 1, eve_add_item_simple, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_JOB,		N_("_Manager"),		E_CONTACT_MANAGER, 1, eve_add_item_simple, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_JOB,		N_("_Assistant"),	E_CONTACT_ASSISTANT, 1, eve_add_item_simple, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_SUBMENU_END, NULL, E_CONTACT_FIELD_LAST, 0, NULL, NULL, NULL },

	{ SECTION_KIND_EMAIL,		N_("_Email"),		E_CONTACT_EMAIL, 0, eve_add_item_email, NULL, NULL },
	{ SECTION_KIND_PHONE,		N_("_Phone"),		E_CONTACT_TEL, 0, eve_add_item_phone, NULL, NULL },
	{ SECTION_KIND_SIP,		N_("_SIP"),		E_CONTACT_SIP, 0, eve_add_item_sip, NULL, NULL },
	{ SECTION_KIND_IM,		N_("Inst_ant Messaging"), E_CONTACT_IMPP, 0, eve_add_item_im, NULL, NULL },
	{ SECTION_KIND_ADDRESS,		N_("_Mailing Address"),	E_CONTACT_ADDRESS, 0, eve_add_item_addr, NULL, NULL },

	{ SECTION_KIND_SUBMENU_START, N_("_Web Address"), E_CONTACT_FIELD_LAST, 0, NULL, NULL, NULL },
	{ SECTION_KIND_WEB,		N_("_Blog"),		E_CONTACT_BLOG_URL, 0, eve_add_item_web, NULL, NULL },
	{ SECTION_KIND_WEB,		N_("_Calendar"),	E_CONTACT_CALENDAR_URI, 0, eve_add_item_web, NULL, NULL },
	{ SECTION_KIND_WEB,		N_("_Free/Busy"),	E_CONTACT_FREEBUSY_URL, 0, eve_add_item_web, NULL, NULL },
	{ SECTION_KIND_WEB,		N_("_Video Chat"),	E_CONTACT_VIDEO_URL, 0, eve_add_item_web, NULL, NULL },
	{ SECTION_KIND_WEB,		N_("_URL"),		E_CONTACT_HOMEPAGE_URL, 0, eve_add_item_web, NULL, NULL },
	{ SECTION_KIND_SUBMENU_END, NULL, E_CONTACT_FIELD_LAST, 0, NULL, NULL, NULL },

	{ SECTION_KIND_SUBMENU_START, N_("_Location"), E_CONTACT_FIELD_LAST, 0, NULL, NULL, NULL },
	{ SECTION_KIND_LOCATION,	N_("_Geo"),		E_CONTACT_GEO, 1, eve_add_item_geo, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_LOCATION,	N_("_Time zone"),	E_CONTACT_TZ, 1, eve_add_item_tz, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_SUBMENU_END, NULL, E_CONTACT_FIELD_LAST, 0, NULL, NULL, NULL },

	{ SECTION_KIND_SUBMENU_START, N_("Li_fe Events"), E_CONTACT_FIELD_LAST, 0, NULL, NULL, NULL },
	{ SECTION_KIND_LIFE_EVENTS,	N_("_Anniversary"),	E_CONTACT_ANNIVERSARY, 1, eve_add_item_date, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_LIFE_EVENTS,	N_("_Birthday"),	E_CONTACT_BIRTH_DATE, 1, eve_add_item_date, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_LIFE_EVENTS,	N_("_Date of death"),	E_CONTACT_DEATHDATE, 1, eve_add_item_date, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_SEPARATOR, NULL, E_CONTACT_FIELD_LAST, 0, NULL, NULL, NULL },
	{ SECTION_KIND_LIFE_EVENTS,	N_("Birth _place"),	E_CONTACT_BIRTHPLACE, 1, eve_add_item_simple, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_LIFE_EVENTS,	N_("Death _place"),	E_CONTACT_DEATHPLACE, 1, eve_add_item_simple, eve_check_exists_simple, eve_remove_field_simple },
	{ SECTION_KIND_SUBMENU_END, NULL, E_CONTACT_FIELD_LAST, 0, NULL, NULL, NULL },

	{ SECTION_KIND_CERTS,		N_("_X.509 certificate"), E_CONTACT_X509_CERT, 0, eve_add_item_cert, NULL, NULL },
	{ SECTION_KIND_CERTS,		N_("P_GP key"),		E_CONTACT_PGP_CERT, 0, eve_add_item_cert, NULL, NULL },
	{ SECTION_KIND_NOTES,		N_("_Notes"),		E_CONTACT_NOTE, 1, eve_add_item_notes, eve_check_exists_simple, eve_remove_field_simple }
};

static gint
eve_sort_by_add_items (SectionKind kind,
		       gconstpointer ptr1,
		       gconstpointer ptr2)
{
	EVCardEditorItem *item1 = (EVCardEditorItem *) ptr1;
	EVCardEditorItem *item2 = (EVCardEditorItem *) ptr2;
	EContactField field_id1 = e_vcard_editor_item_get_field_id (item1);
	EContactField field_id2 = e_vcard_editor_item_get_field_id (item2);
	gint index1 = -1, index2 = -1;
	guint ii;

	for (ii = 0; ii < G_N_ELEMENTS (add_items); ii++) {
		if (add_items[ii].kind == kind)
			break;
	}

	for (; ii < G_N_ELEMENTS (add_items) && (index1 == -1 || index2 == -1); ii++) {
		if (add_items[ii].kind != kind && add_items[ii].kind != SECTION_KIND_SEPARATOR)
			break;

		if (index1 == -1 && add_items[ii].field_id == field_id1)
			index1 = (gint) ii;
		if (index2 == -1 && add_items[ii].field_id == field_id2)
			index2 = (gint) ii;
	}

	return index1 - index2;
}

static gint
eve_sort_identity_section_cb (gconstpointer ptr1,
			      gconstpointer ptr2)
{
	return eve_sort_by_add_items (SECTION_KIND_IDENTITY, ptr1, ptr2);
}

static gint
eve_sort_identity_details_section_cb (gconstpointer ptr1,
				      gconstpointer ptr2)
{
	return eve_sort_by_add_items (SECTION_KIND_IDENTITY_DETAILS, ptr1, ptr2);
}

static gint
eve_sort_life_events_section_cb (gconstpointer ptr1,
				 gconstpointer ptr2)
{
	return eve_sort_by_add_items (SECTION_KIND_LIFE_EVENTS, ptr1, ptr2);
}

static gint
eve_sort_organization_section_cb (gconstpointer ptr1,
				  gconstpointer ptr2)
{
	return eve_sort_by_add_items (SECTION_KIND_ORGANIZATION, ptr1, ptr2);
}

static gint
eve_sort_job_section_cb (gconstpointer ptr1,
			 gconstpointer ptr2)
{
	return eve_sort_by_add_items (SECTION_KIND_JOB, ptr1, ptr2);
}

static gint
eve_sort_web_section_cb (gconstpointer ptr1,
			 gconstpointer ptr2)
{
	return eve_sort_by_add_items (SECTION_KIND_WEB, ptr1, ptr2);
}

static gint
eve_sort_location_section_cb (gconstpointer ptr1,
			      gconstpointer ptr2)
{
	return eve_sort_by_add_items (SECTION_KIND_LOCATION, ptr1, ptr2);
}

static void
eve_item_logo_photo_file_chooser_response_cb (GtkFileChooserNative *file_chooser,
					      gint response,
					      gpointer user_data)
{
	EImageChooser *image_chooser = user_data;

	if (response == GTK_RESPONSE_ACCEPT) {
		gchar *file_name;

		file_name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_chooser));

		if (file_name)
			e_image_chooser_set_from_file (image_chooser, file_name);

		g_free (file_name);
	}

	g_clear_object (&file_chooser);
}

static void
eve_item_logo_photo_clicked_cb (GtkButton *button,
				gpointer user_data)
{
	EVCardEditor *self = user_data;
	GtkFileChooserNative *file_chooser;
	GtkFileFilter *filter;

	file_chooser = gtk_file_chooser_native_new (
		_("Please select an image for this contact"),
		GTK_WINDOW (self),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Open"), _("_Cancel"));

	filter = gtk_file_filter_new ();
	gtk_file_filter_add_mime_type (filter, "image/*");
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (file_chooser), filter);

	g_signal_connect_object (file_chooser, "response",
		G_CALLBACK (eve_item_logo_photo_file_chooser_response_cb), gtk_bin_get_child (GTK_BIN (button)), 0);

	gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (file_chooser), TRUE);
	gtk_native_dialog_show (GTK_NATIVE_DIALOG (file_chooser));
}

static void
eve_item_logo_photo_trash_clicked_cb (GtkButton *button,
				      gpointer user_data)
{
	EImageChooser *chooser = user_data;

	g_return_if_fail (E_IS_IMAGE_CHOOSER (chooser));

	e_image_chooser_unset_image (chooser);
}

static void
eve_fill_item_logo_photo (EVCardEditorItem *item,
			  EVCardEditor *editor,
			  EContact *contact)
{
	EContactField field_id = e_vcard_editor_get_contact_kind (editor) == E_VCARD_EDITOR_CONTACT_KIND_ORG ?
		E_CONTACT_LOGO : E_CONTACT_PHOTO;
	EContactPhoto *photo;
	EImageChooser *chooser;
	GtkWidget *widget;
	gboolean image_set = FALSE;

	widget = e_vcard_editor_item_get_data_widget (item, 0);
	g_warn_if_fail (widget != NULL);

	chooser = E_IMAGE_CHOOSER (gtk_bin_get_child (GTK_BIN (widget)));

	photo = e_contact_get (contact, field_id);
	if (!photo)
		photo = e_contact_get (contact, field_id == E_CONTACT_PHOTO ? E_CONTACT_LOGO : E_CONTACT_PHOTO);

	if (photo && photo->type == E_CONTACT_PHOTO_TYPE_INLINED) {
		e_image_chooser_set_image_data (chooser,
			(gchar *) photo->data.inlined.data,
			photo->data.inlined.length);
		image_set = TRUE;
	} else if (photo && photo->type == E_CONTACT_PHOTO_TYPE_URI) {
		gchar *file_name = g_filename_from_uri (photo->data.uri, NULL, NULL);
		if (file_name) {
			e_image_chooser_set_from_file (chooser, file_name);
			image_set = TRUE;
			g_free (file_name);
		}
	}

	e_contact_photo_free (photo);

	if (!image_set)
		e_image_chooser_unset_image (chooser);
}

static gboolean
eve_fill_contact_logo_photo (EVCardEditorItem *item,
			     EVCardEditor *editor,
			     EContact *contact,
			     gchar **out_error_message,
			     GtkWidget **out_error_widget)
{
	EContactField field_id = e_vcard_editor_get_contact_kind (editor) == E_VCARD_EDITOR_CONTACT_KIND_ORG ?
		E_CONTACT_LOGO : E_CONTACT_PHOTO;
	EContactPhoto photo;
	EImageChooser *chooser;
	GtkWidget *widget;
	gchar *img_buff = NULL;

	widget = e_vcard_editor_item_get_data_widget (item, 0);
	g_warn_if_fail (widget != NULL);

	chooser = E_IMAGE_CHOOSER (gtk_bin_get_child (GTK_BIN (widget)));

	photo.type = E_CONTACT_PHOTO_TYPE_INLINED;
	photo.data.inlined.mime_type = NULL;

	/* unset previous value, like if the kind changed, to not have two photo & logo in the contact */
	e_contact_set (contact, field_id == E_CONTACT_LOGO ? E_CONTACT_PHOTO : E_CONTACT_LOGO, NULL);

	if (e_image_chooser_has_image (chooser) &&
	    e_image_chooser_get_image_data (chooser, &img_buff, &photo.data.inlined.length)) {
		GdkPixbuf *pixbuf, *scaled;
		GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();

		photo.data.inlined.data = (guchar *) g_steal_pointer (&img_buff);

		gdk_pixbuf_loader_write (loader, photo.data.inlined.data, photo.data.inlined.length, NULL);
		gdk_pixbuf_loader_close (loader, NULL);

		pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
		if (pixbuf) {
			gint width, height;

			g_object_ref (pixbuf);

			height = gdk_pixbuf_get_height (pixbuf);
			width = gdk_pixbuf_get_width (pixbuf);

			if (height > MAX_PHOTO_SIZE_PX || width > MAX_PHOTO_SIZE_PX) {
				gint prompt_response;

				prompt_response = e_alert_run_dialog_for_args (GTK_WINDOW (editor), "addressbook:prompt-resize", NULL);

				if (prompt_response == GTK_RESPONSE_YES) {
					if (width > height) {
						height = height * MAX_PHOTO_SIZE_PX / width;
						width = MAX_PHOTO_SIZE_PX;
					} else {
						width = width * MAX_PHOTO_SIZE_PX / height;
						height = MAX_PHOTO_SIZE_PX;
					}

					scaled = e_icon_factory_pixbuf_scale (pixbuf, width, height);
					if (scaled) {
						GdkPixbufFormat *format = gdk_pixbuf_loader_get_format (loader);
						gchar *format_name = gdk_pixbuf_format_get_name (format);

						g_free (photo.data.inlined.data);
						gdk_pixbuf_save_to_buffer (scaled, &img_buff, &photo.data.inlined.length, format_name, NULL, NULL);
						photo.data.inlined.data = (guchar *) g_steal_pointer (&img_buff);

						g_free (format_name);
						g_object_unref (scaled);
					}
				} else if (prompt_response == GTK_RESPONSE_CANCEL) {
					g_object_unref (pixbuf);
					g_object_unref (loader);

					return FALSE;
				}
			}

			g_object_unref (pixbuf);
		}

		e_contact_set (contact, field_id, &photo);

		g_object_unref (loader);
		g_free (photo.data.inlined.data);
	} else {
		e_contact_set (contact, field_id, NULL);
	}

	return TRUE;
}

static EVCardEditorItem *
eve_new_item_logo_photo (EVCardEditor *self)
{
	GtkWidget *chooser, *button, *trash;
	EVCardEditorItem *item;

	chooser = g_object_new (E_TYPE_IMAGE_CHOOSER,
		"pixel-size", 128,
		"icon-name", "avatar-default",
		NULL);

	g_object_set (chooser,
		"visible", TRUE,
		"halign", GTK_ALIGN_CENTER,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	button = gtk_button_new ();
	g_object_set (button,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_START,
		"width-request", 148,
		"height-request", 148,
		NULL);
	gtk_style_context_add_class (gtk_widget_get_style_context (button), "flat");
	gtk_container_add (GTK_CONTAINER (button), chooser);

	trash = gtk_button_new_from_icon_name ("user-trash", GTK_ICON_SIZE_SMALL_TOOLBAR);
	g_object_set (trash,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_END,
		"margin-end", 8, /* to have a gap between the trash icon and the item on the right */
		"tooltip-text", _("Remove image"),
		NULL);
	gtk_style_context_add_class (gtk_widget_get_style_context (trash), "flat");

	item = e_vcard_editor_item_new (NULL, button, FALSE, eve_fill_item_logo_photo, eve_fill_contact_logo_photo);
	e_vcard_editor_item_add_data_widget (item, trash, FALSE);

	g_signal_connect_object (chooser, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);
	g_signal_connect_object (button, "clicked",
		G_CALLBACK (eve_item_logo_photo_clicked_cb), self, 0);
	g_signal_connect_object (trash, "clicked",
		G_CALLBACK (eve_item_logo_photo_trash_clicked_cb), chooser, 0);

	e_binding_bind_property (chooser, "has-image",
		trash, "sensitive",
		G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	return item;
}

static EVCardEditorContactKind
eve_get_contact_kind_from_item (EVCardEditorItem *item,
				const gchar **out_as_string)
{
	GtkWidget *widget;
	GtkComboBox *combo;
	EVCardEditorContactKind kind;
	gint active;

	widget = e_vcard_editor_item_get_data_widget (item, 0);
	g_warn_if_fail (widget != NULL);

	combo = GTK_COMBO_BOX (widget);
	active = gtk_combo_box_get_active (combo);

	if (active >= 0 && active < 4)
		kind = 1 << active;
	else
		kind = E_VCARD_EDITOR_CONTACT_KIND_CUSTOM;

	if (out_as_string) {
		if (active >= 0)
			*out_as_string = gtk_combo_box_get_active_id (combo);
		else
			*out_as_string = NULL;
	}

	return kind;
}

static void
eve_fill_item_kind (EVCardEditorItem *item,
		    EVCardEditor *editor,
		    EContact *contact)
{
	GtkWidget *widget;
	GtkComboBox *combo;
	GtkComboBoxText *text_combo;
	GtkTreeModel *model;
	gchar *kind_str;

	widget = e_vcard_editor_item_get_data_widget (item, 0);
	g_warn_if_fail (widget != NULL);

	combo = GTK_COMBO_BOX (widget);
	text_combo = GTK_COMBO_BOX_TEXT (combo);
	model = gtk_combo_box_get_model (combo);

	while (gtk_tree_model_iter_n_children (model, NULL) > 4) {
		gtk_combo_box_text_remove (text_combo, 4);
	}

	kind_str = e_contact_get (contact, E_CONTACT_KIND);

	if (kind_str && *kind_str) {
		/* vCard stores case insensitive values */
		struct _values {
			const gchar *value;
			EVCardEditorContactKind kind;
		} values[] = {
			{ "individual", E_VCARD_EDITOR_CONTACT_KIND_INDIVIDUAL },
			{ "group", E_VCARD_EDITOR_CONTACT_KIND_GROUP },
			{ "org", E_VCARD_EDITOR_CONTACT_KIND_ORG },
			{ "location", E_VCARD_EDITOR_CONTACT_KIND_LOCATION }
		};
		guint ii;

		for (ii = 0; ii < G_N_ELEMENTS (values); ii++) {
			if (g_ascii_strcasecmp (values[ii].value, kind_str) == 0) {
				gtk_combo_box_set_active_id (combo, values[ii].value);
				editor->contact_kind = values[ii].kind;
				break;
			}
		}

		if (ii >= G_N_ELEMENTS (values)) {
			gtk_combo_box_text_append (text_combo, kind_str, kind_str);
			editor->contact_kind = E_VCARD_EDITOR_CONTACT_KIND_CUSTOM;
		}
	} else {
		gtk_combo_box_set_active_id (combo, "individual");
		editor->contact_kind = E_VCARD_EDITOR_CONTACT_KIND_INDIVIDUAL;
	}

	g_free (kind_str);
}

static gboolean
eve_fill_contact_kind (EVCardEditorItem *item,
		       EVCardEditor *editor,
		       EContact *contact,
		       gchar **out_error_message,
		       GtkWidget **out_error_widget)
{
	const gchar *kind_str = NULL;

	eve_get_contact_kind_from_item (item, &kind_str);

	e_contact_set (contact, E_CONTACT_KIND, kind_str);

	return TRUE;
}

static EVCardEditorItem *
eve_new_item_kind (EVCardEditor *self)
{
	GtkWidget *widget;
	GtkComboBoxText *text_combo;
	EVCardEditorItem *item;

	widget = gtk_combo_box_text_new ();
	gtk_widget_set_halign (widget, GTK_ALIGN_START);

	text_combo = GTK_COMBO_BOX_TEXT (widget);
	gtk_combo_box_text_append (text_combo, "individual", _("Individual"));
	/* can be enabled if/when the editor will cover also the groups/lists */
	/* gtk_combo_box_text_append (text_combo, "group", _("Group")); */
	gtk_combo_box_text_append (text_combo, "org", _("Organization"));
	gtk_combo_box_text_append (text_combo, "location", _("Location"));

	/* Translators: it is a contact kind, like "individual", "group", "organization", "location" and "custom" */
	item = e_vcard_editor_item_new (gtk_label_new_with_mnemonic (C_("Contact", "_Kind:")), widget, TRUE,
		eve_fill_item_kind, eve_fill_contact_kind);
	e_vcard_editor_item_set_field_id (item, E_CONTACT_KIND);

	g_signal_connect_object (text_combo, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);

	return item;
}

static void
eve_address_book_opened_cb (GObject *source_object,
			    GAsyncResult *result,
			    gpointer user_data)
{
	EVCardEditor *self = user_data;
	EClient *client;
	GError *local_error = NULL;

	client = e_client_combo_box_get_client_finish (E_CLIENT_COMBO_BOX (source_object), result, &local_error);

	if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_clear_error (&local_error);
		return;
	}

	if (self->opening_client)
		g_clear_object (&self->opening_client);

	if (client) {
		e_vcard_editor_set_target_client (self, E_BOOK_CLIENT (client));
		g_object_unref (client);
	} else if (local_error) {
		eve_report_error (self, local_error->message, NULL);
	}

	g_clear_error (&local_error);
}

static void
eve_address_book_changed_cb (EClientComboBox *client_combo,
			     gpointer user_data)
{
	EVCardEditor *self = user_data;
	EClient *client;
	ESource *selected;

	if (self->opening_client) {
		g_cancellable_cancel (self->opening_client);
		g_clear_object (&self->opening_client);
	}

	selected = e_source_combo_box_ref_active (E_SOURCE_COMBO_BOX (client_combo));
	if (!selected)
		return;

	if ((self->target_client && e_source_equal (selected, e_client_get_source (E_CLIENT (self->target_client)))) ||
	    (!self->target_client && e_source_equal (selected, e_client_get_source (E_CLIENT (self->source_client))))) {
		g_object_unref (selected);
		return;
	}

	client = e_client_combo_box_ref_cached_client (client_combo, selected);
	if (client) {
		e_vcard_editor_set_target_client (self, E_BOOK_CLIENT (client));
		g_object_unref (client);
	} else {
		self->opening_client = g_cancellable_new ();

		e_client_combo_box_get_client (client_combo, selected, self->opening_client,
			eve_address_book_opened_cb, self);
	}

	g_object_unref (selected);
}

static EVCardEditorItem *
eve_new_item_address_book (EVCardEditor *self)
{
	EVCardEditorItem *item;
	GtkWidget *widget;

	widget = e_client_combo_box_new (self->client_cache, E_SOURCE_EXTENSION_ADDRESS_BOOK);
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"show-colors", FALSE,
		"max-natural-width", 100,
		NULL);

	if (self->source_client)
		e_source_combo_box_set_active (E_SOURCE_COMBO_BOX (widget), e_client_get_source (E_CLIENT (self->source_client)));

	item = e_vcard_editor_item_new (gtk_label_new_with_mnemonic (_("Address _book:")), widget, TRUE, NULL, NULL);

	g_signal_connect_object (widget, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);

	g_signal_connect_object (widget, "changed",
		G_CALLBACK (eve_address_book_changed_cb), self, 0);

	self->client_combo = widget;

	return item;
}

static void
eve_file_under_entry_changed_cb (EVCardEditor *self,
				 GtkEntry *file_under_entry)
{
	const gchar *text;

	text = gtk_entry_get_text (file_under_entry);

	if (text && *text) {
		gchar *str;

		str = g_strdup_printf (_("Contact Editor — %s"), text);
		gtk_window_set_title (GTK_WINDOW (self), str);
		g_free (str);
	} else {
		gtk_window_set_title (GTK_WINDOW (self), _("Contact Editor"));
	}
}

static void
eve_fill_item_file_under (EVCardEditorItem *item,
			  EVCardEditor *editor,
			  EContact *contact)
{
	GtkWidget *widget;
	GtkEntry *entry;
	gchar *value;

	widget = e_vcard_editor_item_get_data_widget (item, 0);
	g_warn_if_fail (widget != NULL);

	entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget)));

	value = e_contact_get (contact, E_CONTACT_FILE_AS);

	gtk_entry_set_text (entry, value ? value : "");

	g_free (value);
}

static gboolean
eve_fill_contact_file_under (EVCardEditorItem *item,
			     EVCardEditor *editor,
			     EContact *contact,
			     gchar **out_error_message,
			     GtkWidget **out_error_widget)
{
	GtkWidget *widget;
	GtkEntry *entry;
	const gchar *value;

	widget = e_vcard_editor_item_get_data_widget (item, 0);
	g_warn_if_fail (widget != NULL);

	entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget)));
	value = gtk_entry_get_text (entry);

	e_contact_set (contact, E_CONTACT_FILE_AS, value);

	return TRUE;
}

static EVCardEditorItem *
eve_new_item_file_under (EVCardEditor *self)
{
	EVCardEditorItem *item;
	GtkWidget *widget;

	widget = gtk_combo_box_text_new_with_entry ();

	self->file_under_combo = GTK_COMBO_BOX_TEXT (widget);

	item = e_vcard_editor_item_new (gtk_label_new_with_mnemonic (_("_File under:")), widget, TRUE,
		eve_fill_item_file_under, eve_fill_contact_file_under);
	e_vcard_editor_item_set_field_id (item, E_CONTACT_FILE_AS);
	e_vcard_editor_item_set_permanent (item, TRUE);

	g_signal_connect_object (widget, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);

	g_signal_connect_object (gtk_bin_get_child (GTK_BIN (widget)), "changed",
		G_CALLBACK (eve_file_under_entry_changed_cb), self, G_CONNECT_SWAPPED);

	widget = e_vcard_editor_item_get_label_widget (item);
	gtk_widget_set_halign (widget, GTK_ALIGN_END);

	widget = e_vcard_editor_item_get_data_box (item);
	gtk_widget_show_all (widget);

	return item;
}

static void
eve_item_categories_button_clicked_cb (GtkWidget *button,
				       gpointer user_data)
{
	EVCardEditorItem *item = user_data;
	GtkWidget *dialog, *widget;
	GtkEntry *entry;
	GtkWindow *window;

	entry = GTK_ENTRY (e_vcard_editor_item_get_data_widget (item, 0));
	dialog = e_categories_dialog_new (gtk_entry_get_text (entry));

	widget = gtk_widget_get_ancestor (button, GTK_TYPE_WINDOW);

	window = GTK_WINDOW (dialog);

	gtk_window_set_destroy_with_parent (window, TRUE);
	gtk_window_set_modal (window, TRUE);

	if (GTK_IS_WINDOW (widget))
		gtk_window_set_transient_for (window, GTK_WINDOW (widget));

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		gchar *categories;

		categories = e_categories_dialog_get_categories (E_CATEGORIES_DIALOG (dialog));

		gtk_entry_set_text (entry, categories);

		g_free (categories);
	}

	gtk_widget_destroy (dialog);
}

static void
eve_fill_item_categories (EVCardEditorItem *item,
			  EVCardEditor *editor,
			  EContact *contact)
{
	GtkWidget *widget;
	GtkEntry *entry;
	gchar *value;

	widget = e_vcard_editor_item_get_data_widget (item, 0);
	g_warn_if_fail (widget != NULL);

	entry = GTK_ENTRY (widget);

	value = e_contact_get (contact, E_CONTACT_CATEGORIES);

	gtk_entry_set_text (entry, value ? value : "");

	g_free (value);
}

static gboolean
eve_fill_contact_categories (EVCardEditorItem *item,
			     EVCardEditor *editor,
			     EContact *contact,
			     gchar **out_error_message,
			     GtkWidget **out_error_widget)
{
	GtkWidget *widget;
	GtkEntry *entry;
	const gchar *value;

	widget = e_vcard_editor_item_get_data_widget (item, 0);
	g_warn_if_fail (widget != NULL);

	entry = GTK_ENTRY (widget);
	value = gtk_entry_get_text (entry);

	e_contact_set (contact, E_CONTACT_CATEGORIES, value);

	return TRUE;
}

static EVCardEditorItem *
eve_new_item_categories (EVCardEditor *self)
{
	EVCardEditorItem *item;
	GtkWidget *button, *entry;
	GtkEntryCompletion *completion;

	button = gtk_button_new_with_mnemonic (_("Ca_tegories…"));
	entry = gtk_entry_new ();

	completion = e_category_completion_new ();
	gtk_entry_set_completion (GTK_ENTRY (entry), completion);
	g_object_unref (completion);

	g_object_set (entry,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		NULL);

	item = e_vcard_editor_item_new (button, entry, TRUE,
		eve_fill_item_categories, eve_fill_contact_categories);

	g_signal_connect_object (entry, "changed",
		G_CALLBACK (e_vcard_editor_item_emit_changed), item, G_CONNECT_SWAPPED);

	g_signal_connect_object (button, "clicked",
		G_CALLBACK (eve_item_categories_button_clicked_cb), item, 0);

	return item;
}

static void
eve_update (EVCardEditor *self)
{
	if (self->button_save)
		gtk_widget_set_sensitive (self->button_save, self->changed);
}

#define ADD_MENU_ITEM_INDEX "eve-add-menu-item-index"

static void
eve_add_menu_item_activated_cb (GtkMenuItem *menu_item,
				gpointer user_data)
{
	EVCardEditor *self = user_data;
	EVCardEditorSection *section;
	EVCardEditorItem *item;
	guint item_index = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (menu_item), ADD_MENU_ITEM_INDEX));

	g_return_if_fail (item_index >= 1 && item_index <= G_N_ELEMENTS (add_items));

	if (!self->sections)
		return;

	section = g_hash_table_lookup (self->sections, GINT_TO_POINTER (add_items[item_index - 1].kind));
	g_return_if_fail (section != NULL);

	item = add_items[item_index - 1].add_item_func (section, add_items[item_index - 1].field_id, _(add_items[item_index - 1].label));
	eve_focus_added_item (item);
}

static void
eve_update_add_menu (EVCardEditor *self)
{
	GtkWidget *menu;
	GtkMenuShell *menu_shell;
	GtkMenuShell *submenu_shell = NULL;
	GHashTable *unused_onces;
	const gchar *need_submenu = NULL;
	gboolean has_supported_fields;
	guint ii;

	self->add_menu_needs_update = FALSE;

	if (!self->add_menu_button)
		return;

	has_supported_fields = self->supported_fields && g_hash_table_size (self->supported_fields) > 0;

	menu = gtk_menu_new ();
	menu_shell = GTK_MENU_SHELL (menu);

	unused_onces = g_hash_table_new (g_direct_hash, g_direct_equal);

	for (ii = 0; ii < G_N_ELEMENTS (add_items); ii++) {
		if (add_items[ii].cardinality == 1 && add_items[ii].field_id != E_CONTACT_FIELD_LAST) {
			gboolean used = FALSE;

			if (self->sections) {
				EVCardEditorSection *section;
				guint n_items, jj;

				section = g_hash_table_lookup (self->sections, GINT_TO_POINTER (add_items[ii].kind));
				n_items = e_vcard_editor_section_get_n_items (section);

				for (jj = 0; jj < n_items && !used; jj++) {
					EVCardEditorItem *item = e_vcard_editor_section_get_item (section, jj);

					used = e_vcard_editor_item_get_field_id (item) == add_items[ii].field_id;
				}
			}

			if (!used)
				g_hash_table_add (unused_onces, GINT_TO_POINTER (add_items[ii].field_id));
		}
	}

	for (ii = 0; ii < G_N_ELEMENTS (add_items); ii++) {
		GtkWidget *menu_item;

		if (add_items[ii].cardinality == 1 && add_items[ii].field_id != E_CONTACT_FIELD_LAST &&
		    !g_hash_table_contains (unused_onces, GINT_TO_POINTER (add_items[ii].field_id)))
			continue;

		if (add_items[ii].kind == SECTION_KIND_SEPARATOR) {
			if (need_submenu)
				continue;

			menu_item = gtk_separator_menu_item_new ();
		} else if (add_items[ii].kind == SECTION_KIND_SUBMENU_START) {
			g_warn_if_fail (submenu_shell == NULL && need_submenu == NULL);
			need_submenu = _(add_items[ii].label);
			continue;
		} else if (add_items[ii].kind == SECTION_KIND_SUBMENU_END) {
			g_warn_if_fail (submenu_shell != NULL || need_submenu != NULL);
			submenu_shell = NULL;
			need_submenu = NULL;
			continue;
		} else {
			menu_item = gtk_menu_item_new_with_mnemonic (_(add_items[ii].label));
			g_object_set_data (G_OBJECT (menu_item), ADD_MENU_ITEM_INDEX, GUINT_TO_POINTER (ii + 1));
			g_signal_connect (menu_item, "activate",
				G_CALLBACK (eve_add_menu_item_activated_cb), self);

			if (add_items[ii].field_id != E_CONTACT_FIELD_LAST && has_supported_fields)
				gtk_widget_set_sensitive (menu_item, eve_is_supported_field (self, add_items[ii].field_id));
		}

		if (need_submenu) {
			GtkWidget *submenu, *subitem;

			g_warn_if_fail (submenu_shell == NULL);

			submenu = gtk_menu_new ();
			subitem = gtk_menu_item_new_with_mnemonic (need_submenu);
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (subitem), submenu);
			submenu_shell = GTK_MENU_SHELL (submenu);
			gtk_menu_shell_append (menu_shell, subitem);

			need_submenu = NULL;
		}

		gtk_menu_shell_append (submenu_shell ? submenu_shell : menu_shell, menu_item);
	}

	gtk_widget_show_all (menu);

	gtk_menu_button_set_popup (GTK_MENU_BUTTON (self->add_menu_button), menu);

	g_hash_table_destroy (unused_onces);
}

static void
eve_contact_kind_changed_cb (EVCardEditorItem *item,
			     EVCardEditor *self)
{
	EVCardEditorContactKind new_kind;

	new_kind = eve_get_contact_kind_from_item (item, NULL);

	if (self->contact_kind != new_kind) {
		self->contact_kind = new_kind;

		eve_update_add_menu (self);
		eve_update (self);
	}
}

static void
eve_fill_widgets (EVCardEditor *self)
{
	guint ii;

	self->updating = TRUE;

	if (self->items) {
		for (ii = 0; ii < self->items->len; ii++) {
			EVCardEditorItem *item = g_ptr_array_index (self->items, ii);

			e_vcard_editor_item_fill_item (item, self, self->contact);
		}
	}

	if (self->sections && g_hash_table_size (self->sections) > 0) {
		EVCardEditorSection *section;
		GHashTableIter iter;
		gpointer value = NULL;

		g_hash_table_iter_init (&iter, self->sections);
		while (g_hash_table_iter_next (&iter, NULL, &value)) {
			section = value;

			e_vcard_editor_section_remove_dynamic (section);
			e_vcard_editor_section_fill_section (section, self->contact);
		}

		for (ii = 0; ii < G_N_ELEMENTS (add_items); ii++) {
			if (add_items[ii].add_item_func != NULL &&
			    add_items[ii].check_exists_func != NULL &&
			    add_items[ii].check_exists_func (self, self->contact, add_items[ii].field_id)) {
				EVCardEditorItem *item;

				section = g_hash_table_lookup (self->sections, GINT_TO_POINTER (add_items[ii].kind));
				g_warn_if_fail (section != NULL);

				if (!section)
					continue;

				item = add_items[ii].add_item_func (section, add_items[ii].field_id, _(add_items[ii].label));
				e_vcard_editor_item_fill_item (item, self, self->contact);
			}
		}

		if ((self->flags & E_VCARD_EDITOR_FLAG_IS_NEW) != 0) {
			if (self->contact_kind == E_VCARD_EDITOR_CONTACT_KIND_ORG) {
				section = g_hash_table_lookup (self->sections, GINT_TO_POINTER (SECTION_KIND_ORGANIZATION));
				if (!gtk_widget_get_visible (GTK_WIDGET (section))) {
					eve_add_item_simple (section, E_CONTACT_ORG, _("_Company"));
					eve_add_item_simple (section, E_CONTACT_OFFICE, _("_Office"));
				}
			}

			if (self->contact_kind == E_VCARD_EDITOR_CONTACT_KIND_INDIVIDUAL ||
			    self->contact_kind == E_VCARD_EDITOR_CONTACT_KIND_ORG ||
			    self->contact_kind == E_VCARD_EDITOR_CONTACT_KIND_GROUP) {
				section = g_hash_table_lookup (self->sections, GINT_TO_POINTER (SECTION_KIND_EMAIL));
				if (!gtk_widget_get_visible (GTK_WIDGET (section)))
					eve_add_item_email (section, E_CONTACT_EMAIL, NULL);
			}

			if (self->contact_kind == E_VCARD_EDITOR_CONTACT_KIND_INDIVIDUAL ||
			    self->contact_kind == E_VCARD_EDITOR_CONTACT_KIND_ORG) {
				section = g_hash_table_lookup (self->sections, GINT_TO_POINTER (SECTION_KIND_PHONE));
				if (!gtk_widget_get_visible (GTK_WIDGET (section)))
					eve_add_item_phone (section, E_CONTACT_TEL, NULL);
			}
		}

		g_hash_table_iter_init (&iter, self->sections);
		while (g_hash_table_iter_next (&iter, NULL, &value)) {
			section = value;

			e_vcard_editor_section_set_changed (section, FALSE);
		}
	}

	self->updating = FALSE;

	eve_update_file_under_choices (self);
	eve_update_add_menu (self);
	eve_update (self);

	if (self->file_under_combo)
		eve_file_under_entry_changed_cb (self, GTK_ENTRY (gtk_bin_get_child (GTK_BIN (self->file_under_combo))));

	e_vcard_editor_set_changed (self, FALSE);
}

static void
eve_finish_save (EVCardEditor *self)
{
	g_signal_emit (self, signals[AFTER_SAVE], 0, e_vcard_editor_get_target_client (self), self->contact, NULL);

	gtk_widget_destroy (GTK_WIDGET (self));
}

static void
eve_prepare_finish_save (EVCardEditor *self)
{
	gtk_widget_set_sensitive (self->top_box, TRUE);
	gtk_widget_set_sensitive (self->section_box, TRUE);
	gtk_widget_set_sensitive (self->button_save, self->changed);
	gtk_widget_set_sensitive (self->add_menu_button, TRUE);
	gtk_widget_set_visible (self->info_bar, FALSE);
	g_clear_object (&self->saving);
}

static void
eve_contact_removed_cb (GObject *source_object,
			GAsyncResult *result,
			gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	EVCardEditor *self = user_data;
	GError *local_error = NULL;

	eve_prepare_finish_save (self);

	if (e_book_client_remove_contact_by_uid_finish (book_client, result, &local_error) ||
	    g_error_matches (local_error, E_BOOK_CLIENT_ERROR, E_BOOK_ERROR_CONTACT_NOT_FOUND))
		eve_finish_save (self);
	else
		eve_report_error (self, local_error ? local_error->message : _("Unknown error"), NULL);

	g_clear_error (&local_error);
}

static void
eve_contact_added_cb (EBookClient *book_client,
                      const GError *error,
                      const gchar *id,
                      gpointer user_data)
{
	EVCardEditor *self = user_data;

	if (self->source_client != self->target_client && !e_client_is_readonly (E_CLIENT (self->source_client)) &&
	    !error && !(self->flags & E_VCARD_EDITOR_FLAG_IS_NEW)) {
		gchar *uid = e_contact_get (self->contact, E_CONTACT_UID);

		e_contact_set (self->contact, E_CONTACT_UID, id);

		if (uid && *uid) {
			e_book_client_remove_contact_by_uid (self->source_client, uid, E_BOOK_OPERATION_FLAG_NONE, NULL,
				eve_contact_removed_cb, self);
		} else {
			eve_prepare_finish_save (self);
			eve_finish_save (self);
		}

		g_free (uid);

		return;
	}

	eve_prepare_finish_save (self);

	if (id && !error)
		e_contact_set (self->contact, E_CONTACT_UID, id);

	if (error)
		eve_report_save_error (self, error, NULL);
	else
		eve_finish_save (self);
}

static void
eve_contact_modified_cb (EBookClient *book_client,
			 const GError *error,
			 gpointer user_data)
{
	EVCardEditor *self = user_data;

	if (g_error_matches (error, E_BOOK_CLIENT_ERROR, E_BOOK_ERROR_CONTACT_NOT_FOUND)) {
		eab_merging_book_add_contact (self->registry, book_client,
			self->contact, NULL, eve_contact_added_cb, self, FALSE);
		return;
	}

	eve_prepare_finish_save (self);

	if (error)
		eve_report_save_error (self, error, NULL);
	else
		eve_finish_save (self);
}

static void
eve_run_save (EVCardEditor *self)
{
	EBookClient *target_client;
	EContact *contact;

	target_client = e_vcard_editor_get_target_client (self);
	contact = e_vcard_editor_get_contact (self);

	if (g_signal_has_handler_pending (self, signals[CUSTOM_SAVE], 0, FALSE)) {
		gchar *error_message = NULL;
		gboolean success = FALSE;

		g_signal_emit (self, signals[CUSTOM_SAVE], 0, target_client, contact, &error_message, &success);

		if (error_message)
			eve_report_error (self, error_message, NULL);

		g_free (error_message);

		if (success)
			eve_finish_save (self);
	} else if (self->client_combo) {
		ESource *selected = e_source_combo_box_ref_active (E_SOURCE_COMBO_BOX (self->client_combo));

		if (self->opening_client) {
			e_alert_run_dialog_for_args (GTK_WINDOW (self), "addressbook:error-still-opening",
				e_source_get_display_name (selected), NULL);
		} else if (selected) {
			if (!e_client_is_readonly (E_CLIENT (target_client)) &&
			    e_client_is_readonly (E_CLIENT (self->source_client)) &&
			    e_alert_run_dialog_for_args (GTK_WINDOW (self), "addressbook:prompt-move", NULL) == GTK_RESPONSE_NO)
				return;

			g_cancellable_cancel (self->saving);
			g_clear_object (&self->saving);
			self->saving = g_cancellable_new ();
			gtk_widget_set_sensitive (self->top_box, FALSE);
			gtk_widget_set_sensitive (self->section_box, FALSE);
			gtk_widget_set_sensitive (self->button_save, FALSE);
			gtk_widget_set_sensitive (self->add_menu_button, FALSE);
			gtk_info_bar_set_response_sensitive (GTK_INFO_BAR (self->info_bar), GTK_RESPONSE_CANCEL, TRUE);
			gtk_widget_set_visible (self->info_bar, TRUE);

			if ((self->flags & E_VCARD_EDITOR_FLAG_IS_NEW) != 0 ||
			    !e_source_equal (e_client_get_source (E_CLIENT (target_client)), e_client_get_source (E_CLIENT (self->source_client)))) {
				eab_merging_book_add_contact (self->registry, target_client,
					self->contact, self->saving, eve_contact_added_cb, self, FALSE);
			} else {
				eab_merging_book_modify_contact (self->registry, target_client,
					self->contact, self->saving, eve_contact_modified_cb, self);
			}
		}

		g_clear_object (&selected);
	}
}

static gboolean
eve_prepare_save (EVCardEditor *self)
{
	GtkWidget *error_widget = NULL;
	gchar *error_message = NULL;
	gboolean success = FALSE;

	success = e_vcard_editor_validate (self, &error_message, &error_widget);

	if (error_message)
		eve_report_error (self, error_message, error_widget);

	g_free (error_message);

	return success;
}

static void
eve_save_clicked_cb (GtkButton *button,
		     gpointer user_data)
{
	EVCardEditor *self = user_data;

	if (self->saving)
		return;

	if (eve_prepare_save (self))
		eve_run_save (self);
}

static void
eve_help_clicked_cb (GtkButton *button,
		     gpointer user_data)
{
	EVCardEditor *self = user_data;

	e_display_help (GTK_WINDOW (self), "contacts-usage-add-contact");
}

static void
eve_cancel_clicked_cb (GtkButton *button,
				  gpointer user_data)
{
	EVCardEditor *self = user_data;

	gtk_widget_destroy (GTK_WIDGET (self));
}

static void
eve_changed_cb (EVCardEditor *self)
{
	if (!self->updating)
		e_vcard_editor_set_changed (self, TRUE);
}

static void
eve_section_changed_cb (EVCardEditorSection *section,
			gpointer user_data)
{
	EVCardEditor *self = user_data;
	GHashTableIter iter;
	gpointer key = NULL, value = NULL;

	if (self->updating)
		return;

	e_vcard_editor_set_changed (self, TRUE);

	if (self->add_menu_needs_update || !self->sections)
		return;

	g_hash_table_iter_init (&iter, self->sections);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		SectionKind kind = GPOINTER_TO_INT (key);
		EVCardEditorSection *stored_section = value;

		if (stored_section == section) {
			guint ii;

			for (ii = 0; ii < G_N_ELEMENTS (add_items); ii++) {
				if (add_items[ii].kind == kind && add_items[ii].cardinality == 1) {
					self->add_menu_needs_update = TRUE;
					break;
				}
			}

			break;
		}
	}
}

static void
eve_set_editor_changed_cb (EVCardEditor *self)
{
	if (!self->updating)
		e_vcard_editor_set_changed (self, TRUE);
}

static gboolean
eve_add_menu_button_press_event_cb (GtkMenuButton *button,
				    GdkEvent *event,
				    gpointer user_data)
{
	EVCardEditor *self = user_data;

	if (self->add_menu_needs_update)
		eve_update_add_menu (self);

	return FALSE;
}

static void
eve_section_add_item_cb (GtkWidget *button,
			 gpointer user_data)
{
	EVCardEditorAddItemFunc add_item_func = user_data;
	EVCardEditorSection *section;
	EVCardEditorItem *item;
	GtkWidget *widget;

	widget = gtk_widget_get_ancestor (button, E_TYPE_VCARD_EDITOR_SECTION);
	section = E_VCARD_EDITOR_SECTION (widget);

	item = add_item_func (section, E_CONTACT_FIELD_LAST, NULL);

	eve_focus_added_item (item);
}

static void
eve_event_after_cb (EVCardEditor *self,
		    GdkEvent *event,
		    gpointer user_data)
{
	if (event->type == GDK_KEY_PRESS) {
		GdkEventKey *key_event;
		gboolean has_modifier_pressed;

		key_event = (GdkEventKey *) event;
		has_modifier_pressed = (key_event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK |
					GDK_SUPER_MASK | GDK_HYPER_MASK |
					GDK_META_MASK)) != 0;

		if (key_event->keyval == GDK_KEY_F10 && !has_modifier_pressed && self->add_menu_button) {
			if (self->add_menu_needs_update)
				eve_update_add_menu (self);

			gtk_button_clicked (GTK_BUTTON (self->add_menu_button));
		}
	}
}

static gboolean
eve_delete_event_cb (GtkWidget *widget,
		     GdkEvent *event,
		     gpointer user_data)
{
	EVCardEditor *self = E_VCARD_EDITOR (widget);

	if (self->saving)
		return TRUE;

	if (e_vcard_editor_get_changed (self)) {
		switch (e_alert_run_dialog_for_args (GTK_WINDOW (self), "addressbook:prompt-save", NULL)) {
		case GTK_RESPONSE_YES:
			if (eve_prepare_save (self))
				eve_run_save (self);
			return TRUE;
		case GTK_RESPONSE_NO:
			return FALSE;
		case GTK_RESPONSE_CANCEL:
		default:
			return TRUE;
		}
	}

	return FALSE;
}

static void
eve_info_bar_response_cb (GtkInfoBar *info_bar,
			  gint response_id,
			  gpointer user_data)
{
	EVCardEditor *self = user_data;

	g_cancellable_cancel (self->saving);
	gtk_info_bar_set_response_sensitive (info_bar, GTK_RESPONSE_CANCEL, FALSE);
}

static void
eve_add_button_with_menu_clicked_cb (GtkWidget *button,
				     gpointer user_data)
{
	GtkMenu *menu = user_data;
	GdkEvent *event;

	g_object_set (menu,
		"anchor-hints", GDK_ANCHOR_FLIP_Y | GDK_ANCHOR_SLIDE | GDK_ANCHOR_RESIZE,
		NULL);

	event = gtk_get_current_event ();

	gtk_menu_popup_at_widget (menu, button, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, event);

	g_clear_pointer (&event, gdk_event_free);
}

static void
eve_got_supported_fields_cb (GObject *source_object,
			     GAsyncResult *result,
			     gpointer user_data)
{
	EVCardEditor *self = user_data;
	EBookClient *book_client;
	gchar *prop_value = NULL;
	GError *local_error = NULL;

	if (!e_client_get_backend_property_finish (E_CLIENT (source_object), result, &prop_value, &local_error)) {
		if (local_error && !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			gchar *error_message;

			error_message = g_strdup_printf (_("Failed to receive list of supported fields: %s"), local_error->message);

			eve_report_error (self, error_message, NULL);

			g_free (error_message);
		}

		g_clear_error (&local_error);

		return;
	}

	book_client = E_BOOK_CLIENT (source_object);

	if (book_client != e_vcard_editor_get_target_client (self)) {
		g_free (prop_value);
		return;
	}

	if (self->supported_fields) {
		GSList *fields, *link;

		fields = e_client_util_parse_comma_strings (prop_value);
		for (link = fields; link; link = g_slist_next (link)) {
			const gchar *field_name = link->data;
			EContactField field_id;

			field_id = e_contact_field_id (field_name);

			if (field_id != E_CONTACT_FIELD_LAST)
				g_hash_table_add (self->supported_fields, GINT_TO_POINTER (field_id));
		}

		g_slist_free_full (fields, g_free);

		if (self->logo_photo_widget)
			gtk_widget_set_sensitive (self->logo_photo_widget, eve_is_supported_field (self, E_CONTACT_LOGO) || eve_is_supported_field (self, E_CONTACT_PHOTO));

		eve_update_add_menu (self);
	}

	g_free (prop_value);
}

static void
e_vcard_editor_set_property (GObject *object,
			     guint prop_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	EVCardEditor *self = E_VCARD_EDITOR (object);

	switch (prop_id) {
	case PROP_CHANGED:
		e_vcard_editor_set_changed (self, g_value_get_boolean (value));
		break;
	case PROP_CLIENT_CACHE:
		g_set_object (&self->client_cache, g_value_get_object (value));
		break;
	case PROP_CONTACT:
		e_vcard_editor_set_contact (self, g_value_get_object (value));
		break;
	case PROP_FLAGS:
		e_vcard_editor_set_flags (self, g_value_get_uint (value));
		break;
	case PROP_REGISTRY:
		g_set_object (&self->registry, g_value_get_object (value));
		break;
	case PROP_SOURCE_CLIENT:
		g_set_object (&self->source_client, g_value_get_object (value));
		break;
	case PROP_TARGET_CLIENT:
		e_vcard_editor_set_target_client (self, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_vcard_editor_get_property (GObject *object,
			     guint prop_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	EVCardEditor *self = E_VCARD_EDITOR (object);

	switch (prop_id) {
	case PROP_CHANGED:
		g_value_set_boolean (value, e_vcard_editor_get_changed (self));
		break;
	case PROP_CLIENT_CACHE:
		g_value_set_object (value, e_vcard_editor_get_client_cache (self));
		break;
	case PROP_CONTACT:
		g_value_set_object (value, e_vcard_editor_get_contact (self));
		break;
	case PROP_FLAGS:
		g_value_set_uint (value, e_vcard_editor_get_flags (self));
		break;
	case PROP_REGISTRY:
		g_value_set_object (value, e_vcard_editor_get_registry (self));
		break;
	case PROP_SOURCE_CLIENT:
		g_value_set_object (value, e_vcard_editor_get_source_client (self));
		break;
	case PROP_TARGET_CLIENT:
		g_value_set_object (value, e_vcard_editor_get_target_client (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_vcard_editor_constructed (GObject *object)
{
	EVCardEditor *self = E_VCARD_EDITOR (object);
	GApplication *application;
	GtkAccelGroup *accel_group;
	GtkWidget *widget, *container;
	GtkWidget *button_help;
	GtkOverlay *overlay;
	GtkGrid *grid;
	ESectionBox *section_box;
	EVCardEditorItem *item;
	gint row = 0;
	guint ii;

	G_OBJECT_CLASS (e_vcard_editor_parent_class)->constructed (object);

	g_object_set (self,
		"icon-name", "contact-editor",
		"default-width", 600,
		"default-height", 650,
		NULL);

	g_signal_connect (self, "delete-event",
		G_CALLBACK (eve_delete_event_cb), NULL);

	self->add_menu_button = gtk_menu_button_new ();
	gtk_button_set_image (GTK_BUTTON (self->add_menu_button), gtk_image_new_from_icon_name ("list-add", GTK_ICON_SIZE_SMALL_TOOLBAR));
	g_object_set (self->add_menu_button,
		"visible", TRUE,
		"tooltip-text", _("Add"),
		"always-show-image", TRUE,
		NULL);

	g_signal_connect (self->add_menu_button, "button-press-event",
		G_CALLBACK (eve_add_menu_button_press_event_cb), self);

	if (e_util_get_use_header_bar ()) {
		GtkHeaderBar *header_bar;

		header_bar = GTK_HEADER_BAR (gtk_dialog_get_header_bar (GTK_DIALOG (self)));

		widget = gtk_button_new_with_mnemonic (_("_Save"));
		gtk_widget_set_visible (widget, TRUE);
		gtk_style_context_add_class (gtk_widget_get_style_context (widget), "suggested-action");
		gtk_header_bar_pack_start (header_bar, widget);

		self->button_save = widget;

		widget = gtk_button_new_from_icon_name ("help-browser", GTK_ICON_SIZE_SMALL_TOOLBAR);
		g_object_set (widget,
			"visible", TRUE,
			"use-underline", TRUE,
			"tooltip-text", _("Help"),
			NULL);
		gtk_style_context_add_class (gtk_widget_get_style_context (widget), "image-button");
		gtk_header_bar_pack_end (header_bar, widget);

		button_help = widget;

		gtk_header_bar_pack_end (header_bar, self->add_menu_button);
	} else {
		GtkDialog *dialog = GTK_DIALOG (self);

		widget = gtk_button_new_with_mnemonic (_("_Cancel"));
		gtk_widget_set_visible (widget, TRUE);
		gtk_widget_set_valign (widget, GTK_ALIGN_FILL);
		gtk_dialog_add_action_widget (dialog, widget, GTK_RESPONSE_CANCEL);

		g_signal_connect (widget, "clicked", G_CALLBACK (eve_cancel_clicked_cb), self);

		widget = gtk_button_new_with_mnemonic (_("_Save"));
		gtk_widget_set_visible (widget, TRUE);
		gtk_widget_set_valign (widget, GTK_ALIGN_FILL);
		gtk_style_context_add_class (gtk_widget_get_style_context (widget), "suggested-action");
		gtk_dialog_add_action_widget (dialog, widget, GTK_RESPONSE_OK);

		self->button_save = widget;

		widget = gtk_button_new_from_icon_name ("help-browser", GTK_ICON_SIZE_SMALL_TOOLBAR);
		gtk_button_set_label (GTK_BUTTON (widget), _("_Help"));
		gtk_widget_set_visible (widget, TRUE);
		gtk_widget_set_valign (widget, GTK_ALIGN_FILL);
		gtk_button_set_use_underline (GTK_BUTTON (widget), TRUE);
		gtk_style_context_remove_class (gtk_widget_get_style_context (widget), "image-button");
		gtk_dialog_add_action_widget (dialog, widget, GTK_RESPONSE_HELP);

		/* it's not a 'help' response, it makes it packed beside the Help button, on the other
		   side of the dialog than the Cancel/Save buttons are */
		gtk_dialog_add_action_widget (dialog, self->add_menu_button, GTK_RESPONSE_HELP);

		button_help = widget;
	}

	accel_group = gtk_accel_group_new ();
	gtk_widget_add_accelerator (self->button_save, "clicked", accel_group, 's', GDK_CONTROL_MASK, 0);
	gtk_window_add_accel_group (GTK_WINDOW (self), accel_group);
	g_clear_object (&accel_group);

	gtk_widget_set_sensitive (self->button_save, self->changed);

	g_signal_connect (self->button_save, "clicked", G_CALLBACK (eve_save_clicked_cb), self);
	g_signal_connect (button_help, "clicked", G_CALLBACK (eve_help_clicked_cb), self);

	container = gtk_dialog_get_action_area (GTK_DIALOG (self));
	gtk_container_set_border_width (GTK_CONTAINER (container), 12);
	container = gtk_dialog_get_content_area (GTK_DIALOG (self));
	gtk_container_set_border_width (GTK_CONTAINER (container), 0);

	widget = gtk_overlay_new ();
	g_object_set (widget,
		"visible", TRUE,
		NULL);
	gtk_container_add (GTK_CONTAINER (container), widget);

	overlay = GTK_OVERLAY (widget);

	widget = gtk_info_bar_new ();
	g_object_set (widget,
		"halign", GTK_ALIGN_CENTER,
		"valign", GTK_ALIGN_START,
		"no-show-all", TRUE,
		"visible", FALSE,
		"revealed", TRUE,
		"message-type", GTK_MESSAGE_INFO,
		NULL);

	gtk_overlay_add_overlay (overlay, widget);

	gtk_info_bar_add_button (GTK_INFO_BAR (widget), _("_Cancel"), GTK_RESPONSE_CANCEL);

	self->info_bar = widget;

	g_signal_connect (widget, "response",
		G_CALLBACK (eve_info_bar_response_cb), self);

	container = gtk_info_bar_get_content_area (GTK_INFO_BAR (widget));

	widget = gtk_label_new (_("Saving changes, please wait…"));
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_CENTER,
		"margin-start", 12,
		"margin-end", 12,
		"margin-top", 12,
		"margin-bottom", 12,
		"visible", TRUE,
		"max-width-chars", 40,
		"wrap", TRUE,
		NULL);

	gtk_container_add (GTK_CONTAINER (container), widget);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_add (GTK_CONTAINER (overlay), widget);

	self->top_box = widget;

	container = widget;

	item = eve_new_item_logo_photo (self);
	widget = e_vcard_editor_item_get_data_box (item);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	g_signal_connect_swapped (item, "changed", G_CALLBACK (eve_changed_cb), self);
	g_ptr_array_add (self->items, item);

	self->logo_photo_widget = widget;

	widget = gtk_grid_new ();
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"column-spacing", 4,
		"row-spacing", 4,
		NULL);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);

	grid = GTK_GRID (widget);

	item = eve_new_item_kind (self);
	widget = e_vcard_editor_item_get_label_widget (item);
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_grid_attach (grid, widget, 0, row, 1, 1);
	widget = e_vcard_editor_item_get_data_box (item);
	gtk_grid_attach (grid, widget, 1, row, 1, 1);
	g_signal_connect_swapped (item, "changed", G_CALLBACK (eve_changed_cb), self);
	g_signal_connect (item, "changed", G_CALLBACK (eve_contact_kind_changed_cb), self);
	g_ptr_array_add (self->items, item);

	row++;

	item = eve_new_item_address_book (self);
	widget = e_vcard_editor_item_get_label_widget (item);
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_grid_attach (grid, widget, 0, row, 1, 1);
	widget = e_vcard_editor_item_get_data_box (item);
	gtk_grid_attach (grid, widget, 1, row, 1, 1);
	g_signal_connect_swapped (item, "changed", G_CALLBACK (eve_changed_cb), self);
	g_ptr_array_add (self->items, item);

	row++;

	item = eve_new_item_categories (self);
	widget = e_vcard_editor_item_get_label_widget (item);
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_grid_attach (grid, widget, 0, row, 1, 1);
	widget = e_vcard_editor_item_get_data_box (item);
	gtk_grid_attach (grid, widget, 1, row, 1, 1);
	g_signal_connect_swapped (item, "changed", G_CALLBACK (eve_changed_cb), self);
	g_ptr_array_add (self->items, item);

	gtk_widget_show_all (container);

	container = gtk_dialog_get_content_area (GTK_DIALOG (self));

	widget = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
	g_object_set (widget,
		"margin-top", 6,
		"margin-bottom", 6,
		"visible", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	self->top_separator = widget;

	widget = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"shadow-type", GTK_SHADOW_NONE,
		"kinetic-scrolling", TRUE,
		"min-content-height", 120,
		"propagate-natural-height", TRUE,
		"propagate-natural-width", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);

	container = widget;

	widget = e_section_box_new ();
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);
	gtk_container_add (GTK_CONTAINER (container), widget);

	self->section_box = widget;

	section_box = E_SECTION_BOX (widget);

	e_section_box_connect_parent_container (section_box, container);

	gtk_widget_show_all (container);

	for (ii = 0; ii < G_N_ELEMENTS (sections); ii++) {
		g_warn_if_fail (!g_hash_table_contains (self->sections, GINT_TO_POINTER (sections[ii].kind)));

		widget = e_vcard_editor_section_new (self, sections[ii].label, sections[ii].fill_section_func, sections[ii].fill_contact_func);

		if (sections[ii].kind == SECTION_KIND_IDENTITY)
			gtk_widget_set_margin_end (widget, 12);

		if (sections[ii].kind == SECTION_KIND_IDENTITY)
			e_vcard_editor_section_set_sort_function (E_VCARD_EDITOR_SECTION (widget), eve_sort_identity_section_cb);
		else if (sections[ii].kind == SECTION_KIND_IDENTITY_DETAILS)
			e_vcard_editor_section_set_sort_function (E_VCARD_EDITOR_SECTION (widget), eve_sort_identity_details_section_cb);
		else if (sections[ii].kind == SECTION_KIND_ORGANIZATION)
			e_vcard_editor_section_set_sort_function (E_VCARD_EDITOR_SECTION (widget), eve_sort_organization_section_cb);
		else if (sections[ii].kind == SECTION_KIND_JOB)
			e_vcard_editor_section_set_sort_function (E_VCARD_EDITOR_SECTION (widget), eve_sort_job_section_cb);
		else if (sections[ii].kind == SECTION_KIND_WEB)
			e_vcard_editor_section_set_sort_function (E_VCARD_EDITOR_SECTION (widget), eve_sort_web_section_cb);
		else if (sections[ii].kind == SECTION_KIND_LOCATION)
			e_vcard_editor_section_set_sort_function (E_VCARD_EDITOR_SECTION (widget), eve_sort_location_section_cb);
		else if (sections[ii].kind == SECTION_KIND_LIFE_EVENTS)
			e_vcard_editor_section_set_sort_function (E_VCARD_EDITOR_SECTION (widget), eve_sort_life_events_section_cb);

		if (sections[ii].kind == SECTION_KIND_ADDRESS ||
		    sections[ii].kind == SECTION_KIND_NOTES ||
		    sections[ii].kind == SECTION_KIND_CERTS)
			e_vcard_editor_section_set_single_line (E_VCARD_EDITOR_SECTION (widget), FALSE);

		if (sections[ii].kind == SECTION_KIND_CERTS)
			g_signal_connect (widget, "changed", G_CALLBACK (eve_certs_section_changed_cb), self);

		if (sections[ii].kind == SECTION_KIND_EMAIL) {
			GtkWidget *button;

			button = gtk_check_button_new_with_mnemonic (_("_Wants to receive HTML mail"));
			g_object_set (button,
				"visible", TRUE,
				"halign", GTK_ALIGN_START,
				NULL);

			e_vcard_editor_section_take_widget (E_VCARD_EDITOR_SECTION (widget), button, GTK_PACK_END);

			g_signal_connect_object (button, "toggled",
				G_CALLBACK (eve_set_editor_changed_cb), self, G_CONNECT_SWAPPED);
		}

		g_signal_connect (widget, "changed", G_CALLBACK (eve_section_changed_cb), self);
		g_hash_table_insert (self->sections, GINT_TO_POINTER (sections[ii].kind), widget);

		/* notes are always at the bottom, using full width */
		if (sections[ii].kind == SECTION_KIND_NOTES)
			gtk_box_pack_start (GTK_BOX (section_box), widget, FALSE, FALSE, 0);
		else
			e_section_box_add (section_box, widget);

		if (sections[ii].add_item_func) {
			GtkButton *button;

			button = e_vcard_editor_section_get_add_button (E_VCARD_EDITOR_SECTION (widget));

			gtk_widget_set_visible (GTK_WIDGET (button), TRUE);

			g_signal_connect (button, "clicked", G_CALLBACK (eve_section_add_item_cb), sections[ii].add_item_func);
		} else if (sections[ii].kind == SECTION_KIND_WEB) {
			GtkButton *button;
			GtkWidget *menu;
			GtkMenuShell *menu_shell;
			guint jj;

			button = e_vcard_editor_section_get_add_button (E_VCARD_EDITOR_SECTION (widget));

			gtk_widget_set_visible (GTK_WIDGET (button), TRUE);

			menu = gtk_menu_new ();
			menu_shell = GTK_MENU_SHELL (menu);

			for (jj = 0; jj < G_N_ELEMENTS (add_items); jj++) {
				GtkWidget *menu_item;

				if (add_items[jj].kind != sections[ii].kind || !add_items[jj].add_item_func)
					continue;

				menu_item = gtk_menu_item_new_with_mnemonic (_(add_items[jj].label));
				g_object_set_data (G_OBJECT (menu_item), ADD_MENU_ITEM_INDEX, GUINT_TO_POINTER (jj + 1));
				g_signal_connect (menu_item, "activate",
					G_CALLBACK (eve_add_menu_item_activated_cb), self);

				gtk_menu_shell_append (menu_shell, menu_item);
			}

			gtk_widget_show_all (menu);

			gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (button), NULL);

			g_signal_connect_data (button, "clicked",
				G_CALLBACK (eve_add_button_with_menu_clicked_cb), g_object_ref_sink (menu), (GClosureNotify) g_object_unref, 0);
		}
	}

	widget = g_hash_table_lookup (self->sections, GINT_TO_POINTER (SECTION_KIND_IDENTITY));
	if (widget) {
		EVCardEditorSection *section = E_VCARD_EDITOR_SECTION (widget);

		item = eve_new_item_file_under (self);
		e_vcard_editor_section_take_item (section, item);

		item = eve_add_item_simple (section, E_CONTACT_FULL_NAME, _("Full _Name"));
		e_vcard_editor_item_set_permanent (item, TRUE);

		widget = e_vcard_editor_item_get_data_widget (item, 0);

		self->full_name_entry = GTK_ENTRY (widget);

		g_signal_connect_object (self->full_name_entry, "changed",
			G_CALLBACK (eve_full_name_changed_cb), self, G_CONNECT_SWAPPED);

		eve_focus_added_item (item);
	} else {
		g_warn_if_reached ();
	}

	eve_fill_widgets (self);

	g_signal_connect_object (self, "event-after",
		G_CALLBACK (eve_event_after_cb), NULL, G_CONNECT_AFTER);

	e_restore_window (
		GTK_WINDOW (self), "/org/gnome/evolution/addressbook/vcard-editor-window/",
		E_RESTORE_WINDOW_SIZE | E_RESTORE_WINDOW_POSITION);

	application = g_application_get_default ();
	if (application && GTK_IS_APPLICATION (application))
		gtk_application_add_window (GTK_APPLICATION (application), GTK_WINDOW (self));
}

static void
e_vcard_editor_dispose (GObject *object)
{
	EVCardEditor *self = E_VCARD_EDITOR (object);

	g_cancellable_cancel (self->opening_client);
	g_clear_object (&self->opening_client);

	g_cancellable_cancel (self->saving);
	g_clear_object (&self->saving);

	g_cancellable_cancel (self->supported_fields_cancellable);
	g_clear_object (&self->supported_fields_cancellable);

	self->top_box = NULL;
	self->section_box = NULL;
	self->button_save = NULL;
	self->logo_photo_widget = NULL;
	self->client_combo = NULL;
	self->file_under_combo = NULL;
	self->full_name_entry = NULL;
	self->name_parts[0] = NULL;
	self->name_parts[1] = NULL;
	self->name_parts[2] = NULL;
	self->name_parts[3] = NULL;
	self->name_parts[4] = NULL;
	self->company_entry = NULL;
	self->add_menu_button = NULL;
	self->top_separator = NULL;
	self->info_bar = NULL;

	g_clear_pointer (&self->items, g_ptr_array_unref);
	g_clear_pointer (&self->sections, g_hash_table_unref);
	g_clear_pointer (&self->supported_fields, g_hash_table_unref);

	G_OBJECT_CLASS (e_vcard_editor_parent_class)->dispose (object);
}

static void
e_vcard_editor_finalize (GObject *object)
{
	EVCardEditor *self = E_VCARD_EDITOR (object);

	g_clear_object (&self->registry);
	g_clear_object (&self->client_cache);
	g_clear_object (&self->source_client);
	g_clear_object (&self->target_client);
	g_clear_object (&self->contact);

	G_OBJECT_CLASS (e_vcard_editor_parent_class)->finalize (object);
}

static void
e_vcard_editor_class_init (EVCardEditorClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_vcard_editor_set_property;
	object_class->get_property = e_vcard_editor_get_property;
	object_class->constructed = e_vcard_editor_constructed;
	object_class->dispose = e_vcard_editor_dispose;
	object_class->finalize = e_vcard_editor_finalize;

	properties[PROP_CHANGED] = g_param_spec_boolean ("changed", NULL, NULL, FALSE,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	properties[PROP_CLIENT_CACHE] = g_param_spec_object ("client-cache", NULL, NULL, E_TYPE_CLIENT_CACHE,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	properties[PROP_CONTACT] = g_param_spec_object ("contact", NULL, NULL, E_TYPE_CONTACT,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	properties[PROP_FLAGS] = g_param_spec_uint ("flags", NULL, NULL, 0, G_MAXUINT, 0,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	properties[PROP_REGISTRY] = g_param_spec_object ("registry", NULL, NULL, E_TYPE_SOURCE_REGISTRY,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	properties[PROP_SOURCE_CLIENT] = g_param_spec_object ("source-client", NULL, NULL, E_TYPE_BOOK_CLIENT,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	properties[PROP_TARGET_CLIENT] = g_param_spec_object ("target-client", NULL, NULL, E_TYPE_BOOK_CLIENT,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

	/* mainly for testing purposes, replaces internal saving routine;
	   gboolean custom_save (EVCardEditor *editor, EBookClient *target_client, EContact *contact, gchar **out_error_message); */
	signals[CUSTOM_SAVE] = g_signal_new (
		"custom-save",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		NULL,
		G_TYPE_BOOLEAN, 3, E_TYPE_BOOK_CLIENT, E_TYPE_CONTACT, G_TYPE_POINTER);

	/* void after_save (EVCardEditor *editor, EBookClient *target_client, EContact *contact); */
	signals[AFTER_SAVE] = g_signal_new (
		"after-save",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 2, E_TYPE_BOOK_CLIENT, E_TYPE_CONTACT);
}

static void
e_vcard_editor_init (EVCardEditor *self)
{
	self->items = g_ptr_array_new_with_free_func (g_object_unref);
	self->sections = g_hash_table_new (g_direct_hash, g_direct_equal);
	self->supported_fields = g_hash_table_new (g_direct_hash, g_direct_equal);
}

/*
 * e_vcard_editor_new:
 * @transient_for: (nullable): a "parent" #GtkWindow, or %NULL
 * @shell: (transfer none) (nullable): an #EShell, or %NULL
 * @source_client: (transfer none): an #EBookClient
 * @contact: (nullable) (transfer none): an #EContact to edit, or %NULL to create a new
 * @flags: bit-or of #EVCardEditorFlags
 *
 * Creates a new vCard editor. The @shell can be %NULL, then a new #ESourceRegistry
 * and an #EClientCache is created.
 *
 * Returns: (transfer full): a new #EVCardEditor
 *
 * Since: 3.60
 */
EVCardEditor *
e_vcard_editor_new (GtkWindow *transient_for,
		    EShell *shell,
		    EBookClient *source_client,
		    EContact *contact,
		    EVCardEditorFlags flags)
{
	EVCardEditor *self;
	ESourceRegistry *registry;
	EClientCache *client_cache;

	if (transient_for)
		g_return_val_if_fail (GTK_IS_WINDOW (transient_for), NULL);
	if (shell)
		g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (E_IS_BOOK_CLIENT (source_client), NULL);

	if (contact) {
		g_return_val_if_fail (E_IS_CONTACT (contact), NULL);
		g_object_ref (contact);
	} else {
		contact = e_contact_new ();
	}

	if (shell) {
		registry = g_object_ref (e_shell_get_registry (shell));
		client_cache = g_object_ref (e_shell_get_client_cache (shell));
	} else {
		registry = e_source_registry_new_sync (NULL, NULL);
		client_cache = e_client_cache_new (registry);
	}

	eve_convert_contact_for_client (&contact, source_client);

	self = g_object_new (E_TYPE_VCARD_EDITOR,
		"can-focus", FALSE,
		"border-width", 6,
		"window-position", GTK_WIN_POS_CENTER,
		"type-hint", GDK_WINDOW_TYPE_HINT_NORMAL,
		"transient-for", transient_for,
		"use-header-bar", e_util_get_use_header_bar (),
		"title", _("Contact Editor"),
		"registry", registry,
		"client-cache", client_cache,
		"source-client", source_client,
		"contact", contact,
		"flags", flags,
		NULL);

	self->supported_fields_cancellable = g_cancellable_new ();

	e_client_get_backend_property (E_CLIENT (self->source_client), E_BOOK_BACKEND_PROPERTY_SUPPORTED_FIELDS,
		self->supported_fields_cancellable, eve_got_supported_fields_cb, self);

	g_clear_object (&client_cache);
	g_clear_object (&registry);
	g_object_unref (contact);

	return self;
}

ESourceRegistry *
e_vcard_editor_get_registry (EVCardEditor *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR (self), NULL);

	return self->registry;
}

EClientCache *
e_vcard_editor_get_client_cache (EVCardEditor *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR (self), NULL);

	return self->client_cache;
}

EBookClient *
e_vcard_editor_get_source_client (EVCardEditor *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR (self), NULL);

	return self->source_client;
}

EBookClient *
e_vcard_editor_get_target_client (EVCardEditor *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR (self), NULL);

	return self->target_client ? self->target_client : self->source_client;
}

void
e_vcard_editor_set_target_client (EVCardEditor *self,
				  EBookClient *client)
{
	g_return_if_fail (E_IS_VCARD_EDITOR (self));
	if (client)
		g_return_if_fail (E_IS_BOOK_CLIENT (client));

	if (self->target_client == NULL && self->source_client == client)
		return;

	if (g_set_object (&self->target_client, client)) {
		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TARGET_CLIENT]);

		g_cancellable_cancel (self->supported_fields_cancellable);
		g_clear_object (&self->supported_fields_cancellable);

		if (self->supported_fields)
			g_hash_table_remove_all (self->supported_fields);

		self->supported_fields_cancellable = g_cancellable_new ();

		e_client_get_backend_property (E_CLIENT (self->target_client), E_BOOK_BACKEND_PROPERTY_SUPPORTED_FIELDS,
			self->supported_fields_cancellable, eve_got_supported_fields_cb, self);

		eve_update (self);
	}
}

EVCardEditorFlags
e_vcard_editor_get_flags (EVCardEditor *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR (self), E_VCARD_EDITOR_FLAG_NONE);

	return self->flags;
}

void
e_vcard_editor_set_flags (EVCardEditor *self,
			  EVCardEditorFlags flags)
{
	g_return_if_fail (E_IS_VCARD_EDITOR (self));

	if (self->flags != flags) {
		self->flags = flags;
		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FLAGS]);

		eve_update (self);
	}
}

EVCardEditorContactKind
e_vcard_editor_get_contact_kind	(EVCardEditor *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR (self), E_VCARD_EDITOR_CONTACT_KIND_UNKNOWN);

	return self->contact_kind;
}

/*
 * e_vcard_editor_get_contact:
 * @self: an #EVCardEditor
 *
 * Gets the #EContact used to populate the dialog. The actual content does
 * not necessarily contains the changes in the GUI, those are propagated
 * to it within e_vcard_editor_validate() call.
 *
 * Returns: (transfer none): a used #EContact
 *
 * Since: 3.60
 */
EContact *
e_vcard_editor_get_contact (EVCardEditor *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR (self), NULL);

	return self->contact;
}

void
e_vcard_editor_set_contact (EVCardEditor *self,
			    EContact *contact)
{
	g_return_if_fail (E_IS_VCARD_EDITOR (self));
	g_return_if_fail (E_IS_CONTACT (contact));

	if (g_set_object (&self->contact, contact)) {
		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONTACT]);

		eve_fill_widgets (self);
	}
}

gboolean
e_vcard_editor_get_changed (EVCardEditor *self)
{
	g_return_val_if_fail (E_IS_VCARD_EDITOR (self), FALSE);

	return self->changed;
}

void
e_vcard_editor_set_changed (EVCardEditor *self,
			    gboolean value)
{
	g_return_if_fail (E_IS_VCARD_EDITOR (self));

	if ((!self->changed) != (!value)) {
		self->changed = value;
		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHANGED]);

		if (self->button_save)
			gtk_widget_set_sensitive (self->button_save, self->changed);
	}
}

/*
 * e_vcard_editor_validate:
 * @self: an #EVCardEditor
 * @out_error_message: (out) (optional) (nullable) (transfer full): a location to store an actual
 *    localized error message containing the reason for failed validation; can be %NULL to not get it
 * @out_error_widget: (out) (optional) (nullable) (transfer none): a location to store a widget,
 *    which reported an error, or %NULL to not get it
 *
 * Validates user input and updates the internal contact - it can be received
 * with e_vcard_editor_get_contact(). When the function returns %FALSE,
 * the editor contains some errors, which are described in the @out_error_message,
 * coming from @out_error_widget, or they are set to %NULL to indicate the save
 * was cancelled by the user. Both out arguments can be %NULL, then the information
 * is not provided to the caller.
 *
 * The content of the @out_error_message should be freed with g_free(),
 * when no longer needed.
 *
 * Returns: %TRUE, when the information in the dialog is valid for the client, %FALSE otherwise
 *
 * Since: 3.60
 */
gboolean
e_vcard_editor_validate (EVCardEditor *self,
			 gchar **out_error_message,
			 GtkWidget **out_error_widget)
{
	GHashTableIter iter;
	gpointer key = NULL, value = NULL;
	guint ii;

	g_return_val_if_fail (E_IS_VCARD_EDITOR (self), FALSE);
	g_return_val_if_fail (self->items, FALSE);
	g_return_val_if_fail (self->sections, FALSE);

	if (self->full_name_entry) {
		gchar *text;
		gboolean filled;

		text = g_strstrip (g_strdup (gtk_entry_get_text (self->full_name_entry)));
		filled = text && *text;
		g_free (text);
		if (!filled) {
			if (out_error_message)
				*out_error_message = g_strdup (_("“Full Name” cannot be empty"));
			if (out_error_widget)
				*out_error_widget = GTK_WIDGET (self->full_name_entry);

			return FALSE;
		}
	}

	if (self->file_under_combo) {
		gchar *text;
		gboolean filled;

		text = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (self->file_under_combo))))));
		filled = text && *text;
		g_free (text);
		if (!filled) {
			if (out_error_message)
				*out_error_message = g_strdup (_("“File under” cannot be empty"));
			if (out_error_widget)
				*out_error_widget = GTK_WIDGET (self->file_under_combo);

			return FALSE;
		}
	}

	eve_convert_contact_for_client (&self->contact, e_vcard_editor_get_target_client (self));

	for (ii = 0; ii < self->items->len; ii++) {
		EVCardEditorItem *item = g_ptr_array_index (self->items, ii);

		if (!e_vcard_editor_item_fill_contact (item, self, self->contact, out_error_message, out_error_widget))
			return FALSE;
	}

	g_hash_table_iter_init (&iter, self->sections);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		SectionKind kind = GPOINTER_TO_INT (key);
		EVCardEditorSection *section = value;

		if (e_vcard_editor_section_get_changed (section)) {
			for (ii = 0; ii < G_N_ELEMENTS (add_items); ii++) {
				if (add_items[ii].kind == kind)
					break;
			}

			for (; ii < G_N_ELEMENTS (add_items); ii++) {
				if (add_items[ii].kind != kind && add_items[ii].kind != SECTION_KIND_SEPARATOR)
					break;

				if (add_items[ii].remove_field_func)
					add_items[ii].remove_field_func (self, self->contact, add_items[ii].field_id);
			}
		}

		if (!e_vcard_editor_section_fill_contact (section, self->contact, out_error_message, out_error_widget))
			return FALSE;
	}

	return TRUE;
}
