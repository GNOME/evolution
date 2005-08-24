/*
 *
 *
 * Copyright (C) 2004 David Trowbridge
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

#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtktable.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkhbox.h>
#include <e-util/e-config.h>
#include <calendar/gui/e-cal-config.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-url.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

GtkWidget *e_calendar_file_dummy (EPlugin *epl, EConfigHookItemFactoryData *data);

GtkWidget *
e_calendar_file_dummy (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESource *source = t->source;
	char *uri_text;
	const char *relative_uri;

        uri_text = e_source_get_uri (source);
	if (strncmp (uri_text, "file", 4)) {
		g_free (uri_text);
		
		return NULL;
	}

	relative_uri = e_source_peek_relative_uri (source);
	if (relative_uri && *relative_uri) {
	  	g_free (uri_text);
		
		return NULL;
	}

	e_source_set_relative_uri (source, e_source_peek_uid (source));
        uri_text = e_source_get_uri (source);

	return NULL;
}
