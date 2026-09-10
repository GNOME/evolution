/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>

#include <atk/atk.h>
#include <e-util/e-util.h>

guint		_e_virtual_tree_get_first_visible_row	(EVirtualTree *self);
guint		_e_virtual_tree_get_visible_count	(EVirtualTree *self);
gint		_e_virtual_tree_get_row_stride		(EVirtualTree *self);
GtkAdjustment *	_e_virtual_tree_get_vadjustment		(EVirtualTree *self);
gchar *		_e_virtual_tree_get_cell_text		(EVirtualTree *self,
							 guint row_index,
							 guint column_index);

static guint event_processing_delay_ms = 25;
static gboolean in_background = FALSE;

#define TEST_TYPE_NODE (test_node_get_type ())
G_DECLARE_FINAL_TYPE (TestNode, test_node, TEST, NODE, GObject)

struct _TestNode {
	GObject parent_instance;
	gchar *label;
	GNode *gnode;
};

G_DEFINE_FINAL_TYPE (TestNode, test_node, G_TYPE_OBJECT)

static void
test_node_finalize (GObject *object)
{
	TestNode *self = TEST_NODE (object);

	g_free (self->label);

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
	GHashTable *expanded;
};

static void test_virtual_tree_model_iface_init (EVirtualTreeModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (TestVirtualTreeModel, test_virtual_tree_model, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (E_TYPE_VIRTUAL_TREE_MODEL, test_virtual_tree_model_iface_init))

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
		TestNode *tn = node->data;

		if (*counter >= target_start && *counter <= target_end && result)
			g_ptr_array_add (result, g_object_ref (tn));

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

	result = g_ptr_array_new_with_free_func (g_object_unref);
	walk_visible (self->root, self->expanded, &counter, (gint) first_row, (gint) last_row, include_collapsed, result);

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
			e_virtual_tree_model_emit_rows_inserted (
				model, row_index + 1,
				row_index + n_desc);
	} else {
		n_desc = count_visible_descendants (node->gnode, self->expanded);
		g_hash_table_remove (self->expanded, row_object);

		if (n_desc > 0)
			e_virtual_tree_model_emit_rows_removed (
				model, row_index + 1,
				row_index + n_desc);
	}

	e_virtual_tree_model_emit_rows_changed (model, row_index, row_index);
}

static gconstpointer
test_model_get_row_key (EVirtualTreeModel *model,
			GObject *row_object)
{
	TestNode *node = TEST_NODE (row_object);

	return node->label;
}

static EVirtualTreeKeyType
test_model_get_key_type (EVirtualTreeModel *model)
{
	EVirtualTreeKeyType kt = {
		g_str_hash,
		g_str_equal,
		(GBoxedCopyFunc) g_strdup,
		g_free
	};
	return kt;
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
	iface->get_row_key = test_model_get_row_key;
	iface->get_key_type = test_model_get_key_type;
}

static gboolean
unref_node_data (GNode *node,
		 gpointer data)
{
	if (node->data)
		g_object_unref (node->data);
	return FALSE;
}

