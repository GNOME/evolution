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

#include <string.h>
#include <libecal/libecal.h>
#include <calendar/gui/e-cal-data-model.h>

G_BEGIN_DECLS

typedef enum {
	E_ITIP_SEND_COMPONENT_FLAG_NONE				= 0,
	E_ITIP_SEND_COMPONENT_FLAG_STRIP_ALARMS			= 1 << 0,
	E_ITIP_SEND_COMPONENT_FLAG_ONLY_NEW_ATTENDEES		= 1 << 1,
	E_ITIP_SEND_COMPONENT_FLAG_ENSURE_MASTER_OBJECT		= 1 << 2,
	E_ITIP_SEND_COMPONENT_FLAG_SAVE_RESPONSE_ACCEPTED	= 1 << 3,
	E_ITIP_SEND_COMPONENT_FLAG_SAVE_RESPONSE_DECLINED	= 1 << 4,
	E_ITIP_SEND_COMPONENT_FLAG_SAVE_RESPONSE_TENTATIVE	= 1 << 5,
	E_ITIP_SEND_COMPONENT_FLAG_AS_ATTACHMENT		= 1 << 6
} EItipSendComponentFlags;

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
gchar **	itip_get_user_identities	(ESourceRegistry *registry);
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
gboolean	itip_attendee_is_user		(ESourceRegistry *registry,
						 ECalComponent *comp,
						 ECalClient *cal_client);
gchar *		itip_get_comp_attendee		(ESourceRegistry *registry,
						 ECalComponent *comp,
						 ECalClient *cal_client);
ECalComponentAttendee *
		itip_dup_comp_attendee		(ESourceRegistry *registry,
						 ECalComponent *comp,
						 ECalClient *cal_client,
						 gboolean *out_is_sent_by);
gboolean	itip_send_comp_sync		(ESourceRegistry *registry,
						 ICalPropertyMethod method,
						 ECalComponent *send_comp,
						 ECalClient *cal_client,
						 ICalComponent *zones,
						 GSList *attachments_list,
						 GSList *users,
						 gboolean strip_alarms,
						 gboolean only_new_attendees,
						 GCancellable *cancellable,
						 GError **error);
void		itip_send_component_with_model	(ECalDataModel *model,
						 ICalPropertyMethod method,
						 ECalComponent *send_comp,
						 ECalClient *cal_client,
						 ICalComponent *zones,
						 GSList *attachments_list,
						 GSList *users,
						 EItipSendComponentFlags flags);
void		itip_send_component		(ESourceRegistry *registry,
						 ICalPropertyMethod method,
						 ECalComponent *send_comp,
						 ECalClient *cal_client,
						 ICalComponent *zones,
						 GSList *attachments_list,
						 GSList *users,
						 EItipSendComponentFlags flags,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	itip_send_component_finish	(GAsyncResult *result,
						 GError **error);
gboolean	reply_to_calendar_comp		(ESourceRegistry *registry,
						 ICalPropertyMethod method,
						 ECalComponent *send_comp,
						 ECalClient *cal_client,
						 gboolean reply_all,
						 ICalComponent *zones,
						 GSList *attachments_list);
gboolean	itip_is_component_valid		(ICalComponent *icomp);
gboolean	itip_component_has_recipients	(ECalComponent *comp);
void		itip_utils_update_cdo_replytime	(ICalComponent *icomp);
gboolean	itip_utils_remove_all_but_attendee
						(ICalComponent *icomp,
						 const gchar *attendee);
ICalProperty *	itip_utils_find_attendee_property
						(ICalComponent *icomp,
						 const gchar *address);
void		itip_utils_prepare_attendee_response
						(ESourceRegistry *registry,
						 ICalComponent *icomp,
						 const gchar *address,
						 ICalParameterPartstat partstat);

G_END_DECLS

#endif /* ITIP_UTILS_H */
