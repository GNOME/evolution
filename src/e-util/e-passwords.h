/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef EVO_DISABLE_DEPRECATED

#ifndef E_PASSWORDS_H
#define E_PASSWORDS_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

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

/*
 * initialization is now implicit when you call any of the functions
 * below, although this is only correct if the functions are called
 * from the main thread.
 */
void		e_passwords_init		(void);
void		e_passwords_set_online		(gint state);
void		e_passwords_remember_password	(const gchar *key);
void		e_passwords_add_password	(const gchar *key,
						 const gchar *passwd);
gchar *		e_passwords_get_password	(const gchar *key);
void		e_passwords_forget_password	(const gchar *key);
gchar *		e_passwords_ask_password	(const gchar *title,
						 const gchar *key,
						 const gchar *prompt,
						 EPasswordsRememberType remember_type,
						 gboolean *remember,
						 GtkWindow *parent);

G_END_DECLS

#endif /* E_PASSWORDS_H */

#endif /* EVO_DISABLE_DEPRECATED */
