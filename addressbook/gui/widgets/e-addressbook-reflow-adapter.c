/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>
#include "e-addressbook-reflow-adapter.h"
#include "e-addressbook-model.h"
#include "eab-gui-util.h"

#include "e-minicard.h"
#include <e-util/e-util.h>
#include "addressbook/printing/e-contact-print.h"

#define E_ADDRESSBOOK_REFLOW_ADAPTER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ADDRESSBOOK_REFLOW_ADAPTER, EAddressbookReflowAdapterPrivate))

struct _EAddressbookReflowAdapterPrivate {
	EAddressbookModel *model;

	gboolean loading;

	gulong create_contact_id, remove_contact_id, modify_contact_id, model_changed_id;
	gulong search_started_id, search_result_id;
	gulong notify_client_id;
};

#define d(x)

enum {
	PROP_0,
	PROP_CLIENT,
	PROP_QUERY,
	PROP_EDITABLE,
	PROP_MODEL
};

enum {
	DRAG_BEGIN,
	OPEN_CONTACT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0, };

G_DEFINE_TYPE (
	EAddressbookReflowAdapter,
	e_addressbook_reflow_adapter,
	E_TYPE_REFLOW_MODEL)

static void
unlink_model (EAddressbookReflowAdapter *adapter)
{
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	if (priv->model && priv->create_contact_id)
		g_signal_handler_disconnect (
			priv->model,
			priv->create_contact_id);
	if (priv->model && priv->remove_contact_id)
		g_signal_handler_disconnect (
			priv->model,
			priv->remove_contact_id);
	if (priv->model && priv->modify_contact_id)
		g_signal_handler_disconnect (
			priv->model,
			priv->modify_contact_id);
	if (priv->model && priv->model_changed_id)
		g_signal_handler_disconnect (
			priv->model,
			priv->model_changed_id);
	if (priv->model && priv->search_started_id)
		g_signal_handler_disconnect (
			priv->model,
			priv->search_started_id);
	if (priv->model && priv->search_result_id)
		g_signal_handler_disconnect (
			priv->model,
			priv->search_result_id);
	if (priv->model && priv->notify_client_id)
		g_signal_handler_disconnect (
			priv->model,
			priv->notify_client_id);

	priv->create_contact_id = 0;
	priv->remove_contact_id = 0;
	priv->modify_contact_id = 0;
	priv->model_changed_id = 0;
	priv->search_started_id = 0;
	priv->search_result_id = 0;
	priv->notify_client_id = 0;

	if (priv->model)
		g_object_unref (priv->model);

	priv->model = NULL;
}

static gint
text_height (PangoLayout *layout,
             const gchar *text)
{
	gint height;

	pango_layout_set_text (layout, text, -1);

	pango_layout_get_pixel_size (layout, NULL, &height);

	return height;
}

static void
addressbook_dispose (GObject *object)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER (object);

	unlink_model (adapter);
}

static void
addressbook_set_width (EReflowModel *erm,
                       gint width)
{
}

/* This function returns the number of items in our EReflowModel. */
static gint
addressbook_count (EReflowModel *erm)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER (erm);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	return e_addressbook_model_contact_count (priv->model);
}

/* This function returns the height of the minicontact in question */
static gint
addressbook_height (EReflowModel *erm,
                    gint i,
                    GnomeCanvasGroup *parent)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER (erm);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;
	EContactField field;
	gint count = 0;
	gchar *string;
	EContact *contact = (EContact *) e_addressbook_model_contact_at (priv->model, i);
	PangoLayout *layout;
	gint height;

	layout = gtk_widget_create_pango_layout (
		GTK_WIDGET (GNOME_CANVAS_ITEM (parent)->canvas), "");

	string = e_contact_get (contact, E_CONTACT_FILE_AS);
	height = text_height (layout, string ? string : "") + 10.0;
	g_free (string);

	for (field = E_CONTACT_FULL_NAME;
	     field != E_CONTACT_LAST_SIMPLE_STRING && count < 5; field++) {

		if (field == E_CONTACT_FAMILY_NAME || field == E_CONTACT_GIVEN_NAME)
			continue;

		string = e_contact_get (contact, field);
		if (string && *string) {
			gint this_height;
			gint field_text_height;

			this_height = text_height (layout, e_contact_pretty_name (field));

			field_text_height = text_height (layout, string);
			if (this_height < field_text_height)
				this_height = field_text_height;

			this_height += 3;

			height += this_height;
			count++;
		}
		g_free (string);
	}
	height += 2;

	g_object_unref (layout);

	return height;
}

