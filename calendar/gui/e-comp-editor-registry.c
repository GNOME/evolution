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
	ECompEditorRegistry *registry;
	CompEditor *editor;
	char *uid;
};

typedef struct _ECompEditorRegistryData ECompEditorRegistryData;
typedef struct _ECompEditorRegistryForeachData ECompEditorRegistryForeachData;

static void editor_destroy_cb (gpointer data, GObject *where_object_was);

G_DEFINE_TYPE (ECompEditorRegistry, e_comp_editor_registry, G_TYPE_OBJECT);

static void
registry_data_free (gpointer data)
{
	ECompEditorRegistryData *rdata = data;

	if (rdata->editor)
		g_object_weak_unref (G_OBJECT (rdata->editor), editor_destroy_cb, rdata);
	g_free (rdata->uid);
	g_free (rdata);
}

static void
e_comp_editor_registry_dispose (GObject *obj)
{
	ECompEditorRegistry *reg;
	ECompEditorRegistryPrivate *priv;

	reg = E_COMP_EDITOR_REGISTRY (obj);
	priv = reg->priv;
	
	if (priv->editors) {
		g_hash_table_destroy (priv->editors);
		priv->editors = NULL;
	}

	(* G_OBJECT_CLASS (e_comp_editor_registry_parent_class)->dispose) (obj);
}

static void
e_comp_editor_registry_finalize (GObject *obj)
{
	ECompEditorRegistry *reg;
	ECompEditorRegistryPrivate *priv;

	reg = E_COMP_EDITOR_REGISTRY (obj);
	priv = reg->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (e_comp_editor_registry_parent_class)->finalize) (obj);
}

static void
e_comp_editor_registry_class_init (ECompEditorRegistryClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = e_comp_editor_registry_dispose;
	object_class->finalize = e_comp_editor_registry_finalize;
}

static void
e_comp_editor_registry_init (ECompEditorRegistry *reg)
{
	ECompEditorRegistryPrivate *priv;

	priv = g_new0 (ECompEditorRegistryPrivate, 1);

	reg->priv = priv;
	priv->editors = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, registry_data_free);
}

GObject *
e_comp_editor_registry_new (void)
{
	return g_object_new (E_TYPE_COMP_EDITOR_REGISTRY, NULL);
}

void
e_comp_editor_registry_add (ECompEditorRegistry *reg, CompEditor *editor, gboolean remote)
{
	ECompEditorRegistryPrivate *priv;
	ECompEditorRegistryData *rdata;
	ECalComponent *comp;
	const char *uid;
	
	g_return_if_fail (reg != NULL);
	g_return_if_fail (E_IS_COMP_EDITOR_REGISTRY (reg));
	g_return_if_fail (editor != NULL);
	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = reg->priv;
	
	comp = comp_editor_get_comp (editor);
	e_cal_component_get_uid (comp, &uid);

	rdata = g_new0 (ECompEditorRegistryData, 1);

	rdata->registry = reg;
	rdata->editor = editor;
	rdata->uid = g_strdup (uid);

	g_hash_table_insert (priv->editors, g_strdup (uid), rdata);

	/* FIXME Need to know when uid on the editor changes (if the component changes locations) */
	g_object_weak_ref (G_OBJECT (editor), editor_destroy_cb, rdata);
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

	g_signal_handlers_block_matched (rdata->editor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, data);
	
	comp_editor_focus (rdata->editor);
	if (!comp_editor_close (rdata->editor)) {
		g_signal_handlers_unblock_matched (rdata->editor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, data);
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
editor_destroy_cb (gpointer data, GObject *where_object_was) 
{
	ECompEditorRegistryData *rdata = data;

	/* We null it out because its dead, so we won't try to weak
	 * unref it in the hash destroyer */
	rdata->editor = NULL;
	g_hash_table_remove (rdata->registry->priv->editors, rdata->uid);
}
