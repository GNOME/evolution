/*
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
 *
 * Authors:
 *		Miguel de Icaza <miguel@ximian.com>
 *		Jody Goldberg (jgoldberg@home.com)
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <libintl.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "e-popup-menu.h"

/*
 * Creates an item with an optional icon
 */
static void
make_item (GtkMenu *menu,
           GtkMenuItem *item,
           const gchar *name)
{
	GtkWidget *label;

	if (*name == '\0')
		return;

	/*
	 * Ugh.  This needs to go into Gtk+
	 */
	label = gtk_label_new_with_mnemonic (name);
	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_widget_show (label);

	gtk_container_add (GTK_CONTAINER (item), label);
}

GtkMenu *
e_popup_menu_create_with_domain (EPopupMenu *menu_list,
                                 guint32 disable_mask,
                                 guint32 hide_mask,
                                 gpointer default_closure,
                                 const gchar *domain)
{
	GtkMenu *menu = GTK_MENU (gtk_menu_new ());
	gboolean last_item_separator = TRUE;
	gint last_non_separator = -1;
	gint i;

	for (i = 0; menu_list[i].name; i++) {
		if (strcmp ("", menu_list[i].name) && !(menu_list[i].disable_mask & hide_mask)) {
			last_non_separator = i;
		}
	}

	for (i = 0; i <= last_non_separator; i++) {
		gboolean separator;

		separator = !strcmp ("", menu_list[i].name);

		if ((!(separator && last_item_separator)) && !(menu_list[i].disable_mask & hide_mask)) {
			GtkWidget *item = NULL;

			if (!separator) {
				item = gtk_menu_item_new ();

				make_item (menu, GTK_MENU_ITEM (item), dgettext (domain, menu_list[i].name));
			} else {
				item = gtk_menu_item_new ();
			}

			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

			if (menu_list[i].fn)
				g_signal_connect (
					item, "activate",
					G_CALLBACK (menu_list[i].fn),
					default_closure);

			if (menu_list[i].disable_mask & disable_mask)
				gtk_widget_set_sensitive (item, FALSE);

			gtk_widget_show (item);

			last_item_separator = separator;
		}
	}

	return menu;
}
