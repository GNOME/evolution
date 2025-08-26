/*
 * e-shell-content.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/**
 * SECTION: e-shell-content
 * @short_description: the right side of the main window
 * @include: shell/e-shell-content.h
 **/

#include "evolution-config.h"

#include "e-shell-content.h"

#include <glib/gi18n.h>
#include <libebackend/libebackend.h>

#include "e-shell-backend.h"
#include "e-shell-searchbar.h"
#include "e-shell-view.h"
#include "e-shell-window-actions.h"

struct _EShellContentPrivate {

	gpointer shell_view;	/* weak pointer */

	GtkWidget *alert_bar;
	GtkWidget *searchbar;	/* not referenced */

	/* Custom search rules. */
	gchar *user_filename;
};

enum {
	PROP_0,
	PROP_ALERT_BAR,
	PROP_SHELL_VIEW
};

/* Forward Declarations */
static void	e_shell_content_alert_sink_init
					(EAlertSinkInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EShellContent, e_shell_content, GTK_TYPE_BIN,
	G_ADD_PRIVATE (EShellContent)
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, e_shell_content_alert_sink_init)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL));

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
		case PROP_ALERT_BAR:
			g_value_set_object (
				value, e_shell_content_get_alert_bar (
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
	EShellContent *self = E_SHELL_CONTENT (object);

	if (self->priv->shell_view != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (self->priv->shell_view), &self->priv->shell_view);
		self->priv->shell_view = NULL;
	}

	if (self->priv->alert_bar) {
		gtk_widget_unparent (self->priv->alert_bar);
		g_clear_object (&self->priv->alert_bar);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_shell_content_parent_class)->dispose (object);
}

static void
shell_content_finalize (GObject *object)
{
	EShellContent *self = E_SHELL_CONTENT (object);

	g_free (self->priv->user_filename);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_shell_content_parent_class)->finalize (object);
}

static void
shell_content_constructed (GObject *object)
{
	EShellContent *shell_content;
	EShellBackend *shell_backend;
	EShellView *shell_view;
	GtkWidget *widget;
	const gchar *config_dir;

	shell_content = E_SHELL_CONTENT (object);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	widget = e_alert_bar_new ();
	gtk_widget_set_parent (widget, GTK_WIDGET (shell_content));
	shell_content->priv->alert_bar = g_object_ref_sink (widget);
	/* EAlertBar controls its own visibility. */

	/* XXX Regenerate the filename for custom saved search as done
	 *     in shell_view_init_search_context().  ERuleContext ought
	 *     to remember the filename when loading rules so you don't
	 *     have to keep passing it in when saving rules. */
	config_dir = e_shell_backend_get_config_dir (shell_backend);
	shell_content->priv->user_filename =
		g_build_filename (config_dir, "searches.xml", NULL);

	e_extensible_load_extensions (E_EXTENSIBLE (object));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_shell_content_parent_class)->constructed (object);
}

static void
shell_content_get_preferred_width (GtkWidget *widget,
                                   gint *minimum,
                                   gint *natural)
{
	EShellContent *self = E_SHELL_CONTENT (widget);
	GtkWidget *child;

	*minimum = *natural = 0;

	child = gtk_bin_get_child (GTK_BIN (widget));
	gtk_widget_get_preferred_width (child, minimum, natural);

	if (gtk_widget_get_visible (self->priv->alert_bar)) {
		gint child_minimum;
		gint child_natural;

		gtk_widget_get_preferred_width (self->priv->alert_bar, &child_minimum, &child_natural);

		*minimum = MAX (*minimum, child_minimum);
		*natural = MAX (*natural, child_natural);
	}

	if (self->priv->searchbar != NULL) {
		gint child_minimum;
		gint child_natural;

		gtk_widget_get_preferred_width (self->priv->searchbar, &child_minimum, &child_natural);

		*minimum = MAX (*minimum, child_minimum);
		*natural = MAX (*natural, child_natural);
	}
}

