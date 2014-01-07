/*
 * e-table-column-selector.h
 *
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
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TABLE_COLUMN_SELECTOR_H
#define E_TABLE_COLUMN_SELECTOR_H

#include <e-util/e-table-state.h>
#include <e-util/e-tree-view-frame.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_COLUMN_SELECTOR \
	(e_table_column_selector_get_type ())
#define E_TABLE_COLUMN_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_COLUMN_SELECTOR, ETableColumnSelector))
#define E_TABLE_COLUMN_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_COLUMN_SELECTOR, ETableColumnSelectorClass))
#define E_IS_TABLE_COLUMN_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_COLUMN_SELECTOR))
#define E_IS_TABLE_COLUMN_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_COLUMN_SELECTOR))
#define E_TABLE_COLUMN_SELECTOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_COLUMN_SELECTOR, ETableColumnSelectorClass))

G_BEGIN_DECLS

typedef struct _ETableColumnSelector ETableColumnSelector;
typedef struct _ETableColumnSelectorClass ETableColumnSelectorClass;
typedef struct _ETableColumnSelectorPrivate ETableColumnSelectorPrivate;

/**
 * ETableColumnSelector:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _ETableColumnSelector {
	ETreeViewFrame parent;
	ETableColumnSelectorPrivate *priv;
};

struct _ETableColumnSelectorClass {
	ETreeViewFrameClass parent_class;
};

GType		e_table_column_selector_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_table_column_selector_new
					(ETableState *state);
ETableState *	e_table_column_selector_get_state
					(ETableColumnSelector *selector);
void		e_table_column_selector_apply
					(ETableColumnSelector *selector);

G_END_DECLS

#endif /* E_TABLE_COLUMN_SELECTOR_H */

