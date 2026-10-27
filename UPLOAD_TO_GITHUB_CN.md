# 这次必须一起更新的文件

主板固件和网页控制台模拟器是两套文件。只更新 `.bin`，网页仍会显示旧动画。

请按下列路径覆盖 GitHub 仓库中的同名文件：

1. `firmware/D2D_Saliva_Full_Physical_v2_20261027_204023.bin`：ESP32 无线更新使用。它使用唯一文件名，避免 GitHub Pages 返回旧缓存。
2. `live.js`：绘制实体实时画面和模拟器画面；缺少它时页面会没有反应。
3. `sim.js`：控制台模拟器加载与播放逻辑。
4. `sim/anim.js`：网页离线加载的动画程序；控制台通常优先读取它。
5. `sim/anim.wasm`：编译后的动画程序。
6. `sim/anim.json`：动画版本资料。
7. `dev.html`：已加入版本号 `3422f036`，避免浏览器继续使用缓存的旧动画。

上传后等待 GitHub Pages 完成部署，再强制刷新控制台：Mac 使用 `Command + Shift + R`。

无线更新时填写这个新链接：

`https://silviawang5888-create.github.io/saliva-site/firmware/D2D_Saliva_Full_Physical_v2_20261027_204023.bin`

不要继续使用旧的 `D2D_Saliva_Full.ino.bin` 链接。

控制台动画版本应显示：`3422f036`，总时间应为 `137` 秒，四个 Box 均为 `28` 秒。

## 以后重新生成控制台动画

完整 Arduino 工程内已经加入：

- `tools/build_sim.py`
- `tools/sim_wrapper.cpp`
- `tools/mock/FastLED.h`
- `tools/mock/math.h`

在完整 Arduino 工程文件夹中运行：

```bash
python3 tools/build_sim.py
```

它会重新生成 `sim/anim.js`、`sim/anim.wasm` 和 `sim/anim.json`。