static GHashTable *
addressbook_create_cmp_cache (EReflowModel *erm)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER (erm);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;
	GHashTable *cmp_cache;
	gint ii, count;

	count = e_reflow_model_count (erm);

	if (priv->loading || count <= 0)
		return NULL;

	cmp_cache = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

	for (ii = 0; ii < count; ii++) {
		EContact *contact;

		contact = (EContact *)
			e_addressbook_model_contact_at (priv->model, ii);
		if (contact != NULL) {
			const gchar *file_as;

			file_as = e_contact_get_const (
				contact, E_CONTACT_FILE_AS);
			if (file_as != NULL)
				g_hash_table_insert (
					cmp_cache, GINT_TO_POINTER (ii),
					g_utf8_collate_key (file_as, -1));
		}
	}

	return cmp_cache;
}

static gint
addressbook_compare (EReflowModel *erm,
                     gint n1,
                     gint n2,
                     GHashTable *cmp_cache)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER (erm);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;
	EContact *contact1, *contact2;

	if (priv->loading) {
		return n1 - n2;
	}
	else {
		contact1 = (EContact *) e_addressbook_model_contact_at (priv->model, n1);
		contact2 = (EContact *) e_addressbook_model_contact_at (priv->model, n2);

		if (contact1 && contact2) {
			const gchar *file_as1, *file_as2;
			const gchar *uid1, *uid2;

			if (cmp_cache) {
				file_as1 = g_hash_table_lookup (cmp_cache, GINT_TO_POINTER (n1));
				file_as2 = g_hash_table_lookup (cmp_cache, GINT_TO_POINTER (n2));
				if (file_as1 && file_as2)
					return strcmp (file_as1, file_as2);
			} else {
				file_as1 = e_contact_get_const (contact1, E_CONTACT_FILE_AS);
				file_as2 = e_contact_get_const (contact2, E_CONTACT_FILE_AS);
				if (file_as1 && file_as2)
					return g_utf8_collate (file_as1, file_as2);
			}
			if (file_as1)
				return -1;
			if (file_as2)
				return 1;
			uid1 = e_contact_get_const (contact1, E_CONTACT_UID);
			uid2 = e_contact_get_const (contact2, E_CONTACT_UID);
			if (uid1 && uid2)
				return strcmp (uid1, uid2);
			if (uid1)
				return -1;
			if (uid2)
				return 1;
		}
		if (contact1)
			return -1;
		if (contact2)
			return 1;
		return 0;
	}
}

static gint
adapter_drag_begin (EMinicard *card,
                    GdkEvent *event,
                    EAddressbookReflowAdapter *adapter)
{
	gint ret_val = 0;

	g_signal_emit (
		adapter,
		signals[DRAG_BEGIN], 0,
		event, &ret_val);

	return ret_val;
}

static void
adapter_open_contact (EMinicard *card,
                      EContact *contact,
                      EAddressbookReflowAdapter *adapter)
{
	g_signal_emit (adapter, signals[OPEN_CONTACT], 0, contact);
}

static GnomeCanvasItem *
addressbook_incarnate (EReflowModel *erm,
                       gint i,
                       GnomeCanvasGroup *parent)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER (erm);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;
	GnomeCanvasItem *item;

	item = gnome_canvas_item_new (
		parent, e_minicard_get_type (),
		"contact", e_addressbook_model_contact_at (priv->model, i),
		"editable", e_addressbook_model_get_editable (priv->model),
		NULL);

	g_signal_connect (
		item, "drag_begin",
		G_CALLBACK (adapter_drag_begin), adapter);

	g_signal_connect (
		item, "open-contact",
		G_CALLBACK (adapter_open_contact), adapter);

	return item;
}

static void
addressbook_reincarnate (EReflowModel *erm,
                         gint i,
                         GnomeCanvasItem *item)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER (erm);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	gnome_canvas_item_set (
		item,
		"contact", e_addressbook_model_contact_at (priv->model, i),
		NULL);
}

static void
create_contact (EAddressbookModel *model,
                gint index,
                gint count,
                EAddressbookReflowAdapter *adapter)
{
	e_reflow_model_items_inserted (
		E_REFLOW_MODEL (adapter),
		index,
		count);
}

static void
remove_contacts (EAddressbookModel *model,
                gpointer data,
                EAddressbookReflowAdapter *adapter)
{
	GArray *indices = (GArray *) data;
	gint count = indices->len;

	if (count == 1)
		e_reflow_model_item_removed (
			E_REFLOW_MODEL (adapter),
			g_array_index (indices, gint, 0));
	else
		e_reflow_model_changed (E_REFLOW_MODEL (adapter));

}

static void
modify_contact (EAddressbookModel *model,
                gint index,
                EAddressbookReflowAdapter *adapter)
{
	e_reflow_model_item_changed (E_REFLOW_MODEL (adapter), index);
}

static void
model_changed (EAddressbookModel *model,
               EAddressbookReflowAdapter *adapter)
{
	e_reflow_model_changed (E_REFLOW_MODEL (adapter));
}

