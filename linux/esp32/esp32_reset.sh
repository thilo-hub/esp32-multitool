#!/bin/sh

cd /sys/class/gpio

test -d gpio117 || echo -n 117 >export
test -d gpio49  || echo -n 49 >export

echo -n out > gpio117/direction
echo -n out > gpio49/direction

CALL=$0
if [ $# -eq 1 ]; then
	CALL=$1
fi

case $CALL in
	*download*)
		echo -n 0 >gpio117/value &&  echo -n 0 > gpio49/value && sleep 1 &&   echo -n 1 > gpio49/value ;
		;;
	*reset*)
		echo -n 1 >gpio117/value &&  echo -n 0 > gpio49/value && sleep 1 &&   echo -n 1 > gpio49/value ;
		;;
esac
