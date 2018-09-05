--function oldevent_995()

    if instruct_4(174,2,0) ==false then    --  4(4):是否使用物品[银两]？是则跳转到:Label0
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0


    if instruct_31(720,6,0) ==false then    --  31(1F):判断银子是否够720是则跳转到:Label1
        instruct_1(3824,225,0);   --  1(1):[???]说: 因为你是武林盟主，所以我*才给你打了九折，可不能再*便宜了。
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label1

    instruct_2(8,1);   --  2(2):得到物品[天王保命丹][1]
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_32(174,-720);   --  32(20):物品[银两]+[-720]
    instruct_0();   --  0(0)::空语句(清屏)
--end

