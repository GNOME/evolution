#ifndef E_SHELL_SHORTCUT_H
#define E_SHELL_SHORTCUT_H

#include "e-shell-view.h"


void shortcut_bar_item_selected (EShortcutBar *shortcut_bar,
				 GdkEvent *event, gint group_num, gint item_num,
				 EShellView *eshell_view);

#endif /* E_SHELL_SHORTCUT_H */
