/*
 * Calendar Object editor.
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Author: Miguel de Icaza (miguel@kernel.org)
 */

#include <gnome.h>

/* Day start and day end in hours */
int day_start, day_end;

typedef struct {
	GtkWidget     *property_box;

	GtkWidget     *general;
	
	GtkTable      *general_table;
	GtkWidget     *general_time_table;
} ObjEditor;

GtkWidget *
calendar_object_editor_setup_time_frame (ObjEditor *oe)
{
	GtkWidget *frame;
	GtkWidget *start_time, *end_time;
	GtkTable  *t;
	
	frame = gtk_frame_new (_("Time"));
	t = GTK_TABLE (oe->general_time_table = gtk_table_new (1, 1, 0));
	gtk_container_add (GTK_CONTAINER (frame), oe->general_time_table);
	
	start_time = gnome_date_edit_new (0);
	end_time   = gnome_date_edit_new (0);
	gnome_date_edit_set_popup_range ((GnomeDateEdit *) start_time, day_start, day_end);
	gnome_date_edit_set_popup_range ((GnomeDateEdit *) end_time, day_start, day_end);

	gtk_table_attach (t, gtk_label_new (_("Start time")), 1, 2, 1, 2, 0, 0, 0, 0);
	gtk_table_attach (t, gtk_label_new (_("End time")), 1, 2, 2, 3, 0, 0, 0, 0);
	
	gtk_table_attach (t, start_time, 2, 3, 1, 2, 0, 0, 0, 0);
	gtk_table_attach (t, end_time, 2, 3, 2, 3, 0, 0, 0, 0);
	return frame;
}

void
calendar_general_editor_new (ObjEditor *oe)
{
	GtkWidget *frame;

	oe->general = gtk_hbox_new (0, 0);
	oe->general_table = (GtkTable *) gtk_table_new (1, 1, 0);

	gtk_box_pack_start (GTK_BOX (oe->general), (GtkWidget *) oe->general_table, 1, 1, 0);

	frame = calendar_object_editor_setup_time_frame (oe);
	gtk_table_attach (oe->general_table, frame,
			  1, 2, 1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	
	gnome_property_box_append_page (oe->property_box, oe->general, gtk_label_new (_("General")));
}

ObjEditor *
calendar_object_editor_new (void)
{
	ObjEditor *oe;

	oe = g_new0 (ObjEditor, 1);
	
	oe->property_box = gnome_property_box_new ();
	calendar_general_editor_new (oe);

	return oe;
}

main (int argc, char *argv [])
{
	ObjEditor *oe;

	day_start = 7;
	day_end   = 19;
	gnome_init ("myapp", NULL, argc, argv, 0, NULL);

	oe = calendar_object_editor_new ();
	gtk_widget_show_all (oe->property_box);
	gtk_main ();
}
