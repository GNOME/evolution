/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Author: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999 Ximian (www.ximian.com/).
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

#ifndef _CAMEL_LOCK_H
#define _CAMEL_LOCK_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-exception.h>

/* for .lock locking, retry, delay and stale counts */
#define CAMEL_LOCK_DOT_RETRY (5) /* number of times to retry lock */
#define CAMEL_LOCK_DOT_DELAY (2) /* delay between locking retries */
#define CAMEL_LOCK_DOT_STALE (60) /* seconds before a lock becomes stale */

/* for locking folders, retry/interretry delay */
#define CAMEL_LOCK_RETRY (5) /* number of times to retry lock */
#define CAMEL_LOCK_DELAY (2) /* delay between locking retries */

typedef enum {
	CAMEL_LOCK_READ,
	CAMEL_LOCK_WRITE,
} CamelLockType;

/* specific locking strategies */
int camel_lock_dot(const char *path, CamelException *ex);
int camel_lock_fcntl(int fd, CamelLockType type, CamelException *ex);
int camel_lock_flock(int fd, CamelLockType type, CamelException *ex);

void camel_unlock_dot(const char *path);
void camel_unlock_fcntl(int fd);
void camel_unlock_flock(int fd);

/* lock a folder in a standard way */
int camel_lock_folder(const char *path, int fd, CamelLockType type, CamelException *ex);
void camel_unlock_folder(const char *path, int fd);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_CAMEL_LOCK_H */
