/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <gtk/gtk.h>
#include <gnome.h>

#include "score-rule.h"

static xmlNodePtr xml_encode(FilterRule *);
static int xml_decode(FilterRule *, xmlNodePtr, struct _RuleContext *f);
/*static void build_code(FilterRule *, GString *out);*/
static GtkWidget *get_widget(FilterRule *fr, struct _RuleContext *f);

static void score_rule_class_init	(ScoreRuleClass *class);
static void score_rule_init	(ScoreRule *gspaper);
static void score_rule_finalise	(GtkObject *obj);

#define _PRIVATE(x) (((ScoreRule *)(x))->priv)

struct _ScoreRulePrivate {
};

static FilterRuleClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
score_rule_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"ScoreRule",
			sizeof(ScoreRule),
			sizeof(ScoreRuleClass),
			(GtkClassInitFunc)score_rule_class_init,
			(GtkObjectInitFunc)score_rule_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(filter_rule_get_type (), &type_info);
	}
	
	return type;
}

static void
score_rule_class_init (ScoreRuleClass *class)
{
	GtkObjectClass *object_class;
	FilterRuleClass *rule_class = (FilterRuleClass *)class;

	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(filter_rule_get_type ());

	object_class->finalize = score_rule_finalise;

	/* override methods */
	rule_class->xml_encode = xml_encode;
	rule_class->xml_decode = xml_decode;
/*	rule_class->build_code = build_code;*/
	rule_class->get_widget = get_widget;

	/* signals */

	gtk_object_class_add_signals(object_class, signals, LAST_SIGNAL);
}

static void
score_rule_init (ScoreRule *o)
{
	o->priv = g_malloc0(sizeof(*o->priv));
}

static void
score_rule_finalise(GtkObject *obj)
{
	ScoreRule *o = (ScoreRule *)obj;

	o = o;

        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * score_rule_new:
 *
 * Create a new ScoreRule object.
 * 
 * Return value: A new #ScoreRule object.
 **/
ScoreRule *
score_rule_new(void)
{
	ScoreRule *o = (ScoreRule *)gtk_type_new(score_rule_get_type ());
	return o;
}

static xmlNodePtr xml_encode(FilterRule *fr)
{
	ScoreRule *sr = (ScoreRule *)fr;
	xmlNodePtr node, value;
	char number[16];

	node = ((FilterRuleClass *)(parent_class))->xml_encode(fr);
	sprintf(number, "%d", sr->score);
	value = xmlNewNode(NULL, "score");
	xmlSetProp(value, "value", number);
	xmlAddChild(node, value);
	return node;
}

static int xml_decode(FilterRule *fr, xmlNodePtr node, struct _RuleContext *f)
{
	ScoreRule *sr = (ScoreRule *)fr;
	xmlNodePtr value;
	int result;
	char *str;

	result = ((FilterRuleClass *)(parent_class))->xml_decode(fr, node, f);
	if (result != 0)
		return result;
	value = node->childs;
	while (value) {
		if (!strcmp(value->name, "score")) {
			str = xmlGetProp(value, "value");
			sscanf(str, "%d", &sr->score);
		}
		value = value->next;
	}
	return 0;
}

/*static void build_code(FilterRule *fr, GString *out)
{
}*/

static void spin_changed(GtkAdjustment *adj, ScoreRule *sr)
{
	sr->score = adj->value;
}

static GtkWidget *get_widget(FilterRule *fr, struct _RuleContext *f)
{
	GtkWidget *widget;
	GtkFrame *frame;
	GtkLabel *label;
	GtkHBox *hbox;
	GtkAdjustment *adj;
	ScoreRule *sr = (ScoreRule *)fr;
	GtkSpinButton *spin;

        widget = ((FilterRuleClass *)(parent_class))->get_widget(fr, f);
	frame = (GtkFrame *)gtk_frame_new("Score");
	hbox = (GtkHBox *)gtk_hbox_new(FALSE, 3);
	label = (GtkLabel *)gtk_label_new("Score");
	gtk_box_pack_start((GtkBox *)hbox, (GtkWidget *)label, FALSE, FALSE, 3);
	adj = (GtkAdjustment *)gtk_adjustment_new((float)sr->score, -100, 100, 1, 10, 10);
	gtk_signal_connect((GtkObject *)adj, "value_changed", spin_changed, sr);
	spin = (GtkSpinButton *)gtk_spin_button_new(adj, 1.0, 0);
	gtk_box_pack_start((GtkBox *)hbox, (GtkWidget *)spin, FALSE, FALSE, 3);
	gtk_container_add((GtkContainer *)frame, (GtkWidget *)hbox);
	gtk_widget_show_all((GtkWidget *)frame);

	gtk_box_pack_start((GtkBox *)widget, (GtkWidget *)frame, FALSE, FALSE, 3);
	return widget;
}
