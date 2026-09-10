/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include "comp-util.h"
#include "e-cal-dialogs.h"
#include "e-cal-ops.h"

#include "e-cal-table-list-base.h"

struct _ECalTableListBasePrivate {
	EVirtualTree *vtree;
	EShellView *shell_view; /* weak pointer */
	ECalModel *model;

	ICalComponent *tmp_vcal;

	GtkTargetList *copy_target_list;
	GtkTargetList *paste_target_list;

	const gchar *cut_tooltip;
	const gchar *copy_tooltip;
	const gchar *paste_tooltip;
	const gchar *delete_tooltip;
	const gchar *select_all_tooltip;
	const gchar *no_search_results_message;

	gboolean search_active;
};

enum {
	PROP_0,
	PROP_COPY_TARGET_LIST,
	PROP_PASTE_TARGET_LIST
};

enum {
	OPEN_COMPONENT,
	POPUP_EVENT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void e_cal_table_list_base_selectable_init (ESelectableInterface *iface);
static void e_cal_table_list_base_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);

G_DEFINE_TYPE_WITH_CODE (ECalTableListBase, e_cal_table_list_base, GTK_TYPE_BOX,
	G_ADD_PRIVATE (ECalTableListBase)
	G_IMPLEMENT_INTERFACE (E_TYPE_SELECTABLE, e_cal_table_list_base_selectable_init))

static void
e_cal_table_list_base_delete_selected_components (ECalTableListBase *self)
{
	GSList *objs;

	objs = e_cal_table_list_base_get_selected (self);
	e_cal_ops_delete_ecalmodel_components (self->priv->model, objs);
	g_slist_free (objs);
}

static void
e_cal_table_list_base_update_actions (ESelectable *selectable,
				      EFocusTracker *focus_tracker,
				      GdkAtom *clipboard_targets,
				      gint n_clipboard_targets)
{
	ECalTableListBase *self;
	EUIAction *action;
	GtkTargetList *target_list;
	GSList *list, *iter;
	gboolean can_paste = FALSE;
	gboolean sources_are_editable = TRUE;
	gboolean sensitive;
	guint n_selected;
	gint ii;

	self = E_CAL_TABLE_LIST_BASE (selectable);
	n_selected = e_virtual_tree_selected_count (self->priv->vtree);

	list = e_cal_table_list_base_get_selected (self);
	for (iter = list; iter != NULL && sources_are_editable; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;

		if (!comp_data)
			continue;

		sources_are_editable = sources_are_editable &&
			!e_client_is_readonly (E_CLIENT (comp_data->client));
	}
	g_slist_free (list);

	target_list = e_selectable_get_paste_target_list (selectable);
	for (ii = 0; ii < n_clipboard_targets && !can_paste; ii++) {
		can_paste = gtk_target_list_find (
			target_list, clipboard_targets[ii], NULL);
	}

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	sensitive = (n_selected > 0) && sources_are_editable;
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, self->priv->cut_tooltip);

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	sensitive = (n_selected > 0);
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, self->priv->copy_tooltip);

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	sensitive = sources_are_editable && can_paste;
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, self->priv->paste_tooltip);

	action = e_focus_tracker_get_delete_selection_action (focus_tracker);
	sensitive = (n_selected > 0) && sources_are_editable;
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, self->priv->delete_tooltip);

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	sensitive = TRUE;
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, self->priv->select_all_tooltip);
}

static void
e_cal_table_list_base_cut_clipboard (ESelectable *selectable)
{
	ECalTableListBase *self = E_CAL_TABLE_LIST_BASE (selectable);

	e_selectable_copy_clipboard (selectable);
	e_cal_table_list_base_delete_selected_components (self);
}

static void
e_cal_table_list_base_copy_clipboard (ESelectable *selectable)
{
	ECalTableListBase *self = E_CAL_TABLE_LIST_BASE (selectable);
	GtkClipboard *clipboard;
	GPtrArray *rows;
	gchar *comp_str;
	guint ii;

	self->priv->tmp_vcal = e_cal_util_new_top_level ();

	rows = e_virtual_tree_get_selected_rows (self->priv->vtree);

	for (ii = 0; ii < rows->len; ii++) {
		ECalModelComponent *comp_data = g_ptr_array_index (rows, ii);
		ICalComponent *child;

		e_cal_util_add_timezones_from_component (self->priv->tmp_vcal, comp_data->icalcomp);

		child = i_cal_component_clone (comp_data->icalcomp);
		if (child)
			i_cal_component_take_component (self->priv->tmp_vcal, child);
	}

	g_ptr_array_unref (rows);

	comp_str = i_cal_component_as_ical_string (self->priv->tmp_vcal);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	e_clipboard_set_calendar (clipboard, comp_str, -1);
	gtk_clipboard_store (clipboard);

	g_free (comp_str);

	g_clear_object (&self->priv->tmp_vcal);
}

