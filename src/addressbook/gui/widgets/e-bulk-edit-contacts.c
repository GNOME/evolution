/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <libebook/libebook.h>

#include "addressbook/gui/widgets/eab-gui-util.h"
#include "e-util/e-util.h"

#include "e-bulk-edit-contacts.h"

typedef struct _EditItem {
	GtkToggleButton *toggle;
	GtkWidget *widget;
} EditItem;

typedef enum {
	EDIT_ITEM_HOMEPAGE_URL = 0,
	EDIT_ITEM_BLOG_URL,
	EDIT_ITEM_CALENDAR_URI,
	EDIT_ITEM_FREEBUSY_URL,
	EDIT_ITEM_VIDEO_URL,
	EDIT_ITEM_ORG,
	EDIT_ITEM_ORG_UNIT,
	EDIT_ITEM_OFFICE,
	EDIT_ITEM_TITLE,
	EDIT_ITEM_ROLE,
	EDIT_ITEM_MANAGER,
	EDIT_ITEM_ASSISTANT,
	EDIT_ITEM_SPOUSE,
	N_EDIT_ITEMS
} EditItems;

typedef struct _MailingAddressItem {
	EditItem address;
	EditItem pobox;
	EditItem city;
	EditItem zip;
	EditItem state;
	EditItem country;
} MailingAddressItem;

struct _EBulkEditContactsPrivate {
	GtkWidget *items_grid;
	GtkWidget *alert_bar;
	GtkWidget *activity_bar;

	EBookClient *book_client;
	GPtrArray *contacts; /* EContact * */
	GCancellable *cancellable;

	ECategoriesSelector *categories;
	EditItem simple_edit_items[N_EDIT_ITEMS];
	MailingAddressItem address_home;
	MailingAddressItem address_work;
	MailingAddressItem address_other;
};

static void e_bulk_edit_contacts_alert_sink_init (EAlertSinkInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EBulkEditContacts, e_bulk_edit_contacts, GTK_TYPE_DIALOG,
	G_ADD_PRIVATE (EBulkEditContacts)
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, e_bulk_edit_contacts_alert_sink_init))

static void
e_bulk_edit_contacts_apply_simple (EBulkEditContacts *self,
				   GSList *contacts,
				   EContactField field,
				   GHashTable *changed,
				   EditItems item_id)
{
	EditItem *edit_item;
	GSList *link;
	const gchar *value;

	g_return_if_fail (item_id >= 0 && item_id < N_EDIT_ITEMS);

	edit_item = &self->priv->simple_edit_items[item_id];

	if (!gtk_toggle_button_get_active (edit_item->toggle))
		return;

	value = gtk_entry_get_text (GTK_ENTRY (edit_item->widget));

	if (value && !*value)
		value = NULL;

	for (link = contacts; link; link = g_slist_next (link)) {
		EContact *contact = link->data;
		gchar *old_value;

		old_value = e_contact_get (contact, field);

		if (g_strcmp0 (old_value, value) != 0) {
			e_contact_set (contact, field, value);
			g_hash_table_add (changed, contact);
		}

		g_free (old_value);
	}
}

static void
e_bulk_edit_contacts_apply_categories (EBulkEditContacts *self,
				       GSList *contacts,
				       EContactField field,
				       GHashTable *changed)
{
	GHashTable *checked = NULL, *unchecked = NULL;
	GSList *link;

	e_categories_selector_get_changes (self->priv->categories, &checked, &unchecked);

	if (!checked && !unchecked)
		return;

	for (link = contacts; link; link = g_slist_next (link)) {
		EContact *contact = link->data;
		gchar *old_value, *new_value;

		old_value = e_contact_get (contact, field);
		new_value = e_categories_selector_util_apply_changes (old_value, checked, unchecked);

		if (g_strcmp0 (old_value, new_value) != 0) {
			g_hash_table_add (changed, contact);

			e_contact_set (contact, field, new_value);
		}

		g_free (old_value);
		g_free (new_value);
	}

	g_clear_pointer (&checked, g_hash_table_destroy);
	g_clear_pointer (&unchecked, g_hash_table_destroy);
}

static void
e_bulk_edit_contacts_apply_homepage_url (EBulkEditContacts *self,
					 GSList *contacts,
					 EContactField field,
					 GHashTable *changed)
{
	e_bulk_edit_contacts_apply_simple (self, contacts, field, changed, EDIT_ITEM_HOMEPAGE_URL);
}

static void
e_bulk_edit_contacts_apply_blog_url (EBulkEditContacts *self,
				     GSList *contacts,
				     EContactField field,
				     GHashTable *changed)
{
	e_bulk_edit_contacts_apply_simple (self, contacts, field, changed, EDIT_ITEM_BLOG_URL);
}

static void
e_bulk_edit_contacts_apply_calendar_uri (EBulkEditContacts *self,
					 GSList *contacts,
					 EContactField field,
					 GHashTable *changed)
{
	e_bulk_edit_contacts_apply_simple (self, contacts, field, changed, EDIT_ITEM_CALENDAR_URI);
}

static void
e_bulk_edit_contacts_apply_freebusy_url (EBulkEditContacts *self,
					 GSList *contacts,
					 EContactField field,
					 GHashTable *changed)
{
	e_bulk_edit_contacts_apply_simple (self, contacts, field, changed, EDIT_ITEM_FREEBUSY_URL);
}

static void
e_bulk_edit_contacts_apply_video_url (EBulkEditContacts *self,
				      GSList *contacts,
				      EContactField field,
				      GHashTable *changed)
{
	e_bulk_edit_contacts_apply_simple (self, contacts, field, changed, EDIT_ITEM_VIDEO_URL);
}

