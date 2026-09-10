/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include "eab-gui-util.h"
#include "eab-book-util.h"

#include "e-addressbook-table.h"

void		_e_addressbook_table_test_enable_local_mode
							(EAddressbookTable *self,
							 gboolean enable);
void		_e_addressbook_table_test_push_local_order
							(EAddressbookTable *self);
void		_e_addressbook_table_test_local_add_contact
							(EAddressbookTable *self,
							 EContact *contact);
void		_e_addressbook_table_test_local_modify_contact
							(EAddressbookTable *self,
							 EContact *contact);
void		_e_addressbook_table_test_local_remove_contact
							(EAddressbookTable *self,
							 const gchar *uid);
void		_e_addressbook_table_test_server_set_total
							(EAddressbookTable *self,
							 guint total);
void		_e_addressbook_table_test_server_set_contact
							(EAddressbookTable *self,
							 guint index,
							 EContact *contact);
void		_e_addressbook_table_test_capture_pending_select
							(EAddressbookTable *self);

/* Column definitions - order matches the legacy e-addressbook-view.etspec
 * column order exactly, so it can double as the legacy column id map used
 * for migrating old ETableState/GalViewEtable data. */
typedef enum {
	EAB_COL_SIMPLE,
	EAB_COL_EMAIL,
	EAB_COL_DATE,
	EAB_COL_ADDRESS
} EABColumnKind;

typedef enum {
	EAB_ADDR_STREET,
	EAB_ADDR_EXT,
	EAB_ADDR_POBOX,
	EAB_ADDR_CITY,
	EAB_ADDR_ZIP,
	EAB_ADDR_STATE,
	EAB_ADDR_COUNTRY
} EABAddressField;

typedef struct _EABColumnDef {
	const gchar *id;
	const gchar *title;
	EABColumnKind kind;
	EContactField field;
	EABAddressField address_field;
} EABColumnDef;

