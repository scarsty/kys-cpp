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

	TShopItem m_ShopItem[18];
	
};

