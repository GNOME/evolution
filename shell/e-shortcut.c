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
#include <gtk/gtkmisc.h>
#include <libgnome/libgnome.h>
#include "e-util/e-util.h"
#include "e-shortcut.h"
#include "shortcut-bar/e-shortcut-bar.h"
#include "shortcut-bar/e-clipped-label.h"

#define SHORTCUT_PARENT_TYPE gtk_object_get_type ()
#define SHORTCUT_BAR_MODEL_PARENT_TYPE gtk_object_get_type ()
#define SHORTCUT_GROUP_PARENT_TYPE gtk_object_get_type ()

static GtkObjectClass *shortcut_parent_class;
static GtkObjectClass *shortcut_group_parent_class;
static GtkObjectClass *shortcut_bar_model_parent_class;

enum {
	STRUCTURE_CHANGED,
	LAST_SIGNAL
};

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
	efg->model = NULL;
	
	shortcut_group_parent_class->destroy (object);
}

static void
e_shortcut_group_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = esg_destroy;
	shortcut_parent_class = gtk_type_class (SHORTCUT_GROUP_PARENT_TYPE);
}

static void
e_shortcut_group_init (GtkObject *object)
{
	EShortcutGroup *esg = E_SHORTCUT_GROUP (object);

	GTK_OBJECT_UNSET_FLAGS (GTK_OBJECT (object), GTK_FLOATING);
	
	esg->shortcuts = g_array_new (FALSE, FALSE, sizeof (EShortcut *));
}

EShortcut *
e_shortcut_new (EFolder *efolder)
{
	EShortcut *shortcut = gtk_type_new (e_shortcut_get_type ());

	shortcut->efolder = efolder;
	gtk_object_ref (GTK_OBJECT (efolder));

	return shortcut;
}

EShortcutGroup *
e_shortcut_group_new (const char *title, EIconBarViewType type)
{
	EShortcutGroup *shortcut_group = gtk_type_new (e_shortcut_group_get_type ());

	shortcut_group->title = g_strdup (title);
	shortcut_group->type = type;
	return shortcut_group;
}

void
e_shortcut_group_append (EShortcutGroup *sg, EShortcut *shortcut)
{
	g_return_if_fail (sg != NULL);
	g_return_if_fail (E_IS_SHORTCUT_GROUP (sg));
	g_return_if_fail (shortcut != NULL);
	g_return_if_fail (E_IS_SHORTCUT (shortcut));

	gtk_object_ref (GTK_OBJECT (shortcut));
	gtk_object_sink (GTK_OBJECT (shortcut));
	
	g_array_append_val (sg->shortcuts, shortcut);

	/* FIXME: Broadcast change */
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
				/* FIXME: Broadcast change */
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
	
	/* FIXME: Broadcast change */
}

void
e_shortcut_group_rename (EShortcutGroup *sg, const char *text)
{
	GSList *l;
	int id;
	
	g_return_if_fail (sg != NULL);
	g_return_if_fail (E_IS_SHORTCUT_GROUP (sg));

	id = e_group_num_from_group_ptr (sg->model, sg);
	for (l = sg->model->views; l; l = l->next){
		EShortcutBar *shortcut_bar = l->data;
		GtkWidget *label;

		label = e_clipped_label_new (text);

		gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
		gtk_widget_show (label);

		e_group_bar_set_group_button_label (
			E_GROUP_BAR (shortcut_bar), id, label);
	}
}

static void
esb_destroy (GtkObject *object)
{
	EShortcutBarModel *esb = E_SHORTCUT_BAR_MODEL (object);
	const int count = esb->groups->len;
	int i;
	
	for (i = 0; i < count; i++){
		EShortcutGroup *esg = g_array_index (esb->groups, EShortcutGroup *, i);
		
		gtk_object_destroy (GTK_OBJECT (esg));
	}

	g_array_free (esb->groups, TRUE);
	shortcut_bar_model_parent_class->destroy (object);
}

static void
e_shortcut_bar_model_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = esb_destroy;
	shortcut_bar_model_parent_class = gtk_type_class (SHORTCUT_BAR_MODEL_PARENT_TYPE);
}

