/*
 * clist.c: All the good stuf to work with the clists that are used for the
 * tasklist.
 *
 * This file is largely based upon GTT code by Eckehard Berns.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <config.h>
#include <gnome.h>

static void
select_row(GtkCList *clist, gint row, gint col, GdkEventButton *event)
{
	if (!event) return;

	g_print("select_row, row=%d col=%d button=%d\n", row, col, event->button);
}

static void
unselect_row(GtkCList *clist, gint row, gint col, GdkEventButton *event)
{
	if (!event) return;

	g_print("unselect_row, row=%d col=%d button=%d\n", row, col, event->button);
}

static void
click_column(GtkCList *clist, gint col)
{

	g_print("click_column, col=%d\n ", col);
}


	

GtkWidget * create_clist(void)
{
	GtkStyle *style;
	GdkGCValues vals;

	GtkWidget *clist;
	char *titles[2] = {
		N_("Time"),
		N_("Event")
	};

	titles[0] = _(titles[0]);
	titles[1] = _(titles[1]);
	clist = gtk_clist_new_with_titles(2,titles);
	gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_SINGLE);
	gtk_clist_set_column_justification(GTK_CLIST(clist), 0, GTK_JUSTIFY_CENTER);
	style = gtk_widget_get_style(clist);
	g_return_val_if_fail(style != NULL, NULL);
	gdk_gc_get_values(style->fg_gc[0], &vals);
	gtk_clist_set_column_width(GTK_CLIST(clist), 0, gdk_string_width(vals.font, "00:00"));
	gtk_clist_set_policy(GTK_CLIST(clist),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_signal_connect(GTK_OBJECT(clist), "select_row",
				GTK_SIGNAL_FUNC(select_row), NULL);
	gtk_signal_connect(GTK_OBJECT(clist), "click_column",
				GTK_SIGNAL_FUNC(click_column), NULL);
	gtk_signal_connect(GTK_OBJECT(clist), "unselect_row",
				GTK_SIGNAL_FUNC(unselect_row), NULL);
	return clist;
}

void
setup_clist(GtkWidget *clist)
{
	char buf1[10];
	char buf2[1000] = "Programming GNOME";
	char *tmp[2] = { buf1, buf2 };
	int i, row;

 	gtk_clist_freeze(GTK_CLIST(clist));
 	gtk_clist_clear(GTK_CLIST(clist));
 	for (i=0; i < 24; i++) {
 		sprintf(buf1, "%d:00", i);
	 	row = gtk_clist_append(GTK_CLIST(clist), tmp);
	 }
 	gtk_clist_thaw(GTK_CLIST(clist));

}	
