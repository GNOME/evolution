/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-source-list.h
 *
 * Copyright (C) 2003  Ximian, Inc.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _E_SOURCE_LIST_H_
#define _E_SOURCE_LIST_H_

#include <libxml/tree.h>
#include <gconf/gconf-client.h>

#include "e-source-group.h"

#define E_TYPE_SOURCE_LIST			(e_source_list_get_type ())
#define E_SOURCE_LIST(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SOURCE_LIST, ESourceList))
#define E_SOURCE_LIST_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SOURCE_LIST, ESourceListClass))
#define E_IS_SOURCE_LIST(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SOURCE_LIST))
#define E_IS_SOURCE_LIST_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_SOURCE_LIST))


typedef struct _ESourceList        ESourceList;
typedef struct _ESourceListPrivate ESourceListPrivate;
typedef struct _ESourceListClass   ESourceListClass;

struct _ESourceList {
	GObject parent;

	ESourceListPrivate *priv;
};

struct _ESourceListClass {
	GObjectClass parent_class;

	/* Signals.  */

	void (* changed) (ESourceList *source_list);

	void (* group_removed) (ESourceList *source_list, ESourceGroup *group);
	void (* group_added) (ESourceList *source_list, ESourceGroup *group);
};


GType    e_source_list_get_type (void);

ESourceList *e_source_list_new            (void);
ESourceList *e_source_list_new_for_gconf  (GConfClient *client,
					   const char  *path);

GSList       *e_source_list_peek_groups         (ESourceList *list);
ESourceGroup *e_source_list_peek_group_by_uid   (ESourceList *list,
						 const char  *uid);
ESource      *e_source_list_peek_source_by_uid  (ESourceList *list,
						 const char  *uid);

gboolean  e_source_list_add_group             (ESourceList  *list,
					       ESourceGroup *group,
					       int           position);
gboolean  e_source_list_remove_group          (ESourceList  *list,
					       ESourceGroup *group);
gboolean  e_source_list_remove_group_by_uid   (ESourceList  *list,
					       const char   *uid);
gboolean  e_source_list_remove_source_by_uid  (ESourceList  *list,
					       const char   *uidj);

gboolean  e_source_list_sync  (ESourceList  *list,
			       GError      **error);


#endif /* _E_SOURCE_LIST_H_ */
