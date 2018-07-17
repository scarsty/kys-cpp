#include "GameUtil.h"
#include "Random.h"
#include "Scene.h"
#include <queue>

Scene::Scene()
{
}

Scene::~Scene()
{
}

void Scene::calViewRegion()
{
    Engine::getInstance()->getMainTextureSize(render_center_x_, render_center_y_);
    render_center_x_ /= 2;
    render_center_y_ /= 2;
    view_width_region_ = render_center_x_ / TILE_W / 2 + 3;
    view_sum_region_ = render_center_y_ / TILE_H + 2;

    //Engine::getInstance()->getPresentSize(window_center_x_, window_center_y_);
    //window_center_x_ /= 2;
    //window_center_y_ /= 2;
}

void Scene::checkWalk(int x, int y, BP_Event& e)
{
}

//�������������ǵ�ǰ��Ļ����λ�õ���Ϸ���꣬ͨ�����˵�����
Point Scene::getPositionOnRender(int x, int y, int view_x, int view_y)
{
    Point p;
    x = x - view_x;
    y = y - view_y;
    p.x = -y * TILE_W + x * TILE_W + render_center_x_;
    p.y = y * TILE_H + x * TILE_H + render_center_y_;
    return p;
}

//������������ͬ�ϣ�һЩ����´��ڳߴ����Ⱦ�ߴ粻ͬ
Point Scene::getPositionOnWindow(int x, int y, int view_x, int view_y)
{
    auto p = getPositionOnRender(x, y, view_x, view_y);
    int w, h;
    Engine::getInstance()->getWindowSize(w, h);
    p.x = p.x * w / render_center_x_ / 2;
    p.y = p.y * h / render_center_y_ / 2;
    return p;
}

//��ɫ����x1��y1������x2��y2ʱ�����ķ���
int Scene::calTowards(int x1, int y1, int x2, int y2)
{
    int d1, d2, dm;
    d1 = y2 - y1;
    d2 = x2 - x1;
    dm = abs(d1) - abs(d2);
    if ((d1 != 0) || (d2 != 0))
    {
        if (dm >= 0)
        {
            if (d1 < 0)
            {
                return Towards_RightUp;
            }
            else
            {
                return Towards_LeftDown;
            }
        }
        else
        {
            if (d2 < 0)
            {
                return Towards_LeftUp;
            }
            else
            {
                return Towards_RightDown;
            }
        }
    }
    return Towards_None;
}

void Scene::changeTowardsByKey(BP_Keycode key)
{
    int tw = getTowardsByKey(key);
    if (tw != Towards_None)
    {
        towards_ = tw;
    }
}

int Scene::getTowardsByKey(BP_Keycode key)
{
    int tw = Towards_None;
    switch (key)
    {
    case BPK_LEFT:
        tw = Towards_LeftUp;
        break;
    case BPK_RIGHT:
        tw = Towards_RightDown;
        break;
    case BPK_UP:
        tw = Towards_RightUp;
        break;
    case BPK_DOWN:
        tw = Towards_LeftDown;
        break;
    }
    return tw;
}

int Scene::getTowardsByMouse(int mouse_x, int mouse_y)
{
    int w, h;
    Engine::getInstance()->getWindowSize(w, h);
    mouse_x = mouse_x * render_center_x_ * 2 / w;
    mouse_y = mouse_y * render_center_y_ * 2 / h;
    if (mouse_x < render_center_x_ && mouse_y < render_center_y_)
    {
        return Towards_LeftUp;
    }
    if (mouse_x < render_center_x_ && mouse_y > render_center_y_)
    {
        return Towards_LeftDown;
    }
    if (mouse_x > render_center_x_ && mouse_y < render_center_y_)
    {
        return Towards_RightUp;
    }
    if (mouse_x > render_center_x_ && mouse_y > render_center_y_)
    {
        return Towards_RightDown;
    }
    return Towards_None;
}

void Scene::getTowardsPosition(int x0, int y0, int tw, int* x1, int* y1)
{
    if (tw == Towards_None)
    {
        return;
    }
    *x1 = x0;
    *y1 = y0;
    switch (tw)
    {
    case Towards_LeftUp:
        (*x1)--;
        break;
    case Towards_RightDown:
        (*x1)++;
        break;
    case Towards_RightUp:
        (*y1)--;
        break;
    case Towards_LeftDown:
        (*y1)++;
        break;
    }
}

