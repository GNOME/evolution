/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Authors :
 *  Damon Chaplin <damon@ximian.com>
 *  Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2000, Ximian, Inc.
 * Copyright 2000, Ximian, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "e-select-names-config-keys.h"
#include "e-select-names-config.h"

static GConfClient *config = NULL;

static void
do_cleanup (void)
{
	g_object_unref (config);
	config = NULL;
}

static void
e_select_names_config_init (void)
{
	if (config)
		return;

	config = gconf_client_get_default ();
	g_atexit ((GVoidFunc) do_cleanup);

	gconf_client_add_dir (config, SELECT_NAMES_CONFIG_PREFIX, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
}

void
e_select_names_config_remove_notification (guint id)
{
	gconf_client_notify_remove (config, id);
}

/* The current list of completion books */
GSList *
e_select_names_config_get_completion_books (void)
{
	e_select_names_config_init ();
	
	return gconf_client_get_list (config, SELECT_NAMES_CONFIG_COMPLETION_BOOKS, GCONF_VALUE_STRING, NULL);
}

void
e_select_names_config_set_completion_books (GSList *selected)
{
	e_select_names_config_init ();

	gconf_client_set_list (config, SELECT_NAMES_CONFIG_COMPLETION_BOOKS, GCONF_VALUE_STRING, selected, NULL);
}

guint
e_select_names_config_add_notification_completion_books (GConfClientNotifyFunc func, gpointer data)
{
	guint id;

	e_select_names_config_init ();
	
	id = gconf_client_notify_add (config, SELECT_NAMES_CONFIG_COMPLETION_BOOKS, func, data, NULL, NULL);
	
	return id;
}

char *
e_select_names_config_get_last_completion_book (void)
{
	e_select_names_config_init ();
	
	return gconf_client_get_string (config, SELECT_NAMES_CONFIG_LAST_COMPLETION_BOOK, NULL);
}

void
e_select_names_config_set_last_completion_book (const char *last_completion_book)
{
	e_select_names_config_init ();

	gconf_client_set_string (config, SELECT_NAMES_CONFIG_LAST_COMPLETION_BOOK, last_completion_book, NULL);
}

guint
e_select_names_config_add_notification_last_completion_book (GConfClientNotifyFunc func, gpointer data)
{
	guint id;

	e_select_names_config_init ();
	
	id = gconf_client_notify_add (config, SELECT_NAMES_CONFIG_LAST_COMPLETION_BOOK, func, data, NULL, NULL);
	
	return id;
}

gint
e_select_names_config_get_min_query_length (void)
{
	e_select_names_config_init ();

	return gconf_client_get_int (config, SELECT_NAMES_CONFIG_MIN_QUERY_LENGTH, NULL);
}


void
e_select_names_config_set_min_query_length (gint day_end_hour)
{
	e_select_names_config_init ();

	gconf_client_set_int (config, SELECT_NAMES_CONFIG_MIN_QUERY_LENGTH, day_end_hour, NULL);
}

guint 
e_select_names_config_add_notification_min_query_length (GConfClientNotifyFunc func, gpointer data)
{
	guint id;

	e_select_names_config_init ();
	
	id = gconf_client_notify_add (config, SELECT_NAMES_CONFIG_MIN_QUERY_LENGTH, func, data, NULL, NULL);
	
	return id;	
}
