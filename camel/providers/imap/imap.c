/*  Spruce
 *  Copyright (C) 1999-2000 Jeffrey Stedfast
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "imap.h"

#define IMAP_LOGGING

/* this is used in the tag before each command */
static guint32 imap_commands = 0;

extern gint timeout;
extern GList *mime_parts;


gint imap_ok (gint tag, gchar *line)
{
   /* returns 1 if <tag> OK was found */
   gchar find[64];
   gint ret;

   g_snprintf(find, sizeof(find)-1, "A%.5d OK", tag);

   ret = find_string (line, find);
   
   if (ret < 0)
      return 0;
      
   return 1;
}

gint imap_login_cram_md5 (gint socket, gchar *username, gchar *password)
{
   /* Log in to server using CRAM-MD5 keyed hash. */ 
   gchar buffer[512];
   gchar *retstr;
   gint pos;

   if (username == NULL || password == NULL) 
      return ERROR;

   memset(buffer, 0, sizeof(buffer));
   if (recvline(socket, buffer, sizeof(buffer)-1) < 0)
      return ERROR; /* Fetch the OK line from the server */

   if (find_string(buffer, "OK") == -1)
      return ERROR;

   g_snprintf(buffer, sizeof(buffer)-1, "A%.5d AUTHENTICATE CRAM-MD5\r\n", imap_commands);

   if (send(socket, buffer, strlen(buffer), 0) < 0)
      return ERROR;

   memset(buffer, 0, sizeof(buffer));
   if (recvline(socket, buffer, sizeof(buffer)-1) < 0)
      return ERROR;

   pos = find_string(buffer, "\r\n");
   if (pos != -1)
      buffer[pos] = '\0';
   retstr = cram_md5(username, password, buffer);

   if (retstr[strlen(retstr)-1] == '\n')
      retstr[strlen(retstr)-1] = '\0';

   g_snprintf(buffer, sizeof(buffer)-1, "%s\r\n", retstr);
   g_free(retstr);

   if (send (socket, buffer, strlen(buffer), 0) < 0) 
      return ERROR;

   if (recvline(socket, buffer, sizeof(buffer)-1) < 0)
      return ERROR;

   if (!imap_ok(imap_commands, buffer))
      return ERROR;

   imap_commands++;

   return SUCCESS;
}
      
gint imap_login (gint socket, gchar *username, gchar *password)
{
   /* this logs us in to the server */
   gchar buffer[512];
   gchar temp[64];
   
   if (username == NULL || password == NULL)
      return ERROR;

   g_snprintf(buffer, sizeof(buffer)-1, "A%.5d LOGIN \"%s\" \"%s\"\r\n", imap_commands, username, password);
#ifdef IMAP_LOGGING
   fprintf(stderr, "%s", buffer);
#endif

   if (send (socket, buffer, strlen(buffer), 0) < 0)
   {
      return ERROR;
   }
      
   g_snprintf(temp, sizeof(temp)-1, "A%.5d", imap_commands);

   memset(buffer, 0, sizeof(buffer));
   recvline_timeo(socket, buffer, sizeof(buffer)-1, timeout);
   while (!strstr(buffer, temp))
   {
      memset(buffer, 0, sizeof(buffer));
      recvline_timeo(socket, buffer, sizeof(buffer)-1, timeout);
   }

   if (!imap_ok(imap_commands, buffer))
      return ERROR;

   imap_commands++;
   
   return SUCCESS;   
}

