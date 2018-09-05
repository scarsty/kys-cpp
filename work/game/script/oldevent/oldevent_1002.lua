--function oldevent_1002()
    instruct_1(3843,111,0);   --  1(1):[???]说: 这位客官，您要在小店休息*一晚吗？只需20两银子。
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_11(2,0) ==false then    --  11(B):是否住宿是则跳转到:Label0
        instruct_0();   --  0(0)::空语句(清屏)
        do return; end
    end    --:Label0


    if instruct_31(20,5,0) ==false then    --  31(1F):判断银子是否够20是则跳转到:Label1
        instruct_1(3844,111,0);   --  1(1):[???]说: 客官，我们这是小本生意，*概不赊帐。
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label1

    instruct_32(174,-20);   --  32(20):物品[银两]+[-20]
    instruct_14();   --  14(E):场景变黑
    instruct_12();   --  12(C):住宿休息
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3845,111,0);   --  1(1):[???]说: 客官，昨晚的泰式按摩还舒*服吗？您可要再来光顾小店*哦。
    instruct_0();   --  0(0)::空语句(清屏)
--end

