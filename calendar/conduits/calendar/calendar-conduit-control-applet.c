/* Control applet ("capplet") for the gnome-pilot calendar conduit,         */
/* based on                                                                 */
/* gpilotd control applet ('capplet') for use with the GNOME control center */
/* $Id$ */

#include <pwd.h>
#include <sys/types.h>
#include <signal.h>
#include <gnome.h>
#include <ctype.h>

#include <config.h>
#include <capplet-widget.h>

#include <gpilotd/gpilotd-conduit-mgmt.h>
#include <gpilotd/gpilotd-app.h>
#include <gpilotd/gpilotd-app-dummy-callbacks.h>

#include "calendar-conduit.h"

/* tell changes callbacks to ignore changes or not */
static gboolean ignore_changes=FALSE;

/* capplet widget */
static GtkWidget *capplet=NULL;

/* host/device/pilot configuration windows */
GtkWidget *cfgOptionsWindow=NULL;
GtkWidget *cfgStateWindow=NULL;
GtkWidget *dialogWindow=NULL;

GnomePilotConduitMgmt *conduit;

static void doTrySettings(GtkWidget *widget, gpointer);
static void doRevertSettings(GtkWidget *widget, gpointer);
static void doSaveSettings(GtkWidget *widget, gpointer);

static void readStateCfg(GtkWidget *w,GCalConduitCfg *cfg);
static void setStateCfg(GtkWidget *w,GCalConduitCfg *cfg);
void about_cb (GtkWidget *, gpointer);
GCalConduitCfg *origState = NULL;
GCalConduitCfg *curState = NULL;

gint pilotId;
CORBA_Environment ev;

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
setSettings(GCalConduitCfg* conduitCfg)
{
	if(conduitCfg->sync_type!=GnomePilotConduitSyncTypeCustom)
		gpilotd_conduit_mgmt_enable(conduit,pilotId,conduitCfg->sync_type);
	else
		gpilotd_conduit_mgmt_disable(conduit,pilotId);

	gcalconduit_save_configuration(conduitCfg);
}

static void
doTrySettings(GtkWidget *widget, gpointer whatever)
{
	readStateCfg(cfgStateWindow,curState);
	setSettings(curState);
}

static void
doSaveSettings(GtkWidget *widget, gpointer whatever)
{
	doTrySettings(widget,whatever);
}

static void
doCancelSettings(GtkWidget *widget, gpointer whatever)
{
	setSettings(origState);
}

static void
doRevertSettings(GtkWidget *widget, gpointer whatever)
{
	gcalconduit_destroy_configuration(&curState);
	curState = gcalconduit_dupe_configuration(origState);
	setStateCfg(cfgStateWindow,curState);
	setSettings(curState);
}

static void
insert_dir_callback (GtkEditable    *editable, const gchar    *text,
		     gint len, gint *position, void *data)
{
    gint i;
    gchar *curname;

    curname = gtk_entry_get_text(GTK_ENTRY(editable));
    if (*curname == '\0' && len > 0) {
	if (isspace(text[0])) {
	    gtk_signal_emit_stop_by_name(GTK_OBJECT(editable), "insert_text");
	    return;
	}
    } else {
	for (i=0; i<len; i++) {
	    if (isspace(text[i])) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(editable), 
					     "insert_text");
		return;
	    }
	}
    }
}
static void
insert_dir_callback2(GtkEditable    *editable, const gchar    *text,
		      gint            length, gint           *position,
		      void *data)
{
    if (!ignore_changes)
        capplet_widget_state_changed(CAPPLET_WIDGET(capplet), TRUE);
}

static void
clist_changed(GtkWidget *widget, gpointer data)
{
    if (!ignore_changes)
	capplet_widget_state_changed(CAPPLET_WIDGET(capplet), TRUE);
}
	
void about_cb (GtkWidget *widget, gpointer data) {
  GtkWidget *about;
  const gchar *authors[] = {_("Eskil Heyn Olsen <deity@eskil.dk>"),NULL};
  
  about = gnome_about_new(_("GnomeCalendar Conduit"), VERSION,
			  _("(C) 1998"),
			  authors,
			  _("Configuration utility for the calendar conduit.\n"),
			  _("gnome-calendar-conduit.png"));
  gtk_widget_show (about);
  
  return;
}

static void toggled_cb(GtkWidget *widget, gpointer data) {
  capplet_widget_state_changed(CAPPLET_WIDGET(capplet), TRUE);
}

