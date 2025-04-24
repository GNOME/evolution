/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "e-alert-dialog.h"
#include "e-misc-utils.h"
#include "e-ui-action.h"
#include "e-ui-customizer.h"
#include "e-ui-manager.h"

#include "e-ui-customize-dialog.h"

#define WEIGHT_NORMAL 400
#define WEIGHT_BOLD 800

/**
 * SECTION: e-ui-customize-dialog
 * @include: e-util/e-util.h
 * @short_description: a UI customize dialog
 *
 * The #EUICustomizeDialog is used to customize the UI.
 *
 * Use e_ui_customize_dialog_add_customizer() to add all the relevant
 * customizers before showing the dialog with e_ui_customize_dialog_run().
 *
 * Since: 3.56
 **/

struct _EUICustomizeDialog {
	GtkDialog parent;

	GtkComboBox *element_combo;		/* not referenced */
	GtkTreeView *actions_tree_view;		/* not referenced */
	GtkNotebook *notebook;			/* not referenced */
	GtkTreeView *layout_tree_view;		/* not referenced */
	GtkWidget *remove_button;		/* not referenced */
	GtkWidget *top_button;			/* not referenced */
	GtkWidget *up_button;			/* not referenced */
	GtkWidget *down_button;			/* not referenced */
	GtkWidget *bottom_button;		/* not referenced */
	GtkWidget *layout_default_button;	/* not referenced */
	GtkLabel *shortcuts_label;		/* not referenced */
	GtkLabel *shortcuts_tooltip;		/* not referenced */
	GtkWidget *shortcuts_default_button;	/* not referenced */
	GtkBox *shortcuts_box;			/* not referenced */

	GPtrArray *customizers; /* EUICustomizer * */
	GHashTable *save_customizers;

	/* which customizer the actions_tree_view are filled for */
	EUICustomizer *actions_for_customizer; /* not referenced */
	guint actions_for_kind;

	/* drag & drop */
	guint autoscroll_id;
	GPtrArray *layout_drag_refs; /* GtkTreeRowReference */

	gulong actions_selection_changed_id;

	GPtrArray *shortcut_entries; /* GtkEntry * */
	GHashTable *all_shortcuts; /* EUIManagerShortcutDef ~> GPtrArray { ShortcutInfo } */
};

G_DEFINE_TYPE (EUICustomizeDialog, e_ui_customize_dialog, GTK_TYPE_DIALOG)

enum {
	COL_COMBO_ID_STR = 0,
	COL_COMBO_DISPLAY_NAME_STR,
	COL_COMBO_CUSTOMIZER_OBJ,
	COL_COMBO_CHANGED_BOOL,
	COL_COMBO_ELEM_KIND_UINT,
	COL_COMBO_DEFAULT_BOOL,
	N_COL_COMBO
};

enum {
	COL_ACTIONS_ELEM_OBJ = 0,
	COL_ACTIONS_NAME_STR,
	COL_ACTIONS_LABEL_STR,
	COL_ACTIONS_TOOLTIP_STR,
	COL_ACTIONS_MARKUP_STR,
	N_COL_ACTIONS
};

enum {
	COL_LAYOUT_ELEM_OBJ = 0,
	COL_LAYOUT_LABEL_STR,
	COL_LAYOUT_CAN_DRAG_BOOL,
	COL_LAYOUT_WEIGHT_INT,
	N_COL_LAYOUT
};

#define SHORTCUTS_PAGE_NUM 1
#define SHORTCUTS_INDEX_KEY "shortcut-index"
#define SHORTCUTS_ACCEL_NAME_KEY "accel-name"

#define DRAG_TARGET_ACTION "EUICustomizeDialogAction"
#define DRAG_TARGET_LAYOUT "EUICustomizeDialogLayout"

static GdkAtom drag_target_action_atom = GDK_NONE;
static GdkAtom drag_target_layout_atom = GDK_NONE;

typedef struct _ShortcutInfo {
	EUICustomizer *customizer;
	EUIAction *action;
	gchar *label;
} ShortcutInfo;

static ShortcutInfo *
shortcut_info_new (EUICustomizer *customizer,
		   EUIAction *action,
		   const gchar *label)
{
	ShortcutInfo *si;

	si = g_new0 (ShortcutInfo, 1);
	si->customizer = g_object_ref (customizer);
	si->action = g_object_ref (action);
	si->label = g_strdup (label);

	return si;
}

static void
shortcut_info_free (gpointer ptr)
{
	ShortcutInfo *si = ptr;

	if (si) {
		g_clear_object (&si->customizer);
		g_clear_object (&si->action);
		g_free (si->label);
		g_free (si);
	}
}

static void
customize_layout_update_buttons (EUICustomizeDialog *self)
{
	GtkTreeSelection *selection;
	gboolean can_up = FALSE, can_down = FALSE;
	gint n_selected;

	selection = gtk_tree_view_get_selection (self->layout_tree_view);
	n_selected = gtk_tree_selection_count_selected_rows (selection);

	if (n_selected > 0) {
		GList *selected, *link;
		GtkTreeModel *model = NULL;
		guint checked_can_drag = 0;

		can_up = TRUE;
		can_down = TRUE;
		selected = gtk_tree_selection_get_selected_rows (selection, &model);

		for (link = selected; link && (can_up || can_down); link = g_list_next (link)) {
			GtkTreePath *path = link->data;
			GtkTreeIter iter;

			if (gtk_tree_model_get_iter (model, &iter, path)) {
				GtkTreeIter iter2 = iter;
				gboolean can_drag = FALSE;

				gtk_tree_model_get (model, &iter, COL_LAYOUT_CAN_DRAG_BOOL, &can_drag, -1);

				if (can_drag)
					checked_can_drag++;

				can_up = can_up && gtk_tree_model_iter_previous (model, &iter2);
				can_down = can_down && gtk_tree_model_iter_next (model, &iter);
			}
		}

		g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);

		can_up = can_up && checked_can_drag > 0;
		can_down = can_down && checked_can_drag > 0;
	}

	gtk_widget_set_sensitive (self->remove_button, n_selected > 0);
	gtk_widget_set_sensitive (self->top_button, can_up);
	gtk_widget_set_sensitive (self->up_button, can_up);
	gtk_widget_set_sensitive (self->down_button, can_down);
	gtk_widget_set_sensitive (self->bottom_button, can_down);
}

static void
customize_layout_store_level (GtkTreeModel *model,
			      GtkTreeIter *parent_iter,
			      EUIElement *parent_elem)
{
	GtkTreeIter iter;

	if (!gtk_tree_model_iter_children (model, &iter, parent_iter))
		return;

	do {
		EUIElement *elem = NULL;

		gtk_tree_model_get (model, &iter, COL_LAYOUT_ELEM_OBJ, &elem, -1);

		/* the non-NULL copy is consumed by the parent_elem */
		if (elem) {
			/* if in the header bar, then use the order from the .eui file */
			if (e_ui_element_get_kind (elem) == E_UI_ELEMENT_KIND_ITEM)
				e_ui_element_item_set_order (elem, 0);

			e_ui_element_add_child (parent_elem, elem);

			customize_layout_store_level (model, &iter, elem);
		}
	} while (gtk_tree_model_iter_next (model, &iter));
}

static void
customize_layout_store_changes (EUICustomizeDialog *self)
{
	EUICustomizer *customizer = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	EUIElement *parent, *root;
	EUIParser *parser;
	gchar *id = NULL;

	if (!gtk_combo_box_get_active_iter (self->element_combo, &iter)) {
		g_warn_if_reached ();
		return;
	}

	model = gtk_combo_box_get_model (self->element_combo);

	gtk_tree_model_get (model, &iter,
		COL_COMBO_ID_STR, &id,
		COL_COMBO_CUSTOMIZER_OBJ, &customizer,
		-1);

	if (!id || !customizer) {
		g_clear_object (&customizer);
		g_free (id);
		g_warn_if_reached ();
		return;
	}

	parser = e_ui_manager_get_parser (e_ui_customizer_get_manager (customizer));
	root = e_ui_parser_get_root (parser);
	parent = e_ui_element_get_child_by_id (root, id);
	parent = e_ui_element_copy (parent);

	model = gtk_tree_view_get_model (self->layout_tree_view);
	customize_layout_store_level (model, NULL, parent);

	parser = e_ui_customizer_get_parser (customizer);
	root = e_ui_parser_get_root (parser);
	if (root)
		e_ui_element_remove_child_by_id (root, id);
	else
		root = e_ui_parser_create_root (parser);
	e_ui_element_add_child (root, parent);

	g_clear_object (&customizer);
	g_free (id);
}

static void
customize_layout_changed (EUICustomizeDialog *self)
{
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter (self->element_combo, &iter)) {
		GtkTreeModel *model;

		model = gtk_combo_box_get_model (self->element_combo);

		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			COL_COMBO_DEFAULT_BOOL, FALSE,
			COL_COMBO_CHANGED_BOOL, TRUE,
			-1);

		gtk_widget_set_sensitive (self->layout_default_button, TRUE);
		customize_layout_store_changes (self);
	}
}

static void
customize_shortcuts_changed (EUICustomizeDialog *self)
{
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter (self->element_combo, &iter)) {
		GtkTreeModel *model;

		model = gtk_combo_box_get_model (self->element_combo);

		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			COL_COMBO_CHANGED_BOOL, TRUE,
			-1);

		gtk_widget_set_sensitive (self->shortcuts_default_button, TRUE);
	}
}

static void
customize_shortcuts_add_to_all (GHashTable *all_shortcuts,
				EUICustomizer *customizer,
				EUIAction *action,
				const gchar *label,
				const gchar *accel)
{
	EUIManagerShortcutDef sd;
	GPtrArray *array;

	if (!accel || !*accel)
		return;

	gtk_accelerator_parse (accel, &sd.key, &sd.mods);

	if (!sd.key)
		return;

	array = g_hash_table_lookup (all_shortcuts, &sd);
	if (!array) {
		EUIManagerShortcutDef *sd_ptr;

		sd_ptr = g_new0 (EUIManagerShortcutDef, 1);
		sd_ptr->key = sd.key;
		sd_ptr->mods = sd.mods;

		array = g_ptr_array_new_with_free_func (shortcut_info_free);
		g_hash_table_insert (all_shortcuts, sd_ptr, array);
	}

	g_ptr_array_add (array, shortcut_info_new (customizer, action, label));
}

static void
customize_shortcuts_remove_from_all (GHashTable *all_shortcuts,
				     EUICustomizer *customizer,
				     EUIAction *action,
				     const gchar *label,
				     const gchar *accel)
{
	EUIManagerShortcutDef sd;
	GPtrArray *array;
	guint ii;

	if (!accel || !*accel)
		return;

	gtk_accelerator_parse (accel, &sd.key, &sd.mods);

	if (!sd.key)
		return;

	array = g_hash_table_lookup (all_shortcuts, &sd);
	if (!array)
		return;

	for (ii = 0; ii < array->len; ii++) {
		ShortcutInfo *nfo = g_ptr_array_index (array, ii);

		if (nfo->customizer == customizer && nfo->action == action) {
			g_ptr_array_remove_index (array, ii);
			break;
		}
	}

	if (!array->len)
		g_hash_table_remove (all_shortcuts, &sd);
}

static void
customize_shortcuts_traverse (GHashTable *all_shortcuts,
			      EUICustomizer *customizer,
			      EUIAction *action,
			      const gchar *label,
			      void (* func) (GHashTable *all_shortcuts,
					     EUICustomizer *customizer,
					     EUIAction *action,
					     const gchar *label,
					     const gchar *accel))
{
	const gchar *action_name = g_action_get_name (G_ACTION (action));
	const gchar *main_accel = NULL;
	GPtrArray *secondary_accels;

	secondary_accels = e_ui_customizer_get_accels (customizer, action_name);
	if (!secondary_accels) {
		main_accel = e_ui_action_get_accel (action);
		secondary_accels = e_ui_action_get_secondary_accels (action);
	}

	if (!main_accel && (!secondary_accels || !secondary_accels->len))
		return;

	func (all_shortcuts, customizer, action, label, main_accel);

	if (secondary_accels) {
		guint ii;

		for (ii = 0; ii < secondary_accels->len; ii++) {
			const gchar *accel = g_ptr_array_index (secondary_accels, ii);

			func (all_shortcuts, customizer, action, label, accel);
		}
	}
}

static void
customize_shortcuts_fill_current (EUICustomizeDialog *self)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (self->actions_tree_view);

	g_signal_emit_by_name (selection, "changed", 0, NULL);
}

static void
customize_shortcuts_action_take_accels (EUICustomizeDialog *self,
					EUICustomizer *customizer,
					const gchar *action_name,
					GPtrArray *accels) /* transfer full */
{
	EUIAction *action;
	gchar *label;

	action = e_ui_manager_get_action (e_ui_customizer_get_manager (customizer), action_name);
	g_return_if_fail (action != NULL);

	label = e_str_without_underscores (e_ui_action_get_label (action));

	customize_shortcuts_traverse (self->all_shortcuts, customizer, action, label, customize_shortcuts_remove_from_all);
	e_ui_customizer_take_accels (customizer, g_action_get_name (G_ACTION (action)), accels);
	customize_shortcuts_traverse (self->all_shortcuts, customizer, action, label, customize_shortcuts_add_to_all);

	g_free (label);
}

typedef struct _UnsetShortcutData {
	EUICustomizeDialog *self;
	gchar *action_name; /* the one to receive the shortcut */
	guint n_clashing;
	EUIManagerShortcutDef shortcut;
} UnsetShortcutData;

static void
unset_shortcut_data_free (UnsetShortcutData *usd)
{
	if (usd) {
		g_free (usd->action_name);
		g_free (usd);
	}
}

static UnsetShortcutData *
unset_shortcut_data_copy (UnsetShortcutData *src)
{
	UnsetShortcutData *usd;

	if (!src)
		return NULL;

	usd = g_new0 (UnsetShortcutData, 1);
	usd->self = src->self;
	usd->action_name = g_strdup (src->action_name);
	usd->n_clashing = src->n_clashing;
	usd->shortcut = src->shortcut;

	return usd;
}

