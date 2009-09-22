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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef GW_UI_HEADER
#define GW_UI_HEADER

#include <gtk/gtk.h>
#include <shell/e-shell-view.h>

void gw_new_shared_folder_cb (GtkAction *action, EShellView *shell_view);
void gw_proxy_login_cb (GtkAction *action, EShellView *shell_view);

void gw_junk_mail_settings_cb (GtkAction *action, EShellView *shell_view);
void gw_track_message_status_cb (GtkAction *action, EShellView *shell_view);
void gw_retract_mail_cb (GtkAction *action, EShellView *shell_view);

void gw_meeting_accept_cb (GtkAction *action, EShellView *shell_view);
void gw_meeting_accept_tentative_cb (GtkAction *action, EShellView *shell_view);
void gw_meeting_decline_cb (GtkAction *action, EShellView *shell_view);
void gw_resend_meeting_cb (GtkAction *action, EShellView *shell_view);

#endif
