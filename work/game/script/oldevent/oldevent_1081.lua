--function oldevent_1081()

    if instruct_16(82,2,0) ==false then    --  16(10):队伍是否有[宋青书]是则跳转到:Label0
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0

    instruct_14();   --  14(E):场景变黑
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3996,82,0);   --  1(1):[???]说: 我不能回武当，我不回！
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_21(82);   --  21(15):[宋青书]离队
    instruct_3(70,54,1,0,179,0,0,7040,7040,7040,-2,-2,-2);   --  3(3):修改事件定义:场景[小村]:场景事件编号 [54]
    instruct_0();   --  0(0)::空语句(清屏)
--end

