/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "eab-config.h"

struct _EABConfigPrivate {
	guint source_changed_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (EABConfig, eab_config, E_TYPE_CONFIG)

static void
eab_config_init (EABConfig *cfg)
{
	cfg->priv = eab_config_get_instance_private (cfg);
}

static void
ecp_target_free (EConfig *ec,
                 EConfigTarget *t)
{
	struct _EABConfigPrivate *p = EAB_CONFIG (ec)->priv;

	if (ec->target == t) {
		switch (t->type) {
		case EAB_CONFIG_TARGET_SOURCE: {
			EABConfigTargetSource *s = (EABConfigTargetSource *) t;

			if (p->source_changed_id) {
				g_signal_handler_disconnect (s->source, p->source_changed_id);
				p->source_changed_id = 0;
			}
			break; }
		case EAB_CONFIG_TARGET_PREFS:
			break;
		}
	}

	switch (t->type) {
	case EAB_CONFIG_TARGET_SOURCE: {
		EABConfigTargetSource *s = (EABConfigTargetSource *) t;

		if (s->source)
			g_object_unref (s->source);
		break; }
	case EAB_CONFIG_TARGET_PREFS: {
		EABConfigTargetPrefs *s = (EABConfigTargetPrefs *) t;

		if (s->settings)
			g_object_unref (s->settings);
		break; }
	}

	((EConfigClass *) eab_config_parent_class)->target_free (ec, t);
}

static void
ecp_source_changed (ESource *source,
                    EConfig *ec)
{
	e_config_target_changed (ec, E_CONFIG_TARGET_CHANGED_STATE);
}

static void
ecp_set_target (EConfig *ec,
                EConfigTarget *t)
{
	EABConfig *self = EAB_CONFIG (ec);

	((EConfigClass *) eab_config_parent_class)->set_target (ec, t);

	if (t) {
		switch (t->type) {
		case EAB_CONFIG_TARGET_SOURCE: {
			EABConfigTargetSource *s = (EABConfigTargetSource *) t;

			self->priv->source_changed_id = g_signal_connect (
				s->source, "changed",
				G_CALLBACK (ecp_source_changed), ec);
			break; }
		case EAB_CONFIG_TARGET_PREFS:
			break;
		}
	}
}

static void
eab_config_class_init (EABConfigClass *class)
{
	EConfigClass *config_class;

	config_class = E_CONFIG_CLASS (class);
	config_class->set_target = ecp_set_target;
	config_class->target_free = ecp_target_free;
}

EABConfig *
eab_config_new (const gchar *menuid)
{
	EABConfig *ecp = g_object_new (eab_config_get_type (), NULL);
	e_config_construct (&ecp->config, menuid);
	return ecp;
}

EABConfigTargetSource *
eab_config_target_new_source (EABConfig *ecp,
                              ESource *source)
{
	EABConfigTargetSource *t = e_config_target_new (
		&ecp->config, EAB_CONFIG_TARGET_SOURCE, sizeof (*t));

	t->source = source;
	g_object_ref (source);

	return t;
}

EABConfigTargetPrefs *
eab_config_target_new_prefs (EABConfig *ecp,
                             GSettings *settings)
{
	EABConfigTargetPrefs *t = e_config_target_new (
		&ecp->config, EAB_CONFIG_TARGET_PREFS, sizeof (*t));

	if (settings)
		t->settings = g_object_ref (settings);
	else
		t->settings = NULL;

	return t;
}
