--function oldevent_454()

    if instruct_16(30,0,12) ==true then    --  16(10):队伍是否有[郭靖]否则跳转到:Label0
        instruct_1(1672,55,1);   --  1(1):[郭靖]说: 六师父好。
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_1(1673,135,0);   --  1(1):[???]说: 靖儿，做生意要学会占便宜*，但是做人要学会吃亏。
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label0


    if instruct_16(55,0,11) ==true then    --  16(10):队伍是否有[郭靖]否则跳转到:Label1
        instruct_1(1672,55,1);   --  1(1):[郭靖]说: 六师父好。
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_1(1673,135,0);   --  1(1):[???]说: 靖儿，做生意要学会占便宜*，但是做人要学会吃亏。
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label1

    instruct_1(1660,135,0);   --  1(1):[???]说: 我就是人称闹事侠隐的全金*发。
    instruct_0();   --  0(0)::空语句(清屏)
--end

