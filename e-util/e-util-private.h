/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-util-private.h
 * Copyright 2005, Novell, Inc.
 *
 * Authors:
 *   Tor Lillqvist <tml@novell.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _E_UTIL_PRIVATE_H_
#define _E_UTIL_PRIVATE_H_

#include <glib.h>

#ifdef G_OS_WIN32

const char *_gal_get_localedir (void) G_GNUC_CONST;
const char *_gal_get_gladedir (void) G_GNUC_CONST;
const char *_gal_get_imagesdir (void) G_GNUC_CONST;

#undef EVOLUTION_LOCALEDIR
#define EVOLUTION_LOCALEDIR _gal_get_localedir ()

#undef EVOLUTION_GLADEDIR
#define EVOLUTION_GLADEDIR _gal_get_gladedir ()

#undef GAL_IMAGESDIR
#define GAL_IMAGESDIR _gal_get_imagesdir ()

#endif	/* G_OS_WIN32 */

#endif	/* _E_UTIL_PRIVATE_H_ */
