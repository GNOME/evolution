#include <config.h>

#include <gnome.h>
#include <bonobo.h>

#include "Evolution.h"
#include "evolution-storage.h"

#include "evolution-shell-component.h"

#include "e-util/e-gui-utils.h"

#define COMPONENT_FACTORY_ID "OAFIID:evolution-shell-component-factory:evolution-notes:f2f0f57f-27d9-4498-b54b-248f223ee772"

static const EvolutionShellComponentFolderType folder_types[] = {
	{ "notes", "evolution-notes.png" },
	{ NULL, NULL }
};

static void
new_note_cb (BonoboUIHandler *uih, void *user_data, const char *path)
{
	g_print ("new note!\n");
}

static GnomeUIInfo gnome_toolbar [] = {
	GNOMEUIINFO_ITEM_STOCK (N_("New"), N_("Create a new note"), new_note_cb, GNOME_STOCK_PIXMAP_NEW),
	GNOMEUIINFO_END
};

static void
control_deactivate (BonoboControl *control, BonoboUIHandler *uih)
{
	bonobo_ui_handler_dock_remove (uih, "/Toolbar");
}

static void
control_activate (BonoboControl *control, BonoboUIHandler *uih)
{
	Bonobo_UIHandler remote_uih;
	GtkWidget *toolbar, *toolbar_frame;
	BonoboControl *toolbar_control ;

	remote_uih = bonobo_control_get_remote_ui_handler (control);
	bonobo_ui_handler_set_container (uih, remote_uih);

	toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL,
				   GTK_TOOLBAR_BOTH);

	gnome_app_fill_toolbar_with_data (GTK_TOOLBAR (toolbar),
					  gnome_toolbar, 
					  NULL, NULL);

	toolbar_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (toolbar_frame), GTK_SHADOW_OUT);
	gtk_container_add (GTK_CONTAINER (toolbar_frame), toolbar);
	gtk_widget_show (toolbar_frame);

	gtk_widget_show_all (toolbar_frame);

	toolbar_control = bonobo_control_new (toolbar_frame);
	bonobo_ui_handler_dock_add (
				    uih, "/Toolbar",
				    bonobo_object_corba_objref (BONOBO_OBJECT (toolbar_control)),
				    GNOME_DOCK_ITEM_BEH_EXCLUSIVE,
				    GNOME_DOCK_TOP,
				    1, 1, 0);
}

		  
static void
control_activate_cb (BonoboControl *control,
		     gboolean activate)
{
	BonoboUIHandler *uih;

	uih = bonobo_control_get_ui_handler (control);
	g_assert (uih);

	if (activate)
		control_activate (control, uih);
	else
		control_deactivate (control, uih);
}


static BonoboControl *
create_view (EvolutionShellComponent *shell_component,
	     const char *physical_uri,
	     void *closure)
{
	BonoboControl * control;

	control = notes_factory_new_control ();

	gtk_signal_connect (GTK_OBJECT (control), "activate",
			    control_activate_cb, NULL);
	

	return control;
}

static void
owner_set_cb (EvolutionShellComponent *shell_component,
	      EvolutionShellClient shell_client,
	      gpointer user_data)
{
	g_print ("evolution-notes: Yeeeh! We have an owner!\n");	/* FIXME */
}

static void
owner_unset_cb (EvolutionShellComponent *shell_component, gpointer user_data)
{
	g_print ("No owner anymore\n");
}

/* The factory function */
static BonoboObject *
notes_component_factory (BonoboGenericFactory *factory,
			 void *closure)
{
	EvolutionShellComponent *shell_component;

	shell_component = evolution_shell_component_new (folder_types, create_view, NULL);

	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_set",
			    GTK_SIGNAL_FUNC (owner_set_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_unset",
			    GTK_SIGNAL_FUNC (owner_unset_cb), NULL);
	
	return BONOBO_OBJECT (shell_component);
}


void
component_factory_init (void)
{
	static BonoboGenericFactory *factory = NULL;

	if (factory != NULL)
		return;

	factory = bonobo_generic_factory_new (COMPONENT_FACTORY_ID, notes_component_factory, NULL);

	if (factory == NULL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize Evolution's notes component."));
		exit (1);
	}
}