static void
e_cal_table_list_base_paste_clipboard (ESelectable *selectable)
{
	ECalTableListBase *self = E_CAL_TABLE_LIST_BASE (selectable);
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	if (e_clipboard_wait_is_calendar_available (clipboard)) {
		gchar *ical_str;

		ical_str = e_clipboard_wait_for_calendar (clipboard);
		e_cal_ops_paste_components (self->priv->model, ical_str);
		g_free (ical_str);
	}
}

static void
e_cal_table_list_base_delete_selection (ESelectable *selectable)
{
	ECalTableListBase *self = E_CAL_TABLE_LIST_BASE (selectable);
	ECalTableListBaseClass *klass = E_CAL_TABLE_LIST_BASE_GET_CLASS (self);
	ECalModelComponent *comp_data;
	ECalComponent *comp = NULL;
	gboolean delete = TRUE;
	gboolean retract_handled = FALSE;
	guint n_selected;

	n_selected = e_virtual_tree_selected_count (self->priv->vtree);
	if (n_selected == 0)
		return;

	comp_data = n_selected == 1 ? e_cal_table_list_base_get_selected_comp (self) : NULL;

	if (comp_data) {
		comp = e_cal_component_new_from_icalcomponent (
			i_cal_component_clone (comp_data->icalcomp));
	}

	if (n_selected == 1 && comp && klass->maybe_retract_selection)
		retract_handled = klass->maybe_retract_selection (self, comp, comp_data, &delete);

	if (!retract_handled && e_cal_model_get_confirm_delete (self->priv->model)) {
		ECalComponentVType vtype = E_CAL_COMPONENT_NO_TYPE;

		if (klass->get_delete_confirm_vtype)
			vtype = klass->get_delete_confirm_vtype (self);

		delete = e_cal_dialogs_delete_component (
			comp, FALSE, n_selected, vtype, GTK_WIDGET (self));
	}

	if (delete)
		e_cal_table_list_base_delete_selected_components (self);

	g_clear_object (&comp);
}

static void
e_cal_table_list_base_select_all (ESelectable *selectable)
{
	ECalTableListBase *self = E_CAL_TABLE_LIST_BASE (selectable);

	e_virtual_tree_select_all (self->priv->vtree);
}

static void
e_cal_table_list_base_selectable_init (ESelectableInterface *iface)
{
	iface->update_actions = e_cal_table_list_base_update_actions;
	iface->cut_clipboard = e_cal_table_list_base_cut_clipboard;
	iface->copy_clipboard = e_cal_table_list_base_copy_clipboard;
	iface->paste_clipboard = e_cal_table_list_base_paste_clipboard;
	iface->delete_selection = e_cal_table_list_base_delete_selection;
	iface->select_all = e_cal_table_list_base_select_all;
}

static void
cal_table_list_base_update_empty_message (ECalTableListBase *self)
{
	guint row_count;

	if (!self->priv->vtree || !self->priv->model)
		return;

	row_count = e_virtual_tree_model_get_row_count (E_VIRTUAL_TREE_MODEL (self->priv->model));

	if (row_count > 0 || !self->priv->search_active)
		e_virtual_tree_set_empty_message (self->priv->vtree, NULL);
	else
		e_virtual_tree_set_empty_message (self->priv->vtree, self->priv->no_search_results_message);
}

static void
cal_table_list_base_row_count_changed_cb (EVirtualTreeModel *model,
					  gpointer user_data)
{
	ECalTableListBase *self = user_data;

	cal_table_list_base_update_empty_message (self);
}

static gboolean
e_cal_table_list_base_popup_menu (GtkWidget *widget)
{
	e_cal_table_list_base_emit_popup_event (E_CAL_TABLE_LIST_BASE (widget), NULL);

	return TRUE;
}

