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



#include <pbs_config.h>   /* the master config generated by configure */

#include "pbs_ifl.h"
#include "dis.h"
#include "uthash.h"
#include "u_hash_map_structs.h"
#include "u_memmgr.h"
#include <stdio.h>
#include <stdlib.h>



/*
 * encode_DIS_attropl() - encode a list of PBS API "attropl" structures
 *
 * The first item encoded is a unsigned integer, a count of the
 * number of attropl entries in the linked list.  This is encoded
 * even when there are no attropl entries in the list.
 *
 * Each individual entry is then encoded as:
 *  u int size of the three strings (name, resource, value)
 *   including the terminating nulls
 *  string attribute name
 *  u int 1 or 0 if resource name does or does not follow
 *  string resource name (if one)
 *  string  value of attribute/resource
 *  u int "op" of attrlop
 *
 * Note, the encoding of a attropl is the same as the encoding of
 * the pbs_ifl.h structures "attrl" and the server svrattrl.  Any
 * one of the three forms can be decoded into any of the three with the
 * possible loss of the "flags" field (which is the "op" of the attrlop).
 */

/* This whole method is a workaround until the server code is updated */
int build_var_list(

  memmgr   **mm,
  char     **var_list,
  job_data **attrs)

  {
  job_data *atr; 
  job_data *tmp;
  int       current_len = 0;
  int       name_len = 0;
  int       value_len = 0;
  int       item_count = 0;
  int       preexisting_var_list = FALSE;

  HASH_ITER(hh, *attrs, atr, tmp)
    {
    if ((strncmp(atr->name, "pbs_o", 5) == 0)
        || (strncmp(atr->name, "PBS_O", 5) == 0))
      {
/*       if (*var_list != NULL) */
/*         len = strlen(*var_list); */
/*      new_len = len + 1; *//* Existing string, */
      name_len = strlen(atr->name); /* name= */
      value_len = strlen(atr->value); /* value\0 */
      *var_list = (char *)memmgr_realloc(mm, *var_list,
          current_len + 1 + name_len + 1 + value_len + 1);
      if (current_len != 0)
        {
        (*var_list)[current_len] = ',';
        current_len++;
        }
      memcpy((*var_list) + current_len, atr->name, name_len);
      current_len += name_len;

      (*var_list)[current_len] = '=';
      current_len++;

      memcpy((*var_list) + current_len, atr->value, value_len);
      current_len += value_len;

      (*var_list)[current_len] = '\0';

      item_count++;
      }
    else if (strncmp(atr->name, "pbs_var_", 8) == 0)
      {
      name_len = strlen(atr->name)-8; /* name= */
      value_len = strlen(atr->value); /* value\0 */
      *var_list = (char *)memmgr_realloc(mm, *var_list,
          current_len + 1 + name_len + 1 + value_len + 1);
      if (current_len != 0)
        {
        (*var_list)[current_len] = ',';
        current_len++;
        }
      memcpy((*var_list) + current_len, (atr->name)+8, name_len);
      current_len += name_len;
      (*var_list)[current_len] = '=';
      current_len++;
      memcpy((*var_list) + current_len, atr->value, value_len);
      current_len += value_len;
      (*var_list)[current_len] = '\0';
      hash_del_item(mm, attrs, atr->name);
      }
    else if (strcmp(atr->name, ATTR_v) == 0)
      {
/*       if (*var_list != NULL) */
/*         len = strlen(*var_list); */
/*      new_len = len + 1; *//* Existing string, */
      value_len = strlen(atr->value); /* value\0 */
      *var_list = (char *)memmgr_realloc(mm, *var_list,
          current_len + 1 + value_len + 1);
      if (current_len != 0)
        {
        (*var_list)[current_len] = ',';
        current_len++;
        }
      memcpy((*var_list) + current_len, atr->value, value_len);
      current_len += value_len;

      (*var_list)[current_len] = '\0';
      preexisting_var_list = TRUE;
      /* In this case, do not add an item (this is taken care or OUTSIDE this call) */
      /* item_count++; */
      }
    }
  if (preexisting_var_list == TRUE)
    {
    hash_del_item(mm, attrs, (char *)ATTR_v);
    }

  return(item_count);
  } /* END build_var_list() */




int encode_DIS_attropl_hash_single(
    
  struct tcp_chan *chan,
  job_data *attrs,
  int       is_res)

  {
  int           rc = 0;
  unsigned int  len;
  unsigned int  attr_len = 0;
  job_data     *atr;
  job_data     *tmp;

  if (is_res)
    attr_len = strlen(ATTR_l);
  /* An iterator requires access at a lower level that the wrapper
   * can provide */
  HASH_ITER(hh, attrs, atr, tmp)
    {
    /* Data pattern:
     * len of (name)(0 OR 1resource)(value)(op) */
    if ((strncmp(atr->name, "pbs_o", 5) == 0)
        || (strncmp(atr->name, "PBS_O", 5) == 0))
      continue;
    if (is_res)
      {
      len = attr_len + 1;
      len += strlen(atr->name) + 1;
      }
    else
      len = strlen(atr->name) + 1;
    len += strlen(atr->value) + 1;
/*     fprintf(stderr, "[%s]=[%s]\n", atr->name, atr->value); */
    if ((rc = diswui(chan, len)))             /* total length */
      break;
    if (is_res)
      {
      if ((rc = diswst(chan, ATTR_l)) ||      /* name */
          (rc = diswui(chan, 1))      ||      /* resource name exists */
          (rc = diswst(chan, atr->name)))     /* resource name */
      break;
      }
    else
      {
      if ((rc = diswst(chan, atr->name)) || /* name */
          (rc = diswui(chan, 0)))           /* no resource */
        break;
      }

    /* Value is always populated. "\0" == "" */
    if ((rc = diswst(chan, atr->value)) ||               /* value */
        (rc = diswui(chan, (unsigned int)atr->op_type))) /* op */
      break;

    }
  return rc;
  }




int encode_DIS_attropl_hash(

  struct tcp_chan *chan,
  memmgr **mm,
  job_data *job_attr,
  job_data *res_attr)

  {
  unsigned int  ct = 0;
  unsigned int  var_list_count = 0;
  unsigned int  len;
  char         *var_list = NULL;
  int           rc;
  memmgr       *var_mm;

  if ((rc = memmgr_init(&var_mm, 0)) == PBSE_NONE)
    {
    var_list_count = build_var_list(&var_mm, &var_list, &job_attr);
    ct = hash_count(job_attr) - var_list_count;
    ct += hash_count(res_attr);
    ct++; /* var_list */
    }

  if (rc != PBSE_NONE)
    {}
  else if ((rc = diswui(chan, ct)))
    {}
  else if ((rc = encode_DIS_attropl_hash_single(chan, job_attr, 0)))
    {}
  else if ((rc = encode_DIS_attropl_hash_single(chan, res_attr, 1)))
    {}
  else
    {
    len = strlen(ATTR_v) + 1;
    len += strlen(var_list) + 1;
    if ((rc = diswui(chan, len))               ||  /* attr length */
        (rc = diswst(chan, ATTR_v))            ||  /* attr name */
        (rc = diswui(chan, 0))                 ||  /* no resource */
        (rc = diswst(chan, var_list))          ||  /* attr value */
        (rc = diswui(chan, (unsigned int)SET)))    /* attr op type */
      {}
    }
  memmgr_destroy(&var_mm);

  return(rc);
  }  /* END encode_DIS_attropl() */