static void
test_virtual_tree_model_finalize (GObject *object)
{
	TestVirtualTreeModel *self = TEST_VIRTUAL_TREE_MODEL (object);

	g_hash_table_unref (self->expanded);

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

static GNode *
insert_test_node_before (TestVirtualTreeModel *model,
			 GNode *parent,
			 GNode *sibling,
			 const gchar *label)
{
	TestNode *node = test_node_new (label);
	GNode *gnode = g_node_insert_before (parent, sibling, g_node_new (node));

	node->gnode = gnode;

	return gnode;
}

/* Build the standard test tree:
 * 0: A
 * 1:   A1
 * 2:     A1a
 * 3:     A1b
 * 4:   A2
 * 5: B
 * 6:   B1
 * 7:     B1a
 * 8: C
 */
static void
build_test_data (TestVirtualTreeModel *model)
{
	GNode *a, *a1, *b, *b1;

	a = add_test_node (model, model->root, "A");
	a1 = add_test_node (model, a, "A1");
	add_test_node (model, a1, "A1a");
	add_test_node (model, a1, "A1b");
	add_test_node (model, a, "A2");

	b = add_test_node (model, model->root, "B");
	b1 = add_test_node (model, b, "B1");
	add_test_node (model, b1, "B1a");

	add_test_node (model, model->root, "C");

	g_hash_table_add (model->expanded, a->data);
	g_hash_table_add (model->expanded, a1->data);
	g_hash_table_add (model->expanded, b->data);
	g_hash_table_add (model->expanded, b1->data);
}

static void
build_large_test_data (TestVirtualTreeModel *model)
{
	guint ii;

	for (ii = 1; ii <= 50; ii++) {
		gchar *label = g_strdup_printf ("L%02u", ii);
		add_test_node (model, model->root, label);
		g_free (label);
	}
}

static void
serialize_tree_node (GNode *node,
		     GString *out)
{
	GNode *child;
	gboolean first = TRUE;

	for (child = node->children; child; child = child->next) {
		TestNode *tn = child->data;

		if (!first)
			g_string_append_c (out, ',');
		first = FALSE;

		g_string_append (out, tn->label);

		if (child->children) {
			g_string_append_c (out, '(');
			serialize_tree_node (child, out);
			g_string_append_c (out, ')');
		}
	}
}

static gchar *
get_tree_string (TestVirtualTreeModel *model)
{
	GString *out = g_string_new ("");

	serialize_tree_node (model->root, out);

	return g_string_free (out, FALSE);
}

static GNode *
find_node_by_label (GNode *root,
		    const gchar *label)
{
	GNode *child;

	for (child = root->children; child; child = child->next) {
		TestNode *tn = child->data;
		GNode *found;

		if (g_str_equal (tn->label, label))
			return child;

		found = find_node_by_label (child, label);
		if (found)
			return found;
	}

	return NULL;
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

typedef struct _TestFixture {
	GtkWidget *window;
	GtkWidget *scrolled_window;
	EVirtualTree *vtree;
	TestVirtualTreeModel *model;
	guint key_state;
	GString *signal_log;
} TestFixture;

static void
on_selection_changed (EVirtualTree *tree,
		      gpointer user_data)
{
	TestFixture *fixture = user_data;

	g_string_append (fixture->signal_log, "selection-changed|");
}

static void
on_cursor_changed (EVirtualTree *tree,
		   guint row,
		   GObject *row_object,
		   gpointer user_data)
{
	TestFixture *fixture = user_data;
	TestNode *node = TEST_NODE (row_object);

	g_string_append_printf (fixture->signal_log,
		"cursor-changed:%u:%s|", row, node->label);
}

static void
on_row_expanded (EVirtualTree *tree,
		 guint row,
		 GObject *row_object,
		 gboolean expanded,
		 gpointer user_data)
{
	TestFixture *fixture = user_data;
	TestNode *node = TEST_NODE (row_object);

	g_string_append_printf (fixture->signal_log,
		"row-expanded:%u:%s:%s|", row, node->label,
		expanded ? "TRUE" : "FALSE");
}

static void
on_row_activated (EVirtualTree *tree,
		  guint row,
		  GObject *row_object,
		  gpointer user_data)
{
	TestFixture *fixture = user_data;
	TestNode *node = TEST_NODE (row_object);

	g_string_append_printf (fixture->signal_log,
		"row-activated:%u:%s|", row, node->label);
}

static gboolean
on_cell_clicked (EVirtualTree *tree,
		 guint row,
		 GObject *row_object,
		 guint column,
		 GtkCellRenderer *renderer,
		 gpointer user_data)
{
	TestFixture *fixture = user_data;
	TestNode *node = TEST_NODE (row_object);

	g_string_append_printf (fixture->signal_log,
		"cell-clicked:%u:%s:%u|", row, node->label, column);

	return FALSE;
}

static gboolean
wait_timeout_cb (gpointer user_data)
{
	GMainLoop *loop = user_data;

	g_main_loop_quit (loop);

	return FALSE;
}

static void
wait_milliseconds (guint milliseconds)
{
	GMainLoop *loop;

	loop = g_main_loop_new (NULL, FALSE);
	g_timeout_add (milliseconds, wait_timeout_cb, loop);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);
}

static void
flush_main_context (void)
{
	GMainContext *ctx = g_main_context_default ();
	gint64 deadline = g_get_monotonic_time () + 5 * G_USEC_PER_SEC;

	while (g_main_context_iteration (ctx, FALSE) && g_get_monotonic_time () < deadline) {
		/* keep dispatching until nothing pending or timeout */
	}
}

static void
send_key_event (GtkWidget *widget,
		GdkEventType type,
		guint keyval,
		guint state)
{
	GdkKeymap *keymap;
	GdkKeymapKey *keys = NULL;
	gint n_keys;
	GdkEvent *event;

	event = gdk_event_new (type);
	event->key.is_modifier =
		keyval == GDK_KEY_Shift_L ||
		keyval == GDK_KEY_Shift_R ||
		keyval == GDK_KEY_Control_L ||
		keyval == GDK_KEY_Control_R ||
		keyval == GDK_KEY_Alt_L ||
		keyval == GDK_KEY_Alt_R;
	event->key.keyval = keyval;
	event->key.state = state;
	event->key.window = g_object_ref (gtk_widget_get_window (widget));
	event->key.send_event = TRUE;
	event->key.length = 0;
	event->key.string = NULL;
	event->key.hardware_keycode = 0;
	event->key.group = 0;
	event->key.time = GDK_CURRENT_TIME;

	gdk_event_set_device (event,
		gdk_seat_get_keyboard (
			gdk_display_get_default_seat (
				gtk_widget_get_display (widget))));

	keymap = gdk_keymap_get_for_display (gtk_widget_get_display (widget));
	if (gdk_keymap_get_entries_for_keyval (keymap, keyval, &keys, &n_keys)) {
		if (n_keys > 0) {
			event->key.hardware_keycode = keys[0].keycode;
			event->key.group = keys[0].group;
		}

		g_free (keys);
	}

	gtk_main_do_event (event);

	wait_milliseconds (event_processing_delay_ms);

	gdk_event_free (event);
}

static void
send_button_event (GtkWidget *widget,
		   GdkEventType type,
		   gint x,
		   gint y,
		   guint button,
		   guint state)
{
	GdkEvent *event;

	event = gdk_event_new (type);
	event->button.window = g_object_ref (gtk_widget_get_window (widget));
	event->button.send_event = TRUE;
	event->button.time = GDK_CURRENT_TIME;
	event->button.x = x;
	event->button.y = y;
	event->button.button = button;
	event->button.state = state;

	gdk_event_set_device (event,
		gdk_seat_get_pointer (
			gdk_display_get_default_seat (
				gtk_widget_get_display (widget))));

	gtk_main_do_event (event);

	gdk_event_free (event);
}

static void
send_click (GtkWidget *widget,
	    gint x,
	    gint y,
	    guint state)
{
	send_button_event (widget, GDK_BUTTON_PRESS, x, y, 1, state);
	wait_milliseconds (event_processing_delay_ms);
	send_button_event (widget, GDK_BUTTON_RELEASE, x, y, 1,
		state | GDK_BUTTON1_MASK);
	wait_milliseconds (event_processing_delay_ms);
}

static gboolean
process_sequence (TestFixture *fixture,
		  const gchar *sequence)
{
	GtkWidget *widget;
	const gchar *seq;
	guint keyval;
	gboolean success = TRUE;

	widget = GTK_WIDGET (e_virtual_tree_get_tree_view (fixture->vtree));

	for (seq = sequence; *seq && success; seq++) {
		gboolean call_press = TRUE, call_release = TRUE;
		guint change_state = fixture->key_state;

		switch (*seq) {
		case 'S':
			keyval = GDK_KEY_Shift_L;
			if ((fixture->key_state & GDK_SHIFT_MASK) != 0) {
				success = FALSE;
				g_warning ("%s: Shift is already pressed", G_STRFUNC);
			} else {
				change_state |= GDK_SHIFT_MASK;
			}
			call_release = FALSE;
			break;
		case 's':
			keyval = GDK_KEY_Shift_L;
			if ((fixture->key_state & GDK_SHIFT_MASK) == 0) {
				success = FALSE;
				g_warning ("%s: Shift is already released", G_STRFUNC);
			} else {
				change_state &= ~GDK_SHIFT_MASK;
			}
			call_press = FALSE;
			break;
		case 'C':
			keyval = GDK_KEY_Control_L;
			if ((fixture->key_state & GDK_CONTROL_MASK) != 0) {
				success = FALSE;
				g_warning ("%s: Control is already pressed", G_STRFUNC);
			} else {
				change_state |= GDK_CONTROL_MASK;
			}
			call_release = FALSE;
			break;
		case 'c':
			keyval = GDK_KEY_Control_L;
			if ((fixture->key_state & GDK_CONTROL_MASK) == 0) {
				success = FALSE;
				g_warning ("%s: Control is already released", G_STRFUNC);
			} else {
				change_state &= ~GDK_CONTROL_MASK;
			}
			call_press = FALSE;
			break;
		case 'h':
			keyval = GDK_KEY_Home;
			break;
		case 'e':
			keyval = GDK_KEY_End;
			break;
		case 'P':
			keyval = GDK_KEY_Page_Up;
			break;
		case 'p':
			keyval = GDK_KEY_Page_Down;
			break;
		case 'u':
			keyval = GDK_KEY_Up;
			break;
		case 'd':
			keyval = GDK_KEY_Down;
			break;
		case 'n':
			keyval = GDK_KEY_Return;
			break;
		case ' ':
			keyval = GDK_KEY_space;
			break;
		default:
			success = FALSE;
			g_warning ("%s: Unknown sequence command '%c' in '%s'",
				G_STRFUNC, *seq, sequence);
			break;
		}

		if (success) {
			if (call_press)
				send_key_event (widget, GDK_KEY_PRESS,
					keyval, fixture->key_state);
			if (call_release)
				send_key_event (widget, GDK_KEY_RELEASE,
					keyval, fixture->key_state);
		}

		fixture->key_state = change_state;
	}

	wait_milliseconds (event_processing_delay_ms);

	return success;
}

static gint
compare_guint (gconstpointer a,
	       gconstpointer b)
{
	guint ua = *(const guint *) a;
	guint ub = *(const guint *) b;

	if (ua < ub)
		return -1;
	if (ua > ub)
		return 1;
	return 0;
}

static gboolean
process_command (TestFixture *fixture,
		 const gchar *command)
{
	if (g_str_has_prefix (command, "seq:")) {
		return process_sequence (fixture, command + 4);

	} else if (g_str_has_prefix (command, "wait:")) {
		guint ms = (guint) atoi (command + 5);
		wait_milliseconds (ms);
		return TRUE;

	} else if (g_str_has_prefix (command, "cursor:")) {
		gint row = atoi (command + 7);
		e_virtual_tree_set_cursor (fixture->vtree, row);
		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (g_str_has_prefix (command, "select:")) {
		guint row = (guint) atoi (command + 7);
		e_virtual_tree_select_row (fixture->vtree, row);
		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (g_str_has_prefix (command, "unselect:")) {
		guint row = (guint) atoi (command + 9);
		e_virtual_tree_unselect_row (fixture->vtree, row);
		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (g_str_equal (command, "select-all")) {
		e_virtual_tree_select_all (fixture->vtree);
		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (g_str_equal (command, "unselect-all")) {
		e_virtual_tree_unselect_all (fixture->vtree);
		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (g_str_has_prefix (command, "selection-mode:")) {
		const gchar *mode = command + 15;
		if (g_str_equal (mode, "single"))
			e_virtual_tree_set_selection_mode (fixture->vtree, GTK_SELECTION_SINGLE);
		else if (g_str_equal (mode, "multiple"))
			e_virtual_tree_set_selection_mode (fixture->vtree, GTK_SELECTION_MULTIPLE);
		else if (g_str_equal (mode, "browse"))
			e_virtual_tree_set_selection_mode (fixture->vtree, GTK_SELECTION_BROWSE);
		else if (g_str_equal (mode, "none"))
			e_virtual_tree_set_selection_mode (fixture->vtree, GTK_SELECTION_NONE);
		else {
			g_warning ("%s: Unknown selection mode '%s'", G_STRFUNC, mode);
			return FALSE;
		}
		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (g_str_has_prefix (command, "click-row:")) {
		const gchar *args = command + 10;
		gchar *endptr;
		guint row;
		guint state = 0;
		GtkTreeView *tv;
		GtkTreePath *path;
		GdkRectangle rect;
		GtkAllocation alloc;
		guint first_visible;
		guint visible_count;
		gint tv_offset;
		gint attempt;

		row = (guint) strtol (args, &endptr, 10);
		if (*endptr == ':') {
			const gchar *mods = endptr + 1;
			if (strstr (mods, "ctrl"))
				state |= GDK_CONTROL_MASK;
			if (strstr (mods, "shift"))
				state |= GDK_SHIFT_MASK;
		}

		tv = e_virtual_tree_get_tree_view (fixture->vtree);
		first_visible = _e_virtual_tree_get_first_visible_row (fixture->vtree);
		visible_count = _e_virtual_tree_get_visible_count (fixture->vtree);

		/* The backing GtkListStore only ever holds the currently
		 * loaded window of rows. How many rows actually fit in that
		 * window depends on the row height, which varies with fonts
		 * and theme across machines - so a row assumed to already be
		 * on screen may not be. Scroll it into view first rather than
		 * assuming the caller's layout. */
		if (row < first_visible || row >= first_visible + visible_count) {
			e_virtual_tree_scroll_to_row (fixture->vtree, row);
			flush_main_context ();
			wait_milliseconds (50);
			flush_main_context ();
			first_visible = _e_virtual_tree_get_first_visible_row (fixture->vtree);
		}

		if (row < first_visible) {
			g_warning ("click-row: row %u not visible (first_visible=%u)",
				row, first_visible);
			return FALSE;
		}

		tv_offset = (gint) (row - first_visible);
		path = gtk_tree_path_new_from_indices (tv_offset, -1);

		/* GtkTreeView validates row heights lazily off idle/frame-clock
		 * sources; a single main-context flush is not always enough
		 * under a slow or headless X server, so retry briefly. */
		rect.height = 0;
		for (attempt = 0; attempt < 40 && rect.height <= 0; attempt++) {
			gtk_tree_view_get_background_area (tv, path, NULL, &rect);
			if (rect.height <= 0) {
				flush_main_context ();
				wait_milliseconds (25);
			}
		}

		gtk_tree_path_free (path);

		if (rect.height <= 0) {
			g_warning ("click-row: row %u has zero-height cell rect", row);
			return FALSE;
		}

		gtk_widget_get_allocation (GTK_WIDGET (tv), &alloc);

		send_click (GTK_WIDGET (tv),
			alloc.width / 2,
			rect.y + rect.height / 2,
			state);
		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (g_str_has_prefix (command, "scroll:")) {
		guint row = (guint) atoi (command + 7);
		e_virtual_tree_scroll_to_row (fixture->vtree, row);
		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (g_str_has_prefix (command, "scroll-pixels:")) {
		GtkAdjustment *adj;
		gdouble pixels = g_ascii_strtod (command + 14, NULL);

		adj = _e_virtual_tree_get_vadjustment (fixture->vtree);
		if (adj)
			gtk_adjustment_set_value (adj, pixels);
		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (g_str_has_prefix (command, "scroll-half-row:")) {
		GtkAdjustment *adj;
		guint row = (guint) atoi (command + 16);
		gint stride = _e_virtual_tree_get_row_stride (fixture->vtree);

		adj = _e_virtual_tree_get_vadjustment (fixture->vtree);
		if (adj && stride > 0)
			gtk_adjustment_set_value (adj,
				(gdouble) row * stride + stride / 2);
		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (g_str_has_prefix (command, "expand:")) {
		const gchar *label = command + 7;
		GNode *gnode;

		gnode = find_node_by_label (fixture->model->root, label);
		if (!gnode) {
			g_warning ("expand: '%s' not found", label);
			return FALSE;
		}

		e_virtual_tree_model_set_expanded (
			E_VIRTUAL_TREE_MODEL (fixture->model),
			G_OBJECT (gnode->data), TRUE);
		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (g_str_has_prefix (command, "collapse:")) {
		const gchar *label = command + 9;
		GNode *gnode;

		gnode = find_node_by_label (fixture->model->root, label);
		if (!gnode) {
			g_warning ("collapse: '%s' not found", label);
			return FALSE;
		}

		e_virtual_tree_model_set_expanded (
			E_VIRTUAL_TREE_MODEL (fixture->model),
			G_OBJECT (gnode->data), FALSE);
		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (g_str_has_prefix (command, "add-node:")) {
		const gchar *args = command + 9;
		gchar **parts = g_strsplit (args, ":", 2);
		const gchar *label = parts[0];
		GNode *parent_gnode;
		GNode *new_gnode;
		gboolean parent_visible, parent_expanded;
		gint parent_index;

		if (parts[1] && *parts[1]) {
			parent_gnode = find_node_by_label (
				fixture->model->root, parts[1]);
			if (!parent_gnode) {
				g_warning ("add-node: parent '%s' not found",
					parts[1]);
				g_strfreev (parts);
				return FALSE;
			}
		} else {
			parent_gnode = fixture->model->root;
		}

		new_gnode = add_test_node (fixture->model, parent_gnode, label);

		parent_visible = G_NODE_IS_ROOT (parent_gnode) ||
			find_visible_index (fixture->model->root, parent_gnode, fixture->model->expanded) >= 0;

		parent_expanded = G_NODE_IS_ROOT (parent_gnode) ||
			g_hash_table_contains (fixture->model->expanded, parent_gnode->data);

		if (parent_visible && parent_expanded) {
			gint new_index;

			new_index = find_visible_index (fixture->model->root, new_gnode, fixture->model->expanded);

			if (new_index >= 0)
				e_virtual_tree_model_emit_rows_inserted (E_VIRTUAL_TREE_MODEL (fixture->model), new_index, new_index);
		}

		if (parent_visible && !G_NODE_IS_ROOT (parent_gnode)) {
			parent_index = find_visible_index (fixture->model->root, parent_gnode, fixture->model->expanded);
			if (parent_index >= 0)
				e_virtual_tree_model_emit_rows_changed (E_VIRTUAL_TREE_MODEL (fixture->model), parent_index, parent_index);
		}

		g_strfreev (parts);
		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (g_str_has_prefix (command, "insert-before:")) {
		const gchar *args = command + 14;
		gchar **parts = g_strsplit (args, ":", 2);
		const gchar *label = parts[0];
		const gchar *sibling_label = parts[1];
		GNode *sibling_gnode, *new_gnode;
		gint new_index;

		if (!sibling_label || !*sibling_label) {
			g_warning ("insert-before: missing sibling label");
			g_strfreev (parts);
			return FALSE;
		}

		sibling_gnode = find_node_by_label (fixture->model->root, sibling_label);
		if (!sibling_gnode) {
			g_warning ("insert-before: sibling '%s' not found", sibling_label);
			g_strfreev (parts);
			return FALSE;
		}

		new_gnode = insert_test_node_before (fixture->model,
			sibling_gnode->parent, sibling_gnode, label);

		new_index = find_visible_index (fixture->model->root, new_gnode, fixture->model->expanded);
		if (new_index >= 0)
			e_virtual_tree_model_emit_rows_inserted (E_VIRTUAL_TREE_MODEL (fixture->model), new_index, new_index);

		g_strfreev (parts);
		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (g_str_has_prefix (command, "remove-node:")) {
		const gchar *label = command + 12;
		GNode *gnode, *parent_gnode;
		gboolean was_expanded;
		guint n_desc = 0;
		gint row_index;

		gnode = find_node_by_label (fixture->model->root, label);
		if (!gnode) {
			g_warning ("remove-node: '%s' not found", label);
			return FALSE;
		}

		parent_gnode = gnode->parent;

		was_expanded = g_hash_table_contains (fixture->model->expanded, gnode->data);

		if (was_expanded)
			n_desc = count_visible_descendants (gnode, fixture->model->expanded);

		row_index = find_visible_index (fixture->model->root, gnode, fixture->model->expanded);

		g_hash_table_remove (fixture->model->expanded, gnode->data);

		g_node_unlink (gnode);

		if (row_index >= 0) {
			gint last = row_index + (gint) n_desc;
			e_virtual_tree_model_emit_rows_removed (E_VIRTUAL_TREE_MODEL (fixture->model), row_index, last);
		}

		if (!G_NODE_IS_ROOT (parent_gnode)) {
			gint parent_index;

			parent_index = find_visible_index (fixture->model->root, parent_gnode, fixture->model->expanded);
			if (parent_index >= 0)
				e_virtual_tree_model_emit_rows_changed (E_VIRTUAL_TREE_MODEL (fixture->model), parent_index, parent_index);
		}

		g_node_traverse (gnode, G_PRE_ORDER, G_TRAVERSE_ALL, -1, unref_node_data, NULL);
		g_node_destroy (gnode);

		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (g_str_has_prefix (command, "rebuild:")) {
		const gchar *spec = command + 8;
		gchar **labels;
		guint ii;

		e_virtual_tree_model_emit_before_rebuild (E_VIRTUAL_TREE_MODEL (fixture->model));

		g_hash_table_remove_all (fixture->model->expanded);
		g_node_traverse (fixture->model->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, unref_node_data, NULL);
		g_node_destroy (fixture->model->root);
		fixture->model->root = g_node_new (NULL);

		labels = g_strsplit (spec, ",", -1);
		for (ii = 0; labels[ii] && *labels[ii]; ii++) {
			add_test_node (fixture->model, fixture->model->root, labels[ii]);
		}
		g_strfreev (labels);

		e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (fixture->model));
		e_virtual_tree_model_emit_after_rebuild (E_VIRTUAL_TREE_MODEL (fixture->model));

		flush_main_context ();
		return TRUE;

	} else if (g_str_equal (command, "clear-log")) {
		g_string_truncate (fixture->signal_log, 0);
		return TRUE;

	} else if (g_str_has_prefix (command, "assert-cursor:")) {
		gint expected = atoi (command + 14);
		gint actual = e_virtual_tree_get_cursor (fixture->vtree);
		if (actual != expected) {
			g_warning ("assert-cursor: expected %d, got %d", expected, actual);
			return FALSE;
		}
		return TRUE;

	} else if (g_str_has_prefix (command, "assert-selected:")) {
		const gchar *spec = command + 16;
		GPtrArray *selected;
		GArray *indices;
		GString *actual;
		gboolean result;
		guint ii;

		selected = e_virtual_tree_get_selected_rows (fixture->vtree);
		indices = g_array_new (FALSE, FALSE, sizeof (guint));

		for (ii = 0; ii < selected->len; ii++) {
			GObject *row_obj = g_ptr_array_index (selected, ii);
			gconstpointer key = e_virtual_tree_model_get_row_key (E_VIRTUAL_TREE_MODEL (fixture->model), row_obj);
			guint idx = e_virtual_tree_model_find_row_by_key (E_VIRTUAL_TREE_MODEL (fixture->model), key);
			if (idx != G_MAXUINT)
				g_array_append_val (indices, idx);
		}

		g_array_sort (indices, (GCompareFunc) compare_guint);

		actual = g_string_new ("");
		for (ii = 0; ii < indices->len; ii++) {
			if (actual->len > 0)
				g_string_append_c (actual, ',');
			g_string_append_printf (actual, "%u", g_array_index (indices, guint, ii));
		}

		result = g_str_equal (actual->str, spec);
		if (!result)
			g_warning ("assert-selected: expected '%s', got '%s'",
				spec, actual->str);

		g_string_free (actual, TRUE);
		g_array_free (indices, TRUE);
		g_ptr_array_unref (selected);
		return result;

	} else if (g_str_has_prefix (command, "assert-selected-count:")) {
		guint expected = (guint) atoi (command + 22);
		GPtrArray *selected;
		guint actual;
		gboolean result;

		selected = e_virtual_tree_get_selected_rows (fixture->vtree);
		actual = selected->len;
		g_ptr_array_unref (selected);

		result = (actual == expected);
		if (!result)
			g_warning ("assert-selected-count: expected %u, got %u",
				expected, actual);
		return result;

	} else if (g_str_has_prefix (command, "assert-n-rows:")) {
		guint expected = (guint) atoi (command + 14);
		guint actual = e_virtual_tree_model_get_row_count (
			E_VIRTUAL_TREE_MODEL (fixture->model));
		if (actual != expected) {
			g_warning ("assert-n-rows: expected %u, got %u",
				expected, actual);
			return FALSE;
		}
		return TRUE;

	} else if (g_str_has_prefix (command, "assert-signals:")) {
		const gchar *expected = command + 15;
		if (!g_str_equal (fixture->signal_log->str, expected)) {
			g_warning ("assert-signals:\n  expected: '%s'\n  actual:   '%s'",
				expected, fixture->signal_log->str);
			return FALSE;
		}
		return TRUE;

	} else if (g_str_has_prefix (command, "assert-tree:")) {
		const gchar *expected = command + 12;
		gchar *actual = get_tree_string (fixture->model);
		if (!g_str_equal (actual, expected)) {
			g_warning ("assert-tree:\n  expected: '%s'\n  actual:   '%s'",
				expected, actual);
			g_free (actual);
			return FALSE;
		}
		g_free (actual);
		return TRUE;

	} else if (g_str_has_prefix (command, "assert-first-visible:")) {
		guint expected = (guint) atoi (command + 21);
		guint actual = _e_virtual_tree_get_first_visible_row (fixture->vtree);
		if (actual != expected) {
			g_warning ("assert-first-visible: expected %u, got %u",
				expected, actual);
			return FALSE;
		}
		return TRUE;

	} else if (g_str_has_prefix (command, "assert-row-fully-visible:")) {
		GtkAdjustment *adj;
		const gchar *arg = command + 25;
		guint row;
		gint stride = _e_virtual_tree_get_row_stride (fixture->vtree);
		gdouble scroll_pos, page_size;
		gdouble row_top, row_bottom;

		if (g_str_equal (arg, "cursor")) {
			gint cur = e_virtual_tree_get_cursor (fixture->vtree);
			if (cur < 0) {
				g_warning ("assert-row-fully-visible: no cursor set");
				return FALSE;
			}
			row = (guint) cur;
		} else {
			row = (guint) atoi (arg);
		}

		adj = _e_virtual_tree_get_vadjustment (fixture->vtree);
		if (!adj || stride <= 0) {
			g_warning ("assert-row-fully-visible: no adjustment or zero stride");
			return FALSE;
		}

		scroll_pos = gtk_adjustment_get_value (adj);
		page_size = gtk_adjustment_get_page_size (adj);
		row_top = (gdouble) row * stride;
		row_bottom = row_top + stride;

		if (row_top < scroll_pos - 0.5 || row_bottom > scroll_pos + page_size + 0.5) {
			g_warning ("assert-row-fully-visible: row %u not fully visible "
				"(row_top=%.1f row_bottom=%.1f scroll=%.1f page=%.1f)",
				row, row_top, row_bottom, scroll_pos, page_size);
			return FALSE;
		}
		return TRUE;

	} else if (g_str_equal (command, "focus-away")) {
		GtkWidget *entry;

		entry = gtk_entry_new ();
		gtk_box_pack_start (GTK_BOX (gtk_widget_get_parent (fixture->scrolled_window)), entry, FALSE, FALSE, 0);
		gtk_widget_show (entry);
		gtk_widget_grab_focus (entry);
		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (g_str_equal (command, "focus-vtree")) {
		gtk_widget_grab_focus (GTK_WIDGET (e_virtual_tree_get_tree_view (fixture->vtree)));
		wait_milliseconds (event_processing_delay_ms);
		return TRUE;

	} else if (*command == '\0') {
		return TRUE;

	} else {
		g_warning ("%s: Unknown command '%s'", G_STRFUNC, command);
		return FALSE;
	}
}

static gboolean
process_commands (TestFixture *fixture,
		  const gchar *commands)
{
	gchar **cmds;
	gint cc;
	gboolean success = TRUE;

	cmds = g_strsplit (commands, "\n", -1);
	for (cc = 0; cmds && cmds[cc] && success; cc++) {
		success = process_command (fixture, cmds[cc]);
	}

	g_strfreev (cmds);

	return success;
}

static gchar *	get_selected_string (EVirtualTree *vtree);

static void
fixture_set_up (TestFixture *fixture,
		gconstpointer user_data)
{
	GtkCellRenderer *renderer;
	GtkWidget *box;
	guint col;

	fixture->signal_log = g_string_new ("");
	fixture->key_state = 0;

	fixture->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (fixture->window), 400, 300);

	if (in_background) {
		gtk_window_set_keep_below (GTK_WINDOW (fixture->window), TRUE);
		gtk_window_set_focus_on_map (GTK_WINDOW (fixture->window), FALSE);
	}

	fixture->model = g_object_new (TEST_TYPE_VIRTUAL_TREE_MODEL, NULL);

	if (user_data == build_large_test_data)
		build_large_test_data (fixture->model);
	else
		build_test_data (fixture->model);

	fixture->vtree = E_VIRTUAL_TREE (
		e_virtual_tree_new (E_VIRTUAL_TREE_MODEL (fixture->model)));

	col = e_virtual_tree_add_column (fixture->vtree, "label", "Label");
	renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (fixture->vtree, col, 0, renderer,
		TRUE, label_data_func, NULL, NULL);
	e_virtual_tree_set_expander_column (fixture->vtree, col, 0);

	e_virtual_tree_set_selection_mode (fixture->vtree, GTK_SELECTION_SINGLE);

	fixture->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (fixture->scrolled_window),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (fixture->scrolled_window),
		GTK_WIDGET (fixture->vtree));

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (box), fixture->scrolled_window, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (fixture->window), box);

	g_signal_connect (fixture->vtree, "selection-changed",
		G_CALLBACK (on_selection_changed), fixture);
	g_signal_connect (fixture->vtree, "cursor-changed",
		G_CALLBACK (on_cursor_changed), fixture);
	g_signal_connect (fixture->vtree, "row-expanded",
		G_CALLBACK (on_row_expanded), fixture);
	g_signal_connect (fixture->vtree, "row-activated",
		G_CALLBACK (on_row_activated), fixture);
	g_signal_connect (fixture->vtree, "cell-clicked",
		G_CALLBACK (on_cell_clicked), fixture);

	e_virtual_tree_model_emit_row_count_changed (
		E_VIRTUAL_TREE_MODEL (fixture->model));

	gtk_widget_show_all (fixture->window);

	wait_milliseconds (100);
	flush_main_context ();

	g_string_truncate (fixture->signal_log, 0);
}

static void
fixture_tear_down (TestFixture *fixture,
		   gconstpointer user_data)
{
	gtk_widget_destroy (fixture->window);
	fixture->vtree = NULL;
	g_clear_object (&fixture->model);
	g_string_free (fixture->signal_log, TRUE);
	fixture->signal_log = NULL;
}

typedef void (* TestFunc) (TestFixture *fixture);
typedef void (* TestFuncData) (TestFixture *fixture, gconstpointer user_data);

static void
add_test (const gchar *name,
	  TestFunc func)
{
	g_test_add (name, TestFixture, NULL,
		fixture_set_up, (TestFuncData) func, fixture_tear_down);
}

static void
add_test_with_data (const gchar *name,
		    TestFunc func,
		    gconstpointer data)
{
	g_test_add (name, TestFixture, data,
		fixture_set_up, (TestFuncData) func, fixture_tear_down);
}

static void
test_cursor_set_get (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"assert-tree:A(A1(A1a,A1b),A2),B(B1(B1a)),C\n"
		"assert-n-rows:9\n"
		"cursor:3\n"
		"assert-cursor:3\n"))
		g_test_fail ();
}

static void
test_cursor_arrow_down (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"cursor:0\n"
		"clear-log\n"
		"seq:d\n"
		"assert-cursor:1\n"))
		g_test_fail ();
}

static void
test_cursor_arrow_up (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"cursor:2\n"
		"clear-log\n"
		"seq:u\n"
		"assert-cursor:1\n"))
		g_test_fail ();
}

static void
test_cursor_home_end (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"cursor:4\n"
		"seq:h\n"
		"assert-cursor:0\n"
		"seq:e\n"
		"assert-cursor:8\n"))
		g_test_fail ();
}

static void
test_selection_single_arrow (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"cursor:0\n"
		"select:0\n"
		"seq:dd\n"
		"assert-cursor:2\n"))
		g_test_fail ();
}

static void
test_selection_multiple_shift_down (TestFixture *fixture)
{
	/* Shift+Down from row 0 should extend selection downward, keeping
	 * the anchor at row 0 across both Down presses. */
	if (!process_commands (fixture,
		"selection-mode:multiple\n"
		"seq:h\n"
		"clear-log\n"
		"seq:Sdds\n"
		"assert-cursor:2\n"
		"assert-selected-count:3\n"))
		g_test_fail ();
}

static void
test_selection_multiple_ctrl_down (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"selection-mode:multiple\n"
		"seq:h\n"
		"clear-log\n"
		"seq:Cdc\n"
		"assert-cursor:1\n"
		"assert-selected:0\n"))
		g_test_fail ();
}

static void
test_selection_multiple_ctrl_space (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"selection-mode:multiple\n"
		"seq:h\n"
		"clear-log\n"
		/* Ctrl+Down moves cursor without selecting, Ctrl+Space selects */
		"seq:Cd c\n"
		"assert-cursor:1\n"
		"assert-selected:0,1\n"
		/* Ctrl+Space again on same row should deselect it */
		"seq:C c\n"
		"assert-cursor:1\n"
		"assert-selected:0\n"
		/* Ctrl+Space once more to re-select */
		"seq:C c\n"
		"assert-cursor:1\n"
		"assert-selected:0,1\n"))
		g_test_fail ();
}

