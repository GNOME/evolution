/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors :
 *  Damon Chaplin <damon@ximian.com>
 *  Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2000, Ximian, Inc.
 * Copyright 2000, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef _E_SELECT_NAMES_CONFIG_H_
#define _E_SELECT_NAMES_CONFIG_H_

#include <gconf/gconf-client.h>

void e_select_names_config_remove_notification (guint id);

/* The last completion book */
char 	  *e_select_names_config_get_last_completion_book	(void);
void	  e_select_names_config_set_last_completion_book	(const char *last_completion_book);
guint e_select_names_config_add_notification_last_completion_book (GConfClientNotifyFunc func, gpointer data);


/* The minimum query length */
gint	  e_select_names_config_get_min_query_length	(void);
void	  e_select_names_config_set_min_query_length	(gint	      day_end_hour);
guint e_select_names_config_add_notification_min_query_length (GConfClientNotifyFunc func, gpointer data);

#endif /* _E_SELECT_NAMES_CONFIG_H_ */
