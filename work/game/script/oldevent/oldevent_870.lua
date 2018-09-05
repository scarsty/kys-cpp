--function oldevent_870()

    if instruct_4(174,2,0) ==false then    --  4(4):是否使用物品[银两]？是则跳转到:Label0
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0


    if instruct_31(1000,6,0) ==false then    --  31(1F):判断银子是否够1000是则跳转到:Label1
        instruct_1(3395,225,0);   --  1(1):[???]说: 1000两银子十个，不二价，*我韦小宝从来不作亏本的生*意。
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label1

    instruct_32(174,-1000);   --  32(20):物品[银两]+[-1000]
    instruct_1(3396,225,0);   --  1(1):[???]说: 好，给你，我韦小宝一向是*货真价实，玩骰子从不加水*银。
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_2(31,10);   --  2(2):得到物品[霹雳弹][10]
    instruct_0();   --  0(0)::空语句(清屏)
--end

