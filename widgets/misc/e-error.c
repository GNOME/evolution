/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- *
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Novell Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <glib.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkimage.h>

#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-url.h>

#include "e-error.h"

#define d(x) x

struct _e_error_button {
	struct _e_error_button *next;
	char *stock;
	char *label;
	int response;
};

struct _e_error {
	guint32 flags;
	char *id;
	int type;
	int default_response;
	char *title;
	char *primary;
	char *secondary;
	char *help_uri;
	struct _e_error_button *buttons;
};

struct _e_error_table {
	char *domain;
	GHashTable *errors;
};

static GHashTable *error_table;
static GSList *ee_parent_list;

/* ********************************************************************** */

static struct _e_error_button default_ok_button = {
	NULL, "gtk-ok", NULL, GTK_RESPONSE_OK
};

static struct _e_error default_errors[] = {
	{ GTK_DIALOG_MODAL, "error", 3, GTK_RESPONSE_OK, N_("Evolution Error"), "{0}", "{1}", NULL, &default_ok_button },
	{ GTK_DIALOG_MODAL, "error-primary", 3, GTK_RESPONSE_OK, N_("Evolution Error"), "{0}", NULL, NULL, &default_ok_button },
	{ GTK_DIALOG_MODAL, "warning", 1, GTK_RESPONSE_OK, N_("Evolution Warning"), "{0}", "{1}", NULL, &default_ok_button },
	{ GTK_DIALOG_MODAL, "warning-primary", 1, GTK_RESPONSE_OK, N_("Evolution Warning"), "{0}", NULL, NULL, &default_ok_button },
};

/* ********************************************************************** */

static struct {
	char *name;
	int id;
} response_map[] = {
	{ "GTK_RESPONSE_REJECT", GTK_RESPONSE_REJECT },
	{ "GTK_RESPONSE_ACCEPT", GTK_RESPONSE_ACCEPT },
	{ "GTK_RESPONSE_OK", GTK_RESPONSE_OK },
	{ "GTK_RESPONSE_CANCEL", GTK_RESPONSE_CANCEL },
	{ "GTK_RESPONSE_CLOSE", GTK_RESPONSE_CLOSE },
	{ "GTK_RESPONSE_YES", GTK_RESPONSE_YES },
	{ "GTK_RESPONSE_NO", GTK_RESPONSE_NO },
	{ "GTK_RESPONSE_APPLY", GTK_RESPONSE_APPLY },
	{ "GTK_RESPONSE_HELP", GTK_RESPONSE_HELP },
};

static int
map_response(const char *name)
{
	int i;
	
	for (i=0;i<sizeof(response_map)/sizeof(response_map[0]);i++)
		if (!strcmp(name, response_map[i].name))
			return response_map[i].id;

	return 0;
}

static struct {
	const char *name;
	const char *icon;
	const char *title;
} type_map[] = {
	{ "info", GTK_STOCK_DIALOG_INFO, N_("Evolution Information") },
	{ "warning", GTK_STOCK_DIALOG_WARNING, N_("Evolution Warning") },
	{ "question", GTK_STOCK_DIALOG_QUESTION, N_("Evolution Query") },
	{ "error", GTK_STOCK_DIALOG_ERROR, N_("Evolution Error") },
};

static int
map_type(const char *name)
{
	int i;

	if (name) {
		for (i=0;i<sizeof(type_map)/sizeof(type_map[0]);i++)
			if (!strcmp(name, type_map[i].name))
				return i;
	}

	return 3;
}

