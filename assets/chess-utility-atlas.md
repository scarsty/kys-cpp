# 整備操作圖示來源

原有生成圖集保存在專案外的 `../asset-sources/chess-preparation/chess-utility-atlas.png`，不納入 Git。原始來源為 `tmp/asset-sources/preparation-icons/utility-atlas.png`，解析度 1024×1024。

圖集為 **3 欄 × 3 列**，採用前兩列的人物、佈陣、讀檔、存檔、搜尋、調度。第三列為未採用的變體。原先以兩列切圖會跨越格線及相鄰圖示，造成存讀檔輪廓失真。

執行 `python tools/pack_chess_utility_icons.py --utility-only`，從原稿重新縮放為 192×192 無損透明 WebP（22–27），遊戲再依按鈕尺寸縮放；不放大先前的 96×96 打包檔。
