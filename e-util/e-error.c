/*
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
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <string.h>
#include <sys/types.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libedataserver/e-xml-utils.h>

#include "e-util.h"
#include "e-util-private.h"
#include "e-error.h"

#define d(x)

struct _EError
{
	gchar *tag;
	GPtrArray *args;
};

void
e_error_free (EError *error)
{
	if (error == NULL)
		return;
	g_free (error->tag);
	/* arg strings will be freed automatically since we set a free func when
	 * creating the ptr array */
	g_ptr_array_free (error->args, TRUE);
}

struct _e_error_button {
	struct _e_error_button *next;
	const gchar *stock;
	const gchar *label;
	gint response;
};

struct _e_error {
	guint32 flags;
	const gchar *id;
	gint type;
	gint default_response;
	const gchar *title;
	const gchar *primary;
	const gchar *secondary;
	const gchar *help_uri;
	gboolean scroll;
	struct _e_error_button *buttons;
};

struct _e_error_table {
	const gchar *domain;
	const gchar *translation_domain;
	GHashTable *errors;
};

static GHashTable *error_table;

/* ********************************************************************** */

static struct _e_error_button default_ok_button = {
	NULL, "gtk-ok", NULL, GTK_RESPONSE_OK
};

static struct _e_error default_errors[] = {
	{ GTK_DIALOG_MODAL, "error", 3, GTK_RESPONSE_OK, N_("Evolution Error"), "{0}", "{1}", NULL, FALSE, &default_ok_button },
	{ GTK_DIALOG_MODAL, "error-primary", 3, GTK_RESPONSE_OK, N_("Evolution Error"), "{0}", NULL, NULL, FALSE, &default_ok_button },
	{ GTK_DIALOG_MODAL, "warning", 1, GTK_RESPONSE_OK, N_("Evolution Warning"), "{0}", "{1}", NULL, FALSE, &default_ok_button },
	{ GTK_DIALOG_MODAL, "warning-primary", 1, GTK_RESPONSE_OK, N_("Evolution Warning"), "{0}", NULL, NULL, FALSE, &default_ok_button },
};

/* ********************************************************************** */

static struct {
	const gchar *name;
	gint id;
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

static gint
map_response(const gchar *name)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (response_map); i++)
		if (!strcmp(name, response_map[i].name))
			return response_map[i].id;

	return 0;
}

static struct {
	const gchar *name;
	const gchar *icon;
} type_map[] = {
	{ "info", GTK_STOCK_DIALOG_INFO },
	{ "warning", GTK_STOCK_DIALOG_WARNING },
	{ "question", GTK_STOCK_DIALOG_QUESTION },
	{ "error", GTK_STOCK_DIALOG_ERROR },
};