static void
e_bulk_edit_contacts_apply_org (EBulkEditContacts *self,
				GSList *contacts,
				EContactField field,
				GHashTable *changed)
{
	e_bulk_edit_contacts_apply_simple (self, contacts, field, changed, EDIT_ITEM_ORG);
}

static void
e_bulk_edit_contacts_apply_org_unit (EBulkEditContacts *self,
				     GSList *contacts,
				     EContactField field,
				     GHashTable *changed)
{
	e_bulk_edit_contacts_apply_simple (self, contacts, field, changed, EDIT_ITEM_ORG_UNIT);
}

static void
e_bulk_edit_contacts_apply_office (EBulkEditContacts *self,
				   GSList *contacts,
				   EContactField field,
				   GHashTable *changed)
{
	e_bulk_edit_contacts_apply_simple (self, contacts, field, changed, EDIT_ITEM_OFFICE);
}

static void
e_bulk_edit_contacts_apply_title (EBulkEditContacts *self,
				  GSList *contacts,
				  EContactField field,
				  GHashTable *changed)
{
	e_bulk_edit_contacts_apply_simple (self, contacts, field, changed, EDIT_ITEM_TITLE);
}

static void
e_bulk_edit_contacts_apply_role (EBulkEditContacts *self,
				 GSList *contacts,
				 EContactField field,
				 GHashTable *changed)
{
	e_bulk_edit_contacts_apply_simple (self, contacts, field, changed, EDIT_ITEM_ROLE);
}

static void
e_bulk_edit_contacts_apply_manager (EBulkEditContacts *self,
				    GSList *contacts,
				    EContactField field,
				    GHashTable *changed)
{
	e_bulk_edit_contacts_apply_simple (self, contacts, field, changed, EDIT_ITEM_MANAGER);
}

static void
e_bulk_edit_contacts_apply_assistant (EBulkEditContacts *self,
				      GSList *contacts,
				      EContactField field,
				      GHashTable *changed)
{
	e_bulk_edit_contacts_apply_simple (self, contacts, field, changed, EDIT_ITEM_ASSISTANT);
}

static void
e_bulk_edit_contacts_apply_spouse (EBulkEditContacts *self,
				   GSList *contacts,
				   EContactField field,
				   GHashTable *changed)
{
	e_bulk_edit_contacts_apply_simple (self, contacts, field, changed, EDIT_ITEM_SPOUSE);
}

static void
e_bulk_edit_contacts_update_addr (EContact *contact,
				  EContactField addr_field,
				  EContactAddress *addr)
{
	EContactField label_field = E_CONTACT_FIELD_LAST;
	GString *str = NULL;
	GSettings *settings;
	gchar *address_label = NULL;
	gboolean format_address;

	if (addr_field == E_CONTACT_ADDRESS_WORK)
		label_field = E_CONTACT_ADDRESS_LABEL_WORK;
	else if (addr_field == E_CONTACT_ADDRESS_HOME)
		label_field = E_CONTACT_ADDRESS_LABEL_HOME;
	else if (addr_field == E_CONTACT_ADDRESS_OTHER)
		label_field = E_CONTACT_ADDRESS_LABEL_OTHER;
	else
		g_warn_if_reached ();

	e_contact_set (contact, addr_field, addr);

	if (!addr) {
		if (label_field != E_CONTACT_FIELD_LAST)
			e_contact_set (contact, label_field, NULL);
		return;
	}

	if (label_field == E_CONTACT_FIELD_LAST)
		return;

	settings = e_util_ref_settings ("org.gnome.evolution.addressbook");
	format_address = g_settings_get_boolean (settings, "address-formatting");
	g_object_unref (settings);

	if (format_address)
		address_label = eab_format_address (contact, addr_field);

	if (!format_address || !address_label) {
		str = g_string_new (NULL);

		#define append_addr_val(_str, _val, _delim) G_STMT_START { \
			if ((_val) && *(_val)) { \
				if ((_str)->len) \
					g_string_append ((_str), (_delim)); \
				g_string_append ((_str), (_val)); \
			} \
			} G_STMT_END

		append_addr_val (str, addr->street, "\n");
		append_addr_val (str, addr->ext, "\n");
		append_addr_val (str, addr->locality, "\n");
		append_addr_val (str, addr->region, ", ");
		append_addr_val (str, addr->code, "\n");
		append_addr_val (str, addr->po, "\n");
		append_addr_val (str, addr->country, "\n");

		#undef append_addr_val
	}

	e_contact_set (contact, label_field, address_label ? address_label : str->str);

	if (str)
		g_string_free (str, TRUE);

	g_free (address_label);
}

