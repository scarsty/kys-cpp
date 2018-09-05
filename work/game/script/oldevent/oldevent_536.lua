--function oldevent_536()

    if instruct_16(51,2,0) ==false then    --  16(10):队伍是否有[慕容复]是则跳转到:Label0
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0

    instruct_14();   --  14(E):场景变黑
    instruct_19(33,44);   --  19(13):主角移动至21-2C
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
--end

