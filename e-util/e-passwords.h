/*
 * e-passwords.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 */

/*
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
 * USA.
 */

#ifndef _E_PASSWORD_H_
#define _E_PASSWORD_H_

#include <glib.h>
#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

/*
  initialization is now implicit when you call any of the functions
  below, although this is only correct if the functions are called
  from the main thread.

  e_passwords_shutdown should be called at exit time to synch the
  password on-disk storage, and to free up in-memory storage. */
void e_passwords_init (void);

void        e_passwords_shutdown          (void);
void	    e_passwords_cancel(void);
void        e_passwords_set_online(int state);
void        e_passwords_remember_password (const char *component, const char *key);
void        e_passwords_add_password      (const char *key, const char *passwd);
char       *e_passwords_get_password      (const char *component, const char *key);
void        e_passwords_forget_password   (const char *component, const char *key);
void        e_passwords_forget_passwords  (void);
void        e_passwords_clear_passwords (const char *component);

typedef enum {
	E_PASSWORDS_REMEMBER_NEVER,
	E_PASSWORDS_REMEMBER_SESSION,
	E_PASSWORDS_REMEMBER_FOREVER,
	E_PASSWORDS_REMEMBER_MASK = 0xf,

	/* option bits */
	E_PASSWORDS_SECRET = 1<<8,
	E_PASSWORDS_REPROMPT = 1<<9,
	E_PASSWORDS_ONLINE = 1<<10, /* only ask if we're online */
} EPasswordsRememberType;

char *      e_passwords_ask_password      (const char *title, 
					   const char*component_name, const char *key,
					   const char *prompt,
					   EPasswordsRememberType remember_type,
					   gboolean *remember,
					   GtkWindow *parent);

G_END_DECLS

#endif /* _E_PASSWORD_H_ */
