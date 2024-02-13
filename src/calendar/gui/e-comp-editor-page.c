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

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "e-comp-editor.h"
#include "e-comp-editor-page.h"

struct _ECompEditorPagePrivate {
	GWeakRef editor; /* Holds an ECompEditor * */

	GSList *parts; /* PropertyPartData * */
};

enum {
	PROP_0,
	PROP_EDITOR
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ECompEditorPage, e_comp_editor_page, GTK_TYPE_GRID)

typedef struct _ProperyPartData {
	ECompEditorPropertyPart *part;
	gulong changed_handler_id;
} PropertyPartData;

static void
property_part_data_free (gpointer ptr)
{
	PropertyPartData *ppd = ptr;

	if (ppd) {
		if (ppd->changed_handler_id)
			g_signal_handler_disconnect (ppd->part, ppd->changed_handler_id);
		g_clear_object (&ppd->part);
		g_free (ppd);
	}
}

static void
ecep_sensitize_widgets (ECompEditorPage *page,
		        gboolean force_insensitive)
{
	GSList *link;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE (page));

	for (link = page->priv->parts; link; link = g_slist_next (link)) {
		PropertyPartData *ppd = link->data;

		g_warn_if_fail (ppd != NULL);

		if (ppd)
			e_comp_editor_property_part_sensitize_widgets (ppd->part, force_insensitive);
	}
}

static void
ecep_fill_widgets (ECompEditorPage *page,
		   ICalComponent *component)
{
	GSList *link;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE (page));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	for (link = page->priv->parts; link; link = g_slist_next (link)) {
		PropertyPartData *ppd = link->data;

		g_warn_if_fail (ppd != NULL);
		if (!ppd)
			continue;

		e_comp_editor_property_part_fill_widget (ppd->part, component);
	}
}

static gboolean
ecep_fill_component (ECompEditorPage *page,
		     ICalComponent *component)
{
	GSList *link;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE (page), FALSE);
	g_return_val_if_fail (I_CAL_IS_COMPONENT (component), FALSE);

	for (link = page->priv->parts; link; link = g_slist_next (link)) {
		PropertyPartData *ppd = link->data;

		g_warn_if_fail (ppd != NULL);
		if (!ppd)
			continue;

		e_comp_editor_property_part_fill_component (ppd->part, component);
	}

	return TRUE;
}

static void
e_comp_editor_page_set_editor (ECompEditorPage *page,
			       ECompEditor *editor)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE (page));
	g_return_if_fail (E_IS_COMP_EDITOR (editor));

	g_weak_ref_set (&page->priv->editor, editor);
}

