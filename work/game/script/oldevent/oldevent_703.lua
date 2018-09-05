--function oldevent_703()

    if instruct_29(0,80,999,20,0) ==false then    --  29(1D):判断AAA武力80-999是则跳转到:Label0
        instruct_1(2874,210,0);   --  1(1):[???]说: 我刚刚开始学习罗汉拳，咱*们一起练练吧。
        instruct_0();   --  0(0)::空语句(清屏)

        if instruct_5(2,0) ==false then    --  5(5):是否选择战斗？是则跳转到:Label1
            instruct_0();   --  0(0)::空语句(清屏)
            do return; end
        end    --:Label1


        if instruct_6(79,1,0,1) ==false then    --  6(6):战斗[79]是则跳转到:Label2
            instruct_0();   --  0(0)::空语句(清屏)
        end    --:Label2

        instruct_0();   --  0(0)::空语句(清屏)
        instruct_13();   --  13(D):重新显示场景
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0

    instruct_1(2875,210,0);   --  1(1):[???]说: 施主武艺已经如此高强，小*僧万万不是对手。
    instruct_0();   --  0(0)::空语句(清屏)
    do return; end
--end

