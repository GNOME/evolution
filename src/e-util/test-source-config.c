#include <stdlib.h>
#include <gtk/gtk.h>

#include <libedataserver/libedataserver.h>

#include "e-misc-utils.h"
#include "e-source-config-dialog.h"

static void
dialog_response (GtkDialog *dialog,
                 gint response_id)
{
	gtk_main_quit ();
}

gint
main (gint argc,
      gchar **argv)
{
	ESourceRegistry *registry;
	ESource *source = NULL;
	GtkWidget *config;
	GtkWidget *dialog;
	GError *error = NULL;

	gtk_init (&argc, &argv);

	registry = e_source_registry_new_sync (NULL, &error);

	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		exit (EXIT_FAILURE);
	}

	if (argc > 1) {
		source = e_source_registry_ref_source (registry, argv[1]);
		if (source == NULL) {
			g_printerr ("No such UID: %s\n", argv[1]);
			exit (EXIT_FAILURE);
		}
	}

	config = e_source_config_new (registry, source);
	dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

	g_signal_connect (
		dialog, "response",
		G_CALLBACK (dialog_response), NULL);

	gtk_widget_show (config);
	gtk_widget_show (dialog);

	g_object_unref (source);

	gtk_main ();

	e_misc_util_free_global_memory ();

	return 0;
}
