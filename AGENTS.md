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
libcef_dll/       C→C++ 桥接层（由工具自动生成）
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
    arkweb_render_handler_ext.h  渲染回调扩展
    cef_form_handler.h    表单处理
    cef_media_handler.h   媒体播放器
    cef_web_storage.h     Web 存储
    ...                   权限、下载、安全浏览等
  libcef/
    browser/              浏览器进程扩展实现
      alloy/              Alloy 模式扩展（browser_host_impl_ext 等）
      devtools/           DevTools 扩展（arkweb 目录下含 DevTools 消息处理）
      javascript/         JS Bridge（oh_gin_javascript_bridge_*）
      net/                网络扩展（applink、url_rewrite、extra_headers）
      osr/                离屏渲染扩展
      permission/         权限管理（geolocation、generic permission）
      storage/            Web 存储
      printing/           打印
      policy/             浏览器策略
      password/           密码存储
      ohos_safe_browsing/ 安全浏览（URL 信任列表、恶意站点检测）
      adblock/            广告过滤
      anti_tracking/      反追踪（第三方 Cookie 策略）
      ...                 下载、媒体、UA、全局配置等
    common/               多进程共享扩展
      mojom/              Mojo IPC 定义
      javascript/         JS Bridge 公共类型
    renderer/             渲染进程扩展
      javascript/         JS Bridge 渲染端实现
      extensions/         扩展渲染支持
```

### 测试和工具

```
tests/
  cefsimple/       最小浏览器示例（学习入口）
  cefclient/       完整功能演示（100+ 源文件）
  ceftests/        gtest 自动化测试
  shared/          测试共享工具
tools/
  translator.py    cpptoc/ctocpp 代码生成器（核心工具）
  make_*.py        API/版本头文件生成
  patch.sh         Chromium 补丁管理
  distribute/      发布打包
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

## 知识路由

| 遇到什么问题 | 读什么 | 关键概念 |
|-------------|--------|---------|
| 要调 CDP 命令（DevTools） | `include/cef_browser.h` → SendDevToolsMessage / ExecuteDevToolsMethod | CDP 消息是 JSON 格式，异步回调通过 CefDevToolsMessageObserver 接收结果 |
| 要添加新的 Handler 回调 | `include/cef_client.h` → GetXXXHandler 方法 | CEF 用 Handler 模式，所有回调通过 CefClient 注册 |
| 要操作 DOM | `include/cef_dom.h` | DOM 访问必须在渲染进程，通过 VisitDom 回调获取 CefDomDocument |
| 要在渲染进程执行 JS | `include/cef_v8.h` → ExecuteFunction / ExecuteFunctionWithContext | V8 上下文必须在渲染进程 UI 线程使用 |
| 要操作 Frame | `include/cef_frame.h` | 主 Frame = 主页面，子 Frame = iframe。SendDevToolsMessage 在 Frame 级别操作 |
| 要理解 C↔C++ 桥接 | `libcef_dll/cpptoc/` + `libcef_dll/ctocpp/` + `tools/translator.py` | cpptoc = 宿主→CEF，ctocpp = CEF→宿主；由 translator.py 自动生成，不要手动改 |
| 要做 OpenHarmony 特有功能 | `ohos_cef_ext/` | 所有 OH 扩展在这个目录。编译由 `cef_arkweb_extension.gni` 控制 |
| 要理解编译条件开关 | `ohos_cef_ext/cef_arkweb_extension.gni` | is_arkweb_ext、arkweb_devtools、arkweb_useragent、arkweb_ex_fallback_proxy 等 GN 变量 |
| 要理解安全浏览机制 | `ohos_cef_ext/libcef/browser/ohos_safe_browsing/` | URL 信任列表 + 恶意站点检测 + 安全浏览 TabHelper |
| 要理解 JS Bridge | `ohos_cef_ext/libcef/browser/javascript/` + `renderer/javascript/` | Gin 框架实现，命名规则 oh_gin_javascript_bridge_* |

## 专家经验与约束

### 硬约束（必须遵守）

- `include/` 和 `libcef/` 是上游 CEF 代码，**不要直接修改**。修改必须通过 `patch/` 目录提交补丁
- `libcef_dll/cpptoc/` 和 `ctocpp/` 由 `tools/translator.py` 自动生成，**禁止手动编辑**
- OpenHarmony 扩展代码只放在 `ohos_cef_ext/` 目录内
- 新增 OH 扩展源文件必须在 `ohos_cef_ext/cef_arkweb_extension.gni` 中注册
- CDP 调用（SendDevToolsMessage）是异步的，结果通过 CefDevToolsMessageObserver 回调，不能同步等待
- V8 操作（`cef_v8.h`）和 DOM 操作（`cef_dom.h`）只能在渲染进程中使用

### 依赖关系禁止

- ❌ `ohos_cef_ext/` 代码不应直接 include `libcef/` 的内部头文件（使用 `include/` 中的公共 API）
- ❌ 浏览器进程代码不能直接调用渲染进程对象（必须通过 Mojo IPC）
- ❌ `include/` 中的接口类不应依赖 `ohos_cef_ext/` 的类型

### 反模式

- 不要在 `AlloyBrowserHostImpl` 的继承链中添加虚方法来扩展功能，应该用 `ohos_cef_ext/` 的 `_ext` 后缀类扩展
- 不要用 `Runtime.evaluate` 做 JS 注入来操控页面，应走 CDP 协议（SendDevToolsMessage / ExecuteDevToolsMethod）
- 不要忽略 `*_for_include.cc` 文件的作用——它们是在编译时通过 include 合并的方式注入到主实现类中的扩展点

### 关键黑话

| 术语 | 含义 |
|------|------|
| Alloy 模式 | CEF 两种模式之一（Alloy / Chrome），Alloy 不使用 Chrome 外壳，适合嵌入式 |
| OSR | Off-Screen Rendering，离屏渲染，不使用系统窗口 |
| cpptoc / ctocpp | C++→C / C→C++ 桥接层，由 translator.py 自动生成 |
| Handler 模式 | CEF 通过 CefClient 注册各种 Handler 回调（display、load、request 等） |
| Frame | 页面帧，主 Frame = 主页面，子 Frame = iframe |
| _ext 后缀 | OpenHarmony 扩展文件，如 `arkweb_browser_host_ext.h` |
| _for_include | 编译时 include 注入文件，扩展上游类而不修改源码 |
| Mojo IPC | Chromium 进程间通信机制，浏览器进程 ↔ 渲染进程 |

## 编译和测试

本仓库不能独立编译，必须在 Chromium 源码树中作为 `cef/` 子目录编译。

### 编译前置

1. 需要完整的 Chromium 源码（含 `src/` 目录）
2. CEF 代码放置在 `src/cef/`
3. 运行 `cef_create_projects.sh` 生成 GN 构建文件

### 编译命令

```sh
# 从 Chromium src/ 目录执行
# GN 参数配置
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
# 运行 cefsimple 验证基本渲染能力
./out/Default/cefsimple https://example.com

# 运行 ceftests 自动化测试（gtest）
./out/Default/ceftests --gtest_filter=*
```

### OpenHarmony 平台验证

OpenHarmony 平台的完整构建从 OH 源码根目录执行，不在本仓库内操作。编译产物为 ArkWebCore.hap。

提交使用 `git commit -s`，保留 DCO Signed-off-by 签名。
