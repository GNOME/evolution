/*
 * E-shell.c: Shell object for Evolution
 *
 * Authors:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 1999 Miguel de Icaza
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gtk/gtkmain.h>
#include <libgnome/libgnome.h>
#include "Evolution.h"
#include "e-util/e-util.h"
#include "e-shell.h"

#define PARENT_TYPE (bonobo_object_get_type ())

static BonoboObjectClass *e_shell_parent_class;
POA_Evolution_Shell__vepv eshell_vepv;

GtkType e_shell_get_type (void);

void
e_shell_new_appointment (EShell *eshell)
{
	printf ("Unimplemented function invoked: %s\n", __FUNCTION__);
}

void
e_shell_new_meeting_request (EShell *eshell)
{
	printf ("Unimplemented function invoked: %s\n", __FUNCTION__);
}

void
e_shell_new_task (EShell *eshell)
{
	printf ("Unimplemented function invoked: %s\n", __FUNCTION__);
}

void
e_shell_new_task_request (EShell *eshell)
{
	printf ("Unimplemented function invoked: %s\n", __FUNCTION__);
}

void
e_shell_new_contact (EShell *eshell)
{
	printf ("Unimplemented function invoked: %s\n", __FUNCTION__);
}

void
e_shell_new_mail_message (EShell *eshell)
{
	printf ("Unimplemented function invoked: %s\n", __FUNCTION__);
}

void
e_shell_new_distribution_list (EShell *eshell)
{
	printf ("Unimplemented function invoked: %s\n", __FUNCTION__);
}

void
e_shell_new_journal_entry (EShell *eshell)
{
	printf ("Unimplemented function invoked: %s\n", __FUNCTION__);
}

void
e_shell_new_note (EShell *eshell)
{
	printf ("Unimplemented function invoked: %s\n", __FUNCTION__);
}

static void
EShell_cmd_new (PortableServer_Servant servant,
		const Evolution_Shell_NewType type,
		CORBA_Environment *ev)
{
	EShell *eshell = E_SHELL (bonobo_object_from_servant (servant));

	switch (type){
	case Evolution_Shell_APPOINTMENT:
		e_shell_new_appointment (eshell);
		break;
		
	case Evolution_Shell_MEETING_REQUEST:
		e_shell_new_meeting_request (eshell);
		break;

	case Evolution_Shell_TASK:
		e_shell_new_task (eshell);
		break;

	case Evolution_Shell_TASK_REQUEST:
		e_shell_new_task_request (eshell);
		break;

	case Evolution_Shell_CONTACT:
		e_shell_new_contact (eshell);
		break;

	case Evolution_Shell_MAIL_MESSAGE:
		e_shell_new_mail_message (eshell);
		break;

	case Evolution_Shell_DISTRIBUTION_LIST:
		e_shell_new_distribution_list (eshell);
		break;

	case Evolution_Shell_JOURNAL_ENTRY:
		e_shell_new_journal_entry (eshell);
		break;
		
	case Evolution_Shell_NOTE:
		e_shell_new_note (eshell);
		break;
		
	default:
	}
}

static POA_Evolution_Shell__epv *
e_shell_get_epv (void)
{
	POA_Evolution_Shell__epv *epv;

	epv = g_new0 (POA_Evolution_Shell__epv, 1);

	epv->new = EShell_cmd_new;

	return epv;
}

static void
init_e_shell_corba_class (void)
{
	eshell_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	eshell_vepv.Evolution_Shell_epv = e_shell_get_epv ();
}

static void
es_destroy_default_folders (EShell *eshell)
{
	gtk_object_unref (GTK_OBJECT (eshell->default_folders.inbox));
	gtk_object_unref (GTK_OBJECT (eshell->default_folders.outbox));
	gtk_object_unref (GTK_OBJECT (eshell->default_folders.drafts));
	gtk_object_unref (GTK_OBJECT (eshell->default_folders.calendar));
	gtk_object_unref (GTK_OBJECT (eshell->default_folders.tasks));
}

static void
e_shell_destroy (GtkObject *object)
{
	EShell *eshell = E_SHELL (object);

	gtk_object_unref (GTK_OBJECT (eshell->shortcut_bar));
	es_destroy_default_folders (eshell);
	
	GTK_OBJECT_CLASS (e_shell_parent_class)->destroy (object);
}

static void
e_shell_class_init (GtkObjectClass *object_class)
{
	e_shell_parent_class = gtk_type_class (PARENT_TYPE);
	init_e_shell_corba_class ();

	object_class->destroy = e_shell_destroy;
}

static void
e_shell_destroy_views (EShell *eshell)
{

	/*
	 * Notice that eshell->views is updated by the various views
	 * during unregistration
	 */
	while (eshell->views){
		EShellView *view = eshell->views->data;

		gtk_object_destroy (GTK_OBJECT (view));
	}
}

void
e_shell_quit (EShell *eshell)
{
	g_return_if_fail (eshell != NULL);
	g_return_if_fail (E_IS_SHELL (eshell));

	e_shell_destroy_views (eshell);
	
	gtk_main_quit ();
}

