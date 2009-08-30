/*
 * e-mail-junk-hook.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_JUNK_HOOK_H
#define E_MAIL_JUNK_HOOK_H

#include <e-util/e-plugin.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_JUNK_HOOK \
	(e_mail_junk_hook_get_type ())
#define E_MAIL_JUNK_HOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_JUNK_HOOK, EMailJunkHook))
#define E_MAIL_JUNK_HOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_JUNK_HOOK, EMailJunkHookClass))
#define E_IS_MAIL_JUNK_HOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_JUNK_HOOK))
#define E_IS_MAIL_JUNK_HOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_JUNK_HOOK))
#define E_MAIL_JUNK_HOOK_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_JUNK_HOOK, EMailJunkHookClass))

G_BEGIN_DECLS

typedef struct _EMailJunkHook EMailJunkHook;
typedef struct _EMailJunkHookClass EMailJunkHookClass;
typedef struct _EMailJunkHookPrivate EMailJunkHookPrivate;

struct _EMailJunkHook {
	EPluginHook parent;
	EMailJunkHookPrivate *priv;
};

struct _EMailJunkHookClass {
	EPluginHookClass parent_class;
};

GType		e_mail_junk_hook_get_type	(void);
void		e_mail_junk_hook_register_type	(GTypeModule *type_module);

G_END_DECLS

#endif /* E_MAIL_JUNK_HOOK_H */
