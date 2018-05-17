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

#include <string.h>

#include <gdk/gdkkeysyms.h>

#include "e-text-event-processor-emacs-like.h"

static gint	e_text_event_processor_emacs_like_event
					(ETextEventProcessor *tep,
					 ETextEventProcessorEvent *event);

G_DEFINE_TYPE (
	ETextEventProcessorEmacsLike,
	e_text_event_processor_emacs_like,
	E_TYPE_TEXT_EVENT_PROCESSOR)

static const ETextEventProcessorCommand control_keys[26] = {
	{ E_TEP_START_OF_LINE,      E_TEP_MOVE, 0, "" }, /* a */
	{ E_TEP_BACKWARD_CHARACTER, E_TEP_MOVE, 0, "" }, /* b */
	{ E_TEP_SELECTION,          E_TEP_COPY, 0, "" }, /* c */
	{ E_TEP_FORWARD_CHARACTER,  E_TEP_DELETE, 0, "" }, /* d */
	{ E_TEP_END_OF_LINE,        E_TEP_MOVE, 0, "" }, /* e */
	{ E_TEP_FORWARD_CHARACTER,  E_TEP_MOVE, 0, "" }, /* f */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* g */
	{ E_TEP_BACKWARD_CHARACTER, E_TEP_DELETE, 0, "" }, /* h */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* i */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* j */
	{ E_TEP_END_OF_LINE,        E_TEP_DELETE, 0, "" }, /* k */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* l */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* m */
	{ E_TEP_FORWARD_LINE,       E_TEP_MOVE, 0, "" }, /* n */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* o */
	{ E_TEP_BACKWARD_LINE,      E_TEP_MOVE, 0, "" }, /* p */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* q */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* r */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* s */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* t */
	{ E_TEP_START_OF_LINE,      E_TEP_DELETE, 0, "" }, /* u */
	{ E_TEP_SELECTION,          E_TEP_PASTE, 0, "" }, /* v */
	{ E_TEP_SELECTION,          E_TEP_NOP, 0, "" }, /* w */
	{ E_TEP_SELECTION,          E_TEP_DELETE, 0, "" }, /* x */
	{ E_TEP_SELECTION,          E_TEP_PASTE, 0, "" }, /* y */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" }           /* z */
};

static const ETextEventProcessorCommand alt_keys[26] = {
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* a */
	{ E_TEP_BACKWARD_WORD,      E_TEP_MOVE, 0, "" }, /* b */
	{ E_TEP_SELECTION, E_TEP_CAPS, E_TEP_CAPS_TITLE, "" },/* c */
	{ E_TEP_FORWARD_WORD,       E_TEP_DELETE, 0, "" }, /* d */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* e */
	{ E_TEP_FORWARD_WORD,       E_TEP_MOVE, 0, "" }, /* f */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* g */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* h */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* i */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* j */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* k */
	{ E_TEP_SELECTION, E_TEP_CAPS, E_TEP_CAPS_LOWER, "" },           /* l */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* m */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* n */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* o */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* p */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* q */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* r */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* s */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* t */
	{ E_TEP_SELECTION, E_TEP_CAPS, E_TEP_CAPS_UPPER, "" },           /* u */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* v */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* w */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* x */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" },           /* y */
	{ E_TEP_SELECTION, E_TEP_NOP, 0, "" }           /* z */

};

static void
e_text_event_processor_emacs_like_class_init (ETextEventProcessorEmacsLikeClass *class)
{
  ETextEventProcessorClass *processor_class;

  processor_class = (ETextEventProcessorClass *) class;

  processor_class->event = e_text_event_processor_emacs_like_event;
}

static void
e_text_event_processor_emacs_like_init (ETextEventProcessorEmacsLike *tep)
{
}

