/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

struct _EAddressBookSearchDialog {
	GtkWidget *search;
	GtkWidget *view;
};

static void
button_press (GtkWidget *widget, EAddressBookSearchDialog *dialog)
{
	char *query;
	gtk_widget_show(dialog->view);
	query = get_query();
	gtk_object_set(GTK_OBJECT(dialog->view),
		       "query", query,
		       NULL);
	g_free(query);
}

GtkWidget *
get_addressbook_search_dialog(EBook *book)
{
	GtkWidget *vbox;
	GtkWidget *search;
	GtkWidget *search_button;
	GtkWidget *view;

	vbox = gtk_vbox_new(FALSE, 0);

	search = get_widget();
	gtk_box_pack_start(GTK_BOX(vbox), search, TRUE, TRUE, 0);
	gtk_widget_show(search);

	button = gtk_button_new_with_label(_("Search"));
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	view = e_minicard_view_widget_new();
	gtk_box_pack_start(GTK_BOX(vbox), view, TRUE, TRUE, 0);
	gtk_object_set(GTK_OBJECT(dialog->view),
		       "book", book,
		       NULL);

	gtk_widget_show(vbox);
	return vbox;
}
