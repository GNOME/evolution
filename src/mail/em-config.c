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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "em-config.h"
#include "em-utils.h"
#include "em-composer-utils.h"

#include <e-util/e-util.h>

G_DEFINE_TYPE (EMConfig, em_config, E_TYPE_CONFIG)

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
		case EM_CONFIG_TARGET_SETTINGS: {
			EMConfigTargetSettings *s = (EMConfigTargetSettings *) t;

			em_config_target_update_settings (
				ep, s,
				s->email_address,
				s->storage_protocol,
				s->storage_settings,
				s->transport_protocol,
				s->transport_settings);
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
		case EM_CONFIG_TARGET_SETTINGS: {
			EMConfigTargetSettings *s = (EMConfigTargetSettings *) t;

			em_config_target_update_settings (
				ep, s, NULL, NULL, NULL, NULL, NULL);
			break; }
		}
	}

	switch (t->type) {
	case EM_CONFIG_TARGET_FOLDER: {
		EMConfigTargetFolder *s = (EMConfigTargetFolder *) t;

		g_object_unref (s->folder);
		break; }
	case EM_CONFIG_TARGET_PREFS: {
		/* EMConfigTargetPrefs *s = (EMConfigTargetPrefs *) t; */
		break; }
	case EM_CONFIG_TARGET_SETTINGS: {
		EMConfigTargetSettings *s = (EMConfigTargetSettings *) t;

		g_free (s->email_address);
		if (s->storage_settings != NULL)
			g_object_unref (s->storage_settings);
		if (s->transport_settings != NULL)
			g_object_unref (s->transport_settings);
		break; }
	}

	/* Chain up to parent's target_free() method. */
	E_CONFIG_CLASS (em_config_parent_class)->target_free (ep, t);
}

static void
em_config_class_init (EMConfigClass *class)
{
	EConfigClass *config_class;

	config_class = E_CONFIG_CLASS (class);
	config_class->set_target = em_config_set_target;
	config_class->target_free = em_config_target_free;
}

static void
em_config_init (EMConfig *emp)
{
}

EMConfig *
em_config_new (const gchar *menuid)
{
	EMConfig *emp;

	emp = g_object_new (EM_TYPE_CONFIG, NULL);
	e_config_construct (&emp->config, menuid);

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
em_config_target_new_prefs (EMConfig *emp)
{
	EMConfigTargetPrefs *t;

	t = e_config_target_new (
		&emp->config, EM_CONFIG_TARGET_PREFS, sizeof (*t));

	return t;
}

EMConfigTargetSettings *
em_config_target_new_settings (EMConfig *emp,
                               const gchar *email_address,
                               const gchar *storage_protocol,
                               CamelSettings *storage_settings,
                               const gchar *transport_protocol,
                               CamelSettings *transport_settings)
{
	EMConfigTargetSettings *target;

	target = e_config_target_new (
		&emp->config, EM_CONFIG_TARGET_SETTINGS, sizeof (*target));

	if (storage_protocol != NULL)
		storage_protocol = g_intern_string (storage_protocol);

	if (storage_settings != NULL)
		g_object_ref (storage_settings);

	if (transport_protocol != NULL)
		transport_protocol = g_intern_string (transport_protocol);

	if (transport_settings != NULL)
		g_object_ref (transport_settings);

	target->email_address = g_strdup (email_address);

	target->storage_protocol = storage_protocol;
	target->storage_settings = storage_settings;

	target->transport_protocol = transport_protocol;
	target->transport_settings = transport_settings;

	return target;
}

void
em_config_target_update_settings (EConfig *ep,
                                  EMConfigTargetSettings *target,
                                  const gchar *email_address,
                                  const gchar *storage_protocol,
                                  CamelSettings *storage_settings,
                                  const gchar *transport_protocol,
                                  CamelSettings *transport_settings)
{
	gchar *tmp;

	g_return_if_fail (ep != NULL);
	g_return_if_fail (target != NULL);

	if (storage_protocol != NULL)
		storage_protocol = g_intern_string (storage_protocol);

	if (storage_settings != NULL)
		g_object_ref (storage_settings);

	if (transport_protocol != NULL)
		transport_protocol = g_intern_string (transport_protocol);

	if (transport_settings != NULL)
		g_object_ref (transport_settings);

	if (target->storage_settings != NULL)
		g_object_unref (target->storage_settings);

	if (target->transport_settings != NULL)
		g_object_unref (target->transport_settings);

	/* the pointers can be same, thus avoid use-after-free */
	tmp = g_strdup (email_address);
	g_free (target->email_address);
	target->email_address = tmp;

	target->storage_protocol = storage_protocol;
	target->storage_settings = storage_settings;

	target->transport_protocol = transport_protocol;
	target->transport_settings = transport_settings;
}
