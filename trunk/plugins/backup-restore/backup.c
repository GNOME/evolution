#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>

#define EVOLUTION "evolution-" BASE_VERSION
#define EVOLUTION_DIR "~/.evolution/"
#define EVOLUTION_DIR_BACKUP "~/.evolution-old/"
#define GCONF_DUMP_FILE "backup-restore-gconf.xml"
#define GCONF_DUMP_PATH EVOLUTION_DIR GCONF_DUMP_FILE
#define GCONF_DIR "/apps/evolution"
#define ARCHIVE_NAME "evolution-backup.tar.gz"

static gboolean backup_op = FALSE;
static gboolean restore_op = FALSE;
static gboolean check_op = FALSE;
static gboolean restart_arg = FALSE;

#define d(x) x

/* #define s(x) system (x) */
#define s(x) G_STMT_START { g_message (x); system (x); } G_STMT_END

static void
backup (const char *filename) 
{
	char *command;
	
	/* FIXME Will the versioned setting always work? */
	s (EVOLUTION " --force-shutdown");

	s ("gconftool-2 --dump " GCONF_DIR " > " GCONF_DUMP_PATH);

	/* FIXME stay on this file system ,other options?" */
	/* FIXME compression type?" */
	/* FIXME date/time stamp?" */
	/* FIXME archive location?" */
	command = g_strdup_printf ("cd ~ && tar zpcf %s .evolution", filename);
	s (command);
	g_free (command);

	if (restart_arg)
		s (EVOLUTION);
}

static void
restore (const char *filename) 
{
	char *command;
	
	/* FIXME Will the versioned setting always work? */
	s (EVOLUTION " --force-shutdown");

	s ("mv " EVOLUTION_DIR " " EVOLUTION_DIR_BACKUP);

	command = g_strdup_printf ("cd ~ && tar zxf %s", filename);
	s (command);
	g_free (command);

	s ("gconftool-2 --load " GCONF_DUMP_PATH);
	s ("rm -rf " GCONF_DUMP_PATH);
	s ("rm -rf " EVOLUTION_DIR_BACKUP);

	if (restart_arg)
		s (EVOLUTION);
}

static void
check (const char *filename) 
{
	char *command;
	int result;

	command = g_strdup_printf ("tar ztf %s | grep -e \"^\\.evolution/$\"", filename);
	result = system (command);
	g_free (command);

	g_message ("First result %d", result);
	if (result)
		exit (result);

	command = g_strdup_printf ("tar ztf %s | grep -e \"^\\.evolution/%s$\"", filename, GCONF_DUMP_FILE);
	result = system (command);
	g_free (command);

	g_message ("Second result %d", result);

	exit (result);
}

int
main (int argc, char **argv)
{
	GValue popt_context_value = { 0, };
	GnomeProgram *program;
	poptContext popt_context;
	const char **args;

	struct poptOption options[] = {
		{ "backup", '\0', POPT_ARG_NONE, &backup_op, 0, 
		  N_("Backup Evolution directory"), NULL },
		{ "restore", '\0', POPT_ARG_NONE, &restore_op, 0, 
		  N_("Restore Evolution directory"), NULL },
		{ "check", '\0', POPT_ARG_NONE, &check_op, 0, 
		  N_("Check Evolution archive"), NULL },
		{ "restart", '\0', POPT_ARG_NONE, &restart_arg, 0, 
		  N_("Restart Evolution"), NULL },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	program = gnome_program_init (PACKAGE, VERSION, LIBGNOME_MODULE, argc, argv, 
				      GNOME_PROGRAM_STANDARD_PROPERTIES,
				      GNOME_PARAM_POPT_TABLE, options,
				      NULL);

	g_value_init (&popt_context_value, G_TYPE_POINTER);
	g_object_get_property (G_OBJECT (program), GNOME_PARAM_POPT_CONTEXT, &popt_context_value);
	popt_context = g_value_get_pointer (&popt_context_value);
	args = poptGetArgs (popt_context);

	if (args != NULL) {
		const char **p;
		
		for (p = args; *p != NULL; p++) {
			if (backup_op) {
				d(g_message ("Backing up to %s", (char *) *p));
				backup ((char *) *p);
			} else if (restore_op) {
				d(g_message ("Restoring from %s", (char *) *p));
				restore ((char *) *p);
			} else if (check_op) {
				d(g_message ("Checking %s", (char *) *p));
				check ((char *) *p);			
			}
		}
	}

	g_value_unset (&popt_context_value);
	
	return 0;
}
