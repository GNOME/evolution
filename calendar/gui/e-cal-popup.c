/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#include "e-cal-popup.h"
#include "widgets/misc/e-source-selector.h"

#include "gui/e-calendar-view.h"
#include "gui/e-cal-model.h"
#include "itip-utils.h"

static GObjectClass *ecalp_parent;

static void
ecalp_init(GObject *o)
{
	/*ECalPopup *eabp = (ECalPopup *)o; */
}

static void
ecalp_finalise(GObject *o)
{
	((GObjectClass *)ecalp_parent)->finalize(o);
}

static void
ecalp_target_free(EPopup *ep, EPopupTarget *t)
{
	switch (t->type) {
	case E_CAL_POPUP_TARGET_SELECT: {
		ECalPopupTargetSelect *s = (ECalPopupTargetSelect *)t;
		int i;

		for (i=0;i<s->events->len;i++)
			e_cal_model_free_component_data(s->events->pdata[i]);
		g_ptr_array_free(s->events, TRUE);
		g_object_unref(s->model);
		break; }
	case E_CAL_POPUP_TARGET_SOURCE: {
		ECalPopupTargetSource *s = (ECalPopupTargetSource *)t;

		g_object_unref(s->selector);
		break; }
	}

	((EPopupClass *)ecalp_parent)->target_free(ep, t);
}

static void
ecalp_class_init(GObjectClass *klass)
{
	klass->finalize = ecalp_finalise;
	((EPopupClass *)klass)->target_free = ecalp_target_free;
}

GType
e_cal_popup_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(ECalPopupClass),
			NULL, NULL,
			(GClassInitFunc)ecalp_class_init,
			NULL, NULL,
			sizeof(ECalPopup), 0,
			(GInstanceInitFunc)ecalp_init
		};
		ecalp_parent = g_type_class_ref(e_popup_get_type());
		type = g_type_register_static(e_popup_get_type(), "ECalPopup", &info, 0);
	}

	return type;
}

ECalPopup *e_cal_popup_new(const char *menuid)
{
	ECalPopup *eabp = g_object_new(e_cal_popup_get_type(), 0);

	e_popup_construct(&eabp->popup, menuid);

	return eabp;
}

/**
 * e_cal_popup_target_new_select:
 * 
 * Create a new selection popup target.
 * 
 * Return value: 
 **/
ECalPopupTargetSelect *
e_cal_popup_target_new_select(ECalPopup *eabp, ECalendarView *view)
{
	ECalPopupTargetSelect *t = e_popup_target_new(&eabp->popup, E_CAL_POPUP_TARGET_SELECT, sizeof(*t));
	guint32 mask = ~0;
	GList *events, *l;
	ECal *client;
	gboolean read_only;

	t->model = e_calendar_view_get_model(view);
	g_object_ref(t->model);
	t->events = g_ptr_array_new();
	l = events = e_calendar_view_get_selected_events(view);
	for (l=events;l;l=g_list_next(l)) {
		ECalendarViewEvent *event = l->data;

		if (event)
			g_ptr_array_add(t->events, e_cal_model_copy_component_data(event->comp_data));
	}

	/* In reality this is only ever called with a single event or none */

	if (t->events->len == 0) {
		client = e_cal_model_get_default_client(t->model);
	} else {
		ECalendarViewEvent *event = (ECalendarViewEvent *)events->data;

		mask &= ~E_CAL_POPUP_SELECT_MANY;
		if (events->next == NULL)
			mask &= ~E_CAL_POPUP_SELECT_ONE;

		if (e_cal_util_component_has_recurrences (event->comp_data->icalcomp))
			mask &= ~E_CAL_POPUP_SELECT_RECURRING;
		else
			mask &= ~E_CAL_POPUP_SELECT_NONRECURRING;

		if (e_cal_util_component_is_instance (event->comp_data->icalcomp))
			mask &= ~E_CAL_POPUP_SELECT_INSTANCE;

		if (e_cal_util_component_has_organizer (event->comp_data->icalcomp)) {
			ECalComponent *comp;

			comp = e_cal_component_new ();
			e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
			if (!itip_organizer_is_user (comp, event->comp_data->client))
				mask &= ~E_CAL_POPUP_SELECT_ORGANIZER;

			g_object_unref (comp);
		} else {
			/* organiser is synonym for owner in this case */
			mask &= ~(E_CAL_POPUP_SELECT_ORGANIZER|E_CAL_POPUP_SELECT_NOTMEETING);
		}

		client = event->comp_data->client;
	}

	g_list_free(events);

	e_cal_is_read_only(client, &read_only, NULL);
	if (!read_only)
		mask &= ~E_CAL_POPUP_SELECT_EDITABLE;

	/* This bit isn't implemented ... */
	mask &= ~E_CAL_POPUP_SELECT_NOTEDITING;

	t->target.mask = mask;

	return t;
}

