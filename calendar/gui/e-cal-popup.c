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
#include <libedataserverui/e-source-selector.h>

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

static icalproperty *
get_attendee_prop (icalcomponent *icalcomp, const char *address)
{
	
	icalproperty *prop;

	if (!(address && *address))
		return NULL;
	
	for (prop = icalcomponent_get_first_property (icalcomp, ICAL_ATTENDEE_PROPERTY);
			prop;
			prop = icalcomponent_get_next_property (icalcomp, ICAL_ATTENDEE_PROPERTY)) {
		const char *attendee = icalproperty_get_attendee (prop);

		if (g_str_equal (itip_strip_mailto (attendee), address)) {
			return prop;
		}
	}
	return NULL;
}
	
static gboolean 
is_delegated (icalcomponent *icalcomp, char *user_email)
{
	icalproperty *prop;
	icalparameter *param;
	const char *delto = NULL;
	
	prop = get_attendee_prop (icalcomp, user_email);

	if (prop) {
		param = icalproperty_get_first_parameter (prop, ICAL_DELEGATEDTO_PARAMETER);
		delto = icalparameter_get_delegatedto (param);
	} else
		return FALSE;
	
	prop = get_attendee_prop (icalcomp, itip_strip_mailto (delto));	

	if (prop) {
		const char *delfrom;
		icalparameter_partstat	status;

		param = icalproperty_get_first_parameter (prop, ICAL_DELEGATEDFROM_PARAMETER);
		delfrom = icalparameter_get_delegatedfrom (param);
		param = icalproperty_get_first_parameter (prop, ICAL_PARTSTAT_PARAMETER);
		status = icalparameter_get_partstat (param);
		if ((delfrom && *delfrom) && g_str_equal (itip_strip_mailto (delfrom), user_email)
				&& status != ICAL_PARTSTAT_DECLINED)
			return TRUE;
	}

	return FALSE;	
}

static gboolean
needs_to_accept (icalcomponent *icalcomp, char *user_email) 
{
	icalproperty *prop;
	icalparameter *param;
	icalparameter_partstat status;
	const char *delto = NULL;
	
	prop = get_attendee_prop (icalcomp, user_email);

	/* It might be a mailing list */
	if (!prop)	
		return TRUE;
	param = icalproperty_get_first_parameter (prop, ICAL_PARTSTAT_PARAMETER);
	status = icalparameter_get_partstat (param);

	if (status == ICAL_PARTSTAT_ACCEPTED || status == ICAL_PARTSTAT_TENTATIVE)
		return FALSE;

	return TRUE;
}

/**
 * e_cal_popup_target_new_select:
 * @eabp:
 * @model: The calendar model.
 * @events: An array of pointers to ECalModelComponent items.  These
 * items must be copied.  They, and the @events array will be freed by
 * the popup menu automatically.
 * 
 * Create a new selection popup target.
 * 
 * Return value: 
 **/
ECalPopupTargetSelect *
e_cal_popup_target_new_select(ECalPopup *eabp, struct _ECalModel *model, GPtrArray *events)
{
	ECalPopupTargetSelect *t = e_popup_target_new(&eabp->popup, E_CAL_POPUP_TARGET_SELECT, sizeof(*t));
	guint32 mask = ~0;
	ECal *client;
	gboolean read_only, user_org = FALSE;

	/* FIXME: This is duplicated in e-cal-menu */

	t->model = model;
	g_object_ref(t->model);
	t->events = events;
	
	if (t->events->len == 0) {
		client = e_cal_model_get_default_client(t->model);
	} else {
		ECalModelComponent *comp_data = (ECalModelComponent *)t->events->pdata[0];
		ECalComponent *comp;
		char *user_email = NULL;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp));
		user_email = itip_get_comp_attendee (comp, comp_data->client);

		mask &= ~E_CAL_POPUP_SELECT_ANY;
		if (t->events->len == 1)
			mask &= ~E_CAL_POPUP_SELECT_ONE;
		else
			mask &= ~E_CAL_POPUP_SELECT_MANY;

		if (icalcomponent_get_first_property (comp_data->icalcomp, ICAL_URL_PROPERTY))
			mask &= ~E_CAL_POPUP_SELECT_HASURL;

		if (e_cal_util_component_has_recurrences (comp_data->icalcomp))
			mask &= ~E_CAL_POPUP_SELECT_RECURRING;
		else if (e_cal_util_component_is_instance (comp_data->icalcomp))
			mask &= ~E_CAL_POPUP_SELECT_RECURRING;
		else
			mask &= ~E_CAL_POPUP_SELECT_NONRECURRING;

		if (e_cal_util_component_is_instance (comp_data->icalcomp))
			mask &= ~E_CAL_POPUP_SELECT_INSTANCE;

		if (e_cal_util_component_has_attendee (comp_data->icalcomp))
			mask &= ~E_CAL_POPUP_SELECT_MEETING;

		if (e_cal_util_component_has_organizer (comp_data->icalcomp)) {

			if (itip_organizer_is_user (comp, comp_data->client)) {
				mask &= ~E_CAL_POPUP_SELECT_ORGANIZER;
				user_org = TRUE;
			}

		} else {
			/* organiser is synonym for owner in this case */
			mask &= ~(E_CAL_POPUP_SELECT_ORGANIZER|E_CAL_POPUP_SELECT_NOTMEETING);
		}

		client = comp_data->client;

		if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_DELEGATE_SUPPORTED)) {

			if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_DELEGATE_TO_MANY))
				mask &= ~E_CAL_POPUP_SELECT_DELEGATABLE;
			else if (!user_org && !is_delegated (comp_data->icalcomp, user_email))
				mask &= ~E_CAL_POPUP_SELECT_DELEGATABLE;
		}

		if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_HAS_UNACCEPTED_MEETING) &&
				needs_to_accept (comp_data->icalcomp, user_email))
			mask &= ~E_CAL_POPUP_SELECT_ACCEPTABLE;
		
		g_object_unref (comp);
		g_free (user_email);
	}

	e_cal_is_read_only(client, &read_only, NULL);
	if (!read_only)
		mask &= ~E_CAL_POPUP_SELECT_EDITABLE;

	
		
	if (!e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_NO_TASK_ASSIGNMENT)
	    && !e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_NO_CONV_TO_ASSIGN_TASK))
		mask &= ~E_CAL_POPUP_SELECT_ASSIGNABLE;

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
	const char *offline = NULL;

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


	source = e_source_selector_peek_primary_selection (selector);
	/* check for e_target_selector's offline_status property here */
	offline = e_source_get_property (source, "offline");

	if (offline  && strcmp (offline,"1") == 0) { 
		/* set the menu item to Mark Offline - */
		mask &= ~E_CAL_POPUP_SOURCE_NO_OFFLINE;
	}
	else {
		mask &= ~E_CAL_POPUP_SOURCE_OFFLINE;
	}

	t->target.mask = mask;

	return t;
}

