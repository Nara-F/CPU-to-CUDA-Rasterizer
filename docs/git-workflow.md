# 双人 lab Git 简单约定

## 记住这几条

- A/B 按原分工开发，改共享接口或同一段代码前先和对方说一声。
- `main` 保持能编译、已有功能能跑；每个小任务开一个分支，例如 `myName/lab03-mvp`。
- 一次提交做一件事，说明写清楚即可，例如 `lab02: 实现三角形插值`，不强制提交类型。
- 不提交构建目录、临时输出和大文件，不用强制推送覆盖对方的改动。
- 做完提 PR，让另一人看一下、确认没问题后合并。沟通用聊天即可，不要求建 issue 或填复杂模板。

## 每次照这个流程走

以下以 `myName` 做 lab02 为例，替换任务名和文件名即可。

### 1. 开始前同步，开自己的分支

先用 `git status` 确认没有未提交改动；有的话先在原分支处理好。

```sh
git switch main
git pull --ff-only origin main
git switch -c myName/lab02-triangle
```

### 2. 写完检查、测试、提交

按 [README](../README.md#开始) 的命令构建和测试，再跑一下本次 lab 的用例。涉及 CUDA 的功能要在 GPU 上验证；自己没设备就请对方帮忙，并说明哪些还没测。

```sh
git diff
git add src/rasterizer.cpp tests/smoke.cpp
git diff --cached
git commit -m "lab02: 实现三角形插值"
```

只 `git add` 本次实际修改的文件。

### 3. 同步对方的改动，推送并提 PR

在自己的分支上执行：

```sh
git fetch origin
git merge origin/main
# 合入新改动后，再跑一遍相关测试
git push -u origin myName/lab02-triangle
```

在仓库页面创建 PR，目标选 `main`。写清三件事：**做了什么、怎么验证、还有什么问题**。另一人看改动和结果，有问题就修改、补提交并再次 push。

遇到冲突，两人确认要保留的内容，编辑冲突文件、删除冲突标记，再 `git add` 对应文件并 `git commit`，测试通过后推送。不确定时用 `git merge --abort` 取消这次合并，先一起看。

### 4. 合并，开始下一项

对方确认最终改动、相关验证通过后，点 **Squash and merge**，并在页面删除已合并的远程分支。如果对方的 PR 先合并了，先回到步骤 3 同步、测试，再合并自己的。

```sh
git switch main
git pull --ff-only origin main
```

下一项任务重新开分支即可。合并后发现问题，就开一个修复分支，按同样流程修复。
