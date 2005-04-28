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

#include "e-profile-event.h"
#include "libedataserver/e-msgport.h"

static GObjectClass *eme_parent;
static EProfileEvent *e_profile_event;

static void
eme_init(GObject *o)
{
	/*EProfileEvent *eme = (EProfileEvent *)o; */
}

static void
eme_finalise(GObject *o)
{
	((GObjectClass *)eme_parent)->finalize(o);
}

static void
eme_target_free(EEvent *ep, EEventTarget *t)
{
	switch (t->type) {
	case E_PROFILE_EVENT_TARGET: {
		EProfileEventTarget *s = (EProfileEventTarget *)t;

		g_free(s->id);
		g_free(s->uid);
		break; }
	}

	((EEventClass *)eme_parent)->target_free(ep, t);
}

static void
eme_class_init(GObjectClass *klass)
{
	klass->finalize = eme_finalise;
	((EEventClass *)klass)->target_free = eme_target_free;
}

GType
e_profile_event_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EProfileEventClass),
			NULL, NULL,
			(GClassInitFunc)eme_class_init,
			NULL, NULL,
			sizeof(EProfileEvent), 0,
			(GInstanceInitFunc)eme_init
		};
		eme_parent = g_type_class_ref(e_event_get_type());
		type = g_type_register_static(e_event_get_type(), "EProfileEvent", &info, 0);
	}

	return type;
}

/**
 * e_profile_event_peek:
 * @void: 
 * 
 * Get the singular instance of the profile event handler.
 * 
 * Return value: 
 **/
EProfileEvent *e_profile_event_peek(void)
{
	if (e_profile_event == NULL) {
		e_profile_event = g_object_new(e_profile_event_get_type(), 0);
		e_event_construct(&e_profile_event->popup, "org.gnome.evolution.profile.events");
	}

	return e_profile_event;
}

EProfileEventTarget *
e_profile_event_target_new(EProfileEvent *eme, const char *id, const char *uid, guint32 flags)
{
	EProfileEventTarget *t = e_event_target_new(&eme->popup, E_PROFILE_EVENT_TARGET, sizeof(*t));

	t->id = g_strdup(id);
	t->uid = g_strdup(uid);
	t->target.mask = ~flags;
	gettimeofday(&t->tv, NULL);

	return t;
}

void
e_profile_event_emit(const char *id, const char *uid, guint32 flags)
{
#ifdef ENABLE_PROFILING
	EProfileEvent *epe = e_profile_event_peek();
	EProfileEventTarget *t = e_profile_event_target_new(epe, id, uid, flags);

	e_event_emit((EEvent *)epe, "event", (EEventTarget *)t);
#endif
}

/* ********************************************************************** */

static void *emeh_parent_class;
#define emeh ((EProfileEventHook *)eph)

static const EEventHookTargetMask emeh_profile_masks[] = {
	{ "start", E_PROFILE_EVENT_START },
	{ "end", E_PROFILE_EVENT_END },
	{ "cancel", E_PROFILE_EVENT_CANCEL },
	{ 0 }
};

static const EEventHookTargetMap emeh_targets[] = {
	{ "event", E_PROFILE_EVENT_TARGET, emeh_profile_masks },
	{ 0 }
};

static void
emeh_finalise(GObject *o)
{
	/*EPluginHook *eph = (EPluginHook *)o;*/

	((GObjectClass *)emeh_parent_class)->finalize(o);
}

static void
emeh_class_init(EPluginHookClass *klass)
{
	int i;

	((GObjectClass *)klass)->finalize = emeh_finalise;
	((EPluginHookClass *)klass)->id = "org.gnome.evolution.profile.events:1.0";

	for (i=0;emeh_targets[i].type;i++)
		e_event_hook_class_add_target_map((EEventHookClass *)klass, &emeh_targets[i]);

	((EEventHookClass *)klass)->event = (EEvent *)e_profile_event_peek();
}

GType
e_profile_event_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(EProfileEventHookClass), NULL, NULL, (GClassInitFunc) emeh_class_init, NULL, NULL,
			sizeof(EProfileEventHook), 0, (GInstanceInitFunc) NULL,
		};

		emeh_parent_class = g_type_class_ref(e_event_hook_get_type());
		type = g_type_register_static(e_event_hook_get_type(), "EProfileEventHook", &info, 0);
	}
	
	return type;
}
