/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-sorted.c: Implements a table that sorts another table
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Helix Code, Inc.
 */
#include <config.h>
#include <stdlib.h>
#include <gtk/gtksignal.h>
#include <string.h>
#include "gal/util/e-util.h"
#include "e-table-sorted-variable.h"

#define d(x)

#define PARENT_TYPE E_TABLE_SUBSET_VARIABLE_TYPE

#define INCREMENT_AMOUNT 100

/* maximum insertions between an idle event that we will do without scheduling an idle sort */
#define ETSV_INSERT_MAX (4)

static ETableSubsetVariableClass *etsv_parent_class;

static void etsv_proxy_model_changed      (ETableModel *etm, ETableSortedVariable *etsv);
#if 0
static void etsv_proxy_model_row_changed  (ETableModel *etm, int row, ETableSortedVariable *etsv);
static void etsv_proxy_model_cell_changed (ETableModel *etm, int col, int row, ETableSortedVariable *etsv);
#endif
static void etsv_sort_info_changed        (ETableSortInfo *info, ETableSortedVariable *etsv);
static void etsv_sort                     (ETableSortedVariable *etsv);
static void etsv_add                      (ETableSubsetVariable *etssv, gint                  row);
static void etsv_add_all                  (ETableSubsetVariable *etssv);

static void
etsv_destroy (GtkObject *object)
{
	ETableSortedVariable *etsv = E_TABLE_SORTED_VARIABLE (object);
	ETableSubset *etss = E_TABLE_SUBSET (object);

	gtk_signal_disconnect (GTK_OBJECT (etss->source),
			       etsv->table_model_changed_id);
#if 0
	gtk_signal_disconnect (GTK_OBJECT (etss->source),
			       etsv->table_model_row_changed_id);
	gtk_signal_disconnect (GTK_OBJECT (etss->source),
			       etsv->table_model_cell_changed_id);
#endif
	gtk_signal_disconnect (GTK_OBJECT (etsv->sort_info),
			       etsv->sort_info_changed_id);

	if (etsv->sort_idle_id) {
		g_source_remove(etsv->sort_idle_id);
	}
	if (etsv->insert_idle_id) {
		g_source_remove(etsv->insert_idle_id);
	}

	etsv->table_model_changed_id = 0;
	etsv->table_model_row_changed_id = 0;
	etsv->table_model_cell_changed_id = 0;

	if (etsv->sort_info)
		gtk_object_unref(GTK_OBJECT(etsv->sort_info));
	if (etsv->full_header)
		gtk_object_unref(GTK_OBJECT(etsv->full_header));

	GTK_OBJECT_CLASS (etsv_parent_class)->destroy (object);
}

static void
etsv_class_init (GtkObjectClass *object_class)
{
	ETableSubsetVariableClass *etssv_class = E_TABLE_SUBSET_VARIABLE_CLASS(object_class);

	etsv_parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = etsv_destroy;

	etssv_class->add = etsv_add;
	etssv_class->add_all = etsv_add_all;
}

static void
etsv_init (ETableSortedVariable *etsv)
{
	etsv->full_header = NULL;
	etsv->sort_info = NULL;

	etsv->table_model_changed_id = 0;
	etsv->table_model_row_changed_id = 0;
	etsv->table_model_cell_changed_id = 0;
	etsv->sort_info_changed_id = 0;

	etsv->sort_idle_id = 0;
	etsv->insert_count = 0;
}

E_MAKE_TYPE(e_table_sorted_variable, "ETableSortedVariable", ETableSortedVariable, etsv_class_init, etsv_init, PARENT_TYPE);

static gboolean
etsv_sort_idle(ETableSortedVariable *etsv)
{
	gtk_object_ref(GTK_OBJECT(etsv));
	etsv_sort(etsv);
	etsv->sort_idle_id = 0;
	etsv->insert_count = 0;
	gtk_object_unref(GTK_OBJECT(etsv));
	return FALSE;
}