static void
shell_content_get_preferred_height (GtkWidget *widget,
                                    gint *minimum,
                                    gint *natural)
{
	EShellContent *self = E_SHELL_CONTENT (widget);
	GtkWidget *child;

	child = gtk_bin_get_child (GTK_BIN (widget));
	gtk_widget_get_preferred_height (child, minimum, natural);

	if (gtk_widget_get_visible (self->priv->alert_bar)) {
		gint child_minimum;
		gint child_natural;

		gtk_widget_get_preferred_height (self->priv->alert_bar, &child_minimum, &child_natural);

		*minimum += child_minimum;
		*natural += child_natural;
	}

	if (self->priv->searchbar != NULL) {
		gint child_minimum;
		gint child_natural;

		gtk_widget_get_preferred_height (self->priv->searchbar, &child_minimum, &child_natural);

		*minimum += child_minimum;
		*natural += child_natural;
	}
}

static void
shell_content_size_allocate (GtkWidget *widget,
                             GtkAllocation *allocation)
{
	EShellContent *self = E_SHELL_CONTENT (widget);
	GtkAllocation child_allocation;
	GtkRequisition child_requisition;
	GtkWidget *child;
	gint remaining_height;

	remaining_height = allocation->height;
	gtk_widget_set_allocation (widget, allocation);

	child_allocation.x = allocation->x;
	child_allocation.y = allocation->y;
	child_allocation.width = allocation->width;

	child_requisition.height = 0;

	/* Alert bar gets to be as tall as it wants (if visible). */

	child = self->priv->alert_bar;
	child_allocation.y += child_requisition.height;

	if (gtk_widget_get_visible (child))
		gtk_widget_get_preferred_height_for_width (
			child, child_allocation.width,
			&child_requisition.height, NULL);
	else
		child_requisition.height = 0;

	remaining_height -= child_requisition.height;
	child_allocation.height = child_requisition.height;

	if (child_allocation.height > 0)
		gtk_widget_size_allocate (child, &child_allocation);

	/* Search bar gets to be as tall as it wants (if we have one). */

	child = self->priv->searchbar;
	child_allocation.y += child_requisition.height;

	if (child != NULL)
		gtk_widget_get_preferred_size (child, &child_requisition, NULL);
	else
		child_requisition.height = 0;

	remaining_height -= child_requisition.height;
	child_allocation.height = child_requisition.height;

	if (child != NULL)
		gtk_widget_size_allocate (child, &child_allocation);

	/* The GtkBin child gets whatever vertical space is left. */

	child_allocation.y += child_requisition.height;
	child_allocation.height = remaining_height;

	child = gtk_bin_get_child (GTK_BIN (widget));
	if (child != NULL)
		gtk_widget_size_allocate (child, &child_allocation);
}

static void
shell_content_remove (GtkContainer *container,
                      GtkWidget *widget)
{
	EShellContent *self = E_SHELL_CONTENT (container);

	if (widget == self->priv->alert_bar) {
		gtk_widget_unparent (self->priv->alert_bar);
		g_clear_object (&self->priv->alert_bar);
		return;
	}

	if (widget == self->priv->searchbar) {
		gtk_widget_unparent (self->priv->searchbar);
		self->priv->searchbar = NULL;
		return;
	}

	/* Chain up to parent's remove() method. */
	GTK_CONTAINER_CLASS (e_shell_content_parent_class)->remove (container, widget);
}

static void
shell_content_forall (GtkContainer *container,
                      gboolean include_internals,
                      GtkCallback callback,
                      gpointer callback_data)
{
	EShellContent *self = E_SHELL_CONTENT (container);

	if (self->priv->alert_bar != NULL)
		callback (self->priv->alert_bar, callback_data);

	if (self->priv->searchbar != NULL)
		callback (self->priv->searchbar, callback_data);

	/* Chain up to parent's forall() method. */
	GTK_CONTAINER_CLASS (e_shell_content_parent_class)->forall (container, include_internals, callback, callback_data);
}

static void
shell_content_submit_alert (EAlertSink *alert_sink,
                            EAlert *alert)
{
	GtkWidget *alert_bar;

	alert_bar = e_shell_content_get_alert_bar (E_SHELL_CONTENT (alert_sink));

	e_alert_bar_submit_alert (E_ALERT_BAR (alert_bar), alert);
}

