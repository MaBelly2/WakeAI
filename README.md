# WakeAI

AI智能运动唤醒助手

WakeAI 第一轮动作检测优化包



直接覆盖/新增以下文件：

1\. include/core/PoseSmoother.h      （新增）

2\. src/core/PoseSmoother.cpp        （新增）

3\. include/exercise/ExerciseBase.h  （覆盖）

4\. include/exercise/Squat.h         （覆盖）

5\. src/exercise/Squat.cpp           （覆盖）

6\. include/exercise/JumpingJack.h   （覆盖）

7\. src/exercise/JumpingJack.cpp     （覆盖）

8\. include/exercise/Cycling.h       （覆盖）

9\. src/exercise/Cycling.cpp         （覆盖）

10\. src/main.cpp                    （覆盖）



PoseData.h 仅用于本地语法检查，不需要覆盖仓库现有 PoseData.h。



注意：项目当前 CMakeLists.txt 使用 GLOB\_RECURSE 收集 src/\*.cpp；新增 PoseSmoother.cpp 后请在 VS2022 中重新配置 CMake，否则旧缓存可能没有把它加入构建。

WakeAI 床上蹬腿 V2 补丁



本补丁针对“用户躺在床上，手持手机，只拍胯以下”的场景。



替换文件：

1\. include/exercise/Cycling.h

2\. src/exercise/Cycling.cpp

3\. src/main.cpp



不要修改：PoseData.h、PoseDetector.cpp、PoseSmoother.\*、Squat.\*、JumpingJack.\*



V2核心变化：

\- 不再要求左右髋点都可见；主计数只要求左右膝+左右踝。

\- 不再用左右膝角 A/B 作为唯一计数依据。

\- 自动从两种二维信号中选择更稳定者：

&#x20; A. 左右小腿二维投影长度的 log 比值（ShinScale）

&#x20; B. 左右膝 y 坐标差 / 平均小腿长度（KneeVertical）

\- 开始前自动标定运动动态范围。

\- A->B->A 记为 1 个完整循环。

\- 允许短暂关键点丢失，降低局部人体拍摄下的断帧影响。

\- 新 CSV 会输出髋/膝/踝置信度、信号、标定状态和所选信号源。



VS2022替换后：

1\. 删除 CMake 缓存并重新配置（如 VS 有该菜单）。

2\. Ctrl+Shift+B 全部生成。

3\. 继续用 cycling\_01.mp4 测试。

4\. 运行前几次蹬腿是自动标定阶段，CALIB 会从 NO 变 YES。

5\. 如果整段视频一直 ACTION\_VALID=NO：main.cpp 中 setPartialBodyConfig 的 0.22f 改为 0.18f。

6\. 如果 CALIB=YES 但不切 LEFT\_PHASE/RIGHT\_PHASE：把 setCalibrationConfig 中 0.10f 和 0.30f 分别降至 0.07f 和 0.20f。

7\. 若误计数多：confirmFrames 从2改3，或 minHalfCycleFrames 从4改5/6。



说明：热力颜色只表示二维相对尺度/运动趋势，不代表真实三维距离。

WakeAI Cycling V3（针对 cycling\_02：人工15，程序19 的过计数修复）



替换文件：

1\. include/exercise/Cycling.h

2\. src/exercise/Cycling.cpp

3\. src/main.cpp



V3 关键修改：

\- SHIN\_SCALE trigger 绝对下限：0.10（旧日志为0.035，过小）

\- KneeY trigger 下限：0.12

\- 稳定相位最小间隔：8帧

\- 两次计数最小间隔：10帧

\- 保持膝+踝局部人体检测方案

\- 默认 EachPedal：每次稳定左右相位切换计1次



依据 debug\_log(2).csv：

\- 自动标定约第21帧完成，baseline=0.017，trigger=0.035

\- 后续真实 SHIN\_SCALE 动态可接近 \[-1.2, +1.2]

\- 两个主要误触发短段：237\~242帧、430\~435帧

\- 它们只是信号在很小阈值附近来回穿越，造成 RIGHT->LEFT->RIGHT 假相位

\- 使用 trigger=0.10、confirm=2、stable gap=8 后，在该 CSV 离线重放得到15次相位切换计数



编译：覆盖文件后 Ctrl+Shift+B。若VS未刷新，删除CMake缓存并重新配置。



