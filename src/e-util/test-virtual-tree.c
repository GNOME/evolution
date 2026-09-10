/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <e-util/e-util.h>

#define TEST_TYPE_NODE (test_node_get_type ())
G_DECLARE_FINAL_TYPE (TestNode, test_node, TEST, NODE, GObject)

struct _TestNode {
	GObject parent_instance;
	gchar *label;
	GNode *gnode;
	gboolean checked;
	GdkRGBA *bg_color;
};

G_DEFINE_FINAL_TYPE (TestNode, test_node, G_TYPE_OBJECT)

static void
test_node_finalize (GObject *object)
{
	TestNode *self = TEST_NODE (object);

	g_free (self->label);
	g_clear_pointer (&self->bg_color, gdk_rgba_free);

	G_OBJECT_CLASS (test_node_parent_class)->finalize (object);
}

static void
test_node_class_init (TestNodeClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = test_node_finalize;
}

static void
test_node_init (TestNode *self)
{
}

static TestNode *
test_node_new (const gchar *label)
{
	TestNode *node = g_object_new (TEST_TYPE_NODE, NULL);

	node->label = g_strdup (label);

	return node;
}

#define TEST_TYPE_VIRTUAL_TREE_MODEL (test_virtual_tree_model_get_type ())
G_DECLARE_FINAL_TYPE (TestVirtualTreeModel, test_virtual_tree_model,
	TEST, VIRTUAL_TREE_MODEL, GObject)

struct _TestVirtualTreeModel {
	GObject parent_instance;
	GNode *root;
	GHashTable *expanded; /* TestNode* -> TRUE */
	GHashTable *rows_fetched; /* TestNode* -> TRUE, unique rows ever requested */
};

