vDevices
---------------

How to use it:

- Get devices: ls -l /dev/

> crw------- root     root     252,   1 2013-02-25 17:43 vialab1
> crw------- root     root     252,   0 2013-02-25 17:43 vialab0

- Write into a device

> echo 13414 > /dev/vialab0

- Read from a device

> cat /dev/vialab0
> 13414