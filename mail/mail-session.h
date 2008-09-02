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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef MAIL_SESSION_H
#define MAIL_SESSION_H

#include <glib.h>
#include <bonobo/bonobo-ui-component.h>
#include <camel/camel-session.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

void mail_session_init (const char *base_directory);
void mail_session_shutdown (void);
gboolean mail_session_get_interactive (void);
void mail_session_set_interactive (gboolean interactive);
char *mail_session_request_dialog (const char *prompt, gboolean secret,
				   const char *key, gboolean async);
gboolean mail_session_accept_dialog (const char *prompt, const char *key,
				     gboolean async);
char *mail_session_get_password (const char *url);
void mail_session_add_password (const char *url, const char *passwd);
void mail_session_forget_passwords (BonoboUIComponent *uih, void *user_data,
				    const char *path);
void mail_session_remember_password (const char *url);

void mail_session_forget_password (const char *key);

void mail_session_flush_filter_log (void);

void mail_session_add_junk_plugin (const char *plugin_name, CamelJunkPlugin *junk_plugin);

const GList * mail_session_get_junk_plugins (void);
void mail_session_set_junk_headers (const char **name, const char **value, int len);

extern CamelSession *session;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! MAIL_SESSION_H */
