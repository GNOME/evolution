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

static GObjectClass *emp_parent;

struct _EMConfigPrivate {
	gint account_changed_id;
};

static void
emp_init(GObject *o)
{
	EMConfig *emp = (EMConfig *)o;

	emp->priv = g_malloc0(sizeof(*emp->priv));
}

static void
emp_finalise(GObject *o)
{
	struct _EMConfigPrivate *p = ((EMConfig *)o)->priv;

	/* Note we can't be unreffed if a target exists, so the target
	 * will need to be freed first which will clean up any
	 * listeners */

	g_free(p);

	((GObjectClass *)emp_parent)->finalize(o);
}

static void
emp_target_free(EConfig *ep, EConfigTarget *t)
{
	if (ep->target == t) {
		switch (t->type) {
		case EM_CONFIG_TARGET_FOLDER: {
			/*EMConfigTargetFolder *s = (EMConfigTargetFolder *)t;*/
			break; }
		case EM_CONFIG_TARGET_PREFS: {
			/*EMConfigTargetPrefs *s = (EMConfigTargetPrefs *)t;*/
			break; }
		case EM_CONFIG_TARGET_ACCOUNT: {
			EMConfigTargetAccount *s = (EMConfigTargetAccount *)t;

			if (((EMConfig *)ep)->priv->account_changed_id) {
				g_signal_handler_disconnect(s->account, ((EMConfig *)ep)->priv->account_changed_id);
				((EMConfig *)ep)->priv->account_changed_id = 0;
			}
			break; }
		}
	}

	switch (t->type) {
	case EM_CONFIG_TARGET_FOLDER: {
		EMConfigTargetFolder *s = (EMConfigTargetFolder *)t;

		g_free(s->uri);
		g_object_unref (s->folder);
		break; }
	case EM_CONFIG_TARGET_PREFS: {
		EMConfigTargetPrefs *s = (EMConfigTargetPrefs *)t;

		if (s->gconf)
			g_object_unref(s->gconf);
		break; }
	case EM_CONFIG_TARGET_ACCOUNT: {
		EMConfigTargetAccount *s = (EMConfigTargetAccount *)t;

		g_object_unref(s->account);
		break; }
	}

	((EConfigClass *)emp_parent)->target_free(ep, t);
}

static void
emp_account_changed(struct _EAccount *ea, gint id, EMConfig *emc)
{
	e_config_target_changed((EConfig *)emc, E_CONFIG_TARGET_CHANGED_STATE);
}

static void
emp_set_target(EConfig *ep, EConfigTarget *t)
{
	((EConfigClass *)emp_parent)->set_target(ep, t);

	if (t) {
		switch (t->type) {
		case EM_CONFIG_TARGET_FOLDER: {
			/*EMConfigTargetFolder *s = (EMConfigTargetFolder *)t;*/
			break; }
		case EM_CONFIG_TARGET_PREFS: {
			/*EMConfigTargetPrefs *s = (EMConfigTargetPrefs *)t;*/
			break; }
		case EM_CONFIG_TARGET_ACCOUNT: {
			EMConfigTargetAccount *s = (EMConfigTargetAccount *)t;

			((EMConfig *)ep)->priv->account_changed_id = g_signal_connect(s->account, "changed", G_CALLBACK(emp_account_changed), ep);
			break; }
		}
	}
}

static void
emp_class_init(GObjectClass *klass)
{
	klass->finalize = emp_finalise;
	((EConfigClass *)klass)->set_target = emp_set_target;
	((EConfigClass *)klass)->target_free = emp_target_free;
}

GType
em_config_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EMConfigClass),
			NULL, NULL,
			(GClassInitFunc)emp_class_init,
			NULL, NULL,
			sizeof(EMConfig), 0,
			(GInstanceInitFunc)emp_init
		};
		emp_parent = g_type_class_ref(e_config_get_type());
		type = g_type_register_static(e_config_get_type(), "EMConfig", &info, 0);
	}

	return type;
}

EMConfig *em_config_new(gint type, const gchar *menuid)
{
	EMConfig *emp = g_object_new(em_config_get_type(), NULL);

	e_config_construct(&emp->config, type, menuid);

	return emp;
}

EMConfigTargetFolder *
em_config_target_new_folder(EMConfig *emp, CamelFolder *folder, const gchar *uri)
{
	EMConfigTargetFolder *t = e_config_target_new(&emp->config, EM_CONFIG_TARGET_FOLDER, sizeof(*t));

	t->uri = g_strdup(uri);
	t->folder = folder;
	g_object_ref (folder);

	return t;
}

EMConfigTargetPrefs *
em_config_target_new_prefs(EMConfig *emp, struct _GConfClient *gconf)
{
	EMConfigTargetPrefs *t = e_config_target_new(&emp->config, EM_CONFIG_TARGET_PREFS, sizeof(*t));

	t->gconf = gconf;
	if (gconf)
		g_object_ref(gconf);

	return t;
}

EMConfigTargetAccount *
em_config_target_new_account(EMConfig *emp, struct _EAccount *account)
{
	EMConfigTargetAccount *t = e_config_target_new(&emp->config, EM_CONFIG_TARGET_ACCOUNT, sizeof(*t));

	t->account = account;
	g_object_ref(account);

	return t;
}
