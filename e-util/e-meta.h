/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- *
 *
 * A simple persistent meta-data api.
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2003 Ximian, Inc. (www.ximian.com)
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
 */

#ifndef _E_META_H
#define _E_META_H 1

#include <glib.h>
#include <glib-object.h>

typedef struct _EMeta EMeta;
typedef struct _EMetaClass EMetaClass;

struct _EMeta {
	GObject object;

	struct _EMetaPrivate *priv;
};

struct _EMetaClass {
	GObjectClass object;
};

GType e_meta_get_type (void);

/* 'trivial' meta-data api */
EMeta *e_meta_new(const char *path);
void e_meta_set(EMeta *em, const char *key, ...);
void e_meta_get(EMeta *em, const char *key, ...);
int e_meta_sync(EMeta *em);

/* helpers */
gboolean e_meta_get_bool(EMeta *, const char *key, gboolean def);
void e_meta_set_bool(EMeta *, const char *key, gboolean val);

/* 'class' methods */
EMeta *e_meta_data_find(const char *base, const char *key);
void e_meta_data_delete(const char *base, const char *key);

#endif /* ! _E_META_H */
