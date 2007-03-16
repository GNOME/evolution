/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-autofilter.h
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Authors:
 *   Michael Zucchi <notzed@ximian.com>
 *   Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _MAIL_AUTOFILTER_H
#define _MAIL_AUTOFILTER_H

struct _FilterRule;
struct _EMVFolderContext;
struct _EMFilterContext;
struct _CamelMimeMessage;
struct _CamelInternetAddress;
struct _CamelStore;

enum {
	AUTO_SUBJECT = 1,
	AUTO_FROM = 2,
	AUTO_TO = 4,
	AUTO_MLIST = 8,
};

struct _FilterRule *em_vfolder_rule_from_message(struct _EMVFolderContext *context, struct _CamelMimeMessage *msg, int flags, const char *source);
struct _FilterRule *filter_rule_from_message(struct _EMFilterContext *context, struct _CamelMimeMessage *msg, int flags);
struct _FilterRule *em_vfolder_rule_from_address(struct _EMVFolderContext *context, struct _CamelInternetAddress *addr, int flags, const char *source);

/* easiest place to put this */
void  filter_gui_add_from_message(struct _CamelMimeMessage *msg, const char *source, int flags);

/* Also easiest place for these, we should really share a global rule context for this stuff ... */
void mail_filter_rename_uri(struct _CamelStore *store, const char *olduri, const char *newuri);
void mail_filter_delete_uri(struct _CamelStore *store, const char *uri);

#endif
