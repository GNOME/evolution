/*
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eab-config.h"

static GObjectClass *ecp_parent_class;

struct _EABConfigPrivate {
	guint source_changed_id;
};

#define _PRIVATE(o) (g_type_instance_get_private((GTypeInstance *)o, eab_config_get_type()))

static void
ecp_init (GObject *o)
{
}

static void
ecp_target_free (EConfig *ec, EConfigTarget *t)
{
	struct _EABConfigPrivate *p = _PRIVATE(ec);

	if (ec->target == t) {
		switch (t->type) {
		case EAB_CONFIG_TARGET_SOURCE: {
			EABConfigTargetSource *s = (EABConfigTargetSource *)t;

			if (p->source_changed_id) {
				g_signal_handler_disconnect(s->source, p->source_changed_id);
				p->source_changed_id = 0;
			}
			break; }
		}
	}

	switch (t->type) {
	case EAB_CONFIG_TARGET_SOURCE: {
		EABConfigTargetSource *s = (EABConfigTargetSource *)t;

		if (s->source)
			g_object_unref (s->source);
		break; }
	}

	((EConfigClass *) ecp_parent_class)->target_free (ec, t);
}

static void
ecp_source_changed(struct _ESource *source, EConfig *ec)
{
	e_config_target_changed(ec, E_CONFIG_TARGET_CHANGED_STATE);
}

static void
ecp_set_target (EConfig *ec, EConfigTarget *t)
{
	struct _EABConfigPrivate *p = _PRIVATE(ec);

	((EConfigClass *)ecp_parent_class)->set_target(ec, t);

	if (t) {
		switch (t->type) {
		case EAB_CONFIG_TARGET_SOURCE: {
			EABConfigTargetSource *s = (EABConfigTargetSource *)t;

			p->source_changed_id = g_signal_connect(s->source, "changed", G_CALLBACK(ecp_source_changed), ec);
			break; }
		}
	}
}

static void
ecp_class_init (GObjectClass *klass)
{
	((EConfigClass *)klass)->set_target = ecp_set_target;
	((EConfigClass *)klass)->target_free = ecp_target_free;

	g_type_class_add_private(klass, sizeof(struct _EABConfigPrivate));
}

GType
eab_config_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (EABConfigClass),
			NULL, NULL,
			(GClassInitFunc) ecp_class_init,
			NULL, NULL,
			sizeof (EABConfig), 0,
			(GInstanceInitFunc) ecp_init
		};

		ecp_parent_class = g_type_class_ref (e_config_get_type ());
		type = g_type_register_static (e_config_get_type (), "EABConfig", &info, 0);
	}

	return type;
}

EABConfig *
eab_config_new (gint type, const gchar *menuid)
{
	EABConfig *ecp = g_object_new (eab_config_get_type(), NULL);
	e_config_construct (&ecp->config, type, menuid);
	return ecp;
}

EABConfigTargetSource *
eab_config_target_new_source (EABConfig *ecp, struct _ESource *source)
{
	EABConfigTargetSource *t = e_config_target_new (&ecp->config, EAB_CONFIG_TARGET_SOURCE, sizeof (*t));

	t->source = source;
	g_object_ref (source);

	return t;
}
