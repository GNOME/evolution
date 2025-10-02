/*
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libebook/libebook.h>

#include "e-addressbook-model.h"
#include "e-addressbook-table-adapter.h"
#include "eab-book-util.h"
#include "eab-contact-merging.h"
#include "eab-gui-util.h"
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

/* preserve order with the #define-s */
typedef enum _EAddressField {
	E_ADDRESS_FIELD_STREET,
	E_ADDRESS_FIELD_EXT,
	E_ADDRESS_FIELD_POBOX,
	E_ADDRESS_FIELD_CITY,
	E_ADDRESS_FIELD_ZIP,
	E_ADDRESS_FIELD_STATE,
	E_ADDRESS_FIELD_COUNTRY
} EAddressField;

#define E_VIRT_COLUMN_FIRST			170
#define E_VIRT_COLUMN_HOME_ADDRESS_STREET	(E_VIRT_COLUMN_FIRST)
#define E_VIRT_COLUMN_HOME_ADDRESS_EXT		(E_VIRT_COLUMN_HOME_ADDRESS_STREET + 1)
#define E_VIRT_COLUMN_HOME_ADDRESS_POBOX	(E_VIRT_COLUMN_HOME_ADDRESS_EXT + 1)
#define E_VIRT_COLUMN_HOME_ADDRESS_CITY		(E_VIRT_COLUMN_HOME_ADDRESS_POBOX + 1)
#define E_VIRT_COLUMN_HOME_ADDRESS_ZIP		(E_VIRT_COLUMN_HOME_ADDRESS_CITY + 1)
#define E_VIRT_COLUMN_HOME_ADDRESS_STATE	(E_VIRT_COLUMN_HOME_ADDRESS_ZIP + 1)
#define E_VIRT_COLUMN_HOME_ADDRESS_COUNTRY	(E_VIRT_COLUMN_HOME_ADDRESS_STATE + 1)
#define E_VIRT_COLUMN_WORK_ADDRESS_STREET	(E_VIRT_COLUMN_HOME_ADDRESS_COUNTRY + 1)
#define E_VIRT_COLUMN_WORK_ADDRESS_EXT		(E_VIRT_COLUMN_WORK_ADDRESS_STREET + 1)
#define E_VIRT_COLUMN_WORK_ADDRESS_POBOX	(E_VIRT_COLUMN_WORK_ADDRESS_EXT + 1)
#define E_VIRT_COLUMN_WORK_ADDRESS_CITY		(E_VIRT_COLUMN_WORK_ADDRESS_POBOX + 1)
#define E_VIRT_COLUMN_WORK_ADDRESS_ZIP		(E_VIRT_COLUMN_WORK_ADDRESS_CITY + 1)
#define E_VIRT_COLUMN_WORK_ADDRESS_STATE	(E_VIRT_COLUMN_WORK_ADDRESS_ZIP + 1)
#define E_VIRT_COLUMN_WORK_ADDRESS_COUNTRY	(E_VIRT_COLUMN_WORK_ADDRESS_STATE + 1)
#define E_VIRT_COLUMN_OTHER_ADDRESS_STREET	(E_VIRT_COLUMN_WORK_ADDRESS_COUNTRY + 1)
#define E_VIRT_COLUMN_OTHER_ADDRESS_EXT		(E_VIRT_COLUMN_OTHER_ADDRESS_STREET + 1)
#define E_VIRT_COLUMN_OTHER_ADDRESS_POBOX	(E_VIRT_COLUMN_OTHER_ADDRESS_EXT + 1)
#define E_VIRT_COLUMN_OTHER_ADDRESS_CITY	(E_VIRT_COLUMN_OTHER_ADDRESS_POBOX + 1)
#define E_VIRT_COLUMN_OTHER_ADDRESS_ZIP		(E_VIRT_COLUMN_OTHER_ADDRESS_CITY + 1)
#define E_VIRT_COLUMN_OTHER_ADDRESS_STATE	(E_VIRT_COLUMN_OTHER_ADDRESS_ZIP + 1)
#define E_VIRT_COLUMN_OTHER_ADDRESS_COUNTRY	(E_VIRT_COLUMN_OTHER_ADDRESS_STATE + 1)
#define E_VIRT_COLUMN_LAST			(E_VIRT_COLUMN_OTHER_ADDRESS_COUNTRY)

