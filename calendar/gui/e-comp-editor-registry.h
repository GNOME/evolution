/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-comp-editor-registry.h
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

#ifndef _E_COMP_EDITOR_REGISTRY_H_
#define _E_COMP_EDITOR_REGISTRY_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <dialogs/comp-editor.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_COMP_EDITOR_REGISTRY			(e_comp_editor_registry_get_type ())
#define E_COMP_EDITOR_REGISTRY(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_COMP_EDITOR_REGISTRY, ECompEditorRegistry))
#define E_COMP_EDITOR_REGISTRY_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_COMP_EDITOR_REGISTRY, ECompEditorRegistryClass))
#define E_IS_COMP_EDITOR_REGISTRY(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_COMP_EDITOR_REGISTRY))
#define E_IS_COMP_EDITOR_REGISTRY_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_COMP_EDITOR_REGISTRY))


typedef struct _ECompEditorRegistry        ECompEditorRegistry;
typedef struct _ECompEditorRegistryPrivate ECompEditorRegistryPrivate;
typedef struct _ECompEditorRegistryClass   ECompEditorRegistryClass;

struct _ECompEditorRegistry {
	GtkObject parent;

	ECompEditorRegistryPrivate *priv;
};

struct _ECompEditorRegistryClass {
	GtkObjectClass parent_class;
};

typedef void (* ECompEditorRegistryForeachFn) (CompEditor *editor, gpointer data);




GtkType     e_comp_editor_registry_get_type  (void);
GtkObject  *e_comp_editor_registry_new       (void);
void        e_comp_editor_registry_add       (ECompEditorRegistry *reg,
					      CompEditor          *editor,
					      gboolean             remote);
CompEditor *e_comp_editor_registry_find      (ECompEditorRegistry *reg,
					      const char          *uid);
void        e_comp_editor_registry_close_all (ECompEditorRegistry *reg);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_COMP_EDITOR_REGISTRY_H_ */