static const EABColumnDef eab_table_columns[] = {
	{ "file-as", N_("File As"), EAB_COL_SIMPLE, E_CONTACT_FILE_AS, 0 },
	{ "full-name", N_("Full Name"), EAB_COL_SIMPLE, E_CONTACT_FULL_NAME, 0 },
	{ "given-name", N_("Given Name"), EAB_COL_SIMPLE, E_CONTACT_GIVEN_NAME, 0 },
	{ "family-name", N_("Family Name"), EAB_COL_SIMPLE, E_CONTACT_FAMILY_NAME, 0 },
	{ "nickname", N_("Nickname"), EAB_COL_SIMPLE, E_CONTACT_NICKNAME, 0 },
	{ "email-1", N_("Email"), EAB_COL_EMAIL, E_CONTACT_EMAIL_1, 0 },
	{ "email-2", N_("Email 2"), EAB_COL_EMAIL, E_CONTACT_EMAIL_2, 0 },
	{ "email-3", N_("Email 3"), EAB_COL_EMAIL, E_CONTACT_EMAIL_3, 0 },
	{ "assistant-phone", N_("Assistant Phone"), EAB_COL_SIMPLE, E_CONTACT_PHONE_ASSISTANT, 0 },
	{ "business-phone", N_("Business Phone"), EAB_COL_SIMPLE, E_CONTACT_PHONE_BUSINESS, 0 },
	{ "business-phone-2", N_("Business Phone 2"), EAB_COL_SIMPLE, E_CONTACT_PHONE_BUSINESS_2, 0 },
	{ "business-fax", N_("Business Fax"), EAB_COL_SIMPLE, E_CONTACT_PHONE_BUSINESS_FAX, 0 },
	{ "callback-phone", N_("Callback Phone"), EAB_COL_SIMPLE, E_CONTACT_PHONE_CALLBACK, 0 },
	{ "car-phone", N_("Car Phone"), EAB_COL_SIMPLE, E_CONTACT_PHONE_CAR, 0 },
	{ "company-phone", N_("Company Phone"), EAB_COL_SIMPLE, E_CONTACT_PHONE_COMPANY, 0 },
	{ "home-phone", N_("Home Phone"), EAB_COL_SIMPLE, E_CONTACT_PHONE_HOME, 0 },
	{ "home-phone-2", N_("Home Phone 2"), EAB_COL_SIMPLE, E_CONTACT_PHONE_HOME_2, 0 },
	{ "home-fax", N_("Home Fax"), EAB_COL_SIMPLE, E_CONTACT_PHONE_HOME_FAX, 0 },
	{ "isdn-phone", N_("ISDN Phone"), EAB_COL_SIMPLE, E_CONTACT_PHONE_ISDN, 0 },
	{ "mobile-phone", N_("Mobile Phone"), EAB_COL_SIMPLE, E_CONTACT_PHONE_MOBILE, 0 },
	{ "other-phone", N_("Other Phone"), EAB_COL_SIMPLE, E_CONTACT_PHONE_OTHER, 0 },
	{ "other-fax", N_("Other Fax"), EAB_COL_SIMPLE, E_CONTACT_PHONE_OTHER_FAX, 0 },
	{ "pager", N_("Pager"), EAB_COL_SIMPLE, E_CONTACT_PHONE_PAGER, 0 },
	{ "primary-phone", N_("Primary Phone"), EAB_COL_SIMPLE, E_CONTACT_PHONE_PRIMARY, 0 },
	{ "radio", N_("Radio"), EAB_COL_SIMPLE, E_CONTACT_PHONE_RADIO, 0 },
	{ "telex", N_("Telex"), EAB_COL_SIMPLE, E_CONTACT_PHONE_TELEX, 0 },
	{ "ttytdd", N_("TTYTDD"), EAB_COL_SIMPLE, E_CONTACT_PHONE_TTYTDD, 0 },
	{ "company", N_("Company"), EAB_COL_SIMPLE, E_CONTACT_ORG, 0 },
	{ "unit", N_("Unit"), EAB_COL_SIMPLE, E_CONTACT_ORG_UNIT, 0 },
	{ "office", N_("Office"), EAB_COL_SIMPLE, E_CONTACT_OFFICE, 0 },
	{ "title", N_("Title"), EAB_COL_SIMPLE, E_CONTACT_TITLE, 0 },
	{ "role", N_("Role"), EAB_COL_SIMPLE, E_CONTACT_ROLE, 0 },
	{ "manager", N_("Manager"), EAB_COL_SIMPLE, E_CONTACT_MANAGER, 0 },
	{ "assistant", N_("Assistant"), EAB_COL_SIMPLE, E_CONTACT_ASSISTANT, 0 },
	{ "web-site", N_("Web Site"), EAB_COL_SIMPLE, E_CONTACT_HOMEPAGE_URL, 0 },
	{ "journal", N_("Journal"), EAB_COL_SIMPLE, E_CONTACT_BLOG_URL, 0 },
	{ "categories", N_("Categories"), EAB_COL_SIMPLE, E_CONTACT_CATEGORIES, 0 },
	{ "spouse", N_("Spouse"), EAB_COL_SIMPLE, E_CONTACT_SPOUSE, 0 },
	{ "note", N_("Note"), EAB_COL_SIMPLE, E_CONTACT_NOTE, 0 },
	{ "birthday", N_("Birthday"), EAB_COL_DATE, E_CONTACT_BIRTH_DATE, 0 },
	{ "anniversary", N_("Anniversary"), EAB_COL_DATE, E_CONTACT_ANNIVERSARY, 0 },
	{ "home-address-street", N_("Home Address Street"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_HOME, EAB_ADDR_STREET },
	{ "home-address-ext", N_("Home Address Extended"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_HOME, EAB_ADDR_EXT },
	{ "home-address-pobox", N_("Home Address PO Box"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_HOME, EAB_ADDR_POBOX },
	{ "home-address-city", N_("Home Address City"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_HOME, EAB_ADDR_CITY },
	{ "home-address-zip", N_("Home Address Zip/Postal code"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_HOME, EAB_ADDR_ZIP },
	{ "home-address-state", N_("Home Address State/Province"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_HOME, EAB_ADDR_STATE },
	{ "home-address-country", N_("Home Address Country"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_HOME, EAB_ADDR_COUNTRY },
	{ "work-address-street", N_("Work Address Street"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_WORK, EAB_ADDR_STREET },
	{ "work-address-ext", N_("Work Address Extended"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_WORK, EAB_ADDR_EXT },
	{ "work-address-pobox", N_("Work Address PO Box"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_WORK, EAB_ADDR_POBOX },
	{ "work-address-city", N_("Work Address City"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_WORK, EAB_ADDR_CITY },
	{ "work-address-zip", N_("Work Address Zip/Postal code"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_WORK, EAB_ADDR_ZIP },
	{ "work-address-state", N_("Work Address State/Province"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_WORK, EAB_ADDR_STATE },
	{ "work-address-country", N_("Work Address Country"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_WORK, EAB_ADDR_COUNTRY },
	{ "other-address-street", N_("Other Address Street"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_OTHER, EAB_ADDR_STREET },
	{ "other-address-ext", N_("Other Address Extended"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_OTHER, EAB_ADDR_EXT },
	{ "other-address-pobox", N_("Other Address PO Box"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_OTHER, EAB_ADDR_POBOX },
	{ "other-address-city", N_("Other Address City"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_OTHER, EAB_ADDR_CITY },
	{ "other-address-zip", N_("Other Address Zip/Postal code"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_OTHER, EAB_ADDR_ZIP },
	{ "other-address-state", N_("Other Address State/Province"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_OTHER, EAB_ADDR_STATE },
	{ "other-address-country", N_("Other Address Country"), EAB_COL_ADDRESS, E_CONTACT_ADDRESS_OTHER, EAB_ADDR_COUNTRY }
};

#define N_EAB_TABLE_COLUMNS (G_N_ELEMENTS (eab_table_columns))

/* Default visible columns for a brand new (unmigrated) view. */
static const gchar *eab_default_visible_columns[] = {
	"file-as", "full-name", "email-1", "home-phone", "company"
};

static gboolean
eab_column_is_sortable (const EABColumnDef *def)
{
	return def->kind == EAB_COL_SIMPLE &&
		(def->field == E_CONTACT_FILE_AS ||
		 def->field == E_CONTACT_GIVEN_NAME ||
		 def->field == E_CONTACT_FAMILY_NAME);
}

static gchar *
eab_dup_address_field (EContact *contact,
		       EContactField contact_field,
		       EABAddressField address_field)
{
	EContactAddress *address;
	const gchar *value = NULL;
	gchar *res = NULL;

	address = e_contact_get (contact, contact_field);
	if (!address)
		return NULL;

	switch (address_field) {
	case EAB_ADDR_STREET:
		value = address->street;
		break;
	case EAB_ADDR_EXT:
		value = address->ext;
		break;
	case EAB_ADDR_POBOX:
		value = address->po;
		break;
	case EAB_ADDR_CITY:
		value = address->locality;
		break;
	case EAB_ADDR_ZIP:
		value = address->code;
		break;
	case EAB_ADDR_STATE:
		value = address->region;
		break;
	case EAB_ADDR_COUNTRY:
		value = address->country;
		break;
	}

	if (value && *value)
		res = g_strdup (value);

	e_contact_address_free (address);

	return res;
}

static gchar *
eab_table_dup_column_text (EContact *contact,
			   const EABColumnDef *def)
{
	if (!contact)
		return NULL;

	switch (def->kind) {
	case EAB_COL_SIMPLE: {
		const gchar *value = e_contact_get_const (contact, def->field);

		return (value && *value) ? g_strdup (value) : NULL;
	}
	case EAB_COL_EMAIL: {
		const gchar *value = e_contact_get_const (contact, def->field);
		gchar *name = NULL, *mail = NULL, *res;

		if (!value || !*value)
			return NULL;

		if (eab_parse_qp_email (value, &name, &mail))
			res = g_strdup_printf ("%s <%s>", name, mail);
		else
			res = g_strdup (value);

		g_free (name);
		g_free (mail);

		return res;
	}
	case EAB_COL_DATE: {
		EContactDate *date = e_contact_get (contact, def->field);
		gchar *res = NULL;

		if (date) {
			if (g_date_valid_dmy (date->day, date->month, date->year)) {
				GDate gdate;
				gchar buff[128];

				g_date_clear (&gdate, 1);
				g_date_set_dmy (&gdate, date->day, date->month, date->year);

				if (g_date_strftime (buff, sizeof (buff), "%x", &gdate) > 0)
					res = g_strdup (buff);
			}

			e_contact_date_free (date);
		}

		return res;
	}
	case EAB_COL_ADDRESS:
		return eab_dup_address_field (contact, def->field, def->address_field);
	}

	return NULL;
}

#define E_TYPE_ADDRESSBOOK_TABLE_NODE (e_addressbook_table_node_get_type ())
G_DECLARE_FINAL_TYPE (EAddressbookTableNode, e_addressbook_table_node, E, ADDRESSBOOK_TABLE_NODE, GObject)

struct _EAddressbookTableNode {
	GObject parent_instance;
	EContact *contact; /* nullable */
	guint row_index;
	gboolean is_group;
	gchar *group_label;
	gchar *group_key;
	guint depth;
	gboolean expandable;
	gboolean expanded;
};

G_DEFINE_FINAL_TYPE (EAddressbookTableNode, e_addressbook_table_node, G_TYPE_OBJECT)

static void
e_addressbook_table_node_finalize (GObject *object)
{
	EAddressbookTableNode *self = E_ADDRESSBOOK_TABLE_NODE (object);

	g_clear_object (&self->contact);
	g_free (self->group_label);
	g_free (self->group_key);

	G_OBJECT_CLASS (e_addressbook_table_node_parent_class)->finalize (object);
}

static void
e_addressbook_table_node_class_init (EAddressbookTableNodeClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = e_addressbook_table_node_finalize;
}

static void
e_addressbook_table_node_init (EAddressbookTableNode *self)
{
}

typedef struct _EABGroupNode {
	struct _EABGroupNode *parent;
	struct _EABGroupNode *child;
	struct _EABGroupNode *next;
	gboolean is_group;
	gchar *group_label;
	gchar *group_key;
	gboolean expanded;
	guint depth;
	EContact *contact; /* owned ref, iff !is_group */
} EABGroupNode;

typedef struct _OrderColumn {
	guint col_idx;
	GtkSortType order;
	gboolean is_group;
} OrderColumn;

static EABGroupNode *
eab_group_node_new_leaf (EContact *contact)
{
	EABGroupNode *node;

	node = g_new0 (EABGroupNode, 1);
	node->contact = g_object_ref (contact);

	return node;
}

static void
eab_group_node_free_leaf (gpointer ptr)
{
	EABGroupNode *node = ptr;

	g_clear_object (&node->contact);
	g_free (node);
}

static void
eab_group_tree_free (EABGroupNode *node,
		     guint remaining_levels)
{
	EABGroupNode *next;

	if (remaining_levels == 0)
		return;

	while (node) {
		next = node->next;

		eab_group_tree_free (node->child, remaining_levels - 1);
		g_free (node->group_label);
		g_free (node->group_key);
		g_free (node);

		node = next;
	}
}

typedef struct _RangeRead {
	GWeakRef self_weakref;
	guint stamp;
	guint first_row;
	guint count;
} RangeRead;

static void
range_read_free (RangeRead *rr)
{
	g_weak_ref_clear (&rr->self_weakref);
	g_free (rr);
}

#define E_TYPE_ADDRESSBOOK_TABLE_MODEL (e_addressbook_table_model_get_type ())
G_DECLARE_FINAL_TYPE (EAddressbookTableModel, e_addressbook_table_model, E, ADDRESSBOOK_TABLE_MODEL, GObject)

struct _EAddressbookTableModel {
	GObject parent_instance;
	EBookClientView *book_view; /* owned, nullable; lazy mode only */
	GPtrArray *cache; /* of EContact *, nullable entries; lazy mode only */
	guint stamp;
	GSList *range_read_queue; /* of RangeRead * */
	RangeRead *ongoing_range_read;
	gulong sig_content_changed;
	gulong sig_notify_n_total;

	gboolean local_mode;
	GHashTable *contacts_by_uid; /* gchar *uid -> EABGroupNode * (leaf), owned */
	GArray *order_columns; /* OrderColumn */
	guint n_group_levels;
	guint root_group_levels; /* n_group_levels that "root" was actually built with */
	EABGroupNode *root; /* is_group wrapper nodes, or leaf chain if n_group_levels == 0 */
	GPtrArray *visible_rows; /* borrowed EABGroupNode *, flattened from root */
	gboolean visible_dirty;
	GHashTable *collapsed_group_keys; /* set of gchar *group_key, session-only */
	guint local_rebuild_idle_id;
	GPtrArray *pending_free_leaves; /* EABGroupNode * */
};

static void e_addressbook_table_model_iface_init (EVirtualTreeModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EAddressbookTableModel, e_addressbook_table_model, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (E_TYPE_VIRTUAL_TREE_MODEL, e_addressbook_table_model_iface_init))

static void
atm_got_contacts_cb (GObject *source_object,
		     GAsyncResult *result,
		     gpointer user_data);

static gint
atm_compare_order_column (EContact *contact_a,
			  EContact *contact_b,
			  const OrderColumn *order_column)
{
	gint result;

	if (eab_table_columns[order_column->col_idx].kind == EAB_COL_DATE) {
		EContactDate *date_a = e_contact_get (contact_a, eab_table_columns[order_column->col_idx].field);
		EContactDate *date_b = e_contact_get (contact_b, eab_table_columns[order_column->col_idx].field);
		gint value_a = date_a ? (gint) (date_a->year * 10000 + date_a->month * 100 + date_a->day) : -1;
		gint value_b = date_b ? (gint) (date_b->year * 10000 + date_b->month * 100 + date_b->day) : -1;

		result = (value_a > value_b) - (value_a < value_b);

		if (date_a)
			e_contact_date_free (date_a);
		if (date_b)
			e_contact_date_free (date_b);
	} else {
		gchar *text_a = eab_table_dup_column_text (contact_a, &eab_table_columns[order_column->col_idx]);
		gchar *text_b = eab_table_dup_column_text (contact_b, &eab_table_columns[order_column->col_idx]);

		result = g_utf8_collate (text_a ? text_a : "", text_b ? text_b : "");

		g_free (text_a);
		g_free (text_b);
	}

	if (order_column->order == GTK_SORT_DESCENDING)
		result = -result;

	return result;
}

static gint
atm_compare_contacts (gconstpointer aa,
		      gconstpointer bb,
		      gpointer user_data)
{
	EABGroupNode *node_a = *(EABGroupNode * const *) aa;
	EABGroupNode *node_b = *(EABGroupNode * const *) bb;
	GArray *order_columns = user_data;
	guint ii;
	gint result = 0;

	for (ii = 0; ii < order_columns->len && result == 0; ii++)
		result = atm_compare_order_column (node_a->contact, node_b->contact, &g_array_index (order_columns, OrderColumn, ii));

	if (result == 0)
		result = g_strcmp0 (
			e_contact_get_const (node_a->contact, E_CONTACT_UID),
			e_contact_get_const (node_b->contact, E_CONTACT_UID));

	return result;
}

static gchar *
atm_compute_group_label (EContact *contact,
			 guint col_idx)
{
	gchar *text;

	text = eab_table_dup_column_text (contact, &eab_table_columns[col_idx]);

	if (text && *text)
		return text;

	g_free (text);

	return g_strdup (_("(None)"));
}

static gchar *
atm_compute_group_key (const gchar *parent_key,
		       guint col_idx,
		       const gchar *label)
{
	return g_strdup_printf ("%s\x1f%u\x1f%s", parent_key ? parent_key : "", col_idx, label);
}

static EABGroupNode *
atm_build_group_level (EAddressbookTableModel *self,
		       GPtrArray *sorted,
		       guint start,
		       guint end,
		       guint order_idx,
		       const gchar *parent_key,
		       guint depth)
{
	EABGroupNode *head = NULL, *tail = NULL;
	EABGroupNode *leaf;
	EABGroupNode *group;
	EABGroupNode *child;
	const OrderColumn *order_column;
	gchar *label;
	gchar *other_label;
	gboolean same;
	guint ii, run_end;

	if (order_idx >= self->n_group_levels) {
		for (ii = start; ii < end; ii++) {
			leaf = g_ptr_array_index (sorted, ii);
			leaf->parent = NULL;
			leaf->next = NULL;
			leaf->depth = depth;

			if (tail)
				tail->next = leaf;
			else
				head = leaf;

			tail = leaf;
		}

		return head;
	}

	order_column = &g_array_index (self->order_columns, OrderColumn, order_idx);
	ii = start;

	while (ii < end) {
		label = atm_compute_group_label (((EABGroupNode *) g_ptr_array_index (sorted, ii))->contact, order_column->col_idx);

		run_end = ii + 1;
		while (run_end < end) {
			other_label = atm_compute_group_label (((EABGroupNode *) g_ptr_array_index (sorted, run_end))->contact, order_column->col_idx);
			same = g_strcmp0 (label, other_label) == 0;
			g_free (other_label);

			if (!same)
				break;

			run_end++;
		}

		group = g_new0 (EABGroupNode, 1);
		group->is_group = TRUE;
		group->group_label = label;
		group->group_key = atm_compute_group_key (parent_key, order_column->col_idx, label);
		group->expanded = !g_hash_table_contains (self->collapsed_group_keys, group->group_key);
		group->depth = depth;
		group->child = atm_build_group_level (self, sorted, ii, run_end, order_idx + 1, group->group_key, depth + 1);

		for (child = group->child; child; child = child->next)
			child->parent = group;

		if (tail)
			tail->next = group;
		else
			head = group;

		tail = group;
		ii = run_end;
	}

	return head;
}

static void
atm_rebuild_local_tree (EAddressbookTableModel *self)
{
	GPtrArray *sorted;
	GHashTableIter iter;
	gpointer value;

	eab_group_tree_free (self->root, self->root_group_levels);
	self->root = NULL;

	sorted = g_ptr_array_new ();

	g_hash_table_iter_init (&iter, self->contacts_by_uid);
	while (g_hash_table_iter_next (&iter, NULL, &value))
		g_ptr_array_add (sorted, value);

	g_ptr_array_sort_with_data (sorted, atm_compare_contacts, self->order_columns);

	self->root = atm_build_group_level (self, sorted, 0, sorted->len, 0, NULL, 0);
	self->root_group_levels = self->n_group_levels;

	g_ptr_array_unref (sorted);

	g_ptr_array_set_size (self->pending_free_leaves, 0);

	self->visible_dirty = TRUE;
}

static void
atm_flatten_recursive (EABGroupNode *node,
		       GPtrArray *out)
{
	while (node) {
		g_ptr_array_add (out, node);

		if (node->is_group && node->child && node->expanded)
			atm_flatten_recursive (node->child, out);

		node = node->next;
	}
}

static void
atm_ensure_visible_rows (EAddressbookTableModel *self)
{
	if (!self->visible_dirty)
		return;

	g_ptr_array_set_size (self->visible_rows, 0);

	atm_flatten_recursive (self->root, self->visible_rows);

	self->visible_dirty = FALSE;
}

static void
atm_local_rebuild_and_notify (EAddressbookTableModel *self)
{
	e_virtual_tree_model_emit_before_rebuild (E_VIRTUAL_TREE_MODEL (self));

	atm_rebuild_local_tree (self);

	e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (self));
	e_virtual_tree_model_emit_after_rebuild (E_VIRTUAL_TREE_MODEL (self));
}

static gboolean
atm_local_rebuild_idle_cb (gpointer user_data)
{
	EAddressbookTableModel *self = user_data;

	self->local_rebuild_idle_id = 0;

	atm_local_rebuild_and_notify (self);

	return G_SOURCE_REMOVE;
}

static void
atm_local_schedule_rebuild (EAddressbookTableModel *self)
{
	if (self->local_rebuild_idle_id)
		return;

	self->local_rebuild_idle_id = g_idle_add (atm_local_rebuild_idle_cb, self);
}

static void
atm_local_cancel_rebuild (EAddressbookTableModel *self)
{
	if (self->local_rebuild_idle_id) {
		g_source_remove (self->local_rebuild_idle_id);
		self->local_rebuild_idle_id = 0;
	}
}

static EABGroupNode *
atm_find_group_node (EABGroupNode *node,
		     guint remaining_levels,
		     const gchar *group_key)
{
	EABGroupNode *found;

	if (remaining_levels == 0)
		return NULL;

	while (node) {
		if (g_strcmp0 (node->group_key, group_key) == 0)
			return node;

		found = atm_find_group_node (node->child, remaining_levels - 1, group_key);
		if (found)
			return found;

		node = node->next;
	}

	return NULL;
}

static void
e_addressbook_table_model_local_add (EAddressbookTableModel *self,
				     EContact *contact)
{
	const gchar *uid = e_contact_get_const (contact, E_CONTACT_UID);
	EABGroupNode *leaf;

	if (!uid)
		return;

	leaf = g_hash_table_lookup (self->contacts_by_uid, uid);

	if (leaf) {
		g_object_unref (leaf->contact);
		leaf->contact = g_object_ref (contact);
	} else {
		leaf = eab_group_node_new_leaf (contact);
		g_hash_table_insert (self->contacts_by_uid, g_strdup (uid), leaf);
	}

	atm_local_schedule_rebuild (self);
}

static void
e_addressbook_table_model_local_remove (EAddressbookTableModel *self,
					const gchar *uid)
{
	gpointer stolen_key = NULL;
	gpointer stolen_value = NULL;

	if (!uid)
		return;

	if (!g_hash_table_steal_extended (self->contacts_by_uid, uid, &stolen_key, &stolen_value))
		return;

	g_free (stolen_key);
	g_ptr_array_add (self->pending_free_leaves, stolen_value);

	atm_local_schedule_rebuild (self);
}

static void
e_addressbook_table_model_local_reset (EAddressbookTableModel *self)
{
	GHashTableIter iter;
	gpointer key, value;

	atm_local_cancel_rebuild (self);

	g_hash_table_iter_init (&iter, self->contacts_by_uid);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		g_hash_table_iter_steal (&iter);
		g_free (key);
		g_ptr_array_add (self->pending_free_leaves, value);
	}
}

static void
e_addressbook_table_model_set_local_mode (EAddressbookTableModel *self,
					  gboolean local_mode)
{
	if ((self->local_mode ? 1 : 0) == (local_mode ? 1 : 0))
		return;

	self->local_mode = local_mode;

	if (!local_mode) {
		e_addressbook_table_model_local_reset (self);
		eab_group_tree_free (self->root, self->root_group_levels);
		self->root = NULL;
		self->root_group_levels = 0;
		self->visible_dirty = TRUE;

		g_ptr_array_set_size (self->pending_free_leaves, 0);
	}
}

static void
e_addressbook_table_model_set_order_columns (EAddressbookTableModel *self,
					     GArray *order_columns,
					     guint n_group_levels)
{
	g_array_set_size (self->order_columns, 0);
	g_array_append_vals (self->order_columns, order_columns->data, order_columns->len);
	self->n_group_levels = n_group_levels;

	atm_local_rebuild_and_notify (self);
}

static guint
atm_get_row_count (EVirtualTreeModel *model)
{
	EAddressbookTableModel *self = E_ADDRESSBOOK_TABLE_MODEL (model);

	if (self->local_mode) {
		atm_ensure_visible_rows (self);
		return self->visible_rows->len;
	}

	return self->cache ? self->cache->len : 0;
}

static gboolean
atm_range_fully_cached (EAddressbookTableModel *self,
			guint first_row,
			guint count)
{
	guint ii;

	if (first_row + count > self->cache->len)
		return TRUE;

	for (ii = 0; ii < count; ii++) {
		if (!g_ptr_array_index (self->cache, first_row + ii))
			return FALSE;
	}

	return TRUE;
}

static void
atm_process_queue (EAddressbookTableModel *self)
{
	if (self->ongoing_range_read || !self->book_view)
		return;

	while (self->range_read_queue) {
		RangeRead *rr = self->range_read_queue->data;

		self->range_read_queue = g_slist_remove (self->range_read_queue, rr);

		if (rr->stamp != self->stamp || atm_range_fully_cached (self, rr->first_row, rr->count)) {
			range_read_free (rr);
			continue;
		}

		self->ongoing_range_read = rr;
		e_book_client_view_dup_contacts (self->book_view, rr->first_row, rr->count,
			NULL, atm_got_contacts_cb, rr);
		break;
	}
}

static void
atm_got_contacts_cb (GObject *source_object,
		     GAsyncResult *result,
		     gpointer user_data)
{
	RangeRead *rr = user_data;
	EAddressbookTableModel *self;
	guint out_range_start = 0;
	GPtrArray *contacts = NULL;
	GError *local_error = NULL;

	self = g_weak_ref_get (&rr->self_weakref);
	if (!self) {
		range_read_free (rr);
		return;
	}

	if (e_book_client_view_dup_contacts_finish (E_BOOK_CLIENT_VIEW (source_object), result,
	    &out_range_start, &contacts, &local_error) && rr->stamp == self->stamp) {
		guint ii, first_changed = G_MAXUINT, last_changed = 0;

		for (ii = 0; ii < contacts->len; ii++) {
			guint pos = out_range_start + ii;
			EContact *contact = g_ptr_array_index (contacts, ii);

			if (pos < self->cache->len && !g_ptr_array_index (self->cache, pos)) {
				g_ptr_array_index (self->cache, pos) = g_object_ref (contact);
				first_changed = MIN (first_changed, pos);
				last_changed = MAX (last_changed, pos);
			}
		}

		if (first_changed <= last_changed)
			e_virtual_tree_model_emit_rows_changed (E_VIRTUAL_TREE_MODEL (self), first_changed, last_changed);
	} else if (local_error &&
		   !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
		   !g_error_matches (local_error, E_CLIENT_ERROR, E_CLIENT_ERROR_OUT_OF_SYNC)) {
		g_warning ("%s: Failed to get contacts: %s", G_STRFUNC, local_error->message);
	}

	g_clear_pointer (&contacts, g_ptr_array_unref);
	g_clear_error (&local_error);

	self->ongoing_range_read = NULL;
	range_read_free (rr);

	atm_process_queue (self);

	g_object_unref (self);
}

static void
atm_ensure_range (EAddressbookTableModel *self,
		  guint first_row,
		  guint count)
{
	RangeRead *rr;

	if (!self->book_view || !self->cache || first_row >= self->cache->len)
		return;

	count = MIN (count, self->cache->len - first_row);

	if (count == 0 || atm_range_fully_cached (self, first_row, count))
		return;

	rr = g_new0 (RangeRead, 1);
	g_weak_ref_init (&rr->self_weakref, self);
	rr->stamp = self->stamp;
	rr->first_row = first_row;
	rr->count = count;

	self->range_read_queue = g_slist_append (self->range_read_queue, rr);

	atm_process_queue (self);
}

static GPtrArray *
atm_dup_rows (EVirtualTreeModel *model,
	      guint first_row,
	      guint last_row,
	      gboolean include_collapsed)
{
	EAddressbookTableModel *self = E_ADDRESSBOOK_TABLE_MODEL (model);
	GPtrArray *result;
	guint ii;

	result = g_ptr_array_new_with_free_func (g_object_unref);

	if (self->local_mode) {
		atm_ensure_visible_rows (self);

		if (!self->visible_rows->len)
			return result;

		last_row = MIN (last_row, self->visible_rows->len - 1);

		for (ii = first_row; ii <= last_row && ii < self->visible_rows->len; ii++) {
			EABGroupNode *gnode = g_ptr_array_index (self->visible_rows, ii);
			EAddressbookTableNode *node = g_object_new (E_TYPE_ADDRESSBOOK_TABLE_NODE, NULL);

			node->row_index = ii;
			node->is_group = gnode->is_group;
			node->group_label = g_strdup (gnode->group_label);
			node->group_key = g_strdup (gnode->group_key);
			node->depth = gnode->depth;
			node->expandable = gnode->is_group;
			node->expanded = gnode->expanded;
			node->contact = gnode->contact ? g_object_ref (gnode->contact) : NULL;

			g_ptr_array_add (result, node);
		}

		return result;
	}

	if (!self->cache || !self->cache->len)
		return result;

	last_row = MIN (last_row, self->cache->len - 1);

	for (ii = first_row; ii <= last_row && ii < self->cache->len; ii++) {
		EAddressbookTableNode *node = g_object_new (E_TYPE_ADDRESSBOOK_TABLE_NODE, NULL);
		EContact *contact = g_ptr_array_index (self->cache, ii);

		node->row_index = ii;
		node->contact = contact ? g_object_ref (contact) : NULL;

		g_ptr_array_add (result, node);
	}

	if (last_row >= first_row)
		atm_ensure_range (self, first_row, last_row - first_row + 1);

	return result;
}

static guint
atm_get_depth (EVirtualTreeModel *model,
	       GObject *row_object)
{
	return E_ADDRESSBOOK_TABLE_NODE (row_object)->depth;
}

static gboolean
atm_is_expandable (EVirtualTreeModel *model,
		   GObject *row_object)
{
	return E_ADDRESSBOOK_TABLE_NODE (row_object)->expandable;
}

static gboolean
atm_get_expanded (EVirtualTreeModel *model,
		  GObject *row_object)
{
	return E_ADDRESSBOOK_TABLE_NODE (row_object)->expanded;
}

static void
atm_set_expanded (EVirtualTreeModel *model,
		  GObject *row_object,
		  gboolean expanded)
{
	EAddressbookTableModel *self = E_ADDRESSBOOK_TABLE_MODEL (model);
	EAddressbookTableNode *node = E_ADDRESSBOOK_TABLE_NODE (row_object);
	EABGroupNode *gnode;

	if (!self->local_mode || !node->is_group || !node->group_key)
		return;

	gnode = atm_find_group_node (self->root, self->root_group_levels, node->group_key);
	if (!gnode)
		return;

	gnode->expanded = expanded;

	if (expanded)
		g_hash_table_remove (self->collapsed_group_keys, node->group_key);
	else
		g_hash_table_add (self->collapsed_group_keys, g_strdup (node->group_key));

	self->visible_dirty = TRUE;

	e_virtual_tree_model_emit_row_count_changed (model);
}

static gconstpointer
atm_get_row_key (EVirtualTreeModel *model,
		 GObject *row_object)
{
	EAddressbookTableNode *node = E_ADDRESSBOOK_TABLE_NODE (row_object);

	if (node->is_group)
		return node->group_key;

	return node->contact ? e_contact_get_const (node->contact, E_CONTACT_UID) : NULL;
}

static EVirtualTreeKeyType
atm_get_key_type (EVirtualTreeModel *model)
{
	EVirtualTreeKeyType kt = {
		g_str_hash,
		g_str_equal,
		(GBoxedCopyFunc) g_strdup,
		g_free
	};
	return kt;
}

static guint
atm_find_row_by_key (EVirtualTreeModel *model,
		     gconstpointer key)
{
	EAddressbookTableModel *self = E_ADDRESSBOOK_TABLE_MODEL (model);
	guint ii;

	if (!key)
		return G_MAXUINT;

	if (self->local_mode) {
		EABGroupNode *leaf;

		atm_ensure_visible_rows (self);

		leaf = g_hash_table_lookup (self->contacts_by_uid, key);

		for (ii = 0; ii < self->visible_rows->len; ii++) {
			EABGroupNode *gnode = g_ptr_array_index (self->visible_rows, ii);

			if (leaf ? gnode == leaf : (gnode->is_group && g_strcmp0 (gnode->group_key, (const gchar *) key) == 0))
				return ii;
		}

		return G_MAXUINT;
	}

	if (!self->cache)
		return G_MAXUINT;

	for (ii = 0; ii < self->cache->len; ii++) {
		EContact *contact = g_ptr_array_index (self->cache, ii);

		if (contact && g_strcmp0 (e_contact_get_const (contact, E_CONTACT_UID), (const gchar *) key) == 0)
			return ii;
	}

	return G_MAXUINT;
}

static void
e_addressbook_table_model_iface_init (EVirtualTreeModelInterface *iface)
{
	iface->get_row_count = atm_get_row_count;
	iface->dup_rows = atm_dup_rows;
	iface->get_depth = atm_get_depth;
	iface->is_expandable = atm_is_expandable;
	iface->get_expanded = atm_get_expanded;
	iface->set_expanded = atm_set_expanded;
	iface->get_row_key = atm_get_row_key;
	iface->get_key_type = atm_get_key_type;
	iface->find_row_by_key = atm_find_row_by_key;
}

static void
atm_cancel_pending (EAddressbookTableModel *self)
{
	self->stamp++;
	g_slist_free_full (self->range_read_queue, (GDestroyNotify) range_read_free);
	self->range_read_queue = NULL;
}

static void
atm_clear_cache_contacts (EAddressbookTableModel *self)
{
	guint ii;

	if (!self->cache)
		return;

	for (ii = 0; ii < self->cache->len; ii++) {
		EContact *contact = g_ptr_array_index (self->cache, ii);

		if (contact) {
			g_object_unref (contact);
			g_ptr_array_index (self->cache, ii) = NULL;
		}
	}
}

static void
atm_resize_and_invalidate (EAddressbookTableModel *self)
{
	guint new_total, old_total;

	new_total = self->book_view ? e_book_client_view_get_n_total (self->book_view) : 0;
	old_total = self->cache ? self->cache->len : 0;

	atm_cancel_pending (self);
	atm_clear_cache_contacts (self);

	if (!self->cache)
		self->cache = g_ptr_array_new ();

	g_ptr_array_set_size (self->cache, 0);
	g_ptr_array_set_size (self->cache, new_total);

	if (old_total != new_total)
		e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (self));

	if (new_total > 0)
		e_virtual_tree_model_emit_rows_changed (E_VIRTUAL_TREE_MODEL (self), 0, new_total - 1);
}

static void
atm_content_changed_cb (EBookClientView *book_view,
			gpointer user_data)
{
	atm_resize_and_invalidate (E_ADDRESSBOOK_TABLE_MODEL (user_data));
}

static void
atm_notify_n_total_cb (GObject *book_view,
		       GParamSpec *pspec,
		       gpointer user_data)
{
	atm_resize_and_invalidate (E_ADDRESSBOOK_TABLE_MODEL (user_data));
}

static void
atm_disconnect (EAddressbookTableModel *self)
{
	if (self->book_view) {
		if (self->sig_content_changed)
			g_signal_handler_disconnect (self->book_view, self->sig_content_changed);
		if (self->sig_notify_n_total)
			g_signal_handler_disconnect (self->book_view, self->sig_notify_n_total);
		self->sig_content_changed = 0;
		self->sig_notify_n_total = 0;
		g_clear_object (&self->book_view);
	}
}

static void
e_addressbook_table_model_set_book_view (EAddressbookTableModel *self,
					 EBookClientView *book_view)
{
	if (book_view == self->book_view)
		return;

	atm_disconnect (self);

	if (book_view) {
		self->book_view = g_object_ref (book_view);
		self->sig_content_changed = g_signal_connect (book_view, "content-changed",
			G_CALLBACK (atm_content_changed_cb), self);
		self->sig_notify_n_total = g_signal_connect (book_view, "notify::n-total",
			G_CALLBACK (atm_notify_n_total_cb), self);
	}

	e_virtual_tree_model_emit_before_rebuild (E_VIRTUAL_TREE_MODEL (self));
	atm_resize_and_invalidate (self);
	e_virtual_tree_model_emit_after_rebuild (E_VIRTUAL_TREE_MODEL (self));
}

static void
e_addressbook_table_model_dispose (GObject *object)
{
	EAddressbookTableModel *self = E_ADDRESSBOOK_TABLE_MODEL (object);

	atm_disconnect (self);
	atm_cancel_pending (self);
	atm_clear_cache_contacts (self);
	g_clear_pointer (&self->cache, g_ptr_array_unref);

	atm_local_cancel_rebuild (self);
	eab_group_tree_free (self->root, self->root_group_levels);
	self->root = NULL;
	self->root_group_levels = 0;
	g_clear_pointer (&self->contacts_by_uid, g_hash_table_unref);
	g_clear_pointer (&self->order_columns, g_array_unref);
	g_clear_pointer (&self->visible_rows, g_ptr_array_unref);
	g_clear_pointer (&self->collapsed_group_keys, g_hash_table_unref);
	g_clear_pointer (&self->pending_free_leaves, g_ptr_array_unref);

	G_OBJECT_CLASS (e_addressbook_table_model_parent_class)->dispose (object);
}

static void
e_addressbook_table_model_class_init (EAddressbookTableModelClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = e_addressbook_table_model_dispose;
}

static void
e_addressbook_table_model_init (EAddressbookTableModel *self)
{
	self->cache = g_ptr_array_new ();
	self->contacts_by_uid = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, eab_group_node_free_leaf);
	self->order_columns = g_array_new (FALSE, FALSE, sizeof (OrderColumn));
	self->collapsed_group_keys = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	self->visible_rows = g_ptr_array_new ();
	self->pending_free_leaves = g_ptr_array_new_with_free_func (eab_group_node_free_leaf);
}

static void
eab_table_column_data_func (EVirtualTree *tree,
			    GtkCellRenderer *renderer,
			    GObject *row_object,
			    guint visible_row,
			    gpointer user_data)
{
	const EABColumnDef *def = user_data;
	EAddressbookTableNode *node = E_ADDRESSBOOK_TABLE_NODE (row_object);
	gchar *text;

	if (node->is_group) {
		g_object_set (renderer, "text", node->group_label ? node->group_label : "", NULL);
		return;
	}

	text = node->contact ? eab_table_dup_column_text (node->contact, def) : NULL;

	g_object_set (renderer, "text", text ? text : "", NULL);

	g_free (text);
}

typedef struct _SortEntry {
	guint col_idx;
	gint priority;
	GtkSortType order;
} SortEntry;

static gint
sort_entry_compare (gconstpointer aa,
		    gconstpointer bb)
{
	const SortEntry *ea = aa;
	const SortEntry *eb = bb;

	return ea->priority - eb->priority;
}

/* EAddressbookTable widget */
struct _EAddressbookTablePrivate {
	EVirtualTree *vtree;
	EAddressbookTableModel *model;

	GCancellable *cancellable;
	EBookClient *book_client;
	EBookClientView *book_view;
	gchar *query;
	gboolean loading;
	gboolean local_mode;
	gboolean search_active;

	gulong view_progress_id;
	gulong view_complete_id;
	gulong view_objects_added_id;
	gulong view_objects_modified_id;
	gulong view_objects_removed_id;

	gchar *pending_select_uid;
	guint pending_select_scan_pos;

	GPtrArray *dnd_contacts; /* EContact *, owned */
};

enum {
	STATUS_MESSAGE,
	COUNT_CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_LOADING,
	N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EAddressbookTable, e_addressbook_table, GTK_TYPE_BOX)

enum RefreshFlags {
	REFRESH_FLAG_NONE = 0,
	REFRESH_FLAG_WITH_QUERY = 1 << 0
};

static void addressbook_table_refresh (EAddressbookTable *self, enum RefreshFlags flags);
static void addressbook_table_take_book_view (EAddressbookTable *self, EBookClientView *book_view);
static void addressbook_table_apply_sort_and_groups (EAddressbookTable *self);
static void addressbook_table_capture_pending_select (EAddressbookTable *self);

static void
addressbook_table_set_loading (EAddressbookTable *self,
			       gboolean loading)
{
	if ((self->priv->loading ? 1 : 0) == (loading ? 1 : 0))
		return;

	self->priv->loading = loading;

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
}

static void
addressbook_table_update_empty_message (EAddressbookTable *self)
{
	guint row_count;

	if (!self->priv->vtree)
		return;

	if (self->priv->loading) {
		e_virtual_tree_set_empty_message (self->priv->vtree, _("Searching for the Contacts…"));
		return;
	}

	row_count = e_virtual_tree_model_get_row_count (E_VIRTUAL_TREE_MODEL (self->priv->model));

	if (row_count > 0) {
		e_virtual_tree_set_empty_message (self->priv->vtree, NULL);
	} else if (self->priv->search_active) {
		e_virtual_tree_set_empty_message (self->priv->vtree,
			_("No contact satisfies your search criteria. "
			"Change search criteria by selecting a new "
			"Show contacts filter from the drop down list "
			"above or by running a new search either by "
			"clearing it with Search->Clear menu item or "
			"by changing the query above."));
	} else {
		gboolean perform_initial_query = FALSE;

		if (self->priv->book_client)
			perform_initial_query = !e_client_check_capability (E_CLIENT (self->priv->book_client), "do-initial-query");

		if (perform_initial_query)
			e_virtual_tree_set_empty_message (self->priv->vtree, _("Search for the Contact."));
		else
			e_virtual_tree_set_empty_message (self->priv->vtree, _("There are no items to show in this view."));
	}
}

static void
addressbook_table_got_view_cb (GObject *source_object,
			       GAsyncResult *result,
			       gpointer user_data)
{
	EAddressbookTable *self = user_data;
	EBookClientView *book_view = NULL;
	GError *error = NULL;

	if (e_book_client_get_view_finish (E_BOOK_CLIENT (source_object), result, &book_view, &error)) {
		addressbook_table_take_book_view (self, book_view);
		g_clear_object (&book_view);
	} else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warning ("%s: Failed to get book view: %s", G_STRFUNC, error ? error->message : "Unknown error");
	}

	g_clear_error (&error);
	g_object_unref (self);
}

static void
addressbook_table_refresh (EAddressbookTable *self,
			   enum RefreshFlags flags)
{
	if (!self->priv->book_client || !self->priv->query) {
		g_clear_pointer (&self->priv->pending_select_uid, g_free);
		self->priv->pending_select_scan_pos = 0;
		addressbook_table_take_book_view (self, NULL);
		addressbook_table_update_empty_message (self);
		return;
	}

	if (!self->priv->book_view || (flags & REFRESH_FLAG_WITH_QUERY) != 0) {
		if (self->priv->book_view && (flags & REFRESH_FLAG_WITH_QUERY) != 0)
			addressbook_table_capture_pending_select (self);

		addressbook_table_take_book_view (self, NULL);
		addressbook_table_update_empty_message (self);

		e_book_client_get_view (self->priv->book_client, self->priv->query,
			self->priv->cancellable, addressbook_table_got_view_cb, g_object_ref (self));
	}
}

static void
addressbook_table_view_progress_cb (EBookClientView *book_view,
				    guint percentage,
				    const gchar *message,
				    gpointer user_data)
{
	EAddressbookTable *self = user_data;

	g_signal_emit (self, signals[STATUS_MESSAGE], 0, message, percentage);
}

static void
addressbook_table_view_complete_cb (EBookClientView *book_view,
				    const GError *error,
				    gpointer user_data)
{
	EAddressbookTable *self = user_data;

	g_signal_emit (self, signals[STATUS_MESSAGE], 0, NULL, -1);

	addressbook_table_set_loading (self, FALSE);
	addressbook_table_update_empty_message (self);
}

static void
addressbook_table_objects_added_cb (EBookClientView *book_view,
				    GSList *objects,
				    gpointer user_data)
{
	EAddressbookTable *self = user_data;
	GSList *link;

	for (link = objects; link; link = g_slist_next (link))
		e_addressbook_table_model_local_add (self->priv->model, link->data);
}

static void
addressbook_table_objects_removed_cb (EBookClientView *book_view,
				      GSList *uids,
				      gpointer user_data)
{
	EAddressbookTable *self = user_data;
	GSList *link;

	for (link = uids; link; link = g_slist_next (link))
		e_addressbook_table_model_local_remove (self->priv->model, link->data);
}

static void
addressbook_table_take_book_view (EAddressbookTable *self,
				  EBookClientView *book_view)
{
	if (book_view == self->priv->book_view)
		return;

	addressbook_table_set_loading (self, FALSE);

	if (self->priv->book_view) {
		if (self->priv->view_progress_id)
			g_signal_handler_disconnect (self->priv->book_view, self->priv->view_progress_id);
		if (self->priv->view_complete_id)
			g_signal_handler_disconnect (self->priv->book_view, self->priv->view_complete_id);
		if (self->priv->view_objects_added_id)
			g_signal_handler_disconnect (self->priv->book_view, self->priv->view_objects_added_id);
		if (self->priv->view_objects_modified_id)
			g_signal_handler_disconnect (self->priv->book_view, self->priv->view_objects_modified_id);
		if (self->priv->view_objects_removed_id)
			g_signal_handler_disconnect (self->priv->book_view, self->priv->view_objects_removed_id);
		self->priv->view_progress_id = 0;
		self->priv->view_complete_id = 0;
		self->priv->view_objects_added_id = 0;
		self->priv->view_objects_modified_id = 0;
		self->priv->view_objects_removed_id = 0;

		e_addressbook_table_model_set_book_view (self->priv->model, NULL);
		e_addressbook_table_model_local_reset (self->priv->model);

		g_clear_object (&self->priv->book_view);
	}

	if (book_view) {
		self->priv->book_view = g_object_ref (book_view);
		addressbook_table_set_loading (self, TRUE);

		self->priv->view_progress_id = g_signal_connect (book_view, "progress",
			G_CALLBACK (addressbook_table_view_progress_cb), self);
		self->priv->view_complete_id = g_signal_connect (book_view, "complete",
			G_CALLBACK (addressbook_table_view_complete_cb), self);

		if (self->priv->local_mode) {
			self->priv->view_objects_added_id = g_signal_connect (book_view, "objects-added",
				G_CALLBACK (addressbook_table_objects_added_cb), self);
			self->priv->view_objects_modified_id = g_signal_connect (book_view, "objects-modified",
				G_CALLBACK (addressbook_table_objects_added_cb), self);
			self->priv->view_objects_removed_id = g_signal_connect (book_view, "objects-removed",
				G_CALLBACK (addressbook_table_objects_removed_cb), self);
		} else {
			e_book_client_view_set_flags (book_view, E_BOOK_CLIENT_VIEW_FLAGS_MANUAL_QUERY, NULL);

			e_addressbook_table_model_set_book_view (self->priv->model, book_view);
		}

		addressbook_table_apply_sort_and_groups (self);

		e_book_client_view_start (book_view, NULL);
	}
}

static void
addressbook_table_capture_pending_select (EAddressbookTable *self)
{
	GObject *cursor_obj;

	g_clear_pointer (&self->priv->pending_select_uid, g_free);
	self->priv->pending_select_scan_pos = 0;

	cursor_obj = e_virtual_tree_get_cursor_object (self->priv->vtree);
	if (cursor_obj) {
		gconstpointer key = e_virtual_tree_model_get_row_key (E_VIRTUAL_TREE_MODEL (self->priv->model), cursor_obj);

		if (key)
			self->priv->pending_select_uid = g_strdup (key);
	}
}

static void
addressbook_table_apply_server_sort_fields (EAddressbookTable *self)
{
	GArray *entries;
	EBookClientViewSortFields *sort_fields;
	guint ii, n_fields;
	GError *local_error = NULL;

	addressbook_table_capture_pending_select (self);

	entries = g_array_new (FALSE, FALSE, sizeof (SortEntry));

	for (ii = 0; ii < N_EAB_TABLE_COLUMNS; ii++) {
		GtkSortType order;
		gint priority;

		if (!eab_column_is_sortable (&eab_table_columns[ii]))
			continue;

		if (e_virtual_tree_get_column_sort (self->priv->vtree, ii, &order, &priority)) {
			SortEntry entry;

			entry.col_idx = ii;
			entry.priority = priority;
			entry.order = order;

			g_array_append_val (entries, entry);
		}
	}

	g_array_sort (entries, sort_entry_compare);

	n_fields = MAX (entries->len, 1);
	sort_fields = g_new0 (EBookClientViewSortFields, n_fields + 1);

	if (entries->len == 0) {
		sort_fields[0].field = E_CONTACT_FILE_AS;
		sort_fields[0].sort_type = E_BOOK_CURSOR_SORT_ASCENDING;
	} else {
		for (ii = 0; ii < entries->len; ii++) {
			SortEntry *entry = &g_array_index (entries, SortEntry, ii);

			sort_fields[ii].field = eab_table_columns[entry->col_idx].field;
			sort_fields[ii].sort_type = entry->order == GTK_SORT_ASCENDING ?
				E_BOOK_CURSOR_SORT_ASCENDING : E_BOOK_CURSOR_SORT_DESCENDING;
		}
	}

	sort_fields[n_fields].field = E_CONTACT_FIELD_LAST;

	if (!e_book_client_view_set_sort_fields_sync (self->priv->book_view, sort_fields, NULL, &local_error) &&
	    !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		g_warning ("%s: Failed to set sort fields: %s", G_STRFUNC, local_error ? local_error->message : "Unknown error");

	g_clear_error (&local_error);
	g_free (sort_fields);
	g_array_unref (entries);
}

#define PENDING_SELECT_SCAN_CHUNK 100

/* Whether every row in the (non-local) cache already holds real contact
 * data. Used to tell "not found yet, still streaming in" apart from
 * "streaming finished and the target row genuinely isn't there anymore". */
static gboolean
addressbook_table_model_fully_cached (EAddressbookTableModel *model)
{
	guint ii;

	if (!model->cache)
		return TRUE;

	for (ii = 0; ii < model->cache->len; ii++) {
		if (!g_ptr_array_index (model->cache, ii))
			return FALSE;
	}

	return TRUE;
}

static void
addressbook_table_try_restore_pending_select (EAddressbookTable *self)
{
	EVirtualTreeModel *model;
	guint row, total, next_pos;

	if (!self->priv->pending_select_uid)
		return;

	model = E_VIRTUAL_TREE_MODEL (self->priv->model);

	row = e_virtual_tree_model_find_row_by_key (model, self->priv->pending_select_uid);

	if (row != G_MAXUINT) {
		e_virtual_tree_set_cursor_centered (self->priv->vtree, (gint) row);
		e_virtual_tree_select_row (self->priv->vtree, row);

		g_clear_pointer (&self->priv->pending_select_uid, g_free);
		self->priv->pending_select_scan_pos = 0;
		return;
	}

	total = e_virtual_tree_model_get_row_count (model);

	if (total == 0 || addressbook_table_model_fully_cached (self->priv->model)) {
		g_clear_pointer (&self->priv->pending_select_uid, g_free);
		self->priv->pending_select_scan_pos = 0;
		return;
	}

	if (self->priv->pending_select_scan_pos >= total)
		self->priv->pending_select_scan_pos = 0;

	next_pos = MIN (self->priv->pending_select_scan_pos + PENDING_SELECT_SCAN_CHUNK, total);

	if (next_pos > self->priv->pending_select_scan_pos) {
		GPtrArray *rows;

		rows = e_virtual_tree_model_dup_rows (model, self->priv->pending_select_scan_pos, next_pos - 1, TRUE);
		g_clear_pointer (&rows, g_ptr_array_unref);
	}

	self->priv->pending_select_scan_pos = next_pos;
}

static void
addressbook_table_model_rows_changed_cb (EVirtualTreeModel *model,
					 guint first_row,
					 guint last_row,
					 gpointer user_data)
{
	EAddressbookTable *self = user_data;

	addressbook_table_try_restore_pending_select (self);
}

static GArray *
addressbook_table_compute_order_columns (EAddressbookTable *self,
					 guint *out_n_group_levels,
					 gboolean *out_want_local)
{
	GArray *group_entries;
	GArray *sort_entries;
	GArray *order_columns;
	guint ii, jj;

	group_entries = g_array_new (FALSE, FALSE, sizeof (SortEntry));
	sort_entries = g_array_new (FALSE, FALSE, sizeof (SortEntry));

	for (ii = 0; ii < N_EAB_TABLE_COLUMNS; ii++) {
		GtkSortType order;
		gint priority;
		SortEntry entry;

		entry.col_idx = ii;

		if (e_virtual_tree_get_column_group (self->priv->vtree, ii, &order, &priority)) {
			entry.priority = priority;
			entry.order = order;

			g_array_append_val (group_entries, entry);
		}

		if (e_virtual_tree_get_column_sort (self->priv->vtree, ii, &order, &priority)) {
			entry.priority = priority;
			entry.order = order;

			g_array_append_val (sort_entries, entry);
		}
	}

	g_array_sort (group_entries, sort_entry_compare);
	g_array_sort (sort_entries, sort_entry_compare);

	*out_want_local = group_entries->len > 0;

	for (ii = 0; !*out_want_local && ii < sort_entries->len; ii++) {
		SortEntry *entry = &g_array_index (sort_entries, SortEntry, ii);

		*out_want_local = !eab_column_is_sortable (&eab_table_columns[entry->col_idx]);
	}

	*out_n_group_levels = group_entries->len;
	order_columns = g_array_new (FALSE, FALSE, sizeof (OrderColumn));

	for (ii = 0; ii < group_entries->len; ii++) {
		SortEntry *entry = &g_array_index (group_entries, SortEntry, ii);
		OrderColumn order_column;

		order_column.col_idx = entry->col_idx;
		order_column.order = entry->order;
		order_column.is_group = TRUE;

		g_array_append_val (order_columns, order_column);
	}

	for (ii = 0; ii < sort_entries->len; ii++) {
		SortEntry *entry = &g_array_index (sort_entries, SortEntry, ii);
		OrderColumn order_column;
		gboolean already_grouped = FALSE;

		for (jj = 0; jj < group_entries->len; jj++) {
			if (g_array_index (group_entries, SortEntry, jj).col_idx == entry->col_idx) {
				already_grouped = TRUE;
				break;
			}
		}

		if (already_grouped)
			continue;

		order_column.col_idx = entry->col_idx;
		order_column.order = entry->order;
		order_column.is_group = FALSE;

		g_array_append_val (order_columns, order_column);
	}

	g_array_unref (group_entries);
	g_array_unref (sort_entries);

	return order_columns;
}

static void
addressbook_table_apply_sort_and_groups (EAddressbookTable *self)
{
	GArray *order_columns;
	guint n_group_levels;
	gboolean want_local, mode_changed;

	if (!self->priv->book_view || !self->priv->vtree)
		return;

	order_columns = addressbook_table_compute_order_columns (self, &n_group_levels, &want_local);

	mode_changed = (want_local ? 1 : 0) != (self->priv->local_mode ? 1 : 0);

	self->priv->local_mode = want_local;

	if (want_local) {
		e_addressbook_table_model_set_local_mode (self->priv->model, TRUE);
		e_addressbook_table_model_set_order_columns (self->priv->model, order_columns, n_group_levels);
		e_virtual_tree_set_group_depth (self->priv->vtree, n_group_levels);
	} else {
		e_addressbook_table_model_set_local_mode (self->priv->model, FALSE);
		e_virtual_tree_set_group_depth (self->priv->vtree, 0);
	}

	g_array_unref (order_columns);

	if (mode_changed) {
		addressbook_table_refresh (self, REFRESH_FLAG_WITH_QUERY);
		return;
	}

	if (!want_local)
		addressbook_table_apply_server_sort_fields (self);
}

static void
addressbook_table_column_state_changed_cb (EVirtualTree *vtree,
					   gpointer user_data)
{
	addressbook_table_apply_sort_and_groups (E_ADDRESSBOOK_TABLE (user_data));
}

enum {
	DND_TARGET_TYPE_SOURCE_VCARD,
	DND_TARGET_TYPE_VCARD
};

static GtkTargetEntry eab_table_drag_types[] = {
	{ (gchar *) "text/x-source-vcard", 0, DND_TARGET_TYPE_SOURCE_VCARD },
	{ (gchar *) "text/x-vcard", 0, DND_TARGET_TYPE_VCARD }
};

static void
addressbook_table_dnd_contacts_received_cb (GObject *source_object,
					    GAsyncResult *result,
					    gpointer user_data)
{
	EAddressbookTable *self = user_data;
	GPtrArray *contacts;
	GError *error = NULL;

	contacts = e_addressbook_table_dup_selected_contacts_finish (self, result, &error);
	if (contacts) {
		g_clear_pointer (&self->priv->dnd_contacts, g_ptr_array_unref);
		self->priv->dnd_contacts = contacts;
	} else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warning ("%s: Failed to receive DND contacts: %s", G_STRFUNC, error ? error->message : "Unknown error");
	}

	g_clear_error (&error);
	g_object_unref (self);
}

static void
addressbook_table_drag_begin_cb (EVirtualTree *vtree,
				 guint row,
				 GdkDragContext *context,
				 gpointer user_data)
{
	EAddressbookTable *self = user_data;

	g_clear_pointer (&self->priv->dnd_contacts, g_ptr_array_unref);

	self->priv->dnd_contacts = e_addressbook_table_peek_selected_contacts (self);

	if (!self->priv->dnd_contacts) {
		e_addressbook_table_dup_selected_contacts (self, self->priv->cancellable,
			addressbook_table_dnd_contacts_received_cb, g_object_ref (self));
	}

	gtk_drag_set_icon_default (context);
}

static void
addressbook_table_drag_data_get_cb (EVirtualTree *vtree,
				    GdkDragContext *context,
				    GtkSelectionData *selection_data,
				    guint info,
				    guint time,
				    gpointer user_data)
{
	EAddressbookTable *self = user_data;
	GdkAtom target;
	gchar *value;

	if (!self->priv->dnd_contacts) {
		g_warning ("%s: Failed to read contacts before the drag operation finished; repeat the action later", G_STRFUNC);
		gtk_drag_cancel (context);
		return;
	}

	target = gtk_selection_data_get_target (selection_data);

	switch (info) {
	case DND_TARGET_TYPE_VCARD:
		value = eab_contact_array_to_string (self->priv->dnd_contacts);
		gtk_selection_data_set (selection_data, target, 8, (guchar *) value, strlen (value));
		g_free (value);
		break;
	case DND_TARGET_TYPE_SOURCE_VCARD:
		value = eab_book_and_contact_array_to_string (self->priv->book_client, self->priv->dnd_contacts);
		gtk_selection_data_set (selection_data, target, 8, (guchar *) value, strlen (value));
		g_free (value);
		break;
	}
}

static void
addressbook_table_drag_end_cb (EVirtualTree *vtree,
			       GdkDragContext *context,
			       gpointer user_data)
{
	EAddressbookTable *self = user_data;

	g_clear_pointer (&self->priv->dnd_contacts, g_ptr_array_unref);
}

static void
addressbook_table_setup_columns (EAddressbookTable *self)
{
	guint ii;

	for (ii = 0; ii < N_EAB_TABLE_COLUMNS; ii++) {
		const EABColumnDef *def = &eab_table_columns[ii];
		GtkCellRenderer *renderer;
		GtkTreeViewColumn *tvc;
		guint col;
		guint jj;
		gboolean visible = FALSE;

		col = e_virtual_tree_add_column (self->priv->vtree, def->id, gettext (def->title));

		e_virtual_tree_set_column_groupable (self->priv->vtree, col, TRUE);

		renderer = gtk_cell_renderer_text_new ();
		g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

		e_virtual_tree_column_pack_start (self->priv->vtree, col, 0,
			renderer, TRUE, eab_table_column_data_func, (gpointer) def, NULL);

		e_virtual_tree_set_column_min_width (self->priv->vtree, col, 50);

		for (jj = 0; jj < G_N_ELEMENTS (eab_default_visible_columns); jj++) {
			if (g_strcmp0 (def->id, eab_default_visible_columns[jj]) == 0) {
				visible = TRUE;
				break;
			}
		}

		tvc = e_virtual_tree_get_column (self->priv->vtree, col);
		gtk_tree_view_column_set_visible (tvc, visible);
	}

	e_virtual_tree_set_expander_column (self->priv->vtree, 0, 0);
}

static const gchar * const *
eab_table_get_legacy_etable_column_ids (guint *out_n_column_ids)
{
	static const gchar *column_ids[N_EAB_TABLE_COLUMNS];
	static gboolean initialized = FALSE;
	guint ii;

	if (!initialized) {
		for (ii = 0; ii < N_EAB_TABLE_COLUMNS; ii++)
			column_ids[ii] = eab_table_columns[ii].id;
		initialized = TRUE;
	}

	if (out_n_column_ids)
		*out_n_column_ids = N_EAB_TABLE_COLUMNS;

	return column_ids;
}

static gpointer
addressbook_table_get_legacy_etable_column_map_cb (EVirtualTree *vtree,
						   guint *out_n_column_ids,
						   gpointer user_data)
{
	return (gpointer) eab_table_get_legacy_etable_column_ids (out_n_column_ids);
}

static void
addressbook_table_model_row_count_changed_cb (EVirtualTreeModel *model,
					      gpointer user_data)
{
	EAddressbookTable *self = user_data;

	addressbook_table_update_empty_message (self);

	g_signal_emit (self, signals[COUNT_CHANGED], 0);
}

static void
e_addressbook_table_constructed (GObject *object)
{
	EAddressbookTable *self = E_ADDRESSBOOK_TABLE (object);
	GtkWidget *sw;
	GSettings *settings;

	G_OBJECT_CLASS (e_addressbook_table_parent_class)->constructed (object);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);

	self->priv->cancellable = g_cancellable_new ();
	self->priv->model = g_object_new (E_TYPE_ADDRESSBOOK_TABLE_MODEL, NULL);

	g_signal_connect_object (self->priv->model, "row-count-changed",
		G_CALLBACK (addressbook_table_model_row_count_changed_cb), self, 0);
	g_signal_connect (self->priv->model, "rows-changed",
		G_CALLBACK (addressbook_table_model_rows_changed_cb), self);

	self->priv->vtree = E_VIRTUAL_TREE (e_virtual_tree_new (E_VIRTUAL_TREE_MODEL (self->priv->model)));
	g_signal_connect (self->priv->vtree, "destroy",
		G_CALLBACK (gtk_widget_destroyed), &self->priv->vtree);
	e_virtual_tree_set_selection_mode (self->priv->vtree, GTK_SELECTION_MULTIPLE);

	settings = e_util_ref_settings ("org.gnome.evolution.addressbook");
	g_settings_bind (settings, "table-sort-on-header-click",
		self->priv->vtree, "header-click-sort-policy",
		G_SETTINGS_BIND_DEFAULT);
	g_clear_object (&settings);

	g_signal_connect (self->priv->vtree, "get-legacy-etable-column-map",
		G_CALLBACK (addressbook_table_get_legacy_etable_column_map_cb), NULL);

	addressbook_table_setup_columns (self);

	g_signal_connect (self->priv->vtree, "column-state-changed",
		G_CALLBACK (addressbook_table_column_state_changed_cb), self);

	e_virtual_tree_enable_drag_source (self->priv->vtree,
		GDK_BUTTON1_MASK, eab_table_drag_types, G_N_ELEMENTS (eab_table_drag_types),
		GDK_ACTION_MOVE | GDK_ACTION_COPY);

	g_signal_connect (self->priv->vtree, "tree-drag-begin",
		G_CALLBACK (addressbook_table_drag_begin_cb), self);
	g_signal_connect (self->priv->vtree, "tree-drag-data-get",
		G_CALLBACK (addressbook_table_drag_data_get_cb), self);
	g_signal_connect (self->priv->vtree, "tree-drag-end",
		G_CALLBACK (addressbook_table_drag_end_cb), self);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (self->priv->vtree));

	gtk_box_pack_start (GTK_BOX (self), sw, TRUE, TRUE, 0);

	addressbook_table_update_empty_message (self);

	gtk_widget_show_all (GTK_WIDGET (self));
}

static void
e_addressbook_table_dispose (GObject *object)
{
	EAddressbookTable *self = E_ADDRESSBOOK_TABLE (object);

	if (self->priv->cancellable) {
		g_cancellable_cancel (self->priv->cancellable);
		g_clear_object (&self->priv->cancellable);
	}

	if (self->priv->vtree) {
		e_virtual_tree_set_model (self->priv->vtree, NULL);
		self->priv->vtree = NULL;
	}

	if (self->priv->model) {
		g_signal_handlers_disconnect_by_func (self->priv->model, addressbook_table_model_row_count_changed_cb, self);
		g_signal_handlers_disconnect_by_func (self->priv->model, addressbook_table_model_rows_changed_cb, self);
	}

	addressbook_table_take_book_view (self, NULL);

	g_clear_object (&self->priv->book_client);
	g_clear_object (&self->priv->model);
	g_clear_pointer (&self->priv->query, g_free);
	g_clear_pointer (&self->priv->pending_select_uid, g_free);
	g_clear_pointer (&self->priv->dnd_contacts, g_ptr_array_unref);

	G_OBJECT_CLASS (e_addressbook_table_parent_class)->dispose (object);
}

static void
e_addressbook_table_get_property (GObject *object,
				  guint property_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	EAddressbookTable *self = E_ADDRESSBOOK_TABLE (object);

	switch (property_id) {
	case PROP_LOADING:
		g_value_set_boolean (value, e_addressbook_table_get_loading (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
e_addressbook_table_class_init (EAddressbookTableClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = e_addressbook_table_constructed;
	object_class->get_property = e_addressbook_table_get_property;
	object_class->dispose = e_addressbook_table_dispose;

	properties[PROP_LOADING] =
		g_param_spec_boolean (
			"loading", NULL, NULL,
			FALSE,
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, N_PROPS, properties);

	signals[STATUS_MESSAGE] = g_signal_new (
		"status-message",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookTableClass, status_message),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		G_TYPE_STRING,
		G_TYPE_INT);

	signals[COUNT_CHANGED] = g_signal_new (
		"count-changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookTableClass, count_changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);
}

static void
e_addressbook_table_init (EAddressbookTable *self)
{
	self->priv = e_addressbook_table_get_instance_private (self);
}

GtkWidget *
e_addressbook_table_new (void)
{
	return g_object_new (E_TYPE_ADDRESSBOOK_TABLE, NULL);
}

EVirtualTree *
e_addressbook_table_get_virtual_tree (EAddressbookTable *self)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_TABLE (self), NULL);

	return self->priv->vtree;
}

EBookClient *
e_addressbook_table_get_book_client (EAddressbookTable *self)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_TABLE (self), NULL);

	return self->priv->book_client;
}

void
e_addressbook_table_set_book_client (EAddressbookTable *self,
				     EBookClient *book_client)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_TABLE (self));

	if (self->priv->book_client == book_client)
		return;

	g_clear_object (&self->priv->book_client);
	self->priv->book_client = book_client ? g_object_ref (book_client) : NULL;

	addressbook_table_refresh (self, REFRESH_FLAG_WITH_QUERY);
}

const gchar *
e_addressbook_table_get_query (EAddressbookTable *self)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_TABLE (self), NULL);

	return self->priv->query;
}

void
e_addressbook_table_set_query (EAddressbookTable *self,
			       const gchar *query)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_TABLE (self));

	if (g_strcmp0 (self->priv->query, query) == 0)
		return;

	g_free (self->priv->query);
	self->priv->query = g_strdup (query);

	addressbook_table_refresh (self, REFRESH_FLAG_WITH_QUERY);
}

guint
e_addressbook_table_get_n_total (EAddressbookTable *self)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_TABLE (self), 0);

	return e_virtual_tree_model_get_row_count (E_VIRTUAL_TREE_MODEL (self->priv->model));
}

gboolean
e_addressbook_table_get_loading (EAddressbookTable *self)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_TABLE (self), FALSE);

	return self->priv->loading;
}

void
e_addressbook_table_set_search_active (EAddressbookTable *self,
				       gboolean search_active)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_TABLE (self));

	search_active = !!search_active;

	if ((self->priv->search_active ? 1 : 0) == (search_active ? 1 : 0))
		return;

	self->priv->search_active = search_active;

	addressbook_table_update_empty_message (self);
}

gboolean
e_addressbook_table_get_search_active (EAddressbookTable *self)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_TABLE (self), FALSE);

	return self->priv->search_active;
}

