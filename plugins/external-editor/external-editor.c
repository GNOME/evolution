/* Sankar P - <psankar@novell.com> - GPL V3 or Later */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <mail/em-menu.h>
#include <mail/em-config.h>
#include <mail/em-composer-utils.h>
#include <mail/mail-config.h>
#include <e-util/e-error.h>
#include <e-msg-composer.h>

#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include <stdlib.h>
#include <string.h>

#include <gconf/gconf-client.h>

#define d(x) 

#define EDITOR_GCONF_KEY "/apps/evolution/eplugin/external-editor/editor-command"

void org_gnome_external_editor (EPlugin *ep, EMMenuTargetSelect *select);
void ee_editor_command_changed (GtkWidget *textbox);
GtkWidget * e_plugin_lib_get_configure_widget (EPlugin *epl);
void async_external_editor (GArray *array);
static gboolean show_composer_dialog (EMsgComposer *composer);

/* Utility function to convert an email address to CamelInternetAddress.
May be this should belong to CamelInternetAddress.h file itself. */
static CamelInternetAddress * convert_to_camel_internet_address (char * emails)
{
	CamelInternetAddress *cia = camel_internet_address_new();
	gchar **address_tokens = NULL;
	int i;

	d(printf ("\n\aconvert called with : [%s] \n\a", emails));

	emails = g_strstrip (emails);

	if (emails && strlen (emails) > 1) {
		address_tokens = g_strsplit (emails, ",", 0);

		if (address_tokens) {
			for (i = 0; address_tokens[i]; ++i) {
				camel_internet_address_add (cia, " ", address_tokens [i]);
				d(printf ("\nAdding camel_internet_address[%s] \n", address_tokens [i]));
			}
			g_strfreev (address_tokens);

			g_free (emails);
			return cia;
		}
	}
	camel_object_unref (cia);
	g_free (emails);
	return NULL;
}

void ee_editor_command_changed (GtkWidget *textbox)
{
	const char *editor;
	GConfClient *gconf;

	editor = gtk_entry_get_text (GTK_ENTRY(textbox));
	d(printf ("\n\aeditor is : [%s] \n\a", editor));
	
	/* gconf access for every key-press. Sucky ? */
	gconf = gconf_client_get_default ();
	gconf_client_set_string (gconf, EDITOR_GCONF_KEY, editor, NULL);
	g_object_unref (gconf);
}

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *epl)
{
	GtkWidget *vbox, *textbox, *label, *help;
	GConfClient *gconf;
	char *editor;

	vbox = gtk_vbox_new (FALSE, 10);
	textbox = gtk_entry_new ();
	label = gtk_label_new (_("Command to be executed to launch the editor: "));
	help = gtk_label_new (_("For Emacs use \"xemacs\"\nFor VI use \"gvim\""));
	gconf = gconf_client_get_default ();

	editor = gconf_client_get_string (gconf, EDITOR_GCONF_KEY, NULL);
	if (editor) {
		gtk_entry_set_text (GTK_ENTRY(textbox), editor);
		g_free (editor);
	}
	g_object_unref (gconf);

	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), textbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), help, FALSE, FALSE, 0);

	g_signal_connect (textbox, "changed", G_CALLBACK(ee_editor_command_changed), textbox);
	
	gtk_widget_show_all (vbox);
	return vbox;
}