GList *imap_list (gint socket, gchar *namespace)
{
   /* this gets the names of all the mailboxes */
   gchar buffer[512];
   gchar flags[256];
   gchar temp[64], *ptr = NULL, *flagptr = NULL;
   gchar slashdot = '\0';
   GList *list = NULL;
   gint ret, size = 0, flaglen = 0;

   if (namespace && *namespace)
   {
      if (*namespace && namespace[strlen(namespace)-1] != '/' && namespace[strlen(namespace)-1] != '.')
         slashdot = '/';
      g_snprintf(buffer, sizeof(buffer)-1, "A%.5d LIST \"\" %s%c*\r\n", imap_commands, namespace, slashdot);
   }
   else
      g_snprintf(buffer, sizeof(buffer)-1, "A%.5d LIST \"\" INBOX.*\r\n", imap_commands);
#ifdef IMAP_LOGGING
   fprintf(stderr, "%s", buffer);
#endif

   if (send(socket, buffer, strlen(buffer), 0) < 0)
   {
      return NULL;
   }

   do
   {
      memset(buffer, 0, sizeof(buffer));
      ret = recvline(socket, buffer, sizeof(buffer)-1);
      if (ret > 0)
      {
#ifdef IMAP_LOGGING
         fprintf(stderr, "received: %s", buffer);
#endif
         if (buffer[0] == '*')
         {
            strip(buffer, '\r');
            strip(buffer, '\n');

            /* skip ahead to the flag section */
            ptr = strstr(buffer, "(");

            /* find the end of the flags section */
            flagptr = ptr + 1;
            ptr = strstr(ptr, ")") + 1;

            /* eventually we will need to parse this */
            memset(flags, 0, sizeof(flags));
            flaglen = (gint)(ptr - flagptr) - 1;
            size = sizeof(flags);
            strncpy(flags, flagptr, flaglen > size ? size : flaglen);
            if (!strstrcase(flags, "\\NoSelect")) /* is this a selectable mailbox? */
            {
               /* skip the reference name */
               ptr += imap_get_string (ptr, temp, sizeof(temp)-1, "");

               /* the rest of the return string is fair play... */
               g_strstrip(ptr);       /* trim off any extra white space */
               unquote(ptr);           /* unquote the mailbox name if it is quoted */
               if (slashdot)
                  strcut(ptr, 0, strlen(namespace)+1); /* cut out the namespace and the '/' */
               else
                  strcut(ptr, 0, strlen(namespace));   /* cut out the namespace */
					

               list = g_list_append (list, g_strdup(ptr));
            }
         }
         else 
	         break;
      }
   } while (ret > 0);

   imap_commands++;

   return list;
}

gint imap_select_mailbox (gint socket, gchar *mailbox, gchar *namespace)
{
   /* selects a mailbox, returns the number of messages in that mailbox
    * or -1 on error */
   gchar *cmdbuf, buffer[512], temp[64], *index, mesgs[16];
   gchar slashdot = '\0';
   gint ret, i;

   if (mailbox == NULL)
      return ERROR;

   if (namespace && strcmp(mailbox, "INBOX"))
   {
      if (*namespace && namespace[strlen(namespace)-1] != '/' && namespace[strlen(namespace)-1] != '.')
         slashdot = '/';

      cmdbuf = g_strdup_printf("A%.5d SELECT %s%c%s\r\n", imap_commands, namespace, slashdot, mailbox);
   }
   else
      cmdbuf = g_strdup_printf("A%.5d SELECT %s\r\n", imap_commands, mailbox);
#ifdef IMAP_LOGGING
   fprintf(stderr, "%s", cmdbuf);
#endif

   if (send(socket, cmdbuf, strlen(cmdbuf), 0) < 0)
   {
      g_free(cmdbuf);
      return -1;
   }
   g_free(cmdbuf);
   
   g_snprintf(temp, sizeof(temp)-1, "A%.5d", imap_commands);

   memset(buffer, 0, sizeof(buffer));
   ret = recvline(socket, buffer, sizeof(buffer)-1);
   while (ret > 0)
   {
#ifdef IMAP_LOGGING
      fprintf(stderr, "received: %s", buffer);
#endif
      if (strstr(buffer, temp))
         break;
      if (buffer[0] == '*')
      {
         if (strstr(buffer, "EXISTS"))
         {
            index = buffer;
            while (*index != ' ')
               index++;
            index++;
            
            i = 0;
            memset(mesgs, 0, sizeof(mesgs));
            while (*index != ' ' && i < sizeof(mesgs)-1)
            {
               mesgs[i] = *index;
               index++;
               i++;
            }
         }
      }
      memset(buffer, 0, sizeof(buffer));
      ret = recvline(socket, buffer, sizeof(buffer)-1);   
   }
   
   if (!imap_ok(imap_commands, buffer))
      return -1;
   
   imap_commands++;

   return atoi(mesgs);
}

gint imap_logout (gint socket)
{
   /* logs out */
   gchar buffer[256];

   g_snprintf(buffer, sizeof(buffer)-1, "A%.5d LOGOUT\r\n", imap_commands);
#ifdef IMAP_LOGGING
   fprintf(stderr, "%s", buffer);
#endif

   if (send(socket, buffer, strlen(buffer), 0) < 0)
   {
      return ERROR;
   }

   return SUCCESS;
}

