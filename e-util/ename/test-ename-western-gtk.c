#include <gnome.h>
#include <e-util/e-unicode.h>
#include <ename/e-name-western.h>

ENameWestern *name;
GtkWidget    *full;
GtkWidget    *prefix;
GtkWidget    *first;
GtkWidget    *middle;
GtkWidget    *nick;
GtkWidget    *last;
GtkWidget    *suffix;

static void
fill_entries (void)
{

#define SET(a,b) (e_utf8_gtk_entry_set_text (GTK_ENTRY (a), (b) == NULL ? "" : (b)))
	SET(prefix, name->prefix);
	SET(first,  name->first);
	SET(middle, name->middle);
	SET(nick,   name->nick);
	SET(last,   name->last);
	SET(suffix, name->suffix);
}

static void
full_changed_cb (GtkEntry *fulle)
{
	gchar *str;

	e_name_western_free (name);
	str = e_utf8_gtk_entry_get_text (fulle);
	name = e_name_western_parse (str);
	fill_entries ();

	g_free (str);
}

static void
create_window (void)
{
	GtkWidget *app;
	GtkTable  *table;

	GtkWidget *prefix_label;
	GtkWidget *first_label;
	GtkWidget *middle_label;
	GtkWidget *nick_label;
	GtkWidget *last_label;
	GtkWidget *suffix_label;

	app = gnome_app_new ("test", "Evolution Western Name Parser");

	table = GTK_TABLE (gtk_table_new (3, 6, FALSE));

	full   = gtk_entry_new ();
	prefix = gtk_entry_new ();
	first  = gtk_entry_new ();
	middle = gtk_entry_new ();
	nick   = gtk_entry_new ();
	last   = gtk_entry_new ();
	suffix = gtk_entry_new ();

	gtk_widget_set_usize (prefix, 100, 0);
	gtk_widget_set_usize (first,  100, 0);
	gtk_widget_set_usize (middle, 100, 0);
	gtk_widget_set_usize (nick,   100, 0);
	gtk_widget_set_usize (last,   100, 0);
	gtk_widget_set_usize (suffix, 100, 0);

	gtk_table_attach (table, full, 0, 6, 0, 1,
			  GTK_EXPAND | GTK_FILL, 0,
			  0, 0);

	gtk_table_attach (table, prefix, 0, 1, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0,
			  0, 0);

	gtk_table_attach (table, first, 1, 2, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0,
			  0, 0);

	gtk_table_attach (table, middle, 2, 3, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0,
			  0, 0);

	gtk_table_attach (table, nick, 3, 4, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0,
			  0, 0);

	gtk_table_attach (table, last, 4, 5, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0,
			  0, 0);

	gtk_table_attach (table, suffix, 5, 6, 1, 2,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0,
			  0, 0);

	prefix_label = gtk_label_new ("Prefix"); 
	first_label  = gtk_label_new ("First"); 
	middle_label = gtk_label_new ("Middle"); 
	nick_label   = gtk_label_new ("Nick"); 
	last_label   = gtk_label_new ("Last"); 
	suffix_label = gtk_label_new ("Suffix"); 

	gtk_table_attach (table, prefix_label, 0, 1, 2, 3,
			  GTK_SHRINK, 0,
			  0, 0);

	gtk_table_attach (table, first_label, 1, 2, 2, 3,
			  GTK_SHRINK, 0,
			  0, 0);

	gtk_table_attach (table, middle_label, 2, 3, 2, 3,
			  GTK_SHRINK, 0,
			  0, 0);

	gtk_table_attach (table, nick_label, 3, 4, 2, 3,
			  GTK_SHRINK, 0,
			  0, 0);

	gtk_table_attach (table, last_label, 4, 5, 2, 3,
			  GTK_SHRINK, 0,
			  0, 0);

	gtk_table_attach (table, suffix_label, 5, 6, 2, 3,
			  GTK_SHRINK, 0,
			  0, 0);

	gnome_app_set_contents (GNOME_APP (app), GTK_WIDGET (table));

	gtk_widget_show_all (app);

	gtk_entry_set_text (GTK_ENTRY (full),
			    "The Honorable Doctor van Jacobsen, Albert Roderick \"The Clenched Fist\" Jr, MD, PhD, Esquire");

	name = e_name_western_parse ("The Honorable Doctor van Jacobsen, Albert Roderick \"The Clenched Fist\" Jr, MD, PhD, Esquire");
	fill_entries ();

	gtk_signal_connect (GTK_OBJECT (full), "changed", full_changed_cb, NULL);
}

int
main (int argc, char **argv)
{
	gnome_init ("Test EName", "Test EName", argc, argv);

	create_window ();

	gtk_main ();

	return 0;
}
