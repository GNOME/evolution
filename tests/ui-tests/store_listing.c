/* Bertrand.Guiheneuf@aful.org                                               */



/* 
   gcc -o store_listing `gnome-config --cflags gnomeui libglade` \
   store_listing.c `gnome-config --libs gnomeui libglade`
*/
/******************************************************************************/


#include <gnome.h>
#include <glade/glade.h>


static GladeXML *xml;


void
on_exit1_activate (GtkWidget *widget, void *data)
{
  gtk_main_quit ();
}


void
on_about1_activate (GtkWidget *widget, void *data)
{
   GtkWidget *about_widget;
   
   about_widget = glade_xml_get_widget (xml, "about_widget");
   gtk_widget_show (about_widget);
}




int
main(int argc, char *argv[])
{
 
  gnome_init ("store_listing", "1.0", argc, argv);

  glade_gnome_init();

  xml = glade_xml_new ("store_listing.glade", NULL);
  if (xml) glade_xml_signal_autoconnect (xml);

  
  gtk_main ();
  
  return 0;
}

