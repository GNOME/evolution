/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-tree-sorted.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Chris Toshok <toshok@ximian.com>
 *
 * Adapted from the gtree code and ETableModel.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/* FIXME: Overall e-tree-sorted.c needs to be made more efficient. */

#include <config.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "gal/util/e-util.h"
#include "gal/util/e-xml-utils.h"

#include "e-table-sorting-utils.h"
#include "e-tree-sorted.h"

/* maximum insertions between an idle event that we will do without scheduling an idle sort */
#define ETS_INSERT_MAX (4)

#define TREEPATH_CHUNK_AREA_SIZE (30 * sizeof (ETreeSortedPath))

#define d(x)

static ETreeModel *parent_class;
static GMemChunk  *node_chunk;

enum {
	NODE_RESORTED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = {0, };

typedef struct ETreeSortedPath ETreeSortedPath;

struct ETreeSortedPath {
	ETreePath         corresponding;

	/* parent/child/sibling pointers */
	ETreeSortedPath  *parent;
	gint              num_children;
	ETreeSortedPath **children;
	int               position;
	int               orig_position;

	guint             needs_resort : 1;
	guint             child_needs_resort : 1;
	guint             resort_all_children : 1;
	guint             needs_regen_to_sort : 1;
};

struct ETreeSortedPriv {
	ETreeModel      *source;
	ETreeSortedPath *root;

	ETableSortInfo   *sort_info;
	ETableHeader     *full_header;

	ETreeSortedPath  *last_access;

	int          tree_model_pre_change_id;
	int          tree_model_no_change_id;
	int          tree_model_node_changed_id;
	int          tree_model_node_data_changed_id;
	int          tree_model_node_col_changed_id;
	int          tree_model_node_inserted_id;
	int          tree_model_node_removed_id;
	int          tree_model_node_deleted_id;
	int          tree_model_node_request_collapse_id;

	int          sort_info_changed_id;
	int          sort_idle_id;
	int          insert_idle_id;
	int          insert_count;

	guint        in_resort_idle : 1;
	guint        nested_resort_idle : 1;
};

enum {
	ARG_0,