static void
test_selection_multiple_ctrl_click (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"selection-mode:multiple\n"
		"seq:h\n"
		"click-row:1\n"
		"assert-cursor:1\n"
		"assert-selected:1\n"
		/* Ctrl+Click on row 3: both rows 1 and 3 selected */
		"click-row:3:ctrl\n"
		"assert-cursor:3\n"
		"assert-selected:1,3\n"
		/* Ctrl+Click on row 1 to deselect it */
		"click-row:1:ctrl\n"
		"assert-cursor:1\n"
		"assert-selected:3\n"
		/* Ctrl+Click on row 5 to add it */
		"click-row:5:ctrl\n"
		"assert-cursor:5\n"
		"assert-selected:3,5\n"
		/* Plain click on row 2 clears everything, selects only 2 */
		"click-row:2\n"
		"assert-cursor:2\n"
		"assert-selected:2\n"))
		g_test_fail ();
}

static void
test_tree_add_remove (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"assert-tree:A(A1(A1a,A1b),A2),B(B1(B1a)),C\n"
		"assert-n-rows:9\n"
		"add-node:D\n"
		"assert-tree:A(A1(A1a,A1b),A2),B(B1(B1a)),C,D\n"
		"assert-n-rows:10\n"
		"add-node:D1:D\n"
		"assert-tree:A(A1(A1a,A1b),A2),B(B1(B1a)),C,D(D1)\n"
		"assert-n-rows:10\n"
		"add-node:A1c:A1\n"
		"assert-tree:A(A1(A1a,A1b,A1c),A2),B(B1(B1a)),C,D(D1)\n"
		"assert-n-rows:11\n"
		"remove-node:A1b\n"
		"assert-tree:A(A1(A1a,A1c),A2),B(B1(B1a)),C,D(D1)\n"
		"assert-n-rows:10\n"
		"remove-node:B\n"
		"assert-tree:A(A1(A1a,A1c),A2),C,D(D1)\n"
		"assert-n-rows:7\n"))
		g_test_fail ();
}

static void
test_signals_cursor_changed (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"cursor:0\n"
		"clear-log\n"
		"seq:d\n"
		"assert-signals:cursor-changed:1:A1|selection-changed|"))
		g_test_fail ();
}

static void
test_signals_selection_changed (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"cursor:0\n"
		"clear-log\n"
		"select:3\n"
		"assert-signals:selection-changed|"))
		g_test_fail ();
}

static void
test_signals_no_redundant_on_reclick (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"click-row:2\n"
		"assert-cursor:2\n"
		"assert-selected:2\n"
		"clear-log\n"
		/* Click same row again — no cursor or selection change */
		"click-row:2\n"
		"assert-cursor:2\n"
		"assert-selected:2\n"
		"assert-signals:cell-clicked:2:A1a:0|\n"
		/* Click different row — both signals expected */
		"clear-log\n"
		"click-row:3\n"
		"assert-cursor:3\n"
		"assert-selected:3\n"
		"assert-signals:cell-clicked:3:A1b:0|cursor-changed:3:A1b|selection-changed|\n"
		/* Click same row again — no redundant signals */
		"clear-log\n"
		"click-row:3\n"
		"assert-cursor:3\n"
		"assert-selected:3\n"
		"assert-signals:cell-clicked:3:A1b:0|\n"
		/* Multiple selection: click row, then click same row again */
		"selection-mode:multiple\n"
		"click-row:4\n"
		"clear-log\n"
		"click-row:4\n"
		"assert-cursor:4\n"
		"assert-selected:4\n"
		"assert-signals:cell-clicked:4:A2:0|\n"))
		g_test_fail ();
}

static void
test_signals_on_remove_cursored (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"assert-tree:A(A1(A1a,A1b),A2),B(B1(B1a)),C\n"
		/* Single selection: click row 3 (A1b), sets cursor + selection */
		"click-row:3\n"
		"assert-cursor:3\n"
		"assert-selected:3\n"
		"clear-log\n"
		/* Remove A1b — cursor should move to row 3 (now A2),
		 * both signals must fire */
		"remove-node:A1b\n"
		"assert-cursor:3\n"
		"assert-signals:cursor-changed:3:A2|selection-changed|\n"
		/* Row 3 (A2) should be selected after cursor moved there */
		"assert-selected:3\n"
		/* Remove A2 — cursor moves to row 3 (now B),
		 * both signals must fire again consistently */
		"clear-log\n"
		"remove-node:A2\n"
		"assert-cursor:3\n"
		"assert-signals:cursor-changed:3:B|selection-changed|\n"
		"assert-selected:3\n"
		/* Multiple selection: select rows 2,3,4 (A1a,B,B1),
		 * cursor on 3 (B), remove B (with children B1,B1a) —
		 * cursor to 3 (C), both signals must fire */
		"selection-mode:multiple\n"
		"click-row:2\n"
		"click-row:3:ctrl\n"
		"click-row:4:ctrl\n"
		"assert-cursor:4\n"
		"assert-selected:2,3,4\n"
		"clear-log\n"
		"cursor:3\n"
		"clear-log\n"
		"remove-node:B\n"
		"assert-signals:cursor-changed:3:C|selection-changed|\n"
		"assert-selected:2\n"))
		g_test_fail ();
}

static void
test_signals_on_remove_multi_select (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"assert-tree:A(A1(A1a,A1b),A2),B(B1(B1a)),C\n"
		"selection-mode:multiple\n"
		/* Select rows 3,5,7 with cursor at 7 */
		"click-row:3\n"
		"click-row:5:ctrl\n"
		"click-row:7:ctrl\n"
		"assert-cursor:7\n"
		"assert-selected:3,5,7\n"
		/* Remove B1a (row 7, cursor+selected):
		 * cursor moves to 7 (C), both signals fire */
		"clear-log\n"
		"remove-node:B1a\n"
		"assert-cursor:7\n"
		"assert-signals:cursor-changed:7:C|selection-changed|\n"
		"assert-selected:3,5\n"
		/* Now cursor=7(C) is NOT selected. Remove C:
		 * cursor moves to 6(B1), only cursor-changed fires */
		"clear-log\n"
		"remove-node:C\n"
		"assert-cursor:6\n"
		"assert-signals:cursor-changed:6:B1|\n"
		"assert-selected:3,5\n"
		/* Ctrl+click cursor row 6(B1) to select it, then
		 * ctrl+click again to unselect — cursor stays at 6 */
		"click-row:6:ctrl\n"
		"assert-selected:3,5,6\n"
		"click-row:6:ctrl\n"
		"assert-cursor:6\n"
		"assert-selected:3,5\n"
		/* Cursor at 6(B1) NOT selected, selected={3,5}.
		 * Remove B1 (row 6): cursor moves to 5(B),
		 * only cursor-changed fires */
		"clear-log\n"
		"remove-node:B1\n"
		"assert-cursor:5\n"
		"assert-signals:cursor-changed:5:B|\n"
		"assert-selected:3,5\n"
		/* Remove B (row 5, cursor+selected, leaf now):
		 * cursor to 5(C gone, so 4=A2), both signals fire */
		"clear-log\n"
		"remove-node:B\n"
		"assert-cursor:4\n"
		"assert-signals:cursor-changed:4:A2|selection-changed|\n"
		"assert-selected:3\n"))
		g_test_fail ();
}

static gboolean
capture_hit_renderer_cb (EVirtualTree *tree,
			 guint row,
			 GObject *row_object,
			 guint column,
			 GtkCellRenderer *renderer,
			 gpointer user_data)
{
	GtkCellRenderer **out_renderer = user_data;

	*out_renderer = renderer;

	return FALSE;
}

static void
test_cell_click_hit_test_accounts_for_expander_indent (TestFixture *fixture)
{
	GtkTreeView *tv = e_virtual_tree_get_tree_view (fixture->vtree);
	GtkCellRenderer *hit_renderer = NULL;
	GtkTreePath *path;
	GdkRectangle cell_rect;
	gint style_expander_size = 0, horizontal_separator = 0;
	gint expander_size, indent;
	guint depth = 2; /* "A1a" is at row 2, depth 2 */
	gulong handler_id;

	gtk_widget_style_get (GTK_WIDGET (tv),
		"expander-size", &style_expander_size,
		"horizontal-separator", &horizontal_separator,
		NULL);

	expander_size = style_expander_size + horizontal_separator / 2;
	indent = (gint) (depth + 1) * expander_size;

	path = gtk_tree_path_new_from_indices (2, -1);
	gtk_tree_view_get_cell_area (tv, path, NULL, &cell_rect);
	gtk_tree_path_free (path);

	handler_id = g_signal_connect (fixture->vtree, "cell-clicked",
		G_CALLBACK (capture_hit_renderer_cb), &hit_renderer);

	send_click (GTK_WIDGET (tv), cell_rect.x + indent + 2, cell_rect.y + cell_rect.height / 2, 0);

	g_signal_handler_disconnect (fixture->vtree, handler_id);

	g_assert_nonnull (hit_renderer);
	g_assert_true (GTK_IS_CELL_RENDERER_TEXT (hit_renderer));
}

static void
test_cell_click_expander_indent_not_hit_for_leaf_row (TestFixture *fixture)
{
	GtkTreeView *tv = e_virtual_tree_get_tree_view (fixture->vtree);
	GtkCellRenderer *hit_renderer = GINT_TO_POINTER (1);
	GtkTreePath *path;
	GdkRectangle cell_rect;
	gint style_expander_size = 0, horizontal_separator = 0;
	gint expander_size, indent;
	guint depth = 2; /* "A1a" is at row 2, depth 2, and has no children */
	gulong handler_id;

	gtk_widget_style_get (GTK_WIDGET (tv),
		"expander-size", &style_expander_size,
		"horizontal-separator", &horizontal_separator,
		NULL);

	expander_size = style_expander_size + horizontal_separator / 2;
	indent = (gint) (depth + 1) * expander_size;

	path = gtk_tree_path_new_from_indices (2, -1);
	gtk_tree_view_get_cell_area (tv, path, NULL, &cell_rect);
	gtk_tree_path_free (path);

	handler_id = g_signal_connect (fixture->vtree, "cell-clicked",
		G_CALLBACK (capture_hit_renderer_cb), &hit_renderer);

	send_click (GTK_WIDGET (tv), cell_rect.x + indent - 2, cell_rect.y + cell_rect.height / 2, 0);

	g_signal_handler_disconnect (fixture->vtree, handler_id);

	g_assert_null (hit_renderer);
}

static void
test_signals_on_remove_cursored_offscreen (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"assert-n-rows:50\n"
		"click-row:5\n"
		"assert-cursor:5\n"
		"assert-selected:5\n"
		/* Scroll cursor out of sight */
		"scroll:30\n"
		"wait:50\n"
		"clear-log\n"
		/* Remove L06 (row 5) while it's offscreen;
		 * cursor should move to row 5 (now L07),
		 * cursor-changed must still fire */
		"remove-node:L06\n"
		"assert-cursor:5\n"
		"assert-signals:cursor-changed:5:L07|selection-changed|\n"
		"assert-selected:5\n"))
		g_test_fail ();
}

static void
test_scroll_stability_single (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"assert-n-rows:50\n"
		"scroll:20\n"
		"wait:50\n"
		"assert-first-visible:20\n"
		"cursor:25\n"
		"unselect-all\n"
		"select:25\n"
		"clear-log\n"
		/* Add child to L40 (below viewport, collapsed) — no visible change */
		"add-node:X1:L40\n"
		"assert-n-rows:50\n"
		"assert-cursor:25\n"
		"assert-selected:25\n"
		"assert-first-visible:20\n"
		"assert-signals:\n"
		/* Expand L40 — child now visible below viewport */
		"clear-log\n"
		"expand:L40\n"
		"assert-n-rows:51\n"
		"assert-cursor:25\n"
		"assert-selected:25\n"
		"assert-first-visible:20\n"
		"assert-signals:\n"
		/* Add child to L10 (above viewport, collapsed) — no visible change */
		"clear-log\n"
		"add-node:X2:L10\n"
		"assert-n-rows:51\n"
		"assert-cursor:25\n"
		"assert-selected:25\n"
		"assert-first-visible:20\n"
		"assert-signals:\n"
		/* Expand L10 — inserts 1 row above viewport;
		 * first_visible_row shifts to keep showing the same content */
		"clear-log\n"
		"expand:L10\n"
		"assert-n-rows:52\n"
		"assert-cursor:26\n"
		"assert-selected:26\n"
		"assert-first-visible:21\n"
		/* Collapse L10 — removes row above viewport, shifts back */
		"clear-log\n"
		"collapse:L10\n"
		"assert-n-rows:51\n"
		"assert-cursor:25\n"
		"assert-selected:25\n"
		"assert-first-visible:20\n"
		/* Add child to L22 (in viewport), expand — row inserted in view */
		"clear-log\n"
		"add-node:X3:L22\n"
		"expand:L22\n"
		"assert-n-rows:52\n"
		"assert-cursor:26\n"
		"assert-selected:26\n"
		/* Collapse L22 — shifts back */
		"clear-log\n"
		"collapse:L22\n"
		"assert-n-rows:51\n"
		"assert-cursor:25\n"
		"assert-selected:25\n"
		/* Remove a leaf below viewport (X1 child of L40) */
		"clear-log\n"
		"collapse:L40\n"
		"remove-node:X1\n"
		"assert-n-rows:50\n"
		"assert-cursor:25\n"
		"assert-selected:25\n"
		"assert-first-visible:20\n"
		/* Remove a leaf above viewport (X2 child of L10, collapsed) */
		"remove-node:X2\n"
		"assert-cursor:25\n"
		"assert-first-visible:20\n"
		/* Remove a visible node above cursor (L19, row 19) —
		 * above viewport, so first_visible shifts down */
		"clear-log\n"
		"remove-node:L19\n"
		"assert-n-rows:49\n"
		"assert-cursor:24\n"
		"assert-selected:24\n"
		"assert-first-visible:19\n"
		/* Remove a visible node below cursor (L28, now at row 26) */
		"clear-log\n"
		"remove-node:L28\n"
		"assert-n-rows:48\n"
		"assert-cursor:24\n"
		"assert-selected:24\n"
		"assert-first-visible:19\n"
		/* Verify cursor is navigable */
		"seq:d\n"
		"assert-cursor:25\n"
		"seq:u\n"
		"assert-cursor:24\n"))
		g_test_fail ();
}

