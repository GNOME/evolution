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
#include <libecal/e-cal.h>
#include "../itip-utils.h"
#include "comp-editor-page.h"

G_BEGIN_DECLS



#define TYPE_COMP_EDITOR            (comp_editor_get_type ())
#define COMP_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_COMP_EDITOR, CompEditor))
#define COMP_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_COMP_EDITOR, CompEditorClass))
#define IS_COMP_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_COMP_EDITOR))
#define IS_COMP_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_COMP_EDITOR))

typedef struct _CompEditorPrivate CompEditorPrivate;

typedef struct {
	GtkDialog object;

	/* Private data */
	CompEditorPrivate *priv;
} CompEditor;

typedef struct {
	GtkDialogClass parent_class;

	/* Virtual functions */
	void (* set_e_cal) (CompEditor *page, ECal *client);
	void (* edit_comp) (CompEditor *page, ECalComponent *comp);
	gboolean (* send_comp) (CompEditor *page, ECalComponentItipMethod method);
} CompEditorClass;

typedef enum {
	COMP_EDITOR_NEW_ITEM = 1<<0,
	COMP_EDITOR_MEETING = 1<<1,
	COMP_EDITOR_DELEGATE = 1<<2,
	COMP_EDITOR_USER_ORG = 1<<3,
	COMP_EDITOR_IS_ASSIGNED = 1<<4,
} CompEditorFlags;

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
void          comp_editor_set_group_item     (CompEditor             *editor,
					    gboolean                is_group_item);
gboolean      comp_editor_get_group_item     (CompEditor             *editor);
void          comp_editor_append_page      (CompEditor             *editor,
					    CompEditorPage         *page,
					    const char             *label);
void          comp_editor_remove_page      (CompEditor             *editor,
					    CompEditorPage         *page);
void          comp_editor_show_page        (CompEditor             *editor,
					    CompEditorPage         *page);
void          comp_editor_set_e_cal        (CompEditor             *editor,
					    ECal              *client);
void          comp_editor_set_help_section (CompEditor             *editor,
					    const char             *section);
ECal         *comp_editor_get_e_cal        (CompEditor             *editor);
void          comp_editor_edit_comp        (CompEditor             *ee,
					    ECalComponent           *comp);
ECalComponent *comp_editor_get_comp        (CompEditor             *editor);
ECalComponent *comp_editor_get_current_comp (CompEditor             *editor);
gboolean      comp_editor_save_comp        (CompEditor             *editor,
					    gboolean                send);
void          comp_editor_delete_comp      (CompEditor             *editor);
gboolean      comp_editor_send_comp        (CompEditor             *editor,
					    ECalComponentItipMethod  method);
GSList        *comp_editor_get_mime_attach_list (CompEditor        *editor); 
gboolean      comp_editor_close            (CompEditor             *editor);
void          comp_editor_focus            (CompEditor             *editor);

void          comp_editor_notify_client_changed (CompEditor *editor, ECal *client);

void comp_editor_sensitize_attachment_bar (CompEditor *editor, gboolean set);
void comp_editor_set_flags (CompEditor *editor, CompEditorFlags flags);
CompEditorFlags comp_editor_get_flags (CompEditor *editor);


G_END_DECLS

#endif
