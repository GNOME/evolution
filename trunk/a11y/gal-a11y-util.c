/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2002 Ximian, Inc.
 */

#include <config.h>

#include "gal-a11y-util.h"

GType
gal_a11y_type_register_static_with_private  (GType            parent_type,
					     const gchar     *type_name,
					     GTypeInfo *info,
					     GTypeFlags       flags,
					     gint             priv_size,
					     gint            *priv_offset)
{
	GTypeQuery query;

	g_type_query (parent_type, &query);

	info->class_size = query.class_size;
	info->instance_size = query.instance_size + priv_size;

	if (priv_offset)
		*priv_offset = query.instance_size;

	return g_type_register_static (parent_type, type_name, info, flags);
}
