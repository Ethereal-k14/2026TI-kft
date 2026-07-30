@echo off
chcp 65001 >nul
set SRC=D:\Backup\Documents\26电赛 2\oled_timer
set DST=F:\BaiduNetdiskDownload\STM32入门教程资料\程序源码\程序源码\STM32Project-有注释版\4-1 OLED显示屏 - 副本
echo Copying files to Keil project...
copy /Y "%SRC%\Hardware\Timer.h" "%DST%\Hardware\Timer.h"
copy /Y "%SRC%\Hardware\Timer.c" "%DST%\Hardware\Timer.c"
copy /Y "%SRC%\User\main.c"      "%DST%\User\main.c"
copy /Y "%SRC%\User\stm32f10x_it.c" "%DST%\User\stm32f10x_it.c"
echo Done! Then in Keil: right-click Hardware - Add Timer.c
pause