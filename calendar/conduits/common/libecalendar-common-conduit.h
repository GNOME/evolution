/*
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <libedataserver/e-categories.h>
#include <libecal/e-cal.h>
#include <pi-appinfo.h>

#define PILOT_MAX_CATEGORIES 16

gint e_pilot_add_category_if_possible(gchar *cat_to_add, struct CategoryAppInfo *category);
void e_pilot_local_category_to_remote(gint * pilotCategory, ECalComponent *comp, struct CategoryAppInfo *category);
void e_pilot_remote_category_to_local(gint   pilotCategory, ECalComponent *comp, struct CategoryAppInfo *category);

gboolean e_pilot_setup_get_bool (const gchar *path, const gchar *key, gboolean def);
void e_pilot_setup_set_bool (const gchar *path, const gchar *key, gboolean value);
gint e_pilot_setup_get_int (const gchar *path, const gchar *key, gint def);
void e_pilot_setup_set_int (const gchar *path, const gchar *key, gint value);
gchar *e_pilot_setup_get_string (const gchar *path, const gchar *key, const gchar *def);
void e_pilot_setup_set_string (const gchar *path, const gchar *key, const gchar *value);
