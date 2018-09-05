--function oldevent_30()
    instruct_1(156,223,0);   --  1(1):[???]说: 这位少侠想休息一下吗？10*两银子一晚。
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_11(2,0) ==false then    --  11(B):是否住宿是则跳转到:Label0
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label0


    if instruct_31(10,6,0) ==false then    --  31(1F):判断银子是否够10是则跳转到:Label1
        instruct_1(157,223,0);   --  1(1):[???]说: 10两银子也没有，一边凉快*去！
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label1

    instruct_32(174,-10);   --  32(20):物品[银两]+[-10]
    instruct_12();   --  12(C):住宿休息
    instruct_13();   --  13(D):重新显示场景
    instruct_1(158,223,0);   --  1(1):[???]说: 少侠慢走，欢迎下次光临
    instruct_0();   --  0(0)::空语句(清屏)
--end

