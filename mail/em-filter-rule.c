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
 *		Not Zed <notzed@ximian.com>
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "em-filter-rule.h"
#include "em-filter-context.h"

#define d(x)

static gint validate(FilterRule *fr);
static gint filter_eq(FilterRule *fr, FilterRule *cm);
static xmlNodePtr xml_encode(FilterRule *fr);
static gint xml_decode(FilterRule *fr, xmlNodePtr, RuleContext *rc);
static void rule_copy(FilterRule *dest, FilterRule *src);
/*static void build_code(FilterRule *, GString *out);*/
static GtkWidget *get_widget(FilterRule *fr, RuleContext *rc);

static void em_filter_rule_class_init(EMFilterRuleClass *klass);
static void em_filter_rule_init(EMFilterRule *ff);
static void em_filter_rule_finalise(GObject *obj);

static FilterRuleClass *parent_class = NULL;

GType
em_filter_rule_get_type(void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof(EMFilterRuleClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) em_filter_rule_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(EMFilterRule),
			0,    /* n_preallocs */
			(GInstanceInitFunc)em_filter_rule_init,
		};

		type = g_type_register_static(FILTER_TYPE_RULE, "EMFilterRule", &info, 0);
	}

	return type;
}

static void
em_filter_rule_class_init(EMFilterRuleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	FilterRuleClass *fr_class =(FilterRuleClass *)klass;

	parent_class = g_type_class_ref(FILTER_TYPE_RULE);

	object_class->finalize = em_filter_rule_finalise;

	/* override methods */
	fr_class->validate = validate;
	fr_class->eq = filter_eq;
	fr_class->xml_encode = xml_encode;
	fr_class->xml_decode = xml_decode;
	/*fr_class->build_code = build_code;*/
	fr_class->copy = rule_copy;
	fr_class->get_widget = get_widget;
}

static void
em_filter_rule_init(EMFilterRule *ff)
{
	;
}

static void
unref_list(GList *l)
{
	while (l) {
		g_object_unref(l->data);
		l = l->next;
	}
}

static void
em_filter_rule_finalise(GObject *obj)
{
	EMFilterRule *ff =(EMFilterRule *) obj;

	unref_list(ff->actions);
	g_list_free(ff->actions);

        G_OBJECT_CLASS(parent_class)->finalize(obj);
}

/**
 * em_filter_rule_new:
 *
 * Create a new EMFilterRule object.
 *
 * Return value: A new #EMFilterRule object.
 **/
EMFilterRule *
em_filter_rule_new(void)
{
	return (EMFilterRule *)g_object_new(em_filter_rule_get_type(), NULL, NULL);
}

void
em_filter_rule_add_action(EMFilterRule *fr, FilterPart *fp)
{
	fr->actions = g_list_append(fr->actions, fp);

	filter_rule_emit_changed((FilterRule *)fr);
}

void
em_filter_rule_remove_action(EMFilterRule *fr, FilterPart *fp)
{
	fr->actions = g_list_remove(fr->actions, fp);

	filter_rule_emit_changed((FilterRule *)fr);
}

void
em_filter_rule_replace_action(EMFilterRule *fr, FilterPart *fp, FilterPart *new)
{
	GList *l;

	l = g_list_find(fr->actions, fp);
	if (l) {
		l->data = new;
	} else {
		fr->actions = g_list_append(fr->actions, new);
	}

	filter_rule_emit_changed((FilterRule *)fr);
}

void
em_filter_rule_build_action(EMFilterRule *fr, GString *out)
{
	g_string_append(out, "(begin\n");
	filter_part_build_code_list(fr->actions, out);
	g_string_append(out, ")\n");
}

static gint
validate(FilterRule *fr)
{
	EMFilterRule *ff =(EMFilterRule *)fr;
	GList *parts;
	gint valid;

        valid = FILTER_RULE_CLASS(parent_class)->validate(fr);

	/* validate rule actions */
	parts = ff->actions;
	while (parts && valid) {
		valid = filter_part_validate((FilterPart *)parts->data);
		parts = parts->next;
	}

	return valid;
}

