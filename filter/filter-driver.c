
#include <glib.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <gtkhtml/gtkhtml.h>

#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>

#include "filter-arg-types.h"
#include "filter-xml.h"
#include "e-sexp.h"
#include "filter-format.h"
x
extern int filter_find_arg(FilterArg *a, char *name);

#include "check.xpm"
#include "blank.xpm"

/*
  splices ${cc} lines into a single string
*/
int
expand_variables(GString *out, char *source, GList *args, GHashTable *globals)
{
	GList *argl;
	FilterArg *arg;
	char *name= alloca(32);
	char *start, *end, *newstart, *tmp, *val;
	int namelen=32;
	int len=0;
	int ok = 0;

	start = source;
	while ( (newstart = strstr(start, "${"))
		&& (end = strstr(newstart+2, "}")) ) {
		len = end-newstart-2;
		if (len+1>namelen) {
			namelen = (len+1)*2;
			name = alloca(namelen);
		}
		memcpy(name, newstart+2, len);
		name[len] = 0;
		argl = g_list_find_custom(args, name, (GCompareFunc) filter_find_arg);
		if (argl) {
			int i, count;

			tmp = g_strdup_printf("%.*s", newstart-start, start);
			printf("appending: %s\n", tmp);
			g_string_append(out, tmp);
			g_free(tmp);

			arg = argl->data;
			count = filter_arg_get_count(arg);
			for (i=0;i<count;i++) {
				printf("appending '%s'\n", filter_arg_get_value_as_string(arg, i));
				g_string_append(out, " \"");
				g_string_append(out, filter_arg_get_value_as_string(arg, i));
				g_string_append(out, "\"");
			}
		} else if ( (val = g_hash_table_lookup(globals, name)) ) {
			tmp = g_strdup_printf("%.*s", newstart-start, start);
			printf("appending: %s\n", tmp);
			g_string_append(out, tmp);
			g_free(tmp);
			g_string_append(out, " \"");
			g_string_append(out, val);
			g_string_append(out, "\"");
		} else {
			ok = 1;
			tmp = g_strdup_printf("%.*s", end-start+1, start);
			printf("appending: %s\n", tmp);
			g_string_append(out, tmp);
			g_free(tmp);
		}
		start = end+1;
	}
	g_string_append(out, start);

	return ok;
}

/*
  build an expression for the filter
*/
static void
expand_filter_option(GString *s, struct filter_option *op)
{
	GList *optionl;
	FilterArg *arg;
	GHashTable *globals;

	globals = g_hash_table_new(g_str_hash, g_str_equal);

	g_hash_table_insert(globals, "self-email", "mzucchi@dehaa.sa.gov.au");

	g_string_append(s, "(and ");
	optionl = op->options;
	while (optionl) {
		struct filter_optionrule *or = optionl->data;
		if (or->rule->type == FILTER_XML_MATCH
		    || or->rule->type == FILTER_XML_EXCEPT) {
			expand_variables(s, or->rule->code, or->args, globals);
		}
		optionl = g_list_next(optionl);
	}

	g_string_append(s, ")");
#if 0
	optionl = op->options;
	while (optionl) {
		struct filter_optionrule *or = optionl->data;
		if (or->rule->type == FILTER_XML_ACTION) {
			g_string_append(s, or->rule->code);
			g_string_append(s, " ");
		}
		optionl = g_list_next(optionl);
	}
	g_string_append(s, ")))");
#endif
	printf("combined rule '%s'\n", s->str);
}

struct filter_optionrule *
find_optionrule(struct filter_option *option, char *name)
{
	GList *optionrulel;
	struct filter_optionrule *or;
	
	optionrulel = option->options;
	while (optionrulel) {
		or = optionrulel->data;
		if (!strcmp(or->rule->name, name)) {
			return or;
		}
		optionrulel = g_list_next(optionrulel);
	}
	return NULL;
}

static char nooption[] = "<h1>Select option</h1><p>Select an option type from the list above.</p>";

void
html_write_options(GtkHTML *html, struct filter_option *option)
{
	GtkHTMLStreamHandle *stream;
	GList *optionrulel;
	
	stream = gtk_html_begin(html, "");
	gtk_html_write(html, stream, "<body bgcolor=white alink=blue>", strlen("<body bgcolor=white alink=blue>"));
	if (option) {
		optionrulel = option->options;
		while (optionrulel) {
			struct filter_optionrule *or = optionrulel->data;
			
			filter_description_html_write(or->rule->description, or->args, html, stream);
			gtk_html_write(html, stream, "<br>", strlen("<br>"));
			optionrulel = g_list_next(optionrulel);
		}
	} else {
		gtk_html_write(html, stream, nooption, strlen(nooption));
	}
	gtk_html_end(html, stream, GTK_HTML_STREAM_OK);
}

