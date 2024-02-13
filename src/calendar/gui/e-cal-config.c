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

#include <gtk/gtk.h>

#include "e-cal-config.h"

struct _ECalConfigPrivate {
	guint source_changed_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (ECalConfig, e_cal_config, E_TYPE_CONFIG)

static void
ecp_target_free (EConfig *ec,
                 EConfigTarget *t)
{
	ECalConfigPrivate *p = E_CAL_CONFIG (ec)->priv;

	if (ec->target == t) {
		switch (t->type) {
		case EC_CONFIG_TARGET_SOURCE: {
			ECalConfigTargetSource *s = (ECalConfigTargetSource *) t;

			if (p->source_changed_id) {
				g_signal_handler_disconnect (
					s->source, p->source_changed_id);
				p->source_changed_id = 0;
			}
			break; }
		case EC_CONFIG_TARGET_PREFS: {
			break; }
		}
	}

	switch (t->type) {
	case EC_CONFIG_TARGET_SOURCE: {
		ECalConfigTargetSource *s = (ECalConfigTargetSource *) t;
		if (s->source)
			g_object_unref (s->source);
		break; }
	case EC_CONFIG_TARGET_PREFS: {
		ECalConfigTargetPrefs *s = (ECalConfigTargetPrefs *) t;
		if (s->settings)
			g_object_unref (s->settings);
		break; }
	}

	((EConfigClass *) e_cal_config_parent_class)->target_free (ec, t);
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
	ECalConfig *self = E_CAL_CONFIG (ec);

	((EConfigClass *) e_cal_config_parent_class)->set_target (ec, t);

	if (t) {
		switch (t->type) {
		case EC_CONFIG_TARGET_SOURCE: {
			ECalConfigTargetSource *s = (ECalConfigTargetSource *) t;

			self->priv->source_changed_id = g_signal_connect (
				s->source, "changed",
				G_CALLBACK (ecp_source_changed), ec);
			break; }
		case EC_CONFIG_TARGET_PREFS: {
			/* ECalConfigTargetPrefs *s = (ECalConfigTargetPrefs *)t; */
			break; }
		}
	}
}

static void
e_cal_config_class_init (ECalConfigClass *class)
{
	EConfigClass *config_class;

	config_class = E_CONFIG_CLASS (class);
	config_class->set_target = ecp_set_target;
	config_class->target_free = ecp_target_free;
}

static void
e_cal_config_init (ECalConfig *cfg)
{
	cfg->priv = e_cal_config_get_instance_private (cfg);
}

ECalConfig *
e_cal_config_new (const gchar *menuid)
{
	ECalConfig *ecp = g_object_new (e_cal_config_get_type (), NULL);
	e_config_construct (&ecp->config, menuid);
	return ecp;
}

ECalConfigTargetSource *
e_cal_config_target_new_source (ECalConfig *ecp,
                                ESource *source)
{
	ECalConfigTargetSource *t;

	t = e_config_target_new (
		&ecp->config, EC_CONFIG_TARGET_SOURCE, sizeof (*t));

	t->source = g_object_ref (source);

	return t;
}

ECalConfigTargetPrefs *
e_cal_config_target_new_prefs (ECalConfig *ecp)
{
	ECalConfigTargetPrefs *t;

	t = e_config_target_new (
		&ecp->config, EC_CONFIG_TARGET_PREFS, sizeof (*t));

	t->settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	return t;
}
