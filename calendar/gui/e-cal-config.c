/*
 * Authors: David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 2004 Novell, Inc (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkwidget.h>
#include "e-cal-config.h"

static GObjectClass *ecp_parent_class;
static GObjectClass *ecph_parent_class;

struct _ECalConfigPrivate {
	ulong source_changed_id;
};

#define _PRIVATE(o) (g_type_instance_get_private ((GTypeInstance *)o, e_cal_config_get_type ()))

static void
ecp_init (GObject *o)
{
}

static void
ecp_target_free (EConfig *ec, EConfigTarget *t)
{
	struct _ECalConfigPrivate *p = _PRIVATE (ec);

	if (ec->target == t) {
		switch (t->type) {
		case EC_CONFIG_TARGET_SOURCE: {
			ECalConfigTargetSource *s = (ECalConfigTargetSource *)t;

			if (p->source_changed_id) {
				g_signal_handler_disconnect (s->source, p->source_changed_id);
				p->source_changed_id = NULL;
			}
			break; }
		}
	}

	switch (t->type) {
	case EC_CONFIG_TARGET_SOURCE: {
		ECalConfigTargetSource *s = (ECalConfigTargetSource *)t;
		if (s->source)
			g_object_unref (s->source);
		break; }
	}

	((EConfigClass *) ecp_parent_class)->target_free (ec, t);
}

static void
ecp_source_changed (struct _ESource *source, EConfig *ec)
{
	e_config_target_changed (ec, E_CONFIG_TARGET_CHANGED_STATE);
}

static void
ecp_set_target (EConfig *ec, EConfigTarget *t)
{
	struct _ECalConfigPrivate *p = _PRIVATE(ec);

	((EConfigClass *)ecp_parent_class)->set_target(ec, t);

	if (t) {
		switch (t->type) {
		case EC_CONFIG_TARGET_SOURCE: {
			ECalConfigTargetSource *s = (ECalConfigTargetSource *)t;

			p->source_changed_id = g_signal_connect (s->source, "changed", G_CALLBACK (ecp_source_changed), ec);
			break; }
		}
	}
}

static void
ecp_class_init (GObjectClass *klass)
{
	((EConfigClass *)klass)->set_target = ecp_set_target;
	((EConfigClass *)klass)->target_free = ecp_target_free;

	g_type_class_add_private (klass, sizeof (struct _ECalConfigPrivate));
}

GType
e_cal_config_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (ECalConfigClass),
			NULL, NULL,
			(GClassInitFunc) ecp_class_init,
			NULL, NULL,
			sizeof (ECalConfig), 0,
			(GInstanceInitFunc) ecp_init
		};

		ecp_parent_class = g_type_class_ref (e_config_get_type ());
		type = g_type_register_static (e_config_get_type (), "ECalConfig", &info, 0);
	}

	return type;
}

ECalConfig *
e_cal_config_new (int type, const char *menuid)
{
	ECalConfig *ecp = g_object_new (e_cal_config_get_type(), 0);
	e_config_construct (&ecp->config, type, menuid);
	return ecp;
}

ECalConfigTargetSource *
e_cal_config_target_new_source (ECalConfig *ecp, struct _ESource *source)
{
	ECalConfigTargetSource *t = e_config_target_new (&ecp->config, EC_CONFIG_TARGET_SOURCE, sizeof (*t));

	t->source = source;
	g_object_ref (source);

	return t;
}

static const EConfigHookTargetMask ecph_no_masks[] = {
	{ 0 }
};

static const EConfigHookTargetMap ecph_targets[] = {
	{ "source", EC_CONFIG_TARGET_SOURCE, ecph_no_masks },
	{ 0 },
};

static void
ecph_class_init (EPluginHookClass *klass)
{
	int i;

	klass->id = "org.gnome.evolution.calendar.config:1.0";

	for (i = 0; ecph_targets[i].type; i++) {
		g_print ("adding hook target '%s'\n", ecph_targets[i].type);
		e_config_hook_class_add_target_map ((EConfigHookClass *)klass, &ecph_targets[i]);
	}

	((EConfigHookClass *)klass)->config_class = g_type_class_ref (e_cal_config_get_type ());
}

GType
e_cal_config_hook_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (ECalConfigHookClass),
			NULL, NULL,
			(GClassInitFunc) ecph_class_init,
			NULL, NULL,
			sizeof (ECalConfigHook), 0,
			(GInstanceInitFunc) NULL
		};

		ecph_parent_class = g_type_class_ref (e_config_hook_get_type ());
		type = g_type_register_static (e_config_hook_get_type (), "ECalConfigHook", &info, 0);
	}

	return type;
}
