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

gboolean activated,org_activation_state;
GnomePilotConduitMgmt *conduit;

static void doTrySettings(GtkWidget *widget, ConduitCfg *conduitCfg);
static void doRevertSettings(GtkWidget *widget, ConduitCfg *conduitCfg);
static void doSaveSettings(GtkWidget *widget, ConduitCfg *conduitCfg);

static void readStateCfg(GtkWidget *w);
static void setStateCfg(GtkWidget *w);

gchar *pilotId;
CORBA_Environment ev;

static void
doTrySettings(GtkWidget *widget, ConduitCfg *conduitCfg)
{
    readStateCfg(cfgStateWindow);
    if(activated)
      gpilotd_conduit_mgmt_enable(conduit,pilotId);
    else
      gpilotd_conduit_mgmt_disable(conduit,pilotId);
}

static void
doSaveSettings(GtkWidget *widget, ConduitCfg *conduitCfg)
{
    doTrySettings(widget, conduitCfg);
    save_configuration(NULL);
}


static void
doRevertSettings(GtkWidget *widget, ConduitCfg *conduitCfg)
{
    activated = org_activation_state;
    setStateCfg(cfgStateWindow);
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
  
  about = gnome_about_new(_("Gpilotd calendar conduit"), VERSION,
			  _("(C) 1998 the Free Software Foundation"),
			  authors,
			  _("Configuration utility for the calendar conduit.\n"),
			  _("gnome-unknown.xpm"));
  gtk_widget_show (about);
  
  return;
}

static void toggled_cb(GtkWidget *widget, gpointer data) {
  gtk_widget_set_sensitive(cfgOptionsWindow,GTK_TOGGLE_BUTTON(widget)->active);
  capplet_widget_state_changed(CAPPLET_WIDGET(capplet), TRUE);
}

static GtkWidget
*createStateCfgWindow(void)
{
    GtkWidget *vbox, *table;
    GtkWidget *entry, *label;
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
    gchar num[40];

    button = gtk_object_get_data(GTK_OBJECT(cfg), "conduit_on_off");

    g_assert(button!=NULL);

    ignore_changes = TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),activated);
    gtk_widget_set_sensitive(cfgOptionsWindow,GTK_TOGGLE_BUTTON(button)->active);
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
			GTK_SIGNAL_FUNC(doTrySettings), NULL);
    gtk_signal_connect(GTK_OBJECT(capplet), "revert",
			GTK_SIGNAL_FUNC(doRevertSettings), NULL);
    gtk_signal_connect(GTK_OBJECT(capplet), "ok",
			GTK_SIGNAL_FUNC(doSaveSettings), NULL);
    gtk_signal_connect(GTK_OBJECT(capplet), "help",
			GTK_SIGNAL_FUNC(about_cb), NULL);


    setStateCfg(cfgStateWindow);

    gtk_widget_show_all(capplet);
}

void run_error_dialog(gchar *mesg,...) {
  char tmp[80];
  va_list ap;

  va_start(ap,mesg);
  vsnprintf(tmp,79,mesg,ap);
  dialogWindow = gnome_message_box_new(mesg,GNOME_MESSAGE_BOX_ERROR,GNOME_STOCK_BUTTON_OK,NULL);
  gnome_dialog_run_and_close(GNOME_DIALOG(dialogWindow));
  va_end(ap);
}

gchar *get_pilot_id_from_gpilotd() {
  gchar **pilots;
  int i;
  
  i=0;
  gpilotd_get_pilots(&pilots);
  if(pilots) {
    while(pilots[i]) { g_message("pilot %d = \"%s\"",i,pilots[i]); i++; }
    if(i==0) {
      run_error_dialog(_("No pilot configured, please choose the\n'Pilot Link Properties' capplet first."));
      return NULL;
    } else
      if(i==1) 
	return pilots[0];
      else {
	g_message("too many pilots...");
	return pilots[0];
      }
  } else {
    run_error_dialog(_("No pilot configured, please choose the\n'Pilot Link Properties' capplet first."));
    return NULL;
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
    gpilotd_init(&argc,argv);
    /* 2 connect to gpilotd */
    if(!gpilotd_connect()) g_error("Cannot connect to gpilotd");
    
    pilotId = get_pilot_id_from_gpilotd();
    if(!pilotId) return -1;
    org_activation_state = activated = gpilotd_conduit_mgmt_is_enabled(conduit,pilotId);
    
    pilot_capplet_setup();


    /* done setting up, now run main loop */
    capplet_gtk_main();
    g_free(pilotId);
    return 0;
}    
