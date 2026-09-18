from docx import Document
from docx.shared import Pt, RGBColor, Inches
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml.ns import qn

doc = Document()

# 设置默认字体
doc.styles['Normal'].font.name = '微软雅黑'
doc.styles['Normal']._element.rPr.rFonts.set(qn('w:eastAsia'), '微软雅黑')
doc.styles['Normal'].font.size = Pt(11)

# 标题
title = doc.add_heading('WakeAI 智能运动唤醒助手', level=0)
title.alignment = WD_ALIGN_PARAGRAPH.CENTER

subtitle = doc.add_paragraph()
subtitle.alignment = WD_ALIGN_PARAGRAPH.CENTER
run = subtitle.add_run('结题答辩演讲稿')
run.font.size = Pt(14)
run.font.color.rgb = RGBColor(0x66, 0x66, 0x66)

info = doc.add_paragraph()
info.alignment = WD_ALIGN_PARAGRAPH.CENTER
run = info.add_run('建议总时长：8-10分钟 | 四板块：背景与价值 → 功能与技术实现 → 问题与解决 → 测试与总结')
run.font.size = Pt(10)
run.font.color.rgb = RGBColor(0x99, 0x99, 0x99)

doc.add_paragraph()

# 开场白
h = doc.add_heading('【开场白】（约30秒）', level=1)
p = doc.add_paragraph()
p.add_run('各位老师好，我们今天答辩的项目是 ').bold = False
run = p.add_run('WakeAI —— 智能运动唤醒助手')
run.bold = True
p.add_run('。')

p = doc.add_paragraph('这是一个用人体姿态识别技术来强制叫醒用户的智能闹钟系统。简单来说，就是')
run = p.add_run('你必须做完一组运动，它才会停止响铃')
run.bold = True
p.add_run('。')

doc.add_paragraph('接下来我会从四个部分来介绍我们的项目：项目背景与价值、功能与技术实现、开发中遇到的问题与解决、以及最后的测试与总结。')

# 板块一
h = doc.add_heading('【板块一：背景与价值】（约1.5分钟，3页）', level=1)

h2 = doc.add_heading('第1页：封面（约20秒）', level=2)
doc.add_paragraph('这就是我们的项目 WakeAI，全称是"智能运动唤醒助手"。')
doc.add_paragraph('技术栈用的是 C++17、Qt 框架、OpenCV 图像处理、YOLOv8 人体姿态估计模型，以及 SQLite 数据库。')
p = doc.add_paragraph()
run = p.add_run('一句话概括：用一组运动，完成一次清醒的开始。')
run.bold = True

h2 = doc.add_heading('第2页：项目背景与设计目标（约30秒）', level=2)
doc.add_paragraph('我们为什么要做这个项目？因为我们发现了一个大学生每天都会遇到的痛点——早八起床困难。')
doc.add_paragraph('普通闹钟响了，你随手关掉，"再睡5分钟"，一睁眼半小时过去了，迟到了。这是因为睡眠惯性——大脑还没完全开机，单纯的听觉刺激不够把你叫醒。')
doc.add_paragraph('所以我们的设计思路是：把关闹钟和运动任务绑定。不是简单地响铃，而是要求你站起来做几组运动，身体动起来了，心率上来了，你自然就清醒了。')
doc.add_paragraph('用户流程很简单：响铃 → 完成运动 → 达到目标 → 点击关闭。')
doc.add_paragraph('这里也说明一下，我们定位是运动唤醒辅助工具，不是医疗健康设备，实际效果还是需要用户体验来验证。')

h2 = doc.add_heading('第3页：创新点：直击早八起床痛点（约40秒）', level=2)
doc.add_paragraph('这一页是我们的核心创新点——为什么普通闹钟叫不醒大学生，我们用运动来解决。')
doc.add_paragraph('左边是痛点：', style='List Bullet')
doc.add_paragraph('闹钟一响，随手关掉继续睡', style='List Bullet 2')
doc.add_paragraph('"再睡5分钟" → 迟到半小时', style='List Bullet 2')
doc.add_paragraph('睡眠惯性：大脑还没开机', style='List Bullet 2')
doc.add_paragraph('普通闹钟只能"响"，不能让人"醒"', style='List Bullet 2')

doc.add_paragraph('右边是我们的解法：', style='List Bullet')
doc.add_paragraph('必须完成运动才能关闹钟 —— 物理上你就没法继续睡', style='List Bullet 2')
doc.add_paragraph('身体动起来 → 心率提升 → 真正清醒 —— 从生理层面唤醒', style='List Bullet 2')
doc.add_paragraph('摄像头自动计数 —— 不用你手动数，算法帮你算好', style='List Bullet 2')
doc.add_paragraph('从"被动响铃"到"主动唤醒" —— 这是本质区别', style='List Bullet 2')

