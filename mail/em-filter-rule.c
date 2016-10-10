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
 *
 * Authors:
 *		Not Zed <notzed@ximian.com>
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <e-util/e-util.h>

#include "em-filter-rule.h"
#include "em-filter-context.h"

#define d(x)

static gint validate (EFilterRule *fr, EAlert **alert);
static gint filter_eq (EFilterRule *fr, EFilterRule *cm);
static xmlNodePtr xml_encode (EFilterRule *fr);
static gint xml_decode (EFilterRule *fr, xmlNodePtr, ERuleContext *rc);
static void rule_copy (EFilterRule *dest, EFilterRule *src);
static GtkWidget *get_widget (EFilterRule *fr, ERuleContext *rc);

G_DEFINE_TYPE (EMFilterRule, em_filter_rule, E_TYPE_FILTER_RULE)

static void
em_filter_rule_finalize (GObject *object)
{
	EMFilterRule *ff =(EMFilterRule *) object;

	g_list_free_full (ff->actions, (GDestroyNotify) g_object_unref);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_filter_rule_parent_class)->finalize (object);
}

static void
em_filter_rule_class_init (EMFilterRuleClass *class)
{
	GObjectClass *object_class;
	EFilterRuleClass *filter_rule_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = em_filter_rule_finalize;

	filter_rule_class = E_FILTER_RULE_CLASS (class);
	filter_rule_class->validate = validate;
	filter_rule_class->eq = filter_eq;
	filter_rule_class->xml_encode = xml_encode;
	filter_rule_class->xml_decode = xml_decode;
	filter_rule_class->copy = rule_copy;
	filter_rule_class->get_widget = get_widget;
}

static void
em_filter_rule_init (EMFilterRule *ff)
{
}

/**
 * em_filter_rule_new:
 *
 * Create a new EMFilterRule object.
 *
 * Return value: A new #EMFilterRule object.
 **/
EFilterRule *
em_filter_rule_new (void)
{
	return g_object_new (em_filter_rule_get_type (), NULL, NULL);
}

void
em_filter_rule_add_action (EMFilterRule *fr,
                           EFilterPart *fp)
{
	fr->actions = g_list_append (fr->actions, fp);

	e_filter_rule_emit_changed ((EFilterRule *) fr);
}

void
em_filter_rule_remove_action (EMFilterRule *fr,
                              EFilterPart *fp)
{
	fr->actions = g_list_remove (fr->actions, fp);

	e_filter_rule_emit_changed ((EFilterRule *) fr);
}

void
em_filter_rule_replace_action (EMFilterRule *fr,
                               EFilterPart *fp,
                               EFilterPart *new)
{
	GList *l;

	l = g_list_find (fr->actions, fp);
	if (l) {
		l->data = new;
	} else {
		fr->actions = g_list_append (fr->actions, new);
	}

	e_filter_rule_emit_changed ((EFilterRule *) fr);
}

void
em_filter_rule_build_action (EMFilterRule *fr,
                             GString *out)
{
	g_string_append (out, "(begin\n");
	e_filter_part_build_code_list (fr->actions, out);
	g_string_append (out, ")\n");
}

static gint
validate (EFilterRule *fr,
          EAlert **alert)
{
	EMFilterRule *ff =(EMFilterRule *) fr;
	GList *parts;
	gint valid;

	valid = E_FILTER_RULE_CLASS (em_filter_rule_parent_class)->
		validate (fr, alert);

	/* validate rule actions */
	parts = ff->actions;
	while (parts && valid) {
		valid = e_filter_part_validate ((EFilterPart *) parts->data, alert);
		parts = parts->next;
	}

	return valid;
}

static gint
list_eq (GList *al,
         GList *bl)
{
	gint truth = TRUE;

	while (truth && al && bl) {
		EFilterPart *a = al->data, *b = bl->data;

		truth = e_filter_part_eq (a, b);
		al = al->next;
		bl = bl->next;
	}

	return truth && al == NULL && bl == NULL;
}

