/*
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
 */

/* A plugin manager ui */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <glib/gi18n-lib.h>
#include <libebackend/libebackend.h>

#include <shell/e-shell-window.h>
#include <shell/e-shell-window-actions.h>

/* Standard GObject macros */
#define E_TYPE_PLUGIN_MANAGER \
	(e_plugin_manager_get_type ())
#define E_PLUGIN_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PLUGIN_MANAGER, EPluginManager))

typedef struct _EPluginManager EPluginManager;
typedef struct _EPluginManagerClass EPluginManagerClass;

struct _EPluginManager {
	EExtension parent;
};

struct _EPluginManagerClass {
	EExtensionClass parent_class;
};

enum {
	LABEL_NAME,
	LABEL_AUTHOR,
	LABEL_DESCRIPTION,
	LABEL_LAST
};

enum
{
	COL_PLUGIN_ENABLED = 0,
	COL_PLUGIN_NAME,
	COL_PLUGIN_DATA,
	COL_PLUGIN_CFG_WIDGET
};

static struct {
	const gchar *label;
} label_info[LABEL_LAST] = {
	{ N_("Name"), },
	{ N_("Author(s)"), },
	{ N_("Description"), },
};

typedef struct _Manager Manager;
struct _Manager {
	GtkLabel *labels[LABEL_LAST];
	GtkLabel *items[LABEL_LAST];

	GtkWidget *config_plugin_label;
	GtkWidget *active_cfg_widget;
};

/* for tracking if we're shown */
static GtkWidget *notebook;
static GtkWidget *configure_page;
static gint last_selected_page;
static gulong switch_page_handler_id;

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_plugin_manager_get_type (void);

G_DEFINE_DYNAMIC_TYPE (EPluginManager, e_plugin_manager, E_TYPE_EXTENSION)

static void
eppm_set_label (GtkLabel *l,
                const gchar *v)
{
	gtk_label_set_label (l, v ? v:_("Unknown"));
}

static void
eppm_switch_page_cb (GtkNotebook *notebook,
                     GtkWidget *page,
                     guint page_num)
{
	last_selected_page = page_num;
}

static void
eppm_show_plugin (Manager *m,
                  EPlugin *ep,
                  GtkWidget *cfg_widget)
{
	if (ep) {
		gchar *string;

		string = g_markup_printf_escaped ("<b>%s</b>", ep->name);
		gtk_label_set_markup (GTK_LABEL (m->items[LABEL_NAME]), string);
		gtk_label_set_markup (GTK_LABEL (m->config_plugin_label), string);
		g_free (string);

		if (ep->authors) {
			GSList *l = ep->authors;
			GString *out = g_string_new ("");

			for (; l; l = g_slist_next (l)) {
				EPluginAuthor *epa = l->data;

				if (l != ep->authors)
					g_string_append (out, ",\n");
				if (epa->name)
					g_string_append (out, epa->name);
				if (epa->email) {
					g_string_append (out, " <");
					g_string_append (out, epa->email);
					g_string_append (out, ">");
				}
			}
			gtk_label_set_label (m->items[LABEL_AUTHOR], out->str);
			g_string_free (out, TRUE);
		} else {
			eppm_set_label (m->items[LABEL_AUTHOR], NULL);
		}

		eppm_set_label (m->items[LABEL_DESCRIPTION], ep->description);
	} else {
		gint i;

		gtk_label_set_markup (GTK_LABEL (m->config_plugin_label), "");
		for (i = 0; i < LABEL_LAST; i++)
			gtk_label_set_label (m->items[i], "");
	}

	if (cfg_widget != NULL)
		gtk_notebook_append_page_menu (
			GTK_NOTEBOOK (notebook), configure_page,
			gtk_label_new (_("Configuration")), NULL);

	if (m->active_cfg_widget != cfg_widget) {
		if (m->active_cfg_widget)
			gtk_widget_hide (m->active_cfg_widget);

		if (cfg_widget)
			gtk_widget_show (cfg_widget);

		m->active_cfg_widget = cfg_widget;
	}
}

static void
eppm_selection_changed (GtkTreeSelection *selection,
                        Manager *m)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_signal_handler_block (notebook, switch_page_handler_id);
	gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), 1);
	g_signal_handler_unblock (notebook, switch_page_handler_id);

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		EPlugin *ep;
		GtkWidget *cfg_widget = NULL;

		gtk_tree_model_get (
			model, &iter,
			COL_PLUGIN_DATA, &ep,
			COL_PLUGIN_CFG_WIDGET, &cfg_widget, -1);
		eppm_show_plugin (m, ep, cfg_widget);

	} else {
		eppm_show_plugin (m, NULL, NULL);
	}

	g_signal_handler_block (notebook, switch_page_handler_id);
	gtk_notebook_set_current_page (
		GTK_NOTEBOOK (notebook), last_selected_page);
	g_signal_handler_unblock (notebook, switch_page_handler_id);
}

