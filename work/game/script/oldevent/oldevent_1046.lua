--function oldevent_1046()
    instruct_1(3898,64,0);   --  1(1):[周伯通]说: 来来来，和老顽童过两招。
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_5(6,0) ==false then    --  5(5):是否选择战斗？是则跳转到:Label0
        instruct_1(3899,0,1);   --  1(1):[AAA]说: 前辈别开玩笑了，晚辈哪里*是您的对手！
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0


    if instruct_6(67,8,0,1) ==false then    --  6(6):战斗[67]是则跳转到:Label1
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_13();   --  13(D):重新显示场景
        instruct_1(3900,64,0);   --  1(1):[周伯通]说: 你的功夫还不行，去练练再*来！
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label1

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3901,64,0);   --  1(1):[周伯通]说: 小兄弟，你这是什么功夫？*教教我好不好？
    instruct_0();   --  0(0)::空语句(清屏)
--end