static gint
e_text_event_processor_emacs_like_event (ETextEventProcessor *tep,
                                         ETextEventProcessorEvent *event)
{
	ETextEventProcessorCommand command;
	ETextEventProcessorEmacsLike *tep_el = E_TEXT_EVENT_PROCESSOR_EMACS_LIKE (tep);
	ETextEventProcessorEventKey key;

	g_return_val_if_fail (event != NULL, 0);

	command.action = E_TEP_NOP;
	key = event->key;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (event->button.button == 1 || event->button.button == 2) {
			command.action = E_TEP_GRAB;
			command.device = event->button.device;
			command.time = event->button.time;
			g_signal_emit_by_name (tep, "command", &command);
			if (event->button.button == 1 && event->button.state & GDK_SHIFT_MASK)
				command.action = E_TEP_SELECT;
			else
				command.action = E_TEP_MOVE;
			command.position = E_TEP_VALUE;
			command.value = event->button.position;
			command.time = event->button.time;
			tep_el->mouse_down = event->button.button == 1;
		}
		break;

	case GDK_2BUTTON_PRESS:
		if (event->button.button == 1) {
			command.action = E_TEP_SELECT;
			command.position = E_TEP_SELECT_WORD;
			command.time = event->button.time;
		}
		break;

	case GDK_3BUTTON_PRESS:
		if (event->button.button == 1) {
			command.action = E_TEP_SELECT;
			command.position = E_TEP_SELECT_ALL;
			command.time = event->button.time;
		}
		break;

	case GDK_BUTTON_RELEASE:
		if (event->button.button == 1) {
			command.action = E_TEP_UNGRAB;
			command.time = event->button.time;
			tep_el->mouse_down = FALSE;
		} else if (event->button.button == 2) {
			command.action = E_TEP_MOVE;
			command.position = E_TEP_VALUE;
			command.value = event->button.position;
			command.time = event->button.time;
			g_signal_emit_by_name (tep, "command", &command);

			command.action = E_TEP_GET_SELECTION;
			command.position = E_TEP_SELECTION;
			command.value = 0;
			command.time = event->button.time;
		}
		break;

	case GDK_MOTION_NOTIFY:
		if (tep_el->mouse_down) {
			command.action = E_TEP_SELECT;
			command.position = E_TEP_VALUE;
			command.time = event->motion.time;
			command.value = event->motion.position;
		}
		break;

	case GDK_KEY_PRESS:
		{
			command.time = event->key.time;

			if (key.state & GDK_SHIFT_MASK)
				command.action = E_TEP_SELECT;
			else if (key.state & GDK_MOD1_MASK)
				command.action = E_TEP_NOP;
			else
				command.action = E_TEP_MOVE;

			switch (key.keyval) {
			case GDK_KEY_Home:
			case GDK_KEY_KP_Home:
				if (key.state & GDK_CONTROL_MASK)
					command.position = E_TEP_START_OF_BUFFER;
				else
					command.position = E_TEP_START_OF_LINE;
				break;

			case GDK_KEY_End:
			case GDK_KEY_KP_End:
				if (key.state & GDK_CONTROL_MASK)
					command.position = E_TEP_END_OF_BUFFER;
				else
					command.position = E_TEP_END_OF_LINE;
				break;

			case GDK_KEY_Page_Up:
			case GDK_KEY_KP_Page_Up:
				command.position = E_TEP_BACKWARD_PAGE;
				break;

			case GDK_KEY_Page_Down:
			case GDK_KEY_KP_Page_Down:
				command.position = E_TEP_FORWARD_PAGE;
				break;

			/* CUA has Ctrl-Up/Ctrl-Down as paragraph up down */
			case GDK_KEY_Up:
			case GDK_KEY_KP_Up:
				command.position = E_TEP_BACKWARD_LINE;
				break;

			case GDK_KEY_Down:
			case GDK_KEY_KP_Down:
				command.position = E_TEP_FORWARD_LINE;
				break;

			case GDK_KEY_Left:
			case GDK_KEY_KP_Left:
				if (key.state & GDK_CONTROL_MASK)
					command.position = E_TEP_BACKWARD_WORD;
				else
					command.position = E_TEP_BACKWARD_CHARACTER;
				break;

			case GDK_KEY_Right:
			case GDK_KEY_KP_Right:
				if (key.state & GDK_CONTROL_MASK)
					command.position = E_TEP_FORWARD_WORD;
				else
					command.position = E_TEP_FORWARD_CHARACTER;
				break;

			case GDK_KEY_BackSpace:
				command.action = E_TEP_DELETE;
				if (key.state & GDK_CONTROL_MASK)
					command.position = E_TEP_BACKWARD_WORD;
				else
					command.position = E_TEP_BACKWARD_CHARACTER;
				break;

			case GDK_KEY_Clear:
				command.action = E_TEP_DELETE;
				command.position = E_TEP_END_OF_LINE;
				break;

			case GDK_KEY_Insert:
			case GDK_KEY_KP_Insert:
				if (key.state & GDK_SHIFT_MASK) {
					command.action = E_TEP_PASTE;
					command.position = E_TEP_SELECTION;
				} else if (key.state & GDK_CONTROL_MASK) {
					command.action = E_TEP_COPY;
					command.position = E_TEP_SELECTION;
				} else {
				/* gtk_toggle_insert(text) -- IMPLEMENT -- FIXME */
				}
				break;

			case GDK_KEY_F16:
				command.action = E_TEP_COPY;
				command.position = E_TEP_SELECTION;
				break;

			case GDK_KEY_F18:
				command.action = E_TEP_PASTE;
				command.position = E_TEP_SELECTION;
				break;

			case GDK_KEY_F20:
				command.action = E_TEP_COPY;
				command.position = E_TEP_SELECTION;
				g_signal_emit_by_name (
					tep, "command", &command);

				command.action = E_TEP_DELETE;
				command.position = E_TEP_SELECTION;
				break;

			case GDK_KEY_Delete:
			case GDK_KEY_KP_Delete:
				if (key.state & GDK_CONTROL_MASK) {
					command.action = E_TEP_DELETE;
					command.position = E_TEP_FORWARD_WORD;
				} else if (key.state & GDK_SHIFT_MASK) {
					command.action = E_TEP_COPY;
					command.position = E_TEP_SELECTION;
					g_signal_emit_by_name (
						tep, "command", &command);

					command.action = E_TEP_DELETE;
					command.position = E_TEP_SELECTION;
				} else {
					command.action = E_TEP_DELETE;
					command.position = E_TEP_FORWARD_CHARACTER;
				}
				break;

			case GDK_KEY_Tab:
			case GDK_KEY_KP_Tab:
			case GDK_KEY_ISO_Left_Tab:
			case GDK_KEY_3270_BackTab:
				/* Don't insert literally */
				command.action = E_TEP_NOP;
				command.position = E_TEP_SELECTION;
				break;

			case GDK_KEY_Return:
			case GDK_KEY_KP_Enter:
				if (tep->allow_newlines) {
					if (key.state & GDK_CONTROL_MASK) {
						command.action = E_TEP_ACTIVATE;
						command.position = E_TEP_SELECTION;
					} else {
						command.action = E_TEP_INSERT;
						command.position = E_TEP_SELECTION;
						command.value = 1;
						command.string = "\n";
					}
				} else {
					if (key.state & GDK_CONTROL_MASK) {
						command.action = E_TEP_NOP;
						command.position = E_TEP_SELECTION;
					} else {
						command.action = E_TEP_ACTIVATE;
						command.position = E_TEP_SELECTION;
					}
				}
				break;

			case GDK_KEY_Escape:
				/* Don't insert literally */
				command.action = E_TEP_NOP;
				command.position = E_TEP_SELECTION;
				break;

			case GDK_KEY_KP_Space:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = " ";
				break;

			case GDK_KEY_KP_Equal:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = "=";
				break;

			case GDK_KEY_KP_Multiply:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = "*";
				break;

			case GDK_KEY_KP_Add:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = "+";
				break;

			case GDK_KEY_KP_Subtract:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = "-";
				break;

			case GDK_KEY_KP_Decimal:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = ".";
				break;

			case GDK_KEY_KP_Divide:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = "/";
				break;

			case GDK_KEY_KP_0:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = "0";
				break;

			case GDK_KEY_KP_1:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = "1";
				break;

			case GDK_KEY_KP_2:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = "2";
				break;

			case GDK_KEY_KP_3:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = "3";
				break;

			case GDK_KEY_KP_4:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = "4";
				break;

			case GDK_KEY_KP_5:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = "5";
				break;

			case GDK_KEY_KP_6:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = "6";
				break;

			case GDK_KEY_KP_7:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = "7";
				break;

			case GDK_KEY_KP_8:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = "8";
				break;

			case GDK_KEY_KP_9:
				command.action = E_TEP_INSERT;
				command.position = E_TEP_SELECTION;
				command.value = 1;
				command.string = "9";
				break;

			default:
				if ((key.state & GDK_CONTROL_MASK) &&
				   !(key.state & GDK_MOD1_MASK)) {
					if ((key.keyval >= 'A') && (key.keyval <= 'Z'))
						key.keyval -= 'A' - 'a';

					if ((key.keyval >= 'a') && (key.keyval <= 'z')) {
						command.position = control_keys[(gint) (key.keyval - 'a')].position;
						if (control_keys[(gint) (key.keyval - 'a')].action != E_TEP_MOVE)
							command.action = control_keys[(gint) (key.keyval - 'a')].action;
						command.value = control_keys[(gint) (key.keyval - 'a')].value;
						command.string = control_keys[(gint) (key.keyval - 'a')].string;
					}

					if (key.keyval == ' ') {
						command.action = E_TEP_NOP;
					}

					if (key.keyval == 'x') {
						command.action = E_TEP_COPY;
						command.position = E_TEP_SELECTION;
						g_signal_emit_by_name (tep, "command", &command);

						command.action = E_TEP_DELETE;
						command.position = E_TEP_SELECTION;
					}

					break;

				} else if ((key.state & GDK_MOD1_MASK) &&
					  !(key.state & GDK_CONTROL_MASK)) {
					if ((key.keyval >= 'A') && (key.keyval <= 'Z'))
						key.keyval -= 'A' - 'a';

					if ((key.keyval >= 'a') && (key.keyval <= 'z')) {
						command.position = alt_keys[(gint) (key.keyval - 'a')].position;
						if (alt_keys[(gint) (key.keyval - 'a')].action != E_TEP_MOVE)
							command.action = alt_keys[(gint) (key.keyval - 'a')].action;
						command.value = alt_keys[(gint) (key.keyval - 'a')].value;
						command.string = alt_keys[(gint) (key.keyval - 'a')].string;
					}
				} else if (!(key.state & GDK_MOD1_MASK) &&
					!(key.state & GDK_CONTROL_MASK) &&
					key.length > 0) {
					if (key.keyval >= GDK_KEY_KP_0 &&
					    key.keyval <= GDK_KEY_KP_9) {
						key.keyval = '0';
						key.string = "0";
					}
					command.action = E_TEP_INSERT;
					command.position = E_TEP_SELECTION;
					command.value = strlen (key.string);
					command.string = key.string;

				} else {
					command.action = E_TEP_NOP;
				}
			}
			break;

		case GDK_KEY_RELEASE:
			command.time = event->key.time;
			command.action = E_TEP_NOP;
			break;

		default:
			command.action = E_TEP_NOP;
			break;
		}
	}

	if (command.action != E_TEP_NOP) {
		g_signal_emit_by_name (tep, "command", &command);
		return 1;
	} else
		return 0;
}

ETextEventProcessor *
e_text_event_processor_emacs_like_new (void)
{
	return g_object_new (E_TYPE_TEXT_EVENT_PROCESSOR_EMACS_LIKE, NULL);
}

