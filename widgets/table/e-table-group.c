/*
 * E-Table-Group.c: Implements the grouping objects for elements on a table
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org()
 *
 * Copyright 1999, Helix Code, Inc.
 */

#include <config.h>
#include "e-table-group.h"

void
e_table_group_destroy (ETableGroup *etg)
{
	g_return_if_fail (etg != NULL);

	g_free (etg->title);

	if (etg->is_leaf == 0){
		GSList *l;

		for (l = etg->u.children; l; l = l->next){
			ETableGroup *child = l->data;

			e_table_group_destroy (child);
		}
		g_slist_free (etg->u.children);
		etg->u.children = NULL;
	}
	g_free (etg);
}

ETableGroup *
e_table_group_new_leaf (const char *title, ETableModel *table)
{
	ETableGroup *etg;

	g_return_val_if_fail (title != NULL, NULL);
	g_return_val_if_fail (table != NULL, NULL);
	
	etg = g_new (ETableGroup, 1);

	etg->expanded = 0;
	etg->is_leaf = 1;
	etg->u.table = table;
	etg->title = g_strdup (title);

	return etg;
}

ETableGroup *
e_table_group_new (const char *title)
{
	ETableGroup *etg;

	g_return_val_if_fail (title != NULL, NULL);
	
	etg = g_new (ETableGroup, 1);

	etg->expanded = 0;
	etg->is_leaf = 0;
	etg->u.children = NULL;
	etg->title = g_strdup (title);

	return etg;
}

void
e_table_group_append_child (ETableGroup *etg, ETableGroup *child)
{
	g_return_if_fail (etg != NULL);
	g_return_if_fail (child != NULL);
	g_return_if_fail (etg->is_leaf != 0);

	etg->u.children = g_slist_append (etg->u.children, child);
}

int
e_table_group_size (ETableGroup *etg)
{
	g_return_if_fail (etg != NULL);

	if (etg->is_leaf)
		return e_table_model_height (etg->u.table);
	else {
		GSList *l;
		int size = 0;
		
		for (l = etg->u.children; l; l = l->next){
			ETableGroup *child = l->data;

			size += e_table_group_size (child);
		}
		return size;
	}
}

