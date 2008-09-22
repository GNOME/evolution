/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-content.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-shell-content.h"

#include <glib/gi18n.h>

#include <filter/rule-editor.h>
#include <widgets/misc/e-action-combo-box.h>
#include <widgets/misc/e-icon-entry.h>

#include <e-shell-module.h>
#include <e-shell-view.h>
#include <e-shell-window-actions.h>

#define E_SHELL_CONTENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_CONTENT, EShellContentPrivate))

struct _EShellContentPrivate {

	gpointer shell_view;  /* weak pointer */

	RuleContext *search_context;
	FilterRule *search_rule;
	gchar *system_filename;
	gchar *user_filename;

	/* Container for the following widgets */
	GtkWidget *search_bar;

	/* Search bar widgets */
	GtkWidget *filter_label;
	GtkWidget *filter_combo_box;
	GtkWidget *search_label;
	GtkWidget *search_entry;
	GtkWidget *scope_label;
	GtkWidget *scope_combo_box;

	GtkStateType search_state;
};

enum {
	PROP_0,
	PROP_FILTER_ACTION,
	PROP_FILTER_VALUE,
	PROP_FILTER_VISIBLE,
	PROP_SEARCH_CONTEXT,
	PROP_SEARCH_RULE,
	PROP_SEARCH_TEXT,
	PROP_SEARCH_VALUE,
	PROP_SEARCH_VISIBLE,
	PROP_SCOPE_ACTION,
	PROP_SCOPE_VALUE,
	PROP_SCOPE_VISIBLE,
	PROP_SHELL_VIEW
};

static gpointer parent_class;

static void
shell_content_dialog_rule_changed (GtkWidget *dialog,
                                            FilterRule *rule)
{
	gboolean sensitive;

	sensitive = (rule != NULL) && (rule->parts != NULL);

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK, sensitive);
	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (dialog), GTK_RESPONSE_APPLY, sensitive);
}

static void
action_search_execute_cb (GtkAction *action,
                          EShellContent *shell_content)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EIconEntry *icon_entry;
	GtkWidget *child;
	GtkStateType visual_state;
	const gchar *search_text;

	/* EShellView subclasses are responsible for actually
	 * executing the search.  This is all cosmetic stuff. */

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	if (!e_shell_view_is_active (shell_view))
		return;

	icon_entry = E_ICON_ENTRY (shell_content->priv->search_entry);
	search_text = e_shell_content_get_search_text (shell_content);

	if (search_text != NULL && *search_text != '\0')
		visual_state = GTK_STATE_SELECTED;
	else
		visual_state = GTK_STATE_NORMAL;

	e_icon_entry_set_visual_state (icon_entry, visual_state);

	action = E_SHELL_WINDOW_ACTION_SEARCH_CLEAR (shell_window);
	gtk_action_set_sensitive (action, TRUE);

	action = E_SHELL_WINDOW_ACTION_SEARCH_SAVE (shell_window);
	gtk_action_set_sensitive (action, TRUE);

	/* Direct the focus away from the search entry, so that a
	 * focus-in event is required before the text can be changed.
	 * This will reset the entry to the appropriate visual state. */
	gtk_widget_grab_focus (gtk_bin_get_child (GTK_BIN (shell_content)));
}

static void
shell_content_entry_activated_cb (EShellContent *shell_content,
                                  GtkWidget *entry)
{
	EShellWindow *shell_window;
	EShellView *shell_view;
	GtkAction *action;

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	/* Verify the shell view is active before proceeding. */
	if (!e_shell_view_is_active (shell_view))
		return;

	action = E_SHELL_WINDOW_ACTION_SEARCH_EXECUTE (shell_window);
	gtk_action_activate (action);
}

static gboolean
shell_content_entry_focus_in_cb (EShellContent *shell_content,
                                 GdkEventFocus *focus_event,
                                 GtkWidget *entry)
{
	EIconEntry *icon_entry;
	GtkStateType visual_state;

	icon_entry = E_ICON_ENTRY (shell_content->priv->search_entry);
	visual_state = e_icon_entry_get_visual_state (icon_entry);

	if (visual_state == GTK_STATE_INSENSITIVE)
		gtk_entry_set_text (GTK_ENTRY (entry), "");

	e_icon_entry_set_visual_state (icon_entry, GTK_STATE_NORMAL);

	return FALSE;
}

