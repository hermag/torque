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
 * svr_func.c - miscellaneous server functions
 */
#include <pbs_config.h>   /* the master config generated by configure */

#include "portability.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "resource.h"
#include "pbs_job.h"
#include "queue.h"
#include "server.h"
#include "pbs_error.h"
#include "svrfunc.h"
#include "sched_cmds.h"
#include "csv.h"
#include "log.h"
#include "../lib/Liblog/pbs_log.h"

extern int              LOGLEVEL;
extern int              scheduler_sock;
extern pthread_mutex_t *scheduler_sock_jobct_mutex;
extern int              svr_do_schedule;
extern pthread_mutex_t *svr_do_schedule_mutex;
extern pthread_mutex_t *listener_command_mutex;
extern int              listener_command;

/*
 * the following array of strings is used in decoding/encoding the server state
 */
static char *svr_idle   = "Idle";
static char *svr_sched  = "Scheduling";
static char *svr_state_names[] =
  {
  "",   /* SV_STATE_DOWN */
  "",   /* SV_STATE_INIT */
  "Hot_Start",  /* SV_STATE_HOT  */
  "Active",  /* SV_STATE_RUN  */
  "Terminating_Delay", /* SV_STATE_SHUTDEL */
  "Terminating",  /* SV_STATE_SHUTIMM */
  "Terminating"  /* SV_STATE_SHUTSIG */
  };





/*
 * encode_svrstate - encode the current server state from the internal
 * integer to a state name string.
 */

int encode_svrstate(

  pbs_attribute  *pattr,   /* ptr to pbs_attribute */
  tlist_head     *phead,   /* head of attrlist list */
  char           *atname,  /* pbs_attribute name */
  char           *rsname,  /* null */
  int             mode,    /* encode mode */
  int             perm)    /* only used for resources */

  {
  svrattrl *pal;
  char *psname;

  if (pattr == NULL)
    {
    /* FAILURE */

    return(-1);
    }

  if ((mode == ATR_ENCODE_SAVE)   ||
      (pattr->at_val.at_long <= SV_STATE_DOWN) ||
      (pattr->at_val.at_long > SV_STATE_SHUTSIG))
    {
    /* SUCCESS */

    return(0);  /* don't bother to encode it */
    }

  psname = svr_state_names[pattr->at_val.at_long];

  if (pattr->at_val.at_long == SV_STATE_RUN)
    {
    pthread_mutex_lock(server.sv_attr_mutex);
    if (server.sv_attr[SRV_ATR_scheduling].at_val.at_long == 0)
      psname = svr_idle;
    else 
      {
      pthread_mutex_lock(scheduler_sock_jobct_mutex);
      if (scheduler_sock != -1)
        psname = svr_sched;
      pthread_mutex_unlock(scheduler_sock_jobct_mutex);
      }
    pthread_mutex_unlock(server.sv_attr_mutex);
    }

  pal = attrlist_create(atname, rsname, strlen(psname) + 1);

  if (pal == NULL)
    {
    /* FAILURE */

    return(-1);
    }

  strcpy(pal->al_value, psname);

  pal->al_flags = pattr->at_flags;

  append_link(phead, &pal->al_link, pal);

  /* SUCCESS */

  return(1);
  }  /* END encode_svrstate() */




/*
 * set_resc_assigned - set the resources used by a job in the server and
 * queue resources_used pbs_attribute
 */

