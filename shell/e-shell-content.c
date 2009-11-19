/*
 * e-shell-content.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-shell-content.h"

#include <glib/gi18n.h>

#include "e-util/e-binding.h"
#include "e-util/e-util.h"
#include "filter/e-rule-editor.h"
#include "widgets/misc/e-action-combo-box.h"
#include "widgets/misc/e-hinted-entry.h"

#include "e-shell-backend.h"
#include "e-shell-view.h"
#include "e-shell-window-actions.h"

#define E_SHELL_CONTENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_CONTENT, EShellContentPrivate))

#define STATE_KEY_SEARCH_FILTER		"SearchFilter"
#define STATE_KEY_SEARCH_SCOPE		"SearchScope"
#define STATE_KEY_SEARCH_TEXT		"SearchText"

struct _EShellContentPrivate {

	gpointer shell_view;  /* weak pointer */

	ERuleContext *search_context;
	EFilterRule *search_rule;
	gchar *system_filename;
	gchar *user_filename;

	GtkWidget *search_bar;

	/* Search bar children (not referenced) */
	GtkWidget *filter_label;
	GtkWidget *filter_combo_box;
	GtkWidget *search_label;
	GtkWidget *search_entry;
	GtkWidget *scope_label;
	GtkWidget *scope_combo_box;
	GtkRadioAction *search_radio; /* to be able to manage radio here */
};

enum {
	PROP_0,
	PROP_FILTER_ACTION,
	PROP_FILTER_VALUE,
	PROP_FILTER_VISIBLE,
	PROP_SEARCH_CONTEXT,
	PROP_SEARCH_HINT,
	PROP_SEARCH_RULE,
	PROP_SEARCH_TEXT,
	PROP_SEARCH_VISIBLE,
	PROP_SCOPE_ACTION,
	PROP_SCOPE_VALUE,
	PROP_SCOPE_VISIBLE,
	PROP_SEARCH_RADIO_ACTION,
	PROP_SHELL_VIEW
};

static gpointer parent_class;

static void
shell_content_dialog_rule_changed (GtkWidget *dialog,
                                   EFilterRule *rule)
{
	gboolean sensitive;

	sensitive = (rule != NULL) && (rule->parts != NULL);

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK, sensitive);
	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (dialog), GTK_RESPONSE_APPLY, sensitive);
}

static void
shell_content_update_search_widgets (EShellContent *shell_content)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkAction *action;
	GtkWidget *widget;
	const gchar *search_text;
	gboolean sensitive;

	g_return_if_fail (shell_content != NULL);

	shell_view = e_shell_content_get_shell_view (shell_content);
	g_return_if_fail (shell_view != NULL);

	/* EShellView subclasses are responsible for actually
	 * executing the search.  This is all cosmetic stuff. */

	widget = shell_content->priv->search_entry;
	shell_window = e_shell_view_get_shell_window (shell_view);
	search_text = e_shell_content_get_search_text (shell_content);

	sensitive =
		(search_text != NULL && *search_text != '\0') ||
		(e_shell_content_get_search_rule (shell_content) != NULL);

	if (sensitive) {
		GtkStyle *style;
		const GdkColor *color;

		style = gtk_widget_get_style (widget);
		color = &style->base[GTK_STATE_SELECTED];
		gtk_widget_modify_base (widget, GTK_STATE_NORMAL, color);

		style = gtk_widget_get_style (widget);
		color = &style->text[(search_text != NULL && *search_text != '\0') ? GTK_STATE_SELECTED : GTK_STATE_INSENSITIVE];
		gtk_widget_modify_text (widget, GTK_STATE_NORMAL, color);
	} else {
		/* Text color will be updated when we move the focus. */
		gtk_widget_modify_base (widget, GTK_STATE_NORMAL, NULL);
	}

	action = E_SHELL_WINDOW_ACTION_SEARCH_CLEAR (shell_window);
	gtk_action_set_sensitive (action, sensitive);

	action = E_SHELL_WINDOW_ACTION_SEARCH_SAVE (shell_window);
	gtk_action_set_sensitive (action, sensitive);
}

