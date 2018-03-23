/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __BBDB_H__
#define __BBDB_H__

/* Where to store the config values */
#define CONF_SCHEMA                     "org.gnome.evolution.plugin.autocontacts" 
#define CONF_KEY_ENABLE		        "enable"
#define CONF_KEY_ENABLE_GAIM		"auto-sync-gaim"
#define CONF_KEY_WHICH_ADDRESSBOOK	"addressbook-source"
#define CONF_KEY_WHICH_ADDRESSBOOK_GAIM "gaim-addressbook-source"
#define CONF_KEY_GAIM_LAST_SYNC_TIME	"gaim-last-sync-time"
#define CONF_KEY_GAIM_LAST_SYNC_MD5	"gaim-last-sync-md5"
#define CONF_KEY_GAIM_CHECK_INTERVAL	"gaim-check-interval"
#define CONF_KEY_FILE_UNDER_AS_FIRST_LAST "file-under-as-first-last"

/* How often to poll the buddy list for changes (every two minutes is default) */
#define BBDB_BLIST_DEFAULT_CHECK_INTERVAL (2 * 60)

#define GAIM_ADDRESSBOOK 1
#define AUTOMATIC_CONTACTS_ADDRESSBOOK 0

#include <libebook/libebook.h>

/* bbdb.c */
EBookClient *	bbdb_create_book_client		(gint type,
						 GCancellable *cancellable,
						 GError **error);
gboolean	bbdb_check_gaim_enabled		(void);

/* gaimbuddies.c */
void		bbdb_sync_buddy_list		(void);
void		bbdb_sync_buddy_list_check	(void);

#endif /* __BBDB_H__ */
