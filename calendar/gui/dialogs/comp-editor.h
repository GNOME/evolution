/* Evolution calendar - Framework for a calendar component editor dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
#include <bonobo/bonobo-win.h>
#include <bonobo/bonobo-ui-engine.h>
#include <bonobo/bonobo-ui-component.h>
#include "cal-client.h"
#include "../itip-utils.h"
#include "comp-editor-page.h"
#include "evolution-shell-component-utils.h"

BEGIN_GNOME_DECLS



#define TYPE_COMP_EDITOR            (comp_editor_get_type ())
#define COMP_EDITOR(obj)            (GTK_CHECK_CAST ((obj), TYPE_COMP_EDITOR, CompEditor))
#define COMP_EDITOR_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_COMP_EDITOR, CompEditorClass))
#define IS_COMP_EDITOR(obj)         (GTK_CHECK_TYPE ((obj), TYPE_COMP_EDITOR))
#define IS_COMP_EDITOR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_COMP_EDITOR))

typedef struct _CompEditorPrivate CompEditorPrivate;

typedef struct {
	BonoboWindow object;

	/* Private data */
	CompEditorPrivate *priv;
} CompEditor;

typedef struct {
	BonoboWindowClass parent_class;

	/* Virtual functions */
	void (* set_cal_client) (CompEditor *page, CalClient *client);
	void (* edit_comp) (CompEditor *page, CalComponent *comp);
	gboolean (* send_comp) (CompEditor *page, CalComponentItipMethod method);
} CompEditorClass;

GtkType       comp_editor_get_type         (void);
void          comp_editor_set_changed      (CompEditor             *editor,
					    gboolean                changed);
gboolean      comp_editor_get_changed      (CompEditor             *editor);
void          comp_editor_set_needs_send   (CompEditor             *editor,
					    gboolean                needs_send);
gboolean      comp_editor_get_needs_send   (CompEditor             *editor);
void          comp_editor_set_existing_org (CompEditor             *editor,
					    gboolean                existing_org);
gboolean      comp_editor_get_existing_org (CompEditor             *editor);
void          comp_editor_set_user_org     (CompEditor             *editor,
					    gboolean                user_org);
gboolean      comp_editor_get_user_org     (CompEditor             *editor);
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
CalComponent *comp_editor_get_comp         (CompEditor             *editor);
CalComponent *comp_editor_get_current_comp (CompEditor             *editor);
gboolean      comp_editor_save_comp        (CompEditor             *editor,
					    gboolean                send);
void          comp_editor_delete_comp      (CompEditor             *editor);
gboolean      comp_editor_send_comp        (CompEditor             *editor,
					    CalComponentItipMethod  method);
gboolean      comp_editor_close            (CompEditor             *editor);
void          comp_editor_merge_ui         (CompEditor             *editor,
					    const char             *filename,
					    BonoboUIVerb           *verbs,
					    EPixmap                *pixmaps);
void          comp_editor_set_ui_prop      (CompEditor             *editor,
					    const char             *path,
					    const char             *attr,
					    const char             *val);
void          comp_editor_focus            (CompEditor             *editor);




END_GNOME_DECLS

#endif