static gint
list_eq(GList *al, GList *bl)
{
	gint truth = TRUE;

	while (truth && al && bl) {
		FilterPart *a = al->data, *b = bl->data;

		truth = filter_part_eq(a, b);
		al = al->next;
		bl = bl->next;
	}

	return truth && al == NULL && bl == NULL;
}

static gint
filter_eq(FilterRule *fr, FilterRule *cm)
{
        return FILTER_RULE_CLASS(parent_class)->eq(fr, cm)
		&& list_eq(((EMFilterRule *)fr)->actions,((EMFilterRule *)cm)->actions);
}

static xmlNodePtr
xml_encode(FilterRule *fr)
{
	EMFilterRule *ff =(EMFilterRule *)fr;
	xmlNodePtr node, set, work;
	GList *l;

        node = FILTER_RULE_CLASS(parent_class)->xml_encode(fr);
	g_return_val_if_fail (node != NULL, NULL);
	set = xmlNewNode(NULL, (const guchar *)"actionset");
	xmlAddChild(node, set);
	l = ff->actions;
	while (l) {
		work = filter_part_xml_encode((FilterPart *)l->data);
		xmlAddChild(set, work);
		l = l->next;
	}

	return node;

}

static void
load_set(xmlNodePtr node, EMFilterRule *ff, RuleContext *rc)
{
	xmlNodePtr work;
	gchar *rulename;
	FilterPart *part;

	work = node->children;
	while (work) {
		if (!strcmp((gchar *)work->name, "part")) {
			rulename = (gchar *)xmlGetProp(work, (const guchar *)"name");
			part = em_filter_context_find_action((EMFilterContext *)rc, rulename);
			if (part) {
				part = filter_part_clone(part);
				filter_part_xml_decode(part, work);
				em_filter_rule_add_action(ff, part);
			} else {
				g_warning("cannot find rule part '%s'\n", rulename);
			}
			xmlFree(rulename);
		} else if (work->type == XML_ELEMENT_NODE) {
			g_warning("Unknown xml node in part: %s", work->name);
		}
		work = work->next;
	}
}

static gint
xml_decode(FilterRule *fr, xmlNodePtr node, RuleContext *rc)
{
	EMFilterRule *ff =(EMFilterRule *)fr;
	xmlNodePtr work;
	gint result;

        result = FILTER_RULE_CLASS(parent_class)->xml_decode(fr, node, rc);
	if (result != 0)
		return result;

	work = node->children;
	while (work) {
		if (!strcmp((gchar *)work->name, "actionset")) {
			load_set(work, ff, rc);
		}
		work = work->next;
	}

	return 0;
}

static void
rule_copy(FilterRule *dest, FilterRule *src)
{
	EMFilterRule *fdest, *fsrc;
	GList *node;

	fdest =(EMFilterRule *)dest;
	fsrc =(EMFilterRule *)src;

	if (fdest->actions) {
		g_list_foreach(fdest->actions, (GFunc)g_object_unref, NULL);
		g_list_free(fdest->actions);
		fdest->actions = NULL;
	}

	node = fsrc->actions;
	while (node) {
		FilterPart *part = node->data;

		g_object_ref(part);
		fdest->actions = g_list_append(fdest->actions, part);
		node = node->next;
	}

	FILTER_RULE_CLASS(parent_class)->copy(dest, src);
}

/*static void build_code(FilterRule *fr, GString *out)
{
        return FILTER_RULE_CLASS(parent_class)->build_code(fr, out);
}*/

struct _part_data {
	FilterRule *fr;
	EMFilterContext *f;
	FilterPart *part;
	GtkWidget *partwidget, *container;
};

