#pragma once
#include "BattleCursor.h"
#include "BattleMap.h"
#include "BattleMenu.h"
#include "Point.h"
#include "Random.h"
#include "Scene.h"

class BattleScene : public Scene
{
public:
    BattleScene();
    BattleScene(int id);
    virtual ~BattleScene();
    void setID(int id);

    //�̳��Ի���ĺ���
    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;     //ս����ѭ��
    virtual void dealEvent2(BP_Event& e) override;    //����ֹͣ�Զ�
    virtual void onEntrance() override;
    virtual void onExit() override;
    virtual void backRun() override;

protected:
    int battle_id_ = 0;
    BattleInfo* info_;

    Save* save_;
    RandomDouble rand_;

    std::vector<Role*> battle_roles_;    //���в�ս��ɫ
    std::vector<Role*> friends_;         //��ʼ�Ͳ�ս���ҷ���ɫ����������ʧ�ܾ���
    Role* acting_role_ = nullptr;        //��ǰ�����ж��еĽ�ɫ

    BattleActionMenu* battle_menu_;    //ս���ж��˵�
    BattleCursor* battle_cursor_;      //ս��ʱ�Ĺ��
    Head* head_self_;                  //ͷ��

    //����㣬�����㣬ѡ��㣨��ֵΪ����ѡ��0����ֵΪ��ѡ����Ч����
    MapSquareInt *earth_layer_, *building_layer_, *select_layer_, *effect_layer_;

    //��ɫ��
    MapSquare<Role*>* role_layer_;

    int select_type_ = 0;    //0-������1-ѡ�ƶ�Ŀ�꣬2-ѡ�ж�Ŀ�꣨����û���õ���

    //���ѡ���λ��
    int select_x_ = 0, select_y_ = 0;

    //���»�ͼ��
    int action_frame_ = 0;
    int action_type_ = -1;
    int show_number_y_ = 0;
    int effect_id_ = -1;
    int effect_frame_ = 0;
    uint8_t dead_alpha_ = 255;
    static const int animation_delay_ = 2;

    bool fail_exp_ = false;    //����Ƿ��о���

    int semi_real_ = 0;    //�Ƿ�뼴ʱ

public:
    void setSelectPosition(int x, int y)    //����ѡ�������
    {
        select_x_ = x;
        select_y_ = y;
    }

    const std::vector<Role*>& getBattleRoles() { return battle_roles_; }
    MapSquare<Role*>*& getRoleLayer() { return role_layer_; }
    int selectX() { return select_x_; }
    int selectY() { return select_y_; }

    virtual void setHaveFailExp(bool b) { fail_exp_ = b; }    //�Ƿ�����Ҳ�о���

    virtual void readBattleInfo();                                          //��ȡս�����������
    virtual void setRoleInitState(Role* r);                                 //��ʼ�����������
    virtual void setFaceTowardsNearest(Role* r, bool in_effect = false);    //���������ĵ����������򣬲���Ϊ�Ƿ�������Լ��ж�Ч����
    virtual void readFightFrame(Role* r);                                   //��ȡ�����ж�֡��
    virtual void sortRoles();                                               //��ɫ����
    virtual void resetRolesAct();                                           //����������δ�ж���
    virtual int calMoveStep(Role* r);                                       //������ƶ�����(����װ��)
    virtual int calActionStep(int ability) { return ability / 15 + 1; }     //��������ֵ�����ж��ķ�Χ����
    virtual int calRolePic(Role* r, int style = -1, int frame = 0);         //���ݶ���֡�������ɫ����ͼ���

    //������Ա�ѡ��ķ�Χ�����дѡ���
    //mode���壺0-�ƶ����ܲ������ϰ�Ӱ�죻1�����ö�ҽ�ƵȽ��ܲ���Ӱ�죻2�鿴״̬��ȫ����ѡ��3����ѡֱ�ߵĸ��ӣ�4����ѡ�Լ�
    virtual void calSelectLayer(Role* r, int mode, int step = 0);
    virtual void calSelectLayer(int x, int y, int team, int mode, int step = 0);

