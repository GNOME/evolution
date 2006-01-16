
#ifndef ITIP_UTILS_HEADER
#define ITIP_UTILS_HEADER

#include <libical/ical.h>
#include <string.h>
#include <glib.h>
#include <libecal/e-cal.h>
#include <libecal/e-cal-component.h>
#include <libedataserver/e-account-list.h>

typedef enum {
	E_CAL_COMPONENT_METHOD_PUBLISH,
	E_CAL_COMPONENT_METHOD_REQUEST,
	E_CAL_COMPONENT_METHOD_REPLY,
	E_CAL_COMPONENT_METHOD_ADD,
	E_CAL_COMPONENT_METHOD_CANCEL,
	E_CAL_COMPONENT_METHOD_REFRESH,
	E_CAL_COMPONENT_METHOD_COUNTER,
	E_CAL_COMPONENT_METHOD_DECLINECOUNTER
} ECalComponentItipMethod;

struct CalMimeAttach {
	char *filename;
	char *content_type;
	char *description;
	char *encoded_data;
	gboolean disposition;
	guint length;
};

EAccountList *itip_addresses_get (void);
EAccount *itip_addresses_get_default (void);

gboolean itip_organizer_is_user (ECalComponent *comp, ECal *client);
gboolean itip_sentby_is_user (ECalComponent *comp);

const gchar *itip_strip_mailto (const gchar *address);

char *itip_get_comp_attendee (ECalComponent *comp, ECal *client);

gboolean itip_send_comp (ECalComponentItipMethod method, ECalComponent *comp,
			 ECal *client, icalcomponent *zones, GSList *attachments_list);

gboolean itip_publish_comp (ECal *client, gchar* uri, gchar* username, 
			    gchar* password, ECalComponent **pub_comp);

gboolean itip_publish_begin (ECalComponent *pub_comp, ECal *client, 
			     gboolean cloned, ECalComponent **clone);

gboolean reply_to_calendar_comp (ECalComponentItipMethod method, ECalComponent *send_comp,
				ECal *client, gboolean reply_all, icalcomponent *zones, GSList *attachments_list);

#endif
