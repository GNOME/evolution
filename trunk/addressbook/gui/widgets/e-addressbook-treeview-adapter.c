/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>
#include "e-addressbook-model.h"
#include "e-addressbook-treeview-adapter.h"
#include "e-card-merging.h"
#include "eab-gui-util.h"
#include <gtk/gtktreednd.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

struct _EAddressbookTreeViewAdapterPrivate {
	EAddressbookModel *model;

	gint stamp;

	ECardSimple **simples;
	int count;

	int create_card_id, remove_card_id, modify_card_id, model_changed_id;
};

#define PARENT_TYPE G_TYPE_OBJECT
GObjectClass *parent_class;

#define COLS (E_CARD_SIMPLE_FIELD_LAST)

static void
unlink_model(EAddressbookTreeViewAdapter *adapter)
{
	EAddressbookTreeViewAdapterPrivate *priv = adapter->priv;
	int i;

	g_signal_handler_disconnect (priv->model,
				     priv->create_card_id);
	g_signal_handler_disconnect (priv->model,
				     priv->remove_card_id);
	g_signal_handler_disconnect (priv->model,
				     priv->modify_card_id);
	g_signal_handler_disconnect (priv->model,
				     priv->model_changed_id);

	priv->create_card_id = 0;
	priv->remove_card_id = 0;
	priv->modify_card_id = 0;
	priv->model_changed_id = 0;

	/* free up the existing mapping if there is one */
	if (priv->simples) {
		for (i = 0; i < priv->count; i ++)
			g_object_unref (priv->simples[i]);
		g_free (priv->simples);
		priv->simples = NULL;
	}

	g_object_unref (priv->model);

	priv->model = NULL;
}

static void
build_simple_mapping(EAddressbookTreeViewAdapter *adapter)
{
	EAddressbookTreeViewAdapterPrivate *priv = adapter->priv;
	int i;

	/* free up the existing mapping if there is one */
	if (priv->simples) {
		for (i = 0; i < priv->count; i ++)
			g_object_unref (priv->simples[i]);
		g_free (priv->simples);
	}

	/* build up our mapping to ECardSimple*'s */
	priv->count = e_addressbook_model_card_count (priv->model);
	priv->simples = g_new (ECardSimple*, priv->count);
	for (i = 0; i < priv->count; i ++) {
		priv->simples[i] = e_card_simple_new (e_addressbook_model_card_at (priv->model, i));
		g_object_ref (priv->simples[i]);
	}
}

