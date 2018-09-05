--function oldevent_202()

    if instruct_4(208,0,23) ==true then    --  4(4):是否使用物品[金钥匙]？否则跳转到:Label0
        instruct_3(-2,-2,1,0,0,0,0,3500,3500,3500,-2,-2,-2);   --  3(3):修改事件定义:当前场景:当前场景事件编号
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_32(208,-1);   --  32(20):物品[金钥匙]+[-1]
        instruct_2(218,1);   --  2(2):得到物品[鸯刀][1]
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0

    instruct_0();   --  0(0)::空语句(清屏)
--end

