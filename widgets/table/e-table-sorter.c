/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-sorted.c: Implements a table that sorts another table
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Ximian, Inc.
 */
#include <config.h>
#include <stdlib.h>
#include <gtk/gtksignal.h>
#include <string.h>
#include "gal/util/e-util.h"
#include "e-table-sorter.h"

#define d(x)

/* The arguments we take */
enum {
	ARG_0,
	ARG_SORT_INFO
};

#define PARENT_TYPE gtk_object_get_type()

#define INCREMENT_AMOUNT 100

static GtkObjectClass *parent_class;

static void ets_model_changed      (ETableModel *etm, ETableSorter *ets);
static void ets_model_row_changed  (ETableModel *etm, int row, ETableSorter *ets);
static void ets_model_cell_changed (ETableModel *etm, int col, int row, ETableSorter *ets);
static void ets_sort_info_changed  (ETableSortInfo *info, ETableSorter *ets);
static void ets_clean              (ETableSorter *ets);
static void ets_sort               (ETableSorter *ets);
static void ets_backsort           (ETableSorter *ets);

static void
ets_destroy (GtkObject *object)
{
	ETableSorter *ets = E_TABLE_SORTER (object);

	gtk_signal_disconnect (GTK_OBJECT (ets->source),
			       ets->table_model_changed_id);
	gtk_signal_disconnect (GTK_OBJECT (ets->source),
			       ets->table_model_row_changed_id);
	gtk_signal_disconnect (GTK_OBJECT (ets->source),
			       ets->table_model_cell_changed_id);
	gtk_signal_disconnect (GTK_OBJECT (ets->sort_info),
			       ets->sort_info_changed_id);

	ets->table_model_changed_id = 0;
	ets->table_model_row_changed_id = 0;
	ets->table_model_cell_changed_id = 0;
	ets->sort_info_changed_id = 0;
	
	if (ets->sort_info)
		gtk_object_unref(GTK_OBJECT(ets->sort_info));
	if (ets->full_header)
		gtk_object_unref(GTK_OBJECT(ets->full_header));
	if (ets->source)
		gtk_object_unref(GTK_OBJECT(ets->source));

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
ets_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETableSorter *ets = E_TABLE_SORTER (object);

	switch (arg_id) {
	case ARG_SORT_INFO:
		if (ets->sort_info) {
			if (ets->sort_info_changed_id)
				gtk_signal_disconnect(GTK_OBJECT(ets->sort_info), ets->sort_info_changed_id);
			gtk_object_unref(GTK_OBJECT(ets->sort_info));
		}

		ets->sort_info = E_TABLE_SORT_INFO(GTK_VALUE_OBJECT (*arg));
		ets->sort_info_changed_id = gtk_signal_connect (GTK_OBJECT (ets->sort_info), "sort_info_changed",
								GTK_SIGNAL_FUNC (ets_sort_info_changed), ets);

		ets_clean (ets);
		break;
	default:
		break;
	}
}

static void
ets_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETableSorter *ets = E_TABLE_SORTER (object);
	switch (arg_id) {
	case ARG_SORT_INFO:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(ets->sort_info);
		break;
	}
}

static void
ets_class_init (ETableSorterClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = ets_destroy;
	object_class->set_arg = ets_set_arg;
	object_class->get_arg = ets_get_arg;

	gtk_object_add_arg_type ("ETableSorter::sort_info", GTK_TYPE_OBJECT, 
				 GTK_ARG_READWRITE, ARG_SORT_INFO); 
}

static void
ets_init (ETableSorter *ets)
{
	ets->full_header = NULL;
	ets->sort_info = NULL;
	ets->source = NULL;

	ets->needs_sorting = -1;

	ets->table_model_changed_id = 0;
	ets->table_model_row_changed_id = 0;
	ets->table_model_cell_changed_id = 0;
	ets->sort_info_changed_id = 0;
}

E_MAKE_TYPE(e_table_sorter, "ETableSorter", ETableSorter, ets_class_init, ets_init, PARENT_TYPE);

