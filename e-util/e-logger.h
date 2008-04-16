/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Srinivasa Ragavan <sragavan@gnome.org>
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

#ifndef __E_LOGGER_H__
#define __E_LOGGER_H__

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _ELogger ELogger;
typedef struct _ELoggerClass ELoggerClass;
typedef struct _ELoggerPrivate ELoggerPrivate;

typedef void (* ELogFunction) (char *line, gpointer data);

enum e_log_level_t {
	E_LOG_ERROR,
	E_LOG_WARNINGS,
	E_LOG_DEBUG
};

/* The object */
struct _ELogger {
	GObject parent;

	struct _ELoggerPrivate *priv;
};

struct _ELoggerClass {
	GObjectClass popup_class;
};

GType e_logger_get_type(void);
ELogger *e_logger_create(char *component);
void e_logger_log (ELogger *el, int level, char *primary, char *secondary);
void e_logger_get_logs (ELogger *el, ELogFunction func, gpointer data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_LOGGER_H__ */
