/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifndef __GAL_A11Y_UTIL_H__
#define __GAL_A11Y_UTIL_H__

#include <glib-object.h>

GType  gal_a11y_type_register_static_with_private  (GType        parent_type,
						    const gchar *type_name,
						    GTypeInfo   *info,
						    GTypeFlags   flags,
						    int          priv_size,
						    gint        *priv_offset);

#endif /* ! __GAL_A11Y_UTIL_H__ */
