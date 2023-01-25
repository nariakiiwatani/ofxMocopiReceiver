# ofxMocopiReceiver

[mocopi](https://www.sony.net/Products/mocopi-dev/jp/)の[UDP送信](https://www.sony.net/Products/mocopi-dev/jp/documents/ReceiverPlugin/SendData.html)データを受信するaddonです。

mocopi持ってないんですが、ネットの情報と[BVHSender](https://www.sony.net/Products/mocopi-dev/jp/downloads/DownloadInfo.html#BVH_Sender)からのパケットを元に作ってみました。  
実際のデータで動かしていないので、動かなかったらごめんなさい。  
報告をいただけると嬉しいです。  

もしくは[mocopiを買わせてください](https://www.buymeacoffee.com/nariakiiwatani/w/11328)  
そもそも品切れ状態(2023/01/25時点)ですが。。

# 使い方

```cpp
#include "ofxMocopiReceiver.h"

ofxMocopiReceiver mocopi;

// 初期化
mocopi.setup();
// mocopi.setup(port);

// データ受信
mocopi.update();

// データ取得
const std::vector<ofNode> &skeleton = mocopi.getBones();

```

# 参考にしたサイト

- [技術仕様](https://www.sony.net/Products/mocopi-dev/jp/documents/Home/TechSpec.html)
- [mocopiの通信内容を解析してみた Vol.01](https://zenn.dev/toyoshimorioka/articles/96dbe00b87601f)
- [mocopiの通信内容を解析してみた Vol.02](https://zenn.dev/toyoshimorioka/articles/761fe45ebe4802)
- [seagetch/mcp-receiver/doc/Protocol.md](https://github.com/seagetch/mcp-receiver/blob/main/doc/Protocol.md)


# ライセンス
MIT: https://nariakiiwatani.mit-license.org