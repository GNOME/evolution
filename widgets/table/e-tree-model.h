/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TREE_MODEL_H_
#define _E_TREE_MODEL_H_

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkobject.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define E_TREE_MODEL_TYPE        (e_tree_model_get_type ())
#define E_TREE_MODEL(o)          (GTK_CHECK_CAST ((o), E_TREE_MODEL_TYPE, ETreeModel))
#define E_TREE_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TREE_MODEL_TYPE, ETreeModelClass))
#define E_IS_TREE_MODEL(o)       (GTK_CHECK_TYPE ((o), E_TREE_MODEL_TYPE))
#define E_IS_TREE_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TREE_MODEL_TYPE))

typedef void * ETreePath;
typedef struct ETreeModel ETreeModel;
typedef struct ETreeModelClass ETreeModelClass;
typedef gint (*ETreePathCompareFunc)(ETreeModel *model, ETreePath path1, ETreePath path2);
typedef gboolean (*ETreePathFunc)(ETreeModel *model, ETreePath path, gpointer data);

struct ETreeModel {
	GtkObject base;
};

struct ETreeModelClass {
	GtkObjectClass parent_class;

	/*
	 * Virtual methods
	 */
	ETreePath (*get_root)              (ETreeModel *etm);

	ETreePath (*get_parent)            (ETreeModel *etm, ETreePath node);
	ETreePath (*get_first_child)       (ETreeModel *etm, ETreePath node);
	ETreePath (*get_last_child)        (ETreeModel *etm, ETreePath node);
	ETreePath (*get_next)              (ETreeModel *etm, ETreePath node);
	ETreePath (*get_prev)              (ETreeModel *etm, ETreePath node);

	gboolean   (*is_root)              (ETreeModel *etm, ETreePath node);
	gboolean   (*is_expandable)        (ETreeModel *etm, ETreePath node);
	guint      (*get_children)         (ETreeModel *etm, ETreePath node, ETreePath **paths);
	guint      (*depth)                (ETreeModel *etm, ETreePath node);

	GdkPixbuf *(*icon_at)              (ETreeModel *etm, ETreePath node);

	gboolean   (*get_expanded_default) (ETreeModel *etm);
	gint       (*column_count)         (ETreeModel *etm);

	gboolean   (*has_save_id)          (ETreeModel *etm);
	gchar     *(*get_save_id)          (ETreeModel *etm, ETreePath node);

	/*
	 * ETable analogs
	 */
	void      *(*value_at)             (ETreeModel *etm, ETreePath node, int col);
	void       (*set_value_at)         (ETreeModel *etm, ETreePath node, int col, const void *val);
	gboolean   (*is_editable)          (ETreeModel *etm, ETreePath node, int col);

	void      *(*duplicate_value)      (ETreeModel *etm, int col, const void *value);
	void       (*free_value)           (ETreeModel *etm, int col, void *value);
	void	  *(*initialize_value)     (ETreeModel *etm, int col);
	gboolean   (*value_is_empty)       (ETreeModel *etm, int col, const void *value);
	char      *(*value_to_string)      (ETreeModel *etm, int col, const void *value);

	/*
	 * Signals
	 */
	void       (*pre_change)           (ETreeModel *etm);
	void       (*node_changed)         (ETreeModel *etm, ETreePath node);
	void       (*node_data_changed)    (ETreeModel *etm, ETreePath node);
	void       (*node_col_changed)     (ETreeModel *etm, ETreePath node,   int col);
	void       (*node_inserted)        (ETreeModel *etm, ETreePath parent, ETreePath inserted_node);
	void       (*node_removed)         (ETreeModel *etm, ETreePath parent, ETreePath removed_node);
};
GtkType     e_tree_model_get_type              (void);
ETreeModel *e_tree_model_new                   (void);

/* tree traversal operations */
ETreePath   e_tree_model_get_root              (ETreeModel     *etree);
ETreePath   e_tree_model_node_get_parent       (ETreeModel     *etree,
						ETreePath       path);
ETreePath   e_tree_model_node_get_first_child  (ETreeModel     *etree,
						ETreePath       path);
ETreePath   e_tree_model_node_get_last_child   (ETreeModel     *etree,
						ETreePath       path);
ETreePath   e_tree_model_node_get_next         (ETreeModel     *etree,
						ETreePath       path);
ETreePath   e_tree_model_node_get_prev         (ETreeModel     *etree,
						ETreePath       path);

/* node accessors */
gboolean    e_tree_model_node_is_root          (ETreeModel     *etree,
						ETreePath       path);
gboolean    e_tree_model_node_is_expandable    (ETreeModel     *etree,
						ETreePath       path);
guint       e_tree_model_node_get_children     (ETreeModel     *etree,
						ETreePath       path,
						ETreePath     **paths);
guint       e_tree_model_node_depth            (ETreeModel     *etree,
						ETreePath       path);
GdkPixbuf  *e_tree_model_icon_at               (ETreeModel     *etree,
						ETreePath       path);
gboolean    e_tree_model_get_expanded_default  (ETreeModel     *model);
gint        e_tree_model_column_count          (ETreeModel     *model);


gboolean    e_tree_model_has_save_id           (ETreeModel     *model);
gchar      *e_tree_model_get_save_id           (ETreeModel     *model,
						ETreePath       node);

void       *e_tree_model_value_at              (ETreeModel     *etree,
						ETreePath       node,
						int             col);
void        e_tree_model_set_value_at          (ETreeModel     *etree,
						ETreePath       node,
						int             col,
						const void     *val);
gboolean    e_tree_model_node_is_editable      (ETreeModel     *etree,
						ETreePath       node,
						int             col);
void       *e_tree_model_duplicate_value       (ETreeModel     *etree,
						int             col,
						const void     *value);
void        e_tree_model_free_value            (ETreeModel     *etree,
						int             col,
						void           *value);
void       *e_tree_model_initialize_value      (ETreeModel     *etree,
						int             col);
gboolean    e_tree_model_value_is_empty        (ETreeModel     *etree,
						int             col,
						const void     *value);
char       *e_tree_model_value_to_string       (ETreeModel     *etree,
						int             col,
						const void     *value);

/* depth first traversal of path's descendents, calling func on each one */
void        e_tree_model_node_traverse         (ETreeModel     *model,
						ETreePath       path,
						ETreePathFunc   func,
						gpointer        data);

/*
** Routines for emitting signals on the ETreeModel
*/
void        e_tree_model_pre_change            (ETreeModel     *tree_model);
void        e_tree_model_node_changed          (ETreeModel     *tree_model,
						ETreePath       node);
void        e_tree_model_node_data_changed     (ETreeModel     *tree_model,
						ETreePath       node);
void        e_tree_model_node_col_changed      (ETreeModel     *tree_model,
						ETreePath       node,
						int             col);
void        e_tree_model_node_inserted         (ETreeModel     *tree_model,
						ETreePath       parent_node,
						ETreePath       inserted_node);
void        e_tree_model_node_removed          (ETreeModel     *tree_model,
						ETreePath       parent_node,
						ETreePath       removed_node);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TREE_MODEL_H */
