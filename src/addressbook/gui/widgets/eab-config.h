/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: David Trowbridge <trowbrds@cs.colorado.edu>
 */

#ifndef __EAB_CONFIG_H__
#define __EAB_CONFIG_H__

#include <libedataserver/libedataserver.h>

#include <e-util/e-util.h>

#define EAB_TYPE_CONFIG (eab_config_get_type ())
#define EAB_CONFIG(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EAB_TYPE_CONFIG, EABConfig))

G_BEGIN_DECLS

typedef struct _EABConfig EABConfig;
typedef struct _EABConfigClass EABConfigClass;
typedef struct _EABConfigPrivate EABConfigPrivate;

struct _EABConfig {
	EConfig config;

	EABConfigPrivate *priv;
};

struct _EABConfigClass {
	EConfigClass config_class;
};

enum _eab_config_target_t {
	EAB_CONFIG_TARGET_SOURCE,
	EAB_CONFIG_TARGET_PREFS
};

typedef struct _EABConfigTargetSource EABConfigTargetSource;

struct _EABConfigTargetSource {
	EConfigTarget target;

	ESource *source;
};

typedef struct _EABConfigTargetPrefs EABConfigTargetPrefs;

struct _EABConfigTargetPrefs {
	EConfigTarget target;

	/* preferences are global from GSettings */
	GSettings *settings;
};

typedef struct _EConfigItem EABConfigItem;

GType eab_config_get_type (void);
EABConfig *eab_config_new (const gchar *menuid);

EABConfigTargetSource *eab_config_target_new_source (EABConfig *ecp, ESource *source);
EABConfigTargetPrefs *eab_config_target_new_prefs (EABConfig *ecp, GSettings *settings);

G_END_DECLS

#endif
