/*
 * e-shell-shortcut.c: Handles events from the shortcut bar widget on the
 * e-shell-view
 *
 * Authors:
 *   Damon Chaplin (damon@gtk.org)
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * (C) 1999, 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gnome.h>
#include "shortcut-bar/e-shortcut-bar.h"
#include "e-shell-shortcut.h"
#include "e-shell-view.h"

#define SMALL_ICONS 1
#define LARGE_ICONS 2

typedef struct {
	EShellView *eshell_view;
	EShortcutGroup *sg;
} closure_group_t;

static void
set_large_icons (GtkMenuItem *menu_item, closure_group_t *closure)
{
	e_shortcut_group_set_view_type (closure->sg, E_ICON_BAR_LARGE_ICONS);
}

static void
set_small_icons (GtkMenu *menu_item, closure_group_t *closure)
{
	e_shortcut_group_set_view_type (closure->sg, E_ICON_BAR_SMALL_ICONS);
}

static void
add_group (GtkMenu *menu, closure_group_t *closure)
{
	int group_num;
	GtkWidget *entry;
	
	group_num = e_shortcut_bar_model_add_group (closure->eshell_view->eshell->shortcut_bar);

	/*
	 * FIXME: Figure out why this does not quite work
	 */
	entry = gtk_entry_new ();
	gtk_widget_show (entry);

	e_group_bar_set_group_button_label (
		E_GROUP_BAR (closure->eshell_view->shortcut_bar),
		group_num,
		entry);
}

static void
remove_group (GtkMenuItem *menu_item, closure_group_t *closure)
{
	e_shortcut_bar_model_remove_group (closure->eshell_view->eshell->shortcut_bar, closure->sg);
}

static void
do_rename (GtkEntry *entry, EShortcutGroup *sg)
{
	e_shortcut_group_rename (sg, gtk_entry_get_text (entry));
}

static void
rename_group (GtkMenuItem *menu_item, closure_group_t *closure)
{
	GtkWidget *entry;
	int item;
	
	item = e_group_num_from_group_ptr (closure->eshell_view->eshell->shortcut_bar, closure->sg);
	e_shortcut_group_rename (closure->sg, "Ren Test");

	return;
	
	entry = gtk_entry_new ();
	gtk_widget_show (entry);
	gtk_widget_grab_focus (entry);

	gtk_signal_connect (GTK_OBJECT (entry), "activate", GTK_SIGNAL_FUNC (do_rename), closure->sg);
		
	e_group_bar_set_group_button_label (E_GROUP_BAR (closure->eshell_view->shortcut_bar), item, entry);
}

static void
add_shortcut (GtkMenu *menu, closure_group_t *closure)
{
}

static struct {
	char          *label;
	int            flags;
	GtkSignalFunc  callback;
} shortcut_menu [] = {
	{ N_("Large Icons"),   SMALL_ICONS, GTK_SIGNAL_FUNC (set_large_icons) },
	{ N_("Small Icons"),   LARGE_ICONS, GTK_SIGNAL_FUNC (set_small_icons) },
	{ NULL, 0, NULL },
	{ N_("Add New Group"), 0,           GTK_SIGNAL_FUNC (add_group) },
	{ N_("Remove Group"),  0,           GTK_SIGNAL_FUNC (remove_group) },
	{ N_("Rename Group"),  0,           GTK_SIGNAL_FUNC (rename_group) },
	{ NULL, 0, NULL },
	{ N_("Add Shortcut"),  0,           GTK_SIGNAL_FUNC (add_shortcut) },
};

#define ELEMENTS(x) (sizeof (x) / sizeof (x[0]))

static void
shortcut_bar_show_standard_popup (EShellView *eshell_view, GdkEvent *event, EShortcutGroup *shortcut_group)
{
	GtkWidget *menu, *menuitem;
	int i;
	closure_group_t closure;
	
	menu = gtk_menu_new ();

	closure.sg = shortcut_group;
	closure.eshell_view = eshell_view;
	
	for (i = 0; i < ELEMENTS (shortcut_menu); i++){
		gboolean disable = FALSE;
		
		if (shortcut_menu [i].flags & SMALL_ICONS)
			if (shortcut_group->type != E_ICON_BAR_SMALL_ICONS)
				disable = TRUE;

		if (shortcut_menu [i].flags & LARGE_ICONS)
			if (shortcut_group->type != E_ICON_BAR_LARGE_ICONS)
				disable = TRUE;
		
		if (shortcut_menu [i].label == NULL){
			menuitem = gtk_menu_item_new ();
			gtk_widget_set_sensitive (menuitem, FALSE);
		} else 
			menuitem = gtk_menu_item_new_with_label (_(shortcut_menu [i].label));

		if (disable)
			gtk_widget_set_sensitive (menuitem, FALSE);
		
		gtk_widget_show (menuitem);
		gtk_menu_append (GTK_MENU (menu), menuitem);

		gtk_signal_connect (
			GTK_OBJECT (menuitem), "activate",
			shortcut_menu [i].callback, &closure);
	}

	gtk_signal_connect (GTK_OBJECT (menu), "deactivate",
			    GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
			event->button.button, event->button.time);

	gtk_main ();

	gtk_object_destroy (GTK_OBJECT (menu));
}

