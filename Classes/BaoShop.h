#pragma once

#include "cocos2d.h"
class BaoShop
{
public:
	BaoShop();
	~BaoShop();

	struct TShopItem
	{
		short INum, Amount;
	};

	TShopItem ShopItem[18];
	
};