static void
test_scroll_stability_multiple (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"assert-n-rows:50\n"
		"selection-mode:multiple\n"
		"scroll:20\n"
		"wait:50\n"
		"assert-first-visible:20\n"
		"cursor:25\n"
		"unselect-all\n"
		/* Select rows spanning above, inside, and below viewport */
		"select:15\n"
		"select:22\n"
		"select:25\n"
		"select:30\n"
		"select:45\n"
		"assert-selected:15,22,25,30,45\n"
		/* Add child to L08 (above viewport) + expand — all shift +1 */
		"clear-log\n"
		"add-node:M1:L08\n"
		"expand:L08\n"
		"assert-n-rows:51\n"
		"assert-cursor:26\n"
		"assert-selected:16,23,26,31,46\n"
		"assert-first-visible:21\n"
		/* Add child to L45 (below viewport) + expand — no shift */
		"clear-log\n"
		"add-node:M2:L45\n"
		"expand:L45\n"
		"assert-n-rows:52\n"
		"assert-cursor:26\n"
		"assert-selected:16,23,26,31,47\n"
		"assert-first-visible:21\n"
		/* Collapse L08 — shift back above */
		"clear-log\n"
		"collapse:L08\n"
		"assert-n-rows:51\n"
		"assert-cursor:25\n"
		"assert-selected:15,22,25,30,46\n"
		/* Collapse L45 and remove M2 */
		"collapse:L45\n"
		"remove-node:M2\n"
		"assert-selected:15,22,25,30,45\n"
		/* Remove M1 (under collapsed L08) — no visible change */
		"remove-node:M1\n"
		"assert-selected:15,22,25,30,45\n"
		/* Remove selected row below viewport (L46 at row 45) */
		"clear-log\n"
		"remove-node:L46\n"
		"assert-n-rows:49\n"
		"assert-selected:15,22,25,30\n"
		/* Remove selected row in viewport (L23 at row 22) */
		"clear-log\n"
		"remove-node:L23\n"
		"assert-n-rows:48\n"
		"assert-selected:15,24,29\n"
		"assert-cursor:24\n"
		/* Remove non-selected row above viewport — all shift -1 */
		"clear-log\n"
		"remove-node:L05\n"
		"assert-n-rows:47\n"
		"assert-selected:14,23,28\n"
		"assert-cursor:23\n"))
		g_test_fail ();
}

static void
test_cursor_follows_resort (TestFixture *fixture)
{
	GString *script;
	gint ii;

	/* 50 rows, L01..L50, only ~10 fit in the viewport. L13 (row 12)
	 * is selected and visible near the top. A resort must keep it
	 * as the cursor row and scroll it back into view, wherever it
	 * ends up, exactly like a column-header sort click would drive
	 * the backing model through before-rebuild/after-rebuild. */
	script = g_string_new (
		"assert-n-rows:50\n"
		"click-row:12\n"
		"assert-cursor:12\n"
		"assert-selected:12\n"
		"assert-row-fully-visible:cursor\n"
		"rebuild:");

	/* Sort descending: L13 moves from row 12 to row 37 (near the bottom). */
	for (ii = 50; ii >= 1; ii--)
		g_string_append_printf (script, "L%02d%c", ii, ii > 1 ? ',' : '\n');

	g_string_append (script,
		"assert-cursor:37\n"
		"assert-selected:37\n"
		"assert-row-fully-visible:cursor\n"
		"rebuild:");

	/* Sorting turned off: an arbitrary order moves L13 to row 7. */
	for (ii = 6; ii <= 50; ii++)
		g_string_append_printf (script, "L%02d,", ii);
	for (ii = 1; ii <= 5; ii++)
		g_string_append_printf (script, "L%02d%c", ii, ii < 5 ? ',' : '\n');

	g_string_append (script,
		"assert-cursor:7\n"
		"assert-selected:7\n"
		"assert-row-fully-visible:cursor\n"
		"rebuild:");

	/* Sort ascending again: L13 is back near the top, at row 12. */
	for (ii = 1; ii <= 50; ii++)
		g_string_append_printf (script, "L%02d%c", ii, ii < 50 ? ',' : '\n');

	g_string_append (script,
		"assert-cursor:12\n"
		"assert-selected:12\n"
		"assert-row-fully-visible:cursor\n"
		/* Keyboard navigation must continue from the restored cursor row. */
		"seq:d\n"
		"assert-cursor:13\n");

	if (!process_commands (fixture, script->str))
		g_test_fail ();

	g_string_free (script, TRUE);
}

static void
test_selection_across_pages (TestFixture *fixture)
{
	/* Selected nodes throughout the test: L04, L11, L21, L31, L41, L49.
	 * The indices shift as rows are inserted/removed but the same
	 * nodes stay selected. */
	if (!process_commands (fixture,
		"assert-n-rows:50\n"
		"selection-mode:multiple\n"
		"cursor:5\n"
		"unselect-all\n"
		/* Select rows spread across multiple pages:
		 * L04=3, L11=10, L21=20, L31=30, L41=40, L49=48 */
		"select:3\n"
		"select:10\n"
		"select:20\n"
		"select:30\n"
		"select:40\n"
		"select:48\n"
		"assert-selected:3,10,20,30,40,48\n"
		/* Scroll to various positions and verify selection sticks */
		"scroll:0\n"
		"wait:50\n"
		"assert-selected:3,10,20,30,40,48\n"
		"scroll:15\n"
		"wait:50\n"
		"assert-selected:3,10,20,30,40,48\n"
		"scroll:35\n"
		"wait:50\n"
		"assert-selected:3,10,20,30,40,48\n"
		"scroll:0\n"
		"wait:50\n"
		"assert-selected:3,10,20,30,40,48\n"
		/* Add child to L01 (before all selected) and expand:
		 * P1 inserted at index 1, all selected shift +1 */
		"add-node:P1:L01\n"
		"expand:L01\n"
		"assert-selected:4,11,21,31,41,49\n"
		/* Add child to L20 (between selected) and expand:
		 * L20 is now at index 20; P2 inserted at 21,
		 * shifting L21 from 21 to 22 */
		"add-node:P2:L20\n"
		"expand:L20\n"
		"assert-selected:4,11,22,32,42,50\n"
		/* Add child to L49 (at a selected row) and expand:
		 * L49 is now at index 50; P3 inserted at 51,
		 * nothing at or after 51 is selected */
		"add-node:P3:L49\n"
		"expand:L49\n"
		"assert-selected:4,11,22,32,42,50\n"
		/* Scroll around after inserts — selection still correct */
		"scroll:20\n"
		"wait:50\n"
		"assert-selected:4,11,22,32,42,50\n"
		"scroll:40\n"
		"wait:50\n"
		"assert-selected:4,11,22,32,42,50\n"
		"scroll:0\n"
		"wait:50\n"
		"assert-selected:4,11,22,32,42,50\n"
		/* Collapse L49, remove P3 (after all selected):
		 * P3 removed from index 51, no selected rows shift */
		"collapse:L49\n"
		"remove-node:P3\n"
		"assert-selected:4,11,22,32,42,50\n"
		/* Collapse L20, remove P2 (between selected):
		 * P2 removed from index 21, L21 shifts 22→21 etc */
		"collapse:L20\n"
		"remove-node:P2\n"
		"assert-selected:4,11,21,31,41,49\n"
		/* Collapse L01, remove P1 (before all selected):
		 * P1 removed from index 1, all shift -1 */
		"collapse:L01\n"
		"remove-node:P1\n"
		"assert-selected:3,10,20,30,40,48\n"
		/* Final scroll verification */
		"scroll:25\n"
		"wait:50\n"
		"assert-selected:3,10,20,30,40,48\n"))
		g_test_fail ();
}

static void
test_scroll_stability_edges (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"assert-n-rows:50\n"
		"scroll:20\n"
		"wait:50\n"
		"cursor:25\n"
		"unselect-all\n"
		"select:25\n"
		/* Remove the cursor row — cursor should move to next row */
		"clear-log\n"
		"remove-node:L26\n"
		"assert-n-rows:49\n"
		"assert-cursor:25\n"
		/* Add child to collapsed node — no visible change */
		"clear-log\n"
		"add-node:Y1:L30\n"
		"assert-n-rows:49\n"
		"assert-cursor:25\n"
		"assert-signals:\n"
		/* Move cursor to last row and remove it — cursor moves to previous */
		"cursor:48\n"
		"select:48\n"
		"clear-log\n"
		"remove-node:L50\n"
		"assert-n-rows:48\n"
		"assert-cursor:47\n"
		/* Select cursor row, remove it — cursor moves, selection drops */
		"cursor:30\n"
		"unselect-all\n"
		"select:30\n"
		"select:35\n"
		"clear-log\n"
		"remove-node:L32\n"
		"assert-n-rows:47\n"
		"assert-cursor:30\n"
		"assert-selected:34\n"))
		g_test_fail ();
}

static void
test_scroll_stability_flat_insert (TestFixture *fixture)
{
	/* Simulates new messages arriving: flat rows inserted above
	 * the viewport should not shift what the user sees. */
	if (!process_commands (fixture,
		"assert-n-rows:50\n"
		"scroll:20\n"
		"wait:50\n"
		"assert-first-visible:20\n"
		"cursor:25\n"
		"unselect-all\n"
		"select:25\n"
		/* Insert a flat row before L01 (top of tree, above viewport) */
		"clear-log\n"
		"insert-before:N1:L01\n"
		"assert-n-rows:51\n"
		"assert-cursor:26\n"
		"assert-selected:26\n"
		"assert-first-visible:21\n"
		/* Insert another flat row before L01 (still above viewport) */
		"clear-log\n"
		"insert-before:N2:L01\n"
		"assert-n-rows:52\n"
		"assert-cursor:27\n"
		"assert-selected:27\n"
		"assert-first-visible:22\n"
		/* Insert a flat row before a node below viewport — no shift */
		"clear-log\n"
		"insert-before:N3:L40\n"
		"assert-n-rows:53\n"
		"assert-cursor:27\n"
		"assert-selected:27\n"
		"assert-first-visible:22\n"
		/* Insert a flat row before a node inside the viewport */
		"clear-log\n"
		"insert-before:N4:L25\n"
		"assert-n-rows:54\n"
		"assert-cursor:28\n"
		"assert-selected:28\n"
		/* Remove one of the rows above viewport */
		"clear-log\n"
		"remove-node:N1\n"
		"assert-n-rows:53\n"
		"assert-cursor:27\n"
		"assert-selected:27\n"
		"assert-first-visible:21\n"
		/* Remove the other row above viewport */
		"clear-log\n"
		"remove-node:N2\n"
		"assert-n-rows:52\n"
		"assert-cursor:26\n"
		"assert-selected:26\n"
		"assert-first-visible:20\n"
		/* Remove remaining extras */
		"remove-node:N3\n"
		"remove-node:N4\n"
		"assert-n-rows:50\n"
		"assert-cursor:25\n"
		"assert-selected:25\n"
		"assert-first-visible:20\n"
		/* Verify cursor is still navigable */
		"seq:d\n"
		"assert-cursor:26\n"
		"seq:u\n"
		"assert-cursor:25\n"))
		g_test_fail ();
}

static void
test_scroll_stability_expand_after_collapse_hides_cursor (TestFixture *fixture)
{
	GString *script;
	gint ii;

	/* L25 (row 24) gets 20 children, more than fit in the viewport.
	 * Cursor sits on one of them, so collapsing L25 removes the cursor
	 * row and relocates the cursor to the next visible row (L26).
	 * Re-expanding L25 then pushes that relocated cursor row far below
	 * the viewport again. Neither the collapse nor the re-expand may
	 * scroll the view to chase the cursor - only the row the user
	 * actually clicked on should move into view. */
	script = g_string_new ("assert-n-rows:50\n");

	for (ii = 1; ii <= 20; ii++)
		g_string_append_printf (script, "add-node:C%02d:L25\n", ii);

	g_string_append (script,
		"scroll:20\n"
		"wait:50\n"
		"assert-first-visible:20\n"
		"expand:L25\n"
		"assert-n-rows:70\n"
		"assert-first-visible:20\n"
		"cursor:26\n"
		"unselect-all\n"
		"select:26\n"
		"clear-log\n"
		/* Collapse L25: cursor (row 26, a child of L25) is removed
		 * and relocates to row 25 (now L26). The viewport must stay
		 * put. */
		"collapse:L25\n"
		"assert-n-rows:50\n"
		"assert-cursor:25\n"
		"assert-selected:25\n"
		"assert-first-visible:20\n"
		"assert-row-fully-visible:cursor\n"
		"clear-log\n"
		/* Re-expand L25: the cursor (still L26) shifts from row 25
		 * to row 45, well outside the viewport. The viewport must
		 * still not jump to follow it. */
		"expand:L25\n"
		"assert-n-rows:70\n"
		"assert-cursor:45\n"
		"assert-selected:45\n"
		"assert-first-visible:20\n");

	if (!process_commands (fixture, script->str))
		g_test_fail ();

	g_string_free (script, TRUE);
}

static void
test_scroll_partial_row (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"assert-n-rows:50\n"
		/* Scroll so row 10 is partially visible at the top */
		"scroll-half-row:10\n"
		"wait:50\n"
		"assert-first-visible:10\n"
		/* Place cursor on row 11 (fully visible) */
		"cursor:11\n"
		/* Press Up — cursor moves to row 10 which was partially
		 * visible; auto-scroll should align it fully visible */
		"seq:u\n"
		"assert-cursor:10\n"
		"assert-row-fully-visible:10\n"
		/* Scroll up via click on partially visible top row */
		"scroll-half-row:20\n"
		"wait:50\n"
		"cursor:21\n"
		"click-row:20\n"
		"assert-cursor:20\n"
		"assert-row-fully-visible:20\n"
		/* Scroll to row-aligned position, put cursor near the
		 * bottom, then press Down into the partial row */
		"scroll-half-row:10\n"
		"wait:50\n"
		/* Place cursor on a row near the bottom of the viewport;
		 * pressing Down will reach the partially visible row */
		"cursor:12\n"
		"seq:ppp\n"
		/* Now the cursor is near the bottom; pressing Down should
		 * scroll to make the new row fully visible */
		"seq:d\n"
		"assert-row-fully-visible:cursor\n"
		"seq:d\n"
		"assert-row-fully-visible:cursor\n"
		"seq:d\n"
		"assert-row-fully-visible:cursor\n"))
		g_test_fail ();
}

static gchar *
get_selected_string (EVirtualTree *vtree)
{
	EVirtualTreeModel *model = e_virtual_tree_get_model (vtree);
	GPtrArray *selected;
	GArray *indices;
	GString *str;
	guint ii;

	selected = e_virtual_tree_get_selected_rows (vtree);
	indices = g_array_new (FALSE, FALSE, sizeof (guint));

	for (ii = 0; ii < selected->len; ii++) {
		GObject *row_obj = g_ptr_array_index (selected, ii);
		gconstpointer key = e_virtual_tree_model_get_row_key (model, row_obj);
		guint idx = e_virtual_tree_model_find_row_by_key (model, key);
		if (idx != G_MAXUINT)
			g_array_append_val (indices, idx);
	}

	g_array_sort (indices, (GCompareFunc) compare_guint);

	str = g_string_new ("");
	for (ii = 0; ii < indices->len; ii++) {
		if (str->len > 0)
			g_string_append_c (str, ',');
		g_string_append_printf (str, "%u", g_array_index (indices, guint, ii));
	}

	g_ptr_array_unref (selected);
	g_array_free (indices, TRUE);

	return g_string_free (str, FALSE);
}

static void
streaming_respond (const gchar *fmt, ...)
{
	va_list args;

	va_start (args, fmt);
	vprintf (fmt, args);
	va_end (args);

	fflush (stdout);
}

static gboolean
streaming_process_command (TestFixture *fixture,
			   const gchar *command)
{
	if (g_str_has_prefix (command, "click:")) {
		GtkWidget *widget;
		const gchar *ptr = command + 6;
		gchar *endptr;
		gint x, y;
		guint state = 0;

		x = (gint) strtol (ptr, &endptr, 10);
		if (*endptr != ',') {
			streaming_respond ("ERROR: bad click format\n");
			return TRUE;
		}
		ptr = endptr + 1;
		y = (gint) strtol (ptr, &endptr, 10);

		if (*endptr == ':') {
			const gchar *mods = endptr + 1;
			if (strstr (mods, "ctrl"))
				state |= GDK_CONTROL_MASK;
			if (strstr (mods, "shift"))
				state |= GDK_SHIFT_MASK;
		}

		widget = GTK_WIDGET (e_virtual_tree_get_tree_view (fixture->vtree));
		send_click (widget, x, y, state);
		streaming_respond ("OK\n");
		return TRUE;

	} else if (g_str_equal (command, "get-cursor")) {
		gint cursor = e_virtual_tree_get_cursor (fixture->vtree);
		streaming_respond ("cursor:%d\n", cursor);
		return TRUE;

	} else if (g_str_equal (command, "get-selected")) {
		gchar *sel = get_selected_string (fixture->vtree);
		streaming_respond ("selected:%s\n", sel);
		g_free (sel);
		return TRUE;

	} else if (g_str_equal (command, "get-n-rows")) {
		guint n = e_virtual_tree_model_get_row_count (
			E_VIRTUAL_TREE_MODEL (fixture->model));
		streaming_respond ("n-rows:%u\n", n);
		return TRUE;

	} else if (g_str_equal (command, "get-signals")) {
		streaming_respond ("%s.\n", fixture->signal_log->str);
		return TRUE;

	} else if (g_str_equal (command, "get-tree")) {
		gchar *tree = get_tree_string (fixture->model);
		streaming_respond ("tree:%s\n", tree);
		g_free (tree);
		return TRUE;

	} else if (g_str_equal (command, "get-first-visible")) {
		guint fv = _e_virtual_tree_get_first_visible_row (fixture->vtree);
		streaming_respond ("first-visible:%u\n", fv);
		return TRUE;

	} else if (g_str_equal (command, "get-geometry")) {
		GtkWidget *widget;
		GtkAllocation alloc;
		gint wx, wy;

		widget = GTK_WIDGET (e_virtual_tree_get_tree_view (fixture->vtree));
		gtk_widget_get_allocation (widget, &alloc);
		gtk_widget_translate_coordinates (widget,
			fixture->window, 0, 0, &wx, &wy);

		streaming_respond ("geometry:%dx%d+%d+%d\n",
			alloc.width, alloc.height, wx, wy);
		return TRUE;

	} else if (g_str_has_prefix (command, "get-row-geometry:")) {
		GtkTreeView *tv;
		GtkTreePath *path;
		GdkRectangle rect;
		gint row_in_tv;
		guint row;
		gint wx, wy;

		row = (guint) atoi (command + 17);

		tv = e_virtual_tree_get_tree_view (fixture->vtree);
		row_in_tv = (gint) row - (gint) 0;

		path = gtk_tree_path_new_from_indices (row_in_tv, -1);
		gtk_tree_view_get_cell_area (tv, path, NULL, &rect);
		gtk_tree_path_free (path);

		gtk_widget_translate_coordinates (GTK_WIDGET (tv),
			fixture->window, rect.x, rect.y, &wx, &wy);

		streaming_respond ("row-geometry:%dx%d+%d+%d\n",
			rect.width, rect.height, wx, wy);
		return TRUE;

	} else {
		return process_command (fixture, command);
	}
}

