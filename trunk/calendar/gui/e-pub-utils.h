/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Evolution calendar Free/Busy utilities and types
 *
 * Copyright (C) 2004 Ximian, Inc.
 *
 * Author: Gary Ekker <gekker@novell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef E_CAL_FB_UTIL_H
#define E_CAL_FB_UTIL_H

#include <glib.h>
#include <libxml/tree.h>

G_BEGIN_DECLS

enum publish_frequency{
	URI_PUBLISH_DAILY,
	URI_PUBLISH_WEEKLY,
	URI_PUBLISH_USER
};

static const int pub_frequency_type_map[] = {
	URI_PUBLISH_DAILY,
	URI_PUBLISH_WEEKLY,
	URI_PUBLISH_USER,
	-1
};

struct _EPublishUri {
	gint enabled;
	gchar *location;
	gint publish_freq;
	gchar *username;
	gchar *password;
	GSList *calendars;
	gchar *last_pub_time;
};

typedef struct _EPublishUri EPublishUri;

void e_pub_uri_from_xml (EPublishUri *uri, const gchar *xml);
gchar  *e_pub_uri_to_xml (EPublishUri *uri);
void e_pub_publish (gboolean publish) ;

G_END_DECLS

#endif
