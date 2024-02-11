#include <stdio.h>

#include "contiki.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "lib/random.h"
#include "net/netstack.h"
#include "net/rime/collect.h"
#include "net/rime/rime.h"

static struct collect_conn tc;

/*---------------------------------------------------------------------------*/
PROCESS(hello_world_process2, "Hello world process 2");
AUTOSTART_PROCESSES(&hello_world_process2);
/*---------------------------------------------------------------------------*/
void callback(int delay) { printf("Callback for delay: %d\n", delay); }
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(hello_world_process2, ev, data) {
  static struct etimer periodic;
  static struct etimer et;
  static struct ctimer n;

  PROCESS_BEGIN();

  /* Allow some time for the network to settle. */

  clock_time_t delay = 1 * CLOCK_SECOND;
  ctimer_set(&n, delay, callback, delay);
  printf("Delay %u\n", delay);
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
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
