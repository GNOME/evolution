/* test posix thread folder proxy */


#include "camel.h"

CamelThreadProxy *proxy;
CamelFuncDef *func_def;


void 
test_sync_func (int num)
{
	printf ("Sync function number %d\n", num);
	printf ("Sync function : current thread : %d\n", pthread_self ());
  
}


void 
test_async_cb (int num)
{
	printf ("Callback number %d\n", num);	
	printf ("Callback : current thread : %d\n", pthread_self ());
}

void 
test_async_func (int num)
{
  CamelOp *cb;

  printf ("Async function number %d\n", num);
  printf ("Async function : current thread : %d\n", pthread_self ());
  sleep (1);	
  cb = camel_marshal_create_op (func_def, test_async_cb, num);
  camel_thread_proxy_push_cb (proxy, cb);
  
	
}

int 
main (int argc, char **argv)
{
	int i;
	CamelOp *op;

	camel_init ();

	func_def = 
	  camel_func_def_new (camel_marshal_NONE__INT, 
			      1, 
			      GTK_TYPE_INT);

	printf ("--== Testing Simple marshalling system ==--\n");
	for (i=0; i<5; i++) {
	  printf ("Iterration number %d\n", i);
	  op = camel_marshal_create_op (func_def, test_sync_func, i);
	  camel_op_run (op);
	  camel_op_free (op);

	}	       	
	printf ("\n\n");

	proxy = camel_thread_proxy_new ();

	printf ("--== Testing Asynchronous Operation System ==--\n");
	for (i=0; i<5; i++) {
	  printf ("Pushing async operation number %d for execution\n", i);
	  op = camel_marshal_create_op (func_def, test_async_func, i);
	  camel_thread_proxy_push_op (proxy, op);
	}	       	
	printf ("\n\n");
	printf ("--== Operations execution planned ==--\n");
	gtk_main ();
} 

