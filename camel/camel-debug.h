/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- *
 *
 * Author: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2004 Novell Inc. (www.novell.com)
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

#ifndef CAMEL_DEBUG_H
#define CAMEL_DEBUG_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>

/* This is how the basic debug checking strings should be done */
#define CAMEL_DEBUG_IMAP "imap"
#define CAMEL_DEBUG_IMAP_FOLDER "imap:folder"

void camel_debug_init(void);
gboolean camel_debug(const char *mode);

gboolean camel_debug_start(const char *mode);
void camel_debug_end(void);

/* This interface is deprecated */
extern int camel_verbose_debug;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_DEBUG_H */