static void test_virtual_tree_model_iface_init (EVirtualTreeModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (TestVirtualTreeModel, test_virtual_tree_model, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (E_TYPE_VIRTUAL_TREE_MODEL, test_virtual_tree_model_iface_init))

/* Walk the visible (expanded) tree in pre-order, skipping root */
static gboolean
walk_visible (GNode *node,
	      GHashTable *expanded,
	      gint *counter,
	      gint target_start,
	      gint target_end,
	      gboolean include_collapsed,
	      GPtrArray *result)
{
	GNode *child;

	if (!G_NODE_IS_ROOT (node)) {
		TestNode *test_node = node->data;

		if (*counter >= target_start && *counter <= target_end && result)
			g_ptr_array_add (result, g_object_ref (test_node));

		if (*counter > target_end && !include_collapsed)
			return TRUE;

		(*counter)++;
	}

	if (G_NODE_IS_ROOT (node) ||
	    g_hash_table_contains (expanded, node->data) ||
	    include_collapsed) {
		for (child = node->children; child; child = child->next) {
			if (walk_visible (child, expanded, counter,
					  target_start, target_end,
					  include_collapsed, result))
				return TRUE;
		}
	}

	return FALSE;
}

static guint
test_model_get_row_count (EVirtualTreeModel *model)
{
	TestVirtualTreeModel *self = TEST_VIRTUAL_TREE_MODEL (model);
	gint count = 0;

	walk_visible (self->root, self->expanded, &count, 0, G_MAXINT, FALSE, NULL);

	return (guint) count;
}

static GPtrArray *
test_model_get_rows (EVirtualTreeModel *model,
		     guint first_row,
		     guint last_row,
		     gboolean include_collapsed)
{
	TestVirtualTreeModel *self = TEST_VIRTUAL_TREE_MODEL (model);
	GPtrArray *result;
	gint counter = 0;
	guint ii;

	result = g_ptr_array_new_with_free_func (g_object_unref);
	walk_visible (self->root, self->expanded, &counter,
		      (gint) first_row, (gint) last_row, include_collapsed, result);

	for (ii = 0; ii < result->len; ii++) {
		g_hash_table_add (self->rows_fetched, g_ptr_array_index (result, ii));
	}

	return result;
}

static guint
test_model_get_depth (EVirtualTreeModel *model,
		      GObject *row_object)
{
	TestNode *node = TEST_NODE (row_object);

	return g_node_depth (node->gnode) - 2;
}

static gboolean
test_model_is_expandable (EVirtualTreeModel *model,
			  GObject *row_object)
{
	TestNode *node = TEST_NODE (row_object);

	return node->gnode->children != NULL;
}

static gboolean
test_model_get_expanded (EVirtualTreeModel *model,
			 GObject *row_object)
{
	TestVirtualTreeModel *self = TEST_VIRTUAL_TREE_MODEL (model);

	return g_hash_table_contains (self->expanded, row_object);
}

/* Count visible descendants of a node */
static guint
count_visible_descendants (GNode *node,
			   GHashTable *expanded)
{
	GNode *child;
	guint count = 0;

	for (child = node->children; child; child = child->next) {
		count++;
		if (g_hash_table_contains (expanded, child->data))
			count += count_visible_descendants (child, expanded);
	}

	return count;
}

/* Find the visible row index for a given GNode */
static gboolean
find_visible_index_recurse (GNode *node,
			    GNode *target,
			    GHashTable *expanded,
			    gint *counter)
{
	GNode *child;

	for (child = node->children; child; child = child->next) {
		if (child == target)
			return TRUE;
		(*counter)++;

		if (g_hash_table_contains (expanded, child->data)) {
			if (find_visible_index_recurse (child, target, expanded, counter))
				return TRUE;
		}
	}

	return FALSE;
}

static gint
find_visible_index (GNode *root,
		    GNode *target,
		    GHashTable *expanded)
{
	gint counter = 0;

	if (find_visible_index_recurse (root, target, expanded, &counter))
		return counter;

	return -1;
}

static void
test_model_set_expanded (EVirtualTreeModel *model,
			 GObject *row_object,
			 gboolean expanded)
{
	TestVirtualTreeModel *self = TEST_VIRTUAL_TREE_MODEL (model);
	TestNode *node = TEST_NODE (row_object);
	guint n_desc;
	gint row_index;

	if (expanded == g_hash_table_contains (self->expanded, row_object))
		return;

	row_index = find_visible_index (self->root, node->gnode, self->expanded);
	if (row_index < 0)
		return;

	if (expanded) {
		g_hash_table_add (self->expanded, row_object);

		n_desc = count_visible_descendants (node->gnode, self->expanded);
		if (n_desc > 0)
			e_virtual_tree_model_emit_rows_inserted (model, row_index + 1, row_index + n_desc);
	} else {
		n_desc = count_visible_descendants (node->gnode, self->expanded);
		g_hash_table_remove (self->expanded, row_object);

		if (n_desc > 0)
			e_virtual_tree_model_emit_rows_removed (model, row_index + 1, row_index + n_desc);
	}

	e_virtual_tree_model_emit_rows_changed (model, row_index, row_index);
}

static void
test_virtual_tree_model_iface_init (EVirtualTreeModelInterface *iface)
{
	iface->get_row_count = test_model_get_row_count;
	iface->dup_rows = test_model_get_rows;
	iface->get_depth = test_model_get_depth;
	iface->is_expandable = test_model_is_expandable;
	iface->get_expanded = test_model_get_expanded;
	iface->set_expanded = test_model_set_expanded;
}

static gboolean
unref_node_data (GNode *node,
		 gpointer data)
{
	if (node->data) {
		TEST_NODE (node->data)->gnode = NULL;
		g_object_unref (node->data);
	}
	return FALSE;
}

static void
test_virtual_tree_model_finalize (GObject *object)
{
	TestVirtualTreeModel *self = TEST_VIRTUAL_TREE_MODEL (object);

	g_hash_table_unref (self->expanded);
	g_hash_table_unref (self->rows_fetched);

	if (self->root) {
		g_node_traverse (self->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			unref_node_data, NULL);
		g_node_destroy (self->root);
	}

	G_OBJECT_CLASS (test_virtual_tree_model_parent_class)->finalize (object);
}

static void
test_virtual_tree_model_class_init (TestVirtualTreeModelClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = test_virtual_tree_model_finalize;
}

static void
test_virtual_tree_model_init (TestVirtualTreeModel *self)
{
	self->expanded = g_hash_table_new (g_direct_hash, g_direct_equal);
	self->rows_fetched = g_hash_table_new (g_direct_hash, g_direct_equal);
	self->root = g_node_new (NULL);
}

static GNode *
add_test_node (TestVirtualTreeModel *model,
	       GNode *parent,
	       const gchar *label)
{
	TestNode *node = test_node_new (label);
	GNode *gnode = g_node_append_data (parent, node);

	node->gnode = gnode;

	return gnode;
}

static void
build_test_data (TestVirtualTreeModel *model,
		 GtkTreeStore *left_store)
{
	GNode *a, *b, *c, *a1, *a2, *b1;
	GtkTreeIter ia, ib, ic, ia1, ia2, ib1, tmp;

	/* Build tree:
	 * A
	 *   A1
	 *     A1a
	 *     A1b
	 *   A2
	 * B
	 *   B1
	 *     B1a
	 * C
	 */

	a = add_test_node (model, model->root, "A");
	a1 = add_test_node (model, a, "A1");
	add_test_node (model, a1, "A1a");
	add_test_node (model, a1, "A1b");
	a2 = add_test_node (model, a, "A2");
	(void) a2;

	b = add_test_node (model, model->root, "B - This is a long group label to test ellipsization near the edge of the visible width");
	b1 = add_test_node (model, b, "B1");
	add_test_node (model, b1, "B1a");

	c = add_test_node (model, model->root, "C");
	(void) c;

	/* Expand all parent nodes by default */
	g_hash_table_add (model->expanded, a->data);
	g_hash_table_add (model->expanded, a1->data);
	g_hash_table_add (model->expanded, b->data);
	g_hash_table_add (model->expanded, b1->data);

	/* Populate left tree store with the same data */
	gtk_tree_store_append (left_store, &ia, NULL);
	gtk_tree_store_set (left_store, &ia, 0, "A", -1);

	gtk_tree_store_append (left_store, &ia1, &ia);
	gtk_tree_store_set (left_store, &ia1, 0, "A1", -1);

	gtk_tree_store_append (left_store, &tmp, &ia1);
	gtk_tree_store_set (left_store, &tmp, 0, "A1a", -1);
	gtk_tree_store_append (left_store, &tmp, &ia1);
	gtk_tree_store_set (left_store, &tmp, 0, "A1b", -1);

	gtk_tree_store_append (left_store, &ia2, &ia);
	gtk_tree_store_set (left_store, &ia2, 0, "A2", -1);

	gtk_tree_store_append (left_store, &ib, NULL);
	gtk_tree_store_set (left_store, &ib, 0, "B - This is a long group label to test ellipsization near the edge of the visible width", -1);

	gtk_tree_store_append (left_store, &ib1, &ib);
	gtk_tree_store_set (left_store, &ib1, 0, "B1", -1);

	gtk_tree_store_append (left_store, &tmp, &ib1);
	gtk_tree_store_set (left_store, &tmp, 0, "B1a", -1);

	gtk_tree_store_append (left_store, &ic, NULL);
	gtk_tree_store_set (left_store, &ic, 0, "C", -1);
}

static GtkTextBuffer *log_buffer;
static GtkTextView *log_text_view;

static void
log_message (const gchar *fmt, ...)
{
	va_list args;
	gchar *text;
	gchar *line;
	GtkTextIter end;
	GDateTime *now;
	gchar *timestamp;

	va_start (args, fmt);
	text = g_strdup_vprintf (fmt, args);
	va_end (args);

	now = g_date_time_new_now_local ();
	timestamp = g_strdup_printf ("%02d:%02d:%02d.%03d",
		g_date_time_get_hour (now),
		g_date_time_get_minute (now),
		g_date_time_get_second (now),
		g_date_time_get_microsecond (now) / 1000);

	gtk_text_buffer_get_end_iter (log_buffer, &end);

	line = g_strdup_printf ("[%s] %s\n", timestamp, text);
	gtk_text_buffer_insert (log_buffer, &end, line, -1);
	g_free (line);

	/* Auto-scroll to bottom */
	if (log_text_view) {
		GtkTextMark *mark;

		gtk_text_buffer_get_end_iter (log_buffer, &end);
		mark = gtk_text_buffer_get_mark (log_buffer, "end");
		if (!mark)
			mark = gtk_text_buffer_create_mark (log_buffer, "end", &end, FALSE);
		else
			gtk_text_buffer_move_mark (log_buffer, mark, &end);

		gtk_text_view_scroll_to_mark (log_text_view, mark, 0.0, FALSE, 0.0, 0.0);
	}

	g_free (timestamp);
	g_date_time_unref (now);
	g_free (text);
}

static GtkCellRenderer *label_renderer;
static GtkCellRenderer *link_renderer;
static GtkCellRenderer *button_renderer;
static GtkCellRenderer *color_renderer;
static GtkCellRenderer *toggle_renderer;

static void
on_model_rows_changed_log (EVirtualTreeModel *model,
			   guint first,
			   guint last,
			   gpointer user_data)
{
	log_message ("model::rows-changed(%u, %u)", first, last);
}

static void
on_model_rows_inserted_log (EVirtualTreeModel *model,
			    guint first,
			    guint last,
			    gpointer user_data)
{
	log_message ("model::rows-inserted(%u, %u)", first, last);
}

static void
on_model_rows_removed_log (EVirtualTreeModel *model,
			   guint first,
			   guint last,
			   gpointer user_data)
{
	log_message ("model::rows-removed(%u, %u)", first, last);
}

static void
on_model_row_count_changed_log (EVirtualTreeModel *model,
				gpointer user_data)
{
	log_message ("model::row-count-changed (total=%u)",
		e_virtual_tree_model_get_row_count (model));
}

static void
on_vtree_selection_changed_log (EVirtualTree *tree,
				gpointer user_data)
{
	log_message ("vtree::selection-changed");
}

static void
on_vtree_cursor_changed_log (EVirtualTree *tree,
			     guint row,
			     GObject *row_object,
			     gpointer user_data)
{
	TestNode *node = TEST_NODE (row_object);

	log_message ("vtree::cursor-changed(row=%u, label=\"%s\")", row, node->label);
}

static void
on_vtree_row_expanded_log (EVirtualTree *tree,
			   guint row,
			   GObject *row_object,
			   gboolean expanded,
			   gpointer user_data)
{
	TestNode *node = TEST_NODE (row_object);

	log_message ("vtree::row-expanded(row=%u, label=\"%s\", expanded=%s)",
		row, node->label, expanded ? "TRUE" : "FALSE");
}

static void
on_vtree_row_activated_log (EVirtualTree *tree,
			    guint row,
			    GObject *row_object,
			    gpointer user_data)
{
	TestNode *node = TEST_NODE (row_object);

	log_message ("vtree::row-activated(row=%u, label=\"%s\")", row, node->label);
}

typedef struct {
	EVirtualTreeModel *model;
	guint row;
} DeferredRowChanged;

static gboolean
emit_rows_changed_idle_cb (gpointer user_data)
{
	DeferredRowChanged *data = user_data;

	e_virtual_tree_model_emit_rows_changed (data->model, data->row, data->row);

	g_object_unref (data->model);
	g_free (data);

	return G_SOURCE_REMOVE;
}

static gboolean
on_vtree_cell_clicked_log (EVirtualTree *tree,
			   guint row,
			   GObject *row_object,
			   guint column,
			   GtkCellRenderer *renderer,
			   gpointer user_data)
{
	TestNode *node = TEST_NODE (row_object);

	if (renderer == link_renderer) {
		log_message ("*** LINK clicked on \"%s\"", node->label);
		return TRUE;
	} else if (renderer == button_renderer) {
		log_message ("*** BUTTON clicked on \"%s\"", node->label);
		return TRUE;
	} else if (renderer == toggle_renderer && gtk_cell_renderer_toggle_get_activatable (GTK_CELL_RENDERER_TOGGLE (renderer))) {
		node->checked = !node->checked;
		log_message ("*** TOGGLE %s on \"%s\"", node->checked ? "ON" : "OFF", node->label);
		e_virtual_tree_model_emit_rows_changed (E_VIRTUAL_TREE_MODEL (e_virtual_tree_get_model (tree)), row, row);
		return TRUE;
	} else if (renderer == color_renderer) {
		GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (e_virtual_tree_get_tree_view (tree)));
		GtkWidget *dialog = gtk_color_chooser_dialog_new ("Pick Row Background Color",
			GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL);

		if (node->bg_color)
			gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dialog), node->bg_color);

		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
			GdkRGBA rgba;
			DeferredRowChanged *data;

			gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog), &rgba);

			g_clear_pointer (&node->bg_color, gdk_rgba_free);
			node->bg_color = gdk_rgba_copy (&rgba);

			log_message ("*** Set row background color for \"%s\"", node->label);

			data = g_new0 (DeferredRowChanged, 1);
			data->model = g_object_ref (e_virtual_tree_get_model (tree));
			data->row = row;
			g_idle_add (emit_rows_changed_idle_cb, data);
		}

		gtk_widget_destroy (dialog);

		return TRUE;
	}

	log_message ("vtree::cell-clicked(row=%u, label=\"%s\", col=%u, renderer=%p [%s])",
		row, node->label, column, (gpointer) renderer,
		renderer ? G_OBJECT_TYPE_NAME (renderer) : "null");

	return FALSE;
}

