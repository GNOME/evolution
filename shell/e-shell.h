#ifndef E_SHELL_H
#define E_SHELL_H

#include <gnome.h>
#include <bonobo/gnome-object.h>

#define E_SHELL_GOAD_ID         "GOADID:GNOME:Evolution:Shell:1.0"
#define E_SHELL_FACTORY_GOAD_ID "GOADID:GNOME:Evolution:ShellFactory:1.0"

#define E_SHELL_TYPE        (e_shell_get_type ())
#define E_SHELL(o)          (GTK_CHECK_CAST ((o), E_SHELL_TYPE, EShell))
#define E_SHELL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_SHELL_TYPE, EShellClass))
#define E_IS_SHELL(o)       (GTK_CHECK_TYPE ((o), E_SHELL_TYPE))
#define E_IS_SHELL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SHELL_TYPE))

typedef struct {
	GnomeObject base_object;

	GtkWidget *gnome_app;

	char *base_uri;
} EShell;

typedef struct {
	GnomeObjectClass *parent_class;
} EShellClass;

EShell     *e_shell_new           (const char *base_uri);
void        e_shell_set_base_uri (EShell *eshell, const char *base_uri);
const char *e_shell_get_base_uri (EShell *eshell);

#endif /* EVOLUTION_SHELL_H */
