/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-autofilter.h
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 *   Michael Zucchi <notzed@helixcode.com>
 *   Ettore Perazzoli <ettore@helixcode.com>
 */

#ifndef _MAIL_AUTOFILTER_H
#define _MAIL_AUTOFILTER_H

#include "filter/filter-rule.h"
#include "filter/filter-context.h"
#include "filter/vfolder-context.h"
#include "camel/camel-mime-message.h"

enum {
	AUTO_SUBJECT = 1,
	AUTO_FROM = 2,
	AUTO_TO = 4
};

FilterRule *vfolder_rule_from_message(VfolderContext *context, CamelMimeMessage *msg, int flags, const char *source);
FilterRule *filter_rule_from_message(FilterContext *context, CamelMimeMessage *msg, int flags);

/* easiest place to put this */

void  filter_gui_add_from_message      (CamelMimeMessage *msg,
					int               flags);

void  filter_gui_add_for_mailing_list  (CamelMimeMessage *msg,
					const char       *list_name,
					const char       *header_name,
					const char       *header_value);

#endif
