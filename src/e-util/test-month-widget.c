/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <e-util/e-util.h>

static gboolean
window_delete_event_cb (GtkWidget *widget,
			GdkEvent *event,
			gpointer user_data)
{
	gtk_main_quit ();

	return FALSE;
}

static void
month_changed_cb (GtkSpinButton *spin_button,
		  EMonthWidget *month_widget)
{
	GDateMonth month;
	guint year = 0;

	month = gtk_spin_button_get_value_as_int (spin_button);
	e_month_widget_get_month (month_widget, NULL, &year);
	e_month_widget_set_month (month_widget, month, year);
}

static void
year_changed_cb (GtkSpinButton *spin_button,
		 EMonthWidget *month_widget)
{
	GDateMonth month = G_DATE_JANUARY;
	guint year;

	year = gtk_spin_button_get_value_as_int (spin_button);
	e_month_widget_get_month (month_widget, &month, NULL);
	e_month_widget_set_month (month_widget, month, year);
}

static void
week_start_day_changed_cb (GtkComboBox *combo,
			   EMonthWidget *month_widget)
{
	const gchar *id = gtk_combo_box_get_active_id (combo);
	e_month_widget_set_week_start_day (month_widget, g_ascii_strtoll (id, NULL, 10));
}

static gboolean
month_widget_montion_notify_event_cb (EMonthWidget *month_widget,
				      GdkEvent *event,
				      guint *p_selected_day)
{
	gdouble x_win = -1, y_win = -1;

	if (gdk_event_get_coords (event, &x_win, &y_win)) {
		guint select_day;

		select_day = e_month_widget_get_day_at_position (month_widget, x_win, y_win);

		if (select_day == *p_selected_day)
			return FALSE;

		if (*p_selected_day)
			e_month_widget_set_day_selected (month_widget, *p_selected_day, FALSE);

		if (select_day)
			e_month_widget_set_day_selected (month_widget, select_day, TRUE);

		*p_selected_day = select_day;
	} else if (p_selected_day) {
		e_month_widget_set_day_selected (month_widget, *p_selected_day, FALSE);
		*p_selected_day = 0;
	}

	return FALSE;
}

static void
bold_toggled_cb (GtkToggleButton *toggle_button,
		 guint *p_set_mark)
{
	*p_set_mark = (*p_set_mark & ~1) |
		(gtk_toggle_button_get_active (toggle_button) ? 1 : 0);
}

static void
italic_toggled_cb (GtkToggleButton *toggle_button,
		   guint *p_set_mark)
{
	*p_set_mark = (*p_set_mark & ~2) |
		(gtk_toggle_button_get_active (toggle_button) ? 2 : 0);
}

static void
underline_toggled_cb (GtkToggleButton *toggle_button,
		      guint *p_set_mark)
{
	*p_set_mark = (*p_set_mark & ~4) |
		(gtk_toggle_button_get_active (toggle_button) ? 4 : 0);
}


static void
highlight_toggled_cb (GtkToggleButton *toggle_button,
		      guint *p_set_mark)
{
	*p_set_mark = (*p_set_mark & ~8) |
		(gtk_toggle_button_get_active (toggle_button) ? 8 : 0);
}

static void
clear_marks_clicked_cb (GtkToggleButton *toggle_button,
			EMonthWidget *month_widget)
{
	e_month_widget_clear_day_css_classes (month_widget);
}

static void
month_widget_day_clicked_cb (EMonthWidget *widget,
			     GdkEventButton *event,
			     guint year,
			     gint month,
			     guint day,
			     guint *p_set_mark)
{
	void (* func) (EMonthWidget *widget, guint day, const gchar *name);
	gchar buff[128];

	if (event->button == GDK_BUTTON_PRIMARY)
		func = e_month_widget_add_day_css_class;
	else if (event->button == GDK_BUTTON_SECONDARY)
		func = e_month_widget_remove_day_css_class;
	else
		return;

	if ((*p_set_mark) & 1)
		func (widget, day, E_MONTH_WIDGET_CSS_CLASS_BOLD);
	if ((*p_set_mark) & 2)
		func (widget, day, E_MONTH_WIDGET_CSS_CLASS_ITALIC);
	if ((*p_set_mark) & 4)
		func (widget, day, E_MONTH_WIDGET_CSS_CLASS_UNDERLINE);
	if ((*p_set_mark) & 8)
		func (widget, day, E_MONTH_WIDGET_CSS_CLASS_HIGHLIGHT);

	g_snprintf (buff, sizeof (buff), "Last clicked day: %04u-%02d-%02u", year, month, day);
	gtk_widget_set_tooltip_text (GTK_WIDGET (widget), buff);
}