p = doc.add_paragraph()
run = p.add_run('底部这句话是我们的核心价值：运动唤醒 + 自动计数 + 成就激励，让起床从"痛苦"变成"有成就感的小挑战"。')
run.bold = True

# 板块二
h = doc.add_heading('【板块二：功能介绍与技术实现】（约4-5分钟，11页）', level=1)

h2 = doc.add_heading('第4页：功能总览与使用流程（约30秒）', level=2)
doc.add_paragraph('接下来进入第二部分，介绍我们的功能和技术实现。')
doc.add_paragraph('上半部分是主流程：设置闹钟 → 到时响铃 → 开始识别 → 达到目标 → 点击完成 → 停铃 → 保存记录。')
doc.add_paragraph('下半部分是支持的三种运动：深蹲、开合跳、床上蹬腿。')
doc.add_paragraph('左下角是闹钟与音频功能：新增、编辑、删除、启停、重复日期、稍后提醒、铃声导入、试听与音量。')
doc.add_paragraph('右下角是持续反馈：历史记录、连续完成天数、成就、实时次数与目标进度。')

h2 = doc.add_heading('第5页：系统架构与技术选型（约30秒）', level=2)
doc.add_paragraph('这是我们的系统架构，分为四层：')
doc.add_paragraph('最上层是界面层，用 Qt Widgets 做的，负责闹钟设置、实时计数显示、铃声播放。', style='List Bullet')
doc.add_paragraph('往下是业务逻辑层，包括闹钟调度、任务状态管理、达标判断。', style='List Bullet')
doc.add_paragraph('再往下是算法层，负责人体姿态识别、关键点平滑、动作计数。', style='List Bullet')
doc.add_paragraph('最底层是数据层，用 SQLite 存储闹钟配置、唤醒记录、成就数据。', style='List Bullet')
doc.add_paragraph('技术选型上，我们选了 YOLOv8n-pose 这个轻量级姿态估计模型，在 CPU 上也能跑，不需要 GPU。')

h2 = doc.add_heading('第6页：闹钟调度与任务状态管理（约25秒）', level=2)
doc.add_paragraph('闹钟调度模块是整个系统的入口。')
doc.add_paragraph('我们用了状态机来管理闹钟的完整生命周期：未激活 → 已设置 → 待触发 → 响铃中 → 识别中 → 已完成。')
p = doc.add_paragraph()
run = p.add_run('核心规则是：挑战未完成时，stop() 会被拒绝。也就是说，你不做完运动，这个闹钟就停不下来。')
run.bold = True
doc.add_paragraph('这是我们系统最核心的一个业务约束。')

h2 = doc.add_heading('第7页：人体姿态识别流程（约30秒）', level=2)
doc.add_paragraph('接下来是算法部分。人体姿态识别的流程是这样的：')
doc.add_paragraph('摄像头采集一帧图像 → YOLOv8n-pose 推理 → 输出 17 个身体关键点坐标 → 关键点平滑处理 → 动作特征计算 → 动作判定 → 计数更新。')
doc.add_paragraph('这里我们用的是 YOLOv8 的 nano 版本，精度足够而且速度快，普通笔记本 CPU 也能实时跑。')

h2 = doc.add_heading('第8页：关键点平滑与统一动作接口（约25秒）', level=2)
doc.add_paragraph('原始的关键点坐标每一帧都会有抖动，直接用来计数会很不稳定。')
doc.add_paragraph('所以我们做了两件事：')
doc.add_paragraph('第一，关键点平滑——用滑动窗口对坐标做滤波，去除抖动。', style='List Bullet')
doc.add_paragraph('第二，统一动作接口——不管是深蹲、开合跳还是蹬腿，都实现同一个计数接口，方便扩展新动作。', style='List Bullet')
doc.add_paragraph('这是一个设计上的考虑，让系统更容易维护和扩展。')

h2 = doc.add_heading('第9页：深蹲计数：膝角与状态机（约30秒）', level=2)
doc.add_paragraph('深蹲的识别是这样的：我们用膝角来判断深蹲动作——大腿和小腿之间的夹角。')
doc.add_paragraph('用了一个三态状态机：')
doc.add_paragraph('Ready 站直：膝角 > 155°', style='List Bullet')
doc.add_paragraph('Down 下蹲：膝角 < 140°', style='List Bullet')
doc.add_paragraph('完成一次：从 Down 回到 Ready，次数 +1', style='List Bullet')
doc.add_paragraph('中间 140° 到 155° 是缓冲带，避免抖动导致的误判。')