/**
 * e_cal_popup_target_new_attachments:
 * @ecp: 
 * @attachments: A list of CalAttachment objects, reffed for
 * the list.  Will be unreff'd once finished with.
 * 
 * Owns the list @attachments and their items after they're passed in.
 * 
 * Return value: 
 **/
ECalPopupTargetAttachments *
e_cal_popup_target_new_attachments(ECalPopup *ecp, GSList *attachments)
{
	ECalPopupTargetAttachments *t = e_popup_target_new(&ecp->popup, E_CAL_POPUP_TARGET_ATTACHMENTS, sizeof(*t));
	guint32 mask = ~0;
	int len = g_slist_length(attachments);

	t->attachments = attachments;
	if (len > 0)
		mask &= ~ E_CAL_POPUP_ATTACHMENTS_MANY;
	if (len == 1)
		mask &= ~ E_CAL_POPUP_ATTACHMENTS_ONE;
	t->target.mask = mask;

	return t;
}

/* ********************************************************************** */
/* Popup menu plugin handler */

/*
<e-plugin
  class="org.gnome.mail.plugin.popup:1.0"
  id="org.gnome.mail.plugin.popup.iteab:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  name="imap"
  description="IMAP4 and IMAP4v1 mail store">
  <hook class="org.gnome.mail.popupMenu:1.0"
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
	{ "meeting", E_CAL_POPUP_SELECT_MEETING },
	{ "assignable", E_CAL_POPUP_SELECT_ASSIGNABLE },
	{ "hasurl", E_CAL_POPUP_SELECT_HASURL },
	{ "delegate", E_CAL_POPUP_SELECT_DELEGATABLE }, 
	{ "accept", E_CAL_POPUP_SELECT_ACCEPTABLE },
	{ 0 }
};

static const EPopupHookTargetMask ecalph_source_masks[] = {
	{ "primary", E_CAL_POPUP_SOURCE_PRIMARY },
	{ "system", E_CAL_POPUP_SOURCE_SYSTEM },
	{ "user", E_CAL_POPUP_SOURCE_USER },
	{ "offline", E_CAL_POPUP_SOURCE_OFFLINE},
	{ "no-offline", E_CAL_POPUP_SOURCE_NO_OFFLINE},	
	{ 0 }
};

static const EPopupHookTargetMask ecalph_attachments_masks[] = {
	{ "one", E_CAL_POPUP_ATTACHMENTS_ONE },
	{ "many", E_CAL_POPUP_ATTACHMENTS_MANY },
	{ 0 }
};

static const EPopupHookTargetMap ecalph_targets[] = {
	{ "select", E_CAL_POPUP_TARGET_SELECT, ecalph_select_masks },
	{ "source", E_CAL_POPUP_TARGET_SOURCE, ecalph_source_masks },
	{ "attachments", E_CAL_POPUP_TARGET_ATTACHMENTS, ecalph_attachments_masks },
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
	((EPluginHookClass *)klass)->id = "org.gnome.evolution.calendar.popup:1.0";

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
