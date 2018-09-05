--function oldevent_50()

    if instruct_28(0,2,999,6,0) ==false then    --  28(1C):判断AAA品德2-999是则跳转到:Label0
        instruct_1(235,0,1);   --  1(1):[AAA]说: 功德箱里已经没有钱了……
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label0

    instruct_1(234,0,1);   --  1(1):[AAA]说: 功德箱里有一些香火钱，现*在四下无人，我是不是拿来*一点花花？
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_11(0,8) ==true then    --  11(B):是否住宿否则跳转到:Label1
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_2(174,30);   --  2(2):得到物品[银两][30]
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_37(-2);   --  37(25):增加道德-2
        do return; end
    end    --:Label1

--end