/* called by the sync_type GtkOptionMenu */
static void
sync_action_selection(GtkMenuShell *widget, gpointer unused) 
{
	if (!ignore_changes) {
		capplet_widget_state_changed(CAPPLET_WIDGET (capplet), TRUE);
	}
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
	gtk_signal_connect(GTK_OBJECT(menu), "selection-done",
			   GTK_SIGNAL_FUNC(sync_action_selection),
			   NULL);
  
	gtk_box_pack_start(GTK_BOX(table), optionMenu, FALSE, FALSE, 0);    
	
	return vbox;
}

static void
setStateCfg(GtkWidget *widget,GCalConduitCfg *cfg)
{
	GtkOptionMenu *optionMenu;
	GtkMenu *menu;
	
	optionMenu = gtk_object_get_data(GTK_OBJECT(widget), "conduit_state");
	g_assert(optionMenu!=NULL);
	menu = GTK_MENU(gtk_option_menu_get_menu(optionMenu));

  
	ignore_changes = TRUE;
	/* Here were are relying on the items in menu being the same 
	   order as in GnomePilotConduitSyncType. */
	gtk_option_menu_set_history(optionMenu,(int)cfg->sync_type);
	ignore_changes = FALSE;
}


static void
readStateCfg(GtkWidget *w,GCalConduitCfg *cfg)
{
/*
  GtkWidget *button;

  button  = gtk_object_get_data(GTK_OBJECT(w), "conduit_on_off");
  
  g_assert(button!=NULL);

  activated = GTK_TOGGLE_BUTTON(button)->active;
*/
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
			   GTK_SIGNAL_FUNC(doTrySettings), NULL);
	gtk_signal_connect(GTK_OBJECT(capplet), "revert",
			   GTK_SIGNAL_FUNC(doRevertSettings), NULL);
	gtk_signal_connect(GTK_OBJECT(capplet), "ok",
			   GTK_SIGNAL_FUNC(doSaveSettings), NULL);
	gtk_signal_connect(GTK_OBJECT(capplet), "cancel",
			   GTK_SIGNAL_FUNC(doCancelSettings), NULL);
	gtk_signal_connect(GTK_OBJECT(capplet), "help",
			   GTK_SIGNAL_FUNC(about_cb), NULL);


	setStateCfg(cfgStateWindow,curState);

	gtk_widget_show_all(capplet);
}

static void 
run_error_dialog(gchar *mesg,...) {
	char tmp[80];
	va_list ap;
	
	va_start(ap,mesg);
	vsnprintf(tmp,79,mesg,ap);
	dialogWindow = gnome_message_box_new(mesg,GNOME_MESSAGE_BOX_ERROR,GNOME_STOCK_BUTTON_OK,NULL);
	gnome_dialog_run_and_close(GNOME_DIALOG(dialogWindow));
	va_end(ap);
}

gint get_pilot_id_from_gpilotd() {
	gint *pilots;
	int i;
  
	i=0;
	gpilotd_get_pilot_ids(&pilots);
	if(pilots) {
		while(pilots[i]!=-1) { g_message("pilot %d = \"%d\"",i,pilots[i]); i++; }
		if(i==0) {
			run_error_dialog(_("No pilot configured, please choose the\n'Pilot Link Properties' capplet first."));
			return -1;
		} else {
			if(i==1) 
				return pilots[0];
			else {
				g_message("too many pilots...");
				return pilots[0];
			}
		}
	} else {
		run_error_dialog(_("No pilot configured, please choose the\n'Pilot Link Properties' capplet first."));
		return -1;
	}    
}

int
main( int argc, char *argv[] )
{
	/* we're a capplet */
	gnome_capplet_init ("calendar conduit control applet", NULL, argc, argv, 
			    NULL,
			    0, NULL);

	/* put all code to set things up in here */
	conduit = gpilotd_conduit_mgmt_new("calendar_conduit");

	/* get pilot name from gpilotd */
	/* 1. initialize the gpilotd connection */
	if (gpilotd_init(&argc,argv)!=0) {
		run_error_dialog(_("Cannot initialze the GnomePilot Daemon"));
		g_error(_("Cannot initialze the GnomePilot Daemon"));
		return -1;
	}
    
	/* 2 connect to gpilotd */
	if (gpilotd_connect()!=0) {
		run_error_dialog(_("Cannot connect to the GnomePilot Daemon"));
		g_error(_("Cannot connect to the GnomePilot Daemon"));
		return -1;
	}
    
	pilotId = get_pilot_id_from_gpilotd();
	if(!pilotId) return -1;

	/* put all code to set things up in here */
	gcalconduit_load_configuration(&origState,pilotId);
	gpilotd_conduit_mgmt_get_sync_type(conduit,pilotId,&origState->sync_type);

	curState = gcalconduit_dupe_configuration(origState);
    
	pilot_capplet_setup();


	/* done setting up, now run main loop */
	capplet_gtk_main();
	return 0;
}    
