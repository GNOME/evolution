#include <gnome.h>
#include "widget-color-combo.h"
#include "color-palette.h"
#include "e-colors.h"
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

	gnome_init ("tester", "1.0", argc, argv);

	dialog = gnome_dialog_new ("TESTER", GNOME_STOCK_BUTTON_OK,
				   GNOME_STOCK_BUTTON_CANCEL, NULL);
	cg = color_group_fetch ("fore_color_group", dialog);
	T = color_palette_new ("Color Palette", NULL, cg);

	gtk_box_pack_start(GTK_BOX (GNOME_DIALOG (dialog)-> vbox ),
			   T, TRUE, TRUE, 5);
	gtk_widget_show_all (T);

	cg = color_group_fetch ("fore_color_group", dialog);
	T = color_combo_new (cursor_hand_open_xpm, _("Automatic"),
			     &e_black, cg);
	gtk_box_pack_start(GTK_BOX (GNOME_DIALOG (dialog)-> vbox ),
			   T, TRUE, TRUE, 5);
	gtk_widget_show_all (T);

	cg = color_group_fetch ("back_color_group", dialog);
	T = color_combo_new (cursor_hand_open_xpm, _("Automatic"),
			     &e_black, cg);
	gtk_box_pack_start(GTK_BOX (GNOME_DIALOG (dialog)-> vbox ),
			   T, TRUE, TRUE, 5);
	gtk_widget_show_all (T);

	gnome_dialog_run_and_close ( GNOME_DIALOG (dialog) );
	return 0;
}
