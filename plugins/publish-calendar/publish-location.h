/*
 * Authors:
 *  David Trowbridge <trowbrds@cs.colorado.edu>
 *  Gary Ekker <gekker@novell.com>
 *
 * Copyright (C) 2005 Novell, Inc (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef PUBLISH_LOCATION_H
#define PUBLISH_LOCATION_H

#include <glib.h>

G_BEGIN_DECLS

enum publish_frequency {
	URI_PUBLISH_DAILY,
	URI_PUBLISH_WEEKLY,
	URI_PUBLISH_MANUAL,
	/* URI_PUBLISH_AUTO, */
};

static const int publish_frequency_type_map[] = {
	URI_PUBLISH_DAILY,
	URI_PUBLISH_WEEKLY,
	URI_PUBLISH_MANUAL,
	/* URI_PUBLISH_AUTO, */
	-1,
};

enum publish_format {
	URI_PUBLISH_AS_ICAL,
	URI_PUBLISH_AS_FB,
	/* URI_PUBLISH_AS_HTML, */
};

static const int publish_format_type_mask[] = {
	URI_PUBLISH_AS_ICAL,
	URI_PUBLISH_AS_FB,
	/* URI_PUBLISH_AS_HTML, */
	-1,
};

typedef struct _EPublishUri EPublishUri;
struct _EPublishUri {
	gboolean enabled;
	gchar *location;
	gint publish_frequency;
	gint publish_format;
	gchar *password;
	GSList *events;
	gchar *last_pub_time;

	gint service_type;
};

EPublishUri *e_publish_uri_from_xml (const gchar *xml);
gchar       *e_publish_uri_to_xml (EPublishUri *uri);

G_END_DECLS

#endif
