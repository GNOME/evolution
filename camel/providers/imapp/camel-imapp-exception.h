
/* This implements 'real' exceptions that work a bit like c++/java exceptions */

/* Still experimental code */

#ifndef __CAMEL_IMAPP_EXCEPTION_H
#define __CAMEL_IMAPP_EXCEPTION_H

#include <setjmp.h>
#include "camel/camel-exception.h"

struct _CamelExceptionEnv {
	struct _CamelExceptionEnv *parent;
	CamelException *ex;
	jmp_buf env;
};

#define CAMEL_TRY { struct _CamelExceptionEnv __env; camel_exception_try(&__env); if (setjmp(__env.env) == 0)
#define CAMEL_IGNORE camel_exception_done(&__env); }
#define CAMEL_CATCH(x) { CamelException *x; x=__env.ex; if (x != NULL)
#define CAMEL_DONE } camel_exception_done(&__env); }
#define CAMEL_DROP() camel_exception_drop(&__env)

void camel_exception_setup(void);

/* internal functions, use macro's above */
void camel_exception_try(struct _CamelExceptionEnv *env);
void camel_exception_done(struct _CamelExceptionEnv *env);
void camel_exception_drop(struct _CamelExceptionEnv *env);

/* user functions */
void camel_exception_throw_ex(CamelException *ex) __attribute__ ((noreturn));
void camel_exception_throw(int id, char *fmt, ...) __attribute__ ((noreturn));

#endif
