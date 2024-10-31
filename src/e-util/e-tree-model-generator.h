/* e-tree-model-generator.h - Model wrapper that permutes underlying rows.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Authors: Hans Petter Jansson <hpj@novell.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TREE_MODEL_GENERATOR_H
#define E_TREE_MODEL_GENERATOR_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_TREE_MODEL_GENERATOR \
	(e_tree_model_generator_get_type ())
#define E_TREE_MODEL_GENERATOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TREE_MODEL_GENERATOR, ETreeModelGenerator))
#define E_TREE_MODEL_GENERATOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TREE_MODEL_GENERATOR, ETreeModelGeneratorClass))
#define E_IS_TREE_MODEL_GENERATOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TREE_MODEL_GENERATOR))
#define E_IS_TREE_MODEL_GENERATOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TREE_MODEL_GENERATOR))
#define E_TREE_MODEL_GENERATOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TREE_MODEL_GENERATOR, ETreeModelGeneratorClass))

G_BEGIN_DECLS

typedef gint (*ETreeModelGeneratorGenerateFunc) (GtkTreeModel *model, GtkTreeIter *child_iter,
						 gpointer data);
typedef void (*ETreeModelGeneratorModifyFunc)   (GtkTreeModel *model, GtkTreeIter *child_iter,
						 gint permutation_n, gint column, GValue *value,
						 gpointer data);

typedef struct _ETreeModelGenerator ETreeModelGenerator;
typedef struct _ETreeModelGeneratorClass ETreeModelGeneratorClass;
typedef struct _ETreeModelGeneratorPrivate ETreeModelGeneratorPrivate;

struct _ETreeModelGenerator {
	GObject parent;
	ETreeModelGeneratorPrivate *priv;
};

struct _ETreeModelGeneratorClass {
	GObjectClass parent_class;
};

GType		e_tree_model_generator_get_type	(void) G_GNUC_CONST;
ETreeModelGenerator *
		e_tree_model_generator_new	(GtkTreeModel *child_model);
GtkTreeModel *	e_tree_model_generator_get_model (ETreeModelGenerator *tree_model_generator);
void		e_tree_model_generator_set_generate_func
						(ETreeModelGenerator *tree_model_generator,
						 ETreeModelGeneratorGenerateFunc func,
						 gpointer data,
						 GDestroyNotify destroy);
void		e_tree_model_generator_set_modify_func
						(ETreeModelGenerator *tree_model_generator,
						 ETreeModelGeneratorModifyFunc func,
						 gpointer data,
						 GDestroyNotify destroy);
GtkTreePath *	e_tree_model_generator_convert_child_path_to_path
						(ETreeModelGenerator *tree_model_generator,
						 GtkTreePath *child_path);
void		e_tree_model_generator_convert_child_iter_to_iter
						(ETreeModelGenerator *tree_model_generator,
						 GtkTreeIter *generator_iter,
						 GtkTreeIter *child_iter);
GtkTreePath *	e_tree_model_generator_convert_path_to_child_path
						(ETreeModelGenerator *tree_model_generator,
						 GtkTreePath *generator_path);
gboolean	e_tree_model_generator_convert_iter_to_child_iter
						(ETreeModelGenerator *tree_model_generator,
						 GtkTreeIter *child_iter,
						 gint *permutation_n,
						 GtkTreeIter *generator_iter);

G_END_DECLS

#endif  /* E_TREE_MODEL_GENERATOR_H */
