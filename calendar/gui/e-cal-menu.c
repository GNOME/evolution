/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "e-cal-menu.h"
#include "gui/e-cal-model.h"
#include "itip-utils.h"

static void ecalm_standard_menu_factory(EMenu *emp, void *data);

static GObjectClass *ecalm_parent;

static void
ecalm_init(GObject *o)
{
	/*ECalMenu *emp = (ECalMenu *)o; */
}

static void
ecalm_finalise(GObject *o)
{
	((GObjectClass *)ecalm_parent)->finalize(o);
}

static void
ecalm_target_free(EMenu *ep, EMenuTarget *t)
{
	switch (t->type) {
	case E_CAL_MENU_TARGET_SELECT: {
		ECalMenuTargetSelect *s = (ECalMenuTargetSelect *)t;
		int i;

		for (i=0;i<s->events->len;i++)
			e_cal_model_free_component_data(s->events->pdata[i]);
		g_ptr_array_free(s->events, TRUE);
		g_object_unref(s->model);
		break; }
	}

	((EMenuClass *)ecalm_parent)->target_free(ep, t);
}

static void
ecalm_class_init(GObjectClass *klass)
{
	klass->finalize = ecalm_finalise;
	((EMenuClass *)klass)->target_free = ecalm_target_free;

	e_menu_class_add_factory((EMenuClass *)klass, NULL, (EMenuFactoryFunc)ecalm_standard_menu_factory, NULL);
}

GType
e_cal_menu_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(ECalMenuClass),
			NULL, NULL,
			(GClassInitFunc)ecalm_class_init,
			NULL, NULL,
			sizeof(ECalMenu), 0,
			(GInstanceInitFunc)ecalm_init
		};
		ecalm_parent = g_type_class_ref(e_menu_get_type());
		type = g_type_register_static(e_menu_get_type(), "ECalMenu", &info, 0);
	}

	return type;
}

ECalMenu *e_cal_menu_new(const char *menuid)
{
	ECalMenu *emp = g_object_new(e_cal_menu_get_type(), 0);

	e_menu_construct(&emp->menu, menuid);

	return emp;
}

/**
 * e_cal_menu_target_new_select:
 * @folder: The selection will ref this for the life of it.
 * @folder_uri: 
 * @uids: The selection will free this when done with it.
 * 
 * Create a new selection popup target.
 * 
 * Return value: 
 **/
ECalMenuTargetSelect *
e_cal_menu_target_new_select(ECalMenu *eabp, struct _ECalModel *model, GPtrArray *events)
{
	ECalMenuTargetSelect *t = e_menu_target_new(&eabp->menu, E_CAL_MENU_TARGET_SELECT, sizeof(*t));
	guint32 mask = ~0;
	ECal *client;
	gboolean read_only;

	/* FIXME: This is duplicated in e-cal-popup */

	t->model = model;
	g_object_ref(t->model);
	t->events = events;

	if (t->events->len == 0) {
		client = e_cal_model_get_default_client(t->model);
	} else {
		ECalModelComponent *comp_data = (ECalModelComponent *)t->events->pdata[0];

		mask &= ~E_CAL_MENU_SELECT_ANY;
		if (t->events->len == 1)
			mask &= ~E_CAL_MENU_SELECT_ONE;
		else
			mask &= ~E_CAL_MENU_SELECT_MANY;

		if (icalcomponent_get_first_property (comp_data->icalcomp, ICAL_URL_PROPERTY))
			mask &= ~E_CAL_MENU_SELECT_HASURL;

		if (e_cal_util_component_has_recurrences (comp_data->icalcomp))
			mask &= ~E_CAL_MENU_SELECT_RECURRING;
		else
			mask &= ~E_CAL_MENU_SELECT_NONRECURRING;

		if (e_cal_util_component_is_instance (comp_data->icalcomp))
			mask &= ~E_CAL_MENU_SELECT_INSTANCE;

		if (e_cal_util_component_has_organizer (comp_data->icalcomp)) {
			ECalComponent *comp;

			comp = e_cal_component_new ();
			e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp));
			if (!itip_organizer_is_user (comp, comp_data->client))
				mask &= ~E_CAL_MENU_SELECT_ORGANIZER;

			g_object_unref (comp);
		} else {
			/* organiser is synonym for owner in this case */
			mask &= ~(E_CAL_MENU_SELECT_ORGANIZER|E_CAL_MENU_SELECT_NOTMEETING);
		}

		client = comp_data->client;
	}

	if (client) {
		e_cal_is_read_only(client, &read_only, NULL);
		if (!read_only)
			mask &= ~E_CAL_MENU_SELECT_EDITABLE;

		if (!e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_NO_TASK_ASSIGNMENT)
		    && !e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_NO_CONV_TO_ASSIGN_TASK))
			mask &= ~E_CAL_MENU_SELECT_ASSIGNABLE;
	}

	/* This bit isn't implemented ... */
	mask &= ~E_CAL_MENU_SELECT_NOTEDITING;

	t->target.mask = mask;

	return t;
}

