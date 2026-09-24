# etherAdapter

**etherAdapter** — 设备采样数据接入适配器：接收设备发送的采样数据，解析后写入
[EtherDB](../EtherDB) 时序数据库。

> 设计文档：`etherAdapter.txt`（本目录）。
> 数据流：`设备 → muduo → moodycamel → 单线程解析 → 写入队列 → EtherDB`
> （发布队列 → ZMQ PUB 已预留，暂未实现）

---

## 1. 架构

```
                         ┌──────────────────────────── muduo (net) ────────────┐
 device (raw_data)  ──►  │ TcpServer(60382, ...)  onMessage → RawChunk          │
                         └───────────────────────────────┬─────────────────────┘
                                                         │ ingest queue (moodycamel)
                                                         ▼
                                          ┌─ ParserWorker（单线程：切帧 + 字段解析）─┐
                                          │  FrameCodec: start/end flag, frame_len, │
                                          │  逐字段 byte_offset/bit/factor 解码      │
                                          └───────┬──────────────────────┬──────────┘
                                                  │ RowBatch             │ RowBatch(副本)
                                                  ▼                      ▼
                                     write queue (moodycamel)      publish queue
                                                  │                      │
                                                  ▼                      ▼
                                        EtherDBWriter（批量插入）    ZMQ PUB（预留 stub）
                                                  │
                                                  ▼
                                              EtherDB (127.0.0.1:7040)
```

- 网络：复用 EtherDB 的 muduo 风格网络库（`src/net`）+ base 库（`src/base`），
  与 EtherDB 服务端同一套代码，Windows/Linux 均可构建。
- 队列：moodycamel `BlockingConcurrentQueue`（`src/base/blockingconcurrentqueue.h`），
  与 EtherDB 服务端同样的 traits（大块 + 块回收）。
- 解析：**单线程**完成全部协议解析（完全按配置驱动，无硬编码协议）。
- 写入：EtherDB 客户端 SDK（`ETDB::Client`）列绑定预编译语句
  `EtDBStmt::bindParamBatch + execute` —— SDK 最快的插入路径。

## 2. 目录结构

```
etherAdapter/
├── CMakeLists.txt            顶层构建（base/net/sqlite3/adapter/tools/tests）
├── config/
│   ├── etherAdapter.cfg      进程级配置样例（INI）
│   └── schema.sql            SQLite 配置库 schema（三张表）
├── deps/sqlite3/             SQLite amalgamation（静态编译进程序）
├── src/
│   ├── base/                 复用 EtherDB 的 muduo base（日志/线程/队列头）
│   ├── net/                  复用 EtherDB 的 muduo net（TcpServer/EventLoop...）
│   ├── wincompat/            Windows 兼容层（posix_compat.h 等）
│   └── adapter/              ★ 本项目应用代码
│       ├── AdapterConfig.*   INI 配置加载
│       ├── ConfigDB.*        SQLite 三表读取 → 运行时设备/协议注册表
│       ├── DeviceModel.h     字段/帧/设备描述（解析表的物化）
│       ├── FrameCodec.*      流式切帧 + 逐字段解码（大小端/位段/factor）
│       ├── Pipeline.h        RawChunk / RowBatch / 队列与统计
│       ├── IngestServer.*    muduo TCP 接入（按 ip:port 匹配设备）
│       ├── ParserWorker.*    单线程解析（解析→批次）
│       ├── EtherDBWriter.*   批量插入 EtherDB（列绑定 + 预编译）
│       ├── PublishQueue.*    发布队列（ZMQ PUB 预留 stub）
│       ├── IngestStubs.*     http / mqtt / modbus 接入（stub）
│       └── AdapterApp.*     装配与生命周期；main.cpp 入口
├── tools/csv2sqlite.cpp      配置 CSV → SQLite 导入工具
├── tests/device_sim.cpp      设备模拟器（端到端测试）
│   ├── tests/query_check.cpp 小查询工具（验证工具，链接 SDK）
│   └── fixtures/             测试用配置 CSV（样例数据的修正版）
└── scripts/build_windows.bat / build_linux.sh
```

