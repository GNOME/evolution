/*  
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#ifndef MAIL_SESSION_REMOTE_H
#define MAIL_SESSION_REMOTE_H

typedef struct _MailSessionRemote  {
	char *object_id;
}MailSessionRemote;

#define mail_session_init	mail_session_remote_init
#define mail_session_get_interactive	mail_session_remote_get_interactive
#define mail_session_set_interactive	mail_session_remote_set_interactive
#define mail_session_get_password	mail_session_remote_get_password
#define mail_session_add_password	mail_session_remote_add_password
#define mail_session_remember_password	mail_session_remote_remember_password
#define mail_session_forget_password	mail_session_remote_forget_password
#define	mail_session_flush_filter_log	mail_session_remote_flush_filter_log
#define mail_session_shutdown	mail_session_remote_shutdown


void
mail_session_remote_init (const char *base_dir);

gboolean
mail_session_remote_get_interactive (void);

void
mail_session_remote_set_interactive (gboolean interactive);

char *
mail_session_remote_get_password (const char *url_string);

void
mail_session_remote_add_password (const char *url, const char *passwd);

void
mail_session_remote_remember_password (const char *url_string);

void
mail_session_remote_forget_password (const char *key);

void 
mail_session_remote_flush_filter_log (void);

void
mail_session_remote_shutdown (void);

#endif
