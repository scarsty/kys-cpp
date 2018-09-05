--function oldevent_850()

    if instruct_4(174,2,0) ==false then    --  4(4):是否使用物品[银两]？是则跳转到:Label0
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0


    if instruct_31(300,6,0) ==false then    --  31(1F):判断银子是否够300是则跳转到:Label1
        instruct_1(3379,236,0);   --  1(1):[???]说: 钱不够啊，这可不行，我这*已经是成本价了，怎么说你*也要让我糊口啊。
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label1

    instruct_32(174,-300);   --  32(20):物品[银两]+[-300]
    instruct_1(3380,236,0);   --  1(1):[???]说: 好，一手交钱，一手交货。
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_2(42,1);   --  2(2):得到物品[白虹剑][1]
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_26(-2,45,2,2,0);   --  26(1A):增加场景事件编号的三个触发事件编号
--end

