#include "contiki.h"
#include "net/"
#include "net/linkaddr.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/rime/runicast.h"
#include <stdio.h>

#define DESTINO 3

PROCESS(aloha_process, "Aloha unicast");
AUTOSTART_PROCESSES(&aloha_process);

static void recv_uc(struct unicast_conn *c, const linkaddr_t *from) {
  printf("%d recebe '%s' de  %d\n", DESTINO, (char *)packetbuf_dataptr(),
         from->u8[0]);
}

static const struct unicast_callbacks unicast_callbacks = {recv_uc};
static struct unicast_conn uc;

PROCESS_THREAD(aloha_process, ev, data) {
  PROCESS_EXITHANDLER(unicast_close(&uc);)

  PROCESS_BEGIN();

  unicast_open(&uc, 146, &unicast_callbacks);

  while (1) {
    static struct etimer et;
    linkaddr_t addr;

    etimer_set(&et, CLOCK_SECOND);

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    packetbuf_copyfrom("Hello", 5);
    addr.u8[0] = DESTINO;
    addr.u8[1] = 0;
    if (!linkaddr_cmp(&addr, &linkaddr_node_addr)) {
      if (channel_is_free()) {
        unicast_send(&uc, &addr);
      } else {
        random_wait();
      }
    }
  }

  PROCESS_END();
}