typedef struct {
	EShellView     *eshell_view;
	EShortcutGroup *sg;
	EShortcut      *shortcut;
} closure_context_t;

static void
shortcut_open (GtkMenuItem *menuitem, closure_context_t *closure)
{
	e_shell_view_set_view (closure->eshell_view, closure->shortcut->efolder);
}

static void
shortcut_open_new_window (GtkMenuItem *menuitem, closure_context_t *closure)
{
	GtkWidget *toplevel;

	toplevel = e_shell_view_new (closure->eshell_view->eshell, closure->shortcut->efolder, FALSE);
	gtk_widget_show (toplevel);
}

static void
shortcut_remove (GtkMenuItem *menuitem, closure_context_t *closure)
{
	e_shortcut_group_remove (closure->sg, closure->shortcut);
}

static void
shortcut_rename (GtkMenuItem *menuitem, closure_context_t *closure)
{
	printf ("Implement: %s %s\n", __FILE__, __FUNCTION__);
}

static void
shortcut_properties (GtkMenuItem *menuitem, closure_context_t *closure)
{
	printf ("Implement: %s %s\n", __FILE__, __FUNCTION__);
}

#define NOT_IMPLEMENTED 1
static struct {
	char          *label;
	char          *stock_id;
	int            flags;
	GtkSignalFunc  callback;
} context_shortcut_menu [] = {
	{ N_("Open Folder"),        GNOME_STOCK_MENU_OPEN, 0, GTK_SIGNAL_FUNC (shortcut_open) },
	{ N_("Open in New Window"), NULL,                  0, GTK_SIGNAL_FUNC (shortcut_open_new_window) },
	{ N_("Advanced Find"),      NULL, NOT_IMPLEMENTED, NULL },
	{ NULL, },
	{ N_("Remove From Shortcut Bar"), NULL, 0, GTK_SIGNAL_FUNC (shortcut_remove) },
	{ N_("Rename Shortcut"),          NULL, 0, GTK_SIGNAL_FUNC (shortcut_rename) },
	{ NULL, },
	{ N_("Properties"),          NULL, 0, GTK_SIGNAL_FUNC (shortcut_properties) },
};

static void
shortcut_bar_show_context_popup (EShellView *eshell_view, GdkEvent *event,
				 EShortcutGroup *shortcut_group, EShortcut *shortcut)
{
	closure_context_t closure;
	GtkWidget *menu, *menuitem;
	int i;
	gboolean disable;
	
	menu = gtk_menu_new ();

	closure.eshell_view = eshell_view;
	closure.sg = shortcut_group;
	closure.shortcut = shortcut;
	
	for (i = 0; i < ELEMENTS (context_shortcut_menu); i++){
		disable = FALSE;
		
		if (context_shortcut_menu [i].flags & NOT_IMPLEMENTED)
			disable = TRUE;
			
		if (context_shortcut_menu [i].label == NULL){
			menuitem = gtk_menu_item_new ();
			gtk_widget_set_sensitive (menuitem, FALSE);
		} else {
			GtkWidget *label;
			
			if (context_shortcut_menu [i].stock_id){
				GtkWidget *stock;
				
				menuitem = gtk_pixmap_menu_item_new ();
				stock = gnome_stock_pixmap_widget (
					menu, 
					context_shortcut_menu [i].stock_id);
				if (stock){
					gtk_widget_show (stock);
					gtk_pixmap_menu_item_set_pixmap (
						GTK_PIXMAP_MENU_ITEM (menuitem), stock);
				}
			} else
				menuitem = gtk_menu_item_new ();
			
			label = gtk_label_new (_(context_shortcut_menu [i].label));
			gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
			gtk_widget_show (label);
			gtk_container_add (GTK_CONTAINER (menuitem), label);
		}

		if (disable)
			gtk_widget_set_sensitive (menuitem, FALSE);
		
		gtk_widget_show (menuitem);
		gtk_menu_append (GTK_MENU (menu), menuitem);

		gtk_signal_connect (
			GTK_OBJECT (menuitem), "activate",
			context_shortcut_menu [i].callback, &closure);
	}

	gtk_signal_connect (GTK_OBJECT (menu), "deactivate",
			    GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
			event->button.button, event->button.time);

	gtk_main ();

	gtk_object_destroy (GTK_OBJECT (menu));
}

void
shortcut_bar_item_selected (EShortcutBar *e_shortcut_bar,
			    GdkEvent *event, gint group_num, gint item_num,
			    EShellView *eshell_view)
{
	EShortcut *shortcut;
	EShortcutGroup *shortcut_group;
	EShortcutBarModel *shortcut_bar = eshell_view->eshell->shortcut_bar;
		
	shortcut_group = e_shortcut_group_from_pos (shortcut_bar, group_num);
	if (shortcut_group == NULL)
		return;
			
	shortcut = e_shortcut_from_pos (shortcut_group, item_num);

	if (shortcut == NULL)
		return;
	
	if (event->button.button == 1) {
		e_shell_view_set_view (eshell_view, shortcut->efolder);
	} else if (event->button.button == 3) {
		
		if (shortcut == NULL)
			shortcut_bar_show_standard_popup (
				eshell_view, event, shortcut_group);
		else
			shortcut_bar_show_context_popup (
				eshell_view, event, shortcut_group, shortcut);
	}
}

