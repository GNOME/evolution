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

#include <gnome.h>
#include <config.h>

#include "gncal.h"
#include "calcs.h"
#include "clist.h"
#include "gncal-week-view.h"

void
prueba (void)
{
	GtkWidget *window;
	GtkWidget *wview;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy (GTK_WINDOW (window), TRUE, TRUE, FALSE);
	gtk_container_border_width (GTK_CONTAINER (window), 6);

	wview = gncal_week_view_new (NULL, time (NULL));
	gtk_container_add (GTK_CONTAINER (window), wview);
	gtk_widget_show (wview);

	gtk_widget_show (window);
}

/* Function declarations */
void parse_args(int argc, char *argv[]);
static int save_state      (GnomeClient        *client,
			    gint                phase,
			    GnomeRestartStyle   save_style,
			    gint                shutdown,
			    GnomeInteractStyle  interact_style,
			    gint                fast,
			    gpointer            client_data);
static void connect_client (GnomeClient *client, 
			    gint         was_restarted, 
			    gpointer     client_data);

void discard_session (gchar *id);

static GtkMenuEntry menu_items[] =
{
        { N_("File/Exit"), N_("<control>Q"), menu_file_quit, NULL},
        { N_("Help/About"), N_("<control>A"), menu_help_about, NULL},
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
GtkWidget *calendar;

int restarted = 0;
/* Stuff we use for session state */
int os_x = 0,
    os_y = 0,
    os_w = 0,
    os_h = 0;


/* True if parsing determined that all the work is already done.  */
int just_exit = 0;

/* These are the arguments that our application supports.  */
static struct argp_option arguments[] =
{
#define DISCARD_KEY -1
  { "discard-session", DISCARD_KEY, N_("ID"), 0, N_("Discard session"), 1 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* Forward declaration of the function that gets called when one of
   our arguments is recognized.  */
static error_t parse_an_arg (int key, char *arg, struct argp_state *state);

/* This structure defines our parser.  It can be used to specify some
   options for how our parsing function should be called.  */
static struct argp parser =
{
  arguments,			/* Options.  */
  parse_an_arg,			/* The parser function.  */
  NULL,				/* Some docs.  */
  NULL,				/* Some more docs.  */
  NULL,				/* Child arguments -- gnome_init fills
				   this in for us.  */
  NULL,				/* Help filter.  */
  NULL				/* Translation domain; for the app it
				   can always be NULL.  */
};

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
	msgbox = gnome_message_box_new(buf, "error", "OK", NULL, NULL);

	gtk_widget_show(msgbox);
}


void menu_file_quit(GtkWidget *widget, gpointer data)
{
	gtk_main_quit();
}

void menu_help_about(GtkWidget *widget, gpointer data)
{
	GtkWidget *about;
	gchar *authors[] = {
		"Craig Small <csmall@small.dropbear.id.au>",
		NULL };
	about = gnome_about_new( _("Gnome Calendar"), VERSION,
				"(C) 1998",
				authors,
				/* Comments */
				_("This program shows a nice pretty "
				"calendar and will do scheduling "
				"real soon now!"),
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
void month_changed(GtkWidget *widget, gpointer data)
{
	curr_month = GTK_CALENDAR(widget)->month;
	curr_year = GTK_CALENDAR(widget)->year;
}

void update_calendar()
{
	int tmpday;
	int i;
	char buf[50];
	int month_changed;
	static int offset;

	gtk_calendar_unmark_day(GTK_CALENDAR(calendar),old_day);
	gtk_calendar_mark_day(GTK_CALENDAR(calendar), curr_day);
	printf("Date changed (nothing happens much\n");
/*	gtk_calendar_select_day(GTK_CALENDAR(calendar), curr_day); */
#ifdef 0
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
#endif /* 0 */
}

/*
 * Updates all the main window widgets when the current day of interest is
 * changed
 */
void update_today(void)
{
	char buf[50];

	/* This needs to be fixed to get the right date order for the country*/
/*	if (curr_month != old_month) {
		gtk_label_set(GTK_LABEL(month_label), month_name(curr_month));
	}
	if (curr_year != old_year) {
		sprintf(buf, "%4d", curr_year);
		gtk_label_set(GTK_LABEL(year_label), buf);
	}*/
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
	if (restarted) {
		gtk_widget_set_uposition(app, os_x, os_y);
		gtk_widget_set_usize(app, os_w, os_h);
	} else {
		gtk_widget_set_usize(app,300,300);
	}
	main_vbox = gtk_vbox_new(FALSE, 1);
	gnome_app_set_contents(GNOME_APP(app), main_vbox);
	gtk_widget_show(main_vbox);

	menuf = create_menu();
	gnome_app_set_menus(GNOME_APP(app), GTK_MENU_BAR(menuf->widget));

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
/*
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
*/
	/* Build up the calendar table */
/*	cal_table = gtk_table_new(7,7,TRUE);
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
*/
	calendar = gtk_calendar_new();
	gtk_calendar_display_options(GTK_CALENDAR(calendar), GTK_CALENDAR_SHOW_DAY_NAMES | GTK_CALENDAR_SHOW_HEADING);
	gtk_box_pack_start(GTK_BOX(left_vbox), calendar, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(calendar), "month_changed",
		GTK_SIGNAL_FUNC(month_changed), NULL);
	gtk_widget_show(calendar);


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


	dailylist = create_clist();
	gtk_box_pack_start(GTK_BOX(right_vbox), dailylist, TRUE, TRUE, 0);
	gtk_widget_show(dailylist);
	setup_clist(dailylist);

	gtk_widget_show(app);

}


int 
main(int argc, char *argv[])
{
	GnomeClient *client;

	argp_program_version = VERSION;


	/* Initialise the i18n stuff */
	bindtextdomain(PACKAGE, GNOMELOCALEDIR);
	textdomain(PACKAGE);

	/* This create a default client and arrages for it to parse some
	   command line arguments
	 */
	client = gnome_client_new_default();

  	/* Arrange to be told when something interesting happens.  */
  	gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
		      GTK_SIGNAL_FUNC (save_state), (gpointer) argv[0]);
  	gtk_signal_connect (GTK_OBJECT (client), "connect",
		      GTK_SIGNAL_FUNC (connect_client), NULL);

	gnome_init("gncal", &parser, argc, argv, 0, NULL);

	show_main_window();

	/* Initialse date to the current day */
	old_day = old_month = old_year = 0;
	get_system_date(&curr_day, &curr_month, &curr_year);
	update_today();

	prueba ();

	gtk_main();

	return 0;
}

static error_t
parse_an_arg (int key, char *arg, struct argp_state *state)
{
  if (key == DISCARD_KEY)
    {
      discard_session (arg);
      just_exit = 1;
      return 0;
    }

  /* We didn't recognize it.  */
  return ARGP_ERR_UNKNOWN;
}


/* Session Management routines */


static int
save_state (GnomeClient        *client,
	    gint                phase,
	    GnomeRestartStyle   save_style,
	    gint                shutdown,
	    GnomeInteractStyle  interact_style,
	    gint                fast,
	    gpointer            client_data)
{
  gchar *session_id;
  gchar *sess;
  gchar *buf;
  gchar *argv[3];
  gint x, y, w, h;

  session_id= gnome_client_get_id (client);

  /* The only state that gnome-hello has is the window geometry. 
     Get it. */
  gdk_window_get_geometry (app->window, &x, &y, &w, &h, NULL);

  /* Save the state using gnome-config stuff. */
  sess = g_copy_strings ("/gncal/Saved-Session-",
                         session_id,
                         NULL);

  buf = g_copy_strings ( sess, "/x", NULL);
  gnome_config_set_int (buf, x);
  g_free(buf);
  buf = g_copy_strings ( sess, "/y", NULL);
  gnome_config_set_int (buf, y);
  g_free(buf);
  buf = g_copy_strings ( sess, "/w", NULL);
  gnome_config_set_int (buf, w);
  g_free(buf);
  buf = g_copy_strings ( sess, "/h", NULL);
  gnome_config_set_int (buf, h);
  g_free(buf);

  gnome_config_sync();
  g_free(sess);

  /* Here is the real SM code. We set the argv to the parameters needed
     to restart/discard the session that we've just saved and call
     the gnome_session_set_*_command to tell the session manager it. */
  argv[0] = (char*) client_data;
  argv[1] = "--discard-session";
  argv[2] = session_id;
  gnome_client_set_discard_command (client, 3, argv);

  /* Set commands to clone and restart this application.  Note that we
     use the same values for both -- the session management code will
     automatically add whatever magic option is required to set the
     session id on startup.  */
  gnome_client_set_clone_command (client, 1, argv);
  gnome_client_set_restart_command (client, 1, argv);

  g_print("save state\n");
  return TRUE;
}

/* Connected to session manager. If restarted from a former session:
   reads the state of the previous session. Sets os_* (prepare_app
   uses them) */
void
connect_client (GnomeClient *client, gint was_restarted, gpointer client_data)
{
  gchar *session_id;

  /* Note that information is stored according to our *old*
     session id.  The id can change across sessions.  */
  session_id = gnome_client_get_previous_id (client);

  if (was_restarted && session_id != NULL)
    {
      gchar *sess;
      gchar *buf;

      restarted = 1;

      sess = g_copy_strings ("/gncal/Saved-Session-", session_id, NULL);

      buf = g_copy_strings ( sess, "/x", NULL);
      os_x = gnome_config_get_int (buf);
      g_free(buf);
      buf = g_copy_strings ( sess, "/y", NULL);
      os_y = gnome_config_get_int (buf);
      g_free(buf);
      buf = g_copy_strings ( sess, "/w", NULL);
      os_w = gnome_config_get_int (buf);
      g_free(buf);
      buf = g_copy_strings ( sess, "/h", NULL);
      os_h = gnome_config_get_int (buf);
      g_free(buf);
    }

  /* If we had an old session, we clean up after ourselves.  */
  if (session_id != NULL)
    discard_session (session_id);

  return;
}

void
discard_session (gchar *id)
{
  gchar *sess;

  sess = g_copy_strings ("/gncal/Saved-Session-", id, NULL);

  /* we use the gnome_config_get_* to work around a bug in gnome-config 
     (it's going under a redesign/rewrite, so i didn't correct it) */
  gnome_config_get_int ("/gncal/Bug/work-around=0");

  gnome_config_clean_section (sess);
  gnome_config_sync ();

  g_free (sess);
  return;
}

