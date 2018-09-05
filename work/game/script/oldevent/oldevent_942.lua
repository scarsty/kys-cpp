--function oldevent_942()

    if instruct_4(157,2,0) ==false then    --  4(4):是否使用物品[鸳鸯刀]？是则跳转到:Label0
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0

    instruct_32(157,-1);   --  32(20):物品[鸳鸯刀]+[-1]
    instruct_3(-2,-2,1,0,0,0,0,4664,4664,4664,-2,-2,-2);   --  3(3):修改事件定义:当前场景:当前场景事件编号
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_43(144,98,0) ==false then    --  43(2B):是否有物品飞狐外传是则跳转到:Label1

        if instruct_43(145,93,0) ==false then    --  43(2B):是否有物品雪山飞狐是则跳转到:Label2

            if instruct_43(146,88,0) ==false then    --  43(2B):是否有物品连城诀是则跳转到:Label3

                if instruct_43(147,83,0) ==false then    --  43(2B):是否有物品天龙八部是则跳转到:Label4

                    if instruct_43(148,78,0) ==false then    --  43(2B):是否有物品射鵰英雄传是则跳转到:Label5

                        if instruct_43(149,73,0) ==false then    --  43(2B):是否有物品白马啸西风是则跳转到:Label6

                            if instruct_43(150,68,0) ==false then    --  43(2B):是否有物品鹿鼎记是则跳转到:Label7

                                if instruct_43(151,63,0) ==false then    --  43(2B):是否有物品笑傲江湖是则跳转到:Label8

                                    if instruct_43(152,58,0) ==false then    --  43(2B):是否有物品书剑恩仇录是则跳转到:Label9

                                        if instruct_43(153,53,0) ==false then    --  43(2B):是否有物品神鵰侠侣是则跳转到:Label10

                                            if instruct_43(154,48,0) ==false then    --  43(2B):是否有物品侠客行是则跳转到:Label11

                                                if instruct_43(155,43,0) ==false then    --  43(2B):是否有物品倚天屠龙记是则跳转到:Label12

                                                    if instruct_43(156,38,0) ==false then    --  43(2B):是否有物品碧血剑是则跳转到:Label13

                                                        if instruct_43(157,33,0) ==false then    --  43(2B):是否有物品鸳鸯刀是则跳转到:Label14
                                                            instruct_14();   --  14(E):场景变黑
                                                            instruct_3(-2,75,0,0,0,0,0,0,0,0,0,0,0);   --  3(3):修改事件定义:当前场景:场景事件编号 [75]
                                                            instruct_3(-2,76,0,0,0,0,0,0,0,0,0,0,0);   --  3(3):修改事件定义:当前场景:场景事件编号 [76]
                                                            instruct_0();   --  0(0)::空语句(清屏)
                                                            instruct_13();   --  13(D):重新显示场景
                                                            instruct_0();   --  0(0)::空语句(清屏)
                                                            do return; end
                                                        end    --:Label14

                                                        instruct_0();   --  0(0)::空语句(清屏)
                                                    end    --:Label13

                                                    instruct_0();   --  0(0)::空语句(清屏)
                                                end    --:Label12

                                                instruct_0();   --  0(0)::空语句(清屏)
                                            end    --:Label11

                                            instruct_0();   --  0(0)::空语句(清屏)
                                        end    --:Label10

                                        instruct_0();   --  0(0)::空语句(清屏)
                                    end    --:Label9

                                    instruct_0();   --  0(0)::空语句(清屏)
                                end    --:Label8

                                instruct_0();   --  0(0)::空语句(清屏)
                            end    --:Label7

                            instruct_0();   --  0(0)::空语句(清屏)
                        end    --:Label6

                        instruct_0();   --  0(0)::空语句(清屏)
                    end    --:Label5

                    instruct_0();   --  0(0)::空语句(清屏)
                end    --:Label4

                instruct_0();   --  0(0)::空语句(清屏)
            end    --:Label3

            instruct_0();   --  0(0)::空语句(清屏)
        end    --:Label2

        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label1

    instruct_0();   --  0(0)::空语句(清屏)
--end

