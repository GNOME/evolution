/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
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


/* MT safe */

 
#include <config.h>
#include "camel-log.h"
#include "camel-op-queue.h"

static GStaticMutex op_queue_mutex = G_STATIC_MUTEX_INIT;



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

	CAMEL_LOG_FULL_DEBUG ("Entering CamelOpQueue::new\n");

	op_queue = g_new (CamelOpQueue, 1);
	op_queue->ops_tail = NULL;
	op_queue->ops_head = NULL;
	op_queue->service_available = TRUE;

	CAMEL_LOG_FULL_DEBUG ("Leaving CamelOpQueue::new\n");
	return op_queue;
}


void 
camel_op_queue_free (CamelOpQueue *op_queue)
{
	CAMEL_LOG_FULL_DEBUG ("Entering CamelOpQueue::free\n");
	g_list_free (op_queue->ops_head);	
	g_free (op_queue);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelOpQueue::free\n");
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
	CAMEL_LOG_FULL_DEBUG ("Entering CamelOpQueue::push_op\n");
	g_assert (queue);
	g_static_mutex_lock (&op_queue_mutex);
	if (!queue->ops_tail) {
		CAMEL_LOG_FULL_DEBUG ("CamelOpQueue::push_op queue does not exists yet. "
				      "Creating it\n");
		queue->ops_head = g_list_prepend (NULL, op);
		queue->ops_tail = queue->ops_head;
	} else 
		queue->ops_head = g_list_prepend (queue->ops_head, op);	
	g_static_mutex_unlock (&op_queue_mutex);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelOpQueue::push_op\n");
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
	GList *op_list;
	CamelOp *op;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelOpQueue::pop_op\n");
	g_assert (queue);

	g_static_mutex_lock (&op_queue_mutex);
	op_list = queue->ops_tail;
	if (!op_list) return NULL;

	queue->ops_tail = queue->ops_tail->prev;
	op = (CamelOp *)op_list->data;
	g_static_mutex_unlock (&op_queue_mutex);

	CAMEL_LOG_FULL_DEBUG ("Leaving CamelOpQueue::pop_op\n");
	return op;
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

	CAMEL_LOG_FULL_DEBUG ("Entering CamelOpQueue::run_next_op\n");
	op = camel_op_queue_pop_op (queue);
	if (!op) return FALSE;

	
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelOpQueue::run_next_op\n");
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
	CAMEL_LOG_FULL_DEBUG ("Entering CamelOpQueue::set_service_availability\n");
	g_static_mutex_lock (&op_queue_mutex);
	queue->service_available = available;
	g_static_mutex_unlock (&op_queue_mutex);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelOpQueue::set_service_availability\n");
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
	gboolean available;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelOpQueue::get_service_availability\n");
	g_static_mutex_lock (&op_queue_mutex);
	available = queue->service_available;
	g_static_mutex_unlock (&op_queue_mutex);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelOpQueue::get_service_availability\n");
	return available;
}

