#include <esp_event.h>

void		wifiTunTxTask(void *);
void		onWifiEvent(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

void		registerWifitun(void);
void		registerWifi(void);
void		registerHttpdServer(void);