static gboolean
on_vtree_right_click_log (EVirtualTree *tree,
			  guint row,
			  GObject *row_object,
			  GdkEvent *event,
			  gpointer user_data)
{
	TestNode *node = TEST_NODE (row_object);

	log_message ("vtree::right-click(row=%u, label=\"%s\")", row, node->label);
	return TRUE;
}

static gchar *
search_func (EVirtualTree *tree,
	     GObject *row_object,
	     gpointer user_data)
{
	TestNode *node = TEST_NODE (row_object);

	return g_strdup (node->label);
}

static void
label_data_func (EVirtualTree *tree,
		 GtkCellRenderer *renderer,
		 GObject *row_object,
		 guint visible_row,
		 gpointer user_data)
{
	TestNode *node = TEST_NODE (row_object);

	g_object_set (renderer, "text", node->label, NULL);
}

static void
depth_data_func (EVirtualTree *tree,
		 GtkCellRenderer *renderer,
		 GObject *row_object,
		 guint visible_row,
		 gpointer user_data)
{
	TestNode *node = TEST_NODE (row_object);
	gchar *text;

	text = g_strdup_printf ("%u children",
		node->gnode ? g_node_n_children (node->gnode) : 0);
	g_object_set (renderer, "text", text, NULL);
	g_free (text);
}

