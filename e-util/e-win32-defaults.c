/*
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
 * Authors:
 *		Fridrich Strba <fridrich.strba@bluewin.ch>
 *
 * Copyright (C) 2010 Fridrich Strba
 *
 */

#include <windows.h>

#include "e-win32-defaults.h"
#include "e-util-private.h"

#define MAX_VALUE_NAME 4096
#define EVOBINARY "evolution.exe"
#define EUTILDLL "libeutil-0.dll"
#define CANONICALNAME "Evolution"

static gchar *
_e_win32_sanitize_path (gchar *path)
{
	gchar *p = path;
	while (*p != '\0') {
		if (G_IS_DIR_SEPARATOR (*p))
			*p = G_DIR_SEPARATOR;
		p = g_utf8_find_next_char (p, NULL);
	}
	return path;
}

static void
_e_register_mailto_structure (HKEY hKey)
{
	LONG returnValue;
	DWORD dwDisposition;
	gchar *defaultIcon = NULL;
	gchar *evolutionBinary = NULL;
	gchar *mailtoCommand = NULL;
	BYTE editFlags[4] = { 0x02, 0x00, 0x00, 0x00 };
	
	static HKEY tmp_subkey = (HKEY) INVALID_HANDLE_VALUE;
	
	if ((returnValue = RegSetValueExA (hKey, NULL, 0, REG_SZ, (const BYTE *)"URL:MailTo Protocol", strlen ("URL:MailTo Protocol") + 1)))
		goto cleanup;
	if ((returnValue = RegSetValueExA (hKey, "EditFlags", 0, REG_BINARY, editFlags, G_N_ELEMENTS (editFlags))))
		goto cleanup;
	if ((returnValue = RegSetValueExA (hKey, "URL Protocol", 0, REG_SZ, (const BYTE *)"", strlen ("") + 1)))
		goto cleanup;

	RegFlushKey (hKey);

	if ((returnValue = RegCreateKeyExA (hKey, "DefaultIcon", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &tmp_subkey, &dwDisposition)))
		goto cleanup;

	evolutionBinary = _e_win32_sanitize_path (g_build_path (G_DIR_SEPARATOR_S, _e_get_bindir (), EVOBINARY, NULL));
	defaultIcon = g_strconcat (evolutionBinary, ",1", NULL);

	if ((returnValue = RegSetValueExA (tmp_subkey, NULL, 0, REG_SZ, (const BYTE *)defaultIcon, strlen (defaultIcon) + 1)))
		goto cleanup;

	
	RegFlushKey (tmp_subkey);
	
	RegCloseKey (tmp_subkey);
	
	if ((returnValue = RegCreateKeyExA (hKey, "shell\\open\\command", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &tmp_subkey, &dwDisposition)))
		goto cleanup;

	evolutionBinary = _e_win32_sanitize_path (g_build_path (G_DIR_SEPARATOR_S, _e_get_bindir (), EVOBINARY, NULL));
	mailtoCommand = g_strconcat("\"", evolutionBinary,  "\" --component=mail mailto:\%1", NULL);

	if ((returnValue = RegSetValueExA (tmp_subkey, NULL, 0, REG_SZ, (const BYTE *)mailtoCommand, strlen(mailtoCommand) + 1)))
		goto cleanup;

	
	RegFlushKey (tmp_subkey);
	
	RegCloseKey (tmp_subkey);

cleanup:
	g_free (defaultIcon);
	g_free (evolutionBinary);
	g_free (mailtoCommand);
}

