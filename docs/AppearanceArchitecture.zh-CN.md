# 外观架构

[English](AppearanceArchitecture.md)

面向 AI 贡献者。改动前读[项目边界](ProjectArchitecture.md)及[窗口渲染](WindowAppearance.zh-CN.md)。
本文说明当前设计，不是运行验收报告。

## 所有权和源码入口

| 所有者 | 契约 |
| --- | --- |
| `shell/desktop/modules/common/AppearanceConfigService.qml` | Shell 外观校验、迁移、快照 |
| 同目录 `AppearanceTokens.qml` | 派生视觉 Token，版本 9；消费者用 Token，不重复样式分支 |
| `IconAppearanceService.qml`、`BundledIcons.qml`、`BundledIcon.qml`、`SystemIconResolver.qml` | 图标持久化、命名资源、系统主题集中回退 |
| `shell/desktop/DesktopEnvironment.qml` | `appearance-settings` IPC |
| `apps/settings/` | 独立进程，经 IPC 编辑，不导入 Shell 内部 |
| `shared/qml/colorize/` | 可移植纯 JS 调色/HCT，不依赖 Quickshell |
| `shared/qml/controls/` 与 Shell 公共组件 | 通过显式主题输入复用表面和控件 |

配置归 `AppearanceConfigService`，颜色/几何/透明度/动效由 `AppearanceTokens` 派生。
Shell 形态（`windows12`/`macos`/`material`）、组件样式、玻璃预设（`liquid`/`soft`/`frosted`）、
Bar 融合/布局/显示及 Dock 配置相互独立。切换样式不重置 Dock 固定项、尺寸或独立状态。
独立应用偏好尚未与 Shell 同步，见[项目上下文](../PROJECT_CONTEXT.zh-CN.md)。

## 持久化与 IPC

`Quickshell.stateDir/appearance/config.json` 当前写入 schema **30**；完整字段以源码
`serialize()` 和加载迁移为准，不用不完整示例覆盖用户整份 JSON。保留旧版迁移、各预设光学值、校验及范围裁剪。
图标另存 `appearance/icon-appearance.json`（schema 1），含旧 Dock 一次性迁移。
经防抖、`JsonConfigStore` → 平台 `state.write` → `QSaveFile` 保存，不从 QML 执行 shell 命令。

Settings 调用 `appearance-settings` 快照/更新并取得完整快照。新增项检查端点、范围、兼容别名及 Shell/平台不可用路径。
切换玻璃应用完整预设，Material 有独立模糊策略；重置玻璃强度不等于重置所有偏好。

## 玻璃前景动态适配

玻璃协议 v10 新增 `backdrop_map`：在同一时机对整个可见面板取样，约每 8 逻辑像素一格
（最多 64×48、3072 格），每格 1 字节、首行为顶部，并沿用标量的 75ms 时间常数平滑；
本帧未完整重绘整个面板时推迟读取。原生桥接以 `backdropMapSource`（`image://kos-backdrop/...` 纹理）提供给 QML。
亮度图继续保留测量含义，包括 v10 客户端与 `backdropAt` 诊断。原生 `BackdropInkPolicy`
另生成二值 `backdropInkMapSource`：编码亮度约 0.46 为初始深色字阈值，使用 0.42/0.50
滞回与 110ms 候选确认，快速反转后等待 450ms 稳定期。25ms 定时器只在候选待确认时运行，
单次协议更新后也能完成确认；网格尺寸变化时重置历史。中央平均值不变时，KWin 也继续完成各格的平滑。

`GlassText`、`BundledIcon`、`WifiSignalIcon` 及控制中心图标从最近的 `glassContrastSource`
读取字色。同色网格直接返回深浅端点；文字已经使用该端点时，以垂直字体微调直接原生绘制，
不创建离屏层和文字衬底。混合网格才对中性字色应用 `AdaptiveInkEffect`；显式声明的 `source`
接收离屏字形纹理，保留覆盖度，对双线性插值后的二值网格再做阈值判断，避免中灰背景上的持久灰字。
相对位置逐可见帧更新，不在效果对象中监听祖先信号，仅坐标变化时写回着色器参数。
彩色、激活状态与图片原色保持不变。缺少字色网格的旧原生桥接退回整卡反馈；v9 只有标量，
v8 保持主题字色。QML 着色器源码和 `.qsb` 需一起重建。

静止且使用原生字形的 `GlassText` 通过 `pixelAlignedInk` 按窗口 DPR 将离屏绘制起点
对齐到最近的物理像素，平移至多半个物理像素，不改锚点或布局尺寸。
`AdaptiveInkEffect` 先更新该修正，再将字形位置映射到背景网格，字色分界随修正后的位置移动。
缩放/旋转、距离场文字及同色网格的原生直绘不应用修正，避免分数层起点重复采样已微调的小字。
这项修正不恢复字形转为 alpha 后丢失的 LCD 子像素覆盖度。

协议 v11 新增 `set_contrast_only`：前景探针只取自身区域背景，不绘制模糊、染色、遮罩、
边缘高光或噪声。`BackdropInkProbe` 为浮动通知标题提供独立网格，不依赖第一张卡片或滚动位置。
低于 v11 的合成器不发布探针，旧原生模块也不会激活；标题退回主题字色，不借用其他区域。
页脚玻璃按钮以自身玻璃作为对比度来源。

