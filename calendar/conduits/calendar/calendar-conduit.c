/* $Id$ */

#include <glib.h>
#include <gnome.h>

#include <pi-source.h>
#include <pi-socket.h>
#include <pi-file.h>
#include <pi-dlp.h>
#include <pi-version.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <errno.h>

#include <libgnorba/gnorba.h>
#include <libgnorba/gnome-factory.h>

#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-standard-abs.h>

#include "GnomeCal.h"
#include "calobj.h"
#include "calendar.h"

#include "calendar-conduit.h"

typedef struct _ConduitData ConduitData;

struct _ConduitData {
  struct AppointmentAppInfo ai;
  Calendar *cal;
};

#define GET_DATA(c) ((ConduitData*)gtk_object_get_data(GTK_OBJECT(c),"conduit_data"))

GNOME_Calendar_Repository calendar;
CORBA_Environment ev;

static GNOME_Calendar_Repository
calendar_server (void)
{
  if(calendar!=CORBA_OBJECT_NIL) return calendar;
  
  calendar = goad_server_activate_with_id (NULL, "IDL:GNOME:Calendar:Repository:1.0",
					   0, NULL);
  
  if (calendar == CORBA_OBJECT_NIL)
    g_error ("Can not communicate with GnomeCalendar server");
  
  if (ev._major != CORBA_NO_EXCEPTION){
    printf ("Exception: %s\n", CORBA_exception_id (&ev));
    abort ();
  }
  
  return calendar;
}

static gint
load_records(GnomePilotConduit *c)
{
  char *vcalendar_string;
  char *error;
  ConduitData *cd;

  vcalendar_string = 
    GNOME_Calendar_Repository_get_updated_objects (calendar_server(), &ev);

  cd = GET_DATA(c);
  cd->cal = calendar_new("Temporary");

  error = calendar_load_from_memory(cd->cal,vcalendar_string);

  return 0;
}

static gint
pre_sync(GnomePilotConduit *c, GnomePilotDBInfo *dbi) 
{
  int l;
  unsigned char *buf;

  gtk_object_set_data(GTK_OBJECT(c),"dbinfo",dbi);
  
  load_records(c);

  buf = (unsigned char*)g_malloc(0xffff);
  if((l=dlp_ReadAppBlock(dbi->pilot_socket,dbi->db_handle,0,(unsigned char *)buf,0xffff))<0) {
    return -1;
  }
  unpack_AppointmentAppInfo(&(GET_DATA(c)->ai),buf,l);
  g_free(buf);

  return 0;
}

static gint 
post_sync(GnomePilotConduit *c) 
{
  return 0;
}

static gint
match_record	(GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
		 LocalRecord **local,
		 PilotRecord *remote,
		 gpointer data)
{
	g_print ("in match_record\n");
	return 0;
}
static gint
free_match	(GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
		 LocalRecord **local,
		 gpointer data)
{
        g_print ("entering free_match\n");
        *local = NULL;

	return 0;
}
static gint
archive_local (GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
	       LocalRecord *local,
	       gpointer data)
{
	g_print ("entering archive_local\n");
	return 1;

}
static gint
archive_remote (GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
		LocalRecord *local,
		PilotRecord *remote,
		gpointer data)
{
	g_print ("entering archive_remote\n");
	return 1;
}
static gint
store_remote (GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
	      PilotRecord *remote,
	      gpointer data)
{
	g_print ("entering store_remote\n");
        g_print ("Rec:%s:\nLength:%d\n", remote->record, remote->length);
        return 1;
}
static gint
clear_status_archive_local (GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
			    LocalRecord *local,
			    gpointer data)
{
	g_print ("entering clear_status_archive_local\n");
        return 1;
}
static gint
iterate (GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
	 LocalRecord **local,
	 gpointer data)
{
	g_print ("entering iterate\n");
        return 1;
}
static gint
iterate_specific (GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
		  LocalRecord **local,
		  gint flag,
		  gint archived,
		  gpointer data)
{
	g_print ("entering iterate_specific\n");
        return 1;
}
static gint
purge (GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
       gpointer data)
{
	g_print ("entering purge\n");
        return 1;
}
static gint
set_status (GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
	    LocalRecord *local,
	    gint status,
	    gpointer data)
{
	g_print ("entering set_status\n");
        return 1;
}
static gint
set_archived (GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
	      LocalRecord *local,
	      gint archived,
	      gpointer data)
{
	g_print ("entering set_archived\n");
        return 1;
}
static gint
set_pilot_id (GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
	      LocalRecord *local,
	      guint32 ID,
	      gpointer data)
{
	g_print ("entering set_pilot_id\n");
        return 1;
}
static gint
compare (GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
	 LocalRecord *local,
	 PilotRecord *remote,
	 gpointer data)
{
	g_print ("entering compare\n");
        return 1;
}
static gint
compare_backup (GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
		LocalRecord *local,
		PilotRecord *remote,
		gpointer data)
{
	g_print ("entering compare_backup\n");
        return 1;
}
static gint
free_transmit (GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
	       LocalRecord *local,
	       PilotRecord *remote,
	       gpointer data)
{
	g_print ("entering free_transmit\n");
        return 1;
}
static gint
delete_all (GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
	    gpointer data)
{
	g_print ("entering delete_all\n");
        return 1;
}
static PilotRecord *
transmit (GnomePilotConduitStandardAbs *pilot_conduit_standard_abs,
	  LocalRecord *local,
	  gpointer data)
{
	g_print ("entering transmit\n");
	return NULL;
}

