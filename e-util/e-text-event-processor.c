/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-text-event-processor.c
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtksignal.h>
#include "e-text-event-processor.h"

static void e_text_event_processor_init		(ETextEventProcessor		 *card);
static void e_text_event_processor_class_init	(ETextEventProcessorClass	 *klass);

static void e_text_event_processor_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_text_event_processor_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

static GtkObjectClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_ALLOW_NEWLINES,
};

enum {
	E_TEP_EVENT,
	E_TEP_LAST_SIGNAL
};

static guint e_tep_signals[E_TEP_LAST_SIGNAL] = { 0 };

GtkType
e_text_event_processor_get_type (void)
{
  static GtkType text_event_processor_type = 0;

  if (!text_event_processor_type)
    {
      static const GtkTypeInfo text_event_processor_info =
      {
        "ETextEventProcessor",
        sizeof (ETextEventProcessor),
        sizeof (ETextEventProcessorClass),
        (GtkClassInitFunc) e_text_event_processor_class_init,
        (GtkObjectInitFunc) e_text_event_processor_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      text_event_processor_type = gtk_type_unique (gtk_object_get_type (), &text_event_processor_info);
    }

  return text_event_processor_type;
}

static void
e_text_event_processor_class_init (ETextEventProcessorClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) klass;

	parent_class = gtk_type_class (gtk_object_get_type ());

	e_tep_signals[E_TEP_EVENT] =
		gtk_signal_new ("command",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETextEventProcessorClass, command),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, e_tep_signals, E_TEP_LAST_SIGNAL);

	gtk_object_add_arg_type ("ETextEventProcessor::allow_newlines", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_ALLOW_NEWLINES);

	klass->event = NULL;
	klass->command = NULL;

	object_class->set_arg = e_text_event_processor_set_arg;
	object_class->get_arg = e_text_event_processor_get_arg;
}

static void
e_text_event_processor_init (ETextEventProcessor *tep)
{
	tep->allow_newlines = TRUE;
}

gint
e_text_event_processor_handle_event (ETextEventProcessor *tep, ETextEventProcessorEvent *event)
{
	if (E_TEXT_EVENT_PROCESSOR_CLASS(GTK_OBJECT(tep)->klass)->event) {
		return E_TEXT_EVENT_PROCESSOR_CLASS(GTK_OBJECT(tep)->klass)->event(tep, event);
	} else {
		return 0;
	}
}

/* Set_arg handler for the text item */
static void
e_text_event_processor_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETextEventProcessor *tep = E_TEXT_EVENT_PROCESSOR (object);

	switch (arg_id) {
	case ARG_ALLOW_NEWLINES:
		tep->allow_newlines = GTK_VALUE_BOOL (*arg);
		break;
	default:
		return;
	}
}

/* Get_arg handler for the text item */
static void
e_text_event_processor_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ETextEventProcessor *tep = E_TEXT_EVENT_PROCESSOR (object);

	switch (arg_id) {
	case ARG_ALLOW_NEWLINES:
		GTK_VALUE_BOOL (*arg) = tep->allow_newlines;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}