//������λ�÷��Ƴ�����Ϸ��ͼ�ϵ�����
Point Scene::getMousePosition(int mouse_x, int mouse_y, int view_x, int view_y)
{
    int w, h;
    Engine::getInstance()->getWindowSize(w, h);
    double mouse_x1 = mouse_x * render_center_x_ * 2.0 / w;
    double mouse_y1 = mouse_y * render_center_y_ * 2.0 / h;

    //mouse_x1 += TILE_W;
    mouse_y1 += TILE_H * 2;

    Point p;
    p.x = ((mouse_x1 - render_center_x_) / TILE_W + (mouse_y1 - render_center_y_) / TILE_H) / 2 + view_x;
    p.y = ((-mouse_x1 + render_center_x_) / TILE_W + (mouse_y1 - render_center_y_) / TILE_H) / 2 + view_y;
    return p;
}

Point Scene::getMousePosition(int view_x, int view_y)
{
    int mouse_x, mouse_y;
    Engine::getInstance()->getMouseState(mouse_x, mouse_y);
    return getMousePosition(mouse_x, mouse_y, view_x, view_y);
}

void Scene::calCursorPosition(int x, int y)
{
    //����λ��
    auto p = getMousePosition(x, y);
    cursor_x_ = p.x;
    cursor_y_ = p.y;
}

//A*
void Scene::FindWay(int Mx, int My, int Fx, int Fy)
{
    struct PointAStar : public Point
    {
        PointAStar() {}
        PointAStar(int _x, int _y)
            : Point(_x, _y)
        {
        }
        ~PointAStar() {}

    private:
        int step = 0, f = 0;
        PointAStar* parent = nullptr;

    public:
        //g: step, h: distance between current and final points
        //controlled by coefficients
        void calF(int Fx, int Fy) { f = step + 2 * (abs(x - Fx) + abs(y - Fy)); }
        PointAStar* getParent() { return parent; }
        void setParent(PointAStar* p)
        {
            parent = p;
            step = p->step + 1;
        }
        class Compare
        {
        public:
            bool operator()(PointAStar* point1, PointAStar* point2) { return point1->f > point2->f; }
        };
    };

    way_que_.clear();
    std::map<std::pair<int, int>, PointAStar> point_map;                                   //�Ѿ����ʹ��ĵ��ָ��(�ر��б�)
    Point dirs[4] = { { 1, 0 }, { 0, -1 }, { 0, 1 }, { -1, 0 } };                           //�ĸ�����
    std::priority_queue<PointAStar*, std::vector<PointAStar*>, PointAStar::Compare> que;    //��С���ȼ�����(�����б�)
    //RandomDouble rand;

    auto begin_point = PointAStar(Mx, My);
    begin_point.calF(Fx, Fy);
    point_map[{ Mx, My }] = begin_point;
    que.push(&begin_point);

    int s_num = 0;
    while (!que.empty() && s_num <= 4096)
    {
        auto t = que.top();    //ѡ��Ŀǰ��õĵ�
        que.pop();
        s_num++;
        if (t->x == Fx && t->y == Fy)
        {
            way_que_.push_back(*t);
            while (t->getParent())
            {
                way_que_.push_back(*t);
                t = t->getParent();
            }
            break;
        }
        else
        {
            for (int i = 0; i < 4; i++)
            {
                int x = t->x + dirs[i].x, y = t->y + dirs[i].y;
                if ((canWalk(x, y) || x == Fx && y == Fy) && point_map.find({ x, y }) == point_map.end())
                {
                    point_map[{ x, y }] = PointAStar(x, y);
                    auto& point_ref = point_map[{ x, y }];
                    point_ref.setParent(t);
                    point_ref.calF(Fx, Fy);
                    que.push(&point_ref);
                }
            }
        }
    }
    printf("Found a way in %d times, %d steps\n", s_num, way_que_.size());
}

void Scene::lightScene()
{
    for (int i = 10; i <= 0; i--)
    {
        auto fill = [&](void*) -> void
        {
            uint8_t alpha = GameUtil::limit(i * 25, 0, 255);
            Engine::getInstance()->fillColor({ 0, 0, 0, alpha }, 0, 0, -1, -1);
        };
        drawAndPresent(1, fill);
    }
}

void Scene::darkScene()
{
    for (int i = 0; i <= 10; i++)
    {
        auto fill = [&](void*) -> void
        {
            uint8_t alpha = GameUtil::limit(i * 25, 0, 255);
            Engine::getInstance()->fillColor({ 0, 0, 0, alpha }, 0, 0, -1, -1);
        };
        drawAndPresent(1, fill);
    }
}

bool Scene::isOutLine(int x, int y)
{
    return (x < 0 || x >= COORD_COUNT || y < 0 || y >= COORD_COUNT);
}
