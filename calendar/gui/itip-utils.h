
#ifndef ITIP_UTILS_HEADER
#define ITIP_UTILS_HEADER

#include <libical/ical.h>
#include <string.h>
#include <glib.h>
#include <cal-client/cal-client.h>
#include <cal-util/cal-component.h>
#include <e-util/e-account-list.h>

typedef enum {
	CAL_COMPONENT_METHOD_PUBLISH,
	CAL_COMPONENT_METHOD_REQUEST,
	CAL_COMPONENT_METHOD_REPLY,
	CAL_COMPONENT_METHOD_ADD,
	CAL_COMPONENT_METHOD_CANCEL,
	CAL_COMPONENT_METHOD_REFRESH,
	CAL_COMPONENT_METHOD_COUNTER,
	CAL_COMPONENT_METHOD_DECLINECOUNTER
} CalComponentItipMethod;

EAccountList *itip_addresses_get (void);
EAccount *itip_addresses_get_default (void);

gboolean itip_organizer_is_user (CalComponent *comp, CalClient *client);
gboolean itip_sentby_is_user (CalComponent *comp);

const gchar *itip_strip_mailto (const gchar *address);

gboolean itip_send_comp (CalComponentItipMethod method, CalComponent *comp,
			 CalClient *client, icalcomponent *zones);


#endif
