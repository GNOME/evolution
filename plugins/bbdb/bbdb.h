#ifndef __BBDB_H__
#define __BBDB_H__

/* Where to store the config values */
#define GCONF_KEY_ENABLE "/apps/evolution/autocontacts/enable_autocontacts"
#define GCONF_KEY_ENABLE_GAIM "/apps/evolution/autocontacts/auto_sync_gaim"
#define GCONF_KEY_WHICH_ADDRESSBOOK "/apps/evolution/autocontacts/addressbook_source"
#define GCONF_KEY_GAIM_LAST_SYNC "/apps/evolution/autocontacts/gaim_last_sync_time"

/* How often to poll the buddy list for changes (every two minutes) */
#define BBDB_BLIST_CHECK_INTERVAL (2 * 60 * 1000) 

/* bbdb.c */
EBook *bbdb_open_addressbook (void);
gboolean bbdb_check_gaim_enabled (void);

/* gaimbuddies.c */
void bbdb_sync_buddy_list (void);
void bbdb_sync_buddy_list_check (void);


#endif /* __BBDB_H__ */
