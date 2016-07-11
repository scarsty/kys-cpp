#include "Collector.h"

Collector::Collector():
	m_time(1.0f)
{
	m_callback = [](){};
}

Collector* Collector::create(){
	Collector* ret = new Collector();
	if(ret && ret->init()){
		ret->autorelease();
		return ret;
	}
	CC_SAFE_DELETE(ret);
	return nullptr;
}

bool Collector::init(){
	if(!Node::init())	return false;
	this->scheduleUpdate();
	return true;
}

void Collector::addSource(Node* node){
	MoveTo* moveTo = MoveTo::create(m_time,this->getPosition());
	EaseBackInOut* easeMove = EaseBackInOut::create(moveTo);
	
	ScaleTo* scaleBig = ScaleTo::create(0.3*m_time,1.5f);
	ScaleTo* scaleSmall = ScaleTo::create(0.7*m_time,0.5f);
	Sequence* scale = Sequence::create(scaleBig,scaleSmall,NULL);
	
	Spawn* move = Spawn::create(easeMove,scale,NULL);

	CallFunc* callFunc = CallFunc::create([=](){
		node->removeFromParentAndCleanup(true);
		m_callback();
	});

	Sequence* action = Sequence::create(move,callFunc,NULL);
	node->runAction(action);
}

void Collector::setCallback(std::function<void()> callback){
	m_callback = callback;
}

void Collector::setFloatTime(float time){
	m_time = time;
}

void Collector::update(float delta){

}

