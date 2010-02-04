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
 * Copyright (C) 1999-2010 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_PLUGIN_UTIL_H
#define _E_PLUGIN_UTIL_H

#include <glib.h>
#include <gtk/gtk.h>

#include <libsoup/soup.h>

#include <libedataserver/e-source.h>
#include <libedataserver/e-source.h>

gboolean e_plugin_util_is_source_proto (ESource *source, const gchar *protocol);
gboolean e_plugin_util_is_group_proto (ESourceGroup *group, const gchar *protocol);

gchar *e_plugin_util_replace_at_sign (const gchar *str);
gchar *e_plugin_util_uri_no_proto (SoupURI *uri);

/* common widgets used in plugin setup */
GtkWidget *e_plugin_util_add_entry (GtkWidget *parent, const gchar *label, ESource *source, const gchar *source_property);
GtkWidget *e_plugin_util_add_check (GtkWidget *parent, const gchar *label, ESource *source, const gchar *source_property, const gchar *true_value, const gchar *false_value);

/* multipack widgets */
GtkWidget *e_plugin_util_add_refresh (GtkWidget *parent, const gchar *label, ESource *source, const gchar *source_property);

#endif /* _E_PLUGIN_UTIL_H */