static void
part_combobox_changed (GtkComboBox *combobox, struct _part_data *data)
{
	FilterPart *part = NULL;
	FilterPart *newpart;
	gint index, i;

	index = gtk_combo_box_get_active (combobox);
	for (i = 0, part = em_filter_context_next_action (data->f, part); part && i < index; i++, part = em_filter_context_next_action (data->f, part)) {
		/* traverse until reached index */
	}

	g_return_if_fail (part != NULL);
	g_return_if_fail (i == index);

	/* dont update if we haven't changed */
	if (!strcmp (part->title, data->part->title))
		return;

	/* here we do a widget shuffle, throw away the old widget/rulepart,
	   and create another */
	if (data->partwidget)
		gtk_container_remove (GTK_CONTAINER (data->container), data->partwidget);

	newpart = filter_part_clone (part);
	filter_part_copy_values (newpart, data->part);
	em_filter_rule_replace_action ((EMFilterRule *)data->fr, data->part, newpart);
	g_object_unref (data->part);
	data->part = newpart;
	data->partwidget = filter_part_get_widget (newpart);
	if (data->partwidget)
		gtk_box_pack_start (GTK_BOX (data->container), data->partwidget, TRUE, TRUE, 0);
}

static GtkWidget *
get_rule_part_widget(EMFilterContext *f, FilterPart *newpart, FilterRule *fr)
{
	FilterPart *part = NULL;
	GtkWidget *combobox;
	GtkWidget *hbox;
	GtkWidget *p;
	gint index = 0, current = 0;
	struct _part_data *data;

	data = g_malloc0(sizeof(*data));
	data->fr = fr;
	data->f = f;
	data->part = newpart;

	hbox = gtk_hbox_new(FALSE, 0);
	/* only set to automatically clean up the memory and for less_parts */
	g_object_set_data_full ((GObject *) hbox, "data", data, g_free);

	p = filter_part_get_widget(newpart);

	data->partwidget = p;
	data->container = hbox;

	combobox = gtk_combo_box_new_text ();
	while ((part = em_filter_context_next_action(f, part))) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _(part->title));

		if (!strcmp(newpart->title, part->title))
			current = index;

		index++;
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), current);
	g_signal_connect (combobox, "changed", G_CALLBACK (part_combobox_changed), data);
	gtk_widget_show (combobox);

	gtk_box_pack_start(GTK_BOX(hbox), combobox, FALSE, FALSE, 0);
	if (p)
		gtk_box_pack_start(GTK_BOX(hbox), p, TRUE, TRUE, 0);

	gtk_widget_show_all(hbox);

	return hbox;
}

struct _rule_data {
	FilterRule *fr;
	EMFilterContext *f;
	GtkWidget *parts;
};

static void
less_parts(GtkWidget *button, struct _rule_data *data)
{
	FilterPart *part;
	GtkWidget *rule;
	struct _part_data *part_data;
	GList *l;

	l =((EMFilterRule *)data->fr)->actions;
	if (g_list_length(l) < 2)
		return;

	rule = g_object_get_data((GObject *)button, "rule");
	part_data = g_object_get_data ((GObject *) rule, "data");

	g_return_if_fail (part_data != NULL);

	part = part_data->part;

	/* remove the part from the list */
	em_filter_rule_remove_action((EMFilterRule *)data->fr, part);
	g_object_unref(part);

	/* and from the display */
	gtk_container_remove(GTK_CONTAINER(data->parts), rule);
	gtk_container_remove(GTK_CONTAINER(data->parts), button);
}

static void
attach_rule(GtkWidget *rule, struct _rule_data *data, FilterPart *part, gint row)
{
	GtkWidget *remove;

	gtk_table_attach(GTK_TABLE(data->parts), rule, 0, 1, row, row + 1,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);

	remove = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
	g_object_set_data((GObject *)remove, "rule", rule);
	/*gtk_button_set_relief(GTK_BUTTON(remove), GTK_RELIEF_NONE);*/
	g_signal_connect(remove, "clicked", G_CALLBACK(less_parts), data);
	gtk_table_attach(GTK_TABLE(data->parts), remove, 1, 2, row, row + 1,
			  0, 0, 0, 0);
	gtk_widget_show(remove);
}

static void
do_grab_focus_cb (GtkWidget *widget, gpointer data)
{
	gboolean *done = (gboolean *) data;

	if (*done)
		return;

	if (widget && GTK_WIDGET_CAN_FOCUS (widget)) {
		*done = TRUE;
		gtk_widget_grab_focus (widget);
	}
}

