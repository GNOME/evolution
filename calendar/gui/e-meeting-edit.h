/* Evolution calendar - Meeting editor dialog
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Jesse Pavel <jpavel@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __E_MEETING_EDIT_H__
#define __E_MEETING_EDIT_H__

#include <config.h>
#include <gnome.h>
#include <glade/glade.h>
#include <cal-util/cal-component.h>
#include <cal-client/cal-client.h>
#include "event-editor.h"

typedef struct _EMeetingEditor EMeetingEditor;

struct _EMeetingEditor {
	gpointer priv;
};


EMeetingEditor * e_meeting_editor_new (CalComponent *comp, CalClient *client, 
				       EventEditor *ee);
void e_meeting_edit (EMeetingEditor *editor);
void e_meeting_editor_free (EMeetingEditor *editor);


#endif /*  __E_MEETING_EDIT_H__  */

