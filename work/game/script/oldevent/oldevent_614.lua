--function oldevent_614()

    if instruct_16(47,2,0) ==false then    --  16(10):队伍是否有[阿紫]是则跳转到:Label0
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label0

    instruct_14();   --  14(E):场景变黑
    instruct_13();   --  13(D):重新显示场景
    instruct_1(2675,47,0);   --  1(1):[阿紫]说: 不，我不要回这里，我不要*回这里……
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_21(47);   --  21(15):[阿紫]离队
    instruct_3(70,23,1,0,133,0,0,6374,6374,6374,-2,-2,-2);   --  3(3):修改事件定义:场景[小村]:场景事件编号 [23]
--end