static void
toggle_data_func (EVirtualTree *tree,
		  GtkCellRenderer *renderer,
		  GObject *row_object,
		  guint visible_row,
		  gpointer user_data)
{
	TestNode *node = TEST_NODE (row_object);

	g_object_set (renderer, "active", node->checked, NULL);
}

static void
link_data_func (EVirtualTree *tree,
		GtkCellRenderer *renderer,
		GObject *row_object,
		guint visible_row,
		gpointer user_data)
{
	TestNode *node = TEST_NODE (row_object);
	gboolean is_a1 = g_strcmp0 (node->label, "A1") == 0;

	gtk_cell_renderer_set_visible (renderer, is_a1);
	if (is_a1) {
		g_object_set (renderer,
			"markup", "<u><span foreground=\"#0066cc\">Details...</span></u>",
			NULL);
	}
}

static void
button_data_func (EVirtualTree *tree,
		  GtkCellRenderer *renderer,
		  GObject *row_object,
		  guint visible_row,
		  gpointer user_data)
{
	TestNode *node = TEST_NODE (row_object);
	gboolean is_a1 = g_strcmp0 (node->label, "A1") == 0;

	gtk_cell_renderer_set_visible (renderer, is_a1);
}

static void
color_data_func (EVirtualTree *tree,
		 GtkCellRenderer *renderer,
		 GObject *row_object,
		 guint visible_row,
		 gpointer user_data)
{
	TestNode *node = TEST_NODE (row_object);

	g_object_set (renderer, "text", node->bg_color ? "Color: set" : "Color...", NULL);
}

static gboolean
test_row_background_func (EVirtualTree *tree,
			  GObject *row_object,
			  guint visible_row,
			  GdkRGBA *out_color,
			  gpointer user_data)
{
	TestNode *node = TEST_NODE (row_object);

	if (!node->bg_color)
		return FALSE;

	*out_color = *node->bg_color;

	return TRUE;
}

typedef struct {
	TestVirtualTreeModel *model;
	EVirtualTree *vtree;
	GtkTreeView *left_tree;
	guint label_column;
	gboolean two_lines;
	gint next_id;
} TestContext;

static void
on_add_child_clicked (GtkButton *button,
		      TestContext *ctx)
{
	gint cursor = e_virtual_tree_get_cursor (ctx->vtree);
	gchar *label;
	GNode *parent_gnode;
	gint parent_index;
	gboolean was_leaf;

	label = g_strdup_printf ("New-%d", ctx->next_id++);

	if (cursor >= 0) {
		GPtrArray *rows = e_virtual_tree_model_dup_rows (E_VIRTUAL_TREE_MODEL (ctx->model), cursor, cursor, FALSE);
		if (rows && rows->len > 0) {
			TestNode *parent_node = TEST_NODE (g_ptr_array_index (rows, 0));
			parent_gnode = parent_node->gnode;
		} else {
			parent_gnode = ctx->model->root;
		}
		if (rows)
			g_ptr_array_unref (rows);
	} else {
		parent_gnode = ctx->model->root;
	}

	was_leaf = (parent_gnode->children == NULL);

	add_test_node (ctx->model, parent_gnode, label);

	if (was_leaf && !G_NODE_IS_ROOT (parent_gnode)) {
		g_hash_table_add (ctx->model->expanded, parent_gnode->data);
	}

	parent_index = find_visible_index (ctx->model->root, parent_gnode, ctx->model->expanded);

	if (!G_NODE_IS_ROOT (parent_gnode) && parent_index >= 0) {
		e_virtual_tree_model_emit_rows_changed (E_VIRTUAL_TREE_MODEL (ctx->model), parent_index, parent_index);
	}

	if (parent_index >= 0 &&
	    g_hash_table_contains (ctx->model->expanded, parent_gnode->data)) {
		guint insert_pos = parent_index + count_visible_descendants (parent_gnode, ctx->model->expanded);

		e_virtual_tree_model_emit_rows_inserted (E_VIRTUAL_TREE_MODEL (ctx->model), insert_pos, insert_pos);
	} else if (G_NODE_IS_ROOT (parent_gnode)) {
		guint total = e_virtual_tree_model_get_row_count (E_VIRTUAL_TREE_MODEL (ctx->model));
		e_virtual_tree_model_emit_rows_inserted (E_VIRTUAL_TREE_MODEL (ctx->model), total - 1, total - 1);
	}

	log_message ("Added node \"%s\"", label);
	g_free (label);
}

