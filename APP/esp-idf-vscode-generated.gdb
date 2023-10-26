target remote :3333
set remote hardware-watchpoint-limit 2
symbol-file C:\Users\acebear\source\S3F4E\ESP32\APP/build/APP.elf
mon reset halt
flushregs
thb app_main