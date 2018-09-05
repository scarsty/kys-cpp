--function oldevent_59()

    if instruct_43(36,22,0) ==false then    --  43(2B):是否有物品玄铁剑是则跳转到:Label0
        instruct_0();   --  0(0)::空语句(清屏)

        if instruct_43(37,17,0) ==false then    --  43(2B):是否有物品倚天剑是则跳转到:Label1
            instruct_0();   --  0(0)::空语句(清屏)

            if instruct_43(43,12,0) ==false then    --  43(2B):是否有物品屠龙刀是则跳转到:Label2
                instruct_0();   --  0(0)::空语句(清屏)

                if instruct_43(55,7,0) ==false then    --  43(2B):是否有物品神山剑是则跳转到:Label3
                    instruct_0();   --  0(0)::空语句(清屏)

                    if instruct_43(56,2,0) ==false then    --  43(2B):是否有物品玄铁菜刀是则跳转到:Label4
                        instruct_0();   --  0(0)::空语句(清屏)
                        do return; end
                    end    --:Label4

                end    --:Label3

            end    --:Label2

        end    --:Label1

    end    --:Label0


    if instruct_29(0,100,999,2,0) ==false then    --  29(1D):判断AAA武力100-999是则跳转到:Label5
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label5

    instruct_1(268,0,1);   --  1(1):[AAA]说: 芝麻开门——
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_57();   --  57(39):高昌迷宫劈门
    instruct_3(-2,2,1,1,0,0,0,7746,7746,7746,-2,-2,-2);   --  3(3):修改事件定义:当前场景:场景事件编号 [2]
    instruct_3(-2,3,0,0,0,0,0,7804,7804,7804,-2,-2,-2);   --  3(3):修改事件定义:当前场景:场景事件编号 [3]
    instruct_3(-2,4,1,0,0,0,0,7862,7862,7862,-2,-2,-2);   --  3(3):修改事件定义:当前场景:场景事件编号 [4]
--end

