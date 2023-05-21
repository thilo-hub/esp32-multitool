#include <lwip/raw.h>
#include <driver/uart.h>
#include <esp_private/wifi.h>
#include <esp_wifi_netif.h>
#include <esp_wifi.h>
#include "wifi_comm.h"
#include "serio.h"
#if CONFIG_WIFI_TUNNEL

static struct raw_pcb *rawIcmp, *rawIgmp, *rawUdp, *rawUdpLite, *rawTcp;


u8_t onRawInputFilter(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
	void *item;

	u8_t o2 =pbuf_get_at(p,2);
	u8_t o3 =pbuf_get_at(p,3);
	if ( o2 == 0 && o3 == 1 ) {
	    return 0;
	}
	if (xRingbufferSendAcquire(wifiToSerial, &item, p->tot_len+4, 0))
	{
		pbuf_copy_partial(p, (uint8_t*)item+4, p->tot_len, 0);
		xRingbufferSendComplete(wifiToSerial, item);
	}
	pbuf_free(p);

	return 1; // stop further processing by lwip
}
u8_t onRawInput(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
	void *item;
	if (xRingbufferSendAcquire(wifiToSerial, &item, p->tot_len+4, 0))
	{
		pbuf_copy_partial(p, (uint8_t*)item+4, p->tot_len, 0);
		xRingbufferSendComplete(wifiToSerial, item);
	}
	pbuf_free(p);

	return 1; // stop further processing by lwip
}

extern wifi_interface_t wirelessInterface;
void onWifiEvent(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	switch (event_id)
	{
		case WIFI_EVENT_STA_START:
			rawIcmp = raw_new(IP_PROTO_ICMP);
			rawIgmp = raw_new(IP_PROTO_IGMP);
			rawUdp = raw_new(IP_PROTO_UDP);
			rawUdpLite = raw_new(IP_PROTO_UDPLITE);
			rawTcp = raw_new(IP_PROTO_TCP);

			raw_set_flags(rawIcmp, RAW_FLAGS_HDRINCL);
			raw_set_flags(rawIgmp, RAW_FLAGS_HDRINCL);
			raw_set_flags(rawUdp, RAW_FLAGS_HDRINCL);
			raw_set_flags(rawUdpLite, RAW_FLAGS_HDRINCL);
			raw_set_flags(rawTcp, RAW_FLAGS_HDRINCL);

			raw_recv(rawIcmp, onRawInput, NULL);
			raw_recv(rawIgmp, onRawInput, NULL);
			raw_recv(rawUdp, onRawInput, NULL);
			raw_recv(rawUdpLite, onRawInput, NULL);
			raw_recv(rawTcp, onRawInputFilter, NULL);
			esp_wifi_connect();
			break;
		case WIFI_EVENT_STA_CONNECTED:
			break;
		case WIFI_EVENT_STA_DISCONNECTED: // connection lost or AP not found during scan
			esp_wifi_connect(); // try to reconnect
			break;
		default:
			break;
	}
}


void wifiTxTask(void *)
{
	size_t len;

	while (true)
	{
		char *buffer = (char*)xRingbufferReceive(serialToWifi, &len, portMAX_DELAY);
		struct pbuf *p = pbuf_alloc(PBUF_IP, len, PBUF_RAM);
		if (p != NULL)
		{
			ip_addr_t dest;

			pbuf_take(p, buffer, len);

			if (IP_HDR_GET_VERSION(p->payload) == 6)
			{
				struct ip6_hdr *ip6hdr = (struct ip6_hdr*)p->payload;
				ip_addr_copy_from_ip6_packed(dest, ip6hdr->dest);
			}
			else
			{
				struct ip_hdr *iphdr = (struct ip_hdr*)p->payload;
				ip_addr_copy_from_ip4(dest, iphdr->dest);
			}

			raw_sendto(rawIcmp, p, &dest);
			pbuf_free(p);
		}
		vRingbufferReturnItem(serialToWifi, buffer);
	}
}
#endif