static gint
filter_eq (EFilterRule *fr,
           EFilterRule *cm)
{
	return E_FILTER_RULE_CLASS (em_filter_rule_parent_class)->eq (fr, cm)
		&& list_eq (
			((EMFilterRule *) fr)->actions,
			((EMFilterRule *) cm)->actions);
}

static xmlNodePtr
xml_encode (EFilterRule *fr)
{
	EMFilterRule *ff =(EMFilterRule *) fr;
	xmlNodePtr node, set, work;
	GList *l;

	node = E_FILTER_RULE_CLASS (em_filter_rule_parent_class)->
		xml_encode (fr);
	g_return_val_if_fail (node != NULL, NULL);
	set = xmlNewNode (NULL, (const guchar *)"actionset");
	xmlAddChild (node, set);
	l = ff->actions;
	while (l) {
		work = e_filter_part_xml_encode ((EFilterPart *) l->data);
		xmlAddChild (set, work);
		l = l->next;
	}

	return node;

}

static void
load_set (xmlNodePtr node,
          EMFilterRule *ff,
          ERuleContext *rc)
{
	xmlNodePtr work;
	gchar *rulename;
	EFilterPart *part;

	work = node->children;
	while (work) {
		if (!strcmp ((gchar *) work->name, "part")) {
			rulename = (gchar *) xmlGetProp (work, (const guchar *)"name");
			part = em_filter_context_find_action ((EMFilterContext *) rc, rulename);
			if (part) {
				part = e_filter_part_clone (part);
				e_filter_part_xml_decode (part, work);
				em_filter_rule_add_action (ff, part);
			} else {
				g_warning ("cannot find rule part '%s'\n", rulename);
			}
			xmlFree (rulename);
		} else if (work->type == XML_ELEMENT_NODE) {
			g_warning ("Unknown xml node in part: %s", work->name);
		}
		work = work->next;
	}
}

static gint
xml_decode (EFilterRule *fr,
            xmlNodePtr node,
            ERuleContext *rc)
{
	EMFilterRule *ff =(EMFilterRule *) fr;
	xmlNodePtr work;
	gint result;

	result = E_FILTER_RULE_CLASS (em_filter_rule_parent_class)->
		xml_decode (fr, node, rc);
	if (result != 0)
		return result;

	work = node->children;
	while (work) {
		if (!strcmp ((gchar *) work->name, "actionset")) {
			load_set (work, ff, rc);
		}
		work = work->next;
	}

	return 0;
}

static void
rule_copy (EFilterRule *dest,
           EFilterRule *src)
{
	EMFilterRule *fdest, *fsrc;
	GList *node;

	fdest =(EMFilterRule *) dest;
	fsrc =(EMFilterRule *) src;

	if (fdest->actions) {
		g_list_foreach (fdest->actions, (GFunc) g_object_unref, NULL);
		g_list_free (fdest->actions);
		fdest->actions = NULL;
	}

	node = fsrc->actions;
	while (node) {
		EFilterPart *part = node->data;

		g_object_ref (part);
		fdest->actions = g_list_append (fdest->actions, part);
		node = node->next;
	}

	E_FILTER_RULE_CLASS (em_filter_rule_parent_class)->copy (dest, src);
}

struct _part_data {
	EFilterRule *fr;
	EMFilterContext *f;
	EFilterPart *part;
	GtkWidget *partwidget, *container;
};

static void
part_combobox_changed (GtkComboBox *combobox,
                       struct _part_data *data)
{
	EFilterPart *part = NULL;
	EFilterPart *newpart;
	gint index, i;

	index = gtk_combo_box_get_active (combobox);
	for (i = 0, part = em_filter_context_next_action (data->f, part);
		part && i < index;
		i++, part = em_filter_context_next_action (data->f, part)) {
		/* traverse until reached index */
	}

	if (!part) {
		g_warn_if_reached ();
		return;
	}
	g_return_if_fail (i == index);

	/* dont update if we haven't changed */
	if (!strcmp (part->title, data->part->title))
		return;

