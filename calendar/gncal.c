/*
 * gnlp.c: LPQ/LPR stuff
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <config.h>

#include "gncal.h"
#include "calcs.h"

static GtkMenuEntry menu_items[] =
{
        {"File/Exit", "<control>Q", menu_file_quit, NULL},
        {"Help/About", NULL, menu_help_about, NULL},
};

#define DAY_ARRAY_MAX 35
/* The naughty global variables */
int curr_day, old_day;
int curr_month, old_month;
int curr_year, old_year;
GtkWidget *month_label;
GtkWidget *year_label;
GtkWidget *dailylist;
GtkWidget *calendar_days[DAY_ARRAY_MAX];
GtkWidget *calendar_buttons[DAY_ARRAY_MAX];
GtkWidget *app;

#define ELEMENTS(x) (sizeof (x) / sizeof (x [0]))

GtkMenuFactory *
create_menu () 
{
  GtkMenuFactory *subfactory;
  int i;

  subfactory = gtk_menu_factory_new  (GTK_MENU_FACTORY_MENU_BAR);
  gtk_menu_factory_add_entries (subfactory, menu_items, ELEMENTS(menu_items));

  return subfactory;
}

/* place marker until i get get something better */
void print_error(char *text)
{
	GtkWidget *msgbox;
	char buf[512];

	if (errno == 0)
		sprintf(buf, "%s", text);
	else
		sprintf(buf, "%s (%s)", text, g_strerror(errno));

	g_warning("%s\n", buf);
	msgbox = gnome_messagebox_new(buf, "error", "OK", NULL, NULL);

	gtk_widget_show(msgbox);
}


void menu_file_quit(GtkWidget *widget, gpointer data)
{
	gtk_exit(0);
}

void menu_help_about(GtkWidget *widget, gpointer data)
{
	GtkWidget *about;
	gchar *authors[] = {
		"Craig Small <csmall@small.dropbear.id.au>",
		NULL };
	about = gnome_about_new("Gnome Calendar", VERSION,
				"(C) 1998",
				authors,
				/* Comments */
				"This program shows a nice pretty "
				"calendar and will do scheduling "
				"real soon now!",
				NULL);
				
	gtk_widget_show(about);
}

void dailylist_item_select(GtkWidget *widget, gpointer data)
{
	int *x = (int*)data;

	g_print("Selected %d\n", x);
}

void update_today_list(void)
{
	GtkWidget *listitem;
	GtkWidget *list_hbox;
	GtkWidget *hour_label;
	GtkWidget *event_label;
	char buf[50];
	int tmphr, tmpmin,i;

}

/*
 * updates the calendar that appears in the left collumn
 */
void update_calendar()
{
	int tmpday;
	int i;
	char buf[50];
	int month_changed;
	static int offset;

	/* Only update the whole calendar if the year or month has changed */
	tmpday=1;
	month_changed = FALSE;
	if (curr_month != old_month || curr_year != old_year)  {
		month_changed = TRUE;
		offset = weekday_of_date(tmpday, curr_month, curr_year) - 1;
	}

	for(i=0; i < DAY_ARRAY_MAX; i++) {
			tmpday = i - offset +1;
		if (valid_date(tmpday, curr_month, curr_year)) {
			sprintf(buf, "%2d", tmpday);
			/*if (month_changed) {*/
				gtk_label_set(GTK_LABEL(calendar_days[i]), buf);
				gtk_widget_show(calendar_buttons[i]);
			/*}*/
			if (tmpday == curr_day) {
				gtk_container_border_width(GTK_CONTAINER(calendar_buttons[i]), 2);
				gtk_widget_show(calendar_buttons[i]);
			} else {
				gtk_container_border_width(GTK_CONTAINER(calendar_buttons[i]), 0);
			}
		} else if (month_changed) {
			gtk_label_set(GTK_LABEL(calendar_days[i]), "");
			gtk_widget_hide(calendar_buttons[i]);
			gtk_container_border_width(GTK_CONTAINER(calendar_buttons[i]), 0);
		}
	} /* for i */	

}

/*
 * Updates all the main window widgets when the current day of interest is
 * changed
 */