static gboolean
etsv_insert_idle(ETableSortedVariable *etsv)
{
	etsv->insert_count = 0;
	etsv->insert_idle_id = 0;
	return FALSE;
}

/* This takes source rows. */
static int
etsv_compare(ETableSortedVariable *etsv, int row1, int row2)
{
	int j;
	int sort_count = e_table_sort_info_sorting_get_count(etsv->sort_info);
	int comp_val = 0;
	int ascending = 1;
	ETableSubset *etss = E_TABLE_SUBSET(etsv);

	for (j = 0; j < sort_count; j++) {
		ETableSortColumn column = e_table_sort_info_sorting_get_nth(etsv->sort_info, j);
		ETableCol *col;
		if (column.column > e_table_header_count (etsv->full_header))
			col = e_table_header_get_column (etsv->full_header, e_table_header_count (etsv->full_header) - 1);
		else
			col = e_table_header_get_column (etsv->full_header, column.column);
		comp_val = (*col->compare)(e_table_model_value_at (etss->source, col->col_idx, row1),
					   e_table_model_value_at (etss->source, col->col_idx, row2));
		ascending = column.ascending;
		if (comp_val != 0)
			break;
	}
	if (comp_val == 0) {
		if (row1 < row2)
			comp_val = -1;
		if (row1 > row2)
			comp_val = 1;
	}
	if (!ascending)
		comp_val = -comp_val;
	return comp_val;
}


static void
etsv_add       (ETableSubsetVariable *etssv,
		gint                  row)
{
	ETableModel *etm = E_TABLE_MODEL(etssv);
	ETableSubset *etss = E_TABLE_SUBSET(etssv);
	ETableSortedVariable *etsv = E_TABLE_SORTED_VARIABLE (etssv);
	int i;

	if (etss->n_map + 1 > etssv->n_vals_allocated) {
		etssv->n_vals_allocated += INCREMENT_AMOUNT;
		etss->map_table = g_realloc (etss->map_table, (etssv->n_vals_allocated) * sizeof(int));
	}
	i = etss->n_map;
	if (etsv->sort_idle_id == 0) {
		/* this is to see if we're inserting a lot of things between idle loops.
		   If we are, we're busy, its faster to just append and perform a full sort later */
		etsv->insert_count++;
		if (etsv->insert_count > ETSV_INSERT_MAX) {
			/* schedule a sort, and append instead */
			etsv->sort_idle_id = g_idle_add_full(50, (GSourceFunc) etsv_sort_idle, etsv, NULL);
		} else {
			/* make sure we have an idle handler to reset the count every now and then */
			if (etsv->insert_idle_id == 0) {
				etsv->insert_idle_id = g_idle_add_full(40, (GSourceFunc) etsv_insert_idle, etsv, NULL);
			}
			i = 0;
			/* handle insertions when we have a 'sort group' */
			if (e_table_model_has_sort_group(etss->source)) {
				/* find the row this row maps to */
				char *group = g_strdup(e_table_model_row_sort_group(etss->source, row));
				const char *newgroup;
				int cmp, grouplen, newgrouplen;
				
				newgroup = strrchr(group, '/');
				grouplen = strlen(group);
				if (newgroup)
					cmp = newgroup-group;
				else
					cmp = grouplen;
				
				/* find first common parent */
				while (i<etss->n_map) {
					newgroup = e_table_model_row_sort_group(etss->source, etss->map_table[i]);
					if (strncmp(newgroup, group, cmp) == 0) {
						break;
					}
					i++;
				}

				/* check matching records */
				while (i<etss->n_map) {
					newgroup = e_table_model_row_sort_group(etss->source, etss->map_table[i]);
					newgrouplen = strlen(newgroup);
					if (strncmp(newgroup, group, cmp) == 0) {
						/* common parent, check for same level */
						if (grouplen == newgrouplen) {
							if (etsv_compare(etsv, etss->map_table[i], row) >= 0)
								break;
						} else if (strncmp(newgroup + cmp, group + cmp, grouplen - cmp) == 0)
							/* Found a child of the inserted node.  Insert here. */
							break;
					} else {
						/* ran out of common parents, insert here */
						break;
					}
					i++;
				}
				g_free(group);
			} else {
				while (i < etss->n_map && etsv_compare(etsv, etss->map_table[i], row) < 0)
					i++;
			}
			memmove(etss->map_table + i + 1, etss->map_table + i, (etss->n_map - i) * sizeof(int));
		}
	}
	etss->map_table[i] = row;
	etss->n_map++;

	e_table_model_row_inserted (etm, i);
}

