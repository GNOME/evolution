
#include <gnome.h>
#include "filter-editor.h"

int main(int argc, char **argv)
{
	GList *rules, *options, *options2;
	xmlDocPtr doc, out, optionset, filteroptions;
	GString *s;
	GtkWidget *w;

	gnome_init("Test", "0.0", argc, argv);
	gdk_rgb_init ();
	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual (gdk_rgb_get_visual ());

	w = filter_editor_new();

	doc = xmlParseFile("filterdescription.xml");
	rules = filter_load_ruleset(doc);
	options = filter_load_optionset(doc, rules);
	options2 = options;
	out = xmlParseFile("saveoptions.xml");
	options = filter_load_optionset(out, rules);
	
	filter_editor_set_rules(w, rules, options2, options);

	gtk_widget_show(w);
	gtk_main();
}