void update_today(void)
{
	char buf[50];

	/* This needs to be fixed to get the right date order for the country*/
	if (curr_month != old_month) {
		gtk_label_set(GTK_LABEL(month_label), month_name(curr_month));
	}
	if (curr_year != old_year) {
		sprintf(buf, "%4d", curr_year);
		gtk_label_set(GTK_LABEL(year_label), buf);
	}
	update_today_list();
	update_calendar();
}

void next_day_but_clicked(GtkWidget *widget, gpointer data)
{
	old_day = curr_day;
	old_month = curr_month;
	old_year = curr_year;
	next_date(&curr_day, &curr_month, &curr_year);
	update_today();
}

void prev_day_but_clicked(GtkWidget *widget, gpointer data)
{
	old_day = curr_day;
	old_month = curr_month;
	old_year = curr_year;
	prev_date(&curr_day, &curr_month, &curr_year);
	update_today();
}

void today_but_clicked(GtkWidget *widget, gpointer data)
{
	old_day = curr_day;
	old_month = curr_month;
	old_year = curr_year;
	get_system_date(&curr_day, &curr_month, &curr_year);
	update_today();
}

void prev_month_but_clicked(GtkWidget *widget, gpointer data)
{
	if (curr_year == 0 && curr_month == MONTH_MIN) 
		return;
	old_day = curr_day;
	old_month = curr_month;
	old_year = curr_year;
	curr_month--;
	if (curr_month < MONTH_MIN) {
		curr_month = MONTH_MAX;
		curr_year--;
	}
	update_today();
}

void next_month_but_clicked(GtkWidget *widget, gpointer data)
{
	if (curr_year == 3000 && curr_month == MONTH_MAX) 
		return;
	old_day = curr_day;
	old_month = curr_month;
	old_year = curr_year;
	curr_month++;
	if (curr_month > MONTH_MAX ) {
		curr_month = MONTH_MIN;
		curr_year++;
	}
	update_today();
}

void prev_year_but_clicked(GtkWidget *widget, gpointer data)
{
	if (curr_year == 0) 
		return;

	old_day = curr_day;
	old_month = curr_month;
	old_year = curr_year;
	curr_year--;
	update_today();
}

	
void next_year_but_clicked(GtkWidget *widget, gpointer data)
{
	if (curr_year == 3000) 
		return;

	old_day = curr_day;
	old_month = curr_month;
	old_year = curr_year;
	curr_year++;
	update_today();
}


void calendar_but_clicked(GtkWidget *widget, gpointer data)
{
	char *ptr; 
	int x;

	ptr = GTK_LABEL(GTK_BUTTON(widget)->child)->label;
	x = atoi(ptr);

	if (valid_date(x, curr_month, curr_year)) {
		old_day = curr_day;
		old_month = curr_month;
		old_year = curr_year;
		curr_day = x;
		update_today();
	}
}

void test_foreach(GtkWidget *widget, gpointer data)
{
	char *ptr;

	ptr = GTK_LABEL(GTK_BUTTON(widget)->child)->label;
	g_print("%s\n", ptr);
}

