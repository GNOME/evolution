
#ifndef ITIP_UTILS_HEADER
#define ITIP_UTILS_HEADER

#include <config.h>
#include <ical.h>
#include <string.h>
#include <glib.h>
#include <cal-util/cal-component.h>

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

typedef struct {
	gchar *name;
	gchar *address;
	gchar *full;

	gboolean default_address;
} ItipAddress;

GList *itip_addresses_get (void);
void itip_addresses_free (GList *addresses);

void itip_send_comp (CalComponentItipMethod method, CalComponent *comp);


#endif
