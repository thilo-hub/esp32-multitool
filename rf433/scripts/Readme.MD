# Scripts to generate the RF433 patterns
# It seems my RF works differently...

# 

But "capture" and "make-send-blobs" works.

* Capture

- run "capture" on beaglbone
- press the remote buttons
- run "stop" on beaglebone
- copy the displayed output into "logfile"


* Convert

perl parse_capture.pl "logfile"

# Eventuall errors can be fixed maybe by changing the jitter...

for i in Pattern.0* ; do 
	perl transform.pl $i >c.$i ; 
done

The resulting "c.Patterh.*" files can simply be typed on the ESP console

or converted into binary using "xxd -i {..}" and then passed into the player or stored in the ESP filesystem and executed using "play2 file.."






