#ifndef E_SHELL_H
#define E_SHELL_H

#include <bonobo/gnome-object.h>
#include "evolution.h"
#include "e-folder.h"

#define E_SHELL_GOAD_ID         "GOADID:GNOME:Evolution:Shell:1.0"
#define E_SHELL_FACTORY_GOAD_ID "GOADID:GNOME:Evolution:ShellFactory:1.0"

#define E_SHELL_TYPE        (e_shell_get_type ())
#define E_SHELL(o)          (GTK_CHECK_CAST ((o), E_SHELL_TYPE, EShell))
#define E_SHELL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_SHELL_TYPE, EShellClass))
#define E_IS_SHELL(o)       (GTK_CHECK_TYPE ((o), E_SHELL_TYPE))
#define E_IS_SHELL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SHELL_TYPE))

struct _EShell {
	GnomeObject base_object;

	/* A list of EShellViews */
	GSList *views;

	struct {
		EFolder *inbox;
		EFolder *outbox;
		EFolder *drafts;
		EFolder *calendar;
		EFolder *tasks;
	} default_folders;
};

typedef struct {
	GnomeObjectClass parent_class;
} EShellClass;

EShell     *e_shell_new             (void);
void        e_shell_register_view   (EShell *eshell, EShellView *eshell_view);
void        e_shell_unregister_view (EShell *eshell, EShellView *eshell_view);

/*
 * New
 */
void e_shell_new_appointment       (EShell *eshell);
void e_shell_new_meeting_request   (EShell *eshell);
void e_shell_new_task              (EShell *eshell);
void e_shell_new_task_request      (EShell *eshell);
void e_shell_new_contact           (EShell *eshell);
void e_shell_new_mail_message      (EShell *eshell);
void e_shell_new_distribution_list (EShell *eshell);
void e_shell_new_journal_entry     (EShell *eshell);
void e_shell_new_note              (EShell *eshell);

void e_shell_quit                  (EShell *eshell);

#endif /* EVOLUTION_SHELL_H */
