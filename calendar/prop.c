/*
 * Calendar properties dialog box
 * (C) 1998 the Free Software Foundation
 *
 * Author: Miguel de Icaza <miguel@kernel.org>
 */
#include <config.h>
#include <gnome.h>
#include "main.h"
#include "gnome-cal.h"

static GtkWidget *prop_win, *r1;
static GtkObject *sa, *ea;

void
start_changed (GtkAdjustment *sa, GtkAdjustment *ea)
{
	if (sa->value > 23.0){
		sa->value = 23.0;
		ea->value = 24.0;
		gtk_signal_emit_by_name (GTK_OBJECT (sa), "value_changed");
		gtk_signal_emit_by_name (GTK_OBJECT (ea), "value_changed");
	} else if (sa->value >= ea->value){
		ea->value = sa->value + 1.0;
		gtk_signal_emit_by_name (GTK_OBJECT (ea), "value_changed");
	}
}

void
end_changed (GtkAdjustment *ea, GtkAdjustment *sa)
{
	if (ea->value < 1.0){
		ea->value = 1.0;
		sa->value = 0.0;
		gtk_signal_emit_by_name (GTK_OBJECT (ea), "value_changed");
		gtk_signal_emit_by_name (GTK_OBJECT (sa), "value_changed");
	} else if (ea->value < sa->value){
		sa->value = ea->value - 1.0;
		gtk_signal_emit_by_name (GTK_OBJECT (sa), "value_changed");
	}
}

/* justifies the text */
static GtkWidget *
align (GtkWidget *w, float side)
{
	GtkWidget *a;

	a = gtk_alignment_new (side, 0.5, 1.0, 1.0);
	gtk_container_add (GTK_CONTAINER (a), w);

	return a;
}

static void
prop_cancel (void)
{
	gtk_widget_destroy (prop_win);
	prop_win = 0;
}

static void
prop_ok (void)
{
	day_begin = GTK_ADJUSTMENT (sa)->value;
	day_end   = GTK_ADJUSTMENT (ea)->value;
	gnome_config_set_int ("/calendar/Calendar/Day start", day_begin);
	gnome_config_set_int ("/calendar/Calendar/Day end", day_end);

	am_pm_flag = (GTK_TOGGLE_BUTTON (r1)->active) == 0;
	gnome_config_set_bool ("/calendar/Calendar/AM PM flag", am_pm_flag);

	gnome_config_sync ();

	prop_cancel ();
	day_range_changed ();
}

void
properties (void)
{
	GtkWidget *t, *f, *l, *ds, *de;
	GtkWidget *r2;
	GtkWidget *ok, *cancel, *hbox;

	if (prop_win)
		return;
	
	prop_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	t = gtk_table_new (0, 0, 0);
	f = gtk_frame_new (_("Calendar global parameters"));
	gtk_container_add (GTK_CONTAINER (prop_win), f);
	gtk_container_add (GTK_CONTAINER (f), t);
	gtk_container_border_width (GTK_CONTAINER (prop_win), GNOME_PAD);
	gtk_container_border_width (GTK_CONTAINER (t), GNOME_PAD);
	
	l = gtk_label_new (_("Day start:"));
	gtk_table_attach (GTK_TABLE (t), l,
			  0, 1, 0, 1, 0, 0, 0, 0);
	sa = gtk_adjustment_new (day_begin, 0.0, 25.00, 1.0, 1.0, 1.0);
	ds = gtk_hscale_new (GTK_ADJUSTMENT (sa));
	gtk_scale_set_digits (GTK_SCALE (ds), 0);
	gtk_table_attach (GTK_TABLE (t), ds,
			  1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
		
	l = gtk_label_new (_("Day end:"));
	gtk_table_attach (GTK_TABLE (t), l,
			  0, 1, 1, 2, 0, 0, 0, 0);
	ea = gtk_adjustment_new (day_end, 0.0, 25.00, 1.0, 1.0, 1.0);
	de = gtk_hscale_new (GTK_ADJUSTMENT (ea));
	gtk_scale_set_digits (GTK_SCALE (de), 0);
	gtk_table_attach (GTK_TABLE (t), de,
			  1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);

	gtk_signal_connect (sa, "value_changed",
			    GTK_SIGNAL_FUNC (start_changed), ea);
	gtk_signal_connect (ea, "value_changed",
			    GTK_SIGNAL_FUNC (end_changed), sa);

	/* Nice spacing :-) */
	gtk_table_attach (GTK_TABLE (t), gtk_label_new (""),
			  0, 1, 2, 3, 0, 0, 0, 0);
	
	r1 = gtk_radio_button_new_with_label (NULL, _("24 hour format"));
	r2 = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (r1),
							  _("12 hour format"));
	if (am_pm_flag)
		gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (r2), 1);
	
	gtk_table_attach (GTK_TABLE (t), align (r1, 0.0), 0, 2, 3, 4, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (t), align (r2, 0.0), 0, 2, 4, 5, GTK_FILL | GTK_EXPAND, 0, 0, 0);

	hbox = gtk_hbox_new (0, 0);
	ok = gnome_stock_button (GNOME_STOCK_BUTTON_OK);
	cancel = gnome_stock_button (GNOME_STOCK_BUTTON_CANCEL);
	gtk_box_pack_start (GTK_BOX (hbox), ok, 0, 0, 0);
	gtk_box_pack_end (GTK_BOX (hbox), cancel, 0, 0, 0);

	gtk_table_attach (GTK_TABLE (t), hbox, 0, 2, 5, 6, GTK_FILL | GTK_EXPAND, 0, 0, GNOME_PAD);
	
	gtk_signal_connect (GTK_OBJECT (ok), "clicked",
			    GTK_SIGNAL_FUNC (prop_ok), NULL);
	gtk_signal_connect (GTK_OBJECT (cancel), "clicked",
			    GTK_SIGNAL_FUNC (prop_cancel), NULL);
	gtk_widget_show_all (prop_win);
}

