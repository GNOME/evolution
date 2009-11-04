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

#ifndef __BBDB_H__
#define __BBDB_H__

/* Where to store the config values */
#define GCONF_ROOT_PATH			  "/apps/evolution/autocontacts"
#define GCONF_KEY_ENABLE		  GCONF_ROOT_PATH "/enable_autocontacts"
#define GCONF_KEY_ENABLE_GAIM		  GCONF_ROOT_PATH "/auto_sync_gaim"
#define GCONF_KEY_WHICH_ADDRESSBOOK	  GCONF_ROOT_PATH "/addressbook_source"
#define GCONF_KEY_WHICH_ADDRESSBOOK_GAIM  GCONF_ROOT_PATH "/gaim_addressbook_source"
#define GCONF_KEY_GAIM_LAST_SYNC_TIME	  GCONF_ROOT_PATH "/gaim_last_sync_time"
#define GCONF_KEY_GAIM_LAST_SYNC_MD5	  GCONF_ROOT_PATH "/gaim_last_sync_md5"
#define GCONF_KEY_GAIM_CHECK_INTERVAL	  GCONF_ROOT_PATH "/gaim_check_interval"

/* How often to poll the buddy list for changes (every two minutes is default) */
#define BBDB_BLIST_DEFAULT_CHECK_INTERVAL (2 * 60)

#define GAIM_ADDRESSBOOK 1
#define AUTOMATIC_CONTACTS_ADDRESSBOOK 0

/* bbdb.c */
/* creates an EBook for a given type (gaim or contacts), but doesn't open it;
   this function should be called in a main thread. */
EBook *bbdb_create_ebook (gint type);

/* opens an EBook. Returns false if it fails, and unrefs the book too;
   this function can be called in any thread */
gboolean bbdb_open_ebook (EBook *book);

gboolean bbdb_check_gaim_enabled (void);

/* gaimbuddies.c */
void bbdb_sync_buddy_list (void);
void bbdb_sync_buddy_list_check (void);

#endif /* __BBDB_H__ */
