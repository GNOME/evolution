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
#include <bonobo/bonobo-ui-component.h>
#include <gtk/gtkwindow.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

void        e_passwords_init              (const char *component);
void        e_passwords_shutdown          (void);

void        e_passwords_remember_password (const char *key);
void        e_passwords_add_password      (const char *key, const char *passwd);
char       *e_passwords_get_password      (const char *key);
void        e_passwords_forget_password   (const char *key);
void        e_passwords_forget_passwords  (void);
void        e_passwords_clear_component_passwords (void);

typedef enum {
	E_PASSWORDS_DO_NOT_REMEMBER,
	E_PASSWORDS_REMEMBER_FOR_SESSION,
	E_PASSWORDS_REMEMBER_FOREVER
} EPasswordsRememberType;

char *      e_passwords_ask_password      (const char *title, const char *key,
					   const char *prompt, gboolean secret,
					   EPasswordsRememberType remember_type,
					   gboolean *remember,
					   GtkWindow *parent);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_PASSWORD_H_ */
