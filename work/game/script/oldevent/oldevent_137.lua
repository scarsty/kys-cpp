--function oldevent_137()
    instruct_1(390,49,0);   --  1(1):[虚竹]说: 有需要我帮忙的地方吗？
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_9(0,43) ==true then    --  9(9):是否要求加入?否则跳转到:Label0

        if instruct_20(34,0) ==false then    --  20(14):队伍是否满？是则跳转到:Label1
            instruct_10(49);   --  10(A):加入人物[虚竹]
            instruct_14();   --  14(E):场景变黑
            instruct_3(-2,11,0,0,0,0,0,0,0,0,-2,-2,-2);   --  3(3):修改事件定义:当前场景:场景事件编号 [11]
            instruct_3(-2,10,0,0,0,0,0,0,0,0,-2,-2,-2);   --  3(3):修改事件定义:当前场景:场景事件编号 [10]
            instruct_0();   --  0(0)::空语句(清屏)
            instruct_13();   --  13(D):重新显示场景
            do return; end
        end    --:Label1

        instruct_1(391,49,0);   --  1(1):[虚竹]说: 你的队伍已满，我无法加入。
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0

--end