static gboolean
shell_content_entry_focus_out_cb (EShellContent *shell_content,
                                  GdkEventFocus *focus_event,
                                  GtkWidget *entry)
{
	/* FIXME */
	return FALSE;
}

static gboolean
shell_content_entry_key_press_cb (EShellContent *shell_content,
                                  GdkEventKey *key_event,
                                  GtkWidget *entry)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkAction *action;
	guint mask;

	mask = gtk_accelerator_get_default_mod_mask ();
	if ((key_event->state & mask) != GDK_MOD1_MASK)
		return FALSE;

	if (key_event->keyval != GDK_Down)
		return FALSE;

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	action = E_SHELL_WINDOW_ACTION_SEARCH_OPTIONS (shell_window);
	gtk_action_activate (action);

	return TRUE;
}

static void
shell_content_init_search_context (EShellContent *shell_content)
{
	EShellView *shell_view;
	EShellModule *shell_module;
	EShellViewClass *shell_view_class;
	RuleContext *context;
	FilterRule *rule;
	FilterPart *part;
	gchar *system_filename;
	gchar *user_filename;

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	shell_module = E_SHELL_MODULE (shell_view_class->type_module);

	/* The filename for built-in searches is specified in a
	 * module's EShellModuleInfo.  All built-in search rules
	 * live in the same directory. */
	system_filename = g_build_filename (
		EVOLUTION_RULEDIR,
		e_shell_module_get_searches (shell_module), NULL);

	/* The filename for custom saved searches is always of
	 * the form "$(shell_module_data_dir)/searches.xml". */
	user_filename = g_build_filename (
		e_shell_module_get_data_dir (shell_module),
		"searches.xml", NULL);

	context = rule_context_new ();
	rule_context_add_part_set (
		context, "partset", FILTER_TYPE_PART,
		rule_context_add_part, rule_context_next_part);
	rule_context_add_rule_set (
		context, "ruleset", FILTER_TYPE_RULE,
		rule_context_add_rule, rule_context_next_rule);
	rule_context_load (context, system_filename, user_filename);

	/* XXX Not sure why this is necessary. */
	g_object_set_data_full (
		G_OBJECT (context), "system",
		g_strdup (system_filename), g_free);
	g_object_set_data_full (
		G_OBJECT (context), "user",
		g_strdup (user_filename), g_free);

	rule = filter_rule_new ();
	part = rule_context_next_part (context, NULL);
	if (part == NULL)
		g_warning (
			"Could not load %s search: no parts",
			shell_view_class->type_module->name);
	else
		filter_rule_add_part (rule, filter_part_clone (part));

	shell_content->priv->search_context = context;
	shell_content->priv->system_filename = system_filename;
	shell_content->priv->user_filename = user_filename;
}

static void
shell_content_set_shell_view (EShellContent *shell_content,
                              EShellView *shell_view)
{
	g_return_if_fail (shell_content->priv->shell_view == NULL);

	shell_content->priv->shell_view = shell_view;

	g_object_add_weak_pointer (
		G_OBJECT (shell_view),
		&shell_content->priv->shell_view);
}

