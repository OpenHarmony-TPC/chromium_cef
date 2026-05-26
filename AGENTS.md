# Chromium CEF (Chromium Embedded Framework) 指引

## 项目定位

本仓库是 CEF（Chromium Embedded Framework）的 OpenHarmony 适配版本。CEF 基于 Chromium content 层封装，为宿主应用提供嵌入式浏览器能力。本仓库在上游 CEF 基础上新增 `ohos_cef_ext/` 目录，承载 OpenHarmony 平台扩展（ArkWeb 集成、安全浏览、广告过滤、JS Bridge 等）。

## 代码地图

### 上游 CEF 核心（不应修改）

```
include/          CEF 公共 API 定义（70+ 头文件）
  cef_browser.h   浏览器生命周期和操作接口（含 SendDevToolsMessage / ExecuteDevToolsMethod）
  cef_client.h    客户端回调注册入口（handler 集合）
  cef_frame.h     帧（frame）操作接口
  cef_v8.h        V8 JS 交互接口
  views/          Views 框架（原生 UI 控件）
  wrapper/        辅助工具类（helpers、message_router、resource_manager）
  capi/           C API 定义
  internal/       平台相关内部类型
  base/           基础类型库（callback、weak_ptr、ref_counted 等）
libcef/           CEF 实现（编译进 libcef.so）
  browser/        浏览器进程实现（context、browser_host、download、devtools 等）
  common/         多进程共享代码（switches、values、request、response 等）
  renderer/       渲染进程实现（DOM、V8、frame observer、blink_glue）
libcef_dll/       C→C++ 桥接层（由 translator.py 自动生成）
  cpptoc/         C++ → C 转换（宿主调用 CEF 实现的桥）
  ctocpp/         C → C++ 转换（CEF 回调宿主的桥）
  wrapper/        高级封装（message_router、resource_manager 等）
```

### OpenHarmony 扩展（本仓库维护重点）

```
ohos_cef_ext/
  include/                OpenHarmony 扩展接口定义
    arkweb_browser_ext.h  浏览器扩展接口
    arkweb_client_ext.h   客户端扩展回调
    arkweb_frame_ext.h    帧扩展接口
    cef_form_handler.h    表单处理
    cef_media_handler.h   媒体播放器
    cef_web_storage.h     Web 存储
    ...                   权限、下载、安全浏览等
  libcef/
    browser/              浏览器进程扩展
      alloy/              Alloy 模式扩展（browser_host_impl_ext）
      devtools/           DevTools 扩展（arkweb/ 下含消息处理和前端）
      javascript/         JS Bridge（oh_gin_javascript_bridge_*）
      net/                网络扩展（applink、url_rewrite、extra_headers）
      osr/                离屏渲染扩展
      permission/         权限管理
      ohos_safe_browsing/ 安全浏览
      adblock/            广告过滤
      anti_tracking/      反追踪
      ...                 下载、媒体、UA、全局配置等
    common/               多进程共享扩展
      mojom/              Mojo IPC 定义
      javascript/         JS Bridge 公共类型
    renderer/             渲染进程扩展
      javascript/         JS Bridge 渲染端
      extensions/         扩展渲染支持
```

### 测试和工具

```
tests/
  cefsimple/       最小浏览器示例（学习入口）
  cefclient/       完整功能演示
  ceftests/        gtest 自动化测试
  shared/          测试共享工具
tools/
  translator.py    cpptoc/ctocpp 代码生成器
  make_*.py        API/版本头文件生成
  patch.sh         Chromium 补丁管理
patch/
  patch.cfg        补丁配置（每个补丁条目含 name/path/condition）
  patches/         补丁文件（通过 cef_create_projects.sh 应用）
```

## 分层架构

```
宿主应用 (ArkTS/C++/Java)
  ↓ ohos_interface / ohos_nweb (web_webview 仓库)
  ↓
ArkWebCore.hap (Chromium + CEF 合编产物)
  ├── libcef/  (CEF 核心)
  │     ↓ include/ (CEF 公共 API)
  │     ↓ libcef_dll/ (C↔C++ 桥接，自动生成)
  │     ↓ libcef/browser|common|renderer (CEF 实现)
  │
  └── ohos_cef_ext/ (OpenHarmony 扩展)
        ↓ include/ (扩展接口)
        ↓ libcef/browser|common|renderer (扩展实现)
        ↓ 编译条件：is_arkweb_ext GN 变量控制
```

## 知识索引