static void
customize_shortcuts_unset_from_other_cb (GtkWidget *menu_item,
					 gpointer user_data)
{
	UnsetShortcutData *usd = user_data;
	GPtrArray *used_shortcut_array;
	guint key;
	GdkModifierType mods;
	guint ii, sz;

	used_shortcut_array = g_hash_table_lookup (usd->self->all_shortcuts, &usd->shortcut);
	sz = used_shortcut_array ? used_shortcut_array->len : 0;

	for (ii = 0; ii < sz; ii++) {
		ShortcutInfo *nfo = g_ptr_array_index (used_shortcut_array, ii);
		GPtrArray *accels;
		GPtrArray *new_accels;
		const gchar *action_name;
		guint jj;

		action_name = g_action_get_name (G_ACTION (nfo->action));

		if (g_strcmp0 (usd->action_name, action_name) == 0)
			continue;

		new_accels = g_ptr_array_new_with_free_func (g_free);

		action_name = g_action_get_name (G_ACTION (nfo->action));
		accels = e_ui_customizer_get_accels (nfo->customizer, action_name);

		if (accels) {
			for (jj = 0; jj < accels->len; jj++) {
				const gchar *accel = g_ptr_array_index (accels, jj);

				key = 0;
				gtk_accelerator_parse (accel, &key, &mods);

				if (key && (key != usd->shortcut.key || mods != usd->shortcut.mods))
					g_ptr_array_add (new_accels, g_strdup (accel));
			}
		} else {
			const gchar *accel;

			accel = e_ui_action_get_accel (nfo->action);
			if (accel) {
				key = 0;
				gtk_accelerator_parse (accel, &key, &mods);

				if (key && (key != usd->shortcut.key || mods != usd->shortcut.mods))
					g_ptr_array_add (new_accels, g_strdup (accel));
			}

			accels = e_ui_action_get_secondary_accels (nfo->action);
			for (jj = 0; accels && jj < accels->len; jj++) {
				accel = g_ptr_array_index (accels, jj);

				key = 0;
				gtk_accelerator_parse (accel, &key, &mods);

				if (key && (key != usd->shortcut.key || mods != usd->shortcut.mods))
					g_ptr_array_add (new_accels, g_strdup (accel));
			}
		}

		if (!g_hash_table_contains (usd->self->save_customizers, nfo->customizer))
			g_hash_table_add (usd->self->save_customizers, g_object_ref (nfo->customizer));

		/* this frees `nfo` and can also free `used_shortcut_array` */
		customize_shortcuts_action_take_accels (usd->self, nfo->customizer, action_name, g_steal_pointer (&new_accels));
		ii--;
		sz--;
	}

	customize_shortcuts_fill_current (usd->self);
}

static void
customize_shortcuts_entry_icon_press_cb (GtkEntry *entry,
					 GtkEntryIconPosition icon_pos,
					 GdkEvent *event,
					 gpointer user_data)
{
	UnsetShortcutData *usd = user_data;
	GtkMenu *menu;
	GtkWidget *item;
	GtkWidget *widget;

	if (icon_pos != GTK_ENTRY_ICON_SECONDARY || !usd)
		return;

	menu = GTK_MENU (gtk_menu_new ());

	widget = gtk_label_new (gtk_entry_get_icon_tooltip_text (entry, icon_pos));
	g_object_set (widget,
		"use-underline", FALSE,
		"visible", TRUE,
		"wrap", TRUE,
		"width-chars", 30,
		"max-width-chars", 40,
		NULL);

	item = gtk_menu_item_new ();
	g_object_set (item,
		"visible", TRUE,
		"sensitive", FALSE,
		NULL);
	gtk_container_add (GTK_CONTAINER (item), widget);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_menu_item_new_with_mnemonic (g_dngettext (GETTEXT_PACKAGE, "_Unset from other action", "_Unset from other actions", usd->n_clashing));
	g_object_set (item,
		"visible", TRUE,
		NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	g_signal_connect_data (item, "activate",
		G_CALLBACK (customize_shortcuts_unset_from_other_cb), unset_shortcut_data_copy (usd), (GClosureNotify) unset_shortcut_data_free, 0);

	gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (entry), NULL);
	e_util_connect_menu_detach_after_deactivate (GTK_MENU (menu));

	gtk_menu_popup_at_pointer (GTK_MENU (menu), event);
}

static void
customize_shortcuts_check_existing (EUICustomizeDialog *self, /* NULL to _not_ add popup menu */
				    const gchar *action_name,
				    GHashTable *all_shortcuts, /* EUIManagerShortcutDef ~> GPtrArray { ShortcutInfo } */
				    guint key,
				    GdkModifierType mods,
				    GtkEntry *entry)
{
	gboolean any_set = FALSE;

	g_signal_handlers_disconnect_matched (entry, G_SIGNAL_MATCH_FUNC, 0,0, NULL, G_CALLBACK (customize_shortcuts_entry_icon_press_cb), NULL);

	if (all_shortcuts && key) {
		EUIManagerShortcutDef s_def = { 0, };
		GPtrArray *used_shortcut_array;

		s_def.key = key;
		s_def.mods = mods;

		used_shortcut_array = g_hash_table_lookup (all_shortcuts, &s_def);

		if (used_shortcut_array && used_shortcut_array->len) {
			guint ii, n_clashing = 0;

			for (ii = 0; ii < used_shortcut_array->len; ii++) {
				ShortcutInfo *nfo = g_ptr_array_index (used_shortcut_array, ii);

				if (g_strcmp0 (action_name, g_action_get_name (G_ACTION (nfo->action))) != 0)
					n_clashing++;
			}

			if (n_clashing > 0) {
				gchar *text;

				if (n_clashing > 1) {
					/* Translators: the count is always more than one */
					text = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
						"Shortcut is used by other %u action",
						"Shortcut is used by other %u actions",
						n_clashing), n_clashing);
				} else /* n_clashing == 1 */ {
					ShortcutInfo *nfo = g_ptr_array_index (used_shortcut_array, 0);
					if (used_shortcut_array->len > 1 &&
					    g_strcmp0 (action_name, g_action_get_name (G_ACTION (nfo->action))) == 0)
						nfo = g_ptr_array_index (used_shortcut_array, 1);

					text = g_strdup_printf (_("Shortcut is used by action “%s”"), nfo->label);
				}

				gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, "dialog-warning");
				gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY, text);

				if (self) {
					UnsetShortcutData *usd;

					usd = g_new0 (UnsetShortcutData, 1);
					usd->self = self;
					usd->action_name = g_strdup (action_name);
					usd->n_clashing = n_clashing;
					usd->shortcut.key = key;
					usd->shortcut.mods = mods;

					g_signal_connect_data (entry, "icon-press",
						G_CALLBACK (customize_shortcuts_entry_icon_press_cb), g_steal_pointer (&usd),
						(GClosureNotify) unset_shortcut_data_free, 0);
				}

				any_set = TRUE;

				g_free (text);
			}
		}
	}

	if (!any_set) {
		gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
		gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
	}
}

static gboolean
customize_shortcuts_dup_customizer_and_action_name (EUICustomizeDialog *self,
						    EUICustomizer **out_customizer,
						    gchar **out_action_name)
{
	GtkTreeIter customizer_iter;
	GtkTreeIter actions_iter;
	GtkTreeModel *model = NULL;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (self->actions_tree_view);

	if (gtk_tree_selection_get_selected (selection, &model, &actions_iter) &&
	    gtk_combo_box_get_active_iter (self->element_combo, &customizer_iter)) {
		if (out_customizer) {
			gtk_tree_model_get (gtk_combo_box_get_model (self->element_combo), &customizer_iter,
				COL_COMBO_CUSTOMIZER_OBJ, out_customizer,
				-1);
		}

		if (out_action_name) {
			gtk_tree_model_get (model, &actions_iter,
				COL_ACTIONS_NAME_STR, out_action_name,
				-1);
		}

		return TRUE;
	}

	return FALSE;
}

static void
customize_shortcuts_save_new_shortcut (EUICustomizeDialog *self,
				       EUICustomizer *customizer,
				       const gchar *action_name,
				       GPtrArray *existing,
				       const gchar *shortcut,
				       gpointer user_data)
{
	EUIAction *action;

	action = e_ui_manager_get_action (e_ui_customizer_get_manager (customizer), action_name);

	if (action && shortcut) {
		GPtrArray *new_shortcuts = NULL;

		if (!existing) {
			new_shortcuts = g_ptr_array_new_with_free_func (g_free);
			existing = new_shortcuts;
		}

		g_ptr_array_add (existing, g_strdup (shortcut));

		customize_shortcuts_action_take_accels (self, customizer, action_name, g_ptr_array_ref (existing));

		customize_shortcuts_changed (self);
		customize_shortcuts_fill_current (self);

		g_clear_pointer (&new_shortcuts, g_ptr_array_unref);
	}
}

static void
customize_shortcuts_save_edit_shortcut (EUICustomizeDialog *self,
					EUICustomizer *customizer,
					const gchar *action_name,
					GPtrArray *existing,
					const gchar *shortcut,
					gpointer user_data)
{
	guint index = GPOINTER_TO_UINT (user_data);

	if (shortcut) {
		GtkEntry *entry = g_ptr_array_index (self->shortcut_entries, index);
		gchar *label;
		guint key = 0;
		GdkModifierType mods = 0;

		gtk_accelerator_parse (shortcut, &key, &mods);
		label = gtk_accelerator_get_label (key, mods);
		gtk_entry_set_text (entry, label);
		g_object_set_data_full (G_OBJECT (entry), SHORTCUTS_ACCEL_NAME_KEY, g_strdup (shortcut), g_free);
		g_free (label);

		g_free (existing->pdata[index]);
		existing->pdata[index] = g_strdup (shortcut);

		customize_shortcuts_action_take_accels (self, customizer, action_name, g_ptr_array_ref (existing));

		customize_shortcuts_changed (self);
		customize_shortcuts_fill_current (self);
	}
}

typedef void (* ShortcutsSaveFunc) (EUICustomizeDialog *self,
				    EUICustomizer *customizer,
				    const gchar *action_name,
				    GPtrArray *existing,
				    const gchar *shortcut,
				    gpointer user_data);

typedef struct _ShortcutsData {
	EUICustomizeDialog *self;
	EUICustomizer *customizer;
	GHashTable *all_shortcuts;
	gchar *action_name;
	ShortcutsSaveFunc save_func;
	gpointer save_func_user_data;
	GtkPopover *popover;
	GtkEntry *entry;
	GtkWidget *save_button;
	GPtrArray *existing; /* gchar * */
	guint current_key;
	GdkModifierType current_mods;
	gboolean save_current;
} ShortcutsData;

static void
shortcuts_data_free (ShortcutsData *sd)
{
	if (!sd)
		return;

	g_clear_pointer (&sd->existing, g_ptr_array_unref);
	g_clear_pointer (&sd->all_shortcuts, g_hash_table_unref);
	g_clear_object (&sd->popover);
	g_clear_object (&sd->customizer);
	g_free (sd->action_name);
	g_free (sd);
}

static gboolean
customize_shortcuts_popover_key_press_cb (GtkWidget *widget,
					  GdkEventKey *event,
					  gpointer user_data)
{
	ShortcutsData *sd = user_data;
	guint key;
	GdkModifierType mods;
	gboolean consumed = FALSE;

	switch (event->keyval) {
	case GDK_KEY_Num_Lock:
	case GDK_KEY_Shift_L:
	case GDK_KEY_Shift_R:
	case GDK_KEY_Control_L:
	case GDK_KEY_Control_R:
	case GDK_KEY_Caps_Lock:
	case GDK_KEY_Shift_Lock:
	case GDK_KEY_Meta_L:
	case GDK_KEY_Meta_R:
	case GDK_KEY_Alt_L:
	case GDK_KEY_Alt_R:
	case GDK_KEY_Super_L:
	case GDK_KEY_Super_R:
	case GDK_KEY_Hyper_L:
	case GDK_KEY_Hyper_R:
		return FALSE;
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
	case GDK_KEY_ISO_Enter:
		if (!(event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK))) {
			gtk_button_clicked (GTK_BUTTON (sd->save_button));
			return TRUE;
		}
		return FALSE;
	}

	key = event->keyval;
	mods = (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK));

	if (key != GDK_KEY_Tab && key != GDK_KEY_ISO_Left_Tab && key != GDK_KEY_KP_Tab && key != GDK_KEY_Escape && (
	    sd->current_key != key || sd->current_mods != mods)) {
		gchar *name;
		gchar *label;
		guint ii;

		name = gtk_accelerator_name (key, mods);
		/* to normalize the key when Shift is pressed (which uses upper case letter here, but lowercase letter in the parse) */
		gtk_accelerator_parse (name, &key, &mods);

		sd->current_key = key;
		sd->current_mods = mods;

		label = gtk_accelerator_get_label (key, mods);
		gtk_entry_set_text (sd->entry, label ? label : "");
		g_object_set_data_full (G_OBJECT (sd->entry), SHORTCUTS_ACCEL_NAME_KEY, g_steal_pointer (&name), g_free);

		g_free (label);

		for (ii = 0; sd->existing && ii < sd->existing->len; ii++) {
			const gchar *shortcut = g_ptr_array_index (sd->existing, ii);

			gtk_accelerator_parse (shortcut, &key, &mods);

			if (key == sd->current_key && mods == sd->current_mods)
				break;
		}

		/* allow saving only shortcuts not in the list yet */
		gtk_widget_set_sensitive (sd->save_button, !sd->existing || ii >= sd->existing->len);

		if (sd->all_shortcuts && gtk_widget_get_sensitive (sd->save_button))
			customize_shortcuts_check_existing (NULL, sd->action_name, sd->all_shortcuts, sd->current_key, sd->current_mods, sd->entry);

		consumed = TRUE;
	}

	return consumed;
}

static void
customize_shortcuts_popover_save_clicked_cb (GtkWidget *button,
					     gpointer user_data)
{
	ShortcutsData *sd = user_data;

	sd->save_current = sd->current_key != 0;

	gtk_widget_set_visible (GTK_WIDGET (sd->popover), FALSE);
}

static void
customize_shortcuts_popover_closed_cb (GtkWidget *popover,
				       gpointer user_data)
{
	ShortcutsData *sd = user_data;

	if (sd->save_current) {
		gchar *shortcut;

		shortcut = gtk_accelerator_name (sd->current_key, sd->current_mods);
		sd->save_func (sd->self, sd->customizer, sd->action_name, sd->existing, shortcut, sd->save_func_user_data);
		g_free (shortcut);
	}

	gtk_widget_destroy (popover);
}

