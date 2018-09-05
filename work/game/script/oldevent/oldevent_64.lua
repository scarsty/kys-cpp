--function oldevent_64()
    instruct_1(276,203,0);   --  1(1):[???]说: 什么人，胆敢擅闯神龙教！
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(94,4,0,0) ==false then    --  6(6):战斗[94]是则跳转到:Label0
        instruct_15(0);   --  15(F):战斗失败，死亡
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0

    instruct_3(-2,-2,0,0,0,0,0,0,0,0,0,0,0);   --  3(3):修改事件定义:当前场景:当前场景事件编号
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
--end