typedef struct _PrefetchAllData {
	EAddressbookTableModel *model; /* owned ref */
	guint stamp;
} PrefetchAllData;

static void
prefetch_all_data_free (PrefetchAllData *data)
{
	g_clear_object (&data->model);
	g_free (data);
}

static void
addressbook_table_prefetch_all_got_contacts_cb (GObject *source_object,
						GAsyncResult *result,
						gpointer user_data)
{
	GTask *task = user_data;
	PrefetchAllData *data = g_task_get_task_data (task);
	guint out_range_start = 0;
	guint ii, first_changed, last_changed;
	GPtrArray *contacts = NULL;
	GError *local_error = NULL;

	if (e_book_client_view_dup_contacts_finish (E_BOOK_CLIENT_VIEW (source_object), result,
	    &out_range_start, &contacts, &local_error)) {
		first_changed = G_MAXUINT;
		last_changed = 0;

		if (data->stamp == data->model->stamp && data->model->cache) {
			for (ii = 0; ii < contacts->len; ii++) {
				guint pos = out_range_start + ii;
				EContact *contact = g_ptr_array_index (contacts, ii);

				if (pos < data->model->cache->len && !g_ptr_array_index (data->model->cache, pos)) {
					g_ptr_array_index (data->model->cache, pos) = g_object_ref (contact);
					first_changed = MIN (first_changed, pos);
					last_changed = MAX (last_changed, pos);
				}
			}

			if (first_changed <= last_changed)
				e_virtual_tree_model_emit_rows_changed (E_VIRTUAL_TREE_MODEL (data->model), first_changed, last_changed);
		}

		g_task_return_boolean (task, TRUE);
	} else {
		g_task_return_error (task, local_error);
	}

	g_clear_pointer (&contacts, g_ptr_array_unref);
	g_object_unref (task);
}

