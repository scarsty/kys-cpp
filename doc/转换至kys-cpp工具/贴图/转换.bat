.\ConsoleImz.exe -rlew mmap.idx mmap.grp mmap
.\ConsoleImz.exe -rlew sdx smp smap
.\ConsoleImz.exe -rlew hdgrp.idx hdgrp.grp head
.\ConsoleImz.exe -rlew wdx wmp wmap
.\ConsoleImz.exe -rlew eft.idx eft.grp eft
del *_1.png
del *_2.png
del *_3.png
del *_4.png
del *_5.png
del *_6.png
del *_7.png
del *_8.png
del *_9.png

for /l %%i in (0, 1, 9) do call for_fight.bat fight00%%i
for /l %%i in (10, 1, 99) do call for_fight.bat fight0%%i
for /l %%i in (100, 1, 999) do call for_fight.bat fight%%i

