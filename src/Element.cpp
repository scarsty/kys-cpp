#include "Element.h"
#include "UISystem.h"

std::vector<Element*> Element::root_;
int Element::prev_present_ticks_ = 0;
int Element::refresh_interval_ = 25;

Element::~Element()
{
    for (auto c : childs_)
    {
        if (c && c->parent_ == this)
        {
            delete c;
        }
    }
}

void Element::drawAll()
{
    //�����һ����ռ��Ļ�ĳ�����ʼ��
    int begin_base = 0;
    for (int i = 0; i < root_.size(); i++)    //��¼���һ��ȫ���Ĳ�
    {
        root_[i]->backRun();
        root_[i]->current_frame_++;
        if (root_[i]->full_window_)
        {
            begin_base = i;
        }
    }
    for (int i = begin_base; i < root_.size(); i++)    //�����һ��ȫ���㿪ʼ��
    {
        root_[i]->drawSelfChilds();
    }
}

//����λ�ã���ı��ӽڵ��λ��
void Element::setPosition(int x, int y)
{
    for (auto c : childs_)
    {
        c->setPosition(c->x_ + x - x_, c->y_ + y - y_);
    }
    x_ = x;
    y_ = y;
}

//�ӻ��Ƶĸ��ڵ��Ƴ�
Element* Element::removeFromRoot(Element* element)
{
    if (element == nullptr)
    {
        if (!root_.empty())
        {
            element = root_.back();
            root_.pop_back();
            return element;
        }
    }
    else
    {
        for (int i = 0; i < root_.size(); i++)
        {
            if (root_[i] == element)
            {
                root_.erase(root_.begin() + i);
                return element;
                break;
            }
        }
    }
    return nullptr;
}

//����ӽڵ�
void Element::addChild(Element* element)
{
    element->setTag(childs_.size());
    if (element->parent_ == nullptr)
    {
        element->parent_ = this;
    }
    childs_.push_back(element);
}

//��ӽڵ㲢ͬʱ�����ӽڵ��λ��
void Element::addChild(Element* element, int x, int y)
{
    addChild(element);
    element->setPosition(x_ + x, y_ + y);
}

//�Ƴ�ĳ���ڵ�
void Element::removeChild(Element* element)
{
    for (int i = 0; i < childs_.size(); i++)
    {
        if (childs_[i] == element)
        {
            childs_.erase(childs_.begin() + i);
            break;
        }
    }
}

//����ӽڵ�
void Element::clearChilds()
{
    for (auto c : childs_)
    {
        delete c;
    }
    childs_.clear();
}

//����������ӽڵ�
void Element::drawSelfChilds()
{
    if (visible_)
    {
        draw();
        for (auto c : childs_)
        {
            if (c->visible_)
            {
                c->drawSelfChilds();
            }
        }
    }
}

void Element::setAllChildState(int s)
{
    for (auto c : childs_)
    {
        c->state_ = s;
    }
}

void Element::setAllChildVisible(bool v)
{
    for (auto c : childs_)
    {
        c->visible_ = v;
    }
}

int Element::findNextVisibleChild(int i0, int direct)
{
    if (direct == 0 || childs_.size() == 0)
    {
        return i0;
    }
    direct = direct > 0 ? 1 : -1;

    int i1 = i0;
    for (int i = 1; i < childs_.size(); i++)
    {
        i1 += direct;
        i1 = (i1 + childs_.size()) % childs_.size();
        if (childs_[i1]->visible_)
        {
            return i1;
        }
    }
    return i0;
}

int Element::findFristVisibleChild()
{
    for (int i = 0; i < childs_.size(); i++)
    {
        if (childs_[i]->visible_)
        {
            return i;
        }
    }
    return -1;
}

void Element::checkFrame()
{
    if (stay_frame_ > 0 && current_frame_ >= stay_frame_)
    {
        exit_ = true;
    }
}

//����������¼���Ӧ
//ֻ����ǰ�Ľڵ�͵�ǰ�ڵ���ӽڵ㣬�������Ƿ��ڷ�Χ��
//ע��ȫ����Ľڵ�Ҫһֱ�����¼�
void Element::checkStateSelfChilds(BP_Event& e, bool check_event)
{
    //if (exit_) { return; }
    if (visible_ || full_window_)
    {
        //ע�������Ƿ���
        for (int i = childs_.size() - 1; i >= 0; i--)
        {
            childs_[i]->checkStateSelfChilds(e, check_event);
        }
        if (check_event)
        {
            checkSelfState(e);
            checkChildState();
            //������dealEvent�иı�ԭ��״̬��ǿ������ĳЩ���
            dealEvent(e);
            //Ϊ�򻯴��룬�����»س���ESC�Ĳ���д�ڴ˴�
            if (isPressOK(e))
            {
                onPressedOK();
                setAllChildState(Normal);
            }
            if (isPressCancel(e))
            {
                onPressedCancel();
                setAllChildState(Normal);
            }
        }
        dealEvent2(e);
    }
    else
    {
        state_ = Normal;
    }
}

