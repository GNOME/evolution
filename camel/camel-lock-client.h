/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Author: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 2001 Ximian Inc (www.ximian.com/)
 *
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
 * USA
 */

/* defines protocol for lock helper process ipc */

#ifndef _CAMEL_LOCK_CLIENT_H
#define _CAMEL_LOCK_CLIENT_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-exception.h>

int camel_lock_helper_lock(const char *path , CamelException *ex);
int camel_lock_helper_unlock(int lockid);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_CAMEL_LOCK_HELPER_H */
