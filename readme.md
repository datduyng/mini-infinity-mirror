



# Developer Notes
## Arduino Nano


## Problem with uploading code to Arduino on Mac

This section will apply to you if you get below error while trying to upload

```
avrdude: stk500_recv(): programmer is not responding
avrdude: stk500_getsync() attempt 1 of 10: not in sync: resp=0x00
avrdude: stk500_recv(): programmer is not responding
avrdude: stk500_getsync() attempt 2 of 10: not in sync: resp=0x00
avrdude: stk500_recv(): programmer is not responding
avrdude: stk500_getsync() attempt 3 of 10: not in sync: resp=0x00
....
```

- You need to download CH341 driver. Check this post out in more detail. https://medium.com/@thuc/connect-arduino-nano-with-mac-osx-f922a46c0a5d
- Make sure you choose `Atmega328P (Old Bootloader)`



## NodeMCU
nodemcu board version: 2.7.4

## Cad files
- metrics: mm



## References
Project inspired by: [thingiverse](https://www.thingiverse.com/thing:4118457)
