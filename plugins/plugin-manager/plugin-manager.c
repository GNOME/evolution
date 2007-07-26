
/* Copyright (C) 2004 Novell Inc.
   by Michael Zucchi <notzed@ximian.com> */

/* This file is licensed under the GNU GPL v2 or later */

/* A plugin manager ui */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdio.h>

#include <gtk/gtklabel.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreesortable.h>
#include <gtk/gtktreemodelsort.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkcellrenderertoggle.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>

#include "e-util/e-plugin.h"
#include "shell/es-menu.h"

#define d(S) (S)

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
	COL_PLUGIN_DATA
};

static struct {
	const char *label;
} label_info[LABEL_LAST] = {
	{ N_("Name"), },
	{ N_("Author(s)"), },
	{ N_("Description"), },
};

typedef struct _Manager Manager;
struct _Manager {
	GtkDialog *dialog;
	GtkTreeView *treeview;
	GtkTreeModel *model;

	GtkLabel *labels[LABEL_LAST];
	GtkLabel *items[LABEL_LAST];

	GSList *plugins;
};

/* for tracking if we're shown */
static GtkDialog *dialog;

const int RESPONSE_CONFIGURE = 1;

void org_gnome_plugin_manager_manage(void *ep, ESMenuTargetShell *t);
int e_plugin_lib_configure(EPluginLib *ep);

static void
eppm_set_label(GtkLabel *l, const char *v)
{
	gtk_label_set_label(l, v?v:_("Unknown"));
}

static void
eppm_show_plugin(Manager *m, EPlugin *ep)
{
	char *string = NULL;

	if (ep) {
		string = g_strdup_printf("<span size=\"x-large\">%s</span>", ep->name);
		gtk_label_set_markup(GTK_LABEL(m->items[LABEL_NAME]), string);
		g_free(string);
		
		if (ep->authors) {
			GSList *l = ep->authors;
			GString *out = g_string_new("");

			for (;l;l = g_slist_next(l)) {
				EPluginAuthor *epa = l->data;

				if (l != ep->authors)
					g_string_append(out, ",\n");
				if (epa->name)
					g_string_append(out, epa->name);
				if (epa->email) {
					g_string_append(out, " <");
					g_string_append(out, epa->email);
					g_string_append(out, ">");
				}
			}
			gtk_label_set_label(m->items[LABEL_AUTHOR], out->str);
			g_string_free(out, TRUE);
		} else {
			eppm_set_label(m->items[LABEL_AUTHOR], NULL);
		}

		eppm_set_label(m->items[LABEL_DESCRIPTION], ep->description);
	} else {
		int i;

		for (i=0;i<LABEL_LAST;i++)
			gtk_label_set_label(m->items[i], "");
	}
}

static void
eppm_selection_changed(GtkTreeSelection *selection, Manager *m)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		EPlugin *ep;

		gtk_tree_model_get(model, &iter, COL_PLUGIN_DATA, &ep, -1);
		eppm_show_plugin(m, ep);
	} else {
		eppm_show_plugin(m, NULL);
	}
}

static void
eppm_enable_toggled(GtkCellRendererToggle *renderer, const char *path_string, Manager *m)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	EPlugin *plugin;

	path = gtk_tree_path_new_from_string(path_string);
	
	if (gtk_tree_model_get_iter(m->model, &iter, path)) {
		gtk_tree_model_get(m->model, &iter, COL_PLUGIN_DATA, &plugin, -1);
		e_plugin_enable(plugin, !plugin->enabled);

		g_warning( plugin->name );

		gtk_list_store_set(GTK_LIST_STORE(m->model), &iter,
				   COL_PLUGIN_ENABLED, plugin->enabled,
				   -1);
	}
	
	gtk_tree_path_free(path);
}

static void
eppm_free(void *data)
{
	Manager *m = data;
	GSList *l;

	for (l = m->plugins;l;l=g_slist_next(l))
		g_object_unref(l->data);
	g_slist_free(m->plugins);

	g_object_unref(m->model);

	g_free(m);
}

static void
eppm_response(GtkDialog *w, int button, Manager *m)
{
	if (button == GTK_RESPONSE_CLOSE) {
		gtk_widget_destroy((GtkWidget*)w);
		dialog = NULL;
	} else {
		GtkTreeSelection *selection = NULL;
		GtkTreeModel *model = NULL;
		GtkTreeIter iter;

		selection = gtk_tree_view_get_selection(m->treeview);
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			EPlugin *ep =  NULL;

			gtk_tree_model_get(model, &iter, COL_PLUGIN_DATA, &ep, -1);

			e_plugin_configure (ep);
		} else
			d(printf ("\n\a No Plugin is selected \n\a"));
	}
}