void
fill_rules(GtkWidget *list, GList *rules, struct filter_option *option, int type)
{
	GList *optionl, *rulel;
	GtkWidget *listitem, *hbox, *checkbox, *label;
	GList *items = NULL;

	rulel = rules;
	while (rulel) {
		struct filter_rule *fr = rulel->data;
		char *labeltext;

		if (fr->type == type) {
			int state;

			state = find_optionrule(option, fr->name) != NULL;

			labeltext = filter_description_text(fr->description, NULL);
			
			hbox = gtk_hbox_new(FALSE, 3);
			checkbox = gnome_pixmap_new_from_xpm_d(state?check_xpm:blank_xpm);
			gtk_box_pack_start(GTK_BOX(hbox), checkbox, FALSE, FALSE, 0);
			label = gtk_label_new(labeltext);
			gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
			listitem = gtk_list_item_new();
			gtk_container_add(GTK_CONTAINER(listitem), hbox);
			gtk_widget_show_all(listitem);
			
			gtk_object_set_data(GTK_OBJECT(listitem), "checkbox", checkbox);
			gtk_object_set_data(GTK_OBJECT(listitem), "checkstate", (void *)state);
			gtk_object_set_data(GTK_OBJECT(listitem), "rule", fr);
			
			items = g_list_append(items, listitem);
		}
		rulel = g_list_next(rulel);
	}
	gtk_list_append_items(GTK_LIST(list), items);
}

void
fill_options(GtkWidget *list, GList *options)
{
	GList *optionl, *rulel, *optionrulel;
	GtkWidget *listitem, *hbox, *checkbox, *label;
	GList *items = NULL;

	optionl = options;
	while (optionl) {
		struct filter_option *op = optionl->data;
		char *labeltext;

		labeltext = filter_description_text(op->description, NULL);
		listitem = gtk_list_item_new_with_label(labeltext);
		g_free(labeltext);
		gtk_widget_show_all(listitem);

		gtk_object_set_data(GTK_OBJECT(listitem), "option", op);
		
		items = g_list_append(items, listitem);
		optionl = g_list_next(optionl);
	}
	gtk_list_append_items(GTK_LIST(list), items);
}

GtkWidget *list_global, *html_global;
struct filter_option *option_current;

static void
select_rule_child(GtkList *list, GtkWidget *child, void *data)
{
	GtkWidget *w;
	struct filter_rule *fr = gtk_object_get_data(GTK_OBJECT(child), "rule");
	int state;
	struct filter_optionrule *rule;

	w = gtk_object_get_data(GTK_OBJECT(child), "checkbox");
	state = !(int) gtk_object_get_data(GTK_OBJECT(child), "checkstate");

	gnome_pixmap_load_xpm_d(GNOME_PIXMAP(w), state?check_xpm:blank_xpm);
	gtk_object_set_data(GTK_OBJECT(child), "checkstate", (void *)state);

	if (state) {
		printf("adding rule %p\n", fr);
		rule = g_malloc0(sizeof(*rule));
		rule->rule = fr;
		option_current->options = g_list_prepend(option_current->options, rule);
	} else {
		/* FIXME: free optionrule */
		rule = find_optionrule(option_current, fr->name);
		if (rule)
			option_current->options = g_list_remove(option_current->options, rule);
	}

	{
		GString *s = g_string_new("");
		expand_filter_option(s, option_current);
		printf("Rules: %s\n", s->str);
		g_string_free(s, TRUE);
	}

	html_write_options(html_global, option_current);
}

static void
select_option_child(GtkList *list, GtkWidget *child, void *data)
{
	struct filter_option *op = gtk_object_get_data(GTK_OBJECT(child), "option");
	struct filter_option *new;
	GList *optionsl;

	if (option_current) {
		/* free option_current copy */
		optionsl = option_current->options;
		while (optionsl) {
			GList *op = optionsl;
			optionsl = g_list_next(optionsl);
			g_free(op->data);
		}
		g_list_free(option_current->options);
		g_free(option_current);
		option_current = NULL;
	}

	/* clone the option */
	new = g_malloc(sizeof(*new));
	new->type = op->type;
	new->description = op->description;
	new->options = NULL;
	optionsl = op->options;
	while (optionsl) {
		struct filter_optionrule *ornew = g_malloc(sizeof(*ornew)),
			*or = optionsl->data;
		ornew->rule = or->rule;
		/* FIXME: must copy args too *sigh* */
		ornew->args = or->args;
		new->options = g_list_append(new->options, ornew);
		optionsl = g_list_next(optionsl);
	}
	option_current = new;

	html_write_options(GTK_HTML(html_global), option_current);
}

static void
arg_link_clicked(GtkHTML *html, const char *url, void *data)
{
	printf("url clicked: %s\n", url);
	if (!strncmp(url, "arg:", 4)) {
		FilterArg *arg;
		void *dummy;

		if (sscanf(url+4, "%p %p", &dummy, &arg)==2
			&& arg) {
			printf("arg = %p\n", arg);
			filter_arg_edit_values(arg);
			/* should have a changed signal which propagates the rewrite */
			html_write_options(GTK_HTML(html_global), option_current);
		}
	}
}

