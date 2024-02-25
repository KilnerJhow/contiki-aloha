/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Implementation of the ContikiMAC power-saving radio duty cycling
 * protocol \author Adam Dunkels <adam@sics.se> Niclas Finne <nfi@sics.se>
 *         Joakim Eriksson <joakime@sics.se>
 */

#include "net/mac/contikimac/contikimac-for-aloha-rdc.h"

#include <string.h>

#include "contiki-conf.h"
#include "dev/leds.h"
#include "dev/radio.h"
#include "dev/watchdog.h"
#include "lib/random.h"
#include "net/mac/mac-sequence.h"
#include "net/netstack.h"
#include "net/rime/rime.h"
#include "sys/compower.h"
#include "sys/pt.h"
#include "sys/rtimer.h"

/* More aggressive radio sleeping when channel is busy with other traffic */
#ifndef WITH_FAST_SLEEP
#define WITH_FAST_SLEEP 1
#endif

/* Radio returns TX_OK/TX_NOACK after autoack wait */
#ifndef RDC_CONF_HARDWARE_ACK
#define RDC_CONF_HARDWARE_ACK 0
#endif
/* MCU can sleep during radio off */
#ifndef RDC_CONF_MCU_SLEEP
#define RDC_CONF_MCU_SLEEP 0
#endif

/* CYCLE_TIME for channel cca checks, in rtimer ticks. */
#define CYCLE_TIME (RTIMER_ARCH_SECOND / NETSTACK_RDC_CHANNEL_CHECK_RATE)

/* Are we currently receiving a burst? */
static int we_are_receiving_burst = 0;

/* INTER_PACKET_DEADLINE is the maximum time a receiver waits for the
   next packet of a burst when FRAME_PENDING is set. */
#ifdef CONTIKIMAC_CONF_INTER_PACKET_DEADLINE
#define INTER_PACKET_DEADLINE CONTIKIMAC_CONF_INTER_PACKET_DEADLINE
#else
#define INTER_PACKET_DEADLINE CLOCK_SECOND / 32
#endif

/* CCA_CHECK_TIME is the time it takes to perform a CCA check. */
/* Note this may be zero. AVRs have 7612 ticks/sec, but block until cca is done
 */

#define CCA_CHECK_TIME RTIMER_ARCH_SECOND / 8192

#define CCA_SLEEP_TIME RTIMER_ARCH_SECOND / 2000

/* CHECK_TIME is the total time it takes to perform CCA_COUNT_MAX
   CCAs. */
#define CHECK_TIME (CCA_COUNT_MAX * (CCA_CHECK_TIME + CCA_SLEEP_TIME))
// #define CHECK_TIME 1

/* LISTEN_TIME_AFTER_PACKET_DETECTED is the time that we keep checking
   for activity after a potential packet has been detected by a CCA
   check. */
#ifdef CONTIKIMAC_CONF_LISTEN_TIME_AFTER_PACKET_DETECTED
#define LISTEN_TIME_AFTER_PACKET_DETECTED \
  CONTIKIMAC_CONF_LISTEN_TIME_AFTER_PACKET_DETECTED
#else
#define LISTEN_TIME_AFTER_PACKET_DETECTED RTIMER_ARCH_SECOND / 80
#endif

/* MAX_SILENCE_PERIODS is the maximum amount of periods (a period is
   CCA_CHECK_TIME + CCA_SLEEP_TIME) that we allow to be silent before
   we turn of the radio. */
#ifdef CONTIKIMAC_CONF_MAX_SILENCE_PERIODS
#define MAX_SILENCE_PERIODS CONTIKIMAC_CONF_MAX_SILENCE_PERIODS
#else
#define MAX_SILENCE_PERIODS 5
#endif

/* MAX_NONACTIVITY_PERIODS is the maximum number of periods we allow
   the radio to be turned on without any packet being received, when
   WITH_FAST_SLEEP is enabled. */
#ifdef CONTIKIMAC_CONF_MAX_NONACTIVITY_PERIODS
#define MAX_NONACTIVITY_PERIODS CONTIKIMAC_CONF_MAX_NONACTIVITY_PERIODS
#else
#define MAX_NONACTIVITY_PERIODS 10
#endif

