#include <freertos/freeRTOS.h>
#include <freertos/message_buffer.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <esp_console.h>
#include <string.h>

#include "utimer.h"

static const char *TAG = "timer";

#define ENABLE_TIMERBOUNDS 1

struct TimerQueue {
    struct TimerQueue *next;
    long event;
    long repeat;
    int  repeat_cnt;
    timerJob job;
    char cmd[];
};
struct TimerQueue *queue = NULL;


// input values are in seconds`
struct TimerQueue *newTimer(long event,long repeat,timerJob job,char *cmd)
{
    size_t cmd_len = strlen(cmd);
    struct TimerQueue *new=malloc(sizeof(struct TimerQueue)+cmd_len+1);
    ESP_LOGI(TAG,"New Jobs %ld *+%ld %s\n",event,repeat,cmd);
    if ( !new) {
	ESP_LOGI(TAG,"Failed allocate %d",sizeof(struct TimerQueue)+cmd_len+1);
	return NULL;
    }
    // seconds -> ticks
    repeat = pdMS_TO_TICKS(1000 * repeat);
    event  = pdMS_TO_TICKS(1000 * event);

    new->job = job;
    new->event = event;
    strcpy(new->cmd,cmd);
    new->repeat_cnt = 0; 

#if ENABLE_TIMERBOUNDS 
    // limit repeat to max once per second
    if ( repeat > 0) {
	if (repeat < pdMS_TO_TICKS(1000) ) 
	    repeat = pdMS_TO_TICKS(1000);
	// make sure there is a limit of 24  events per day
	// if the repeat time is less than 1h, limit to 24 repeats
	if (repeat < pdMS_TO_TICKS(3600 * 1000))
	    new->repeat_cnt = 24;
    }
#endif
    new->repeat=repeat;
    return new;
}
    
void addTimer(struct TimerQueue *new)
{
    struct TimerQueue **p=&queue;
    while( *p && (*p)->event < new->event) {
	p=&((*p)->next);
    }
    new->next = *p;
    *p=new;
    if ( p == &queue ) {
	// Start timer
    }
}
struct TimerQueue *removeTimer(struct TimerQueue **pp, int tn)
{
    while( tn-- > 0 && *pp )
	pp = &(*pp)->next;
    struct TimerQueue *item = *pp;
    if ( item ) {
	*pp  = item->next;
	item -> next = NULL;
    }
    return item;
}

void timerRemoveJobs(char *match)
{
    struct TimerQueue **pp = &queue;;
    while(*pp) {
	if ( strncmp(match,(*pp)->cmd ,strlen(match)) == 0 ){
	    struct TimerQueue *next = (*pp)->next;
	    free(*pp);
	    *pp  = next;
	} else {
	    pp= &(*pp)->next;
	}
    }
}


esp_err_t send_file(char *file) ;

void expireTimer(void)
{
    struct TimerQueue *item = removeTimer(&queue,0);

    if ( !item )
	return;

    // Timer job...
    printf("Fire:  %s (+ %ld)\n",item->cmd,item->repeat);
    if ( ((*item->job)(item->cmd)) != ESP_OK ){
	// no point in repeating something that failed...
	item->repeat = 0;
    }
    if ( ! item->repeat) {
	free(item);
	return;
    }
    item->event += item->repeat;
    if (item->repeat_cnt > 0 && item->repeat_cnt-- == 1 ) {
	item->repeat = 0;
    }

    // Link new item back into queue
    addTimer(item);
}
static char *getTimerList(void)
{
    struct TimerQueue *p;
    int n=0;
    char *rv=malloc(4096);
    *rv=0;

    TickType_t now = xTaskGetTickCount();
    // printf("\n======= %ld ========\n",now);
    for(p=queue ; p ; p=p->next){
	char *b=NULL;
	asprintf(&b,"%dE: %ld +%ld :%s\n",n++,p->event-now,p->repeat,p->cmd);
	if (b) {
	    rv=reallocf(rv,strlen(rv)+strlen(b)+1);
	    strcat(rv,b);
	    free(b);
	}
    }
    return rv;
}
static void dmpList(void)
{
    printf("\n===============\n");
    char *gl=getTimerList();
    if ( gl ) {
	printf("%s",gl);
	free(gl);
    }
}
#if 0