void
e_addressbook_table_prefetch_all_contacts (EAddressbookTable *self,
					   GCancellable *cancellable,
					   GAsyncReadyCallback callback,
					   gpointer user_data)
{
	GTask *task;
	PrefetchAllData *data;
	guint total;

	g_return_if_fail (E_IS_ADDRESSBOOK_TABLE (self));

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_addressbook_table_prefetch_all_contacts);

	if (self->priv->local_mode || !self->priv->model->book_view) {
		g_task_return_boolean (task, TRUE);
		g_object_unref (task);
		return;
	}

	total = e_book_client_view_get_n_total (self->priv->model->book_view);

	if (total == 0 || atm_range_fully_cached (self->priv->model, 0, total)) {
		g_task_return_boolean (task, TRUE);
		g_object_unref (task);
		return;
	}

	data = g_new0 (PrefetchAllData, 1);
	data->model = g_object_ref (self->priv->model);
	data->stamp = self->priv->model->stamp;

	g_task_set_task_data (task, data, (GDestroyNotify) prefetch_all_data_free);

	e_book_client_view_dup_contacts (self->priv->model->book_view, 0, total,
		cancellable, addressbook_table_prefetch_all_got_contacts_cb, task);
}

gboolean
e_addressbook_table_prefetch_all_contacts_finish (EAddressbookTable *self,
						  GAsyncResult *result,
						  GError **error)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_TABLE (self), FALSE);
	g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

