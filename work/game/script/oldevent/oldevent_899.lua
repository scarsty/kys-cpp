--function oldevent_899()
    instruct_1(3588,19,0);   --  1(1):[岳不群]说: 下月十五的嵩山大会上，*岳某将尽力而为．
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3589,0,1);   --  1(1):[AAA]说: 到时我一定去帮你．
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_16(35,2,0) ==false then    --  16(10):队伍是否有[令狐冲]是则跳转到:Label0
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0

    instruct_1(3590,35,1);   --  1(1):[令狐冲]说: 是啊，师父，*到时我们一定会去帮你．
    instruct_0();   --  0(0)::空语句(清屏)
--end

