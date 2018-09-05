--function oldevent_450()

    if instruct_16(30,0,12) ==true then    --  16(10):队伍是否有[郭靖]否则跳转到:Label0
        instruct_1(1664,55,1);   --  1(1):[郭靖]说: 二师父好。
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_1(1665,131,0);   --  1(1):[???]说: 靖儿，闯荡江湖的时候，要*机灵点，防人之心不可无。
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label0


    if instruct_16(55,0,11) ==true then    --  16(10):队伍是否有[郭靖]否则跳转到:Label1
        instruct_1(1664,55,1);   --  1(1):[郭靖]说: 二师父好。
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_1(1665,131,0);   --  1(1):[???]说: 靖儿，闯荡江湖的时候，要*机灵点，防人之心不可无。
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label1

    instruct_1(1656,131,0);   --  1(1):[???]说: 在下在江南七怪中排行在二*，妙手书生朱聪是也。
    instruct_0();   --  0(0)::空语句(清屏)
--end

