/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Blanket header containing the typedefs for object types used in the
 * PAS stuff, so we can disentangle the #includes.
 *
 * Author: Chris Toshok <toshok@ximian.com>
 *
 * Copyright 2003, Ximian, Inc.
 */

#ifndef __PAS_TYPES_H__
#define __PAS_TYPES_H__

typedef struct _PASBookView        PASBookView;
typedef struct _PASBookViewClass   PASBookViewClass;

typedef struct _PASBackendCardSExp PASBackendCardSExp;
typedef struct _PASBackendCardSExpClass PASBackendCardSExpClass;

typedef struct _PASBackend        PASBackend;
typedef struct _PASBackendClass   PASBackendClass;

typedef struct _PASBackendSummary PASBackendSummary;
typedef struct _PASBackendSummaryClass PASBackendSummaryClass;

typedef struct _PASBackendSync        PASBackendSync;
typedef struct _PASBackendSyncClass   PASBackendSyncClass;

typedef struct _PASBook        PASBook;
typedef struct _PASBookClass   PASBookClass;

#endif /* __PAS_TYPES_H__ */
