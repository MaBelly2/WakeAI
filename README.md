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



