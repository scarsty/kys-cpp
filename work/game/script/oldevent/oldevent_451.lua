--function oldevent_451()

    if instruct_16(30,0,12) ==true then    --  16(10):队伍是否有[郭靖]否则跳转到:Label0
        instruct_1(1666,55,1);   --  1(1):[郭靖]说: 三师父好。
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_1(1667,132,0);   --  1(1):[???]说: 靖儿，马是人类的朋友，要*爱护动物。
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label0


    if instruct_16(55,0,11) ==true then    --  16(10):队伍是否有[郭靖]否则跳转到:Label1
        instruct_1(1666,55,1);   --  1(1):[郭靖]说: 三师父好。
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_1(1667,132,0);   --  1(1):[???]说: 靖儿，马是人类的朋友，要*爱护动物。
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label1

    instruct_1(1657,132,0);   --  1(1):[???]说: 马王神韩宝驹，就是我啦
    instruct_0();   --  0(0)::空语句(清屏)
--end

