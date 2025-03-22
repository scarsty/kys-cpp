.\ConsoleImz.exe -rlew mmap.idx mmap.grp mmap
.\ConsoleImz.exe -rlew sdx smp smap
.\ConsoleImz.exe -rlew hdgrp.idx hdgrp.grp head
.\ConsoleImz.exe -rlew wdx wmp wmap
del *_1.png
del *_2.png
del *_3.png
del *_4.png
del *_5.png
del *_6.png
del *_7.png
del *_8.png
del *_9.png

for /l %%i in (0, 1, 9) do ConsoleImz -rlew fight00%%i.idx fight00%%i.grp fight00%%i
for /l %%i in (10, 1, 99) do ConsoleImz -rlew fight0%%i.idx fight0%%i.grp fight0%%i
for /l %%i in (100, 1, 999) do ConsoleImz -rlew fight%%i.idx fight%%i.grp fight%%i

del f*\*_1.png
del f*\*_2.png
del f*\*_3.png
del f*\*_4.png
del f*\*_5.png
del f*\*_6.png
del f*\*_7.png
del f*\*_8.png
del f*\*_9.png