static gboolean
expand_all_traverse (GNode *node,
		     gpointer data)
{
	TestVirtualTreeModel *model = data;

	if (!G_NODE_IS_ROOT (node) && node->children)
		g_hash_table_add (model->expanded, node->data);
	return FALSE;
}

static void
on_expand_all_clicked (GtkButton *button,
		       TestContext *ctx)
{
	g_node_traverse (ctx->model->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, expand_all_traverse, ctx->model);

	e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (ctx->model));

	log_message ("Expanded all");
}

static void
on_collapse_all_clicked (GtkButton *button,
			 TestContext *ctx)
{
	g_hash_table_remove_all (ctx->model->expanded);
	e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (ctx->model));

	log_message ("Collapsed all");
}

static void
on_add_1000_clicked (GtkButton *button,
		     TestContext *ctx)
{
	gint cursor = e_virtual_tree_get_cursor (ctx->vtree);
	GNode *parent_gnode;
	guint old_count, new_count, ii;

	if (cursor >= 0) {
		GPtrArray *rows = e_virtual_tree_model_dup_rows (E_VIRTUAL_TREE_MODEL (ctx->model), cursor, cursor, FALSE);
		if (rows && rows->len > 0) {
			TestNode *node = TEST_NODE (g_ptr_array_index (rows, 0));
			parent_gnode = node->gnode;
		} else {
			parent_gnode = ctx->model->root;
		}
		if (rows)
			g_ptr_array_unref (rows);
	} else {
		parent_gnode = ctx->model->root;
	}

	if (!G_NODE_IS_ROOT (parent_gnode))
		g_hash_table_add (ctx->model->expanded, parent_gnode->data);

	old_count = e_virtual_tree_model_get_row_count (E_VIRTUAL_TREE_MODEL (ctx->model));

	for (ii = 0; ii < 1000; ii++) {
		gchar *label = g_strdup_printf ("Bulk-%d", ctx->next_id++);
		add_test_node (ctx->model, parent_gnode, label);
		g_free (label);
	}

	new_count = e_virtual_tree_model_get_row_count (E_VIRTUAL_TREE_MODEL (ctx->model));
	if (new_count > old_count)
		e_virtual_tree_model_emit_rows_inserted (E_VIRTUAL_TREE_MODEL (ctx->model), old_count, new_count - 1);

	log_message ("Added 1000 children under %s (total=%u)",
		G_NODE_IS_ROOT (parent_gnode) ? "root" : TEST_NODE (parent_gnode->data)->label,
		e_virtual_tree_model_get_row_count (E_VIRTUAL_TREE_MODEL (ctx->model)));
}

static void
on_toggle_groups_clicked (GtkButton *button,
			  TestContext *ctx)
{
	guint depth = e_virtual_tree_get_group_depth (ctx->vtree);

	depth = (depth == 0) ? 1 : 0;
	e_virtual_tree_set_group_depth (ctx->vtree, depth);
	log_message ("Group depth set to %u", depth);
}

static void
on_rebuild_clicked (GtkButton *button,
		    TestContext *ctx)
{
	log_message ("Emitting before-rebuild / after-rebuild");
	e_virtual_tree_model_emit_before_rebuild (E_VIRTUAL_TREE_MODEL (ctx->model));
	e_virtual_tree_model_emit_after_rebuild (E_VIRTUAL_TREE_MODEL (ctx->model));
}

static void
on_toggle_editable_clicked (GtkButton *button,
			    TestContext *ctx)
{
	gboolean editable = !e_virtual_tree_get_editable (ctx->vtree);

	e_virtual_tree_set_editable (ctx->vtree, editable);
	log_message ("Editable set to %s", editable ? "TRUE" : "FALSE");
}

static void
rebuild_label_column (TestContext *ctx)
{
	GtkCellRenderer *renderer;

	e_virtual_tree_column_clear_renderers (ctx->vtree, ctx->label_column);

	toggle_renderer = gtk_cell_renderer_toggle_new ();
	g_object_bind_property (ctx->vtree, "editable", toggle_renderer, "activatable", G_BINDING_SYNC_CREATE);
	e_virtual_tree_column_pack_start (ctx->vtree, ctx->label_column, 0, toggle_renderer, FALSE, toggle_data_func, NULL, NULL);

	label_renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (ctx->vtree, ctx->label_column, 0, label_renderer, TRUE, label_data_func, NULL, NULL);

	if (ctx->two_lines) {
		renderer = gtk_cell_renderer_text_new ();
		g_object_set (renderer,
			"scale", 0.8,
			"foreground", "#888888",
			NULL);
		e_virtual_tree_column_pack_start (ctx->vtree, ctx->label_column, 1, renderer, TRUE, depth_data_func, NULL, NULL);
	}

	e_virtual_tree_set_expander_column (ctx->vtree, ctx->label_column, 0);

	e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (ctx->model));
}

