--function oldevent_724()

    if instruct_29(0,100,999,19,0) ==false then    --  29(1D):判断AAA武力100-999是则跳转到:Label0
        instruct_1(2897,209,0);   --  1(1):[???]说: 少侠想与我切磋武功吗？
        instruct_0();   --  0(0)::空语句(清屏)

        if instruct_5(2,0) ==false then    --  5(5):是否选择战斗？是则跳转到:Label1
            instruct_0();   --  0(0)::空语句(清屏)
            do return; end
        end    --:Label1


        if instruct_6(220,1,0,1) ==false then    --  6(6):战斗[220]是则跳转到:Label2
            instruct_0();   --  0(0)::空语句(清屏)
        end    --:Label2

        instruct_0();   --  0(0)::空语句(清屏)
        instruct_13();   --  13(D):重新显示场景
        do return; end
    end    --:Label0

    instruct_1(2898,209,0);   --  1(1):[???]说: 少侠武功高强，贫道不是对*手。
    instruct_0();   --  0(0)::空语句(清屏)
    do return; end
--end

