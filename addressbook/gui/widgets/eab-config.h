/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EAB_CONFIG_H__
#define __EAB_CONFIG_H__

#include <glib-object.h>

#include "e-util/e-config.h"

G_BEGIN_DECLS

typedef struct _EABConfig EABConfig;
typedef struct _EABConfigClass EABConfigClass;

struct _EABConfig {
	EConfig config;
};

struct _EABConfigClass {
	EConfigClass config_class;
};

enum _eab_config_target_t {
	EAB_CONFIG_TARGET_SOURCE
};

typedef struct _EABConfigTargetSource EABConfigTargetSource;

struct _EABConfigTargetSource {
	EConfigTarget target;

	struct _ESource *source;
};

typedef struct _EConfigItem EABConfigItem;

GType eab_config_get_type (void);
EABConfig *eab_config_new (gint type, const gchar *menuid);

EABConfigTargetSource *eab_config_target_new_source (EABConfig *ecp, struct _ESource *source);

G_END_DECLS

#endif
