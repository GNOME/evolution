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

#ifndef E_COMP_EDITOR_PAGE_H
#define E_COMP_EDITOR_PAGE_H

#include <gtk/gtk.h>
#include <libecal/libecal.h>

#include <calendar/gui/e-comp-editor-property-part.h>

/* Standard GObject macros */

#define E_TYPE_COMP_EDITOR_PAGE \
	(e_comp_editor_page_get_type ())
#define E_COMP_EDITOR_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PAGE, ECompEditorPage))
#define E_COMP_EDITOR_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMP_EDITOR_PAGE, ECompEditorPageClass))
#define E_IS_COMP_EDITOR_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PAGE))
#define E_IS_COMP_EDITOR_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMP_EDITOR_PAGE))
#define E_COMP_EDITOR_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMP_EDITOR_PAGE, ECompEditorPageClass))

G_BEGIN_DECLS

struct _ECompEditor;

typedef struct _ECompEditorPage ECompEditorPage;
typedef struct _ECompEditorPageClass ECompEditorPageClass;
typedef struct _ECompEditorPagePrivate ECompEditorPagePrivate;

struct _ECompEditorPage {
	GtkGrid parent;

	ECompEditorPagePrivate *priv;
};

struct _ECompEditorPageClass {
	GtkGridClass parent_class;

	/* Virtual functions */
	void		(* sensitize_widgets)	(ECompEditorPage *page,
						 gboolean force_insensitive);
	void		(* fill_widgets)	(ECompEditorPage *page,
						 ICalComponent *component);
	gboolean	(* fill_component)	(ECompEditorPage *page,
						 ICalComponent *component);

	/* Signals */
	void		(* changed)		(ECompEditorPage *page);
};

GType		e_comp_editor_page_get_type		(void) G_GNUC_CONST;

struct _ECompEditor *
		e_comp_editor_page_ref_editor		(ECompEditorPage *page);
void		e_comp_editor_page_add_property_part	(ECompEditorPage *page,
							 ECompEditorPropertyPart *part,
							 gint attach_left,
							 gint attach_top,
							 gint attach_width,
							 gint attach_height);
ECompEditorPropertyPart *
		e_comp_editor_page_get_property_part	(ECompEditorPage *page,
							 ICalPropertyKind prop_kind);
void		e_comp_editor_page_sensitize_widgets	(ECompEditorPage *page,
							 gboolean force_insensitive);
void		e_comp_editor_page_fill_widgets		(ECompEditorPage *page,
							 ICalComponent *component);
gboolean	e_comp_editor_page_fill_component	(ECompEditorPage *page,
							 ICalComponent *component);
void		e_comp_editor_page_emit_changed		(ECompEditorPage *page);
gboolean	e_comp_editor_page_get_updating		(ECompEditorPage *page);
void		e_comp_editor_page_set_updating		(ECompEditorPage *page,
							 gboolean updating);
void		e_comp_editor_page_select		(ECompEditorPage *page);

G_END_DECLS

#endif /* E_COMP_EDITOR_PAGE_H */