	/* here we do a widget shuffle, throw away the old widget/rulepart,
	 * and create another */
	if (data->partwidget)
		gtk_container_remove (GTK_CONTAINER (data->container), data->partwidget);

	newpart = e_filter_part_clone (part);
	e_filter_part_copy_values (newpart, data->part);
	em_filter_rule_replace_action ((EMFilterRule *) data->fr, data->part, newpart);
	g_object_unref (data->part);
	data->part = newpart;
	data->partwidget = e_filter_part_get_widget (newpart);
	if (data->partwidget)
		gtk_box_pack_start (
			GTK_BOX (data->container),
			data->partwidget, TRUE, TRUE, 0);
}

static GtkWidget *
get_rule_part_widget (EMFilterContext *f,
                      EFilterPart *newpart,
                      EFilterRule *fr)
{
	EFilterPart *part = NULL;
	GtkWidget *combobox;
	GtkWidget *hbox;
	GtkWidget *p;
	gint index = 0, current = 0;
	struct _part_data *data;

	data = g_malloc0 (sizeof (*data));
	data->fr = fr;
	data->f = f;
	data->part = newpart;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	/* only set to automatically clean up the memory and for less_parts */
	g_object_set_data_full ((GObject *) hbox, "data", data, g_free);

	p = e_filter_part_get_widget (newpart);

	data->partwidget = p;
	data->container = hbox;

	combobox = gtk_combo_box_text_new ();
	while ((part = em_filter_context_next_action (f, part))) {
		gtk_combo_box_text_append_text (
			GTK_COMBO_BOX_TEXT (combobox), _(part->title));

		if (!strcmp (newpart->title, part->title))
			current = index;

		index++;
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), current);
	g_signal_connect (
		combobox, "changed",
		G_CALLBACK (part_combobox_changed), data);
	gtk_widget_show (combobox);

	gtk_box_pack_start (GTK_BOX (hbox), combobox, FALSE, FALSE, 0);
	if (p)
		gtk_box_pack_start (GTK_BOX (hbox), p, TRUE, TRUE, 0);

	gtk_widget_show_all (hbox);

	return hbox;
}

struct _rule_data {
	EFilterRule *fr;
	EMFilterContext *f;
	GtkWidget *parts;
};

static void
less_parts (GtkWidget *button,
            struct _rule_data *data)
{
	EFilterPart *part;
	GtkWidget *rule;
	struct _part_data *part_data;
	GList *l;

	l =((EMFilterRule *) data->fr)->actions;
	if (g_list_length (l) < 2)
		return;

	rule = g_object_get_data ((GObject *) button, "rule");
	part_data = g_object_get_data ((GObject *) rule, "data");

	g_return_if_fail (part_data != NULL);

	part = part_data->part;

	/* remove the part from the list */
	em_filter_rule_remove_action ((EMFilterRule *) data->fr, part);
	g_object_unref (part);

	/* and from the display */
	gtk_container_remove (GTK_CONTAINER (data->parts), rule);
	gtk_container_remove (GTK_CONTAINER (data->parts), button);
}

static void
attach_rule (GtkWidget *rule,
             struct _rule_data *data,
             EFilterPart *part,
             gint row)
{
	GtkWidget *remove;

	gtk_table_attach (
		GTK_TABLE (data->parts), rule, 0, 1, row, row + 1,
		GTK_EXPAND | GTK_FILL, 0, 0, 0);

	remove = e_dialog_button_new_with_icon ("list-remove", _("_Remove"));
	g_object_set_data ((GObject *) remove, "rule", rule);
	/*gtk_button_set_relief(GTK_BUTTON(remove), GTK_RELIEF_NONE);*/
	g_signal_connect (
		remove, "clicked",
		G_CALLBACK (less_parts), data);
	gtk_table_attach (
		GTK_TABLE (data->parts), remove, 1, 2, row, row + 1,
		0, 0, 0, 0);
	gtk_widget_show (remove);
}

