--function oldevent_471()
    instruct_1(1894,208,0);   --  1(1):[???]说: 铁掌帮重地，不得擅闯！
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_5(1,0) ==false then    --  5(5):是否选择战斗？是则跳转到:Label0
        do return; end
    end    --:Label0


    if instruct_6(70,3,0,0) ==false then    --  6(6):战斗[70]是则跳转到:Label1
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
    end    --:Label1

    instruct_3(-2,2,0,0,0,0,0,0,0,0,-2,-2,-2);   --  3(3):修改事件定义:当前场景:场景事件编号 [2]
    instruct_3(-2,3,0,0,0,0,0,0,0,0,-2,-2,-2);   --  3(3):修改事件定义:当前场景:场景事件编号 [3]
    instruct_37(-1);   --  37(25):增加道德-1
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
--end