/*
  XML format:

 <error id="error-id" type="info|warning|question|error"? response="default_response"? modal="true"? >
  <title>Window Title</title>?
  <primary>Primary error text.</primary>?
  <secondary>Secondary error text.</secondary>?
  <help uri="help uri"/> ?
  <button stock="stock-button-id"? label="button label"? response="response_id"? /> *
 </error>

 The tool e-error-tool is used to extract the translatable strings for
 translation.

*/
static void
ee_load(const char *path)
{
	xmlDocPtr doc;
	xmlNodePtr root, error, scan;
	struct _e_error *e;
	struct _e_error_button *lastbutton;
	struct _e_error_table *table;
	char *tmp;

	d(printf("loading error file %s\n", path));
	
	doc = xmlParseFile(path);
	if (doc == NULL) {
		g_warning("Error file '%s' not found", path);
		return;
	}

	root = xmlDocGetRootElement(doc);
	if (root == NULL
	    || strcmp(root->name, "error-list") != 0
	    || (tmp = xmlGetProp(root, "domain")) == NULL) {
		g_warning("Error file '%s' invalid format", path);
		xmlFreeDoc(doc);
		return;
	}

	table = g_hash_table_lookup(error_table, tmp);
	if (table == NULL) {
		table = g_malloc0(sizeof(*table));
		table->domain = g_strdup(tmp);
		table->errors = g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(error_table, table->domain, table);
	} else
		g_warning("Error file '%s', domain '%s' already used, merging", path, tmp);
	g_free(tmp);

	for (error = root->children;error;error = error->next) {
		if (!strcmp(error->name, "error")) {
			tmp = xmlGetProp(error, "id");
			if (tmp == NULL)
				continue;

			e = g_malloc0(sizeof(*e));
			e->id = g_strdup(tmp);
			xmlFree(tmp);
			lastbutton = (struct _e_error_button *)&e->buttons;

			tmp = xmlGetProp(error, "modal");
			if (tmp) {
				if (!strcmp(tmp, "true"))
					e->flags |= GTK_DIALOG_MODAL;
				xmlFree(tmp);
			}

			tmp = xmlGetProp(error, "type");
			e->type = map_type(tmp);
			if (tmp)
				xmlFree(tmp);

			tmp = xmlGetProp(error, "default");
			if (tmp) {
				e->default_response = map_response(tmp);
				xmlFree(tmp);
			}

			for (scan = error->children;scan;scan=scan->next) {
				if (!strcmp(scan->name, "primary")) {
					if ((tmp = xmlNodeGetContent(scan))) {
						e->primary = g_strdup(_(tmp));
						xmlFree(tmp);
					}
				} else if (!strcmp(scan->name, "secondary")) {
					if ((tmp = xmlNodeGetContent(scan))) {
						e->secondary = g_strdup(_(tmp));
						xmlFree(tmp);
					}
				} else if (!strcmp(scan->name, "title")) {
					if ((tmp = xmlNodeGetContent(scan))) {
						e->title = g_strdup(_(tmp));
						xmlFree(tmp);
					}
				} else if (!strcmp(scan->name, "help")) {
					tmp = xmlGetProp(scan, "uri");
					if (tmp) {
						e->help_uri = g_strdup(tmp);
						xmlFree(tmp);
					}
				} else if (!strcmp(scan->name, "button")) {
					struct _e_error_button *b;

					b = g_malloc0(sizeof(*b));
					tmp = xmlGetProp(scan, "stock");
					if (tmp) {
						b->stock = g_strdup(tmp);
						xmlFree(tmp);
					}
					tmp = xmlGetProp(scan, "label");
					if (tmp) {
						b->label = g_strdup(_(tmp));
						xmlFree(tmp);
					}
					tmp = xmlGetProp(scan, "response");
					if (tmp) {
						b->response = map_response(tmp);
						xmlFree(tmp);
					}

					if (b->stock == NULL && b->label == NULL) {
						g_warning("Error file '%s': missing button details in error '%s'", path, e->id);
						g_free(b->stock);
						g_free(b->label);
						g_free(b);
					} else {
						lastbutton->next = b;
						lastbutton = b;
					}
				}
			}

			g_hash_table_insert(table->errors, e->id, e);
		}
	}

	xmlFreeDoc(doc);
}

static void
ee_load_tables(void)
{
	DIR *dir;
	struct dirent *d;
	const char *base = EVOLUTION_PRIVDATADIR "/errors";
	struct _e_error_table *table;
	int i;

	if (error_table != NULL)
		return;

	error_table = g_hash_table_new(g_str_hash, g_str_equal);

	/* setup system error types */
	table = g_malloc0(sizeof(*table));
	table->domain = "builtin";
	table->errors = g_hash_table_new(g_str_hash, g_str_equal);
	for (i=0;i<sizeof(default_errors)/sizeof(default_errors[0]);i++)
		g_hash_table_insert(table->errors, default_errors[i].id, &default_errors[i]);
	g_hash_table_insert(error_table, table->domain, table);

	/* look for installed error tables */
	dir = opendir(base);
	if (dir == NULL)
		return;

	while ( (d = readdir(dir)) ) {
		char *path;

		if (d->d_name[0] == '.')
			continue;

		path = g_build_filename(base, d->d_name, NULL);
		ee_load(path);
		g_free(path);
	}

	closedir(dir);
}