static void
customize_shortcuts_run_popover (EUICustomizeDialog *self,
				 GtkWidget *for_widget,
				 GPtrArray *existing, /* gchar * */
				 EUICustomizer *customizer,
				 gchar *action_name, /* (transfer full) */
				 ShortcutsSaveFunc save_func,
				 gpointer save_func_user_data)
{
	ShortcutsData *sd;
	GtkPopover *popover;
	GtkWidget *widget;
	GtkBox *box, *box2;

	sd = g_new0 (ShortcutsData, 1);

	sd->self = self;
	sd->customizer = g_object_ref (customizer);
	sd->all_shortcuts = self->all_shortcuts ? g_hash_table_ref (self->all_shortcuts) : NULL;
	sd->action_name = action_name; /* assumes ownership */
	sd->save_func = save_func;
	sd->save_func_user_data = save_func_user_data;
	sd->existing = existing ? g_ptr_array_ref (existing) : NULL;

	widget = gtk_popover_new (for_widget);
	popover = GTK_POPOVER (widget);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
	box = GTK_BOX (widget);

	widget = gtk_label_new (_("Press the keys to be used as a shortcut."));
	g_object_set (widget,
		"wrap", TRUE,
		NULL);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	widget = gtk_entry_new ();
	g_object_set (widget,
		"halign", GTK_ALIGN_CENTER,
		"hexpand", TRUE,
		"editable", FALSE,
		NULL);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	sd->entry = GTK_ENTRY (widget);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	g_object_set (widget,
		"halign", GTK_ALIGN_CENTER,
		NULL);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	box2 = GTK_BOX (widget);

	widget = gtk_button_new_with_label (_("Save"));
	gtk_box_pack_start (box2, widget, FALSE, FALSE, 0);
	sd->save_button = widget;
	gtk_widget_set_sensitive (sd->save_button, FALSE);
	g_signal_connect (widget, "clicked",
		G_CALLBACK (customize_shortcuts_popover_save_clicked_cb), sd);

	widget = gtk_button_new_with_label (_("Cancel"));
	gtk_box_pack_start (box2, widget, FALSE, FALSE, 0);
	g_signal_connect_object (widget, "clicked",
		G_CALLBACK (gtk_popover_popdown), popover, G_CONNECT_SWAPPED);

	gtk_container_add (GTK_CONTAINER (popover), GTK_WIDGET (box));
	gtk_container_set_border_width (GTK_CONTAINER (popover), 6);
	gtk_popover_set_position (popover, GTK_POS_BOTTOM);
	gtk_popover_set_modal (popover, TRUE);

	widget = GTK_WIDGET (popover);

	sd->popover = g_object_ref_sink (popover);
	sd->current_key = 0;
	sd->current_mods = 0;
	sd->save_current = FALSE;

	g_signal_connect (sd->entry, "key-press-event",
		G_CALLBACK (customize_shortcuts_popover_key_press_cb), sd);
	g_signal_connect_data (popover, "closed",
		G_CALLBACK (customize_shortcuts_popover_closed_cb), sd, (GClosureNotify) shortcuts_data_free, 0);

	gtk_widget_show_all (GTK_WIDGET (box));
	gtk_popover_popup (popover);
}

static void
customize_shortcuts_add_clicked_cb (GtkWidget *button,
				    gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	EUICustomizer *customizer = NULL;
	gchar *action_name = NULL;

	if (customize_shortcuts_dup_customizer_and_action_name (self, &customizer, &action_name)) {
		GPtrArray *shortcuts;
		guint ii;

		shortcuts = g_ptr_array_new_full (self->shortcut_entries->len + 1, g_free);

		for (ii = 0; ii < self->shortcut_entries->len; ii++) {
			GObject *entry = g_ptr_array_index (self->shortcut_entries, ii);
			const gchar *accel_name = g_object_get_data (entry, SHORTCUTS_ACCEL_NAME_KEY);

			if (accel_name && *accel_name)
				g_ptr_array_add (shortcuts, g_strdup (accel_name));
		}

		customize_shortcuts_run_popover (self, button, shortcuts, customizer, g_steal_pointer (&action_name),
			customize_shortcuts_save_new_shortcut, NULL);

		g_clear_pointer (&shortcuts, g_ptr_array_unref);
		g_clear_object (&customizer);
	} else {
		g_warn_if_reached ();
	}
}

static void
customize_shortcuts_edit_clicked_cb (GtkWidget *button,
				     gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	EUICustomizer *customizer = NULL;
	gchar *action_name = NULL;
	guint index;

	index = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), SHORTCUTS_INDEX_KEY));
	g_return_if_fail (index < self->shortcut_entries->len);

	if (customize_shortcuts_dup_customizer_and_action_name (self, &customizer, &action_name)) {
		GPtrArray *shortcuts;
		guint ii;

		shortcuts = g_ptr_array_new_full (self->shortcut_entries->len, g_free);

		for (ii = 0; ii < self->shortcut_entries->len; ii++) {
			GObject *entry = g_ptr_array_index (self->shortcut_entries, ii);
			const gchar *accel_name = g_object_get_data (entry, SHORTCUTS_ACCEL_NAME_KEY);

			if (accel_name && *accel_name)
				g_ptr_array_add (shortcuts, g_strdup (accel_name));
		}

		customize_shortcuts_run_popover (self, button, shortcuts, customizer, g_steal_pointer (&action_name),
			customize_shortcuts_save_edit_shortcut, GUINT_TO_POINTER (index));

		g_clear_pointer (&shortcuts, g_ptr_array_unref);
		g_clear_object (&customizer);
	} else {
		g_warn_if_reached ();
	}
}

static void
customize_shortcuts_add_no_shortcut_box (EUICustomizeDialog *self)
{
	GtkWidget *widget;
	GtkBox *box;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start (self->shortcuts_box, widget, FALSE, FALSE, 2);

	box = GTK_BOX (widget);

	widget = gtk_label_new (_("No shortcut set"));
	gtk_widget_set_sensitive (widget, FALSE);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 4);

	widget = gtk_button_new_with_mnemonic (_("_Add"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	g_signal_connect (widget, "clicked",
		G_CALLBACK (customize_shortcuts_add_clicked_cb), self);

	gtk_widget_show_all (GTK_WIDGET (box));
}

static void
customize_shortcuts_remove_clicked_cb (GtkWidget *button,
				       gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	EUICustomizer *customizer = NULL;
	GtkWidget *parent;
	gchar *action_name = NULL;
	guint index;

	index = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), SHORTCUTS_INDEX_KEY));
	g_return_if_fail (index < self->shortcut_entries->len);

	parent = gtk_widget_get_parent (g_ptr_array_index (self->shortcut_entries, index));
	g_return_if_fail (parent != NULL);

	g_ptr_array_remove_index (self->shortcut_entries, index);
	gtk_container_remove (GTK_CONTAINER (self->shortcuts_box), parent);

	if (customize_shortcuts_dup_customizer_and_action_name (self, &customizer, &action_name)) {
		GPtrArray *shortcuts;

		shortcuts = g_ptr_array_new_full (self->shortcut_entries->len, g_free);
		for (index = 0; index < self->shortcut_entries->len; index++) {
			GObject *entry = g_ptr_array_index (self->shortcut_entries, index);
			const gchar *accel_name = g_object_get_data (entry, SHORTCUTS_ACCEL_NAME_KEY);

			if (accel_name && *accel_name)
				g_ptr_array_add (shortcuts, g_strdup (accel_name));
		}

		customize_shortcuts_action_take_accels (self, customizer, action_name, shortcuts);
		customize_shortcuts_changed (self);

		g_clear_object (&customizer);
		g_free (action_name);
	} else {
		g_warn_if_reached ();
	}

	if (self->shortcut_entries->len == 0)
		customize_shortcuts_add_no_shortcut_box (self);
	else if (index >= self->shortcut_entries->len) /* to add also the 'Add' button */
		customize_shortcuts_fill_current (self);
}

static void
customize_shortcuts_default_clicked_cb (GtkWidget *button,
					gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	EUICustomizer *customizer = NULL;
	gchar *action_name = NULL;

	if (!customize_shortcuts_dup_customizer_and_action_name (self, &customizer, &action_name)) {
		g_warn_if_reached ();
		return;
	}

	customize_shortcuts_action_take_accels (self, customizer, action_name, NULL);

	g_clear_object (&customizer);
	g_free (action_name);

	customize_shortcuts_changed (self);
	customize_shortcuts_fill_current (self);
}

