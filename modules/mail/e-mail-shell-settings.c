/*
 * e-mail-shell-settings.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-shell-settings.h"

#include <mail/e-mail-backend.h>

#include <shell/e-shell.h>

#define MAIL_SCHEMA "org.gnome.evolution.mail"

static gboolean
transform_no_folder_dots_to_ellipsize (GBinding *binding,
                                       const GValue *source_value,
                                       GValue *target_value,
                                       gpointer user_data)
{
	PangoEllipsizeMode ellipsize;

	if (g_value_get_boolean (source_value))
		ellipsize = PANGO_ELLIPSIZE_NONE;
	else
		ellipsize = PANGO_ELLIPSIZE_END;

	g_value_set_enum (target_value, ellipsize);

	return TRUE;
}

void
e_mail_shell_settings_init (EShellBackend *shell_backend)
{
	EShell *shell;
	EShellSettings *shell_settings;
	EMailBackend *backend;
	EMailSession *session;

	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	/*** Global Objects ***/

	e_shell_settings_install_property (
		g_param_spec_pointer (
			"mail-session",
			NULL,
			NULL,
			G_PARAM_READWRITE));

	e_shell_settings_set_pointer (
		shell_settings, "mail-session",
		g_object_ref (session));

	/*** Mail Preferences ***/

	e_shell_settings_install_property_for_key (
		"mail-address-compress",
		MAIL_SCHEMA,
		"address-compress");

	e_shell_settings_install_property_for_key (
		"mail-address-count",
		MAIL_SCHEMA,
		"address-count");

	e_shell_settings_install_property_for_key (
		"mail-charset",
		MAIL_SCHEMA,
		"charset");

	e_shell_settings_install_property_for_key (
		"mail-check-for-junk",
		MAIL_SCHEMA,
		"junk-check-incoming");

	e_shell_settings_install_property_for_key (
		"mail-check-on-start",
		MAIL_SCHEMA,
		"send-recv-on-start");

	e_shell_settings_install_property_for_key (
		"mail-check-all-on-start",
		MAIL_SCHEMA,
		"send-recv-all-on-start");

	e_shell_settings_install_property_for_key (
		"mail-citation-color",
		MAIL_SCHEMA,
		"citation-color");

	e_shell_settings_install_property_for_key (
		"mail-confirm-expunge",
		MAIL_SCHEMA,
		"prompt-on-expunge");

	e_shell_settings_install_property_for_key (
		"mail-confirm-unwanted-html",
		MAIL_SCHEMA,
		"prompt-on-unwanted-html");

	e_shell_settings_install_property_for_key (
		"mail-empty-junk-on-exit",
		MAIL_SCHEMA,
		"junk-empty-on-exit");

	e_shell_settings_install_property_for_key (
		"mail-empty-trash-on-exit",
		MAIL_SCHEMA,
		"trash-empty-on-exit");

	e_shell_settings_install_property_for_key (
		"mail-enable-unmatched-search-folder",
		MAIL_SCHEMA,
		"enable-unmatched");

	e_shell_settings_install_property_for_key (
		"mail-font-monospace",
		MAIL_SCHEMA,
		"monospace-font");

	e_shell_settings_install_property_for_key (
		"mail-font-variable",
		MAIL_SCHEMA,
		"variable-width-font");

	/* This value corresponds to the EMailForwardStyle enum. */
	e_shell_settings_install_property_for_key (
		"mail-forward-style",
		MAIL_SCHEMA,
		"forward-style");

	/* This value corresponds to MailConfigHTTPMode enum. */
	e_shell_settings_install_property_for_key (
		"mail-image-loading-policy",
		MAIL_SCHEMA,
		"load-http-images");

	e_shell_settings_install_property_for_key (
		"mail-magic-spacebar",
		MAIL_SCHEMA,
		"magic-spacebar");

	e_shell_settings_install_property_for_key (
		"mail-global-view-setting",
		MAIL_SCHEMA,
		"global-view-setting");

	e_shell_settings_install_property_for_key (
		"mail-mark-citations",
		MAIL_SCHEMA,
		"mark-citations");

	e_shell_settings_install_property_for_key (
		"mail-mark-seen",
		MAIL_SCHEMA,
		"mark-seen");

	e_shell_settings_install_property_for_key (
		"mail-mark-seen-timeout",
		MAIL_SCHEMA,
		"mark-seen-timeout");

	/* Do not bind to this.  Use "mail-sidebar-ellipsize" instead. */
	e_shell_settings_install_property_for_key (
		"mail-no-folder-dots",
		MAIL_SCHEMA,
		"no-folder-dots");

	e_shell_settings_install_property_for_key (
		"mail-only-local-photos",
		MAIL_SCHEMA,
		"photo-local");

	e_shell_settings_install_property_for_key (
		"mail-show-real-date",
		MAIL_SCHEMA,
		"show-real-date");

	e_shell_settings_install_property_for_key (
		"mail-sort-accounts-alpha",
		MAIL_SCHEMA,
		"sort-accounts-alpha");

	e_shell_settings_install_property_for_key (
		"mail-prompt-delete-in-vfolder",
		MAIL_SCHEMA,
		"prompt-on-delete-in-vfolder");

	/* This value corresponds to the EMailReplyStyle enum,
	 * but the ordering of the combo box items in preferences
	 * has changed.  We use transformation functions there. */
	e_shell_settings_install_property_for_key (
		"mail-reply-style",
		MAIL_SCHEMA,
		"reply-style");

	e_shell_settings_install_property_for_key (
		"mail-safe-list",
		MAIL_SCHEMA,
		"safe-list");

	e_shell_settings_install_property_for_key (
		"mail-show-animated-images",
		MAIL_SCHEMA,
		"show-animated-images");

	e_shell_settings_install_property_for_key (
		"mail-show-sender-photo",
		MAIL_SCHEMA,
		"show-sender-photo");

	e_shell_settings_install_property_for_key (
		"mail-sidebar-search",
		MAIL_SCHEMA,
		"side-bar-search");

	e_shell_settings_install_property_for_key (
		"mail-thread-by-subject",
		MAIL_SCHEMA,
		"thread-subject");

	e_shell_settings_install_property_for_key (
		"mail-use-custom-fonts",
		MAIL_SCHEMA,
		"use-custom-font");

	/*** Composer Preferences ***/

	e_shell_settings_install_property_for_key (
		"composer-charset",
		MAIL_SCHEMA,
		"composer-charset");

	e_shell_settings_install_property_for_key (
		"composer-format-html",
		MAIL_SCHEMA,
		"composer-send-html");

	e_shell_settings_install_property_for_key (
		"composer-inline-spelling",
		MAIL_SCHEMA,
		"composer-inline-spelling");

	e_shell_settings_install_property_for_key (
		"composer-magic-links",
		MAIL_SCHEMA,
		"composer-magic-links");

	e_shell_settings_install_property_for_key (
		"composer-magic-smileys",
		MAIL_SCHEMA,
		"composer-magic-smileys");

	e_shell_settings_install_property_for_key (
		"composer-outlook-filenames",
		MAIL_SCHEMA,
		"composer-outlook-filenames");

	e_shell_settings_install_property_for_key (
		"composer-localized-re",
		MAIL_SCHEMA,
		"composer-localized-re");

	e_shell_settings_install_property_for_key (
		"composer-ignore-list-reply-to",
		MAIL_SCHEMA,
		"composer-ignore-list-reply-to");

	e_shell_settings_install_property_for_key (
		"composer-group-reply-to-list",
		MAIL_SCHEMA,
		"composer-group-reply-to-list");

	e_shell_settings_install_property_for_key (
		"composer-sign-reply-if-signed",
		MAIL_SCHEMA,
		"composer-sign-reply-if-signed");

	e_shell_settings_install_property_for_key (
		"composer-prompt-only-bcc",
		MAIL_SCHEMA,
		"prompt-on-only-bcc");

	e_shell_settings_install_property_for_key (
		"composer-prompt-private-list-reply",
		MAIL_SCHEMA,
		"prompt-on-private-list-reply");

	e_shell_settings_install_property_for_key (
		"composer-prompt-reply-many-recips",
		MAIL_SCHEMA,
		"prompt-on-reply-many-recips");

	e_shell_settings_install_property_for_key (
		"composer-prompt-list-reply-to",
		MAIL_SCHEMA,
		"prompt-on-list-reply-to");

	e_shell_settings_install_property_for_key (
		"composer-prompt-empty-subject",
		MAIL_SCHEMA,
		"prompt-on-empty-subject");

	e_shell_settings_install_property_for_key (
		"composer-prompt-send-invalid-recip",
		MAIL_SCHEMA,
		"prompt-on-invalid-recip");

	e_shell_settings_install_property_for_key (
		"composer-reply-start-bottom",
		MAIL_SCHEMA,
		"composer-reply-start-bottom");

	e_shell_settings_install_property_for_key (
		"composer-request-receipt",
		MAIL_SCHEMA,
		"composer-request-receipt");

	e_shell_settings_install_property_for_key (
		"composer-spell-color",
		MAIL_SCHEMA,
		"composer-spell-color");

	e_shell_settings_install_property_for_key (
		"composer-top-signature",
		MAIL_SCHEMA,
		"composer-top-signature");

	e_shell_settings_install_property_for_key (
		"composer-no-signature-delim",
		MAIL_SCHEMA,
		"composer-no-signature-delim");

	e_shell_settings_install_property_for_key (
		"composer-gallery-path",
		MAIL_SCHEMA,
		"composer-gallery-path");

	e_shell_settings_install_property_for_key (
		"mail-headers-collapsed",
		MAIL_SCHEMA,
		"headers-collapsed");

	e_shell_settings_install_property (
		g_param_spec_enum (
			"mail-sidebar-ellipsize",
			NULL,
			NULL,
			PANGO_TYPE_ELLIPSIZE_MODE,
			PANGO_ELLIPSIZE_NONE,
			G_PARAM_READWRITE));

	g_object_bind_property_full (
		shell_settings, "mail-no-folder-dots",
		shell_settings, "mail-sidebar-ellipsize",
		G_BINDING_SYNC_CREATE,
		transform_no_folder_dots_to_ellipsize,
		NULL,
		g_object_ref (shell_settings),
		(GDestroyNotify) g_object_unref);
}
