/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-msg-composer-select-file.c
 *
 * Copyright (C) 2000 Ximian, Inc.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifndef E_MSG_COMPOSER_SELECT_FILE_H
#define E_MSG_COMPOSER_SELECT_FILE_H

#include "e-msg-composer.h"

char *e_msg_composer_select_file (EMsgComposer *composer,
				  const char *title,
				  gboolean save_mode);

GPtrArray *e_msg_composer_select_file_attachments (EMsgComposer *composer,
						   gboolean *inline_p);

#endif /* E_MSG_COMPOSER_SELECT_FILE_H */
