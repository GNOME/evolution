/* Evolution calendar - Framework for a calendar component editor dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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

#ifndef COMP_EDITOR_H
#define COMP_EDITOR_H

#include <gtk/gtk.h>
#include <bonobo/bonobo-ui-engine.h>
#include <bonobo/bonobo-ui-component.h>
#include "cal-client.h"
#include "../itip-utils.h"
#include "comp-editor-page.h"

BEGIN_GNOME_DECLS



#define TYPE_COMP_EDITOR            (comp_editor_get_type ())
#define COMP_EDITOR(obj)            (GTK_CHECK_CAST ((obj), TYPE_COMP_EDITOR, CompEditor))
#define COMP_EDITOR_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_COMP_EDITOR, CompEditorClass))
#define IS_COMP_EDITOR(obj)         (GTK_CHECK_TYPE ((obj), TYPE_COMP_EDITOR))
#define IS_COMP_EDITOR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_COMP_EDITOR))

typedef struct _CompEditorPrivate CompEditorPrivate;

typedef struct {
	GtkObject object;

	/* Private data */
	CompEditorPrivate *priv;
} CompEditor;

typedef struct {
	GtkObjectClass parent_class;

	/* Virtual functions */
	void (* edit_comp) (CompEditor *page, CalComponent *comp);
} CompEditorClass;
GtkType       comp_editor_get_type         (void);
CompEditor   *comp_editor_new              (void);
void          comp_editor_append_page      (CompEditor             *editor,
					    CompEditorPage         *page,
					    const char             *label);
void          comp_editor_remove_page      (CompEditor             *editor,
					    CompEditorPage         *page);
void          comp_editor_show_page        (CompEditor             *editor,
					    CompEditorPage         *page);
void          comp_editor_set_cal_client   (CompEditor             *editor,
					    CalClient              *client);
CalClient    *comp_editor_get_cal_client   (CompEditor             *editor);
void          comp_editor_edit_comp        (CompEditor             *ee,
					    CalComponent           *comp);
CalComponent *comp_editor_get_current_comp (CompEditor             *editor);
void          comp_editor_save_comp        (CompEditor             *editor);
void          comp_editor_delete_comp      (CompEditor             *editor);
void          comp_editor_send_comp        (CompEditor             *editor,
					    CalComponentItipMethod  method);
void          comp_editor_merge_ui         (CompEditor             *editor,
					    const char             *filename,
					    BonoboUIVerb           *verbs);
void          comp_editor_focus            (CompEditor             *editor);






END_GNOME_DECLS

#endif
