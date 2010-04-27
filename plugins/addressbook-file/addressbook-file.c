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
 *
 * Authors:
 *		Sivaiah Nallagatla <snallagatla@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <e-util/e-config.h>
#include <addressbook/gui/widgets/eab-config.h>
#include <libedataserver/e-source.h>
#include <string.h>

gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep, gint enable)
{
	return 0;
}

GtkWidget *e_book_file_dummy (EPlugin *epl, EConfigHookItemFactoryData *data);

GtkWidget *
e_book_file_dummy (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EABConfigTargetSource *t = (EABConfigTargetSource *) data->target;
	ESource *source = t->source;
	gchar *uri_text;
	const gchar *relative_uri;

        uri_text = e_source_get_uri (source);
	if (strncmp (uri_text, "file", 4)) {
		g_free (uri_text);

		return NULL;
	}

	relative_uri = e_source_peek_relative_uri (source);
	g_free (uri_text);

	if (relative_uri && *relative_uri) {
		return NULL;
	}

	e_source_set_relative_uri (source, e_source_peek_uid (source));

	return NULL;
}