static void
shell_content_execute_search_cb (EShellView *shell_view,
                                 EShellContent *shell_content)
{
	GtkWidget *widget;

	shell_content_update_search_widgets (shell_content);

	if (!e_shell_view_is_active (shell_view))
		return;

	/* Direct the focus away from the search entry, so that a
	 * focus-in event is required before the text can be changed.
	 * This will reset the entry to the appropriate visual state. */
	widget = gtk_bin_get_child (GTK_BIN (shell_content));
	if (GTK_IS_PANED (widget))
		widget = gtk_paned_get_child1 (GTK_PANED (widget));
	gtk_widget_grab_focus (widget);
}

static void
shell_content_entry_activate_cb (EShellContent *shell_content,
                                 GtkEntry *entry)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkAction *action;

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	action = E_SHELL_WINDOW_ACTION_SEARCH_QUICK (shell_window);
	gtk_action_activate (action);
}

static void
shell_content_entry_changed_cb (EShellContent *shell_content,
                                GtkEntry *entry)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkAction *action;
	const gchar *text;
	gboolean sensitive;

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	text = gtk_entry_get_text (entry);
	sensitive = (text != NULL && *text != '\0');

	action = E_SHELL_WINDOW_ACTION_SEARCH_QUICK (shell_window);
	gtk_action_set_sensitive (action, sensitive);
}

static void
shell_content_entry_icon_press_cb (EShellContent *shell_content,
                                   GtkEntryIconPosition icon_pos,
                                   GdkEvent *event)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkAction *action;

	/* Show the search options menu when the icon is pressed. */

	if (icon_pos != GTK_ENTRY_ICON_PRIMARY)
		return;

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	action = E_SHELL_WINDOW_ACTION_SEARCH_OPTIONS (shell_window);
	gtk_action_activate (action);
}

static void
shell_content_entry_icon_release_cb (EShellContent *shell_content,
                                     GtkEntryIconPosition icon_pos,
                                     GdkEvent *event)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkAction *action;

	/* Clear the search when the icon is pressed and released. */

	if (icon_pos != GTK_ENTRY_ICON_SECONDARY)
		return;

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	action = E_SHELL_WINDOW_ACTION_SEARCH_CLEAR (shell_window);
	gtk_action_activate (action);
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
	EShellContentClass *shell_content_class;
	EShellView *shell_view;
	EShellViewClass *shell_view_class;
	EShellBackend *shell_backend;
	ERuleContext *context;
	EFilterRule *rule;
	EFilterPart *part;
	gchar *system_filename;
	gchar *user_filename;

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	g_return_if_fail (shell_view_class->search_rules != NULL);

	shell_content_class = E_SHELL_CONTENT_GET_CLASS (shell_content);
	g_return_if_fail (shell_content_class->new_search_context != NULL);

	/* The basename for built-in searches is specified in the
	 * shell view class.  All built-in search rules live in the
	 * same directory. */
	system_filename = g_build_filename (
		EVOLUTION_RULEDIR, shell_view_class->search_rules, NULL);

	/* The filename for custom saved searches is always of
	 * the form "$(shell_backend_data_dir)/searches.xml". */
	user_filename = g_build_filename (
		e_shell_backend_get_data_dir (shell_backend),
		"searches.xml", NULL);

	context = shell_content_class->new_search_context ();
	e_rule_context_add_part_set (
		context, "partset", E_TYPE_FILTER_PART,
		e_rule_context_add_part, e_rule_context_next_part);
	e_rule_context_add_rule_set (
		context, "ruleset", E_TYPE_FILTER_RULE,
		e_rule_context_add_rule, e_rule_context_next_rule);
	e_rule_context_load (context, system_filename, user_filename);

	/* XXX Not sure why this is necessary. */
	g_object_set_data_full (
		G_OBJECT (context), "system",
		g_strdup (system_filename), g_free);
	g_object_set_data_full (
		G_OBJECT (context), "user",
		g_strdup (user_filename), g_free);

	rule = e_filter_rule_new ();
	part = e_rule_context_next_part (context, NULL);
	if (part == NULL)
		g_warning (
			"Could not load %s search: no parts",
			e_shell_view_get_name (shell_view));
	else
		e_filter_rule_add_part (rule, e_filter_part_clone (part));

	shell_content->priv->search_context = context;
	shell_content->priv->system_filename = system_filename;
	shell_content->priv->user_filename = user_filename;
}