static void
e_shortcut_bar_model_init (GtkObject *object)
{
	EShortcutBarModel *esb = E_SHORTCUT_BAR_MODEL (object);

	/* The shortcut bar model is self owned */
	GTK_OBJECT_UNSET_FLAGS (object, GTK_FLOATING);
	
	esb->groups = g_array_new (FALSE, FALSE, sizeof (EShortcutGroup *));
}

EShortcutBarModel *
e_shortcut_bar_model_new (void)
{
	EShortcutBarModel *bm;

	bm = gtk_type_new (e_shortcut_bar_model_get_type ());

	return bm;
}

void
e_shortcut_bar_model_append (EShortcutBarModel *bm, EShortcutGroup *sg)
{
	g_return_if_fail (bm != NULL);
	g_return_if_fail (sg != NULL);
	g_return_if_fail (E_IS_SHORTCUT_BAR_MODEL (bm));
	g_return_if_fail (E_IS_SHORTCUT_GROUP (sg));

	gtk_object_ref (GTK_OBJECT (sg));
	gtk_object_sink (GTK_OBJECT (sg));

	sg->model = bm;
	
	g_array_append_val (bm->groups, sg);
}

EShortcutGroup *
e_shortcut_group_from_pos (EShortcutBarModel *bm, int group_num)
{
	EShortcutGroup *group;
	
	if (group_num == -1)
		return NULL;

	group = g_array_index (bm->groups, EShortcutGroup *, group_num);
	return group;
}

EShortcut *
e_shortcut_from_pos (EShortcutGroup *group, int item_num)
{
	EShortcut *shortcut;

	g_return_val_if_fail (group != NULL, NULL);
	g_return_val_if_fail (E_IS_SHORTCUT_GROUP (group), NULL);
	
	if (item_num == -1)
		return NULL;

	g_return_val_if_fail (item_num < group->shortcuts->len, NULL);

	shortcut = g_array_index (group->shortcuts, EShortcut *, item_num);
	return shortcut;
}

static void
populate_group (EShortcutBarModel *bm, EShortcutGroup *esg, EShortcutBar *shortcut_bar)
{
	int group_num, i;
	const int items = esg->shortcuts->len;

	group_num = e_shortcut_bar_add_group (shortcut_bar, esg->title);
	e_shortcut_bar_set_view_type (shortcut_bar, group_num, esg->type);

	for (i = 0; i < items; i++){
		EShortcut *shortcut = E_SHORTCUT (g_array_index (esg->shortcuts, EShortcut *, i));
		EFolder *folder = shortcut->efolder;
		char *type = NULL;
		
		switch (folder->type){
		case E_FOLDER_SUMMARY:
			type = "summary:";
			break;

		case E_FOLDER_MAIL:
			type = "mail:";
			break;
			
		case E_FOLDER_CONTACTS:
			type = "contacts:";
			break;
			
		case E_FOLDER_CALENDAR:
			type = "calendar:";
			break;
			
		case E_FOLDER_TASKS:
			type = "todo:";
			break;
			
		case E_FOLDER_OTHER:
			type = "file:";
			break;

		default:
			g_assert_not_reached ();
		}
		
		e_shortcut_bar_add_item (shortcut_bar, group_num, type, folder->name);
	}
}

static void
populate_from_model (EShortcutBarModel *bm, EShortcutBar *shortcut_bar)
{
	const int groups = bm->groups->len;
	int i;
	
	for (i = 0; i < groups; i++){
		EShortcutGroup *esg;

		esg = g_array_index (bm->groups, EShortcutGroup *, i);

		populate_group (bm, esg, shortcut_bar);
	}
	
}

static struct {
	char *prefix, *path;
	GdkPixbuf *image;
} shell_icons[] = {
	{ "summary:", "evolution-today.png", NULL },
	{ "mail:", "evolution-inbox.png", NULL },
	{ "calendar:", "evolution-calendar.png", NULL },
	{ "contacts:", "evolution-contacts.png", NULL },
	{ "notes:", "evolution-notes.png", NULL },
	{ "todo:", "evolution-tasks.png", NULL }
};
#define NSHELL_ICONS (sizeof (shell_icons) / sizeof (shell_icons[0]))

