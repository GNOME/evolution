/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __CAMEL_PROCESS_H__
#define __CAMEL_PROCESS_H__

#include <sys/types.h>

#include <camel/camel-exception.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

pid_t camel_process_fork (const char *path, char **argv, int *infd, int *outfd, int *errfd, CamelException *ex);

int camel_process_wait (pid_t pid);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_PROCESS_H__ */