static void
e_cal_table_list_base_dispose (GObject *object)
{
	ECalTableListBase *self = E_CAL_TABLE_LIST_BASE (object);

	if (self->priv->shell_view != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (self->priv->shell_view), (gpointer *) &self->priv->shell_view);
		self->priv->shell_view = NULL;
	}

	if (self->priv->vtree) {
		e_virtual_tree_set_model (self->priv->vtree, NULL);
		self->priv->vtree = NULL;
	}

	g_clear_object (&self->priv->model);
	g_clear_pointer (&self->priv->copy_target_list, gtk_target_list_unref);
	g_clear_pointer (&self->priv->paste_target_list, gtk_target_list_unref);

	G_OBJECT_CLASS (e_cal_table_list_base_parent_class)->dispose (object);
}

static void
e_cal_table_list_base_class_init (ECalTableListBaseClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = e_cal_table_list_base_get_property;
	object_class->dispose = e_cal_table_list_base_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->popup_menu = e_cal_table_list_base_popup_menu;

	g_object_class_override_property (object_class, PROP_COPY_TARGET_LIST, "copy-target-list");
	g_object_class_override_property (object_class, PROP_PASTE_TARGET_LIST, "paste-target-list");

	signals[OPEN_COMPONENT] = g_signal_new (
		"open-component",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalTableListBaseClass, open_component),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		E_TYPE_CAL_MODEL_COMPONENT);

	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalTableListBaseClass, popup_event),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