typedef struct {
	TestFixture *fixture;
	GMainLoop *loop;
} StreamingData;

static gboolean
on_stdin_input (GIOChannel *channel,
		GIOCondition condition,
		gpointer user_data)
{
	StreamingData *data = user_data;
	gchar *line = NULL;
	gsize length;
	GIOStatus status;

	if (condition & G_IO_ERR) {
		g_main_loop_quit (data->loop);
		return FALSE;
	}

	if (condition & G_IO_IN) {
		gboolean got_eof = FALSE;

		while (TRUE) {
			status = g_io_channel_read_line (channel, &line, &length, NULL, NULL);

			if (status == G_IO_STATUS_EOF) {
				got_eof = TRUE;
				break;
			}

			if (status != G_IO_STATUS_NORMAL || !line) {
				g_free (line);
				break;
			}

			g_strstrip (line);

			if (*line != '\0') {
				gboolean ok;

				ok = streaming_process_command (data->fixture, line);
				if (!ok)
					streaming_respond ("ERROR: command failed: %s\n", line);
			}

			g_free (line);
		}

		if (got_eof) {
			g_main_loop_quit (data->loop);
			return FALSE;
		}
	}

	if (condition & G_IO_HUP) {
		g_main_loop_quit (data->loop);
		return FALSE;
	}

	return TRUE;
}

static void
run_streaming_mode (void)
{
	TestFixture fixture;
	GIOChannel *stdin_channel;
	StreamingData data;
	GMainLoop *loop;

	fixture_set_up (&fixture, NULL);

	streaming_respond ("ready\n");

	loop = g_main_loop_new (NULL, FALSE);

	data.fixture = &fixture;
	data.loop = loop;

	stdin_channel = g_io_channel_unix_new (0);
	g_io_channel_set_encoding (stdin_channel, NULL, NULL);
	g_io_channel_set_flags (stdin_channel,
		g_io_channel_get_flags (stdin_channel) | G_IO_FLAG_NONBLOCK, NULL);
	g_io_add_watch (stdin_channel,
		G_IO_IN | G_IO_HUP | G_IO_ERR,
		on_stdin_input, &data);

	g_main_loop_run (loop);

	g_io_channel_unref (stdin_channel);
	g_main_loop_unref (loop);

	fixture_tear_down (&fixture, NULL);
}

static void
test_selection_restore_after_rebuild (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"assert-n-rows:9\n"
		"selection-mode:multiple\n"
		"unselect-all\n"
		"cursor:2\n"
		"select:1\n"
		"select:2\n"
		"select:5\n"
		"assert-selected:1,2,5\n"
		"assert-cursor:2\n"
		/* Rebuild with same nodes in different order */
		"rebuild:C,B,A,A1,A2,A1a,A1b,B1,B1a\n"
		/* A1 was at row 1, now at row 3; A1a was at 2, now 5; B was at 5, now 1 */
		"assert-selected:1,3,5\n"
		"assert-cursor:5\n"))
		g_test_fail ();
}

static void
test_selection_restore_partial (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"assert-n-rows:9\n"
		"selection-mode:multiple\n"
		"unselect-all\n"
		"cursor:3\n"
		"select:1\n"
		"select:3\n"
		"select:5\n"
		"assert-selected:1,3,5\n"
		/* Rebuild with some nodes removed (A1b at row 3 is gone) */
		"rebuild:A,A1,A1a,A2,B,B1,B1a,C\n"
		/* A1 still at 1, B still at 4, A1b is gone */
		"assert-selected:1,4\n"))
		g_test_fail ();
}

static void
test_selection_restore_no_match (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"assert-n-rows:9\n"
		"selection-mode:multiple\n"
		"unselect-all\n"
		"cursor:2\n"
		"select:0\n"
		"select:2\n"
		"select:5\n"
		"assert-selected:0,2,5\n"
		/* Rebuild with completely different content */
		"rebuild:X,Y,Z\n"
		"assert-selected:\n"
		"assert-cursor:-1\n"))
		g_test_fail ();
}

static void
test_selection_restore_same_count (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"assert-n-rows:9\n"
		"selection-mode:multiple\n"
		"unselect-all\n"
		"cursor:0\n"
		"select:0\n"
		"select:4\n"
		"select:8\n"
		"assert-selected:0,4,8\n"
		/* Rebuild with same count but reversed order */
		"rebuild:C,B1a,B1,B,A2,A1b,A1a,A1,A\n"
		/* A was at 0, now at 8; A2 was at 4, now at 4; C was at 8, now at 0 */
		"assert-selected:0,4,8\n"
		"assert-cursor:8\n"))
		g_test_fail ();
}

static void
test_cursor_preserved_on_focus (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"cursor:3\n"
		"assert-cursor:3\n"
		"scroll:50\n"
		"focus-away\n"
		"clear-log\n"
		"focus-vtree\n"
		"assert-cursor:3\n"
		"assert-signals:\n"))
		g_test_fail ();
}

static void
test_cursor_no_shift_on_resize_when_visible (TestFixture *fixture)
{
	if (!process_commands (fixture,
		"scroll-half-row:10\n"
		"wait:50\n"
		"assert-first-visible:10\n"
		"cursor:13\n"
		"assert-first-visible:10\n"
		"assert-row-fully-visible:cursor\n"
		"clear-log\n"
		"focus-away\n"
		"focus-vtree\n"
		"assert-cursor:13\n"
		"assert-first-visible:10\n"
		"assert-row-fully-visible:cursor\n"))
		g_test_fail ();
}

static void
test_cursor_get_key (TestFixture *fixture)
{
	gconstpointer key;

	key = e_virtual_tree_get_cursor_key (fixture->vtree);
	g_assert_nonnull (key);
	g_assert_cmpstr ((const gchar *) key, ==, "A");

	e_virtual_tree_set_cursor (fixture->vtree, 5);
	key = e_virtual_tree_get_cursor_key (fixture->vtree);
	g_assert_nonnull (key);
	g_assert_cmpstr ((const gchar *) key, ==, "B");

	e_virtual_tree_set_cursor (fixture->vtree, -1);
	key = e_virtual_tree_get_cursor_key (fixture->vtree);
	g_assert_null (key);
}

static void
test_cursor_get_object (TestFixture *fixture)
{
	GObject *obj;
	TestNode *node;

	obj = e_virtual_tree_get_cursor_object (fixture->vtree);
	g_assert_nonnull (obj);
	g_assert_true (TEST_IS_NODE (obj));
	node = TEST_NODE (obj);
	g_assert_cmpstr (node->label, ==, "A");

	e_virtual_tree_set_cursor (fixture->vtree, 2);
	obj = e_virtual_tree_get_cursor_object (fixture->vtree);
	g_assert_nonnull (obj);
	node = TEST_NODE (obj);
	g_assert_cmpstr (node->label, ==, "A1a");

	e_virtual_tree_set_cursor (fixture->vtree, 8);
	obj = e_virtual_tree_get_cursor_object (fixture->vtree);
	g_assert_nonnull (obj);
	node = TEST_NODE (obj);
	g_assert_cmpstr (node->label, ==, "C");

	e_virtual_tree_set_cursor (fixture->vtree, -1);
	g_assert_null (e_virtual_tree_get_cursor_object (fixture->vtree));
}

static void
test_selection_count (TestFixture *fixture)
{
	e_virtual_tree_set_selection_mode (fixture->vtree, GTK_SELECTION_MULTIPLE);

	e_virtual_tree_unselect_all (fixture->vtree);
	g_assert_cmpuint (e_virtual_tree_selected_count (fixture->vtree), ==, 0);

	e_virtual_tree_select_row (fixture->vtree, 3);
	g_assert_cmpuint (e_virtual_tree_selected_count (fixture->vtree), ==, 1);

	e_virtual_tree_select_row (fixture->vtree, 5);
	g_assert_cmpuint (e_virtual_tree_selected_count (fixture->vtree), ==, 2);

	e_virtual_tree_select_all (fixture->vtree);
	g_assert_cmpuint (e_virtual_tree_selected_count (fixture->vtree), ==, 9);

	e_virtual_tree_unselect_all (fixture->vtree);
	g_assert_cmpuint (e_virtual_tree_selected_count (fixture->vtree), ==, 0);
}

static void
test_selection_count_inverted (TestFixture *fixture)
{
	e_virtual_tree_set_selection_mode (fixture->vtree, GTK_SELECTION_MULTIPLE);

	e_virtual_tree_select_all (fixture->vtree);
	g_assert_cmpuint (e_virtual_tree_selected_count (fixture->vtree), ==, 9);

	e_virtual_tree_unselect_row (fixture->vtree, 4);
	g_assert_cmpuint (e_virtual_tree_selected_count (fixture->vtree), ==, 8);

	e_virtual_tree_unselect_row (fixture->vtree, 0);
	g_assert_cmpuint (e_virtual_tree_selected_count (fixture->vtree), ==, 7);
}

typedef struct {
	GtkWidget *window;
	EVirtualTree *vtree;
	TestVirtualTreeModel *model;
	gchar *state_file_a;
	gchar *state_file_b;
} ColumnStateFixture;

static void
label_data_func_cs (EVirtualTree *tree,
		    GtkCellRenderer *renderer,
		    GObject *row_object,
		    guint visible_row,
		    gpointer user_data)
{
	TestNode *node = TEST_NODE (row_object);

	g_object_set (renderer, "text", node->label, NULL);
}

static EVirtualTree *
create_multi_column_tree (TestVirtualTreeModel *model)
{
	EVirtualTree *vtree;
	GtkCellRenderer *renderer;
	guint col;

	vtree = E_VIRTUAL_TREE (e_virtual_tree_new (E_VIRTUAL_TREE_MODEL (model)));

	col = e_virtual_tree_add_column (vtree, "alpha", "Alpha");
	renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (vtree, col, 0, renderer, TRUE,
		label_data_func_cs, NULL, NULL);
	e_virtual_tree_set_expander_column (vtree, col, 0);

	col = e_virtual_tree_add_column (vtree, "beta", "Beta");
	renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (vtree, col, 0, renderer, TRUE,
		label_data_func_cs, NULL, NULL);

	col = e_virtual_tree_add_column (vtree, "gamma", "Gamma");
	renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (vtree, col, 0, renderer, TRUE,
		label_data_func_cs, NULL, NULL);

	col = e_virtual_tree_add_column (vtree, "delta", "Delta");
	renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (vtree, col, 0, renderer, TRUE,
		label_data_func_cs, NULL, NULL);

	col = e_virtual_tree_add_column (vtree, "epsilon", "Epsilon");
	renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (vtree, col, 0, renderer, TRUE,
		label_data_func_cs, NULL, NULL);

	return vtree;
}

static void
cs_fixture_set_up (ColumnStateFixture *fixture,
		   gconstpointer user_data)
{
	fixture->model = g_object_new (TEST_TYPE_VIRTUAL_TREE_MODEL, NULL);
	build_test_data (fixture->model);

	fixture->vtree = create_multi_column_tree (fixture->model);

	fixture->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (fixture->window), 600, 300);
	if (in_background) {
		gtk_window_set_keep_below (GTK_WINDOW (fixture->window), TRUE);
		gtk_window_set_focus_on_map (GTK_WINDOW (fixture->window), FALSE);
	}

	gtk_container_add (GTK_CONTAINER (fixture->window), GTK_WIDGET (fixture->vtree));

	e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (fixture->model));
	gtk_widget_show_all (fixture->window);
	wait_milliseconds (100);
	flush_main_context ();

	fixture->state_file_a = g_build_filename (g_get_tmp_dir (), "vtree-test-state-a.ini", NULL);
	fixture->state_file_b = g_build_filename (g_get_tmp_dir (), "vtree-test-state-b.ini", NULL);

	g_unlink (fixture->state_file_a);
	g_unlink (fixture->state_file_b);
}

static void
cs_fixture_tear_down (ColumnStateFixture *fixture,
		      gconstpointer user_data)
{
	gtk_widget_destroy (fixture->window);
	fixture->vtree = NULL;
	g_clear_object (&fixture->model);

	g_unlink (fixture->state_file_a);
	g_unlink (fixture->state_file_b);

	g_free (fixture->state_file_a);
	g_free (fixture->state_file_b);
}

static GList *
get_column_ids_in_display_order (EVirtualTree *vtree)
{
	GtkTreeView *tv = e_virtual_tree_get_tree_view (vtree);
	GList *columns, *link, *ids = NULL;

	columns = gtk_tree_view_get_columns (tv);
	for (link = columns; link; link = link->next) {
		GtkTreeViewColumn *tvc = link->data;

		ids = g_list_append (ids, (gpointer) gtk_tree_view_column_get_title (tvc));
	}
	g_list_free (columns);

	return ids;
}

static gchar *
column_order_string (EVirtualTree *vtree)
{
	GList *ids, *link;
	GString *str;

	str = g_string_new (NULL);
	ids = get_column_ids_in_display_order (vtree);

	for (link = ids; link; link = link->next) {
		if (str->len > 0)
			g_string_append_c (str, ',');
		g_string_append (str, (const gchar *) link->data);
	}
	g_list_free (ids);

	return g_string_free (str, FALSE);
}

static gchar *
column_visibility_string (EVirtualTree *vtree)
{
	GtkTreeView *tv = e_virtual_tree_get_tree_view (vtree);
	GList *columns, *link;
	GString *str;

	str = g_string_new (NULL);
	columns = gtk_tree_view_get_columns (tv);

	for (link = columns; link; link = link->next) {
		GtkTreeViewColumn *tvc = link->data;

		if (str->len > 0)
			g_string_append_c (str, ',');
		g_string_append (str, gtk_tree_view_column_get_title (tvc));
		g_string_append_c (str, ':');
		g_string_append (str, gtk_tree_view_column_get_visible (tvc) ? "1" : "0");
	}
	g_list_free (columns);

	return g_string_free (str, FALSE);
}

static void
test_column_state_save_load_roundtrip (ColumnStateFixture *fixture,
				       gconstpointer user_data)
{
	GtkTreeView *tv = e_virtual_tree_get_tree_view (fixture->vtree);
	GtkTreeViewColumn *col_beta, *col_delta, *col_epsilon;
	GtkSortType sort_order;
	gchar *order_before, *vis_before, *order_after, *vis_after;
	gboolean sorted;

	/* Set filename first so changes trigger auto-save */
	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_a);

	/* Hide Beta and Delta */
	col_beta = gtk_tree_view_get_column (tv, 1);
	col_delta = gtk_tree_view_get_column (tv, 3);
	gtk_tree_view_column_set_visible (col_beta, FALSE);
	gtk_tree_view_column_set_visible (col_delta, FALSE);

	/* Set sort on Gamma ascending */
	e_virtual_tree_set_column_sort (fixture->vtree, 2, GTK_SORT_ASCENDING, 1);

	/* Reorder: move Epsilon before Alpha */
	col_epsilon = gtk_tree_view_get_column (tv, 4);
	gtk_tree_view_move_column_after (tv, col_epsilon, NULL);

	order_before = column_order_string (fixture->vtree);
	vis_before = column_visibility_string (fixture->vtree);

	/* Flush pending save by switching to another file */
	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_b);

	/* Verify file was created */
	g_assert_true (g_file_test (fixture->state_file_a, G_FILE_TEST_EXISTS));

	gtk_container_remove (GTK_CONTAINER (fixture->window), GTK_WIDGET (fixture->vtree));
	fixture->vtree = create_multi_column_tree (fixture->model);
	gtk_container_add (GTK_CONTAINER (fixture->window), GTK_WIDGET (fixture->vtree));
	e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (fixture->model));
	gtk_widget_show_all (fixture->window);
	wait_milliseconds (100);
	flush_main_context ();

	/* Load state from file */
	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_a);
	wait_milliseconds (50);

	order_after = column_order_string (fixture->vtree);
	vis_after = column_visibility_string (fixture->vtree);

	/* Column order and visibility must match */
	g_assert_cmpstr (order_before, ==, order_after);
	g_assert_cmpstr (vis_before, ==, vis_after);

	/* Verify sort was restored on Gamma */
	sorted = e_virtual_tree_get_column_sort (fixture->vtree, 2, &sort_order, NULL);
	g_assert_true (sorted);
	g_assert_cmpint (sort_order, ==, GTK_SORT_ASCENDING);

	g_free (order_before);
	g_free (vis_before);
	g_free (order_after);
	g_free (vis_after);
}

