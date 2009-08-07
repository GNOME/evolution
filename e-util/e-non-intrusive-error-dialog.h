/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Ashish Shrivastava <shashish@novell.com>
 *
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __E_ERROR_DIALOG_H__
#define __E_ERROR_DIALOG_H__

#include <glib-object.h>
#include "e-error.h"
#include "e-icon-factory.h"
#include "e-logger.h"

G_BEGIN_DECLS

struct _log_data {
        gint level;
        const gchar *key;
        const gchar *text;
        const gchar *stock_id;
        GdkPixbuf *pbuf;
} ldata [] = {
        { E_LOG_ERROR, N_("Error"), N_("Errors"), GTK_STOCK_DIALOG_ERROR },
        { E_LOG_WARNINGS, N_("Warning"), N_("Warnings and Errors"), GTK_STOCK_DIALOG_WARNING },
        { E_LOG_DEBUG, N_("Debug"), N_("Error, Warnings and Debug messages"), GTK_STOCK_DIALOG_INFO }
};

enum
{
        COL_LEVEL = 0,
        COL_TIME,
        COL_DATA
};

/* eni - error non intrusive*/
guint		eni_config_get_error_timeout	(const gchar *path);
void		eni_show_logger			(ELogger *logger,
						 GtkWidget *widget,
						 const gchar *error_timeout_path,
						 const gchar *error_level_path);

G_END_DECLS

#endif /* __E_ERROR_DIALOG_H__ */
