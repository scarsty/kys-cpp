--function oldevent_94()

    if instruct_4(229,6,0) ==false then    --  4(4):是否使用物品[四十二章经]？是则跳转到:Label0
        instruct_1(384,0,1);   --  1(1):[AAA]说: 这个，别烧了，还是留着吧
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0


    if instruct_31(10000,0,11) ==true then    --  31(1F):判断银子是否够10000否则跳转到:Label1
        instruct_1(385,0,1);   --  1(1):[AAA]说: 燃烧吧，我的《四十二章经*》……咦，有字显示出来，*真神奇，*“鹿鼎山，（23，81）……”
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_32(229,-1);   --  32(20):物品[四十二章经]+[-1]
        instruct_39(67);   --  39(27):打开场景鹿鼎山
        do return; end
    end    --:Label1


    if instruct_31(5000,0,11) ==true then    --  31(1F):判断银子是否够5000否则跳转到:Label2
        instruct_1(386,0,1);   --  1(1):[AAA]说: 燃烧吧，我的《四十二章经*》……咦，有字显示出来，*真神奇，*“鹿鼎山，（66，117）……”
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_32(229,-1);   --  32(20):物品[四十二章经]+[-1]
        instruct_39(90);   --  39(27):打开场景鹿鼎山
        do return; end
    end    --:Label2

    instruct_1(387,0,1);   --  1(1):[AAA]说: 燃烧吧，我的《四十二章经*》……咦，有字显示出来，*真神奇，*“鹿鼎山，（32，104）……”
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_32(229,-1);   --  32(20):物品[四十二章经]+[-1]
    instruct_39(91);   --  39(27):打开场景鹿鼎山
    do return; end
--end

