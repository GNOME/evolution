/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Control applet ("capplet") for the gnome-pilot todo conduit,             */
/* based on                                                                 */
/* gpilotd control applet ('capplet') for use with the GNOME control center */

#include <pwd.h>
#include <sys/types.h>
#include <signal.h>
#include <gnome.h>

#include <config.h>
#include <capplet-widget.h>

#include <gpilotd/gnome-pilot-client.h>

#include "todo-conduit-config.h"

/* tell changes callbacks to ignore changes or not */
static gboolean ignore_changes=FALSE;

/* capplet widget */
static GtkWidget *capplet=NULL;

/* host/device/pilot configuration windows */
GtkWidget *cfgOptionsWindow=NULL;
GtkWidget *cfgStateWindow=NULL;
GtkWidget *dialogWindow=NULL;

gboolean activated,org_activation_state;
GnomePilotConduitManagement *conduit;
GnomePilotConduitConfig *conduit_config;
ToDoConduitCfg *origState = NULL;
ToDoConduitCfg *curState = NULL;

static void doTrySettings(GtkWidget *widget, ToDoConduitCfg *c);
static void doRevertSettings(GtkWidget *widget, ToDoConduitCfg *c);
static void doSaveSettings(GtkWidget *widget, ToDoConduitCfg *c);

static void readStateCfg (GtkWidget *w, ToDoConduitCfg *c);
static void setStateCfg (GtkWidget *w, ToDoConduitCfg *c);

gint pilotId;
static GnomePilotClient *gpc;



/* This array must be in the same order as enumerations
   in GnomePilotConduitSyncType as they are used as index.
   Custom type implies Disabled state.
*/
static gchar* sync_options[] ={ N_("Disabled"),
				N_("Synchronize"),
				N_("Copy From Pilot"),
				N_("Copy To Pilot"),
				N_("Merge From Pilot"),
				N_("Merge To Pilot")};
#define SYNC_OPTIONS_COUNT 6

static void
doTrySettings (GtkWidget *widget, ToDoConduitCfg *c)
{
	if (c->sync_type != GnomePilotConduitSyncTypeCustom)
		gnome_pilot_conduit_config_enable_with_first_sync (conduit_config,
								   c->sync_type,
								   c->sync_type,
								   TRUE);
	else
		gnome_pilot_conduit_config_disable (conduit_config);

	todoconduit_save_configuration (c);
}

static void
doRevertSettings (GtkWidget *widget, ToDoConduitCfg *c)
{
	activated = org_activation_state;
	*c = *origState;
	setStateCfg (cfgStateWindow, c);
	doTrySettings (widget, c);
}

static void
doSaveSettings (GtkWidget *widget, ToDoConduitCfg *c)
{
	*origState = *c;
	doTrySettings (widget, c);
}


static void 
doHelp (GtkWidget *widget, gpointer data) 
{
	GtkWidget *about;
	const gchar *authors[] = {_("Eskil Heyn Olsen <deity@eskil.dk>"),NULL};
  
	about = gnome_about_new (_("Gpilotd todo conduit"), VERSION,
				 _("(C) 1998 the Free Software Foundation"),
				 authors,
				 _("Configuration utility for the todo conduit.\n"),
				 _("gnome-unknown.xpm"));
	gtk_widget_show (about);
  
	return;
}


/* called by the sync_type GtkOptionMenu */
static void
activate_sync_type(GtkMenuItem *widget, gpointer data)
{
	curState->sync_type = GPOINTER_TO_INT(data);
	if(!ignore_changes)
		capplet_widget_state_changed(CAPPLET_WIDGET(capplet), TRUE);
}


static GtkWidget
*createStateCfgWindow(void)
{
	GtkWidget *vbox, *table;
	GtkWidget *label;
	GtkWidget *optionMenu,*menuItem;
	GtkMenu   *menu;
	gint i;
	
	vbox = gtk_vbox_new(FALSE, GNOME_PAD);

	table =  gtk_hbox_new(FALSE, 0); 
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, GNOME_PAD);

	label = gtk_label_new(_("Synchronize Action"));
	gtk_box_pack_start(GTK_BOX(table), label, FALSE, FALSE, GNOME_PAD);    

	optionMenu=gtk_option_menu_new();
	gtk_object_set_data(GTK_OBJECT(vbox), "conduit_state", optionMenu);
	menu = GTK_MENU(gtk_menu_new());

	for (i=0; i<SYNC_OPTIONS_COUNT;i++) {
		sync_options[i]=_(sync_options[i]);
		menuItem = gtk_menu_item_new_with_label(sync_options[i]);
		gtk_widget_show(menuItem);
		gtk_signal_connect(GTK_OBJECT(menuItem),"activate",
				   GTK_SIGNAL_FUNC(activate_sync_type),
				   GINT_TO_POINTER(i));
		gtk_menu_append(menu,menuItem);
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optionMenu),GTK_WIDGET(menu));
	gtk_box_pack_start(GTK_BOX(table), optionMenu, FALSE, FALSE, 0);    
	
	return vbox;
}


