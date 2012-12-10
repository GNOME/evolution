/*
 * e-passwords.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef EDS_DISABLE_DEPRECATED

#ifndef _E_PASSWORD_H_
#define _E_PASSWORD_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

/*
 * initialization is now implicit when you call any of the functions
 * below, although this is only correct if the functions are called
 * from the main thread.
 *
 * e_passwords_shutdown should be called at exit time to synch the
 * password on-disk storage, and to free up in-memory storage. */
void e_passwords_init (void);

void        e_passwords_shutdown          (void);
void	    e_passwords_cancel (void);
void        e_passwords_set_online (gint state);
void        e_passwords_remember_password (const gchar *unused, const gchar *key);
void        e_passwords_add_password      (const gchar *key, const gchar *passwd);
gchar       *e_passwords_get_password      (const gchar *unused, const gchar *key);
void        e_passwords_forget_password   (const gchar *unused, const gchar *key);
void        e_passwords_forget_passwords  (void);
void        e_passwords_clear_passwords (const gchar *unused);

typedef enum {
	E_PASSWORDS_REMEMBER_NEVER,
	E_PASSWORDS_REMEMBER_SESSION,
	E_PASSWORDS_REMEMBER_FOREVER,
	E_PASSWORDS_REMEMBER_MASK = 0xf,

	/* option bits */
	E_PASSWORDS_SECRET = 1 << 8,
	E_PASSWORDS_REPROMPT = 1 << 9,
	E_PASSWORDS_ONLINE = 1<<10, /* only ask if we're online */
	E_PASSWORDS_DISABLE_REMEMBER = 1<<11, /* disable the 'remember password' checkbox */
	E_PASSWORDS_PASSPHRASE = 1<<12 /* We are asking a passphrase */
} EPasswordsRememberType;

gchar *      e_passwords_ask_password     (const gchar *title,
					   const gchar *unused,
					   const gchar *key,
					   const gchar *prompt,
					   EPasswordsRememberType remember_type,
					   gboolean *remember,
					   GtkWindow *parent);

G_END_DECLS

#endif /* _E_PASSWORD_H_ */

#endif /* EDS_DISABLE_DEPRECATED */