static void
search_started (EAddressbookModel *model,
                EAddressbookReflowAdapter *adapter)
{
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	priv->loading = TRUE;
}

static void
search_result (EAddressbookModel *model,
               const GError *error,
               EAddressbookReflowAdapter *adapter)
{
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	priv->loading = FALSE;

	e_reflow_model_comparison_changed (E_REFLOW_MODEL (adapter));
}

static void
notify_client_cb (EAddressbookModel *model,
		  GParamSpec *param,
		  GObject *adapter)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_REFLOW_ADAPTER (adapter));

	g_object_notify (adapter, "client");
}

static void
addressbook_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER (object);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	switch (property_id) {
	case PROP_CLIENT:
		g_object_set (
			priv->model,
			"client", g_value_get_object (value),
			NULL);
		break;
	case PROP_QUERY:
		g_object_set (
			priv->model,
			"query", g_value_get_string (value),
			NULL);
		break;
	case PROP_EDITABLE:
		g_object_set (
			priv->model,
			"editable", g_value_get_boolean (value),
			NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
addressbook_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER (object);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	switch (property_id) {
	case PROP_CLIENT: {
		g_object_get_property (
			G_OBJECT (priv->model),
			"client", value);
		break;
	}
	case PROP_QUERY: {
		g_object_get_property (
			G_OBJECT (priv->model),
			"query", value);
		break;
	}
	case PROP_EDITABLE: {
		g_object_get_property (
			G_OBJECT (priv->model),
			"editable", value);
		break;
	}
	case PROP_MODEL:
		g_value_set_object (value, priv->model);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_addressbook_reflow_adapter_class_init (EAddressbookReflowAdapterClass *class)
{
	GObjectClass *object_class;
	EReflowModelClass *reflow_model_class;

	g_type_class_add_private (
		class, sizeof (EAddressbookReflowAdapterPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = addressbook_set_property;
	object_class->get_property = addressbook_get_property;
	object_class->dispose = addressbook_dispose;

	reflow_model_class = E_REFLOW_MODEL_CLASS (class);
	reflow_model_class->set_width = addressbook_set_width;
	reflow_model_class->count = addressbook_count;
	reflow_model_class->height = addressbook_height;
	reflow_model_class->create_cmp_cache = addressbook_create_cmp_cache;
	reflow_model_class->compare = addressbook_compare;
	reflow_model_class->incarnate = addressbook_incarnate;
	reflow_model_class->reincarnate = addressbook_reincarnate;

	g_object_class_install_property (
		object_class,
		PROP_CLIENT,
		g_param_spec_object (
			"client",
			"EBookClient",
			NULL,
			E_TYPE_BOOK_CLIENT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_QUERY,
		g_param_spec_string (
			"query",
			"Query",
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_EDITABLE,
		g_param_spec_boolean (
			"editable",
			"Editable",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_object (
			"model",
			"Model",
			NULL,
			E_TYPE_ADDRESSBOOK_MODEL,
			G_PARAM_READABLE));

	signals[DRAG_BEGIN] = g_signal_new (
		"drag_begin",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookReflowAdapterClass, drag_begin),
		NULL, NULL,
		e_marshal_INT__POINTER,
		G_TYPE_INT, 1,
		G_TYPE_POINTER);

	signals[OPEN_CONTACT] = g_signal_new (
		"open-contact",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookReflowAdapterClass, open_contact),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CONTACT);
}

static void
e_addressbook_reflow_adapter_init (EAddressbookReflowAdapter *adapter)
{
	adapter->priv = E_ADDRESSBOOK_REFLOW_ADAPTER_GET_PRIVATE (adapter);
}

void
e_addressbook_reflow_adapter_construct (EAddressbookReflowAdapter *adapter,
                                        EAddressbookModel *model)
{
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	priv->model = g_object_ref (model);

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

	priv->search_started_id = g_signal_connect (
		priv->model, "search_started",
		G_CALLBACK (search_started), adapter);

	priv->search_result_id = g_signal_connect (
		priv->model, "search_result",
		G_CALLBACK (search_result), adapter);

	priv->notify_client_id = g_signal_connect (
		priv->model, "notify::client",
		G_CALLBACK (notify_client_cb), adapter);
}

EReflowModel *
e_addressbook_reflow_adapter_new (EAddressbookModel *model)
{
	EAddressbookReflowAdapter *et;

	et = g_object_new (E_TYPE_ADDRESSBOOK_REFLOW_ADAPTER, NULL);

	e_addressbook_reflow_adapter_construct (et, model);

	return E_REFLOW_MODEL (et);
}

EContact *
e_addressbook_reflow_adapter_get_contact (EAddressbookReflowAdapter *adapter,
                                          gint index)
{
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	return e_addressbook_model_get_contact (priv->model, index);
}
