--function oldevent_928()
    instruct_14();   --  14(E):场景变黑
    instruct_13();   --  13(D):重新显示场景
    instruct_25(33,26,21,26);   --  25(19):场景移动33-26--21-26
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3761,255,0);   --  1(1):[???]说: 又是一年秋风送爽，又是一*日天高云淡。
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3762,256,0);   --  1(1):[???]说: 我们怀着无比激动的心情，*迎来了第29届奥林匹克大会*。
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3763,255,0);   --  1(1):[???]说: 本次大会的主题是——同一*个武林，同一个梦想！
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3764,256,0);   --  1(1):[???]说: 首先进行的是个人赛，下面*请一号选手上场！
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3765,255,0);   --  1(1):[???]说: （叫你呢，快上来！）
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3766,0,1);   --  1(1):[AAA]说: 哦？我是一号吗？
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_30(31,25,25,25);   --  30(1E):主角走动31-25--25-25
    instruct_1(3767,256,0);   --  1(1):[???]说: 比赛规则是，一号选手为擂*主。任何人都可以向擂主挑*战，战胜擂主的人将成为新*的擂主，直到无人挑战为止*。每个人上台前请先通名报*姓。
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3768,246,1);   --  1(1):[???]说: 这就是你的公平规则啊！！*为什么我是一号？
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3769,255,0);   --  1(1):[???]说: 好，我宣布，比赛正式开始*！
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3770,43,0);   --  1(1):[白万剑]说: 雪山掌门白万剑，前来领教
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(102,4,0,0) ==false then    --  6(6):战斗[102]是则跳转到:Label0
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label0

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3771,98,0);   --  1(1):[???]说: 让你尝尝我恶贯满盈段延庆*的厉害
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(103,4,0,0) ==false then    --  6(6):战斗[103]是则跳转到:Label1
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label1

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3772,150,0);   --  1(1):[???]说: 我就是一剑无血冯锡范
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(104,4,0,0) ==false then    --  6(6):战斗[104]是则跳转到:Label2
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label2

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3773,255,0);   --  1(1):[???]说: 年轻人，你已连赛三场，可*以稍微休息一下。
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_12();   --  12(C):住宿休息
    instruct_1(3774,162,0);   --  1(1):[???]说: 我丁不三今日还没杀够三个*人，算你一个吧。
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(105,4,0,0) ==false then    --  6(6):战斗[105]是则跳转到:Label3
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label3

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3775,67,0);   --  1(1):[裘千仞]说: 铁掌水上飘裘千仞来也。
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(106,4,0,0) ==false then    --  6(6):战斗[106]是则跳转到:Label4
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label4

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3776,19,0);   --  1(1):[岳不群]说: 华山君子剑向你挑战。
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(107,4,0,0) ==false then    --  6(6):战斗[107]是则跳转到:Label5
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label5

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3773,255,0);   --  1(1):[???]说: 年轻人，你已连赛三场，可*以稍微休息一下。
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_12();   --  12(C):住宿休息
    instruct_1(3777,189,0);   --  1(1):[???]说: 晋阳大侠萧半和前来讨教。
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(108,4,0,0) ==false then    --  6(6):战斗[108]是则跳转到:Label6
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label6

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3778,71,0);   --  1(1):[洪教主]说: 我乃神龙教主洪安通是也！
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(109,4,0,0) ==false then    --  6(6):战斗[109]是则跳转到:Label7
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label7

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3779,70,0);   --  1(1):[玄慈]说: 阿弥陀佛，少林方丈玄慈来*领教阁下高招。
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(110,4,0,0) ==false then    --  6(6):战斗[110]是则跳转到:Label8
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label8

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3773,255,0);   --  1(1):[???]说: 年轻人，你已连赛三场，可*以稍微休息一下。
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_12();   --  12(C):住宿休息
    instruct_1(3780,103,0);   --  1(1):[???]说: 我乃吐蕃国师，鸠摩智！
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(111,4,0,0) ==false then    --  6(6):战斗[111]是则跳转到:Label9
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label9

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3781,26,0);   --  1(1):[任我行]说: 日月神教教主任我行驾到！
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(112,4,0,0) ==false then    --  6(6):战斗[112]是则跳转到:Label10
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label10

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3782,57,0);   --  1(1):[黄药师]说: 我乃桃花岛主，人称东邪黄*药师的便是
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(119,4,0,0) ==false then    --  6(6):战斗[119]是则跳转到:Label11
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label11

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3773,255,0);   --  1(1):[???]说: 年轻人，你已连赛三场，可*以稍微休息一下。
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_12();   --  12(C):住宿休息
    instruct_1(3783,69,0);   --  1(1):[洪七公]说: 北丐洪七公！
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(113,4,0,0) ==false then    --  6(6):战斗[113]是则跳转到:Label12
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label12

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3784,62,0);   --  1(1):[金轮法王]说: 贫僧乃蒙古国师金轮法王。
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(114,4,0,0) ==false then    --  6(6):战斗[114]是则跳转到:Label13
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label13

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3785,60,0);   --  1(1):[欧阳锋]说: 我是欧阳锋，我就是西毒欧*阳锋。
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(115,4,0,0) ==false then    --  6(6):战斗[115]是则跳转到:Label14
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label14

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3773,255,0);   --  1(1):[???]说: 年轻人，你已连赛三场，可*以稍微休息一下。
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_12();   --  12(C):住宿休息
    instruct_1(3786,64,0);   --  1(1):[周伯通]说: 我老顽童周伯通来陪你玩玩*。
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(116,4,0,0) ==false then    --  6(6):战斗[116]是则跳转到:Label15
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label15

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3787,129,0);   --  1(1):[???]说: 全真教主，中神通王重阳，*再次复活。
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(117,4,0,0) ==false then    --  6(6):战斗[117]是则跳转到:Label16
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label16

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3788,5,0);   --  1(1):[张三丰]说: 老朽武当掌门张三丰。
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_6(118,4,0,0) ==false then    --  6(6):战斗[118]是则跳转到:Label17
        instruct_15(0);   --  15(F):战斗失败，死亡
        do return; end
        instruct_0();   --  0(0)::空语句(清屏)
    end    --:Label17

    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
    instruct_1(3789,256,0);   --  1(1):[???]说: 还有人挑战吗？还有人吗？
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3790,255,0);   --  1(1):[???]说: 好，个人赛结束，下面进行*的是团体赛。
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3791,256,0);   --  1(1):[???]说: 团体战的规则是：有仇报仇*，有冤报冤，一场定输赢，*人数无限制！
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3792,246,1);   --  1(1):[???]说: 你这是什么规则啊！
    instruct_0();   --  0(0)::空语句(清屏)

    if instruct_28(0,50,999,16,0) ==false then    --  28(1C):判断AAA品德50-999是则跳转到:Label18
        instruct_1(3794,69,0);   --  1(1):[洪七公]说: 小子，你看看你都干了些什*么！武林正道绝不容你！
        instruct_0();   --  0(0)::空语句(清屏)

        if instruct_6(134,4,0,0) ==false then    --  6(6):战斗[134]是则跳转到:Label19
            instruct_15(0);   --  15(F):战斗失败，死亡
            do return; end
            instruct_0();   --  0(0)::空语句(清屏)
        end    --:Label19

        instruct_0();   --  0(0)::空语句(清屏)
        instruct_13();   --  13(D):重新显示场景
    end    --:Label18


    if instruct_28(0,50,999,0,16) ==true then    --  28(1C):判断AAA品德50-999否则跳转到:Label20
        instruct_1(3793,60,0);   --  1(1):[欧阳锋]说: 哈哈，小子，我们单打不是*你的对手，一群人来怎么样*？
        instruct_0();   --  0(0)::空语句(清屏)

        if instruct_6(133,4,0,0) ==false then    --  6(6):战斗[133]是则跳转到:Label21
            instruct_15(0);   --  15(F):战斗失败，死亡
            do return; end
            instruct_0();   --  0(0)::空语句(清屏)
        end    --:Label21

        instruct_0();   --  0(0)::空语句(清屏)
        instruct_13();   --  13(D):重新显示场景
    end    --:Label20

    instruct_1(3795,0,1);   --  1(1):[AAA]说: 呼～终于都解决了。
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3796,256,0);   --  1(1):[???]说: 好，团体赛结束。现在我宣*布，本次大会的冠军是——**这小子！
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3797,0,1);   --  1(1):[AAA]说: ……怎么从头到尾都没人叫*我的名字啊……
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_1(3798,255,0);   --  1(1):[???]说: 请到这边领取冠军奖牌——*盟主神杖！
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_14();   --  14(E):场景变黑
    instruct_19(11,43);   --  19(13):主角移动至B-2B
    instruct_40(0);   --  40(28):改变主角站立方向0
    instruct_0();   --  0(0)::空语句(清屏)
    instruct_13();   --  13(D):重新显示场景
--end