ECalPopupTargetSource *
e_cal_popup_target_new_source(ECalPopup *eabp, ESourceSelector *selector)
{
	ECalPopupTargetSource *t = e_popup_target_new(&eabp->popup, E_CAL_POPUP_TARGET_SOURCE, sizeof(*t));
	guint32 mask = ~0;
	const char *source_uri;
	ESource *source;

	/* TODO: this is duplicated for addressbook too */

	t->selector = selector;
	g_object_ref(selector);

	/* TODO: perhaps we need to copy this so it doesn't change during the lifecycle */
	source = e_source_selector_peek_primary_selection(selector);
	if (source)
		mask &= ~E_CAL_POPUP_SOURCE_PRIMARY;

	/* FIXME Gross hack, should have a property or something */
	source_uri = e_source_peek_relative_uri(source);
	if (source_uri && !strcmp("system", source_uri))
		mask &= ~E_CAL_POPUP_SOURCE_SYSTEM;
	else
		mask &= ~E_CAL_POPUP_SOURCE_USER;

	t->target.mask = mask;

	return t;
}

/* ********************************************************************** */
/* Popup menu plugin handler */

/*
<e-plugin
  class="com.ximian.mail.plugin.popup:1.0"
  id="com.ximian.mail.plugin.popup.iteab:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  name="imap"
  description="IMAP4 and IMAP4v1 mail store">
  <hook class="com.ximian.mail.popupMenu:1.0"
        handler="HandlePopup">
  <menu id="any" target="select">
   <iteab
    type="iteab|toggle|radio|image|submenu|bar"
    active
    path="foo/bar"
    label="label"
    icon="foo"
    mask="select_one"
    activate="ecalp_view_eabacs"/>
  </menu>
  </extension>

*/

static void *ecalph_parent_class;
#define ecalph ((ECalPopupHook *)eph)

static const EPopupHookTargetMask ecalph_select_masks[] = {
	{ "one", E_CAL_POPUP_SELECT_ONE },
	{ "many", E_CAL_POPUP_SELECT_MANY },
	{ "editable", E_CAL_POPUP_SELECT_EDITABLE },
	{ "recurring", E_CAL_POPUP_SELECT_RECURRING },
	{ "non-recurring", E_CAL_POPUP_SELECT_NONRECURRING },
	{ "instance", E_CAL_POPUP_SELECT_INSTANCE },
	{ "organizer", E_CAL_POPUP_SELECT_ORGANIZER },
	{ "not-editing", E_CAL_POPUP_SELECT_NOTEDITING },
	{ "not-meeting", E_CAL_POPUP_SELECT_NOTMEETING },
	{ 0 }
};

static const EPopupHookTargetMask ecalph_source_masks[] = {
	{ "primary", E_CAL_POPUP_SOURCE_PRIMARY },
	{ "system", E_CAL_POPUP_SOURCE_SYSTEM },
	{ "user", E_CAL_POPUP_SOURCE_USER },
	{ 0 }
};

static const EPopupHookTargetMap ecalph_targets[] = {
	{ "select", E_CAL_POPUP_TARGET_SELECT, ecalph_select_masks },
	{ "source", E_CAL_POPUP_TARGET_SOURCE, ecalph_source_masks },
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
	((EPluginHookClass *)klass)->id = "com.ximian.evolution.addressbook.popup:1.0";

	for (i=0;ecalph_targets[i].type;i++)
		e_popup_hook_class_add_target_map((EPopupHookClass *)klass, &ecalph_targets[i]);

	((EPopupHookClass *)klass)->popup_class = g_type_class_ref(e_cal_popup_get_type());
}

GType
e_cal_popup_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(ECalPopupHookClass), NULL, NULL, (GClassInitFunc) ecalph_class_init, NULL, NULL,
			sizeof(ECalPopupHook), 0, (GInstanceInitFunc) NULL,
		};

		ecalph_parent_class = g_type_class_ref(e_popup_hook_get_type());
		type = g_type_register_static(e_popup_hook_get_type(), "ECalPopupHook", &info, 0);
	}
	
	return type;
}
