/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-comp-editor-registry.c
 *
 * Copyright (C) 2002  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: JP Rosevear
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include "e-comp-editor-registry.h"

struct _ECompEditorRegistryPrivate {
	GHashTable *editors;
};

struct _ECompEditorRegistryData
{
	CompEditor *editor;
	char *uid;
};

typedef struct _ECompEditorRegistryData ECompEditorRegistryData;
typedef struct _ECompEditorRegistryForeachData ECompEditorRegistryForeachData;

static GtkObjectClass *parent_class = NULL;

static void editor_destroy_cb (GtkWidget *widget, gpointer data);

static void
destroy (GtkObject *obj)
{
	ECompEditorRegistry *reg;
	ECompEditorRegistryPrivate *priv;
	
	reg = E_COMP_EDITOR_REGISTRY (obj);
	priv = reg->priv;

	g_hash_table_destroy (priv->editors);
	
	g_free (priv);
}

static void
class_init (ECompEditorRegistryClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	parent_class = gtk_type_class (gtk_object_get_type ());

	object_class->destroy = destroy;
}

static void
init (ECompEditorRegistry *reg)
{
	ECompEditorRegistryPrivate *priv;

	priv = g_new0 (ECompEditorRegistryPrivate, 1);

	reg->priv = priv;

	priv->editors = g_hash_table_new (g_str_hash, g_str_equal);
}



GtkType
e_comp_editor_registry_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static const GtkTypeInfo info = {
			"ECompEditorRegistry",
			sizeof (ECompEditorRegistry),
			sizeof (ECompEditorRegistryClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (gtk_object_get_type (), &info);
	}

	return type;
}

GtkObject *
e_comp_editor_registry_new (void)
{
	return g_object_new (E_TYPE_COMP_EDITOR_REGISTRY, NULL);
}

void
e_comp_editor_registry_add (ECompEditorRegistry *reg, CompEditor *editor, gboolean remote)
{
	ECompEditorRegistryPrivate *priv;
	ECompEditorRegistryData *rdata;
	CalComponent *comp;
	const char *uid;
	
	g_return_if_fail (reg != NULL);
	g_return_if_fail (E_IS_COMP_EDITOR_REGISTRY (reg));
	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = reg->priv;
	
	comp = comp_editor_get_comp (editor);
	cal_component_get_uid (comp, &uid);

	rdata = g_new0 (ECompEditorRegistryData, 1);

	rdata->editor = editor;
	rdata->uid = g_strdup (uid);
	g_hash_table_insert (priv->editors, rdata->uid, rdata);

	g_signal_connect (editor, "destroy", G_CALLBACK (editor_destroy_cb), reg);

}

CompEditor *
e_comp_editor_registry_find (ECompEditorRegistry *reg, const char *uid)
{
	ECompEditorRegistryPrivate *priv;
	ECompEditorRegistryData *rdata;
	
	g_return_val_if_fail (reg != NULL, NULL);
	g_return_val_if_fail (E_IS_COMP_EDITOR_REGISTRY (reg), NULL);
	g_return_val_if_fail (uid != NULL, NULL);

	priv = reg->priv;

	rdata = g_hash_table_lookup (priv->editors, uid);
	if (rdata != NULL)
		return rdata->editor;
	
	return NULL;
}

static gboolean
foreach_close_cb (gpointer key, gpointer value, gpointer data)
{
	ECompEditorRegistryData *rdata;

	rdata = value;

	gtk_signal_handler_block_by_data (GTK_OBJECT (rdata->editor), data);
	
	comp_editor_focus (rdata->editor);
	if (!comp_editor_close (rdata->editor)) {
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (rdata->editor), data);
		return FALSE;
	}
	
	g_free (rdata->uid);
	g_free (rdata);
		
	return TRUE;
}

gboolean
e_comp_editor_registry_close_all (ECompEditorRegistry *reg)
{
	ECompEditorRegistryPrivate *priv;
	
	g_return_val_if_fail (reg != NULL, FALSE);
	g_return_val_if_fail (E_IS_COMP_EDITOR_REGISTRY (reg), FALSE);

	priv = reg->priv;

	g_hash_table_foreach_remove (priv->editors, foreach_close_cb, reg);
	if (g_hash_table_size (priv->editors) != 0)
		return FALSE;
	
	return TRUE;
}

static void
editor_destroy_cb (GtkWidget *widget, gpointer data) 
{
	ECompEditorRegistry *reg;
	ECompEditorRegistryPrivate *priv;
	ECompEditorRegistryData *rdata;
	CalComponent *comp;
	const char *uid;
	
	reg = E_COMP_EDITOR_REGISTRY (data);
	priv = reg->priv;
	
	comp = comp_editor_get_comp (COMP_EDITOR (widget));
	cal_component_get_uid (comp, &uid);

	rdata = g_hash_table_lookup (priv->editors, uid);
	g_assert (rdata != NULL);
	
	g_hash_table_remove (priv->editors, rdata->uid);
	g_free (rdata->uid);
	g_free (rdata);
}