static void
etsv_add_all   (ETableSubsetVariable *etssv)
{
	ETableModel *etm = E_TABLE_MODEL(etssv);
	ETableSubset *etss = E_TABLE_SUBSET(etssv);
	ETableSortedVariable *etsv = E_TABLE_SORTED_VARIABLE (etssv);
	int rows;
	int i;

	e_table_model_pre_change(etm);

	rows = e_table_model_row_count(etss->source);

	if (etss->n_map + rows > etssv->n_vals_allocated){
		etssv->n_vals_allocated += MAX(INCREMENT_AMOUNT, rows);
		etss->map_table = g_realloc (etss->map_table, etssv->n_vals_allocated * sizeof(int));
	}
	for (i = 0; i < rows; i++)
		etss->map_table[etss->n_map++] = i;

	if (etsv->sort_idle_id == 0) {
		etsv->sort_idle_id = g_idle_add_full(50, (GSourceFunc) etsv_sort_idle, etsv, NULL);
	}

	e_table_model_changed (etm);
}

ETableModel *
e_table_sorted_variable_new (ETableModel *source, ETableHeader *full_header, ETableSortInfo *sort_info)
{
	ETableSortedVariable *etsv = gtk_type_new (E_TABLE_SORTED_VARIABLE_TYPE);
	ETableSubsetVariable *etssv = E_TABLE_SUBSET_VARIABLE (etsv);

	if (e_table_subset_variable_construct (etssv, source) == NULL){
		gtk_object_destroy (GTK_OBJECT (etsv));
		return NULL;
	}

	etsv->sort_info = sort_info;
	gtk_object_ref(GTK_OBJECT(etsv->sort_info));
	etsv->full_header = full_header;
	gtk_object_ref(GTK_OBJECT(etsv->full_header));

	etsv->table_model_changed_id = gtk_signal_connect (GTK_OBJECT (source), "model_changed",
							   GTK_SIGNAL_FUNC (etsv_proxy_model_changed), etsv);
#if 0
	etsv->table_model_row_changed_id = gtk_signal_connect (GTK_OBJECT (source), "model_row_changed",
							       GTK_SIGNAL_FUNC (etsv_proxy_model_row_changed), etsv);
	etsv->table_model_cell_changed_id = gtk_signal_connect (GTK_OBJECT (source), "model_cell_changed",
								GTK_SIGNAL_FUNC (etsv_proxy_model_cell_changed), etsv);
#endif
	etsv->sort_info_changed_id = gtk_signal_connect (GTK_OBJECT (sort_info), "sort_info_changed",
							 GTK_SIGNAL_FUNC (etsv_sort_info_changed), etsv);

	return E_TABLE_MODEL(etsv);
}

static void
etsv_proxy_model_changed (ETableModel *etm, ETableSortedVariable *etsv)
{
	/* FIXME: do_resort (); */
}
#if 0
static void
etsv_proxy_model_row_changed (ETableModel *etm, int row, ETableSortedVariable *etsv)
{
	ETableSubsetVariable *etssv = E_TABLE_SUBSET_VARIABLE(etsv);

	if (e_table_subset_variable_remove(etssv, row))
		e_table_subset_variable_add (etssv, row);
}