static GdkPixbuf *
shell_icon_cb (EShortcutBar *shortcut_bar, const gchar *url, gpointer data)
{
	int i;

	for (i = 0; i < NSHELL_ICONS; i++) {
		if (!strncmp (shell_icons[i].prefix, url,
			      strlen (shell_icons[i].prefix))) {
			if (!shell_icons[i].image) {
				char *pixmap_path;

				pixmap_path = g_strconcat (EVOLUTION_IMAGES "/", shell_icons[i].path, NULL);
				if (pixmap_path)
					shell_icons[i].image = gdk_pixbuf_new_from_file (pixmap_path);
				else {
					g_warning ("Couldn't find image: %s",
						   pixmap_path);
				}
				g_free (pixmap_path);
			}
			return shell_icons[i].image;
		}
	}

	return NULL;
}

static void
view_destroyed (EShortcutBar *shortcut_bar, EShortcutBarModel *bm)
{
	bm->views = g_slist_remove (bm->views, shortcut_bar);
}

GtkWidget *
e_shortcut_bar_view_new (EShortcutBarModel *bm)
{
	GtkWidget *shortcut_bar;
	
	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	shortcut_bar = e_shortcut_bar_new ();
	e_shortcut_bar_set_icon_callback (E_SHORTCUT_BAR (shortcut_bar),
					  shell_icon_cb, NULL);

	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();

	populate_from_model (bm, E_SHORTCUT_BAR (shortcut_bar));

	bm->views = g_slist_prepend (bm->views, shortcut_bar);
	gtk_signal_connect (GTK_OBJECT (shortcut_bar), "destroy", GTK_SIGNAL_FUNC (view_destroyed), bm);
	
	return shortcut_bar;
}

int
e_group_num_from_group_ptr (EShortcutBarModel *bm, EShortcutGroup *group)
{
	const int n = bm->groups->len;
	int i;
	
	for (i = 0; i < n; i++)
		if (g_array_index (bm->groups, EShortcutGroup *, i) == group)
			return i;
	return -1;
}

/*
 * Sets the view mode in all the views
 */
void
e_shortcut_group_set_view_type (EShortcutGroup *group, EIconBarViewType type)
{
	GSList *l;
	int group_num;
	
	g_return_if_fail (group != NULL);
	g_return_if_fail (E_IS_SHORTCUT_GROUP (group));

	group_num = e_group_num_from_group_ptr (group->model, group);

	g_assert (group_num != -1);
	
	group->type = type;
	
	for (l = group->model->views; l ; l = l->next){
		EShortcutBar *shortcut_bar = l->data;

		e_shortcut_bar_set_view_type (shortcut_bar, group_num, type);
	}
}

gint
e_shortcut_bar_model_add_group (EShortcutBarModel *model)
{
	int id = -1;
	GSList *l = NULL;
	
	g_return_val_if_fail (model != NULL, -1);
	g_return_val_if_fail (E_IS_SHORTCUT_BAR_MODEL (model), -1);

	for (l = model->views; l; l = l->next){
		EShortcutBar *shortcut_bar = l->data;
		
		id = e_shortcut_bar_add_group (shortcut_bar, _("New group"));
	}

	return id;
}

void
e_shortcut_bar_model_remove_group (EShortcutBarModel *model, EShortcutGroup *sg)
{
	GSList *l = NULL;
	int group_num;
	
	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_SHORTCUT_BAR_MODEL (model));
	g_return_if_fail (sg != NULL);
	g_return_if_fail (E_IS_SHORTCUT_GROUP (sg));

	group_num = e_group_num_from_group_ptr (model, sg);
	
	for (l = model->views; l; l = l->next){
		EShortcutBar *shortcut_bar = l->data;
		
		e_shortcut_bar_remove_group (shortcut_bar, group_num);
	}
	
}

E_MAKE_TYPE (e_shortcut, "EShortcut", EShortcut, e_shortcut_class_init, NULL, SHORTCUT_PARENT_TYPE);
E_MAKE_TYPE (e_shortcut_group, "EShortcutGroup", EShortcutGroup, e_shortcut_group_class_init, e_shortcut_group_init, SHORTCUT_GROUP_PARENT_TYPE);
E_MAKE_TYPE (e_shortcut_bar_model, "EShortcutBarModel", EShortcutBarModel, e_shortcut_bar_model_class_init, e_shortcut_bar_model_init, SHORTCUT_BAR_MODEL_PARENT_TYPE);

