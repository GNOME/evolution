/*
 *  Copyright (C) 2001 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *           Jeffrey Stedfast <fejj@helixcode.com>
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

#include <config.h>

#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include <gal/widgets/e-unicode.h>

#include "score-editor.h"
#include "score-context.h"
#include "score-rule.h"

#define d(x)

static FilterRule * create_rule(RuleEditor *re);

static void score_editor_class_init (ScoreEditorClass *class);
static void score_editor_init	(ScoreEditor *gspaper);
static void score_editor_finalise (GtkObject *obj);

#define _PRIVATE(x) (((ScoreEditor *)(x))->priv)

struct _ScoreEditorPrivate {
};

static RuleEditorClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
score_editor_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"ScoreEditor",
			sizeof(ScoreEditor),
			sizeof(ScoreEditorClass),
			(GtkClassInitFunc)score_editor_class_init,
			(GtkObjectInitFunc)score_editor_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique (rule_editor_get_type (), &type_info);
	}
	
	return type;
}

static void
score_editor_class_init (ScoreEditorClass *class)
{
	GtkObjectClass *object_class = (GtkObjectClass *)class;
	RuleEditorClass *re_class = (RuleEditorClass *)class;

	parent_class = gtk_type_class (rule_editor_get_type ());
	
	object_class->finalize = score_editor_finalise;

	/* override methods */
	re_class->create_rule = create_rule;

	/* signals */
	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
score_editor_init (ScoreEditor *o)
{
	o->priv = g_malloc0 (sizeof (*o->priv));
}

static void
score_editor_finalise(GtkObject *obj)
{
	ScoreEditor *o = (ScoreEditor *)obj;

	g_free(o->priv);

        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * score_editor_new:
 *
 * Create a new ScoreEditor object.
 * 
 * Return value: A new #ScoreEditor object.
 **/
ScoreEditor *
score_editor_new(ScoreContext *f)
{
	GladeXML *gui;
	ScoreEditor *o = (ScoreEditor *)gtk_type_new (score_editor_get_type ());
	GtkWidget *w;

	gui = glade_xml_new(FILTER_GLADEDIR "/filter.glade", "rule_editor");
	rule_editor_construct((RuleEditor *)o, (RuleContext *)f, gui, NULL);

        w = glade_xml_get_widget(gui, "rule_frame");
	gtk_frame_set_label((GtkFrame *)w, _("Score Rules"));

	gtk_object_unref((GtkObject *)gui);
	
	return o;
}

static FilterRule *
create_rule(RuleEditor *re)
{
	FilterRule *rule = filter_rule_new();
	FilterPart *part;

	/* create a rule with 1 part in it */
	rule = (FilterRule *)score_rule_new ();
	part = rule_context_next_part(re->context, NULL);
	filter_rule_add_part(rule, filter_part_clone(part));

	return rule;
}
