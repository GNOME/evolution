#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include <bonobo.h>
#include <executive-summary-component.h>
#include <liboaf/liboaf.h>

static int running_views = 0;
ExecutiveSummaryComponent *component;

#define TEST_SERVICE_ID "OAFIID:evolution-summary-component-factory:test-service:0ea887d5-622b-4b8c-b525-18aa1cbe18a6"

static BonoboGenericFactory *factory = NULL;

void
clicked_cb (GtkWidget *widget,
	    gpointer data) 
{
  executive_summary_component_set_title (component, "Iain's title");
  executive_summary_component_flash (component);
}

void
view_destroyed (GtkWidget *widget,
		gpointer data)
{
  running_views--;

  g_print ("Destroying view: %d\n", running_views);

  if (running_views <= 0) {
    g_print ("No views left, quitting\n");
    gtk_main_quit ();
  }
}

static BonoboObject *
create_view (ExecutiveSummaryComponent *component,
	     char **title,
	     void *closure)
{
  BonoboControl *control;
  GtkWidget *button;

  *title = g_strdup ("This is the test bonobo service");
  button = gtk_button_new_with_label ("A test service with a whole button");
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (clicked_cb), NULL);
  
  control = bonobo_control_new (button);
  gtk_signal_connect (GTK_OBJECT (control), "destroy",
		      GTK_SIGNAL_FUNC (view_destroyed), NULL);
  
  gtk_widget_show_all (button);

  g_assert (control != NULL);

  return BONOBO_OBJECT (control);
}

static char *
create_html (ExecutiveSummaryComponent *component,
	     char **title,
	     void *closure)
{
  *title = g_strdup ("This is the test service");
  return g_strdup ("<b>This is<p>An <i>HTML</i></b><br><h1>Component!!!</h1>");
}

static void
configure (ExecutiveSummaryComponent *component,
	   void *closure)
{
  GtkWidget *window, *label;

  g_print ("configuring\n");
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  label = gtk_label_new ("This is a configuration dialog.\nNo it really is");

  gtk_container_add (GTK_CONTAINER (window), label);
  gtk_widget_show_all (window);
}
  
static BonoboObject *
factory_fn (BonoboGenericFactory *_factory,
	    void *closure)
{
  running_views++;
  component = executive_summary_component_new (create_view,
					       create_html,
					       configure,
					       NULL);
  gtk_signal_connect (GTK_OBJECT (component), "destroy",
		      GTK_SIGNAL_FUNC (view_destroyed), NULL);
  return BONOBO_OBJECT (component);
}

void
test_service_factory_init (void)
{
  if (factory != NULL)
    return;

  factory = bonobo_generic_factory_new (TEST_SERVICE_ID, factory_fn, NULL);
  if (factory == NULL) {
    g_warning ("Cannot initialize test service");
    exit (0);
  }
}

int
main (int argc, char **argv)
{
  CORBA_ORB orb;

  gnome_init_with_popt_table ("Test service", VERSION,
			      argc, argv, oaf_popt_options, 0, NULL);
  orb = oaf_init (argc, argv);

  if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE) {
    g_error ("Could not initialize Bonobo");
  }

  test_service_factory_init ();

  bonobo_main ();

  return 0;
}