void
org_gnome_plugin_manager_manage(void *ep, ESMenuTargetShell *t)
{
	Manager *m;
	int i;
	GtkWidget *hbox, *w;
	GtkListStore *store;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GSList *l;

	if (dialog) {
		gdk_window_raise(((GtkWidget *)dialog)->window);
		return;
	}

	m = g_malloc0(sizeof(*m));

	/* Setup the ui */
	m->dialog = (GtkDialog *)gtk_dialog_new_with_buttons(_("Plugin Manager"),
							     (GtkWindow *)gtk_widget_get_toplevel(t->target.widget),
							     GTK_DIALOG_DESTROY_WITH_PARENT,
							     _("Configure"), RESPONSE_CONFIGURE, 
							     GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, 
							     NULL);

	gtk_window_set_default_size((GtkWindow *)m->dialog, 640, 400);
	g_object_set((GObject *)m->dialog, "has_separator", FALSE, NULL);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width((GtkContainer *)hbox, 12);
	gtk_box_pack_start((GtkBox *)m->dialog->vbox, hbox, TRUE, TRUE, 0);

	char *string;
	string = g_strdup_printf("<i>%s</i>", _("Note: Some changes will not take effect until restart"));

	w = g_object_new(gtk_label_get_type(),
			 "label", string,
			 "wrap", FALSE,
			 "use_markup", TRUE,
			 NULL);
	gtk_widget_show(w);

	gtk_box_pack_start((GtkBox *)m->dialog->vbox, w, FALSE, TRUE, 6);

	store = gtk_list_store_new(3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);

	/* fill store */
	m->plugins = e_plugin_list_plugins();

	for (l=m->plugins;l;l=g_slist_next(l)) {
		EPlugin *ep = l->data;
		GtkTreeIter iter;

		/* hide ourselves always */
		if (!strcmp(ep->id, "org.gnome.evolution.plugin.manager"))
			continue;

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				   COL_PLUGIN_ENABLED, ep->enabled,
				   COL_PLUGIN_NAME, ep->name?ep->name:ep->id,
				   COL_PLUGIN_DATA, ep,
				   -1);
	}

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE (store),
					     COL_PLUGIN_NAME, GTK_SORT_ASCENDING);

	/* setup the treeview */
	m->treeview = (GtkTreeView *)gtk_tree_view_new();
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(m->treeview), FALSE);
	gtk_tree_view_set_model(m->treeview, GTK_TREE_MODEL (store));
	gtk_tree_view_set_search_column(m->treeview, COL_PLUGIN_NAME);
	gtk_tree_view_set_headers_visible(m->treeview, TRUE);

	m->model = GTK_TREE_MODEL(store);

	renderer = gtk_cell_renderer_toggle_new();
	gtk_tree_view_insert_column_with_attributes(m->treeview,
						    COL_PLUGIN_ENABLED, _("Enabled"),
						    renderer, "active", COL_PLUGIN_ENABLED,
						    NULL);
	g_signal_connect(renderer, "toggled", G_CALLBACK(eppm_enable_toggled), m);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes(m->treeview,
						    COL_PLUGIN_NAME, _("Plugin"),
						    renderer, "text", COL_PLUGIN_NAME,
						    NULL);

	/* set sort column */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(m->model), COL_PLUGIN_NAME, GTK_SORT_ASCENDING);

	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy((GtkScrolledWindow *)w, GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type((GtkScrolledWindow *)w, GTK_SHADOW_IN);
	gtk_container_add((GtkContainer *)w, (GtkWidget *)m->treeview);
	gtk_box_pack_start((GtkBox *)hbox, (GtkWidget *)w, FALSE, TRUE, 6);

	GtkWidget *vbox;
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start((GtkBox *)hbox, vbox, TRUE, TRUE, 6);

	/* this is plugin's name label */
	GtkWidget *subvbox;
	subvbox = gtk_vbox_new(FALSE, 0);
	m->items[0] = g_object_new(gtk_label_get_type(),
				   "wrap", TRUE,
				   "selectable", FALSE,
				   "xalign", 0.0,
				   "yalign", 0.0, NULL);
	gtk_box_pack_start((GtkBox *)subvbox, (GtkWidget *)m->items[0], TRUE, TRUE, 0);
	gtk_box_pack_start((GtkBox *)vbox, subvbox, FALSE, TRUE, 6);

	/* this is every other data */
	for (i=1;i<LABEL_LAST;i++) {
		subvbox = gtk_vbox_new(FALSE, 0);

		char *markup;
		markup = g_strdup_printf("<span weight=\"bold\">%s :</span>", _(label_info[i].label));
		m->labels[i] = g_object_new(gtk_label_get_type(),
					    "label", markup,
					    "use_markup", TRUE,
					    "xalign", 0.0,
					    "yalign", 0.0, NULL);
		gtk_box_pack_start((GtkBox *)subvbox, (GtkWidget *)m->labels[i], FALSE, TRUE, 0);
		g_free(markup);

		m->items[i] = g_object_new(gtk_label_get_type(),
					   "wrap", TRUE,
					   "selectable", TRUE,
					   "xalign", 0.0,
					   "yalign", 0.0, NULL);
		gtk_box_pack_start((GtkBox *)subvbox, (GtkWidget *)m->items[i], TRUE, TRUE, 0);

		gtk_box_pack_start((GtkBox *)vbox, subvbox, FALSE, TRUE, 6);
	}

	gtk_widget_show_all(hbox);

	selection = gtk_tree_view_get_selection(m->treeview);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(selection, "changed", G_CALLBACK(eppm_selection_changed), m);


	atk_object_set_name (gtk_widget_get_accessible ((GtkWidget *)m->treeview), _("Plugin"));

	g_object_set_data_full((GObject *)m->dialog, "plugin-manager", m, eppm_free);
	g_signal_connect(m->dialog, "response", G_CALLBACK(eppm_response), m);

	gtk_widget_show((GtkWidget *)m->dialog);
}

int e_plugin_lib_enable(EPluginLib *ep, int enable);

int e_plugin_lib_configure(EPluginLib *ep);

int
e_plugin_lib_enable(EPluginLib *ep, int enable)
{
	if (enable) {
	} else {
		/* This plugin can't be disabled ... */
		return -1;
	}

	return 0;
}

int 
e_plugin_lib_configure(EPluginLib *ep)
{
	d(printf ("\n\a e_plugin_lib_configure in plugin-manager\n\a"));
	/* Makes no sense as this plugin is never shown */
	return 0;
}