void Element::backRunSelfChilds()
{
    for (auto c : childs_)
    {
        c->backRunSelfChilds();
    }
    backRun();
}

//����¼�
void Element::dealEventSelfChilds(bool check_event)
{
    if (check_event)
    {
        BP_Event e;
        e.type = BP_FIRSTEVENT;
        while (Engine::pollEvent(e))
        {
            //һЩ��������ǰֹͣ���ⶪ��
            if (e.type == BP_MOUSEBUTTONDOWN || e.type == BP_MOUSEBUTTONUP || e.type == BP_KEYDOWN || e.type == BP_KEYUP)
            {
                break;
            }
        }
        //Engine::pollEvent(e);
        //if (e.type == BP_MOUSEBUTTONUP)
        //{
        //    printf("BP_MOUSEBUTTONUP\n");
        //}
        checkStateSelfChilds(e, check_event);
        switch (e.type)
        {
        case BP_QUIT:
            UISystem::askExit(1);
            break;
        default:
            break;
        }
    }
    else
    {
        Engine::pollEvent();
    }
}

void Element::forceActiveChild()
{
    for (int i = 0; i < childs_.size(); i++)
    {
        childs_[i]->setState(Normal);
        if (i == active_child_)
        {
            childs_[i]->setState(Pass);
        }
    }
}

void Element::checkChildState()
{
    //ע��active�ǲ��ĵģ�ά����һ�ε�״̬
    //��ȡ�ӽڵ��״̬
    for (int i = 0; i < getChildCount(); i++)
    {
        if (getChild(i)->getState() != Normal)
        {
            active_child_ = i;
        }
    }
}

void Element::checkSelfState(BP_Event& e)
{
    //�����꾭�������µ�״̬
    //ע��BP_MOUSEMOTION��mac������Щ���⣬����
    if (e.type == BP_MOUSEMOTION)
    {
        if (inSide(e.motion.x, e.motion.y))
        {
            state_ = Pass;
            if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT))
            {
                state_ = Press;
            }
        }
        else
        {
            state_ = Normal;
        }
    }
    if ((e.type == BP_MOUSEBUTTONDOWN || e.type == BP_MOUSEBUTTONUP) && e.button.button == BP_BUTTON_LEFT)
    {
        if (inSide(e.button.x, e.button.y))
        {
            state_ = Press;
        }
        else
        {
            state_ = Normal;
        }
    }
    if ((e.type == BP_KEYDOWN || e.type == BP_KEYUP) && (e.key.keysym.sym == BPK_RETURN || e.key.keysym.sym == BPK_SPACE))
    {
        //���¼��̵Ŀո���߻س�ʱ����pass�İ�����Ϊpress
        if (state_ == Pass)
        {
            state_ = Press;
        }
    }
}

void Element::present()
{
    int t1 = Engine::getTicks();
    int t = refresh_interval_ - (t1 - prev_present_ticks_);
    if (t > refresh_interval_)
    {
        t = refresh_interval_;
    }
    if (t > 0)
    {
        Engine::delay(t);
    }
    prev_present_ticks_ = t1;
    Engine::getInstance()->renderPresent();
}

//���б��ڵ㣬����Ϊ�Ƿ���root�����У�Ϊ���������ƣ�Ϊ���򲻻ᱻ����
int Element::run(bool in_root /*= true*/)
{
    exit_ = false;
    visible_ = true;
    if (in_root)
    {
        addOnRootTop(this);
    }
    onEntrance();
    running_ = true;
    while (!exit_)
    {
        if (root_.empty())
        {
            break;
        }
        dealEventSelfChilds(true);
        drawAll();
        checkFrame();
        present();
    }
    running_ = false;
    onExit();
    if (in_root)
    {
        removeFromRoot(this);
    }
    return result_;
}

void Element::exitAll(int begin)
{
    for (int i = begin; i < root_.size(); i++)
    {
        root_[i]->exit_ = true;
        root_[i]->visible_ = false;
    }
}

//ר������ĳЩ����¶�������ʾ����ʱ
//�м���Բ���һ����������Щʲô���벻�����õķ�����
int Element::drawAndPresent(int times, std::function<void(void*)> func, void* data)
{
    if (times < 1)
    {
        return 0;
    }
    if (times > 100)
    {
        times = 100;
    }
    for (int i = 0; i < times; i++)
    {
        dealEventSelfChilds(false);
        drawAll();
        if (func)
        {
            func(data);
        }
        present();
        if (exit_)
        {
            break;
        }
    }
    return times;
}