e_cal_table_list_base_get_property (GObject *object,
				    guint property_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COPY_TARGET_LIST:
			g_value_set_boxed (value, e_cal_table_list_base_get_copy_target_list (E_CAL_TABLE_LIST_BASE (object)));
			return;
		case PROP_PASTE_TARGET_LIST:
			g_value_set_boxed (value, e_cal_table_list_base_get_paste_target_list (E_CAL_TABLE_LIST_BASE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_cal_table_list_base_init (ECalTableListBase *self)
{
	GtkTargetList *target_list;

	self->priv = e_cal_table_list_base_get_instance_private (self);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);

	target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_calendar_targets (target_list, 0);
	self->priv->copy_target_list = target_list;

	target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_calendar_targets (target_list, 0);
	self->priv->paste_target_list = target_list;
}

GtkWidget *
e_cal_table_list_base_construct (ECalTableListBase *self,
				 EShellView *shell_view,
				 ECalModel *model,
				 const ECalTableListBaseSetup *setup)
{
	GtkWidget *scrolled_window;
	AtkObject *a11y;
	GSettings *settings;

	g_return_val_if_fail (E_IS_CAL_TABLE_LIST_BASE (self), NULL);
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (setup != NULL, NULL);

	self->priv->shell_view = shell_view;
	g_object_add_weak_pointer (G_OBJECT (shell_view), (gpointer *) &self->priv->shell_view);

	self->priv->model = g_object_ref (model);

	self->priv->cut_tooltip = setup->cut_tooltip;
	self->priv->copy_tooltip = setup->copy_tooltip;
	self->priv->paste_tooltip = setup->paste_tooltip;
	self->priv->delete_tooltip = setup->delete_tooltip;
	self->priv->select_all_tooltip = setup->select_all_tooltip;
	self->priv->no_search_results_message = setup->no_search_results_message;

	self->priv->vtree = E_VIRTUAL_TREE (e_virtual_tree_new (E_VIRTUAL_TREE_MODEL (model)));
	e_virtual_tree_set_selection_mode (self->priv->vtree, GTK_SELECTION_MULTIPLE);

	g_signal_connect_object (model, "row-count-changed",
		G_CALLBACK (cal_table_list_base_row_count_changed_cb), self, 0);

	cal_table_list_base_update_empty_message (self);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	g_settings_bind (settings, "table-sort-on-header-click",
		self->priv->vtree, "header-click-sort-policy",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (settings, "allow-direct-summary-edit",
		self->priv->vtree, "editable",
		G_SETTINGS_BIND_DEFAULT);
	g_clear_object (&settings);

	g_signal_connect (self->priv->vtree, "get-legacy-etable-column-map",
		G_CALLBACK (e_cal_table_common_get_legacy_etable_column_map_cb), setup->get_legacy_etable_column_ids_func);

	setup->setup_columns_func (self);

	g_signal_connect (self->priv->vtree, "column-state-changed",
		setup->column_state_changed_cb, self);
	g_signal_connect (self->priv->vtree, "row-activated",
		setup->row_activated_cb, self);
	g_signal_connect (self->priv->vtree, "right-click",
		setup->right_click_cb, self);
	g_signal_connect (self->priv->vtree, "cell-clicked",
		setup->cell_clicked_cb, self);
	g_signal_connect (self->priv->vtree, "cell-edited",
		setup->cell_edited_cb, self);

	e_virtual_tree_set_row_tooltip_markup_func (self->priv->vtree, e_cal_table_common_row_tooltip_markup_cb, self->priv->model, NULL);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (self->priv->vtree));

	gtk_box_pack_start (GTK_BOX (self), scrolled_window, TRUE, TRUE, 0);

	gtk_widget_show_all (GTK_WIDGET (self));

	a11y = gtk_widget_get_accessible (GTK_WIDGET (self));
	if (a11y)
		atk_object_set_name (a11y, setup->accessible_name);

	return GTK_WIDGET (self);
}

EVirtualTree *
e_cal_table_list_base_get_virtual_tree (ECalTableListBase *self)
{
	g_return_val_if_fail (E_IS_CAL_TABLE_LIST_BASE (self), NULL);

	return self->priv->vtree;
}

ECalModel *
e_cal_table_list_base_get_model (ECalTableListBase *self)
{
	g_return_val_if_fail (E_IS_CAL_TABLE_LIST_BASE (self), NULL);

	return self->priv->model;
}

EShellView *
e_cal_table_list_base_get_shell_view (ECalTableListBase *self)
{
	g_return_val_if_fail (E_IS_CAL_TABLE_LIST_BASE (self), NULL);

	return self->priv->shell_view;
}

GSList *
e_cal_table_list_base_get_selected (ECalTableListBase *self)
{
	GPtrArray *rows;
	GSList *list = NULL;
	guint ii;

	g_return_val_if_fail (E_IS_CAL_TABLE_LIST_BASE (self), NULL);

	rows = e_virtual_tree_get_selected_rows (self->priv->vtree);

	for (ii = 0; ii < rows->len; ii++) {
		list = g_slist_prepend (list, g_ptr_array_index (rows, ii));
	}

	g_ptr_array_unref (rows);

	return list;
}

ECalModelComponent *
e_cal_table_list_base_get_selected_comp (ECalTableListBase *self)
{
	GPtrArray *rows;
	ECalModelComponent *comp_data = NULL;

	g_return_val_if_fail (E_IS_CAL_TABLE_LIST_BASE (self), NULL);

	rows = e_virtual_tree_get_selected_rows (self->priv->vtree);

	if (rows->len == 1)
		comp_data = E_CAL_MODEL_COMPONENT (g_ptr_array_index (rows, 0));

	g_ptr_array_unref (rows);

	return comp_data;
}

GtkTargetList *
e_cal_table_list_base_get_copy_target_list (ECalTableListBase *self)
{
	g_return_val_if_fail (E_IS_CAL_TABLE_LIST_BASE (self), NULL);

	return self->priv->copy_target_list;
}

GtkTargetList *
e_cal_table_list_base_get_paste_target_list (ECalTableListBase *self)
{
	g_return_val_if_fail (E_IS_CAL_TABLE_LIST_BASE (self), NULL);

	return self->priv->paste_target_list;
}

EPrintable *
e_cal_table_list_base_get_printable (ECalTableListBase *self)
{
	g_return_val_if_fail (E_IS_CAL_TABLE_LIST_BASE (self), NULL);

	return e_virtual_tree_get_printable (self->priv->vtree);
}

void
e_cal_table_list_base_set_search_active (ECalTableListBase *self,
					 gboolean search_active)
{
	g_return_if_fail (E_IS_CAL_TABLE_LIST_BASE (self));

	search_active = !!search_active;

	if ((self->priv->search_active ? 1 : 0) == (search_active ? 1 : 0))
		return;

	self->priv->search_active = search_active;

	cal_table_list_base_update_empty_message (self);
}

gboolean
e_cal_table_list_base_get_search_active (ECalTableListBase *self)
{
	g_return_val_if_fail (E_IS_CAL_TABLE_LIST_BASE (self), FALSE);

	return self->priv->search_active;
}

void
e_cal_table_list_base_emit_open_component (ECalTableListBase *self,
					   ECalModelComponent *comp_data)
{
	g_return_if_fail (E_IS_CAL_TABLE_LIST_BASE (self));

	g_signal_emit (self, signals[OPEN_COMPONENT], 0, comp_data);
}

void
e_cal_table_list_base_emit_popup_event (ECalTableListBase *self,
					GdkEvent *event)
{
	g_return_if_fail (E_IS_CAL_TABLE_LIST_BASE (self));

	g_signal_emit (self, signals[POPUP_EVENT], 0, event);
}
