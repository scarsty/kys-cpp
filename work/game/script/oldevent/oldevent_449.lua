--function oldevent_449()

    if instruct_16(30,0,12) ==true then    --  16(10):队伍是否有[郭靖]否则跳转到:Label0
        instruct_1(1662,55,1);   --  1(1):[郭靖]说: 大师父好。
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_1(1663,130,0);   --  1(1):[???]说: 靖儿，你要是敢结交匪类，*胡作非为，我就一杖打死你*！
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label0


    if instruct_16(55,0,11) ==true then    --  16(10):队伍是否有[郭靖]否则跳转到:Label1
        instruct_1(1662,55,1);   --  1(1):[郭靖]说: 大师父好。
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_1(1663,130,0);   --  1(1):[???]说: 靖儿，你要是敢结交匪类，*胡作非为，我就一杖打死你*！
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label1

    instruct_1(1655,130,0);   --  1(1):[???]说: 我乃江南七怪之首，飞天蝙*蝠柯镇恶。
    instruct_0();   --  0(0)::空语句(清屏)
--end