static void
on_toggle_two_lines_clicked (GtkButton *button,
			     TestContext *ctx)
{
	ctx->two_lines = !ctx->two_lines;
	rebuild_label_column (ctx);
	log_message ("Two-line rows: %s", ctx->two_lines ? "ON" : "OFF");
}

static void
on_selection_mode_clicked (GtkButton *button,
			   TestContext *ctx)
{
	GtkSelectionMode mode;

	mode = e_virtual_tree_get_selection_mode (ctx->vtree);

	switch (mode) {
	case GTK_SELECTION_SINGLE:
		mode = GTK_SELECTION_MULTIPLE;
		gtk_button_set_label (button, "Sel: Multiple");
		break;
	case GTK_SELECTION_MULTIPLE:
		mode = GTK_SELECTION_BROWSE;
		gtk_button_set_label (button, "Sel: Browse");
		break;
	case GTK_SELECTION_BROWSE:
		mode = GTK_SELECTION_NONE;
		gtk_button_set_label (button, "Sel: None");
		break;
	default:
		mode = GTK_SELECTION_SINGLE;
		gtk_button_set_label (button, "Sel: Single");
		break;
	}

	e_virtual_tree_set_selection_mode (ctx->vtree, mode);

	if (ctx->left_tree)
		gtk_tree_selection_set_mode (gtk_tree_view_get_selection (ctx->left_tree), mode);
}

static void
on_show_stats_clicked (GtkButton *button,
		       TestContext *ctx)
{
	guint total = g_node_n_nodes (ctx->model->root, G_TRAVERSE_ALL) - 1;
	guint visible = e_virtual_tree_model_get_row_count (E_VIRTUAL_TREE_MODEL (ctx->model));

	log_message ("Stats: rows total=%u visible=%u unique fetched=%u",
		total, visible, g_hash_table_size (ctx->model->rows_fetched));
}

static void
on_empty_message_changed (GtkEntry *entry,
			  TestContext *ctx)
{
	const gchar *text = gtk_entry_get_text (entry);

	e_virtual_tree_set_empty_message (ctx->vtree, (text && *text) ? text : NULL);
	log_message ("Empty message set to: \"%s\"", text ? text : "(null)");
}

static gint
compare_nodes_by_label (GNode *a,
			GNode *b,
			gboolean ascending)
{
	TestNode *na = a->data;
	TestNode *nb = b->data;
	gint result = g_strcmp0 (na->label, nb->label);

	return ascending ? result : -result;
}

static void
sort_children (GNode *parent,
	       gboolean ascending)
{
	GNode *child;
	GList *list = NULL, *link;

	for (child = parent->children; child; child = child->next) {
		list = g_list_prepend (list, child);
	}

	list = g_list_sort_with_data (list,
		(GCompareDataFunc) compare_nodes_by_label,
		GINT_TO_POINTER (ascending));

	/* Re-link children in sorted order */
	parent->children = NULL;
	for (link = list; link; link = link->next) {
		GNode *node = link->data;

		node->prev = NULL;
		node->next = NULL;
		node->parent = NULL;
		g_node_append (parent, node);

		sort_children (node, ascending);
	}

	g_list_free (list);
}

static void
on_vtree_column_state_changed_log (EVirtualTree *tree,
				   TestContext *ctx)
{
	GtkSortType sort_order;
	gint priority;
	gboolean sorted;
	const gchar *label;
	gint cursor;
	GObject *cursor_obj = NULL;

	sorted = e_virtual_tree_get_column_sort (tree, 0, &sort_order, &priority);

	cursor = e_virtual_tree_get_cursor (tree);

	if (cursor >= 0) {
		GPtrArray *rows = e_virtual_tree_model_dup_rows (E_VIRTUAL_TREE_MODEL (ctx->model), cursor, cursor, FALSE);
		if (rows && rows->len > 0)
			cursor_obj = g_object_ref (g_ptr_array_index (rows, 0));
		if (rows)
			g_ptr_array_unref (rows);
	}

	if (sorted) {
		sort_children (ctx->model->root,
			sort_order == GTK_SORT_ASCENDING);
		label = (sort_order == GTK_SORT_ASCENDING) ? "asc" : "desc";
	} else {
		label = "none";
	}

	if (sorted) {
		e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (ctx->model));

		if (cursor_obj) {
			TestNode *node = TEST_NODE (cursor_obj);
			gint new_idx = find_visible_index (ctx->model->root, node->gnode, ctx->model->expanded);

			if (new_idx >= 0) {
				e_virtual_tree_set_cursor (tree, new_idx);
				e_virtual_tree_scroll_to_row (tree, (guint) new_idx);
			}
		}
	}

	g_clear_object (&cursor_obj);

	log_message ("vtree::column-state-changed(col=0, sort=%s)", label);
}

static void
on_vtree_cell_edited_log (EVirtualTree *tree,
			  guint row,
			  GObject *row_object,
			  guint column,
			  GtkCellRenderer *renderer,
			  const gchar *new_text,
			  gpointer user_data)
{
	TestNode *node = TEST_NODE (row_object);

	log_message ("vtree::cell-edited(row=%u, label=\"%s\", col=%u, renderer=%p, new_text=\"%s\")",
		row, node->label, column, (gpointer) renderer, new_text);

	if (renderer == label_renderer) {
		g_free (node->label);
		node->label = g_strdup (new_text);
	}
}