static void
addressbook_destroy(GtkObject *object)
{
	EAddressbookTreeViewAdapter *adapter = E_ADDRESSBOOK_TREEVIEW_ADAPTER(object);

	unlink_model(adapter);

	g_free (adapter->priv);
	adapter->priv = NULL;

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

#if 0
static void
addressbook_set_value_at (ETableModel *etc, int col, int row, const void *val)
{
	EAddressbookTreeViewAdapter *adapter = E_ADDRESSBOOK_TABLE_ADAPTER(etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	if (e_addressbook_model_editable (priv->model)) {
		ECard *card;

		if ( col >= COLS|| row >= e_addressbook_model_card_count (priv->model) )
			return;

		e_table_model_pre_change(etc);

		e_card_simple_set(priv->simples[row],
				  col,
				  val);
		g_object_get(priv->simples[row],
			     "card", &card,
			     NULL);

		e_card_merging_book_commit_card(e_addressbook_model_get_ebook(priv->model),
						card, card_modified_cb, NULL);

		/* XXX do we need this?  shouldn't the commit_card generate a changed signal? */
		e_table_model_cell_changed(etc, col, row);
	}
}

/* This function returns whether a particular cell is editable. */
static gboolean
addressbook_is_cell_editable (ETableModel *etc, int col, int row)
{
	EAddressbookTableAdapter *adapter = E_ADDRESSBOOK_TABLE_ADAPTER(etc);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	ECard *card;

	if (row >= 0 && row < e_addressbook_model_card_count (priv->model))
		card = e_addressbook_model_card_at (priv->model, row);
	else
		card = NULL;

	if (!e_addressbook_model_editable(priv->model))
		return FALSE;
	else if (card && e_card_evolution_list (card))
		/* we only allow editing of the name and file as for
                   lists */
		return col == E_CARD_SIMPLE_FIELD_FULL_NAME || col == E_CARD_SIMPLE_FIELD_FILE_AS; 
	else
		return col < E_CARD_SIMPLE_FIELD_LAST_SIMPLE_STRING;
}

static void
addressbook_append_row (ETableModel *etm, ETableModel *source, gint row)
{
	EAddressbookTableAdapter *adapter = E_ADDRESSBOOK_TABLE_ADAPTER(etm);
	EAddressbookTableAdapterPrivate *priv = adapter->priv;
	ECard *card;
	ECardSimple *simple;
	int col;

	card = e_card_new("");
	simple = e_card_simple_new(card);

	for (col = 0; col < E_CARD_SIMPLE_FIELD_LAST_SIMPLE_STRING; col++) {
		const void *val = e_table_model_value_at(source, col, row);
		e_card_simple_set(simple, col, val);
	}
	e_card_simple_sync_card(simple);
	e_card_merging_book_add_card (e_addressbook_model_get_ebook (priv->model), card, NULL, NULL);
	g_object_unref (simple);
	g_object_unref (card);
}
#endif

static void
e_addressbook_treeview_adapter_class_init (GtkObjectClass *object_class)
{
	parent_class = g_type_class_peek_parent (object_class);

	object_class->destroy = addressbook_destroy;
}

static void
e_addressbook_treeview_adapter_init (GtkObject *object)
{
	EAddressbookTreeViewAdapter *adapter = E_ADDRESSBOOK_TREEVIEW_ADAPTER(object);
	EAddressbookTreeViewAdapterPrivate *priv;

	priv = adapter->priv = g_new0 (EAddressbookTreeViewAdapterPrivate, 1);

	priv->create_card_id = 0;
	priv->remove_card_id = 0;
	priv->modify_card_id = 0;
	priv->model_changed_id = 0;
	priv->simples = NULL;
	priv->count = 0;
}

static void
get_iter (EAddressbookTreeViewAdapter *adapter, gint index, GtkTreeIter *iter)
{
	EAddressbookTreeViewAdapterPrivate *priv = adapter->priv;

	iter->stamp = priv->stamp;
	iter->user_data = GINT_TO_POINTER (index);
}

static void
create_card (EAddressbookModel *model,
	     gint index, gint count,
	     EAddressbookTreeViewAdapter *adapter)
{
	EAddressbookTreeViewAdapterPrivate *priv = adapter->priv;
	int i;

	priv->count += count;
	priv->simples = g_renew(ECardSimple *, priv->simples, priv->count);
	memmove (priv->simples + index + count, priv->simples + index, (priv->count - index - count) * sizeof (ECardSimple *));

	for (i = 0; i < count; i ++) {
		GtkTreeIter iter;
		GtkTreePath *path;

		priv->simples[index + i] = e_card_simple_new (e_addressbook_model_card_at (priv->model, index + i));

		get_iter (adapter, index + i, &iter);
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (adapter), &iter);

		gtk_tree_model_row_inserted (GTK_TREE_MODEL (adapter), path, &iter);
		gtk_tree_model_row_changed (GTK_TREE_MODEL (adapter), path, &iter);

		gtk_tree_path_free (path);
	}
}

static void
remove_card (EAddressbookModel *model,
	     gint index,
	     EAddressbookTreeViewAdapter *adapter)
{
	EAddressbookTreeViewAdapterPrivate *priv = adapter->priv;
	GtkTreeIter iter;
	GtkTreePath *path;

	g_object_unref (priv->simples[index]);
	memmove (priv->simples + index, priv->simples + index + 1, (priv->count - index - 1) * sizeof (ECardSimple *));
	priv->count --;
	get_iter (adapter, index, &iter);
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (adapter), &iter);

	gtk_tree_model_row_deleted (GTK_TREE_MODEL (adapter), path);

	gtk_tree_path_free (path);
}

static void
modify_card (EAddressbookModel *model,
	     gint index,
	     EAddressbookTreeViewAdapter *adapter)
{
	EAddressbookTreeViewAdapterPrivate *priv = adapter->priv;
	GtkTreeIter iter;
	GtkTreePath *path;

	g_object_unref (priv->simples[index]);
	priv->simples[index] = e_card_simple_new (e_addressbook_model_card_at (priv->model, index));

	get_iter (adapter, index, &iter);
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (adapter), &iter);

	gtk_tree_model_row_changed (GTK_TREE_MODEL (adapter), path, &iter);

	gtk_tree_path_free (path);
}

static void
model_changed (EAddressbookModel *model,
	       EAddressbookTreeViewAdapter *adapter)
{
	int i;

	/* there has *got* to be an easier/faster way to do this... */
	for (i = 0; i < adapter->priv->count; i++) {
		remove_card (model, i, adapter);
	}

	build_simple_mapping (adapter);

	if (adapter->priv->count) {
		printf ("AIIEEEEEE\n");
	}

	/* XXX this isn't right either, we need to add the new cards */
}

