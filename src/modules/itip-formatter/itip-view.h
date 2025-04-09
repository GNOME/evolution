/*
 *
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
 * Authors:
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef ITIP_VIEW_H
#define ITIP_VIEW_H

#include <stdarg.h>
#include <unistd.h>

#include <gtk/gtk.h>

#include <libecal/libecal.h>

#include <e-util/e-util.h>
#include <em-format/e-mail-formatter.h>

/* Standard GObject macros */
#define ITIP_TYPE_VIEW \
	(itip_view_get_type ())
#define ITIP_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), ITIP_TYPE_VIEW, ItipView))
#define ITIP_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), ITIP_TYPE_VIEW, ItipViewClass))
#define ITIP_IS_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), ITIP_TYPE_VIEW))
#define ITIP_IS_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), ITIP_TYPE_VIEW))
#define ITIP_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), ITIP_TYPE_VIEW, ItipViewClass))

G_BEGIN_DECLS

typedef struct _ItipView ItipView;
typedef struct _ItipViewClass ItipViewClass;
typedef struct _ItipViewPrivate ItipViewPrivate;

typedef enum {
	ITIP_VIEW_MODE_NONE,
	ITIP_VIEW_MODE_PUBLISH,
	ITIP_VIEW_MODE_REQUEST,
	ITIP_VIEW_MODE_COUNTER,
	ITIP_VIEW_MODE_DECLINECOUNTER,
	ITIP_VIEW_MODE_ADD,
	ITIP_VIEW_MODE_REPLY,
	ITIP_VIEW_MODE_REFRESH,
	ITIP_VIEW_MODE_CANCEL,
	ITIP_VIEW_MODE_HIDE_ALL
} ItipViewMode;

typedef enum {
	ITIP_VIEW_RESPONSE_NONE,
	ITIP_VIEW_RESPONSE_ACCEPT,
	ITIP_VIEW_RESPONSE_TENTATIVE,
	ITIP_VIEW_RESPONSE_DECLINE,
	ITIP_VIEW_RESPONSE_UPDATE,
	ITIP_VIEW_RESPONSE_CANCEL,
	ITIP_VIEW_RESPONSE_REFRESH,
	ITIP_VIEW_RESPONSE_OPEN,
	ITIP_VIEW_RESPONSE_SAVE,
	ITIP_VIEW_RESPONSE_IMPORT,
	ITIP_VIEW_RESPONSE_IMPORT_BARE
} ItipViewResponse;

typedef enum {
	ITIP_VIEW_INFO_ITEM_TYPE_NONE,
	ITIP_VIEW_INFO_ITEM_TYPE_INFO,
	ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
	ITIP_VIEW_INFO_ITEM_TYPE_ERROR,
	ITIP_VIEW_INFO_ITEM_TYPE_PROGRESS
} ItipViewInfoItemType;

struct _ItipView {
	GObject parent;
	ItipViewPrivate *priv;
};

struct _ItipViewClass {
	GObjectClass parent_class;

	void		(*source_selected)	(ItipView *view,
						 ESource *selected_source);
	void		(*response)		(ItipView *view,
						 gint response);
};

GType		itip_view_get_type		(void);
ItipView *	itip_view_new			(const gchar *part_id,
						 gpointer itip_part_ptr,
						 CamelFolder *folder,
						 const gchar *message_uid,
						 CamelMimeMessage *message,
						 CamelMimePart *itip_mime_part,
						 const gchar *vcalendar,
						 GCancellable *cancellable);
void		itip_view_init_view		(ItipView *view);
void		itip_view_set_web_view		(ItipView *view,
						 EWebView *web_view);
EWebView *	itip_view_ref_web_view		(ItipView *view);
void		itip_view_write			(gpointer itip_part,
						 EMailFormatter *formatter,
						 GString *buffer,
						 gboolean show_day_agenda);
void		itip_view_write_for_printing	(ItipView *view,
						 GString *buffer);
EClientCache *	itip_view_get_client_cache	(ItipView *view);
const gchar *	itip_view_get_extension_name	(ItipView *view);
void		itip_view_set_extension_name	(ItipView *view,
						 const gchar *extension_name);
ItipViewMode	itip_view_get_mode		(ItipView *view);
void		itip_view_set_mode		(ItipView *view,
						 ItipViewMode mode);
ECalClientSourceType
		itip_view_get_item_type		(ItipView *view);
void		itip_view_set_item_type		(ItipView *view,
						 ECalClientSourceType type);
const gchar *	itip_view_get_organizer		(ItipView *view);
void		itip_view_set_organizer		(ItipView *view,
						 const gchar *organizer);
const gchar *	itip_view_get_organizer_sentby	(ItipView *view);
void		itip_view_set_organizer_sentby	(ItipView *view,
						 const gchar *sentby);
const gchar *	itip_view_get_attendee		(ItipView *view);
void		itip_view_set_attendee		(ItipView *view,
						 const gchar *attendee);
