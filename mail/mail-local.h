/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-local.h: Local mailbox support. */

/* 
 * Author: 
 *  Michael Zucchi <NotZed@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
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

#ifndef _MAIL_LOCAL_H
#define _MAIL_LOCAL_H

#include "camel/camel-folder.h"
#include "folder-browser.h"

/* mail-local.c */
CamelFolder *mail_tool_local_uri_to_folder(const char *uri, CamelException *ex);
void local_reconfigure_folder(FolderBrowser *fb);

#endif
