#ifndef SHELL_SHORTCUT_H
#define SHELL_SHORTCUT_H

#include <gtk/gtkobject.h>
#include "e-folder.h"
#include "shortcut-bar/e-icon-bar.h"

typedef struct _EShortcut EShortcut;
typedef struct _EShortcutGroup EShortcutGroup;
typedef struct _EShortcutBarModel EShortcutBarModel;

#define E_SHORTCUT_TYPE        (e_shortcut_get_type ())
#define E_SHORTCUT(o)          (GTK_CHECK_CAST ((o), E_SHORTCUT_TYPE, EShortcut))
#define E_SHORTCUT_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_SHORTCUT_TYPE, EShortcutClass))
#define E_IS_SHORTCUT(o)       (GTK_CHECK_TYPE ((o), E_SHORTCUT_TYPE))
#define E_IS_SHORTCUT_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SHORTCUT_TYPE))

struct _EShortcut {
	GtkObject object;
	EFolder  *efolder;
};

typedef struct {
	GtkObjectClass parent_class;
} EShortcutClass;

#define E_SHORTCUT_GROUP_TYPE        (e_shortcut_group_get_type ())
#define E_SHORTCUT_GROUP(o)          (GTK_CHECK_CAST ((o), E_SHORTCUT_GROUP_TYPE, EShortcutGroup))
#define E_SHORTCUT_GROUP_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_SHORTCUT_GROUP_TYPE, EShortcutGroupClass))
#define E_IS_SHORTCUT_GROUP(o)       (GTK_CHECK_TYPE ((o), E_SHORTCUT_GROUP_TYPE))
#define E_IS_SHORTCUT_GROUP_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SHORTCUT_GROUP_TYPE))

struct _EShortcutGroup {
	GtkObject          object;
	EShortcutBarModel *model;
	char              *group_name;
	GArray            *shortcuts;
	char              *title;
	EIconBarViewType   type;
};

typedef struct {
	GtkObjectClass parent_class;
} EShortcutGroupClass;
 
GtkType    e_shortcut_get_type  (void);
EShortcut *e_shortcut_new       (EFolder *efolder);

GtkType         e_shortcut_group_get_type (void);
EShortcutGroup *e_shortcut_group_new      (const char *name, EIconBarViewType type);
void            e_shortcut_group_append   (EShortcutGroup *sg, EShortcut *shortcut);
void            e_shortcut_group_destroy  (EShortcutGroup *sg);
void            e_shortcut_group_remove   (EShortcutGroup *sg, EShortcut *shortcut);
void            e_shortcut_group_move     (EShortcutGroup *sg, int from, int to);
void            e_shortcut_group_set_view_type (EShortcutGroup *sg, EIconBarViewType type);
void            e_shortcut_group_rename   (EShortcutGroup *sg, const char *text);

#define E_SHORTCUT_BAR_MODEL_TYPE        (e_shortcut_bar_model_get_type ())
#define E_SHORTCUT_BAR_MODEL(o)          (GTK_CHECK_CAST ((o), E_SHORTCUT_BAR_MODEL_TYPE, EShortcutBarModel))
#define E_SHORTCUT_BAR_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_SHORTCUT_BAR_MODEL_TYPE, EShortcutBarMNodelClass))
#define E_IS_SHORTCUT_BAR_MODEL(o)       (GTK_CHECK_TYPE ((o), E_SHORTCUT_BAR_MODEL_TYPE))
#define E_IS_SHORTCUT_BAR_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SHORTCUT_BAR_MODEL_TYPE))

struct _EShortcutBarModel {
	GtkObject object;

	GArray   *groups;
	GSList   *views;
};

typedef struct {
	GtkObjectClass object_class;
} EShortcutBarModelClass;

GtkType            e_shortcut_bar_model_get_type  (void);
EShortcutBarModel *e_shortcut_bar_model_new       (void);
void               e_shortcut_bar_model_append    (EShortcutBarModel *shortcut_bar,
						   EShortcutGroup    *group);
int                e_shortcut_bar_model_add_group (EShortcutBarModel *shortcut_bar);
void               e_shortcut_bar_model_remove_group
                                                  (EShortcutBarModel *model,
						   EShortcutGroup *sg);

/* Ugly api name */
int                e_group_num_from_group_ptr     (EShortcutBarModel *bm,
						   EShortcutGroup *group);

/*
 * Produces a new view of the Shortcut Bar model
 */
GtkWidget         *e_shortcut_bar_view_new       (EShortcutBarModel *bm);

/*
 * Locating objects
 */
EShortcutGroup    *e_shortcut_group_from_pos     (EShortcutBarModel *bm,
						  int group_num);
EShortcut         *e_shortcut_from_pos           (EShortcutGroup *group,
						  int item_num);

#endif

