/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef E_MAIL_PART_ITIP_H
#define E_MAIL_PART_ITIP_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libecal/libecal.h>
#include <libebackend/libebackend.h>

#include <em-format/e-mail-part.h>

#include "itip-view.h"

#define E_MAIL_PART_ITIP(p) ((EMailPartItip *) p)

G_BEGIN_DECLS

typedef struct _EMailPartItip EMailPartItip;

struct _EMailPartItip {
	EMailPart parent;

	CamelFolder *folder;
	CamelMimeMessage *msg;
	CamelMimePart *part;

	gchar *uid;

	EClientCache *client_cache;

	ECalClient *current_client;
	ECalClientSourceType type;

	/* cancelled when freeing the puri */
	GCancellable *cancellable;

	gchar *vcalendar;
	ECalComponent *comp;
	icalcomponent *main_comp;
	icalcomponent *ical_comp;
	icalcomponent *top_level;
	icalcompiter iter;
	icalproperty_method method;
	time_t start_time;
	time_t end_time;

	gint current;
	gint total;

	gchar *calendar_uid;

	gchar *from_address;
	gchar *from_name;
	gchar *to_address;
	gchar *to_name;
	gchar *delegator_address;
	gchar *delegator_name;
	gchar *my_address;
	gint   view_only;

	guint progress_info_id;

	gboolean delete_message;
	/* a reply can only be sent if and only if there is an organizer */
	gboolean has_organizer;
	/*
	 * Usually replies are sent unless the user unchecks that option.
	 * There are some cases when the default is not to sent a reply
	 * (but the user can still chose to do so by checking the option):
	 * - the organizer explicitly set RSVP=FALSE for the current user
	 * - the event has no ATTENDEEs: that's the case for most non-meeting
	 *   events
	 *
	 * The last case is meant for forwarded non-meeting
	 * events. Traditionally Evolution hasn't offered to send a
	 * reply, therefore the updated implementation mimics that
	 * behavior.
	 *
	 * Unfortunately some software apparently strips all ATTENDEEs
	 * when forwarding a meeting; in that case sending a reply is
	 * also unchecked by default. So the check for ATTENDEEs is a
	 * tradeoff between sending unwanted replies in cases where
	 * that wasn't done in the past and not sending a possibly
	 * wanted reply where that wasn't possible in the past
	 * (because replies to forwarded events were not
	 * supported). Overall that should be an improvement, and the
	 * user can always override the default.
	 */
	gboolean no_reply_wanted;

	guint update_item_progress_info_id;
	guint update_item_error_info_id;
	ItipViewResponse update_item_response;
	GHashTable *real_comps; /* ESource's UID -> ECalComponent stored on the server */

	ItipView *view;
};

G_END_DECLS

#endif /* E_MAIL_PART_ITIP_H */
