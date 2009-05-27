/*
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
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_LOGGER_H__
#define __E_LOGGER_H__

#include <glib-object.h>

/* Standard GObject macros */
#define E_TYPE_LOGGER \
	(e_logger_get_type ())
#define E_LOGGER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_LOGGER, ELogger))
#define E_LOGGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_LOGGER, ELoggerClass))
#define E_IS_LOGGER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_LOGGER))
#define E_IS_LOGGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_LOGGER))
#define E_LOGGER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_LOGGER, ELoggerClass))

G_BEGIN_DECLS

typedef struct _ELogger ELogger;
typedef struct _ELoggerClass ELoggerClass;
typedef struct _ELoggerPrivate ELoggerPrivate;

typedef void (*ELogFunction) (gchar *line, gpointer data);

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

GType		e_logger_get_type		(void);
ELogger *	e_logger_create			(const gchar *component);
const gchar *	e_logger_get_component		(ELogger *logger);
void		e_logger_log			(ELogger *logger,
						 gint level,
						 gchar *primary,
						 gchar *secondary);
void		e_logger_get_logs		(ELogger *logger,
						 ELogFunction func,
						 gpointer data);

G_END_DECLS

#endif /* __E_LOGGER_H__ */
