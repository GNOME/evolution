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
#define GCONF_KEY_ENABLE "/apps/evolution/autocontacts/enable_autocontacts"
#define GCONF_KEY_ENABLE_GAIM "/apps/evolution/autocontacts/auto_sync_gaim"
#define GCONF_KEY_WHICH_ADDRESSBOOK "/apps/evolution/autocontacts/addressbook_source"
#define GCONF_KEY_WHICH_ADDRESSBOOK_GAIM "/apps/evolution/autocontacts/gaim_addressbook_source"
#define GCONF_KEY_GAIM_LAST_SYNC "/apps/evolution/autocontacts/gaim_last_sync_time"

#define GAIM_ADDRESSBOOK 1
#define AUTOMATIC_CONTACTS_ADDRESSBOOK 0

/* How often to poll the buddy list for changes (every two minutes) */
#define BBDB_BLIST_CHECK_INTERVAL (2 * 60)

/* bbdb.c */
EBook *bbdb_open_addressbook (gint type);
gboolean bbdb_check_gaim_enabled (void);

/* gaimbuddies.c */
void bbdb_sync_buddy_list (void);
void bbdb_sync_buddy_list_check (void);

#endif /* __BBDB_H__ */
