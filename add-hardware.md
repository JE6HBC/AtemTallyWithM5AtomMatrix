# M5Stack Atom Matrix 外部タリーLED接続ガイド

このドキュメントは、M5Stack Atom Matrixに外部の大型タリーLEDを接続するための技術的な情報を提供します。

## 1. GPIOピン割り当て

タリー状態（PGM/PVW/CALL）は、以下のGPIOピンから3.3Vの電圧で出力されます。

| 色   | タリー状態 | GPIOピン |
| :--- | :--- | :--- |
| **赤** | **PGM** (Program) | `G21` |
| **緑** | **PVW** (Preview) | `G22` |
| **黄** | **CALL** | `G23` |

これらのピンは、ファームウェア `m5/src/main.cpp` 内で定義されています。

## 2. 電気的特性と注意事項

**警告:** M5Stack Atom Matrixに搭載されているESP32-PICO-D4のGPIOピンから直接取り出せる電流は**非常に小さい**です（最大40mA程度、推奨は12mA以下）。
**絶対にGPIOピンに直接高輝度の大型LEDを接続しないでください。** 過電流により、M5Stack本体が恒久的に破損する可能性があります。

### 安全な接続方法

大型LEDやLEDテープ（DC12V駆動など）を制御するには、GPIOからの信号をトリガーとして、外部電源でLEDを駆動する**電流増幅回路**が必須です。

一般的に、以下のいずれかの電子部品を使用します。

*   **NPNトランジスタ** (例: 2N3904)
*   **NチャネルMOSFET** (例: 2N7000)

#### 回路例 (NPNトランジスタ)

以下は、1つのGPIOピンでLEDを駆動する最も基本的な回路例です。

```mermaid
graph LR
    subgraph M5Stack
        GPIO[GPIO Pin (e.g., G21)]
        GND_M5[GND]
    end

    subgraph External Circuit
        VCC[+12V Power Supply]
        LED[High Power LED]
        R_LED[Current Limiting Resistor for LED]
        T1[NPN Transistor (e.g., 2N3904)]
        R_Base[Base Resistor (e.g., 1kΩ)]
    end

    VCC --|> R_LED --|> LED --|> T1(Collector)
    GPIO --|> R_Base --|> T1(Base)
    T1(Emitter) --|> GND_M5
    VCC --|> GND_External[GND]

    style T1 fill:#f9f,stroke:#333,stroke-width:2px
```

*   **VCC:** LEDを駆動するための外部電源（例: +12V）。
*   **LED:** 使用する大型LED。
*   **R_LED:** LEDに適した電流制限抵抗。LEDの仕様に合わせて計算してください。
*   **T1:** NPNトランジスタ。GPIOからの微弱な電流で、LEDを流れる大きな電流をスイッチングします。
*   **R_Base:** トランジスタのベースを保護するための抵抗（1kΩ程度が一般的）。
*   **重要:** M5StackのGNDと、外部電源のGNDは必ず接続（コモンGND化）してください。

この回路を、赤・緑・黄の3つのGPIOピン（G21, G22, G23）それぞれに対して用意することで、3色の外部タリーランプを安全に駆動できます。