static void
eppm_enable_toggled (GtkCellRendererToggle *renderer,
                     const gchar *path_string,
                     GtkTreeModel *model)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	EPlugin *plugin;

	path = gtk_tree_path_new_from_string (path_string);

	if (gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_model_get (
			model, &iter, COL_PLUGIN_DATA, &plugin, -1);

		e_plugin_enable (plugin, !plugin->enabled);

		gtk_list_store_set (
			GTK_LIST_STORE (model), &iter,
			COL_PLUGIN_ENABLED, plugin->enabled, -1);
	}

	gtk_tree_path_free (path);
}

static void
action_plugin_manager_cb (GtkAction *action,
                          EExtension *extension)
{
	Manager *m;
	gint i;
	GtkWidget *dialog;
	GtkWidget *hbox, *w;
	GtkWidget *overview_page;
	GtkWidget *content_area;
	GtkListStore *store;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GSList *plugins, *link;
	gchar *string;
	GtkWidget *subvbox;
	EExtensible *extensible;

	m = g_malloc0 (sizeof (*m));

	/* Retrieve the parent EShellWindow. */
	extensible = e_extension_get_extensible (extension);

	/* Setup the ui */
	dialog = gtk_dialog_new_with_buttons (
		_("Plugin Manager"),
		GTK_WINDOW (extensible),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Close"), GTK_RESPONSE_CLOSE, NULL);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	gtk_window_set_default_size (GTK_WINDOW (dialog), 640, 400);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
	gtk_box_pack_start (GTK_BOX (content_area), hbox, TRUE, TRUE, 0);

	string = g_markup_printf_escaped (
		"<i>%s</i>", _("Note: Some changes "
		"will not take effect until restart"));

	w = g_object_new (
		GTK_TYPE_LABEL,
		"label", string,
		"wrap", FALSE,
		"use_markup", TRUE, NULL);
	gtk_widget_show (w);
	g_free (string);

	gtk_box_pack_start (GTK_BOX (content_area), w, FALSE, TRUE, 12);

	notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), TRUE);
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);

	switch_page_handler_id = g_signal_connect (
		notebook, "switch-page",
		G_CALLBACK (eppm_switch_page_cb), NULL);

	overview_page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	configure_page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	g_object_ref_sink (configure_page);
	gtk_container_set_border_width (GTK_CONTAINER (overview_page), 12);
	gtk_container_set_border_width (GTK_CONTAINER (configure_page), 12);
	gtk_notebook_append_page_menu (
		GTK_NOTEBOOK (notebook), overview_page,
		gtk_label_new (_("Overview")), NULL);

	gtk_widget_show (notebook);
	gtk_widget_show (overview_page);
	gtk_widget_show (configure_page);

	/* name of plugin on "Configuration" tab */
	m->config_plugin_label = g_object_new (
		GTK_TYPE_LABEL,
		"wrap", TRUE,
		"selectable", FALSE,
		"xalign", 0.0,
		"yalign", 0.0, NULL);
	gtk_widget_show (m->config_plugin_label);
	gtk_box_pack_start (
		GTK_BOX (configure_page),
		m->config_plugin_label, FALSE, FALSE, 0);

	store = gtk_list_store_new (
		4, G_TYPE_BOOLEAN, G_TYPE_STRING,
		G_TYPE_POINTER, G_TYPE_POINTER);

	/* fill store */
	plugins = e_plugin_list_plugins ();

	for (link = plugins; link != NULL; link = g_slist_next (link)) {
		EPlugin *ep = E_PLUGIN (link->data);
		GtkTreeIter iter;
		GtkWidget *cfg_widget;

		if (!g_getenv ("EVO_SHOW_ALL_PLUGINS")) {
			if (ep->flags & E_PLUGIN_FLAGS_SYSTEM_PLUGIN)
				continue;
		}

		cfg_widget = e_plugin_get_configure_widget (ep);
		if (cfg_widget) {
			gtk_widget_hide (cfg_widget);
			gtk_box_pack_start (
				GTK_BOX (configure_page),
				cfg_widget, TRUE, TRUE, 12);
		}

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			COL_PLUGIN_ENABLED, ep->enabled,
			COL_PLUGIN_NAME, ep->name ? ep->name : ep->id,
			COL_PLUGIN_DATA, ep,
			COL_PLUGIN_CFG_WIDGET, cfg_widget, -1);
	}

	/* setup the treeview */
	tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
	gtk_tree_view_set_reorderable (tree_view, FALSE);
	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (store));
	gtk_tree_view_set_search_column (tree_view, COL_PLUGIN_NAME);
	gtk_tree_view_set_headers_visible (tree_view, TRUE);

	renderer = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_insert_column_with_attributes (
		tree_view, COL_PLUGIN_ENABLED, _("Enabled"),
		renderer, "active", COL_PLUGIN_ENABLED, NULL);
	g_signal_connect (
		renderer, "toggled",
		G_CALLBACK (eppm_enable_toggled), store),

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (
		tree_view, COL_PLUGIN_NAME, _("Plugin"),
		renderer, "text", COL_PLUGIN_NAME, NULL);

	/* set sort column */
	gtk_tree_sortable_set_sort_column_id (
		GTK_TREE_SORTABLE (store),
		COL_PLUGIN_NAME, GTK_SORT_ASCENDING);

	w = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (w),
		GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (w), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (w), GTK_WIDGET (tree_view));

	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (w), FALSE, TRUE, 0);

	/* Show all widgets in hbox before we pack the notebook, because not
	 * all widgets in notebook are going to be visible at one moment. */
	gtk_widget_show_all (hbox);

	gtk_box_pack_start (GTK_BOX (hbox), notebook, TRUE, TRUE, 0);

	/* this is plugin's name label */
	subvbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	m->items[0] = g_object_new (
		GTK_TYPE_LABEL,
		"wrap", TRUE,
		"selectable", FALSE,
		"xalign", 0.0,
		"yalign", 0.0, NULL);
	gtk_box_pack_start (
		GTK_BOX (subvbox),
		GTK_WIDGET (m->items[0]), TRUE, TRUE, 0);
	gtk_box_pack_start (
		GTK_BOX (overview_page), subvbox, FALSE, TRUE, 0);

	/* this is every other data */
	for (i = 1; i < LABEL_LAST; i++) {
		gchar *markup;

		subvbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

		markup = g_markup_printf_escaped (
			"<span weight=\"bold\">%s:</span>",
			_(label_info[i].label));
		m->labels[i] = g_object_new (
			GTK_TYPE_LABEL,
			"label", markup,
			"use_markup", TRUE,
			"xalign", 0.0,
			"yalign", 0.0, NULL);
		gtk_box_pack_start (
			GTK_BOX (subvbox),
			GTK_WIDGET (m->labels[i]), FALSE, TRUE, 0);
		g_free (markup);

		m->items[i] = g_object_new (
			GTK_TYPE_LABEL,
			"wrap", TRUE,
			"selectable", TRUE,
			"can-focus", FALSE,
			"xalign", 0.0,
			"yalign", 0.0, NULL);
		gtk_box_pack_start (
			GTK_BOX (subvbox),
			GTK_WIDGET (m->items[i]), TRUE, TRUE, 0);

		gtk_box_pack_start (
			GTK_BOX (overview_page), subvbox, FALSE, TRUE, 12);
	}

	gtk_widget_show_all (overview_page);

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (eppm_selection_changed), m);

	atk_object_set_name (
		gtk_widget_get_accessible (
		GTK_WIDGET (tree_view)), _("Plugin"));

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	g_slist_foreach (plugins, (GFunc) g_object_unref, NULL);
	g_slist_free (plugins);

	g_object_unref (store);

	g_free (m);
}