static void
on_remove_selected_clicked (GtkButton *button,
			    TestContext *ctx)
{
	gint cursor = e_virtual_tree_get_cursor (ctx->vtree);
	GObject *row_obj;
	TestNode *node;
	GNode *gnode;
	guint visible_count_before;
	guint n_visible_removed;

	if (cursor < 0) {
		log_message ("Remove: no row selected");
		return;
	}

	row_obj = e_virtual_tree_model_dup_row (E_VIRTUAL_TREE_MODEL (ctx->model), cursor);
	if (!row_obj)
		return;

	node = TEST_NODE (row_obj);
	gnode = node->gnode;

	if (G_NODE_IS_ROOT (gnode)) {
		g_clear_object (&row_obj);
		return;
	}

	visible_count_before = e_virtual_tree_model_get_row_count (E_VIRTUAL_TREE_MODEL (ctx->model));

	/* Remove expanded state for this node and descendants */
	g_hash_table_remove (ctx->model->expanded, node);

	n_visible_removed = 1 + count_visible_descendants (gnode, ctx->model->expanded);

	log_message ("Removing \"%s\" (row %d, %u visible rows)",
		node->label, cursor, n_visible_removed);

	g_clear_object (&row_obj);

	g_node_unlink (gnode);

	if (n_visible_removed > 0 && (guint) cursor < visible_count_before)
		e_virtual_tree_model_emit_rows_removed (E_VIRTUAL_TREE_MODEL (ctx->model), cursor, cursor + n_visible_removed - 1);

	/* Free the node and its descendants */
	g_node_traverse (gnode, G_PRE_ORDER, G_TRAVERSE_ALL, -1, unref_node_data, NULL);
	g_node_destroy (gnode);
}

