/*
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
 * Authors:
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "e-addressbook-model.h"
#include "e-addressbook-table-adapter.h"
#include "eab-contact-merging.h"
#include "eab-gui-util.h"
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

struct _EAddressbookTableAdapterPrivate {
	EABModel *model;

	gint create_contact_id, remove_contact_id, modify_contact_id, model_changed_id;

	GHashTable *emails;
};

#define PARENT_TYPE e_table_model_get_type()
static ETableModelClass *parent_class;

#define COLS (E_CONTACT_FIELD_LAST)

static void
unlink_model(EAddressbookTableAdapter *adapter)
{
	EAddressbookTableAdapterPrivate *priv = adapter->priv;

	g_signal_handler_disconnect (priv->model,
				     priv->create_contact_id);
	g_signal_handler_disconnect (priv->model,
				     priv->remove_contact_id);
	g_signal_handler_disconnect (priv->model,
				     priv->modify_contact_id);
	g_signal_handler_disconnect (priv->model,
				     priv->model_changed_id);

	priv->create_contact_id = 0;
	priv->remove_contact_id = 0;
	priv->modify_contact_id = 0;
	priv->model_changed_id = 0;

	g_object_unref (priv->model);

	priv->model = NULL;
}

static void
addressbook_dispose(GObject *object)
{
	EAddressbookTableAdapter *adapter = EAB_TABLE_ADAPTER(object);

	if (adapter->priv) {
		unlink_model(adapter);

		g_hash_table_remove_all (adapter->priv->emails);
		g_hash_table_destroy (adapter->priv->emails);

		g_free (adapter->priv);
		adapter->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

/* This function returns the number of columns in our ETableModel. */
static gint
addressbook_col_count (ETableModel *etc)
{
	return COLS;
}

