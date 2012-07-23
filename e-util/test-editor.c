/*
 * e-editor-test.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#include <gtk/gtk.h>
#include <e-util/e-util.h>

static WebKitWebView *
open_inspector (WebKitWebInspector *inspector,
		WebKitWebView *webview,
		gpointer user_data)
{
	GtkWidget *window;
	GtkWidget *inspector_view;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	inspector_view = webkit_web_view_new ();

	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (inspector_view));

	gtk_widget_set_size_request (window, 600, 480);
	gtk_widget_show (window);

	return WEBKIT_WEB_VIEW (inspector_view);
}

gint main (gint argc,
	   gchar **argv)
{
        GtkWidget *window;
	GtkWidget *editor;
	WebKitWebInspector *inspector;

	gtk_init (&argc, &argv);

        window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_widget_set_size_request (window, 600, 400);
        g_signal_connect_swapped (window, "destroy",
                G_CALLBACK (gtk_main_quit), NULL);

	editor = GTK_WIDGET (e_editor_widget_new ());
        gtk_container_add (GTK_CONTAINER (window), editor);

        gtk_widget_show_all (window);

	inspector = webkit_web_view_get_inspector (WEBKIT_WEB_VIEW (editor));
	g_signal_connect (inspector, "inspect-web-view",
			  G_CALLBACK (open_inspector), NULL);
	/*
	webkit_web_view_load_html_string (
		WEBKIT_WEB_VIEW (editor),
		"<html><head></head><body>\n"
		"<table border=1 width=100%>\n"
		"  <tr><td></td><td></td><td></td></tr>\n"
		"  <tr><td></td><td></td><td></td></tr>\n"
		"  <tr><td></td><td></td><td></td></tr>\n"
		"</table></body></html>", NULL);
	*/

        gtk_main ();

        return 0;
}
