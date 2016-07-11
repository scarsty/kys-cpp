#pragma once

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