static gint
map_type(const gchar *name)
{
	gint i;

	if (name) {
		for (i = 0; i < G_N_ELEMENTS (type_map); i++)
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
ee_load(const gchar *path)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr root, error, scan;
	struct _e_error *e;
	struct _e_error_button *lastbutton;
	struct _e_error_table *table;
	gchar *tmp;

	d(printf("loading error file %s\n", path));

	doc = e_xml_parse_file (path);
	if (doc == NULL) {
		g_warning("Error file '%s' not found", path);
		return;
	}

	root = xmlDocGetRootElement(doc);
	if (root == NULL
	    || strcmp((gchar *)root->name, "error-list") != 0
	    || (tmp = (gchar *)xmlGetProp(root, (const guchar *)"domain")) == NULL) {
		g_warning("Error file '%s' invalid format", path);
		xmlFreeDoc(doc);
		return;
	}

	table = g_hash_table_lookup(error_table, tmp);
	if (table == NULL) {
		gchar *tmp2;

		table = g_malloc0(sizeof(*table));
		table->domain = g_strdup(tmp);
		table->errors = g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(error_table, (gpointer) table->domain, table);

		tmp2 = (gchar *)xmlGetProp(root, (const guchar *)"translation-domain");
		if (tmp2) {
			table->translation_domain = g_strdup(tmp2);
			xmlFree(tmp2);

			tmp2 = (gchar *)xmlGetProp(root, (const guchar *)"translation-localedir");
			if (tmp2) {
				bindtextdomain(table->translation_domain, tmp2);
				xmlFree(tmp2);
			}
		}
	} else
		g_warning("Error file '%s', domain '%s' already used, merging", path, tmp);
	xmlFree(tmp);

	for (error = root->children;error;error = error->next) {
		if (!strcmp((gchar *)error->name, "error")) {
			tmp = (gchar *)xmlGetProp(error, (const guchar *)"id");
			if (tmp == NULL)
				continue;

			e = g_malloc0(sizeof(*e));
			e->id = g_strdup(tmp);
			e->scroll = FALSE;

			xmlFree(tmp);
			lastbutton = (struct _e_error_button *)&e->buttons;

			tmp = (gchar *)xmlGetProp(error, (const guchar *)"modal");
			if (tmp) {
				if (!strcmp(tmp, "true"))
					e->flags |= GTK_DIALOG_MODAL;
				xmlFree(tmp);
			}

			tmp = (gchar *)xmlGetProp(error, (const guchar *)"type");
			e->type = map_type(tmp);
			if (tmp)
				xmlFree(tmp);

			tmp = (gchar *)xmlGetProp(error, (const guchar *)"default");
			if (tmp) {
				e->default_response = map_response(tmp);
				xmlFree(tmp);
			}

			tmp = (gchar *)xmlGetProp(error, (const guchar *)"scroll");
			if (tmp) {
				if (!strcmp(tmp, "yes"))
					e->scroll = TRUE;
				xmlFree(tmp);
			}

			for (scan = error->children;scan;scan=scan->next) {
				if (!strcmp((gchar *)scan->name, "primary")) {
					if ((tmp = (gchar *)xmlNodeGetContent(scan))) {
						e->primary = g_strdup(dgettext(table->translation_domain, tmp));
						xmlFree(tmp);
					}
				} else if (!strcmp((gchar *)scan->name, "secondary")) {
					if ((tmp = (gchar *)xmlNodeGetContent(scan))) {
						e->secondary = g_strdup(dgettext(table->translation_domain, tmp));
						xmlFree(tmp);
					}
				} else if (!strcmp((gchar *)scan->name, "title")) {
					if ((tmp = (gchar *)xmlNodeGetContent(scan))) {
						e->title = g_strdup(dgettext(table->translation_domain, tmp));
						xmlFree(tmp);
					}
				} else if (!strcmp((gchar *)scan->name, "help")) {
					tmp = (gchar *)xmlGetProp(scan, (const guchar *)"uri");
					if (tmp) {
						e->help_uri = g_strdup(tmp);
						xmlFree(tmp);
					}
				} else if (!strcmp((gchar *)scan->name, "button")) {
					struct _e_error_button *b;
					gchar *label = NULL;
					gchar *stock = NULL;

					b = g_malloc0(sizeof(*b));
					tmp = (gchar *)xmlGetProp(scan, (const guchar *)"stock");
					if (tmp) {
						stock = g_strdup(tmp);
						b->stock = stock;
						xmlFree(tmp);
					}
					tmp = (gchar *)xmlGetProp(scan, (const guchar *)"label");
					if (tmp) {
						label = g_strdup(dgettext(table->translation_domain, tmp));
						b->label = label;
						xmlFree(tmp);
					}
					tmp = (gchar *)xmlGetProp(scan, (const guchar *)"response");
					if (tmp) {
						b->response = map_response(tmp);
						xmlFree(tmp);
					}

					if (stock == NULL && label == NULL) {
						g_warning("Error file '%s': missing button details in error '%s'", path, e->id);
						g_free(stock);
						g_free(label);
						g_free(b);
					} else {
						lastbutton->next = b;
						lastbutton = b;
					}
				}
			}

			g_hash_table_insert(table->errors, (gpointer) e->id, e);
		}
	}

	xmlFreeDoc(doc);
}

static void
ee_load_tables(void)
{
	GDir *dir;
	const gchar *d;
	gchar *base;
	struct _e_error_table *table;
	gint i;

	if (error_table != NULL)
		return;

	error_table = g_hash_table_new(g_str_hash, g_str_equal);

	/* setup system error types */
	table = g_malloc0(sizeof(*table));
	table->domain = "builtin";
	table->errors = g_hash_table_new(g_str_hash, g_str_equal);
	for (i = 0; i < G_N_ELEMENTS (default_errors); i++)
		g_hash_table_insert(table->errors, (gpointer) default_errors[i].id, &default_errors[i]);
	g_hash_table_insert(error_table, (gpointer) table->domain, table);

	/* look for installed error tables */
	base = g_build_filename (EVOLUTION_PRIVDATADIR, "errors", NULL);
	dir = g_dir_open(base, 0, NULL);
	if (dir == NULL) {
		g_free (base);
		return;
	}

	while ( (d = g_dir_read_name(dir)) ) {
		gchar *path;

		if (d[0] == '.')
			continue;

		path = g_build_filename(base, d, NULL);
		ee_load(path);
		g_free(path);
	}

	g_dir_close(dir);
	g_free (base);
}

/* unfortunately, gmarkup_escape doesn't expose its gstring based api :( */
static void
ee_append_text(GString *out, const gchar *text)
{
	gchar c;

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
ee_build_label(GString *out, const gchar *fmt, GPtrArray *args,
               gboolean escape_args)
{
	const gchar *end, *newstart;
	gint id;

	while (fmt
	       && (newstart = strchr(fmt, '{'))
	       && (end = strchr(newstart+1, '}'))) {
		g_string_append_len(out, fmt, newstart-fmt);
		id = atoi(newstart+1);
		if (id < args->len) {
			if (escape_args)
				ee_append_text(out, args->pdata[id]);
			else
				g_string_append(out, args->pdata[id]);
		} else
			g_warning("Error references argument %d not supplied by caller", id);
		fmt = end+1;
	}

	g_string_append(out, fmt);
}

static void
ee_response(GtkWidget *w, guint button, struct _e_error *e)
{
	if (button == GTK_RESPONSE_HELP) {
		g_signal_stop_emission_by_name(w, "response");
		e_display_help (GTK_WINDOW (w), e->help_uri);
	}
}

EError *
e_error_newv(const gchar *tag, const gchar *arg0, va_list ap)
{
	gchar *tmp;
	GPtrArray *args;
	EError *err = g_slice_new0 (EError);
	err->tag = g_strdup (tag);
	err->args = g_ptr_array_new_with_free_func (g_free);

	tmp = (gchar *)arg0;
	while (tmp) {
		g_ptr_array_add(args, g_strdup (tmp));
		tmp = va_arg(ap, gchar *);
	}

	return err;
}

/**
 * e_error_new:
 * @tag: error identifier
 * @arg0: The first argument for the error formatter.  The list must
 * be NULL terminated.
 *
 * Creates a new EError.  The @tag argument is used to determine
 * which error to use, it is in the format domain:error-id.  The NULL
 * terminated list of arguments, starting with @arg0 is used to fill
 * out the error definition.
 **/
EError *
e_error_new(const gchar *tag, const gchar *arg0, ...)
{
	EError *e;
	va_list ap;

	va_start(ap, arg0);
	e = e_error_newv(tag, arg0, ap);
	va_end(ap);

	return e;
}

GtkWidget *
e_error_new_dialog(GtkWindow *parent, EError *error)
{
	struct _e_error_table *table;
	struct _e_error *e;
	struct _e_error_button *b;
	GtkWidget *hbox, *w, *scroll=NULL;
	GtkWidget *action_area;
	GtkWidget *content_area;
	gchar *tmp, *domain, *id;
	GString *out, *oerr;
	GtkDialog *dialog;
	gchar *str, *perr=NULL, *serr=NULL;

	g_return_val_if_fail (error != NULL, NULL);

	if (error_table == NULL)
		ee_load_tables();

	dialog = (GtkDialog *)gtk_dialog_new();
	action_area = gtk_dialog_get_action_area (dialog);
	content_area = gtk_dialog_get_content_area (dialog);

	gtk_dialog_set_has_separator(dialog, FALSE);

	gtk_widget_ensure_style ((GtkWidget *)dialog);
	gtk_container_set_border_width (GTK_CONTAINER (action_area), 12);
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 0);

	if (parent)
		gtk_window_set_transient_for ((GtkWindow *)dialog, parent);
	else
		g_warning (
			"Something called %s() with a NULL parent window.  "
			"This is no longer legal, please fix it.", G_STRFUNC);

	domain = alloca(strlen(error->tag)+1);
	strcpy(domain, error->tag);
	id = strchr(domain, ':');
	if (id)
		*id++ = 0;

	if ( id == NULL
	     || (table = g_hash_table_lookup(error_table, domain)) == NULL
	     || (e = g_hash_table_lookup(table->errors, id)) == NULL) {
		/* setup a dummy error */
		str = g_strdup_printf(_("Internal error, unknown error '%s' requested"), error->tag);
		tmp = g_strdup_printf("<span weight=\"bold\">%s</span>", str);
		g_free(str);
		w = gtk_label_new(NULL);
		gtk_label_set_selectable((GtkLabel *)w, TRUE);
		gtk_label_set_line_wrap((GtkLabel *)w, TRUE);
		gtk_label_set_markup((GtkLabel *)w, tmp);
		GTK_WIDGET_UNSET_FLAGS (w, GTK_CAN_FOCUS);
		gtk_widget_show(w);
		gtk_box_pack_start (GTK_BOX (content_area), w, TRUE, TRUE, 12);

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
#if 0
					/* FIXME: So although this looks like it will work, it wont.
					   Need to do it the hard way ... it also breaks the
					   default_response stuff */
					w = gtk_button_new_from_stock(b->stock);
					gtk_button_set_label((GtkButton *)w, b->label);
					gtk_widget_show(w);
					gtk_dialog_add_action_widget(dialog, w, b->response);
#endif
					gtk_dialog_add_button(dialog, b->label, b->response);
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
	gtk_box_pack_start((GtkBox *)hbox, w, FALSE, FALSE, 12);

	out = g_string_new("");

	if (e->title && *e->title) {
		ee_build_label(out, e->title, error->args, FALSE);
		gtk_window_set_title((GtkWindow *)dialog, out->str);
		g_string_truncate(out, 0);
	} else
		gtk_window_set_title((GtkWindow *)dialog, out->str);

	if (e->primary) {
		g_string_append(out, "<span weight=\"bold\" size=\"larger\">");
		ee_build_label(out, e->primary, error->args, TRUE);
		g_string_append(out, "</span>\n\n");
		oerr = g_string_new("");
		ee_build_label(oerr, e->primary, error->args, FALSE);
		perr = g_strdup (oerr->str);
		g_string_free (oerr, TRUE);
	} else
		perr = g_strdup (gtk_window_get_title (GTK_WINDOW (dialog)));

	if (e->secondary) {
		ee_build_label(out, e->secondary, error->args, TRUE);
		oerr = g_string_new("");
		ee_build_label(oerr, e->secondary, error->args, TRUE);
		serr = g_strdup (oerr->str);
		g_string_free (oerr, TRUE);
	}

	if (e->scroll) {
		scroll = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy ((GtkScrolledWindow *)scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	}
	w = gtk_label_new(NULL);
	gtk_label_set_selectable((GtkLabel *)w, TRUE);
	gtk_label_set_line_wrap((GtkLabel *)w, TRUE);
	gtk_label_set_markup((GtkLabel *)w, out->str);
	GTK_WIDGET_UNSET_FLAGS (w, GTK_CAN_FOCUS);
	g_string_free(out, TRUE);
	if (e->scroll) {
		gtk_scrolled_window_add_with_viewport ((GtkScrolledWindow *)scroll, w);
		gtk_box_pack_start((GtkBox *)hbox, scroll, FALSE, FALSE, 0);
		gtk_window_set_default_size ((GtkWindow *)dialog, 360, 180);
	} else
		gtk_box_pack_start((GtkBox *)hbox, w, TRUE, TRUE, 0);

	gtk_widget_show_all(hbox);

	gtk_box_pack_start (GTK_BOX (content_area), hbox, TRUE, TRUE, 0);
	g_object_set_data_full ((GObject *) dialog, "primary", perr, g_free);
	g_object_set_data_full ((GObject *) dialog, "secondary", serr, g_free);

	return (GtkWidget *)dialog;
}

gint
e_error_run_dialog(GtkWindow *parent, EError *error)
{
	GtkWidget *w;
	gint res;

	w = e_error_new_dialog(parent, error);

	res = gtk_dialog_run((GtkDialog *)w);
	gtk_widget_destroy(w);

	return res;
}

/**
 * e_error_dialog_count_buttons:
 * @dialog: a #GtkDialog
 *
 * Counts the number of buttons in @dialog's action area.
 *
 * Returns: number of action area buttons
 **/
guint
e_error_dialog_count_buttons (GtkDialog *dialog)
{
	GtkWidget *container;
	GList *children, *iter;
	guint n_buttons = 0;

	g_return_val_if_fail (GTK_DIALOG (dialog), 0);

	container = gtk_dialog_get_action_area (dialog);
	children = gtk_container_get_children (GTK_CONTAINER (container));

	/* Iterate over the children looking for buttons. */
	for (iter = children; iter != NULL; iter = iter->next)
		if (GTK_IS_BUTTON (iter->data))
			n_buttons++;

	g_list_free (children);

	return n_buttons;
}

GtkWidget *
e_error_new_dialog_for_args (GtkWindow *parent, const gchar *tag, const gchar *arg0, ...)
{
	GtkWidget *w;
	EError *e;
	va_list ap;

	va_start(ap, arg0);
	e = e_error_newv(tag, arg0, ap);
	va_end(ap);

	w = e_error_new_dialog (parent, e);
	e_error_free (e);

	return w;
}

gint
e_error_run_dialog_for_args (GtkWindow *parent, const gchar *tag, const gchar *arg0, ...)
{
	EError *e;
	va_list ap;
	gint response;

	va_start(ap, arg0);
	e = e_error_newv(tag, arg0, ap);
	va_end(ap);

	response = e_error_run_dialog (parent, e);
	e_error_free (e);

	return response;
}
