/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org> .
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
#include <config.h>
#include <camel/data-wrapper-repository.h>
#include <camel/data-wrapper-repository.h>
#include <camel/camel-log.h>	
#include <camel/camel-data-wrapper.h>
#include <camel-simple-data-wrapper.h>
#include <camel-folder.h>
#include <camel-mime-body-part.h>
#include <camel-mime-message.h>
#include <camel-mime-part.h>
#include <camel-multipart.h>
#include <camel-provider.h>
#include <camel-service.h>
#include <camel-session.h>
#include <camel-store.h>
#include <camel-stream.h>
#include <camel-stream-fs.h>
#include <camel-stream-mem.h>
#include <data-wrapper-repository.h>
#include <gmime-content-field.h>
#include <gmime-utils.h>
#include <gstring-util.h>
#include <string-utils.h>
#include <url-util.h>

gint camel_init ();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_H */