玻璃协议 v9 新增按面板订阅的 `set_contrast_feedback` / `backdrop_luminance`。
合成器在玻璃材质绘制后、客户端前景绘制前取样，范围限制在客户端边界及本帧已绘制的形状区域，
标量取中央一半的平均值。异步 PBO 回读有容量限制，静态间隔 150ms、检测到变化时 50ms，以 75ms 时间常数平滑亮度，
绘制钩子不等待 GPU；限频期间错过的单次背景变化
通过延迟局部重绘补采样。旧协议保持主题字色。启动器、控制中心和通知启用面板独立字色，
以 0.45/0.55 阈值缓冲、110ms 稳定确认和 90ms 字色渐变避免突变。600ms 内三次跨越阈值时，
后续换色等待 450ms 稳定期。没有稳定字色网格时，浅色文字保留轻微柔和衬底；浅色单色图标使用低透明度暗影，深色字形不加白边。
单色符号经最近的 `glassContrastSource` 读取角色，
彩色图标、启用按钮及危险操作的状态色保持显式定义。只有标量回退路径在混合背景上使用中央平均值。
`tests/glass-contrast/` 检查策略逻辑、最终字形 RGB/alpha、动态帧及独立标题取样；
CI 清单包含 4 项逻辑及 6 组文字合成检查（1/1.5/2 倍、固定/自然宽度），后者使用候选
着色器和原生模块以及合成网格，不加载 Glass 效果；真实反馈测试还需候选 Glass 插件。
注册测试不代表运行验收通过。多屏、HDR、真实分数缩放
与性能仍需单独实测。新版合成器插件需注销登录后加载，不能热替换当前合成器。

## 色彩与对比度

用 `AppearanceTokens.isDarkTheme` 和语义内容 Token，特别是 `content.glassInk`，不直接用系统明暗标志选文字色。
`glassFollowsAppearanceMode` 为 false 时浅色桌面仍可能有深色玻璃，需要浅色文字。
材质填充透明度、scrim、背景模糊分开处理；染色/填充透明不意味着关闭模糊；协议材质透明度则衰减包括模糊在内的完整效果，协议见[窗口外观](WindowAppearance.zh-CN.md)。

`shared/qml/colorize/` 本地生成 Material 配色：`monet` 用 CAM16/HCT，`chinese`、`japanese` 用传统色数据和共用对比策略。
保持 sRGB 字节舍入、HCT 色域和对比度回归，不换成外部 `matugen` 或近似 Lab。
从原壁纸取色，不采 Plasma 黑白占位图；通过 `plasmashellrc` 当前 ShellPackage 定位配置，不硬编码旧 appletsrc。

源码 colorize 目录不是编译后的 `Kos.Ui` 模块。Shell/Settings 两种运行方式均需正确别名、`qmldir`、资源部署、相对导入。
共享/应用代码不导入 Quickshell 专属适配器。Material 卡片表面/花瓣模糊走公共组件，不把样式分支散落各控件。

## 图标、几何、动效

Shell 图案在 `BundledIcons` 用稳定名称登记，通过 `BundledIcon` 或注册表来源绘制，不依赖本机绝对路径或字体字形。
应用图标、媒体封面、文件缩略图各有契约。Dock 回收站等系统主题例外经 `SystemIconResolver` 集中候选和内置回退。

托盘身份在 `modules/bar/TrayIdentity.mjs` 解析：应用明确提供的提示标题、标题与专用 ID 优先于
按共享生成 ID 缓存的 daemon 回退名。两个匿名项共用 ID 时保持未解析；托盘增删会清空缓存并丢弃
过期的在途响应。单色模板使用精确应用别名，保留实时提醒/状态图案，替代模板加载失败时回退原始图标。

Dock、Bar、组件统一使用几何/动效 Token。窗口动画目标使用合成器全局坐标中的稳定槽位中心及静态图标尺寸，悬停放大不改变最小化落点。
桌面间距见[桌面图标](DesktopIcons.zh-CN.md)。Material 表面与连续窗口圆角渲染所有者不同，不重复裁剪或阴影。

## Shell 文字渲染

`common/ShellText.qml` 在小字号文字静止时使用平台原生渲染，与顶部栏的原生标签一致。
`GlassText`、右键菜单行、启动器标签及 Dock 名称提示共用此策略；通知及历史、控制中心、
快速搜索、桌面组件和 Dock 信息弹窗通过 `GlassText` 继承。字体、字号、字重及系统 hinting
偏好仍由原调用方或平台设置负责。

`ShellText` 跟踪自身及祖先 Item 的缩放和旋转，变换期间使用距离场渲染，静止后恢复原生渲染。
使用显式 Scale/Rotation 变换列表的宿主需在变换生效时设置 `scalableRendering`。
启动器搜索及重命名输入框也跟随其动画容器，在静止时使用原生渲染。

右键菜单在胶囊展开期间及列表溢出时遮罩全部内容，避免滚动中的半行越过圆角。
无需滚动的菜单静止后，内缩的菜单行直接绘制到窗口。
`LiquidGlassPanel.maskContent: false` 时 QML 背景保留独立遮罩，使 Material 及回退背景仍有
正确圆角，无需把文字与背景栅格化到同一张离屏纹理。

## 验证与审阅

按[开发指南](Development.zh-CN.md)仅 VM/CI 验证。覆盖迁移、快照/IPC、浅/深/系统主题、浅桌面深玻璃、
玻璃/Material/配色、Bar/Dock 融合、源码/安装模块路径、图标回退、减少动态效果。
全屏/最大化/混合缩放另见[窗口外观](WindowAppearance.zh-CN.md)。offscreen Token 测试不能证明实时模糊、GPU 性能或多屏行为。
文字改动在 100%、125%、150%、200% 输出缩放及跨屏移动时对比中英文标签和输入框。
覆盖菜单展开、子菜单及滚动，启动器搜索、文件夹、编辑及拖放，以及开关动画反向；
在玻璃和 Material 主题下分别检查静止及运动中的文字。
