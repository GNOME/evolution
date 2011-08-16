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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "em-config.h"
#include "em-utils.h"
#include "em-composer-utils.h"

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <e-util/e-util.h>

G_DEFINE_TYPE (EMConfig, em_config, E_TYPE_CONFIG)

struct _EMConfigPrivate {
	gint account_changed_id;
};

static void
emp_account_changed (struct _EAccount *ea, gint id, EMConfig *emc)
{
	e_config_target_changed ((EConfig *) emc, E_CONFIG_TARGET_CHANGED_STATE);
}

static void
em_config_finalize (GObject *object)
{
	/* Note we can't be unreffed if a target exists, so the target
	 * will need to be freed first which will clean up any
	 * listeners */

	g_free (((EMConfig *) object)->priv);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_config_parent_class)->finalize (object);
}

static void
em_config_set_target (EConfig *ep,
                      EConfigTarget *t)
{
	/* Chain up to parent's set_target() method. */
	E_CONFIG_CLASS (em_config_parent_class)->set_target (ep, t);

	if (t) {
		switch (t->type) {
		case EM_CONFIG_TARGET_FOLDER: {
			/*EMConfigTargetFolder *s = (EMConfigTargetFolder *)t;*/
			break; }
		case EM_CONFIG_TARGET_PREFS: {
			/*EMConfigTargetPrefs *s = (EMConfigTargetPrefs *)t;*/
			break; }
		case EM_CONFIG_TARGET_ACCOUNT: {
			EMConfigTargetAccount *s = (EMConfigTargetAccount *) t;
			EMConfig *config = (EMConfig *) ep;

			config->priv->account_changed_id = g_signal_connect (
				s->modified_account, "changed",
				G_CALLBACK (emp_account_changed), ep);

			em_config_target_new_account_update_settings (ep, s, s->settings);
			break; }
		}
	}
}

static void
em_config_target_free (EConfig *ep,
                       EConfigTarget *t)
{
	if (ep->target == t) {
		switch (t->type) {
		case EM_CONFIG_TARGET_FOLDER:
			break;
		case EM_CONFIG_TARGET_PREFS:
			break;
		case EM_CONFIG_TARGET_ACCOUNT: {
			EMConfigTargetAccount *s = (EMConfigTargetAccount *) t;
			EMConfig *config = (EMConfig *) ep;

			if (config->priv->account_changed_id > 0) {
				g_signal_handler_disconnect (
					s->modified_account,
					config->priv->account_changed_id);
				config->priv->account_changed_id = 0;
			}

			em_config_target_new_account_update_settings (ep, s, NULL);
			break; }
		}
	}

	switch (t->type) {
	case EM_CONFIG_TARGET_FOLDER: {
		EMConfigTargetFolder *s = (EMConfigTargetFolder *) t;

		g_object_unref (s->folder);
		break; }
	case EM_CONFIG_TARGET_PREFS: {
		EMConfigTargetPrefs *s = (EMConfigTargetPrefs *) t;

		if (s->gconf)
			g_object_unref (s->gconf);
		break; }
	case EM_CONFIG_TARGET_ACCOUNT: {
		EMConfigTargetAccount *s = (EMConfigTargetAccount *) t;

		if (s->original_account != NULL)
			g_object_unref (s->original_account);
		if (s->modified_account != NULL)
			g_object_unref (s->modified_account);
		if (s->settings != NULL)
			g_object_unref (s->settings);
		break; }
	}

	/* Chain up to parent's target_free() method. */
	E_CONFIG_CLASS (em_config_parent_class)->target_free (ep, t);
}

static void
em_config_class_init (EMConfigClass *class)
{
	GObjectClass *object_class;
	EConfigClass *config_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = em_config_finalize;

	config_class = E_CONFIG_CLASS (class);
	config_class->set_target = em_config_set_target;
	config_class->target_free = em_config_target_free;
}

static void
em_config_init (EMConfig *emp)
{
	emp->priv = g_malloc0 (sizeof (*emp->priv));
}

EMConfig *
em_config_new (gint type,
               const gchar *menuid)
{
	EMConfig *emp;

	emp = g_object_new (em_config_get_type (), NULL);
	e_config_construct (&emp->config, type, menuid);

	return emp;
}

EMConfigTargetFolder *
em_config_target_new_folder (EMConfig *emp,
                             CamelFolder *folder)
{
	EMConfigTargetFolder *t;

	t = e_config_target_new (
		&emp->config, EM_CONFIG_TARGET_FOLDER, sizeof (*t));

	t->folder = g_object_ref (folder);

	return t;
}

EMConfigTargetPrefs *
em_config_target_new_prefs (EMConfig *emp,
                            GConfClient *gconf)
{
	EMConfigTargetPrefs *t;

	t = e_config_target_new (
		&emp->config, EM_CONFIG_TARGET_PREFS, sizeof (*t));

	if (GCONF_IS_CLIENT (gconf))
		t->gconf = g_object_ref (gconf);
	else
		t->gconf = NULL;

	return t;
}

EMConfigTargetAccount *
em_config_target_new_account (EMConfig *emp,
                              EAccount *original_account,
                              EAccount *modified_account,
                              CamelSettings *settings)
{
	EMConfigTargetAccount *t;

	t = e_config_target_new (
		&emp->config, EM_CONFIG_TARGET_ACCOUNT, sizeof (*t));

	if (original_account != NULL)
		t->original_account = g_object_ref (original_account);
	else
		t->original_account = NULL;

	if (modified_account != NULL)
		t->modified_account = g_object_ref (modified_account);
	else
		t->modified_account = NULL;

	if (settings != NULL)
		t->settings = g_object_ref (settings);
	else
		t->settings = NULL;

	return t;
}

void
em_config_target_new_account_update_settings (EConfig *ep, EMConfigTargetAccount *target, CamelSettings *settings)
{
	g_return_if_fail (ep != NULL);
	g_return_if_fail (target != NULL);

	if (settings)
		g_object_ref (settings);

	if (target->settings != NULL) {
		g_signal_handlers_disconnect_by_func (target->settings, G_CALLBACK (emp_account_changed), ep);
		g_object_unref (target->settings);
	}

	target->settings = settings;

	if (target->settings != NULL)
		g_signal_connect (
			target->settings, "notify",
			G_CALLBACK (emp_account_changed), ep);
}