/* unfortunately, gmarkup_escape doesn't expose its gstring based api :( */
static void
ee_append_text(GString *out, const char *text)
{
	char c;

	while ( (c=*text++) ) {
		if (c == '<')
			g_string_append(out, "&lt;");
		else if (c == '>')
			g_string_append(out, "&gt;");
		else if (c == '"')
			g_string_append(out, "&quot;");
		else if (c == '\'')
			g_string_append(out, "&apos;");
		else if (c == '&')
			g_string_append(out, "&amp;");
		else
			g_string_append_c(out, c);
	}
}

static void
ee_build_label(GString *out, const char *fmt, GPtrArray *args)
{
	const char *end, *newstart;
	int id;

	while (fmt
	       && (newstart = strchr(fmt, '{'))
	       && (end = strchr(newstart+1, '}'))) {
		g_string_append_len(out, fmt, newstart-fmt);
		id = atoi(newstart+1);
		if (id < args->len)
			ee_append_text(out, args->pdata[id]);
		else
			g_warning("Error references argument %d not supplied by caller", id);
		fmt = end+1;
	}

	g_string_append(out, fmt);
}

static void
ee_response(GtkWidget *w, guint button, struct _e_error *e)
{
	GError *err = NULL;

	if (button == GTK_RESPONSE_HELP) {
		g_signal_stop_emission_by_name(w, "response");
		gnome_url_show(e->help_uri, &err);
		if (err) {
			g_warning("Unable to run help uri: %s", err->message);
			g_error_free(err);
		}
	}
}

