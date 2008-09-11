/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>  
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <gnome.h>

#include <glib/gi18n.h>

#include "color-palette.h"
#include "e-colors.h"
#include "widget-color-combo.h"

#include "pixmaps/cursor_hand_open.xpm"

/* To compile (from src/widgets):

gcc -I..  -I../.. -L. -Wall -o tester tester.c ../color.c `gnome-config --cflags --libs gnome gnomeui` -lwidgets

*/

gint
main ( gint argc, gchar* argv[] )
{
	GtkWidget * dialog;
	GtkWidget * T;
	ColorGroup *cg;

	gnome_program_init ("tester", "1.0",
			    LIBGNOMEUI_MODULE,
			    argc, argv, NULL);

	dialog = gnome_dialog_new ("TESTER", GNOME_STOCK_BUTTON_OK,
				   GNOME_STOCK_BUTTON_CANCEL, NULL);

	cg = color_group_fetch ("fore_color_group", dialog);
	T = color_palette_new ("Color Palette", NULL, cg);

	gtk_box_pack_start(GTK_BOX (GNOME_DIALOG (dialog)-> vbox ),
			   T, TRUE, TRUE, 5);
	gtk_widget_show_all (T);

	cg = color_group_fetch ("fore_color_group", dialog);
	T = color_combo_new (
		gdk_pixbuf_new_from_xpm_data ((char const **)cursor_hand_open_xpm),
		_("Automatic"), &e_black, cg);
	gtk_box_pack_start(GTK_BOX (GNOME_DIALOG (dialog)-> vbox ),
			   T, TRUE, TRUE, 5);
	gtk_widget_show_all (T);

	cg = color_group_fetch ("back_color_group", dialog);
	T = color_combo_new (
		gdk_pixbuf_new_from_xpm_data ((char const **)cursor_hand_open_xpm),
		_("Automatic"), &e_black, cg);
	gtk_box_pack_start(GTK_BOX (GNOME_DIALOG (dialog)-> vbox ),
			   T, TRUE, TRUE, 5);
	gtk_widget_show_all (T);

	gnome_dialog_run_and_close ( GNOME_DIALOG (dialog) );
	return 0;
}