static gboolean
on_idle_create_widget (gpointer user_data)
{
	GtkWidget *window, *vpaned, *hpaned, *toolbar, *toolbar2;
	GtkWidget *scrolled_log, *log_view;
	GtkWidget *vbox, *sw, *btn, *lbl, *entry;
	GtkTreeView *left_tree;
	GtkTreeStore *left_store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	TestVirtualTreeModel *model;
	EVirtualTree *vtree;
	TestContext *ctx;
	guint col;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "test-virtual-tree");
	gtk_window_set_default_size (GTK_WINDOW (window), 900, 600);
	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	vpaned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_paned_set_position (GTK_PANED (vpaned), 400);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 2);

	toolbar2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start (GTK_BOX (vbox), toolbar2, FALSE, FALSE, 2);

	hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_paned_set_position (GTK_PANED (hpaned), 400);
	gtk_box_pack_start (GTK_BOX (vbox), hpaned, TRUE, TRUE, 0);

	gtk_paned_pack1 (GTK_PANED (vpaned), vbox, TRUE, TRUE);

	left_store = gtk_tree_store_new (1, G_TYPE_STRING);
	left_tree = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (left_store)));

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Label (GtkTreeView)", renderer, "text", 0, NULL);
	gtk_tree_view_append_column (left_tree, column);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (left_tree));
	gtk_paned_pack1 (GTK_PANED (hpaned), sw, TRUE, TRUE);

	model = g_object_new (TEST_TYPE_VIRTUAL_TREE_MODEL, NULL);
	vtree = E_VIRTUAL_TREE (e_virtual_tree_new (E_VIRTUAL_TREE_MODEL (model)));

	col = e_virtual_tree_add_column (vtree, "label", "Label (EVirtualTree)");
	e_virtual_tree_set_column_expand (vtree, col, TRUE);
	e_virtual_tree_set_search_func (vtree, search_func, NULL, NULL);

	col = e_virtual_tree_add_column (vtree, "children", "Children");
	e_virtual_tree_set_column_min_width (vtree, col, 80);
	renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (vtree, col, 0, renderer, FALSE, depth_data_func, NULL, NULL);

	/* Actions column: clickable link + button, visible only on A1 */
	col = e_virtual_tree_add_column (vtree, "actions", "Actions");
	e_virtual_tree_set_column_min_width (vtree, col, 120);

	link_renderer = gtk_cell_renderer_text_new ();
	g_object_set (link_renderer, "xpad", 4, NULL);
	e_virtual_tree_column_pack_start (vtree, col, 0, link_renderer, FALSE, link_data_func, NULL, NULL);

	button_renderer = e_cell_renderer_button_new ();
	g_object_set (button_renderer, "text", "Mark Read", NULL);
	e_virtual_tree_column_pack_start (vtree, col, 0, button_renderer, FALSE, button_data_func, NULL, NULL);

	color_renderer = e_cell_renderer_button_new ();
	e_virtual_tree_column_pack_start (vtree, col, 0, color_renderer, FALSE, color_data_func, NULL, NULL);

	e_virtual_tree_set_unselected_row_color_func (vtree, test_row_background_func, NULL, NULL);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (vtree));
	gtk_paned_pack2 (GTK_PANED (hpaned), sw, TRUE, TRUE);

	log_buffer = gtk_text_buffer_new (NULL);
	log_view = gtk_text_view_new_with_buffer (log_buffer);
	log_text_view = GTK_TEXT_VIEW (log_view);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (log_view), FALSE);
	gtk_text_view_set_monospace (GTK_TEXT_VIEW (log_view), TRUE);

	scrolled_log = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_log), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scrolled_log), log_view);
	gtk_paned_pack2 (GTK_PANED (vpaned), scrolled_log, FALSE, TRUE);

	ctx = g_new0 (TestContext, 1);
	ctx->model = model;
	ctx->vtree = vtree;
	ctx->left_tree = left_tree;
	ctx->label_column = 0;
	ctx->two_lines = TRUE;
	ctx->next_id = 100;
	g_object_set_data_full (G_OBJECT (window), "test-context", ctx, g_free);

	rebuild_label_column (ctx);
	build_test_data (model, left_store);
	gtk_tree_view_expand_all (left_tree);

	btn = gtk_button_new_with_label ("Add Child");
	g_signal_connect (btn, "clicked", G_CALLBACK (on_add_child_clicked), ctx);
	gtk_box_pack_start (GTK_BOX (toolbar), btn, FALSE, FALSE, 0);

	btn = gtk_button_new_with_label ("Add 1000");
	g_signal_connect (btn, "clicked", G_CALLBACK (on_add_1000_clicked), ctx);
	gtk_box_pack_start (GTK_BOX (toolbar), btn, FALSE, FALSE, 0);

	btn = gtk_button_new_with_label ("Remove Selected");
	g_signal_connect (btn, "clicked", G_CALLBACK (on_remove_selected_clicked), ctx);
	gtk_box_pack_start (GTK_BOX (toolbar), btn, FALSE, FALSE, 0);

	btn = gtk_button_new_with_label ("Expand All");
	g_signal_connect (btn, "clicked", G_CALLBACK (on_expand_all_clicked), ctx);
	gtk_box_pack_start (GTK_BOX (toolbar), btn, FALSE, FALSE, 0);

	btn = gtk_button_new_with_label ("Collapse All");
	g_signal_connect (btn, "clicked", G_CALLBACK (on_collapse_all_clicked), ctx);
	gtk_box_pack_start (GTK_BOX (toolbar), btn, FALSE, FALSE, 0);

	btn = gtk_button_new_with_label ("Pretend Rebuild");
	g_signal_connect (btn, "clicked", G_CALLBACK (on_rebuild_clicked), ctx);
	gtk_box_pack_start (GTK_BOX (toolbar), btn, FALSE, FALSE, 0);

	btn = gtk_button_new_with_label ("Toggle Groups");
	g_signal_connect (btn, "clicked", G_CALLBACK (on_toggle_groups_clicked), ctx);
	gtk_box_pack_start (GTK_BOX (toolbar2), btn, FALSE, FALSE, 0);

	btn = gtk_button_new_with_label ("Toggle Editable");
	g_signal_connect (btn, "clicked", G_CALLBACK (on_toggle_editable_clicked), ctx);
	gtk_box_pack_start (GTK_BOX (toolbar2), btn, FALSE, FALSE, 0);

	btn = gtk_button_new_with_label ("Toggle Two Lines");
	g_signal_connect (btn, "clicked", G_CALLBACK (on_toggle_two_lines_clicked), ctx);
	gtk_box_pack_start (GTK_BOX (toolbar2), btn, FALSE, FALSE, 0);

	btn = gtk_button_new_with_label ("Sel: Single");
	g_signal_connect (btn, "clicked", G_CALLBACK (on_selection_mode_clicked), ctx);
	gtk_box_pack_start (GTK_BOX (toolbar2), btn, FALSE, FALSE, 0);

	btn = gtk_button_new_with_label ("Stats");
	g_signal_connect (btn, "clicked", G_CALLBACK (on_show_stats_clicked), ctx);
	gtk_box_pack_start (GTK_BOX (toolbar2), btn, FALSE, FALSE, 0);

	lbl = gtk_label_new ("Empty msg:");
	gtk_box_pack_start (GTK_BOX (toolbar2), lbl, FALSE, FALSE, 0);

	entry = gtk_entry_new ();
	gtk_entry_set_placeholder_text (GTK_ENTRY (entry), "Set empty message...");
	g_signal_connect (entry, "changed",
		G_CALLBACK (on_empty_message_changed), ctx);
	gtk_box_pack_start (GTK_BOX (toolbar2), entry, TRUE, TRUE, 0);

	g_signal_connect (model, "rows-changed",
		G_CALLBACK (on_model_rows_changed_log), NULL);
	g_signal_connect (model, "rows-inserted",
		G_CALLBACK (on_model_rows_inserted_log), NULL);
	g_signal_connect (model, "rows-removed",
		G_CALLBACK (on_model_rows_removed_log), NULL);
	g_signal_connect (model, "row-count-changed",
		G_CALLBACK (on_model_row_count_changed_log), NULL);

	g_signal_connect (vtree, "selection-changed",
		G_CALLBACK (on_vtree_selection_changed_log), NULL);
	g_signal_connect (vtree, "cursor-changed",
		G_CALLBACK (on_vtree_cursor_changed_log), NULL);
	g_signal_connect (vtree, "row-expanded",
		G_CALLBACK (on_vtree_row_expanded_log), NULL);
	g_signal_connect (vtree, "row-activated",
		G_CALLBACK (on_vtree_row_activated_log), NULL);
	g_signal_connect (vtree, "cell-clicked",
		G_CALLBACK (on_vtree_cell_clicked_log), NULL);
	g_signal_connect (vtree, "right-click",
		G_CALLBACK (on_vtree_right_click_log), NULL);
	g_signal_connect (vtree, "cell-edited",
		G_CALLBACK (on_vtree_cell_edited_log), NULL);
	g_signal_connect (vtree, "column-state-changed",
		G_CALLBACK (on_vtree_column_state_changed_log), ctx);

	gtk_container_add (GTK_CONTAINER (window), vpaned);
	gtk_widget_show_all (window);

	log_message ("Test started: %u rows visible",
		e_virtual_tree_model_get_row_count (E_VIRTUAL_TREE_MODEL (model)));

	return G_SOURCE_REMOVE;
}

gint
main (gint argc,
      gchar *argv[])
{
	gtk_init (&argc, &argv);

	g_idle_add (on_idle_create_widget, NULL);

	gtk_main ();

	e_misc_util_free_global_memory ();

	return 0;
}
