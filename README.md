# etherAdapter

**etherAdapter** — 设备采样数据接入适配器：接收设备发送的采样数据，解析后写入
[EtherDB](https://github.com/kinyi6666/EtherDB) 时序数据库。

> 🌏 English: [README.en.md](README.en.md)

> 设计文档：`etherAdapter.txt`（本目录）。
> 数据流：`设备 → muduo → moodycamel → 单线程解析 → 写入队列 → EtherDB`
> （发布队列 → ZMQ PUB 已预留，暂未实现）
> 接入协议：custom_data（自定义头，设备主动上报，含状态帧/事件帧）、
> modbus（适配器 3s 轮询寄存器）、http（单 HTTP 服务端，POST JSON）；
> mqtt 仍为 stub。

---

## 1. 架构

```
  custom_data 设备 ────┐（同一 ip:port 一条 TCP，可同时发状态帧 + 事件帧）
                       ▼
            ┌───────────────── muduo (net) ─────────────────┐
 modbus 设备 ◄─请求─ ModbusPoller（3s 轮询，长连接）            │
           │  └─响应─► TcpServer(60382, ...) onMessage → RawChunk │
           └─────────► TcpServer(60382, ...)   ↑（响应经统一端口进入）
                      └───────────────────────────┬─────────────────┘
                                                  │ ingest queue (moodycamel)
 http 客户端 ──POST JSON──► HttpIngest(独立端口)     │
                            │ 解析 JSON → RowBatch  │
                            └──────────► write queue
                                                  ▼
                                  ┌─ ParserWorker（单线程：切帧 + 字段解析）─┐
                                  │  custom_data：6B 自定义头，按 frameType  │
                                  │  → 状态设备表 / 事件设备表              │
                                  │  modbus：响应头 + 大端寄存器            │
                                  │  逐字段 byte_offset/bit 解码（原始值）  │
                                  └───────┬──────────────────────┬──────────┘
                                          │ RowBatch             │ RowBatch(副本)
                                          ▼                      ▼
                             write queue (moodycamel)      publish queue
                                          │                      │
                                          ▼                      ▼
                                EtherDBWriter（列绑定/直拼 SQL）  ZMQ PUB（预留 stub）
                                          │
                                          ▼
                                      EtherDB (127.0.0.1:7040)
```

- 网络：复用 EtherDB 的 muduo 风格网络库（`src/net`）+ base 库（`src/base`），
  与 EtherDB 服务端同一套代码，Windows/Linux 均可构建。
- 队列：moodycamel `BlockingConcurrentQueue`（`src/base/blockingconcurrentqueue.h`），
  与 EtherDB 服务端同样的 traits（大块 + 块回收）。
- 解析：**单线程**完成全部协议解析（完全按配置驱动，无硬编码协议）：
  custom_data 帧与 modbus 响应都从统一监听端口进入解析线程。
- 写入：custom_data/modbus 设备走 SDK 列绑定预编译语句
  （`EtDBStmt::bindParamBatch + execute`，最快路径）；http 慢速数据直接拼
  `INSERT INTO ... VALUES(...)`（无需预绑定）。

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
│       ├── HttpIngest.*     单一 HTTP 服务端（POST JSON → 写队列）
│       ├── ModbusPoller.*   modbus 定时寄存器请求（3s，长连接+退避重连）
│       ├── IngestStubs.*    mqtt 接入（stub）
│       └── AdapterApp.*     装配与生命周期；main.cpp 入口
├── tools/csv2sqlite.cpp      配置 CSV → SQLite 导入工具
├── tests/device_sim.cpp      custom_data 设备模拟器（状态帧 + 事件帧）
├── tests/modbus_sim.cpp      modbus 设备模拟器（请求响应 + 数据回推）
├── tests/query_check.cpp     小查询工具（验证工具，链接 SDK）
├── tests/fixtures/           测试用配置 CSV（样例数据的修正版）
├── docs/                     文章 / 软文（中、英）
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
| | `batchRows / flushIntervalMs` | 事件批大小 / 部分批空闲刷写间隔（状态数据不受此值影响，来了就提交） |
| `[http]` | `port` | HTTP 监听端口；0 = 取 device_table 中 http 设备的 local_server_port（只起**一个** HTTP 服务端） |
| `[modbus]` | `pollIntervalMs` | 寄存器读请求周期（默认 3000ms） |
| | `connectTimeoutMs` | 设备请求连接的超时（非阻塞 connect，默认 1500ms） |
| `[log]` | `dir / level` | 异步文件日志目录与级别 |
| `[zmq]` | `enabled / pubEndpoint` | 发布路径预留（当前为 stub） |

### 3.2 设备配置（SQLite 三张表）

解析所需配置全部来自 SQLite 配置库（只读打开），三张表与现场导出保持一致：

- `device_table`：每设备一行，主键为 `device_id`（同时作为 EtherDB 表名）。
  `(device_ip, device_port)` 用于把连接匹配到设备：custom_data 为设备的源端口；
  modbus 为设备的**请求端口**（适配器主动连接它发读取命令）；http 设备可留空。
  共享同一 `(device_ip, device_port)` 的多行组成**一个对端组**：第一条（`data_proto_id`
  最小）= 状态设备，其余 = 事件设备，靠帧类型区分 — 于是一台设备可以用**一条 TCP
  连接**同时上报状态采样和事件采样，并分别落入不同的 EtherDB 表。
  `local_server_port` 为适配器监听端口（custom_data/modbus 共用统一数据端口，
  http 用自己的独立端口，两类端口不能相同）；`data_proto_id` 关联另外两张表。
- `data_header_table`：每个 `data_proto_id` 的帧格式 + 帧类型。
  - **custom_data**（自定义头 6 字节，小端）：
    `[frameType 1B][flag 1B][frameLen 2B][sequenceId 2B][payload ...][flag 1B?]`
    - `frameType` **就是本表的 `frame_type`**：一个 `(ip, device_port)` 端点下
      第一行是状态设备（周期采样，一般 1Hz），其余行是**事件设备**（突发采样，
      测点数/频率可以不同），各自写自己的 EtherDB 表（事件表）。
    - `flag` 即 `start_flag`（= `end_flag`，同一个字节）：**只对事件帧有意义**，
      用于在同一条 TCP 数据流里识别“另一套解析规则 / 另一张表”；状态帧不带分隔符，
      该字节被忽略。两列不一致时按“前导/尾部”分别使用（兼容 0x01…0x03）。
    - `frameLen` = 本帧 payload 字节数；一次可携带 `frameLen / frame_len` 条记录
      （多条记录共用一个头；0 = 一条记录）。
    - `sequenceId` 预留：**不校验、不解析**，将来扩展用。
  - **modbus**：`frame_type` **即响应头长度**（9/7/2/0）：
    9 = MBAP(7B)+功能码+字节数，7 = 仅 MBAP，2 = 功能码+字节数，0 = 无头裸寄存器数据；
    请求参数 `slave_addr / fun_code / start_addr / addr_num` 也在此表配置：
    `[transId 2][0x0000 2][length=6 2][slave_addr 1][fun_code 1][start_addr 2][addr_num 2]`
    （大端，transId 1..0xFE 循环）。
- `data_proto_table`：每个 `data_proto_id` 的传感器字段表（一个字段=一列）：
  `field_type`（类型码，见 `ConfigDB.cpp::mapFieldType`）、`byte_offset`、
  `bit_offset/bit_len`（位段）。**`factor` 暂不参与计算**：数据库存原始值
  （至少非 float 类型一律原始值；配置了非 1 的 factor 只记一条告警）。

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
  （`EtherDB\scripts\build_sdk.bat`）即可
  （EtherDB 仓库：https://github.com/kinyi6666/EtherDB）。
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
为每台设备（含事件设备）建表
`CREATE TABLE IF NOT EXISTS <device_id> (ts TIMESTAMP, ...)` →
预编译 INSERT（http 设备跳过预绑定）→ 然后：

- **custom_data**：监听 `local_server_port`（统一端口），设备主动上报；帧头
  `frameType` 决定写入哪张表 —— 状态帧（周期采样，一般 1Hz）**来了就提交**，
  事件帧（突发/高频、测点数可不同）仍按 `batchRows` 攒批后写入事件表；
- **modbus**：每 3s（`[modbus].pollIntervalMs`）向设备请求端口发送寄存器读命令
  （长连接 + 非阻塞 connect + 退避重连），设备的响应仍从统一端口进入；
- **http**：单独监听一个 HTTP 端口（`[http].port` 或 http 设备的
  `local_server_port`），设备 POST JSON 即写库。

按 `Ctrl+C` 优雅退出（停止收数 → 解析完存量 → 刷写队列 → 落库）。

数据表结构：`<device_id>(ts TIMESTAMP, <字段1> <类型>, ...)`，
`ts` = 适配器**接收时间**（毫秒）。状态数据与事件数据字段同构但分表
（如 `dev_T100_001` / `dev_T100_001_E`）。运行统计每 5 秒记录到日志
（`log\etherAdapter*.log`，含 event/modbus/http 计数）。

## 6. 端到端测试

```bat
build\bin\Release\csv2sqlite.exe build\bin\Release\etherAdapter.db tests\fixtures --force
cd build\bin
etherdb_dserver.exe -p 7040      :: 另开窗口
etherAdapter.exe
device_sim.exe -n 2000 -i 1      :: 模拟 dev_T100_001 连 60382，发 2000 帧
```

`device_sim` 以**源端口 10002** 连接（匹配 device_table）。状态帧 73B/记录、
事件帧 62B/记录，可分别发，也可（推荐）在**同一条连接**上交替发：

```bat
:: 1a) 只发状态帧（frameType 0xC → dev_T100_001）
device_sim.exe -n 2000 -i 1

:: 1b) 状态 + 事件在一条 TCP 连接上交替（真实部署场景）
::     -t C,E = 帧类型 0xC/0xE；-l 73,62 = 记录长度；-f/-e = 事件分隔字节
device_sim.exe -n 60 -i 20 -t C,E -l 73,62 -f 0,01 -e 0,03 -v 100
```

验证：

```bat
query_check.exe 127.0.0.1 7040 adapter "SELECT COUNT(*) FROM dev_T100_001"
query_check.exe 127.0.0.1 7040 adapter "SELECT * FROM dev_T100_001 LIMIT 10"
:: 状态表 item1 = 0,2,4...（交替模式下帧号递增）
query_check.exe 127.0.0.1 7040 adapter "SELECT * FROM dev_T100_001_E LIMIT 10"
:: 事件表 dev_T100_001_E 的 item1/item2 = 1,3,5...（frameType 0xE 路由）
```

（`tests/fixtures/README.md` 有完整步骤说明。）

### modbus 测试（fixtures: dev_MB_401, ip 127.0.0.2, 请求端口 10004）

```bat
modbus_sim.exe -n 3 -m 9 -v 2000 -B 127.0.0.2
:: -m 9/7/2/0 = 响应头模式（对应 data_header_table.frame_type）
:: -B 指定推送连接源 IP（模拟设备独立 IP；推送源端口也可用 -b）
query_check.exe 127.0.0.1 7040 adapter "SELECT * FROM dev_MB_401"
:: reg1/reg2 = 2000+k / 2100+k（大端解码，原始值 —— factor 不参与计算）
```

### http 测试（fixtures: dev_HTTP_001, HTTP 端口 60390）

```powershell
Invoke-RestMethod -Method Post -Uri http://127.0.0.1:60390/ -Body '{"temp":12.5,"humidity":60}'
# -> {"ok":true,"device":"dev_HTTP_001","matched":2}
query_check.exe 127.0.0.1 7040 adapter "SELECT * FROM dev_HTTP_001"
```

## 7. 当前范围与扩展点

| 能力 | 状态 |
|---|---|
| custom_data TCP 接入（自定义头 + 状态帧/事件帧路由）+ EtherDB 批量写入 | ✅ 已实现 |
| 状态数据（~1Hz）来了就提交；事件帧攒批写入事件表 | ✅ 已实现 |
| MODBUS 接入（conn_proto=modbus(2)，3s 轮询 + 响应头 9/7/2/0 解析） | ✅ 已实现 |
| HTTP 接入（conn_proto=http(3)，单服务端 + JSON → 直拼 INSERT SQL） | ✅ 已实现 |
| MQTT 订阅接入（conn_proto=mttq(4)） | ⏳ stub（`IngestStubs.h`） |
| 发布队列 → ZMQ PUB | ⏳ 预留（`PublishQueue`，启用时仅拷贝+丢弃） |
| sequenceId 解析/校验 | ⏳ 预留（仅占位，不校验不解析） |
| factor 换算 | ⏳ 暂不实现（存原始值） |

扩展点（保持数据流不变，只加生产者/消费者）：

- **MQTT**：仿照 `HttpIngest`（直接解析 → 写队列）或 `ModbusPoller`
  （请求/订阅 → 统一端口收数）实现，复用现有解析/写入路径。
- **ZMQ PUB**：实现 `PublishQueue::run()` 里 `TODO(zmq)` 标注的 PUB 发送。
- **事件流扩展**：同一 `(ip, device_port)` 下再加一行 device_table + 一个
  `frame_type` 即可新增事件流（事件类型 2、3…），无需改代码。
- **协议维护**：只改 SQLite 三张表；`field_type` 码表集中在
  `ConfigDB.cpp::mapFieldType()`。

## 8. 注意事项

- 设备匹配优先级：`(ip, 源端口)` 精确匹配 → 该 IP 唯一对端组时按 IP 匹配；
  都不中则丢弃并限频告警（可在日志统计 `drop` 列观察）。同 IP 多设备测试时
  请绑定设备的源端口/IP（参见 `device_sim -b` / `modbus_sim -b/-B`）。
- **一个 `(ip, 源端口)` 可以对应多行 device_table**（状态 + 多个事件流），
  它们组成**对端组**并按 `data_header_table.frame_type` 路由；`device_id`
  即各自的 EtherDB 表名（事件表）。
- 同一 `local_server_port` 可服务多台设备（按对端区分）；`extraListenPorts`
  仅用于测试/临时端口。**HTTP 端口必须与其他监听端口不同**。
- modbus 寄存器数据**默认为大端**（协议惯例）：`endian` 配 0/缺省即为大端，
  配 1 才按小端。custom_data 的 endian 语义不变（0=小端）。
- `start_flag`/`end_flag` 只对事件帧有意义（用于在同一条 TCP 流中识别另一套
  解析规则/另一张表）；两列一般是同一个字节，不一致时按前导/尾部各用各的。
- `data_proto_table.factor` 目前**不参与计算**，数据库存原始值；无符号整型按
  相邻更宽的**有符号**列存储（避免溢出）。http 的 JSON 数值直接按原值存储。
- http 缺失字段写 SQL NULL；查询时 `WHERE col IS NULL` 可正确匹配（列表
  展示路径会将 NULL 显示为 0，属 EtherDB 查询响应不含 NULL 位图的已知特性）。
- Windows 下同一源端口短时间内重连会撞 TIME_WAIT（4 元组），换 `-b/-B` 或
  等约 2 分钟；`[parse].frameLenMode` 已移除（帧长由自定义头携带）。
