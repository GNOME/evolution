/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Author:
 *   Chris Toshok (toshok@ximian.com)
 * Copyright (C) 2003, Ximian, Inc.
 */

#ifndef __PAS_BACKEND_VCF_H__
#define __PAS_BACKEND_VCF_H__

#include "pas-backend-sync.h"

#define PAS_TYPE_BACKEND_VCF        (pas_backend_vcf_get_type ())
#define PAS_BACKEND_VCF(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), PAS_TYPE_BACKEND_VCF, PASBackendVCF))
#define PAS_BACKEND_VCF_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), PAS_BACKEND_TYPE, PASBackendVCFClass))
#define PAS_IS_BACKEND_VCF(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), PAS_TYPE_BACKEND_VCF))
#define PAS_IS_BACKEND_VCF_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), PAS_TYPE_BACKEND_VCF))
#define PAS_BACKEND_VCF_GET_CLASS(k) (G_TYPE_INSTANCE_GET_CLASS ((obj), PAS_TYPE_BACKEND_VCF, PASBackendVCFClass))

typedef struct _PASBackendVCFPrivate PASBackendVCFPrivate;

typedef struct {
	PASBackendSync         parent_object;
	PASBackendVCFPrivate *priv;
} PASBackendVCF;

typedef struct {
	PASBackendSyncClass parent_class;
} PASBackendVCFClass;

PASBackend *pas_backend_vcf_new      (void);
GType       pas_backend_vcf_get_type (void);

#endif /* ! __PAS_BACKEND_VCF_H__ */

