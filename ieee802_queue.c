/*
 * Copyright (c) 2008
 * Telecooperation Office (TecO), Universitaet Karlsruhe (TH), Germany.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. Neither the name of the Universitaet Karlsruhe (TH) nor the names
 *    of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author(s): Philipp Scholl <scholl@teco.edu>
 */

#include "ieee_mac_sap.h"
#include "ieee_mac_pib.h"
#include "AppApi.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "conf.h"

#ifndef JENNIC_CONF_IEEE802154_RXQ_ITEMS
# define RX_QUEUE_SIZE 4
#else
# define RX_QUEUE_SIZE JENNIC_CONF_IEEE802154_RXQ_ITEMS
#endif

typedef enum { FREE=0x0, MLME, MCPS, MLME_ALLOC, MCPS_ALLOC } types_t;

/* storage for queues */
static MAC_MlmeDcfmInd_s rxbuf[RX_QUEUE_SIZE] __attribute__((aligned(4)));

/* receive queue, queueing handled here */
static struct {
  volatile uint8_t head, tail, types[RX_QUEUE_SIZE];
} rxq;

/* rx queue handling functions */
static MAC_DcfmIndHdr_s*
rxq_peek()
{
  if(rxq.types[rxq.head]==FREE ||
     rxq.types[rxq.head]==MCPS_ALLOC ||
     rxq.types[rxq.head]==MLME_ALLOC)
    return NULL;

  return (MAC_DcfmIndHdr_s*) &rxbuf[rxq.head];
}

static types_t
rxq_peektype()
{
  return rxq.types[rxq.head];
}

static void
rxq_dequeue()
{
  if(rxq.types[rxq.head]==MCPS ||
     rxq.types[rxq.head]==MLME)
  {
    rxq.types[rxq.head] = FREE;
    rxq.head = (rxq.head+1)%RX_QUEUE_SIZE;
  }
}

static MAC_DcfmIndHdr_s*
rxq_enqueue(void *p, types_t type)
{
  MAC_DcfmIndHdr_s *ind = NULL;

  if(rxq.types[rxq.tail]==FREE)
  {
    rxq.types[rxq.tail] = type;
    ind = (MAC_DcfmIndHdr_s*) &rxbuf[rxq.tail];
    rxq.tail = (rxq.tail+1)%RX_QUEUE_SIZE;
  }
//  else
//    leds_toggle(LEDS_ALL);

  return ind;
}

MAC_DcfmIndHdr_s*
rxq_mlme_alloc(void *p)
{
  return rxq_enqueue(p, MLME_ALLOC);
}

MAC_DcfmIndHdr_s*
rxq_mcps_alloc(void *p)
{
  return rxq_enqueue(p, MCPS_ALLOC);
}

static void
rxq_tail_complete()
{
  uint16_t cur;

  if (rxq.tail==0) cur = RX_QUEUE_SIZE-1;
  else             cur = rxq.tail-1;

  switch(rxq.types[cur])
  {
    case MLME_ALLOC:
      rxq.types[cur] = MLME;
      break;
    case MCPS_ALLOC:
      if (((MAC_McpsDcfmInd_s*) &rxbuf[cur])->u8Type==MAC_MCPS_DCFM_DATA)
      {
        rxq.types[cur] = FREE;
        rxq.tail = cur;
      }
      else
        rxq.types[cur] = MCPS;
      break;
    case FREE:
      break;
    default:
      break;
  }
}

static void
rxq_init()
{
  memset(rxbuf, 0x00, sizeof(rxbuf));
  memset(&rxq, 0x00, sizeof(rxq));
}