static void
e_bulk_edit_contacts_apply_address (EBulkEditContacts *self,
				    GSList *contacts,
				    EContactField field,
				    GHashTable *changed,
				    MailingAddressItem *address_item)
{
	GSList *link;
	gboolean update_street_ext = FALSE;
	gchar *street = NULL, *ext = NULL;
	const gchar *locality = NULL, *region = NULL, *code = NULL, *country = NULL, *po = NULL;

	if (!gtk_toggle_button_get_active (address_item->address.toggle) &&
	    !gtk_toggle_button_get_active (address_item->pobox.toggle) &&
	    !gtk_toggle_button_get_active (address_item->city.toggle) &&
	    !gtk_toggle_button_get_active (address_item->zip.toggle) &&
	    !gtk_toggle_button_get_active (address_item->state.toggle) &&
	    !gtk_toggle_button_get_active (address_item->country.toggle))
		return;

	if (gtk_toggle_button_get_active (address_item->address.toggle)) {
		GtkTextBuffer *text_buffer;
		GtkTextIter iter_1, iter_2;

		update_street_ext = TRUE;

		text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (address_item->address.widget));
		gtk_text_buffer_get_start_iter (text_buffer, &iter_1);

		/* Skip blank lines */
		while (gtk_text_iter_get_chars_in_line (&iter_1) < 1 &&
		       !gtk_text_iter_is_end (&iter_1))
			gtk_text_iter_forward_line (&iter_1);

		if (!gtk_text_iter_is_end (&iter_1)) {
			iter_2 = iter_1;
			gtk_text_iter_forward_to_line_end (&iter_2);

			/* Extract street (first line of text) */
			street = gtk_text_iter_get_text (&iter_1, &iter_2);

			iter_1 = iter_2;
			gtk_text_iter_forward_line (&iter_1);

			if (!gtk_text_iter_is_end (&iter_1)) {
				gtk_text_iter_forward_to_end (&iter_2);

				/* Extract extended address (remaining lines of text) */
				ext = gtk_text_iter_get_text (&iter_1, &iter_2);
			}
		}
	}

	#define get_entry_value(_into, _edit_item) G_STMT_START { \
		if (gtk_toggle_button_get_active ((_edit_item).toggle)) \
			_into = gtk_entry_get_text (GTK_ENTRY ((_edit_item).widget)); \
		} G_STMT_END

	get_entry_value (locality, address_item->city);
	get_entry_value (region, address_item->state);
	get_entry_value (code, address_item->zip);
	get_entry_value (country, address_item->country);
	get_entry_value (po, address_item->pobox);

	#undef get_entry_value

	for (link = contacts; link; link = g_slist_next (link)) {
		EContact *contact = link->data;
		EContactAddress *addr;

		addr = e_contact_get (contact, field);

		if (addr) {
			gboolean did_change = FALSE;

			#define update_member_value(_member) G_STMT_START { \
				if (_member && e_util_strcmp0 (_member, addr->_member) != 0) { \
					did_change = TRUE; \
					if (!addr) \
						addr = e_contact_address_new (); \
					addr->_member = g_strdup (_member); \
				} else if (_member && !*_member && addr->_member && *addr->_member) { \
					did_change = TRUE; \
					g_clear_pointer (&addr->_member, g_free); \
				} \
				} G_STMT_END

			update_member_value (locality);
			update_member_value (region);
			update_member_value (code);
			update_member_value (country);
			update_member_value (po);

			#undef set_member_value

			if (update_street_ext) {
				if (!street && addr->street && *addr->street) {
					did_change = TRUE;
					g_clear_pointer (&addr->street, g_free);
				} else if (street && g_strcmp0 (street, addr->street) != 0) {
					did_change = TRUE;
					g_clear_pointer (&addr->street, g_free);
					addr->street = g_strdup (street);
				}

				if (!ext && addr->ext && *addr->ext) {
					did_change = TRUE;
					g_clear_pointer (&addr->ext, g_free);
				} else if (ext && g_strcmp0 (ext, addr->ext) != 0) {
					did_change = TRUE;
					g_clear_pointer (&addr->ext, g_free);
					addr->ext = g_strdup (ext);
				}
			}

			if (did_change) {
				g_hash_table_add (changed, contact);

				if ((addr->street && *addr->street) ||
				    (addr->ext && *addr->ext) ||
				    (addr->locality && *addr->locality) ||
				    (addr->region && *addr->region) ||
				    (addr->code && *addr->code) ||
				    (addr->country && *addr->country) ||
				    (addr->po && *addr->po)) {
					e_bulk_edit_contacts_update_addr (contact, field, addr);
				} else {
					e_bulk_edit_contacts_update_addr (contact, field, NULL);
				}
			}
		} else {
			#define set_member_value(_member) G_STMT_START { \
				if (_member && *_member) { \
					if (!addr) \
						addr = e_contact_address_new (); \
					addr->_member = g_strdup (_member); \
				} \
				} G_STMT_END

			set_member_value (street);
			set_member_value (ext);
			set_member_value (locality);
			set_member_value (region);
			set_member_value (code);
			set_member_value (country);
			set_member_value (po);

			#undef set_member_value

			if (addr) {
				g_hash_table_add (changed, contact);

				e_bulk_edit_contacts_update_addr (contact, field, addr);
			}
		}

		g_clear_pointer (&addr, e_contact_address_free);
	}

	g_free (street);
	g_free (ext);
}

static void
e_bulk_edit_contacts_apply_address_home (EBulkEditContacts *self,
					 GSList *contacts,
					 EContactField field,
					 GHashTable *changed)
{
	e_bulk_edit_contacts_apply_address (self, contacts, field, changed, &self->priv->address_home);
}

static void
e_bulk_edit_contacts_apply_address_work (EBulkEditContacts *self,
					 GSList *contacts,
					 EContactField field,
					 GHashTable *changed)
{
	e_bulk_edit_contacts_apply_address (self, contacts, field, changed, &self->priv->address_work);
}

static void
e_bulk_edit_contacts_apply_address_other (EBulkEditContacts *self,
					  GSList *contacts,
					  EContactField field,
					  GHashTable *changed)
{
	e_bulk_edit_contacts_apply_address (self, contacts, field, changed, &self->priv->address_other);
}

