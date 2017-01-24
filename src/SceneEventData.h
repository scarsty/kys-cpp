#include "config.h"

#pragma once
class SceneEventData
{
public:
	SceneEventData();
	~SceneEventData();

	struct TSceneEvent
	{
		short CanWalk, Num, Event1, Event2, Event3, BeginPic1, EndPic, BeginPic2, PicDelay, XPos, YPos, StartTime, Duration, Interval, DoneTime,
			IsActive, OutTime, OutEvent;
	};
	TSceneEvent Data[config::PerSceneMaxEvent];

};

