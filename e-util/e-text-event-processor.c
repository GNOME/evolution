/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-text-event-processor.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include "e-i18n.h"
#include "e-marshal.h"
#include "e-text-event-processor.h"
#include "e-util.h"

static void e_text_event_processor_init		(ETextEventProcessor		 *card);
static void e_text_event_processor_class_init	(ETextEventProcessorClass	 *klass);

static void e_text_event_processor_set_property (GObject *object,
						 guint prop_id,
						 const GValue *value,
						 GParamSpec *pspec);
static void e_text_event_processor_get_property (GObject *object,
						 guint prop_id,
						 GValue *value,
						 GParamSpec *pspec);

#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class = NULL;

/* The arguments we take */
enum {
	PROP_0,
	PROP_ALLOW_NEWLINES
};

enum {
	E_TEP_EVENT,
	E_TEP_LAST_SIGNAL
};

static guint e_tep_signals[E_TEP_LAST_SIGNAL] = { 0 };

E_MAKE_TYPE (e_text_event_processor,
	     "ETextEventProcessor",
	     ETextEventProcessor,
	     e_text_event_processor_class_init,
	     e_text_event_processor_init,
	     PARENT_TYPE)

static void
e_text_event_processor_class_init (ETextEventProcessorClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass*) klass;

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->set_property = e_text_event_processor_set_property;
	object_class->get_property = e_text_event_processor_get_property;

	e_tep_signals[E_TEP_EVENT] =
		g_signal_new ("command",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETextEventProcessorClass, command),
			      NULL, NULL,
			      e_marshal_NONE__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	g_object_class_install_property (object_class, PROP_ALLOW_NEWLINES,
					 g_param_spec_boolean ("allow_newlines",
							       _( "Allow newlines" ),
							       _( "Allow newlines" ),
							       FALSE,
							       G_PARAM_READWRITE));

	klass->event = NULL;
	klass->command = NULL;

}

static void
e_text_event_processor_init (ETextEventProcessor *tep)
{
	tep->allow_newlines = TRUE;
}

gint
e_text_event_processor_handle_event (ETextEventProcessor *tep, ETextEventProcessorEvent *event)
{
	if (E_TEXT_EVENT_PROCESSOR_GET_CLASS(tep)->event)
		return E_TEXT_EVENT_PROCESSOR_GET_CLASS(tep)->event(tep, event);
	else
		return 0;
}

/* Set_arg handler for the text item */
static void
e_text_event_processor_set_property (GObject *object,
				     guint prop_id,
				     const GValue *value,
				     GParamSpec *pspec)
{
	ETextEventProcessor *tep = E_TEXT_EVENT_PROCESSOR (object);

	switch (prop_id) {
	case PROP_ALLOW_NEWLINES:
		tep->allow_newlines = g_value_get_boolean (value);
		break;
	default:
		return;
	}
}

/* Get_arg handler for the text item */
static void
e_text_event_processor_get_property (GObject *object,
				     guint prop_id,
				     GValue *value,
				     GParamSpec *pspec)
{
	ETextEventProcessor *tep = E_TEXT_EVENT_PROCESSOR (object);

	switch (prop_id) {
	case PROP_ALLOW_NEWLINES:
		g_value_set_boolean (value, tep->allow_newlines);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}
