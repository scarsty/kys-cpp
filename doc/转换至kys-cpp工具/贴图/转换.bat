@echo off

.\ConsoleImz.exe -rlew mmap.idx mmap.grp mmap
.\ConsoleImz.exe -rlew sdx smp smap
.\ConsoleImz.exe -rlew hdgrp.idx hdgrp.grp head
.\ConsoleImz.exe -rlew wdx wmp wmap
.\ConsoleImz.exe -rlew eft.idx eft.grp eft

set filepath=%cd%
cd head
call:clear_to1
cd %filepath%

for /l %%i in (0, 1, 9) do call:for_fight 00%%i
for /l %%i in (10, 1, 99) do call:for_fight 0%%i
for /l %%i in (100, 1, 999) do call:for_fight %%i

exit /b 0

:for_fight
.\ConsoleImz.exe -rlew fight%1.idx fight%1.grp fight%1
.\ConsoleImz.exe -rlew fdx%1 fmp%1 fight%1
set filepath=%cd%
cd fight%1
call:clear_to1
cd %filepath%
goto:eof


:clear_to1
del *_1.png
del *_2.png
del *_3.png
del *_4.png
del *_5.png
del *_6.png
del *_7.png
del *_8.png
del *_9.png

rename ?_0.png ?.png
rename ??_0.png ??.png
rename ???_0.png ???.png
goto:eof