改动前按场景读取对应文件：

| 场景 | 先读 |
|------|------|
| 添加 OH 扩展源文件、注册到编译 | `docs/HOW_TO_ADD_OHOS_EXTENSION.md` |
| CDP/DevTools 消息收发 | `include/cef_browser.h` → SendDevToolsMessage / ExecuteDevToolsMethod |
| 添加 Handler 回调 | `include/cef_client.h` → GetXXXHandler 方法 |
| 操作 DOM | `include/cef_dom.h`（仅渲染进程可用） |
| 执行 JS | `include/cef_v8.h`（仅渲染进程 UI 线程） |
| 操作 Frame | `include/cef_frame.h` |
| C↔C++ 桥接生成 | `tools/translator.py` |
| 编译条件开关 | `ohos_cef_ext/cef_arkweb_extension.gni` |
| 安全浏览机制 | `ohos_cef_ext/libcef/browser/ohos_safe_browsing/` |
| JS Bridge | `ohos_cef_ext/libcef/browser/javascript/` + `renderer/javascript/` |
| Chromium 补丁 | `patch/patch.cfg` + `patch/README.txt` |

## 专家经验与约束

### 硬约束

- `include/` 和 `libcef/` 是上游 CEF 代码，**不要直接修改**。修改必须通过 `patch/` 目录提交补丁
- `libcef_dll/cpptoc/` 和 `ctocpp/` 由 `tools/translator.py` 自动生成，**禁止手动编辑**
- OpenHarmony 扩展代码只放在 `ohos_cef_ext/` 目录内
- 新增 OH 扩展源文件必须在 `cef_arkweb_extension.gni` 中注册
- CDP 调用（SendDevToolsMessage）是异步的，结果通过 CefDevToolsMessageObserver 回调
- V8/DOM 操作只能在渲染进程中使用

### 依赖方向禁止

- ❌ `ohos_cef_ext/` 代码不应直接 include `libcef/` 内部头文件（使用 `include/` 公共 API）
- ❌ 浏览器进程不能直接调用渲染进程对象（必须通过 Mojo IPC）
- ❌ `include/` 接口类不应依赖 `ohos_cef_ext/` 类型

### 反模式

- 不要在 AlloyBrowserHostImpl 继承链中添加虚方法，应使用 `ohos_cef_ext/` 的 `_ext` 后缀类扩展
- 不要用 Runtime.evaluate 做 JS 注入操控页面，应走 CDP 协议
- 不要忽略 `*_for_include.cc` 文件——它们通过 include 合并方式注入到主实现类中

### 关键黑话

| 术语 | 含义 |
|------|------|
| Alloy 模式 | CEF 两种运行模式之一（Alloy / Chrome），Alloy 无 Chrome 外壳，适合嵌入式 |
| OSR | Off-Screen Rendering，离屏渲染 |
| cpptoc / ctocpp | C++→C / C→C++ 桥接层，由 translator.py 自动生成 |
| Handler 模式 | CEF 通过 CefClient 注册各种 Handler 回调 |
| Frame | 页面帧，主 Frame = 主页面，子 Frame = iframe |
| _ext 后缀 | OpenHarmony 扩展文件命名约定 |
| _for_include | 编译时 include 注入文件，扩展上游类而不修改源码 |
| Mojo IPC | Chromium 进程间通信机制 |

## 编译和测试

本仓库不能独立编译，必须在 Chromium 源码树中作为 `cef/` 子目录编译。

### 编译前置

1. 需要完整 Chromium 源码（含 `src/` 目录）
2. CEF 代码放置在 `src/cef/`
3. 运行 `cef_create_projects.sh` 生成 GN 构建文件

### 编译命令

```sh
# 从 Chromium src/ 目录执行
gn args out/Default
# 添加: is_arkweb_ext=true 启用 OH 扩展

# 编译 libcef
ninja -C out/Default cef

# 编译测试
ninja -C out/Default ceftests

# 编译示例
ninja -C out/Default cefsimple
ninja -C out/Default cefclient
```

### 最小验证

```sh
# 运行 cefsimple 验证基本渲染
./out/Default/cefsimple https://example.com

# 运行 gtest 自动化测试
./out/Default/ceftests --gtest_filter=*
```

### OpenHarmony 平台

OH 平台完整构建从 OH 源码根目录执行，不在本仓库内操作。产物为 ArkWebCore.hap。

提交使用 `git commit -s`，保留 DCO Signed-off-by 签名。