/* This function returns the number of rows in our ETableModel. */
static gint
addressbook_row_count (ETableModel *etc)
{
	EAddressbookTableAdapter *adapter = EAB_TABLE_ADAPTER(etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;

	return eab_model_contact_count (priv->model);
}

/* This function returns the value at a particular point in our ETableModel. */
static gpointer
addressbook_value_at (ETableModel *etc, gint col, gint row)
{
	EAddressbookTableAdapter *adapter = EAB_TABLE_ADAPTER(etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	const gchar *value;

	if ( col >= COLS || row >= eab_model_contact_count (priv->model) )
		return NULL;

	value = e_contact_get_const((EContact*)eab_model_contact_at (priv->model, row), col);

	if (value && *value && (col == E_CONTACT_EMAIL_1 || col == E_CONTACT_EMAIL_2 || col == E_CONTACT_EMAIL_3)) {
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

	return (gpointer)(value ? value : "");
}

/* This function sets the value at a particular point in our ETableModel. */
static void
contact_modified_cb (EBook* book, EBookStatus status,
		     gpointer user_data)
{
	if (status != E_BOOK_ERROR_OK)
		eab_error_dialog (_("Error modifying card"), status);
}

static void
addressbook_set_value_at (ETableModel *etc, gint col, gint row, gconstpointer val)
{
	EAddressbookTableAdapter *adapter = EAB_TABLE_ADAPTER(etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;

	if (eab_model_editable (priv->model)) {
		EContact *contact;

		if (col >= COLS || row >= eab_model_contact_count (priv->model))
			return;

		contact = eab_model_get_contact (priv->model, row);
		if (!contact)
			return;

		e_table_model_pre_change(etc);

		if (col == E_CONTACT_EMAIL_1 || col == E_CONTACT_EMAIL_2 || col == E_CONTACT_EMAIL_3) {
			const gchar *old_value = e_contact_get_const (contact, col);

			/* remove old value from cache and use new one */
			if (old_value && *old_value)
				g_hash_table_remove (priv->emails, old_value);
		}

		e_contact_set(contact, col, (gpointer) val);
		eab_merging_book_commit_contact (eab_model_get_ebook (priv->model),
						 contact, contact_modified_cb, etc);

		g_object_unref (contact);

		/* XXX do we need this?  shouldn't the commit_contact generate a changed signal? */
		e_table_model_cell_changed(etc, col, row);
	}
}

/* This function returns whether a particular cell is editable. */
static gboolean
addressbook_is_cell_editable (ETableModel *etc, gint col, gint row)
{
#if 0
	EAddressbookTableAdapter *adapter = EAB_TABLE_ADAPTER(etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	const EContact *contact;

	if (row >= 0 && row < eab_model_contact_count (priv->model))
		contact = eab_model_contact_at (priv->model, row);
	else
		contact = NULL;

	if (!eab_model_editable(priv->model))
		return FALSE;
	else if (contact && e_contact_get ((EContact *) contact, E_CONTACT_IS_LIST))
		/* we only allow editing of the name and file as for
                   lists */
		return col == E_CONTACT_FULL_NAME || col == E_CONTACT_FILE_AS;
	else
		return col < E_CONTACT_LAST_SIMPLE_STRING;
#endif

	return FALSE;
}

static void
addressbook_append_row (ETableModel *etm, ETableModel *source, gint row)
{
	EAddressbookTableAdapter *adapter = EAB_TABLE_ADAPTER(etm);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	EContact *contact;
	gint col;

	contact = e_contact_new ();

	for (col = 1; col < E_CONTACT_LAST_SIMPLE_STRING; col++) {
		gconstpointer val = e_table_model_value_at (source, col, row);
		e_contact_set (contact, col, (gpointer) val);
	}

	eab_merging_book_add_contact (eab_model_get_ebook (priv->model), contact, NULL, NULL);

	g_object_unref (contact);
}

/* This function duplicates the value passed to it. */
static gpointer
addressbook_duplicate_value (ETableModel *etc, gint col, gconstpointer value)
{
	return g_strdup(value);
}

/* This function frees the value passed to it. */
static void
addressbook_free_value (ETableModel *etc, gint col, gpointer value)
{
	g_free(value);
}

static gpointer
addressbook_initialize_value (ETableModel *etc, gint col)
{
	return g_strdup("");
}

static gboolean
addressbook_value_is_empty (ETableModel *etc, gint col, gconstpointer value)
{
	return !(value && *(gchar *)value);
}

static gchar *
addressbook_value_to_string (ETableModel *etc, gint col, gconstpointer value)
{
	return g_strdup(value);
}

static void
eab_table_adapter_class_init (GObjectClass *object_class)
{
	ETableModelClass *model_class = (ETableModelClass *) object_class;

	parent_class = g_type_class_peek_parent (object_class);

	object_class->dispose = addressbook_dispose;

	model_class->column_count = addressbook_col_count;
	model_class->row_count = addressbook_row_count;
	model_class->value_at = addressbook_value_at;
	model_class->set_value_at = addressbook_set_value_at;
	model_class->is_cell_editable = addressbook_is_cell_editable;
	model_class->append_row = addressbook_append_row;
	model_class->duplicate_value = addressbook_duplicate_value;
	model_class->free_value = addressbook_free_value;
	model_class->initialize_value = addressbook_initialize_value;
	model_class->value_is_empty = addressbook_value_is_empty;
	model_class->value_to_string = addressbook_value_to_string;
}

static void
eab_table_adapter_init (GObject *object)
{
	EAddressbookTableAdapter *adapter = EAB_TABLE_ADAPTER(object);
	EAddressbookTableAdapterPrivate *priv;

	priv = adapter->priv = g_new0 (EAddressbookTableAdapterPrivate, 1);

	priv->create_contact_id = 0;
	priv->remove_contact_id = 0;
	priv->modify_contact_id = 0;
	priv->model_changed_id = 0;
}

static void
create_contact (EABModel *model,
		gint index, gint count,
		EAddressbookTableAdapter *adapter)
{
	e_table_model_pre_change (E_TABLE_MODEL (adapter));
	e_table_model_rows_inserted (E_TABLE_MODEL (adapter), index, count);
}

static void
remove_contacts (EABModel *model,
		gpointer data,
		EAddressbookTableAdapter *adapter)
{
	GArray *indices = (GArray *) data;
	gint count = indices->len;

	/* clear whole cache */
	g_hash_table_remove_all (adapter->priv->emails);

	e_table_model_pre_change (E_TABLE_MODEL (adapter));
	if (count == 1)
		e_table_model_rows_deleted (E_TABLE_MODEL (adapter), g_array_index (indices, gint, 0), 1);
	else
		e_table_model_changed (E_TABLE_MODEL (adapter));
}

static void
modify_contact (EABModel *model,
		gint index,
		EAddressbookTableAdapter *adapter)
{
	/* clear whole cache */
	g_hash_table_remove_all (adapter->priv->emails);

	e_table_model_pre_change (E_TABLE_MODEL (adapter));
	e_table_model_row_changed (E_TABLE_MODEL (adapter), index);
}

static void
model_changed (EABModel *model,
	       EAddressbookTableAdapter *adapter)
{
	/* clear whole cache */
	g_hash_table_remove_all (adapter->priv->emails);

	e_table_model_pre_change (E_TABLE_MODEL (adapter));
	e_table_model_changed (E_TABLE_MODEL (adapter));
}

GType
eab_table_adapter_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (EAddressbookTableAdapterClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) eab_table_adapter_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EAddressbookTableAdapter),
			0,             /* n_preallocs */
			(GInstanceInitFunc) eab_table_adapter_init,
		};

		type = g_type_register_static (PARENT_TYPE, "EAddressbookTableAdapter", &info, 0);
	}

	return type;
}

void
eab_table_adapter_construct (EAddressbookTableAdapter *adapter,
				       EABModel *model)
{
	EAddressbookTableAdapterPrivate *priv = adapter->priv;

	priv->model = model;
	g_object_ref (priv->model);

	priv->create_contact_id = g_signal_connect(priv->model,
						   "contact_added",
						   G_CALLBACK(create_contact),
						   adapter);
	priv->remove_contact_id = g_signal_connect(priv->model,
						   "contacts_removed",
						   G_CALLBACK(remove_contacts),
						   adapter);
	priv->modify_contact_id = g_signal_connect(priv->model,
						   "contact_changed",
						   G_CALLBACK(modify_contact),
						   adapter);
	priv->model_changed_id = g_signal_connect(priv->model,
						  "model_changed",
						  G_CALLBACK(model_changed),
						  adapter);

	priv->emails = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free, (GDestroyNotify) g_free);
}

ETableModel *
eab_table_adapter_new (EABModel *model)
{
	EAddressbookTableAdapter *et;

	et = g_object_new(E_TYPE_AB_TABLE_ADAPTER, NULL);

	eab_table_adapter_construct (et, model);

	return E_TABLE_MODEL(et);
}