static GnomePilotConduit *
conduit_get_gpilot_conduit (guint32 pilotId)
{
	GtkObject *retval;
	ConduitCfg *cfg;
	ConduitData *cdata;

	CORBA_exception_init (&ev);

	g_print ("creating our new conduit\n");
	retval = gnome_pilot_conduit_standard_abs_new ("DatebookDB", 0x64617465);
	g_assert (retval != NULL);
	gnome_pilot_conduit_construct(GNOME_PILOT_CONDUIT(retval),"calendar");

	cfg = g_new0(ConduitCfg,1);
	g_assert(cfg != NULL);
	gtk_object_set_data(retval,"conduit_cfg",cfg);

	cdata = g_new0(ConduitData,1);
	g_assert(cdata != NULL);
	cdata = NULL;
	gtk_object_set_data(retval,"conduit_data",cdata);

	calendar = CORBA_OBJECT_NIL;

	gtk_signal_connect (retval, "match_record", (GtkSignalFunc) match_record, NULL);
	gtk_signal_connect (retval, "free_match", (GtkSignalFunc) free_match, NULL);
	gtk_signal_connect (retval, "archive_local", (GtkSignalFunc) archive_local, NULL);
	gtk_signal_connect (retval, "archive_remote", (GtkSignalFunc) archive_remote, NULL);
	gtk_signal_connect (retval, "store_remote", (GtkSignalFunc) store_remote, NULL);
	gtk_signal_connect (retval, "clear_status_archive_local", (GtkSignalFunc) clear_status_archive_local, NULL);
	gtk_signal_connect (retval, "iterate", (GtkSignalFunc) iterate, NULL);
	gtk_signal_connect (retval, "iterate_specific", (GtkSignalFunc) iterate_specific, NULL);
	gtk_signal_connect (retval, "purge", (GtkSignalFunc) purge, NULL);
	gtk_signal_connect (retval, "set_status", (GtkSignalFunc) set_status, NULL);
	gtk_signal_connect (retval, "set_archived", (GtkSignalFunc) set_archived, NULL);
	gtk_signal_connect (retval, "set_pilot_id", (GtkSignalFunc) set_pilot_id, NULL);
	gtk_signal_connect (retval, "compare", (GtkSignalFunc) compare, NULL);
	gtk_signal_connect (retval, "compare_backup", (GtkSignalFunc) compare_backup, NULL);
	gtk_signal_connect (retval, "free_transmit", (GtkSignalFunc) free_transmit, NULL);
	gtk_signal_connect (retval, "delete_all", (GtkSignalFunc) delete_all, NULL);
	gtk_signal_connect (retval, "transmit", (GtkSignalFunc) transmit, NULL);
	gtk_signal_connect (retval, "pre_sync", (GtkSignalFunc) pre_sync, NULL);
	gtk_signal_connect (retval, "post_sync", (GtkSignalFunc) post_sync, NULL);

	load_configuration(&cfg,pilotId);

	return GNOME_PILOT_CONDUIT (retval);
}

static void
conduit_destroy_gpilot_conduit (GnomePilotConduit *conduit)
{ 
        ConduitCfg *cc;
	ConduitData *cd;

        cc = GET_CONFIG(conduit);
        destroy_configuration(&cc);

	cd = GET_DATA(conduit);
	if(cd->cal!=NULL) calendar_destroy(cd->cal);

	gtk_object_destroy (GTK_OBJECT (conduit));

	GNOME_Calendar_Repository_done (calendar_server(), &ev);

}


