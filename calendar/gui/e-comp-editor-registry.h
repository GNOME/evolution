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

#include <glib-object.h>
#include <dialogs/comp-editor.h>

G_BEGIN_DECLS

#define E_TYPE_COMP_EDITOR_REGISTRY            (e_comp_editor_registry_get_type ())
#define E_COMP_EDITOR_REGISTRY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_COMP_EDITOR_REGISTRY, ECompEditorRegistry))
#define E_COMP_EDITOR_REGISTRY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_COMP_EDITOR_REGISTRY, ECompEditorRegistryClass))
#define E_IS_COMP_EDITOR_REGISTRY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_COMP_EDITOR_REGISTRY))
#define E_IS_COMP_EDITOR_REGISTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_COMP_EDITOR_REGISTRY))


typedef struct _ECompEditorRegistry        ECompEditorRegistry;
typedef struct _ECompEditorRegistryPrivate ECompEditorRegistryPrivate;
typedef struct _ECompEditorRegistryClass   ECompEditorRegistryClass;

struct _ECompEditorRegistry {
	GObject parent;

	ECompEditorRegistryPrivate *priv;
};

struct _ECompEditorRegistryClass {
	GObjectClass parent_class;
};

typedef void (* ECompEditorRegistryForeachFn) (CompEditor *editor, gpointer data);




GType     e_comp_editor_registry_get_type  (void);
GObject  *e_comp_editor_registry_new       (void);
void        e_comp_editor_registry_add       (ECompEditorRegistry *reg,
					      CompEditor          *editor,
					      gboolean             remote);
CompEditor *e_comp_editor_registry_find      (ECompEditorRegistry *reg,
					      const char          *uid);
gboolean    e_comp_editor_registry_close_all (ECompEditorRegistry *reg);

G_END_DECLS

#endif /* _E_COMP_EDITOR_REGISTRY_H_ */