static void
etsv_proxy_model_cell_changed (ETableModel *etm, int col, int row, ETableSortedVariable *etsv)
{
	ETableSubsetVariable *etssv = E_TABLE_SUBSET_VARIABLE(etsv);

	if (e_table_subset_variable_remove(etssv, row))
		e_table_subset_variable_add (etssv, row);
}
#endif

static void
etsv_sort_info_changed (ETableSortInfo *info, ETableSortedVariable *etsv)
{
	etsv_sort(etsv);
}

static ETableSortedVariable *etsv_closure;
void **vals_closure;
int cols_closure;
int *ascending_closure;
GCompareFunc *compare_closure;

/* FIXME: Make it not cache the second and later columns (as if anyone cares.) */

static int
qsort_callback(const void *data1, const void *data2)
{
	gint row1 = *(int *)data1;
	gint row2 = *(int *)data2;
	int j;
	int sort_count = e_table_sort_info_sorting_get_count(etsv_closure->sort_info);
	int comp_val = 0;
	int ascending = 1;
	for (j = 0; j < sort_count; j++) {
		comp_val = (*(compare_closure[j]))(vals_closure[cols_closure * row1 + j], vals_closure[cols_closure * row2 + j]);
		ascending = ascending_closure[j];
		if (comp_val != 0)
			break;
	}
	if (comp_val == 0) {
		if (row1 < row2)
			comp_val = -1;
		if (row1 > row2)
			comp_val = 1;
	}
	if (!ascending)
		comp_val = -comp_val;
	return comp_val;
}

struct _subinfo {
	int start;
	GArray *rowsort;	/* an array of row info's */
};

struct _rowinfo {
	int row;
	struct _subinfo *subinfo;
	struct _group_info *groupinfo;
};

static int
qsort_callback_complex(const void *data1, const void *data2)
{
	gint row1 = ((struct _rowinfo *)data1)->row;
	gint row2 = ((struct _rowinfo *)data2)->row;
	int j;
	int sort_count = e_table_sort_info_sorting_get_count(etsv_closure->sort_info);
	int comp_val = 0;
	int ascending = 1;
	for (j = 0; j < sort_count; j++) {
		comp_val = (*(compare_closure[j]))(vals_closure[cols_closure * row1 + j], vals_closure[cols_closure * row2 + j]);
		ascending = ascending_closure[j];
		if (comp_val != 0)
			break;
	}
	if (comp_val == 0) {
		if (row1 < row2)
			comp_val = -1;
		if (row1 > row2)
			comp_val = 1;
	}
	if (!ascending)
		comp_val = -comp_val;
	return comp_val;
}

/* if sortgroup is like:
0   1  	     1 	1
1   1	     2 	  2
2    2	     3 	  2
3    2	     4 	   3
4     3	     5 	  3
5    2 	     6 	1
6   1	     0	1

   Want to sort the 1's first
   Then sort each group of 2's, offsetting into the output by the new root 1 location
   ... Recursively ...
*/

struct _group_info {
	char *group;
	int row;
};

#ifdef DEBUG
#undef DEBUG
#endif
/*#define DEBUG*/

#ifdef DEBUG
static int total=0;
static int total_sorted=0;
#endif