void set_resc_assigned(

  job *pjob,         /* I */
  enum batch_op op)  /* INCR or DECR */

  {
  resource      *jobrsc;
  resource      *pr;
  pbs_attribute *queru;
  resource_def  *rscdef;
  pbs_attribute *sysru;
  pbs_queue     *pque;
  char           log_buf[LOCAL_LOG_BUF_SIZE];

  if ((pjob == NULL))
    return;

  if ((pque = get_jobs_queue(&pjob)) != NULL)
    {
    if (pque->qu_qs.qu_type == QTYPE_Execution)
      {
      if (op == DECR)
        {
        /* if freeing completed job resources, ignore constraint (???) */
        /* NO-OP */
        }
      }
    else
      {
      snprintf(log_buf,sizeof(log_buf),
        "job %s isn't in an execution queue, can't modify resources\njob is in queue %s",
        pjob->ji_qs.ji_jobid,
        pque->qu_qs.qu_name);
      log_err(-1, __func__, log_buf);
    
      unlock_queue(pque, __func__, (char *)NULL, LOGLEVEL);
      return;
      }
  
    if (op == INCR)
      {
      if (pjob->ji_qs.ji_svrflags & JOB_SVFLG_RescAssn)
        {
        unlock_queue(pque, __func__, (char *)NULL, LOGLEVEL);
        return;  /* already added in */
        }
      
      pjob->ji_qs.ji_svrflags |= JOB_SVFLG_RescAssn;
      }
    else if (op == DECR)
      {
      if ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_RescAssn) == 0)
        {
        unlock_queue(pque, __func__, (char *)NULL, LOGLEVEL);
        return;  /* not currently included */
        }
      
      pjob->ji_qs.ji_svrflags &= ~JOB_SVFLG_RescAssn;
      }
    else
      {
      unlock_queue(pque, __func__, (char *)NULL, LOGLEVEL);
      return;   /* invalid op */
      }
    
    sysru = &server.sv_attr[SRV_ATR_resource_assn];

    queru = &pque->qu_attr[QE_ATR_ResourceAssn];
    jobrsc = (resource *)GET_NEXT(pjob->ji_wattr[JOB_ATR_resource].at_val.at_list);

    while (jobrsc != NULL)
      {
      rscdef = jobrsc->rs_defin;

      /* if resource usage is to be tracked */

      if ((rscdef->rs_flags & ATR_DFLAG_RASSN) &&
          (jobrsc->rs_value.at_flags & ATR_VFLAG_SET))
        {
        /* update system pbs_attribute of resources assigned */

        pr = find_resc_entry(sysru, rscdef);

        if (pr == NULL)
          {
          pr = add_resource_entry(sysru, rscdef);

          if (pr == NULL)
            {
            unlock_queue(pque, __func__, (char *)"sysru", LOGLEVEL);
            return;
            }
          }

        rscdef->rs_set(&pr->rs_value, &jobrsc->rs_value, op);

        /* update queue pbs_attribute of resources assigned */

        pr = find_resc_entry(queru, rscdef);

        if (pr == NULL)
          {
          pr = add_resource_entry(queru, rscdef);

          if (pr == NULL)
            {
            unlock_queue(pque, __func__, (char *)"queru", LOGLEVEL);
            return;
            }
          }

        rscdef->rs_set(&pr->rs_value, &jobrsc->rs_value, op);
        }

      jobrsc = (resource *)GET_NEXT(jobrsc->rs_link);
      }  /* END while (jobrsc != NULL) */

    unlock_queue(pque, __func__, (char *)"success", LOGLEVEL);
    }
  else if (pjob == NULL)
    {
    log_err(PBSE_JOBNOTFOUND, __func__, "Job lost while acquiring queue 9");
    }

  return;
  }  /* END set_resc_assigned() */





/*
 * ck_checkpoint - check validity of job checkpoint pbs_attribute value
 *
 * This routine is not directly called.
 * Rather it is referenced by an at_action field.
 * This is invoked inside of a loop over attributes in req_quejob.
 */

int ck_checkpoint(

  pbs_attribute *pattr,
  void          *pobject, /* not used */
  int            mode) /* not used */

  {
  char *val;
  int field_count;
  int i;
  int len;
  char *str;

  val = pattr->at_val.at_str;

  if (val == NULL)
    {
    /* SUCCESS */

    return(0);
    }

  field_count = csv_length(val);

  for (i = 0; i < field_count; i++)
    {
    str = csv_nth(val, i);

    if (str)
      {
      if ((len = strlen(str)) > 0)
        {
        if (len == 1 && !((str[0] == 'n') || (str[0] == 's') || (str[0] == 'u')))
          {
          return(PBSE_BADATVAL);
          }
        else if (!strncmp(str, "c=", 2))
          {
          if (atoi(&str[2]) <= 0)
            {
            return(PBSE_BADATVAL);
            }

          continue;
          }
        else if (!strcmp(str, "none"))            {}
        else if (!strcmp(str, "periodic"))        {}
        else if (!strcmp(str, "shutdown"))        {}
        else if (!strcmp(str, "enabled"))       {}
        else if (!strncmp(str, "interval=", 9))    {}
        else if (!strncmp(str, "depth=", 6))       {}
        else if (!strncmp(str, "dir=", 4))         {}
        else
          {
          return(PBSE_BADATVAL);
          }
        }
      }
    }

  /* SUCCESS */

  return(0);
  }





/*
 * decode_null - Null pbs_attribute decode routine for Read Only (server
 * and queue ) attributes.  It just returns 0.
 */

int decode_null(pbs_attribute *patr, char *name, char *rn, char *val, int perm)
  {
  return 0;
  }





/*
 * set_null - Null set routine for Read Only attributes.
 */

int set_null(

  pbs_attribute *pattr,
  pbs_attribute *new_attr,
  enum batch_op  op)

  {
  return(0);
  }




int is_svr_attr_set(

  int attr_index)

  {
  int is_set = FALSE;

  if (SRV_ATR_LAST <= attr_index)
    return(is_set);

  pthread_mutex_lock(server.sv_attr_mutex);
  if (server.sv_attr[attr_index].at_flags & ATR_VFLAG_SET)
    is_set = TRUE;
  pthread_mutex_unlock(server.sv_attr_mutex);

  return(is_set);
  } /* END is_svr_attr_set() */




