/*
 * e-table-model.h
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

#ifndef E_TABLE_MODEL_H
#define E_TABLE_MODEL_H

#include <glib-object.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_MODEL \
	(e_table_model_get_type ())
#define E_TABLE_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_MODEL, ETableModel))
#define E_IS_TABLE_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_MODEL))
#define E_TABLE_MODEL_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_TABLE_MODEL, ETableModelInterface))

G_BEGIN_DECLS

typedef struct _ETableModel ETableModel;
typedef struct _ETableModelInterface ETableModelInterface;

struct _ETableModelInterface {
	GTypeInterface parent_interface;

	gint		(*column_count)		(ETableModel *table_model);
	gint		(*row_count)		(ETableModel *table_model);
	void		(*append_row)		(ETableModel *table_model,
						 ETableModel *source,
						 gint row);

	gpointer	(*value_at)		(ETableModel *table_model,
						 gint col,
						 gint row);
	void		(*set_value_at)		(ETableModel *table_model,
						 gint col,
						 gint row,
						 gconstpointer value);
	gboolean	(*is_cell_editable)	(ETableModel *table_model,
						 gint col,
						 gint row);

	gboolean	(*has_save_id)		(ETableModel *table_model);
	gchar *		(*get_save_id)		(ETableModel *table_model,
						 gint row);

	gboolean	(*has_change_pending)	(ETableModel *table_model);

	/* Allocate a copy of the given value. */
	gpointer	(*duplicate_value)	(ETableModel *table_model,
						 gint col,
						 gconstpointer value);
	/* Free an allocated value. */
	void		(*free_value)		(ETableModel *table_model,
						 gint col,
						 gpointer value);
	/* Return an allocated empty value. */
	gpointer	(*initialize_value)	(ETableModel *table_model,
						 gint col);
	/* Return TRUE if value is equivalent to an empty cell. */
	gboolean	(*value_is_empty)	(ETableModel *table_model,
						 gint col,
						 gconstpointer value);
	/* Return an allocated string. */
	gchar *		(*value_to_string)	(ETableModel *table_model,
						 gint col,
						 gconstpointer value);

	/*
	 * Signals
	 */

	/*
	 * These all come after the change has been made.
	 * No changes, cancel pre_change: no_change
	 * Major structural changes: model_changed
	 * Changes only in a row: row_changed
	 * Only changes in a cell: cell_changed
	 * A row inserted: row_inserted
	 * A row deleted: row_deleted
	 */
	void		(*model_pre_change)	(ETableModel *table_model);

	void		(*model_no_change)	(ETableModel *table_model);
	void		(*model_changed)	(ETableModel *table_model);
	void		(*model_row_changed)	(ETableModel *table_model,
						 gint row);
	void		(*model_cell_changed)	(ETableModel *table_model,
						 gint col,
						 gint row);
	void		(*model_rows_inserted)	(ETableModel *table_model,
						 gint row,
						 gint count);
	void		(*model_rows_deleted)	(ETableModel *table_model,
						 gint row,
						 gint count);
};

GType		e_table_model_get_type		(void) G_GNUC_CONST;

/**/
gint		e_table_model_column_count	(ETableModel *table_model);
const gchar *	e_table_model_column_name	(ETableModel *table_model,
						 gint col);
gint		e_table_model_row_count		(ETableModel *table_model);
void		e_table_model_append_row	(ETableModel *table_model,
						 ETableModel *source,
						 gint row);

/**/
gpointer	e_table_model_value_at		(ETableModel *table_model,
						 gint col,
						 gint row);
void		e_table_model_set_value_at	(ETableModel *table_model,
						 gint col,
						 gint row,
						 gconstpointer value);
gboolean	e_table_model_is_cell_editable	(ETableModel *table_model,
						gint col,
						 gint row);

/**/
gboolean	e_table_model_has_save_id	(ETableModel *table_model);
gchar *		e_table_model_get_save_id	(ETableModel *table_model,
						 gint row);

/**/
gboolean	e_table_model_has_change_pending
						(ETableModel *table_model);

/**/
gpointer	e_table_model_duplicate_value	(ETableModel *table_model,
						 gint col,
						 gconstpointer value);
void		e_table_model_free_value	(ETableModel *table_model,
						 gint col,
						 gpointer value);
gpointer	e_table_model_initialize_value	(ETableModel *table_model,
						 gint col);
gboolean	e_table_model_value_is_empty	(ETableModel *table_model,
						 gint col,
						 gconstpointer value);
gchar *		e_table_model_value_to_string	(ETableModel *table_model,
						 gint col,
						 gconstpointer value);

/*
 * Routines for emitting signals on the e_table
 */
void		e_table_model_pre_change	(ETableModel *table_model);
void		e_table_model_no_change		(ETableModel *table_model);
void		e_table_model_changed		(ETableModel *table_model);
void		e_table_model_row_changed	(ETableModel *table_model,
						 gint row);
void		e_table_model_cell_changed	(ETableModel *table_model,
						 gint col,
						 gint row);
void		e_table_model_rows_inserted	(ETableModel *table_model,
						 gint row,
						 gint count);
void		e_table_model_rows_deleted	(ETableModel *table_model,
						 gint row,
						 gint count);

/**/
void		e_table_model_row_inserted	(ETableModel *table_model,
						 gint row);
void		e_table_model_row_deleted	(ETableModel *table_model,
						 gint row);

void		e_table_model_freeze		(ETableModel *table_model);
void		e_table_model_thaw		(ETableModel *table_model);

G_END_DECLS

#endif /* E_TABLE_MODEL_H */
