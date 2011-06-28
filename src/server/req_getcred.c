/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/
/*
 * req_getcred.c
 *
 * This file contains function relating to the PBS credential system,
 * it includes the major functions:
 *   req_authenuser - Authenticate a user connection based on pbs_iff  (new)
 *   req_connect    - validate the credential in a Connection Request (old)
 */
#include <pbs_config.h>   /* the master config generated by configure */

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <pthread.h>
#include "libpbs.h"
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "credential.h"
#include "net_connect.h"
#include "batch_request.h"
#include "log.h"

#define SPACE 32 /* ASCII space character */

/* External Global Data Items Referenced */

extern time_t time_now;

extern struct connection svr_conn[];
extern char *path_credentials;

/* Global Data Home in this file */

struct credential conn_credent[PBS_NET_MAX_CONNECTIONS];
/* there is one per possible connectinn */

/*
 * req_connect - process a Connection Request
 *  Almost does nothing.
 */

void req_connect(

  struct batch_request *preq)

  {
  int  sock = preq->rq_conn;

  pthread_mutex_lock(svr_conn[sock].cn_mutex);

  if (svr_conn[sock].cn_authen == 0)
    {
    reply_ack(preq);
    }
  else
    {
    req_reject(PBSE_BADCRED, 0, preq, NULL, NULL);
    }

  pthread_mutex_unlock(svr_conn[sock].cn_mutex);

  return;
  }  /* END req_connect() */




int get_encode_host(
  
  int s, 
  char *munge_buf, 
  struct batch_request *preq)
  
  {
  char *ptr;
  char host_name[PBS_MAXHOSTNAME];
  int i;

  /* ENCODE_HOST: is a keyword in the unmunge data that holds the host name */
  ptr = strstr(munge_buf, "ENCODE_HOST:");
  if (!ptr)
    {
    req_reject(PBSE_SYSTEM, 0, preq, NULL, "could not read unmunge data host");
    return(-1);
    }

  /* Move us to the end of the ENCODE_HOST keyword. There are spaces
   	 between the : and the first letter of the host name */
  ptr = strchr(ptr, ':');
  ptr++;
  while (*ptr == SPACE)
    {
    ptr++;
    }

  memset(host_name, 0, PBS_MAXHOSTNAME);
  i = 0;
  while (*ptr != SPACE && !isspace(*ptr))
    {
    host_name[i++] = *ptr;
    ptr++;
    }

  strcpy(conn_credent[s].hostname, host_name);

  return(0);
  } /* END get_encode_host() */





int get_UID(
    
  int s, 
  char *munge_buf, 
  struct batch_request *preq)
  
  {
  char *ptr;
  char user_name[PBS_MAXHOSTNAME];
  int i;


  ptr = strstr(munge_buf, "UID:");
	if (!ptr)
		{
		req_reject(PBSE_SYSTEM, 0, preq, NULL, "could not read unmunge data user");
		return(-1);
		}

	ptr = strchr(ptr, ':');
	ptr++;
	while (*ptr == SPACE)
	  {
	  ptr++;
	  }

	memset(user_name, 0, PBS_MAXHOSTNAME);
	i = 0;
	while (*ptr != SPACE && !isspace(*ptr))
	  {
	  user_name[i++] = *ptr;
	  ptr++;
	  }

	strcpy(conn_credent[s].username, user_name);
	
  return(0);
  }