static GtkWidget *
customize_shortcuts_new_accel_box (EUICustomizeDialog *self,
				   const gchar *action_name,
				   const gchar *accel_name,
				   gboolean is_last)
{
	GtkBox *box;
	GtkWidget *widget;
	gchar *accel_label = NULL;
	guint key = 0;
	GdkModifierType mods = 0;
	guint index = self->shortcut_entries->len;

	gtk_accelerator_parse (accel_name, &key, &mods);
	accel_label = gtk_accelerator_get_label (key, mods);

	box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));

	widget = gtk_entry_new ();
	g_object_set (widget,
		"text", accel_label ? accel_label : "",
		"editable", FALSE,
		NULL);
	g_object_set_data_full (G_OBJECT (widget), SHORTCUTS_ACCEL_NAME_KEY, g_strdup (accel_name), g_free);

	customize_shortcuts_check_existing (self, action_name, self->all_shortcuts, key, mods, GTK_ENTRY (widget));

	gtk_box_pack_start (box, widget, FALSE, FALSE, 8);

	g_ptr_array_add (self->shortcut_entries, widget);

	g_free (accel_label);

	widget = gtk_button_new_with_mnemonic (_("_Edit"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (widget), SHORTCUTS_INDEX_KEY, GUINT_TO_POINTER (index));
	g_signal_connect (widget, "clicked",
		G_CALLBACK (customize_shortcuts_edit_clicked_cb), self);

	widget = gtk_button_new_with_mnemonic (_("_Remove"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (widget), SHORTCUTS_INDEX_KEY, GUINT_TO_POINTER (index));
	g_signal_connect (widget, "clicked",
		G_CALLBACK (customize_shortcuts_remove_clicked_cb), self);

	if (is_last) {
		widget = gtk_button_new_with_mnemonic (_("_Add"));
		gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
		g_signal_connect (widget, "clicked",
			G_CALLBACK (customize_shortcuts_add_clicked_cb), self);
	}

	return GTK_WIDGET (box);
}

static void
customize_shortcuts_fill_accels (EUICustomizeDialog *self,
				 const gchar *action_name,
				 const gchar *main_accel,
				 GPtrArray *secondary_accels)
{
	GtkWidget *widget;
	guint n_accels, ii, offset;

	g_ptr_array_set_size (self->shortcut_entries, 0);
	offset = (main_accel ? 1 : 0);
	n_accels = offset + (secondary_accels ? secondary_accels->len : 0);

	if (main_accel) {
		widget = customize_shortcuts_new_accel_box (self, action_name, main_accel, n_accels <= 1);
		gtk_box_pack_start (self->shortcuts_box, widget, FALSE, FALSE, 2);
	}

	for (ii = 0; secondary_accels && ii < secondary_accels->len; ii++) {
		const gchar *accel = g_ptr_array_index (secondary_accels, ii);
		widget = customize_shortcuts_new_accel_box (self, action_name, accel, ii + offset + 1 >= n_accels);
		gtk_box_pack_start (self->shortcuts_box, widget, FALSE, FALSE, 2);
	}

	if (!n_accels)
		customize_shortcuts_add_no_shortcut_box (self);

	gtk_widget_show_all (GTK_WIDGET (self->shortcuts_box));
}

static void
customize_shortcut_action_selection_changed_cb (GtkTreeSelection *in_selection,
						gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	GtkTreeSelection *selection;
	GtkTreeModel *model = NULL;
	GtkTreeIter actions_iter;
	GtkTreeIter customizer_iter;
	GtkContainer *container;
	GList *children, *link;

	container = GTK_CONTAINER (self->shortcuts_box);
	children = gtk_container_get_children (container);
	for (link = children; link; link = g_list_next (link)) {
		gtk_container_remove (container, link->data);
	}
	g_list_free (children);

	selection = gtk_tree_view_get_selection (self->actions_tree_view);

	if (gtk_tree_selection_get_selected (selection, &model, &actions_iter) &&
	    gtk_combo_box_get_active_iter (self->element_combo, &customizer_iter)) {
		EUICustomizer *customizer = NULL;
		GPtrArray *accels;
		gchar *label = NULL, *name = NULL, *tooltip = NULL;

		gtk_tree_model_get (gtk_combo_box_get_model (self->element_combo), &customizer_iter,
			COL_COMBO_CUSTOMIZER_OBJ, &customizer,
			-1);

		gtk_tree_model_get (model, &actions_iter,
			COL_ACTIONS_NAME_STR, &name,
			COL_ACTIONS_LABEL_STR, &label,
			COL_ACTIONS_TOOLTIP_STR, &tooltip,
			-1);
		gtk_label_set_label (self->shortcuts_label, label);
		gtk_label_set_label (self->shortcuts_tooltip, tooltip);
		g_free (tooltip);
		g_free (label);

		gtk_widget_set_sensitive (GTK_WIDGET (self->shortcuts_label), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (self->shortcuts_box), TRUE);

		accels = e_ui_customizer_get_accels (customizer, name);
		if (accels) {
			customize_shortcuts_fill_accels (self, name, NULL, accels);
			gtk_widget_set_sensitive (self->shortcuts_default_button, TRUE);
		} else {
			EUIManager *manager;
			EUIAction *action;

			manager = e_ui_customizer_get_manager (customizer);
			action = e_ui_manager_get_action (manager, name);

			if (action) {
				customize_shortcuts_fill_accels (self, name,
					e_ui_action_get_accel (action),
					e_ui_action_get_secondary_accels (action));
			}

			gtk_widget_set_sensitive (self->shortcuts_default_button, FALSE);
		}

		g_clear_object (&customizer);
		g_free (name);
	} else {
		gtk_label_set_label (self->shortcuts_label, _("No action selected"));
		gtk_label_set_label (self->shortcuts_tooltip, "");
		gtk_widget_set_sensitive (GTK_WIDGET (self->shortcuts_label), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (self->shortcuts_box), FALSE);
		gtk_widget_set_sensitive (self->shortcuts_default_button, FALSE);
	}
}

static void
e_ui_customize_dialog_notebook_switch_page_cb (GtkNotebook *notebook,
					       GtkWidget *page,
					       guint page_num,
					       gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	GtkTreeSelection *selection;

	if (!self->shortcuts_box)
		return;

	selection = gtk_tree_view_get_selection (self->actions_tree_view);
	gtk_tree_selection_set_mode (selection, page_num == SHORTCUTS_PAGE_NUM ? GTK_SELECTION_SINGLE : GTK_SELECTION_MULTIPLE);

	if (page_num == SHORTCUTS_PAGE_NUM) {
		if (!self->actions_selection_changed_id) {
			self->actions_selection_changed_id = g_signal_connect_object (selection, "changed",
				G_CALLBACK (customize_shortcut_action_selection_changed_cb), self, 0);
		}
		customize_shortcut_action_selection_changed_cb (selection, self);
	} else if (self->actions_selection_changed_id) {
		g_signal_handler_disconnect (selection, self->actions_selection_changed_id);
		self->actions_selection_changed_id = 0;
	}
}

static void
customize_layout_add_actions (EUICustomizeDialog *self,
			      GtkTreeIter *parent,
			      gint position)
{
	GtkTreeModel *actions_model = NULL;
	GtkTreeSelection *selection;
	GtkTreeStore *tree_store;
	GList *selected_paths, *link;
	gboolean changed = FALSE;

	tree_store = GTK_TREE_STORE (gtk_tree_view_get_model (self->layout_tree_view));
	selection = gtk_tree_view_get_selection (self->actions_tree_view);
	selected_paths = gtk_tree_selection_get_selected_rows (selection, &actions_model);

	for (link = selected_paths; link; link = g_list_next (link)) {
		GtkTreePath *path = link->data;
		GtkTreeIter action_iter;

		if (gtk_tree_model_get_iter (actions_model, &action_iter, path)) {
			EUIElement *elem = NULL;
			gchar *label = NULL;

			gtk_tree_model_get (actions_model, &action_iter,
				COL_ACTIONS_ELEM_OBJ, &elem,
				COL_ACTIONS_LABEL_STR, &label,
				-1);

			if (elem) {
				GtkTreeIter store_iter;

				gtk_tree_store_insert (tree_store, &store_iter, parent, position);
				gtk_tree_store_set (tree_store, &store_iter,
					COL_LAYOUT_ELEM_OBJ, elem,
					COL_LAYOUT_LABEL_STR, label,
					COL_LAYOUT_CAN_DRAG_BOOL, TRUE,
					COL_LAYOUT_WEIGHT_INT, WEIGHT_NORMAL,
					-1);

				if (position != -1)
					position++;

				changed = TRUE;
			}

			e_ui_element_free (elem);
			g_free (label);
		}
	}

	g_list_free_full (selected_paths, (GDestroyNotify) gtk_tree_path_free);

	if (changed)
		customize_layout_changed (self);
}

static void
e_ui_customize_dialog_get_add_destination (EUICustomizeDialog *self,
					   GtkTreeIter *out_tmp_iter,
					   GtkTreeIter **out_parent,
					   gint *out_position)
{
	GtkTreeSelection *selection;
	gint n_selected, position = -1;

	*out_parent = NULL;

	selection = gtk_tree_view_get_selection (self->layout_tree_view);
	n_selected = gtk_tree_selection_count_selected_rows (selection);
	if (n_selected == 1) {
		GtkTreeModel *model = NULL;
		GList *selected_paths;

		selected_paths = gtk_tree_selection_get_selected_rows (selection, &model);

		if (selected_paths && gtk_tree_model_get_iter (model, out_tmp_iter, selected_paths->data)) {
			GtkTreeIter tmp_iter;

			if (gtk_tree_model_iter_has_child (model, out_tmp_iter)) {
				*out_parent = out_tmp_iter;
				position = -1;
			} else {
				tmp_iter = *out_tmp_iter;

				/* add after the selected item, instead of at the end */
				for (position = 1; gtk_tree_model_iter_previous (model, &tmp_iter); position++) {
				}

				if (gtk_tree_model_iter_parent (model, &tmp_iter, out_tmp_iter)) {
					*out_tmp_iter = tmp_iter;
					*out_parent = out_tmp_iter;
				}
			}
		}

		g_list_free_full (selected_paths, (GDestroyNotify) gtk_tree_path_free);
	}

	if (!*out_parent) {
		GtkTreeIter combo_iter;

		if (gtk_combo_box_get_active_iter (self->element_combo, &combo_iter)) {
			EUIElementKind for_kind = E_UI_ELEMENT_KIND_UNKNOWN;

			gtk_tree_model_get (gtk_combo_box_get_model (self->element_combo), &combo_iter,
				COL_COMBO_ELEM_KIND_UINT, &for_kind,
				-1);

			if (for_kind == E_UI_ELEMENT_KIND_HEADERBAR) {
				GtkTreeModel *layout_model;

				layout_model = gtk_tree_view_get_model (self->layout_tree_view);

				/* header bar has the start/end toplevels, it cannot have
				   anything out of these, thus put below the end */
				if (gtk_tree_model_get_iter_first (layout_model, out_tmp_iter) &&
				    gtk_tree_model_iter_next (layout_model, out_tmp_iter)) {
					*out_parent = out_tmp_iter;
					position = -1;
				}
			}
		}
	}

	*out_position = position;
}

static void
e_ui_customize_dialog_layout_add_selected_activated_cb (GtkButton *button,
							gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	GtkTreeSelection *selection;
	GtkTreeIter tmp_iter, *parent = NULL;
	gint n_selected, position = -1;

	selection = gtk_tree_view_get_selection (self->actions_tree_view);
	n_selected = gtk_tree_selection_count_selected_rows (selection);
	if (n_selected <= 0)
		return;

	e_ui_customize_dialog_get_add_destination (self, &tmp_iter, &parent, &position);
	customize_layout_add_actions (self, parent, position);
}

static void
e_ui_customize_dialog_layout_add_separator_activated_cb (GtkButton *button,
							 gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	EUIElement *elem;
	GtkTreeStore *tree_store;
	GtkTreeIter tmp_iter, *parent = NULL;
	GtkTreeIter store_iter;
	gint position = -1;

	e_ui_customize_dialog_get_add_destination (self, &tmp_iter, &parent, &position);

	elem = e_ui_element_new_separator ();

	tree_store = GTK_TREE_STORE (gtk_tree_view_get_model (self->layout_tree_view));
	gtk_tree_store_insert (tree_store, &store_iter, parent, position);
	gtk_tree_store_set (tree_store, &store_iter,
		COL_LAYOUT_ELEM_OBJ, elem,
		/* Translators: a "textual form" of a menu or toolbar separator, usually drawn as a line between the items  */
		COL_LAYOUT_LABEL_STR, _("Separator"),
		COL_LAYOUT_CAN_DRAG_BOOL, TRUE,
		COL_LAYOUT_WEIGHT_INT, WEIGHT_BOLD,
		-1);

	e_ui_element_free (elem);

	customize_layout_changed (self);
}

static void
e_ui_customize_dialog_layout_add_clicked_cb (GtkWidget *button,
					     gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	GtkWidget *popup_menu;
	GtkMenuShell *menu_shell;
	GtkWidget *item;
	GtkTreeSelection *selection;
	gint n_selected;

	selection = gtk_tree_view_get_selection (self->actions_tree_view);
	n_selected = gtk_tree_selection_count_selected_rows (selection);

	popup_menu = gtk_menu_new ();
	menu_shell = GTK_MENU_SHELL (popup_menu);

	item = gtk_menu_item_new_with_mnemonic (g_dngettext (GETTEXT_PACKAGE, _("Selected _Action"), _("Selected _Actions"), n_selected));
	g_signal_connect (item, "activate", G_CALLBACK (e_ui_customize_dialog_layout_add_selected_activated_cb), self);
	gtk_widget_set_sensitive (item, n_selected > 0);
	gtk_menu_shell_append (menu_shell, item);

	item = gtk_menu_item_new_with_mnemonic (_("_Separator"));
	g_signal_connect (item, "activate", G_CALLBACK (e_ui_customize_dialog_layout_add_separator_activated_cb), self);
	gtk_menu_shell_append (menu_shell, item);

	gtk_widget_show_all (popup_menu);

	gtk_menu_attach_to_widget (GTK_MENU (popup_menu), button, NULL);
	e_util_connect_menu_detach_after_deactivate (GTK_MENU (popup_menu));

	g_object_set (popup_menu,
	              "anchor-hints", (GDK_ANCHOR_FLIP_Y |
	                               GDK_ANCHOR_SLIDE |
	                               GDK_ANCHOR_RESIZE),
	              NULL);

	gtk_menu_popup_at_widget (GTK_MENU (popup_menu),
	                          button,
	                          GDK_GRAVITY_SOUTH_WEST,
	                          GDK_GRAVITY_NORTH_WEST,
	                          NULL);
}

static gint
sort_refs_array_by_path_cb (gconstpointer aa,
			    gconstpointer bb)
{
	GtkTreeRowReference *ref1 = *((GtkTreeRowReference **) aa);
	GtkTreeRowReference *ref2 = *((GtkTreeRowReference **) bb);
	GtkTreePath *path1, *path2;
	gint res = 0;

	path1 = gtk_tree_row_reference_get_path (ref1);
	path2 = gtk_tree_row_reference_get_path (ref2);

	if (path1 && path2)
		res = gtk_tree_path_compare (path1, path2);

	g_clear_pointer (&path1, gtk_tree_path_free);
	g_clear_pointer (&path2, gtk_tree_path_free);

	return res;
}

static GPtrArray * /* GtkTreeRowReference *; (transfer container) */
e_ui_customize_dialog_dup_layout_selected_dragables (EUICustomizeDialog *self)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model = NULL;
	GtkTreePath *path;
	GList *selected_paths, *link;
	GPtrArray *drag_refs = NULL;
	GtkTreePath *parent = NULL;
	gboolean parent_set = FALSE;

	selection = gtk_tree_view_get_selection (self->layout_tree_view);
	selected_paths = gtk_tree_selection_get_selected_rows (selection, &model);

	/* filter out those which cannot be dragged */
	for (link = selected_paths; link; link = g_list_next (link)) {
		GtkTreeIter iter;
		gboolean can_drag = FALSE;

		path = link->data;

		if (gtk_tree_model_get_iter (model, &iter, path))
			gtk_tree_model_get (model, &iter, COL_LAYOUT_CAN_DRAG_BOOL, &can_drag, -1);

		/* can drag only rows with the same parent */
		if (can_drag) {
			GtkTreePath *copy = gtk_tree_path_copy (path);

			if (!gtk_tree_path_up (copy))
				g_clear_pointer (&copy, gtk_tree_path_free);

			if (parent_set) {
				can_drag = (!parent && !copy) ||
					(parent && copy && (
					 (gtk_tree_path_get_depth (parent) == 0 &&
					 gtk_tree_path_get_depth (copy) == 0) || gtk_tree_path_compare (parent, copy) == 0));
			} else {
				g_clear_pointer (&parent, gtk_tree_path_free);
				parent_set = TRUE;
				parent = g_steal_pointer (&copy);
			}

			g_clear_pointer (&copy, gtk_tree_path_free);
		}

		if (can_drag) {
			if (!drag_refs)
				drag_refs = g_ptr_array_new_with_free_func ((GDestroyNotify) gtk_tree_row_reference_free);
			g_ptr_array_add (drag_refs, gtk_tree_row_reference_new (model, path));
		}
	}

	g_list_free_full (selected_paths, (GDestroyNotify) gtk_tree_path_free);

	if (drag_refs)
		g_ptr_array_sort (drag_refs, sort_refs_array_by_path_cb);

	g_clear_pointer (&parent, gtk_tree_path_free);

	return drag_refs;
}

static void
iter_from_ref (GtkTreeRowReference *ref,
	       GtkTreeIter *out_iter)
{
	GtkTreePath *tmp_path = gtk_tree_row_reference_get_path (ref);

	g_warn_if_fail (tmp_path != NULL);
	g_warn_if_fail (gtk_tree_model_get_iter (gtk_tree_row_reference_get_model (ref), out_iter, tmp_path));

	gtk_tree_path_free (tmp_path);
}

static void
e_ui_customize_dialog_layout_remove_clicked_cb (GtkButton *button,
						gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	GtkTreeStore *tree_store;
	GtkTreeModel *model;
	GPtrArray *refs;
	guint ii;
	gboolean changed = FALSE;

	refs = e_ui_customize_dialog_dup_layout_selected_dragables (self);

	if (!refs)
		return;

	model = gtk_tree_view_get_model (self->layout_tree_view);
	tree_store = GTK_TREE_STORE (model);

	for (ii = 0; ii < refs->len; ii++) {
		GtkTreeRowReference *ref = g_ptr_array_index (refs, refs->len - ii - 1);
		GtkTreePath *path;
		GtkTreeIter iter;

		path = gtk_tree_row_reference_get_path (ref);
		if (path && gtk_tree_model_get_iter (model, &iter, path)) {
			gtk_tree_store_remove (tree_store, &iter);
			changed = TRUE;
		}

		g_clear_pointer (&path, gtk_tree_path_free);
	}

	g_ptr_array_unref (refs);

	if (changed)
		customize_layout_changed (self);
}

static void
e_ui_customize_dialog_move_layout_selection (EUICustomizeDialog *self,
					     GtkScrollType where)
{
	GtkTreeModel *model;
	GtkTreeStore *tree_store;
	GPtrArray *refs; /* GtkTreeRowReference */
	guint ii;

	refs = e_ui_customize_dialog_dup_layout_selected_dragables (self);
	if (!refs)
		return;

	model = gtk_tree_view_get_model (self->layout_tree_view);
	tree_store = GTK_TREE_STORE (model);

	for (ii = 0; ii < refs->len; ii++) {
		GtkTreeRowReference *ref;
		GtkTreeIter iter, position;

		switch (where) {
		case GTK_SCROLL_PAGE_UP:
			ref = g_ptr_array_index (refs, refs->len - ii - 1);
			iter_from_ref (ref, &iter);
			gtk_tree_store_move_after (tree_store, &iter, NULL);
			break;
		case GTK_SCROLL_STEP_UP:
			ref = g_ptr_array_index (refs, ii);
			iter_from_ref (ref, &iter);

			position = iter;
			if (gtk_tree_model_iter_previous (model, &position))
				gtk_tree_store_move_before (tree_store, &iter, &position);
			break;
		case GTK_SCROLL_STEP_DOWN:
			ref = g_ptr_array_index (refs, refs->len - ii - 1);
			iter_from_ref (ref, &iter);

			position = iter;
			if (gtk_tree_model_iter_next (model, &position))
				gtk_tree_store_move_after (tree_store, &iter, &position);
			break;
		case GTK_SCROLL_PAGE_DOWN:
			ref = g_ptr_array_index (refs, ii);
			iter_from_ref (ref, &iter);
			gtk_tree_store_move_before (tree_store, &iter, NULL);
			break;
		default:
			g_warn_if_reached ();
			break;
		}
	}

	g_clear_pointer (&refs, g_ptr_array_unref);

	customize_layout_changed (self);
	customize_layout_update_buttons (self);
}