static struct _DataMap {
	EContactField field;
	void (* apply_func) (EBulkEditContacts *self, GSList *contacts /* EContact */, EContactField field, GHashTable *changed /* EContact ~> NULL */);
	const gchar *title; /* only for E_CONTACT_FIELD_FIRST, E_CONTACT_FIELD_LAST */
} data_map[] = {
	{ E_CONTACT_FIELD_LAST, NULL, N_("Categories") }, /* tab title */
	{ E_CONTACT_CATEGORIES, e_bulk_edit_contacts_apply_categories, NULL },

	{ E_CONTACT_FIELD_LAST, NULL, N_("Personal Information") }, /* tab title */
	{ E_CONTACT_FIELD_FIRST, NULL, N_("Web Addresses") }, /* section title */
	{ E_CONTACT_HOMEPAGE_URL, e_bulk_edit_contacts_apply_homepage_url, NULL },
	{ E_CONTACT_BLOG_URL, e_bulk_edit_contacts_apply_blog_url, NULL },
	{ E_CONTACT_CALENDAR_URI, e_bulk_edit_contacts_apply_calendar_uri, NULL },
	{ E_CONTACT_FREEBUSY_URL, e_bulk_edit_contacts_apply_freebusy_url, NULL },
	{ E_CONTACT_VIDEO_URL, e_bulk_edit_contacts_apply_video_url, NULL },
	{ E_CONTACT_FIELD_FIRST, NULL, N_("Job") }, /* section title */
	{ E_CONTACT_MANAGER, e_bulk_edit_contacts_apply_manager, NULL },
	{ E_CONTACT_ASSISTANT, e_bulk_edit_contacts_apply_assistant, NULL },
	{ E_CONTACT_ROLE, e_bulk_edit_contacts_apply_role, NULL },
	{ E_CONTACT_TITLE, e_bulk_edit_contacts_apply_title, NULL },
	{ E_CONTACT_ORG, e_bulk_edit_contacts_apply_org, NULL },
	{ E_CONTACT_ORG_UNIT, e_bulk_edit_contacts_apply_org_unit, NULL },
	{ E_CONTACT_OFFICE, e_bulk_edit_contacts_apply_office, NULL },
	{ E_CONTACT_FIELD_FIRST, NULL, N_("Miscellaneous") }, /* section title */
	{ E_CONTACT_SPOUSE, e_bulk_edit_contacts_apply_spouse, NULL },

	{ E_CONTACT_FIELD_LAST, NULL, N_("Mailing Address") }, /* tab title */
	{ E_CONTACT_FIELD_FIRST, NULL, N_("Home") }, /* section title */
	{ E_CONTACT_ADDRESS_HOME, e_bulk_edit_contacts_apply_address_home, NULL },
	{ E_CONTACT_FIELD_FIRST, NULL, N_("Work") }, /* section title */
	{ E_CONTACT_ADDRESS_WORK, e_bulk_edit_contacts_apply_address_work, NULL },
	{ E_CONTACT_FIELD_FIRST, NULL, N_("Other") }, /* section title */
	{ E_CONTACT_ADDRESS_OTHER, e_bulk_edit_contacts_apply_address_other, NULL },
};

static GSList * /* (transfer full) (element-type EContact) */
e_bulk_edit_contacts_apply_changes_gui (EBulkEditContacts *self)
{
	GHashTable *changed;
	GSList *contacts = NULL, *changed_contacts = NULL, *link;
	guint ii;

	for (ii = 0; ii < self->priv->contacts->len; ii++) {
		EContact *contact = g_ptr_array_index (self->priv->contacts, ii);

		if (contact)
			contacts = g_slist_prepend (contacts, e_contact_duplicate (contact));
	}

	contacts = g_slist_reverse (contacts);
	changed = g_hash_table_new (g_direct_hash, g_direct_equal);

	for (ii = 0; ii < G_N_ELEMENTS (data_map); ii++) {
		if (data_map[ii].apply_func)
			data_map[ii].apply_func (self, contacts, data_map[ii].field, changed);
	}

	if (g_slist_length (contacts) != g_hash_table_size (changed)) {
		for (link = contacts; link; link = g_slist_next (link)) {
			EContact *contact = link->data;

			if (g_hash_table_contains (changed, contact))
				changed_contacts = g_slist_prepend (changed_contacts, g_object_ref (contact));
		}

		g_slist_free_full (contacts, g_object_unref);

		contacts = changed_contacts;
	}

	g_hash_table_destroy (changed);

	return contacts;
}

typedef struct _SaveChangesData {
	EBulkEditContacts *self;
	GSList *contacts; /* EContact * */
	gboolean success;
} SaveChangesData;

static void
e_bulk_edit_contacts_save_changes_thread (EAlertSinkThreadJobData *job_data,
					  gpointer user_data,
					  GCancellable *cancellable,
					  GError **error)
{
	SaveChangesData *scd = user_data;

	scd->success = e_book_client_modify_contacts_sync (scd->self->priv->book_client, scd->contacts, E_BOOK_OPERATION_FLAG_NONE, cancellable, error);
}

static void
e_bulk_edit_contacts_save_changes_done_cb (gpointer ptr)
{
	SaveChangesData *scd = ptr;

	if (scd->self->priv->items_grid) {
		gtk_widget_set_sensitive (scd->self->priv->items_grid, TRUE);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (scd->self), GTK_RESPONSE_APPLY, TRUE);

		if (scd->success)
			gtk_widget_destroy (GTK_WIDGET (scd->self));
	}

	g_clear_object (&scd->self->priv->cancellable);
	g_slist_free_full (scd->contacts, g_object_unref);
	g_object_unref (scd->self);
	g_free (scd);
}

