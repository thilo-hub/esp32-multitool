#include <esp_event.h>

void wifiTxTask(void *);
void onWifiEvent(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

