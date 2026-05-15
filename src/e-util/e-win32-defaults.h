/*
 * SPDX-FileCopyrightText: (C) 2010 Fridrich Strba
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Fridrich Strba <fridrich.strba@bluewin.ch>
 */

#ifndef __E_WIN32_DEFAULTS__
#define __E_WIN32_DEFAULTS__

#include <glib.h>

G_BEGIN_DECLS

void	_e_win32_register_mailer (void);
void	_e_win32_unregister_mailer (void);
void	_e_win32_set_default_mailer (void);
void	_e_win32_unset_default_mailer (void);

void	_e_win32_register_addressbook (void);
void	_e_win32_unregister_addressbook (void);

G_END_DECLS

#endif /* __E_WIN32_DEFAULTS__ */