	ARG_SORT_INFO
};

static void ets_sort_info_changed (ETableSortInfo *sort_info, ETreeSorted *ets);
static void resort_node (ETreeSorted *ets, ETreeSortedPath *path, gboolean resort_all_children, gboolean needs_regen, gboolean send_signals);
static void mark_path_needs_resort (ETreeSorted *ets, ETreeSortedPath *path, gboolean needs_rebuild, gboolean resort_all_children);
static void schedule_resort (ETreeSorted *ets, ETreeSortedPath *path, gboolean needs_regen, gboolean resort_all_children);
static void free_path (ETreeSortedPath *path);
static void generate_children(ETreeSorted *ets, ETreeSortedPath *path);
static void regenerate_children(ETreeSorted *ets, ETreeSortedPath *path);



/* idle callbacks */

static gboolean
ets_sort_idle(gpointer user_data)
{
	ETreeSorted *ets = user_data;
	if (ets->priv->in_resort_idle) {
		ets->priv->nested_resort_idle = TRUE;
		return FALSE;
	}
	ets->priv->in_resort_idle = TRUE;
	if (ets->priv->root) {
		do {
			ets->priv->nested_resort_idle = FALSE;
			resort_node (ets, ets->priv->root, FALSE, FALSE, TRUE);
		} while (ets->priv->nested_resort_idle);
	}
	ets->priv->in_resort_idle = FALSE;
	ets->priv->sort_idle_id = 0;
	return FALSE;
}

#define ETS_SORT_IDLE_ACTIVATED(ets) ((ets)->priv->sort_idle_id != 0)

inline static void
ets_stop_sort_idle (ETreeSorted *ets)
{
	if (ets->priv->sort_idle_id) {
		g_source_remove(ets->priv->sort_idle_id);
		ets->priv->sort_idle_id = 0;
	}
}

static gboolean
ets_insert_idle(ETreeSorted *ets)
{
	ets->priv->insert_count = 0;
	ets->priv->insert_idle_id = 0;
	return FALSE;
}



/* Helper functions */

#define CHECK_AROUND_LAST_ACCESS

static inline ETreeSortedPath *
check_last_access (ETreeSorted *ets, ETreePath corresponding)
{
#ifdef CHECK_AROUND_LAST_ACCESS
	ETreeSortedPath *parent;
#endif

	if (ets->priv->last_access == NULL)
		return NULL;

	if (ets->priv->last_access == corresponding) {
		d(g_print("Found last access %p at %p.", ets->priv->last_access, ets->priv->last_access));
		return ets->priv->last_access;
	}

#ifdef CHECK_AROUND_LAST_ACCESS
	parent = ets->priv->last_access->parent;
	if (parent && parent->children) {
		int position = ets->priv->last_access->position;
		int end = MIN(parent->num_children, position + 10);
		int start = MAX(0, position - 10);
		int initial = MAX (MIN (position, end), start);
		int i;

		for (i = initial; i < end; i++) {
			if (parent->children[i] && parent->children[i]->corresponding == corresponding) {
				d(g_print("Found last access %p at %p.", ets->priv->last_access, parent->children[i]));
				return parent->children[i];
			}
		}

		for (i = initial - 1; i >= start; i--) {
			if (parent->children[i] && parent->children[i]->corresponding == corresponding) {
				d(g_print("Found last access %p at %p.", ets->priv->last_access, parent->children[i]));
				return parent->children[i];
			}
		}
	}
#endif
	return NULL;
}

static ETreeSortedPath *
find_path(ETreeSorted *ets, ETreePath corresponding)
{
	int depth;
	ETreePath *sequence;
	int i;
	ETreeSortedPath *path;
	ETreeSortedPath *check_last;

	if (corresponding == NULL)
		return NULL;

	check_last = check_last_access (ets, corresponding);
	if (check_last) {
		d(g_print(" (find_path)\n"));
		return check_last;
	}

	depth = e_tree_model_node_depth(ets->priv->source, corresponding);

	sequence = g_new(ETreePath, depth + 1);

	sequence[0] = corresponding;

	for (i = 0; i < depth; i++)
		sequence[i + 1] = e_tree_model_node_get_parent(ets->priv->source, sequence[i]);

	path = ets->priv->root;

	for (i = depth - 1; i >= 0 && path != NULL; i --) {
		int j;

		if (path->num_children == -1) {
			path = NULL;
			break;
		}

		for (j = 0; j < path->num_children; j++) {
			if (path->children[j]->corresponding == sequence[i]) {
				break;
			}
		}

		if (j < path->num_children) {
			path = path->children[j];
		} else {
			path = NULL;
		}
	}
	g_free (sequence);

	d(g_print("Didn't find last access %p.  Setting to %p. (find_path)\n", ets->priv->last_access, path));
	ets->priv->last_access = path;

	return path;
}

static ETreeSortedPath *
find_child_path(ETreeSorted *ets, ETreeSortedPath *parent, ETreePath corresponding)
{
	int i;

	if (corresponding == NULL)
		return NULL;

	if (parent->num_children == -1) {
		return NULL;
	}

	for (i = 0; i < parent->num_children; i++)
		if (parent->children[i]->corresponding == corresponding)
			return parent->children[i];

	return NULL;
}

static ETreeSortedPath *
find_or_create_path(ETreeSorted *ets, ETreePath corresponding)
{
	int depth;
	ETreePath *sequence;
	int i;
	ETreeSortedPath *path;
	ETreeSortedPath *check_last;

	if (corresponding == NULL)
		return NULL;

	check_last = check_last_access (ets, corresponding);
	if (check_last) {
		d(g_print(" (find_or_create_path)\n"));
		return check_last;
	}

	depth = e_tree_model_node_depth(ets->priv->source, corresponding);

	sequence = g_new(ETreePath, depth + 1);

	sequence[0] = corresponding;

	for (i = 0; i < depth; i++)
		sequence[i + 1] = e_tree_model_node_get_parent(ets->priv->source, sequence[i]);

	path = ets->priv->root;

	for (i = depth - 1; i >= 0 && path != NULL; i --) {
		int j;

		if (path->num_children == -1) {
			generate_children(ets, path);
		}

		for (j = 0; j < path->num_children; j++) {
			if (path->children[j]->corresponding == sequence[i]) {
				break;
			}
		}

		if (j < path->num_children) {
			path = path->children[j];
		} else {
			path = NULL;
		}
	}
	g_free (sequence);

	d(g_print("Didn't find last access %p.  Setting to %p. (find_or_create_path)\n", ets->priv->last_access, path));
	ets->priv->last_access = path;

	return path;
}

static void
free_children (ETreeSortedPath *path)
{
	int i;

	if (path == NULL)
		return;

	for (i = 0; i < path->num_children; i++) {
		free_path(path->children[i]);
	}

	g_free(path->children);
	path->children = NULL;
	path->num_children = -1;
}

static void
free_path (ETreeSortedPath *path)
{
	free_children(path);
	g_chunk_free(path, node_chunk);
}

static ETreeSortedPath *
new_path (ETreeSortedPath *parent, ETreePath corresponding)
{
	ETreeSortedPath *path;

	path = g_chunk_new0 (ETreeSortedPath, node_chunk);

	path->corresponding = corresponding;
	path->parent = parent;
	path->num_children = -1;
	path->children = NULL;
	path->position = -1;
	path->orig_position = -1;
	path->child_needs_resort = 0;
	path->resort_all_children = 0;
	path->needs_resort = 0;
	path->needs_regen_to_sort = 0;

	return path;
}

static gboolean
reposition_path (ETreeSorted *ets, ETreeSortedPath *path)
{
	int new_index;
	int old_index = path->position;
	ETreeSortedPath *parent = path->parent;
	gboolean changed = FALSE;
	if (parent) {
		if (ets->priv->sort_idle_id == 0) {
			if (ets->priv->insert_count > ETS_INSERT_MAX) {
				/* schedule a sort, and append instead */
				schedule_resort(ets, parent, TRUE, FALSE);
			} else {
				/* make sure we have an idle handler to reset the count every now and then */
				if (ets->priv->insert_idle_id == 0) {
					ets->priv->insert_idle_id = g_idle_add_full(40, (GSourceFunc) ets_insert_idle, ets, NULL);
				}

				new_index = e_table_sorting_utils_tree_check_position
					(E_TREE_MODEL(ets),
					 ets->priv->sort_info,
					 ets->priv->full_header,
					 (ETreePath *) parent->children,
					 parent->num_children,
					 old_index);

				if (new_index > old_index) {
					int i;
					ets->priv->insert_count++;
					memmove(parent->children + old_index, parent->children + old_index + 1, sizeof (ETreePath) * (new_index - old_index));
					parent->children[new_index] = path;
					for (i = old_index; i <= new_index; i++)
						parent->children[i]->position = i;
					changed = TRUE;
					e_tree_model_node_changed(E_TREE_MODEL(ets), parent);
					e_tree_sorted_node_resorted(ets, parent);
				} else if (new_index < old_index) {
					int i;
					ets->priv->insert_count++;
					memmove(parent->children + new_index + 1, parent->children + new_index, sizeof (ETreePath) * (old_index - new_index));
					parent->children[new_index] = path;
					for (i = new_index; i <= old_index; i++)
						parent->children[i]->position = i;
					changed = TRUE;
					e_tree_model_node_changed(E_TREE_MODEL(ets), parent);
					e_tree_sorted_node_resorted(ets, parent);
				}
			}
		} else
			mark_path_needs_resort(ets, parent, TRUE, FALSE);
	}
	return changed;
}

static void
regenerate_children(ETreeSorted *ets, ETreeSortedPath *path)
{
	ETreeSortedPath **children;
	int i;

	children = g_new(ETreeSortedPath *, path->num_children);
	for (i = 0; i < path->num_children; i++)
		children[path->children[i]->orig_position] = path->children[i];
	g_free(path->children);
	path->children = children;
}

static void
generate_children(ETreeSorted *ets, ETreeSortedPath *path)
{
	ETreePath child;
	int i;
	int count;

	free_children(path);

	count = 0;
	for (child = e_tree_model_node_get_first_child(ets->priv->source, path->corresponding);
	     child;
	     child = e_tree_model_node_get_next(ets->priv->source, child)) {
		count ++;
	}

	path->num_children = count;
	path->children = g_new(ETreeSortedPath *, count);
	for (child = e_tree_model_node_get_first_child(ets->priv->source, path->corresponding), i = 0;
	     child;
	     child = e_tree_model_node_get_next(ets->priv->source, child), i++) {
		path->children[i] = new_path(path, child);
		path->children[i]->position = i;
		path->children[i]->orig_position = i;
	}
	if (path->num_children > 0)
		schedule_resort (ets, path, FALSE, TRUE);
}

static void
resort_node (ETreeSorted *ets, ETreeSortedPath *path, gboolean resort_all_children, gboolean needs_regen, gboolean send_signals)
{
	gboolean needs_resort;
	if (path) {
		needs_resort = path->needs_resort || resort_all_children;
		needs_regen = path->needs_regen_to_sort || needs_regen;
		if (path->num_children > 0) {
			if (needs_resort && send_signals)
				e_tree_model_pre_change(E_TREE_MODEL(ets));
			if (needs_resort) {
				int i;
				d(g_print("Start sort of node %p\n", path));
				if (needs_regen)
					regenerate_children(ets, path);
				d(g_print("Regened sort of node %p\n", path));
				e_table_sorting_utils_tree_sort (E_TREE_MODEL(ets),
								 ets->priv->sort_info,
								 ets->priv->full_header,
								 (ETreePath *) path->children,
								 path->num_children);
				d(g_print("Renumbering sort of node %p\n", path));
				for (i = 0; i < path->num_children; i++) {
					path->children[i]->position = i;
				}
				d(g_print("End sort of node %p\n", path));
			}
			if (path->resort_all_children)
				resort_all_children = TRUE;
			if ((resort_all_children || path->child_needs_resort) && path->num_children >= 0) {
				int i;
				for (i = 0; i < path->num_children; i++) {
					resort_node(ets, path->children[i], resort_all_children, needs_regen, send_signals && !needs_resort);
				}
				path->child_needs_resort = 0;
			}
		}
		path->needs_resort = 0;	
		path->child_needs_resort = 0;
		path->needs_regen_to_sort = 0;
		path->resort_all_children = 0;
		if (needs_resort && send_signals && path->num_children > 0) {
			e_tree_model_node_changed(E_TREE_MODEL(ets), path);
			e_tree_sorted_node_resorted(ets, path);
		}
	}
}

static void
mark_path_child_needs_resort (ETreeSorted *ets, ETreeSortedPath *path)
{
	if (path == NULL)
		return;
	if (!path->child_needs_resort) {
		path->child_needs_resort = 1;
		mark_path_child_needs_resort (ets, path->parent);
	}
}

static void
mark_path_needs_resort (ETreeSorted *ets, ETreeSortedPath *path, gboolean needs_regen, gboolean resort_all_children)
{
	if (path == NULL)
		return;
	if (path->num_children == 0)
		return;
	path->needs_resort = 1;
	path->needs_regen_to_sort = needs_regen;
	path->resort_all_children = resort_all_children;
	mark_path_child_needs_resort(ets, path->parent);
}

static void
schedule_resort (ETreeSorted *ets, ETreeSortedPath *path, gboolean needs_regen, gboolean resort_all_children)
{
	ets->priv->insert_count = 0;
	if (ets->priv->insert_idle_id != 0) {
		g_source_remove(ets->priv->insert_idle_id);
		ets->priv->insert_idle_id = 0;
	}

	if (path == NULL)
		return;
	if (path->num_children == 0)
		return;

	mark_path_needs_resort(ets, path, needs_regen, resort_all_children);
	if (ets->priv->sort_idle_id == 0) {
		ets->priv->sort_idle_id = g_idle_add_full(50, (GSourceFunc) ets_sort_idle, ets, NULL);
	} else if (ets->priv->in_resort_idle) {
		ets->priv->nested_resort_idle = TRUE;
	}
}



/* virtual methods */

static void
ets_dispose (GObject *object)
{
	ETreeSorted *ets = E_TREE_SORTED (object);
	ETreeSortedPriv *priv = ets->priv;

	/* FIXME lots of stuff to free here */
	if (!priv) {
		G_OBJECT_CLASS (parent_class)->dispose (object);
		return;
	}

	if (priv->source) {
		g_signal_handler_disconnect (G_OBJECT (priv->source),
				             priv->tree_model_pre_change_id);
		g_signal_handler_disconnect (G_OBJECT (priv->source),
				             priv->tree_model_no_change_id);
		g_signal_handler_disconnect (G_OBJECT (priv->source),
				             priv->tree_model_node_changed_id);
		g_signal_handler_disconnect (G_OBJECT (priv->source),
				             priv->tree_model_node_data_changed_id);
		g_signal_handler_disconnect (G_OBJECT (priv->source),
				             priv->tree_model_node_col_changed_id);
		g_signal_handler_disconnect (G_OBJECT (priv->source),
				             priv->tree_model_node_inserted_id);
		g_signal_handler_disconnect (G_OBJECT (priv->source),
				             priv->tree_model_node_removed_id);
		g_signal_handler_disconnect (G_OBJECT (priv->source),
				             priv->tree_model_node_deleted_id);
		g_signal_handler_disconnect (G_OBJECT (priv->source),
				             priv->tree_model_node_request_collapse_id);

		g_object_unref (priv->source);
		priv->source = NULL;

		priv->tree_model_pre_change_id = 0;
		priv->tree_model_no_change_id = 0;
		priv->tree_model_node_changed_id = 0;
		priv->tree_model_node_data_changed_id = 0;
		priv->tree_model_node_col_changed_id = 0;
		priv->tree_model_node_inserted_id = 0;
		priv->tree_model_node_removed_id = 0;
		priv->tree_model_node_deleted_id = 0;
		priv->tree_model_node_request_collapse_id = 0;
	}

	if (priv->sort_info) {
		g_signal_handler_disconnect (G_OBJECT (priv->sort_info),
				             priv->sort_info_changed_id);
		priv->sort_info_changed_id = 0;

		g_object_unref (priv->sort_info);
		priv->sort_info = NULL;
	}

	ets_stop_sort_idle (ets);
	if (ets->priv->insert_idle_id) {
		g_source_remove(ets->priv->insert_idle_id);
		ets->priv->insert_idle_id = 0;
	}

	if (priv->full_header)
		g_object_unref(priv->full_header);

}

static void
ets_finalize (GObject *object)
{
	ETreeSorted *ets = (ETreeSorted *) object;

	if (ets->priv->root)
		free_path(ets->priv->root);

	g_free (ets->priv);
	ets->priv = NULL;

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static ETreePath
ets_get_root (ETreeModel *etm)
{
	ETreeSortedPriv *priv = E_TREE_SORTED(etm)->priv;
	if (priv->root == NULL) {
		ETreeSorted *ets = E_TREE_SORTED(etm);
		ETreePath corresponding = e_tree_model_get_root(ets->priv->source);

		if (corresponding) {
			priv->root = new_path(NULL, corresponding);
		}
	}
	if (priv->root && priv->root->num_children == -1) {
		generate_children(E_TREE_SORTED(etm), priv->root);
	}

	return priv->root;
}

static ETreePath
ets_get_parent (ETreeModel *etm, ETreePath node)
{
	ETreeSortedPath *path = node;
	return path->parent;
}

static ETreePath
ets_get_first_child (ETreeModel *etm, ETreePath node)
{
	ETreeSortedPath *path = node;
	ETreeSorted *ets = E_TREE_SORTED(etm);

	if (path->num_children == -1)
		generate_children(ets, path);

	if (path->num_children > 0)
		return path->children[0];
	else
		return NULL;
}

static ETreePath
ets_get_last_child (ETreeModel *etm, ETreePath node)
{
	ETreeSortedPath *path = node;
	ETreeSorted *ets = E_TREE_SORTED(etm);

	if (path->num_children == -1)
		generate_children(ets, path);

	if (path->num_children > 0)
		return path->children[path->num_children - 1];
	else
		return NULL;
}

static ETreePath
ets_get_next (ETreeModel *etm, ETreePath node)
{
	ETreeSortedPath *path = node;
	ETreeSortedPath *parent = path->parent;
	if (parent) {
		if (parent->num_children > path->position + 1)
			return parent->children[path->position + 1];
		else
			return NULL;
	} else
		  return NULL;
}

static ETreePath
ets_get_prev (ETreeModel *etm, ETreePath node)
{
	ETreeSortedPath *path = node;
	ETreeSortedPath *parent = path->parent;
	if (parent) {
		if (path->position - 1 >= 0)
			return parent->children[path->position - 1];
		else
			return NULL;
	} else
		  return NULL;
}

static gboolean
ets_is_root (ETreeModel *etm, ETreePath node)
{
 	ETreeSortedPath *path = node;
	ETreeSorted *ets = E_TREE_SORTED(etm);

	return e_tree_model_node_is_root (ets->priv->source, path->corresponding);
}

static gboolean
ets_is_expandable (ETreeModel *etm, ETreePath node)
{
 	ETreeSortedPath *path = node;
	ETreeSorted *ets = E_TREE_SORTED(etm);
	gboolean expandable = e_tree_model_node_is_expandable (ets->priv->source, path->corresponding);

	if (path->num_children == -1) {
		generate_children(ets, node);
	}

	return expandable;
}

static guint
ets_get_children (ETreeModel *etm, ETreePath node, ETreePath **nodes)
{
	ETreeSortedPath *path = node;
	guint n_children;

	if (path->num_children == -1) {
		generate_children(E_TREE_SORTED(etm), node);
	}

	n_children = path->num_children;

	if (nodes) {
		int i;

		(*nodes) = g_malloc (sizeof (ETreePath) * n_children);
		for (i = 0; i < n_children; i ++) {
			(*nodes)[i] = path->children[i];
		}
	}

	return n_children;
}

static guint
ets_depth (ETreeModel *etm, ETreePath node)
{
	ETreeSortedPath *path = node;
	ETreeSorted *ets = E_TREE_SORTED(etm);

	return e_tree_model_node_depth(ets->priv->source, path->corresponding);
}

static GdkPixbuf *
ets_icon_at (ETreeModel *etm, ETreePath node)
{
	ETreeSortedPath *path = node;
	ETreeSorted *ets = E_TREE_SORTED(etm);

	return e_tree_model_icon_at(ets->priv->source, path->corresponding);
}

static gboolean
ets_get_expanded_default (ETreeModel *etm)
{
	ETreeSorted *ets = E_TREE_SORTED(etm);

	return e_tree_model_get_expanded_default(ets->priv->source);
}

static gint
ets_column_count (ETreeModel *etm)
{
	ETreeSorted *ets = E_TREE_SORTED(etm);

	return e_tree_model_column_count(ets->priv->source);
}


static gboolean
ets_has_save_id (ETreeModel *etm)
{
	return TRUE;
}

static gchar *
ets_get_save_id (ETreeModel *etm, ETreePath node)
{
	ETreeSorted *ets = E_TREE_SORTED(etm);
	ETreeSortedPath *path = node;

	if (e_tree_model_has_save_id(ets->priv->source))
		return e_tree_model_get_save_id(ets->priv->source, path->corresponding);
	else
		return g_strdup_printf("%p", path->corresponding);
}

static gboolean
ets_has_get_node_by_id (ETreeModel *etm)
{
	ETreeSorted *ets = E_TREE_SORTED(etm);
	return e_tree_model_has_get_node_by_id(ets->priv->source);
}

static ETreePath
ets_get_node_by_id (ETreeModel *etm, const char *save_id)
{
	ETreeSorted *ets = E_TREE_SORTED(etm);
	ETreePath node;

	node = e_tree_model_get_node_by_id (ets->priv->source, save_id);

	return find_path(ets, node);
}

static gboolean
ets_has_change_pending (ETreeModel *etm)
{
	ETreeSorted *ets = E_TREE_SORTED(etm);

	return ets->priv->sort_idle_id != 0;
}


static void *
ets_value_at (ETreeModel *etm, ETreePath node, int col)
{
	ETreeSorted *ets = E_TREE_SORTED(etm);
	ETreeSortedPath *path = node;

	return e_tree_model_value_at(ets->priv->source, path->corresponding, col);
}

static void
ets_set_value_at (ETreeModel *etm, ETreePath node, int col, const void *val)
{
	ETreeSorted *ets = E_TREE_SORTED(etm);
	ETreeSortedPath *path = node;

	e_tree_model_set_value_at (ets->priv->source, path->corresponding, col, val);
}

static gboolean
ets_is_editable (ETreeModel *etm, ETreePath node, int col)
{
	ETreeSorted *ets = E_TREE_SORTED(etm);
	ETreeSortedPath *path = node;

	return e_tree_model_node_is_editable (ets->priv->source, path->corresponding, col);
}


/* The default for ets_duplicate_value is to return the raw value. */
static void *
ets_duplicate_value (ETreeModel *etm, int col, const void *value)
{
	ETreeSorted *ets = E_TREE_SORTED(etm);

	return e_tree_model_duplicate_value (ets->priv->source, col, value);
}

static void
ets_free_value (ETreeModel *etm, int col, void *value)
{
	ETreeSorted *ets = E_TREE_SORTED(etm);

	e_tree_model_free_value (ets->priv->source, col, value);
}

static void *
ets_initialize_value (ETreeModel *etm, int col)
{
	ETreeSorted *ets = E_TREE_SORTED(etm);
	
	return e_tree_model_initialize_value (ets->priv->source, col);
}

static gboolean
ets_value_is_empty (ETreeModel *etm, int col, const void *value)
{
	ETreeSorted *ets = E_TREE_SORTED(etm);
	
	return e_tree_model_value_is_empty (ets->priv->source, col, value);
}

static char *
ets_value_to_string (ETreeModel *etm, int col, const void *value)
{
	ETreeSorted *ets = E_TREE_SORTED(etm);
	
	return e_tree_model_value_to_string (ets->priv->source, col, value);
}

/* Proxy functions */

static void
ets_proxy_pre_change (ETreeModel *etm, ETreeSorted *ets)
{
	e_tree_model_pre_change(E_TREE_MODEL(ets));
}

static void
ets_proxy_no_change (ETreeModel *etm, ETreeSorted *ets)
{
	e_tree_model_no_change(E_TREE_MODEL(ets));
}

static void
ets_proxy_node_changed (ETreeModel *etm, ETreePath node, ETreeSorted *ets)
{
	ets->priv->last_access = NULL;
	d(g_print("Setting last access %p. (ets_proxy_node_changed)\n", ets->priv->last_access));

	if (e_tree_model_node_is_root(ets->priv->source, node)) {
		ets_stop_sort_idle (ets);

		if (ets->priv->root) {
			free_path(ets->priv->root);
		}
		ets->priv->root = new_path(NULL, node);
		e_tree_model_node_changed(E_TREE_MODEL(ets), ets->priv->root);
		return;
	} else {
		ETreeSortedPath *path = find_path(ets, node);

		if (path) {
			free_children(path);
			if (!reposition_path(ets, path)) {
				e_tree_model_node_changed(E_TREE_MODEL(ets), path);
			} else {
				e_tree_model_no_change(E_TREE_MODEL(ets));
			}
		} else {
			e_tree_model_no_change(E_TREE_MODEL(ets));
		}
	}
}

static void
ets_proxy_node_data_changed (ETreeModel *etm, ETreePath node, ETreeSorted *ets)
{
	ETreeSortedPath *path = find_path(ets, node);

	if (path) {
		if (!reposition_path(ets, path))
			e_tree_model_node_data_changed(E_TREE_MODEL(ets), path);
		else
			e_tree_model_no_change(E_TREE_MODEL(ets));
	} else
		e_tree_model_no_change(E_TREE_MODEL(ets));
}

static void
ets_proxy_node_col_changed (ETreeModel *etm, ETreePath node, int col, ETreeSorted *ets)
{
	ETreeSortedPath *path = find_path(ets, node);

	if (path) {
		gboolean changed = FALSE;
		if (e_table_sorting_utils_affects_sort(ets->priv->sort_info, ets->priv->full_header, col))
			changed = reposition_path(ets, path);
		if (!changed)
			e_tree_model_node_col_changed(E_TREE_MODEL(ets), path, col);
		else
			e_tree_model_no_change(E_TREE_MODEL(ets));
	} else
		e_tree_model_no_change(E_TREE_MODEL(ets));
}

static void
ets_proxy_node_inserted (ETreeModel *etm, ETreePath parent, ETreePath child, ETreeSorted *ets)
{
	ETreeSortedPath *parent_path = find_path(ets, parent);

	if (parent_path && parent_path->num_children != -1) {
		int i;
		int j;
		ETreeSortedPath *path;
		int position = parent_path->num_children;
		ETreePath counter;

		for (counter = e_tree_model_node_get_next(etm, child);
		     counter;
		     counter = e_tree_model_node_get_next(etm, counter))
			position --;

		if (position != parent_path->num_children) {
			for (i = 0; i < parent_path->num_children; i++) {
				if (parent_path->children[i]->orig_position >= position)
					parent_path->children[i]->orig_position++;
			}
		}

		i = parent_path->num_children;
		path = new_path(parent_path, child);
		path->orig_position = position;
		if (!ETS_SORT_IDLE_ACTIVATED (ets)) {
			ets->priv->insert_count++;
			if (ets->priv->insert_count > ETS_INSERT_MAX) {
				/* schedule a sort, and append instead */
				schedule_resort(ets, parent_path, TRUE, FALSE);
			} else {
				/* make sure we have an idle handler to reset the count every now and then */
				if (ets->priv->insert_idle_id == 0) {
					ets->priv->insert_idle_id = g_idle_add_full(40, (GSourceFunc) ets_insert_idle, ets, NULL);
				}
				i = e_table_sorting_utils_tree_insert
					(ets->priv->source,
					 ets->priv->sort_info,
					 ets->priv->full_header,
					 (ETreePath *) parent_path->children,
					 parent_path->num_children,
					 path);
			}
		} else {
			mark_path_needs_resort(ets, parent_path, TRUE, FALSE);
		}
		parent_path->num_children ++;
		parent_path->children = g_renew(ETreeSortedPath *, parent_path->children, parent_path->num_children);
		memmove(parent_path->children + i + 1, parent_path->children + i, (parent_path->num_children - 1 - i) * sizeof(int));
		parent_path->children[i] = path;
		for (j = i; j < parent_path->num_children; j++) {
			parent_path->children[j]->position = j;
		}
		e_tree_model_node_inserted(E_TREE_MODEL(ets), parent_path, parent_path->children[i]);
	} else if (ets->priv->root == NULL && parent == NULL) {
		if (child) {
			ets->priv->root = new_path(NULL, child);
			e_tree_model_node_inserted(E_TREE_MODEL(ets), NULL, ets->priv->root);
		} else {
			e_tree_model_no_change(E_TREE_MODEL(ets));
		}
	} else {
		e_tree_model_no_change(E_TREE_MODEL(ets));
	}
}

static void
ets_proxy_node_removed (ETreeModel *etm, ETreePath parent, ETreePath child, int old_position, ETreeSorted *ets)
{
	ETreeSortedPath *parent_path = find_path(ets, parent);
	ETreeSortedPath *path;

	if (parent_path)
		path = find_child_path(ets, parent_path, child);
	else
		path = find_path(ets, child);

	d(g_print("Setting last access %p. (ets_proxy_node_removed)\n ", ets->priv->last_access));
	ets->priv->last_access = NULL;

	if (path && parent_path && parent_path->num_children != -1) {
		int i;
		for (i = 0; i < parent_path->num_children; i++) {
			if (parent_path->children[i]->orig_position > old_position)
				parent_path->children[i]->orig_position --;
		}

		i = path->position;

		parent_path->num_children --;
		memmove(parent_path->children + i, parent_path->children + i + 1, sizeof(ETreeSortedPath *) * (parent_path->num_children - i));
		for (; i < parent_path->num_children; i++) {
			parent_path->children[i]->position = i;
		}
		e_tree_model_node_removed(E_TREE_MODEL(ets), parent_path, path, path->position);
		free_path(path);
	} else if (path && path == ets->priv->root) {
		ets->priv->root = NULL;
		e_tree_model_node_removed(E_TREE_MODEL(ets), NULL, path, -1);
		free_path(path);
	}
}

static void
ets_proxy_node_deleted (ETreeModel *etm, ETreePath child, ETreeSorted *ets)
{
	e_tree_model_node_deleted(E_TREE_MODEL(ets), NULL);
}

static void
ets_proxy_node_request_collapse (ETreeModel *etm, ETreePath node, ETreeSorted *ets)
{
	ETreeSortedPath *path = find_path(ets, node);
	if (path) {
		e_tree_model_node_request_collapse(E_TREE_MODEL(ets), path);
	}
}

static void
ets_sort_info_changed (ETableSortInfo *sort_info, ETreeSorted *ets)
{
	schedule_resort(ets, ets->priv->root, TRUE, TRUE);
}



/* Initialization and creation */

static void
e_tree_sorted_class_init (ETreeSortedClass *klass)
{
	ETreeModelClass *tree_class      = E_TREE_MODEL_CLASS (klass);
	GObjectClass *object_class       = G_OBJECT_CLASS (klass);

	parent_class                     = g_type_class_peek_parent (klass);

	node_chunk                       = g_mem_chunk_create (ETreeSortedPath, TREEPATH_CHUNK_AREA_SIZE, G_ALLOC_AND_FREE);

	klass->node_resorted             = NULL;
	
	object_class->dispose            = ets_dispose;
	object_class->finalize           = ets_finalize;

	tree_class->get_root             = ets_get_root;
	tree_class->get_parent           = ets_get_parent;
	tree_class->get_first_child      = ets_get_first_child;
	tree_class->get_last_child       = ets_get_last_child;
	tree_class->get_prev             = ets_get_prev;
	tree_class->get_next             = ets_get_next;

	tree_class->is_root              = ets_is_root;
	tree_class->is_expandable        = ets_is_expandable;
	tree_class->get_children         = ets_get_children;
	tree_class->depth                = ets_depth;

	tree_class->icon_at              = ets_icon_at;

	tree_class->get_expanded_default = ets_get_expanded_default;
	tree_class->column_count         = ets_column_count;

	tree_class->has_save_id          = ets_has_save_id;
	tree_class->get_save_id          = ets_get_save_id;

	tree_class->has_get_node_by_id   = ets_has_get_node_by_id;
	tree_class->get_node_by_id       = ets_get_node_by_id;

	tree_class->has_change_pending   = ets_has_change_pending;

	tree_class->value_at             = ets_value_at;
	tree_class->set_value_at         = ets_set_value_at;
	tree_class->is_editable          = ets_is_editable;

	tree_class->duplicate_value      = ets_duplicate_value;
	tree_class->free_value           = ets_free_value;
	tree_class->initialize_value     = ets_initialize_value;
	tree_class->value_is_empty       = ets_value_is_empty;
	tree_class->value_to_string      = ets_value_to_string;

	signals [NODE_RESORTED] =
		g_signal_new ("node_resorted",
			      E_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeSortedClass, node_resorted),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
e_tree_sorted_init (GObject *object)
{
	ETreeSorted *ets = (ETreeSorted *)object;

	ETreeSortedPriv *priv;

	priv                                      = g_new0 (ETreeSortedPriv, 1);
	ets->priv                                 = priv;

	priv->root                                = NULL;
	priv->source                              = NULL;

	priv->sort_info                           = NULL;
	priv->full_header                         = NULL;

	priv->last_access                         = NULL;

	priv->tree_model_pre_change_id            = 0;
	priv->tree_model_no_change_id             = 0;
	priv->tree_model_node_changed_id          = 0;
	priv->tree_model_node_data_changed_id     = 0;
	priv->tree_model_node_col_changed_id      = 0;
	priv->tree_model_node_inserted_id         = 0;
	priv->tree_model_node_removed_id          = 0;
	priv->tree_model_node_deleted_id          = 0;
	priv->tree_model_node_request_collapse_id = 0;

	priv->sort_info_changed_id                = 0;
	priv->sort_idle_id                        = 0;
	priv->insert_idle_id                      = 0;
	priv->insert_count                        = 0;

	priv->in_resort_idle                      = 0;
	priv->nested_resort_idle                  = 0;
}

E_MAKE_TYPE(e_tree_sorted, "ETreeSorted", ETreeSorted, e_tree_sorted_class_init, e_tree_sorted_init, E_TREE_MODEL_TYPE)

/**
 * e_tree_sorted_construct:
 * @etree: 
 * 
 * 
 **/
void
e_tree_sorted_construct (ETreeSorted *ets, ETreeModel *source, ETableHeader *full_header, ETableSortInfo *sort_info)
{
	ets->priv->source                              = source;
	if (source)
		g_object_ref(source);

	ets->priv->full_header                         = full_header;
	if (full_header)
		g_object_ref(full_header);

	e_tree_sorted_set_sort_info (ets, sort_info);

	ets->priv->tree_model_pre_change_id            = g_signal_connect (G_OBJECT (source), "pre_change",
									   G_CALLBACK (ets_proxy_pre_change), ets);
	ets->priv->tree_model_no_change_id             = g_signal_connect (G_OBJECT (source), "no_change",
									   G_CALLBACK (ets_proxy_no_change), ets);
	ets->priv->tree_model_node_changed_id          = g_signal_connect (G_OBJECT (source), "node_changed",
									   G_CALLBACK (ets_proxy_node_changed), ets);
	ets->priv->tree_model_node_data_changed_id     = g_signal_connect (G_OBJECT (source), "node_data_changed",
									   G_CALLBACK (ets_proxy_node_data_changed), ets);
	ets->priv->tree_model_node_col_changed_id      = g_signal_connect (G_OBJECT (source), "node_col_changed",
									   G_CALLBACK (ets_proxy_node_col_changed), ets);
	ets->priv->tree_model_node_inserted_id         = g_signal_connect (G_OBJECT (source), "node_inserted",
									   G_CALLBACK (ets_proxy_node_inserted), ets);
	ets->priv->tree_model_node_removed_id          = g_signal_connect (G_OBJECT (source), "node_removed",
									   G_CALLBACK (ets_proxy_node_removed), ets);
	ets->priv->tree_model_node_deleted_id          = g_signal_connect (G_OBJECT (source), "node_deleted",
									   G_CALLBACK (ets_proxy_node_deleted), ets);
	ets->priv->tree_model_node_request_collapse_id = g_signal_connect (G_OBJECT (source), "node_request_collapse",
									   G_CALLBACK (ets_proxy_node_request_collapse), ets);

}

/**
 * e_tree_sorted_new
 *
 * FIXME docs here.
 *
 * return values: a newly constructed ETreeSorted.
 */
ETreeSorted *
e_tree_sorted_new (ETreeModel *source, ETableHeader *full_header, ETableSortInfo *sort_info)
{
	ETreeSorted *ets = g_object_new (E_TREE_SORTED_TYPE, NULL);

	e_tree_sorted_construct(ets, source, full_header, sort_info);

	return ets;
}

ETreePath
e_tree_sorted_view_to_model_path  (ETreeSorted    *ets,
				   ETreePath       view_path)
{
	ETreeSortedPath *path = view_path;
	if (path) {
		ets->priv->last_access = path;
		d(g_print("Setting last access %p. (e_tree_sorted_view_to_model_path)\n", ets->priv->last_access));
		return path->corresponding;
	} else
		return NULL;
}

ETreePath
e_tree_sorted_model_to_view_path  (ETreeSorted    *ets,
				   ETreePath       model_path)
{
	return find_or_create_path(ets, model_path);
}

int
e_tree_sorted_orig_position       (ETreeSorted    *ets,
				   ETreePath       path)
{
	ETreeSortedPath *sorted_path = path;
	return sorted_path->orig_position;
}

int
e_tree_sorted_node_num_children   (ETreeSorted    *ets,
				   ETreePath       path)
{
	ETreeSortedPath *sorted_path = path;

	if (sorted_path->num_children == -1) {
		generate_children(ets, sorted_path);
	}

	return sorted_path->num_children;
}

void
e_tree_sorted_node_resorted  (ETreeSorted *sorted, ETreePath node)
{
	g_return_if_fail (sorted != NULL);
	g_return_if_fail (E_IS_TREE_SORTED (sorted));
	
	g_signal_emit (G_OBJECT (sorted), signals [NODE_RESORTED], 0, node);
}

void
e_tree_sorted_set_sort_info (ETreeSorted *ets, ETableSortInfo *sort_info)
{

	g_return_if_fail (ets != NULL);


	if (ets->priv->sort_info) {
		if (ets->priv->sort_info_changed_id != 0)
			g_signal_handler_disconnect (G_OBJECT (ets->priv->sort_info),
						     ets->priv->sort_info_changed_id);
		ets->priv->sort_info_changed_id = 0;
		g_object_unref (ets->priv->sort_info);
	}

	ets->priv->sort_info = sort_info;
	if (sort_info) {
		g_object_ref(sort_info);
		ets->priv->sort_info_changed_id = g_signal_connect (G_OBJECT (ets->priv->sort_info), "sort_info_changed",
								    G_CALLBACK (ets_sort_info_changed), ets);
	}

	if (ets->priv->root)
		schedule_resort (ets, ets->priv->root, TRUE, TRUE);
}

ETableSortInfo*
e_tree_sorted_get_sort_info (ETreeSorted *ets)
{
	return ets->priv->sort_info;
}

