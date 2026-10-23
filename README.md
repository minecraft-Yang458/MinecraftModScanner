# Minecraft Mod Scanner

一个使用 **C++17 + Qt 6 + CMake** 编写的 Minecraft 模组自动检索桌面程序。它扫描 `mods` 文件夹中的 `.jar` 文件名，清洗出检索名称，异步查询 **Modrinth** 和 **CurseForge**，把两个平台的结果展示在同一张表中，并支持导出 **CSV / Markdown / Excel**。可选启用 MC百科进阶检索。

本项目是完整可构建工程，不是示例片段。构建产物为单个 EXE（运行时依赖 Qt DLL 与平台插件）。

---

## 功能概览

- 扫描 `.jar` 文件，默认仅当前目录，可设置递归子目录。
- 文件名多级清洗：
  - 去 `.jar`
  - 删除 `[]`、`【】`、`［］` 及其中的内容（例如中文名/版本前缀）
  - token 级去除 `beta` / `alpha` / `snapshot`
  - 去除版本段：`1.20.1`、`v0.5.8`、`mc1.20.1`、`MC_1.20.1`、`build.10`、`V29`
  - 去除尾部打包标记：`hotfix`、`universal`、`all`、`slim`、`RE`、`release`、`NOBIOMES`
  - 加载器词清洗（`fabric`、`forge`、`neoforge`、`quilt`、`liteloader`、`rift`、`modloader`）
  - 去除 `+`/`-`/`_` 并合并多余分隔符
- 主界面显示：原始文件名、基础清洗名、检索用名称、两平台名称/ID/作者/许可证/直链、MC百科链接、状态备注、操作按钮。
- 菜单栏“关于”提供程序信息与 CC0 1.0 许可证说明；程序版本 `1.1.0`。
- 异步网络：
  - 每个平台独立的并发数、最小请求间隔、超时、总尝试次数。
  - 404 不重试；408/425/429/5xx/超时/网络错误重试。
  - 429 优先遵守 `Retry-After`，否则指数退避。
  - 所有请求通过 `QNetworkAccessManager`，不为请求创建线程；UI 不阻塞。
