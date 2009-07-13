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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <gtk/gtk.h>
#include <e-util/e-config.h>
#include <addressbook/gui/widgets/eab-config.h>
#include <libedataserver/e-source.h>
#include <glib/gi18n.h>
#include <string.h>

void commit_groupwise_addressbook (EPlugin *epl, EConfigTarget *target);
GtkWidget *e_book_groupwise_dummy (EPlugin *epl, EConfigHookItemFactoryData *data);

void
commit_groupwise_addressbook (EPlugin *epl, EConfigTarget *target)
{
	EABConfigTargetSource *t = (EABConfigTargetSource *) target;
	ESource *source = t->source;
	gchar *uri_text;
	ESourceGroup *source_group;
	gchar *relative_uri;
	GSList *l;

	uri_text = e_source_get_uri (source);
	if (strncmp (uri_text, "groupwise", 9)) {
		g_free (uri_text);

		return;
	}
	e_source_set_property (source, "auth-domain", "Groupwise");
	relative_uri = g_strconcat (";", e_source_peek_name (source), NULL);
	e_source_set_relative_uri (source, relative_uri);
	g_free (relative_uri);

	source_group = e_source_peek_group (source);
	l = e_source_group_peek_sources(source_group);
	if (l && l->data ) {
		e_source_set_property(source, "auth", e_source_get_property(l->data, "auth"));
		e_source_set_property(source, "user", e_source_get_property(l->data, "user"));
		e_source_set_property(source, "use_ssl", e_source_get_property(l->data, "use_ssl"));
		e_source_set_property(source, "port", e_source_get_property(l->data, "port"));
	}
}

GtkWidget *
e_book_groupwise_dummy (EPlugin *epl, EConfigHookItemFactoryData *data)
{

	return NULL;
}