gint imap_mailbox_create (gint socket, gchar *mailbox)
{
   /* creates a new mailbox */
   gchar buffer[256];

   if (mailbox == NULL)
      return ERROR;

   g_snprintf(buffer, sizeof(buffer)-1, "A%.5d CREATE %s\r\n", imap_commands, mailbox);
#ifdef IMAP_LOGGING
   fprintf(stderr, "%s", buffer);
#endif

   if (send(socket, buffer, strlen(buffer), 0) < 0)
   {
      return ERROR;
   }

   memset(buffer, 0, sizeof(buffer));
   if (recvline(socket, buffer, sizeof(buffer)-1) < 0 || !imap_ok(imap_commands, buffer))
   {
      return ERROR;
   }

   imap_commands++;

   return SUCCESS;
}

gint imap_mailbox_delete (gint socket, gchar *mailbox)
{
   /* deletes a mailbox */
   gchar buffer[256];

   if (mailbox == NULL)
      return ERROR;

   g_snprintf(buffer, sizeof(buffer)-1, "A%.5d DELETE %s\r\n", imap_commands, mailbox);
#ifdef IMAP_LOGGING
   fprintf(stderr, "%s", buffer);
#endif

   if (send(socket, buffer, strlen(buffer), 0) < 0)
   {
      return ERROR;
   }

   memset(buffer, 0, sizeof(buffer));
   if (recvline(socket, buffer, sizeof(buffer)-1) < 0 ||
      !imap_ok(imap_commands, buffer))
   {
      return ERROR;
   }

   imap_commands++;

   return SUCCESS;
}

/* fetches the specified part of a message, which can be alot of 
 * if you use peek the \Seen flag is not set */
gchar *imap_fetch (gint socket, gint mesgnum, gchar *part, gint *seen)
{
   /* fetches the specified part of the mesg. */
   gchar *mesg = NULL;
   gchar buffer[512], *index;
   gchar flags[128], size[16], temp[64];
   gint i, n, msgsize = 1000;

   if (mesgnum < 0)
      return (gchar *)NULL;

   g_snprintf(buffer, sizeof(buffer)-1, "A%.5d FETCH %d (FLAGS %s)\r\n", imap_commands, mesgnum, part);
#ifdef IMAP_LOGGING
   fprintf(stderr, "%s", buffer);
#endif

   if (send(socket, buffer, strlen(buffer), 0) < 0)
   {
      return (gchar *)NULL;
   }   


   memset(buffer, 0, sizeof(buffer));
   n = recvline(socket, buffer, sizeof(buffer)-1);

   if (buffer[0] != '*' && imap_ok(imap_commands, buffer))
   {
       memset(buffer, 0, sizeof(buffer));
       n = recvline(socket, buffer, sizeof(buffer)-1);
   }
     
   if (buffer[0] == '*')
   /*if (imap_ok(imap_commands, buffer))*/
   {
      index = strstrcase(buffer, "FLAGS");
      if (index == NULL) /* hmm */
      {
         fprintf(stderr, _("IMAP server replied using unknown tokens.\n"));
         return (gchar *)NULL;
      }
      else
      {
#ifdef IMAP_LOGGING
         fprintf(stderr, "received: %s", buffer);
#endif
         /* skip to the FLAGS token */
         for ( ; *index && *index != '('; index++);
         index++;

         i = 0;
         memset(flags, 0, sizeof(flags));
         while (*index != ')' && i < sizeof(flags)-1)
         {
            flags[i] = *index;
            index++;
            i++;
         }
         flags[i] = '\0';

         /* skip to the next significant token */
         for (index++; *index && *index != '{'; index++);
         index++;
         
         i = 0;
         memset(size, 0, sizeof(size));
         while (*index != '}' && i < sizeof(size)-1)
         {
            size[i] = *index;
            index++;
            i++;
         }
         size[i] = '\0';
         msgsize = atoi(size);
      }
   }
   else
   {
      g_snprintf(temp, sizeof(temp)-1, "A%.5d", imap_commands);
      if (strstr(buffer, temp)) /* this means there's no such message */
      {
         fprintf(stderr, _("IMAP responded with \"no such message\".\n"));
         return (gchar *)NULL;
      }
   }
   

   mesg = g_malloc0(msgsize + 50); /* just to be safe */
   n = recvline(socket, buffer, sizeof(buffer)-1);
   
   while (!(n <= 0) && !imap_ok(imap_commands, buffer))
   {
      strip(buffer, '\r');  /* strip all the \r's */
      strcat(mesg, buffer);
      memset(buffer, 0, sizeof(buffer));
      n = recvline(socket, buffer, sizeof(buffer)-1);
   }

   if (mesg)
      mesg[strlen(mesg)-3] = '\0';   /* strip the ending ) */

   if (seen != NULL)
   {
      if (strstrcase(flags, "\\Seen"))
         *seen = 1;
      else
         *seen = 0;
   }
   
   imap_commands++;

   return (gchar*)mesg;
}