static void
e_ui_customize_dialog_layout_top_clicked_cb (GtkButton *button,
					     gpointer user_data)
{
	EUICustomizeDialog *self = user_data;

	e_ui_customize_dialog_move_layout_selection (self, GTK_SCROLL_PAGE_UP);
}

static void
e_ui_customize_dialog_layout_up_clicked_cb (GtkButton *button,
					    gpointer user_data)
{
	EUICustomizeDialog *self = user_data;

	e_ui_customize_dialog_move_layout_selection (self, GTK_SCROLL_STEP_UP);
}

static void
e_ui_customize_dialog_layout_down_clicked_cb (GtkButton *button,
					      gpointer user_data)
{
	EUICustomizeDialog *self = user_data;

	e_ui_customize_dialog_move_layout_selection (self, GTK_SCROLL_STEP_DOWN);
}

static void
e_ui_customize_dialog_layout_bottom_clicked_cb (GtkButton *button,
						gpointer user_data)
{
	EUICustomizeDialog *self = user_data;

	e_ui_customize_dialog_move_layout_selection (self, GTK_SCROLL_PAGE_DOWN);
}

static void
e_ui_customize_dialog_layout_default_clicked_cb (GtkButton *button,
						 gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	EUICustomizer *customizer = NULL;
	EUIElement *root;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *id = NULL;

	if (!gtk_combo_box_get_active_iter (self->element_combo, &iter))
		return;

	model = gtk_combo_box_get_model (self->element_combo);
	gtk_tree_model_get (model, &iter,
		COL_COMBO_ID_STR, &id,
		COL_COMBO_CUSTOMIZER_OBJ, &customizer,
		-1);

	root = e_ui_parser_get_root (e_ui_customizer_get_parser (customizer));
	if (root)
		e_ui_element_remove_child_by_id (root, id);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		COL_COMBO_DEFAULT_BOOL, TRUE,
		COL_COMBO_CHANGED_BOOL, TRUE,
		-1);

	g_clear_object (&customizer);
	g_free (id);

	g_signal_emit_by_name (G_OBJECT (self->element_combo), "changed", 0, NULL);
}

static void
e_ui_customize_dialog_layout_tree_view_selection_changed_cb (GtkTreeSelection *selection,
							     gpointer user_data)
{
	EUICustomizeDialog *self = user_data;

	customize_layout_update_buttons (self);
}

static gboolean
paths_under_the_same_parent (GtkTreePath *in_path1,
			     GtkTreePath *in_path2)
{
	GtkTreePath *path1, *path2;
	gboolean have_parent1, have_parent2;
	gboolean res;

	if (!in_path1 || !in_path2)
		return FALSE;

	path1 = gtk_tree_path_copy (in_path1);
	path2 = gtk_tree_path_copy (in_path2);

	have_parent1 = gtk_tree_path_up (path1);
	have_parent2 = gtk_tree_path_up (path2);

	res = !have_parent1 && !have_parent2;

	if (!res && have_parent1 && have_parent2) {
		gint depth1, depth2;

		depth1 = gtk_tree_path_get_depth (path1);
		depth2 = gtk_tree_path_get_depth (path2);

		res = depth1 == 0 && depth2 == 0;

		if (!res && depth1 > 0 && depth2 > 0)
			res = gtk_tree_path_compare (path1, path2) == 0;
	}

	gtk_tree_path_free (path1);
	gtk_tree_path_free (path2);

	return res;
}

static gboolean
customize_layout_move_iter_to (GtkTreeView *tree_view,
			       GtkTreeStore *tree_store,
			       GtkTreeIter *src_iter,
			       GtkTreeIter *des_iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL (tree_store);
	GtkTreeIter src_child;
	GtkTreePath *path;
	GtkTreeRowReference *ref;
	EUIElement *elem = NULL;
	gchar *label = NULL;
	gboolean can_drag = FALSE;
	gboolean moved_to_next;
	gint weight = WEIGHT_NORMAL;

	gtk_tree_model_get (model, src_iter,
		COL_LAYOUT_ELEM_OBJ, &elem,
		COL_LAYOUT_LABEL_STR, &label,
		COL_LAYOUT_CAN_DRAG_BOOL, &can_drag,
		COL_LAYOUT_WEIGHT_INT, &weight,
		-1);

	gtk_tree_store_set (tree_store, des_iter,
		COL_LAYOUT_ELEM_OBJ, elem,
		COL_LAYOUT_LABEL_STR, label,
		COL_LAYOUT_CAN_DRAG_BOOL, can_drag,
		COL_LAYOUT_WEIGHT_INT, weight,
		-1);

	e_ui_element_free (elem);
	g_free (label);

	if (gtk_tree_model_iter_children (model, &src_child, src_iter)) {
		GtkTreeIter des_child;

		do {
			gtk_tree_store_append (tree_store, &des_child, des_iter);
		} while (customize_layout_move_iter_to (tree_view, tree_store, &src_child, &des_child));
	}

	path = gtk_tree_model_get_path (model, des_iter);
	ref = gtk_tree_row_reference_new (model, path);
	gtk_tree_path_free (path);

	moved_to_next = gtk_tree_store_remove (tree_store, src_iter);

	if (gtk_tree_row_reference_valid (ref)) {
		path = gtk_tree_row_reference_get_path (ref);
		gtk_tree_view_expand_to_path (tree_view, path);
		gtk_tree_path_free (path);
	}

	gtk_tree_row_reference_free (ref);

	return moved_to_next;
}

static void
customize_layout_tree_finish_drag (EUICustomizeDialog *self,
				   GtkTreePath *drop_path,
				   GdkAtom drag_target)
{
	if (drop_path && drag_target == drag_target_layout_atom && self->layout_drag_refs) {
		GtkTreeRowReference *ref;
		GtkTreePath *drag_path;
		gboolean different_paths;

		ref = g_ptr_array_index (self->layout_drag_refs, 0);
		drag_path = gtk_tree_row_reference_get_path (ref);

		different_paths = drag_path && gtk_tree_path_compare (drag_path, drop_path) != 0;

		/* The paths are not the same or the order was already changed, but they have the same parent */
		if (drag_path && different_paths) {
			GtkTreeModel *model;
			GtkTreeStore *tree_store;
			GtkTreeRowReference *target_ref;
			GtkTreeIter iter, des_iter, drop_iter = { 0, };
			GtkTreeIter *drop_parent = NULL, drop_parent_local;
			gboolean move_before;
			gboolean same_parent;
			gboolean drop_append = FALSE;
			guint ii;

			model = gtk_tree_view_get_model (self->layout_tree_view);
			tree_store = GTK_TREE_STORE (model);

			/* if dragged from the bottom to the top, then add the dragged before the drop position; likewise,
			   if dragged from the top to the bottom, then add the dragged after the drop position */

			move_before = gtk_tree_path_compare (drop_path, drag_path) < 0;
			same_parent = paths_under_the_same_parent (drag_path, drop_path);
			target_ref = gtk_tree_row_reference_new (model, drop_path);

			if (!same_parent) {
				GtkTreeIter tmp_iter;

				iter_from_ref (target_ref, &tmp_iter);

				drop_iter = tmp_iter;

				if (gtk_tree_model_iter_has_child (model, &tmp_iter)) {
					drop_append = TRUE;
					drop_parent_local = tmp_iter;
					drop_parent = &drop_parent_local;
				} else {
					gboolean can_drag = FALSE;

					gtk_tree_model_get (model, &tmp_iter, COL_LAYOUT_CAN_DRAG_BOOL, &can_drag, -1);

					if (can_drag) {
						if (gtk_tree_model_iter_parent (model, &drop_parent_local, &tmp_iter))
							drop_parent = &drop_parent_local;
					} else {
						drop_append = TRUE;
						drop_parent_local = tmp_iter;
						drop_parent = &drop_parent_local;
					}
				}
			}

			for (ii = 0; ii < self->layout_drag_refs->len; ii++) {
				GtkTreeRowReference *drag_ref = g_ptr_array_index (self->layout_drag_refs, ii);

				iter_from_ref (drag_ref, &iter);

				if (same_parent) {
					iter_from_ref (target_ref, &des_iter);

					if (move_before)
						gtk_tree_store_move_before (tree_store, &iter, &des_iter);
					else
						gtk_tree_store_move_after (tree_store, &iter, &des_iter);
				} else if (drop_append) {
					gtk_tree_store_append (tree_store, &des_iter, drop_parent);
					customize_layout_move_iter_to (self->layout_tree_view, tree_store, &iter, &des_iter);
				} else if (move_before) {
					gtk_tree_store_insert_before (tree_store, &des_iter, drop_parent, &drop_iter);
					customize_layout_move_iter_to (self->layout_tree_view, tree_store, &iter, &des_iter);
				} else {
					gtk_tree_store_insert_after (tree_store, &des_iter, drop_parent, &drop_iter);
					customize_layout_move_iter_to (self->layout_tree_view, tree_store, &iter, &des_iter);
					drop_iter = des_iter;
				}
			}

			gtk_tree_row_reference_free (target_ref);

			customize_layout_changed (self);
		}

		gtk_tree_path_free (drag_path);
	}

	if (self->autoscroll_id) {
		g_source_remove (self->autoscroll_id);
		self->autoscroll_id = 0;
	}

	g_clear_pointer (&self->layout_drag_refs, g_ptr_array_unref);
}

#define SCROLL_EDGE_SIZE 15

static gboolean
customize_layout_tree_autoscroll (gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	GtkAdjustment *adjustment;
	GtkTreeView *tree_view;
	GtkScrollable *scrollable;
	GdkRectangle rect;
	GdkWindow *window;
	GdkDisplay *display;
	GdkDeviceManager *device_manager;
	GdkDevice *device;
	gdouble value;
	gint offset, y;

	/* Get the y pointer position relative to the treeview. */
	tree_view = self->layout_tree_view;
	window = gtk_tree_view_get_bin_window (tree_view);
	display = gdk_window_get_display (window);
	device_manager = gdk_display_get_device_manager (display);
	device = gdk_device_manager_get_client_pointer (device_manager);
	gdk_window_get_device_position (window, device, NULL, &y, NULL);

	/* Rect is in coordinates relative to the scrolled window,
	 * relative to the treeview. */
	gtk_tree_view_get_visible_rect (tree_view, &rect);

	/* Move y into the same coordinate system as rect. */
	y += rect.y;

	/* See if we are near the top edge. */
	offset = y - (rect.y + 2 * SCROLL_EDGE_SIZE);
	if (offset > 0) {
		/* See if we are near the bottom edge. */
		offset = y - (rect.y + rect.height - 2 * SCROLL_EDGE_SIZE);
		if (offset < 0)
			return TRUE;
	}

	scrollable = GTK_SCROLLABLE (tree_view);
	adjustment = gtk_scrollable_get_vadjustment (scrollable);
	value = gtk_adjustment_get_value (adjustment);
	gtk_adjustment_set_value (adjustment, MAX (value + offset, 0.0));

	return TRUE;
}

static void
customize_actions_tree_drag_begin_cb (GtkWidget *widget,
				      GdkDragContext *context,
				      gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	GtkTreeSelection *selection;
	GList *selected_paths;

	g_return_if_fail (self != NULL);

	selection = gtk_tree_view_get_selection (self->actions_tree_view);
	selected_paths = gtk_tree_selection_get_selected_rows (selection, NULL);

	if (selected_paths) {
		cairo_surface_t *surface;

		surface = gtk_tree_view_create_row_drag_icon (self->actions_tree_view, selected_paths->data);
		gtk_drag_set_icon_surface (context, surface);
		cairo_surface_destroy (surface);
	}

	g_list_free_full (selected_paths, (GDestroyNotify) gtk_tree_path_free);
}

static void
customize_actions_tree_drag_data_get_cb (GtkWidget *widget,
					 GdkDragContext *context,
					 GtkSelectionData *data,
					 guint info,
					 guint time,
					 gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	GtkTreeSelection *selection;

	g_return_if_fail (self != NULL);

	selection = gtk_tree_view_get_selection (self->actions_tree_view);
	if (gtk_tree_selection_count_selected_rows (selection) > 0) {
		/* the dragged data is not important, the selection is read inside the drag-data-received */
		gtk_selection_data_set (data, drag_target_action_atom, 8, (const guchar *) "-", 1);
	}
}

static GdkAtom
customize_layout_get_drag_target (GdkDragContext *context)
{
	GList *targets;
	GdkAtom drag_target = GDK_NONE;

	targets = gdk_drag_context_list_targets (context);
	/* expects only one target, from the actions or the layout tree view */
	if (targets && !targets->next)
		drag_target = targets->data;

	return drag_target;
}

static void
customize_layout_tree_drag_begin_cb (GtkWidget *widget,
				     GdkDragContext *context,
				     gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	GPtrArray *drag_refs = NULL;

	g_return_if_fail (self != NULL);

	customize_layout_tree_finish_drag (self, NULL, GDK_NONE);

	drag_refs = e_ui_customize_dialog_dup_layout_selected_dragables (self);

	if (drag_refs) {
		GtkTreeRowReference *ref;
		GtkTreePath *path;
		cairo_surface_t *surface;

		self->layout_drag_refs = drag_refs;

		ref = g_ptr_array_index (self->layout_drag_refs, 0);
		path = gtk_tree_row_reference_get_path (ref);

		if (path) {
			surface = gtk_tree_view_create_row_drag_icon (self->layout_tree_view, path);
			gtk_drag_set_icon_surface (context, surface);
			cairo_surface_destroy (surface);

			gtk_tree_path_free (path);
		}
	}
}

