/*
 * Shortcut.c: implements shortcuts and shortcut group models
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * (C) 2000 Helix Code, Inc.
 *
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include "e-util/e-util.h"
#include "e-shortcut.h"

#define SHORTCUT_PARENT_TYPE gtk_object_get_type ()
#define SHORTCUT_GROUP_PARENT_TYPE gtk_object_get_type ()

static GtkObjectClass *shortcut_parent_class;
static GtkObjectClass *shortcut_group_parent_class;

enum {
	STRUCTURE_CHANGED,
	LAST_SIGNAL
};

static unsigned int sg_signals [LAST_SIGNAL] = { 0, };

static void
es_destroy (GtkObject *object)
{
	EShortcut *ef = E_SHORTCUT (object);
	
	gtk_object_unref (GTK_OBJECT (ef->efolder));
			  
	shortcut_parent_class->destroy (object);
}

static void
e_shortcut_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = es_destroy;
	shortcut_parent_class = gtk_type_class (SHORTCUT_PARENT_TYPE);
}

static void
esg_destroy (GtkObject *object)
{
	EShortcutGroup *efg = E_SHORTCUT_GROUP (object);
	const int shortcut_count = efg->shortcuts->len;
	int i;
		
	g_free (efg->title);

	for (i = 0; i < shortcut_count; i++){
		EShortcut *es = g_array_index (efg->shortcuts, EShortcut *, i);
		
		gtk_object_unref (GTK_OBJECT (es));
	}
	
	g_array_free (efg->shortcuts, TRUE);
	
	shortcut_group_parent_class->destroy (object);
}

typedef void (*MyGtkSignal_NONE__INT_INT_INT) (GtkObject * object,
					       int arg1,
					       int arg2,
					       int arg3,
					       gpointer user_data);
static void 
mygtk_marshal_NONE__INT_INT_INT (GtkObject * object,
				 GtkSignalFunc func,
				 gpointer func_data,
				 GtkArg * args)
{
	MyGtkSignal_NONE__INT_INT_INT rfunc;
	rfunc = (MyGtkSignal_NONE__INT_INT_INT) func;

	(*rfunc) (object,
		  GTK_VALUE_INT (args[0]),
		  GTK_VALUE_INT (args[1]),
		  GTK_VALUE_INT (args[2]),
		  func_data);
}

static void
e_shortcut_group_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = esg_destroy;
	shortcut_parent_class = gtk_type_class (SHORTCUT_GROUP_PARENT_TYPE);

	sg_signals [STRUCTURE_CHANGED] =
		gtk_signal_new ("structure_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EShortcutGroupClass, structure_changed),
				mygtk_marshal_NONE__INT_INT_INT,
				GTK_TYPE_NONE,
				3,
				GTK_TYPE_ENUM, GTK_TYPE_INT, GTK_TYPE_INT);
	gtk_object_class_add_signals (
		object_class, sg_signals, LAST_SIGNAL);

}

static void
e_shortcut_group_init (GtkObject *object)
{
	EShortcutGroup *esg = E_SHORTCUT_GROUP (object);

	esg->shortcuts = g_array_new (FALSE, FALSE, sizeof (EShortcut *));
}

E_MAKE_TYPE (e_shortcut, "EShortcut", EShortcut, e_shortcut_class_init, NULL, SHORTCUT_PARENT_TYPE);
E_MAKE_TYPE (e_shortcut_group, "EShortcutGroup", EShortcutGroup, e_shortcut_group_class_init, e_shortcut_group_init, SHORTCUT_GROUP_PARENT_TYPE);

EShortcut *
e_shortcut_new (EFolder *efolder)
{
	EShortcut *shortcut = gtk_type_new (e_shortcut_get_type ());

	shortcut->efolder = efolder;
	gtk_object_ref (GTK_OBJECT (efolder));

	return shortcut;
}

EShortcutGroup *
e_shortcut_group_new (const char *title, gboolean small_icons)
{
	EShortcutGroup *shortcut_group = gtk_type_new (e_shortcut_group_get_type ());

	shortcut_group->title = g_strdup (title);
	shortcut_group->small_icons = small_icons;
	return shortcut_group;
}

static void
es_emit (EShortcutGroup *sg, EShortcutGroupChange change, int arg1, int arg2)
{
	gtk_signal_emit (GTK_OBJECT (sg), sg_signals [STRUCTURE_CHANGED], change, arg1, arg2);
}

void
e_shortcut_group_append (EShortcutGroup *sg, EShortcut *shortcut)
{
	g_return_if_fail (sg != NULL);
	g_return_if_fail (E_IS_SHORTCUT_GROUP (sg));
	g_return_if_fail (shortcut != NULL);
	g_return_if_fail (E_IS_SHORTCUT (shortcut));

	gtk_object_ref (GTK_OBJECT (shortcut));
	g_array_append_val (sg->shortcuts, shortcut);

	es_emit (sg, E_SHORTCUT_GROUP_ITEM_ADDED, sg->shortcuts->len-1, 0);
}

void
e_shortcut_group_remove (EShortcutGroup *sg, EShortcut *shortcut)
{
	g_return_if_fail (sg != NULL);
	g_return_if_fail (E_IS_SHORTCUT_GROUP (sg));
	g_return_if_fail (shortcut != NULL);
	g_return_if_fail (E_IS_SHORTCUT (sg));

	{
		const int len = sg->shortcuts->len;
		int i;
		
		for (i = 0; i < len; i++){
			EShortcut *es = g_array_index (sg->shortcuts, EShortcut *, i);

			if (es == shortcut){
				g_array_remove_index (sg->shortcuts, i);
				es_emit (sg, E_SHORTCUT_GROUP_ITEM_REMOVED, i, 0);
				return;
			}
		}
	}
}

void
e_shortcut_group_move (EShortcutGroup *sg, int from, int to)
{
	EShortcut *t;
	
	g_return_if_fail (sg != NULL);
	g_return_if_fail (E_IS_SHORTCUT_GROUP (sg));

	g_return_if_fail (from < sg->shortcuts->len);
	g_return_if_fail (to < sg->shortcuts->len);
	g_return_if_fail (from >= 0);
	g_return_if_fail (to >= 0);
	
	if (from == to)
		return;

	t = g_array_index (sg->shortcuts, EShortcut *, from);
	g_array_index (sg->shortcuts, EShortcut *, from) =
		g_array_index (sg->shortcuts, EShortcut *, to);
	g_array_index (sg->shortcuts, EShortcut *, to) = t;
	
	es_emit (sg, E_SHORTCUT_GROUP_ITEM_MOVED, from, to);
}
