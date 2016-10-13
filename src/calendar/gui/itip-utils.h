/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef ITIP_UTILS_H
#define ITIP_UTILS_H

#include <libical/ical.h>
#include <string.h>
#include <libecal/libecal.h>
#include <calendar/gui/e-cal-model.h>

G_BEGIN_DECLS

typedef enum {
	E_CAL_COMPONENT_METHOD_NONE = -1,
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
	gchar *filename;
	gchar *content_type;
	gchar *content_id;
	gchar *description;
	gchar *encoded_data;
	gboolean disposition;
	guint length;
};

void		itip_cal_mime_attach_free	(gpointer ptr); /* struct CalMimeAttach * */

gboolean	itip_get_default_name_and_address
						(ESourceRegistry *registry,
						 gchar **name,
						 gchar **address);
gchar **	itip_get_user_identities	(ESourceRegistry *registry);
gchar *		itip_get_fallback_identity	(ESourceRegistry *registry);
gboolean	itip_address_is_user		(ESourceRegistry *registry,
						 const gchar *address);
gboolean	itip_organizer_is_user		(ESourceRegistry *registry,
						 ECalComponent *comp,
						 ECalClient *cal_client);
gboolean	itip_organizer_is_user_ex	(ESourceRegistry *registry,
						 ECalComponent *comp,
						 ECalClient *cal_client,
						 gboolean skip_cap_test);
gboolean	itip_sentby_is_user		(ESourceRegistry *registry,
						 ECalComponent *comp,
						 ECalClient *cal_client);
gboolean	itip_has_any_attendees		(ECalComponent *comp);
const gchar *	itip_strip_mailto		(const gchar *address);
gchar *		itip_get_comp_attendee		(ESourceRegistry *registry,
						 ECalComponent *comp,
						 ECalClient *cal_client);
gboolean	itip_send_comp_sync		(ESourceRegistry *registry,
						 ECalComponentItipMethod method,
						 ECalComponent *send_comp,
						 ECalClient *cal_client,
						 icalcomponent *zones,
						 GSList *attachments_list,
						 GSList *users,
						 gboolean strip_alarms,
						 gboolean only_new_attendees,
						 GCancellable *cancellable,
						 GError **error);
void		itip_send_component_with_model	(ECalModel *model,
						 ECalComponentItipMethod method,
						 ECalComponent *send_comp,
						 ECalClient *cal_client,
						 icalcomponent *zones,
						 GSList *attachments_list,
						 GSList *users,
						 gboolean strip_alarms,
						 gboolean only_new_attendees,
						 gboolean ensure_master_object);
void		itip_send_component		(ESourceRegistry *registry,
						 ECalComponentItipMethod method,
						 ECalComponent *send_comp,
						 ECalClient *cal_client,
						 icalcomponent *zones,
						 GSList *attachments_list,
						 GSList *users,
						 gboolean strip_alarms,
						 gboolean only_new_attendees,
						 gboolean ensure_master_object,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	itip_send_component_finish	(GAsyncResult *result,
						 GError **error);
gboolean	itip_publish_begin		(ECalComponent *pub_comp,
						 ECalClient *cal_client,
						 gboolean cloned,
						 ECalComponent **clone);
gboolean	reply_to_calendar_comp		(ESourceRegistry *registry,
						 ECalComponentItipMethod method,
						 ECalComponent *send_comp,
						 ECalClient *cal_client,
						 gboolean reply_all,
						 icalcomponent *zones,
						 GSList *attachments_list);
gboolean	is_icalcomp_valid		(icalcomponent *icalcomp);
gboolean	itip_component_has_recipients	(ECalComponent *comp);

G_END_DECLS

#endif /* ITIP_UTILS_H */
