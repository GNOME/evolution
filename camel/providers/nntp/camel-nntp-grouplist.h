/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-nntp-grouplist.h : getting/updating the list of newsgroups on the server. */

/* 
 * Author : Chris Toshok <toshok@helixcode.com> 
 *
 * Copyright (C) 2000 Helix Code .
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

#ifndef CAMEL_NNTP_GROUPLIST_H
#define CAMEL_NNTP_GROUPLIST_H 1

#include <time.h>
#include "camel-nntp-store.h"

struct CamelNNTPGroupListEntry {
	char *group_name;
	guint32 low;
	guint32 high;
	guint32 flags;
};

struct CamelNNTPGroupList {
	CamelNNTPStore *store;
	time_t time;
	GList *group_list;
};

CamelNNTPGroupList* camel_nntp_grouplist_fetch  (CamelNNTPStore *store, CamelException *ex);
gint                camel_nntp_grouplist_update (CamelNNTPGroupList *group_list, CamelException *ex);
void                camel_nntp_grouplist_save   (CamelNNTPGroupList *group_list, CamelException *ex);
void                camel_nntp_grouplist_free   (CamelNNTPGroupList *group_list);

#endif /* CAMEL_NNTP_GROUPLIST_H */
