/*---------------------------------------------------------------------------*/
/*-------------------------------LIBS----------------------------------------*/
/*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "lib/random.h"

// transmission
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "net/netstack.h"
#include "net/rime/collect.h"
#include "net/rime/rime.h"

// battery
#include "dev/battery-sensor.h"
#include "powertrace.h"

// VARIAVEIS DO TMOTE SKY
#define _Nb 90  // tamanho do pacote
static uint32_t _Eihop, _P0;
static uint8_t _Dist = 245;
static uint8_t _R = 250;  // TMOTE SKY

// VARIAVEIS DO TMOTE SKY (mA)
static double voltage = 3.6;  // Volts
static double tx = 0.0195 * 1000;
static double rx = 0.0218 * 1000;
static double cpu = 0.0000545 * 1000;
static double cpu_stand = 0.0000051 * 1000;

// DADOS DE TRABNSMISSAO
static struct collect_conn tc;

/*---------------------------------------------------------------------------*/
/*-------------------------------CODE----------------------------------------*/
/*---------------------------battery status-----------------------------------*/
void powertrace_print(char *str) {
  static uint32_t last_cpu, last_lpm, last_transmit, last_listen;
  uint32_t current_cpu, current_idle, current_tx_mode, current_rx_mode;
  uint32_t all_cpu, all_lpm, all_transmit, all_listen;

  uint32_t current, charge, power, energy;

  all_cpu = energest_type_time(ENERGEST_TYPE_CPU);
  all_lpm = energest_type_time(ENERGEST_TYPE_LPM);
  all_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  all_listen = energest_type_time(ENERGEST_TYPE_LISTEN);

  current_cpu = all_cpu - last_cpu;
  current_idle = all_lpm - last_lpm;
  current_tx_mode = all_transmit - last_transmit;
  current_rx_mode = all_listen - last_listen;

  energest_flush();

  // INSERIR NO INICIO DA TRANSMISSAO
  last_cpu = energest_type_time(ENERGEST_TYPE_CPU);
  last_lpm = energest_type_time(ENERGEST_TYPE_LPM);
  last_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  last_listen = energest_type_time(ENERGEST_TYPE_LISTEN);

  current = (tx * current_tx_mode + rx * current_rx_mode + cpu * current_cpu +
             cpu_stand * current_idle) /
            RTIMER_ARCH_SECOND;

  charge = current * (current_cpu + current_idle) / RTIMER_ARCH_SECOND;
  power = current * voltage;
  energy = charge * voltage;

  // printf("power: %u.%02u mW\n",(uint16_t)power/1000,(uint16_t)power%1000);
  // printf("consumption: %u.%02u
  // mJ\n\n",(uint16_t)energy/1000,(uint16_t)energy%1000);

  _Eihop = energy;
  _P0 = power;
}

/*---------------------------transmission------------------------------------*/
/*---------------------------------------------------------------------------*/
PROCESS(example_collect_process, "Test collect process");
AUTOSTART_PROCESSES(&example_collect_process);
/*---------------------------------------------------------------------------*/
static void recv(const linkaddr_t *originator, uint8_t seqno, uint8_t hops) {
  if (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &linkaddr_null)) {
    // Este pacote é um broadcast
    // printf("Broadcast from %d.%d, seqno %d, hops %d: len %d '%s'\n",
    //        originator->u8[0], originator->u8[1], seqno, hops,
    //        packetbuf_datalen(), (char *)packetbuf_dataptr());
    return;
  }

  printf("Data packet from %d.%d, seqno %d, hops %d: len %d '%s'\n",
         originator->u8[0], originator->u8[1], seqno, hops, packetbuf_datalen(),
         (char *)packetbuf_dataptr());

  // if ((hops > 0) && (strncmp(packetbuf_dataptr(), "0.00,0.00", 8) > 0)) {
  //   uint8_t d = _Dist / hops;

  //   /* Eihop: Consumo Energetico Por i saltos
  //      P0: potência de transmissão
  //      i: numero de saltos de uma transmissão
  //      d: distância entre os nós
  //      R: taxa de Transmissão
  //      Nb: tamanho do Pacote
  //   */
  //   // Eihop,P0, i,d,R,Nb
  //   // printf("dataptr: %s, hops: %d, d: %u, _R: %u, _Nb: %u \n",
  //   //        (char *)packetbuf_dataptr(), hops, d, _R, _Nb);
  //   printf("%s,%d,%u,%u,%u \n", (char *)packetbuf_dataptr(), hops, d, _R,
  //   _Nb);
  // }
}

/*---------------------------------------------------------------------------*/
/*----------------------------MOTE OPERATION---------------------------------*/
/*---------------------------------------------------------------------------*/
static const struct collect_callbacks callbacks = {recv};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_collect_process, ev, data) {
  static struct etimer periodic;
  static struct etimer et;

  // CRIACAO DO PACOTE
  powertrace_print("");
  // char *packet = malloc(_Nb);
  char packet[_Nb];
  // "abcedefghijklmnopqrstuvwxyzabcedefghijklmnopqrstuvwxyz";
  sprintf(packet, "%u.%02u,%u.%02u", (uint16_t)_Eihop / 1000,
          (uint16_t)_Eihop % 1000, (uint16_t)_P0 / 1000, (uint16_t)_P0 % 1000);

  PROCESS_BEGIN();

  collect_open(&tc, 130, COLLECT_ROUTER, &callbacks);

  if (linkaddr_node_addr.u8[0] == 1 && linkaddr_node_addr.u8[1] == 0) {
    // printf("I am sink\n");
    collect_set_sink(&tc, 1);
  }

  // Aguarde algum tempo para que a rede se estabilize.
  etimer_set(&et, 120 * CLOCK_SECOND);
  PROCESS_WAIT_UNTIL(etimer_expired(&et));
  printf("Starting to sense\n");

  while (1) {
    // Envio de pacote a cada 30 segundos.
    // printf("Starting to sense\n");
    etimer_set(&periodic, CLOCK_SECOND * 5);
    // printf("periodic timer set");
    etimer_set(&et, random_rand() % (CLOCK_SECOND * 5));
    // printf("et timer set");

    PROCESS_WAIT_UNTIL(etimer_expired(&et));
    {
      static linkaddr_t oldparent;
      const linkaddr_t *parent;

      // printf("Sending\n");
      // packetbuf_set_datalen(_Nb);
      packetbuf_clear();
      // printf("Sending %s\n", packet);
      packetbuf_set_datalen(sprintf(packetbuf_dataptr(), "%s", packet) + 1);
      // printf("Sending %s\n", (char *)packetbuf_dataptr());
      // printf("Length %d\n", packetbuf_datalen());

      energest_flush();
      collect_send(&tc, 15);
      printf("Sending %s\n", (char *)packetbuf_dataptr());

      parent = collect_parent(&tc);

      if (!linkaddr_cmp(parent, &oldparent)) {
        if (!linkaddr_cmp(&oldparent, &linkaddr_null)) {
          printf("#L %d 0\n", oldparent.u8[0]);
        }
        if (!linkaddr_cmp(parent, &linkaddr_null)) {
          printf("#L %d 1\n", parent->u8[0]);
        }
        linkaddr_copy(&oldparent, parent);
      }
    }

    PROCESS_WAIT_UNTIL(etimer_expired(&periodic));
    // printf("Periodic\n");
  }
  // free(packet);

  printf("Process end\n");
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
