/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-source.h
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

#ifndef _E_SOURCE_H_
#define _E_SOURCE_H_

#include <glib-object.h>
#include <libxml/tree.h>

#define E_TYPE_SOURCE			(e_source_get_type ())
#define E_SOURCE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SOURCE, ESource))
#define E_SOURCE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SOURCE, ESourceClass))
#define E_IS_SOURCE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SOURCE))
#define E_IS_SOURCE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_SOURCE))


typedef struct _ESource        ESource;
typedef struct _ESourcePrivate ESourcePrivate;
typedef struct _ESourceClass   ESourceClass;

#include "e-source-group.h"

struct _ESource {
	GObject parent;

	ESourcePrivate *priv;
};

struct _ESourceClass {
	GObjectClass parent_class;

	/* Signals.  */
	void (* changed) (ESource *source);
};


GType    e_source_get_type (void);

ESource *e_source_new                (const char   *name,
				      const char   *relative_uri);
ESource *e_source_new_from_xml_node  (xmlNodePtr    node);

gboolean  e_source_update_from_xml_node  (ESource    *source,
					  xmlNodePtr  node,
					  gboolean   *changed_return);

char *e_source_uid_from_xml_node  (xmlNodePtr node);

void  e_source_set_group         (ESource      *source,
				  ESourceGroup *group);
void  e_source_set_name          (ESource      *source,
				  const char   *name);
void  e_source_set_relative_uri  (ESource      *source,
				  const char   *relative_uri);
void  e_source_set_color         (ESource      *source,
				  guint32       color);
void  e_source_unset_color       (ESource      *source);

ESourceGroup *e_source_peek_group         (ESource *source);
const char   *e_source_peek_uid           (ESource *source);
const char   *e_source_peek_name          (ESource *source);
const char   *e_source_peek_relative_uri  (ESource *source);
gboolean      e_source_get_color          (ESource *source,
					   guint32 *color_return);

char *e_source_get_uri  (ESource *source);

void  e_source_dump_to_xml_node  (ESource    *source,
				  xmlNodePtr  parent_node);


#endif /* _E_SOURCE_H_ */
