#include "camel-mime-message.h"


void
main (int argc, char**argv)
{
  CamelMimeMessage *message;

  gtk_init (&argc, &argv);
  message = camel_mime_message_new_with_session (CAMEL_SESSION (NULL));
  
  gtk_main();

}