    //������ѧ��ѡ��ķ�Χ
    virtual void calSelectLayerByMagic(int x, int y, int team, Magic* magic, int level_index);

    //����Ч���㣬x��y������rΪѡ������ĵ㣬�������ڵ�λ��
    virtual void calEffectLayer(Role* r, int select_x, int select_y, Magic* m = nullptr, int level_index = 0) { calEffectLayer(r->X(), r->Y(), select_x, select_y, m, level_index); }
    virtual void calEffectLayer(int x, int y, int select_x, int select_y, Magic* m = nullptr, int level_index = 0);

    //���������Ƿ���Ч��
    virtual bool haveEffect(int x, int y) { return effect_layer_->data(x, y) >= 0; }

    //r2�ǲ�����Ч�������棬�һᱻr1��Ч������
    virtual bool inEffect(Role* r1, Role* r2);

    virtual bool canSelect(int x, int y);

    virtual void walk(Role* r, int x, int y, Towards t);

    virtual bool canWalk(int x, int y) override;
    virtual bool isBuilding(int x, int y);
    virtual bool isWater(int x, int y);
    virtual bool isRole(int x, int y);
    virtual bool isOutScreen(int x, int y) override;
    virtual bool isNearEnemy(int team, int x, int y);    //�Ƿ�x��y�ϵ�������team��һ��

    virtual int calRoleDistance(Role* r1, Role* r2) { return calDistance(r1->X(), r1->Y(), r2->X(), r2->Y()); }                     //�������
    virtual int calDistanceRound(int x1, int x2, int y1, int y2) { return sqrt((x1 - y1) * (x1 - y1) + (x2 - y2) * (x2 - y2)); }    //����ŷ�Ͼ���

    virtual Role* getSelectedRole();    //��ȡǡ����ѡ���Ľ�ɫ

    virtual void action(Role* r);    //�ж�����

    virtual void actMove(Role* r);               //�ƶ�
    virtual void actUseMagic(Role* r);           //��ѧ
    virtual void actUsePoison(Role* r);          //�ö�
    virtual void actDetoxification(Role* r);     //�ⶾ
    virtual void actMedicine(Role* r);            //ҽ��
    virtual void actUseHiddenWeapon(Role* r);    //����
    virtual void actUseDrag(Role* r);            //��ҩ
    virtual void actWait(Role* r);               //�ȴ�
    virtual void actStatus(Role* r);             //״̬
    virtual void actAuto(Role* r);               //�Զ�
    virtual void actRest(Role* r);               //��Ϣ

    virtual void moveAnimation(Role* r, int x, int y);                                 //�ƶ�����
    virtual void useMagicAnimation(Role* r, Magic* m);                                 //ʹ����ѧ����
    virtual void actionAnimation(Role* r, int style, int effect_id, int shake = 0);    //�ж�����

    virtual int calMagicHurt(Role* r1, Role* r2, Magic* magic);                         //������ѧ�Ե��˵��˺�
    virtual int calMagiclHurtAllEnemies(Role* r, Magic* m, bool simulation = false);    //����ȫ��������˺�

    virtual int calHiddenWeaponHurt(Role* r1, Role* r2, Item* item);    //���㰵���˺�

    virtual void showMagicName(std::string name);                                           //��ʾ��ѧ��
    virtual void showNumberAnimation(int delay = animation_delay_, bool floating = true);   //��ʾ����
    virtual void clearDead();                                                               //��������˵Ľ�ɫ
    virtual void poisonEffect(Role* r);                                                     //�ж�Ч��

    virtual int getTeamMateCount(int team);    //��ȡ��Ա��Ŀ
    virtual int checkResult();                 //�����
    virtual void calExpGot();                  //���㾭��
};
