/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef MODULE_RSS_H
#define MODULE_RSS_H

#include <glib-object.h>

G_BEGIN_DECLS

void		e_rss_shell_extension_type_register	(GTypeModule *type_module);
void		e_rss_shell_view_extension_type_register(GTypeModule *type_module);
void		e_rss_folder_tree_model_extension_type_register
							(GTypeModule *type_module);

G_END_DECLS

#endif /* MODULE_RSS_H */