static gboolean
customize_layout_tree_drag_motion_cb (GtkWidget *widget,
				      GdkDragContext *context,
				      gint xx,
				      gint yy,
				      guint time,
				      gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path = NULL;
	GdkAtom drag_target = GDK_NONE;
	gboolean can = FALSE;

	g_return_val_if_fail (self != NULL, FALSE);

	tree_view = self->layout_tree_view;

	if (!gtk_tree_view_get_dest_row_at_pos (tree_view, xx, yy, &path, NULL)) {
		gdk_drag_status (context, 0, time);
		return FALSE;
	}

	drag_target = customize_layout_get_drag_target (context);

	if (drag_target == drag_target_layout_atom && !self->layout_drag_refs) {
		gdk_drag_status (context, 0, time);
		return FALSE;
	}

	if (!self->autoscroll_id) {
		self->autoscroll_id = e_named_timeout_add (
			150, customize_layout_tree_autoscroll, self);
	}

	model = gtk_tree_view_get_model (tree_view);

	g_warn_if_fail (gtk_tree_model_get_iter (model, &iter, path));

	if (drag_target == drag_target_action_atom) {
		can = TRUE;
	} else if (drag_target == drag_target_layout_atom) {
		GtkTreeRowReference *ref;
		GtkTreePath *drag_path;
		gboolean different_paths;

		ref = g_ptr_array_index (self->layout_drag_refs, 0);
		drag_path = gtk_tree_row_reference_get_path (ref);
		different_paths = drag_path && gtk_tree_path_compare (drag_path, path) != 0;

		/* The paths are not the same or the order was already changed, but they have the same parent */
		can = drag_path && different_paths;

		if (can && paths_under_the_same_parent (drag_path, path)) {
			guint ii;

			for (ii = 0; can && ii < self->layout_drag_refs->len; ii++) {
				g_clear_pointer (&drag_path, gtk_tree_path_free);

				ref = g_ptr_array_index (self->layout_drag_refs, ii);
				drag_path = gtk_tree_row_reference_get_path (ref);

				/* do not allow drop on the selected row */
				can = gtk_tree_path_compare (path, drag_path) != 0;
			}
		}

		g_clear_pointer (&drag_path, gtk_tree_path_free);
	}

	gtk_tree_path_free (path);

	gdk_drag_status (context, can ? GDK_ACTION_MOVE : 0, time);

	return TRUE;
}

static void
customize_layout_tree_drag_leave_cb (GtkWidget *widget,
				     GdkDragContext *context,
				     guint time,
				     gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	GtkTreeView *tree_view = self->layout_tree_view;

	if (self->autoscroll_id) {
		g_source_remove (self->autoscroll_id);
		self->autoscroll_id = 0;
	}

	gtk_tree_view_set_drag_dest_row (tree_view, NULL, GTK_TREE_VIEW_DROP_BEFORE);
}

static gboolean
customize_layout_tree_drag_drop_cb (GtkWidget *widget,
				    GdkDragContext *context,
				    gint xx,
				    gint yy,
				    guint time,
				    gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	GdkAtom drag_target;

	g_return_val_if_fail (self != NULL, FALSE);

	drag_target = customize_layout_get_drag_target (context);

	if (drag_target == drag_target_action_atom) {
		gtk_drag_get_data (widget, context, drag_target, time);
		return TRUE;
	} else {
		GtkTreePath *path = NULL;

		if (!self->layout_drag_refs ||
		    !gtk_tree_view_get_dest_row_at_pos (self->layout_tree_view, xx, yy, &path, NULL))
			path = NULL;

		customize_layout_tree_finish_drag (self, path, drag_target);

		gtk_tree_path_free (path);
	}

	return FALSE;
}

static void
customize_layout_tree_drag_end_cb (GtkWidget *widget,
				   GdkDragContext *context,
				   gpointer user_data)
{
	EUICustomizeDialog *self = user_data;

	g_return_if_fail (self != NULL);

	customize_layout_tree_finish_drag (self, NULL, GDK_NONE);
}

static void
customize_layout_tree_drag_data_received_cb (GtkWidget *widget,
					     GdkDragContext *context,
					     gint xx,
					     gint yy,
					     GtkSelectionData *data,
					     guint info,
					     guint time,
					     gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	GtkTreeModel *layout_model;
	GtkTreePath *dest_path = NULL;
	GtkTreeIter dest_iter;

	g_return_if_fail (self != NULL);

	layout_model = gtk_tree_view_get_model (self->layout_tree_view);

	if (gtk_selection_data_get_data_type (data) == drag_target_action_atom &&
	    gtk_tree_view_get_dest_row_at_pos (self->layout_tree_view, xx, yy, &dest_path, NULL) &&
	    gtk_tree_model_get_iter (layout_model, &dest_iter, dest_path)) {
		GtkTreeIter *parent = NULL, parent_local, iter;
		EUIElementKind for_kind = E_UI_ELEMENT_KIND_UNKNOWN;
		gint position = -1;

		if (gtk_combo_box_get_active_iter (self->element_combo, &iter)) {
			gtk_tree_model_get (gtk_combo_box_get_model (self->element_combo), &iter,
				COL_COMBO_ELEM_KIND_UINT, &for_kind,
				-1);

			/* can add only under the start/end elements */
			if (for_kind == E_UI_ELEMENT_KIND_HEADERBAR &&
			    gtk_tree_path_get_depth (dest_path) > 1 &&
			    gtk_tree_path_get_indices (dest_path)) {
				gint indice = gtk_tree_path_get_indices (dest_path)[0];
				if (gtk_tree_model_get_iter_first (layout_model, &iter)) {
					position = -1;
					do {
						parent_local = iter;
						parent = &parent_local;
						indice--;
					} while (indice >= 0 && gtk_tree_model_iter_next (layout_model, &iter));

					if (gtk_tree_path_get_depth (dest_path) >= 2) {
						position = gtk_tree_path_get_indices (dest_path)[1];
					}
				}
			}
		}

		if (!parent) {
			if (gtk_tree_model_iter_has_child (layout_model, &dest_iter)) {
				parent = &dest_iter;
				position = -1;
			} else if (gtk_tree_path_get_depth (dest_path) > 0 &&
				   gtk_tree_path_get_indices (dest_path)) {
				position = gtk_tree_path_get_indices (dest_path)[gtk_tree_path_get_depth (dest_path) - 1] + 1;
				if (gtk_tree_model_iter_parent (layout_model, &parent_local, &dest_iter))
					parent = &parent_local;
			}
		}

		if (!parent && for_kind == E_UI_ELEMENT_KIND_HEADERBAR) {
			/* header bar has the start/end toplevels, it cannot have
			   anything out of these, thus put below the end */
			if (gtk_tree_model_get_iter_first (layout_model, &iter) &&
			    gtk_tree_model_iter_next (layout_model, &iter)) {
				parent_local = iter;
				parent = &parent_local;
				position = -1;
			}
		}

		customize_layout_add_actions (self, parent, position);
	}

	customize_layout_tree_finish_drag (self, NULL, GDK_NONE);

	gtk_tree_path_free (dest_path);

	gtk_drag_finish (context, TRUE, FALSE, time);
}

static GtkWidget *
customize_dialog_new_layout_add_button (EUICustomizeDialog *self)
{
	GtkWidget *box, *button, *arrow, *label;

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	button = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (button), box);

	label = gtk_label_new_with_mnemonic (_("_Add"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), button);
	g_object_set (G_OBJECT (label),
		"halign", GTK_ALIGN_FILL,
		"hexpand", FALSE,
		"xalign", 0.5,
		NULL);

	gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 2);

	arrow = gtk_image_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start (GTK_BOX (box), arrow, FALSE, FALSE, 2);

	gtk_widget_show_all (button);

	g_signal_connect_object (button, "clicked",
		G_CALLBACK (e_ui_customize_dialog_layout_add_clicked_cb), self, 0);

	return button;
}

static gboolean
customize_dialog_search_equal_cb (GtkTreeModel *model,
				  gint column,
				  const gchar *key,
				  GtkTreeIter *iter,
				  gpointer search_data)
{
	gboolean matches = FALSE;

	if (key && *key) {
		gchar *str = NULL;

		gtk_tree_model_get (model, iter, column, &str, -1);

		matches = str && e_util_utf8_strstrcase (str, key) != NULL;

		g_free (str);
	}

	return !matches;
}

static void
e_ui_customize_dialog_constructed (GObject *object)
{
	const GtkTargetEntry action_targets[] = {
		{ (gchar *) DRAG_TARGET_ACTION, GTK_TARGET_SAME_APP, 0 },
	};
	const GtkTargetEntry layout_source_targets[] = {
		{ (gchar *) DRAG_TARGET_LAYOUT, GTK_TARGET_SAME_WIDGET, 1 }
	};
	const GtkTargetEntry layout_dest_targets[] = {
		{ (gchar *) DRAG_TARGET_ACTION, GTK_TARGET_SAME_APP, 0 },
		{ (gchar *) DRAG_TARGET_LAYOUT, GTK_TARGET_SAME_WIDGET, 1 }
	};
	EUICustomizeDialog *self = E_UI_CUSTOMIZE_DIALOG (object);
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *widget, *content_area, *scrolled_window;
	GtkLabel *label;
	GtkListStore *list_store;
	GtkTreeStore *tree_store;
	GtkTreeSelection *selection;
	GtkBox *box1, *box2;
	GtkPaned *paned;
	PangoAttrList *attrs;

	G_OBJECT_CLASS (e_ui_customize_dialog_parent_class)->constructed (object);

	gtk_window_set_default_size (GTK_WINDOW (self), 640, 480);

	if (!e_util_get_use_header_bar ())
		gtk_dialog_add_button (GTK_DIALOG (self), _("_Close"), GTK_RESPONSE_CLOSE);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (self));

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_add (GTK_CONTAINER (content_area), widget);

	box1 = GTK_BOX (widget);
	g_object_set (box1,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		NULL);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start (box1, widget, FALSE, FALSE, 0);
	box2 = GTK_BOX (widget);

	/* Translators: this is like "Part of the user interface" */
	widget = gtk_label_new_with_mnemonic (_("_Part:"));
	gtk_box_pack_start (box2, widget, FALSE, FALSE, 0);
	label = GTK_LABEL (widget);

	list_store = gtk_list_store_new (N_COL_COMBO,
		G_TYPE_STRING,		/* COL_COMBO_ID_STR */
		G_TYPE_STRING,		/* COL_COMBO_DISPLAY_NAME_STR */
		E_TYPE_UI_CUSTOMIZER,	/* COL_COMBO_CUSTOMIZER_OBJ */
		G_TYPE_BOOLEAN,		/* COL_COMBO_CHANGED_BOOL */
		G_TYPE_UINT,		/* COL_COMBO_ELEM_KIND_UINT */
		G_TYPE_BOOLEAN);	/* COL_COMBO_DEFAULT_BOOL */

	widget = gtk_combo_box_new_with_model (GTK_TREE_MODEL (list_store));

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), renderer,
		"text", COL_COMBO_DISPLAY_NAME_STR,
		NULL);

	g_object_unref (list_store);

	gtk_box_pack_start (box2, widget, TRUE, TRUE, 0);
	gtk_label_set_mnemonic_widget (label, widget);

	self->element_combo = GTK_COMBO_BOX (widget);

	widget = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (box1, widget, TRUE, TRUE, 0);

	paned = GTK_PANED (widget);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
	gtk_paned_pack1 (paned, widget, TRUE, FALSE);

	box1 = GTK_BOX (widget);

	widget = gtk_label_new_with_mnemonic (_("Available _Actions"));
	gtk_box_pack_start (box1, widget, FALSE, FALSE, 0);
	label = GTK_LABEL (widget);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (scrolled_window,
		"hexpand", TRUE,
		"vexpand", TRUE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);

	gtk_box_pack_start (box1, scrolled_window, TRUE, TRUE, 0);

	list_store = gtk_list_store_new (N_COL_ACTIONS,
		E_TYPE_UI_ELEMENT,	/* COL_ACTIONS_ELEM_OBJ */
		G_TYPE_STRING,		/* COL_ACTIONS_NAME_STR */
		G_TYPE_STRING,		/* COL_ACTIONS_LABEL_STR */
		G_TYPE_STRING,		/* COL_ACTIONS_TOOLTIP_STR */
		G_TYPE_STRING);		/* COL_ACTIONS_MARKUP_STR */

	widget = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store));
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"enable-search", TRUE,
		"fixed-height-mode", FALSE,
		"headers-visible", FALSE,
		"search-column", COL_ACTIONS_LABEL_STR,
		NULL);

	g_object_unref (list_store);

	gtk_container_add (GTK_CONTAINER (scrolled_window), widget);

	self->actions_tree_view = GTK_TREE_VIEW (widget);

	gtk_tree_view_set_search_equal_func (self->actions_tree_view, customize_dialog_search_equal_cb, NULL, NULL);
	gtk_label_set_mnemonic_widget (label, widget);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_title (column, "Action"); /* do not localize, it's not visible in the UI */

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer, "markup", COL_ACTIONS_MARKUP_STR);

	gtk_tree_view_append_column (self->actions_tree_view, column);

	gtk_drag_source_set (GTK_WIDGET (self->actions_tree_view), GDK_BUTTON1_MASK, action_targets, G_N_ELEMENTS (action_targets), GDK_ACTION_MOVE);

	g_signal_connect (self->actions_tree_view, "drag-begin",
		G_CALLBACK (customize_actions_tree_drag_begin_cb), self);
	g_signal_connect (self->actions_tree_view, "drag-data-get",
		G_CALLBACK (customize_actions_tree_drag_data_get_cb), self);

	widget = gtk_notebook_new ();
	gtk_paned_pack2 (paned, widget, TRUE, FALSE);

	self->notebook = GTK_NOTEBOOK (widget);

	g_signal_connect (self->notebook, "switch-page",
		G_CALLBACK (e_ui_customize_dialog_notebook_switch_page_cb), self);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_notebook_append_page (self->notebook, widget, gtk_label_new_with_mnemonic (_("_Layout")));

	box1 = GTK_BOX (widget);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (scrolled_window,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);

	gtk_box_pack_start (box1, scrolled_window, TRUE, TRUE, 0);

	tree_store = gtk_tree_store_new (N_COL_LAYOUT,
		E_TYPE_UI_ELEMENT,	/* COL_LAYOUT_ELEM_OBJ */
		G_TYPE_STRING,		/* COL_LAYOUT_LABEL_STR */
		G_TYPE_BOOLEAN,		/* COL_LAYOUT_CAN_DRAG_BOOL */
		G_TYPE_INT);		/* COL_LAYOUT_WEIGHT_INT */

	widget = gtk_tree_view_new_with_model (GTK_TREE_MODEL (tree_store));
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"enable-search", TRUE,
		"fixed-height-mode", FALSE,
		"headers-visible", FALSE,
		"search-column", COL_LAYOUT_LABEL_STR,
		"show-expanders", TRUE,
		NULL);

	g_object_unref (tree_store);

	gtk_container_add (GTK_CONTAINER (scrolled_window), widget);

	self->layout_tree_view = GTK_TREE_VIEW (widget);

	gtk_tree_view_set_search_equal_func (self->layout_tree_view, customize_dialog_search_equal_cb, NULL, NULL);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_title (column, "Element"); /* do not localize, it's not visible in the UI */

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer, "text", COL_LAYOUT_LABEL_STR);
	gtk_tree_view_column_add_attribute (column, renderer, "weight", COL_LAYOUT_WEIGHT_INT);

	gtk_tree_view_append_column (self->layout_tree_view, column);

	gtk_drag_source_set (GTK_WIDGET (self->layout_tree_view), GDK_BUTTON1_MASK, layout_source_targets, G_N_ELEMENTS (layout_source_targets), GDK_ACTION_MOVE);
	gtk_drag_dest_set (GTK_WIDGET (self->layout_tree_view), GTK_DEST_DEFAULT_MOTION, layout_dest_targets, G_N_ELEMENTS (layout_dest_targets), GDK_ACTION_MOVE);

	g_signal_connect (self->layout_tree_view, "drag-begin",
		G_CALLBACK (customize_layout_tree_drag_begin_cb), self);
	g_signal_connect (self->layout_tree_view, "drag-motion",
		G_CALLBACK (customize_layout_tree_drag_motion_cb), self);
	g_signal_connect (self->layout_tree_view, "drag-leave",
		G_CALLBACK (customize_layout_tree_drag_leave_cb), self);
	g_signal_connect (self->layout_tree_view, "drag-drop",
		G_CALLBACK (customize_layout_tree_drag_drop_cb), self);
	g_signal_connect (self->layout_tree_view, "drag-end",
		G_CALLBACK (customize_layout_tree_drag_end_cb), self);
	g_signal_connect (self->layout_tree_view, "drag-data-received",
		G_CALLBACK (customize_layout_tree_drag_data_received_cb), self);

	widget = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		"layout-style", GTK_BUTTONBOX_START,
		"homogeneous", TRUE,
		NULL);
	gtk_box_pack_start (box1, widget, FALSE, FALSE, 0);

	box2 = GTK_BOX (widget);

	widget = customize_dialog_new_layout_add_button (self);
	gtk_box_pack_start (box2, widget, FALSE, FALSE, 0);

	selection = gtk_tree_view_get_selection (self->actions_tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	widget = gtk_button_new_with_mnemonic (_("_Remove"));
	gtk_box_pack_start (box2, widget, FALSE, FALSE, 0);
	g_signal_connect_object (widget, "clicked",
		G_CALLBACK (e_ui_customize_dialog_layout_remove_clicked_cb), self, 0);
	self->remove_button = widget;

	widget = gtk_button_new_with_mnemonic (_("_Top"));
	gtk_box_pack_start (box2, widget, FALSE, FALSE, 0);
	g_signal_connect_object (widget, "clicked",
		G_CALLBACK (e_ui_customize_dialog_layout_top_clicked_cb), self, 0);
	self->top_button = widget;

	widget = gtk_button_new_with_mnemonic (_("_Up"));
	gtk_box_pack_start (box2, widget, FALSE, FALSE, 0);
	g_signal_connect_object (widget, "clicked",
		G_CALLBACK (e_ui_customize_dialog_layout_up_clicked_cb), self, 0);
	self->up_button = widget;

	widget = gtk_button_new_with_mnemonic (_("_Down"));
	gtk_box_pack_start (box2, widget, FALSE, FALSE, 0);
	g_signal_connect_object (widget, "clicked",
		G_CALLBACK (e_ui_customize_dialog_layout_down_clicked_cb), self, 0);
	self->down_button = widget;

	widget = gtk_button_new_with_mnemonic (_("_Bottom"));
	gtk_box_pack_start (box2, widget, FALSE, FALSE, 0);
	g_signal_connect_object (widget, "clicked",
		G_CALLBACK (e_ui_customize_dialog_layout_bottom_clicked_cb), self, 0);
	self->bottom_button = widget;

	widget = gtk_button_new_with_mnemonic (_("_Default"));
	gtk_box_pack_start (box2, widget, FALSE, FALSE, 0);
	g_signal_connect_object (widget, "clicked",
		G_CALLBACK (e_ui_customize_dialog_layout_default_clicked_cb), self, 0);
	self->layout_default_button = widget;

	selection = gtk_tree_view_get_selection (self->layout_tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect_object (selection, "changed",
		G_CALLBACK (e_ui_customize_dialog_layout_tree_view_selection_changed_cb), self, 0);
	e_ui_customize_dialog_layout_tree_view_selection_changed_cb (selection, self);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 8);
	gtk_notebook_append_page (self->notebook, widget, gtk_label_new_with_mnemonic (_("_Shortcuts")));
	box1 = GTK_BOX (widget);

	widget = gtk_label_new (_("Add or remove shortcuts for the selected action."));
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_START,
		"xalign", 0.0f,
		"wrap", TRUE,
		NULL);
	gtk_box_pack_start (box1, widget, FALSE, FALSE, 0);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (box1, widget, FALSE, FALSE, 2);

	box2 = GTK_BOX (widget);

	attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
	pango_attr_list_insert (attrs, pango_attr_scale_new (1.2));

	widget = gtk_label_new ("");
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_START,
		"margin-top", 12,
		"xalign", 0.0f,
		"wrap", TRUE,
		"attributes", attrs,
		NULL);
	gtk_box_pack_start (box2, widget, FALSE, FALSE, 0);
	self->shortcuts_label = GTK_LABEL (widget);

	g_clear_pointer (&attrs, pango_attr_list_unref);

	widget = gtk_button_new_with_mnemonic (_("_Default"));
	gtk_box_pack_end (box2, widget, FALSE, FALSE, 0);
	self->shortcuts_default_button = widget;

	g_signal_connect (self->shortcuts_default_button, "clicked",
		G_CALLBACK (customize_shortcuts_default_clicked_cb), self);

	attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_scale_new (0.8));

	widget = gtk_label_new ("");
	g_object_set (widget,
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_START,
		"sensitive", FALSE,
		"xalign", 0.0f,
		"wrap", TRUE,
		"attributes", attrs,
		NULL);
	gtk_box_pack_start (box1, widget, FALSE, FALSE, 0);
	self->shortcuts_tooltip = GTK_LABEL (widget);

	g_clear_pointer (&attrs, pango_attr_list_unref);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (scrolled_window,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);
	gtk_box_pack_start (box1, scrolled_window, TRUE, TRUE, 0);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	g_object_set (scrolled_window,
		"margin-top", 8,
		NULL);
	gtk_container_add (GTK_CONTAINER (scrolled_window), widget);
	self->shortcuts_box = GTK_BOX (widget);

	gtk_notebook_set_current_page (self->notebook, 0);

	gtk_widget_show_all (content_area);
}