/* builds the info needed to sort everything */
static struct _subinfo *
etsv_sort_build_subset(ETableSortedVariable *etsv, struct _group_info *groupinfo, int start, int *end)
{
/*	ETableSubset *etss = E_TABLE_SUBSET(etsv);*/
	int rows = E_TABLE_SUBSET(etsv)->n_map;
	int i, lastinsert;
	GArray *rowsort = g_array_new(0, 0, sizeof(struct _rowinfo));
	struct _subinfo *subinfo, *newsub;
	char *id, *newid;
	int idlen, newidlen;
	int cmp;
	int cmplen;

	subinfo = g_malloc0(sizeof(*subinfo));
	subinfo->rowsort = rowsort;
	subinfo->start = start;
	lastinsert = -1;
	id = groupinfo[start].group;
	newid = strrchr(id, '/');
	idlen = strlen(id);
	if (newid)
		cmplen = newid-id;
	else
		cmplen = idlen;
	d(printf("%d scanning level %s\n", start, id));
	for (i=start;i<rows;i++) {
		newid = groupinfo[i].group;
		newidlen = strlen(newid);
		d(printf("%d checking group %s\n", start, newid));
		cmp = strncmp(id, newid, cmplen);
		/* check for common parent */
		if (idlen == newidlen && cmp == 0) {
			struct _rowinfo rowinfo;

			d(printf("%d Same parent\n", start));
			rowinfo.row = groupinfo[i].row;
			rowinfo.subinfo = NULL;
			rowinfo.groupinfo = &groupinfo[i];
			lastinsert = rowsort->len;
			g_array_append_val(rowsort, rowinfo);
#ifdef DEBUG
			total++;
#endif
		} else if (newidlen > idlen) {
			/* must be a new subtree */
			d(printf("%d checking subtree instead\n", start));
			newsub = etsv_sort_build_subset(etsv, groupinfo, i, &i);
			d(printf("found %d nodes in subtree\n", newsub->rowsort->len));
			g_array_index(rowsort, struct _rowinfo, lastinsert).subinfo = newsub;
		} else {
			i--;
			break;
		}
	}
	if (end)
		*end = i;
	d(printf("finished level %s start was %d end was %d\n", id, start, i));
	return subinfo;
}

/* sort each level, and then sort each level below that level (once we know
   where the sublevel will fit in the overall list) */
static int
etsv_sort_subset(ETableSortedVariable *etsv, struct _subinfo *subinfo, int startoffset)
{
	GArray *rowsort = subinfo->rowsort;
	ETableSubset *etss = E_TABLE_SUBSET(etsv);
	int offset, i;

	d(printf("sorting subset start %d rows %d\n", startoffset, rowsort->len));

	/* first, sort the actual data */
	qsort(rowsort->data, rowsort->len, sizeof(struct _rowinfo), qsort_callback_complex);

	/* then put it back in the map table, where appropriate */
	offset = startoffset;
	for (i=0;i<rowsort->len;i++) {
		struct _rowinfo *rowinfo;

		d(printf("setting offset %d\n", offset));

		rowinfo = &g_array_index(rowsort, struct _rowinfo, i);
		etss->map_table[offset] = rowinfo->row;
		if (rowinfo->subinfo) {
			offset = etsv_sort_subset(etsv, rowinfo->subinfo, offset+1);
		} else
			offset += 1;
	}
	d(printf("end sort subset start %d\n", startoffset));

	return offset;
}

static void
etsv_sort_free_subset(ETableSortedVariable *etsv, struct _subinfo *subinfo)
{
	int i;

	for (i=0;i<subinfo->rowsort->len;i++) {
		struct _rowinfo *rowinfo;

		rowinfo = &g_array_index(subinfo->rowsort, struct _rowinfo, i);
		if (rowinfo->subinfo)
			etsv_sort_free_subset(etsv, rowinfo->subinfo);
	}
	g_array_free(subinfo->rowsort, TRUE);
	g_free(subinfo);
}

static int
sort_groups_compare(const void *ap, const void *bp)
{
	struct _group_info *a = (struct _group_info *)ap;
	struct _group_info *b = (struct _group_info *)bp;

	return strcmp(a->group, b->group);
}

#ifdef DEBUG
static void
print_id(int key, int val, void *data)
{
	printf("gained id %d\n", key);
}
#endif