static void
sync_year_on_change_cb (EMonthWidget *src_month_widget,
			EMonthWidget *des_month_widget)
{
	GDateMonth month = G_DATE_BAD_MONTH;
	guint year = 0, cur_year = 0;

	e_month_widget_get_month (src_month_widget, NULL, &year);
	e_month_widget_get_month (des_month_widget, &month, &cur_year);

	if (cur_year != year) {
		e_month_widget_set_month (des_month_widget, month, year);
		e_month_widget_clear_day_css_classes (des_month_widget);
	}
}

static gint
on_idle_create_widget (ESourceRegistry *registry)
{
	GDate *date;
	GtkWidget *window, *notebook;
	GtkWidget *widget, *container, *hbox, *month_widget;
	static guint selected_day = 0;
	static guint set_mark = 0;
	guint year = 0;
	gint ii;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 300, 400);

	g_signal_connect (
		window, "delete-event",
		G_CALLBACK (window_delete_event_cb), NULL);

	notebook = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (notebook));

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);

	container = widget;

	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), container, gtk_label_new ("Month"));

	month_widget = e_month_widget_new ();

	g_signal_connect (month_widget, "motion-notify-event",
		G_CALLBACK (month_widget_montion_notify_event_cb), &selected_day);

	g_signal_connect (month_widget, "day-clicked",
		G_CALLBACK (month_widget_day_clicked_cb), &set_mark);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_pack_start (GTK_BOX (container), hbox, FALSE, FALSE, 0);

	widget = gtk_label_new ("Month:");
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	widget = gtk_spin_button_new_with_range (1, 12, 1);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), 3);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	g_signal_connect (widget, "value-changed",
		G_CALLBACK (month_changed_cb), month_widget);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_pack_start (GTK_BOX (container), hbox, FALSE, FALSE, 0);

	widget = gtk_label_new ("Year:");
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	widget = gtk_spin_button_new_with_range (1, 3000, 1);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), 2022);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	e_month_widget_set_month (E_MONTH_WIDGET (month_widget), 3, 2022);

	g_signal_connect (widget, "value-changed",
		G_CALLBACK (year_changed_cb), month_widget);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_pack_start (GTK_BOX (container), hbox, FALSE, FALSE, 0);

	widget = gtk_label_new ("Week start day:");
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	widget = gtk_combo_box_text_new ();
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "1", "Mo");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "2", "Tu");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "3", "We");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "4", "Th");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "5", "Fr");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "6", "Sa");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "7", "Su");

	gtk_combo_box_set_active_id (GTK_COMBO_BOX (widget), "1");
	e_month_widget_set_week_start_day (E_MONTH_WIDGET (month_widget), G_DATE_MONDAY);

	g_signal_connect (widget, "changed", G_CALLBACK (week_start_day_changed_cb), month_widget);

	widget = gtk_check_button_new_with_label ("Show week numbers");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	e_binding_bind_property (widget, "active", month_widget, "show-week-numbers", G_BINDING_SYNC_CREATE);

	widget = gtk_check_button_new_with_label ("Show day names");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	e_binding_bind_property (widget, "active", month_widget, "show-day-names", G_BINDING_SYNC_CREATE);

	widget = gtk_label_new ("Click to left-set/right-unset mark");
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_START,
		NULL);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_pack_start (GTK_BOX (container), hbox, FALSE, FALSE, 0);

	widget = gtk_check_button_new_with_label ("B");
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	g_signal_connect (widget, "toggled", G_CALLBACK (bold_toggled_cb), &set_mark);

	widget = gtk_check_button_new_with_label ("I");
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	g_signal_connect (widget, "toggled", G_CALLBACK (italic_toggled_cb), &set_mark);

	widget = gtk_check_button_new_with_label ("U");
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	g_signal_connect (widget, "toggled", G_CALLBACK (underline_toggled_cb), &set_mark);

	widget = gtk_check_button_new_with_label ("H");
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	g_signal_connect (widget, "toggled", G_CALLBACK (highlight_toggled_cb), &set_mark);

	widget = gtk_button_new_with_label ("Clear Marks");
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	g_signal_connect (widget, "clicked", G_CALLBACK (clear_marks_clicked_cb), month_widget);

	gtk_box_pack_start (GTK_BOX (container), month_widget, TRUE, TRUE, 0);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);

	container = widget;

	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), container, gtk_label_new ("Year"));

	widget = gtk_scrolled_window_new (NULL, NULL);

	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);

	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	container = widget;

	widget = gtk_flow_box_new ();

	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"column-spacing", 12,
		"row-spacing", 12,
		"homogeneous", TRUE,
		"min-children-per-line", 1,
		"max-children-per-line", 6,
		"selection-mode", GTK_SELECTION_NONE,
		NULL);

	gtk_style_context_add_class (gtk_widget_get_style_context (widget), GTK_STYLE_CLASS_VIEW);

	gtk_container_add (GTK_CONTAINER (container), widget);
	container = widget;

	e_month_widget_get_month (E_MONTH_WIDGET (month_widget), NULL, &year);

	date = g_date_new_dmy (1, 1, 2022);

	for (ii = 0; ii < 12; ii++) {
		GtkFlowBoxChild *child;
		GtkWidget *vbox;
		gchar buffer[128];

		g_date_strftime (buffer, sizeof (buffer), "%B", date);
		g_date_add_months (date, 1);

		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

		widget = gtk_label_new (buffer);

		g_object_set (G_OBJECT (widget),
			"halign", GTK_ALIGN_CENTER,
			"valign", GTK_ALIGN_CENTER,
			"xalign", 0.5,
			"yalign", 0.5,
			NULL);

		gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

		widget = e_month_widget_new ();

		g_object_set (G_OBJECT (widget),
			"halign", GTK_ALIGN_CENTER,
			"valign", GTK_ALIGN_CENTER,
			NULL);

		gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

		g_signal_connect (month_widget, "changed",
			G_CALLBACK (sync_year_on_change_cb), widget);

		g_signal_connect (widget, "day-clicked",
			G_CALLBACK (month_widget_day_clicked_cb), &set_mark);

		e_binding_bind_property (month_widget, "week-start-day", widget, "week-start-day", G_BINDING_SYNC_CREATE);
		e_binding_bind_property (month_widget, "show-week-numbers", widget, "show-week-numbers", G_BINDING_SYNC_CREATE);
		e_binding_bind_property (month_widget, "show-day-names", widget, "show-day-names", G_BINDING_SYNC_CREATE);

		e_month_widget_set_month (E_MONTH_WIDGET (widget), ii + 1, year);

		gtk_container_add (GTK_CONTAINER (container), vbox);

		child = gtk_flow_box_get_child_at_index (GTK_FLOW_BOX (container), ii);

		g_object_set (G_OBJECT (child),
			"halign", GTK_ALIGN_CENTER,
			"valign", GTK_ALIGN_START,
			NULL);
	}

	g_date_free (date);

	gtk_widget_show_all (window);

	return FALSE;
}

gint
main (gint argc,
      gchar **argv)
{
	ESourceRegistry *registry;
	GError *local_error = NULL;

	gtk_init (&argc, &argv);

	registry = e_source_registry_new_sync (NULL, &local_error);

	if (local_error != NULL) {
		g_error (
			"Failed to load ESource registry: %s",
			local_error->message);
		g_return_val_if_reached (-1);
	}

	g_idle_add ((GSourceFunc) on_idle_create_widget, registry);

	gtk_main ();

	g_object_unref (registry);
	e_misc_util_free_global_memory ();

	return 0;
}
