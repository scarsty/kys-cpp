--function oldevent_51()

    if instruct_4(174,0,21) ==true then    --  4(4):是否使用物品[银两]？否则跳转到:Label0

        if instruct_31(100,6,0) ==false then    --  31(1F):判断银子是否够100是则跳转到:Label1
            instruct_1(237,0,1);   --  1(1):[AAA]说: 我身上的银子也不多了……
            instruct_0();   --  0(0)::空语句(清屏)
            do return; end
        end    --:Label1

        instruct_1(236,0,1);   --  1(1):[AAA]说: 银子太多了也没啥用，拿出*100两做慈善事业吧。
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_32(174,-100);   --  32(20):物品[银两]+[-100]
        instruct_37(1);   --  37(25):增加道德1
        do return; end
    end    --:Label0

    instruct_0();   --  0(0)::空语句(清屏)
--end

