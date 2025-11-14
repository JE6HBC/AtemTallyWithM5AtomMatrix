# AtemTallyWithM5AtomMatrix working with Bitfocus Companion
# ATEM M5Stack ワイヤレスタリーシステム (Companion 統合 / GUI-less 版) `README.md`

## 1\. 概要 (Project Overview)

本システムは、Blackmagic Design ATEMスイッチャーとM5Stack ATOM MatrixをWi-Fiで連携させる、ワイヤレスのタリーランプシステムです。

最大の特徴は、システムの「頭脳」として**Bitfocus Companion**を採用し、**PC上の専用GUIアプリを一切不要**にしている点です。

Stream Deckの物理ボタンがATEM制御とタリー制御（CALL機能）を統合的に行い、各M5Stack（タリーランプ）は、それ自体に搭載された物理ボタンによって自分のカメラ番号を記憶します。

### 主な機能

  * **統合されたATEM制御:** Stream Deckからカット、マクロ、キー操作などを実行。
  * **物理ボタンによるCALL:** Stream Deckから任意のカメラマンへ「CALL」信号を送信。
  * **手元タリー:** Stream Deckのボタン自体が、ATEMのタリー状態と連動して赤・緑に点灯。
  * **GUIアプリ不要:** M5Stackのカメラ番号は、本体のボタン操作で設定・保存されます。

-----

## 2\. システムアーキテクチャ

システムは「Companion」「MQTTブローカー」「M5Stack」の3要素で完結します。

```mermaid
graph LR
    subgraph "Control Surface"
        SD[Stream Deck]
    end
    
    subgraph "Studio Network (PC)"
        Comp[Bitfocus Companion]
        MQTT[Mosquitto Broker]
        ATEM[ATEM Swer]
    end

    subgraph "On-Camera Units (Wi-Fi)"
        M5_1[M5Stack (Cam 1)]
        M5_2[M5Stack (Cam 2)]
        M5_N[M5Stack (Cam N)]
    end

    %% Connections
    SD -- 物理ボタン操作 --> Comp
    Comp -- ATEM制御 --> ATEM
    ATEM -- タリー状態 --> Comp

    %% MQTT Flow
    Comp -- Publishes Tally (atem/tally/state) --> MQTT
    Comp -- Publishes Call (companion/call) --> MQTT
    
    MQTT -- Tally & Call 情報を購読 --> M5_1
    MQTT -- Tally & Call 情報を購読 --> M5_2
    MQTT -- Tally & Call 情報を購読 --> M5_N
```

### コンポーネントの役割

1.  **Bitfocus Companion + Stream Deck (中核)**

      * **役割:** システムの「頭脳」であり「操作卓」。
      * ATEMと直接接続し、制御します。
      * MQTTブローカーと接続します。
      * ATEMのタリー状態を監視し、MQTTブローカーへ送信（Publish）します。
      * Stream Deckのボタン操作（CALLなど）をMQTTブローカーへ送信します。

2.  **MQTTブローカー (Mosquitto)**

      * **役割:** 全コンポーネント間の通信を仲介する「掲示板」サーバー。
      * Companionと同じPCで起動します。

3.  **M5Stack ATOM Matrix (Arduino C++)**

      * **役割:** カメラタリーランプ本体。
      * 起動時に、**本体ボタンで設定・保存された自分のカメラ番号**（例: `Cam 3`）を読み込みます。
      * 指定Wi-Fi（`Atem_Py`）とMQTTブローカーに接続します。
      * 自分に割り当てられた番号（`Cam 3`）に関するタリー情報とCALL信号を購読し、LEDを光らせます。

-----

## 4\. GUIアプリ廃止によるトレードオフ

この「GUI-less」設計には、明確なメリットとデメリットがあります。

#### 🟢 メリット (得られるもの)

  * **システムの単純化:** 起動するソフトウェアが「Companion」と「Mosquitto」の2つだけになります。
  * **インストールの簡素化:** オペレーターのPCに専用アプリをインストールする必要がありません。
  * **現場での機動力:** PCがなくても、M5Stack本体の操作だけでカメラ番号の変更（例：「Cam 2」を「Cam 5」にする）が可能です。予備機材の投入が容易になります。

#### 🔴 デメリット (失われるもの)

  * **接続監視機能の喪失:**
    これが最大のトレードオフです。「どのM5Stackが今オンラインか」「どのM5Stackの電池が切れたか」を**PCの画面で一覧表示する機能が失われます。** 接続確認は、M5Stack本体のLEDがタリーに反応するかを目視で行う必要があります。
  * **中央管理機能の喪失:**
    カメラ番号の割り当て状況をPCで集中管理できなくなります。

