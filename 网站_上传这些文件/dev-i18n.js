/* Developer console — English / 中文 switch.
   The console is written in Chinese; in English mode every Chinese phrase on the page is swapped for its
   English version (also text that appears later: status, toasts, the simulator). Messages and the log that
   come from the ESP32 itself stay in Chinese.
   To add a phrase: put "中文": "English" in EN below. Longer phrases are replaced first. */
(function () {
  const EN = {
    // ---- header / tabs ----
    "开发者控制台": "Developer console", "连接云端…": "Connecting to the cloud…", "云端已连上": "Cloud connected",
    "云端未连上": "Cloud not connected", "库没加载（没网？）": "library not loaded (no internet?)",
    "等 ESP32…": "Waiting for ESP32…", "ESP32 在线": "ESP32 online", "ESP32 离线": "ESP32 offline",
    "ESP32 很久没消息": "No news from ESP32 for a while", "没填密钥（只能看）": "No key (view only)", "密钥已填": "Key entered",
    "开发者密钥（和 ESP32 上设置的一样）": "Developer key (the same as set on the ESP32)", "记住": "Remember",
    "已记在这个浏览器里": "Saved in this browser",
    "总览": "Overview", "控制": "Control", "音乐": "Music", "网络": "Network", "模拟器": "Simulator", "日志": "Log", "更新程序": "Update",
    // ---- overview ----
    "实时画面": "Live view", "● 实时": "● live", "（等 ESP32 发送画面…）": "(waiting for pictures from the ESP32…)",
    "（ESP32 离线，没有实时画面 —— 可以到「模拟器」里看动画）": "(ESP32 offline, no live view — use the Simulator to see the animations)",
    "Box 1 图案": "Box 1 Pattern", "Box 2 感知": "Box 2 Sense", "Box 3 连接": "Box 3 Connect", "Box 4 组装": "Box 4 Assemble",
    "背板 · 均光膜": "Backlight · diffuser", "底部": "Under-light",
    "每个 16×16 灯板缩小一半显示（每 2×2 个灯合成 1 个点，取最亮的）；光桥 8×32 → 4×16。背板有均光膜，所以画成一片柔光；底部和光桥没有，画成一颗一颗的灯。装置离线时，可以到「模拟器」里看程序里的动画。只有打开页面时 ESP32 才会发送，每秒 4 帧。":
      "Each 16×16 panel is shown at half size (every 2×2 LEDs → 1 dot, brightest kept); bridges 8×32 → 4×16. The backlight has a diffuser film, so it is drawn as soft light; the under-light and bridges are drawn as single LEDs. When the installation is offline, use the Simulator. The ESP32 only sends pictures while this page is open, 4 frames/s.",
    "状态": "State", "本地热点": "Local hotspot", "最近事件": "Last event", "已运行": "Running", "掉线次数": "Drop-outs",
    "霍尔": "Hall", "空闲内存": "Free memory", "芯片温度": "Chip temperature", "无线功率": "Radio power",
    "程序版本": "Firmware version", "最后消息": "Last message", "最近日志": "Recent log", "还没收到": "Nothing received yet",
    "省电待机（简单动画，有人互动就恢复）": "Power-saving standby (simple animation, wakes on any interaction)",
    "（测试图案）": " (test pattern)", "关（省电）": "off (power saving)", "（上次开机：": " (last start: ", "（静止 ": " (resting ",
    "次（这次开机以来）": " times (since this start)", "次 · 云端 ": " times · cloud ",
    "（芯片偏热，已自动降低）": " (chip warm, lowered automatically)", "（信号弱，自动加大）": " (weak signal, raised automatically)",
    "（正常，省电）": " (normal, power saving)", "（偏热，正常范围）": " (warm, still normal)", "（正常）": " (normal)", "⚠ 太热": "⚠ too hot",
    "（弱，容易掉线）": " (weak, drops easily)", "（一般）": " (fair)", "（好）": " (good)", "（弱）": " (weak)",
    "待机": "Idle", "导览": "Tour", "聚焦": "Single box", "离线": "Offline", 
    // ---- control ----
    "（改了就发送，ESP32 会保存）": "(sent as soon as you change it, the ESP32 saves it)", "设置": "Settings",
    "亮度": "Brightness", "总音量（0 最小 · 100 最大）": "Volume (0 lowest · 100 highest)",
    "11 块灯板的正式动画亮度": "Animation brightness for 11 panels",
    "每块板单独设为 0–100%；100% 保留原动画亮度，0% 关闭该板。上方总亮度仍然有效。松开滑块后发送到 ESP32 并保存；单板校准测试不受这里影响。":
      "Set each panel to 0–100%. At 100% its animation is unchanged; at 0% the panel is off. The master brightness above still applies. Releasing a slider sends and saves the setting on the ESP32. The panel calibration test is unaffected.",
    "展示当天关闭装置小音响": "Mute the built-in speaker on exhibition day",
    "允许观众在自己的手机听音乐": "Allow visitors to listen on their own phones",
    "需要安装含观众听音开关的新固件。": "Install the new firmware with the visitor audio switch.",
    "等待 ESP32 在线确认观众听音设置。": "Waiting for the ESP32 to confirm the visitor audio setting.",
    "观众页显示个人听音按钮。": "The visitor page shows the personal audio button.",
    "观众页已隐藏个人听音按钮。": "The visitor page hides the personal audio button.",
    "展示现场声音（这台电脑 → JBL）": "Exhibition sound (this computer → JBL)",
    "只让连接 JBL 的这台电脑播放。观众扫 NFC 打开网页不会自动出声；你可在下方决定他们能否看到个人听音按钮。先把本电脑连接 JBL，再选择模式；两种模式都保留灯板动画，并关闭装置的小音响。":
      "Only this computer sends sound to the JBL. Opening the NFC visitor page never starts sound automatically; you can decide below whether visitors see the personal audio button. Connect this computer to the JBL first. Both modes keep the LED animations and mute the built-in speaker.",
    "展示有音乐": "Exhibition with music", "展示无音乐": "Exhibition without music", "重新准备音乐": "Retry loading music",
    "正在准备整段音乐…": "Preparing the full soundtrack…", "正在完整下载并准备音乐…": "Downloading and preparing the full soundtrack…",
    "音乐未准备好，请检查网络后点“重新准备音乐”。": "Soundtrack unavailable. Check the connection, then retry.",
    "等待音乐准备…": "Waiting for the soundtrack…", "正在关闭装置小音响；确认后电脑会播放。": "Muting the built-in speaker; this computer will play after confirmation.",
    "正在关闭装置小音响和观众手机听音；确认后电脑会播放。": "Muting the built-in speaker and disabling visitor phone audio; this computer will play after confirmation.",
    "展示有音乐：JBL 正在播放。": "Exhibition with music: the JBL is playing.",
    "展示有音乐：已准备，等待导览或 Box 开始。": "Exhibition with music: ready, waiting for the tour or a box.",
    "展示无音乐：这台电脑和装置小音响都已关闭。": "Exhibition without music: this computer and the built-in speaker are muted.",
    "这台电脑已静音；装置小音响仍开着，请等待静音设置生效。": "This computer is silent; the built-in speaker is still on. Wait for the mute setting.",
    "这台电脑已静音；装置小音响状态未确认。": "This computer is silent; the built-in speaker status is unconfirmed.",
    "“展示有音乐”会自动关闭观众手机听音按钮，并关闭装置小音响；“展示无音乐”只关闭现场声音。以后在 Lab 前厅，可重新勾选“允许观众在自己的手机听音乐”。展示时让此页面保持打开、电脑不要休眠。":
      "Exhibition with music automatically hides personal audio on visitor phones and mutes the built-in speaker. Exhibition without music turns off the room sound. Later in the lab foyer, select Allow visitors to listen on their own phones again. Keep this page open and the computer awake during the show.",
    "先连接含静音功能的 ESP32，避免两个音响同时出声": "Connect the ESP32 with mute support first to avoid two speakers playing together",
    "先输入开发者密钥，才能关闭装置小音响": "Enter the developer key to mute the built-in speaker",
    "浏览器未允许这台电脑播放声音": "The browser did not allow sound on this computer",
    "电脑已静音；装置小音响未确认关闭，请检查下方静音选项": "This computer is silent; check the built-in speaker mute option below",
    "装置小音响已打开，电脑音乐已停止以免重叠": "The built-in speaker is on; computer music stopped to avoid overlap",
    "没有确认装置小音响已关闭，电脑音乐未启动": "Built-in speaker mute was not confirmed; computer music did not start",
    "先连接含观众听音开关的新 ESP32 程序": "Connect the ESP32 with the new visitor audio switch firmware first",
    "没有确认小音响已关闭、观众听音已隐藏，电脑音乐未启动": "Built-in speaker mute and visitor audio hiding were not confirmed; computer music did not start",
    "先关闭中央 JBL，再开放观众手机听音": "Turn off the central JBL before allowing visitor phone audio",
    "现场与观众听音设置发生冲突，电脑音乐已停止": "Room and visitor audio settings conflict; computer music stopped",
    "等待新版 ESP32 程序连接…": "Waiting for the updated ESP32 firmware…",
    "新版固件已连接；设置会保存到 ESP32。": "Updated firmware connected; settings are saved on the ESP32.",
    "ESP32 仍是旧固件：先到“更新程序”安装本次 .bin。": "The ESP32 still has old firmware. Install this release's .bin in Update first.",
    "底板": "under-light", "后板": "backlight",
    "现在只用音响 1（音响 2 的零件坏了，程序里已关掉）。总音量 0 = 2000，100 = 20000。":
      "Only speaker 1 is in use (speaker 2's part is broken and switched off in the program). Volume 0 = 2000, 100 = 20000.",
    "试听 音响 1（1.5 秒测试音）": "Test speaker 1 (1.5 s test tone)", "音响 1": "Speaker 1", "音响 2": "Speaker 2",
    "两个音响本身大小不一样时：先把大声的那个调低，直到两个听起来一样，再用总音量调整体大小。":
      "If the two speakers differ: turn the louder one down until they match, then set the overall level with Volume.",
    "没人互动几分钟后省电待机（0 = 不用）": "Standby after this many minutes without visitors (0 = never)",
    "霍尔灵敏度（越小越灵敏）": "Hall sensitivity (smaller = more sensitive)", "聚焦锁定秒数": "Single-box lock (s)",
    "模式": "Mode", "自动（传感器、按钮、观众网站有效）": "Automatic (sensor, buttons and visitor site work)",
    "手动（只用控制台）": "Manual (console only)", "一直待机": "Always idle", "观众网站可以点播": "Visitor site can start boxes",
    "这里的 Box 按钮是开发者用的，不受冷却和锁定限制。": "These Box buttons are for developers and ignore the cooldown and lock.",
    "完整导览": "Whole tour", "停止 / 待机": "Stop / idle", "测试图案": "Test pattern", "现在省电待机": "Standby now", "唤醒": "Wake up",
    "单块灯板校准测试": "Individual LED panel calibration",
    "保存校准记录": "Save calibration record", "复制校准资料": "Copy calibration data", "下载校准文件": "Download calibration file",
    "选择旧校准文件": "Choose an earlier calibration file", "将文件里的设置恢复到 ESP32": "Restore these settings to the ESP32",
    "ESP32 会记住方向和亮度。调整完成后，下载这份不含密钥的校准文件；下次动画师换代码时把文件和新动画一起发给我。换 ESP32 或清空设备后，也能从文件恢复 11 块板的设置。":
      "The ESP32 remembers orientation and brightness. After calibration, download this file without keys and send it with any new animation code. You can also restore all 11 panels after replacing or resetting the ESP32.",
    "等待 ESP32 回报设置…": "Waiting for the ESP32 settings…", "需要 ESP32 安装含方向校准的新固件。": "The ESP32 needs the orientation-calibration firmware.",
    "已收到 ESP32 当前设置；导出前请等刚才的修改回报到这里。": "Current ESP32 settings received; wait for recent changes to appear before exporting.",
    "这是 ESP32 上次回报的设置，设备目前未确认在线。": "These are the last reported settings; the ESP32 is not confirmed online.",
    "校准资料已复制，可以直接发给我": "Calibration data copied; send it with the new animation code.",
    "复制失败，请用“下载校准文件”": "Copy failed; use Download calibration file.",
    "ESP32 尚未回报新版校准设置": "The ESP32 has not reported its calibration settings.",
    "先连接已更新的 ESP32": "Connect the updated ESP32 first.",
    "已发送 11 块板的校准设置；请看实体板核对": "Calibration sent for 11 panels; check the physical panels.",
    "只点亮你选择的一块板。可以显示方向标记、整块、指定横排或竖排；离开测试时点“关闭测试”。测试亮度不会改动正式动画亮度。":
      "Lights only the selected panel. Show direction markers, the whole panel, one row or one column; press “Stop test” when finished. Test brightness does not change animation brightness.",
    "灯板": "Panel", "范围": "Area", "方向标记": "Direction markers", "整块板": "Whole panel", "一条横排": "One horizontal row", "一条竖排": "One vertical column",
    "这块灯板的方向": "This panel's orientation", "旋转": "Rotation", "镜像": "Mirror",
    "0° · 保持原方向": "0° · original orientation", "顺时针 90°": "90° clockwise", "180°": "180°", "顺时针 270°": "270° clockwise",
    "不镜像": "No mirror", "左右镜像": "Mirror left–right", "恢复这块板的原始方向": "Restore this panel's original orientation",
    "调整后自动保存；请看实体灯板确认方向。": "Changes save automatically; check the physical panel.",
    "需要安装含方向校准功能的新固件。": "Install the new firmware with orientation calibration.",
    "先点“开始 / 更新测试”，直接看实体板。选旋转或左右镜像后立即作用于测试图案和正式动画，并保存在 ESP32。光桥只支持 0° 或 180°。":
      "Start the test and check the physical panel. Rotation and mirror changes apply to both the test and animations, and are saved on the ESP32. Bridges support only 0° or 180°.",
    "背板：面对盒子正面，红色在左上、绿色在右上、蓝色在左下、白色在右下。": "Backlight: facing the box, red top-left, green top-right, blue bottom-left, white bottom-right.",
    "底板：从上方看盒子，红色在远侧左边、绿色在远侧右边、蓝色在近侧左边、白色在近侧右边。": "Under-light: looking down, red far-left, green far-right, blue near-left, white near-right.",
    "光桥：红色靠前一个盒子、上侧；绿色靠后一个盒子、上侧；蓝色靠前一个盒子、下侧；白色靠后一个盒子、下侧。": "Bridge: red at the previous box/top, green at the next box/top, blue at the previous box/bottom, white at the next box/bottom.",
    "第几条（从 1 开始）": "Line number (starting at 1)", "颜色": "Colour", "白": "White", "红": "Red", "绿": "Green", "蓝": "Blue", "橙": "Orange", "青": "Teal", "紫": "Purple", "粉": "Pink",
    "这块板的测试亮度": "Test brightness for this panel", "开始 / 更新测试": "Start / update test", "关闭测试": "Stop test",
    "16×16：横排/竖排为 1–16。8×32 光桥：横排 1–8，竖排 1–32。": "16×16: rows/columns 1–16. 8×32 bridge: rows 1–8, columns 1–32.",
    "底板 16×16": "under-light 16×16", "后板 16×16": "backlight 16×16", "光桥": "Bridge",
    "先在上面输入开发者密钥": "Enter the developer key at the top first", "没回应（离线，或者正在忙）": "No answer (offline or busy)",
    "这个浏览器不能签名命令，请用 https 网址打开": "This browser cannot sign commands — open the console from its https address",
    "程序版本变了：": "Firmware changed: ",
    // ---- music ----
    "每个实验室的音乐": "Music for each lab", "有音乐的实验室，播放时用这段音乐（从头开始，盒子结束时淡出），不再播电子提示音。“试听”只在喇叭上放 15 秒，不动灯。":
      "A lab with music plays it (from the start, fading out when the box ends) instead of the electronic cues. “Listen” plays 15 s on the speaker only, the LEDs don't change.",
    "ESP32 实体音频（上传与试听）": "ESP32 device audio (upload and test)",
    "上方“选音乐／试听”只在这台电脑播放，不会改装置。这里查看装置里真正存了哪些音频；要更换文件，电脑需和 ESP32 在同一个 Wi-Fi，或连接 Saliva-Control 热点。":
      "Choose music and Listen above only play on this computer. Here you can see which audio files are stored on the ESP32. To replace one, connect the computer to the same Wi-Fi or the Saliva-Control hotspot.",
    "打开 ESP32 音频上传页面": "Open ESP32 audio upload page", "设备试听 15 秒": "Test on device for 15 s",
    "上传 / 更换音乐": "Upload / replace music",
    "音频文件比较大，要直接传给 ESP32，所以电脑要和 ESP32 在": "Audio files are large and go straight to the ESP32, so the computer must be on",
    "同一个 Wi-Fi": "the same Wi-Fi", "（或者连上它的热点 Saliva-Control）。打开下面的链接，选文件 → 转换并上传，wav / mp3 / m4a 都可以，网页会自动转格式。":
      " (or on its hotspot Saliva-Control). Open the link below, choose a file → convert and upload; wav / mp3 / m4a all work, the page converts them.",
    "打开音乐上传页面": "Open the music upload page", "秒音乐": " s of music", "电子提示音": "electronic cues", "试听": "Listen",
    "完整旅程 Box 1→3": "Whole journey Box 1→3",
    "完整旅程音乐": "Whole journey soundtrack",
    // ---- network ----
    "现在的网络": "Current network", "信号": "Signal", "已保存的网络": "Saved networks", "没有保存的网络": "No saved networks",
    "学校登录": "school login", "普通": "normal", "· 正在用": "· in use", "切换": "Switch",
    "“切换”会让 ESP32 先试那个网络，云端会断开十几秒。连不上的话，同一个网络会再试一次（学校登录每次最多 30 秒），之后换回其他已保存的网络。":
      "“Switch” makes the ESP32 try that network first; the cloud drops for about 15 s. If it can't connect, it tries the same network once more (school login up to 30 s each time), then goes back to the other saved networks.",
    "从 ESP32 删除这个网络？": "Delete this network from the ESP32?", "加一个新网络": "Add a new network",
    "新网络要输入密码，所以只能在现场加（密码不经过云端）：": "A new network needs its password, so it can only be added on site (passwords never go through the cloud):",
    "手机连 Wi-Fi": "Phone → Wi-Fi", "），控制台会自己弹出来；没弹出就打开": "), the control page pops up; if not, open",
    "→ 扫描 → 选网络 → 输密码 → 保存。": "→ Scan → choose network → password → Save.", "（密码 ": " (password ",
    "到新场地前，可以先把那里的 Wi-Fi 或者手机热点加好，最多存 3 个，会按顺序自动试。":
      "Before moving to a new venue, add its Wi-Fi or a phone hotspot first — up to 3 are saved and tried in order.",
    "本地热点 Saliva-Control": "Local hotspot Saliva-Control", "开着": "on", "正在重试启动": "retrying startup", "关（学校网稳定时自动关闭）": "off (switches off while school Wi-Fi is steady)",
    "关着（学校网稳定时自动关闭；断网 30 秒或连按 Box 1 三次会打开）": "off (while school Wi-Fi is steady; opens after 30 s offline or three presses of Box 1)",
    "热点和学校 Wi-Fi 共用同一个无线电，所以新版固件的热点是自动的：开机后开 10 分钟；学校网断开 30 秒会自己打开；连按 Box 1 三次也会打开；有设备连着时不会关；学校网稳定 1 分钟后自动关闭，让学校网更稳。热点关着时，用学校网里 ESP32 的 IP 或这个网站控制。":
      "The hotspot and the school Wi-Fi share one radio, so the new firmware runs the hotspot automatically: on for 10 minutes after power-on, on by itself after 30 s without school Wi-Fi, on after three quick presses of Box 1, never off while a device is connected, and off once school Wi-Fi has been steady for 1 minute so the school link stays stable. While it is off, use the ESP32's school IP or this website.",
    // ---- simulator ----
    "不用连 ESP32": "no ESP32 needed",
    "用的是程序文件里同一份动画代码（编译成网页能跑的版本），每秒 10 帧，和装置一样。现场离线、或者上传新程序前，都可以在这里先看动画、听声音。":
      "Uses the same animation code as the firmware (compiled for the web), 10 frames/s like the installation. Check animations and sound here when the installation is offline or before uploading new firmware.",
    "来自程序文件 ": "From firmware file ", "· 保存于 ": "· saved ", "· 编号 ": "· ID ",
    "改了程序里的动画以后，要重新生成": "After changing the animations in the firmware, rebuild",
    "，或者把新的 .ino 发给 Claude），这里才会变成新的。上面的「编号」和程序文件对得上，就说明模拟器是最新的。电脑音箱和现场喇叭的音量不同，这里只用来听内容和时间点。":
      ", or send the new .ino to Claude) to update this. If the “ID” above matches the firmware file, the simulator is current. Computer speakers are not as loud as the real speaker — use this for content and timing only.",
    "播放": "Play", "暂停": "Pause", "重播": "Replay", "速度": "Speed", "声音": "Sound", "电脑音量": "computer volume",
    "不是 1× 时不放声音（音乐和提示音只有原速才对得上）。": "No sound unless at 1× (music and cues only line up at normal speed).",
    "每个盒子的音乐": "Sound for each box", "选文件只是在这台电脑上试听，不会上传到 ESP32": "a file chosen here only plays on this computer; it is not uploaded to the ESP32",
    "选音乐": "Choose file", "去掉": "Remove", "转换中…": "Converting…", "这个文件读不了：": "This file can't be read: ",
    "没有音乐 → 播放程序里的电子提示音": "No music → the firmware's electronic cues play",
    "没有 → Box 1–3 用各自的音乐或电子提示音": "None → Box 1–3 use their own music or the electronic cues",
    "由「完整旅程」负责（单独播这个盒子时播它对应的那一段）": "Covered by “Whole journey” (a single box plays its part of it)",
    "秒 → 导览从 Box 1 开始一直播到底，代替 Box 1–3 和光桥的提示音": " s → plays once from the start of Box 1 to the end, instead of the cues for Box 1–3 and the bridges",
    "→ 这个盒子的电子提示音不播": "→ this box's electronic cues are off", "· 比盒子短，后面没声音": " · shorter than the box, silent after",
    "· 转换后 ": " · converted ", "秒，只保留前 ": " s, only the first ", "（原来 ": " (originally ", "网站上的 audio/": "from the website audio/",
    "加载程序里的动画…": "Loading the firmware's animations…", "模拟器加载失败：": "Simulator failed to load: ",
    "下载不到模拟器文件": "Could not download the simulator files", "网站上没有 sim/anim.wasm": "sim/anim.wasm is not on the website",
    "。检查：电脑是不是连着 ESP32 的热点（没有外网）？网站的 sim 文件夹里有没有 anim.js？":
      ". Check: is the computer on the ESP32 hotspot (no internet)? Is anim.js in the website's sim folder?",
    "测试图案（看每块灯板方向对不对）": "Test pattern (check every panel's direction)", "待机动画": "Idle animation",
    "只播 Box": "Single Box", "· 盒内 ": "· in box ", "· 播完，停在最后的画面然后淡出": " · finished, holding the last scene then fading",
    "导览结束 → 回到待机": "Tour finished → back to idle", "结尾保持 / 淡出": "Final hold / fade", "光桥 ": "Bridge ",
    // ---- log / update ----
    "日志（最近 40 条，时间是开机后的时:分:秒）": "Log (last 40 lines, time = h:m:s since start)", "刷新": "Refresh", "复制": "Copy",
    "日志已复制": "Log copied", "远程更新程序": "Remote firmware update", "现在的版本": "Current version",
    "把程序文件夹 build 里的": "Upload", "上传到网站仓库的": "from the sketch's build folder to the website repository's",
    "文件夹（替换旧的），等 GitHub Pages 更新约 1 分钟。": "folder (replace the old one) and wait about 1 minute for GitHub Pages.",
    "点下面的按钮。灯会关 30–90 秒，成功后版本号会变成新的编译时间。": "Press the button below. The LEDs switch off for 30–90 s; afterwards the version shows the new build time.",
    "程序链接（https，以 .bin 结尾）": "Firmware link (https, ending in .bin)", "链接要以 https:// 开头、.bin 结尾": "The link must start with https:// and end in .bin",
    "网站上传到 GitHub 后会自动填好（现在是在电脑上直接打开的文件）": "Filled in automatically once the site is on GitHub (this is a file opened from the computer)",
    "确定更新？灯会关掉 30–90 秒": "Update now? The LEDs switch off for 30–90 s", "更新": "Update",
    "失败不会弄坏板子：下载完整并检查通过才会换程序，否则继续用旧的，日志里会写原因。":
      "A failure can't break the board: the new firmware is used only after a complete, checked download; otherwise it keeps the old one and the log says why.",
    "在现场的话也可以用本地页面": "On site you can also use the local page", "直接选文件上传。": "and upload the file directly.",
    "重启": "Restart", "重启 ESP32": "Restart ESP32", "重启 ESP32？灯会黑几秒": "Restart the ESP32? The LEDs go dark for a few seconds",
    "（空）": "(empty)", "还没连上云端": "Not connected to the cloud yet",
    "没设密钥": "no key set", "（运行 ": " (running ", "现在：": "Now: ",
    // ---- words that come from the ESP32 ----
    "通电开机": "power on", "页面/程序重启": "restarted by page/program", "程序出错重启（请截图日志给 Claude）": "crash restart (send a log screenshot to Claude)",
    "卡住后自动重启（看门狗）": "stuck → watchdog restart", "电压不够重启（检查 5V 电源和线）": "brownout restart (check the 5V supply and wires)",
    "按了 RST": "RST pressed", "其他": "other", "按钮 ": "Button ", "云端断开（Wi-Fi 还连着）：代码 ": "Cloud dropped (Wi-Fi still on): code ",
    "Wi-Fi 断开：原因 ": "Wi-Fi dropped: reason ", "，断开前信号 ": ", signal before ", "，信号 ": ", signal ",
    "收不到路由器信号（信号太弱或被挡）": "lost the router (signal too weak or blocked)", "认证超时（信号弱或学校网登录慢）": "login timed out (weak signal or slow school login)",
    "设备自己断开": "left by itself", "长时间没有活动": "idle too long", "路由器不认这台设备了": "router dropped this device",
    "认证失败（用户名/密码）": "login failed (user/password)", "找不到这个网络": "network not found", "连接被拒绝": "connection refused", "连接超时": "connection timed out",
    "Wi-Fi 断开": "Wi-Fi dropped",
  };
  const KEYS = Object.keys(EN).sort((a, b) => b.length - a.length);
  const EXACT = { "开": "on", "删": "Delete" };                       // one-character words: only when they stand alone
  const REGEX = [[/(\d+) 小时 ?/g, "$1 h "], [/(\d+) 分/g, "$1 min"], [/([\d.]+) ?秒/g, "$1 s"], [/秒/g, " s"], [/（/g, " ("], [/）/g, ")"], [/：/g, ": "], [/，/g, ", "], [/。/g, ". "]];
  const HAS_ZH = /[　-〿一-鿿＀-￯]/;
  let lang = (navigator.language || "").toLowerCase().startsWith("zh") ? "zh" : "en";
  try { lang = localStorage.getItem("saliva-dev-lang") || lang; } catch (e) {}

  const tr = (s) => {
    if (lang !== "en" || !s || !HAS_ZH.test(s)) return s;
    const t = s.trim();
    if (EXACT[t]) return s.replace(t, EXACT[t]);
    let out = s;
    for (const k of KEYS) if (out.includes(k)) out = out.split(k).join(EN[k]);
    for (const [re, to] of REGEX) out = out.replace(re, to);
    return out;
  };
  const ORIG = new WeakMap();                        // node → its Chinese text, so switching back works
  const skip = (n) => { const p = n.parentElement; return !p || p.closest("#log, script, style, [data-no-tr]"); };
  const doText = (n) => {
    if (skip(n)) return;
    const cur = n.nodeValue, rec = ORIG.get(n);
    const zh = rec && rec.out === cur ? rec.zh : cur;  // text changed by the page → that is the new original
    const out = tr(zh);
    ORIG.set(n, { zh, out });
    if (out !== cur) n.nodeValue = out;
  };
  const ATTRS = ["placeholder", "title", "aria-label"];
  const doEl = (el) => {
    if (el.closest && el.closest("#log, [data-no-tr]")) return;
    for (const a of ATTRS) {
      if (!el.hasAttribute || !el.hasAttribute(a)) continue;
      const key = "__tr_" + a, cur = el.getAttribute(a), rec = el[key];
      const zh = rec && rec.out === cur ? rec.zh : cur, out = tr(zh);
      el[key] = { zh, out };
      if (out !== cur) el.setAttribute(a, out);
    }
  };
  const walk = (root) => {
    if (root.nodeType === 3) return doText(root);
    if (root.nodeType !== 1) return;
    doEl(root);
    const it = document.createTreeWalker(root, NodeFilter.SHOW_TEXT | NodeFilter.SHOW_ELEMENT);
    let n; while ((n = it.nextNode())) n.nodeType === 3 ? doText(n) : doEl(n);
  };
  let busy = false;
  const obs = new MutationObserver((list) => {
    if (busy) return; busy = true;
    for (const m of list) {
      if (m.type === "characterData") doText(m.target);
      else if (m.type === "attributes") doEl(m.target);
      else m.addedNodes.forEach(walk);
    }
    obs.takeRecords(); busy = false;
  });
  // dialogs
  const conf = window.confirm.bind(window), alrt = window.alert.bind(window);
  window.confirm = (m) => conf(tr(m)); window.alert = (m) => alrt(tr(m));

  const apply = () => {
    document.documentElement.lang = lang === "en" ? "en" : "zh";
    document.title = lang === "en" ? "Saliva developer console" : "Saliva 开发者控制台";
    busy = true; walk(document.body); obs.takeRecords(); busy = false;
    const b = document.getElementById("langBtn"); if (b) b.textContent = lang === "en" ? "中文" : "English";
  };
  window.D2DDevLang = {
    tr, get: () => lang,
    set(l) { lang = l; try { localStorage.setItem("saliva-dev-lang", l); } catch (e) {} apply(); },
  };
  const start = () => {
    obs.observe(document.body, { subtree: true, childList: true, characterData: true, attributes: true, attributeFilter: ATTRS });
    apply();
    const b = document.getElementById("langBtn");
    if (b) b.addEventListener("click", () => window.D2DDevLang.set(lang === "en" ? "zh" : "en"));
  };
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", start); else start();
})();
