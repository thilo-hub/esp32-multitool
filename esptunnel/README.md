
# Web interface API
usage using command line tools shown

## Authenticated interfaces
  - authenticate
      
      ```curl -u Beaglebone:Beagle1  http://$HOSTNAME/basic_auth```

  - Reset beaglebone ( toggle a gpio )
      
      ```curl -u Beaglebone:Beagle1  http://$HOSTNAME/basic_auth"?reset"```

  - Enable uart interface (on port 9876 - hardcoded)
      
      ```curl -u Beaglebone:Beagle1  http://$HOSTNAME/basic_auth &&
	stty -icanon -echo ;  nc -v $HOSTNAME 9876 ; stty sane```

## Nonauthenticated interfaces
  - Get (json) list of button mappings

    > curl http://$HOSTNAME/buttons.json

  - Update (write new) list of mappings

    > curl --data-binary @esp_filesystem/public/buttons.json http://$HOSTNAME/send

  - Run button script now ( valid for every file inside esp_filesystem/buttons )

    > curl -d buttons/oc.on  http://$HOSTNAME/send

  - add new timer job in 60 seconds:

    > curl -d +60buttons/oc.on  http://$HOSTNAME/send

  - add new timer job in 60 seconds, repeating every 50 seconds:

    > curl -d +60+50buttons/oc.on  http://$HOSTNAME/send

  - Query existing timer jobs:

    > curl -d +0  http://$HOSTNAME/send

  - Delete matching timers (here all oc*):

    > curl -d -buttons/oc  http://$HOSTNAME/send

### Timer list
Every call to the API returns "failure" if a job couldnt be run
or the list of previously existing timers.
(a newly added timer is not shown, because a separate task adds it to the queue
Which usually means, the task did not run before retrieving the queue)
The format might change, currenly a simple line separated list:

```
0E: 2652 +0 :buttons/oc.on
1E: 3380 +0 :buttons/oa.on
2E: 5407 +12000 :buttons/oa.on
```

Index, next run in x-ticks (100 ticks are 1 second on the ESP32), repeat intervall in ticks and :{file-name}





