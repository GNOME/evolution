/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* itip-view.h
 *
 * Copyright (C) 2004  Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: JP Rosevear
 */

#ifndef _ITIP_VIEW_H_
#define _ITIP_VIEW_H_

#include <stdarg.h>
#include <unistd.h>
#include <glib-object.h>
#include <gtk/gtkhbox.h>
#include <libedataserver/e-source-list.h>
#include <libecal/e-cal.h>

G_BEGIN_DECLS

#define ITIP_TYPE_VIEW            (itip_view_get_type ())
#define ITIP_VIEW(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), ITIP_TYPE_VIEW, ItipView))
#define ITIP_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), ITIP_TYPE_VIEW, ItipViewClass))
#define ITIP_IS_VIEW(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), ITIP_TYPE_VIEW))
#define ITIP_IS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ITIP_TYPE_VIEW))
#define ITIP_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), ITIP_TYPE_VIEW, ItipViewClass))

typedef struct _ItipView        ItipView;
typedef struct _ItipViewPrivate ItipViewPrivate;
typedef struct _ItipViewClass   ItipViewClass;

typedef enum {
	ITIP_VIEW_MODE_NONE,
	ITIP_VIEW_MODE_PUBLISH,
	ITIP_VIEW_MODE_REQUEST,
	ITIP_VIEW_MODE_COUNTER,
	ITIP_VIEW_MODE_DECLINECOUNTER,
	ITIP_VIEW_MODE_ADD,
	ITIP_VIEW_MODE_REPLY,
	ITIP_VIEW_MODE_REFRESH,
	ITIP_VIEW_MODE_CANCEL
} ItipViewMode;

typedef enum {
	ITIP_VIEW_RESPONSE_NONE,
	ITIP_VIEW_RESPONSE_ACCEPT,
	ITIP_VIEW_RESPONSE_TENTATIVE,
	ITIP_VIEW_RESPONSE_DECLINE,
	ITIP_VIEW_RESPONSE_UPDATE,
	ITIP_VIEW_RESPONSE_CANCEL,
	ITIP_VIEW_RESPONSE_REFRESH,
	ITIP_VIEW_RESPONSE_OPEN
} ItipViewResponse;

typedef enum {
	ITIP_VIEW_INFO_ITEM_TYPE_NONE,
	ITIP_VIEW_INFO_ITEM_TYPE_INFO,
	ITIP_VIEW_INFO_ITEM_TYPE_WARNING,
	ITIP_VIEW_INFO_ITEM_TYPE_ERROR,
	ITIP_VIEW_INFO_ITEM_TYPE_PROGRESS
} ItipViewInfoItemType;

struct _ItipView {
	GtkHBox parent_instance;
	
	ItipViewPrivate *priv;

	GtkWidget *action_vbox;
};

struct _ItipViewClass {
	GtkHBoxClass parent_class;

	void (* source_selected) (ItipView *view, ESource *selected_source);
	void (* response) (ItipView *view, int response);
};

GType      itip_view_get_type (void);
GtkWidget *itip_view_new      (void);

void itip_view_set_mode (ItipView *view, ItipViewMode mode);
ItipViewMode itip_view_get_mode (ItipView *view);

void itip_view_set_item_type (ItipView *view, ECalSourceType type);
ECalSourceType itip_view_get_item_type (ItipView *view);

void itip_view_set_organizer (ItipView *view, const char *organizer);
const char *itip_view_get_organizer (ItipView *view);

void itip_view_set_sentby (ItipView *view, const char *sentby);
const char *itip_view_get_sentby (ItipView *view);

void itip_view_set_attendee (ItipView *view, const char *attendee);
const char *itip_view_get_attendee (ItipView *view);

void itip_view_set_delegator (ItipView *view, const char *delegator);
const char *itip_view_get_delegator (ItipView *view);

void itip_view_set_summary (ItipView *view, const char *summary);
const char *itip_view_get_summary (ItipView *view);

void itip_view_set_location (ItipView *view, const char *location);
const char *itip_view_get_location (ItipView *view);

void itip_view_set_status (ItipView *view, const char *status);
const char *itip_view_get_status (ItipView *view);

void itip_view_set_comment (ItipView *view, const char *comment);
const char *itip_view_get_comment (ItipView *view);

void itip_view_set_description (ItipView *view, const char *description);
const char *itip_view_get_description (ItipView *view);

void itip_view_set_start (ItipView *view, struct tm *start);
const struct tm *itip_view_get_start (ItipView *view);

void itip_view_set_end (ItipView *view, struct tm *end);
const struct tm *itip_view_get_end (ItipView *view);

guint itip_view_add_upper_info_item (ItipView *view, ItipViewInfoItemType type, const char *message); 
guint itip_view_add_upper_info_item_printf (ItipView *view, ItipViewInfoItemType, const char *format, ...) G_GNUC_PRINTF (3, 4);
void itip_view_remove_upper_info_item (ItipView *view, guint id);
void itip_view_clear_upper_info_items (ItipView *view);

guint itip_view_add_lower_info_item (ItipView *view, ItipViewInfoItemType type, const char *message); 
guint itip_view_add_lower_info_item_printf (ItipView *view, ItipViewInfoItemType type, const char *format, ...) G_GNUC_PRINTF (3, 4);
void itip_view_remove_lower_info_item (ItipView *view, guint id);
void itip_view_clear_lower_info_items (ItipView *view);

void itip_view_set_source_list (ItipView *view, ESourceList *source_list);
ESourceList *itip_view_get_source_list (ItipView *view);

void itip_view_set_source (ItipView *view, ESource *source);
ESource *itip_view_get_source (ItipView *view);

void itip_view_set_rsvp (ItipView *view, gboolean rsvp);
gboolean itip_view_get_rsvp (ItipView *view);

void itip_view_set_show_rsvp (ItipView *view, gboolean rsvp);
gboolean itip_view_get_show_rsvp (ItipView *view);

void itip_view_set_update (ItipView *view, gboolean update);
gboolean itip_view_get_update (ItipView *view);

void itip_view_set_show_update (ItipView *view, gboolean update);
gboolean itip_view_get_show_update (ItipView *view);

void itip_view_set_rsvp_comment (ItipView *view, const char *comment);
const char *itip_view_get_rsvp_comment (ItipView *view);

void itip_view_set_buttons_sensitive (ItipView *view, gboolean sensitive);
gboolean itip_view_get_buttons_sensitive (ItipView *view);

G_END_DECLS

#endif