static void
shell_content_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FILTER_ACTION:
			e_shell_content_set_filter_action (
				E_SHELL_CONTENT (object),
				g_value_get_object (value));
			return;

		case PROP_FILTER_VALUE:
			e_shell_content_set_filter_value (
				E_SHELL_CONTENT (object),
				g_value_get_int (value));
			return;

		case PROP_FILTER_VISIBLE:
			e_shell_content_set_filter_visible (
				E_SHELL_CONTENT (object),
				g_value_get_boolean (value));
			return;

		case PROP_SEARCH_RULE:
			e_shell_content_set_search_rule (
				E_SHELL_CONTENT (object),
				g_value_get_object (value));
			return;

		case PROP_SEARCH_TEXT:
			e_shell_content_set_search_text (
				E_SHELL_CONTENT (object),
				g_value_get_string (value));
			return;

		case PROP_SEARCH_VALUE:
			e_shell_content_set_search_value (
				E_SHELL_CONTENT (object),
				g_value_get_int (value));
			return;

		case PROP_SEARCH_VISIBLE:
			e_shell_content_set_search_visible (
				E_SHELL_CONTENT (object),
				g_value_get_boolean (value));
			return;

		case PROP_SCOPE_ACTION:
			e_shell_content_set_scope_action (
				E_SHELL_CONTENT (object),
				g_value_get_object (value));
			return;

		case PROP_SCOPE_VALUE:
			e_shell_content_set_scope_value (
				E_SHELL_CONTENT (object),
				g_value_get_int (value));
			return;

		case PROP_SCOPE_VISIBLE:
			e_shell_content_set_scope_visible (
				E_SHELL_CONTENT (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHELL_VIEW:
			shell_content_set_shell_view (
				E_SHELL_CONTENT (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_content_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FILTER_ACTION:
			g_value_set_object (
				value, e_shell_content_get_filter_action (
				E_SHELL_CONTENT (object)));
			return;

		case PROP_FILTER_VALUE:
			g_value_set_int (
				value, e_shell_content_get_filter_value (
				E_SHELL_CONTENT (object)));
			return;

		case PROP_FILTER_VISIBLE:
			g_value_set_boolean (
				value, e_shell_content_get_filter_visible (
				E_SHELL_CONTENT (object)));
			return;

		case PROP_SEARCH_CONTEXT:
			g_value_set_object (
				value, e_shell_content_get_search_context (
				E_SHELL_CONTENT (object)));
			return;

		case PROP_SEARCH_RULE:
			g_value_set_object (
				value, e_shell_content_get_search_rule (
				E_SHELL_CONTENT (object)));
			return;

		case PROP_SEARCH_TEXT:
			g_value_set_string (
				value, e_shell_content_get_search_text (
				E_SHELL_CONTENT (object)));
			return;

		case PROP_SEARCH_VALUE:
			g_value_set_int (
				value, e_shell_content_get_search_value (
				E_SHELL_CONTENT (object)));
			return;

		case PROP_SEARCH_VISIBLE:
			g_value_set_boolean (
				value, e_shell_content_get_search_visible (
				E_SHELL_CONTENT (object)));
			return;

		case PROP_SCOPE_ACTION:
			g_value_set_object (
				value, e_shell_content_get_scope_action (
				E_SHELL_CONTENT (object)));
			return;

		case PROP_SCOPE_VALUE:
			g_value_set_int (
				value, e_shell_content_get_scope_value (
				E_SHELL_CONTENT (object)));
			return;

		case PROP_SCOPE_VISIBLE:
			g_value_set_boolean (
				value, e_shell_content_get_scope_visible (
				E_SHELL_CONTENT (object)));
			return;

		case PROP_SHELL_VIEW:
			g_value_set_object (
				value, e_shell_content_get_shell_view (
				E_SHELL_CONTENT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_content_dispose (GObject *object)
{
	EShellContentPrivate *priv;

	priv = E_SHELL_CONTENT_GET_PRIVATE (object);

	if (priv->shell_view != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->shell_view), &priv->shell_view);
		priv->shell_view = NULL;
	}

	if (priv->filter_label != NULL) {
		g_object_unref (priv->filter_label);
		priv->filter_label = NULL;
	}

        if (priv->filter_combo_box != NULL) {
                g_object_unref (priv->filter_combo_box);
                priv->filter_combo_box = NULL;
        }

	if (priv->search_context != NULL) {
		g_object_unref (priv->search_context);
		priv->search_context = NULL;
	}

        if (priv->search_label != NULL) {
                g_object_unref (priv->search_label);
                priv->search_label = NULL;
        }

        if (priv->search_entry != NULL) {
                g_object_unref (priv->search_entry);
                priv->search_entry = NULL;
        }

        if (priv->scope_label != NULL) {
                g_object_unref (priv->scope_label);
                priv->scope_label = NULL;
        }

        if (priv->scope_combo_box != NULL) {
                g_object_unref (priv->scope_combo_box);
                priv->scope_combo_box = NULL;
        }

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
shell_content_finalize (GObject *object)
{
	EShellContentPrivate *priv;

	priv = E_SHELL_CONTENT_GET_PRIVATE (object);

	g_free (priv->system_filename);
	g_free (priv->user_filename);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
shell_content_constructed (GObject *object)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	EIconEntry *icon_entry;
	GtkAction *action;

	shell_content = E_SHELL_CONTENT (object);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);
	icon_entry = E_ICON_ENTRY (shell_content->priv->search_entry);

	action = E_SHELL_WINDOW_ACTION_SEARCH_CLEAR (shell_window);
	e_icon_entry_add_action_end (icon_entry, action);

	action = E_SHELL_WINDOW_ACTION_SEARCH_EXECUTE (shell_window);
	g_signal_connect (
		action, "activate",
		G_CALLBACK (action_search_execute_cb), shell_content);

	action = E_SHELL_WINDOW_ACTION_SEARCH_OPTIONS (shell_window);
	e_icon_entry_add_action_start (icon_entry, action);
}

static void
shell_content_realize (GtkWidget *widget)
{
	EShellContent *shell_content;

	/* We can't call this during object construction because the
	 * shell view is still in its instance initialization phase,
	 * and so its GET_CLASS() macro won't work correctly.  So we
	 * delay the bits of our own initialization that require the
	 * E_SHELL_VIEW_GET_CLASS() macro until after the shell view
	 * is fully constructed. */

	shell_content = E_SHELL_CONTENT (widget);
	shell_content_init_search_context (shell_content);

	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (parent_class)->realize (widget);
}

static void
shell_content_size_request (GtkWidget *widget,
                            GtkRequisition *requisition)
{
	EShellContentPrivate *priv;
	GtkRequisition child_requisition;
	GtkWidget *child;

	priv = E_SHELL_CONTENT_GET_PRIVATE (widget);

	requisition->width = 0;
	requisition->height = 0;

	child = gtk_bin_get_child (GTK_BIN (widget));
	gtk_widget_size_request (child, requisition);

	child = priv->search_bar;
	gtk_widget_size_request (child, &child_requisition);
	requisition->width = MAX (requisition->width, child_requisition.width);
	requisition->height += child_requisition.height;
}

static void
shell_content_size_allocate (GtkWidget *widget,
                             GtkAllocation *allocation)
{
	EShellContentPrivate *priv;
	GtkAllocation child_allocation;
	GtkRequisition child_requisition;
	GtkWidget *child;

	priv = E_SHELL_CONTENT_GET_PRIVATE (widget);

	widget->allocation = *allocation;

	child = priv->search_bar;
	gtk_widget_size_request (child, &child_requisition);

	child_allocation.x = allocation->x;
	child_allocation.y = allocation->y;
	child_allocation.width = allocation->width;
	child_allocation.height = child_requisition.height;

	gtk_widget_size_allocate (child, &child_allocation);

	child_allocation.y += child_requisition.height;
	child_allocation.height =
		allocation->height - child_requisition.height;

	child = gtk_bin_get_child (GTK_BIN (widget));
	if (child != NULL)
		gtk_widget_size_allocate (child, &child_allocation);
}

static void
shell_content_remove (GtkContainer *container,
                      GtkWidget *widget)
{
	EShellContentPrivate *priv;

	priv = E_SHELL_CONTENT_GET_PRIVATE (container);

	/* Look in the internal widgets first. */

	if (widget == priv->search_bar) {
		gtk_widget_unparent (priv->search_bar);
		gtk_widget_queue_resize (GTK_WIDGET (container));
		return;
	}

	/* Chain up to parent's remove() method. */
	GTK_CONTAINER_CLASS (parent_class)->remove (container, widget);
}

static void
shell_content_forall (GtkContainer *container,
                      gboolean include_internals,
                      GtkCallback callback,
                      gpointer callback_data)
{
	EShellContentPrivate *priv;

	priv = E_SHELL_CONTENT_GET_PRIVATE (container);

	if (include_internals)
		callback (priv->search_bar, callback_data);

	/* Chain up to parent's forall() method. */
	GTK_CONTAINER_CLASS (parent_class)->forall (
		container, include_internals, callback, callback_data);
}

static void
shell_content_class_init (EShellContentClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EShellContentPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_content_set_property;
	object_class->get_property = shell_content_get_property;
	object_class->dispose = shell_content_dispose;
	object_class->finalize = shell_content_finalize;
	object_class->constructed = shell_content_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = shell_content_realize;
	widget_class->size_request = shell_content_size_request;
	widget_class->size_allocate = shell_content_size_allocate;

	container_class = GTK_CONTAINER_CLASS (class);
	container_class->remove = shell_content_remove;
	container_class->forall = shell_content_forall;

	g_object_class_install_property (
		object_class,
		PROP_FILTER_ACTION,
		g_param_spec_object (
			"filter-action",
			NULL,
			NULL,
			GTK_TYPE_RADIO_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FILTER_VALUE,
		g_param_spec_int (
			"filter-value",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FILTER_VISIBLE,
		g_param_spec_boolean (
			"filter-visible",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SEARCH_CONTEXT,
		g_param_spec_object (
			"search-context",
			NULL,
			NULL,
			RULE_TYPE_CONTEXT,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_SEARCH_RULE,
		g_param_spec_object (
			"search-rule",
			NULL,
			NULL,
			FILTER_TYPE_RULE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SEARCH_TEXT,
		g_param_spec_string (
			"search-text",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SEARCH_VALUE,
		g_param_spec_int (
			"search-value",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SEARCH_VISIBLE,
		g_param_spec_boolean (
			"search-visible",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SCOPE_ACTION,
		g_param_spec_object (
			"scope-action",
			NULL,
			NULL,
			GTK_TYPE_RADIO_ACTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SCOPE_VALUE,
		g_param_spec_int (
			"scope-value",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SCOPE_VISIBLE,
		g_param_spec_boolean (
			"scope-visible",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SHELL_VIEW,
		g_param_spec_object (
			"shell-view",
			NULL,
			NULL,
			E_TYPE_SHELL_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
shell_content_init (EShellContent *shell_content)
{
	GtkBox *box;
	GtkLabel *label;
	GtkWidget *mnemonic;
	GtkWidget *widget;
	EIconEntry *icon_entry;

	shell_content->priv = E_SHELL_CONTENT_GET_PRIVATE (shell_content);

	GTK_WIDGET_SET_FLAGS (shell_content, GTK_NO_WINDOW);

	/*** Build the Search Bar ***/

	widget = gtk_hbox_new (FALSE, 3);
	gtk_widget_set_parent (widget, GTK_WIDGET (shell_content));
	shell_content->priv->search_bar = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	box = GTK_BOX (widget);

	/* Filter Combo Widgets */

	/* Translators: The "Show:" label precedes a combo box that
	 * allows the user to filter the current view.  Examples of
	 * items that appear in the combo box are "Unread Messages",
	 * "Important Messages", or "Active Appointments". */
	widget = gtk_label_new_with_mnemonic (_("Sho_w:"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	shell_content->priv->filter_label = g_object_ref (widget);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = e_action_combo_box_new ();
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	shell_content->priv->filter_combo_box = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Scope Combo Widgets */

	widget = e_action_combo_box_new ();
	gtk_box_pack_end (box, widget, FALSE, FALSE, 0);
	shell_content->priv->scope_combo_box = g_object_ref (widget);
	gtk_widget_show (widget); 

	mnemonic = widget;

	/* Translators: This is part of the quick search interface.
	 * example: Search: [_______________] in [ Current Folder ] */
	widget = gtk_label_new_with_mnemonic (_("i_n"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), mnemonic);
	gtk_box_pack_end (box, widget, FALSE, FALSE, 0);
	shell_content->priv->scope_label = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Search Entry Widgets */

	widget = e_icon_entry_new ();
	gtk_box_pack_end (box, widget, FALSE, FALSE, 0);
	shell_content->priv->search_entry = g_object_ref (widget);
	shell_content->priv->search_state = GTK_STATE_NORMAL;
	gtk_widget_show (widget);

	icon_entry = E_ICON_ENTRY (widget);

	/* Translators: This is part of the quick search interface.
	 * example: Search: [_______________] in [ Current Folder ] */
	widget = gtk_label_new_with_mnemonic (_("Sear_ch:"));
	gtk_box_pack_end (box, widget, FALSE, FALSE, 0);
	shell_content->priv->search_label = g_object_ref (widget);
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = e_icon_entry_get_entry (icon_entry);
	gtk_label_set_mnemonic_widget (label, widget);
	g_signal_connect_swapped (
		widget, "activate",
		G_CALLBACK (shell_content_entry_activated_cb), shell_content);
	g_signal_connect_swapped (
		widget, "focus-in-event",
		G_CALLBACK (shell_content_entry_focus_in_cb), shell_content);
	g_signal_connect_swapped (
		widget, "focus-out-event",
		G_CALLBACK (shell_content_entry_focus_out_cb), shell_content);
	g_signal_connect_swapped (
		widget, "key-press-event",
		G_CALLBACK (shell_content_entry_key_press_cb), shell_content);
}

GType
e_shell_content_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EShellContentClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) shell_content_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EShellContent),
			0,     /* n_preallocs */
			(GInstanceInitFunc) shell_content_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_BIN, "EShellContent", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_shell_content_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_SHELL_CONTENT, "shell-view", shell_view, NULL);
}

EShellView *
e_shell_content_get_shell_view (EShellContent *shell_content)
{
	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), NULL);

	return E_SHELL_VIEW (shell_content->priv->shell_view);
}

GtkRadioAction *
e_shell_content_get_filter_action (EShellContent *shell_content)
{
	EActionComboBox *combo_box;

	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), NULL);

	combo_box = E_ACTION_COMBO_BOX (shell_content->priv->filter_combo_box);

	return e_action_combo_box_get_action (combo_box);
}

void
e_shell_content_set_filter_action (EShellContent *shell_content,
                                   GtkRadioAction *filter_action)
{
	EActionComboBox *combo_box;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	combo_box = E_ACTION_COMBO_BOX (shell_content->priv->filter_combo_box);

	e_action_combo_box_set_action (combo_box, filter_action);
	g_object_notify (G_OBJECT (shell_content), "filter-action");
}

gint
e_shell_content_get_filter_value (EShellContent *shell_content)
{
	EActionComboBox *combo_box;

	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), 0);

	combo_box = E_ACTION_COMBO_BOX (shell_content->priv->filter_combo_box);

	return e_action_combo_box_get_current_value (combo_box);
}

void
e_shell_content_set_filter_value (EShellContent *shell_content,
                                  gint filter_value)
{
	EActionComboBox *combo_box;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	combo_box = E_ACTION_COMBO_BOX (shell_content->priv->filter_combo_box);

	e_action_combo_box_set_current_value (combo_box, filter_value);
	g_object_notify (G_OBJECT (shell_content), "filter-value");
}

gboolean
e_shell_content_get_filter_visible (EShellContent *shell_content)
{
	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), FALSE);

	return GTK_WIDGET_VISIBLE (shell_content->priv->filter_combo_box);
}

void
e_shell_content_set_filter_visible (EShellContent *shell_content,
                                    gboolean filter_visible)
{
	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	if (filter_visible) {
		gtk_widget_show (shell_content->priv->filter_label);
		gtk_widget_show (shell_content->priv->filter_combo_box);
	} else {
		gtk_widget_hide (shell_content->priv->filter_label);
		gtk_widget_hide (shell_content->priv->filter_combo_box);
	}
}

RuleContext *
e_shell_content_get_search_context (EShellContent *shell_content)
{
	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), NULL);

	return shell_content->priv->search_context;
}

FilterRule *
e_shell_content_get_search_rule (EShellContent *shell_content)
{
	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), NULL);

	return shell_content->priv->search_rule;
}

void
e_shell_content_set_search_rule (EShellContent *shell_content,
                                 FilterRule *search_rule)
{
	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	if (search_rule != NULL) {
		g_return_if_fail (IS_FILTER_RULE (search_rule));
		g_object_ref (search_rule);
	}

	if (shell_content->priv->search_rule != NULL)
		g_object_unref (shell_content->priv->search_rule);

	shell_content->priv->search_rule = search_rule;
	g_object_notify (G_OBJECT (shell_content), "search-rule");
}

const gchar *
e_shell_content_get_search_text (EShellContent *shell_content)
{
	EIconEntry *icon_entry;
	GtkWidget *text_entry;

	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), NULL);

	if (shell_content->priv->search_state == GTK_STATE_INSENSITIVE)
		return "";

	icon_entry = E_ICON_ENTRY (shell_content->priv->search_entry);
	text_entry = e_icon_entry_get_entry (icon_entry);

	return gtk_entry_get_text (GTK_ENTRY (text_entry));
}

void
e_shell_content_set_search_text (EShellContent *shell_content,
                                 const gchar *search_text)
{
	EIconEntry *icon_entry;
	GtkWidget *text_entry;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	icon_entry = E_ICON_ENTRY (shell_content->priv->search_entry);
	text_entry = e_icon_entry_get_entry (icon_entry);

	search_text = (search_text != NULL) ? search_text : "";
	gtk_entry_set_text (GTK_ENTRY (text_entry), search_text);
	g_object_notify (G_OBJECT (shell_content), "search-text");
}

gint
e_shell_content_get_search_value (EShellContent *shell_content)
{
	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), 0);

	/* FIXME */
	return 0;
}