/* STROBE_TIME is the maximum amount of time a transmitted packet
   should be repeatedly transmitted as part of a transmission. */
#define STROBE_TIME (CYCLE_TIME + 2 * CHECK_TIME)

/* GUARD_TIME is the time before the expected phase of a neighbor that
   a transmitted should begin transmitting packets. */
#ifdef CONTIKIMAC_CONF_GUARD_TIME
#define GUARD_TIME CONTIKIMAC_CONF_GUARD_TIME
#else
#define GUARD_TIME 10 * CHECK_TIME + CHECK_TIME_TX
#endif

/* INTER_PACKET_INTERVAL is the interval between two successive packet
 * transmissions */
#ifdef CONTIKIMAC_CONF_INTER_PACKET_INTERVAL
#define INTER_PACKET_INTERVAL CONTIKIMAC_CONF_INTER_PACKET_INTERVAL
#else
#define INTER_PACKET_INTERVAL RTIMER_ARCH_SECOND / 2500
#endif

/* ContikiMAC performs periodic channel checks. Each channel check
   consists of two or more CCA checks. CCA_COUNT_MAX is the number of
   CCAs to be done for each periodic channel check. The default is
   two.*/
#ifdef CONTIKIMAC_CONF_CCA_COUNT_MAX
#define CCA_COUNT_MAX (CONTIKIMAC_CONF_CCA_COUNT_MAX)
#else
#define CCA_COUNT_MAX 2
#endif

#define ONE_PERCENT_DUTY_CYCLE 41
#define THREE_PERCENT_DUTY_CYCLE 127
#define FOUR_PERCENT_DUTY_CYCLE 171
#define SEVENTY_PERCENT_DUTY_CYCLE 9557
#define EIGHTY_PERCENT_DUTY_CYCLE 16384
#define NINETY_PERCENT_DUTY_CYCLE 36864

// #define CCA_ACTIVE_TIME 90
#define FIVE_PERCENT_DUTY_CYCLE 216
#define TEN_PERCENT_DUTY_CYCLE 455
#define TWENTY_PERCENT_DUTY_CYCLE 1024
#define THIRTY_PERCENT_DUTY_CYCLE 1755
#define FORTY_PERCENT_DUTY_CYCLE 2731
#define FIFTY_PERCENT_DUTY_CYCLE 4096
#define SIXTY_PERCENT_DUTY_CYCLE 6144

#ifdef ALOHA_RDC_CCA_ACTIVE_TIME
#define CCA_ACTIVE_TIME ALOHA_RDC_CCA_ACTIVE_TIME
#else
#define CCA_ACTIVE_TIME TEN_PERCENT_DUTY_CYCLE
#endif

#define RADIO_ALWAYS_ON 0

/* AFTER_ACK_DETECTED_WAIT_TIME is the time to wait after a potential
   ACK packet has been detected until we can read it out from the
   radio. */
#ifdef CONTIKIMAC_CONF_AFTER_ACK_DETECTED_WAIT_TIME
#define AFTER_ACK_DETECTED_WAIT_TIME \
  CONTIKIMAC_CONF_AFTER_ACK_DETECTED_WAIT_TIME
#else
#define AFTER_ACK_DETECTED_WAIT_TIME RTIMER_ARCH_SECOND / 1500
#endif

/* MAX_PHASE_STROBE_TIME is the time that we transmit repeated packets
   to a neighbor for which we have a phase lock. */
#ifdef CONTIKIMAC_CONF_MAX_PHASE_STROBE_TIME
#define MAX_PHASE_STROBE_TIME CONTIKIMAC_CONF_MAX_PHASE_STROBE_TIME
#else
#define MAX_PHASE_STROBE_TIME RTIMER_ARCH_SECOND / 60
#endif

#ifdef CONTIKIMAC_CONF_SEND_SW_ACK
#define CONTIKIMAC_SEND_SW_ACK CONTIKIMAC_CONF_SEND_SW_ACK
#else
#define CONTIKIMAC_SEND_SW_ACK 0
#endif

#define ACK_LEN 3