int write_munge_temp_file(

  struct batch_request *preq,          /* I */
  char                 *mungeFileName) /* I */

  {
  int fd;
  int cred_size;
  int bytes_written;
  int rc;

  if ((fd = open(mungeFileName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) < 0)
    {
    req_reject(PBSE_SYSTEM, 0, preq, NULL, "could not create temporary munge file");
    return(-1);
    }

  if ((cred_size = strlen(preq->rq_ind.rq_authen.rq_cred)) == 0)
    {
    req_reject(PBSE_BADCRED, 0, preq, NULL, NULL);
    close(fd);
    return(-1);
    }

  bytes_written = write(fd, preq->rq_ind.rq_authen.rq_cred, cred_size);

  if ((bytes_written == -1) || 
      (bytes_written != cred_size))
    {
    req_reject(PBSE_SYSTEM, 0, preq, NULL, "could not write credential to temporary munge file");
    close(fd);
    return(-1);
    }

	if ((rc = fsync(fd)) < 0)
		{
		close(fd);
		return(rc);
		}

  close(fd);

  return(PBSE_NONE);
  } /* END write_munge_temp_file() */




int pipe_and_read_unmunge(

  char                 *mungeFileName, /* I */
  struct batch_request *preq,          /* I */
  int                   sock)          /* I */

  {
  static char *id = "pipe_and_read_unmunge";
  char  munge_buf[MUNGE_SIZE << 4];

  FILE *munge_pipe;
  char *ptr; /* pointer to the current place to copy data into munge_buf */
  char  munge_command[MAXPATHLEN<<1];
  int   bytes_read;
  int   total_bytes_read = 0;
  int   fd;
  int   rc;
  
  snprintf(munge_command,sizeof(munge_command),
    "unmunge --input=%s",
    mungeFileName);
  
  if ((munge_pipe = popen(munge_command,"r")) == NULL)
    {
    /* FAILURE */
    snprintf(log_buffer,sizeof(log_buffer),
      "Unable to popen command '%s' for reading",
      munge_command);
    log_err(errno,id,log_buffer);
    
    unlink(mungeFileName);
    req_reject(PBSE_SYSTEM, 0, preq, NULL, "couldn't create pipe to unmunge");
    return(-1);
    }
  
  memset(munge_buf, 0, MUNGE_SIZE);
  ptr = munge_buf;
  
  fd = fileno(munge_pipe);
  
  while ((bytes_read = read(fd, ptr, MUNGE_SIZE)) > 0)
    {
    total_bytes_read += bytes_read;
    ptr += bytes_read;
    }
  
  pclose(munge_pipe);
  
  if (bytes_read == -1)
    {
    /* read failed */
    req_reject(PBSE_SYSTEM, 0, preq, NULL, "error reading unmunge data");
    rc = -1;
    }
  else if (total_bytes_read == 0)
    {
    /* unmunge failed. Probably a bad credential. But we do not know */
    req_reject(PBSE_SYSTEM, 0, preq, NULL, "could not unmunge credentials");
    rc = -1;
    }
  else if ((rc = get_encode_host(sock, munge_buf, preq)) == PBSE_NONE)
    {
    rc = get_UID(sock, munge_buf, preq);
    }

  return(rc);
  } /* END pipe_and_read_unmunge() */





int unmunge_request(
    
  int sock,                   /* I */
  struct batch_request *preq) /* M */
 
  {
  time_t myTime;
  struct timeval tv;
  suseconds_t millisecs;
  struct tm *timeinfo;
  char mungeFileName[MAXPATHLEN + MAXNAMLEN+1];
  int rc = PBSE_NONE;

  /* create a sudo random file name */
  gettimeofday(&tv, NULL);
  myTime = tv.tv_sec;
  timeinfo = localtime(&myTime);
  millisecs = tv.tv_usec;
  sprintf(mungeFileName, "%smunge-%d-%d-%d-%d", 
	  path_credentials, timeinfo->tm_hour, timeinfo->tm_min, 
	  timeinfo->tm_sec, (int)millisecs);

  /* Write the munge credential to the newly created file */
  if ((rc = write_munge_temp_file(preq,mungeFileName)) == PBSE_NONE)
    {
    /* open the munge command as a pipe and read the result */
    rc = pipe_and_read_unmunge(mungeFileName,preq,sock);
    }
  
  /* delete the old file */
  unlink(mungeFileName);

  return(rc);
  } /* END unmunge_request */



/*
 * req_authenuser - Authenticate a user connection based on the (new)
 * pbs_iff information.  pbs_iff will contact the server on a privileged
 * port and identify the user who has made an existing, but yet unused,
 * non-privileged connection.  This connection is marked as authenticated.
 */

void req_authenuser(

  struct batch_request *preq)  /* I */

  {
  int s;

  /*
   * find the socket whose client side is bound to the port named
   * in the request
   */

  for (s = 0;s < PBS_NET_MAX_CONNECTIONS;++s)
    {
    pthread_mutex_lock(svr_conn[s].cn_mutex);

    if (preq->rq_ind.rq_authen.rq_port != svr_conn[s].cn_port)
      {
      pthread_mutex_unlock(svr_conn[s].cn_mutex);

      continue;
      }

#ifndef NOPRIVPORTS
    if (svr_conn[s].cn_authen == 0)
#endif
      {
      strcpy(conn_credent[s].username, preq->rq_user);
      strcpy(conn_credent[s].hostname, preq->rq_host);

      /* time stamp just for the record */

      conn_credent[s].timestamp = time_now;

      svr_conn[s].cn_authen = PBS_NET_CONN_AUTHENTICATED;
      }

    reply_ack(preq);

    /* SUCCESS */
    pthread_mutex_unlock(svr_conn[s].cn_mutex);

    return;
    }  /* END for (s) */

  req_reject(PBSE_BADCRED, 0, preq, NULL, "cannot authenticate user");

  /* FAILURE */

  return;
  }  /* END req_authenuser() */


/*
 * req_altauthenuser - The intent of this function is to have a way to use 
 * multiple types of authorization utilities. But for right now we are using 
 * munge so this function is munge specific until we add support for another 
 * utility 
 * 
*/
void *req_altauthenuser(

  void *vp)  /* I */

  {
  struct batch_request *preq = (struct batch_request *)vp;
  int s;
  int rc;
  
  /*
   * find the socket whose client side is bound to the port named
   * in the request
   */

  for (s = 0;s < PBS_NET_MAX_CONNECTIONS;++s)
    {
    pthread_mutex_lock(svr_conn[s].cn_mutex);

    if (preq->rq_ind.rq_authen.rq_port != svr_conn[s].cn_port)
      {
      pthread_mutex_unlock(svr_conn[s].cn_mutex);

      continue;
      }
    break;
    }  /* END for (s) */

  /* If s is less than PBS_NET_MAX_CONNECTIONS we have our port */
  if (s >= PBS_NET_MAX_CONNECTIONS)
    {
    req_reject(PBSE_BADCRED, 0, preq, NULL, "cannot authenticate user");
    return(NULL);
    }


  rc = unmunge_request(s, preq);
  if (rc)
    {
    /* FAILED */
    return(NULL);
    }

  /* SUCCESS */

  /* time stamp just for the record */

  conn_credent[s].timestamp = time_now;

  svr_conn[s].cn_authen = PBS_NET_CONN_AUTHENTICATED;

  pthread_mutex_unlock(svr_conn[s].cn_mutex);

  reply_ack(preq);
  
  return(NULL);
  }  /* END req_altauthenuser() */


/* END req_getcred.c */