void
e_shell_content_set_search_value (EShellContent *shell_content,
                                  gint search_value)
{
	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	/* FIXME */

	g_object_notify (G_OBJECT (shell_content), "search-value");
}

gboolean
e_shell_content_get_search_visible (EShellContent *shell_content)
{
	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), FALSE);

	return GTK_WIDGET_VISIBLE (shell_content->priv->search_entry);
}

void
e_shell_content_set_search_visible (EShellContent *shell_content,
                                    gboolean search_visible)
{
	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	if (search_visible) {
		gtk_widget_show (shell_content->priv->search_label);
		gtk_widget_show (shell_content->priv->search_entry);
	} else {
		gtk_widget_hide (shell_content->priv->search_label);
		gtk_widget_hide (shell_content->priv->search_entry);
	}
}

GtkRadioAction *
e_shell_content_get_scope_action (EShellContent *shell_content)
{
	EActionComboBox *combo_box;

	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), NULL);

	combo_box = E_ACTION_COMBO_BOX (shell_content->priv->scope_combo_box);

	return e_action_combo_box_get_action (combo_box);
}

void
e_shell_content_set_scope_action (EShellContent *shell_content,
                                  GtkRadioAction *scope_action)
{
	EActionComboBox *combo_box;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	combo_box = E_ACTION_COMBO_BOX (shell_content->priv->scope_combo_box);

	e_action_combo_box_set_action (combo_box, scope_action);
	g_object_notify (G_OBJECT (shell_content), "scope-action");
}