guint
e_addressbook_table_get_n_selected (EAddressbookTable *self)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_TABLE (self), 0);

	return e_virtual_tree_selected_count (self->priv->vtree);
}

void
e_addressbook_table_select_all (EAddressbookTable *self)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_TABLE (self));

	e_virtual_tree_select_all (self->priv->vtree);
}

guint
e_addressbook_table_get_cursor_row (EAddressbookTable *self)
{
	gint row;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_TABLE (self), G_MAXUINT);

	row = e_virtual_tree_get_cursor (self->priv->vtree);

	return row >= 0 ? (guint) row : G_MAXUINT;
}

void
e_addressbook_table_set_cursor_row (EAddressbookTable *self,
				    guint row)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_TABLE (self));

	e_virtual_tree_set_cursor (self->priv->vtree, (gint) row);
}

EPrintable *
e_addressbook_table_get_printable (EAddressbookTable *self)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_TABLE (self), NULL);

	return e_virtual_tree_get_printable (self->priv->vtree);
}

GPtrArray *
e_addressbook_table_peek_selected_contacts (EAddressbookTable *self)
{
	GPtrArray *rows;
	GPtrArray *contacts;
	guint ii;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_TABLE (self), NULL);

	rows = e_virtual_tree_get_selected_rows (self->priv->vtree);
	contacts = g_ptr_array_new_full (rows->len, g_object_unref);

	for (ii = 0; ii < rows->len; ii++) {
		EAddressbookTableNode *node = g_ptr_array_index (rows, ii);

		if (node->is_group)
			continue;

		if (!node->contact) {
			g_ptr_array_unref (contacts);
			g_ptr_array_unref (rows);
			return NULL;
		}

		g_ptr_array_add (contacts, g_object_ref (node->contact));
	}

	g_ptr_array_unref (rows);

	return contacts;
}

