/*
 *
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TEXT_EVENT_PROCESSOR_TYPES_H
#define E_TEXT_EVENT_PROCESSOR_TYPES_H

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
	gint value;
	const gchar *string;
	GdkDevice *device;
	guint32 time;
} ETextEventProcessorCommand;

typedef struct {
	GdkEventType type;
	guint32 time;
	guint state;
	guint button;
	gint position;
	GdkDevice *device;
} ETextEventProcessorEventButton;

typedef struct {
	GdkEventType type;
	guint32 time;
	guint state;
	guint keyval;
	gint length;
	const gchar *string;
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

#endif /* E_TEXT_EVENT_PROCESSOR_TYPES_H */
