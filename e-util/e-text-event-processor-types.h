/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-text-event-processor-types.h
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __E_TEXT_EVENT_PROCESSOR_TYPES_H__
#define __E_TEXT_EVENT_PROCESSOR_TYPES_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef union _ETextEventProcessorEvent ETextEventProcessorEvent;

typedef enum {
	E_TEP_VALUE,
	E_TEP_SELECTION,

	E_TEP_START_OF_BUFFER,
	E_TEP_END_OF_BUFFER,

	E_TEP_START_OF_LINE,
	E_TEP_END_OF_LINE,

	E_TEP_FORWARD_CHARACTER,
	E_TEP_BACKWARD_CHARACTER,

	E_TEP_FORWARD_WORD,
	E_TEP_BACKWARD_WORD,

	E_TEP_FORWARD_LINE,
	E_TEP_BACKWARD_LINE,

	E_TEP_FORWARD_PARAGRAPH,
	E_TEP_BACKWARD_PARAGRAPH,

	E_TEP_FORWARD_PAGE,
	E_TEP_BACKWARD_PAGE,

	E_TEP_SELECT_WORD,
	E_TEP_SELECT_ALL

} ETextEventProcessorCommandPosition;

typedef enum {
	E_TEP_MOVE,
	E_TEP_SELECT,
	E_TEP_DELETE,
	E_TEP_INSERT,

	E_TEP_CAPS,

	E_TEP_COPY,
	E_TEP_PASTE,
	E_TEP_GET_SELECTION,
	E_TEP_SET_SELECT_BY_WORD,
	E_TEP_ACTIVATE,

	E_TEP_GRAB,
	E_TEP_UNGRAB,

	E_TEP_NOP
} ETextEventProcessorCommandAction;

typedef struct {
	ETextEventProcessorCommandPosition position;
	ETextEventProcessorCommandAction action;
	int value;
	char *string;
	guint32 time;
} ETextEventProcessorCommand;

typedef struct {
	GdkEventType type;
	guint32 time;
	guint state;
	guint button;
	gint position;
} ETextEventProcessorEventButton;

typedef struct {
	GdkEventType type;
	guint32 time;
	guint state;
	guint keyval;
	gint length;
	gchar *string;
} ETextEventProcessorEventKey;

typedef struct {
	GdkEventType type;
	guint32 time;
	guint state;
	gint position;
} ETextEventProcessorEventMotion;

union _ETextEventProcessorEvent {
	GdkEventType type;
	ETextEventProcessorEventButton button;
	ETextEventProcessorEventKey key;
	ETextEventProcessorEventMotion motion;
};

typedef enum _ETextEventProcessorCaps {
	E_TEP_CAPS_UPPER,
	E_TEP_CAPS_LOWER,
	E_TEP_CAPS_TITLE
} ETextEventProcessorCaps;

G_END_DECLS

#endif /* __E_TEXT_EVENT_PROCESSOR_TYPES_H__ */
