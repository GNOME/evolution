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

#include "camel-op-queue.h"

#define NB_OP_CHUNKS 20
static GMemChunk *op_chunk=NULL;

/**
 * camel_op_queue_new: create a new operation queue
 * 
 * Create a new operation queue. 
 *
 * Return value: the newly allcated object
 **/
CamelOpQueue *
camel_op_queue_new ()
{
	CamelOpQueue *op_queue;
	if (!op_chunk)
		op_chunk = g_mem_chunk_create (CamelOp, 
					       NB_OP_CHUNKS,
					       G_ALLOC_AND_FREE);
	
	op_queue = g_new (CamelOpQueue, 1);
	op_queue->ops_tail = NULL;
	op_queue->ops_head = NULL;
	
}


/**
 * camel_op_queue_push_op: Add an operation to the queue
 * @queue: queue object
 * @op: operation to add
 * 
 * Add an operation to an operation queue. 
 * The queue is a FIFO queue. 
 **/
void
camel_op_queue_push_op (CamelOpQueue *queue, CamelOp *op)
{
	GList *new_op;

	g_assert (queue);

	if (!queue->ops_tail) {
		queue->ops_head = g_list_prepend (NULL, op);
		queue->ops_tail = queue->ops_head;
	} else 
		queue->ops_head = g_list_prepend (queue->ops_head, op);
	
}


/**
 * camel_op_queue_pop_op: Pop the next operation pending in the queue
 * @queue: queue object
 * 
 * Pop the next operation pending in the queue.
 * 
 * Return value: 
 **/
CamelOp *
camel_op_queue_pop_op (CamelOpQueue *queue)
{
	GList *op;
	
	g_assert (queue);
	
	op = queue->ops_tail;
	queue->ops_tail = queue->ops_tail->prev;

	return (CamelOp *)op->data;
}


/**
 * camel_op_queue_run_next_op: run the next pending operation
 * @queue: queue object
 * 
 * Run the next pending operation in the queue.
 * 
 * Return value: TRUE if an operation was launched FALSE if there was no operation pending in the queue.
 **/
gboolean
camel_op_queue_run_next_op (CamelOpQueue *queue)
{
	CamelOp *op;

	op = camel_op_queue_pop_op (queue);
	if (!op) return FALSE;

	/* run the operation */
	op->func (op->param);	

	return FALSE;
}

/**
 * camel_op_queue_set_service_availability: set the service availability for an operation queue
 * @queue: queue object
 * @available: availability flag
 * 
 * set the service availability
 **/
void
camel_op_queue_set_service_availability (CamelOpQueue *queue, gboolean available)
{
	queue->service_available = available;	
}

/**
 * camel_op_queue_get_service_availability: determine if an operation queue service is available 
 * @queue: queue object
 * 
 * Determine if the service associated to an operation queue is available.
 * 
 * Return value: service availability.
 **/
gboolean
camel_op_queue_get_service_availability (CamelOpQueue *queue)
{
	return queue->service_available;
}

/**
 * camel_op_new: return a new CamelOp object 
 * 
 * The obtained object must be destroyed with 
 * camel_op_free ()
 * 
 * Return value: the newly allocated CamelOp object
 **/
CamelOp *
camel_op_new ()
{
	return g_chunk_new (CamelOp, op_chunk);
}

/**
 * camel_op_free: free a CamelOp object allocated with camel_op_new
 * @op: CamelOp object to free
 * 
 * Free a CamelOp object allocated with camel_op_new ()
 * this routine won't work with CamelOp objects allocated 
 * with other allocators.
 **/
void 
camel_op_free (CamelOp *op)
{
	g_chunk_free (op, op_chunk);
}


