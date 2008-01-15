/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* cal-attachment-select-file.c
 *
 * Copyright (C) 2004 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * published by the Free Software Foundation; either version 2 of the
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Harish K <kharish@novell.com>
 */

#ifndef E_MSG_COMPOSER_SELECT_FILE_H
#define E_MSG_COMPOSER_SELECT_FILE_H

#include "comp-editor.h"

char *comp_editor_select_file (CompEditor *editor,
				  const char *title,
				  gboolean save_mode);

GPtrArray *comp_editor_select_file_attachments (CompEditor *editor,
						   gboolean *inline_p);

#endif /* CAL_ATTACHMENT_SELECT_FILE_H */