static GtkTreeModelFlags
adapter_get_flags (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (E_IS_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model), 0);

  return GTK_TREE_MODEL_LIST_ONLY;
}

static gint
adapter_get_n_columns (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (E_IS_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model), 0);

  return COLS;
}

static GType
adapter_get_column_type (GtkTreeModel *tree_model,
			 gint          index)
{
  g_return_val_if_fail (E_IS_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model), G_TYPE_INVALID);
  g_return_val_if_fail (index < COLS && index >= 0, G_TYPE_INVALID);
  
  return G_TYPE_STRING;
}

static gboolean
adapter_get_iter (GtkTreeModel *tree_model,
		  GtkTreeIter  *iter,
		  GtkTreePath  *path)
{
	EAddressbookTreeViewAdapter *adapter;
	GSList *list;
	gint i;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model), FALSE);
	g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

	adapter = E_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model);

	i = gtk_tree_path_get_indices (path)[0];

	if (i >= adapter->priv->count)
		return FALSE;

	iter->stamp = adapter->priv->stamp;
	iter->user_data = GINT_TO_POINTER (i);

	return TRUE;
}

static GtkTreePath *
adapter_get_path (GtkTreeModel *tree_model,
		  GtkTreeIter  *iter)
{
	EAddressbookTreeViewAdapter *adapter = E_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model);
	GtkTreePath *retval;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model), NULL);
	g_return_val_if_fail (iter->stamp == adapter->priv->stamp, NULL);


	if (GPOINTER_TO_INT (iter->user_data) >= adapter->priv->count)
		return NULL;

	retval = gtk_tree_path_new ();
	gtk_tree_path_append_index (retval, GPOINTER_TO_INT (iter->user_data));
	return retval;
}

static void
adapter_get_value (GtkTreeModel *tree_model,
		   GtkTreeIter  *iter,
		   gint          column,
		   GValue       *value)
{
	EAddressbookTreeViewAdapter *adapter = E_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model);
	ECardSimple *simple;
	gint tmp_column = column;
	const char *v;

	g_return_if_fail (E_IS_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model));
	g_return_if_fail (column < COLS);
	g_return_if_fail (adapter->priv->stamp == iter->stamp);

	simple = adapter->priv->simples [ GPOINTER_TO_INT (iter->user_data) ];

	v = e_card_simple_get_const(simple, column);

	g_value_init (value, G_TYPE_STRING);
	g_value_set_string (value, (v ? v : ""));
}

static gboolean
adapter_iter_next (GtkTreeModel  *tree_model,
		   GtkTreeIter   *iter)
{
	EAddressbookTreeViewAdapter *adapter;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model), FALSE);
	g_return_val_if_fail (E_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model)->priv->stamp == iter->stamp, FALSE);

	adapter = E_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model);

	iter->user_data = GINT_TO_POINTER (GPOINTER_TO_INT (iter->user_data) + 1);

	return (GPOINTER_TO_INT (iter->user_data) < adapter->priv->count);
}

static gboolean
adapter_iter_children (GtkTreeModel *tree_model,
		       GtkTreeIter  *iter,
		       GtkTreeIter  *parent)
{
	EAddressbookTreeViewAdapter *adapter = E_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model);

	/* this is a list, nodes have no children */
	if (parent)
		return FALSE;

	/* but if parent == NULL we return the list itself as children of the
	 * "root"
	 */
	if (adapter->priv->count) {
		iter->stamp = adapter->priv->stamp;
		iter->user_data = GINT_TO_POINTER (0);
		return TRUE;
	}
	else
		return FALSE;
}

static gboolean
adapter_iter_has_child (GtkTreeModel *tree_model,
			GtkTreeIter  *iter)
{
	return FALSE;
}

static gint
adapter_iter_n_children (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter)
{
	EAddressbookTreeViewAdapter *adapter = E_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model);

	g_return_val_if_fail (E_IS_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model), -1);
	if (iter == NULL)
		return adapter->priv->count;
			
	g_return_val_if_fail (adapter->priv->stamp == iter->stamp, -1);
	return 0;
}

