/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <string.h>
#include <e-table-sorting-utils.h>

#define d(x)

/* This takes source rows. */
static int
etsu_compare(ETableModel *source, ETableSortInfo *sort_info, ETableHeader *full_header, int row1, int row2)
{
	int j;
	int sort_count = e_table_sort_info_sorting_get_count(sort_info);
	int comp_val = 0;
	int ascending = 1;

	for (j = 0; j < sort_count; j++) {
		ETableSortColumn column = e_table_sort_info_sorting_get_nth(sort_info, j);
		ETableCol *col;
		col = e_table_header_get_column_by_col_idx(full_header, column.column);
		if (col == NULL)
			col = e_table_header_get_column (full_header, e_table_header_count (full_header) - 1);
		comp_val = (*col->compare)(e_table_model_value_at (source, col->col_idx, row1),
					   e_table_model_value_at (source, col->col_idx, row2));
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

static ETableSortInfo *sort_info_closure;

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
	int sort_count = e_table_sort_info_sorting_get_count(sort_info_closure);
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
	int sort_count = e_table_sort_info_sorting_get_count(sort_info_closure);
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
etsu_sort_build_subset(int rows, struct _group_info *groupinfo, int start, int *end)
{
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
			newsub = etsu_sort_build_subset(rows, groupinfo, i, &i);
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
etsu_sort_subset(int *map_table, struct _subinfo *subinfo, int startoffset)
{
	GArray *rowsort = subinfo->rowsort;
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
		map_table[offset] = rowinfo->row;
		if (rowinfo->subinfo) {
			offset = etsu_sort_subset(map_table, rowinfo->subinfo, offset+1);
		} else
			offset += 1;
	}
	d(printf("end sort subset start %d\n", startoffset));

	return offset;
}

static void
etsu_sort_free_subset(struct _subinfo *subinfo)
{
	int i;

	for (i=0;i<subinfo->rowsort->len;i++) {
		struct _rowinfo *rowinfo;

		rowinfo = &g_array_index(subinfo->rowsort, struct _rowinfo, i);
		if (rowinfo->subinfo)
			etsu_sort_free_subset(rowinfo->subinfo);
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
etsu_sort_by_group(ETableModel *source, int *map_table, int rows)
{
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
		groups[i].row = map_table[i];
		groups[i].group = g_strdup(e_table_model_row_sort_group(source, groups[i].row));
#ifdef DEBUG
		g_hash_table_insert(members, map_table[i], 1);
		map_table[i] = 0;
#endif
	}

	/* sort the group info */
	qsort(groups, rows, sizeof(struct _group_info), sort_groups_compare);

	d(printf("sorted groups:\n");
	for (i=0;i<rows;i++) {
		printf(" %s\n", groups[i].group);
	});
	
	/* now sort based on the group info */
	subinfo = etsu_sort_build_subset(rows, groups, 0, NULL);
	for (i=0;i<rows;i++) {
		g_free(groups[i].group);
	}
	g_free(groups);
	etsu_sort_subset(map_table, subinfo, 0);
	etsu_sort_free_subset(subinfo);
#ifdef DEBUG
	for (i=0;i<rows;i++) {
		if (g_hash_table_lookup(members, map_table[i]) == 0) {
			printf("lost id %d\n", map_table[i]);
		}
		g_hash_table_remove(members, map_table[i]);
	}
	g_hash_table_foreach(members, print_id, 0);

	printf("total rows = %d, total processed = %d, total sorted = %d\n", rows, total, total_sorted);
#endif

}

void
e_table_sorting_utils_sort(ETableModel *source, ETableSortInfo *sort_info, ETableHeader *full_header, int *map_table, int rows)
{
	int total_rows;
	int i;
	int j;
	int cols;

	g_return_if_fail(source != NULL);
	g_return_if_fail(E_IS_TABLE_MODEL(source));
	g_return_if_fail(sort_info != NULL);
	g_return_if_fail(E_IS_TABLE_SORT_INFO(sort_info));
	g_return_if_fail(full_header != NULL);
	g_return_if_fail(E_IS_TABLE_HEADER(full_header));

	total_rows = e_table_model_row_count(source);
	cols = e_table_sort_info_sorting_get_count(sort_info);
	cols_closure = cols;
	vals_closure = g_new(void *, total_rows * cols);
	sort_info_closure = sort_info;
	ascending_closure = g_new(int, cols);
	compare_closure = g_new(GCompareFunc, cols);
	for (j = 0; j < cols; j++) {
		ETableSortColumn column = e_table_sort_info_sorting_get_nth(sort_info, j);
		ETableCol *col;
		col = e_table_header_get_column_by_col_idx(full_header, column.column);
		if (col == NULL)
			col = e_table_header_get_column (full_header, e_table_header_count (full_header) - 1);
		for (i = 0; i < rows; i++) {
			vals_closure[map_table[i] * cols + j] = e_table_model_value_at (source, col->col_idx, map_table[i]);
		}
		compare_closure[j] = col->compare;
		ascending_closure[j] = column.ascending;
	}

	if (e_table_model_has_sort_group(source)) {
		etsu_sort_by_group(source, map_table, rows);
	} else {
		qsort(map_table, rows, sizeof(int), qsort_callback);
	}
	g_free(vals_closure);
	g_free(ascending_closure);
	g_free(compare_closure);
}

gboolean
e_table_sorting_utils_affects_sort  (ETableModel    *source,
				     ETableSortInfo *sort_info,
				     ETableHeader   *full_header,
				     int             col)
{
	int j;
	int cols;

	g_return_val_if_fail(source != NULL, TRUE);
	g_return_val_if_fail(E_IS_TABLE_MODEL(source), TRUE);
	g_return_val_if_fail(sort_info != NULL, TRUE);
	g_return_val_if_fail(E_IS_TABLE_SORT_INFO(sort_info), TRUE);
	g_return_val_if_fail(full_header != NULL, TRUE);
	g_return_val_if_fail(E_IS_TABLE_HEADER(full_header), TRUE);

	cols = e_table_sort_info_sorting_get_count(sort_info);

	for (j = 0; j < cols; j++) {
		ETableSortColumn column = e_table_sort_info_sorting_get_nth(sort_info, j);
		ETableCol *tablecol;
		tablecol = e_table_header_get_column_by_col_idx(full_header, column.column);
		if (tablecol == NULL)
			tablecol = e_table_header_get_column (full_header, e_table_header_count (full_header) - 1);
		if (col == tablecol->col_idx)
			return TRUE;
	}
	return FALSE;
}


int
e_table_sorting_utils_insert(ETableModel *source, ETableSortInfo *sort_info, ETableHeader *full_header, int *map_table, int rows, int row)
{
	int i;

	i = 0;
	/* handle insertions when we have a 'sort group' */
	if (e_table_model_has_sort_group(source)) {
		/* find the row this row maps to */
		char *group = g_strdup(e_table_model_row_sort_group(source, row));
		const char *newgroup;
		int cmp, grouplen, newgrouplen;
			
		newgroup = strrchr(group, '/');
		grouplen = strlen(group);
		if (newgroup)
			cmp = newgroup-group;
		else
			cmp = grouplen;
				
		/* find first common parent */
		while (i < rows) {
			newgroup = e_table_model_row_sort_group(source, map_table[i]);
			if (strncmp(newgroup, group, cmp) == 0) {
				break;
			}
			i++;
		}

				/* check matching records */
		while (i<row) {
			newgroup = e_table_model_row_sort_group(source, map_table[i]);
			newgrouplen = strlen(newgroup);
			if (strncmp(newgroup, group, cmp) == 0) {
				/* common parent, check for same level */
				if (grouplen == newgrouplen) {
					if (etsu_compare(source, sort_info, full_header, map_table[i], row) >= 0)
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
		while (i < rows && etsu_compare(source, sort_info, full_header, map_table[i], row) < 0)
			i++;
	}

	return i;
}

#if 0
void *bsearch(const void *key, const void *base, size_t nmemb,
              size_t size, int (*compar)(const void *, const void *, void *), gpointer user_data)
{
	
}

int
e_table_sorting_utils_check_position (ETableModel *source, ETableSortInfo *sort_info, ETableHeader *full_header, int *map_table, int rows, int view_row)
{
	int i;
	int row;

	i = view_row;
	row = map_table[i];
	/* handle insertions when we have a 'sort group' */
	if (e_table_model_has_sort_group(source)) {
		/* find the row this row maps to */
		char *group = g_strdup(e_table_model_row_sort_group(source, row));
		const char *newgroup;
		int cmp, grouplen, newgrouplen;
			
		newgroup = strrchr(group, '/');
		grouplen = strlen(group);
		if (newgroup)
			cmp = newgroup-group;
		else
			cmp = grouplen;
				
		/* find first common parent */
		while (i < rows) {
			newgroup = e_table_model_row_sort_group(source, map_table[i]);
			if (strncmp(newgroup, group, cmp) == 0) {
				break;
			}
			i++;
		}

		/* check matching records */
		while (i < row) {
			newgroup = e_table_model_row_sort_group(source, map_table[i]);
			newgrouplen = strlen(newgroup);
			if (strncmp(newgroup, group, cmp) == 0) {
				/* common parent, check for same level */
				if (grouplen == newgrouplen) {
					if (etsu_compare(source, sort_info, full_header, map_table[i], row) >= 0)
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
		i = view_row;
		if (i < rows && etsu_compare(source, sort_info, full_header, map_table[i + 1], row) < 0) {
			i ++;
			while (i < rows - 1 && etsu_compare(source, sort_info, full_header, map_table[i], row) < 0)
				i ++;
		} else if (i > 0 && etsu_compare(source, sort_info, full_header, map_table[i - 1], row) > 0) {
			i --;
			while (i > 0 && etsu_compare(source, sort_info, full_header, map_table[i], row) > 0)
				i --;
		}
	}
	return i;
}
#endif
