#ifndef E_SHELL_VIEW_H
#define E_SHELL_VIEW_H

#include <bonobo/gnome-object.h>
#include <bonobo/gnome-ui-handler.h>
#include "e-shell.h"

#define E_SHELL_VIEW_TYPE        (e_shell_view_get_type ())
#define E_SHELL_VIEW(o)          (GTK_CHECK_CAST ((o), E_SHELL_VIEW_TYPE, EShellView))
#define E_SHELL_VIEW_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_SHELL_VIEW_TYPE, EShellViewClass))
#define E_IS_SHELL_VIEW(o)       (GTK_CHECK_TYPE ((o), E_SHELL_VIEW_TYPE))
#define E_IS_SHELL_VIEW_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SHELL_VIEW_TYPE))

struct _EShellView {
	GnomeApp parent;

	/* Pointer to our model */
	EShell  *eshell;

	/* Our user interface handler */
	GnomeUIHandler *uih;
};

typedef struct {
	GnomeAppClass parent_class;
} EShellViewClass;

GtkWidget *e_shell_view_new          (EShell *eshell);
GtkType    e_shell_view_get_type     (void);

void       e_shell_view_new_folder   (EShellView *esv);
void       e_shell_view_new_shortcut (EShellView *esv);

#endif /* E_SHELL_VIEW_H */
