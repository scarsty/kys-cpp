--function oldevent_708()
    instruct_1(2880,210,0);   --  1(1):[???]说: 少林木人巷，每个人只有一*次挑战机会，你想挑战吗？
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_5(2,0) ==false then    --  5(5):是否选择战斗？是则跳转到:Label0
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label0

    instruct_1(2881,210,0);   --  1(1):[???]说: 好，施主里边请！
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_19(35,15);   --  19(13):主角移动至23-F
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(212,0,23,1) ==true then    --  6(6):战斗[212]否则跳转到:Label1
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_13();   --  13(D):重新显示场景

        if instruct_6(213,0,15,1) ==true then    --  6(6):战斗[213]否则跳转到:Label2
            instruct_0();   --  0(0)::空语句(清屏)
            instruct_13();   --  13(D):重新显示场景

            if instruct_6(214,0,7,1) ==true then    --  6(6):战斗[214]否则跳转到:Label3
                instruct_19(34,10);   --  19(13):主角移动至22-A
                instruct_0();   --  0(0)::空语句(清屏)
                instruct_13();   --  13(D):重新显示场景
                instruct_0();   --  0(0)::空语句(清屏)
                do return; end
            end    --:Label3

            instruct_0();   --  0(0)::空语句(清屏)
        end    --:Label2

        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label1

    instruct_19(35,17);   --  19(13):主角移动至23-11
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_3(-2,-2,1,0,709,0,0,-2,-2,-2,-2,-2,-2);   --  3(3):修改事件定义:当前场景:当前场景事件编号
--end