static void
test_column_state_switch_views (ColumnStateFixture *fixture,
				gconstpointer user_data)
{
	GtkTreeView *tv = e_virtual_tree_get_tree_view (fixture->vtree);
	GtkTreeViewColumn *col, *c_gamma, *c_delta;
	GtkSortType sort_order;
	GList *cols, *link;
	gchar *vis, *order;
	gboolean sorted;

	/* --- Setup view A: hide Beta+Epsilon, sort Alpha descending --- */
	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_a);

	col = gtk_tree_view_get_column (tv, 1); /* Beta */
	gtk_tree_view_column_set_visible (col, FALSE);
	col = gtk_tree_view_get_column (tv, 4); /* Epsilon */
	gtk_tree_view_column_set_visible (col, FALSE);
	e_virtual_tree_set_column_sort (fixture->vtree, 0, GTK_SORT_DESCENDING, 1);

	/* Flush view A by switching to view B file */
	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_b);

	/* Reset: show all columns */
	cols = gtk_tree_view_get_columns (tv);
	for (link = cols; link; link = link->next) {
		gtk_tree_view_column_set_visible (link->data, TRUE);
	}
	g_list_free (cols);

	e_virtual_tree_clear_sort (fixture->vtree);

	/* Reorder to: Gamma, Delta, Alpha, Beta, Epsilon */
	c_gamma = gtk_tree_view_get_column (tv, 2);
	gtk_tree_view_move_column_after (tv, c_gamma, NULL);
	c_delta = gtk_tree_view_get_column (tv, 3);
	gtk_tree_view_move_column_after (tv, c_delta, c_gamma);

	e_virtual_tree_set_column_sort (fixture->vtree, 3, GTK_SORT_ASCENDING, 1);
	e_virtual_tree_set_column_sort (fixture->vtree, 2, GTK_SORT_DESCENDING, 2);

	/* --- Switch to view A (flushes view B to file_b, loads file_a) --- */
	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_a);
	wait_milliseconds (50);

	/* Beta and Epsilon should be hidden */
	vis = column_visibility_string (fixture->vtree);
	g_assert_nonnull (strstr (vis, "Beta:0"));
	g_assert_nonnull (strstr (vis, "Epsilon:0"));
	g_assert_nonnull (strstr (vis, "Alpha:1"));
	g_assert_nonnull (strstr (vis, "Gamma:1"));
	g_assert_nonnull (strstr (vis, "Delta:1"));
	g_free (vis);

	/* Alpha should be sorted descending */
	sorted = e_virtual_tree_get_column_sort (fixture->vtree, 0, &sort_order, NULL);
	g_assert_true (sorted);
	g_assert_cmpint (sort_order, ==, GTK_SORT_DESCENDING);

	/* Delta and Gamma should NOT be sorted (only Alpha was sorted in A) */
	g_assert_false (e_virtual_tree_get_column_sort (fixture->vtree, 2, &sort_order, NULL));
	g_assert_false (e_virtual_tree_get_column_sort (fixture->vtree, 3, &sort_order, NULL));

	/* --- Switch to view B and verify --- */
	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_b);
	wait_milliseconds (50);

	/* All visible */
	vis = column_visibility_string (fixture->vtree);
	g_assert_null (strstr (vis, ":0"));
	g_free (vis);

	/* Column order should be Gamma, Delta, Alpha, Beta, Epsilon */
	order = column_order_string (fixture->vtree);
	g_assert_cmpstr (order, ==, "Gamma,Delta,Alpha,Beta,Epsilon");
	g_free (order);

	/* Delta sorted ascending (priority 1), Gamma sorted descending (priority 2) */
	sorted = e_virtual_tree_get_column_sort (fixture->vtree, 3, &sort_order, NULL);
	g_assert_true (sorted);
	g_assert_cmpint (sort_order, ==, GTK_SORT_ASCENDING);
	sorted = e_virtual_tree_get_column_sort (fixture->vtree, 2, &sort_order, NULL);
	g_assert_true (sorted);
	g_assert_cmpint (sort_order, ==, GTK_SORT_DESCENDING);

	/* --- Switch back to A to confirm it's still correct --- */
	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_a);
	wait_milliseconds (50);

	vis = column_visibility_string (fixture->vtree);
	g_assert_nonnull (strstr (vis, "Beta:0"));
	g_assert_nonnull (strstr (vis, "Epsilon:0"));
	g_free (vis);
}

static void
test_column_state_hidden_reordered (ColumnStateFixture *fixture,
				    gconstpointer user_data)
{
	GtkTreeView *tv = e_virtual_tree_get_tree_view (fixture->vtree);
	GtkTreeViewColumn *col, *c_eps, *c_beta, *last_col;
	GList *cols;
	gchar *order_before, *vis_before, *order_after, *vis_after, *vis;

	/* Set filename first so changes trigger auto-save */
	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_a);

	/* Start: Alpha, Beta, Gamma, Delta, Epsilon
	   Hide Beta and Delta, then reorder aggressively including hidden columns */

	col = gtk_tree_view_get_column (tv, 1); /* Beta */
	gtk_tree_view_column_set_visible (col, FALSE);
	col = gtk_tree_view_get_column (tv, 3); /* Delta */
	gtk_tree_view_column_set_visible (col, FALSE);

	/* Move Epsilon to position 0 (before Alpha) */
	c_eps = gtk_tree_view_get_column (tv, 4);
	gtk_tree_view_move_column_after (tv, c_eps, NULL);

	/* Move hidden Beta to end */
	c_beta = gtk_tree_view_get_column (tv, 2);
	cols = gtk_tree_view_get_columns (tv);
	last_col = g_list_last (cols)->data;
	gtk_tree_view_move_column_after (tv, c_beta, last_col);
	g_list_free (cols);

	order_before = column_order_string (fixture->vtree);
	vis_before = column_visibility_string (fixture->vtree);

	/* Flush pending save by switching to another file */
	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_b);

	/* Recreate tree with fresh columns */
	gtk_container_remove (GTK_CONTAINER (fixture->window), GTK_WIDGET (fixture->vtree));
	fixture->vtree = create_multi_column_tree (fixture->model);
	gtk_container_add (GTK_CONTAINER (fixture->window), GTK_WIDGET (fixture->vtree));
	e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (fixture->model));
	gtk_widget_show_all (fixture->window);
	wait_milliseconds (100);
	flush_main_context ();

	/* Load state */
	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_a);
	wait_milliseconds (50);

	order_after = column_order_string (fixture->vtree);
	vis_after = column_visibility_string (fixture->vtree);

	g_assert_cmpstr (order_before, ==, order_after);
	g_assert_cmpstr (vis_before, ==, vis_after);

	/* Verify the specific hidden columns */
	vis = column_visibility_string (fixture->vtree);
	g_assert_nonnull (strstr (vis, "Beta:0"));
	g_assert_nonnull (strstr (vis, "Delta:0"));
	g_assert_nonnull (strstr (vis, "Epsilon:1"));
	g_free (vis);

	g_free (order_before);
	g_free (vis_before);
	g_free (order_after);
	g_free (vis_after);
}

static void
test_column_state_no_file (ColumnStateFixture *fixture,
			   gconstpointer user_data)
{
	gchar *order_before, *order_after;

	order_before = column_order_string (fixture->vtree);

	/* Setting a nonexistent file should not crash or change state */
	e_virtual_tree_set_column_state_filename (fixture->vtree, "/tmp/vtree-nonexistent-file-12345.ini");
	wait_milliseconds (50);

	order_after = column_order_string (fixture->vtree);
	g_assert_cmpstr (order_before, ==, order_after);
	g_free (order_after);

	/* Setting NULL should not crash */
	e_virtual_tree_set_column_state_filename (fixture->vtree, NULL);

	g_free (order_before);
}

static void
test_column_state_legacy_etable_migration (ColumnStateFixture *fixture,
					   gconstpointer user_data)
{
	static const gchar * const legacy_ids[] = {
		"alpha", "beta", "gamma", "delta", "epsilon"
	};
	static const gchar *legacy_xml =
		"<?xml version=\"1.0\"?>"
		"<ETableState state-version=\"0.1\">"
		"  <column source=\"0\" expansion=\"1.0\"/>"
		"  <column source=\"3\" expansion=\"1.0\"/>"
		"  <column source=\"1\" expansion=\"0.5\"/>"
		"  <grouping>"
		"    <leaf column=\"3\" ascending=\"false\"/>"
		"  </grouping>"
		"</ETableState>";
	GKeyFile *key_file;
	GtkTreeViewColumn *col_alpha, *col_beta, *col_gamma, *col_delta, *col_epsilon;
	GList *ids, *link;
	GString *visible_order;

	key_file = e_virtual_tree_convert_legacy_etable_state (legacy_xml, -1, legacy_ids, G_N_ELEMENTS (legacy_ids));
	g_assert_nonnull (key_file);

	e_virtual_tree_load_column_state_from_key_file (fixture->vtree, key_file);
	g_key_file_unref (key_file);

	col_alpha = e_virtual_tree_get_column (fixture->vtree, 0);
	col_beta = e_virtual_tree_get_column (fixture->vtree, 1);
	col_gamma = e_virtual_tree_get_column (fixture->vtree, 2);
	col_delta = e_virtual_tree_get_column (fixture->vtree, 3);
	col_epsilon = e_virtual_tree_get_column (fixture->vtree, 4);

	g_assert_true (gtk_tree_view_column_get_visible (col_alpha));
	g_assert_true (gtk_tree_view_column_get_visible (col_beta));
	g_assert_true (gtk_tree_view_column_get_visible (col_delta));
	g_assert_false (gtk_tree_view_column_get_visible (col_gamma));
	g_assert_false (gtk_tree_view_column_get_visible (col_epsilon));

	g_assert_true (gtk_tree_view_column_get_sort_indicator (col_delta));
	g_assert_cmpint (gtk_tree_view_column_get_sort_order (col_delta), ==, GTK_SORT_DESCENDING);

	visible_order = g_string_new (NULL);
	ids = get_column_ids_in_display_order (fixture->vtree);
	for (link = ids; link; link = link->next) {
		const gchar *title = link->data;

		if (g_strcmp0 (title, "Alpha") == 0 || g_strcmp0 (title, "Beta") == 0 ||
		    g_strcmp0 (title, "Delta") == 0) {
			if (visible_order->len > 0)
				g_string_append_c (visible_order, ',');
			g_string_append (visible_order, title);
		}
	}
	g_list_free (ids);

	g_assert_cmpstr (visible_order->str, ==, "Alpha,Delta,Beta");
	g_string_free (visible_order, TRUE);
}

static void
test_column_state_legacy_etable_not_recognized (ColumnStateFixture *fixture,
						gconstpointer user_data)
{
	static const gchar * const legacy_ids[] = {
		"alpha", "beta", "gamma", "delta", "epsilon"
	};
	GKeyFile *key_file;

	key_file = e_virtual_tree_convert_legacy_etable_state (
		"[Column-alpha]\norder=0\n", -1, legacy_ids, G_N_ELEMENTS (legacy_ids));
	g_assert_null (key_file);
}

static gpointer
test_get_legacy_column_map_cb (EVirtualTree *vtree,
			       guint *out_n_column_ids,
			       gpointer user_data)
{
	static const gchar * const legacy_ids[] = {
		"alpha", "beta", "gamma", "delta", "epsilon"
	};

	if (out_n_column_ids)
		*out_n_column_ids = G_N_ELEMENTS (legacy_ids);

	return (gpointer) legacy_ids;
}

static void
test_gal_view_legacy_etable_deferred_attach (ColumnStateFixture *fixture,
					     gconstpointer user_data)
{
	static const gchar *legacy_xml =
		"<?xml version=\"1.0\"?>"
		"<ETableState state-version=\"0.1\">"
		"  <column source=\"0\" expansion=\"1.0\"/>"
		"  <column source=\"3\" expansion=\"1.0\"/>"
		"  <column source=\"1\" expansion=\"0.5\"/>"
		"  <grouping>"
		"    <leaf column=\"3\" ascending=\"false\"/>"
		"  </grouping>"
		"</ETableState>";
	GalView *view;
	GtkTreeViewColumn *col_alpha, *col_beta, *col_gamma, *col_delta, *col_epsilon;

	g_file_set_contents (fixture->state_file_a, legacy_xml, -1, NULL);

	g_signal_connect (fixture->vtree, "get-legacy-etable-column-map",
		G_CALLBACK (test_get_legacy_column_map_cb), NULL);

	view = gal_view_virtual_tree_new ("Test View");

	gal_view_load (view, fixture->state_file_a);
	gal_view_virtual_tree_attach (GAL_VIEW_VIRTUAL_TREE (view), fixture->vtree);

	flush_main_context ();
	wait_milliseconds (200);
	flush_main_context ();

	col_alpha = e_virtual_tree_get_column (fixture->vtree, 0);
	col_beta = e_virtual_tree_get_column (fixture->vtree, 1);
	col_gamma = e_virtual_tree_get_column (fixture->vtree, 2);
	col_delta = e_virtual_tree_get_column (fixture->vtree, 3);
	col_epsilon = e_virtual_tree_get_column (fixture->vtree, 4);

	g_assert_true (gtk_tree_view_column_get_visible (col_alpha));
	g_assert_true (gtk_tree_view_column_get_visible (col_beta));
	g_assert_true (gtk_tree_view_column_get_visible (col_delta));
	g_assert_false (gtk_tree_view_column_get_visible (col_gamma));
	g_assert_false (gtk_tree_view_column_get_visible (col_epsilon));

	g_assert_true (gtk_tree_view_column_get_sort_indicator (col_delta));
	g_assert_cmpint (gtk_tree_view_column_get_sort_order (col_delta), ==, GTK_SORT_DESCENDING);

	gal_view_virtual_tree_detach (GAL_VIEW_VIRTUAL_TREE (view));
	g_object_unref (view);
}

static void
test_column_state_folder_switch (ColumnStateFixture *fixture,
				 gconstpointer user_data)
{
	GtkTreeView *tv = e_virtual_tree_get_tree_view (fixture->vtree);
	GtkTreeViewColumn *col;
	TestVirtualTreeModel *model_b;
	GtkSortType sort_order;
	gchar *order_a, *vis_a, *order_b, *vis_b;
	gchar *order, *vis;
	gboolean sorted;

	/* === Folder A: configure columns and sort === */
	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_a);

	col = gtk_tree_view_get_column (tv, 1); /* Beta */
	gtk_tree_view_column_set_visible (col, FALSE);
	col = gtk_tree_view_get_column (tv, 3); /* Delta */
	gtk_tree_view_column_set_visible (col, FALSE);

	col = gtk_tree_view_get_column (tv, 4); /* Epsilon */
	gtk_tree_view_move_column_after (tv, col, NULL);

	e_virtual_tree_set_column_sort (fixture->vtree, 0, GTK_SORT_DESCENDING, 1);

	order_a = column_order_string (fixture->vtree);
	vis_a = column_visibility_string (fixture->vtree);

	/* === Switch to folder B: unset model, set file B, set new model === */
	e_virtual_tree_set_model (fixture->vtree, NULL);

	model_b = g_object_new (TEST_TYPE_VIRTUAL_TREE_MODEL, NULL);
	build_test_data (model_b);

	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_b);
	e_virtual_tree_set_model (fixture->vtree, E_VIRTUAL_TREE_MODEL (model_b));
	e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (model_b));
	wait_milliseconds (50);

	/* Folder B: different sort, all columns visible, different order */
	e_virtual_tree_clear_sort (fixture->vtree);
	col = gtk_tree_view_get_column (tv, 2); /* Gamma */
	gtk_tree_view_move_column_after (tv, col, NULL);
	e_virtual_tree_set_column_sort (fixture->vtree, 2, GTK_SORT_ASCENDING, 1);
	e_virtual_tree_set_column_sort (fixture->vtree, 3, GTK_SORT_DESCENDING, 2);

	order_b = column_order_string (fixture->vtree);
	vis_b = column_visibility_string (fixture->vtree);

	/* === Switch back to folder A: unset model, restore file A, set model === */
	e_virtual_tree_set_model (fixture->vtree, NULL);

	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_a);
	e_virtual_tree_set_model (fixture->vtree, E_VIRTUAL_TREE_MODEL (fixture->model));
	e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (fixture->model));
	wait_milliseconds (50);

	/* Verify folder A state was restored */
	order = column_order_string (fixture->vtree);
	vis = column_visibility_string (fixture->vtree);
	g_assert_cmpstr (order_a, ==, order);
	g_assert_cmpstr (vis_a, ==, vis);
	g_free (order);
	g_free (vis);

	sorted = e_virtual_tree_get_column_sort (fixture->vtree, 0, &sort_order, NULL);
	g_assert_true (sorted);
	g_assert_cmpint (sort_order, ==, GTK_SORT_DESCENDING);
	g_assert_false (e_virtual_tree_get_column_sort (fixture->vtree, 2, &sort_order, NULL));
	g_assert_false (e_virtual_tree_get_column_sort (fixture->vtree, 3, &sort_order, NULL));

	/* === Switch to folder B again: unset model, restore file B, set model === */
	e_virtual_tree_set_model (fixture->vtree, NULL);

	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_b);
	e_virtual_tree_set_model (fixture->vtree, E_VIRTUAL_TREE_MODEL (model_b));
	e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (model_b));
	wait_milliseconds (50);

	/* Verify folder B state was restored */
	order = column_order_string (fixture->vtree);
	vis = column_visibility_string (fixture->vtree);
	g_assert_cmpstr (order_b, ==, order);
	g_assert_cmpstr (vis_b, ==, vis);
	g_free (order);
	g_free (vis);

	sorted = e_virtual_tree_get_column_sort (fixture->vtree, 2, &sort_order, NULL);
	g_assert_true (sorted);
	g_assert_cmpint (sort_order, ==, GTK_SORT_ASCENDING);
	sorted = e_virtual_tree_get_column_sort (fixture->vtree, 3, &sort_order, NULL);
	g_assert_true (sorted);
	g_assert_cmpint (sort_order, ==, GTK_SORT_DESCENDING);
	g_assert_false (e_virtual_tree_get_column_sort (fixture->vtree, 0, &sort_order, NULL));

	g_object_unref (model_b);
	g_free (order_a);
	g_free (vis_a);
	g_free (order_b);
	g_free (vis_b);
}