static CORBA_Object
create_corba_eshell (BonoboObject *object)
{
	POA_Evolution_Shell *servant;
	CORBA_Environment ev;

	servant = (POA_Evolution_Shell *)g_new0 (BonoboObjectServant, 1);
	servant->vepv = &eshell_vepv;

	CORBA_exception_init (&ev);

	POA_Evolution_Shell__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		CORBA_exception_free (&ev);
		g_free (servant);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);

	return bonobo_object_activate_servant (object, servant);
}

static void
e_shell_setup_default_folders (EShell *eshell)
{
	eshell->default_folders.summary = e_folder_new (
		E_FOLDER_MAIL, "internal:summary", _("Today"), _("Executive Summary"),
		NULL, "internal:");
	eshell->default_folders.inbox = e_folder_new (
		E_FOLDER_MAIL, "internal:inbox", _("Inbox"), _("New mail messages"),
		NULL, "internal:mail_view");
	eshell->default_folders.outbox = e_folder_new (
		E_FOLDER_MAIL, "internal:outbox", _("Sent messages"), _("Sent mail messages"),
		NULL, "internal:mail_view");
	eshell->default_folders.drafts = e_folder_new (
		E_FOLDER_MAIL, "internal:drafts", _("Drafts"), _("Draft mail messages"),
		NULL, "internal:mail_view");
	eshell->default_folders.calendar = e_folder_new (
		E_FOLDER_CALENDAR, "internal:personal_calendar", _("Calendar"), _("Your calendar"),
		NULL, "internal:calendar_daily");
	eshell->default_folders.contacts = e_folder_new (
		E_FOLDER_CONTACTS, "internal:personal_contacts", _("Contacts"), _("Your contacts list"),
		NULL, "internal:contact_view");
	eshell->default_folders.tasks = e_folder_new (
		E_FOLDER_TASKS, "internal:personal_calendar", _("Tasks"), _("Tasks list"),
		NULL, "internal:tasks_view");
}

static EShortcutGroup *
setup_main_shortcuts (EShell *eshell)
{
	EShortcutGroup *m;

	m = e_shortcut_group_new (_("Main Shortcuts"), FALSE);
	e_shortcut_group_append (m, e_shortcut_new (eshell->default_folders.summary));
	e_shortcut_group_append (m, e_shortcut_new (eshell->default_folders.inbox));
	e_shortcut_group_append (m, e_shortcut_new (eshell->default_folders.calendar));
	e_shortcut_group_append (m, e_shortcut_new (eshell->default_folders.contacts));
	e_shortcut_group_append (m, e_shortcut_new (eshell->default_folders.tasks));

	return m;
}

static EShortcutGroup *
setup_secondary_shortcuts (EShell *eshell)
{
	EShortcutGroup *sec;

	sec = e_shortcut_group_new (_("Other Shortcuts"), TRUE);
	
	e_shortcut_group_append (sec, e_shortcut_new (eshell->default_folders.drafts));
	e_shortcut_group_append (sec, e_shortcut_new (eshell->default_folders.outbox));

	return sec;
}

static void
e_shell_setup_default_shortcuts (EShell *eshell)
{
	eshell->shortcut_bar = e_shortcut_bar_model_new ();
	e_shortcut_bar_model_append (
		eshell->shortcut_bar,
		setup_main_shortcuts (eshell));
	e_shortcut_bar_model_append (
		eshell->shortcut_bar,
		setup_secondary_shortcuts (eshell));
}

static void
e_shell_init (GtkObject *object)
{
	EShell *eshell = E_SHELL (object);
	
	e_shell_setup_default_folders (eshell);
	e_shell_setup_default_shortcuts (eshell);
}

static void
e_shell_construct (EShell *eshell, Evolution_Shell corba_eshell)
{
	bonobo_object_construct (BONOBO_OBJECT (eshell), corba_eshell);
}

EShell *
e_shell_new (void)
{
	Evolution_Shell corba_eshell;
	EShell *eshell;

	eshell = gtk_type_new (e_shell_get_type ());

	corba_eshell = create_corba_eshell (BONOBO_OBJECT (eshell));
	if (corba_eshell == CORBA_OBJECT_NIL){
		gtk_object_destroy (GTK_OBJECT (eshell));
		return NULL;
	}
	
	e_shell_construct (eshell, corba_eshell);

	return eshell;
}

void
e_shell_register_view (EShell *eshell, EShellView *eshell_view)
{
	g_return_if_fail (eshell != NULL);
	g_return_if_fail (E_IS_SHELL (eshell));
	g_return_if_fail (eshell_view != NULL);

	eshell->views = g_slist_prepend (eshell->views, eshell_view);
}

void
e_shell_unregister_view (EShell *eshell, EShellView *eshell_view)
{
	g_return_if_fail (eshell != NULL);
	g_return_if_fail (E_IS_SHELL (eshell));
	g_return_if_fail (eshell_view != NULL);

	eshell->views = g_slist_remove (eshell->views, eshell_view);
}

E_MAKE_TYPE (e_shell, "EShell", EShell, e_shell_class_init, e_shell_init, PARENT_TYPE);



     