int set_svr_attr(

  int   attr_index, /* I */
  void *val)        /* I */

  {
  char                 *str;
  long                 *l;
  char                 *c;
  struct array_strings *arst;

  if (attr_index >= SRV_ATR_LAST)
    return(-1);

  pthread_mutex_lock(server.sv_attr_mutex);

  server.sv_attr[attr_index].at_flags = ATR_VFLAG_SET;

  switch (svr_attr_def[attr_index].at_type)
    {
    case ATR_TYPE_LONG:

      l = (long *)val;
      server.sv_attr[attr_index].at_val.at_long = *l;

      break;

    case ATR_TYPE_CHAR:

      c = (char *)val;
      server.sv_attr[attr_index].at_val.at_char = *c;

      break;

    case ATR_TYPE_STR:

      str = (char *)val;
      server.sv_attr[attr_index].at_val.at_str = str;

      break;

    case ATR_TYPE_ARST:
    case ATR_TYPE_ACL:

      arst = (struct array_strings *)val;
      server.sv_attr[attr_index].at_val.at_arst = arst;

      break;

    default:

      server.sv_attr[attr_index].at_flags &= ~ATR_VFLAG_SET;
      break;
    }

  pthread_mutex_unlock(server.sv_attr_mutex);

  return(PBSE_NONE);
  } /* END set_svr_attr() */


int get_svr_attr_l(

  int   attr_index,
  long *l)

  {
  int rc = PBSE_NONE;
  pthread_mutex_lock(server.sv_attr_mutex);
  if ((attr_index >= SRV_ATR_LAST) ||
      ((server.sv_attr[attr_index].at_flags & ATR_VFLAG_SET) == FALSE))
    {
    rc = -1;
    }
  else if (svr_attr_def[attr_index].at_type == ATR_TYPE_LONG)
    {
    *l = server.sv_attr[attr_index].at_val.at_long;
    }
  else
    {
    rc = -2;
    }
  pthread_mutex_unlock(server.sv_attr_mutex);
  return rc;
  }


int get_svr_attr_c(

  int   attr_index,
  char *c)

  {
  int rc = PBSE_NONE;
  pthread_mutex_lock(server.sv_attr_mutex);
  if ((attr_index >= SRV_ATR_LAST) ||
      ((server.sv_attr[attr_index].at_flags & ATR_VFLAG_SET) == FALSE))
    {
    rc = -1;
    }
  else if (svr_attr_def[attr_index].at_type == ATR_TYPE_CHAR)
    {
    *c = server.sv_attr[attr_index].at_val.at_char;
    }
  else
    {
    rc = -2;
    }
  pthread_mutex_unlock(server.sv_attr_mutex);
  return rc;
  }

int get_svr_attr_str(
    
  int    attr_index,
  char **str)

  {
  int rc = PBSE_NONE;
  pthread_mutex_lock(server.sv_attr_mutex);
  if ((attr_index >= SRV_ATR_LAST) ||
      ((server.sv_attr[attr_index].at_flags & ATR_VFLAG_SET) == FALSE))
    {
    rc = -1;
    }
  else if (svr_attr_def[attr_index].at_type == ATR_TYPE_STR)
    {
    *str = server.sv_attr[attr_index].at_val.at_str;
    }
  else
    {
    rc = -2;
    }
  pthread_mutex_unlock(server.sv_attr_mutex);
  return rc;
  }

int get_svr_attr_arst(
    
  int                    attr_index,
  struct array_strings **arst)

  {
  int rc = PBSE_NONE;
  pthread_mutex_lock(server.sv_attr_mutex);
  if ((attr_index >= SRV_ATR_LAST) ||
      ((server.sv_attr[attr_index].at_flags & ATR_VFLAG_SET) == FALSE))
    {
    rc = -1;
    }
  else if ((svr_attr_def[attr_index].at_type == ATR_TYPE_ARST)
      || (svr_attr_def[attr_index].at_type == ATR_TYPE_ACL))
    {
    *arst = server.sv_attr[attr_index].at_val.at_arst;
    }
  else
    {
    rc = -2;
    }
  pthread_mutex_unlock(server.sv_attr_mutex);
  return rc;
  }
  

/*
 * poke_scheduler - action routine for the server's "scheduling" pbs_attribute.
 * Call the scheduler whenever the pbs_attribute is set (or reset) to true.
 */

int poke_scheduler(

  pbs_attribute *pattr,
  void          *pobj,
  int            actmode)

  {
  if (actmode == ATR_ACTION_ALTER)
    {
    if (pattr->at_val.at_long)
      {
      pthread_mutex_lock(svr_do_schedule_mutex);
      svr_do_schedule = SCH_SCHEDULE_CMD;
      pthread_mutex_unlock(svr_do_schedule_mutex);
      pthread_mutex_lock(listener_command_mutex);
      listener_command = SCH_SCHEDULE_CMD;
      pthread_mutex_unlock(listener_command_mutex);
      }
    }

  return(0);
  }  /* END poke_scheduler() */