/* use the sort group to select subsorts */
static void
etsv_sort_by_group(ETableSortedVariable *etsv)
{
	ETableSubset *etss = E_TABLE_SUBSET(etsv);
	int rows = E_TABLE_SUBSET(etsv)->n_map;
	struct _group_info *groups;
	struct _subinfo *subinfo;
	int i;
#ifdef DEBUG
	GHashTable *members = g_hash_table_new(0, 0);

	total = 0;
	total_sorted = 0;
#endif

	d(printf("sorting %d rows\n", rows));

	if (rows == 0)
		return;

	/* get the subset rows */
	groups = g_malloc(sizeof(struct _group_info) * rows);
	for (i=0;i<rows;i++) {
		groups[i].row = etss->map_table[i];
		groups[i].group = g_strdup(e_table_model_row_sort_group(etss->source, groups[i].row));
#ifdef DEBUG
		g_hash_table_insert(members, etss->map_table[i], 1);
		etss->map_table[i] = 0;
#endif
	}

	/* sort the group info */
	qsort(groups, rows, sizeof(struct _group_info), sort_groups_compare);

	d(printf("sorted groups:\n");
	for (i=0;i<rows;i++) {
		printf(" %s\n", groups[i].group);
	});
	
	/* now sort based on the group info */
	subinfo = etsv_sort_build_subset(etsv, groups, 0, NULL);
	for (i=0;i<rows;i++) {
		g_free(groups[i].group);
	}
	g_free(groups);
	etsv_sort_subset(etsv, subinfo, 0);
	etsv_sort_free_subset(etsv, subinfo);
#ifdef DEBUG
	for (i=0;i<rows;i++) {
		if (g_hash_table_lookup(members, etss->map_table[i]) == 0) {
			printf("lost id %d\n", etss->map_table[i]);
		}
		g_hash_table_remove(members, etss->map_table[i]);
	}
	g_hash_table_foreach(members, print_id, 0);

	printf("total rows = %d, total processed = %d, total sorted = %d\n", rows, total, total_sorted);
#endif

}

static void
etsv_sort(ETableSortedVariable *etsv)
{
	ETableSubset *etss = E_TABLE_SUBSET(etsv);
	static int reentering = 0;
	int rows = E_TABLE_SUBSET(etsv)->n_map;
	int total_rows;
	int i;
	int j;
	int cols;
	if (reentering)
		return;
	reentering = 1;

	e_table_model_pre_change(E_TABLE_MODEL(etsv));

	total_rows = e_table_model_row_count(E_TABLE_SUBSET(etsv)->source);
	cols = e_table_sort_info_sorting_get_count(etsv->sort_info);
	cols_closure = cols;
	etsv_closure = etsv;
	vals_closure = g_new(void *, total_rows * cols);
	ascending_closure = g_new(int, cols);
	compare_closure = g_new(GCompareFunc, cols);
	for (j = 0; j < cols; j++) {
		ETableSortColumn column = e_table_sort_info_sorting_get_nth(etsv->sort_info, j);
		ETableCol *col;
		if (column.column > e_table_header_count (etsv->full_header))
			col = e_table_header_get_column (etsv->full_header, e_table_header_count (etsv->full_header) - 1);
		else
			col = e_table_header_get_column (etsv->full_header, column.column);
		for (i = 0; i < rows; i++) {
#if 0
			if( !(i & 0xff) ) {
				while(gtk_events_pending())
					gtk_main_iteration();
			}
#endif
			vals_closure[E_TABLE_SUBSET(etsv)->map_table[i] * cols + j] = e_table_model_value_at (etss->source, col->col_idx, E_TABLE_SUBSET(etsv)->map_table[i]);
		}
		compare_closure[j] = col->compare;
		ascending_closure[j] = column.ascending;
	}

	if (e_table_model_has_sort_group(etss->source)) {
		etsv_sort_by_group(etsv);
	} else {
		qsort(E_TABLE_SUBSET(etsv)->map_table, rows, sizeof(int), qsort_callback);
	}
	g_free(vals_closure);
	g_free(ascending_closure);
	g_free(compare_closure);
	e_table_model_changed (E_TABLE_MODEL(etsv));
	reentering = 0;
}