h2 = doc.add_heading('第10页：开合跳计数：手脚联合判定（约25秒）', level=2)
doc.add_paragraph('开合跳比深蹲复杂一点，因为它是手脚联合的动作。')
doc.add_paragraph('我们同时检测两个信号：')
doc.add_paragraph('手：双手是否在头顶上方', style='List Bullet')
doc.add_paragraph('脚：双脚是否分开到足够宽度', style='List Bullet')
doc.add_paragraph('必须两个条件同时满足，才算"打开"状态；两个条件同时回到初始位置，才算"闭合"，计数加一。')
doc.add_paragraph('这样可以避免只动手或者只动脚的误触发。')

h2 = doc.add_heading('第11页：床上蹬腿：二维信号与自动标定（约25秒）', level=2)
doc.add_paragraph('第三种动作是床上蹬腿，这个是专门为不想下床就想完成运动的用户设计的。')
doc.add_paragraph('床上蹬腿的特点是动作幅度小，而且每个人的基线位置不一样。')
doc.add_paragraph('我们用了二维信号（髋关节和膝关节的角度变化）来判定，并且做了自动标定——用户开始做几个动作，系统自动学习他的基线，适配不同身高和床的位置。')
doc.add_paragraph('这样就不会因为每个人体型不同而导致计数不准。')

h2 = doc.add_heading('第12页：后台识别与实时界面反馈（约20秒）', level=2)
doc.add_paragraph('识别过程是在后台线程跑的，不会阻塞界面。')
doc.add_paragraph('界面上实时显示：当前动作名称、实时次数 / 目标次数、进度条、倒计时。')
doc.add_paragraph('用户能直观地看到自己做到哪了，还差几个。')

h2 = doc.add_heading('第13页：达标控制、记录与成就（约20秒）', level=2)
doc.add_paragraph('完成目标后，系统会自动：')
doc.add_paragraph('停止响铃', style='List Number')
doc.add_paragraph('保存这次唤醒记录', style='List Number')
doc.add_paragraph('更新连续完成天数', style='List Number')
doc.add_paragraph('检查是否解锁新成就', style='List Number')
doc.add_paragraph('这些记录和成就可以在主界面查看，给用户持续的正反馈，形成习惯闭环。')

h2 = doc.add_heading('第14页：铃声管理与交互设计（约20秒）', level=2)
doc.add_paragraph('最后是铃声管理模块：支持导入自定义铃声、试听和音量调节、多闹钟、重复日期、稍后提醒。')
doc.add_paragraph('交互设计上我们尽量做得简单，核心操作不超过三步。')

# 板块三
h = doc.add_heading('【板块三：问题的发现与解决】（约1.5分钟，2页）', level=1)

h2 = doc.add_heading('第15页：开发中的问题与解决（一）（约45秒）', level=2)
doc.add_paragraph('接下来第三部分，讲讲我们开发过程中遇到的问题，这部分也是我们真实踩坑的记录。')
doc.add_paragraph('左边是环境配置问题：', style='List Bullet')
doc.add_paragraph('第一个是 OpenCV 和新版 MSVC 不兼容。OpenCV 4.12 官方库是用 vc16 编译的，我们本机是 VS2026，版本号超出了 CMake 的版本映射表，识别不到。解决方法是手动指定 OpenCV_RUNTIME vc16，利用 MSVC 的二进制兼容性来链接。', style='List Bullet 2')
doc.add_paragraph('第二个是 QtMultimedia 模块缺失。Qt 基础安装包不含这个模块，用 MaintenanceTool 补装就好了。', style='List Bullet 2')

doc.add_paragraph('右边是团队协作问题：', style='List Bullet')
doc.add_paragraph('我们用 Git 做版本控制，多人并行开发的时候反复出现合并冲突。尤其是 CMakeLists.txt、MainWindow 这种公共文件，你改我也改。', style='List Bullet 2')
doc.add_paragraph('解决方法是每次都先 fetch + merge 最新主分支，然后人工逐文件解决冲突，保留双方的有效改动。', style='List Bullet 2')

p = doc.add_paragraph()
run = p.add_run('经验就是：公共文件的变更一定要提前在组内沟通，不然合并成本很高。')
run.bold = True