static void
e_bulk_edit_contacts_response_cb (GtkDialog *dialog,
				  gint response_id,
				  gpointer user_data)
{
	EBulkEditContacts *self = E_BULK_EDIT_CONTACTS (dialog);

	g_cancellable_cancel (self->priv->cancellable);
	g_clear_object (&self->priv->cancellable);

	if (response_id == GTK_RESPONSE_APPLY) {
		SaveChangesData *scd;
		EActivity *activity;
		GSList *contacts;

		e_alert_bar_clear (E_ALERT_BAR (self->priv->alert_bar));

		contacts = e_bulk_edit_contacts_apply_changes_gui (self);

		if (!contacts) {
			gtk_widget_destroy (GTK_WIDGET (dialog));
			return;
		}

		scd = g_new0 (SaveChangesData, 1);
		scd->self = g_object_ref (self);
		scd->contacts = contacts;
		scd->success = TRUE;

		activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (self),
			_("Saving changes, please wait…"),
			"system:generic-error",
			_("Failed to save changes"),
			e_bulk_edit_contacts_save_changes_thread, scd,
			e_bulk_edit_contacts_save_changes_done_cb);

		if (activity) {
			self->priv->cancellable = e_activity_get_cancellable (activity);

			if (self->priv->cancellable)
				g_object_ref (self->priv->cancellable);

			e_activity_bar_set_activity (E_ACTIVITY_BAR (self->priv->activity_bar), activity);

			g_object_unref (activity);

			gtk_widget_set_sensitive (self->priv->items_grid, FALSE);
			gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_APPLY, FALSE);
		}
	} else {
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
}

static GHashTable * /* GINT_TO_POINTER (EContactField) ~> NULL */
e_bulk_edit_contacts_get_supported_fields (EBulkEditContacts *self)
{
	GHashTable *supported_fields;
	gchar *prop_value = NULL;

	supported_fields = g_hash_table_new (NULL, NULL);

	if (e_client_get_backend_property_sync (E_CLIENT (self->priv->book_client), E_BOOK_BACKEND_PROPERTY_SUPPORTED_FIELDS, &prop_value, NULL, NULL)) {
		GSList *field_names, *link;

		field_names = e_client_util_parse_comma_strings (prop_value);

		for (link = field_names; link; link = g_slist_next (link)) {
			const gchar *field_name = link->data;

			g_hash_table_add (supported_fields, GINT_TO_POINTER (e_contact_field_id (field_name)));
		}

		g_slist_free_full (field_names, g_free);
		g_free (prop_value);
	}

	return supported_fields;
}

static void
e_bulk_edit_contacts_add_simple_item (GtkGrid *grid,
				      gint *inout_row,
				      EditItem *edit_item,
				      const gchar *title,
				      gint column_from,
				      gint use_columns,
				      EContact *contact,
				      EContactField field,
				      gboolean sensitive)
{
	GtkWidget *widget;

	widget = gtk_check_button_new_with_mnemonic (title);
	g_object_set (widget,
		"visible", TRUE,
		"sensitive", sensitive,
		"margin-start", 12,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_CENTER,
		"active", FALSE,
		NULL);

	gtk_grid_attach (grid, widget, column_from, *inout_row, 1, 1);

	edit_item->toggle = GTK_TOGGLE_BUTTON (widget);

	widget = gtk_entry_new ();
	g_object_set (widget,
		"visible", TRUE,
		"sensitive", sensitive,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_CENTER,
		NULL);

	gtk_grid_attach (grid, widget, column_from + 1, *inout_row, use_columns, 1);

	edit_item->widget = widget;

	e_binding_bind_property (
		edit_item->toggle, "active",
		edit_item->widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	if (contact) {
		gchar *value;

		value = e_contact_get (contact, field);
		if (value && *value)
			gtk_entry_set_text (GTK_ENTRY (widget), value);

		g_free (value);
	}

	*inout_row = (*inout_row) + 1;
}

static void
e_bulk_edit_contacts_add_address (GtkGrid *grid,
				  gint *inout_row,
				  MailingAddressItem *address_item,
				  EContact *contact,
				  EContactField field,
				  gboolean sensitive)
{
	GtkWidget *widget;
	GtkWidget *scrolled_window;

	widget = gtk_check_button_new_with_mnemonic (_("_Address:"));
	g_object_set (widget,
		"visible", TRUE,
		"sensitive", sensitive,
		"margin-start", 12,
		"active", FALSE,
		NULL);

	gtk_grid_attach (grid, widget, 0, *inout_row, 1, 1);

	address_item->address.toggle = GTK_TOGGLE_BUTTON (widget);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (scrolled_window,
		"visible", TRUE,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"can-focus", FALSE,
		"width-request", 193,
		"shadow-type", GTK_SHADOW_IN,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);

	gtk_grid_attach (grid, scrolled_window, 1, *inout_row, 1, 3);

	widget = gtk_text_view_new ();
	g_object_set (widget,
		"visible", TRUE,
		"sensitive", sensitive,
		"can-focus", TRUE,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"accepts-tab", FALSE,
		NULL);

	gtk_container_add (GTK_CONTAINER (scrolled_window), widget);

	address_item->address.widget = widget;

	e_binding_bind_property (
		address_item->address.toggle, "active",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	e_bulk_edit_contacts_add_simple_item (grid, inout_row, &address_item->city, _("_City:"), 2, 1, NULL, E_CONTACT_FIELD_LAST, sensitive);
	e_bulk_edit_contacts_add_simple_item (grid, inout_row, &address_item->zip, _("_Zip/Postal Code:"), 2, 1, NULL, E_CONTACT_FIELD_LAST, sensitive);
	e_bulk_edit_contacts_add_simple_item (grid, inout_row, &address_item->state, _("_State/Province:"), 2, 1, NULL, E_CONTACT_FIELD_LAST, sensitive);
	e_bulk_edit_contacts_add_simple_item (grid, inout_row, &address_item->pobox, _("_PO Box:"), 0, 1, NULL, E_CONTACT_FIELD_LAST, sensitive);
	(*inout_row) = (*inout_row) - 1;
	e_bulk_edit_contacts_add_simple_item (grid, inout_row, &address_item->country, _("Co_untry:"), 2, 1, NULL, E_CONTACT_FIELD_LAST, sensitive);

	gtk_widget_set_hexpand (address_item->city.widget, TRUE);
	gtk_widget_set_hexpand (address_item->zip.widget, TRUE);
	gtk_widget_set_hexpand (address_item->state.widget, TRUE);
	gtk_widget_set_hexpand (address_item->country.widget, TRUE);

	if (contact) {
		EContactAddress *addr;

		addr = e_contact_get (contact, field);

		if (addr) {
			#define set_entry_value(_val, _entry) G_STMT_START { \
				if ((_val) && *(_val)) \
					gtk_entry_set_text (GTK_ENTRY (_entry), _val); \
				} G_STMT_END

			set_entry_value (addr->locality, address_item->city.widget);
			set_entry_value (addr->region, address_item->state.widget);
			set_entry_value (addr->code, address_item->zip.widget);
			set_entry_value (addr->country, address_item->country.widget);
			set_entry_value (addr->po, address_item->pobox.widget);

			#undef set_entry_value

			if ((addr->street && *addr->street) || (addr->ext && *addr->ext)) {
				GtkTextBuffer *text_buffer;
				GtkTextIter iter_start;

				text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (address_item->address.widget));
				gtk_text_buffer_set_text (text_buffer, addr->street ? addr->street : "", -1);

				if (addr->ext && *addr->ext) {
					GtkTextIter iter_end;

					gtk_text_buffer_get_end_iter (text_buffer, &iter_end);
					gtk_text_buffer_insert (text_buffer, &iter_end, "\n", -1);
					gtk_text_buffer_insert (text_buffer, &iter_end, addr->ext, -1);
				}

				gtk_text_buffer_get_iter_at_line (text_buffer, &iter_start, 0);
				gtk_text_buffer_place_cursor (text_buffer, &iter_start);
			}

			e_contact_address_free (addr);
		}
	}
}

