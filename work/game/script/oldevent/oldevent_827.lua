--function oldevent_827()
    instruct_1(3329,198,0);   --  1(1):[???]说: 嵩山派禁地，生人勿近！
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_5(2,0) ==false then    --  5(5):是否选择战斗？是则跳转到:Label0
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label0


    if instruct_6(29,4,0,0) ==false then    --  6(6):战斗[29]是则跳转到:Label1
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label1

    instruct_3(-2,-2,0,0,0,0,0,0,0,0,-2,-2,-2);   --  3(3):修改事件定义:当前场景:当前场景事件编号
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
--end

