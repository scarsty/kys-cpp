@echo off

.\xia.exe mmap.idx mmap.grp mmap 1
.\xia.exe sdx smp smap 1
.\xia.exe hdgrp.idx hdgrp.grp head 0
.\xia.exe wdx wmp wmap 1
.\xia.exe eft.idx eft.grp eft 0

for /l %%i in (0, 1, 9) do call:for_fight 00%%i
for /l %%i in (10, 1, 99) do call:for_fight 0%%i
for /l %%i in (100, 1, 999) do call:for_fight %%i

exit /b 0

:for_fight
.\xia.exe fight%1.idx fight%1.grp fight%1 0
.\xia.exe fdx%1 fmp%1 fight%1 0
goto:eof
