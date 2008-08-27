/*
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EXCHANGE_CHANGE_PASSWORD_H__
#define __EXCHANGE_CHANGE_PASSWORD_H__

#include <exchange-types.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

char *exchange_get_new_password (const char *existing_password,
				 gboolean    voluntary);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EXCHANGE_CHANGE_PASSWORD_H__ */