static void
dialogue_clicked(GtkWidget *w, int button, void *data)
{
	GString *s = g_string_new("");

	printf("button %d clicked ...\n");

	if (option_current)
		expand_filter_option(s, option_current);

	g_string_free(s, TRUE);	
}

void create_dialogue(void)
{
	GtkWidget *dialogue,
		*scrolled_window,
		*list,
		*html,
		*frame;

	dialogue = gnome_dialog_new("Filter Rules",
				    GNOME_STOCK_BUTTON_PREV , GNOME_STOCK_BUTTON_NEXT, 
				    "Finish", GNOME_STOCK_BUTTON_CANCEL, 0);

	list = gtk_list_new();
	frame = gtk_frame_new("Filter Type");
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), list);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_set_focus_vadjustment
		(GTK_CONTAINER (list),
		 gtk_scrolled_window_get_vadjustment
		 (GTK_SCROLLED_WINDOW (scrolled_window)));
	gtk_container_add(GTK_CONTAINER(frame), scrolled_window);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dialogue)->vbox), frame, TRUE, TRUE, GNOME_PAD);

#if 0
	gtk_signal_connect(GTK_OBJECT(list), "select_child", select_rule_child, NULL);
	gtk_signal_connect(GTK_OBJECT(list), "unselect_child", select_rule_child, NULL);
#else
	gtk_signal_connect(GTK_OBJECT(list), "select_child", select_option_child, NULL);
	gtk_signal_connect(GTK_OBJECT(list), "unselect_child", select_option_child, NULL);
#endif

	frame = gtk_frame_new("Filter Description");
	html = gtk_html_new();
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolled_window), html);
	gtk_container_add(GTK_CONTAINER(frame), scrolled_window);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dialogue)->vbox), frame, TRUE, TRUE, GNOME_PAD);

	gtk_signal_connect(GTK_OBJECT(html), "link_clicked", arg_link_clicked, NULL);
	gtk_signal_connect(GTK_OBJECT(dialogue), "clicked", dialogue_clicked, NULL);

	list_global = list;
	html_global = html;

	gtk_widget_show_all(dialogue);
}

int main(int argc, char **argv)
{
	ESExp *f;
	ESExpResult *r;
	GList *rules, *options, *options2;
	xmlDocPtr doc, out, optionset, filteroptions;
	GString *s;

	gnome_init("Test", "0.0", argc, argv);
	gdk_rgb_init ();
	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual (gdk_rgb_get_visual ());

	create_dialogue();

	doc = xmlParseFile("filterdescription.xml");
	rules = filter_load_ruleset(doc);
	options = filter_load_optionset(doc, rules);
	options2 = options;
	out = xmlParseFile("saveoptions.xml");
	options = filter_load_optionset(out, rules);

#if 0
	option_current = options->data;
	fill_rules(list_global, rules, options->data, FILTER_XML_MATCH);
#else
	option_current = NULL;
	fill_options(list_global, options2);
#endif
	gtk_main();

	while (options) {
		struct filter_option *fo = options->data;
		GList *optionrulel;

		optionrulel = fo->options;
		while (optionrulel) {
			struct filter_optionrule *or = optionrulel->data;

			printf("formatting rule: %s\n", or->rule->name);

			/*filter_description_text(or->rule->description, or->args);*/
			filter_description_html_write(or->rule->description, or->args, NULL, NULL);

			optionrulel = g_list_next(optionrulel);
		}
		options = g_list_next(options);
	}

	return 0;

	s = g_string_new("");
	expand_filter_option(s, options->data);
	g_string_append(s, "");

	printf("total rule = '%s'\n", s->str);

	f = e_sexp_new();
	e_sexp_add_variable(f, 0, "sender", NULL);
	e_sexp_add_variable(f, 0, "receipient", NULL);
	e_sexp_add_variable(f, 0, "folder", NULL);

	/* simple functions */
	e_sexp_add_function(f, 0, "header-get", NULL, NULL);
	e_sexp_add_function(f, 0, "header-contains", NULL, NULL);
	e_sexp_add_function(f, 0, "copy-to", NULL, NULL);

	e_sexp_add_ifunction(f, 0, "set", NULL, NULL);

	/* control functions */
	e_sexp_add_ifunction(f, 0, "match-all", NULL, NULL);
	e_sexp_add_ifunction(f, 0, "match", NULL, NULL);
	e_sexp_add_ifunction(f, 0, "action", NULL, NULL);
	e_sexp_add_ifunction(f, 0, "except", NULL, NULL);

	e_sexp_input_text(f, s->str, strlen(s->str));
	e_sexp_parse(f);
	
}