h2 = doc.add_heading('第16页：开发中的问题与解决（二）（约45秒）', level=2)
doc.add_paragraph('这一页是代码层面的问题，全部带真实的编译器错误码：')
doc.add_paragraph('C2672：std::min 类型不匹配，double 和 float 混在一起，模板推不出来，加个 static_cast 就好了。', style='List Bullet')
doc.add_paragraph('C2065：M_PI 未定义。这个坑很多人踩——MSVC 下要定义 _USE_MATH_DEFINES，而且必须早于 cmath 被包含。结果 OpenCV 的头文件抢先包含了 cmath，我们的宏定义就失效了。解决方法是干脆自己定义一个 constexpr kPi，不依赖系统宏。', style='List Bullet')
doc.add_paragraph('LNK2019：Q_OBJECT 的 moc 没生成。这个是因为我们把带 Q_OBJECT 的头文件没加进 CMake 的源文件列表，AUTOMOC 就不会处理它。补进列表就好了。', style='List Bullet')
doc.add_paragraph('LNK2019：还有一个链接错误是因为新写的 ExerciseUtils.cpp 忘了加进 CMakeLists，编译是编译了，但没参与链接。', style='List Bullet')
doc.add_paragraph('运行时错误：数据库参数数量不匹配。这个是因为我们改了表结构，但旧的 .db 文件还在，CREATE TABLE IF NOT EXISTS 不会改已有的表，导致新 SQL 和旧表对不上。解决方法是删掉旧 db 文件重新初始化，并且把 *.db 加进 .gitignore。', style='List Bullet')
doc.add_paragraph('C4819：中文编码问题。MSVC 默认按 GBK 读文件，我们源码是 UTF-8，一堆警告。加个 /utf-8 编译选项就好了。', style='List Bullet')

doc.add_paragraph('右边是测试验证的问题：', style='List Bullet')
doc.add_paragraph('最开始写自测的时候，发现 QTimer 根本不触发。原因是测试程序没跑事件循环，定时器事件没人派发。解决方法是用 QEventLoop 包一层，singleShot 设个超时，然后 loop.exec() 跑起来。', style='List Bullet 2')

p = doc.add_paragraph()
run = p.add_run('最后我们验证了核心规则：挑战未完成时 stop() 返回 false 被拒绝，完成后 stop() 成功。系统模块 10 项自测全部通过。')
run.bold = True

doc.add_paragraph('总结一下我们的经验：善用错误码定位根因，用最小复现缩小问题范围，关注环境差异，规范团队协作。')

# 板块四
h = doc.add_heading('【板块四：测试验证与总结展望】（约1分钟，2页）', level=1)

h2 = doc.add_heading('第17页：测试方案与结果展示（约30秒）', level=2)
doc.add_paragraph('第四部分，测试和总结。')
doc.add_paragraph('我们的测试分几个层面：')
doc.add_paragraph('单元测试：核心计数算法、状态机逻辑，10 项自测全部通过。', style='List Bullet')
doc.add_paragraph('集成测试：闹钟触发 → 识别开始 → 达标停铃 → 记录保存，完整流程跑通。', style='List Bullet')
doc.add_paragraph('边界测试：比如姿势不标准、动作幅度不够、中途停止等情况。', style='List Bullet')
doc.add_paragraph('从测试结果来看，核心功能都是正常工作的。')

h2 = doc.add_heading('第18页：项目总结与后续改进（约30秒）', level=2)
doc.add_paragraph('最后是总结和后续改进方向。')
p = doc.add_paragraph()
run = p.add_run('已实现的功能：')
run.bold = True
doc.add_paragraph('多闹钟与运动解锁的完整流程', style='List Bullet')
doc.add_paragraph('三种动作的规则和状态机', style='List Bullet')
doc.add_paragraph('后台识别、暂停与实时反馈', style='List Bullet')
doc.add_paragraph('本地记录、统计与成就系统', style='List Bullet')

p = doc.add_paragraph()
run = p.add_run('后续改进方向：')
run.bold = True
doc.add_paragraph('多人：支持稳定的人物跟踪，多人场景下能区分谁在做动作', style='List Bullet')
doc.add_paragraph('视角：改进自适应标定，不同角度、不同距离都能识别', style='List Bullet')
doc.add_paragraph('帧率：用时间替代部分帧数限制，提升流畅度', style='List Bullet')
doc.add_paragraph('验证：建立人工标注视频集，做更系统的准确率验证', style='List Bullet')
doc.add_paragraph('运行：评估系统级唤醒和后台支持', style='List Bullet')

p = doc.add_paragraph()
run = p.add_run('底部这句话是我们的愿景：让姿态识别从画面中的关键点，变成真实可用的唤醒助手。')
run.bold = True