typedef struct _DupSelectedData {
	GPtrArray *rows; /* EAddressbookTableNode * */
} DupSelectedData;

static void
dup_selected_data_free (DupSelectedData *data)
{
	g_clear_pointer (&data->rows, g_ptr_array_unref);
	g_free (data);
}

static void
addressbook_table_dup_selected_got_contacts_cb (GObject *source_object,
						GAsyncResult *result,
						gpointer user_data)
{
	GTask *task = user_data;
	DupSelectedData *data = g_task_get_task_data (task);
	guint out_range_start = 0;
	GPtrArray *contacts = NULL;
	GError *local_error = NULL;

	if (e_book_client_view_dup_contacts_finish (E_BOOK_CLIENT_VIEW (source_object), result,
	    &out_range_start, &contacts, &local_error)) {
		GPtrArray *result_contacts;
		guint ii;

		result_contacts = g_ptr_array_new_full (data->rows->len, g_object_unref);

		for (ii = 0; ii < data->rows->len; ii++) {
			EAddressbookTableNode *node = g_ptr_array_index (data->rows, ii);
			guint local_idx;

			if (node->row_index < out_range_start ||
			    node->row_index - out_range_start >= contacts->len)
				continue;

			local_idx = node->row_index - out_range_start;
			g_ptr_array_add (result_contacts, g_object_ref (g_ptr_array_index (contacts, local_idx)));
		}

		g_task_return_pointer (task, result_contacts, (GDestroyNotify) g_ptr_array_unref);
	} else {
		g_task_return_error (task, local_error);
	}

	g_clear_pointer (&contacts, g_ptr_array_unref);
	g_object_unref (task);
}

