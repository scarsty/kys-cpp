--function oldevent_88()

    if instruct_16(87,6,0) ==false then    --  16(10):队伍是否有[苏荃]是则跳转到:Label0
        instruct_1(331,71,0);   --  1(1):[洪教主]说: 夫人啊，我的夫人，你在哪*里啊，你在哪里？
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0

    instruct_1(332,71,0);   --  1(1):[洪教主]说: 夫人，你回来了，太好了*……
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(333,87,1);   --  1(1):[???]说: 对不起，请叫我韦夫人。
    instruct_0();   --  0(0)::空语句(清屏)
--end

