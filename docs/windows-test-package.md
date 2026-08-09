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

测试包同时包含 GameInfer 管理的 `uv` launcher、separator worker、Python 版本声明和完整
依赖锁。测试电脑不需要预装或配置 Python；首次执行音源分离时需要联网，GameInfer 会把
锁定的 separator 运行环境安装到应用数据目录，并由 `audio-separator` 下载选中的分离模型。
首次准备耗时和下载量会明显高于后续运行，运行环境与模型缓存完成后会直接复用。

程序当前没有代码签名，Windows 可能显示 SmartScreen 提示。本包仅供内部测试；在完成
模型及第三方依赖的发布许可复核前，不作为正式公开发行包。

## 公司验收清单

- 中文和 English 可以从设置中即时切换，重启后保持选择。
- 全新 Windows 环境中不安装 Python 也能启动分离流程。
- 首次分离会自动准备锁定依赖并下载默认的人声分离模型。
- 同一批任务先集中完成全部分离，再释放 separator 并集中生成 MIDI。
- `Vocals` 仅保存人声；`Vocals + Instrumental` 同时保存伴奏，但只有人声进入 GAME。
- 单文件转换能够生成可用 MIDI。
- 批量添加后，每项都能单独调整歌曲语言与 BPM。
- 模型、执行设备、分割阈值、分割半径、估计阈值和 D3PM 步数由整批共享。
- 队列严格串行；单项失败后继续后续任务。
- “完成当前后停止”不会中断正在执行的模型调用。
- 失败任务可以查看错误并重试。
- 重复输出、已有文件和无效路径会在执行前提示。
- CPU 模式可用；有兼容设备时再单独验证 DirectML。

已知行为：分离后的人声如果仍存在超过 60 秒的连续切片，该音频仍可能失败，这不是文件
总时长限制。手动裁剪弹窗尚未实现。测试时应分别记录原音频总时长、分离后人声状态、错误
中报告的切片时长，以及音频是否仍含持续底噪或混响。