-----

## 5\. 技術スタック (Technology Stack)

| コンポーネント | 技術 | ライブラリ / ツール |
| :--- | :--- | :--- |
| **制御中核** | **Bitfocus Companion** | `blackmagic-atem` モジュール<br>`generic-mqtt` モジュール |
| **操作卓** | **Stream Deck** | (Companionと連携) |
| **MQTTブローカー** | MQTT | `Mosquitto` |
| **M5Stack** | Arduino C++ | `M5Unified` (M5Stack制御)<br>`PubSubClient` (MQTT)<br>`ArduinoJson` (JSON解析)<br>`EEPROM` (番号保存) |

-----

## 6\. 必要なもの (Prerequisites)

### 🖥️ ハードウェア

  * **Stream Deck** (XL, Standard, Miniなど) (必須)
  * ATEM スイッチャー (ATEM Extreme ISO など)
  * M5Stack ATOM Matrix (最大10台)
  * Wi-Fiアクセスポイント
      * **SSID:** `Atem_Py`
      * **PASS:** `Tally33`
  * Companionを動かすPC (Windows, macOS, or Linux)

### ⚙️ ソフトウェア

  * **Bitfocus Companion** (必須)
      * [Bitfocus Companion 公式サイト](https://bitfocus.io/companion) からダウンロード
  * **Mosquitto** (MQTTブローカー) (必須)
      * [Mosquitto公式ダウンロードページ](https://mosquitto.org/download/) からインストーラを入手
  * **VSCode** と **PlatformIO** 拡張機能 (M5Stack開発用)

-----

## 7\. 実装計画 (Implementation Plan)

### Step 1: MQTTブローカー (Mosquitto) のセットアップ

1.  PCに `Mosquitto` をインストールします。
2.  （通常は自動起動しますが）Mosquittoサービスを起動します。
3.  ファイアウォールで **TCPポート 1883** が開いていることを確認します。

-----

### Step 2: Bitfocus Companion の設定 (最重要)

これは前回の計画と同一です。

1.  **ATEM接続の追加:**

      * `Connections` タブで `Blackmagic Design: ATEM` を追加し、ATEMのIPアドレスを入力します。

2.  **MQTT接続の追加:**

      * `Connections` タブで `Generic: MQTT` を追加し、`Broker IP` に `127.0.0.1`（Companionと同じPCの場合）を入力します。

3.  **タリー情報をMQTTに送信する設定 (Triggers):**

      * `Triggers` タブで `Add trigger` を選択します。
      * `Trigger type` は `On internal clock (1s)` を選びます。
      * `Actions` で `mqtt: Publish` を選択します。
      * **Topic:** `atem/tally/state`
      * **Payload (JSON):** 以下の文字列をコピー＆ペーストします。

    <!-- end list -->

    ```json
    {
      "1": "$(atem:tally_source_1)",
      "2": "$(atem:tally_source_2)",
      "3": "$(atem:tally_source_3)",
      "4": "$(atem:tally_source_4)",
      "5": "$(atem:tally_source_5)",
      "6": "$(atem:tally_source_6)",
      "7": "$(atem:tally_source_7)",
      "8": "$(atem:tally_source_8)",
      "9": "$(atem:tally_source_9)",
      "10": "$(atem:tally_source_10)"
    }
    ```

      * *(注: `$(atem:tally_source_X)` は、PGMなら`2`、PVWなら`1`、OFFなら`0`を返す変数です。)*

4.  **Stream Deck ボタンの設定 (`Buttons` タブ):**

      * **手元タリー (例: CAM 1 ボタン):**
          * `Press action` に `ATEM: Set input on Program` (Input: 1) を設定。
          * `Feedback` に `ATEM: Tally` (Input: 1) を設定。
      * **CALLボタン (例: CALL 1 ボタン):**
          * `Press action` に `mqtt: Publish` を設定。
              * **Topic:** `companion/call`
              * **Payload:** `{"cam": 1, "state": "ON"}`
          * `Release action` (ボタンを離した時) に `mqtt: Publish` を設定。
              * **Topic:** `companion/call`
              * **Payload:** `{"cam": 1, "state": "OFF"}`

-----

### Step 3: M5Stack ファームウェア (Arduino C++) の書き込み

ここがGUI-less版の核心です。

1.  **PlatformIO プロジェクト作成:**

      * VSCode + PlatformIOで `m5stack-atom` ボードのプロジェクトを作成します。

2.  **`platformio.ini` (設定ファイル):**

      * `EEPROM` を扱うため、`lib_deps` に `ArduinoJson` と `PubSubClient` に加えて `M5Unified` を指定します。

    <!-- end list -->

    ```ini
    [env:m5stack-atom]
    platform = espressif32
    board = m5stack-atom
    framework = arduino
    lib_deps =
        m5stack/M5Unified
        knolleary/PubSubClient
        bblanchon/ArduinoJson
    monitor_speed = 115200
    ```

3.  **`src/main.cpp` (メインコード):**

      * **`setup()` 関数のロジック:**

        1.  `M5.begin()` でM5Stackを初期化。
        2.  `EEPROM.begin(16)` でメモリを初期化。
        3.  `M5.Btn.wasPressed()` (本体ボタン) をチェックします。
        4.  **もしボタンが押されていたら (設定モード):**
              * `setCameraIDMode()` 関数を呼び出します。
              * LEDに現在の番号（例: `1`）を表示。
              * `while(true)` ループに入り、ボタンが押されるのを待ちます。
              * 短く押されたら、番号を 1〜10 でインクリメント（`myCameraID++`）します。
              * 2秒間長押しされたら、`EEPROM.write(0, myCameraID)` で番号をメモリに書き込み、`EEPROM.commit()` してループを抜け、再起動します。
        5.  **もしボタンが押されていなかったら (通常起動):**
              * `myCameraID = EEPROM.read(0)` で保存された番号を読み込みます。
              * もし番号が 0 または 10 より大きい場合、`1` にリセットします。
              * Wi-Fi (`Atem_Py`, `Tally33`) とMQTTブローカーに接続します。

      * **`loop()` 関数のロジック:**

        1.  `M5.update()` でボタン状態を更新。
        2.  MQTTの接続維持 (`client.loop()`)。
        3.  （`setCameraIDMode` に入るためのボタン長押しチェックをここにも配置）

      * **`callback()` (MQTT受信) 関数のロジック:**

        1.  `ArduinoJson` を使って受信したトピックのペイロードをパースします。
        2.  **`atem/tally/state` トピック受信時:**
              * `JsonObject doc = ...`
              * `String myKey = String(myCameraID);` // `myCameraID` は 3 など
              * `int state = doc[myKey];` // JSONの `{"3": 2}` の `2` を取得
              * `updateLED(state);` // `state` (0, 1, 2) に応じてLEDを点灯
        3.  **`companion/call` トピック受信時:**
              * `int cam = doc["cam"];`
              * `String state = doc["state"];`
              * もし `cam == myCameraID` なら、CALL状態（黄点灯）またはタリー状態に戻します。

-----

## 8\. 通信プロトコル (MQTTトピック設計)

GUIアプリがなくなったため、プロトコルは非常にシンプルになります。

| トピック | 送信元 | 受信先 | ペイロード (JSON) | 説明 |
| :--- | :--- | :--- | :--- | :--- |
| **`atem/tally/state`** | **Companion** | M5Stack (全台) | `{"1": "2", "2": "1", ...}` | [最重要] 全タリー状態 (2=PGM, 1=PVW, 0=OFF)。Companionが1秒ごとに送信。 |
| **`companion/call`** | **Companion** | M5Stack (全台) | `{"cam": 1, "state": "ON"}`<br>`{"cam": 1, "state": "OFF"}` | Stream Deckから送信されるCALL信号。 |

-----

## 9\. 起動と運用 (Usage)

1.  **事前準備 (M5Stack):**
      * 各M5Stackの電源を入れ、**本体ボタンを操作**して、それぞれにカメラ番号（Cam 1, Cam 2...）を割り当て、保存します。
2.  **起動順序:**
    1.  Wi-Fiアクセスポイント (`Atem_Py`) を起動します。
    2.  ATEMスイッチャーを起動し、ネットワークに接続します。
    3.  PCで**Mosquitto**（MQTTブローカー）が起動していることを確認します。
    4.  PCで**Bitfocus Companion**を起動し、ATEMとMQTTの両方が `OK` になるのを確認します。
    5.  M5Stack ATOM Matrixの電源を入れます。（自動的にWi-FiとMQTTに接続し、自分の番号のタリー監視を開始します）
3.  **操作:**
      * **ATEM操作:** Stream Deckのボタン（CUT, AUTO, マクロ等）でATEMを操作します。
      * **タリー確認:** ATEM操作に連動し、Stream Deckのボタンと、M5Stackの両方が赤・緑に点灯することを確認します。
      * **CALL操作:** Stream Deckの「CALL 1」ボタンを押すと、`Cam 1` を設定したM5Stackが黄色に点灯することを確認します。
