--function oldevent_1066()
    instruct_1(3985,0,1);   --  1(1):[AAA]说: 不知少侠来我崆峒山有何贵*事？
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_5(2,0) ==false then    --  5(5):是否选择战斗？是则跳转到:Label0
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label0

    instruct_1(3986,247,1);   --  1(1):[???]说: 我想找你练练功，*赚些经验点数．
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3987,8,0);   --  1(1):[唐文亮]说: 哼！那就来吧．
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(17,4,0,0) ==false then    --  6(6):战斗[17]是则跳转到:Label1
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label1

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3988,247,1);   --  1(1):[???]说: 嗯，这经验点数还真好赚．
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_37(-1);   --  37(25):增加道德-1
--end

