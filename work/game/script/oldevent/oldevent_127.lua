--function oldevent_127()
    instruct_1(390,38,0);   --  1(1):[石破天]说: 有需要我帮忙的地方吗？
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_9(0,43) ==true then    --  9(9):是否要求加入?否则跳转到:Label0

        if instruct_20(34,0) ==false then    --  20(14):队伍是否满？是则跳转到:Label1
            instruct_10(38);   --  10(A):加入人物[石破天]
            instruct_14();   --  14(E):场景变黑
            instruct_3(-2,16,0,0,0,0,0,0,0,0,-2,-2,-2);   --  3(3):修改事件定义:当前场景:场景事件编号 [16]
            instruct_3(-2,58,0,0,0,0,0,0,0,0,-2,-2,-2);   --  3(3):修改事件定义:当前场景:场景事件编号 [58]
            instruct_0();   --  0(0)::空语句(清屏)
            instruct_13();   --  13(D):重新显示场景
            do return; end
        end    --:Label1

        instruct_1(391,38,0);   --  1(1):[石破天]说: 你的队伍已满，我无法加入。
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0

--end

