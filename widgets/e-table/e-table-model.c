/*
 * e-table-model.c: a Table Model
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 International GNOME Support.
 */
#include <config.h>
#include "e-table-model.h"

#define ETM_CLASS(e) ((ETableModelClass *)((GtkObject *)e)->klass)

static GtkObjectClass *e_table_model_parent_class;

int
e_table_model_column_count (ETableModel *etable)
{
	return ETM_CLASS (etable)->column_count (etable);
}

const char *
e_table_model_column_name (ETableModel *etable, int col)
{
	return ETM_CLASS (etable)->column_name (etable, col);
}

int
e_table_model_row_count (ETableModel *etable)
{
	return ETM_CLASS (etable)->row_count (etable);
}

void *
e_table_model_value_at (ETableModel *etable, int col, int row)
{
	return ETM_CLASS (etable)->value_at (etable, col, row);
}

void
e_table_model_set_value_at (ETableModel *etable, int col, int row, void *data)
{
	return ETM_CLASS (etable)->set_value_at (etable, col, row, data);
}

gboolean
e_table_model_is_cell_editable (ETableModel *etable, int col, int row)
{
	return ETM_CLASS (etable)->is_cell_editable (etable, col, row);
}

int
e_table_model_row_height (ETableModel *etable, int row)
{
	return ETM_CLASS (etable)->row_height (etable, row);
}

typedef struct {
	ETableModelListenerFn listener;
	void *data;
} listener_t;

void
e_table_model_add_listener (ETableModel *etable, ETableModelListenerFn listener, void *data)
{
	listener_t *lis;

	lis = g_new (listener_t, 1);
	lis->listener_t = listener;
	lis->data = data;
	
	etable->listeners = g_list_prepend (etable->listeners, lis);
}

void
e_table_model_remove_listener (ETableModel *etable, ETableModelListenerFn listener, void *data)
{
	GSList *l;
	
	for (l = etable->listeners; l; l = l->next){
		listener_t *lis = l->data;

		if (lis->listener == listener && lis->data == data){
			etable->listeners = g_list_remove (etable->listeners, lis);
			g_free (lis);
			return;
		}
	}
}

static void
e_table_model_destroy (GtkObject *object)
{
	GSList *l;
	
	ETableModel *etable = (ETableModel *) object;

	for (l = etable->listeners; l; l = l->next)
		g_free (l->data);
	g_list_free (etable->listeners);

	if (e_table_model_parent_class->destroy)
		(*e_table_model_parent_class->destroy)(object);
}

static void
e_table_model_class_init (GtkObjectClass *class)
{
	e_table_model_parent_class = gtk_type_class (gtk_object_get_type ());
	
	class->destroy = e_table_model_destroy;
}

GtkType
e_table_model_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ETableModel",
			sizeof (ETableModel),
			sizeof (ETableModelClass),
			(GtkClassInitFunc) e_table_model_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (gtk_object_get_type (), &info);
	}

	return type;
}

int
e_table_model_height (ETableModel *etable)
{
	int rows;
	int size;
	g_return_val_if_fail (etable != NULL);

	rows = e_table_model_row_count (etable);
	size = 0;
	
	for (i = 0; i < rows; i++)
		size += e_table_model_row_height (etable, i);

	return size;
}