static void
shell_content_activate_advanced_search (EShellContent *shell_content)
{
	GtkRadioAction *radio_action;
	const gchar *search_text;

	g_return_if_fail (shell_content != NULL);
	g_return_if_fail (shell_content->priv->search_entry != NULL);

	/* cannot mix text search with an Advanced Search, thus unsetting search text */
	search_text = e_shell_content_get_search_text (shell_content);
	if (search_text)
		e_shell_content_set_search_text (shell_content, NULL);

	radio_action = e_shell_content_get_search_radio_action (shell_content);
	if (radio_action)
		g_object_set (G_OBJECT (radio_action), "current-value", -1, NULL);
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

		case PROP_SEARCH_HINT:
			e_shell_content_set_search_hint (
				E_SHELL_CONTENT (object),
				g_value_get_string (value));
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

		case PROP_SEARCH_RADIO_ACTION:
			e_shell_content_set_search_radio_action (
				E_SHELL_CONTENT (object),
				g_value_get_object (value));
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

		case PROP_SEARCH_HINT:
			g_value_set_string (
				value, e_shell_content_get_search_hint (
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

		case PROP_SEARCH_RADIO_ACTION:
			g_value_set_object (
				value, e_shell_content_get_search_radio_action (
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

	if (priv->search_context != NULL) {
		g_object_unref (priv->search_context);
		priv->search_context = NULL;
	}

	if (priv->search_radio) {
		/* actions are freed before contents, thus skip it here */
		/*g_signal_handlers_disconnect_matched (
			priv->search_radio, G_SIGNAL_MATCH_DATA, 0, 0, NULL,
			NULL, object);*/
		priv->search_radio = NULL;
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
	GtkSizeGroup *size_group;
	GtkAction *action;
	GtkWidget *widget;

	shell_content = E_SHELL_CONTENT (object);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);
	size_group = e_shell_view_get_size_group (shell_view);

	g_signal_connect_after (
		shell_view, "execute-search",
		G_CALLBACK (shell_content_execute_search_cb),
		shell_content);

	widget = shell_content->priv->search_entry;

	action = E_SHELL_WINDOW_ACTION_SEARCH_CLEAR (shell_window);
	e_binding_new (
		action, "sensitive",
		widget, "secondary-icon-sensitive");
	e_binding_new (
		action, "stock-id",
		widget, "secondary-icon-stock");
	e_binding_new (
		action, "tooltip",
		widget, "secondary-icon-tooltip-text");

	action = E_SHELL_WINDOW_ACTION_SEARCH_OPTIONS (shell_window);
	e_binding_new (
		action, "sensitive",
		widget, "primary-icon-sensitive");
	e_binding_new (
		action, "stock-id",
		widget, "primary-icon-stock");
	e_binding_new (
		action, "tooltip",
		widget, "primary-icon-tooltip-text");

	widget = shell_content->priv->search_bar;
	gtk_size_group_add_widget (size_group, widget);

	shell_content_init_search_context (shell_content);
}

static void
shell_content_destroy (GtkObject *gtk_object)
{
	EShellContentPrivate *priv;

	priv = E_SHELL_CONTENT_GET_PRIVATE (gtk_object);

	/* Unparent the widget before destroying it to avoid
	 * writing a custom GtkContainer::remove() method. */

	if (priv->search_bar != NULL) {
		gtk_widget_unparent (priv->search_bar);
		gtk_widget_destroy (priv->search_bar);
		g_object_unref (priv->search_bar);
		priv->search_bar = NULL;
	}

	/* Chain up to parent's destroy() method. */
	GTK_OBJECT_CLASS (parent_class)->destroy (gtk_object);
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
	GtkObjectClass *gtk_object_class;
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

	gtk_object_class = GTK_OBJECT_CLASS (class);
	gtk_object_class->destroy = shell_content_destroy;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->size_request = shell_content_size_request;
	widget_class->size_allocate = shell_content_size_allocate;

	container_class = GTK_CONTAINER_CLASS (class);
	container_class->forall = shell_content_forall;

	class->new_search_context = e_rule_context_new;

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
			E_TYPE_RULE_CONTEXT,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_SEARCH_HINT,
		g_param_spec_string (
			"search-hint",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SEARCH_RULE,
		g_param_spec_object (
			"search-rule",
			NULL,
			NULL,
			E_TYPE_FILTER_RULE,
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
		PROP_SEARCH_RADIO_ACTION,
		g_param_spec_object (
			"search-radio-action",
			NULL,
			NULL,
			GTK_TYPE_RADIO_ACTION,
			G_PARAM_READWRITE));

	/**
	 * EShellContent:shell-view
	 *
	 * The #EShellView to which the content widget belongs.
	 **/
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
	GtkWidget *widget;

	shell_content->priv = E_SHELL_CONTENT_GET_PRIVATE (shell_content);

	GTK_WIDGET_SET_FLAGS (shell_content, GTK_NO_WINDOW);

	/*** Build the Search Bar ***/

	widget = gtk_hbox_new (FALSE, 24);
	gtk_widget_set_parent (widget, GTK_WIDGET (shell_content));
	shell_content->priv->search_bar = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Filter Combo Widgets */

	box = GTK_BOX (shell_content->priv->search_bar);

	widget = gtk_hbox_new (FALSE, 3);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	box = GTK_BOX (widget);

	/* Translators: The "Show:" label precedes a combo box that
	 * allows the user to filter the current view.  Examples of
	 * items that appear in the combo box are "Unread Messages",
	 * "Important Messages", or "Active Appointments". */
	widget = gtk_label_new_with_mnemonic (_("Sho_w:"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	shell_content->priv->filter_label = widget;
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = e_action_combo_box_new ();
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	shell_content->priv->filter_combo_box = widget;
	gtk_widget_show (widget);

	/* Search Entry Widgets */

	box = GTK_BOX (shell_content->priv->search_bar);

	widget = gtk_hbox_new (FALSE, 3);
	gtk_box_pack_start (box, widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	box = GTK_BOX (widget);

	/* Translators: This is part of the quick search interface.
	 * example: Search: [_______________] in [ Current Folder ] */
	widget = gtk_label_new_with_mnemonic (_("Sear_ch:"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	shell_content->priv->search_label = widget;
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = e_hinted_entry_new ();
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_box_pack_start (box, widget, TRUE, TRUE, 0);
	shell_content->priv->search_entry = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "activate",
		G_CALLBACK (shell_content_entry_activate_cb),
		shell_content);

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (shell_content_entry_changed_cb),
		shell_content);

	g_signal_connect_swapped (
		widget, "icon-press",
		G_CALLBACK (shell_content_entry_icon_press_cb),
		shell_content);

	g_signal_connect_swapped (
		widget, "icon-release",
		G_CALLBACK (shell_content_entry_icon_release_cb),
		shell_content);

	g_signal_connect_swapped (
		widget, "key-press-event",
		G_CALLBACK (shell_content_entry_key_press_cb),
		shell_content);

	/* Scope Combo Widgets */

	/* Translators: This is part of the quick search interface.
	 * example: Search: [_______________] in [ Current Folder ] */
	widget = gtk_label_new_with_mnemonic (_("i_n"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	shell_content->priv->scope_label = widget;
	gtk_widget_show (widget);

	label = GTK_LABEL (widget);

	widget = e_action_combo_box_new ();
	gtk_label_set_mnemonic_widget (label, widget);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	shell_content->priv->scope_combo_box = widget;
	gtk_widget_show (widget);
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

/**
 * e_shell_content_new:
 * @shell_view: an #EShellView
 *
 * Creates a new #EShellContent instance belonging to @shell_view.
 *
 * Returns: a new #EShellContent instance
 **/
GtkWidget *
e_shell_content_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_SHELL_CONTENT, "shell-view", shell_view, NULL);
}

/**
 * e_shell_content_check_state:
 * @shell_content: an #EShellContent
 *
 * #EShellContent subclasses should implement the
 * <structfield>check_state</structfield> method in #EShellContentClass
 * to return a set of flags describing the current content selection.
 * Subclasses are responsible for defining their own flags.  This is
 * primarily used to assist shell views with updating actions (see
 * e_shell_view_update_actions()).
 *
 * Returns: a set of flags describing the current @shell_content selection
 **/
guint32
e_shell_content_check_state (EShellContent *shell_content)
{
	EShellContentClass *shell_content_class;

	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), 0);

	shell_content_class = E_SHELL_CONTENT_GET_CLASS (shell_content);
	g_return_val_if_fail (shell_content_class->check_state != NULL, 0);

	return shell_content_class->check_state (shell_content);
}

/**
 * e_shell_content_get_shell_view:
 * @shell_content: an #EShellContent
 *
 * Returns the #EShellView that was passed to e_shell_content_new().
 *
 * Returns: the #EShellView to which @shell_content belongs
 **/
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

void
e_shell_content_add_filter_separator_before (EShellContent *shell_content,
                                             gint action_value)
{
	EActionComboBox *combo_box;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	combo_box = E_ACTION_COMBO_BOX (shell_content->priv->filter_combo_box);

	e_action_combo_box_add_separator_before (combo_box, action_value);
}

void
e_shell_content_add_filter_separator_after (EShellContent *shell_content,
                                            gint action_value)
{
	EActionComboBox *combo_box;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	combo_box = E_ACTION_COMBO_BOX (shell_content->priv->filter_combo_box);

	e_action_combo_box_add_separator_after (combo_box, action_value);
}

ERuleContext *
e_shell_content_get_search_context (EShellContent *shell_content)
{
	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), NULL);

	return shell_content->priv->search_context;
}

const gchar *
e_shell_content_get_search_hint (EShellContent *shell_content)
{
	EHintedEntry *entry;

	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), NULL);

	entry = E_HINTED_ENTRY (shell_content->priv->search_entry);

	return e_hinted_entry_get_hint (entry);
}

void
e_shell_content_set_search_hint (EShellContent *shell_content,
                                 const gchar *search_hint)
{
	EHintedEntry *entry;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	entry = E_HINTED_ENTRY (shell_content->priv->search_entry);

	e_hinted_entry_set_hint (entry, search_hint);

	g_object_notify (G_OBJECT (shell_content), "search-hint");
}

EFilterRule *
e_shell_content_get_search_rule (EShellContent *shell_content)
{
	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), NULL);

	return shell_content->priv->search_rule;
}

void
e_shell_content_set_search_rule (EShellContent *shell_content,
                                 EFilterRule *search_rule)
{
	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	if (search_rule != NULL) {
		g_return_if_fail (E_IS_FILTER_RULE (search_rule));
		g_object_ref (search_rule);
	}

	if (shell_content->priv->search_rule != NULL)
		g_object_unref (shell_content->priv->search_rule);

	shell_content->priv->search_rule = search_rule;

	shell_content_update_search_widgets (shell_content);
	g_object_notify (G_OBJECT (shell_content), "search-rule");
}

/* free returned string with g_free */
gchar *
e_shell_content_get_search_rule_as_string (EShellContent *shell_content)
{
	EFilterRule *rule;
	GString *str;

	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), NULL);

	rule = e_shell_content_get_search_rule (shell_content);

	if (!rule)
		return NULL;

	str = g_string_new ("");
	e_filter_rule_build_code (rule, str);

	return g_string_free (str, FALSE);
}