gint
e_shell_content_get_scope_value (EShellContent *shell_content)
{
	EActionComboBox *combo_box;

	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), 0);

	combo_box = E_ACTION_COMBO_BOX (shell_content->priv->scope_combo_box);

	return e_action_combo_box_get_current_value (combo_box);
}

void
e_shell_content_set_scope_value (EShellContent *shell_content,
                                 gint scope_value)
{
	EActionComboBox *combo_box;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	combo_box = E_ACTION_COMBO_BOX (shell_content->priv->scope_combo_box);

	e_action_combo_box_set_current_value (combo_box, scope_value);
	g_object_notify (G_OBJECT (shell_content), "scope-value");
}

gboolean
e_shell_content_get_scope_visible (EShellContent *shell_content)
{
	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), FALSE);

	return GTK_WIDGET_VISIBLE (shell_content->priv->scope_combo_box);
}

void
e_shell_content_set_scope_visible (EShellContent *shell_content,
                                   gboolean scope_visible)
{
	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	if (scope_visible) {
		gtk_widget_show (shell_content->priv->scope_label);
		gtk_widget_show (shell_content->priv->scope_combo_box);
	} else {
		gtk_widget_hide (shell_content->priv->scope_label);
		gtk_widget_hide (shell_content->priv->scope_combo_box);
	}

	g_object_notify (G_OBJECT (shell_content), "scope-visible");
}

