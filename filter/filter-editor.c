/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
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

#include "filter-druid.h"
#include "filter-editor.h"


static void filter_editor_class_init (FilterEditorClass *klass);
static void filter_editor_init       (FilterEditor *obj);

static GnomeDialogClass *filter_editor_parent;

#define _PRIVATE(x) (((FilterEditor *)(x))->priv)

struct _FilterEditorPrivate {
	FilterDruid *druid;

	GtkWidget *edit, *add, *remove, *up, *down;

	/* for sub-druid */
	struct filter_option *druid_option;
	GtkWidget *druid_dialogue;
	FilterDruid *druid_druid;
};

enum SIGNALS {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_editor_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterEditor",
			sizeof (FilterEditor),
			sizeof (FilterEditorClass),
			(GtkClassInitFunc) filter_editor_class_init,
			(GtkObjectInitFunc) filter_editor_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gnome_dialog_get_type (), &type_info);
	}
	
	return type;
}

static void
object_destroy(FilterEditor *obj)
{
	struct _FilterEditorPrivate *p = _PRIVATE(obj);

	if (p->druid_druid)
		gtk_object_unref((GtkObject *)p->druid_dialogue);

	GTK_OBJECT_CLASS(filter_editor_parent)->destroy(obj);
}

static void
filter_editor_class_init (FilterEditorClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	
	filter_editor_parent = gtk_type_class (gnome_dialog_get_type ());

	object_class->destroy = object_destroy;

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
filter_editor_init (FilterEditor *obj)
{
	obj->priv = g_malloc0(sizeof(*obj->priv));
}


static void
sensitise(FilterEditor *e)
{
	struct _FilterEditorPrivate *p = _PRIVATE(e);

	gtk_widget_set_sensitive(p->add, TRUE);
	gtk_widget_set_sensitive(p->edit, e->option_current != NULL);
	gtk_widget_set_sensitive(p->remove, e->option_current != NULL);
	gtk_widget_set_sensitive(p->up, g_list_index(e->useroptions, e->option_current)>0);
	gtk_widget_set_sensitive(p->down, g_list_index(e->useroptions, e->option_current)!=g_list_length(e->useroptions)-1);
}

static void
druid_option_selected(FilterDruid *f, struct filter_option *option, FilterEditor *e)
{
	printf("option selected: %p\n", option);
	e->option_current = option;
	sensitise(e);
}

static void
druid_dialogue_clicked(GnomeDialog *d, int button, FilterEditor *e)
{
	struct _FilterEditorPrivate *p = _PRIVATE(e);
	int page = filter_druid_get_page(p->druid_druid);

	switch(button) {
	case 1:
		if (page<4) {
			page++;
		}
		break;
	case 0:
		if (page>0) {
			page--;
		}
		break;
	case 2:
		printf("Finish!\n");
		if (p->druid_druid->option_current) {
			struct filrt_optionrule *or;

			printf("refcount = %d\n", ((GtkObject *)p->druid_druid)->ref_count);

			or = p->druid_druid->option_current;
			if (p->druid_option) {
				GList *node;

				node = g_list_find(e->useroptions, p->druid_option);
				if (node) {
					/* fixme: free old one */
					node->data = or;
				} else {
					g_warning("Cannot find node I edited, appending instead");
					e->useroptions = g_list_append(e->useroptions, or);
				}
			} else {
				e->useroptions = g_list_append(e->useroptions, or);
			}
			filter_druid_set_rules(p->druid, e->useroptions, e->rules, or);
		}
	case 3:
		printf("cancel!\n");
		p->druid_dialogue = NULL;
		gnome_dialog_close(d);
		return;
	}
	filter_druid_set_page(p->druid_druid, page);

	gnome_dialog_set_sensitive(p->druid_dialogue, 0, page>0);
	gnome_dialog_set_sensitive(p->druid_dialogue, 1, page<4);
	gnome_dialog_set_sensitive(p->druid_dialogue, 2, page==4); /* FIXME: make this depenedant on when the rules are actually done */	
}

static void
druid_dialogue_option_selected(FilterDruid *f, struct filter_option *option, FilterEditor *e)
{
	struct _FilterEditorPrivate *p = _PRIVATE(e);

	gnome_dialog_set_sensitive(p->druid_dialogue, 1, TRUE);
}

static void
add_or_edit(FilterEditor *e, struct filter_option *option)
{
	GnomeDialog *dialogue;
	FilterDruid *druid;
	struct _FilterEditorPrivate *p = _PRIVATE(e);

	if (p->druid_dialogue) {
		gdk_window_raise(GTK_WIDGET(p->druid_dialogue)->window);
		return;
	}

	dialogue = gnome_dialog_new(option?"Edit Filter":"Create filter", "Prev", "Next", "Finish", "Cancel", 0);

	p->druid_dialogue = dialogue;

	gnome_dialog_set_close(dialogue, FALSE);
	gnome_dialog_set_sensitive(dialogue, 0, FALSE);
	gnome_dialog_set_sensitive(dialogue, 1, FALSE);
	gnome_dialog_set_sensitive(dialogue, 2, FALSE);
	gnome_dialog_set_default(dialogue, 1);

	gtk_signal_connect(GTK_OBJECT(dialogue), "clicked", druid_dialogue_clicked, e);

	druid = filter_druid_new();

	p->druid_druid = druid;

	filter_druid_set_default_html(p->druid_druid, "<h2>Create Filtering Rule</h2>"
				      "<p>Select one of the base rules above, then continue "
				      "forwards to customise it.</p>");

	filter_druid_set_rules(druid, e->systemoptions, e->rules, option);
	gtk_box_pack_start(dialogue->vbox, druid, TRUE, TRUE, 0);

	if (option) {
		druid_dialogue_clicked(dialogue, 1, e);
	}

	p->druid_option = option;

	gtk_signal_connect(druid, "option_selected", druid_dialogue_option_selected, e);

	gtk_widget_show(druid);
	gtk_widget_show(dialogue);
}

static void
add_clicked(GtkWidget *w, FilterEditor *e)
{
	printf("add new ...\n");

	add_or_edit(e, NULL);
}

static void
edit_clicked(GtkWidget *w, FilterEditor *e)
{
	printf("add new ...\n");

	add_or_edit(e, e->option_current);
}

static void
remove_clicked(GtkWidget *w, FilterEditor *e)
{
	printf("remove current ...\n");
}

static void
up_clicked(GtkWidget *w, FilterEditor *e)
{
	printf("up ...\n");
}

static void
down_clicked(GtkWidget *w, FilterEditor *e)
{
	printf("down ...\n");
}

/* build the contents of the editor */
static void
build_editor(FilterEditor *e)
{
	struct _FilterEditorPrivate *p = _PRIVATE(e);
	GtkWidget *hbox;
	GtkWidget *vbox;

	hbox = gtk_hbox_new(FALSE, 3);

	p->druid = (GtkWidget *)filter_druid_new();
	gtk_box_pack_start((GtkBox *)hbox, p->druid, TRUE, TRUE, 0);

	vbox = gtk_vbox_new(FALSE, 0);
	
	p->edit = gtk_button_new_with_label ("Edit");
	p->add = gtk_button_new_with_label ("Add");
	p->remove = gtk_button_new_with_label ("Remove");
	p->up = gtk_button_new_with_label ("Up");
	p->down = gtk_button_new_with_label ("Down");

	gtk_box_pack_start((GtkBox *)vbox, p->edit, FALSE, TRUE, 0);
	gtk_box_pack_start((GtkBox *)vbox, p->add, FALSE, TRUE, 3);
	gtk_box_pack_start((GtkBox *)vbox, p->remove, FALSE, TRUE, 0);
	gtk_box_pack_start((GtkBox *)vbox, p->up, FALSE, TRUE, 3);
	gtk_box_pack_start((GtkBox *)vbox, p->down, FALSE, TRUE, 0);

	gtk_box_pack_start((GtkBox *)hbox, vbox, FALSE, FALSE, 0);

	gtk_box_pack_start((GtkBox *)e->parent.vbox, hbox, TRUE, TRUE, 0);

	gtk_signal_connect(p->druid, "option_selected", druid_option_selected, e);

	gtk_signal_connect(p->edit, "clicked", edit_clicked, e);
	gtk_signal_connect(p->add, "clicked", add_clicked, e);
	gtk_signal_connect(p->remove, "clicked", remove_clicked, e);
	gtk_signal_connect(p->up, "clicked", up_clicked, e);
	gtk_signal_connect(p->down, "clicked", down_clicked, e);

	filter_druid_set_default_html(p->druid, "<h2>Filtering Rules</h2>"
				      "<p>Select one of the rules above to <i>view</i>, and "
				      "<i>edit</i>.  Or <i>Add</i> a new rule.</p>");

	gtk_widget_show_all(hbox);
	sensitise(e);
}


/**
 * filter_editor_new:
 *
 * Create a new FilterEditor object.
 * 
 * Return value: A new FilterEditor widget.
 **/
FilterEditor *
filter_editor_new (void)
{
	FilterEditor *new = FILTER_EDITOR ( gtk_type_new (filter_editor_get_type ()));

	build_editor(new);

	return new;
}

void
filter_editor_set_rules(FilterEditor *e, GList *rules, GList *systemoptions, GList *useroptions)
{
	struct _FilterEditorPrivate *p = _PRIVATE(e);

	e->rules= rules;
	e->systemoptions = systemoptions;
	e->useroptions = useroptions;

	filter_druid_set_rules(p->druid, useroptions, rules, NULL);
}

void
filter_editor_set_rule_files(FilterEditor *e, const char *systemrules, const char *userrules)
{
	GList *rules, *options, *options2;
	xmlDocPtr doc, out, optionset, filteroptions;

	doc = xmlParseFile(systemrules);
	rules = filter_load_ruleset(doc);
	options2 = filter_load_optionset(doc, rules);
	out = xmlParseFile(userrules);
	options = filter_load_optionset(out, rules);
	
	filter_editor_set_rules(e, rules, options2, options);
}

int
filter_editor_save_rules(FilterEditor *e, const char *userrules)
{
	return filter_write_optionset_file(userrules, e->useroptions);
}