void
async_external_editor (GArray *array)
{
	char *filename = NULL;
	gchar *argv[5];
	int status = 0;

	argv[0] = g_array_index (array, gpointer, 0);
	argv[1] = g_array_index (array, gpointer, 1);
	argv[2] = NULL;

	filename = g_strdup (argv[1]);

	if (!g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, &status, NULL))
	{
		g_warning ("Unable to launch %s: ", argv[0]);
		e_error_run (NULL, "org.gnome.evolution.plugins.external-editor:editor-not-launchable", NULL);
		g_free (filename);
		return ;
	}
	
	if (WEXITSTATUS (status) != 0) {
		d(printf ("\n\nsome problem here with external editor\n\n"));
		return ;
	} else {
		gchar *buf;
		CamelMimeMessage *message;
		EMsgComposer *composer;

		message = camel_mime_message_new ();

		if (g_file_get_contents (filename, &buf, NULL, NULL)) {
			gchar **tokens;
			int i, j;

			tokens = g_strsplit (buf, "###|||", 6);

			for (i = 1; tokens[i]; ++i) {

				for (j = 0; tokens[i][j] && tokens[i][j] != '\n'; ++j) {
					tokens [i][j] = ' ';
				}

				if (tokens[i][j] == '\n')
					tokens[i][j] = ' ';

				d(printf ("\nstripped off token[%d] is : %s \n", i, tokens[i]));
			}

			camel_mime_message_set_recipients (message, "To", convert_to_camel_internet_address(g_strchug(g_strdup(tokens[1]))));
			camel_mime_message_set_recipients (message, "Cc", convert_to_camel_internet_address(g_strchug(g_strdup(tokens[2]))));
			camel_mime_message_set_recipients (message, "Bcc", convert_to_camel_internet_address(g_strchug(g_strdup(tokens[3]))));
			camel_mime_message_set_subject (message, tokens[4]);
			camel_mime_part_set_content ((CamelMimePart *)message, tokens [5], strlen (tokens [5]), "text/plain");


			/* FIXME: We need to make mail-remote working properly.
			   So that we neednot invoke composer widget at all.

			   May be we can do it now itself by invoking local CamelTransport.
			   But all that is not needed for the first release.

			   People might want to format mails using their editor (80 cols width etc.) 
			   But might want to use evolution addressbook for auto-completion etc.
			   So starting the composer window anyway.
			 */

			composer = e_msg_composer_new_with_message (message);
			g_signal_connect (GTK_OBJECT (composer), "send", G_CALLBACK (em_utils_composer_send_cb), NULL);
			g_signal_connect (GTK_OBJECT (composer), "save-draft", G_CALLBACK (em_utils_composer_save_draft_cb), NULL);
			
			/* Composer cannot be shown in any random thread. Should happen in main thread */
			g_idle_add ((GSourceFunc) show_composer_dialog, composer);

			g_strfreev (tokens);

			/* We no longer need that temporary file */
			g_remove (filename);
			g_free (filename);
		}
	}
}

static gboolean
show_composer_dialog (EMsgComposer *composer)
{
	gtk_widget_show (GTK_WIDGET(composer));
	return FALSE;
}

void org_gnome_external_editor (EPlugin *ep, EMMenuTargetSelect *select)
{
	/* The template to be used in the external editor */

	/* README: I have not marked this for translation. 
	   As I might change this string to make it more meaningful and friendlier based on feedback. */

	char template[] = "###|||Insert , seperated TO addresses below this line. Do not edit or delete this line. Optional field\n\n###||| Insert , seperated CC addresses below this line. Do not edit or delete this line. Optional field\n\n###|||Insert , seperated BCC addresses below this line. Do not edit or delete this line. Optional field\n\n###|||Insert SUBJECT below this line. Do not edit or delete this line. Optional field\n\n###|||Insert BODY of mail below this line. Do not edit or delete this line.\n\n";

	gint fd;
	char *filename = NULL;
	char *editor = NULL;
	GConfClient *gconf;
	GArray *array;

	d(printf ("\n\nexternal_editor plugin is launched \n\n"));

	fd = g_file_open_tmp (NULL, &filename, NULL);
	if (fd > 0) {
		close (fd);
		/* Push the template contents to the intermediate file */
		g_file_set_contents (filename, template, strlen (template), NULL);
		d(printf ("\n\aTemporary-file Name is : [%s] \n\a", filename));
	} else {
		g_warning ("Temporary file fd is null");
		e_error_run (NULL, "org.gnome.evolution.plugins.external-editor:no-temp-file", NULL);
		return ;
	}

	gconf = gconf_client_get_default ();
	editor = gconf_client_get_string (gconf, EDITOR_GCONF_KEY, NULL);
	if (!editor) {

		if (! (editor = g_strdup(g_getenv ("EDITOR"))) )
			/* Make gedit the default external editor,
			   if the default schemas are not installed
			   and no $EDITOR is set. */
			editor = g_strdup("gedit"); 
	}
	g_object_unref (gconf);

	array = g_array_sized_new (TRUE, TRUE, sizeof (gpointer), 2 * sizeof(gpointer));
	array = g_array_append_val (array, editor);
	array = g_array_append_val (array, filename);

	g_thread_create ( (GThreadFunc) async_external_editor, array, FALSE, NULL);

	return ; 
}
