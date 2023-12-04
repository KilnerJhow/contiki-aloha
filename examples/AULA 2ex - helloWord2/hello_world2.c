#include "contiki.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "lib/random.h"
#include "net/rime/collect.h"
#include "net/rime/rime.h"

#include "net/netstack.h"

#include <stdio.h>

static struct collect_conn tc;

/*---------------------------------------------------------------------------*/
PROCESS(hello_world_process2, "Hello world process 2");
AUTOSTART_PROCESSES(&hello_world_process2);
/*---------------------------------------------------------------------------*/
static void recv(const linkaddr_t *originator, uint8_t seqno, uint8_t hops) {
  if (hops > 0) {
    printf("%d recebe '%s' de  %d (HOPS %d)\n", linkaddr_node_addr.u8[0],
           (char *)packetbuf_dataptr(), originator->u8[0], hops);
  }
}
/*---------------------------------------------------------------------------*/
static const struct collect_callbacks callbacks = {recv};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(hello_world_process2, ev, data) {
  static struct etimer periodic;
  static struct etimer et;

  PROCESS_BEGIN();

  collect_open(&tc, 130, COLLECT_ROUTER, &callbacks);

  if (linkaddr_node_addr.u8[0] == 1 && linkaddr_node_addr.u8[1] == 0) {
    printf("I am sink\n");
    collect_set_sink(&tc, 1);
  }

  /* Allow some time for the network to settle. */
  etimer_set(&et, 120 * CLOCK_SECOND);
  PROCESS_WAIT_UNTIL(etimer_expired(&et));

  while (1) {

    /* Send a packet every 30 seconds. */
    etimer_set(&periodic, CLOCK_SECOND * 30);
    etimer_set(&et, random_rand() % (CLOCK_SECOND * 30));

    PROCESS_WAIT_UNTIL(etimer_expired(&et));

    {
      static linkaddr_t oldparent;
      const linkaddr_t *parent;

      printf("Sending\n");
      packetbuf_clear();
      packetbuf_set_datalen(sprintf(packetbuf_dataptr(), "%s", "Hello") + 1);
      collect_send(&tc, 15);

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
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
