#ifndef E_SHELL_VIEW_H
#define E_SHELL_VIEW_H

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-ui-handler.h>
#include "e-shell.h"

#define E_SHELL_VIEW_TYPE        (e_shell_view_get_type ())
#define E_SHELL_VIEW(o)          (GTK_CHECK_CAST ((o), E_SHELL_VIEW_TYPE, EShellView))
#define E_SHELL_VIEW_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_SHELL_VIEW_TYPE, EShellViewClass))
#define E_IS_SHELL_VIEW(o)       (GTK_CHECK_TYPE ((o), E_SHELL_VIEW_TYPE))
#define E_IS_SHELL_VIEW_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SHELL_VIEW_TYPE))

typedef struct _EShellViewPrivate EShellViewPrivate;

struct _EShellView {
	GnomeApp parent;

	/* Pointer to our model */
	EShell  *eshell;

	/* Our user interface handler */
	BonoboUIHandler *uih;


	EFolder   *efolder;

	gboolean  shortcut_displayed;
	GtkWidget *shortcut_hpaned;
	GtkWidget *shortcut_bar;
	GtkWidget *contents;

	EShellViewPrivate *priv;
};

typedef struct {
	GnomeAppClass parent_class;
} EShellViewClass;

GtkWidget *e_shell_view_new          (EShell *eshell, EFolder *folder,
				      gboolean show_shortcut_bar);
GtkType    e_shell_view_get_type     (void);

void       e_shell_view_new_folder   (EShellView *esv);
void       e_shell_view_new_shortcut (EShellView *esv);

void       e_shell_view_set_view     (EShellView *eshell_view,
				      EFolder *efolder);

void e_shell_view_display_shortcut_bar (EShellView *eshell_view, gboolean display);

#endif /* E_SHELL_VIEW_H */