static void
do_grab_focus_cb (GtkWidget *widget,
                  gpointer data)
{
	gboolean *done = (gboolean *) data;

	if (*done || !widget)
		return;

	if (gtk_widget_get_can_focus (widget) || GTK_IS_COMBO_BOX (widget)) {
		*done = TRUE;
		gtk_widget_grab_focus (widget);
	} else if (GTK_IS_CONTAINER (widget)) {
		gtk_container_foreach (GTK_CONTAINER (widget), do_grab_focus_cb, done);
	}
}

static void ensure_scrolled_height (GtkScrolledWindow *scrolled_window);

static void
more_parts (GtkWidget *button,
            struct _rule_data *data)
{
	EFilterPart *new;

	/* create a new rule entry, use the first type of rule */
	new = em_filter_context_next_action ((EMFilterContext *) data->f, NULL);
	if (new) {
		GtkWidget *w;
		guint rows;

		new = e_filter_part_clone (new);
		em_filter_rule_add_action ((EMFilterRule *) data->fr, new);
		w = get_rule_part_widget (data->f, new, data->fr);

		g_object_get (data->parts, "n-rows", &rows, NULL);
		gtk_table_resize (GTK_TABLE (data->parts), rows + 1, 2);
		attach_rule (w, data, new, rows);

		if (GTK_IS_CONTAINER (w)) {
			gboolean done = FALSE;

			gtk_container_foreach (GTK_CONTAINER (w), do_grab_focus_cb, &done);
		} else
			gtk_widget_grab_focus (w);

		/* also scroll down to see new part */
		w = (GtkWidget *) g_object_get_data (G_OBJECT (button), "scrolled-window");
		if (w) {
			GtkAdjustment *adjustment;

			adjustment = gtk_scrolled_window_get_vadjustment (
				GTK_SCROLLED_WINDOW (w));

			if (adjustment) {
				gdouble upper;

				upper = gtk_adjustment_get_upper (adjustment);
				gtk_adjustment_set_value (adjustment, upper);
			}

			ensure_scrolled_height (GTK_SCROLLED_WINDOW (w));
		}
	}
}

static void
ensure_scrolled_height (GtkScrolledWindow *scrolled_window)
{
	GtkWidget *toplevel;
	GdkScreen *screen;
	gint toplevel_height, scw_height, require_scw_height = 0, max_height;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (scrolled_window));
	if (!toplevel || !gtk_widget_is_toplevel (toplevel))
		return;

	scw_height = gtk_widget_get_allocated_height (GTK_WIDGET (scrolled_window));

	gtk_widget_get_preferred_height_for_width (gtk_bin_get_child (GTK_BIN (scrolled_window)),
		gtk_widget_get_allocated_width (GTK_WIDGET (scrolled_window)),
		&require_scw_height, NULL);

	if (scw_height >= require_scw_height) {
		if (require_scw_height > 0)
			gtk_scrolled_window_set_min_content_height (scrolled_window, require_scw_height);
		return;
	}

	if (!GTK_IS_WINDOW (toplevel) ||
	    !gtk_widget_get_window (toplevel))
		return;

	screen = gtk_window_get_screen (GTK_WINDOW (toplevel));
	if (screen) {
		gint monitor;
		GdkRectangle workarea;

		monitor = gdk_screen_get_monitor_at_window (screen, gtk_widget_get_window (toplevel));
		if (monitor < 0)
			monitor = 0;

		gdk_screen_get_monitor_workarea (screen, monitor, &workarea);

		/* can enlarge up to 4 / 5 monitor's work area height */
		max_height = workarea.height * 4 / 5;
	} else {
		return;
	}

	toplevel_height = gtk_widget_get_allocated_height (toplevel);
	if (toplevel_height + require_scw_height - scw_height > max_height)
		return;

	gtk_scrolled_window_set_min_content_height (scrolled_window, require_scw_height);
}

static void
ensure_scrolled_height_cb (GtkAdjustment *adj,
                           GParamSpec *param_spec,
                           GtkScrolledWindow *scrolled_window)
{
	ensure_scrolled_height (scrolled_window);
}