#include <stdio.h>
static struct rtimer rt;
static struct pt pt;

static volatile uint8_t contikimac_is_on = 0;
static volatile uint8_t contikimac_keep_radio_on = 0;

static volatile unsigned char we_are_sending = 0;
static volatile unsigned char radio_is_on = 0;

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINTDEBUG(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#define PRINTDEBUG(...)
#endif

#define DEFAULT_STREAM_TIME (4 * CYCLE_TIME)

#if CONTIKIMAC_CONF_BROADCAST_RATE_LIMIT
static struct timer broadcast_rate_timer;
static int broadcast_rate_counter;
#endif /* CONTIKIMAC_CONF_BROADCAST_RATE_LIMIT */

/*---------------------------------------------------------------------------*/
static void on(void) {
  if (contikimac_is_on && radio_is_on == 0) {
    radio_is_on = 1;
    NETSTACK_RADIO.on();
  }
}
/*---------------------------------------------------------------------------*/
static void off(void) {
  if (contikimac_is_on && radio_is_on != 0 && contikimac_keep_radio_on == 0) {
    radio_is_on = 0;
    NETSTACK_RADIO.off();
  }
}
/*---------------------------------------------------------------------------*/
static void powercycle_wrapper(struct rtimer *t, void *ptr);
static char powercycle(struct rtimer *t, void *ptr);
/*---------------------------------------------------------------------------*/
static volatile rtimer_clock_t cycle_start;
/*---------------------------------------------------------------------------*/
static void schedule_powercycle(struct rtimer *t, rtimer_clock_t time) {
  int r;
  rtimer_clock_t now;

  if (contikimac_is_on) {
    time += RTIMER_TIME(t);
    now = RTIMER_NOW();
    if (RTIMER_CLOCK_LT(time, now + RTIMER_GUARD_TIME)) {
      time = now + RTIMER_GUARD_TIME;
    }

    r = rtimer_set(t, time, 1, powercycle_wrapper, NULL);

    if (r != RTIMER_OK) {
      PRINTF("schedule_powercycle: could not set rtimer\n");
    }
  }
}
/*---------------------------------------------------------------------------*/
static void schedule_powercycle_fixed(struct rtimer *t,
                                      rtimer_clock_t fixed_time) {
  int r;
  rtimer_clock_t now;

  if (contikimac_is_on) {
    now = RTIMER_NOW();
    if (RTIMER_CLOCK_LT(fixed_time, now + RTIMER_GUARD_TIME)) {
      fixed_time = now + RTIMER_GUARD_TIME;
    }

    r = rtimer_set(t, fixed_time, 1, powercycle_wrapper, NULL);
    if (r != RTIMER_OK) {
      PRINTF("schedule_powercycle: could not set rtimer\n");
    }
  }
}
/*---------------------------------------------------------------------------*/
static void powercycle_turn_radio_off(void) {
#if CONTIKIMAC_CONF_COMPOWER
  uint8_t was_on = radio_is_on;
#endif /* CONTIKIMAC_CONF_COMPOWER */

  if (we_are_sending == 0 && we_are_receiving_burst == 0) {
    off();
#if CONTIKIMAC_CONF_COMPOWER
    if (was_on && !radio_is_on) {
      compower_accumulate(&compower_idle_activity);
    }
#endif /* CONTIKIMAC_CONF_COMPOWER */
  }
}
/*---------------------------------------------------------------------------*/
static void powercycle_turn_radio_on(void) {
  if (we_are_sending == 0 && we_are_receiving_burst == 0) {
    on();
  }
}
/*---------------------------------------------------------------------------*/
static void powercycle_wrapper(struct rtimer *t, void *ptr) {
  powercycle(t, ptr);
}
/*---------------------------------------------------------------------------*/
static void advance_cycle_start(void) {
  cycle_start = cycle_start + CYCLE_TIME + CCA_ACTIVE_TIME;
}
/*---------------------------------------------------------------------------*/
static char powercycle(struct rtimer *t, void *ptr) {
  PT_BEGIN(&pt);

  cycle_start = RTIMER_NOW();

  while (1) {
    static uint8_t packet_seen;
    static rtimer_clock_t start;

    packet_seen = 0;

    if (we_are_sending == 0 && we_are_receiving_burst == 0) {
      powercycle_turn_radio_on();
      /* Check if a packet is seen in the air. If so, we keep the
           radio on for a while (LISTEN_TIME_AFTER_PACKET_DETECTED) to
           be able to receive the packet. We also continuously check
           the radio medium to make sure that we wasn't woken up by a
           false positive: a spurious radio interference that was not
           caused by an incoming packet. */
      start = RTIMER_NOW();
      while (RTIMER_CLOCK_LT(RTIMER_NOW(), (start + CCA_ACTIVE_TIME))) {
        if (NETSTACK_RADIO.channel_clear() == 0) {
          packet_seen = 1;
          break;
        }
      }
    }

    if (!packet_seen) {
      powercycle_turn_radio_off();
    }

    if (packet_seen) {
      static rtimer_clock_t start;
      static uint8_t silence_periods, periods;
      start = RTIMER_NOW();

      periods = silence_periods = 0;
      while (we_are_sending == 0 && radio_is_on &&
             RTIMER_CLOCK_LT(RTIMER_NOW(),
                             (start + LISTEN_TIME_AFTER_PACKET_DETECTED))) {
        /* Check for a number of consecutive periods of
             non-activity. If we see two such periods, we turn the
             radio off. Also, if a packet has been successfully
             received (as indicated by the
             NETSTACK_RADIO.pending_packet() function), we stop
             snooping. */
        /* A cca cycle will disrupt rx on some radios, e.g. mc1322x, rf230 */
        if (NETSTACK_RADIO.channel_clear()) {
          ++silence_periods;
        } else {
          silence_periods = 0;
        }

        ++periods;

        if (NETSTACK_RADIO.receiving_packet()) {
          silence_periods = 0;
        }
        if (silence_periods > MAX_SILENCE_PERIODS) {
          powercycle_turn_radio_off();
          break;
        }
        if (WITH_FAST_SLEEP && periods > MAX_NONACTIVITY_PERIODS &&
            !(NETSTACK_RADIO.receiving_packet() ||
              NETSTACK_RADIO.pending_packet())) {
          powercycle_turn_radio_off();
          break;
        }
        if (NETSTACK_RADIO.pending_packet()) {
          break;
        }

        schedule_powercycle(t, CCA_CHECK_TIME + CCA_SLEEP_TIME);
        PT_YIELD(&pt);
      }

      if (radio_is_on) {
        if (!(NETSTACK_RADIO.receiving_packet() ||
              NETSTACK_RADIO.pending_packet()) ||
            !RTIMER_CLOCK_LT(RTIMER_NOW(),
                             (start + LISTEN_TIME_AFTER_PACKET_DETECTED))) {
          powercycle_turn_radio_off();
        }
      }
    }

    advance_cycle_start();

    if (RTIMER_CLOCK_LT(RTIMER_NOW(), cycle_start)) {
      /* Schedule the next powercycle interrupt, or sleep the mcu
      until then.  Sleeping will not exit from this interrupt, so
      ensure an occasional wake cycle or foreground processing will
      be blocked until a packet is detected */
      schedule_powercycle_fixed(t, cycle_start);
      PT_YIELD(&pt);
    }
  }

  PT_END(&pt);
}
/*---------------------------------------------------------------------------*/
static int broadcast_rate_drop(void) {
#if CONTIKIMAC_CONF_BROADCAST_RATE_LIMIT
  if (!timer_expired(&broadcast_rate_timer)) {
    broadcast_rate_counter++;
    if (broadcast_rate_counter < CONTIKIMAC_CONF_BROADCAST_RATE_LIMIT) {
      return 0;
    } else {
      return 1;
    }
  } else {
    timer_set(&broadcast_rate_timer, CLOCK_SECOND);
    broadcast_rate_counter = 0;
    return 0;
  }
#else  /* CONTIKIMAC_CONF_BROADCAST_RATE_LIMIT */
  return 0;
#endif /* CONTIKIMAC_CONF_BROADCAST_RATE_LIMIT */
}
/*---------------------------------------------------------------------------*/
static int send_packet(mac_callback_t mac_callback, void *mac_callback_ptr,
                       struct rdc_buf_list *buf_list) {
  rtimer_clock_t t0;
  uint8_t got_strobe_ack = 0;
  uint8_t is_broadcast = 0;
  // uint8_t is_known_receiver = 0;
  int transmit_len;
  int ret;
  uint8_t contikimac_was_on;
  int len;
  uint8_t seqno;

  /* Exit if RDC and radio were explicitly turned off */
  if (!contikimac_is_on && !contikimac_keep_radio_on) {
    PRINTF("contikimac-aloha: radio is turned off\n");
    return MAC_TX_ERR_FATAL;
  }

  if (packetbuf_totlen() == 0) {
    PRINTF("contikimac-aloha: send_packet data len 0\n");
    return MAC_TX_ERR_FATAL;
  }

#if !NETSTACK_CONF_BRIDGE_MODE
  /* If NETSTACK_CONF_BRIDGE_MODE is set, assume PACKETBUF_ADDR_SENDER is
   * already set. */
  packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
#endif
  if (packetbuf_holds_broadcast()) {
    is_broadcast = 1;
    PRINTDEBUG("contikimac-aloha: send broadcast\n");

    if (broadcast_rate_drop()) {
      return MAC_TX_COLLISION;
    }
  } else {
#if NETSTACK_CONF_WITH_IPV6
    PRINTDEBUG(
        "contikimac-aloha: send unicast to "
        "%02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
        packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
        packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1],
        packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[2],
        packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[3],
        packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[4],
        packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[5],
        packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[6],
        packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[7]);
#else  /* NETSTACK_CONF_WITH_IPV6 */
    PRINTDEBUG("contikimac-aloha: send unicast to %u.%u\n",
               packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
               packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1]);
#endif /* NETSTACK_CONF_WITH_IPV6 */
  }

  if (!packetbuf_attr(PACKETBUF_ATTR_IS_CREATED_AND_SECURED)) {
    packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK, 1);
    if (NETSTACK_FRAMER.create() < 0) {
      PRINTF("contikimac-aloha: framer failed\n");
      return MAC_TX_ERR_FATAL;
    }
  }

  transmit_len = packetbuf_totlen();
  NETSTACK_RADIO.prepare(packetbuf_hdrptr(), transmit_len);

  /* By setting we_are_sending to one, we ensure that the rtimer
     powercycle interrupt do not interfere with us sending the packet. */
  we_are_sending = 1;

  /* If we have a pending packet in the radio, we should not send now,
     because we will trash the received packet. Instead, we signal
     that we have a collision, which lets the packet be received. This
     packet will be retransmitted later by the MAC protocol
     instread. */
  if (NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet()) {
    we_are_sending = 0;
    PRINTF("contikimac-aloha: collision receiving %d, pending %d\n",
           NETSTACK_RADIO.receiving_packet(), NETSTACK_RADIO.pending_packet());
    return MAC_TX_NOACK;
  }

  /* Switch off the radio to ensure that we didn't start sending while
     the radio was doing a channel check. */
  off();

  got_strobe_ack = 0;

  /* Set contikimac_is_on to one to allow the on() and off() functions
     to control the radio. We restore the old value of
     contikimac_is_on when we are done. */
  contikimac_was_on = contikimac_is_on;
  contikimac_is_on = 1;

  if (!is_broadcast) {
    /* Turn radio on to receive expected unicast ack.  Not necessary
       with hardware ack detection, and may trigger an unnecessary cca
       or rx cycle */
    on();
  }
  seqno = packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO);

  watchdog_periodic();
  if (is_broadcast) {
    t0 = RTIMER_NOW();
    while (RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + STROBE_TIME)) {
      watchdog_periodic();

      rtimer_clock_t wt;

      NETSTACK_RADIO.transmit(transmit_len);
      wt = RTIMER_NOW();
      while (RTIMER_CLOCK_LT(RTIMER_NOW(), wt + INTER_PACKET_INTERVAL)) {
      }
    }
  } else {
    rtimer_clock_t wt;

    NETSTACK_RADIO.transmit(transmit_len);
    wt = RTIMER_NOW();
    while (RTIMER_CLOCK_LT(RTIMER_NOW(), wt + INTER_PACKET_INTERVAL)) {
    }

    if (NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet()) {
      uint8_t ackbuf[ACK_LEN];
      wt = RTIMER_NOW();
      while (RTIMER_CLOCK_LT(RTIMER_NOW(), wt + AFTER_ACK_DETECTED_WAIT_TIME)) {
      }

      len = NETSTACK_RADIO.read(ackbuf, ACK_LEN);
      if (len == ACK_LEN && seqno == ackbuf[ACK_LEN - 1]) {
        got_strobe_ack = 1;
      }
    }
  }

  off();

  contikimac_is_on = contikimac_was_on;
  we_are_sending = 0;

  /* Determine the return value that we will return from the
     function. We must pass this value to the phase module before we
     return from the function.  */
  if (!is_broadcast && !got_strobe_ack) {
    ret = MAC_TX_NOACK;
  } else {
    ret = MAC_TX_OK;
  }
  return ret;
}
/*---------------------------------------------------------------------------*/
static void qsend_packet(mac_callback_t sent, void *ptr) {
  int ret = send_packet(sent, ptr, NULL);
  if (ret != MAC_TX_DEFERRED) {
    mac_call_sent_callback(sent, ptr, ret, 1);
  }
}
/*---------------------------------------------------------------------------*/
static void qsend_list(mac_callback_t sent, void *ptr,
                       struct rdc_buf_list *buf_list) {
  struct rdc_buf_list *curr;
  struct rdc_buf_list *next;
  int ret;
  int pending;

  if (buf_list == NULL) {
    return;
  }
  /* Do not send during reception of a burst */
  if (we_are_receiving_burst) {
    /* Prepare the packetbuf for callback */
    queuebuf_to_packetbuf(buf_list->buf);
    /* Return COLLISION so the MAC may try again later */
    mac_call_sent_callback(sent, ptr, MAC_TX_COLLISION, 1);
    return;
  }

  /* Create and secure frames in advance */
  curr = buf_list;
  do {
    next = list_item_next(curr);
    queuebuf_to_packetbuf(curr->buf);
    if (!packetbuf_attr(PACKETBUF_ATTR_IS_CREATED_AND_SECURED)) {
      /* create and secure this frame */
      if (next != NULL) {
        packetbuf_set_attr(PACKETBUF_ATTR_PENDING, 1);
      }
#if !NETSTACK_CONF_BRIDGE_MODE
      /* If NETSTACK_CONF_BRIDGE_MODE is set, assume PACKETBUF_ADDR_SENDER is
       * already set. */
      packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &linkaddr_node_addr);
#endif
      packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK, 1);
      if (NETSTACK_FRAMER.create() < 0) {
        PRINTF("contikimac-aloha: framer failed\n");
        mac_call_sent_callback(sent, ptr, MAC_TX_ERR_FATAL, 1);
        return;
      }

      packetbuf_set_attr(PACKETBUF_ATTR_IS_CREATED_AND_SECURED, 1);
      queuebuf_update_from_packetbuf(curr->buf);
    }
    curr = next;
  } while (next != NULL);

  curr = buf_list;
  do { /* A loop sending a burst of packets from buf_list */
    next = list_item_next(curr);

    /* Prepare the packetbuf */
    queuebuf_to_packetbuf(curr->buf);

    pending = packetbuf_attr(PACKETBUF_ATTR_PENDING);

    /* Send the current packet */
    ret = send_packet(sent, ptr, curr);
    if (ret != MAC_TX_DEFERRED) {
      mac_call_sent_callback(sent, ptr, ret, 1);
    }

    if (ret == MAC_TX_OK) {
      if (next != NULL) {
        /* We're in a burst, no need to wake the receiver up again */
        curr = next;
      }
    } else {
      /* The transmission failed, we stop the burst */
      next = NULL;
    }
  } while ((next != NULL) && pending);
}
/*---------------------------------------------------------------------------*/
/* Timer callback triggered when receiving a burst, after having
   waited for a next packet for a too long time. Turns the radio off
   and leaves burst reception mode */