ETableSorter *
e_table_sorter_new (ETableModel *source, ETableHeader *full_header, ETableSortInfo *sort_info)
{
	ETableSorter *ets = gtk_type_new (E_TABLE_SORTER_TYPE);
	
	ets->sort_info = sort_info;
	gtk_object_ref(GTK_OBJECT(ets->sort_info));
	ets->full_header = full_header;
	gtk_object_ref(GTK_OBJECT(ets->full_header));
	ets->source = source;
	gtk_object_ref(GTK_OBJECT(ets->source));

	ets->table_model_changed_id = gtk_signal_connect (GTK_OBJECT (source), "model_changed",
							   GTK_SIGNAL_FUNC (ets_model_changed), ets);
	ets->table_model_row_changed_id = gtk_signal_connect (GTK_OBJECT (source), "model_row_changed",
							       GTK_SIGNAL_FUNC (ets_model_row_changed), ets);
	ets->table_model_cell_changed_id = gtk_signal_connect (GTK_OBJECT (source), "model_cell_changed",
								GTK_SIGNAL_FUNC (ets_model_cell_changed), ets);
	ets->sort_info_changed_id = gtk_signal_connect (GTK_OBJECT (sort_info), "sort_info_changed",
							 GTK_SIGNAL_FUNC (ets_sort_info_changed), ets);
	
	return ets;
}

static void
ets_model_changed (ETableModel *etm, ETableSorter *ets)
{
	ets_clean(ets);
}

static void
ets_model_row_changed (ETableModel *etm, int row, ETableSorter *ets)
{
	ets_clean(ets);
}

static void
ets_model_cell_changed (ETableModel *etm, int col, int row, ETableSorter *ets)
{
	ets_clean(ets);
}

static void
ets_sort_info_changed (ETableSortInfo *info, ETableSorter *ets)
{
	printf ("sort info changed\n");
	ets_clean(ets);
}

static ETableSorter *ets_closure;
static void **vals_closure;
static int cols_closure;
static int *ascending_closure;
static GCompareFunc *compare_closure;

/* FIXME: Make it not cache the second and later columns (as if anyone cares.) */