void show_main_window()
{
	GtkWidget *main_vbox;
	/*GtkWidget *menubar;
	GtkAcceleratorTable *accel;*/
	GtkMenuFactory *menuf;
	GtkWidget *main_hbox;
	GtkWidget *left_vbox;
	GtkWidget *right_vbox;
	GtkWidget *date_hbox;
	GtkWidget *prev_mth_but;
	GtkWidget *next_mth_but;
	GtkWidget *prev_year_but;
	GtkWidget *next_year_but;
	GtkWidget *day_but_hbox;
	GtkWidget *prev_day_but;
	GtkWidget *today_but;
	GtkWidget *next_day_but;
	GtkWidget *separator;
	GtkWidget *cal_table;
	GtkWidget *day_name_label;
	GtkWidget *scrolledwindow;
	GtkWidget *scroll_hbox;
	GtkWidget *hour_list;
	GtkWidget *list_item;
	GtkWidget *dailylist_item;
	GtkWidget *event_label;
	int i,j;
	struct tm tm;
	char buf[50];

	bzero((char*)&tm, sizeof(struct tm));
	app = gnome_app_new("gncal", "Gnome Calendar");
	gtk_widget_realize(app);
	gtk_signal_connect(GTK_OBJECT(app), "delete_event",
			   GTK_SIGNAL_FUNC(menu_file_quit), NULL);

	main_vbox = gtk_vbox_new(FALSE, 1);
	gnome_app_set_contents(GNOME_APP(app), main_vbox);
	gtk_widget_show(main_vbox);

	menuf = create_menu();
	gnome_app_set_menus(GNOME_APP(app), GTK_MENU_BAR(menuf->widget));
	/*get_main_menu(&menubar, &accel);
	gtk_window_add_accelerator_table(GTK_WINDOW(window), accel);
	gtk_box_pack_start(GTK_BOX(main_vbox), menubar, FALSE, FALSE, 0);
	gtk_widget_show(menubar);*/

	main_hbox = gtk_hbox_new(FALSE,1);
	gtk_box_pack_start(GTK_BOX(main_vbox), main_hbox, TRUE, TRUE, 0);
	gtk_widget_show(main_hbox);

	left_vbox = gtk_vbox_new(FALSE, 1);
	gtk_box_pack_start(GTK_BOX(main_hbox), left_vbox, FALSE, TRUE,0);
	gtk_widget_show(left_vbox);

	separator = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(main_hbox), separator, FALSE, TRUE, 0);
	gtk_widget_show(separator);

	right_vbox = gtk_vbox_new(FALSE, 1);
	gtk_box_pack_start(GTK_BOX(main_hbox), right_vbox, TRUE, TRUE, 0);
	gtk_widget_show(right_vbox);

	date_hbox = gtk_hbox_new(FALSE, 1);
	gtk_box_pack_start(GTK_BOX(left_vbox), date_hbox, FALSE, FALSE, 0);
	gtk_widget_show(date_hbox);

	prev_mth_but = gtk_button_new_with_label("<");
	gtk_box_pack_start(GTK_BOX(date_hbox), prev_mth_but, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(prev_mth_but), "clicked", GTK_SIGNAL_FUNC(prev_month_but_clicked), NULL);
	gtk_widget_show(prev_mth_but);

	month_label = gtk_label_new("Fooary");
	gtk_box_pack_start(GTK_BOX(date_hbox), month_label, TRUE, FALSE, 0);
	gtk_widget_show(month_label);

	next_mth_but = gtk_button_new_with_label(">");
	gtk_box_pack_start(GTK_BOX(date_hbox), next_mth_but, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(next_mth_but), "clicked", GTK_SIGNAL_FUNC(next_month_but_clicked), NULL);
	gtk_widget_show(next_mth_but);

	prev_year_but = gtk_button_new_with_label("<");
	gtk_box_pack_start(GTK_BOX(date_hbox), prev_year_but, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(prev_year_but), "clicked", GTK_SIGNAL_FUNC(prev_year_but_clicked), NULL);
	gtk_widget_show(prev_year_but);

	year_label = gtk_label_new("1971");
	gtk_box_pack_start(GTK_BOX(date_hbox), year_label, TRUE, FALSE, 0);
	gtk_widget_show(year_label);
	
	next_year_but = gtk_button_new_with_label(">");
	gtk_box_pack_start(GTK_BOX(date_hbox), next_year_but, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(next_year_but), "clicked", GTK_SIGNAL_FUNC(next_year_but_clicked), NULL);
	gtk_widget_show(next_year_but);

	/* Build up the calendar table */
	cal_table = gtk_table_new(7,7,TRUE);
	gtk_box_pack_start(GTK_BOX(left_vbox), cal_table, FALSE, FALSE, 0);
	gtk_widget_show(cal_table);

	for(i=DAY_MIN; i <= DAY_MAX; i++) {
		day_name_label = gtk_label_new(short3_day_name(i));
		gtk_table_attach_defaults(GTK_TABLE(cal_table), day_name_label, i-1, i, 0, 1);
		gtk_widget_show(day_name_label);
	}
	for(j=0; j < 5; j++) {
		for(i=0; i < 7; i++) {
			calendar_buttons[i+j*7] = gtk_button_new();
			gtk_container_border_width(GTK_CONTAINER(calendar_buttons[i+j*7]), 0);
			gtk_table_attach_defaults(GTK_TABLE(cal_table), calendar_buttons[i+j*7], i, i+1, j+2, j+3);
			gtk_signal_connect(GTK_OBJECT(calendar_buttons[i+j*7]), "clicked",  GTK_SIGNAL_FUNC(calendar_but_clicked), NULL);
			gtk_widget_show(calendar_buttons[i+j*7]);
			calendar_days[i+j*7] = gtk_label_new("");
			gtk_container_add(GTK_CONTAINER(calendar_buttons[i+j*7]), calendar_days[i+j*7]);
			gtk_widget_show(calendar_days[i+j*7]);
		}
	}




	day_but_hbox = gtk_hbox_new(TRUE, 1);
	gtk_box_pack_start(GTK_BOX(left_vbox), day_but_hbox, FALSE, FALSE, 0);
	gtk_widget_show(day_but_hbox);

	prev_day_but = gtk_button_new_with_label("Prev");
	gtk_box_pack_start(GTK_BOX(day_but_hbox), prev_day_but, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(prev_day_but), "clicked", GTK_SIGNAL_FUNC(prev_day_but_clicked), NULL);
	gtk_widget_show(prev_day_but);
	
	today_but = gtk_button_new_with_label("Today");
	gtk_box_pack_start(GTK_BOX(day_but_hbox), today_but, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(today_but), "clicked", GTK_SIGNAL_FUNC(today_but_clicked), NULL);
	gtk_widget_show(today_but);
	
	next_day_but = gtk_button_new_with_label("Next");
	gtk_box_pack_start(GTK_BOX(day_but_hbox), next_day_but, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(next_day_but), "clicked", GTK_SIGNAL_FUNC(next_day_but_clicked), NULL);
	gtk_widget_show(next_day_but);

	scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(right_vbox), scrolledwindow, TRUE, TRUE, 0);
	gtk_widget_show(scrolledwindow);

	scroll_hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(scrolledwindow), scroll_hbox);
	gtk_widget_show(scroll_hbox);

	hour_list = gtk_list_new();
	gtk_box_pack_start(GTK_BOX(scroll_hbox), hour_list, FALSE, FALSE, 0);
	gtk_widget_show(hour_list);

	separator = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(scroll_hbox), separator, FALSE, FALSE, 0);
	gtk_widget_show(separator);

	dailylist = gtk_list_new();
	gtk_box_pack_start(GTK_BOX(scroll_hbox), dailylist, TRUE, TRUE, 0);
	gtk_widget_show(dailylist);

	for (i=0; i< 24 ; i++) {
		sprintf(buf, "%d:00", i);
		list_item = gtk_list_item_new_with_label(buf);
		gtk_container_add(GTK_CONTAINER(hour_list), list_item);
		gtk_widget_show(list_item);

		dailylist_item = gtk_list_item_new();
		gtk_container_add(GTK_CONTAINER(dailylist), dailylist_item);
		gtk_signal_connect(GTK_OBJECT(dailylist_item), "selected", GTK_SIGNAL_FUNC(dailylist_item_select), list_item);
		gtk_signal_connect_object(GTK_OBJECT(list_item), "selected", GTK_SIGNAL_FUNC(dailylist_item_select), GTK_OBJECT(dailylist_item));
		gtk_widget_show(dailylist_item);
		event_label = gtk_label_new("blah");
		gtk_container_add(GTK_CONTAINER(dailylist_item), event_label);
		gtk_widget_show(event_label);
	}
	gtk_widget_show(app);

}


int main(int argc, char *argv[])
{

	gnome_init("gncal", &argc, &argv);

	show_main_window();

	/* Initialse date to the current day */
	old_day = old_month = old_year = 0;
	get_system_date(&curr_day, &curr_month, &curr_year);
	update_today();

	gtk_main();

	return 0;
}

