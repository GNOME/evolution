/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-extras.c: Pair of hash table sort of thingies.
 *
 * Author:
 *   Chris Lahey <clahey@ximian.com>
 *
 * (C) 2000 Ximian, Inc.
 */
#include <config.h>
#include <stdlib.h>
#include <gtk/gtksignal.h>
#include "gal/util/e-util.h"
#include "gal/e-table/e-cell-text.h"
#include "gal/e-table/e-cell-checkbox.h"
#include "gal/e-table/e-cell-date.h"
#include "gal/e-table/e-cell-number.h"
#include "gal/e-table/e-cell-pixbuf.h"
#include "gal/e-table/e-cell-size.h"
#include "gal/e-table/e-cell-tree.h"
#include "e-table-extras.h"

#define PARENT_TYPE (gtk_object_get_type())

static GtkObjectClass *ete_parent_class;

static void
cell_hash_free(gchar	*key,
	       ECell    *cell,
	       gpointer	user_data)
{
	g_free(key);
	if (cell)
		gtk_object_unref( GTK_OBJECT (cell));
}

static void
pixbuf_hash_free(gchar	*key,
		 GdkPixbuf *pixbuf,
		 gpointer	user_data)
{
	g_free(key);
	if (pixbuf)
		gdk_pixbuf_unref(pixbuf);
}

static void
ete_destroy (GtkObject *object)
{
	ETableExtras *ete = E_TABLE_EXTRAS (object);

	g_hash_table_foreach (ete->cells, (GHFunc) cell_hash_free, NULL);
	g_hash_table_foreach (ete->compares, (GHFunc) g_free, NULL);
	g_hash_table_foreach (ete->pixbufs, (GHFunc) pixbuf_hash_free, NULL);

	g_hash_table_destroy (ete->cells);
	g_hash_table_destroy (ete->compares);
	g_hash_table_destroy (ete->pixbufs);

	ete->cells = NULL;
	ete->compares = NULL;
	ete->pixbufs = NULL;

	GTK_OBJECT_CLASS (ete_parent_class)->destroy (object);
}

static void
ete_class_init (GtkObjectClass *klass)
{
	ete_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->destroy = ete_destroy;
}

static gint
e_strint_compare(gconstpointer data1, gconstpointer data2)
{
	int int1 = atoi(data1);
	int int2 = atoi(data2);

	return g_int_compare(GINT_TO_POINTER(int1), GINT_TO_POINTER(int2));
}

static void
ete_init (ETableExtras *extras)
{
	extras->cells = g_hash_table_new(g_str_hash, g_str_equal);
	extras->compares = g_hash_table_new(g_str_hash, g_str_equal);
	extras->pixbufs = g_hash_table_new(g_str_hash, g_str_equal);

	e_table_extras_add_compare(extras, "string", g_str_compare);
	e_table_extras_add_compare(extras, "integer", g_int_compare);
	e_table_extras_add_compare(extras, "string-integer", e_strint_compare);

	e_table_extras_add_cell(extras, "checkbox", e_cell_checkbox_new());
	e_table_extras_add_cell(extras, "date", e_cell_date_new (NULL, GTK_JUSTIFY_LEFT));
	e_table_extras_add_cell(extras, "number", e_cell_number_new (NULL, GTK_JUSTIFY_RIGHT));
	e_table_extras_add_cell(extras, "pixbuf", e_cell_pixbuf_new ());
	e_table_extras_add_cell(extras, "size", e_cell_size_new (NULL, GTK_JUSTIFY_RIGHT));
	e_table_extras_add_cell(extras, "string", e_cell_text_new (NULL, GTK_JUSTIFY_LEFT));
	e_table_extras_add_cell(extras, "tree-string", e_cell_tree_new (NULL, NULL, TRUE, e_cell_text_new (NULL, GTK_JUSTIFY_LEFT)));
}

E_MAKE_TYPE(e_table_extras, "ETableExtras", ETableExtras, ete_class_init, ete_init, PARENT_TYPE);

ETableExtras *
e_table_extras_new (void)
{
	ETableExtras *ete = gtk_type_new (E_TABLE_EXTRAS_TYPE);

	return (ETableExtras *) ete;
}

void
e_table_extras_add_cell     (ETableExtras *extras,
			     char         *id,
			     ECell        *cell)
{
	gchar *old_key;
	ECell *old_cell;

	if (g_hash_table_lookup_extended (extras->cells, id, (gpointer *)&old_key, (gpointer *)&old_cell)) {
		g_hash_table_remove (extras->cells, old_key);
		g_free (old_key);
		if (old_cell)
			gtk_object_unref (GTK_OBJECT(old_cell));
	}

	if (cell) {
		gtk_object_ref (GTK_OBJECT (cell));
		gtk_object_sink (GTK_OBJECT (cell));
	}
	g_hash_table_insert (extras->cells, g_strdup(id), cell);
}

ECell *
e_table_extras_get_cell     (ETableExtras *extras,
			     char         *id)
{
	return g_hash_table_lookup(extras->cells, id);
}

void
e_table_extras_add_compare  (ETableExtras *extras,
			     char         *id,
			     GCompareFunc  compare)
{
	gchar *old_key;
	GCompareFunc old_compare;

	if (g_hash_table_lookup_extended (extras->cells, id, (gpointer *)&old_key, (gpointer *)&old_compare)) {
		g_hash_table_remove (extras->cells, old_key);
		g_free (old_key);
	}

	g_hash_table_insert(extras->compares, g_strdup(id), compare);
}

GCompareFunc
e_table_extras_get_compare  (ETableExtras *extras,
			     char         *id)
{
	return g_hash_table_lookup(extras->compares, id);
}

void
e_table_extras_add_pixbuf     (ETableExtras *extras,
			       char         *id,
			       GdkPixbuf    *pixbuf)
{
	gchar *old_key;
	GdkPixbuf *old_pixbuf;

	if (g_hash_table_lookup_extended (extras->pixbufs, id, (gpointer *)&old_key, (gpointer *)&old_pixbuf)) {
		g_hash_table_remove (extras->cells, old_key);
		g_free (old_key);
		if (old_pixbuf)
			gdk_pixbuf_unref (old_pixbuf);
	}

	if (pixbuf)
		gdk_pixbuf_ref(pixbuf);
	g_hash_table_insert (extras->pixbufs, g_strdup(id), pixbuf);
}

GdkPixbuf *
e_table_extras_get_pixbuf     (ETableExtras *extras,
			       char         *id)
{
	return g_hash_table_lookup(extras->pixbufs, id);
}