# 结束语
h = doc.add_heading('【结束语】（约20秒）', level=1)
doc.add_paragraph('以上就是我们 WakeAI 项目的全部介绍。')
doc.add_paragraph('我们用姿态识别技术解决了一个大学生每天都会遇到的真实问题——早上起不来床。虽然还有很多可以改进的地方，但核心的运动唤醒闭环已经完整跑通了。')
p = doc.add_paragraph()
run = p.add_run('谢谢各位老师，请提问。')
run.bold = True

# 答辩可能问题
doc.add_page_break()
h = doc.add_heading('【附录：答辩可能被问到的问题 & 回答准备】', level=1)

questions = [
    ('Q1：YOLOv8n-pose 准确率怎么样？能识别得准吗？',
     'A：我们用的是 YOLOv8n-pose，是 nano 版本，精度比 large 版低一些，但对于我们的场景足够了。因为我们不是在做复杂的姿态分析，只是判断几个关键角度的变化，而且我们加了关键点平滑和状态机缓冲带，实际误判率很低。'),
    ('Q2：如果用户姿势不标准，会不会识别错？',
     'A：会有这个问题。所以我们在设计状态机的时候留了缓冲带，比如深蹲 140° 到 155° 之间不算开始也不算结束，就是为了容忍一定的姿势不标准。而且我们有三种动作可选，用户可以选自己最方便的。'),
    ('Q3：这个和那些"摇动手机才能关闹钟"的 App 有什么区别？',
     'A：区别很大。摇手机只需要动动手，大脑还是没醒。我们这个是身体层面的唤醒——站起来做运动，心率上来了，血液循环加快了，是真正从生理层面把你叫醒。而且我们是摄像头自动计数，你不用拿着手机数。'),
    ('Q4：开发用了多长时间？分工是怎样的？',
     'A：我们是三人小组，大概做了一个多月。分工上，有人负责算法和姿态识别，有人负责界面和业务逻辑，我主要负责系统架构、闹钟调度和问题调试。Git 协作中也踩了不少合并冲突的坑，刚才那页有讲。'),
    ('Q5：有没有真实用户测试？效果怎么样？',
     'A：目前主要是我们小组内部自测，10 项功能测试全部通过。外部用户测试还没大规模做，这也是我们后续改进的方向之一——建立人工标注视频集，做更系统的准确率和有效性验证。'),
    ('Q6：为什么选这三种动作？',
     'A：深蹲是最经典的唤醒动作，能快速提升心率；开合跳幅度大，容易识别；床上蹬腿是为了照顾不想下床的用户——毕竟冬天谁都不想离开被窝。三种动作覆盖了不同场景和偏好。'),
]

for q, a in questions:
    p = doc.add_paragraph()
    run = p.add_run(q)
    run.bold = True
    doc.add_paragraph(a)

# 时间分配表
doc.add_page_break()
h = doc.add_heading('【时间分配参考】', level=1)

table = doc.add_table(rows=6, cols=3)
table.style = 'Light Grid Accent 1'

hdr_cells = table.rows[0].cells
hdr_cells[0].text = '板块'
hdr_cells[1].text = '页数'
hdr_cells[2].text = '建议时长'

data = [
    ('开场白', '-', '30秒'),
    ('一、背景与价值', '3页', '1.5分钟'),
    ('二、功能与技术实现', '11页', '4.5分钟'),
    ('三、问题与解决', '2页', '1.5分钟'),
    ('四、测试与总结', '2页', '1分钟'),
]

for i, (a, b, c) in enumerate(data):
    row_cells = table.rows[i+1].cells
    row_cells[0].text = a
    row_cells[1].text = b
    row_cells[2].text = c

p = doc.add_paragraph()
run = p.add_run('合计：18页，约9.5分钟')
run.bold = True

# 演讲小贴士
h = doc.add_heading('【演讲小贴士】', level=1)
tips = [
    '板块切换要有过渡语，比如"讲完了为什么做，接下来看看我们具体怎么做的"',
    '问题解决部分是加分项，讲的时候可以稍微放慢，强调"真实踩坑、真实解决"',
    '创新点那页一定要结合早八场景，老师们都是过来人，能共鸣',
    '技术细节不用讲太深，点到为止，重点讲设计思路和解决了什么问题',
    '结尾要有自信，我们的核心价值主张讲清楚了就好',
]
for tip in tips:
    doc.add_paragraph(tip, style='List Number')

# 保存
doc.save('WakeAI_答辩演讲稿.docx')
print('Word文档生成成功！')