static void
ecalm_standard_menu_factory(EMenu *emp, void *data)
{
	/* noop */
}

/* ********************************************************************** */

/* menu plugin handler */

/*
<e-plugin
  class="org.gnome.mail.plugin.popup:1.0"
  id="org.gnome.mail.plugin.popup.item:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  name="imap"
  description="IMAP4 and IMAP4v1 mail store">
  <hook class="org.gnome.mail.popupMenu:1.0"
        handler="HandlePopup">
  <menu id="any" target="select">
   <item
    type="item|toggle|radio|image|submenu|bar"
    active
    path="foo/bar"
    label="label"
    icon="foo"
    mask="select_one"
    activate="ecalm_view_emacs"/>
  </menu>
  </extension>

*/

static void *ecalph_parent_class;
#define ecalph ((ECalMenuHook *)eph)

static const EMenuHookTargetMask ecalph_select_masks[] = {
	{ "one", E_CAL_MENU_SELECT_ONE },
	{ "many", E_CAL_MENU_SELECT_MANY },
	{ "editable", E_CAL_MENU_SELECT_EDITABLE },
	{ "recurring", E_CAL_MENU_SELECT_RECURRING },
	{ "non-recurring", E_CAL_MENU_SELECT_NONRECURRING },
	{ "instance", E_CAL_MENU_SELECT_INSTANCE },
	{ "organizer", E_CAL_MENU_SELECT_ORGANIZER },
	{ "not-editing", E_CAL_MENU_SELECT_NOTEDITING },
	{ "not-meeting", E_CAL_MENU_SELECT_NOTMEETING },
	{ "assignable", E_CAL_MENU_SELECT_ASSIGNABLE },
	{ "hasurl", E_CAL_MENU_SELECT_HASURL },
	{ 0 }
};

static const EMenuHookTargetMap ecalph_targets[] = {
	{ "select", E_CAL_MENU_TARGET_SELECT, ecalph_select_masks },
	{ 0 }
};

static void
ecalph_finalise(GObject *o)
{
	/*EPluginHook *eph = (EPluginHook *)o;*/

	((GObjectClass *)ecalph_parent_class)->finalize(o);
}

static void
ecalph_class_init(EPluginHookClass *klass)
{
	int i;

	((GObjectClass *)klass)->finalize = ecalph_finalise;
	((EPluginHookClass *)klass)->id = "org.gnome.evolution.calendar.bonobomenu:1.0";

	for (i=0;ecalph_targets[i].type;i++)
		e_menu_hook_class_add_target_map((EMenuHookClass *)klass, &ecalph_targets[i]);

	/* FIXME: leaks parent set class? */
	((EMenuHookClass *)klass)->menu_class = g_type_class_ref(e_cal_menu_get_type());
}

GType
e_cal_menu_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(ECalMenuHookClass), NULL, NULL, (GClassInitFunc) ecalph_class_init, NULL, NULL,
			sizeof(ECalMenuHook), 0, (GInstanceInitFunc) NULL,
		};

		ecalph_parent_class = g_type_class_ref(e_menu_hook_get_type());
		type = g_type_register_static(e_menu_hook_get_type(), "ECalMenuHook", &info, 0);
	}
	
	return type;
}
