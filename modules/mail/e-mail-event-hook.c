/*
 * e-mail-event-hook.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-mail-event-hook.h"

#include "mail/em-event.h"

static const EEventHookTargetMask folder_masks[] = {
	{ "newmail", EM_EVENT_FOLDER_NEWMAIL },
	{ NULL }
};

static const EEventHookTargetMask composer_masks[] = {
	{ "sendoption", EM_EVENT_COMPOSER_SEND_OPTION },
	{ NULL }
};

static const EEventHookTargetMask message_masks[] = {
	{ "replyall", EM_EVENT_MESSAGE_REPLY_ALL },
	{ "reply", EM_EVENT_MESSAGE_REPLY },
	{ NULL }
};

static const EEventHookTargetMask send_receive_masks[] = {
	{ "sendreceive", EM_EVENT_SEND_RECEIVE },
	{ NULL }
};

static const EEventHookTargetMask custom_icon_masks[] = {
	{ "customicon", EM_EVENT_CUSTOM_ICON },
	{ NULL }
};

static const EEventHookTargetMap targets[] = {
	{ "folder", EM_EVENT_TARGET_FOLDER, folder_masks },
	{ "message", EM_EVENT_TARGET_MESSAGE, message_masks },
	{ "composer", EM_EVENT_TARGET_COMPOSER, composer_masks },
	{ "sendreceive", EM_EVENT_TARGET_SEND_RECEIVE, send_receive_masks },
	{ "customicon", EM_EVENT_TARGET_CUSTOM_ICON, custom_icon_masks },
	{ NULL }
};

static void
mail_event_hook_class_init (EEventHookClass *class)
{
	EPluginHookClass *plugin_hook_class;
	gint ii;

	plugin_hook_class = E_PLUGIN_HOOK_CLASS (class);
	plugin_hook_class->id = "org.gnome.evolution.mail.events:1.0";

	class->event = (EEvent *) em_event_peek ();

	for (ii = 0; targets[ii].type != NULL; ii++)
		e_event_hook_class_add_target_map (
			(EEventHookClass *) class, &targets[ii]);
}

void
e_mail_event_hook_register_type (GTypeModule *type_module)
{
	const GTypeInfo type_info = {
		sizeof (EEventHookClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) mail_event_hook_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EEventHook),
		0,     /* n_preallocs */
		(GInstanceInitFunc) NULL,
		NULL   /* value_table */
	};

	g_type_module_register_type (
		type_module, e_event_hook_get_type (),
		"EMailEventHook", &type_info, 0);
}