static int
qsort_callback(const void *data1, const void *data2)
{
	gint row1 = *(int *)data1;
	gint row2 = *(int *)data2;
	int j;
	int sort_count = e_table_sort_info_sorting_get_count(ets_closure->sort_info) + e_table_sort_info_grouping_get_count(ets_closure->sort_info);
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

static void
ets_clean(ETableSorter *ets)
{
	g_free(ets->sorted);
	ets->sorted = NULL;

	g_free(ets->backsorted);
	ets->backsorted = NULL;

	ets->needs_sorting = -1;
}

struct _group_info {
	char *group;
	int row;
};

struct _rowinfo {
	int row;
	struct _subinfo *subinfo;
	struct _group_info *groupinfo;
};

struct _subinfo {
	int start;
	GArray *rowsort;	/* an array of row info's */
};

/* builds the info needed to sort everything */
static struct _subinfo *
ets_sort_build_subset(ETableSorter *ets, struct _group_info *groupinfo, int start, int *end)
{
	int rows = e_table_model_row_count (ets->source);
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
			newsub = ets_sort_build_subset(ets, groupinfo, i, &i);
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
ets_sort_subset(ETableSorter *ets, struct _subinfo *subinfo, int startoffset)
{
	GArray *rowsort = subinfo->rowsort;
	int offset, i;

	d(printf("sorting subset start %d rows %d\n", startoffset, rowsort->len));

	/* first, sort the actual data */
	qsort(rowsort->data, rowsort->len, sizeof(struct _rowinfo), qsort_callback);

	/* then put it back in the map table, where appropriate */
	offset = startoffset;
	for (i=0;i<rowsort->len;i++) {
		struct _rowinfo *rowinfo;

		d(printf("setting offset %d\n", offset));

		rowinfo = &g_array_index(rowsort, struct _rowinfo, i);
		ets->sorted[offset] = rowinfo->row;
		if (rowinfo->subinfo) {
			offset = ets_sort_subset(ets, rowinfo->subinfo, offset+1);
		} else
			offset += 1;
	}
	d(printf("end sort subset start %d\n", startoffset));

	return offset;
}

static void
ets_sort_free_subset(ETableSorter *ets, struct _subinfo *subinfo)
{
	int i;

	for (i=0;i<subinfo->rowsort->len;i++) {
		struct _rowinfo *rowinfo;

		rowinfo = &g_array_index(subinfo->rowsort, struct _rowinfo, i);
		if (rowinfo->subinfo)
			ets_sort_free_subset(ets, rowinfo->subinfo);
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

/* use the sort group to select subsorts */
static void
ets_sort_by_group (ETableSorter *ets)
{
	int rows = e_table_model_row_count (ets->source);
	struct _group_info *groups;
	struct _subinfo *subinfo;
	int i;

	d(printf("sorting %d rows\n", rows));

	if (rows == 0)
		return;

	/* get all the rows' sort groups */
	groups = g_new(struct _group_info, rows);
	for (i=0;i<rows;i++) {
		groups[i].row = i;
		groups[i].group = g_strdup(e_table_model_row_sort_group(ets->source, groups[i].row));
	}

	/* sort the group info */
	qsort(groups, rows, sizeof(struct _group_info), sort_groups_compare);

	d(printf("sorted groups:\n");
	for (i=0;i<rows;i++) {
		printf(" %s\n", groups[i].group);
	});
	
	/* now sort based on the group info */
	subinfo = ets_sort_build_subset(ets, groups, 0, NULL);
	for (i=0;i<rows;i++) {
		g_free(groups[i].group);
	}
	g_free(groups);
	ets_sort_subset(ets, subinfo, 0);
	ets_sort_free_subset(ets, subinfo);
}

static void
ets_sort(ETableSorter *ets)
{
	int rows;
	int i;
	int j;
	int cols;
	int group_cols;

	if (ets->sorted)
		return;

	rows = e_table_model_row_count(ets->source);
	group_cols = e_table_sort_info_grouping_get_count(ets->sort_info);
	cols = e_table_sort_info_sorting_get_count(ets->sort_info) + group_cols;

	ets->sorted = g_new(int, rows);
	for (i = 0; i < rows; i++)
		ets->sorted[i] = i;

	cols_closure = cols;
	ets_closure = ets;

	vals_closure = g_new(void *, rows * cols);
	ascending_closure = g_new(int, cols);
	compare_closure = g_new(GCompareFunc, cols);

	for (j = 0; j < cols; j++) {
		ETableSortColumn column;
		ETableCol *col;

		if (j < group_cols)
			column = e_table_sort_info_grouping_get_nth(ets->sort_info, j);
		else
			column = e_table_sort_info_sorting_get_nth(ets->sort_info, j - group_cols);

		col = e_table_header_get_column_by_col_idx(ets->full_header, column.column);
		if (col == NULL)
			col = e_table_header_get_column (ets->full_header, e_table_header_count (ets->full_header) - 1);

		for (i = 0; i < rows; i++) {
			vals_closure[i * cols + j] = e_table_model_value_at (ets->source, col->col_idx, i);
		}

		compare_closure[j] = col->compare;
		ascending_closure[j] = column.ascending;
	}

	if (e_table_model_has_sort_group (ets->source)) {
		ets_sort_by_group (ets);
	}
	else {
		qsort(ets->sorted, rows, sizeof(int), qsort_callback);
	}

	g_free(vals_closure);
	g_free(ascending_closure);
	g_free(compare_closure);
}

static void
ets_backsort(ETableSorter *ets)
{
	int i, rows;

	if (ets->backsorted)
		return;

	ets_sort(ets);

	rows = e_table_model_row_count(ets->source);
	ets->backsorted = g_new0(int, rows);

	for (i = 0; i < rows; i++) {
		ets->backsorted[ets->sorted[i]] = i;
	}
}

gboolean
e_table_sorter_needs_sorting(ETableSorter *ets)
{
	if (ets->needs_sorting < 0) {
		if (e_table_sort_info_sorting_get_count(ets->sort_info) + e_table_sort_info_grouping_get_count(ets->sort_info))
			ets->needs_sorting = 1;
		else
			ets->needs_sorting = 0;
	}
	return ets->needs_sorting;
}


gint
e_table_sorter_model_to_sorted (ETableSorter *sorter, int row)
{
	int rows = e_table_model_row_count(sorter->source);

	g_return_val_if_fail(row >= 0, -1);
	g_return_val_if_fail(row < rows, -1);

	if (e_table_sorter_needs_sorting(sorter))
		ets_backsort(sorter);

	if (sorter->backsorted)
		return sorter->backsorted[row];
	else
		return row;
}

gint
e_table_sorter_sorted_to_model (ETableSorter *sorter, int row)
{
	int rows = e_table_model_row_count(sorter->source);

	g_return_val_if_fail(row >= 0, -1);
	g_return_val_if_fail(row < rows, -1);

	if (e_table_sorter_needs_sorting(sorter))
		ets_sort(sorter);

	if (sorter->sorted)
		return sorter->sorted[row];
	else
		return row;
}
