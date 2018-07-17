#pragma once
#include "BattleCursor.h"
#include "BattleMap.h"
#include "BattleMenu.h"
#include "Point.h"
#include "Random.h"
#include "Scene.h"

class BattleScene : public Scene
{
private:
    Save* save_;
    RandomDouble rand_;

public:
    BattleScene();
    BattleScene(int id);
    ~BattleScene();

    std::vector<Role*> battle_roles_;
    std::vector<Role*> friends_;    //���濪ʼ�Ͳ�ս�������������ʧ�ܾ���

    BattleActionMenu battle_menu_;
	BattleCursor battle_cursor_;
    Head* head_self_;

    //Head* head_selected_;

    int battle_id_ = 0;
    BattleInfo* info_;
    void setID(int id);

    //����㣬�����㣬ѡ��㣨��ֵΪ����ѡ��0����ֵΪ��ѡ��
    MapSquareInt earth_layer_, building_layer_, select_layer_, effect_layer_;

    //��ɫ��
	MapSquare<Role*> role_layer_;

    int select_state_ = 0;    //0-������1-ѡ�ƶ�Ŀ�꣬2-ѡ�ж�Ŀ��

    int select_x_ = 0, select_y_ = 0;
    void setSelectPosition(int x, int y)
    {
        select_x_ = x;
        select_y_ = y;
    }

    //���»�ͼ��
    int action_frame_ = 0;
    int action_type_ = -1;
    int show_number_y_ = 0;
    int effect_id_ = -1;
    int effect_frame_ = 0;
    uint8_t dead_alpha_ = 255;
    const int animation_delay_ = 2;

    bool fail_exp_ = false;

    void setHaveFailExp(bool b) { fail_exp_ = b; }    //�Ƿ�����Ҳ�о���

    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;     //ս����ѭ��
    virtual void dealEvent2(BP_Event& e) override;    //����ֹͣ�Զ�
    virtual void onEntrance() override;
    virtual void onExit() override;
    virtual void backRun() override;

    void readBattleInfo();                                          //��ȡս�����������
    void setRoleInitState(Role* r);                                 //��ʼ�����������
    void setFaceTowardsNearest(Role* r, bool in_effect = false);    //���������ĵ����������򣬲���Ϊ�Ƿ�������Լ��ж�Ч����
    void readFightFrame(Role* r);                                   //��ȡ�����ж�֡��
    void sortRoles();                                               //��ɫ����
    static bool compareRole(Role* r1, Role* r2);                    //��ɫ����Ĺ���
    void resetRolesAct();                                           //����������δ�ж���
    int calMoveStep(Role* r);                                       //������ƶ�����(����װ��)
    int calActionStep(int ability) { return ability / 15 + 1; }     //��������ֵ�����ж��ķ�Χ����
    int calRolePic(Role* r, int style = -1, int frame = 0);         //���ݶ���֡�������ɫ����ͼ���

    //������Ա�ѡ��ķ�Χ�����дѡ���
    //mode���壺0-�ƶ����ܲ������ϰ�Ӱ�죻1�����ö�ҽ�ƵȽ��ܲ���Ӱ�죻2�鿴״̬��ȫ����ѡ��3����ѡֱ�ߵĸ��ӣ�4����ѡ�Լ�
    void calSelectLayer(Role* r, int mode, int step = 0);
    void calSelectLayer(int x, int y, int team, int mode, int step = 0);

    //������ѧ��ѡ��ķ�Χ
    void calSelectLayerByMagic(int x, int y, int team, Magic* magic, int level_index);

    //����Ч���㣬x��y������rΪѡ������ĵ㣬�������ڵ�λ��
    void calEffectLayer(Role* r, int select_x, int select_y, Magic* m = nullptr, int level_index = 0) { calEffectLayer(r->X(), r->Y(), select_x, select_y, m, level_index); }
    void calEffectLayer(int x, int y, int select_x, int select_y, Magic* m = nullptr, int level_index = 0);

    //���������Ƿ���Ч��
    bool haveEffect(int x, int y) { return effect_layer_.data(x, y) >= 0; }

    //r2�ǲ�����Ч�������棬�һᱻr1��Ч������
    bool inEffect(Role* r1, Role* r2);

    bool canSelect(int x, int y);

    void walk(Role* r, int x, int y, Towards t);

    virtual bool canWalk(int x, int y) override;
    bool isBuilding(int x, int y);
    bool isWater(int x, int y);
    bool isRole(int x, int y);
    bool isOutScreen(int x, int y) override;
    bool isNearEnemy(int team, int x, int y);    //�Ƿ�x��y�ϵ�������team��һ��

    //�������
    int calRoleDistance(Role* r1, Role* r2) { return calDistance(r1->X(), r1->Y(), r2->X(), r2->Y()); }
    int calDistanceRound(int x1, int x2, int y1, int y2) { return sqrt((x1 - y1) * (x1 - y1) + (x2 - y2) * (x2 - y2)); }
    Role* getSelectedRole();    //��ȡǡ����ѡ���Ľ�ɫ

    void action(Role* r);    //�ж�����

    void actMove(Role* r);
    void actUseMagic(Role* r);
    void actUsePoison(Role* r);
    void actDetoxification(Role* r);
    void actMedcine(Role* r);
    void actUseHiddenWeapon(Role* r);
    void actUseDrag(Role* r);
    void actWait(Role* r);
    void actStatus(Role* r);
    void actAuto(Role* r);
    void actRest(Role* r);

    void moveAnimation(Role* r, int x, int y);                                 //�ƶ�����
    void useMagicAnimation(Role* r, Magic* m);                                 //ʹ����ѧ����
    void actionAnimation(Role* r, int style, int effect_id, int shake = 0);    //�ж�����
    void showMagicName(std::string name);

    int calMagicHurt(Role* r1, Role* r2, Magic* magic);
    int calMagiclHurtAllEnemies(Role* r, Magic* m, bool simulation = false);    //����ȫ��������˺�
    int calHiddenWeaponHurt(Role* r1, Role* r2, Item* item);

    void showNumberAnimation();
    void clearDead();
    void poisonEffect(Role* r);    //�ж�Ч��

    int getTeamMateCount(int team);    //��ȡ��Ա��Ŀ
    int checkResult();
    void calExpGot();    //���㾭��
};