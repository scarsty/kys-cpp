#pragma once

//这些宏仅为了在事件程序中简化代码，不要用在其他地方
#define VOID_INSTRUCT_0(num, e, i, content) { case (num): content(); i += 1; break; }
#define VOID_INSTRUCT_1(num, e, i, content) { case (num): content(e[i+1]); i += 2; break; }
#define VOID_INSTRUCT_2(num, e, i, content) { case (num): content(e[i+1],e[i+2]); i += 3; break; }
#define VOID_INSTRUCT_3(num, e, i, content) { case (num): content(e[i+1],e[i+2],e[i+3]); i += 4; break; }
#define VOID_INSTRUCT_4(num, e, i, content) { case (num): content(e[i+1],e[i+2],e[i+3],e[i+4]); i += 5; break; }
#define VOID_INSTRUCT_5(num, e, i, content) { case (num): content(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5]); i += 6; break; }
#define VOID_INSTRUCT_6(num, e, i, content) { case (num): content(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5],e[i+6]); i += 7; break; }
#define VOID_INSTRUCT_7(num, e, i, content) { case (num): content(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5],e[i+6],e[i+7]); i += 8; break; }
#define VOID_INSTRUCT_8(num, e, i, content) { case (num): content(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5],e[i+6],e[i+7],e[i+8]); i += 9; break; }
#define VOID_INSTRUCT_9(num, e, i, content) { case (num): content(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5],e[i+6],e[i+7],e[i+8],e[i+9];) i += 10; break; }
#define VOID_INSTRUCT_10(num, e, i, content) { case (num): content(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5],e[i+6],e[i+7],e[i+8],e[i+9],e[i+10]); i += 11; break; }
#define VOID_INSTRUCT_11(num, e, i, content) { case (num): content(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5],e[i+6],e[i+7],e[i+8],e[i+9],e[i+10],e[i+11]); i += 12; break; }
#define VOID_INSTRUCT_12(num, e, i, content) { case (num): content(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5],e[i+6],e[i+7],e[i+8],e[i+9],e[i+10],e[i+11],e[i+12]); i += 13; break; }
#define VOID_INSTRUCT_13(num, e, i, content) { case (num): content(e[i+1],e[i+2],e[i+3],e[i+4],e[i+5],e[i+6],e[i+7],e[i+8],e[i+9],e[i+10],e[i+11],e[i+12],e[i+13]); i += 14; break; }

#define BOOL_INSTRUCT_0(num, e, i, content) { case (num): if (content()) {i += e[i+1];} else {i+= e[i+2];} break; }
#define BOOL_INSTRUCT_1(num, e, i, content) { case (num): if (content(e[i+1])) {i += e[i+2];} else {i+= e[i+3];} break; }
#define BOOL_INSTRUCT_2(num, e, i, content) { case (num): if (content(e[i+1],e[i+2])) {i += e[i+3];} else {i+= e[i+4];} break; }
#define BOOL_INSTRUCT_3(num, e, i, content) { case (num): if (content(e[i+1],e[i+2],e[i+3])) {i += e[i+4];} else {i+= e[i+5];} break; }

//指令对应宏，仅参考，不使用
#define instruct_0
#define instruct_1 oldTalk
#define instruct_2 getItem
#define instruct_3 modifyEvent
#define instruct_4 isUsingItem
#define instruct_5 askBattle
#define instruct_6 tryBattle
#define instruct_7
#define instruct_8 changeMainMapMusic
#define instruct_9 askJoin

#define instruct_10 join
#define instruct_11 askRest
#define instruct_12 rest
#define instruct_13 lightScence
#define instruct_14 darkScence
#define instruct_15 dead
#define instruct_16 inTeam
#define instruct_17 setSubMapMapData
#define instruct_18 haveItemBool
#define instruct_19 oldSetScencePosition

#define instruct_20 teamIsFull
#define instruct_21 leaveTeam
#define instruct_22 zeroAllMP
#define instruct_23 setOneUsePoi
#define instruct_24 blank
#define instruct_25 submapFromTo
#define instruct_26 add3EventNum
#define instruct_27 playAnimation
#define instruct_28 judgeEthics
#define instruct_29 judgeAttack

#define instruct_30 walkFromTo
#define instruct_31 judgeMoney
#define instruct_32 getItemWithoutHint
#define instruct_33 oldLearnMagic
#define instruct_34 addAptitude
#define instruct_35 setOneMagic
#define instruct_36 judgeSexual
#define instruct_37 addEthics
#define instruct_38 changeScencePic
#define instruct_39 openScence

#define instruct_40 setRoleFace
#define instruct_41 roleGetItem
#define instruct_42 judgeFemaleInTeam
#define instruct_43 haveItemBool
#define instruct_44 play2Amination
#define instruct_45 addSpeed
#define instruct_46 addMP
#define instruct_47 addAttack
#define instruct_48 addHP
#define instruct_49 setMPPro

#define instruct_50 judge5Item
#define instruct_51 askSoftStar
#define instruct_52 showEthics
#define instruct_53 showRepute
#define instruct_54 openAllScence
#define instruct_55 judgeEventNum
#define instruct_56 addRepute
#define instruct_57 breakStoneGate
#define instruct_58 fightForTop
#define instruct_59 allLeave

#define instruct_60 judgeSubMapPic
#define instruct_61 judge14BooksPlaced
#define instruct_62 backHome
#define instruct_63 setSexual
#define instruct_64 weiShop
#define instruct_65
#define instruct_66 playMusic
#define instruct_67 playWave