const gchar *
e_shell_content_get_search_text (EShellContent *shell_content)
{
	EHintedEntry *entry;

	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), NULL);

	entry = E_HINTED_ENTRY (shell_content->priv->search_entry);

	return e_hinted_entry_get_text (entry);
}

void
e_shell_content_set_search_text (EShellContent *shell_content,
                                 const gchar *search_text)
{
	EHintedEntry *entry;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	entry = E_HINTED_ENTRY (shell_content->priv->search_entry);

	e_hinted_entry_set_text (entry, search_text);

	shell_content_update_search_widgets (shell_content);
	g_object_notify (G_OBJECT (shell_content), "search-text");
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

static void
search_radio_changed_cb (GtkRadioAction *action,
                       GtkRadioAction *current,
                       EShellContent *shell_content)
{
	gint current_value;
	gchar *search_text;

	e_shell_content_set_search_hint (shell_content, gtk_action_get_label (GTK_ACTION (current)));

	current_value = gtk_radio_action_get_current_value (current);
	search_text = g_strdup (e_shell_content_get_search_text (shell_content));

	if (current_value != -1) {
		e_shell_content_set_search_rule (shell_content, NULL);
		e_shell_content_set_search_text (shell_content, search_text);
		if (search_text && *search_text)
			e_shell_view_execute_search (e_shell_content_get_shell_view (shell_content));
	} else if (search_text) {
		e_shell_content_set_search_text (shell_content, NULL);
	}

	g_free (search_text);
}

void
e_shell_content_set_search_radio_action (EShellContent *shell_content, GtkRadioAction *search_action)
{
	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	if (shell_content->priv->search_radio)
		g_signal_handlers_disconnect_matched (
			shell_content->priv->search_radio, G_SIGNAL_MATCH_DATA, 0, 0, NULL,
			search_radio_changed_cb, shell_content);

	shell_content->priv->search_radio = search_action;

	if (shell_content->priv->search_radio)
		g_signal_connect (shell_content->priv->search_radio, "changed", G_CALLBACK (search_radio_changed_cb), shell_content);

	g_object_notify (G_OBJECT (shell_content), "search-radio-action");
}

GtkRadioAction *
e_shell_content_get_search_radio_action (EShellContent *shell_content)
{
	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), NULL);

	return shell_content->priv->search_radio;
}

