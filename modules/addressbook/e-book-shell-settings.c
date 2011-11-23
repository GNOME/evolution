/*
 * e-book-shell-settings.c
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

#include "e-book-shell-settings.h"

#define ADDRESSBOOK_SCHEMA "org.gnome.evolution.addressbook"

void
e_book_shell_backend_init_settings (EShell *shell)
{
	e_shell_settings_install_property_for_key (
		"book-completion-show-address",
		ADDRESSBOOK_SCHEMA,
		"completion-show-address");

	e_shell_settings_install_property_for_key (
		"book-primary-selection",
		ADDRESSBOOK_SCHEMA,
		"primary-addressbook");

	e_shell_settings_install_property_for_key (
		"enable-address-formatting",
		ADDRESSBOOK_SCHEMA,
		"address-formatting");
}
