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

#include <filter/filter-rule.h>
#include <filter/filter-context.h>
#include <filter/vfolder-context.h>
#include <camel/camel-mime-message.h>

enum {
	AUTO_SUBJECT = 1,
	AUTO_FROM = 2,
	AUTO_TO = 4,
	AUTO_MLIST = 8,
};

FilterRule *vfolder_rule_from_message(VfolderContext *context, CamelMimeMessage *msg, int flags, const char *source);
FilterRule *filter_rule_from_message(FilterContext *context, CamelMimeMessage *msg, int flags);

/* easiest place to put this */
void  filter_gui_add_from_message (CamelMimeMessage *msg, const char *source, int flags);

/* Also easiest place for these, we should really share a global rule context for this stuff ... */
void mail_filter_rename_uri(CamelStore *store, const char *olduri, const char *newuri);
void mail_filter_delete_uri(CamelStore *store, const char *uri);

#endif
