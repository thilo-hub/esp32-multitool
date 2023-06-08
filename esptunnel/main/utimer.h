
void registerTimer(void);
void timerRemoveJobs(char *match);
typedef esp_err_t (*timerJob)(char *cmd);
// Parse descriptor for [+event_time] [+event_interval] [-]filename
// execute job(filename) at parsed times 
// return queue list as string new-line separated
// On any error return NULL
char *queueTimerJob(char *descriptor,timerJob job);