static gboolean
adapter_iter_nth_child (GtkTreeModel *tree_model,
			GtkTreeIter  *iter,
			GtkTreeIter  *parent,
			gint          n)
{
	EAddressbookTreeViewAdapter *adapter = E_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model);

	g_return_val_if_fail (E_IS_ADDRESSBOOK_TREEVIEW_ADAPTER (tree_model), FALSE);

	if (parent)
		return FALSE;

	if (n < adapter->priv->count) {
		iter->stamp = adapter->priv->stamp;
		iter->user_data = GINT_TO_POINTER (n);
		return TRUE;
	}
	else
		return FALSE;
}

static gboolean
adapter_iter_parent (GtkTreeModel *tree_model,
		     GtkTreeIter  *iter,
		     GtkTreeIter  *child)
{
	return FALSE;
}

static void
adapter_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags = adapter_get_flags;
	iface->get_n_columns = adapter_get_n_columns;
	iface->get_column_type = adapter_get_column_type;
	iface->get_iter = adapter_get_iter;
	iface->get_path = adapter_get_path;
	iface->get_value = adapter_get_value;
	iface->iter_next = adapter_iter_next;
	iface->iter_children = adapter_iter_children;
	iface->iter_has_child = adapter_iter_has_child;
	iface->iter_n_children = adapter_iter_n_children;
	iface->iter_nth_child = adapter_iter_nth_child;
	iface->iter_parent = adapter_iter_parent;
}

static gboolean
adapter_drag_data_delete   (GtkTreeDragSource *drag_source,
			    GtkTreePath       *path)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_TREEVIEW_ADAPTER (drag_source), FALSE);

	return FALSE;
}

static gboolean
adapter_drag_data_get      (GtkTreeDragSource *drag_source,
			    GtkTreePath       *path,
			    GtkSelectionData  *selection_data)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_TREEVIEW_ADAPTER (drag_source), FALSE);

	/* Note that we don't need to handle the GTK_TREE_MODEL_ROW
	 * target, because the default handler does it for us, but
	 * we do anyway for the convenience of someone maybe overriding the
	 * default handler.
	 */

	if (gtk_tree_set_row_drag_data (selection_data,
					GTK_TREE_MODEL (drag_source),
					path)) {
		return TRUE;
	}
	else {
		if (selection_data->target == gdk_atom_intern ("text/x-vcard", FALSE)) {
			printf ("HI THERE\n");
		}
	}

	return FALSE;
}

static void
adapter_drag_source_init (GtkTreeDragSourceIface *iface)
{
	iface->drag_data_delete = adapter_drag_data_delete;
	iface->drag_data_get = adapter_drag_data_get;
}

GType
e_addressbook_treeview_adapter_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo adapter_info =  {
			sizeof (EAddressbookTreeViewAdapterClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_addressbook_treeview_adapter_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EAddressbookTreeViewAdapter),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_addressbook_treeview_adapter_init,
		};

		static const GInterfaceInfo tree_model_info = {
			(GInterfaceInitFunc) adapter_tree_model_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo drag_source_info = {
			(GInterfaceInitFunc) adapter_drag_source_init,
			NULL,
			NULL
		};

		type = g_type_register_static (PARENT_TYPE, "EAddressbookTreeViewAdapter", &adapter_info, 0);

		g_type_add_interface_static (type,
					     GTK_TYPE_TREE_MODEL,
					     &tree_model_info);

		g_type_add_interface_static (type,
					     GTK_TYPE_TREE_DRAG_SOURCE,
					     &drag_source_info);
	}

	return type;
}

void
e_addressbook_treeview_adapter_construct (EAddressbookTreeViewAdapter *adapter,
					  EAddressbookModel *model)
{
	EAddressbookTreeViewAdapterPrivate *priv = adapter->priv;

	priv->model = model;
	g_object_ref (priv->model);

	priv->stamp = g_random_int ();

	priv->create_card_id = g_signal_connect(priv->model,
						"card_added",
						G_CALLBACK(create_card),
						adapter);
	priv->remove_card_id = g_signal_connect(priv->model,
						"card_removed",
						G_CALLBACK(remove_card),
						adapter);
	priv->modify_card_id = g_signal_connect(priv->model,
						"card_changed",
						G_CALLBACK(modify_card),
						adapter);
	priv->model_changed_id = g_signal_connect(priv->model,
						  "model_changed",
						  G_CALLBACK(model_changed),
						  adapter);

	build_simple_mapping (adapter);
}

GtkTreeModel *
e_addressbook_treeview_adapter_new (EAddressbookModel *model)
{
	EAddressbookTreeViewAdapter *et;

	et = g_object_new(E_TYPE_ADDRESSBOOK_TREEVIEW_ADAPTER, NULL);

	e_addressbook_treeview_adapter_construct (et, model);

	return (GtkTreeModel*)et;
}