static void
e_shell_content_class_init (EShellContentClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_content_set_property;
	object_class->get_property = shell_content_get_property;
	object_class->dispose = shell_content_dispose;
	object_class->finalize = shell_content_finalize;
	object_class->constructed = shell_content_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->get_preferred_width = shell_content_get_preferred_width;
	widget_class->get_preferred_height = shell_content_get_preferred_height;
	widget_class->size_allocate = shell_content_size_allocate;

	container_class = GTK_CONTAINER_CLASS (class);
	container_class->remove = shell_content_remove;
	container_class->forall = shell_content_forall;

	/**
	 * EShellContent:alert-bar
	 *
	 * Displays informational and error messages.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_ALERT_BAR,
		g_param_spec_object (
			"alert-bar",
			"Alert Bar",
			"Displays informational and error messages",
			E_TYPE_ALERT_BAR,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

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
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_shell_content_alert_sink_init (EAlertSinkInterface *iface)
{
	iface->submit_alert = shell_content_submit_alert;
}

static void
e_shell_content_init (EShellContent *shell_content)
{
	shell_content->priv = e_shell_content_get_instance_private (shell_content);

	gtk_widget_set_has_window (GTK_WIDGET (shell_content), FALSE);
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
 * e_shell_content_set_searchbar:
 * @shell_content: an #EShellContent
 * @searchbar: a #GtkWidget, or %NULL
 *
 * Packs @searchbar at the top of @shell_content.
 **/
void
e_shell_content_set_searchbar (EShellContent *shell_content,
                               GtkWidget *searchbar)
{
	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	if (searchbar != NULL)
		g_return_if_fail (GTK_IS_WIDGET (searchbar));

	if (shell_content->priv->searchbar != NULL)
		gtk_container_remove (
			GTK_CONTAINER (shell_content),
			shell_content->priv->searchbar);

	shell_content->priv->searchbar = searchbar;

	if (searchbar != NULL)
		gtk_widget_set_parent (searchbar, GTK_WIDGET (shell_content));

	gtk_widget_queue_resize (GTK_WIDGET (shell_content));
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
	g_return_val_if_fail (shell_content_class != NULL, 0);
	g_return_val_if_fail (shell_content_class->check_state != NULL, 0);

	return shell_content_class->check_state (shell_content);
}

/**
 * e_shell_content_focus_search_results:
 * @shell_content: an #EShellContent
 *
 * #EShellContent subclasses should implement the
 * <structfield>focus_search_results</structfield> method in
 * #EShellContentClass to direct input focus to the widget
 * displaying search results.  This is usually called during
 * e_shell_view_execute_search().
 **/
void
e_shell_content_focus_search_results (EShellContent *shell_content)
{
	EShellContentClass *shell_content_class;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	shell_content_class = E_SHELL_CONTENT_GET_CLASS (shell_content);
	g_return_if_fail (shell_content_class != NULL);

	if (shell_content_class->focus_search_results != NULL)
		shell_content_class->focus_search_results (shell_content);
}

/**
 * e_shell_content_get_alert_bar:
 * @shell_content: an #EShellContent
 *
 * Returns the #EAlertBar used to display informational and error messages.
 *
 * Returns: the #EAlertBar for @shell_content
 **/
GtkWidget *
e_shell_content_get_alert_bar (EShellContent *shell_content)
{
	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), NULL);

	return shell_content->priv->alert_bar;
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

