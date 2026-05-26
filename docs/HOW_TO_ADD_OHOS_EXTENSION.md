# 如何添加 OpenHarmony 扩展

本指南说明如何在 chromium_cef 仓库中添加新的 OpenHarmony 扩展代码。

## 概述

OpenHarmony 扩展代码全部放在 `ohos_cef_ext/` 目录内，通过 GN 条件编译（`is_arkweb_ext`）控制是否参与构建。扩展使用 `_ext` 后缀命名和 `*_for_include.cc` 注入机制，在不修改上游 CEF 源码的前提下扩展功能。

### 扩展方式对比

| 方式 | 适用场景 | 示例 |
|------|---------|------|
| `_ext` 后缀文件 | 新增独立功能模块 | `arkweb_browser_host_ext.h` |
| `_for_include.cc` | 扩展上游类的已有方法 | `browser_host_base_for_include.cc` |
| GN 条件编译 | 可选功能开关 | `arkweb_devtools`、`arkweb_ex_fallback_proxy` |

## 添加新的扩展源文件

### 步骤 1：在 ohos_cef_ext 中创建文件

```
ohos_cef_ext/
  include/            ← 接口定义（.h）
    my_feature_ext.h
  libcef/
    browser/          ← 浏览器进程实现
      my_feature_ext.cc
      my_feature_ext.h
```

命名规则：
- 文件名：`arkweb_<feature>_ext.h/.cc` 或 `cef_<feature>_ext.h/.cc`
- 类名：`ArkWeb<Feature>Ext` 或 `Cef<Feature>Ext`
- 优先使用 `ArkWeb` 前缀

### 步骤 2：注册到编译配置

在 `ohos_cef_ext/cef_arkweb_extension.gni` 中添加源文件：

```gni
libcef_arkweb_sources += [
  "//cef/ohos_cef_ext/include/my_feature_ext.h",
  "//cef/ohos_cef_ext/libcef/browser/my_feature_ext.cc",
  "//cef/ohos_cef_ext/libcef/browser/my_feature_ext.h",
]
```

如果功能有编译条件开关：

```gni
if (defined(arkweb_my_feature) && arkweb_my_feature) {
  libcef_arkweb_sources += [
    "//cef/ohos_cef_ext/libcef/browser/my_feature_impl.cc",
    "//cef/ohos_cef_ext/libcef/browser/my_feature_impl.h",
  ]
}
```

### 步骤 3：使用 _for_include.cc 扩展上游类

`_for_include.cc` 文件通过 `#include` 方式注入到上游实现类的编译单元中，可以访问上游类的私有成员：

```cpp
// ohos_cef_ext/libcef/browser/my_feature_for_include.cc
// 这个文件会被 #include 到上游 browser_host_base.cc 中

void CefBrowserHostBase::MyNewMethod() {
  // 可以访问 CefBrowserHostBase 的私有成员
}
```

上游文件中对应的 include 声明：

```cpp
// libcef/browser/browser_host_base.cc（上游文件，不修改）
// 已有的 include 块中：
#include "cef/ohos_cef_ext/libcef/browser/my_feature_for_include.cc"  // OH 扩展注入
```

**注意**：如果上游文件没有预留 include 注入点，需要通过 `patch/` 提交补丁。

## 添加 Chromium 补丁

如果必须修改上游代码：

1. 在 `patch/patches/` 下创建 `.patch` 文件
2. 在 `patch/patch.cfg` 中注册：

```python
patches = [
  # ... 已有补丁 ...
  {
    # 补丁说明 + 关联的 issue/CL 链接
    'name': 'my_ohos_feature',
    'condition': 'IS_OHOS',  # 可选条件
  },
]
```

3. 补丁通过 `cef_create_projects.sh` 自动应用

## 检查清单

- [ ] 新文件放在 `ohos_cef_ext/` 目录内
- [ ] 文件命名使用 `_ext` 后缀
- [ ] 源文件已注册到 `cef_arkweb_extension.gni`
- [ ] 不直接 include `libcef/` 内部头文件
- [ ] 不手动编辑 `libcef_dll/` 下的自动生成文件
- [ ] 如需修改上游，通过 `patch/` 提交补丁
