# GitHub 版本存档操作笔记

## 目的

把本地 ESP-IDF 工程上传到 GitHub，作为版本存档，方便：

- 保存历史版本
- 记录每次修改
- 多台电脑同步
- 后续回退到稳定版本

本笔记以当前工程为例：

`Project_2_1_0_BUTTON`

---

## 一、开始前先确认

当前工程目录：

```powershell
G:\ESP32_WORK\ESP32_PRO\ESP32_projest_exmplate_v2\Project_2_1_0_BUTTON
```

建议上传到 GitHub 的内容：

- `main/`
- `components/`
- `docs/`
- `CMakeLists.txt`
- `.gitignore`
- 需要时可上传 `sdkconfig`

不建议上传的内容：

- `build/`
- `*.log`
- `.vscode/`
- 临时缓存文件
- 编译输出文件如 `.bin`、`.elf`

---

## 二、第一次上传 GitHub 的完整步骤

### 第 1 步：进入工程目录

```powershell
cd "G:\ESP32_WORK\ESP32_PRO\ESP32_projest_exmplate_v2\Project_2_1_0_BUTTON"
```

### 第 2 步：检查 Git 是否初始化

```powershell
git status
```

如果提示不是 Git 仓库，则初始化：

```powershell
git init
```

---

## 三、设置 Git 用户信息

如果第一次使用 Git，需要先设置用户名和邮箱：

```powershell
git config --global user.name "你的GitHub用户名"
git config --global user.email "你的GitHub邮箱"
```

例如：

```powershell
git config --global user.name "yourname"
git config --global user.email "yourname@example.com"
```

检查是否设置成功：

```powershell
git config --global --list
```

---

## 四、检查 `.gitignore`

`.gitignore` 的作用是告诉 Git 哪些文件不要上传。

本工程已经做了适合 ESP-IDF 工程的忽略规则，重点是：

- 忽略 `build/`
- 忽略 `*.log`
- 忽略 `.vscode/`
- 忽略编译产物

查看内容：

```powershell
Get-Content .gitignore
```

### 关于 `sdkconfig`

如果 `.gitignore` 里有：

```gitignore
sdkconfig
```

表示 `sdkconfig` 不会上传。

是否上传 `sdkconfig`，取决于你的目的：

### 情况 1：只做源码存档

可以不上传 `sdkconfig`

优点：

- 仓库更干净
- 不带本机配置痕迹

缺点：

- 别人拉下来后不能完全复现你的编译配置

### 情况 2：要完整复现工程配置

建议上传 `sdkconfig`

做法：

把 `.gitignore` 中这一行删除：

```gitignore
sdkconfig
```

---

## 五、第一次本地提交

### 第 1 步：查看当前状态

```powershell
git status
```

### 第 2 步：加入暂存区

```powershell
git add .
```

### 第 3 步：再次检查

```powershell
git status
```

### 第 4 步：创建第一次提交

```powershell
git commit -m "Initial ESP-IDF project archive"
```

---

## 六、在 GitHub 创建远程仓库

在浏览器打开 GitHub，按以下步骤操作：

1. 登录 GitHub
2. 点击右上角 `New repository`
3. 仓库名建议填写：

```text
Project_2_1_0_BUTTON
```

4. 选择：
   - `Private`：仅自己可见
   - `Public`：公开可见
5. 不要勾选：
   - `Add README`
   - `Add .gitignore`
   - `Choose a license`
6. 点击创建仓库

创建完成后，GitHub 会给出仓库地址，例如：

```text
https://github.com/你的用户名/Project_2_1_0_BUTTON.git
```

---

## 七、绑定远程仓库并上传

### 第 1 步：把本地分支改成 `main`

```powershell
git branch -M main
```

### 第 2 步：绑定远程仓库

把下面地址替换成你自己的 GitHub 仓库地址：

```powershell
git remote add origin https://github.com/你的用户名/Project_2_1_0_BUTTON.git
```

### 第 3 步：检查远程地址

```powershell
git remote -v
```

### 第 4 步：第一次推送

```powershell
git push -u origin main
```

如果 GitHub 弹出登录授权，按提示完成即可。

---

## 八、以后每次做版本存档的方法

以后每改完一次工程，执行下面三步即可。

### 第 1 步：查看改了哪些文件

```powershell
git status
```

### 第 2 步：加入暂存区

```powershell
git add .
```

### 第 3 步：提交

```powershell
git commit -m "描述本次修改内容"
```

### 第 4 步：推送到 GitHub

```powershell
git push
```

---

## 九、推荐的提交信息写法

提交信息建议简短、明确。

例如：

```powershell
git commit -m "Fix component CMakeLists"
git commit -m "Remove esp_lcd from build components"
git commit -m "Add debug documentation"
git commit -m "Update button handling logic"
```

不建议写成：

```powershell
git commit -m "update"
git commit -m "修改了一些东西"
```

因为后面很难回看历史。

---

## 十、如果推送失败，常见处理

### 1. 提示没有用户名邮箱

执行：

```powershell
git config --global user.name "你的GitHub用户名"
git config --global user.email "你的GitHub邮箱"
```

### 2. 提示远程仓库已存在

先查看：

```powershell
git remote -v
```

如果需要替换远程地址：

```powershell
git remote remove origin
git remote add origin https://github.com/你的用户名/Project_2_1_0_BUTTON.git
```

### 3. 提示认证失败

通常原因：

- GitHub 未登录
- Token 失效
- 使用了错误账号

解决方法：

- 重新登录 GitHub
- 或重新配置 Git Credential

### 4. 提示分支冲突

第一次上传通常用：

```powershell
git push -u origin main
```

如果远程仓库是空的，一般不会冲突。

---

## 十一、推荐工作流

推荐每完成一个阶段就提交一次，不要把很多改动堆在一起再提交。

例如：

1. 工程初始导入后提交一次
2. 修复编译错误后提交一次
3. 新增功能后提交一次
4. 调试完成后再提交一次

这样好处是：

- 出问题容易回退
- 历史版本清晰
- 每次修改原因容易追踪

---

## 十二、当前工程可直接执行的命令模板

如果你现在就要上传，直接按下面执行。

### 1. 进入目录

```powershell
cd "G:\ESP32_WORK\ESP32_PRO\ESP32_projest_exmplate_v2\Project_2_1_0_BUTTON"
```

### 2. 第一次提交

```powershell
git add .
git commit -m "Initial ESP-IDF project archive"
```

### 3. 改分支名

```powershell
git branch -M main
```

### 4. 添加远程仓库

```powershell
git remote add origin https://github.com/你的用户名/Project_2_1_0_BUTTON.git
```

### 5. 推送

```powershell
git push -u origin main
```

---

## 十三、最终建议

如果你的目标是“长期稳定保存 ESP-IDF 工程版本”，建议：

1. 保留 `docs/`
2. 每次修复问题就写一份简短笔记
3. 每次功能完成后立刻提交 Git
4. 不要把 `build/` 上传到 GitHub
5. 是否上传 `sdkconfig`，按你是否需要复现编译配置决定

---

## 十四、总结

上传到 GitHub 的核心流程只有四步：

```powershell
git add .
git commit -m "本次修改说明"
git remote add origin 你的仓库地址
git push -u origin main
```

后续日常维护则是：

```powershell
git add .
git commit -m "本次修改说明"
git push
```

如果只是做版本存档，这已经足够。
