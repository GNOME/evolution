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

#include <glib-object.h>
#include <gtk/gtkhbox.h>

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
	ITIP_VIEW_RESPONSE_SEND
} ItipViewResponse;

struct _ItipView 
{
	GtkHBox parent_instance;

	ItipViewPrivate *priv;
};

struct _ItipViewClass 
{
	GtkHBoxClass parent_class;
};

GType      itip_view_get_type (void);
GtkWidget *itip_view_new      (void);

void itip_view_set_mode (ItipView *view, ItipViewMode mode);
ItipViewMode itip_view_get_mode (ItipView *view);

void itip_view_set_organizer (ItipView *view, const char *organizer);
const char *itip_view_get_organizer (ItipView *view);

void itip_view_set_sentby (ItipView *view, const char *sentby);
const char *itip_view_get_sentby (ItipView *view);

void itip_view_set_attendee (ItipView *view, const char *attendee);
const char *itip_view_get_attendee (ItipView *view);

void itip_view_set_summary (ItipView *view, const char *summary);
const char *itip_view_get_summary (ItipView *view);

void itip_view_set_location (ItipView *view, const char *location);
const char *itip_view_get_location (ItipView *view);

void itip_view_set_start (ItipView *view, struct tm *start);
const struct tm *itip_view_get_start (ItipView *view);

void itip_view_set_end (ItipView *view, struct tm *end);
const struct tm *itip_view_get_end (ItipView *view);

G_END_DECLS

#endif