static GtkWidget *
get_widget (EFilterRule *fr,
            ERuleContext *rc)
{
	GtkWidget *widget, *add, *label;
	GtkWidget *parts, *inframe, *w;
	GtkWidget *scrolledwindow;
	GtkGrid *hgrid;
	GtkAdjustment *hadj, *vadj;
	GList *l;
	EFilterPart *part;
	struct _rule_data *data;
	EMFilterRule *ff =(EMFilterRule *) fr;
	gint rows, i = 0;
	gchar *msg;

	widget = E_FILTER_RULE_CLASS (em_filter_rule_parent_class)->
		get_widget (fr, rc);

	/* and now for the action area */
	msg = g_strdup_printf ("<b>%s</b>", _("Then"));
	label = gtk_label_new (msg);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_container_add (GTK_CONTAINER (widget), label);
	g_free (msg);

	hgrid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_column_spacing (hgrid, 3);
	gtk_widget_set_hexpand (GTK_WIDGET (hgrid), TRUE);
	gtk_widget_set_halign (GTK_WIDGET (hgrid), GTK_ALIGN_FILL);
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (hgrid));

	label = gtk_label_new ("");
	gtk_grid_attach (hgrid, label, 0, 0, 1, 1);

	inframe = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (inframe), 6);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (inframe), GTK_ORIENTATION_VERTICAL);
	gtk_widget_set_hexpand (inframe, TRUE);
	gtk_widget_set_halign (inframe, GTK_ALIGN_FILL);
	gtk_widget_set_vexpand (inframe, TRUE);
	gtk_widget_set_valign (inframe, GTK_ALIGN_FILL);

	gtk_grid_attach_next_to (hgrid, inframe, label, GTK_POS_RIGHT, 1, 1);

	rows = g_list_length (ff->actions);
	parts = gtk_table_new (rows, 2, FALSE);
	data = g_malloc0 (sizeof (*data));
	data->f =(EMFilterContext *) rc;
	data->fr = fr;
	data->parts = parts;

	/* only set to automatically clean up the memory */
	g_object_set_data_full ((GObject *) hgrid, "data", data, g_free);

	l = ff->actions;
	while (l) {
		part = l->data;
		d (printf ("adding action %s\n", part->title));
		w = get_rule_part_widget ((EMFilterContext *) rc, part, fr);
		attach_rule (w, data, part, i++);
		l = l->next;
	}

	hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 1.0, 1.0 ,1.0, 1.0));
	vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 1.0, 1.0 ,1.0, 1.0));
	scrolledwindow = gtk_scrolled_window_new (hadj, vadj);

	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolledwindow),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (
		GTK_SCROLLED_WINDOW (scrolledwindow), parts);

	gtk_widget_set_hexpand (scrolledwindow, TRUE);
	gtk_widget_set_halign (scrolledwindow, GTK_ALIGN_FILL);
	gtk_widget_set_vexpand (scrolledwindow, TRUE);
	gtk_widget_set_valign (scrolledwindow, GTK_ALIGN_FILL);

	gtk_container_add (GTK_CONTAINER (inframe), scrolledwindow);

	hgrid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_column_spacing (hgrid, 3);

	add = e_dialog_button_new_with_icon ("list-add", _("Add Ac_tion"));
	g_signal_connect (
		add, "clicked",
		G_CALLBACK (more_parts), data);
	gtk_grid_attach (hgrid, add, 0, 0, 1, 1);

	gtk_container_add (GTK_CONTAINER (inframe), GTK_WIDGET (hgrid));

	g_object_set_data (G_OBJECT (add), "scrolled-window", scrolledwindow);

	e_signal_connect_notify (
		vadj, "notify::upper",
		G_CALLBACK (ensure_scrolled_height_cb), scrolledwindow);

	g_signal_connect (scrolledwindow, "map", G_CALLBACK (ensure_scrolled_height), NULL);

	gtk_widget_show_all (widget);

	return widget;
}