void
e_addressbook_table_dup_selected_contacts (EAddressbookTable *self,
					   GCancellable *cancellable,
					   GAsyncReadyCallback callback,
					   gpointer user_data)
{
	GTask *task;
	GPtrArray *rows;
	DupSelectedData *data;
	guint ii, min_row = G_MAXUINT, max_row = 0;

	g_return_if_fail (E_IS_ADDRESSBOOK_TABLE (self));

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_addressbook_table_dup_selected_contacts);

	rows = e_virtual_tree_get_selected_rows (self->priv->vtree);

	if (self->priv->local_mode) {
		GPtrArray *contacts;

		contacts = g_ptr_array_new_full (rows->len, g_object_unref);

		for (ii = 0; ii < rows->len; ii++) {
			EAddressbookTableNode *node = g_ptr_array_index (rows, ii);

			if (node->contact)
				g_ptr_array_add (contacts, g_object_ref (node->contact));
		}

		g_ptr_array_unref (rows);
		g_task_return_pointer (task, contacts, (GDestroyNotify) g_ptr_array_unref);
		g_object_unref (task);
		return;
	}

	if (rows->len == 0 || !self->priv->book_view) {
		g_ptr_array_unref (rows);
		g_task_return_pointer (task, g_ptr_array_new_with_free_func (g_object_unref), (GDestroyNotify) g_ptr_array_unref);
		g_object_unref (task);
		return;
	}

	for (ii = 0; ii < rows->len; ii++) {
		EAddressbookTableNode *node = g_ptr_array_index (rows, ii);

		min_row = MIN (min_row, node->row_index);
		max_row = MAX (max_row, node->row_index);
	}

	data = g_new0 (DupSelectedData, 1);
	data->rows = rows;
	g_task_set_task_data (task, data, (GDestroyNotify) dup_selected_data_free);

	e_book_client_view_dup_contacts (self->priv->book_view, min_row, max_row - min_row + 1,
		cancellable, addressbook_table_dup_selected_got_contacts_cb, task);
}

