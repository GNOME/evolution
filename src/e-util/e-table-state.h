/*
 * e-table-state.h
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

#ifndef E_TABLE_STATE_H
#define E_TABLE_STATE_H

#include <libxml/tree.h>

#include <e-util/e-table-sort-info.h>
#include <e-util/e-table-column-specification.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_STATE \
	(e_table_state_get_type ())
#define E_TABLE_STATE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_STATE, ETableState))
#define E_TABLE_STATE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_STATE, ETableStateClass))
#define E_IS_TABLE_STATE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_STATE))
#define E_IS_TABLE_STATE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_STATE))
#define E_TABLE_STATE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_STATE, ETableStateClass))

G_BEGIN_DECLS

/* Avoid a circular dependency. */
struct _ETableSpecification;

typedef struct _ETableState ETableState;
typedef struct _ETableStateClass ETableStateClass;
typedef struct _ETableStatePrivate ETableStatePrivate;

struct _ETableState {
	GObject parent;
	ETableStatePrivate *priv;

	ETableSortInfo *sort_info;
	gint col_count;
	ETableColumnSpecification **column_specs;
	gdouble *expansions;
};

struct _ETableStateClass {
	GObjectClass parent_class;
};

GType		e_table_state_get_type		(void) G_GNUC_CONST;
ETableState *	e_table_state_new		(struct _ETableSpecification *specification);
ETableState *	e_table_state_vanilla		(struct _ETableSpecification *specification);
void		e_table_state_parse_context_push
						(GMarkupParseContext *context,
						 struct _ETableSpecification *specification);
ETableState *	e_table_state_parse_context_pop	(GMarkupParseContext *context);
struct _ETableSpecification *
		e_table_state_ref_specification	(ETableState *state);
gboolean	e_table_state_load_from_file	(ETableState *state,
						 const gchar *filename);
void		e_table_state_load_from_string	(ETableState *state,
						 const gchar *xml);
void		e_table_state_load_from_node	(ETableState *state,
						 const xmlNode *node);
void		e_table_state_save_to_file	(ETableState *state,
						 const gchar *filename);
gchar *		e_table_state_save_to_string	(ETableState *state);
xmlNode *	e_table_state_save_to_node	(ETableState *state,
						 xmlNode *parent);
ETableState *	e_table_state_duplicate		(ETableState *state);

G_END_DECLS

#endif /* E_TABLE_STATE_H */
