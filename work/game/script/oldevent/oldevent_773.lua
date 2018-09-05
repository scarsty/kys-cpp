--function oldevent_773()

    if instruct_20(24,0) ==false then    --  20(14):队伍是否满？是则跳转到:Label0
        instruct_1(3130,0,1);   --  1(1):[AAA]说: 令狐兄，我们走吧。
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_3(-2,-2,0,0,0,0,0,0,0,0,0,0,0);   --  3(3):修改事件定义:当前场景:当前场景事件编号
        instruct_0();   --  0(0)::空语句(清屏)
        instruct_10(35);   --  10(A):加入人物[令狐冲]
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0

    instruct_1(3042,35,0);   --  1(1):[令狐冲]说: 可惜，你的队伍已满，我无*法加入。
    instruct_0();   --  0(0)::空语句(清屏)
--end