## 3. 配置

### 3.1 进程配置 `etherAdapter.cfg`

参考 `config/etherAdapter.cfg`，默认从 **可执行文件同目录** 查找
（或用 `-c <file>` 指定）。关键项：

| 节 | 键 | 说明 |
|---|---|---|
| `[server]` | `extraListenPorts` | 附加监听端口（一般留空，监听端口来自 device_table） |
| `[sqlite]` | `path` | 配置数据库路径（相对 exe 目录） |
| `[etherdb]` | `host/port/user/password/db` | EtherDB 连接与目标库 |
| | `createDatabase / createTables` | 启动时自动建库/建表 |
| | `batchRows / flushIntervalMs` | 插入批大小 / 部分批空闲刷写间隔 |
| `[parse]` | `frameLenMode` | `auto`(默认探测) / `total` / `payload` |
| `[log]` | `dir / level` | 异步文件日志目录与级别 |
| `[zmq]` | `enabled / pubEndpoint` | 发布路径预留（当前为 stub） |

### 3.2 设备配置（SQLite 三张表）

解析所需配置全部来自 SQLite 配置库（只读打开），三张表与现场导出保持一致：

- `device_table`：每设备一行。`(device_ip, device_port)` 即设备的对端地址与
  源端口，适配器据此把 TCP 连接匹配到设备；`local_server_port` 为监听端口；
  `data_proto_id` 关联另外两张表。`device_id` 同时作为 EtherDB 表名。
- `data_header_table`：每个 `data_proto_id` 的帧格式：
  `[start_flag][frame_type][frame_len 2B][payload ...][end_flag]`，
  `frame_len` 的含义见 `[parse].frameLenMode`（auto 时按 `end_flag` 与字段
  范围自动探测并锁定）。
- `data_proto_table`：每个 `data_proto_id` 的传感器字段表（一个字段=一列）：
  `field_type`（类型码，见 `ConfigDB.cpp::mapFieldType`）、`byte_offset`、
  `bit_offset/bit_len`（位段）、`factor`（物理值 = 原始值 × factor）。

> `field_type` 类型码表与样例数据的对应关系集中在 `src/adapter/ConfigDB.cpp`
> 的 `mapFieldType()`：`0=BOOL, 1=INT16, 2=INT32, 3=FLOAT32, 4=FLOAT64,
> 5=INT8, 6=UINT8, 7=UINT16, 8=UINT32, 9=INT64, 10=UINT64`。若现场协议定义
> 不同，**只需改这一处**。

### 3.3 导入配置库

```bat
:: [sqlite].path 的默认值是 config/etherAdapter.db（相对 exe 目录），
:: 因此直接把库生成到与 etherAdapter.exe 同级的 config\ 目录下：
build\bin\Release\csv2sqlite.exe build\bin\Release\config\etherAdapter.db <csv 目录>
```
把 `device_table.csv / data_header_table.csv / data_proto_table.csv` 导入
（跳过表头与注释行；GBK 自动转 UTF-8；`--force` 重建表）。

## 4. 构建

```bat
:: Windows (VS2022)
scripts\build_windows.bat
:: 产物：build\bin\Release\{etherAdapter.exe, csv2sqlite.exe, device_sim.exe, query_check.exe, base.dll, net.dll}
```

```sh
# Linux
./scripts/build_linux.sh
```

前置条件：
- **EtherDB 客户端 SDK**：默认取 `../EtherDB/src/bin/sdk`（include + lib），
  可用 `-DETHERDB_SDK_DIR=<...>` 覆盖；先构建一次 EtherDB SDK
  （`EtherDB\scripts\build_sdk.bat`）即可。
- CMake ≥ 3.16；Windows 用 VS2022（/MD 与 SDK 一致）。

## 5. 运行