static void
e_comp_editor_page_set_property (GObject *object,
				 guint property_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EDITOR:
			e_comp_editor_page_set_editor (
				E_COMP_EDITOR_PAGE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_comp_editor_page_get_property (GObject *object,
				 guint property_id,
				 GValue *value,
				 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EDITOR:
			g_value_take_object (
				value,
				e_comp_editor_page_ref_editor (
				E_COMP_EDITOR_PAGE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_comp_editor_page_constructed (GObject *object)
{
	G_OBJECT_CLASS (e_comp_editor_page_parent_class)->constructed (object);

	gtk_widget_show (GTK_WIDGET (object));

	g_object_set (object,
		"column-spacing", 4,
		"row-spacing", 4,
		"border-width", 6,
		NULL);
}

static void
e_comp_editor_page_finalize (GObject *object)
{
	ECompEditorPage *page = E_COMP_EDITOR_PAGE (object);

	g_weak_ref_clear (&page->priv->editor);

	g_slist_free_full (page->priv->parts, property_part_data_free);
	page->priv->parts = NULL;

	G_OBJECT_CLASS (e_comp_editor_page_parent_class)->finalize (object);
}

static void
e_comp_editor_page_init (ECompEditorPage *page)
{
	page->priv = e_comp_editor_page_get_instance_private (page);

	g_weak_ref_init (&page->priv->editor, NULL);
}

static void
e_comp_editor_page_class_init (ECompEditorPageClass *klass)
{
	GObjectClass *object_class;

	klass->sensitize_widgets = ecep_sensitize_widgets;
	klass->fill_widgets = ecep_fill_widgets;
	klass->fill_component = ecep_fill_component;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_comp_editor_page_set_property;
	object_class->get_property = e_comp_editor_page_get_property;
	object_class->constructed = e_comp_editor_page_constructed;
	object_class->finalize = e_comp_editor_page_finalize;

	g_object_class_install_property (
		object_class,
		PROP_EDITOR,
		g_param_spec_object (
			"editor",
			"Editor",
			"ECompEditor the page belongs to",
			E_TYPE_COMP_EDITOR,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (ECompEditorPageClass, changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);
}

ECompEditor *
e_comp_editor_page_ref_editor (ECompEditorPage *page)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE (page), NULL);

	return g_weak_ref_get (&page->priv->editor);
}

/* This consumes the 'part'. */
void
e_comp_editor_page_add_property_part (ECompEditorPage *page,
				      ECompEditorPropertyPart *part,
				      gint attach_left,
				      gint attach_top,
				      gint attach_width,
				      gint attach_height)
{
	GtkWidget *label_widget;
	GtkWidget *edit_widget;
	PropertyPartData *ppd;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE (page));
	g_return_if_fail (E_IS_COMP_EDITOR_PROPERTY_PART (part));

	label_widget = e_comp_editor_property_part_get_label_widget (part);
	edit_widget = e_comp_editor_property_part_get_edit_widget (part);

	g_return_if_fail (label_widget != NULL || edit_widget != NULL);

	ppd = g_new0 (PropertyPartData, 1);
	ppd->part = part; /* takes ownership */
	ppd->changed_handler_id = g_signal_connect_swapped (part, "changed",
		G_CALLBACK (e_comp_editor_page_emit_changed), page);

	if (label_widget) {
		gtk_grid_attach (GTK_GRID (page), label_widget, attach_left, attach_top, 1, attach_height);
	}
	if (edit_widget) {
		gint inc = label_widget ? 1 : 0;

		gtk_grid_attach (GTK_GRID (page), edit_widget, attach_left + inc, attach_top, MAX (attach_width - inc, 1), attach_height);
	}

	page->priv->parts = g_slist_append (page->priv->parts, ppd);
}

ECompEditorPropertyPart *
e_comp_editor_page_get_property_part (ECompEditorPage *page,
				      ICalPropertyKind prop_kind)
{
	GSList *link;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE (page), NULL);

	for (link = page->priv->parts; link; link = g_slist_next (link)) {
		PropertyPartData *ppd = link->data;

		if (E_IS_COMP_EDITOR_PROPERTY_PART_STRING (ppd->part)) {
			ECompEditorPropertyPartStringClass *klass = E_COMP_EDITOR_PROPERTY_PART_STRING_GET_CLASS (ppd->part);
			if (klass->prop_kind == prop_kind)
				return ppd->part;
		}

		if (E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME (ppd->part)) {
			ECompEditorPropertyPartDatetimeClass *klass = E_COMP_EDITOR_PROPERTY_PART_DATETIME_GET_CLASS (ppd->part);
			if (klass->prop_kind == prop_kind)
				return ppd->part;
		}

		if (E_IS_COMP_EDITOR_PROPERTY_PART_SPIN (ppd->part)) {
			ECompEditorPropertyPartSpinClass *klass = E_COMP_EDITOR_PROPERTY_PART_SPIN_GET_CLASS (ppd->part);
			if (klass->prop_kind == prop_kind)
				return ppd->part;
		}
	}

	return NULL;
}

void
e_comp_editor_page_sensitize_widgets (ECompEditorPage *page,
				      gboolean force_insensitive)
{
	ECompEditorPageClass *klass;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE (page));

	klass = E_COMP_EDITOR_PAGE_GET_CLASS (page);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->sensitize_widgets != NULL);

	klass->sensitize_widgets (page, force_insensitive);
}

void
e_comp_editor_page_fill_widgets (ECompEditorPage *page,
				 ICalComponent *component)
{
	ECompEditorPageClass *klass;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE (page));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	klass = E_COMP_EDITOR_PAGE_GET_CLASS (page);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->fill_widgets != NULL);

	e_comp_editor_page_set_updating (page, TRUE);

	klass->fill_widgets (page, component);

	e_comp_editor_page_set_updating (page, FALSE);
}

gboolean
e_comp_editor_page_fill_component (ECompEditorPage *page,
				   ICalComponent *component)
{
	ECompEditorPageClass *klass;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE (page), FALSE);
	g_return_val_if_fail (I_CAL_IS_COMPONENT (component), FALSE);

	klass = E_COMP_EDITOR_PAGE_GET_CLASS (page);
	g_return_val_if_fail (klass != NULL, FALSE);
	g_return_val_if_fail (klass->fill_component != NULL, FALSE);

	return klass->fill_component (page, component);
}

void
e_comp_editor_page_emit_changed (ECompEditorPage *page)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE (page));

	g_signal_emit (page, signals[CHANGED], 0, NULL);
}

gboolean
e_comp_editor_page_get_updating (ECompEditorPage *page)
{
	ECompEditor *comp_editor;
	gboolean updating = FALSE;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE (page), FALSE);

	comp_editor = e_comp_editor_page_ref_editor (page);
	if (comp_editor)
		updating = e_comp_editor_get_updating (comp_editor);
	g_clear_object (&comp_editor);

	return updating;
}

void
e_comp_editor_page_set_updating (ECompEditorPage *page,
				 gboolean updating)
{
	ECompEditor *comp_editor;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE (page));

	comp_editor = e_comp_editor_page_ref_editor (page);
	if (comp_editor)
		e_comp_editor_set_updating (comp_editor, updating);
	g_clear_object (&comp_editor);
}

void
e_comp_editor_page_select (ECompEditorPage *page)
{
	ECompEditor *comp_editor;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE (page));

	comp_editor = e_comp_editor_page_ref_editor (page);
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	e_comp_editor_select_page (comp_editor, page);

	g_clear_object (&comp_editor);
}
