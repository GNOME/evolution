#include <camel/camel-session.h>

#define CAMEL_TEST_SESSION_TYPE     (camel_test_session_get_type ())
#define CAMEL_TEST_SESSION(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_TEST_SESSION_TYPE, CamelTestSession))
#define CAMEL_TEST_SESSION_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_TEST_SESSION_TYPE, CamelTestSessionClass))
#define CAMEL_TEST_IS_SESSION(o)    (CAMEL_CHECK_TYPE((o), CAMEL_TEST_SESSION_TYPE))

typedef struct _CamelTestSession {
	CamelSession parent_object;
	
} CamelTestSession;

typedef struct _CamelTestSessionClass {
	CamelSessionClass parent_class;
	
} CamelTestSessionClass;

CamelType camel_test_session_get_type (void);
CamelSession *camel_test_session_new (const char *path);
