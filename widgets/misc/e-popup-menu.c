/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-popup-menu.c: popup menu display
nnn *
 * Authors:
 *   Miguel de Icaza (miguel@kernel.org)
 *   Jody Goldberg (jgoldberg@home.com)
 *   Jeffrey Stedfast <fejj@helixcode.com>
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gnome.h>
#include "e-popup-menu.h"
#include "e-gui-utils.h"
#include "gal/util/e-i18n.h"

/*
 * Creates an item with an optional icon
 */
static GtkWidget *
make_item (GtkMenu *menu, const char *name, const char *pixname)
{
	GtkWidget *label, *item;
	guint label_accel;

	if (*name == '\0')
		return gtk_menu_item_new ();
	
	/*
	 * Ugh.  This needs to go into Gtk+
	 */
	label = gtk_accel_label_new ("");
	label_accel = gtk_label_parse_uline (GTK_LABEL (label), name);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	
	item = pixname ? gtk_pixmap_menu_item_new () : gtk_menu_item_new ();
	gtk_container_add (GTK_CONTAINER (item), label);
	
	if (label_accel != GDK_VoidSymbol){
		gtk_widget_add_accelerator (
			item,
			"activate_item",
			gtk_menu_ensure_uline_accel_group (GTK_MENU (menu)),
			label_accel, 0,
			GTK_ACCEL_LOCKED);
	}

	if (pixname){
		GtkWidget *pixmap = gnome_stock_pixmap_widget (item, pixname);

		gtk_widget_show (pixmap);
		gtk_pixmap_menu_item_set_pixmap (
			GTK_PIXMAP_MENU_ITEM (item), pixmap);
	}

	return item;
}

GtkMenu *
e_popup_menu_create (EPopupMenu *menu_list, guint32 disable_mask, guint32 hide_mask, void *closure)
{
	GtkMenu *menu = GTK_MENU (gtk_menu_new ());
	gboolean last_item_seperator = TRUE;
	gint last_non_seperator = -1;
	gint i;
	
	for (i = 0; menu_list[i].name; i++) {
		if (strcmp ("", menu_list[i].name) && !(menu_list [i].disable_mask & hide_mask)) {
			last_non_seperator = i;
		}
	}
	
	for (i = 0; i <= last_non_seperator; i++) {
		gboolean seperator;
		
		seperator = !strcmp ("", menu_list[i].name);
		
		if ((!(seperator && last_item_seperator)) && !(menu_list [i].disable_mask & hide_mask)) {
			GtkWidget *item;
			
			item = make_item (menu, seperator ? "" : _(menu_list[i].name), menu_list[i].pixname);
			gtk_menu_append (menu, item);
			
			if (!menu_list[i].submenu) {
				if (menu_list[i].fn)
					gtk_signal_connect (GTK_OBJECT (item), "activate",
							    GTK_SIGNAL_FUNC (menu_list[i].fn),
							    closure);
			} else {
				/* submenu */
				GtkMenu *submenu;
				
				submenu = e_popup_menu_create (menu_list[i].submenu, disable_mask, hide_mask, closure);
				
				gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), GTK_WIDGET (submenu));
			}
			
			if (menu_list[i].disable_mask & disable_mask)
				gtk_widget_set_sensitive (item, FALSE);
			
			gtk_widget_show (item);
		}
		
		last_item_seperator = seperator;
	}
	
	return menu;
}

void
e_popup_menu_run (EPopupMenu *menu_list, GdkEvent *event, guint32 disable_mask, guint32 hide_mask, void *closure)
{
	GtkMenu *menu;
	
	g_return_if_fail (menu_list != NULL);
	g_return_if_fail (event != NULL);
	
	menu = e_popup_menu_create (menu_list, disable_mask, hide_mask, closure);
	
	e_popup_menu (menu, event);
}

