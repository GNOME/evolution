#ifndef SHELL_SHORTCUT_H
#define SHELL_SHORTCUT_H

#include <gtk/gtkobject.h>
#include "e-folder.h"

#define E_SHORTCUT_TYPE        (e_shortcut_get_type ())
#define E_SHORTCUT(o)          (GTK_CHECK_CAST ((o), E_SHORTCUT_TYPE, EShortcut))
#define E_SHORTCUT_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_SHORTCUT_TYPE, EShortcutClass))
#define E_IS_SHORTCUT(o)       (GTK_CHECK_TYPE ((o), E_SHORTCUT_TYPE))
#define E_IS_SHORTCUT_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SHORTCUT_TYPE))

typedef struct {
	GtkObject object;
	EFolder  *efolder;
} EShortcut;

typedef struct {
	GtkObjectClass parent_class;
} EShortcutClass;

#define E_SHORTCUT_GROUP_TYPE        (e_shortcut_group_get_type ())
#define E_SHORTCUT_GROUP(o)          (GTK_CHECK_CAST ((o), E_SHORTCUT_GROUP_TYPE, EShortcutGroup))
#define E_SHORTCUT_GROUP_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_SHORTCUT_GROUP_TYPE, EShortcutGroupClass))
#define E_IS_SHORTCUT_GROUP(o)       (GTK_CHECK_TYPE ((o), E_SHORTCUT_GROUP_TYPE))
#define E_IS_SHORTCUT_GROUP_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SHORTCUT_GROUP_TYPE))

typedef struct {
	GtkObject  object;
	char      *group_name;
	GArray    *shortcuts;
	char      *title;
	gboolean   small_icons;
} EShortcutGroup;

typedef enum {
	E_SHORTCUT_GROUP_ITEM_ADDED,
	E_SHORTCUT_GROUP_ITEM_REMOVED,
	E_SHORTCUT_GROUP_ITEM_MOVED,
} EShortcutGroupChange;

typedef struct {
	GtkObjectClass parent_class;

	void (*structure_changed) (EShortcutGroup *, EShortcutGroupChange change, int arg1, int arg2);
} EShortcutGroupClass;
 
GtkType    e_shortcut_get_type  (void);
EShortcut *e_shortcut_new       (EFolder *efolder);

GtkType         e_shortcut_group_get_type (void);
EShortcutGroup *e_shortcut_group_new      (const char *name, gboolean small_icons);
void            e_shortcut_group_append   (EShortcutGroup *sg, EShortcut *shortcut);
void            e_shortcut_group_destroy  (EShortcutGroup *sg);
void            e_shortcut_group_remove   (EShortcutGroup *sg, EShortcut *shortcut);
void            e_shortcut_group_move     (EShortcutGroup *sg, int from, int to);


#endif