struct _EAddressbookTableAdapterPrivate {
	EAddressbookModel *model;

	gint create_contact_id, remove_contact_id, modify_contact_id, model_changed_id;

	GHashTable *emails;
};

/* Forward Declarations */
static void	e_addressbook_table_adapter_table_model_init
					(ETableModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EAddressbookTableAdapter, e_addressbook_table_adapter, G_TYPE_OBJECT,
	G_ADD_PRIVATE (EAddressbookTableAdapter)
	G_IMPLEMENT_INTERFACE (E_TYPE_TABLE_MODEL, e_addressbook_table_adapter_table_model_init))

static gchar *
eata_dup_address_field (EContact *contact,
			EContactField contact_field,
			EAddressField address_field)
{
	EContactAddress *address;
	const gchar *value = NULL;
	gchar *res;

	g_return_val_if_fail (E_IS_CONTACT (contact), NULL);

	address = e_contact_get (contact, contact_field);

	if (!address)
		return NULL;

	switch (address_field) {
	case E_ADDRESS_FIELD_STREET:
		value = address->street;
		break;
	case E_ADDRESS_FIELD_EXT:
		value = address->ext;
		break;
	case E_ADDRESS_FIELD_POBOX:
		value = address->po;
		break;
	case E_ADDRESS_FIELD_CITY:
		value = address->locality;
		break;
	case E_ADDRESS_FIELD_ZIP:
		value = address->code;
		break;
	case E_ADDRESS_FIELD_STATE:
		value = address->region;
		break;
	case E_ADDRESS_FIELD_COUNTRY:
		value = address->country;
		break;
	}

	if (value && *value)
		res = g_strdup (value);
	else
		res = NULL;

	e_contact_address_free (address);

	return res;
}

static void
unlink_model (EAddressbookTableAdapter *adapter)
{
	EAddressbookTableAdapterPrivate *priv = adapter->priv;

	g_signal_handler_disconnect (priv->model, priv->create_contact_id);
	g_signal_handler_disconnect (priv->model, priv->remove_contact_id);
	g_signal_handler_disconnect (priv->model, priv->modify_contact_id);
	g_signal_handler_disconnect (priv->model, priv->model_changed_id);

	priv->create_contact_id = 0;
	priv->remove_contact_id = 0;
	priv->modify_contact_id = 0;
	priv->model_changed_id = 0;

	g_object_unref (priv->model);

	priv->model = NULL;
}

static void
addressbook_finalize (GObject *object)
{
	EAddressbookTableAdapter *adapter;

	adapter = E_ADDRESSBOOK_TABLE_ADAPTER (object);

	unlink_model (adapter);

	g_hash_table_destroy (adapter->priv->emails);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_addressbook_table_adapter_parent_class)->finalize (object);
}

/* This function returns the number of columns in our ETableModel. */
static gint
addressbook_col_count (ETableModel *etc)
{
	return E_VIRT_COLUMN_LAST + 1;
}

