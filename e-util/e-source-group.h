/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-source-group.h
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

#ifndef _E_SOURCE_GROUP_H_
#define _E_SOURCE_GROUP_H_

#include <glib-object.h>
#include <libxml/tree.h>

#define E_TYPE_SOURCE_GROUP			(e_source_group_get_type ())
#define E_SOURCE_GROUP(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SOURCE_GROUP, ESourceGroup))
#define E_SOURCE_GROUP_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SOURCE_GROUP, ESourceGroupClass))
#define E_IS_SOURCE_GROUP(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SOURCE_GROUP))
#define E_IS_SOURCE_GROUP_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_SOURCE_GROUP))


typedef struct _ESourceGroup        ESourceGroup;
typedef struct _ESourceGroupPrivate ESourceGroupPrivate;
typedef struct _ESourceGroupClass   ESourceGroupClass;

#include "e-source.h"

struct _ESourceGroup {
	GObject parent;

	ESourceGroupPrivate *priv;
};

struct _ESourceGroupClass {
	GObjectClass parent_class;

	/* Signals.  */

	void (* changed) (ESourceGroup *group);

	void (* source_removed) (ESourceGroup *source_list, ESource *source);
	void (* source_added)   (ESourceGroup *source_list, ESource *source);
};


GType    e_source_group_get_type (void);

ESourceGroup *e_source_group_new              (const char *name,
					       const char *base_uri);
ESourceGroup *e_source_group_new_from_xml     (const char *xml);
ESourceGroup *e_source_group_new_from_xmldoc  (xmlDocPtr   doc);

gboolean  e_source_group_update_from_xml     (ESourceGroup *group,
					      const char   *xml,
					      gboolean     *changed_return);
gboolean  e_source_group_update_from_xmldoc  (ESourceGroup *group,
					      xmlDocPtr     doc,
					      gboolean     *changed_return);

char *e_source_group_uid_from_xmldoc  (xmlDocPtr doc);

void  e_source_group_set_name      (ESourceGroup *group,
				    const char   *name);
void  e_source_group_set_base_uri  (ESourceGroup *group,
				    const char   *base_uri);

const char *e_source_group_peek_uid       (ESourceGroup *group);
const char *e_source_group_peek_name      (ESourceGroup *group);
const char *e_source_group_peek_base_uri  (ESourceGroup *group);

GSList  *e_source_group_peek_sources        (ESourceGroup *group);
ESource *e_source_group_peek_source_by_uid  (ESourceGroup *group,
					     const char   *source_uid);
ESource *e_source_group_peek_source_by_name (ESourceGroup *group,
					     const char   *source_name);

gboolean  e_source_group_add_source            (ESourceGroup *group,
						ESource      *source,
						int           position);
gboolean  e_source_group_remove_source         (ESourceGroup *group,
						ESource      *source);
gboolean  e_source_group_remove_source_by_uid  (ESourceGroup *group,
						const char   *uid);

char *e_source_group_to_xml (ESourceGroup *group);


#endif /* _E_SOURCE_GROUP_H_ */