static void
e_ui_customize_dialog_dispose (GObject *object)
{
	EUICustomizeDialog *self = E_UI_CUSTOMIZE_DIALOG (object);

	self->element_combo = NULL;
	self->actions_tree_view = NULL;
	self->notebook = NULL;
	self->layout_tree_view = NULL;
	self->remove_button = NULL;
	self->top_button = NULL;
	self->up_button = NULL;
	self->down_button = NULL;
	self->bottom_button = NULL;
	self->layout_default_button = NULL;
	self->shortcuts_label = NULL;
	self->shortcuts_tooltip = NULL;
	self->shortcuts_default_button = NULL;
	self->shortcuts_box = NULL;

	if (self->autoscroll_id) {
		g_source_remove (self->autoscroll_id);
		self->autoscroll_id = 0;
	}
	g_clear_pointer (&self->layout_drag_refs, g_ptr_array_unref);
	g_clear_pointer (&self->shortcut_entries, g_ptr_array_unref);

	G_OBJECT_CLASS (e_ui_customize_dialog_parent_class)->dispose (object);
}

static void
e_ui_customize_dialog_finalize (GObject *object)
{
	EUICustomizeDialog *self = E_UI_CUSTOMIZE_DIALOG (object);

	g_clear_pointer (&self->customizers, g_ptr_array_unref);
	g_clear_pointer (&self->save_customizers, g_hash_table_unref);
	g_clear_pointer (&self->all_shortcuts, g_hash_table_unref);

	G_OBJECT_CLASS (e_ui_customize_dialog_parent_class)->finalize (object);
}

static void
e_ui_customize_dialog_class_init (EUICustomizeDialogClass *klass)
{
	GObjectClass *object_class;

	drag_target_action_atom = gdk_atom_intern_static_string (DRAG_TARGET_ACTION);
	drag_target_layout_atom = gdk_atom_intern_static_string (DRAG_TARGET_LAYOUT);

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_ui_customize_dialog_constructed;
	object_class->dispose = e_ui_customize_dialog_dispose;
	object_class->finalize = e_ui_customize_dialog_finalize;
}

static void
e_ui_customize_dialog_init (EUICustomizeDialog *self)
{
	self->customizers = g_ptr_array_new_with_free_func (g_object_unref);
	self->save_customizers = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);
	self->shortcut_entries = g_ptr_array_new ();
}

/**
 * e_ui_customize_dialog_new:
 * @parent: (nullable): a parent #GtkWindow, or %NULL
 *
 * Creates a new #EUICustomizeDialog. Use gtk_widget_destroy(),
 * when no longer needed.
 *
 * Returns: (transfer full): a new #EUICustomizeDialog
 *
 * Since: 3.56
 **/
EUICustomizeDialog *
e_ui_customize_dialog_new (GtkWindow *parent)
{
	if (!parent)
		g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);

	return g_object_new (E_TYPE_UI_CUSTOMIZE_DIALOG,
		"destroy-with-parent", TRUE,
		"modal", TRUE,
		"title", _("Customize User Interface"),
		"transient-for", parent,
		"use-header-bar", e_util_get_use_header_bar (),
		NULL);
}

/**
 * e_ui_customize_dialog_add_customizer:
 * @self: an #EUICustomizeDialog
 * @customizer: an #EUICustomizer
 *
 * Adds the @customizer as one source of the customizable UI elements.
 * All the registered elements in the @customizer will be offered
 * for changes in the dialog.
 *
 * Since: 3.56
 **/
void
e_ui_customize_dialog_add_customizer (EUICustomizeDialog *self,
				      EUICustomizer *customizer)
{
	g_return_if_fail (E_IS_UI_CUSTOMIZE_DIALOG (self));
	g_return_if_fail (E_IS_UI_CUSTOMIZER (customizer));
	g_return_if_fail (!g_ptr_array_find (self->customizers, customizer, NULL));

	g_ptr_array_add (self->customizers, g_object_ref (customizer));
}

/**
 * e_ui_customize_dialog_get_customizers:
 * @self: an #EUICustomizeDialog
 *
 * Returns an array of all the #EUICustomizer -s added to the @self
 * with e_ui_customize_dialog_add_customizer(). Do not modify the array,
 * it's owned by the @self.
 *
 * Returns: (transfer none) (element-type EUICustomizer): a #GPtrArray with the customizers
 *
 * Since: 3.56
 **/
GPtrArray *
e_ui_customize_dialog_get_customizers (EUICustomizeDialog *self)
{
	g_return_val_if_fail (E_IS_UI_CUSTOMIZE_DIALOG (self), NULL);

	return self->customizers;
}

static void
gather_elem_ids_cb (gpointer key,
		    gpointer value,
		    gpointer user_data)
{
	GPtrArray *array = user_data;

	g_ptr_array_add (array, key);
}

static gint
sort_by_lookup_value_cb (gconstpointer aa,
			 gconstpointer bb,
			 gpointer user_data)
{
	GHashTable *hash_table = user_data;
	const gchar *id1 = *((const gchar **) aa);
	const gchar *id2 = *((const gchar **) bb);
	const gchar *display_name1, *display_name2;

	display_name1 = g_hash_table_lookup (hash_table, id1);
	display_name2 = g_hash_table_lookup (hash_table, id2);

	if (display_name1 && display_name2)
		return g_utf8_collate (display_name1, display_name2);
	if (display_name1)
		return 1;
	if (display_name2)
		return -1;

	return 0;
}

static gint
sort_actions_by_display_name_cb (gconstpointer aa,
				 gconstpointer bb)
{
	EUIAction *act1 = *((EUIAction **) aa);
	EUIAction *act2 = *((EUIAction **) bb);

	if (act1 && act2)
		return g_utf8_collate (e_ui_action_get_label (act1), e_ui_action_get_label (act2));

	if (act1)
		return -1;
	if (act2)
		return 1;

	return 0;
}

static void
add_element_children (GtkTreeStore *tree_store,
		      EUIManager *manager,
		      EUIElement *elem,
		      GtkTreeIter *parent)
{
	guint ii, sz;

	if (!elem)
		return;

	sz = e_ui_element_get_n_children (elem);
	for (ii = 0; ii < sz; ii++) {
		EUIElement *child = e_ui_element_get_child (elem, ii);
		GtkTreeIter iter;
		gchar *label = NULL;
		const gchar *const_label = NULL, *action_name;
		gboolean can_drag = TRUE;
		gint weight = WEIGHT_NORMAL;

		switch (e_ui_element_get_kind (child)) {
		case E_UI_ELEMENT_KIND_ROOT:
		case E_UI_ELEMENT_KIND_HEADERBAR:
		case E_UI_ELEMENT_KIND_TOOLBAR:
		case E_UI_ELEMENT_KIND_MENU:
		default:
			g_warn_if_reached ();
			continue;
		case E_UI_ELEMENT_KIND_SUBMENU:
			action_name = e_ui_element_submenu_get_action (child);
			if (action_name) {
				EUIAction *action;

				action = e_ui_manager_get_action (manager, action_name);
				if (action)
					label = e_str_without_underscores (e_ui_action_get_label (action));
			}

			if (!label)
				const_label = action_name;
			if (!const_label)
				const_label = "Submenu"; /* not localized, it should not happen */
			break;
		case E_UI_ELEMENT_KIND_PLACEHOLDER:
			/* placeholders are dropped in the user customizations, only their content is used */
			add_element_children (tree_store, manager, child, parent);
			continue;
		case E_UI_ELEMENT_KIND_SEPARATOR:
			const_label = _("Separator");
			weight = WEIGHT_BOLD;
			break;
		case E_UI_ELEMENT_KIND_START:
			/* Translators: which side a header bar items are placed to; for RTL locales it should be "Right side" */
			const_label = _("Left side");
			can_drag = FALSE;
			weight = WEIGHT_BOLD;
			break;
		case E_UI_ELEMENT_KIND_END:
			/* Translators: which side a header bar items are placed to; for RTL locales it should be "Left side" */
			const_label = _("Right side");
			can_drag = FALSE;
			weight = WEIGHT_BOLD;
			break;
		case E_UI_ELEMENT_KIND_ITEM:
			action_name = e_ui_element_item_get_action (child);
			if (action_name) {
				EUIAction *action;

				action = e_ui_manager_get_action (manager, action_name);
				if (action)
					label = e_str_without_underscores (e_ui_action_get_label (action));
			}

			if (!label)
				const_label = action_name;
			if (!const_label)
				const_label = "Item"; /* not localized, it should not happen */
			break;
		}

		gtk_tree_store_append (tree_store, &iter, parent);
		gtk_tree_store_set (tree_store, &iter,
			COL_LAYOUT_ELEM_OBJ, child,
			COL_LAYOUT_LABEL_STR, label ? label : const_label,
			COL_LAYOUT_CAN_DRAG_BOOL, can_drag,
			COL_LAYOUT_WEIGHT_INT, weight,
			-1);

		g_free (label);

		add_element_children (tree_store, manager, child, &iter);
	}
}

