/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <pwd.h>
#include <sys/types.h>
#include <signal.h>
#include <gnome.h>

#include <config.h>
#include <capplet-widget.h>

#include <libgpilotdCM/gnome-pilot-conduit-management.h>
#include <libgpilotdCM/gnome-pilot-conduit-config.h>
#include <gpilotd/gnome-pilot-client.h>

#include "todo-conduit.h"

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
ConduitCfg *origState = NULL;
ConduitCfg *curState = NULL;

static void doTrySettings(GtkWidget *widget, ConduitCfg *conduitCfg);
static void doRevertSettings(GtkWidget *widget, ConduitCfg *conduitCfg);
static void doSaveSettings(GtkWidget *widget, ConduitCfg *conduitCfg);

static void readStateCfg(GtkWidget *w);
static void setStateCfg(GtkWidget *w);

gint pilotId;
CORBA_Environment ev;
static GnomePilotClient *gpc;

#if 0
static void 
load_configuration(ConduitCfg **c,
		   guint32 pilotId) 
{
	g_assert(c!=NULL);
	*c = g_new0(ConduitCfg,1);
	(*c)->pilotId = pilotId;
}
#endif /* 0 */


#if 0
static void 
save_configuration(ConduitCfg *c) 
{
	g_return_if_fail(c!=NULL);
}
#endif /* 0 */


static ConduitCfg*
dupe_configuration(ConduitCfg *c) {
	ConduitCfg *retval;
	g_return_val_if_fail(c!=NULL,NULL);
	retval = g_new0(ConduitCfg,1);
	retval->pilotId = c->pilotId;
	return retval;
}

#if 0
/** this method frees all data from the conduit config */
static void 
destroy_configuration(ConduitCfg **c) 
{
	g_return_if_fail(c!=NULL);
	g_return_if_fail(*c!=NULL);
	g_free(*c);
	*c = NULL;
}
#endif /* 0 */

static void
doTrySettings(GtkWidget *widget, ConduitCfg *conduitCfg)
{
	readStateCfg(cfgStateWindow);
	if(activated)
		gnome_pilot_conduit_config_enable(conduit_config,GnomePilotConduitSyncTypeCustom);
	else
		gnome_pilot_conduit_config_disable(conduit_config);
}

static void
doSaveSettings(GtkWidget *widget, ConduitCfg *conduitCfg)
{
	doTrySettings(widget, conduitCfg);
	save_configuration(conduitCfg);
}


static void
doRevertSettings(GtkWidget *widget, ConduitCfg *conduitCfg)
{
	activated = org_activation_state;
	setStateCfg(cfgStateWindow);
}

static void 
about_cb (GtkWidget *widget, gpointer data) 
{
	GtkWidget *about;
	const gchar *authors[] = {_("Eskil Heyn Olsen <deity@eskil.dk>"),NULL};
  
	about = gnome_about_new(_("Gpilotd todo conduit"), VERSION,
				_("(C) 1998 the Free Software Foundation"),
				authors,
				_("Configuration utility for the todo conduit.\n"),
				_("gnome-unknown.xpm"));
	gtk_widget_show (about);
  
	return;
}

static void toggled_cb(GtkWidget *widget, gpointer data) {
	if(!ignore_changes) {
		/* gtk_widget_set_sensitive(cfgOptionsWindow,GTK_TOGGLE_BUTTON(widget)->active); */
		capplet_widget_state_changed(CAPPLET_WIDGET(capplet), TRUE);
	}
}

static GtkWidget
*createStateCfgWindow(void)
{
	GtkWidget *vbox, *table;
	GtkWidget *label;
	GtkWidget *button;

	vbox = gtk_vbox_new(FALSE, GNOME_PAD);

	table = gtk_table_new(2, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, GNOME_PAD);

	label = gtk_label_new(_("Enabled"));
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1,2);

	button = gtk_check_button_new();
	gtk_object_set_data(GTK_OBJECT(vbox), "conduit_on_off", button);
	gtk_signal_connect(GTK_OBJECT(button), "toggled",
			   GTK_SIGNAL_FUNC(toggled_cb),
			   NULL);
	gtk_table_attach_defaults(GTK_TABLE(table), button, 1, 2, 1,2);

	return vbox;
}

static void
setStateCfg(GtkWidget *cfg)
{
	GtkWidget *button;

	button = gtk_object_get_data(GTK_OBJECT(cfg), "conduit_on_off");

	g_assert(button!=NULL);

	ignore_changes = TRUE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),activated);
	/* gtk_widget_set_sensitive(cfgOptionsWindow,GTK_TOGGLE_BUTTON(button)->active); */
	ignore_changes = FALSE;
}


static void
readStateCfg(GtkWidget *cfg)
{
	GtkWidget *button;

	button  = gtk_object_get_data(GTK_OBJECT(cfg), "conduit_on_off");
  
	g_assert(button!=NULL);

	activated = GTK_TOGGLE_BUTTON(button)->active;
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
			   GTK_SIGNAL_FUNC(about_cb), NULL);


	setStateCfg(cfgStateWindow);

	gtk_widget_show_all(capplet);
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
main( int argc, char *argv[] )
{
	/*
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
	*/
	
	/* we're a capplet */
	gnome_capplet_init ("todo conduit control applet", NULL, argc, argv, 
			    NULL,
			    0, NULL);

   
	gpc = gnome_pilot_client_new();
	gnome_pilot_client_connect_to_daemon(gpc);
	pilotId = get_pilot_id_from_gpilotd();
	if(!pilotId) return -1;

	/* put all code to set things up in here */
	load_configuration(&origState,pilotId);
	curState = dupe_configuration(origState);

	/* put all code to set things up in here */
	conduit = gnome_pilot_conduit_management_new ("todo_conduit",
						      GNOME_PILOT_CONDUIT_MGMT_ID);
	if (conduit==NULL) return -1;
	conduit_config = gnome_pilot_conduit_config_new(conduit,pilotId);
	org_activation_state = activated = gnome_pilot_conduit_config_is_enabled(conduit_config,NULL);
    
	pilot_capplet_setup();


	/* done setting up, now run main loop */
	capplet_gtk_main();
    
	gnome_pilot_conduit_management_destroy(conduit);

	return 0;
}    
