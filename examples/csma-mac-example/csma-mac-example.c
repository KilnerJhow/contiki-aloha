#include "contiki.h"
#include "net/netstack.h"
#include "net/packetbuf.h"

#define SEND_INTERVAL (CLOCK_SECOND * 5)

PROCESS(unicast_sender_process, "CSMA Unicast sender");

AUTOSTART_PROCESSES(&unicast_sender_process);

PROCESS_THREAD(unicast_sender_process, ev, data) {
  static struct etimer et;
  linkaddr_t addr;

  PROCESS_BEGIN();

  /* Set the address of the receiver */
  addr.u8[0] = 0x00;
  addr.u8[1] = 0x01;

  while (1) {
    /* Wait for the send interval */
    etimer_set(&et, SEND_INTERVAL);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    /* Prepare the packet */
    packetbuf_clear();
    packetbuf_copyfrom("Hello", 5);

    /* Send the packet */
    NETSTACK_MAC.send(NULL, NULL);
  }

  PROCESS_END();
}