# GitHub 上传速查卡

## 当前工程目录

```powershell
cd "G:\ESP32_WORK\ESP32_PRO\ESP32_projest_exmplate_v2\Project_2_1_0_BUTTON"
```

## 第一次上传

### 1. 设置 Git 用户

```powershell
git config --global user.name "你的GitHub用户名"
git config --global user.email "你的GitHub邮箱"
```

### 2. 本地提交

```powershell
git add .
git commit -m "Initial ESP-IDF project archive"
```

### 3. 切换主分支名

```powershell
git branch -M main
```

### 4. 添加远程仓库

把下面地址替换成你自己的：

```powershell
git remote add origin https://github.com/你的用户名/Project_2_1_0_BUTTON.git
```

### 5. 推送到 GitHub

```powershell
git push -u origin main
```

## 后续每次存档

```powershell
git add .
git commit -m "本次修改说明"
git push
```

## 常用检查命令

### 查看状态

```powershell
git status
```

### 查看远程仓库

```powershell
git remote -v
```

### 查看当前分支

```powershell
git branch --show-current
```

## 远程仓库地址填错时

```powershell
git remote remove origin
git remote add origin https://github.com/你的用户名/Project_2_1_0_BUTTON.git
```

## 如果想上传 `sdkconfig`

编辑 `.gitignore`，删除这一行：

```gitignore
sdkconfig
```

然后重新执行：

```powershell
git add .
git commit -m "Track sdkconfig"
git push
```

## 推荐提交信息

```powershell
git commit -m "Fix component CMakeLists"
git commit -m "Remove esp_lcd from build components"
git commit -m "Add debug documentation"
```
