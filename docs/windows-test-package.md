# GameInfer Windows x64 测试包

本测试包仅从 `codex/gameinfer-test-package` 分支生成。打包工作流不会修改
`codex/gui-integration` 中准备最终合并的干净源码。

## 获取测试包

1. 打开 GitHub 仓库的 **Actions** 页面。
2. 选择 **Build GameInfer Windows test package**。
3. 使用 **Run workflow** 手动构建；向本分支推送提交时也会自动构建。
4. 构建完成后，从运行页面底部下载 `GameInfer-windows-x64-<commit>` Artifact。
5. 完整解压后运行 `GameInfer.exe`，不要移动或单独复制 EXE。

测试包面向 Windows x64，使用 Release 构建，包含 Qt 运行库、CPU/DirectML ONNX
Runtime，以及经过大小和 SHA-256 校验的官方 `GAME-1.0.3-small-onnx` 模型。模型仅进入
14 天有效的测试 Artifact，不写入 Git 历史。为避免测试电脑因缺少动态库产生假失败，
测试包会包含当前 x64-windows vcpkg 环境中的运行时 DLL 和可获得的依赖 copyright
文件；正式发行前再按实际依赖做裁剪。

程序当前没有代码签名，Windows 可能显示 SmartScreen 提示。本包仅供内部测试；在完成
模型及第三方依赖的发布许可复核前，不作为正式公开发行包。

## 公司验收清单

- 中文和 English 可以从设置中即时切换，重启后保持选择。
- 单文件转换能够生成可用 MIDI。
- 批量添加后，每项都能单独调整歌曲语言与 BPM。
- 模型、执行设备、分割阈值、分割半径、估计阈值和 D3PM 步数由整批共享。
- 队列严格串行；单项失败后继续后续任务。
- “完成当前后停止”不会中断正在执行的模型调用。
- 失败任务可以查看错误并重试。
- 重复输出、已有文件和无效路径会在执行前提示。
- CPU 模式可用；有兼容设备时再单独验证 DirectML。

已知上游行为：静音切分后的任一连续片段超过 60 秒会使该音频失败，这不是文件总时长
限制。测试时应分别记录音频总时长、错误中报告的切片时长，以及音频是否仍含伴奏、持续
底噪或混响。