void
e_shell_content_run_advanced_search_dialog (EShellContent *shell_content)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkWidget *dialog;
	GtkWidget *widget;
	EFilterRule *rule;
	ERuleContext *context;
	const gchar *user_filename;
	gint response;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);
	user_filename = shell_content->priv->user_filename;

	rule = e_shell_content_get_search_rule (shell_content);

	if (rule == NULL)
		rule = e_filter_rule_new ();
	else
		rule = e_filter_rule_clone (rule);

	context = e_shell_content_get_search_context (shell_content);
	widget = e_filter_rule_get_widget (rule, context);
	e_filter_rule_set_source (rule, E_FILTER_SOURCE_INCOMING);

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

	if (!e_filter_rule_validate (rule, GTK_WINDOW (dialog)))
		goto run;

	e_shell_content_set_search_rule (shell_content, rule);

	shell_content_activate_advanced_search (shell_content);
	e_shell_view_execute_search (shell_view);

	if (response == GTK_RESPONSE_APPLY) {
		if (!e_rule_context_find_rule (context, rule->name, rule->source))
			e_rule_context_add_rule (context, rule);
		e_rule_context_save (context, user_filename);
		goto run;
	}

exit:
	g_object_unref (rule);
	gtk_widget_destroy (dialog);
}

void
e_shell_content_run_edit_searches_dialog (EShellContent *shell_content)
{
	ERuleContext *context;
	ERuleEditor *editor;
	const gchar *user_filename;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	context = e_shell_content_get_search_context (shell_content);
	user_filename = shell_content->priv->user_filename;

	editor = e_rule_editor_new (
		context, E_FILTER_SOURCE_INCOMING, _("Searches"));
	gtk_window_set_title (GTK_WINDOW (editor), _("Searches"));

	if (gtk_dialog_run (GTK_DIALOG (editor)) == GTK_RESPONSE_OK)
		e_rule_context_save (context, user_filename);

	gtk_widget_destroy (GTK_WIDGET (editor));
}

