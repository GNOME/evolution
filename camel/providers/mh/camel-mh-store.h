/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mh-store.h : class for an mh store */

/* 
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Michael Zucchi <notzed@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#ifndef CAMEL_MH_STORE_H
#define CAMEL_MH_STORE_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif				/* __cplusplus } */
#include "camel-store.h"
#define CAMEL_MH_STORE_TYPE     (camel_mh_store_get_type ())
#define CAMEL_MH_STORE(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_MH_STORE_TYPE, CamelMhStore))
#define CAMEL_MH_STORE_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_MH_STORE_TYPE, CamelMhStoreClass))
#define IS_CAMEL_MH_STORE(o)    (CAMEL_CHECK_TYPE((o), CAMEL_MH_STORE_TYPE))

typedef struct {
	CamelStore parent_object;
	
} CamelMhStore;

typedef struct {
	CamelStoreClass parent_class;
	
} CamelMhStoreClass;

/* public methods */

/* Standard Camel function */
CamelType camel_mh_store_get_type(void);

const gchar *camel_mh_store_get_toplevel_dir(CamelMhStore * store);

#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* CAMEL_MH_STORE_H */