static void
more_parts(GtkWidget *button, struct _rule_data *data)
{
	FilterPart *new;

	/* create a new rule entry, use the first type of rule */
	new = em_filter_context_next_action((EMFilterContext *)data->f, NULL);
	if (new) {
		GtkWidget *w;
		guint16 rows;

		new = filter_part_clone(new);
		em_filter_rule_add_action((EMFilterRule *)data->fr, new);
		w = get_rule_part_widget(data->f, new, data->fr);

		rows = GTK_TABLE(data->parts)->nrows;
		gtk_table_resize(GTK_TABLE(data->parts), rows + 1, 2);
		attach_rule(w, data, new, rows);

		if (GTK_IS_CONTAINER (w)) {
			gboolean done = FALSE;

			gtk_container_foreach (GTK_CONTAINER (w), do_grab_focus_cb, &done);
		} else
			gtk_widget_grab_focus (w);

		/* also scroll down to see new part */
		w = (GtkWidget*) g_object_get_data (G_OBJECT (button), "scrolled-window");
		if (w) {
			GtkAdjustment *adjustment;

			adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (w));
			if (adjustment)
				gtk_adjustment_set_value (adjustment, adjustment->upper);

		}
	}
}

static GtkWidget *
get_widget(FilterRule *fr, RuleContext *rc)
{
	GtkWidget *widget, *hbox, *add, *label;
	GtkWidget *parts, *inframe, *w;
	GtkWidget *scrolledwindow;
	GtkObject *hadj, *vadj;
	GList *l;
	FilterPart *part;
	struct _rule_data *data;
	EMFilterRule *ff =(EMFilterRule *)fr;
	gint rows, i = 0;
	gchar *msg;

        widget = FILTER_RULE_CLASS(parent_class)->get_widget(fr, rc);

	/* and now for the action area */
	msg = g_strdup_printf("<b>%s</b>", _("Then"));
	label = gtk_label_new(msg);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(widget), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	g_free(msg);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(widget), hbox, TRUE, TRUE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	inframe = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), inframe, TRUE, TRUE, 0);

	rows = g_list_length(ff->actions);
	parts = gtk_table_new(rows, 2, FALSE);
	data = g_malloc0(sizeof(*data));
	data->f =(EMFilterContext *)rc;
	data->fr = fr;
	data->parts = parts;

	/* only set to automatically clean up the memory */
	g_object_set_data_full ((GObject *) hbox, "data", data, g_free);

	hbox = gtk_hbox_new(FALSE, 3);

	add = gtk_button_new_with_mnemonic (_("Add Ac_tion"));
	gtk_button_set_image (GTK_BUTTON (add), gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
	/* gtk_button_set_relief(GTK_BUTTON(add), GTK_RELIEF_NONE); */
	g_signal_connect(add, "clicked", G_CALLBACK(more_parts), data);
	gtk_box_pack_start(GTK_BOX(hbox), add, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(inframe), hbox, FALSE, FALSE, 3);

	l = ff->actions;
	while (l) {
		part = l->data;
		d(printf("adding action %s\n", part->title));
		w = get_rule_part_widget((EMFilterContext *)rc, part, fr);
		attach_rule(w, data, part, i++);
		l = l->next;
	}

	hadj = gtk_adjustment_new(0.0, 0.0, 1.0, 1.0 ,1.0, 1.0);
	vadj = gtk_adjustment_new(0.0, 0.0, 1.0, 1.0 ,1.0, 1.0);
	scrolledwindow = gtk_scrolled_window_new(GTK_ADJUSTMENT(hadj), GTK_ADJUSTMENT(vadj));
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolledwindow), parts);

	gtk_box_pack_start(GTK_BOX(inframe), scrolledwindow, TRUE, TRUE, 0);

	/*gtk_box_pack_start(GTK_BOX(inframe), parts, FALSE, FALSE, 3);*/

	g_object_set_data (G_OBJECT (add), "scrolled-window", scrolledwindow);

	gtk_widget_show_all(widget);

	return widget;
}
