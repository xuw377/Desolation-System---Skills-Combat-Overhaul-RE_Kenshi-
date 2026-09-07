# Kenshi Mods (GPL-3.0)

Kenshi 本地代码注入模组合集，基于 [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) + [KenshiLib](https://github.com/BFrizzleFoShizzle/KenshiLib) 插件框架。
本仓库包含全部四个模组的**完整源码**，以 **GNU GPL-3.0** 许可证发布（与前置 RE_Kenshi / KenshiLib 的许可证保持一致）。

## 模组一览

### 绝境系统 (Desolation System)
技能与战斗大修：27 项宗师专长 + 五大极境天道（肉身成圣 / 万法皆通 / 幽冥主宰 / 机械至尊 / 荒原主宰）+ 流法系统（近战三流派）+ 锻造宗师（人剑合一 / 钢铁之躯）+ 游戏内可点击 UI（模式 / 攻速 / 极境 / 流法切换，专长实时面板）。

### 一键断肢 (One-Click Amputation)
游戏内可点击小面板，一键断肢并自动止血回血。支持骨人/人类/沙克/蜂人等所有人形角色，呼出键可自定义，面板支持中英文切换。

### 中文输入面板 (Cn Input Panel)
游戏内拼音组字面板：点击候选字组句 → 复制 → 粘贴进游戏输入框，绕过 Kenshi 无输入法接口的限制。360 音节表外置可扩充。

## 仓库结构

```
DesolationSystem/      主插件源码 (机制钩子)
DivineGraceUI/         UI 插件源码 (游戏内面板)
SkeletonLimbHotSwap/   一键断肢源码
CnInputPanel/          中文输入面板源码
build/                 构建脚本 (clang-cl + lld-link)
LICENSE                GNU GPL-3.0
```

## 构建说明

- 编译器：clang-cl (LLVM/Clang, MSVC 兼容模式) + lld-link；MSVC 的 STL/Windows SDK 头文件
- 依赖头文件：KenshiLib 的 `Include/`（kenshi / ogre / mygui / ois 反编头，含 KenshiLib.lib 导入库）
- 路径配置：`env_loader.ps1` 读取 `D:\kenshimods\env.json`（缺失时回退内置默认路径，按你本机布局修改）
- 单项构建：`powershell -ExecutionPolicy Bypass -File build/build_desolation_system.ps1` 等
- 构建产物自动部署到 `D:\steam\steamapps\common\Kenshi\mods\<模组名>\`

## 前置要求（运行时）

- [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) 与 KenshiLib —— 本模组的插件加载与引擎桩全部依赖它们
- 绝境系统还需游戏内 UI 渲染（RE_Kenshi 的 Datapanel 桥）

## 许可证

本仓库全部源码以 [GNU GPL-3.0](LICENSE) 发布。
基于 RE_Kenshi / KenshiLib（GPL-3.0）开发，特此致谢 BFrizzleFoShizzle 及 Kenshi 逆向社区。