void
e_shell_content_run_advanced_search_dialog (EShellContent *shell_content)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkWidget *content_area;
	GtkWidget *dialog;
	GtkWidget *widget;
	EFilterRule *rule;
	ERuleContext *context;
	const gchar *user_filename;
	gulong handler_id;
	gint response;
	EAlert *alert = NULL;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);
	user_filename = shell_content->priv->user_filename;

	rule = e_shell_view_get_search_rule (shell_view);

	if (rule == NULL)
		rule = e_filter_rule_new ();
	else
		rule = e_filter_rule_clone (rule);

	context = E_SHELL_VIEW_GET_CLASS (shell_view)->search_context;
	widget = e_filter_rule_get_widget (rule, context);
	e_filter_rule_set_source (rule, E_FILTER_SOURCE_INCOMING);

	dialog = gtk_dialog_new_with_buttons (
		_("Advanced Search"), GTK_WINDOW (shell_window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Save"), GTK_RESPONSE_APPLY,
		_("_OK"), GTK_RESPONSE_OK, NULL);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 7);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 3);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 600, 300);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_start (GTK_BOX (content_area), widget, TRUE, TRUE, 0);

	handler_id = g_signal_connect_swapped (
		rule, "changed", G_CALLBACK (
		shell_content_dialog_rule_changed), dialog);

	shell_content_dialog_rule_changed (dialog, rule);

run:
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response != GTK_RESPONSE_OK && response != GTK_RESPONSE_APPLY)
		goto exit;

	if (!e_filter_rule_validate (rule, &alert)) {
		e_alert_run_dialog (GTK_WINDOW (dialog), alert);
		g_object_unref (alert);
		alert = NULL;
		goto run;
	}

	e_shell_view_custom_search (shell_view, rule);

	if (response == GTK_RESPONSE_APPLY) {
		if (!e_rule_context_find_rule (context, rule->name, rule->source))
			e_rule_context_add_rule (context, g_object_ref (rule));
		e_rule_context_save (context, user_filename);
		goto run;
	}

exit:
	g_signal_handler_disconnect (rule, handler_id);

	g_object_unref (rule);
	gtk_widget_destroy (dialog);
}

void
e_shell_content_run_edit_searches_dialog (EShellContent *shell_content)
{
	EShellView *shell_view;
	ERuleContext *context;
	ERuleEditor *editor;
	const gchar *user_filename;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	shell_view = e_shell_content_get_shell_view (shell_content);
	context = E_SHELL_VIEW_GET_CLASS (shell_view)->search_context;
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
	GtkWidget *content_area;
	GtkWidget *dialog;
	GtkWidget *widget;
	EFilterRule *rule;
	ERuleContext *context;
	const gchar *user_filename;
	gchar *search_name;
	gulong handler_id;
	gint response;
	EAlert *alert = NULL;

	g_return_if_fail (E_IS_SHELL_CONTENT (shell_content));

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);
	user_filename = shell_content->priv->user_filename;

	rule = e_shell_view_get_search_rule (shell_view);
	g_return_if_fail (E_IS_FILTER_RULE (rule));
	rule = e_filter_rule_clone (rule);

	search_name = e_shell_view_get_search_name (shell_view);
	e_filter_rule_set_name (rule, search_name);
	g_free (search_name);

	context = E_SHELL_VIEW_GET_CLASS (shell_view)->search_context;
	widget = e_filter_rule_get_widget (rule, context);
	e_filter_rule_set_source (rule, E_FILTER_SOURCE_INCOMING);

	dialog = gtk_dialog_new_with_buttons (
		_("Save Search"), GTK_WINDOW (shell_window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK, NULL);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 7);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 3);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 300);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_start (GTK_BOX (content_area), widget, TRUE, TRUE, 0);

	handler_id = g_signal_connect_swapped (
		rule, "changed", G_CALLBACK (
		shell_content_dialog_rule_changed), dialog);

	shell_content_dialog_rule_changed (dialog, rule);

run:
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response != GTK_RESPONSE_OK)
		goto exit;

	if (!e_filter_rule_validate (rule, &alert)) {
		e_alert_run_dialog (GTK_WINDOW (dialog), alert);
		g_object_unref (alert);
		alert = NULL;
		goto run;
	}

	/* XXX This function steals the rule reference, so
	 *     counteract that by referencing it again. */
	e_rule_context_add_rule (context, g_object_ref (rule));

	e_rule_context_save (context, user_filename);

exit:
	g_signal_handler_disconnect (rule, handler_id);

	g_object_unref (rule);
	gtk_widget_destroy (dialog);
}
