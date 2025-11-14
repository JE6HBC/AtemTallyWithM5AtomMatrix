# Bitfocus Companion 設定ガイド

このディレクトリは、Bitfocus Companionに関する設定情報やプロファイルを管理するためのものです。

## 概要

本プロジェクトでは、Bitfocus CompanionがATEMスイッチャーと直接通信し、そのタリー情報をMQTTブローカーを介して各M5Stackタリーランプに中継する役割を担います。

Companionの設定は、プロジェクトのルートにあるメインの `README.md` の **「Step 2: Bitfocus Companion の設定」** に詳述されています。以下はその要約です。

### 1. 接続 (Connections) の設定

Companionの `Connections` タブで、以下の2つのインスタンスを追加します。

*   **Blackmagic Design: ATEM**
    *   **Target IP:** ご利用のATEMスイッチャーのIPアドレスを入力します。
*   **Generic: MQTT**
    *   **Broker IP:** `127.0.0.1` を入力します（CompanionとMQTTブローカーが同じPCで動作している場合）。

### 2. トリガー (Triggers) の設定

ATEMのタリー状態を定期的にMQTTブローカーに送信するための設定です。

*   **`On internal clock (1s)`** トリガーを作成します。
*   **Action:** `mqtt: Publish`
*   **Topic:** `atem/tally/state`
*   **Payload:** メインの `README.md` に記載されているJSON文字列をコピー＆ペーストします。これにより、10個の入力ソースのタリー状態（0=OFF, 1=PVW, 2=PGM）が1秒ごとに送信されます。

### 3. ボタン (Buttons) の設定

Stream Deckの物理ボタンに機能を割り当てます。

*   **手元タリーボタン (例: CAM 1):**
    *   `Press action` に `ATEM: Set input on Program` などを設定します。
    *   `Feedback` に `ATEM: Tally` を設定すると、Stream Deckのボタン自体がタリーランプとして機能します。
*   **CALLボタン (例: CALL 1):**
    *   `Press action` に `mqtt: Publish` を設定し、特定のカメラ（例: `{"cam": 1, "state": "ON"}`）にCALL信号を送信します。
    *   `Release action` にも同様に `mqtt: Publish` を設定し、ボタンを離した際にCALLをオフ（`"state": "OFF"`）にします。

---

詳細な手順やPayloadのJSONデータについては、必ずプロジェクトルートの `README.md` を参照してください。