gboolean imap_delete(const ImapAccount_t *imap, GList *sorted)
{
   GList *p = sorted;
   gchar buffer[256];
   gchar temp[16];
   gint ret;

   do
   {
      gint id = GPOINTER_TO_INT(p->data);
      g_snprintf(buffer, sizeof(buffer)-1, "A%.5d STORE %d +FLAGS (\\Deleted)\r\n", imap_commands, id);
#ifdef IMAP_LOGGING
      fprintf(stderr, "%s", buffer);
#endif
      if (send(imap->socket, buffer, strlen(buffer), 0) < 0)
      {
         return FALSE;
      }
      g_snprintf(temp, sizeof(temp)-1, "A%.5d", imap_commands);

      memset(buffer, 0, sizeof(buffer));
      ret = recvline(imap->socket, buffer, sizeof(buffer)-1);
      while (ret > 0)
      {
         if (find_string(buffer, temp) >= 0)
            break;

         memset(buffer, 0, sizeof(buffer));
         ret = recvline(imap->socket, buffer, sizeof(buffer)-1);
      }

      if (!imap_ok(imap_commands, buffer))
      {
         return FALSE;
      }
      imap_commands++;
   } while ((p = g_list_next(p)));

   g_snprintf(buffer, 255, "A%.5d EXPUNGE\r\n", imap_commands);
#ifdef IMAP_LOGGING
   fprintf(stderr, "%s", buffer);
#endif
   if (send(imap->socket, buffer, strlen(buffer), 0) < 0)
   {
      return FALSE;
   }   

   g_snprintf (temp, 15, "A%.5d", imap_commands);

   memset(buffer, 0, sizeof(buffer));
   ret = recvline(imap->socket, buffer, sizeof(buffer)-1);
   while (ret > 0)
   {
      if (find_string(buffer, temp) >= 0)
        break;

     memset(buffer, 0, sizeof(buffer));
     ret = recvline(imap->socket, buffer, sizeof(buffer)-1);
   }
    
   if (!imap_ok(imap_commands, buffer))
   {
      return FALSE;
   }

   imap_commands++;

   return TRUE;
}

gint imap_connect (Server *server)
{
   /* connects to the server and returns the socket or -1 on error */
   gchar buffer[512];
   gint sock;

   if (!Resolve(server))
      return -1;

   sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   if (sock < 0)
      return -1;

   server->sin.sin_family = AF_INET;
   server->sin.sin_port = htons(server->port);

#ifdef IMAP_LOGGING
   fprintf(stderr, _("Connecting to IMAP server (%s)..."), server->ip);
#endif
   if (connect_timeo(sock, (struct sockaddr*)&server->sin, sizeof(server->sin), timeout) < 0)
   {
      fprintf(stderr, _("failed.\n"));
      close(sock);
      return -1;
   }
   fprintf(stderr, _("success.\n"));

   {
      /* read the connect responce */
      memset(buffer, 0, sizeof(buffer));
      recvline_timeo(sock, buffer, sizeof(buffer)-1, timeout);
   }

   return sock;
}

gint imap_add_part(gchar *c)
{
   gchar name[64], value[64];
   gchar temp[64];
   gchar *start = c;
   struct mime_part *part;

   part = g_malloc0(sizeof(struct mime_part));

   c += imap_get_string (c, part->type, sizeof(part->type)-1, "text");
   c += imap_get_string (c, part->subtype, sizeof(part->subtype)-1, "plain");

   /* seek to the beginning of the parameter... */
   for ( ; *c && *c == ' '; c++);

   if (*c)
   {
      gchar *p = part->parameter;
      if (*c == '(')
      {
         c++;
         while (*c && *c != ')')
         {
            c += imap_get_string (c, name, sizeof(name)-1, "");
            c += imap_get_string (c, value, sizeof(value)-1, "");
            /* don't buffer overrun */
            g_snprintf(p, sizeof(part->parameter)-1, "%s=\"%s\"; ", name, value);
            p += strlen(p);
   
            while (*c && *c == ' ') /* skip any spaces */
               c++;
         }
      }
      else
      {
         c += imap_get_string (c, name, sizeof(name)-1, "");
         strcpy(value, name);
         *p++ = '\0';
      }

      c++; /* skip over the ')' belonging to the parameter values */
      if (*c)
      {
         /* ignore id and description */
         c += imap_get_string (c, temp, sizeof(temp)-1, "");	   
         c += imap_get_string (c, temp, sizeof(temp)-1, "");

         /* encoding */
         c += imap_get_string (c, part->encoding, sizeof(part->encoding)-1, "");

         /* size */
         c += imap_get_number (c, &part->len);

         /* skip the optional info */
         c += imap_skip_section(c);

         part->pos = 0; /* isn't useful in imap */
#ifdef IMAP_LOGGING
         fprintf(stderr, "type = %s/%s\n", part->type, part->subtype);
			fprintf(stderr, "encoding = %s\n", part->encoding);
			fprintf(stderr, "param = %s\n", part->parameter);
#endif
         mime_parts = g_list_append (mime_parts, part);

         return (c - start);
      }
   }
   return -1;
}