static void
_e_win32_register_mailer_impl (WINBOOL system)
{
	LONG returnValue;
	DWORD i, dwDisposition;
	gchar *defaultIcon = NULL;
	gchar *dllPath = NULL;
	gchar *evolutionBinary = NULL;
	gchar *openCommand = NULL;
	gchar *reinstallCommand = NULL;
	gchar *showIconsCommand = NULL;
	gchar *hideIconsCommand = NULL;

	static HKEY reg_key = (HKEY) INVALID_HANDLE_VALUE;
	static HKEY reg_subkey = (HKEY) INVALID_HANDLE_VALUE;


	if ((returnValue = RegCreateKeyExA (system ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
		"Software\\Clients\\Mail\\" CANONICALNAME, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &reg_key, &dwDisposition)))
		goto cleanup;
	
	if ((returnValue = RegSetValueExA (reg_key, NULL, 0, REG_SZ, (const BYTE *)CANONICALNAME, strlen(CANONICALNAME) + 1)))
		goto cleanup;
	
	dllPath = _e_win32_sanitize_path (g_build_path(G_DIR_SEPARATOR_S, _e_get_bindir (), EUTILDLL, NULL));

	if ((returnValue = RegSetValueExA (reg_key, "DLLPath", 0, REG_SZ, (const BYTE *)dllPath, strlen (dllPath) + 1)))
		goto cleanup;

	RegFlushKey (reg_key);
	
	if ((returnValue = RegCreateKeyExA (reg_key, "DefaultIcon", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &reg_subkey, &dwDisposition)))
		goto cleanup;
	
	evolutionBinary = _e_win32_sanitize_path (g_build_path (G_DIR_SEPARATOR_S, _e_get_bindir (), EVOBINARY, NULL));
	defaultIcon = g_strconcat(evolutionBinary, ",0", NULL);

	if ((returnValue = RegSetValueExA (reg_subkey, NULL, 0, REG_SZ, (const BYTE *)defaultIcon, strlen (defaultIcon) + 1)))
		goto cleanup;

	
	RegFlushKey (reg_subkey);
	
	RegCloseKey (reg_subkey);
	
	if ((returnValue = RegCreateKeyExA (reg_key, "shell\\open\\command", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &reg_subkey, &dwDisposition)))
		goto cleanup;

	openCommand = g_strconcat("\"", evolutionBinary, "\" --component=mail", NULL);
	if ((returnValue = RegSetValueExA (reg_subkey, NULL, 0, REG_SZ, (const BYTE *)openCommand, strlen (openCommand) + 1)))
		goto cleanup;
	
	RegFlushKey (reg_subkey);
	
	RegCloseKey (reg_subkey);

	if ((returnValue = RegCreateKeyExA (reg_key, "InstallInfo", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &reg_subkey, &dwDisposition)))
		goto cleanup;

	reinstallCommand = g_strconcat ("\"", evolutionBinary, "\" --reinstall", NULL);

	if ((returnValue = RegSetValueExA (reg_subkey, "ReinstallCommand", 0, REG_EXPAND_SZ, (const BYTE *)reinstallCommand, strlen (reinstallCommand) + 1)))
		goto cleanup;


	showIconsCommand = g_strconcat ("\"", evolutionBinary, "\" --show-icons", NULL);
	
	if ((returnValue = RegSetValueExA (reg_subkey, "ShowIconsCommand", 0, REG_EXPAND_SZ, (const BYTE *)showIconsCommand, strlen (showIconsCommand) + 1)))
		goto cleanup;


	hideIconsCommand = g_strconcat ("\"", evolutionBinary, "\" --hide-icons", NULL);

	if ((returnValue = RegSetValueExA (reg_subkey, "HideIconsCommand", 0, REG_EXPAND_SZ, (const BYTE *)hideIconsCommand, strlen (hideIconsCommand) + 1)))
		goto cleanup;
	

	i = 1;	
	if ((returnValue = RegSetValueExA (reg_subkey, "IconsVisible", 0, REG_DWORD, (BYTE*)&i, sizeof (i))))
		goto cleanup;
	
	RegFlushKey (reg_subkey);
	
	RegCloseKey (reg_subkey);
	
	if ((returnValue = RegCreateKeyExA (reg_key, "Protocols\\mailto", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &reg_subkey, &dwDisposition)))
		goto cleanup;

	if ((returnValue = RegSetValueExA (reg_key, NULL, 0, REG_SZ, (const BYTE *)CANONICALNAME, strlen (CANONICALNAME) + 1)))
		goto cleanup;
	
	_e_register_mailto_structure (reg_subkey);

	RegCloseKey (reg_subkey);
	
	RegCloseKey (reg_key);

cleanup:
	g_free (defaultIcon);
	g_free (dllPath);
	g_free (evolutionBinary);
	g_free (openCommand);
	g_free (reinstallCommand);
	g_free (showIconsCommand);
	g_free (hideIconsCommand);
}

void
_e_win32_register_mailer (void)
{
	_e_win32_register_mailer_impl (TRUE);
	_e_win32_register_mailer_impl (FALSE);
}

static void
_e_win32_set_default_mailer_impl (WINBOOL system)
{
	LONG returnValue;
	DWORD dwDisposition;
	
	const char client_message[] = "Software\\Clients\\Mail";
	const char mailto_message[] = "Software\\Classes\\mailto";

	static HKEY reg_key = (HKEY) INVALID_HANDLE_VALUE;

	_e_win32_register_mailer_impl (system);
	
	if ((returnValue = RegCreateKeyExA (system ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
		client_message, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &reg_key, &dwDisposition)))
		return;
	
	if ((returnValue = RegSetValueExA (reg_key, NULL, 0, REG_SZ, (const BYTE *)CANONICALNAME, strlen (CANONICALNAME) + 1)))
		return;
	
	RegFlushKey (reg_key);

	RegCloseKey (reg_key);
	
	if ((returnValue = RegCreateKeyExA (system ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
		mailto_message, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &reg_key, &dwDisposition)))
		return;

	_e_register_mailto_structure (reg_key);
	
	RegFlushKey (reg_key);

	RegCloseKey (reg_key);
	
	SendMessageA (HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)&client_message);
	SendMessageA (HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)&mailto_message);
}

void
_e_win32_set_default_mailer (void)
{
	_e_win32_set_default_mailer_impl (TRUE);
	_e_win32_set_default_mailer_impl (FALSE);
}

void
_e_win32_unset_default_mailer (void)
{
	/* TODO: implement */
}

