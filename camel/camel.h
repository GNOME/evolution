/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 HelixCode (http://www.helixcode.com) .
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


#ifndef CAMEL_H
#define CAMEL_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include <camel/data-wrapper-repository.h>
#include <camel/data-wrapper-repository.h>
#include <camel/camel-log.h>
#include <camel/camel-exception.h>
#include <camel/camel-data-wrapper.h>
#include <camel/camel-simple-data-wrapper.h>
#include <camel/camel-folder.h>
#include <camel/camel-folder-pt-proxy.h>
#include <camel/camel-marshal-utils.h>
#include <camel/camel-mime-body-part.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-multipart.h>
#include <camel/camel-op-queue.h>
#include <camel/camel-provider.h>
#include <camel/camel-service.h>
#include <camel/camel-session.h>
#include <camel/camel-store.h>
#include <camel/camel-stream.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-thread-proxy.h>
#include <camel/data-wrapper-repository.h>
#include <camel/gmime-content-field.h>
#include <camel/gmime-utils.h>
#include <camel/gstring-util.h>
#include <camel/string-utils.h>
#include <camel/url-util.h>

gint camel_init (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_H */