/* This function returns the number of rows in our ETableModel. */
static gint
addressbook_row_count (ETableModel *etc)
{
	EAddressbookTableAdapter *adapter = E_ADDRESSBOOK_TABLE_ADAPTER (etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;

	return e_addressbook_model_contact_count (priv->model);
}

static void
addressbook_append_row (ETableModel *etm,
                        ETableModel *source,
                        gint row)
{
	EAddressbookTableAdapter *adapter = E_ADDRESSBOOK_TABLE_ADAPTER (etm);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	EClientCache *client_cache;
	ESourceRegistry *registry;
	EBookClient *book_client;
	EContact *contact;
	gint col;

	client_cache = e_addressbook_model_get_client_cache (priv->model);
	book_client = e_addressbook_model_get_client (priv->model);

	contact = eab_new_contact_for_book (book_client);

	for (col = 1; col < E_CONTACT_LAST_SIMPLE_STRING; col++) {
		gconstpointer val = e_table_model_value_at (source, col, row);
		e_contact_set (contact, col, (gpointer) val);
	}

	registry = e_client_cache_ref_registry (client_cache);

	eab_merging_book_add_contact (
		registry, book_client, contact, NULL, NULL, FALSE);

	g_object_unref (registry);

	g_object_unref (contact);
}

/* This function returns the value at a particular point in our ETableModel. */
static gpointer
addressbook_value_at (ETableModel *etc,
                      gint col,
                      gint row)
{
	EAddressbookTableAdapter *adapter = E_ADDRESSBOOK_TABLE_ADAPTER (etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	EContact *contact;
	const gchar *value;

	if (col >= E_CONTACT_FIELD_LAST && (col < E_VIRT_COLUMN_FIRST || col > E_VIRT_COLUMN_LAST))
		return NULL;

	if (row >= e_addressbook_model_contact_count (priv->model))
		return NULL;

	contact = e_addressbook_model_contact_at (priv->model, row);

	if (col >= E_VIRT_COLUMN_FIRST && col <= E_VIRT_COLUMN_LAST) {
		if (col >= E_VIRT_COLUMN_HOME_ADDRESS_STREET && col <= E_VIRT_COLUMN_HOME_ADDRESS_COUNTRY)
			return eata_dup_address_field (contact, E_CONTACT_ADDRESS_HOME, col - E_VIRT_COLUMN_HOME_ADDRESS_STREET);
		if (col >= E_VIRT_COLUMN_WORK_ADDRESS_STREET && col <= E_VIRT_COLUMN_WORK_ADDRESS_COUNTRY)
			return eata_dup_address_field (contact, E_CONTACT_ADDRESS_WORK, col - E_VIRT_COLUMN_WORK_ADDRESS_STREET);
		if (col >= E_VIRT_COLUMN_OTHER_ADDRESS_STREET && col <= E_VIRT_COLUMN_OTHER_ADDRESS_COUNTRY)
			return eata_dup_address_field (contact, E_CONTACT_ADDRESS_OTHER, col - E_VIRT_COLUMN_OTHER_ADDRESS_STREET);

		g_warn_if_reached ();

		return NULL;
	}

	if (col == E_CONTACT_BIRTH_DATE ||
	    col == E_CONTACT_ANNIVERSARY) {
		EContactDate *date;

		date = e_contact_get (contact, col);
		if (date) {
			gint int_dt;

			int_dt = 10000 * date->year + 100 * date->month + date->day;

			e_contact_date_free (date);

			return GINT_TO_POINTER (int_dt);
		} else {
			return GINT_TO_POINTER (-1);
		}
	}

	value = e_contact_get_const (contact, col);

	if (value && *value && (col == E_CONTACT_EMAIL_1 ||
	    col == E_CONTACT_EMAIL_2 || col == E_CONTACT_EMAIL_3)) {
		gchar *val = g_hash_table_lookup (priv->emails, value);

		if (val) {
			/* we have this already cached, so use value from the cache */
			value = val;
		} else {
			gchar *name = NULL, *mail = NULL;

			if (eab_parse_qp_email (value, &name, &mail))
				val = g_strdup_printf ("%s <%s>", name, mail);
			else
				val = g_strdup (value);

			g_free (name);
			g_free (mail);

			g_hash_table_insert (priv->emails, g_strdup (value), val);
			value = val;
		}
	}

	return g_strdup (value ? value : "");
}

/* This function sets the value at a particular point in our ETableModel. */
static void
contact_modified_cb (EBookClient *book_client,
                     const GError *error,
                     gpointer user_data)
{
	if (error != NULL)
		eab_error_dialog (
			NULL, NULL, _("Error modifying card"), error);
}

static void
addressbook_set_value_at (ETableModel *etc,
                          gint col,
                          gint row,
                          gconstpointer val)
{
	EAddressbookTableAdapter *adapter = E_ADDRESSBOOK_TABLE_ADAPTER (etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;

	if (e_addressbook_model_get_editable (priv->model)) {
		EClientCache *client_cache;
		ESourceRegistry *registry;
		EBookClient *book_client;
		EContact *contact;

		if (col >= E_CONTACT_FIELD_LAST ||
		    col == E_CONTACT_BIRTH_DATE ||
		    col == E_CONTACT_ANNIVERSARY)
			return;

		if (row >= e_addressbook_model_contact_count (priv->model))
			return;

		contact = e_addressbook_model_get_contact (priv->model, row);
		if (!contact)
			return;

		e_table_model_pre_change (etc);

		if (col == E_CONTACT_EMAIL_1 ||
		    col == E_CONTACT_EMAIL_2 ||
		    col == E_CONTACT_EMAIL_3) {
			const gchar *old_value = e_contact_get_const (contact, col);

			/* remove old value from cache and use new one */
			if (old_value && *old_value)
				g_hash_table_remove (priv->emails, old_value);
		}

		client_cache =
			e_addressbook_model_get_client_cache (priv->model);
		book_client = e_addressbook_model_get_client (priv->model);

		registry = e_client_cache_ref_registry (client_cache);

		e_contact_set (contact, col, (gpointer) val);
		eab_merging_book_modify_contact (
			registry, book_client,
			contact, contact_modified_cb, etc);

		g_object_unref (registry);

		g_object_unref (contact);

		/* XXX Do we need this?  Shouldn't the commit_contact
		 *     generate a changed signal? */
		e_table_model_cell_changed (etc, col, row);
	}
}

/* This function returns whether a particular cell is editable. */
static gboolean
addressbook_is_cell_editable (ETableModel *etc,
                              gint col,
                              gint row)
{
	return FALSE;
}

/* This function duplicates the value passed to it. */
static gpointer
addressbook_duplicate_value (ETableModel *etc,
                             gint col,
                             gconstpointer value)
{
	if (col == E_CONTACT_BIRTH_DATE ||
	    col == E_CONTACT_ANNIVERSARY)
		return GINT_TO_POINTER (GPOINTER_TO_INT (value));

	return g_strdup (value);
}

/* This function frees the value passed to it. */
static void
addressbook_free_value (ETableModel *etc,
                        gint col,
                        gpointer value)
{
	if (col != E_CONTACT_BIRTH_DATE &&
	    col != E_CONTACT_ANNIVERSARY)
		g_free (value);
}

static gpointer
addressbook_initialize_value (ETableModel *etc,
                              gint col)
{
	if (col == E_CONTACT_BIRTH_DATE ||
	    col == E_CONTACT_ANNIVERSARY)
		return GINT_TO_POINTER (-1);

	return g_strdup ("");
}

static gboolean
addressbook_value_is_empty (ETableModel *etc,
                            gint col,
                            gconstpointer value)
{
	if (col == E_CONTACT_BIRTH_DATE ||
	    col == E_CONTACT_ANNIVERSARY)
		return GPOINTER_TO_INT (value) <= 0;

	return !(value && *((const gchar *) value));
}

static gchar *
addressbook_value_to_string (ETableModel *etc,
                             gint col,
                             gconstpointer value)
{
	if (col == E_CONTACT_BIRTH_DATE ||
	    col == E_CONTACT_ANNIVERSARY) {
		gint int_dt = GPOINTER_TO_INT (value);

		if (int_dt <= 0)
			return g_strdup ("");

		return g_strdup_printf ("%04d-%02d-%02d", int_dt / 10000, (int_dt / 100) % 100, int_dt % 100);
	}

	return g_strdup (value);
}

static void
e_addressbook_table_adapter_class_init (EAddressbookTableAdapterClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = addressbook_finalize;

	#ifdef ENABLE_MAINTAINER_MODE
	if (E_CONTACT_FIELD_LAST >= E_VIRT_COLUMN_FIRST) {
		static gboolean i_know = FALSE;
		if (!i_know) {
			i_know = TRUE;
			g_warning ("%s: E_VIRT_COLUMN_FIRST (%d) should be larger than E_CONTACT_FIELD_LAST (%d). Correct it and update e-addressbook-view.etspec accordingly",
				G_STRFUNC, E_VIRT_COLUMN_FIRST, E_CONTACT_FIELD_LAST);
		}
	}
	#endif
}

static void
e_addressbook_table_adapter_table_model_init (ETableModelInterface *iface)
{
	iface->column_count = addressbook_col_count;
	iface->row_count = addressbook_row_count;
	iface->append_row = addressbook_append_row;

	iface->value_at = addressbook_value_at;
	iface->set_value_at = addressbook_set_value_at;
	iface->is_cell_editable = addressbook_is_cell_editable;

	iface->duplicate_value = addressbook_duplicate_value;
	iface->free_value = addressbook_free_value;
	iface->initialize_value = addressbook_initialize_value;
	iface->value_is_empty = addressbook_value_is_empty;
	iface->value_to_string = addressbook_value_to_string;
}

static void
e_addressbook_table_adapter_init (EAddressbookTableAdapter *adapter)
{
	adapter->priv = e_addressbook_table_adapter_get_instance_private (adapter);
}

static void
create_contact (EAddressbookModel *model,
                gint index,
                gint count,
                EAddressbookTableAdapter *adapter)
{
	e_table_model_pre_change (E_TABLE_MODEL (adapter));
	e_table_model_rows_inserted (E_TABLE_MODEL (adapter), index, count);
}

static void
remove_contacts (EAddressbookModel *model,
                gpointer data,
                EAddressbookTableAdapter *adapter)
{
	GArray *indices = (GArray *) data;
	gint count = indices->len;

	/* clear whole cache */
	g_hash_table_remove_all (adapter->priv->emails);

	e_table_model_pre_change (E_TABLE_MODEL (adapter));
	if (count == 1)
		e_table_model_rows_deleted (
			E_TABLE_MODEL (adapter),
			g_array_index (indices, gint, 0), 1);
	else
		e_table_model_changed (E_TABLE_MODEL (adapter));
}

static void
modify_contact (EAddressbookModel *model,
                gint index,
                EAddressbookTableAdapter *adapter)
{
	/* clear whole cache */
	g_hash_table_remove_all (adapter->priv->emails);

	e_table_model_pre_change (E_TABLE_MODEL (adapter));
	e_table_model_row_changed (E_TABLE_MODEL (adapter), index);
}

static void
model_changed (EAddressbookModel *model,
               EAddressbookTableAdapter *adapter)
{
	/* clear whole cache */
	g_hash_table_remove_all (adapter->priv->emails);

	e_table_model_pre_change (E_TABLE_MODEL (adapter));
	e_table_model_changed (E_TABLE_MODEL (adapter));
}

void
e_addressbook_table_adapter_construct (EAddressbookTableAdapter *adapter,
                                       EAddressbookModel *model)
{
	EAddressbookTableAdapterPrivate *priv = adapter->priv;

	priv->model = model;
	g_object_ref (priv->model);

	priv->create_contact_id = g_signal_connect (
		priv->model, "contact_added",
		G_CALLBACK (create_contact), adapter);

	priv->remove_contact_id = g_signal_connect (
		priv->model, "contacts_removed",
		G_CALLBACK (remove_contacts), adapter);

	priv->modify_contact_id = g_signal_connect (
		priv->model, "contact_changed",
		G_CALLBACK (modify_contact), adapter);

	priv->model_changed_id = g_signal_connect (
		priv->model, "model_changed",
		G_CALLBACK (model_changed), adapter);

	priv->emails = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);
}

ETableModel *
e_addressbook_table_adapter_new (EAddressbookModel *model)
{
	EAddressbookTableAdapter *et;

	et = g_object_new (E_TYPE_ADDRESSBOOK_TABLE_ADAPTER, NULL);

	e_addressbook_table_adapter_construct (et, model);

	return E_TABLE_MODEL (et);
}
