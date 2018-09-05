--function oldevent_1045()

    if instruct_4(195,2,0) ==false then    --  4(4):是否使用物品[铁铲]？是则跳转到:Label0
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0

    instruct_14();   --  14(E):场景变黑
    instruct_13();   --  13(D):重新显示场景
    instruct_2(83,1);   --  2(2):得到物品[九阳真经][1]
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_37(-1);   --  37(25):增加道德-1
    instruct_3(-2,-2,-2,0,-2,0,0,-2,-2,-2,-2,-2,-2);   --  3(3):修改事件定义:当前场景:当前场景事件编号
--end

