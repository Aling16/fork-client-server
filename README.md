# Fork-based Client–Server in C

多行程 (fork) TCP client–server 範例，含共用動態函式庫、兩層 debug log 與穩健性機制。

## 建置

```bash
make                          # 預設：ROBUST=1, DEBUG=0
make DEBUG=1                  # 編入 debug log
make ROBUST=0                 # 關閉穩健性機制（做對照實驗用）
make IDLE_TIMEOUT=30          # 設定閒置逾時秒數
make clean                    # 切換選項前務必先 clean
```

## 執行

```bash
./server [-v|-vv] [-p port | port]    # 預設 port 8888
./client [-v|-vv] [host] [port]       # 預設 127.0.0.1 8888
```

Client 指令：`sysinfo`（取得系統資訊）、任意文字（echo）、`quit`。

## 目錄

- `include/`, `lib/`：共用函式庫 (`libcommon.so`)
- `src/`：server、client
- `report/`：封包擷取與報告素材