gint imap_parts (gint socket, gint mesg_num)
{
   GList *tmp;
   gchar *buffer = NULL, *c;
   gint res = 1, cnt;

   tmp = mime_parts;
   while (tmp != NULL)
   {
      g_free(tmp->data);
      tmp = tmp->next;
   }

   if (mime_parts != NULL)
   {
      g_list_free(mime_parts);
      mime_parts = NULL;
   }
  
   buffer = g_malloc0(sizeof(gchar)*2048);   
  
   g_snprintf(buffer, 2047, "A%.5d FETCH %d (BODYSTRUCTURE)\r\n", imap_commands, mesg_num);
#ifdef IMAP_LOGGING
   fprintf(stderr, "%s", buffer);
#endif

   if (send(socket, buffer, strlen(buffer), 0) < 0)
   {
      g_free(buffer);
      return 0;
   }
  
   /* get the structure of the body */
   memset (buffer, 0, sizeof(gchar)*2048);
   recvline (socket, buffer, sizeof(gchar)*2048);
#ifdef IMAP_LOGGING
   fprintf(stderr, "received: %s", buffer);
#endif

   c = buffer;
   /* skip to the BODYSTRUCTURE */
   c = strstr(c, "BODYSTRUCTURE");
   if (c == NULL)
      return 0;

   c += strlen("BODYSTRUCTURE");
   if (*c)
   {
      /* looks good so far, skip to the parts */
      for ( ; *c && *c != '('; c++);

      if (*c && *(c+1) == '(')
      {
         c++;
#ifdef IMAP_LOGGING
         fprintf(stderr, "message is multipart\n");
#endif
         /* multipart */
         while (*c == '(')
         {
            cnt = imap_skip_section(c);
            if (cnt > 1)
            {
               c[cnt-1] = '\0';
               cnt = imap_add_part(c);
               if (cnt == -1)
               {
                  res = 0;
                  break;
               }
               c += cnt;
            }
            else
            {
               res = 0;
               break;
            }
            /* skip to the next mime part */
            for ( ; *c && *c == ' '; c++);
         }
      }
      else
         if (*c)
         {
            /* one part */
            cnt = imap_add_part(c);
            res = res != -1;
         }
         /* just forget the rest, who cares?? */
   }
  
   g_free(buffer);

   return res;
}

gint imap_get_string (gchar *index, gchar *dest, gint destlen, gchar *def)
{
   /* gets a string ("data" or NIL) , if NIL it copies def instead */
   gint i;
   gchar *start = index;

   while (*index && *index == ' ') /* skip white space */
     index++;

   if (strncmp(index, "NIL", 3))
   {
      /* progress to the first quote (we should already be there but just in case) */
      while (*index && *index != '"')
         index++;

      index++;
   
      i = 0;
      while (*index && *index != '"')
      {
         if (i < destlen-1)
         {
            dest[i] = *index;
            i++;
         }
         index++;
      }
      dest[i] = '\0';
   }
   else
   {
      /* if there were no data we just copy def */
      index += 3;
      strncpy (dest, def, destlen);
   }

   return index - start + 1;
}

gint imap_get_number (gchar *index, gint *dest)
{
   /* gets a number */
   gchar number[32];
   gchar *start = index;
   gint i;

   /* skip white space **/
   while (*index == ' ')
     index++;

   i = 0;
   while (*index != ' ' && i < sizeof(number)-1)
   {
      number[i] = *index;
      index++;
      i++;
   }
   number[i] = '\0';

   *dest = atoi(number);

   return index - start;
}

gint imap_skip_section(gchar *index)
{
   gint depth = 1;
   gchar *start = index;

   while (depth != 0 && *index)
   {
      if (*index == '(')
         depth++;
      else if ( *index == ')' )
         depth--;
      index++;
   }

   return index - start;
}