void
e_shell_content_run_save_search_dialog (EShellContent *shell_content)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkWidget *dialog;
	GtkWidget *widget;
	EFilterRule *rule;
	ERuleContext *context;
	const gchar *search_text;
	const gchar *user_filename;
	gchar *search_name;
	gint response;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);
	user_filename = shell_content->priv->user_filename;

	rule = e_shell_content_get_search_rule (shell_content);
	g_return_if_fail (E_IS_FILTER_RULE (rule));
	rule = e_filter_rule_clone (rule);

	search_text = e_shell_content_get_search_text (shell_content);
	if (search_text == NULL || *search_text == '\0')
		search_text = "''";

	search_name = g_strdup_printf ("%s %s", rule->name, search_text);
	e_filter_rule_set_name (rule, search_name);
	g_free (search_name);

	context = e_shell_content_get_search_context (shell_content);
	widget = e_filter_rule_get_widget (rule, context);
	e_filter_rule_set_source (rule, E_FILTER_SOURCE_INCOMING);

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

	if (!e_filter_rule_validate (rule, GTK_WINDOW (dialog)))
		goto run;

	e_rule_context_add_rule (context, rule);
	e_rule_context_save (context, user_filename);

exit:
	g_object_unref (rule);
	gtk_widget_destroy (dialog);
}

