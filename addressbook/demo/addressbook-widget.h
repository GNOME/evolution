/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * demo.h
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DEMO_H__
#define __DEMO_H__

#include "e-test-model.h"

typedef struct _View View;

typedef enum {
	VIEW_TYPE_REFLOW,
	VIEW_TYPE_TABLE
} ViewType;

typedef struct {
	GtkAllocation last_alloc;
	GnomeCanvasItem *reflow;
	GtkWidget *canvas;
	GnomeCanvasItem *rect;
	int model_changed_id;
} Reflow;

struct _View {
	ViewType type;
	ETestModel *model;
	GtkWidget *child;
	GtkWidget *frame;
	Reflow *reflow;

	GtkWidget *widget;
};

void change_type(View *view, ViewType type);
View *create_view(void);

#endif /* __DEMO_H__ */