static void
customize_dialog_gather_elements (EUIElement *elem,
				  GHashTable *elements)
{
	guint sz, ii;

	if (!elem)
		return;

	if (e_ui_element_get_kind (elem) == E_UI_ELEMENT_KIND_ITEM &&
	    e_ui_element_item_get_action (elem) &&
	    !g_hash_table_contains (elements, e_ui_element_item_get_action (elem)))
		g_hash_table_insert (elements, (gpointer) e_ui_element_item_get_action (elem), elem);

	sz = e_ui_element_get_n_children (elem);
	for (ii = 0; ii < sz; ii++) {
		EUIElement *child = e_ui_element_get_child (elem, ii);

		customize_dialog_gather_elements (child, elements);
	}
}

static void
part_combo_changed_cb (GtkComboBox *combo,
		       gpointer user_data)
{
	EUICustomizeDialog *self = user_data;
	EUICustomizer *customizer = NULL;
	EUIManager *manager;
	EUIElement *element;
	GtkListStore *list_store;
	GtkTreeStore *tree_store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GPtrArray *groups; /* EUIActionGroup */
	GPtrArray *all_actions; /* EUIAction */
	gchar *id = NULL;
	guint gg, ii, for_kind = 0;
	gboolean is_default = TRUE;

	if (!gtk_combo_box_get_active_iter (combo, &iter))
		return;

	model = gtk_combo_box_get_model (combo);
	gtk_tree_model_get (model, &iter,
		COL_COMBO_ID_STR, &id,
		COL_COMBO_CUSTOMIZER_OBJ, &customizer,
		COL_COMBO_ELEM_KIND_UINT, &for_kind,
		COL_COMBO_DEFAULT_BOOL, &is_default,
		-1);

	if (!customizer || !id) {
		g_clear_object (&customizer);
		g_free (id);
		return;
	}

	manager = e_ui_customizer_get_manager (customizer);

	if (self->actions_for_customizer != customizer ||
	    self->actions_for_kind != for_kind) {
		GHashTable *elements;

		list_store = GTK_LIST_STORE (gtk_tree_view_get_model (self->actions_tree_view));
		gtk_list_store_clear (list_store);

		self->actions_for_customizer = customizer;
		self->actions_for_kind = for_kind;

		/* gather original UI elements and remember them by action name */
		elements = g_hash_table_new (g_str_hash, g_str_equal);
		customize_dialog_gather_elements (e_ui_parser_get_root (e_ui_manager_get_parser (manager)), elements);

		/* fill the available actions */

		groups = e_ui_manager_list_action_groups (manager);
		all_actions = g_ptr_array_sized_new (256);

		for (gg = 0; groups && gg < groups->len; gg++) {
			EUIActionGroup *group = g_ptr_array_index (groups, gg);
			GPtrArray *actions;

			actions = e_ui_action_group_list_actions (group);
			if (!actions)
				continue;

			for (ii = 0; ii < actions->len; ii++) {
				EUIAction *action = g_ptr_array_index (actions, ii);

				if (!(e_ui_action_get_usable_for_kinds (action) & for_kind))
					continue;

				g_ptr_array_add (all_actions, action);
			}

			g_ptr_array_unref (actions);
		}

		g_ptr_array_sort (all_actions, sort_actions_by_display_name_cb);

		if (self->all_shortcuts)
			g_hash_table_remove_all (self->all_shortcuts);
		else
			self->all_shortcuts = g_hash_table_new_full (e_ui_manager_shortcut_def_hash, e_ui_manager_shortcut_def_equal, g_free, (GDestroyNotify) g_ptr_array_unref);

		list_store = GTK_LIST_STORE (g_object_ref (gtk_tree_view_get_model (self->actions_tree_view)));
		gtk_tree_view_set_model (self->actions_tree_view, NULL);

		for (ii = 0; ii < all_actions->len; ii++) {
			EUIAction *action = g_ptr_array_index (all_actions, ii);
			EUIElement *elem;
			gchar *label;
			gchar *markup;
			const gchar *name, *tooltip;
			gboolean is_new_elem = FALSE;

			name = g_action_get_name (G_ACTION (action));
			elem = g_hash_table_lookup (elements, name);
			if (!elem) {
				elem = e_ui_element_new_for_action (action);
				is_new_elem = TRUE;
			}

			label = e_str_without_underscores (e_ui_action_get_label (action));
			tooltip = e_ui_action_get_tooltip (action);
			if (!tooltip)
				tooltip = "";
			markup = g_markup_printf_escaped ("<b>%s</b>\n<small>%s</small>", label, tooltip);

			gtk_list_store_append (list_store, &iter);
			gtk_list_store_set (list_store, &iter,
				COL_ACTIONS_ELEM_OBJ, elem,
				COL_ACTIONS_NAME_STR, name,
				COL_ACTIONS_MARKUP_STR, markup,
				COL_ACTIONS_LABEL_STR, label,
				COL_ACTIONS_TOOLTIP_STR, tooltip,
				-1);

			customize_shortcuts_traverse (self->all_shortcuts, customizer, action, label, customize_shortcuts_add_to_all);

			g_free (label);
			g_free (markup);

			if (is_new_elem)
				e_ui_element_free (elem);
		}

		gtk_tree_view_set_model (self->actions_tree_view, GTK_TREE_MODEL (list_store));
		gtk_tree_view_set_search_column (self->actions_tree_view, COL_ACTIONS_MARKUP_STR);

		g_clear_object (&list_store);
		g_ptr_array_unref (all_actions);
		g_clear_pointer (&groups, g_ptr_array_unref);
		g_hash_table_destroy (elements);
	}

	/* fill the layout */

	element = e_ui_customizer_get_element (customizer, id);
	if (!element && e_ui_parser_get_root (e_ui_manager_get_parser (manager)))
		element = e_ui_element_get_child_by_id (e_ui_parser_get_root (e_ui_manager_get_parser (manager)), id);
	else
		is_default = FALSE;

	tree_store = GTK_TREE_STORE (g_object_ref (gtk_tree_view_get_model (self->layout_tree_view)));

	if (element) {
		gtk_tree_view_set_model (self->layout_tree_view, NULL);

		gtk_tree_store_clear (tree_store);

		add_element_children (tree_store, manager, element, NULL);

		gtk_tree_view_set_model (self->layout_tree_view, GTK_TREE_MODEL (tree_store));
		gtk_tree_view_set_search_column (self->layout_tree_view, COL_LAYOUT_LABEL_STR);
		gtk_tree_view_expand_all (self->layout_tree_view);
	} else {
		gtk_tree_store_clear (tree_store);
	}

	g_clear_object (&tree_store);
	g_clear_object (&customizer);
	g_free (id);

	gtk_widget_set_sensitive (self->layout_default_button, !is_default);
}


static gboolean
e_ui_customize_dialog_save_changes (EUICustomizeDialog *self,
				    GError **error)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GHashTable *save_customizers;
	GHashTableIter ht_iter;
	gpointer key = NULL;
	gboolean success = TRUE;

	model = gtk_combo_box_get_model (self->element_combo);
	if (!model)
		return success;

	if (!gtk_tree_model_get_iter_first (model, &iter))
		return success;

	save_customizers = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);

	do {
		gboolean changed = FALSE;

		gtk_tree_model_get (model, &iter,
			COL_COMBO_CHANGED_BOOL, &changed,
			-1);

		if (changed) {
			EUICustomizer *customizer = NULL;

			gtk_tree_model_get (model, &iter, COL_COMBO_CUSTOMIZER_OBJ, &customizer, -1);

			if (customizer)
				g_hash_table_add (save_customizers, customizer);
		}
	} while (gtk_tree_model_iter_next (model, &iter));

	g_hash_table_iter_init (&ht_iter, self->save_customizers);
	while (g_hash_table_iter_next (&ht_iter, &key, NULL)) {
		EUICustomizer *customizer = key;

		if (!g_hash_table_contains (save_customizers, customizer))
			g_hash_table_add (save_customizers, g_object_ref (customizer));
	}

	if (g_hash_table_size (save_customizers) > 0) {
		g_hash_table_iter_init (&ht_iter, save_customizers);
		while (g_hash_table_iter_next (&ht_iter, &key, NULL)) {
			EUICustomizer *customizer = key;

			success = e_ui_customizer_save (customizer, error);
			if (!success)
				break;

			e_ui_manager_changed (e_ui_customizer_get_manager (customizer));
		}
	}

	g_hash_table_destroy (save_customizers);

	if (success) {
		g_hash_table_remove_all (self->save_customizers);
		if (gtk_tree_model_get_iter_first (model, &iter)) {
			do {
				gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					COL_COMBO_CHANGED_BOOL, FALSE,
					-1);
			} while (gtk_tree_model_iter_next (model, &iter));
		}
	}

	return success;
}

/**
 * e_ui_customize_dialog_run:
 * @self: an #EUICustomizer
 * @preselect_id: (nullable): an ID to preselect, or %NULL
 *
 * Runs a dialog, which allows UI customizations.
 *
 * When the @preselect_id is not %NULL, it should be one of the registered
 * ID-s by the e_ui_customizer_register(). It will be preselected
 * for the customization.
 *
 * Since: 3.56
 **/
void
e_ui_customize_dialog_run (EUICustomizeDialog *self,
			   const gchar *preselect_id)
{
	GtkTreeIter iter;
	GtkListStore *list_store;
	GHashTable *hash_table;
	GPtrArray *array;
	gulong handler_id;
	guint ii, jj;
	gboolean done;

	g_return_if_fail (E_IS_UI_CUSTOMIZE_DIALOG (self));
	g_return_if_fail (self->customizers->len > 0);

	list_store = GTK_LIST_STORE (g_object_ref (gtk_combo_box_get_model (self->element_combo)));

	gtk_combo_box_set_model (self->element_combo, NULL);

	gtk_list_store_clear (list_store);

	hash_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	for (ii = 0; ii < self->customizers->len; ii++) {
		EUICustomizer *customizer = g_ptr_array_index (self->customizers, ii);

		array = e_ui_customizer_list_registered (customizer);
		for (jj = 0; array && jj < array->len; jj++) {
			const gchar *id = g_ptr_array_index (array, jj);

			if (!id)
				continue;

			g_warn_if_fail (!g_hash_table_contains (hash_table, id));
			g_hash_table_insert (hash_table, g_strdup (id), (gpointer) e_ui_customizer_get_registered_display_name (customizer, id));
		}

		g_clear_pointer (&array, g_ptr_array_unref);
	}

	array = g_ptr_array_sized_new (g_hash_table_size (hash_table));
	g_hash_table_foreach (hash_table, gather_elem_ids_cb, array);

	g_ptr_array_sort_with_data (array, sort_by_lookup_value_cb, hash_table);

	jj = array->len; /* used for "found preselect_id"; out of range is "not found" */
	for (ii = 0; ii < array->len; ii++) {
		const gchar *id = g_ptr_array_index (array, ii);
		const gchar *display_name;
		EUICustomizer *customizer = NULL;
		EUIManager *manager;
		EUIElement *elem;
		guint kk, elem_kind = 0;
		gboolean is_default;

		if (!id)
			continue;

		display_name = g_hash_table_lookup (hash_table, id);
		if (!display_name)
			continue;

		if (jj == array->len && preselect_id && g_strcmp0 (id, preselect_id) == 0)
			jj = (guint) gtk_tree_model_iter_n_children (GTK_TREE_MODEL (list_store), NULL);

		for (kk = 0; kk < self->customizers->len; kk++) {
			customizer = g_ptr_array_index (self->customizers, kk);

			if (e_ui_customizer_get_registered_display_name (customizer, id))
				break;
			else
				customizer = NULL;
		}

		if (!customizer) {
			g_warning ("%s: Failed to find customizer for item id '%s'", G_STRFUNC, id);
			continue;
		}

		manager = e_ui_customizer_get_manager (customizer);
		elem = e_ui_parser_get_root (e_ui_manager_get_parser (manager));
		if (elem) {
			elem = e_ui_element_get_child_by_id (elem, id);
			g_warn_if_fail (elem != NULL);

			if (elem)
				elem_kind = e_ui_element_get_kind (elem);
		} else {
			g_warn_if_reached ();
		}

		is_default = !e_ui_customizer_get_element (customizer, id);

		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter,
			COL_COMBO_ID_STR, id,
			COL_COMBO_DISPLAY_NAME_STR, display_name,
			COL_COMBO_CUSTOMIZER_OBJ, customizer,
			COL_COMBO_CHANGED_BOOL, FALSE,
			COL_COMBO_ELEM_KIND_UINT, elem_kind,
			COL_COMBO_DEFAULT_BOOL, is_default,
			-1);
	}

	gtk_combo_box_set_model (self->element_combo, GTK_TREE_MODEL (list_store));

	if (jj >= array->len)
		jj = 0;

	if (jj < (guint) gtk_tree_model_iter_n_children (GTK_TREE_MODEL (list_store), NULL))
		gtk_combo_box_set_active (self->element_combo, jj);

	g_ptr_array_unref (array);
	g_clear_object (&list_store);
	g_hash_table_destroy (hash_table);

	handler_id = g_signal_connect (self->element_combo, "changed",
		G_CALLBACK (part_combo_changed_cb), self);

	self->actions_for_customizer = NULL;
	self->actions_for_kind = 0;

	part_combo_changed_cb (self->element_combo, self);

	done = FALSE;
	while (!done) {
		GError *local_error = NULL;

		gtk_dialog_run (GTK_DIALOG (self));

		done = e_ui_customize_dialog_save_changes (self, &local_error);
		if (!done) {
			e_alert_run_dialog_for_args (gtk_window_get_transient_for (GTK_WINDOW (self)),
				"system:generic-error", _("Failed to save changes."),
				local_error ? local_error->message : _("Unknown error"), NULL);
		}

		g_clear_error (&local_error);
	}

	g_signal_handler_disconnect (self->element_combo, handler_id);
}