static void
e_bulk_edit_contacts_fill_content (EBulkEditContacts *self)
{
	GHashTable *supported_fields; /* GINT_TO_POINTER (EContactField) ~> NULL */
	GtkWidget *widget;
	GtkGrid *grid, *section_grid = NULL;
	GtkNotebook *notebook;
	PangoAttrList *bold;
	const gchar *tab_title = NULL, *section_title = NULL;
	EContact *first_contact = NULL;
	gint section_row = 0;
	gchar *str;
	guint ii;

	supported_fields = e_bulk_edit_contacts_get_supported_fields (self);

	if (self->priv->contacts->len > 0)
		first_contact = g_ptr_array_index (self->priv->contacts, 0);

	bold = pango_attr_list_new ();
	pango_attr_list_insert (bold, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

	self->priv->items_grid = gtk_grid_new ();
	grid = GTK_GRID (self->priv->items_grid);

	g_object_set (grid,
		"margin", 12,
		"column-spacing", 4,
		"row-spacing", 4,
		NULL);

	str = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
		"Modify a contact",
		"Modify %u contacts", self->priv->contacts->len), self->priv->contacts->len);
	gtk_window_set_title (GTK_WINDOW (self), str);
	g_free (str);

	widget = gtk_label_new (_("Select values to be modified."));
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_CENTER,
		"margin-bottom", 4,
		"visible", TRUE,
		"xalign", 0.0,
		"yalign", 0.5,
		"wrap", TRUE,
		"width-chars", 80,
		"max-width-chars", 100,
		NULL);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	widget = gtk_notebook_new ();
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"vexpand", TRUE,
		"visible", TRUE,
		NULL);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);
	notebook = GTK_NOTEBOOK (widget);

	for (ii = 0; ii < G_N_ELEMENTS (data_map); ii++) {
		gboolean sensitive;

		if (data_map[ii].field == E_CONTACT_FIELD_LAST) {
			tab_title = data_map[ii].title;
			section_title = NULL;
			section_grid = NULL;
			section_row = 0;
			continue;
		}

		if (data_map[ii].field == E_CONTACT_FIELD_FIRST) {
			section_title = data_map[ii].title;
			continue;
		}

		sensitive = g_hash_table_contains (supported_fields, GINT_TO_POINTER (data_map[ii].field));

		if (tab_title) {
			GtkWidget *scrolled_window;

			scrolled_window = gtk_scrolled_window_new (NULL, NULL);
			g_object_set (scrolled_window,
				"visible", TRUE,
				"halign", GTK_ALIGN_FILL,
				"hexpand", TRUE,
				"valign", GTK_ALIGN_FILL,
				"vexpand", TRUE,
				"can-focus", FALSE,
				"shadow-type", GTK_SHADOW_NONE,
				"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
				"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
				"propagate-natural-width", TRUE,
				"propagate-natural-height", TRUE,
				NULL);

			gtk_notebook_append_page (notebook, scrolled_window, gtk_label_new_with_mnemonic (_(tab_title)));

			widget = gtk_grid_new ();
			g_object_set (widget,
				"visible", TRUE,
				"column-spacing", 4,
				"row-spacing", 4,
				"margin-start", 12,
				"margin-end", 12,
				"margin-top", 6, /* because section start has margin-top 6 */
				"margin-bottom", 12,
				NULL);

			gtk_container_add (GTK_CONTAINER (scrolled_window), widget);

			tab_title = NULL;
			section_grid = GTK_GRID (widget);
			section_row = 0;
		}

		if (section_title) {
			g_warn_if_fail (section_grid != NULL);

			if (section_grid) {
				widget = gtk_label_new (_(section_title));
				g_object_set (widget,
					"visible", TRUE,
					"attributes", bold,
					"halign", GTK_ALIGN_START,
					"valign", GTK_ALIGN_CENTER,
					"xalign", 0.0,
					"yalign", 0.5,
					"margin-top", 6,
					NULL);

				gtk_grid_attach (section_grid, widget, 0, section_row, 4, 1);
				section_row++;
			}

			section_title = NULL;
		}

		if (!section_grid) {
			g_warn_if_reached ();
			continue;
		}

		switch (data_map[ii].field) {
		case E_CONTACT_CATEGORIES:
			widget = e_categories_selector_new ();
			g_object_set (widget,
				"visible", TRUE,
				"halign", GTK_ALIGN_FILL,
				"valign", GTK_ALIGN_FILL,
				"hexpand", TRUE,
				"vexpand", TRUE,
				"sensitive", sensitive,
				"use-inconsistent", TRUE,
				NULL);
			gtk_grid_attach (section_grid, widget, 0, section_row, 4, 1);
			section_row++;
			self->priv->categories = E_CATEGORIES_SELECTOR (widget);

			if (first_contact) {
				gchar *value;

				value = e_contact_get (first_contact, data_map[ii].field);

				if (value && *value)
					e_categories_selector_set_checked (self->priv->categories, value);

				g_free (value);
			}

			/* To have the categories widget use the whole space, without margins */
			g_object_set (section_grid,
				"margin-start", 0,
				"margin-end", 0,
				"margin-top", 0,
				"margin-bottom", 0,
				NULL);
			break;
		case E_CONTACT_HOMEPAGE_URL:
			e_bulk_edit_contacts_add_simple_item (section_grid, &section_row, &self->priv->simple_edit_items[EDIT_ITEM_HOMEPAGE_URL],
				_("_Home Page:"), 0, 3, first_contact, data_map[ii].field, sensitive);
			break;
		case E_CONTACT_BLOG_URL:
			e_bulk_edit_contacts_add_simple_item (section_grid, &section_row, &self->priv->simple_edit_items[EDIT_ITEM_BLOG_URL],
				_("_Blog:"), 0, 3, first_contact, data_map[ii].field, sensitive);
			break;
		case E_CONTACT_CALENDAR_URI:
			e_bulk_edit_contacts_add_simple_item (section_grid, &section_row, &self->priv->simple_edit_items[EDIT_ITEM_CALENDAR_URI],
				_("_Calendar:"), 0, 3, first_contact, data_map[ii].field, sensitive);
			break;
		case E_CONTACT_FREEBUSY_URL:
			e_bulk_edit_contacts_add_simple_item (section_grid, &section_row, &self->priv->simple_edit_items[EDIT_ITEM_FREEBUSY_URL],
				_("_Free/Busy:"), 0, 3, first_contact, data_map[ii].field, sensitive);
			break;
		case E_CONTACT_VIDEO_URL:
			e_bulk_edit_contacts_add_simple_item (section_grid, &section_row, &self->priv->simple_edit_items[EDIT_ITEM_VIDEO_URL],
				_("_Video Chat:"), 0, 3, first_contact, data_map[ii].field, sensitive);
			break;
		case E_CONTACT_MANAGER:
			e_bulk_edit_contacts_add_simple_item (section_grid, &section_row, &self->priv->simple_edit_items[EDIT_ITEM_MANAGER],
				_("_Manager:"), 0, 1, first_contact, data_map[ii].field, sensitive);
			section_row--;
			break;
		case E_CONTACT_ASSISTANT:
			e_bulk_edit_contacts_add_simple_item (section_grid, &section_row, &self->priv->simple_edit_items[EDIT_ITEM_ASSISTANT],
				_("_Assistant:"), 2, 1, first_contact, data_map[ii].field, sensitive);
			break;
		case E_CONTACT_ROLE:
			e_bulk_edit_contacts_add_simple_item (section_grid, &section_row, &self->priv->simple_edit_items[EDIT_ITEM_ROLE],
				_("_Profession:"), 0, 1, first_contact, data_map[ii].field, sensitive);
			section_row--;
			break;
		case E_CONTACT_TITLE:
			e_bulk_edit_contacts_add_simple_item (section_grid, &section_row, &self->priv->simple_edit_items[EDIT_ITEM_TITLE],
				_("_Title:"), 2, 1, first_contact, data_map[ii].field, sensitive);
			break;
		case E_CONTACT_ORG:
			e_bulk_edit_contacts_add_simple_item (section_grid, &section_row, &self->priv->simple_edit_items[EDIT_ITEM_ORG],
				_("Compa_ny:"), 0, 1, first_contact, data_map[ii].field, sensitive);
			section_row--;
			break;
		case E_CONTACT_ORG_UNIT:
			e_bulk_edit_contacts_add_simple_item (section_grid, &section_row, &self->priv->simple_edit_items[EDIT_ITEM_ORG_UNIT],
				_("_Department:"), 2, 1, first_contact, data_map[ii].field, sensitive);
			break;
		case E_CONTACT_OFFICE:
			e_bulk_edit_contacts_add_simple_item (section_grid, &section_row, &self->priv->simple_edit_items[EDIT_ITEM_OFFICE],
				_("_Office:"), 0, 1, first_contact, data_map[ii].field, sensitive);
			break;
		case E_CONTACT_SPOUSE:
			e_bulk_edit_contacts_add_simple_item (section_grid, &section_row, &self->priv->simple_edit_items[EDIT_ITEM_SPOUSE],
				_("_Spouse:"), 0, 3, first_contact, data_map[ii].field, sensitive);
			break;
		case E_CONTACT_ADDRESS_HOME:
			e_bulk_edit_contacts_add_address (section_grid, &section_row, &self->priv->address_home,
				first_contact, data_map[ii].field, sensitive);
			break;
		case E_CONTACT_ADDRESS_WORK:
			e_bulk_edit_contacts_add_address (section_grid, &section_row, &self->priv->address_work,
				first_contact, data_map[ii].field, sensitive);
			break;
		case E_CONTACT_ADDRESS_OTHER:
			e_bulk_edit_contacts_add_address (section_grid, &section_row, &self->priv->address_other,
				first_contact, data_map[ii].field, sensitive);
			break;
		default:
			g_warn_if_reached ();
			break;
		}
	}

	gtk_widget_show (self->priv->items_grid);

	self->priv->alert_bar = e_alert_bar_new ();
	gtk_widget_set_margin_bottom (self->priv->alert_bar, 6);

	self->priv->activity_bar = e_activity_bar_new ();
	gtk_widget_set_margin_bottom (self->priv->activity_bar, 6);

	widget = gtk_dialog_get_content_area (GTK_DIALOG (self));
	gtk_box_pack_start (GTK_BOX (widget), self->priv->items_grid, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (widget), self->priv->alert_bar, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (widget), self->priv->activity_bar, FALSE, FALSE, 0);

	gtk_dialog_add_buttons (GTK_DIALOG (self),
		_("M_odify"), GTK_RESPONSE_APPLY,
		_("Ca_ncel"), GTK_RESPONSE_CANCEL,
		NULL);

	g_signal_connect (self, "response",
		G_CALLBACK (e_bulk_edit_contacts_response_cb), NULL);

	g_hash_table_destroy (supported_fields);
	pango_attr_list_unref (bold);
}