main()
{
   dmpList();
   addTimer(100,401,"Cmd1");
   addTimer(300,0,"Cmd2");
   addTimer(200,153,"Cmd3");
   dmpList();
   expireTimer();
   dmpList();
   expireTimer();
   dmpList();
   expireTimer();
   dmpList();
   expireTimer();
   dmpList();
}
#endif

MessageBufferHandle_t timerJobMB=NULL;

static void 
timerTask(void *)
{
    TickType_t delay = portMAX_DELAY;
    timerJobMB = xMessageBufferCreate( 5*sizeof(struct TimerQueue) );
    while(1) 
    {
	struct TimerQueue *new;
	size_t mlen = xMessageBufferReceive( timerJobMB,
				  &new,
				  sizeof(new),
				  delay );
	TickType_t now = xTaskGetTickCount();
	if ( mlen > 0 ) {
	    // New timer received, make it relative to now
	    new->event += now;
	    addTimer(new);
	}
	// check and execute expired timers
	int delta=0;
	while ( queue  && (delta = queue->event - now) <= 0 ) {
	    expireTimer();
	}
	// check max waiting time before next timer fires
	if ( !queue || delta <= 0 )
	    delay = portMAX_DELAY;
	else 
	    delay = delta;
    }
}

static void insertTimer(long event,long repeat,timerJob job,char *cmd)
{
    struct TimerQueue *new;
    // set to fire in relative from now x seconds
    new = newTimer(event,repeat,job,cmd);

    if ( !new ) 
	return ;
    // printf("Add: %ld %ld %s ",new.event,new.repeat,new.cmd);
    int res = xMessageBufferSend( timerJobMB,
		   &new,
		   sizeof(new),
		    portMAX_DELAY);
    if ( res == 0 ) {
	ESP_LOGI(TAG,"Cannot add timer..");
	free(new);
    }
}
esp_err_t peek(char *p)
{
    ESP_LOGI(TAG,"Run: %s",p);
    return ESP_OK;
}

// Queue new 
char *queueTimerJob(char *descriptor,esp_err_t (*job)(char *args))
{
    char *file=descriptor;
	int event_time = 0; // NOW
	int event_repeat = 0; // NOW
	// if prefix by +\d+  assume that is the time the job needs to run
	// if prefix by another +\d+  assume that is the interval to rerun
	// if prefix by '-' then remove ANY jobs matching the file
	if (file[0] == '+' ) {
	    event_time = strtol(file,&file,0);
	    while(*file == ' ') file++;
	}
	if (file[0] == '+' ) {
	    event_repeat = strtol(file,&file,0);
	    while(*file == ' ') file++;
	}
	if (file[0] == '-' ) {
	    timerRemoveJobs(file+1);
	} else if ( event_time > 0 ) {
	    insertTimer(event_time,event_repeat,job,file);
	} else if ( *file && (*job)(file) != ESP_OK) {
	    return NULL;
	}

	return getTimerList();
}

static int cmdTimer(int argc,char **argv)
{

	if ( argc == 2 && strcmp(argv[1],"dmp") == 0) {
	    dmpList();
	} else if ( argc == 3 && strcmp(argv[1],"del") == 0) {
	    struct TimerQueue *item = removeTimer(&queue,strtol(argv[2],NULL,0));
	    free(item);
	} else if ( argc == 5 && strcmp(argv[1],"ins") == 0) {

	    insertTimer( strtol(argv[2],NULL,0),strtol(argv[3],NULL,0),peek,argv[4]);

	}
return 0;
}
static void initializeTimer(void)
{
    xTaskCreate(timerTask, "cron", 4*2048, NULL, tskIDLE_PRIORITY, NULL);
}
void registerTimer(void)
    {
    initializeTimer();
    const esp_console_cmd_t cmd[] = {
	{
	.command = "timer",
	.help = "Start or stop  wifi interface",
	.hint = NULL,
	.func = &cmdTimer  // useless -- shoulnd be there
	},
    };
    for (int i=0;i<(sizeof(cmd)/sizeof(cmd[0]));i++){
	ESP_ERROR_CHECK( esp_console_cmd_register(cmd+i) );
    }
    ESP_LOGI(TAG,"Start wifi");
    // Autostart..
    //cmdWifiStart(0,NULL);
}