```bat
:: 1) 启动 EtherDB 服务端（另开窗口）
EtherDB\src\bin\Release\etherdb_dserver.exe -p 7040

:: 2) 准备配置库（首次）
build\bin\Release\csv2sqlite.exe build\bin\Release\etherAdapter.db <csv 目录>

:: 3) 把 config\etherAdapter.cfg 复制到 etherAdapter.exe 同目录（build\bin\Release\）并按需修改，然后启动适配器
build\bin\Release\etherAdapter.exe            :: 或 etherAdapter.exe -c <cfg>
```

启动后适配器会：连接 EtherDB → `CREATE DATABASE IF NOT EXISTS` + `USE` →
为每台 raw_data 设备建表 `CREATE TABLE IF NOT EXISTS <device_id> (ts TIMESTAMP, ...)`
→ 预编译 INSERT → 监听 `device_table.local_server_port` 收数。
按 `Ctrl+C` 优雅退出（停止收数 → 解析完存量 → 刷写队列 → 落库）。

数据表结构：`<device_id>(ts TIMESTAMP, <字段1> <类型>, ...)`，
`ts` = 适配器**接收时间**（毫秒）。运行统计每 5 秒记录到日志
（`log\etherAdapter*.log`）。

## 6. 端到端测试

```bat
build\bin\Release\csv2sqlite.exe build\bin\Release\etherAdapter.db tests\fixtures --force
cd build\bin
etherdb_dserver.exe -p 7040      :: 另开窗口
etherAdapter.exe
device_sim.exe -n 2000 -i 1      :: 模拟 dev_T100_001 连 60382，发 2000 帧
```

`device_sim` 以**源端口 10002** 连接（匹配 device_table），每帧
`[0x01][0x00][73 LE][item1 INT16=0,1,2...][0x03]`。验证：

```bat
query_check.exe 127.0.0.1 7040 adapter "SELECT COUNT(*) FROM dev_T100_001"
query_check.exe 127.0.0.1 7040 adapter "SELECT * FROM dev_T100_001 LIMIT 10"
:: item1 应为 0..1999（COUNT = 2000）
```

（`tests/fixtures/README.md` 有完整步骤说明；fixtures 修正了样例数据中
`data_proto_id` 100077/100078 的不一致。）

## 7. 当前范围与扩展点

| 能力 | 状态 |
|---|---|
| raw_data TCP 接入 + 配置驱动解析 + EtherDB 批量写入 | ✅ 已实现 |
| HTTP 接入（conn_proto=http(3)） | ⏳ stub（`IngestStubs.h`） |
| MQTT 订阅接入（conn_proto=mttq(4)） | ⏳ stub（`IngestStubs.h`） |
| MODBUS 接入（conn_proto=modbus(2)） | ⏳ stub（`IngestStubs.h`） |
| 发布队列 → ZMQ PUB | ⏳ 预留（`PublishQueue`，启用时仅拷贝+丢弃） |

扩展点（保持数据流不变，只加生产者/消费者）：

- **HTTP/MQTT/MODBUS**：把数据组装成 `RawChunk{dev, recvMs, bytes}` 塞进
  `IngestQueue`（或直接调用 `FrameCodec`）；解析/写入路径完全复用。
- **ZMQ PUB**：实现 `PublishQueue::run()` 里 `TODO(zmq)` 标注的 PUB 发送。
- **协议维护**：只改 SQLite 三张表；`field_type` 码表集中在
  `ConfigDB.cpp::mapFieldType()`。

## 8. 注意事项

- 设备匹配优先级：`(ip, 源端口)` 精确匹配 → 该 IP 唯一设备时按 IP 匹配；
  都不中则丢弃并限频告警（可在日志统计 `drop` 列观察）。
- 同一 `local_server_port` 可服务多台设备（按对端区分）；`extraListenPorts`
  仅用于测试/临时端口。
- `data_proto_table.factor != 1` 的整型字段会按 DOUBLE 列存储（物理值为
  小数）；无符号整型按相邻更宽的**有符号**列存储（避免溢出）。
- 现场协议与样例不一致时：`[parse].frameLenMode` 可强制 `total/payload`；
  帧内 `frame_type` 目前不做校验（仅记录字段中的扩展空间）。