static void
e_bulk_edit_contacts_submit_alert (EAlertSink *alert_sink,
				   EAlert *alert)
{
	EBulkEditContacts *self;

	g_return_if_fail (E_IS_BULK_EDIT_CONTACTS (alert_sink));

	self = E_BULK_EDIT_CONTACTS (alert_sink);

	e_alert_bar_submit_alert (E_ALERT_BAR (self->priv->alert_bar), alert);
}

static void
e_bulk_edit_contacts_dispose (GObject *object)
{
	EBulkEditContacts *self = E_BULK_EDIT_CONTACTS (object);

	g_cancellable_cancel (self->priv->cancellable);
	g_clear_object (&self->priv->cancellable);

	self->priv->items_grid = NULL;
	self->priv->alert_bar = NULL;
	self->priv->activity_bar = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_bulk_edit_contacts_parent_class)->dispose (object);
}

static void
e_bulk_edit_contacts_finalize (GObject *object)
{
	EBulkEditContacts *self = E_BULK_EDIT_CONTACTS (object);

	g_clear_pointer (&self->priv->contacts, g_ptr_array_unref);
	g_clear_object (&self->priv->book_client);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_bulk_edit_contacts_parent_class)->finalize (object);
}

static void
e_bulk_edit_contacts_class_init (EBulkEditContactsClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = e_bulk_edit_contacts_dispose;
	object_class->finalize = e_bulk_edit_contacts_finalize;
}

