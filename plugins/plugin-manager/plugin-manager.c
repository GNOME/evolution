
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
#include <gtk/gtktreeselection.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktable.h>
#include <gtk/gtkcellrenderertoggle.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkhbox.h>

#include "e-util/e-plugin.h"
#include "shell/es-menu.h"

enum {
	LABEL_NAME,
	LABEL_AUTHOR,
	LABEL_ID,
	LABEL_PATH,
	LABEL_DESCRIPTION,
	LABEL_LAST
};

static struct {
	const char *label;
} label_info[LABEL_LAST] = {
	{ N_("Name"), },
	{ N_("Author(s)"), },
	{ N_("Id"), },
	{ N_("Path"), },
	{ N_("Description"), },
};

typedef struct _Manager Manager;
struct _Manager {
	GtkDialog *dialog;
	GtkTreeView *tree;
	GtkListStore *model;

	GtkTable *table;
	GtkLabel *labels[LABEL_LAST];
	GtkLabel *items[LABEL_LAST];

	GSList *plugins;
};

/* for tracking if we're shown */
static GtkDialog *dialog;

void org_gnome_plugin_manager_manage(void *ep, ESMenuTargetShell *t);

static void
eppm_set_label(GtkLabel *l, const char *v)
{
	gtk_label_set_label(l, v?v:_("Unknown"));
}

static void
eppm_show_plugin(Manager *m, EPlugin *ep)
{
	if (ep) {
		eppm_set_label(m->items[LABEL_NAME], ep->name);
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

		eppm_set_label(m->items[LABEL_ID], ep->id);
		eppm_set_label(m->items[LABEL_PATH], ep->path);
		eppm_set_label(m->items[LABEL_DESCRIPTION], ep->description);
		gtk_widget_set_sensitive((GtkWidget *)m->table, TRUE);
	} else {
		int i;

		for (i=0;i<LABEL_LAST;i++)
			gtk_label_set_label(m->items[i], "");
		gtk_widget_set_sensitive((GtkWidget *)m->table, FALSE);
	}
}

static void
eppm_selection_changed(GtkTreeSelection *selection, Manager *m)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		EPlugin *ep;

		gtk_tree_model_get(model, &iter, 2, &ep, -1);
		eppm_show_plugin(m, ep);
	} else {
		eppm_show_plugin(m, NULL);
	}
}

static void
eppm_enable_toggled(GtkCellRendererToggle *renderer, char *arg1, Manager *m)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;
	EPlugin *plugin;

	path = gtk_tree_path_new_from_string(arg1);
	selection = gtk_tree_view_get_selection(m->tree);
	if (gtk_tree_model_get_iter((GtkTreeModel *)m->model, &iter, path)) {
		gtk_tree_model_get((GtkTreeModel *)m->model, &iter, 2, &plugin, -1);
		e_plugin_enable(plugin, !plugin->enabled);
		gtk_list_store_set(m->model, &iter, 1, plugin->enabled, -1);
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

	g_free(m);
}

static void
eppm_response(GtkDialog *w, int button, Manager *m)
{
	gtk_widget_destroy((GtkWidget*)w);
	dialog = NULL;
}

void
org_gnome_plugin_manager_manage(void *ep, ESMenuTargetShell *t)
{
	Manager *m;
	int i;
	GtkWidget *hbox, *w;
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
							     GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	/* this isn't actually big enough, but oh well, i'll work out resizing later */
	gtk_window_set_default_size((GtkWindow *)m->dialog, 640, 400);
	g_object_set((GObject *)m->dialog, "has_separator", FALSE, NULL);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width((GtkContainer *)hbox, 12);
	gtk_box_pack_start((GtkBox *)m->dialog->vbox, hbox, TRUE, TRUE, 0);

	w = g_object_new(gtk_label_get_type(),
			 "label", _("Note: Some changes will not take effect until restart"),
			 "wrap", TRUE,
			 NULL);
	gtk_widget_show(w);
	gtk_box_pack_start((GtkBox *)m->dialog->vbox, w, FALSE, TRUE, 6);

	m->tree = (GtkTreeView *)gtk_tree_view_new();

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes(m->tree, -1, _("Plugin"), renderer, "text", 0, NULL);
	renderer = gtk_cell_renderer_toggle_new();
	/*g_object_set((GObject *)renderer, "activatable", TRUE, NULL);*/
	gtk_tree_view_insert_column_with_attributes(m->tree, -1, _("Enabled"), renderer, "active", 1, NULL);
	g_signal_connect(renderer, "toggled", G_CALLBACK(eppm_enable_toggled), m);

	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy((GtkScrolledWindow *)w, GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type((GtkScrolledWindow *)w, GTK_SHADOW_IN);
	gtk_container_add((GtkContainer *)w, (GtkWidget *)m->tree);	
	gtk_box_pack_start((GtkBox *)hbox, (GtkWidget *)w, FALSE, TRUE, 6);

	m->table = (GtkTable *)gtk_table_new(LABEL_LAST, 2, FALSE);
	gtk_table_set_col_spacings(m->table, 6);
	gtk_table_set_row_spacings(m->table, 6);
	for (i=0;i<LABEL_LAST;i++) {
		char *markup;

		markup = g_strdup_printf("<span weight=\"bold\">%s</span>", _(label_info[i].label));
		m->labels[i] = g_object_new(gtk_label_get_type(),
					    "label", markup,
					    "use_markup", TRUE,
					    "xalign", 1.0,
					    "yalign", 0.0, NULL);
		g_free(markup);
		gtk_table_attach(m->table, (GtkWidget *)m->labels[i], 0, 1, i, i+1, GTK_FILL, GTK_FILL, 0, 0);
		m->items[i] = g_object_new(gtk_label_get_type(),
					   "wrap", TRUE,
					   "selectable", TRUE,
					   "xalign", 0.0,
					   "yalign", 0.0, NULL);
		gtk_table_attach(m->table, (GtkWidget *)m->items[i], 1, 2, i, i+1, GTK_EXPAND|GTK_FILL, GTK_FILL, 0, 0);
	}

	gtk_box_pack_start((GtkBox *)hbox, (GtkWidget *)m->table, TRUE, TRUE, 6);
	gtk_widget_show_all(hbox);

	selection = gtk_tree_view_get_selection(m->tree);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(selection, "changed", G_CALLBACK(eppm_selection_changed), m);

	m->plugins = e_plugin_list_plugins();
	m->model = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_POINTER);

	for (l=m->plugins;l;l=g_slist_next(l)) {
		EPlugin *ep = l->data;
		GtkTreeIter iter;

		/* hide ourselves always */
		if (!strcmp(ep->id, "org.gnome.evolution.plugin.manager"))
			continue;

		gtk_list_store_append(m->model, &iter);
		gtk_list_store_set(m->model, &iter,
				   0, ep->name?ep->name:ep->id,
				   1, ep->enabled,
				   2, ep,
				   -1);
	}
	gtk_tree_view_set_model(m->tree, (GtkTreeModel *)m->model);

	g_object_set_data_full((GObject *)m->dialog, "plugin-manager", m, eppm_free);
	g_signal_connect(m->dialog, "response", G_CALLBACK(eppm_response), m);

	gtk_widget_show((GtkWidget *)m->dialog);
}

int e_plugin_lib_enable(EPluginLib *ep, int enable);

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
