#include <gnome.h>
#include "color.h"
#include "pixmaps/font.xpm"
#include "widget-color-combo.h"
#include "color-palette.h"
/* To compile (from src/widgets):

gcc -I..  -I../.. -L. -Wall -o tester tester.c ../color.c `gnome-config --cflags --libs gnome gnomeui` -lwidgets

*/


gint
main ( gint argc, gchar* argv[] )
{
	GtkWidget * dialog;
	GtkWidget * T;
	
	gnome_init ("tester", "1.0", argc, argv);
	
	dialog = gnome_dialog_new ("TESTER", GNOME_STOCK_BUTTON_OK,
				   GNOME_STOCK_BUTTON_CANCEL, NULL);
	T = color_palette_new("Color Palette", NULL, "for_colorgroup");

	gtk_box_pack_start(GTK_BOX (GNOME_DIALOG (dialog)-> vbox ), 
			   T, TRUE, TRUE, 5);
	gtk_widget_show_all (T);

	T = color_combo_new (font_xpm, _("Automatic"),
			     &gs_black,
			     "for_colorgroup");
	gtk_box_pack_start(GTK_BOX (GNOME_DIALOG (dialog)-> vbox ), 
			   T, TRUE, TRUE, 5);
	gtk_widget_show_all (T);
	
	T = color_combo_new (font_xpm, _("Automatic"),
			     &gs_black,
			     "back_colorgroup");
	gtk_box_pack_start(GTK_BOX (GNOME_DIALOG (dialog)-> vbox ), 
			   T, TRUE, TRUE, 5);
	gtk_widget_show_all (T);

	gnome_dialog_run_and_close ( GNOME_DIALOG (dialog) );
	return 0;
}
		