const gchar *	itip_view_get_attendee_sentby	(ItipView *view);
void		itip_view_set_attendee_sentby	(ItipView *view,
						 const gchar *sentby);
const gchar *	itip_view_get_delegator		(ItipView *view);
void		itip_view_set_delegator		(ItipView *view,
						 const gchar *delegator);
const gchar *	itip_view_get_proxy		(ItipView *view);
void		itip_view_set_proxy		(ItipView *view,
						 const gchar *proxy);
const gchar *	itip_view_get_summary		(ItipView *view);
void		itip_view_set_summary		(ItipView *view,
						 const gchar *summary);
const gchar *	itip_view_get_location		(ItipView *view);
void		itip_view_set_location		(ItipView *view,
						 const gchar *location);
void		itip_view_set_geo		(ItipView *view,
						 const gchar *geo);
const gchar *	itip_view_get_url		(ItipView *view);
void		itip_view_set_url		(ItipView *view,
						 const gchar *url);
const gchar *	itip_view_get_status		(ItipView *view);
void		itip_view_set_status		(ItipView *view,
						 const gchar *status);
const gchar *	itip_view_get_comment		(ItipView *view);
void		itip_view_set_comment		(ItipView *view,
						 const gchar *comment);
const gchar *	itip_view_get_attendees		(ItipView *view);
void		itip_view_set_attendees		(ItipView *view,
						 const gchar *attendees);
const gchar *	itip_view_get_description	(ItipView *view);
void		itip_view_set_description	(ItipView *view,
						 const gchar *description);
const struct tm *
		itip_view_get_start		(ItipView *view,
						 gboolean *is_date);
void		itip_view_set_start		(ItipView *view,
						 struct tm *start,
						 gboolean is_date);
const struct tm *
		itip_view_get_end		(ItipView *view,
						 gboolean *is_date);
void		itip_view_set_end		(ItipView *view,
						 struct tm *end,
						 gboolean is_date);
guint		itip_view_add_upper_info_item	(ItipView *view,
						 ItipViewInfoItemType type,
						 const gchar *message);
guint		itip_view_add_upper_info_item_printf
						(ItipView *view,
						 ItipViewInfoItemType,
						 const gchar *format,
						 ...) G_GNUC_PRINTF (3, 4);
void		itip_view_remove_upper_info_item
						(ItipView *view,
						 guint id);
void		itip_view_clear_upper_info_items
						(ItipView *view);
guint		itip_view_add_lower_info_item	(ItipView *view,
						 ItipViewInfoItemType type,
						 const gchar *message);
guint		itip_view_add_lower_info_item_printf
						(ItipView *view,
						 ItipViewInfoItemType type,
						 const gchar *format,
						 ...) G_GNUC_PRINTF (3, 4);
void		itip_view_remove_lower_info_item
						(ItipView *view,
						 guint id);
void		itip_view_clear_lower_info_items
						(ItipView *view);
ESource *	itip_view_ref_source		(ItipView *view);
void		itip_view_set_source		(ItipView *view,
						 ESource *source);
gboolean	itip_view_get_rsvp		(ItipView *view);
void		itip_view_set_rsvp		(ItipView *view,
						 gboolean rsvp);
void		itip_view_set_show_rsvp_check	(ItipView *view,
						 gboolean show);
gboolean	itip_view_get_update		(ItipView *view);
void		itip_view_set_update		(ItipView *view,
						 gboolean update);
void		itip_view_set_show_update_check (ItipView *view,
						 gboolean show);
const gchar *	itip_view_get_rsvp_comment	(ItipView *view);
void		itip_view_set_rsvp_comment	(ItipView *view,
						 const gchar *comment);
gboolean	itip_view_get_buttons_sensitive	(ItipView *view);
void		itip_view_set_buttons_sensitive	(ItipView *view,
						 gboolean sensitive);
gboolean	itip_view_get_recur_check_state	(ItipView *view);
void		itip_view_set_show_recur_check	(ItipView *view,
						 gboolean show);
void		itip_view_set_needs_decline	(ItipView *view,
						 gboolean needs_decline);
gboolean	itip_view_get_free_time_check_state
						(ItipView *view);
void		itip_view_set_show_free_time_check
						(ItipView *view,
						 gboolean show);
gboolean	itip_view_get_keep_alarm_check_state
						(ItipView *view);
void		itip_view_set_show_keep_alarm_check
						(ItipView *view,
						 gboolean show);
gboolean	itip_view_get_inherit_alarm_check_state
						(ItipView *view);
void		itip_view_set_show_inherit_alarm_check
						(ItipView *view,
						 gboolean show);
void		itip_view_set_error		(ItipView *view,
						 const gchar *error_html,
						 gboolean show_save_btn);

gchar *		itip_view_util_extract_part_content
						(CamelMimePart *part,
						 gboolean convert_charset);

G_END_DECLS

#endif /* ITIP_VIEW_H */

