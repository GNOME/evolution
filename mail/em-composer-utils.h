/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __EM_COMPOSER_UTILS_H__
#define __EM_COMPOSER_UTILS_H__

#include <composer/e-msg-composer.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

void em_composer_utils_setup_callbacks (EMsgComposer *composer, CamelFolder *folder, const char *uid,
					guint32 flags, guint32 set, CamelFolder *drafts, const char *drafts_uid);

#define em_composer_utils_setup_default_callbacks(composer) em_composer_utils_setup_callbacks (composer, NULL, NULL, 0, 0, NULL, NULL)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EM_COMPOSER_UTILS_H__ */
