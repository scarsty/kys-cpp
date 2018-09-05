--function oldevent_861()

    if instruct_60(-2,60,2286,6,0) ==false then    --  60(3C):判断场景-2事件位置60是否有贴图2286是则跳转到:Label0
        instruct_1(3387,236,0);   --  1(1):[???]说: 还没有打造出新兵器，你以*后再来吧。
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0

    instruct_1(3388,236,0);   --  1(1):[???]说: 有了，有了，我有灵感了，*我要给你打造一件最适合你*的兵器！
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_43(166,0,16) ==true then    --  43(2B):是否有物品柴刀十八路否则跳转到:Label1
        instruct_3(-2,-2,1,0,862,863,0,-2,-2,-2,-2,-2,-2);   --  3(3):修改事件定义:当前场景:当前场景事件编号
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label1


    if instruct_43(134,0,17) ==true then    --  43(2B):是否有物品松风剑法否则跳转到:Label2
        instruct_3(-2,-2,1,0,864,865,0,-2,-2,-2,-2,-2,-2);   --  3(3):修改事件定义:当前场景:当前场景事件编号
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label2


    if instruct_43(186,0,17) ==true then    --  43(2B):是否有物品杨家枪否则跳转到:Label3
        instruct_3(-2,-2,1,0,866,867,0,-2,-2,-2,-2,-2,-2);   --  3(3):修改事件定义:当前场景:当前场景事件编号
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label3

--end

