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

#define SMALL_ICONS 1
#define LARGE_ICONS 2

static void
set_large_icons (GtkMenu *menu, EShellView *eshell_view)
{
}

static void
set_small_icons (GtkMenu *menu, EShellView *eshell_view)
{
}

static void
add_group (GtkMenu *menu, EShellView *eshell_view)
{
}

static void
remove_group (GtkMenu *menu, EShellView *eshell_view)
{
}

static void
rename_group (GtkMenu *menu, EShellView *eshell_view)
{
}

static void
add_shortcut (GtkMenu *menu, EShellView *eshell_view)
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
	
	menu = gtk_menu_new ();

	for (i = 0; i < ELEMENTS (shortcut_menu); i++){
		if (shortcut_menu [i].flags & SMALL_ICONS)
			if (!shortcut_group->small_icons)
				continue;

		if (shortcut_menu [i].flags & LARGE_ICONS)
			if (shortcut_group->small_icons)
				continue;
		
		if (shortcut_menu [i].label == NULL){
			menuitem = gtk_menu_item_new ();
			gtk_widget_set_sensitive (menuitem, FALSE);
		} else 
			menuitem = gtk_menu_item_new_with_label (_(shortcut_menu [i].label));
		
		gtk_widget_show (menuitem);
		gtk_menu_append (GTK_MENU (menu), menuitem);

		gtk_signal_connect (
			GTK_OBJECT (menuitem), "activate",
			shortcut_menu [i].callback, eshell_view);
		gtk_object_set_data (
			GTK_OBJECT (menuitem), "shortcut_group",
			shortcut_group);
	}

	gtk_signal_connect (GTK_OBJECT (menu), "deactivate",
			    GTK_SIGNAL_FUNC (gtk_object_destroy), NULL);
	
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
			event->button.button, event->button.time);
}

static void
shortcut_bar_show_context_popup (EShellView *eshell_view, GdkEvent *event, EShortcutGroup *shortcut_group)
{
	printf ("Context popup\n");
}

static EShortcut *
shortcut_from_pos (EShellView *eshell_view, int group_num, int item_num, EShortcutGroup **group_result)
{
	EShell *eshell = eshell_view->eshell;
	EShortcutGroup *group;
	EShortcut *shortcut;

	if (item_num == -1)
		return NULL;
	
	g_assert (group_num < eshell->shortcut_groups->len);
	group = g_array_index (eshell->shortcut_groups, EShortcutGroup *, group_num);

	g_assert (item_num < group->shortcuts->len);
	shortcut = g_array_index (group->shortcuts, EShortcut *, item_num);

	*group_result = group;

	return shortcut;
}

void
shortcut_bar_item_selected (EShortcutBar *shortcut_bar,
			    GdkEvent *event, gint group_num, gint item_num,
			    EShellView *eshell_view)
{
	EShortcut *shortcut;
	EShortcutGroup *shortcut_group;

	shortcut = shortcut_from_pos (eshell_view, group_num, item_num, &shortcut_group);

	if (group_num == -1)
		return;
	
	if (event->button.button == 1) {
		printf ("Item Selected - %i:%i", group_num + 1, item_num + 1);
	} else if (event->button.button == 3) {
		
		if (shortcut == NULL)
			shortcut_bar_show_standard_popup (
				eshell_view, event, shortcut_group);
		else
			shortcut_bar_show_context_popup (
				eshell_view, event, shortcut);
	}
}