void
e_shell_content_restore_state (EShellContent *shell_content,
                               const gchar *group_name)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GKeyFile *key_file;
	GtkAction *action;
	GtkWidget *widget;
	const gchar *key;
	gchar *string;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));
	g_return_if_fail (group_name != NULL);

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);
	key_file = e_shell_view_get_state_key_file (shell_view);

	/* Changing the combo boxes triggers searches, so block
	 * the search action until the state is fully restored. */
	action = E_SHELL_WINDOW_ACTION_SEARCH_QUICK (shell_window);
	gtk_action_block_activate (action);

	key = STATE_KEY_SEARCH_FILTER;
	string = g_key_file_get_string (key_file, group_name, key, NULL);
	if (string != NULL && *string != '\0')
		action = e_shell_window_get_action (shell_window, string);
	else
		action = NULL;
	if (action != NULL)
		gtk_action_activate (action);
	else {
		/* Pick the first combo box item. */
		widget = shell_content->priv->filter_combo_box;
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	}
	g_free (string);

	key = STATE_KEY_SEARCH_SCOPE;
	string = g_key_file_get_string (key_file, group_name, key, NULL);
	if (string != NULL && *string != '\0')
		action = e_shell_window_get_action (shell_window, string);
	else
		action = NULL;
	if (action != NULL)
		gtk_action_activate (action);
	else {
		/* Pick the first combo box item. */
		widget = shell_content->priv->scope_combo_box;
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	}
	g_free (string);

	key = STATE_KEY_SEARCH_TEXT;
	string = g_key_file_get_string (key_file, group_name, key, NULL);
	if (g_strcmp0 (string ? string : "", e_shell_content_get_search_text (shell_content)) != 0)
		e_shell_content_set_search_text (shell_content, string);
	g_free (string);

	action = E_SHELL_WINDOW_ACTION_SEARCH_QUICK (shell_window);
	gtk_action_unblock_activate (action);

	/* Now execute the search. */
	e_shell_view_execute_search (shell_view);
}
