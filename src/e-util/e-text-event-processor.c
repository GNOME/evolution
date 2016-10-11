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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <glib/gi18n.h>

#include "e-text-event-processor.h"

static void e_text_event_processor_set_property (GObject *object,
						 guint property_id,
						 const GValue *value,
						 GParamSpec *pspec);
static void e_text_event_processor_get_property (GObject *object,
						 guint property_id,
						 GValue *value,
						 GParamSpec *pspec);

enum {
	PROP_0,
	PROP_ALLOW_NEWLINES
};

enum {
	E_TEP_EVENT,
	E_TEP_LAST_SIGNAL
};

static guint e_tep_signals[E_TEP_LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (
	ETextEventProcessor,
	e_text_event_processor,
	G_TYPE_OBJECT)

static void
e_text_event_processor_class_init (ETextEventProcessorClass *class)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) class;

	object_class->set_property = e_text_event_processor_set_property;
	object_class->get_property = e_text_event_processor_get_property;

	e_tep_signals[E_TEP_EVENT] = g_signal_new (
		"command",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETextEventProcessorClass, command),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	g_object_class_install_property (
		object_class,
		PROP_ALLOW_NEWLINES,
		g_param_spec_boolean (
			"allow_newlines",
			"Allow newlines",
			"Allow newlines",
			FALSE,
			G_PARAM_READWRITE));

	class->event = NULL;
	class->command = NULL;
}

static void
e_text_event_processor_init (ETextEventProcessor *tep)
{
	tep->allow_newlines = TRUE;
}

gint
e_text_event_processor_handle_event (ETextEventProcessor *tep,
                                     ETextEventProcessorEvent *event)
{
	if (E_TEXT_EVENT_PROCESSOR_GET_CLASS (tep)->event)
		return E_TEXT_EVENT_PROCESSOR_GET_CLASS (tep)->event (tep, event);
	else
		return 0;
}

/* Set_arg handler for the text item */
static void
e_text_event_processor_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
	ETextEventProcessor *tep = E_TEXT_EVENT_PROCESSOR (object);

	switch (property_id) {
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
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
	ETextEventProcessor *tep = E_TEXT_EVENT_PROCESSOR (object);

	switch (property_id) {
	case PROP_ALLOW_NEWLINES:
		g_value_set_boolean (value, tep->allow_newlines);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}
