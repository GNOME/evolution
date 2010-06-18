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
 *		Fridrich Strba <fridrich.strba@bluewin.ch>
 *
 * Copyright (C) 2010 Fridrich Strba
 *
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
