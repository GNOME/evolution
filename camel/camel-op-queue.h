/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org> .
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


#ifndef CAMEL_OP_QUEUE_H
#define CAMEL_OP_QUEUE_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>

typedef void (CamelOpFunc)(gpointer param);

typedef struct {
	CamelOpFunc *op_func;
	gpointer param;

} CamelOp;


typedef struct 
{
	GList *ops_head;
	GList *ops_tail;
	gint pending_ops;
} CamelOpQueue;


/* public methods */
CamelOpQueue *camel_op_queue_new ();
void camel_op_queue_push_op (CamelOpQueue *queue, CamelOp *op);
CamelOp *camel_op_queue_pop_op (CamelOpQueue *queue);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_OP_QUEUE_H */

