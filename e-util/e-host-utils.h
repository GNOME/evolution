/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-host-utils.h
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Chris Toshok, Jeffrey Stedfast
 */

#ifndef E_HOST_UTILS_H
#define E_HOST_UTILS_H

#include <sys/types.h>
#include <netdb.h>

/* gethostbyname_r implementation that works for systems without a
   native gethostbyname_r.  if you use this, you must make sure to
   *only* use this - it can't even coexist with naked calls to
   gethostbyname (even if they exist in libraries.)  yes, this loses
   in many ways.  blame your local OS developer. */
int e_gethostbyname_r (const char *name, struct hostent *host, char *buf, size_t buflen, int *herr);

int e_gethostbyaddr_r (const char *addr, int len, int type, struct hostent *host, char *buf, size_t buflen, int *herr);

#endif /* E_HOST_UTILS_H */