static void
e_bulk_edit_contacts_alert_sink_init (EAlertSinkInterface *iface)
{
	iface->submit_alert = e_bulk_edit_contacts_submit_alert;
}

static void
e_bulk_edit_contacts_init (EBulkEditContacts *self)
{
	self->priv = e_bulk_edit_contacts_get_instance_private (self);
}

/**
 * e_bulk_edit_contacts_new:
 * @parent: (nullable): a parent #GtkWindow for the dialog, or %NULL
 * @book_client: an #EBookClient the @contacts belong to
 * @contacts: (element-type EContact): a #GPtrArray with #EContact-s to edit
 *
 * Creates a new dialog for bulk edit of contacts provided by the @contacts.
 *
 * Returns: (transfer full): a new #EBulkEditContacts dialog.
 *
 * Since: 3.52
 */
GtkWidget *
e_bulk_edit_contacts_new (GtkWindow *parent,
			  EBookClient *book_client,
			  GPtrArray *contacts)
{
	EBulkEditContacts *self;
	guint ii;

	g_return_val_if_fail (E_IS_BOOK_CLIENT (book_client), NULL);
	g_return_val_if_fail (contacts != NULL, NULL);

	self = g_object_new (E_TYPE_BULK_EDIT_CONTACTS,
		"transient-for", parent,
		"destroy-with-parent", TRUE,
		"modal", TRUE,
		"use-header-bar", e_util_get_use_header_bar (),
		NULL);

	self->priv->book_client = g_object_ref (book_client);
	self->priv->contacts = g_ptr_array_new_full (contacts->len, g_object_unref);

	for (ii = 0; ii < contacts->len; ii++) {
		EContact *contact = g_ptr_array_index (contacts, ii);

		if (contact)
			g_ptr_array_add (self->priv->contacts, g_object_ref (contact));
	}

	e_bulk_edit_contacts_fill_content (self);

	return GTK_WIDGET (self);
}
