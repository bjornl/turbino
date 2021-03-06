#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <arpa/inet.h>

#include "load.h"
#include "http.h"

#define SERVER_PORT  12345

#define TRUE             1
#define FALSE            0

struct data * load (char *);

int
main (int argc, char *argv[])
{
   int    i, len, rc, on = 1;
   int    listen_sd, max_sd, new_sd;
   int    desc_ready, end_server = FALSE;
   int    close_conn;
   char   buffer[10000];
   struct sockaddr_in   addr, caddr;
   //struct timeval       timeout;
   //struct fd_set        master_set, working_set;
   fd_set        master_set, working_set;
	//void *img;
	void *resp;
	//struct data *dp;
	struct data **dp = NULL;
	unsigned char j;
	char res[256];
	unsigned int c, dpc = 0;
	DIR *dir;
	struct dirent *de;
	socklen_t clen;

	dir = opendir(".");
	do {
		de = readdir(dir);

		if (de) {
			if (de->d_type == DT_REG || de->d_type == DT_LNK) {
				printf("Loading file \"%s\" into dp[%d]\n", de->d_name, dpc);
				dp = realloc(dp, sizeof(struct data *) * (dpc+1));
				dp[dpc] = load(de->d_name);
				dpc++;
			}
		}
	} while (de);
	closedir(dir);

	/*
	printf("dp[0] len: %d\n", dp[0]->len);
	printf("dp[0] key: \"%s\"\n", dp[0]->key);
	printf("dp[0] type: \"%s\"\n", dp[0]->type);
	printf("dp[1] len: %d\n", dp[1]->len);
	printf("dp[1] key: \"%s\"\n", dp[1]->key);
	printf("dp[1] type: \"%s\"\n", dp[1]->type);
	*/

   /*************************************************************/
   /* Create an AF_INET stream socket to receive incoming       */
   /* connections on                                            */
   /*************************************************************/
   listen_sd = socket(AF_INET, SOCK_STREAM, 0);
   if (listen_sd < 0)
   {
      perror("socket() failed");
      exit(-1);
   }

   /*************************************************************/
   /* Allow socket descriptor to be reuseable                   */
   /*************************************************************/
   rc = setsockopt(listen_sd, SOL_SOCKET,  SO_REUSEADDR,
                   (char *)&on, sizeof(on));
   if (rc < 0)
   {
      perror("setsockopt() failed");
      close(listen_sd);
      exit(-1);
   }

   /*************************************************************/
   /* Set socket to be non-blocking.  All of the sockets for    */
   /* the incoming connections will also be non-blocking since  */
   /* they will inherit that state from the listening socket.   */
   /*************************************************************/
   rc = ioctl(listen_sd, FIONBIO, (char *)&on);
   if (rc < 0)
   {
      perror("ioctl() failed");
      close(listen_sd);
      exit(-1);
   }

   /*************************************************************/
   /* Bind the socket                                           */
   /*************************************************************/
   memset(&addr, 0, sizeof(addr));
   addr.sin_family      = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_ANY);
   addr.sin_port        = htons(SERVER_PORT);
   rc = bind(listen_sd,
             (struct sockaddr *)&addr, sizeof(addr));
   if (rc < 0)
   {
      perror("bind() failed");
      close(listen_sd);
      exit(-1);
   }

   /*************************************************************/
   /* Set the listen back log                                   */
   /*************************************************************/
   rc = listen(listen_sd, 32);
   if (rc < 0)
   {
      perror("listen() failed");
      close(listen_sd);
      exit(-1);
   }

   /*************************************************************/
   /* Initialize the master fd_set                              */
   /*************************************************************/
   FD_ZERO(&master_set);
   max_sd = listen_sd;
   FD_SET(listen_sd, &master_set);

   /*************************************************************/
   /* Initialize the timeval struct to 3 minutes.  If no        */
   /* activity after 3 minutes this program will end.           */
   /*************************************************************/
   //timeout.tv_sec  = 3 * 60;
   //timeout.tv_usec = 0;

   /*************************************************************/
   /* Loop waiting for incoming connects or for incoming data   */
   /* on any of the connected sockets.                          */
   /*************************************************************/
   do
   {
      /**********************************************************/
      /* Copy the master fd_set over to the working fd_set.     */
      /**********************************************************/
      memcpy(&working_set, &master_set, sizeof(master_set));

      /**********************************************************/
      /* Call select() and wait 5 minutes for it to complete.   */
      /**********************************************************/
      printf("Waiting on select()...\n");
      //rc = select(max_sd + 1, &working_set, NULL, NULL, &timeout);
      rc = select(max_sd + 1, &working_set, NULL, NULL, NULL);

      /**********************************************************/
      /* Check to see if the select call failed.                */
      /**********************************************************/
      if (rc < 0)
      {
         perror("  select() failed");
         break;
      }

      /**********************************************************/
      /* Check to see if the 5 minute time out expired.         */
      /**********************************************************/
      if (rc == 0)
      {
         printf("  select() timed out.  End program.\n");
         break;
      }

      /**********************************************************/
      /* One or more descriptors are readable.  Need to         */
      /* determine which ones they are.                         */
      /**********************************************************/
      desc_ready = rc;
      for (i=0; i <= max_sd  &&  desc_ready > 0; ++i)
      {
         /*******************************************************/
         /* Check to see if this descriptor is ready            */
         /*******************************************************/
         if (FD_ISSET(i, &working_set))
         {
            /****************************************************/
            /* A descriptor was found that was readable - one   */
            /* less has to be looked for.  This is being done   */
            /* so that we can stop looking at the working set   */
            /* once we have found all of the descriptors that   */
            /* were ready.                                      */
            /****************************************************/
            desc_ready -= 1;

            /****************************************************/
            /* Check to see if this is the listening socket     */
            /****************************************************/
            if (i == listen_sd)
            {
               printf("  Listening socket is readable\n");
               /*************************************************/
               /* Accept all incoming connections that are      */
               /* queued up on the listening socket before we   */
               /* loop back and call select again.              */
               /*************************************************/
               do
               {
                  /**********************************************/
                  /* Accept each incoming connection.  If       */
                  /* accept fails with EWOULDBLOCK, then we     */
                  /* have accepted all of them.  Any other      */
                  /* failure on accept will cause us to end the */
                  /* server.                                    */
                  /**********************************************/
			memset(&caddr, 0, sizeof(struct sockaddr_in));
			clen = sizeof(caddr);
                  // new_sd = accept(listen_sd, NULL, NULL);
                  new_sd = accept(listen_sd, (struct sockaddr *) &caddr, &clen);
                  if (new_sd < 0)
                  {
                     if (errno != EWOULDBLOCK)
                     {
                        perror("  accept() failed");
                        end_server = TRUE;
                     }
                     break;
                  }

                  /**********************************************/
                  /* Add the new incoming connection to the     */
                  /* master read set                            */
                  /**********************************************/
                  printf("  New incoming connection from %s:%d - %d\n", inet_ntoa(caddr.sin_addr), caddr.sin_port, new_sd);
                  FD_SET(new_sd, &master_set);
                  if (new_sd > max_sd)
                     max_sd = new_sd;

                  /**********************************************/
                  /* Loop back up and accept another incoming   */
                  /* connection                                 */
                  /**********************************************/
               } while (new_sd != -1);
            }

            /****************************************************/
            /* This is not the listening socket, therefore an   */
            /* existing connection must be readable             */
            /****************************************************/
            else
            {
               printf("  Descriptor %d is readable\n", i);
               close_conn = FALSE;
               /*************************************************/
               /* Receive all incoming data on this socket      */
               /* before we loop back and call select again.    */
               /*************************************************/
               do
               {
                  /**********************************************/
                  /* Receive data on this connection until the  */
                  /* recv fails with EWOULDBLOCK.  If any other */
                  /* failure occurs, we will close the          */
                  /* connection.                                */
                  /**********************************************/
                  rc = recv(i, buffer, sizeof(buffer), 0);
                  if (rc < 0)
                  {
                     if (errno != EWOULDBLOCK)
                     {
                        perror("  recv() failed");
                        close_conn = TRUE;
                     }
                     break;
                  }

                  /**********************************************/
                  /* Check to see if the connection has been    */
                  /* closed by the client                       */
                  /**********************************************/
                  if (rc == 0)
                  {
                     printf("  Connection closed\n");
                     close_conn = TRUE;
                     break;
                  }

                  /**********************************************/
                  /* Data was recevied                          */
                  /**********************************************/
                  len = rc;
                  printf("  %d bytes received\n", len);

                  /**********************************************/
                  /* Echo the data back to the client           */
                  /**********************************************/

		j = 0;
		for (c = 5 ; buffer[c] != 32 ; c++) {

			res[j++] = buffer[c];
		}
		res[j] = '\0';

		printf("res: \"%s\"\n", res);

		if (!strcmp(res, "")) {
			printf("requested index.html\n");
			sprintf(res, "index.html");
		}
		for (c = 0 ; c < dpc ; c++) {
			if (!strcmp(res, dp[c]->key)) {
				printf("requested resource is \"%s\"\n", dp[c]->key);
				break;
			}
		}
		if (c == dpc) {
			sprintf(res, "index.html");
			for (c = 0 ; c < dpc ; c++) {
				if (!strcmp(res, dp[c]->key)) {
					printf("Did not find requested resource, sending \"%s\"\n", dp[c]->key);
					break;
				}
			}
		}
		if (c == dpc) {
			c = 0;
		}

		if (!strcmp(dp[c]->type, "html")) {
			sprintf(buffer, "%s%d\r\n\r\n", http_text_html, dp[c]->len);
		} else if (!strcmp(dp[c]->type, "css")) {
			sprintf(buffer, "%s%d\r\n\r\n", http_text_html, dp[c]->len);
		} else if (!strcmp(dp[c]->type, "jpg")) {
			sprintf(buffer, "%s%d\r\n\r\n", http_image_jpeg, dp[c]->len);
		} else if (!strcmp(dp[c]->type, "jpeg")) {
			sprintf(buffer, "%s%d\r\n\r\n", http_image_jpeg, dp[c]->len);
		} else if (!strcmp(dp[c]->type, "png")) {
			sprintf(buffer, "%s%d\r\n\r\n", http_image_png, dp[c]->len);
		} else if (!strcmp(dp[c]->type, "gif")) {
			sprintf(buffer, "%s%d\r\n\r\n", http_image_gif, dp[c]->len);
		}

		resp = malloc(strlen(buffer) + dp[c]->len);

		memcpy(resp, buffer, strlen(buffer));

		memcpy(resp+strlen(buffer), dp[c]->data, dp[c]->len);


                  //rc = send(i, buffer, strlen(buffer), 0);
                  rc = send(i, resp, strlen(buffer)+dp[c]->len, 0);
                  if (rc < 0)
                  {
                     perror("  send() failed");
                     close_conn = TRUE;
                     break;
                  } else {
			printf("Sent %d bytes\n", rc);
		}

		break;
               } while (TRUE);

               /*************************************************/
               /* If the close_conn flag was turned on, we need */
               /* to clean up this active connection.  This     */
               /* clean up process includes removing the        */
               /* descriptor from the master set and            */
               /* determining the new maximum descriptor value  */
               /* based on the bits that are still turned on in */
               /* the master set.                               */
               /*************************************************/
               if (close_conn)
               {
                  close(i);
                  FD_CLR(i, &master_set);
                  if (i == max_sd)
                  {
                     while (FD_ISSET(max_sd, &master_set) == FALSE)
                        max_sd -= 1;
                  }
               }
            } /* End of existing connection is readable */
         } /* End of if (FD_ISSET(i, &working_set)) */
      } /* End of loop through selectable descriptors */

   } while (end_server == FALSE);

   /*************************************************************/
   /* Cleanup all of the sockets that are open                  */
   /*************************************************************/
   for (i=0; i <= max_sd; ++i)
   {
      if (FD_ISSET(i, &master_set))
         close(i);
   }

	return 0;
}
