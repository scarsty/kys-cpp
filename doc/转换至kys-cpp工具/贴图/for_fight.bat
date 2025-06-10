
.\ConsoleImz.exe -rlew %1.idx %1.grp %1

del %1\*_1.png
del %1\*_2.png
del %1\*_3.png
del %1\*_4.png
del %1\*_5.png
del %1\*_6.png
del %1\*_7.png
del %1\*_8.png
del %1\*_9.png

set filepath=%cd%
cd %1
rename ?_0.png ?.png
rename ??_0.png ??.png
rename ???_0.png ???.png
cd %filepath%