static void
setStateCfg (GtkWidget *w, ToDoConduitCfg *c)
{
	GtkOptionMenu *optionMenu;
	GtkMenu *menu;
	
	optionMenu = gtk_object_get_data (GTK_OBJECT(w), "conduit_state");
	g_assert (optionMenu != NULL);
	menu = GTK_MENU (gtk_option_menu_get_menu (optionMenu));
  
	ignore_changes = TRUE;
	/* Here were are relying on the items in menu being the same 
	   order as in GnomePilotConduitSyncType. */
	gtk_option_menu_set_history (optionMenu, (int) c->sync_type);
	ignore_changes = FALSE;
}


static void
readStateCfg (GtkWidget *w, ToDoConduitCfg *c)
{
}


static void
pilot_capplet_setup(void)
{
	GtkWidget *frame, *table;

	capplet = capplet_widget_new();

	table = gtk_table_new(1, 2, FALSE);
	gtk_container_border_width(GTK_CONTAINER(table), GNOME_PAD);
	gtk_container_add(GTK_CONTAINER(capplet), table); 

	frame = gtk_frame_new(_("Conduit state"));
	gtk_container_border_width(GTK_CONTAINER(frame), GNOME_PAD_SMALL);
	gtk_table_attach_defaults(GTK_TABLE(table), frame, 0, 1, 0, 1);
	cfgStateWindow = createStateCfgWindow();
	gtk_container_add(GTK_CONTAINER(frame), cfgStateWindow);

	gtk_signal_connect(GTK_OBJECT(capplet), "try",
			   GTK_SIGNAL_FUNC(doTrySettings), curState);
	gtk_signal_connect(GTK_OBJECT(capplet), "revert",
			   GTK_SIGNAL_FUNC(doRevertSettings), curState);
	gtk_signal_connect(GTK_OBJECT(capplet), "ok",
			   GTK_SIGNAL_FUNC(doSaveSettings), curState);
	gtk_signal_connect(GTK_OBJECT(capplet), "help",
			   GTK_SIGNAL_FUNC(doHelp), NULL);


	setStateCfg (cfgStateWindow, curState);

	gtk_widget_show_all (capplet);
}


static void 
run_error_dialog(gchar *mesg,...) 
{
	char tmp[80];
	va_list ap;

	va_start(ap,mesg);
	vsnprintf(tmp,79,mesg,ap);
	dialogWindow = gnome_message_box_new(mesg,GNOME_MESSAGE_BOX_ERROR,GNOME_STOCK_BUTTON_OK,NULL);
	gnome_dialog_run_and_close(GNOME_DIALOG(dialogWindow));
	va_end(ap);
}


static gint 
get_pilot_id_from_gpilotd() 
{
	GList *pilots=NULL;
	gint pilot;
	int i,err;
  
	i=0;
	/* we don't worry about leaking here, so pilots isn't freed */
	switch(err = gnome_pilot_client_get_pilots(gpc,&pilots)) {
	case GPILOTD_OK: {
		if(pilots) {
			for(i=0;i<g_list_length(pilots);i++) {
				g_message("pilot %d = \"%s\"",i,(gchar*)g_list_nth(pilots,i)->data); 
			}
			if(i==0) {
				run_error_dialog(_("No pilot configured, please choose the\n'Pilot Link Properties' capplet first."));
				return -1;
			} else {
				gnome_pilot_client_get_pilot_id_by_name(gpc,
									pilots->data,  /* this is the first pilot */
									&pilot);
				if(i>1) {
					g_message("too many pilots...");
					/* need a choose here */
				}
				return pilot;
			}
		} else {
			run_error_dialog(_("No pilot configured, please choose the\n'Pilot Link Properties' capplet first."));
			return -1;
		}    
		break;
	}
	case GPILOTD_ERR_NOT_CONNECTED:
		run_error_dialog(_("Not connected to the gnome-pilot daemon"));
		return -1;
		break;
	default:
		g_warning("gnome_pilot_client_get_pilot_ids(...) = %d",err);
		run_error_dialog(_("An error occured when trying to fetch\npilot list from the gnome-pilot daemon"));
		return -1;
		break;
	}
}


int
main (int argc, char *argv[])
{
	g_log_set_always_fatal (G_LOG_LEVEL_ERROR |
				G_LOG_LEVEL_CRITICAL |
				G_LOG_LEVEL_WARNING);
	
	/* Init capplet */
	gnome_capplet_init ("Evolution ToDo conduit control applet", 
			    NULL, argc, argv, 
			    NULL, 0, NULL);
   
	/* Setup Client */
	gpc = gnome_pilot_client_new ();
	gnome_pilot_client_connect_to_daemon (gpc);
	pilotId = get_pilot_id_from_gpilotd ();
	if (!pilotId) 
		return -1;

	
	/* Put all code to set things up in here */
	conduit = gnome_pilot_conduit_management_new ("todo_conduit", GNOME_PILOT_CONDUIT_MGMT_ID);
	if (conduit == NULL) 
		return -1;

	todoconduit_load_configuration (&origState, pilotId);
	conduit_config = gnome_pilot_conduit_config_new (conduit, pilotId);
	org_activation_state = activated =
		gnome_pilot_conduit_config_is_enabled (conduit_config,
						       &origState->sync_type);

	curState = todoconduit_dupe_configuration (origState);
    
	pilot_capplet_setup ();

	/* Done setting up, now run main loop */
	capplet_gtk_main();

	/* Clean up */
	gnome_pilot_conduit_management_destroy (conduit);
	todoconduit_destroy_configuration (&origState);
	todoconduit_destroy_configuration (&curState);
	
	return 0;
}    
