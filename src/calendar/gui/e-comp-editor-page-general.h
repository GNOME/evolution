/*
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef E_COMP_EDITOR_PAGE_GENERAL_H
#define E_COMP_EDITOR_PAGE_GENERAL_H

#include <libedataserver/libedataserver.h>
#include <calendar/gui/e-meeting-store.h>
#include <calendar/gui/e-comp-editor.h>
#include <calendar/gui/e-comp-editor-page.h>

/* Standard GObject macros */

#define E_TYPE_COMP_EDITOR_PAGE_GENERAL \
	(e_comp_editor_page_general_get_type ())
#define E_COMP_EDITOR_PAGE_GENERAL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PAGE_GENERAL, ECompEditorPageGeneral))
#define E_COMP_EDITOR_PAGE_GENERAL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMP_EDITOR_PAGE_GENERAL, ECompEditorPageGeneralClass))
#define E_IS_COMP_EDITOR_PAGE_GENERAL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PAGE_GENERAL))
#define E_IS_COMP_EDITOR_PAGE_GENERAL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMP_EDITOR_PAGE_GENERAL))
#define E_COMP_EDITOR_PAGE_GENERAL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMP_EDITOR_PAGE_GENERAL, ECompEditorPageGeneralClass))

typedef struct _ECompEditorPageGeneral ECompEditorPageGeneral;
typedef struct _ECompEditorPageGeneralClass ECompEditorPageGeneralClass;
typedef struct _ECompEditorPageGeneralPrivate ECompEditorPageGeneralPrivate;

struct _ECompEditorPageGeneral {
	ECompEditorPage parent;

	ECompEditorPageGeneralPrivate *priv;
};

struct _ECompEditorPageGeneralClass {
	ECompEditorPageClass parent_class;
};

/* ECompEditorPageGeneral uses the first two lines of the grid, counting from zero:

        Organizer: [                       | v ] Source: [                       | v ]
  [ Attendees... ] +---------------------------------------------------+  [ Add      ]
                   |                                                   |  [ Edit     ]
                   |                                                   |  [ Remove   ]
                   |                                                   |
                   |                                                   |
                   +---------------------------------------------------+

 and when show-attendees is set to FALSE, the second line and the organizer part are
 hidden, making shown only the first line as:

           Source: [                       | v ]
*/

GType		e_comp_editor_page_general_get_type	(void) G_GNUC_CONST;
ECompEditorPage *
		e_comp_editor_page_general_new		(ECompEditor *editor,
							 const gchar *source_label,
							 const gchar *source_extension_name,
							 ESource *select_source,
							 gboolean show_attendees,
							 gint data_column_width);
const gchar *	e_comp_editor_page_general_get_source_label
							(ECompEditorPageGeneral *page_general);
void		e_comp_editor_page_general_set_source_label
							(ECompEditorPageGeneral *page_general,
							 const gchar *source_label);
const gchar *	e_comp_editor_page_general_get_source_extension_name
							(ECompEditorPageGeneral *page_general);
void		e_comp_editor_page_general_set_source_extension_name
							(ECompEditorPageGeneral *page_general,
							 const gchar *source_extension_name);
ESource *	e_comp_editor_page_general_ref_selected_source
							(ECompEditorPageGeneral *page_general);
void		e_comp_editor_page_general_set_selected_source
							(ECompEditorPageGeneral *page_general,
							 ESource *source);
gboolean	e_comp_editor_page_general_get_show_attendees
							(ECompEditorPageGeneral *page_general);
void		e_comp_editor_page_general_set_show_attendees
							(ECompEditorPageGeneral *page_general,
							 gboolean show_attendees);
gint		e_comp_editor_page_general_get_data_column_width
							(ECompEditorPageGeneral *page_general);
void		e_comp_editor_page_general_set_data_column_width
							(ECompEditorPageGeneral *page_general,
							 gint data_column_width);
void		e_comp_editor_page_general_update_view	(ECompEditorPageGeneral *page_general);
EMeetingStore *	e_comp_editor_page_general_get_meeting_store
							(ECompEditorPageGeneral *page_general);
ENameSelector *	e_comp_editor_page_general_get_name_selector
							(ECompEditorPageGeneral *page_general);
GSList *	e_comp_editor_page_general_get_added_attendees
							(ECompEditorPageGeneral *page_general);
GSList *	e_comp_editor_page_general_get_removed_attendees
							(ECompEditorPageGeneral *page_general);
GtkWidget *	e_comp_editor_page_general_get_source_combo_box
							(ECompEditorPageGeneral *page_general);

G_END_DECLS

#endif /* E_COMP_EDITOR_PAGE_GENERAL_H */