void
e_shell_content_run_advanced_search_dialog (EShellContent *shell_content)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkAction *action;
	GtkWidget *dialog;
	GtkWidget *widget;
	FilterRule *rule;
	RuleContext *context;
	const gchar *user_filename;
	gint response;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);
	user_filename = shell_content->priv->user_filename;

	rule = e_shell_content_get_search_rule (shell_content);

	if (rule == NULL)
		rule = filter_rule_new ();
	else
		rule = filter_rule_clone (rule);

	context = e_shell_content_get_search_context (shell_content);
	widget = filter_rule_get_widget (rule, context);
	filter_rule_set_source (rule, FILTER_SOURCE_INCOMING);

	dialog = gtk_dialog_new_with_buttons (
		_("Advanced Search"), GTK_WINDOW (shell_window),
		GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_APPLY,
		GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 7);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 3);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 600, 300);

	gtk_box_pack_start (
		GTK_BOX (GTK_DIALOG (dialog)->vbox), widget, TRUE, TRUE, 0);

	g_signal_connect_swapped (
		rule, "changed", G_CALLBACK (
		shell_content_dialog_rule_changed), dialog);

	shell_content_dialog_rule_changed (dialog, rule);

run:
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response != GTK_RESPONSE_OK && response != GTK_RESPONSE_APPLY)
		goto exit;

	if (!filter_rule_validate (rule))
		goto run;

	e_shell_content_set_search_rule (shell_content, rule);

	action = E_SHELL_WINDOW_ACTION_SEARCH_EXECUTE (shell_window);
	gtk_action_activate (action);

	if (response == GTK_RESPONSE_APPLY) {
		if (!rule_context_find_rule (context, rule->name, rule->source))
			rule_context_add_rule (context, rule);
		rule_context_save (context, user_filename);
		goto run;
	}