GtkWidget *
e_error_newv(GtkWindow *parent, const char *tag, const char *arg0, va_list ap)
{
	struct _e_error_table *table;
	struct _e_error *e;
	struct _e_error_button *b;
	GtkWidget *hbox, *w;
	char *tmp, *domain, *id;
	GString *out;
	GPtrArray *args;
	GtkDialog *dialog;

	if (error_table == NULL)
		ee_load_tables();

	dialog = (GtkDialog *)gtk_dialog_new();
	gtk_dialog_set_has_separator(dialog, FALSE);

	gtk_container_set_border_width((GtkContainer *)dialog, 6);

	if (parent == NULL && ee_parent_list)
		parent = (GtkWindow *)ee_parent_list->data;
	if (parent)
		gtk_window_set_transient_for((GtkWindow *)dialog, parent);
	else
		g_warning("No parent set, or default parent available for error dialog");

	domain = alloca(strlen(tag)+1);
	strcpy(domain, tag);
	id = strchr(domain, ':');
	if (id)
		*id++ = 0;

	if ( id == NULL
	     || (table = g_hash_table_lookup(error_table, domain)) == NULL
	     || (e = g_hash_table_lookup(table->errors, id)) == NULL) {
		/* setup a dummy error */
		tmp = g_strdup_printf(_("<span weight=\"bold\">Internal error, unknown error '%s' requested</span>"), tag);
		w = gtk_label_new(NULL);
		gtk_label_set_selectable((GtkLabel *)w, TRUE);
		gtk_label_set_line_wrap((GtkLabel *)w, TRUE);
		gtk_label_set_markup((GtkLabel *)w, tmp);
		gtk_widget_show(w);
		gtk_box_pack_start((GtkBox *)dialog->vbox, w, TRUE, TRUE, 12);

		return (GtkWidget *)dialog;
	}

	if (e->flags & GTK_DIALOG_MODAL)
		gtk_window_set_modal((GtkWindow *)dialog, TRUE);
	gtk_window_set_destroy_with_parent((GtkWindow *)dialog, TRUE);

	if (e->help_uri) {
		w = gtk_dialog_add_button(dialog, GTK_STOCK_HELP, GTK_RESPONSE_HELP);
		g_signal_connect(dialog, "response", G_CALLBACK(ee_response), e);
	}

	b = e->buttons;
	if (b == NULL) {
		gtk_dialog_add_button(dialog, GTK_STOCK_OK, GTK_RESPONSE_OK);
	} else {
		for (b = e->buttons;b;b=b->next) {
			if (b->stock) {
				if (b->label) {
					/* FIXME: So although this looks like it will work, it wont.
					   Need to do it the hard way ... */
					w = gtk_button_new_from_stock(b->stock);
					gtk_button_set_label((GtkButton *)w, b->label);
					gtk_widget_show(w);
					gtk_dialog_add_action_widget(dialog, w, b->response);
				} else
					gtk_dialog_add_button(dialog, b->stock, b->response);
			} else
				gtk_dialog_add_button(dialog, b->label, b->response);
		}
	}

	if (e->default_response)
		gtk_dialog_set_default_response(dialog, e->default_response);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width((GtkContainer *)hbox, 12);

	w = gtk_image_new_from_stock(type_map[e->type].icon, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment((GtkMisc *)w, 0.0, 0.0);
	gtk_box_pack_start((GtkBox *)hbox, w, TRUE, TRUE, 12);

	args = g_ptr_array_new();
	tmp = (char *)arg0;
	while (tmp) {
		g_ptr_array_add(args, tmp);
		tmp = va_arg(ap, char *);
	}

	out = g_string_new("");

	if (e->title) {
		ee_build_label(out, _(e->title), args);
		gtk_window_set_title((GtkWindow *)dialog, out->str);
		g_string_truncate(out, 0);
	} else
		gtk_window_set_title((GtkWindow *)dialog, _(type_map[e->type].title));


	if (e->primary) {
		g_string_append(out, "<span weight=\"bold\" size=\"larger\">");
		ee_build_label(out, _(e->primary), args);
		g_string_append(out, "</span>\n\n");
	}

	if (e->secondary)
		ee_build_label(out, _(e->secondary), args);

	g_ptr_array_free(args, TRUE);

	w = gtk_label_new(NULL);
	gtk_label_set_selectable((GtkLabel *)w, TRUE);
	gtk_label_set_line_wrap((GtkLabel *)w, TRUE);
	gtk_label_set_markup((GtkLabel *)w, out->str);
	g_string_free(out, TRUE);
	gtk_box_pack_start((GtkBox *)hbox, w, FALSE, FALSE, 0);
	gtk_widget_show_all(hbox);

	gtk_box_pack_start((GtkBox *)dialog->vbox, hbox, TRUE, TRUE, 0);

	return (GtkWidget *)dialog;
}

/**
 * e_error_new:
 * @parent: 
 * @tag: error identifier
 * @arg0: The first argument for the error formatter.  The list must
 * be NULL terminated.
 * 
 * Creates a new error widget.  The @tag argument is used to determine
 * which error to use, it is in the format domain:error-id.  The NULL
 * terminated list of arguments, starting with @arg0 is used to fill
 * out the error definition.
 * 
 * Return value: A GtkDialog which can be used for showing an error
 * dialog asynchronously.
 **/
struct _GtkWidget *
e_error_new(struct _GtkWindow *parent, const char *tag, const char *arg0, ...)
{
	GtkWidget *w;
	va_list ap;

	va_start(ap, arg0);
	w = e_error_newv(parent, tag, arg0, ap);
	va_end(ap);

	return w;
}

int
e_error_runv(GtkWindow *parent, const char *tag, const char *arg0, va_list ap)
{
	GtkWidget *w;
	int res;

	w = e_error_newv(parent, tag, arg0, ap);

	res = gtk_dialog_run((GtkDialog *)w);
	gtk_widget_destroy(w);

	return res;
}

/**
 * e_error_run:
 * @parent: 
 * @tag: 
 * @arg0: 
 * 
 * Sets up, displays, runs and destroys a standard evolution error
 * dialog based on @tag, which is in the format domain:error-id.
 * 
 * Return value: The response id of the button pressed.
 **/
int
e_error_run(GtkWindow *parent, const char *tag, const char *arg0, ...)
{
	GtkWidget *w;
	va_list ap;
	int res;

	va_start(ap, arg0);
	w = e_error_newv(parent, tag, arg0, ap);
	va_end(ap);

	res = gtk_dialog_run((GtkDialog *)w);
	gtk_widget_destroy(w);

	return res;
}

static void
remove_parent(GtkWidget *w, GtkWidget *parent)
{
	ee_parent_list = g_slist_remove(ee_parent_list, parent);
}

/**
 * e_error_default_parent:
 * @parent: 
 * 
 * Bit of a hack, set a default parent that will be used to parent any
 * error boxes if none is supplied.
 *
 * This may be called multiple times, and the last call will be the
 * main default.  This function will keep track of the parents
 * destruction state.
 **/
void
e_error_default_parent(struct _GtkWindow *parent)
{
	if (g_slist_find(ee_parent_list, parent) == NULL) {
		ee_parent_list = g_slist_prepend(ee_parent_list, parent);
		g_signal_connect(parent, "destroy", G_CALLBACK(remove_parent), parent);
	}
}

