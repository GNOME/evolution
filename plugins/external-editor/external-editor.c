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

#include <sys/stat.h>
#include <sys/wait.h>

#include <stdlib.h>
#include <string.h>

#define d(x) x

#define TEMPORARY_FILE "/tmp/evolution-composer"

void org_gnome_external_editor (EPlugin *ep, EMMenuTargetSelect *select);

/* Utility function to convert an email address to CamelInternetAddress.
May be this should belong to CamelInternetAddress file itself. */

static CamelInternetAddress * convert_to_camel_internet_address (char * emails)
{
	CamelInternetAddress *cia = camel_internet_address_new();
	gchar **address_tokens = NULL;
	int i;

	if (emails && strlen (emails) > 1) {
		address_tokens = g_strsplit (emails, ",", 0);

		if (address_tokens) {
			for (i = 1; address_tokens[i]; ++i) {
				camel_internet_address_add (cia, " ", address_tokens [i]);
				d(printf ("\nAdding camel_internet_address[%s] \n", address_tokens [i]));
			}

			return cia;
		}
	}
	camel_object_unref (cia);
	return NULL;
}

void org_gnome_external_editor (EPlugin *ep, EMMenuTargetSelect *select)
{
	d(printf ("\n\nexternal_editor plugin is launched \n\n"));

	/* The template to be used in the external editor */
	char template[] = "###|||Insert , seperated TO addresses below this line. Do not delete this line. Optional field\n\n###||| Insert , seperated CC addresses below this line. Do not delete this line. Optional field\n\n###|||Insert , seperated BCC addresses below this line. Do not delete this line. Optional field\n\n###|||Insert SUBJECT below this line. Do not delete this line. Optional field\n\n###|||Insert BODY of mail below this line. Do not delete this line.\n\n";


	/* Push the template contents to the intermediate file */
	g_file_set_contents (TEMPORARY_FILE, template, strlen (template), NULL);

	char *editor;

	editor = (char *) g_getenv ("EDITOR");
	if (!editor)
		editor = "gvim";

#if 1
	int status = 0;
	gchar *argv[4];

	argv[0] = editor;

	/*  README: The -- params should come via the "Configure" option */
	argv[1] = "--nofork";

	argv[2] = TEMPORARY_FILE;
	argv[3] = NULL;

	/* FIXME: I guess NULL should do fine instead of /usr/bin */
	if (!g_spawn_sync ("/usr/bin", argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, &status, NULL))
	{
		g_warning ("Unable to launch %s: ", argv[0]);
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

		d(printf ("\n\nexternal editor works like a charm \n\n"));

		if (g_file_get_contents (TEMPORARY_FILE, &buf, NULL, NULL)) {
			gchar **tokens;
			int i, j;

			tokens = g_strsplit (buf, "###|||", 6);

			for (i = 1; tokens[i]; ++i) {

				for (j = 0; tokens[i][j] && tokens[i][j] != '\n'; ++j) {
					tokens [i][j] = ' ';
				}

				if (tokens[i][j] == '\n')
					tokens[i][j] = ' ';

				g_strchug(tokens[i]);

				d(printf ("\nstripped off token[%d] is : %s \n", i, tokens[i]));
			}

			camel_mime_message_set_recipients (message, "To", convert_to_camel_internet_address(tokens[1]));
			camel_mime_message_set_recipients (message, "Cc", convert_to_camel_internet_address(tokens[2]));
			camel_mime_message_set_recipients (message, "Bcc", convert_to_camel_internet_address(tokens[3]));

			camel_mime_message_set_subject (message, tokens[4]);

			camel_mime_part_set_content ((CamelMimePart *)message, tokens [5], strlen (tokens [5]), "text/plain");


			/* FIXME: We need to make mail-remote working properly.
			   So that we neednot invoke composer widget at all.
			   May be we can do it now itself by invoking local CamelTransport.
			   But all that is not needed for the first release.
			 */

			composer = e_msg_composer_new_with_message (message);
			g_signal_connect (GTK_OBJECT (composer), "send", G_CALLBACK (em_utils_composer_send_cb), NULL);

			g_signal_connect (GTK_OBJECT (composer), "save-draft", G_CALLBACK (em_utils_composer_save_draft_cb), NULL);

			gtk_widget_show (GTK_WIDGET (composer));
		}
	}
#else
	char *query;

	query = g_strdup_printf ("%s /tmp/evolution-composer", editor);
	system (query);
	g_free (query);

#endif
}