static void recv_burst_off(void *ptr) {
  off();
  we_are_receiving_burst = 0;
}
/*---------------------------------------------------------------------------*/
static void input_packet(void) {
  static struct ctimer ct;
  int duplicate = 0;

#if CONTIKIMAC_SEND_SW_ACK
  int original_datalen;
  uint8_t *original_dataptr;

  original_datalen = packetbuf_datalen();
  original_dataptr = packetbuf_dataptr();
#endif

  if (!we_are_receiving_burst) {
    off();
  }

  if (packetbuf_datalen() == ACK_LEN) {
    /* Ignore ack packets */
    PRINTF("contikimac-aloha: ignored ack\n");
    return;
  }

  if (packetbuf_totlen() > 0 && NETSTACK_FRAMER.parse() >= 0) {
    if (packetbuf_datalen() > 0 && packetbuf_totlen() > 0 &&
        (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                      &linkaddr_node_addr) ||
         packetbuf_holds_broadcast())) {
      /* This is a regular packet that is destined to us or to the
         broadcast address. */

      /* If FRAME_PENDING is set, we are receiving a packets in a burst */
      we_are_receiving_burst = packetbuf_attr(PACKETBUF_ATTR_PENDING);
      if (we_are_receiving_burst) {
        on();
        /* Set a timer to turn the radio off in case we do not receive
           a next packet */
        ctimer_set(&ct, INTER_PACKET_DEADLINE, recv_burst_off, NULL);
      } else {
        off();
        ctimer_stop(&ct);
      }

      PRINTDEBUG("contikimac-aloha: data (%u)\n", packetbuf_datalen());

      if (!duplicate) {
        NETSTACK_MAC.input();
      }
      return;
    } else {
      PRINTDEBUG("contikimac-aloha: data not for us\n");
    }
  } else {
    PRINTF("contikimac-aloha: failed to parse (%u)\n", packetbuf_totlen());
  }
}
/*---------------------------------------------------------------------------*/
static void init(void) {
  radio_is_on = 0;
  PT_INIT(&pt);
  contikimac_is_on = 1;

  rtimer_set(&rt, RTIMER_NOW() + CYCLE_TIME, 1, powercycle_wrapper, NULL);

  printf("CCA_ACTIVE_TIME: %d\n", CCA_ACTIVE_TIME);
  printf("RTIMER_ARCH_SECOND: %u\n", RTIMER_ARCH_SECOND);
  printf("CLOCK_SECOND: %lu\n", CLOCK_SECOND);
  printf("CYCLE_TIME: %u\n", CYCLE_TIME);
  printf("CCA_CHECK_TIME: %u\n", CCA_CHECK_TIME);
  printf("CCA_SLEEP_TIME: %u\n", CCA_SLEEP_TIME);
  printf("CHECK_TIME: %u\n", CHECK_TIME);
}
/*---------------------------------------------------------------------------*/
static int turn_on(void) {
  if (contikimac_is_on == 0) {
    contikimac_is_on = 1;
    contikimac_keep_radio_on = 0;
    rtimer_set(&rt, RTIMER_NOW() + CYCLE_TIME, 1, powercycle_wrapper, NULL);
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
static int turn_off(int keep_radio_on) {
  contikimac_is_on = 0;
  contikimac_keep_radio_on = keep_radio_on;
  if (keep_radio_on) {
    radio_is_on = 1;
    return NETSTACK_RADIO.on();
  } else {
    radio_is_on = 0;
    return NETSTACK_RADIO.off();
  }
}
/*---------------------------------------------------------------------------*/
static unsigned short duty_cycle(void) {
  return (1ul * CLOCK_SECOND * CYCLE_TIME) / RTIMER_ARCH_SECOND;
}
/*---------------------------------------------------------------------------*/
const struct rdc_driver contikimac_aloha_driver_rdc = {
    "ContikiAlohaMac", init,    qsend_packet, qsend_list,
    input_packet,      turn_on, turn_off,     duty_cycle,
};
/*---------------------------------------------------------------------------*/
uint16_t contikimac_debug_print(void) { return 0; }
/*---------------------------------------------------------------------------*/