static void
plugin_manager_constructed (GObject *object)
{
	EExtensible *extensible;
	EPluginManager *extension;
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkAction *action;
	const gchar *action_name;
	const gchar *action_label;
	const gchar *action_tooltip;
	const gchar *widget_path;
	guint merge_id;

	extension = E_PLUGIN_MANAGER (object);
	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	shell_window = E_SHELL_WINDOW (extensible);
	action_group = E_SHELL_WINDOW_ACTION_GROUP_SHELL (shell_window);
	ui_manager = e_shell_window_get_ui_manager (shell_window);
	merge_id = gtk_ui_manager_new_merge_id (ui_manager);

	action_name = "plugin-manager";
	action_label = _("_Plugins");
	action_tooltip = _("Enable and disable plugins");
	widget_path = "/main-menu/edit-menu/administrative-actions";

	action = gtk_action_new (
		action_name, action_label, action_tooltip, NULL);

	g_signal_connect (
		action, "activate",
		G_CALLBACK (action_plugin_manager_cb), extension);

	gtk_action_group_add_action (action_group, action);

	gtk_ui_manager_add_ui (
		ui_manager, merge_id, widget_path, action_name,
		action_name, GTK_UI_MANAGER_AUTO, FALSE);

	g_object_unref (action);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_plugin_manager_parent_class)->constructed (object);
}

static void
e_plugin_manager_class_init (EPluginManagerClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = plugin_manager_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_SHELL_WINDOW;
}

static void
e_plugin_manager_class_finalize (EPluginManagerClass *class)
{
}

static void
e_plugin_manager_init (EPluginManager *extension)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_plugin_manager_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