GPtrArray *
e_addressbook_table_dup_selected_contacts_finish (EAddressbookTable *self,
						  GAsyncResult *result,
						  GError **error)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_TABLE (self), NULL);
	g_return_val_if_fail (g_task_is_valid (result, self), NULL);
	g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == e_addressbook_table_dup_selected_contacts, NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

EContact *
e_addressbook_table_row_ref_contact (GObject *row_object)
{
	EAddressbookTableNode *node;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_TABLE_NODE (row_object), NULL);

	node = E_ADDRESSBOOK_TABLE_NODE (row_object);

	return node->contact ? g_object_ref (node->contact) : NULL;
}

const gchar * const *
e_addressbook_table_get_legacy_etable_column_ids (guint *out_n_column_ids)
{
	static const gchar *column_ids[N_EAB_TABLE_COLUMNS];
	static gboolean initialized = FALSE;
	guint ii;

	if (!initialized) {
		for (ii = 0; ii < N_EAB_TABLE_COLUMNS; ii++)
			column_ids[ii] = eab_table_columns[ii].id;
		initialized = TRUE;
	}

	if (out_n_column_ids)
		*out_n_column_ids = N_EAB_TABLE_COLUMNS;

	return column_ids;
}

void
_e_addressbook_table_test_enable_local_mode (EAddressbookTable *self,
					     gboolean enable)
{
	self->priv->local_mode = enable;

	e_addressbook_table_model_set_local_mode (self->priv->model, enable);
}

void
_e_addressbook_table_test_push_local_order (EAddressbookTable *self)
{
	GArray *order_columns;
	guint n_group_levels;
	gboolean want_local;

	order_columns = addressbook_table_compute_order_columns (self, &n_group_levels, &want_local);

	e_addressbook_table_model_set_order_columns (self->priv->model, order_columns, n_group_levels);
	e_virtual_tree_set_group_depth (self->priv->vtree, n_group_levels);

	g_array_unref (order_columns);
}

void
_e_addressbook_table_test_local_add_contact (EAddressbookTable *self,
					     EContact *contact)
{
	e_addressbook_table_model_local_add (self->priv->model, contact);
}

void
_e_addressbook_table_test_local_modify_contact (EAddressbookTable *self,
						EContact *contact)
{
	e_addressbook_table_model_local_add (self->priv->model, contact);
}

void
_e_addressbook_table_test_local_remove_contact (EAddressbookTable *self,
						const gchar *uid)
{
	e_addressbook_table_model_local_remove (self->priv->model, uid);
}

void
_e_addressbook_table_test_server_set_total (EAddressbookTable *self,
					    guint total)
{
	EAddressbookTableModel *model = self->priv->model;
	guint old_total = model->cache ? model->cache->len : 0;
	guint ii;

	self->priv->local_mode = FALSE;
	model->local_mode = FALSE;

	if (!model->cache)
		model->cache = g_ptr_array_new ();

	for (ii = 0; ii < model->cache->len; ii++) {
		if (g_ptr_array_index (model->cache, ii))
			g_object_unref (g_ptr_array_index (model->cache, ii));
	}

	g_ptr_array_set_size (model->cache, 0);
	g_ptr_array_set_size (model->cache, total);

	if (old_total != total)
		e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (model));

	if (total > 0)
		e_virtual_tree_model_emit_rows_changed (E_VIRTUAL_TREE_MODEL (model), 0, total - 1);
}

void
_e_addressbook_table_test_server_set_contact (EAddressbookTable *self,
					      guint index,
					      EContact *contact)
{
	EAddressbookTableModel *model = self->priv->model;

	g_return_if_fail (model->cache != NULL);
	g_return_if_fail (index < model->cache->len);

	if (g_ptr_array_index (model->cache, index))
		g_object_unref (g_ptr_array_index (model->cache, index));

	g_ptr_array_index (model->cache, index) = contact ? g_object_ref (contact) : NULL;

	e_virtual_tree_model_emit_rows_changed (E_VIRTUAL_TREE_MODEL (model), index, index);
}

void
_e_addressbook_table_test_capture_pending_select (EAddressbookTable *self)
{
	addressbook_table_capture_pending_select (self);
}