- 检索前并行检查 Modrinth / CurseForge 连通性；CurseForge 同时验证 API Key。
- 进度条数字统一使用阿拉伯数字 `0-9`，并显式指定无衬线 UI 字体，避免系统字体替换。
- 取消时保留已完成结果，中止已发出的请求。
- 操作列动态菜单：复制 TSV、打开直链、移除结果、详细原始信息、重新检索、手动检索、错误详情、删除整行。
- 单独检索窗口：原样使用输入名称，多平台、多候选、多选，添加到表格或更新原行。
- 总结视图：按许可证、按检索状态分组，可重新检索 / 复制名称 / 手动检索，双击跳转主表。
- 导出 CSV、Markdown、Excel（`.xlsx`）。Excel 由内置最小 OOXML 生成器实现，**不依赖 QXlsx**。
- 可选 MC百科检索：
  - 优先使用非官方 JSON API：[`mcmod-api`](https://github.com/zkitefly/mcmod-api)。
  - API 不可用或未精确匹配时回退到 MC百科搜索页链接。
  - 不计入主进度，失败不影响主检索和导出。
- QSettings 持久化设置；非法值回退默认值。
- API Key 与 User-Agent 不写入日志。

---

## 环境要求

- **C++17** 编译器
- **Qt 6.5 或更高版本**，至少包含 `Core`、`Gui`、`Widgets`、`Network`；测试还需要 `Test`
- **CMake 3.21+**
- Windows 推荐 Qt 官方 MinGW 套件或 MSVC 2022；Linux/macOS 使用发行版 Qt 6

本项目已在 **Qt 6.11.2 + MinGW 13.1 + CMake 3.30.5** 上实际构建通过。

无第三方导出库依赖（不需要 QXlsx、libxlsxwriter 等）。

---

## 构建

### Windows：使用 build.ps1（推荐）

`build.ps1` 会自动寻找 Qt 6 Kit、Qt 自带的 MinGW/CMake、配置并构建，还可选跑测试和部署 Qt 运行时。

```powershell
# 清理后构建 Release，并运行核心测试
.\build.ps1 -Clean -Test

# 指定 Qt Kit
.\build.ps1 -QtDir "D:\Qt\6.11.2\mingw_64" -Clean -Test

# 构建 Debug
.\build.ps1 -Configuration Debug

# 构建并生成可直接分发的 dist 目录
.\build.ps1 -Deploy

# 部署后直接启动
.\build.ps1 -Deploy -Run
```

如果系统执行策略禁止运行脚本：

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Clean -Test
```

常用参数：

| 参数 | 说明 |
|---|---|
| `-QtDir <path>` | 指定 Qt Kit；省略时检查 `QTDIR`、`qmake6`、`C:\Qt`、`D:\Qt` |
| `-BuildDir <path>` | 构建目录，默认 `build`，必须在工程目录内 |
| `-Configuration Debug/Release/...` | 构建配置，默认 `Release` |
| `-Clean` | 删除构建目录后重新配置 |
| `-Test` | 构建后运行 CTest |
| `-Deploy` | 复制到 `dist` 并执行 `windeployqt` |
| `-Run` | 构建/部署完成后启动程序 |
| `-Jobs <n>` | 并行构建任务数，默认逻辑处理器数 |
| `-Generator <name>` | 覆盖 CMake 生成器，例如 `"Visual Studio 17 2022"` |

脚本会优先选择最新的 Qt 6 64 位 Kit；同版本下优先 MinGW。MSVC Kit 默认使用
`Visual Studio 17 2022`。

### Windows：手动构建（Qt 官方 MinGW 套件）

假设 Qt 安装于 `D:/Qt/6.11.2/mingw_64`，MinGW 位于 `D:/Qt/Tools/mingw1310_64`：

```powershell
$env:Path = "D:\Qt\6.11.2\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;D:\Qt\Tools\CMake_64\bin;" + $env:Path

cmake -S . -B build -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="D:/Qt/6.11.2/mingw_64" `
  -DCMAKE_CXX_COMPILER="D:/Qt/Tools/mingw1310_64/bin/g++.exe"

cmake --build build -j
```

生成：

```text
build/MinecraftModScanner.exe
```

也可以直接用 Qt Creator 打开 `CMakeLists.txt`，选择对应 Kit 构建。

### Windows：MSVC 2022

```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64"
cmake --build build-msvc --config Release
```

### Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

macOS 会生成 `.app` 包；仍建议使用对应 Qt Kit。

### 运行时的 Qt DLL

Windows 构建默认在链接完成后自动运行 `windeployqt`，把 `Qt6Core.dll`、`Qt6Gui.dll`、
`Qt6Network.dll`、`Qt6Widgets.dll`、`platforms/qwindows.dll` 等复制到 EXE 所在目录。
因此 `build\MinecraftModScanner.exe` 可以脱离 Qt 的 `PATH` 直接启动。

自动部署由 CMake 选项控制，默认开启：

```powershell
# 关闭自动部署（需要自己配置 Qt DLL 或 PATH）
cmake -S . -B build -DMMS_DEPLOY_QT_RUNTIME=OFF
```

发布为独立目录：

```powershell
.\build.ps1 -Deploy
```

该命令会额外生成 `dist/`，包含 EXE 和完整 Qt 运行时，可直接打包分发。

如果旧构建仍提示缺少 `Qt6Network.dll`，请重新执行：

```powershell
.\build.ps1 -Clean -Test
```

或手动对旧 EXE 执行：

```powershell
D:\Qt\6.11.2\mingw_64\bin\windeployqt.exe --release --no-translations build\MinecraftModScanner.exe
```

或使用 CMake 安装与 Qt 部署脚本：

```powershell
cmake --install build --prefix dist
```

Qt 的部署脚本会调用 `windeployqt`，把所需 DLL 和 `platforms/qwindows.dll` 等插件放入输出目录。

---

## 运行与使用

1. 启动程序。
2. 点击 **选择 mods 文件夹**，选择包含 `.jar` 的目录；扫描完成后会弹出提示。
3. （可选）在密码框填入 **CurseForge API Key**，点击 **验证 Key / 检查连接**。
4. 点击 **开始检索**。
   - 未填写 CurseForge Key 时会弹窗询问；选择“仅检索 Modrinth（不填写）”会禁用 CurseForge。
   - 程序会先并行检查两个平台的连通性，再开始检索。
5. 观察进度条和状态文本；可随时点击 **取消**，已有结果会保留。
6. 完成后会显示汇总。可用 **工具 → 总结视图…** 查看分组统计。
7. 用 **导出** 按钮或菜单导出 CSV / Markdown / Excel。
8. 每行最后一列 **操作** 打开按平台分组的动态菜单；主检索进行中该列被禁用。

### 操作列语义

- **复制**：复制该平台结果摘要 TSV：名称、ID、作者、许可证、直链。
- **打开直链 / 复制直链**：调用系统默认浏览器打开，或单独复制 URL。
- **移除结果**：仅清空该平台结果与状态，行保留，状态变为未检索。
- **详细检索原始信息 / 错误详情**：只读详情对话框，显示请求 URL、HTTP 状态、耗时、总尝试次数、重试次数、时间、原始响应 JSON，可复制。
- **重新检索**：同平台、同检索名称重新请求，成功后更新原行。
- **手动检索…**：打开单独检索窗口，预填检索名称；选中的结果更新原行，不新增行。
- **删除整行**：需确认，之后从表格删除。

表格中的 Modrinth / CurseForge / MC百科链接单元格支持**单击直接打开浏览器**；单独检索结果表支持**双击 URL 打开**。

### 浏览器打开与故障处理

Windows 下按以下顺序尝试打开链接：

1. Win32 `ShellExecuteW`（使用完整百分号编码 URL）；
2. `QDesktopServices::openUrl`；
3. `explorer.exe <url>`；
4. `rundll32 url.dll,FileProtocolHandler <url>`。

如果全部失败，程序会：

- 自动把完整链接复制到剪贴板；
- 弹出可读错误对话框，并由“详细信息”显示系统错误码；
- 提示在 Windows“设置 → 应用 → 默认应用”中重新设置默认浏览器。

常见原因：默认浏览器未设置、浏览器关联损坏、链接被安全软件拦截，或程序以与浏览器不兼容的权限运行。可先复制链接到浏览器手动打开，再修复系统默认应用关联。日志文件夹也使用同一套打开逻辑。

### 单独检索

**工具 → 单独检索…**：

- 输入名称原样使用，不做任何清洗。
- 默认勾选两个平台；没有 CurseForge Key 时 CurseForge 不可选。
- 结果表列出最多 10 个候选；默认不选中；支持多选。
- 选择结果后添加到主表：原始文件名显示“（手动）”，基础清洗名为空，备注“手动添加”。
- 若已存在相同平台 + 相同 ID，会先确认。
- 主检索进行中时禁止打开。

### 总结视图

- **按许可证**：MIT、Apache-2.0、GPL-3.0、LGPL、All Rights Reserved、多许可证、未知许可证、未检索/未找到。
- **按检索状态**：成功、404未找到、请求失败、跳过、清洗异常、待检索、用户取消。
- 许可证会归一化；多许可证行可同时出现在具体许可证组和“多许可证”组。
- 非成功条目右侧提供 **重新检索 / 复制名称 / 手动检索**。
- 双击条目跳转并选中主表格对应行。
- 顶部有统计摘要和刷新按钮；模型变化时会自动刷新。

---

## 设置说明

**设置 → 首选项…**，设置变更只影响下一次检索，不打断当前任务。

### 网络

- **User-Agent**：默认
  `MinecraftModScanner/1.1.0 (Qt; contact:yang458coder@163.com)`
  - 旧版默认占位 UA 会自动迁移到该值。
- **恢复默认 UA**：只恢复输入框文本，不改变“使用浏览器 UA”勾选状态。
- 自定义 UA 为空时自动回退默认 UA。
- **`[出现问题时请勾选此项] 使用浏览器 UA`**：
  - 默认不勾选。
  - 勾选后忽略自定义 UA，使用集中定义的浏览器 UA：
    `Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36`
  - 自定义 UA 输入框会禁用并显示说明；取消勾选后恢复。
- 代理：使用系统代理，或手动 HTTP 代理（地址、端口）。

### Modrinth / CurseForge

| 设置 | Modrinth 默认 | CurseForge 默认 | 范围 |
|---|---:|---:|---:|
| 最大尝试次数 | 3 | 3 | 1–5 |
| 单次请求超时 | 15 秒 | 15 秒 | 3–60 |
| 并发请求数 | 2 | 1 | 1–8 |
| 请求最小间隔 | 300 ms | 500 ms | 0–5000 ms |

“最大尝试次数”是**总次数**，包含第一次请求，不是额外重试次数。

### 其他

- 递归扫描子目录：默认否。
- 启用 MC百科进阶检索：默认否。
- 默认导出格式：CSV / Markdown / Excel，默认 CSV。
- 记住 CurseForge API Key：默认否。
  - 勾选时会弹窗明确提示**明文存储风险**。
  - 未勾选时不会持久化 Key；已存在的旧 Key 会在保存设置时被删除。
- **恢复默认**：恢复所有默认值（保留当前输入框中的运行时 Key，但“记住 Key”会回到默认的关闭状态）。

所有数值都会校验；非法值回退到 `AppConfig` 中的唯一默认值定义。

---

## CurseForge API Key 获取

1. 打开 [CurseForge 开发者控制台](https://console.curseforge.com/)。
2. 登录并创建 API Key。
3. 将 Key 填入主界面密码框。
4. 点击 **验证 Key / 检查连接**。

没有 Key 时程序仍可完整使用 Modrinth、单独检索（Modrinth）、总结和导出。CurseForge 请求使用请求头 `x-api-key`，Key 不会放进 URL，也不会写入日志。

> 如果 CurseForge 返回 401/403，通常是 Key 无效、过期、权限不足或被服务端限制。请到控制台重新创建，或在设置中临时勾选“使用浏览器 UA”后重试连通性检查。

---

## 清洗规则细节

实现位于 `src/ScanCleaner.cpp`。

1. 去除文件名末尾 `.jar`（大小写不敏感）。
2. 删除方括号及其中内容：`[中文名]`、`【安全工艺】`、`［Forge 1.20.1］`。
   只删除括号段，不删除括号外的模组名称。
3. 以独立 token 去除 `beta`、`alpha`、`snapshot`（大小写不敏感），保留分隔符。
4. 删除版本段：
   - 点号版本：`0.5.8`、`1.20.1`、`v1.2.3`
   - 带前缀版本：`mc1.20.1`、`MC_1.20.1`、`v2`
   - 构建号：`build.10`、`build-131`、`build131`
   - 短版本号：`V29`
5. 删除尾部打包/变体标记（大小写不敏感，仅当它位于末尾时）：
   - `hotfix`、`universal`、`all`、`slim`
   - `release`、`RE`
   - `NOBIOMES`
6. 加载器清洗，此时仍保留 `+`、`-`、`_`：
   - 加载器词表集中定义在 `AppConfig::loaderWords()`。
   - 仅按独立 token 匹配，不做子串替换。
   - 当加载器词前一个字符是 `-`、`+`、`_` 或空白时删除。
   - 位于开头或前面不是分隔符的加载器词保留。
   - 基础清洗名保留加载器；检索用名称才执行这一步。
   - 删除后若为空，回退删除前结果并记录备注。
7. 把 `+`、`-`、`_` 转为空格。
8. 合并多余分隔符并 trim。
9. 最终为空时回退原始文件名并标记清洗异常。

验收示例：

| 原始 | 基础清洗名 | 检索用名称 |
|---|---|---|
| `sodium-fabric-0.5.8.jar` | `sodium-fabric` | `sodium` |
| `jei-forge-1.20.1.jar` | `jei-forge` | `jei` |
| `sodium+fabric.jar` | `sodium+fabric` | `sodium` |
| `fabric-api.jar` | `fabric-api` | `fabric api` |
| `neoforge.jar` | `neoforge` | `neoforge` |
| `ForgeConfigAPIPort.jar` | `ForgeConfigAPIPort` | `ForgeConfigAPIPort` |
| `[TaC: 绿葡萄附属包] lradd-1.20.1-0.3.0.jar` | `lradd` | `lradd` |
| `[Xaero的世界地图] xaeroworldmap-forge-1.20.1-1.46.0.jar` | `xaeroworldmap-forge` | `xaeroworldmap` |
| `【安全工艺】[1.20.1] SecurityCraft-v1.10.2.1.jar` | `SecurityCraft` | `SecurityCraft` |
| `tacz-1.20.1-1.1.8-hotfix.jar` | `tacz` | `tacz` |
| `rhino-forge-2001.2.3-build.10.jar` | `rhino-forge` | `rhino` |
| `ModernUI-Forge-1.20.1-3.12.0.1-universal.jar` | `ModernUI-Forge` | `ModernUI` |
| `I18nUpdateMod-3.7.0-all.jar` | `I18nUpdateMod` | `I18nUpdateMod` |
| `rubidium-extra-0.5.4.4+mc1.20.1-build.131.jar` | `rubidium-extra` | `rubidium extra` |
| `CreativeCore_FORGE_v2.12.40_mc1.20.1.jar` | `CreativeCore_FORGE` | `CreativeCore` |
| `endofdays-1.9.0-RE-slim.jar` | `endofdays` | `endofdays` |
| `fancymenu_forge_3.9.14_MC_1.20.1.jar` | `fancymenu_forge` | `fancymenu` |
| `MTS Official Pack-1.20.1-V29.jar` | `MTS Official Pack` | `MTS Official Pack` |
| `NaturalDecorMod_NOBIOMES_1.20.1_V1.9.5.jar` | `NaturalDecorMod` | `NaturalDecorMod` |

---

## 网络与重试策略

- 开始检索前检查启用的平台连通性。
- 每个任务独立按平台配置调度：
  - 同时在途请求数不超过“并发请求数”。
  - 两次请求启动至少间隔“最小请求间隔”。
  - 单次请求由独立 `QTimer` 超时；超时后 `abort()`。
- 重试规则：
  - **HTTP 404**：立即判定未找到，不重试。
  - **HTTP 429**：重试；有 `Retry-After` 时优先按它等待。
  - **HTTP 408 / 425 / 5xx、超时、网络错误**：重试。
  - 其他 4xx：直接失败，不重试。
  - 200 但 JSON 解析失败：按可重试错误处理。
- 退避：约 `800ms × 2^(attempt-1) + 0–250ms 抖动`，上限 10 秒；429 的 `Retry-After` 最长等待 10 分钟。
- 状态文本显示“当前文件 · 当前平台 · 检索中 / 重试 n/m / 404 未找到 / 成功 / 失败”。
- 总进度 = 参与检索的扫描行数 × 启用平台数；MC百科单独提示，不占主进度。

---

## 导出

- **CSV**：UTF-8 BOM + CRLF，字段全部加引号并转义，Excel 可直接打开。
- **Markdown**：标准 Markdown 表格，转义 `|`、换行等。
- **Excel**：内置最小 OOXML `.xlsx` 写入器（ZIP Store + inline strings），支持表头样式、冻结首行、自动筛选、列宽。
- 导出包含表格当前所有列（最后一列“操作”为空）和全部行，包括手动添加行。
- 导出前选择保存路径；默认文件名带时间戳。
- 默认导出目录为应用数据目录下的 `exports`，与 `settings.ini`、日志处于同一已确认可写的位置：
  `C:\Users\<用户>\AppData\LocalLow\MinecraftModScanner\MinecraftModScanner\exports`。
  如果该目录不可写，依次尝试用户主目录下的 `MinecraftModScannerExports`、
  程序目录下的 `exports` 和系统临时目录。
- 保存对话框使用 Qt 自带实现，不使用 Windows 原生外壳对话框，避免系统对目标目录的误判。
- 导出菜单提供 **快速导出到默认目录**，可完全绕过保存对话框。
- 导出菜单提供 **打开导出文件夹**。
- 如果写入失败，会显示目标路径和具体错误，并提供“改存到默认目录”的兜底选项。

> 程序**不需要管理员权限**。如果标准用户下导出被拒绝，请先使用“快速导出到默认目录”
> 或检查该目录 ACL/受控文件夹访问。临时以管理员身份运行可以绕过部分目录权限，
> 但会触发 UAC，并可能影响浏览器拉起与拖放；不建议把管理员权限作为长期方案。

本项目**没有使用 QXlsx**，因此 Excel 功能无需可选依赖，也不会静默缺失。

---

## MC百科进阶检索

设置中默认关闭。开启后，在平台检索完成后，对每个有成功结果的行，使用平台返回的模组名称查询 MC百科。

实现位于 `src/McModWikiClient.cpp`：

- 优先请求非官方 API：
  `https://mcmod-api.zkitefly.eu.org/s/key=<uriencoded name>`
- 解析 JSON 数组的 `address`、`title`、`data.chinese_name`、`data.sub_name`、`data.abbr`。
- 只取 `/class/` 条目，按名称/缩写/副标题/标题评分，优先精确匹配。
- API 失败或没有匹配时，返回：
  `https://search.mcmod.cn/s?key=<name>`
- 限制：单并发、请求启动间隔约 1.2 秒、最多 2 次尝试，遵守 429 `Retry-After`。
- 失败不影响主检索和导出，只在状态/备注和 MC百科列 tooltip 提示。

参考项目：[zkitefly/mcmod-api](https://github.com/zkitefly/mcmod-api)（非官方，MIT License，数据版权归 MC百科）。

---

## 持久化与安全

- 配置使用 `QSettings` 的 **IniFormat**，保存到 Qt 的 `AppLocalDataLocation/settings.ini`。
  - Windows 通常为 `%USERPROFILE%\AppData\LocalLow\MinecraftModScanner\MinecraftModScanner\settings.ini`。
  - Linux 通常为 `~/.local/share/MinecraftModScanner/settings.ini`。
  - 具体路径由 Qt 的 `QStandardPaths` 决定，程序启动日志会打印该路径。
- 日志位于 Qt 的 `AppLocalDataLocation/logs/mcmodscanner.log`。
  - 本机测试路径为 `%USERPROFILE%\AppData\LocalLow\MinecraftModScanner\MinecraftModScanner\logs\mcmodscanner.log`。
  - 单文件超过 2 MB 时会轮转为 `.1`。
- 日志只记录平台、查询名、HTTP 状态、耗时、尝试次数和错误码；**不记录 API Key、请求头或 User-Agent**。
- API Key 默认不持久化。勾选记住后以**明文**存入 QSettings；程序在保存前会提示风险。
- 建议不要把配置文件或日志上传到公共位置。

---

## 测试

核心单元测试覆盖清洗示例、加载器规则、默认值回退、QSettings 持久化、许可证归一化、CSV/Markdown/XLSX 生成：

```powershell
cmake --build build -j
ctest --test-dir build --output-on-failure
```

或直接运行：

```powershell
.\build\mcmodscanner_tests.exe
```

可选的实时网络冒烟测试（会访问 Modrinth 与 mcmod-api）：

```powershell
cmake -S . -B build -DMMS_BUILD_NETWORK_TESTS=ON
cmake --build build --target mcmodscanner_network_tests -j
.\build\mcmodscanner_network_tests.exe
```

这些测试默认不启用，避免 CI 依赖外部网络。

可选的界面渲染冒烟测试（使用合成数据，打开主窗口、总结视图、设置和单独检索对话框并截图）：

```powershell
cmake -S . -B build -DMMS_BUILD_UI_TESTS=ON
cmake --build build --target mcmodscanner_ui_tests -j
$env:MMS_UI_SCREENSHOT_DIR = "$PWD\ui-shots"
.\build\mcmodscanner_ui_tests.exe
```

该测试默认关闭，不会影响正式构建。

---

## 数字显示

程序生成的数值统一使用 ASCII 阿拉伯数字 `0-9`：

- 进度条当前值/总数、成功/未找到/失败计数、尝试次数、HTTP 状态码；
- 设置中的次数、超时、并发、间隔、代理端口；
- 日期和时间，例如 `2026-10-23 20:23:19`；
- 导出文件名时间戳。

实现上启动时固定 `QLocale::setDefault(QLocale::c())`，QSpinBox/QProgressBar 使用
`QLocale::c()`，进度文本由 `QString::number` 直接生成，不再依赖 `%v`/`%m` 的本地化替换。
同时为界面指定带阿拉伯数字的 UI 字体族（Segoe UI / Microsoft YaHei UI / Noto Sans 等），
避免系统字体替换导致数字显示成其他符号。

用户原始文件名和 API 返回的原始字段保持原样，不会被改写。

---

## 已知限制

1. **CurseForge 许可证字段不稳定**：CurseForge 搜索响应经常不提供许可证，列会显示“未知”，总结归入“未知许可证”。
2. 只按名称检索，并取最相关的一个项目展示；不判断 Minecraft 版本、加载器兼容性、文件版本，不读取 jar 内部的 `mods.toml` / `fabric.mod.json`。
3. 两个平台名称可能不同，表格按平台分别展示；同一个项目的跨平台映射依赖名称匹配。
4. MC百科使用非官方 API，接口可能变更或不可用；匹配使用启发式评分，可能指向同名/相似模组。失败时会回退到搜索页。
5. 不支持中文名反向匹配英文项目名（除非平台接口本身返回该名称）。
6. Excel 文件使用最小 OOXML 结构，只保证内容与基本样式，不包含公式、图片、复杂格式或图表。
7. 不内置 HTTP 代理认证（用户名/密码），仅支持地址和端口。
8. 不提供安装包、自动更新或代码签名。
9. 日志和数据没有加密；如选择记住 API Key，风险由用户承担。
10. 文件名清洗基于 token/尾部标记的启发式规则，极端命名可能误删或漏删；可在 `ScanCleaner.cpp` 中调整版本正则和尾部标记表。
11. 打开浏览器依赖操作系统的默认应用关联；若系统未设置默认浏览器，程序会复制链接并提示错误，但无法代替系统修复关联。
12. 若系统安全软件或权限策略禁止写入所有可选目录，导出会失败；可使用“快速导出到默认目录”或检查目录权限/受控文件夹访问设置。
13. 管理员权限不是必需条件。以管理员身份运行可能绕过部分目录限制，但会增加权限风险，并可能影响默认浏览器启动；优先使用应用数据导出目录。

---

## 工程结构

```text
CMakeLists.txt
build.ps1                      Windows 自动构建/测试/部署脚本
README.md
LICENSE                        CC0 1.0 许可
resources/
  app.svg
  style.qss
src/
  main.cpp
  AppConfig.h/.cpp             配置结构、默认值、QSettings 持久化
  AppLog.h/.cpp                文件日志
  SearchTypes.h/.cpp           平台/状态/结果数据类型
  LicenseUtils.h/.cpp          许可证归一化与分组
  ScanCleaner.h/.cpp           扫描与文件名清洗
  HttpClient.h/.cpp            异步 GET、超时、Retry-After、代理/UA
  BasePlatformClient.h/.cpp    平台调度、并发、限速、重试
  ModrinthClient.h/.cpp        Modrinth 搜索与连通性
  CurseForgeClient.h/.cpp      CurseForge 搜索、Key 验证
  NetworkTaskManager.h/.cpp    两个客户端的管理与信号转发
  McModWikiClient.h/.cpp       mcmod-api 查询与搜索页回退
  ResultModel.h/.cpp           主表格数据模型与导出数据
  ActionButtonDelegate.h/.cpp  操作列按钮绘制/点击
  ExportService.h/.cpp         CSV / Markdown / 内置 XLSX
  DetailsDialog.h/.cpp         原始信息/错误详情
  UrlLauncher.h/.cpp           默认浏览器打开与失败兜底
  SettingsDialog.h/.cpp        设置界面
  ManualSearchDialog.h/.cpp    单独检索
  SummaryDialog.h/.cpp         总结视图
  MainWindow.h/.cpp            主窗口与动作控制
tests/
  test_core.cpp                核心单元测试
  test_network.cpp             可选实时网络冒烟测试
```

---

## 数据来源与许可

- 本项目原创源代码：**CC0 1.0 Universal（公共领域贡献）**
  - made by **DeepSeek-V4.1-Flash(1017)**
  - 详见 [LICENSE](LICENSE)
- [Modrinth API](https://docs.modrinth.com/api/)
- [CurseForge API](https://docs.curseforge.com/)
- [mcmod-api](https://github.com/zkitefly/mcmod-api)（非官方，MIT）
- Qt 6 遵循其 LGPLv3 / 商业许可；发布时请遵守 Qt 许可并随程序提供相应许可信息。
- MC百科页面内容版权归 MC百科及原编辑者所有；本程序仅链接和展示检索结果摘要，不批量抓取正文。
- CC0 仅覆盖本项目原创源代码，不覆盖上述第三方组件、API、数据或商标。