static void
test_column_state_multi_sort_priority (ColumnStateFixture *fixture,
					gconstpointer user_data)
{
	GtkSortType sort_order;
	gint priority, alpha_prio;
	gboolean sorted;

	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_a);

	/* Set up three-column multi-sort: Alpha(1), Gamma(2), Delta(3) */
	e_virtual_tree_set_column_sort (fixture->vtree, 0, GTK_SORT_ASCENDING, 1);
	e_virtual_tree_set_column_sort (fixture->vtree, 2, GTK_SORT_DESCENDING, 2);
	e_virtual_tree_set_column_sort (fixture->vtree, 3, GTK_SORT_ASCENDING, 3);

	/* Verify all three are sorted with correct priorities */
	sorted = e_virtual_tree_get_column_sort (fixture->vtree, 0, &sort_order, &priority);
	g_assert_true (sorted);
	g_assert_cmpint (sort_order, ==, GTK_SORT_ASCENDING);
	g_assert_cmpint (priority, ==, 1);

	sorted = e_virtual_tree_get_column_sort (fixture->vtree, 2, &sort_order, &priority);
	g_assert_true (sorted);
	g_assert_cmpint (sort_order, ==, GTK_SORT_DESCENDING);
	g_assert_cmpint (priority, ==, 2);

	sorted = e_virtual_tree_get_column_sort (fixture->vtree, 3, &sort_order, &priority);
	g_assert_true (sorted);
	g_assert_cmpint (sort_order, ==, GTK_SORT_ASCENDING);
	g_assert_cmpint (priority, ==, 3);

	/* Beta and Epsilon should not be sorted */
	g_assert_false (e_virtual_tree_get_column_sort (fixture->vtree, 1, NULL, NULL));
	g_assert_false (e_virtual_tree_get_column_sort (fixture->vtree, 4, NULL, NULL));

	/* Remove the middle sort (Gamma) — creates a priority gap (1, _, 3) */
	e_virtual_tree_set_column_sort (fixture->vtree, 2, GTK_SORT_ASCENDING, 0);

	g_assert_false (e_virtual_tree_get_column_sort (fixture->vtree, 2, NULL, NULL));

	sorted = e_virtual_tree_get_column_sort (fixture->vtree, 0, &sort_order, &priority);
	g_assert_true (sorted);
	g_assert_cmpint (priority, ==, 1);

	sorted = e_virtual_tree_get_column_sort (fixture->vtree, 3, &sort_order, &priority);
	g_assert_true (sorted);
	g_assert_cmpint (priority, ==, 3);

	/* Flush to file and reload — priority gap should survive roundtrip */
	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_b);
	e_virtual_tree_clear_sort (fixture->vtree);
	e_virtual_tree_set_column_state_filename (fixture->vtree, fixture->state_file_a);
	wait_milliseconds (50);

	/* Alpha should still be primary sort */
	sorted = e_virtual_tree_get_column_sort (fixture->vtree, 0, &sort_order, &priority);
	g_assert_true (sorted);
	g_assert_cmpint (sort_order, ==, GTK_SORT_ASCENDING);

	/* Delta should still be sorted (secondary after gap) */
	sorted = e_virtual_tree_get_column_sort (fixture->vtree, 3, &sort_order, &priority);
	g_assert_true (sorted);
	g_assert_cmpint (sort_order, ==, GTK_SORT_ASCENDING);

	/* Gamma should not be sorted */
	g_assert_false (e_virtual_tree_get_column_sort (fixture->vtree, 2, NULL, NULL));

	/* The relative order must be preserved: Alpha before Delta */
	sorted = e_virtual_tree_get_column_sort (fixture->vtree, 0, NULL, &priority);
	g_assert_true (sorted);
	g_assert_cmpint (priority, >, 0);
	alpha_prio = priority;

	sorted = e_virtual_tree_get_column_sort (fixture->vtree, 3, NULL, &priority);
	g_assert_true (sorted);
	g_assert_cmpint (priority, >, alpha_prio);
}

static void
increment_guint_cb (guint *counter)
{
	(*counter)++;
}

static void
test_gal_view_switch_does_not_mark_view_changed (ColumnStateFixture *fixture,
						 gconstpointer user_data)
{
	GtkTreeView *tv = e_virtual_tree_get_tree_view (fixture->vtree);
	GalView *view;
	GKeyFile *key_file;
	gchar *data;
	guint ii, changed_count = 0;

	/* Mimic the "composite"/"composite-to" columns: EXPAND + AUTOSIZE,
	 * so GTK recomputes their pixel widths asynchronously across layout
	 * passes instead of settling synchronously like a fixed width. */
	for (ii = 0; ii < 2; ii++) {
		GtkTreeViewColumn *col = gtk_tree_view_get_column (tv, ii);

		gtk_tree_view_column_set_expand (col, TRUE);
		gtk_tree_view_column_set_sizing (col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	}

	view = gal_view_virtual_tree_new ("Test View");
	gal_view_virtual_tree_attach (GAL_VIEW_VIRTUAL_TREE (view), fixture->vtree);

	flush_main_context ();
	wait_milliseconds (1000);
	flush_main_context ();

	key_file = g_key_file_new ();
	g_key_file_set_boolean (key_file, "Column-alpha", "visible", TRUE);
	g_key_file_set_integer (key_file, "Column-alpha", "width", 300);
	g_key_file_set_boolean (key_file, "Column-beta", "visible", FALSE);
	data = g_key_file_to_data (key_file, NULL, NULL);
	g_file_set_contents (fixture->state_file_a, data, -1, NULL);
	g_free (data);
	g_key_file_unref (key_file);

	key_file = g_key_file_new ();
	g_key_file_set_boolean (key_file, "Column-alpha", "visible", FALSE);
	g_key_file_set_boolean (key_file, "Column-beta", "visible", TRUE);
	g_key_file_set_integer (key_file, "Column-beta", "width", 300);
	data = g_key_file_to_data (key_file, NULL, NULL);
	g_file_set_contents (fixture->state_file_b, data, -1, NULL);
	g_free (data);
	g_key_file_unref (key_file);

	gal_view_load (view, fixture->state_file_a);

	flush_main_context ();
	wait_milliseconds (1000);
	flush_main_context ();

	g_signal_connect_swapped (view, "changed", G_CALLBACK (increment_guint_cb), &changed_count);

	/* Switch to a view showing a different EXPAND/AUTOSIZE column,
	 * matching the real "Messages" -> "Messages To" transition. */
	gal_view_load (view, fixture->state_file_b);

	flush_main_context ();
	wait_milliseconds (1000);
	flush_main_context ();

	g_assert_cmpuint (changed_count, ==, 0);

	gal_view_virtual_tree_detach (GAL_VIEW_VIRTUAL_TREE (view));
	g_object_unref (view);
}

static void
test_gal_view_group_via_key_file_marks_view_changed (ColumnStateFixture *fixture,
						     gconstpointer user_data)
{
	GalView *view;
	GKeyFile *key_file;
	guint changed_count = 0;

	view = gal_view_virtual_tree_new ("Test View");
	gal_view_virtual_tree_attach (GAL_VIEW_VIRTUAL_TREE (view), fixture->vtree);

	flush_main_context ();
	wait_milliseconds (1000);
	flush_main_context ();

	g_signal_connect_swapped (view, "changed", G_CALLBACK (increment_guint_cb), &changed_count);

	key_file = g_key_file_new ();
	g_key_file_set_boolean (key_file, "Column-alpha", "visible", TRUE);
	g_key_file_set_integer (key_file, "Column-alpha", "group-order", 0);
	g_key_file_set_string (key_file, "Column-alpha", "group-dir", "ascending");
	g_key_file_set_boolean (key_file, "Column-beta", "visible", TRUE);

	e_virtual_tree_load_column_state_from_key_file (fixture->vtree, key_file);
	g_key_file_unref (key_file);

	flush_main_context ();
	wait_milliseconds (1000);
	flush_main_context ();

	g_assert_cmpuint (changed_count, >=, 1);

	gal_view_virtual_tree_detach (GAL_VIEW_VIRTUAL_TREE (view));
	g_object_unref (view);
}

static EVirtualTree *
create_memo_like_tree (TestVirtualTreeModel *model)
{
	EVirtualTree *vtree;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *tvc;
	guint col_summary, col_icon, col_categories, col_color;

	vtree = E_VIRTUAL_TREE (e_virtual_tree_new (E_VIRTUAL_TREE_MODEL (model)));

	col_summary = e_virtual_tree_add_column (vtree, "summary", "Summary");
	renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (vtree, col_summary, 0, renderer, TRUE,
		label_data_func_cs, NULL, NULL);
	e_virtual_tree_set_column_min_width (vtree, col_summary, 40);

	col_icon = e_virtual_tree_add_column (vtree, "icon", "Type");
	renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (vtree, col_icon, 0, renderer, TRUE,
		label_data_func_cs, NULL, NULL);
	tvc = e_virtual_tree_get_column (vtree, col_icon);
	gtk_tree_view_column_set_resizable (tvc, FALSE);

	col_categories = e_virtual_tree_add_column (vtree, "categories", "Categories");
	renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (vtree, col_categories, 0, renderer, TRUE,
		label_data_func_cs, NULL, NULL);
	e_virtual_tree_set_column_min_width (vtree, col_categories, 40);

	col_color = e_virtual_tree_add_column (vtree, "memolist-color", "Memo List");
	renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (vtree, col_color, 0, renderer, TRUE,
		label_data_func_cs, NULL, NULL);
	tvc = e_virtual_tree_get_column (vtree, col_color);
	gtk_tree_view_column_set_resizable (tvc, FALSE);

	gtk_tree_view_move_column_after (e_virtual_tree_get_tree_view (vtree),
		e_virtual_tree_get_column (vtree, col_color), NULL);

	return vtree;
}

static void
test_gal_view_memo_like_resize_marks_view_changed (ColumnStateFixture *fixture,
						    gconstpointer user_data)
{
	GtkTreeViewColumn *tvc;
	GalView *view;
	guint changed_count = 0;
	gint new_width;

	gtk_container_remove (GTK_CONTAINER (fixture->window), GTK_WIDGET (fixture->vtree));
	fixture->vtree = create_memo_like_tree (fixture->model);
	gtk_container_add (GTK_CONTAINER (fixture->window), GTK_WIDGET (fixture->vtree));
	e_virtual_tree_model_emit_row_count_changed (E_VIRTUAL_TREE_MODEL (fixture->model));
	gtk_widget_show_all (fixture->window);
	wait_milliseconds (100);
	flush_main_context ();

	view = gal_view_virtual_tree_new ("Test View");
	gal_view_virtual_tree_attach (GAL_VIEW_VIRTUAL_TREE (view), fixture->vtree);

	flush_main_context ();
	wait_milliseconds (1000);
	flush_main_context ();

	g_signal_connect_swapped (view, "changed", G_CALLBACK (increment_guint_cb), &changed_count);

	tvc = e_virtual_tree_get_column (fixture->vtree, 0);
	new_width = gtk_tree_view_column_get_width (tvc) + 40;
	gtk_tree_view_column_set_sizing (tvc, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width (tvc, new_width);

	flush_main_context ();
	wait_milliseconds (1000);
	flush_main_context ();

	g_assert_cmpuint (changed_count, >=, 1);

	gal_view_virtual_tree_detach (GAL_VIEW_VIRTUAL_TREE (view));
	g_object_unref (view);
}

static void
on_display_view_reattach_cb (GalViewInstance *instance,
			     GalView *gal_view,
			     gpointer user_data)
{
	EVirtualTree *vtree = user_data;

	if (GAL_IS_VIEW_VIRTUAL_TREE (gal_view))
		gal_view_virtual_tree_attach (GAL_VIEW_VIRTUAL_TREE (gal_view), vtree);
}

static void
test_gal_view_switch_back_to_system_only_view_reapplies_state (ColumnStateFixture *fixture,
								gconstpointer user_data)
{
	GalViewCollection *collection;
	GalViewInstance *instance;
	GtkTreeViewColumn *gamma_col;

	gchar *sys_dir, *user_dir, *filename;

	sys_dir = g_dir_make_tmp ("vtree-test-sys-XXXXXX", NULL);
	user_dir = g_dir_make_tmp ("vtree-test-user-XXXXXX", NULL);

	filename = g_build_filename (sys_dir, "galview.xml", NULL);
	g_file_set_contents (filename,
		"<?xml version=\"1.0\"?>\n"
		"<GalViewCollection default-view=\"TestView\">\n"
		"<GalView id=\"TestView\" title=\"Test View\" filename=\"TestView.galview\" type=\"vtree\"/>\n"
		"</GalViewCollection>\n", -1, NULL);
	g_free (filename);

	filename = g_build_filename (sys_dir, "TestView.galview", NULL);
	g_file_set_contents (filename,
		"[Column-alpha]\nvisible=true\nwidth=222\n\n"
		"[Column-beta]\nvisible=false\n", -1, NULL);
	g_free (filename);

	g_type_ensure (GAL_TYPE_VIEW_VIRTUAL_TREE);

	collection = gal_view_collection_new (sys_dir, user_dir);

	instance = gal_view_instance_new (collection, "test-instance");
	g_signal_connect (instance, "display-view", G_CALLBACK (on_display_view_reattach_cb), fixture->vtree);

	gal_view_instance_load (instance);

	flush_main_context ();
	wait_milliseconds (1000);
	flush_main_context ();

	g_assert_cmpstr (column_visibility_string (fixture->vtree), ==,
		"Alpha:1,Beta:0,Gamma:0,Delta:0,Epsilon:0");

	/* Simulate a user customization: make Gamma visible, which should
	 * clear current_id (mark the view "Custom"). */
	gamma_col = e_virtual_tree_get_column (fixture->vtree, 2);
	gtk_tree_view_column_set_visible (gamma_col, TRUE);

	flush_main_context ();
	wait_milliseconds (1000);
	flush_main_context ();

	g_assert_cmpstr (column_visibility_string (fixture->vtree), ==,
		"Alpha:1,Beta:0,Gamma:1,Delta:0,Epsilon:0");
	g_assert_null (gal_view_instance_get_current_view_id (instance));

	/* Simulate re-selecting "TestView" from the View menu, exactly like
	 * mail_shell_view_notify_view_id_cb / memo_shell_view_notify_view_id_cb do. */
	gal_view_instance_set_current_view_id (instance, "TestView");

	flush_main_context ();
	wait_milliseconds (1000);
	flush_main_context ();

	g_assert_cmpstr (column_visibility_string (fixture->vtree), ==,
		"Alpha:1,Beta:0,Gamma:0,Delta:0,Epsilon:0");

	g_object_unref (instance);
	g_object_unref (collection);

	g_free (sys_dir);
	g_free (user_dir);
}

static void
test_gal_view_apply_legacy_etable (ColumnStateFixture *fixture,
				   gconstpointer user_data)
{
	static const gchar *legacy_xml =
		"<?xml version=\"1.0\"?>"
		"<ETableState state-version=\"0.1\">"
		"  <column source=\"0\" expansion=\"1.0\"/>"
		"  <column source=\"3\" expansion=\"1.0\"/>"
		"  <column source=\"1\" expansion=\"0.5\"/>"
		"  <grouping>"
		"    <leaf column=\"3\" ascending=\"false\"/>"
		"  </grouping>"
		"</ETableState>";
	GalViewCollection *collection;
	GalView *legacy_view;
	GtkTreeViewColumn *col_alpha, *col_beta, *col_gamma, *col_delta;

	g_file_set_contents (fixture->state_file_a, legacy_xml, -1, NULL);

	g_signal_connect (fixture->vtree, "get-legacy-etable-column-map",
		G_CALLBACK (test_get_legacy_column_map_cb), NULL);

	g_type_ensure (GAL_TYPE_VIEW_VIRTUAL_TREE);

	collection = gal_view_collection_new (g_get_tmp_dir (), g_get_tmp_dir ());

	/* Old saved views can still declare type="etable"; the collection
	 * resolves that straight to a GalViewVirtualTree. */
	legacy_view = gal_view_collection_load_view_from_file (collection, "etable", fixture->state_file_a);
	g_assert_nonnull (legacy_view);
	g_assert_true (GAL_IS_VIEW_VIRTUAL_TREE (legacy_view));

	gal_view_virtual_tree_attach (GAL_VIEW_VIRTUAL_TREE (legacy_view), fixture->vtree);

	col_alpha = e_virtual_tree_get_column (fixture->vtree, 0);
	col_beta = e_virtual_tree_get_column (fixture->vtree, 1);
	col_gamma = e_virtual_tree_get_column (fixture->vtree, 2);
	col_delta = e_virtual_tree_get_column (fixture->vtree, 3);

	g_assert_true (gtk_tree_view_column_get_visible (col_alpha));
	g_assert_true (gtk_tree_view_column_get_visible (col_beta));
	g_assert_true (gtk_tree_view_column_get_visible (col_delta));
	g_assert_false (gtk_tree_view_column_get_visible (col_gamma));

	g_assert_true (gtk_tree_view_column_get_sort_indicator (col_delta));
	g_assert_cmpint (gtk_tree_view_column_get_sort_order (col_delta), ==, GTK_SORT_DESCENDING);

	gal_view_virtual_tree_detach (GAL_VIEW_VIRTUAL_TREE (legacy_view));
	g_object_unref (legacy_view);
	g_object_unref (collection);
}

static void
test_a11y_basics (TestFixture *fixture)
{
	AtkObject *atk_obj;
	AtkStateSet *states;

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (fixture->vtree));
	g_assert_nonnull (atk_obj);

	g_assert_true (ATK_IS_TABLE (atk_obj));
	g_assert_true (ATK_IS_SELECTION (atk_obj));

	g_assert_cmpint (atk_object_get_role (atk_obj), ==, ATK_ROLE_TREE_TABLE);

	states = atk_object_ref_state_set (atk_obj);
	g_assert_true (atk_state_set_contains_state (states, ATK_STATE_MANAGES_DESCENDANTS));
	g_object_unref (states);

	g_assert_cmpint (atk_object_get_n_accessible_children (atk_obj), ==, 0);
}

static void
test_a11y_table_row_count (TestFixture *fixture)
{
	AtkObject *atk_obj;
	AtkTable *table;
	GNode *a_node;
	TestNode *a_tn;

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (fixture->vtree));
	table = ATK_TABLE (atk_obj);

	g_assert_cmpint (atk_table_get_n_rows (table), ==, 9);

	a_node = find_node_by_label (fixture->model->root, "A");
	g_assert_nonnull (a_node);
	a_tn = a_node->data;

	e_virtual_tree_model_set_expanded (
		E_VIRTUAL_TREE_MODEL (fixture->model),
		G_OBJECT (a_tn), FALSE);
	wait_milliseconds (event_processing_delay_ms);

	g_assert_cmpint (atk_table_get_n_rows (table), ==, 5);

	e_virtual_tree_model_set_expanded (
		E_VIRTUAL_TREE_MODEL (fixture->model),
		G_OBJECT (a_tn), TRUE);
	wait_milliseconds (event_processing_delay_ms);

	g_assert_cmpint (atk_table_get_n_rows (table), ==, 9);
}

