/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Author: Michael Zucchi <notzed@helixcode.com>
 *
 * Copyright (C) 1999 Helix Code (http://www.helixcode.com/).
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#ifndef _CAMEL_LOCK_HELPER_H
#define _CAMEL_LOCK_HELPER_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>

struct _CamelLockHelperMsg {
	guint32 magic;
	guint32 seq;
	guint32 id;
	guint32 data;
};

/* magic values */
enum {
	CAMEL_LOCK_HELPER_MAGIC = 0xABADF00D,
	CAMEL_LOCK_HELPER_RETURN_MAGIC = 0xDEADBEEF
};

/* return status */
enum {
	CAMEL_LOCK_HELPER_STATUS_OK = 0,
	CAMEL_LOCK_HELPER_STATUS_PROTOCOL,
	CAMEL_LOCK_HELPER_STATUS_NOMEM,
	CAMEL_LOCK_HELPER_STATUS_SYSTEM,
	CAMEL_LOCK_HELPER_STATUS_INVALID, /* not allowed to lock/doesn't exist etc */
};

/* commands */
enum {
	CAMEL_LOCK_HELPER_LOCK = 0xf0f,
	CAMEL_LOCK_HELPER_UNLOCK = 0xf0f0,
};

/* seconds between lock refreshes */
#define CAMEL_DOT_LOCK_REFRESH (30)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_CAMEL_LOCK_HELPER_H */
