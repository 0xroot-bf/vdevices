vDevices
---------------

How to use it:

- Get devices: ls -l /dev/

> crw------- root     root     252,   1 2013-02-25 17:43 debug1

> crw------- root     root     252,   0 2013-02-25 17:43 debug0

- Write into a device

> echo 13414 > /dev/debug0

- Read from a device

> cat /dev/debug0
> 13414