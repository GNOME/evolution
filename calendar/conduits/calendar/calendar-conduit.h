/* $Id$ */

#ifndef __CALENDAR_CONDUIT_H__
#define __CALENDAR_CONDUIT_H__

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <pi-datebook.h>
#include <gnome.h>

#define CALLOCALRECORD(s) ((CalLocalRecord*)(s))
typedef struct _CalLocalRecord CalLocalRecord;

struct _CalLocalRecord {
	/* The stuff from gnome-pilot-conduit-standard-abs.h */
	LocalRecord local;
	/* The corresponding iCal object */
	iCalObject *ical;
	/* used by iterations, points to a GList element */
	GList *list_ptr;
        /* pilot-link appointment structure, used for implementing Transmit */	
	struct Appointment *a;
};

typedef struct _ConduitCfg ConduitCfg;

struct _ConduitCfg {
  gboolean open_secret;
  guint32 pilotId;
};

#define GET_CONFIG(c) ((ConduitCfg*)gtk_object_get_data(GTK_OBJECT(c),"conduit_cfg"))

static void load_configuration(ConduitCfg **c,guint32 pilotId) {
  gchar prefix[256];
  g_snprintf(prefix,255,"/gnome-pilot.d/calendard-conduit/Pilot_%u/",pilotId);

  *c = g_new0(ConduitCfg,1);
  gnome_config_push_prefix(prefix);
  (*c)->open_secret = gnome_config_get_bool("open secret=FALSE");
  gnome_config_pop_prefix();

  (*c)->pilotId = pilotId;
}

static void save_configuration(ConduitCfg *c) {
  gchar prefix[256];

  g_snprintf(prefix,255,"/gnome-pilot.d/calendar-conduit/Pilot_%u/",c->pilotId);

  gnome_config_push_prefix(prefix);
  gnome_config_set_bool("open secret",c->open_secret);
  gnome_config_pop_prefix();

  gnome_config_sync();
  gnome_config_drop_all();
}

static void destroy_configuration(ConduitCfg **c) {
  g_free(*c);
  *c = NULL;
}

#endif __CALENDAR_CONDUIT_H__ 
