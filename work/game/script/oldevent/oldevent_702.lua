--function oldevent_702()
    instruct_1(2873,70,0);   --  1(1):[玄慈]说: 阿弥陀佛，习武之道在于循*序渐进，施主切忌操之过急*。
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_28(0,80,999,2,0) ==false then    --  28(1C):判断AAA品德80-999是则跳转到:Label0
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0


    if instruct_29(0,0,100,2,0) ==false then    --  29(1D):判断AAA武力0-100是则跳转到:Label1
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label1

    instruct_1(2887,70,0);   --  1(1):[玄慈]说: 少侠人品素雅，但武功似乎*还有所欠缺，老衲这里有本*秘笈，就赠与少侠吧。
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_2(137,1);   --  2(2):得到物品[燃木刀法][1]
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(2888,0,1);   --  1(1):[AAA]说: 多谢玄慈大师。
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_3(-2,-2,1,0,886,0,0,-2,-2,-2,-2,-2,-2);   --  3(3):修改事件定义:当前场景:当前场景事件编号
--end