exit:
	g_object_unref (rule);
	gtk_widget_destroy (dialog);
}

void
e_shell_content_run_edit_searches_dialog (EShellContent *shell_content)
{
	RuleContext *context;
	RuleEditor *editor;
	const gchar *user_filename;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	context = e_shell_content_get_search_context (shell_content);
	user_filename = shell_content->priv->user_filename;

	editor = rule_editor_new (
		context, FILTER_SOURCE_INCOMING, _("Searches"));
	gtk_window_set_title (GTK_WINDOW (editor), _("Searches"));

	if (gtk_dialog_run (GTK_DIALOG (editor)) == GTK_RESPONSE_OK)
		rule_context_save (context, user_filename);

	gtk_widget_destroy (GTK_WIDGET (editor));
}

void
e_shell_content_run_save_search_dialog (EShellContent *shell_content)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkWidget *dialog;
	GtkWidget *widget;
	FilterRule *rule;
	RuleContext *context;
	const gchar *search_text;
	const gchar *user_filename;
	gchar *search_name;
	gint response;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);
	user_filename = shell_content->priv->user_filename;

	rule = e_shell_content_get_search_rule (shell_content);
	g_return_if_fail (IS_FILTER_RULE (rule));
	rule = filter_rule_clone (rule);

	search_text = e_shell_content_get_search_text (shell_content);
	if (search_text == NULL || *search_text == '\0')
		search_text = "''";

	search_name = g_strdup_printf ("%s %s", rule->name, search_text);
	filter_rule_set_name (rule, search_name);
	g_free (search_name);

	context = e_shell_content_get_search_context (shell_content);
	widget = filter_rule_get_widget (rule, context);
	filter_rule_set_source (rule, FILTER_SOURCE_INCOMING);

	dialog = gtk_dialog_new_with_buttons (
		_("Save Search"), GTK_WINDOW (shell_window),
		GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 7);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 3);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 300);

	gtk_box_pack_start (
		GTK_BOX (GTK_DIALOG (dialog)->vbox), widget, TRUE, TRUE, 0);

	g_signal_connect_swapped (
		rule, "changed", G_CALLBACK (
		shell_content_dialog_rule_changed), dialog);

	shell_content_dialog_rule_changed (dialog, rule);

run:
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response != GTK_RESPONSE_OK)
		goto exit;

	if (!filter_rule_validate (rule))
		goto run;

	rule_context_add_rule (context, rule);
	rule_context_save (context, user_filename);

exit:
	g_object_unref (rule);
	gtk_widget_destroy (dialog);
}