static void
test_a11y_table_column_count (TestFixture *fixture)
{
	AtkObject *atk_obj;
	AtkTable *table;

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (fixture->vtree));
	table = ATK_TABLE (atk_obj);

	g_assert_cmpint (atk_table_get_n_columns (table), ==, 1);
}

static void
test_a11y_table_column_description (TestFixture *fixture)
{
	AtkObject *atk_obj;
	AtkTable *table;
	const gchar *desc;

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (fixture->vtree));
	table = ATK_TABLE (atk_obj);

	desc = atk_table_get_column_description (table, 0);
	g_assert_cmpstr (desc, ==, "Label");
}

static void
test_a11y_table_ref_at (TestFixture *fixture)
{
	AtkObject *atk_obj, *cell;
	AtkTable *table;
	const gchar *expected_labels[] = {
		"A", "A1", "A1a", "A1b", "A2", "B", "B1", "B1a", "C"
	};
	guint ii;

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (fixture->vtree));
	table = ATK_TABLE (atk_obj);

	for (ii = 0; ii < G_N_ELEMENTS (expected_labels); ii++) {
		cell = atk_table_ref_at (table, (gint) ii, 0);
		g_assert_nonnull (cell);
		g_assert_cmpint (atk_object_get_role (cell), ==, ATK_ROLE_TABLE_CELL);
		g_assert_cmpstr (atk_object_get_name (cell), ==, expected_labels[ii]);
		g_object_unref (cell);
	}
}

static void
test_a11y_table_index_conversion (TestFixture *fixture)
{
	AtkObject *atk_obj;
	AtkTable *table;
	gint idx;

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (fixture->vtree));
	table = ATK_TABLE (atk_obj);

	idx = atk_table_get_index_at (table, 3, 0);
	g_assert_cmpint (idx, ==, 3);
	g_assert_cmpint (atk_table_get_row_at_index (table, idx), ==, 3);
	g_assert_cmpint (atk_table_get_column_at_index (table, idx), ==, 0);
}

static void
test_a11y_table_is_row_selected (TestFixture *fixture)
{
	AtkObject *atk_obj;
	AtkTable *table;

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (fixture->vtree));
	table = ATK_TABLE (atk_obj);

	g_assert_false (atk_table_is_row_selected (table, 2));

	e_virtual_tree_select_row (fixture->vtree, 2);
	wait_milliseconds (event_processing_delay_ms);

	g_assert_true (atk_table_is_row_selected (table, 2));
	g_assert_false (atk_table_is_row_selected (table, 3));
}

static void
test_a11y_selection_add_remove (TestFixture *fixture)
{
	AtkObject *atk_obj;
	AtkTable *table;

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (fixture->vtree));
	table = ATK_TABLE (atk_obj);

	g_assert_true (atk_table_add_row_selection (table, 4));
	wait_milliseconds (event_processing_delay_ms);
	g_assert_true (e_virtual_tree_row_is_selected (fixture->vtree, 4));

	g_assert_true (atk_table_remove_row_selection (table, 4));
	wait_milliseconds (event_processing_delay_ms);
	g_assert_false (e_virtual_tree_row_is_selected (fixture->vtree, 4));
}

static void
test_a11y_selection_count (TestFixture *fixture)
{
	AtkObject *atk_obj;
	AtkSelection *sel;

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (fixture->vtree));
	sel = ATK_SELECTION (atk_obj);

	e_virtual_tree_unselect_all (fixture->vtree);
	wait_milliseconds (event_processing_delay_ms);
	g_assert_cmpint (atk_selection_get_selection_count (sel), ==, 0);

	e_virtual_tree_set_selection_mode (fixture->vtree, GTK_SELECTION_MULTIPLE);
	e_virtual_tree_select_row (fixture->vtree, 1);
	e_virtual_tree_select_row (fixture->vtree, 3);
	wait_milliseconds (event_processing_delay_ms);

	g_assert_cmpint (atk_selection_get_selection_count (sel), ==, 2);
}

static void
test_a11y_selection_clear (TestFixture *fixture)
{
	AtkObject *atk_obj;
	AtkSelection *sel;

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (fixture->vtree));
	sel = ATK_SELECTION (atk_obj);

	e_virtual_tree_set_selection_mode (fixture->vtree, GTK_SELECTION_MULTIPLE);
	e_virtual_tree_select_row (fixture->vtree, 0);
	e_virtual_tree_select_row (fixture->vtree, 2);
	e_virtual_tree_select_row (fixture->vtree, 5);
	wait_milliseconds (event_processing_delay_ms);

	g_assert_cmpint (atk_selection_get_selection_count (sel), >, 0);

	atk_selection_clear_selection (sel);
	wait_milliseconds (event_processing_delay_ms);

	g_assert_cmpint (atk_selection_get_selection_count (sel), ==, 0);
}

static void
test_a11y_cell_hierarchy_states (TestFixture *fixture)
{
	AtkObject *atk_obj, *cell;
	AtkTable *table;
	AtkStateSet *states;

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (fixture->vtree));
	table = ATK_TABLE (atk_obj);

	cell = atk_table_ref_at (table, 0, 0);
	g_assert_nonnull (cell);
	states = atk_object_ref_state_set (cell);
	g_assert_true (atk_state_set_contains_state (states, ATK_STATE_EXPANDABLE));
	g_assert_true (atk_state_set_contains_state (states, ATK_STATE_EXPANDED));
	g_object_unref (states);
	g_object_unref (cell);

	cell = atk_table_ref_at (table, 2, 0);
	g_assert_nonnull (cell);
	states = atk_object_ref_state_set (cell);
	g_assert_false (atk_state_set_contains_state (states, ATK_STATE_EXPANDABLE));
	g_object_unref (states);
	g_object_unref (cell);

	e_virtual_tree_select_row (fixture->vtree, 0);
	wait_milliseconds (event_processing_delay_ms);

	cell = atk_table_ref_at (table, 0, 0);
	g_assert_nonnull (cell);
	states = atk_object_ref_state_set (cell);
	g_assert_true (atk_state_set_contains_state (states, ATK_STATE_SELECTED));
	g_object_unref (states);
	g_object_unref (cell);

	cell = atk_table_ref_at (table, 2, 0);
	g_assert_nonnull (cell);
	states = atk_object_ref_state_set (cell);
	g_assert_false (atk_state_set_contains_state (states, ATK_STATE_SELECTED));
	g_object_unref (states);
	g_object_unref (cell);
}

static void
prefix_data_func (EVirtualTree *tree,
		  GtkCellRenderer *renderer,
		  GObject *row_object,
		  guint visible_row,
		  gpointer user_data)
{
	TestNode *node = TEST_NODE (row_object);
	const gchar *prefix = user_data;
	gchar *text;

	text = g_strconcat (prefix, node->label, NULL);
	g_object_set (renderer, "text", text, NULL);
	g_free (text);
}

static void
test_a11y_cell_text_rtl (TestFixture *fixture)
{
	EVirtualTree *vtree;
	GtkCellRenderer *renderer;
	guint col;
	gchar *text;

	vtree = E_VIRTUAL_TREE (
		e_virtual_tree_new (E_VIRTUAL_TREE_MODEL (fixture->model)));

	col = e_virtual_tree_add_column (vtree, "composite", "Composite");
	renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (vtree, col, 0, renderer, TRUE,
		prefix_data_func, (gpointer) "X:", NULL);
	renderer = gtk_cell_renderer_text_new ();
	e_virtual_tree_column_pack_start (vtree, col, 0, renderer, FALSE,
		prefix_data_func, (gpointer) "Y:", NULL);
	e_virtual_tree_set_expander_column (vtree, col, 0);

	gtk_widget_set_direction (GTK_WIDGET (vtree), GTK_TEXT_DIR_LTR);

	text = _e_virtual_tree_get_cell_text (vtree, 0, 0);
	g_assert_cmpstr (text, ==, "X:A Y:A");
	g_free (text);

	gtk_widget_set_direction (GTK_WIDGET (vtree), GTK_TEXT_DIR_RTL);

	text = _e_virtual_tree_get_cell_text (vtree, 0, 0);
	g_assert_cmpstr (text, ==, "Y:A X:A");
	g_free (text);

	gtk_widget_set_direction (GTK_WIDGET (vtree), GTK_TEXT_DIR_LTR);

	g_object_ref_sink (vtree);
	g_object_unref (vtree);
}

gint
main (gint argc,
      gchar *argv[])
{
	gint cmd_delay = -1;
	gboolean background = FALSE;
	gboolean streaming = FALSE;
	GOptionEntry entries[] = {
		{ "cmd-delay", '\0', 0,
		  G_OPTION_ARG_INT, &cmd_delay,
		  "Specify delay, in milliseconds, to use during processing"
		  " commands. Default is 25 ms.",
		  NULL },
		{ "background", '\0', 0,
		  G_OPTION_ARG_NONE, &background,
		  "Use to run tests in the background.",
		  NULL },
		{ "streaming", '\0', 0,
		  G_OPTION_ARG_NONE, &streaming,
		  "Interactive stdin/stdout mode for external drivers.",
		  NULL },
		{ NULL }
	};
	GOptionContext *context;
	GError *error = NULL;

	g_test_init (&argc, &argv, NULL);
	gtk_init (&argc, &argv);

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_warning ("Failed to parse arguments: %s",
			error ? error->message : "Unknown error");
		g_option_context_free (context);
		g_clear_error (&error);
		return 1;
	}
	g_option_context_free (context);

	if (cmd_delay > 0)
		event_processing_delay_ms = (guint) cmd_delay;
	in_background = background;

	if (streaming) {
		run_streaming_mode ();
		return 0;
	}

	add_test ("/cursor/set-get", test_cursor_set_get);
	add_test ("/cursor/arrow-down", test_cursor_arrow_down);
	add_test ("/cursor/arrow-up", test_cursor_arrow_up);
	add_test ("/cursor/home-end", test_cursor_home_end);
	add_test ("/selection/single-arrow", test_selection_single_arrow);
	add_test ("/selection/multiple-shift-down", test_selection_multiple_shift_down);
	add_test ("/selection/multiple-ctrl-down", test_selection_multiple_ctrl_down);
	add_test ("/selection/multiple-ctrl-space", test_selection_multiple_ctrl_space);
	add_test ("/selection/multiple-ctrl-click", test_selection_multiple_ctrl_click);
	add_test ("/tree/add-remove", test_tree_add_remove);
	add_test ("/signals/cursor-changed", test_signals_cursor_changed);
	add_test ("/signals/selection-changed", test_signals_selection_changed);
	add_test ("/signals/no-redundant-on-reclick", test_signals_no_redundant_on_reclick);
	add_test ("/signals/on-remove-cursored", test_signals_on_remove_cursored);
	add_test ("/signals/on-remove-multi-select", test_signals_on_remove_multi_select);
	add_test ("/cell-click/hit-test-accounts-for-expander-indent", test_cell_click_hit_test_accounts_for_expander_indent);
	add_test ("/cell-click/expander-indent-not-hit-for-leaf-row", test_cell_click_expander_indent_not_hit_for_leaf_row);
	add_test_with_data ("/signals/on-remove-cursored-offscreen", test_signals_on_remove_cursored_offscreen, build_large_test_data);
	add_test_with_data ("/tree/scroll-stability-single", test_scroll_stability_single, build_large_test_data);
	add_test_with_data ("/tree/scroll-stability-multiple", test_scroll_stability_multiple, build_large_test_data);
	add_test_with_data ("/tree/cursor-follows-resort", test_cursor_follows_resort, build_large_test_data);
	add_test_with_data ("/tree/scroll-stability-edges", test_scroll_stability_edges, build_large_test_data);
	add_test_with_data ("/tree/scroll-stability-flat-insert", test_scroll_stability_flat_insert, build_large_test_data);
	add_test_with_data ("/tree/scroll-stability-expand-after-collapse-hides-cursor", test_scroll_stability_expand_after_collapse_hides_cursor, build_large_test_data);
	add_test_with_data ("/selection/across-pages", test_selection_across_pages, build_large_test_data);
	add_test_with_data ("/tree/scroll-partial-row", test_scroll_partial_row, build_large_test_data);
	add_test ("/selection/restore-after-rebuild", test_selection_restore_after_rebuild);
	add_test ("/selection/restore-partial", test_selection_restore_partial);
	add_test ("/selection/restore-no-match", test_selection_restore_no_match);
	add_test ("/selection/restore-same-count", test_selection_restore_same_count);
	add_test_with_data ("/cursor/preserved-on-focus", test_cursor_preserved_on_focus, build_large_test_data);
	add_test_with_data ("/cursor/no-shift-on-resize-when-visible", test_cursor_no_shift_on_resize_when_visible, build_large_test_data);
	add_test ("/cursor/get-key", test_cursor_get_key);
	add_test ("/cursor/get-object", test_cursor_get_object);
	add_test ("/selection/count", test_selection_count);
	add_test ("/selection/count-inverted", test_selection_count_inverted);

	g_test_add ("/column-state/save-load-roundtrip", ColumnStateFixture, NULL,
		cs_fixture_set_up, test_column_state_save_load_roundtrip, cs_fixture_tear_down);
	g_test_add ("/column-state/switch-views", ColumnStateFixture, NULL,
		cs_fixture_set_up, test_column_state_switch_views, cs_fixture_tear_down);
	g_test_add ("/column-state/hidden-reordered", ColumnStateFixture, NULL,
		cs_fixture_set_up, test_column_state_hidden_reordered, cs_fixture_tear_down);
	g_test_add ("/column-state/no-file", ColumnStateFixture, NULL,
		cs_fixture_set_up, test_column_state_no_file, cs_fixture_tear_down);
	g_test_add ("/column-state/folder-switch", ColumnStateFixture, NULL,
		cs_fixture_set_up, test_column_state_folder_switch, cs_fixture_tear_down);
	g_test_add ("/column-state/legacy-etable-migration", ColumnStateFixture, NULL,
		cs_fixture_set_up, test_column_state_legacy_etable_migration, cs_fixture_tear_down);
	g_test_add ("/column-state/legacy-etable-not-recognized", ColumnStateFixture, NULL,
		cs_fixture_set_up, test_column_state_legacy_etable_not_recognized, cs_fixture_tear_down);
	g_test_add ("/gal-view/legacy-etable-deferred-attach", ColumnStateFixture, NULL,
		cs_fixture_set_up, test_gal_view_legacy_etable_deferred_attach, cs_fixture_tear_down);
	g_test_add ("/column-state/multi-sort-priority", ColumnStateFixture, NULL,
		cs_fixture_set_up, test_column_state_multi_sort_priority, cs_fixture_tear_down);
	g_test_add ("/gal-view/switch-does-not-mark-view-changed", ColumnStateFixture, NULL,
		cs_fixture_set_up, test_gal_view_switch_does_not_mark_view_changed, cs_fixture_tear_down);
	g_test_add ("/gal-view/group-via-key-file-marks-view-changed", ColumnStateFixture, NULL,
		cs_fixture_set_up, test_gal_view_group_via_key_file_marks_view_changed, cs_fixture_tear_down);
	g_test_add ("/gal-view/memo-like-resize-marks-view-changed", ColumnStateFixture, NULL,
		cs_fixture_set_up, test_gal_view_memo_like_resize_marks_view_changed, cs_fixture_tear_down);
	g_test_add ("/gal-view/switch-back-to-system-only-view-reapplies-state", ColumnStateFixture, NULL,
		cs_fixture_set_up, test_gal_view_switch_back_to_system_only_view_reapplies_state, cs_fixture_tear_down);
	g_test_add ("/gal-view/apply-legacy-etable", ColumnStateFixture, NULL,
		cs_fixture_set_up, test_gal_view_apply_legacy_etable, cs_fixture_tear_down);

	add_test ("/a11y/basics", test_a11y_basics);
	add_test ("/a11y/table-row-count", test_a11y_table_row_count);
	add_test ("/a11y/table-column-count", test_a11y_table_column_count);
	add_test ("/a11y/table-column-description", test_a11y_table_column_description);
	add_test ("/a11y/table-ref-at", test_a11y_table_ref_at);
	add_test ("/a11y/table-index-conversion", test_a11y_table_index_conversion);
	add_test ("/a11y/table-is-row-selected", test_a11y_table_is_row_selected);
	add_test ("/a11y/selection-add-remove", test_a11y_selection_add_remove);
	add_test ("/a11y/selection-count", test_a11y_selection_count);
	add_test ("/a11y/selection-clear", test_a11y_selection_clear);
	add_test ("/a11y/cell-hierarchy-states", test_a11y_cell_hierarchy_states);
	add_test ("/a11y/cell-text-rtl", test_a11y_cell_text_rtl);

	return g_test_run ();
}
