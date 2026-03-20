# menu_service代码流程解释

## 这个模块是做什么的

`menu_service` 是 `v2.2.0` 新增的轻量菜单状态机模块。

它的职责不是做复杂 UI，而是先建立一个最小可复用的菜单模板：

- 进入菜单
- 退出菜单
- 页面切换
- 选项切换
- 执行动作

当前它配合的主链是：

```text
XL9555 -> button_service -> app_event_task -> menu_service -> display_service
```

## 为什么菜单逻辑不直接写在 app_event_task 里

因为 `app_event_task` 的职责应该是：

- 接收统一事件
- 分发给不同业务模块

如果把菜单状态机直接写进 `app_event_task`，后面会越来越乱：

- 菜单状态
- 页面切换
- 选中项切换
- 动作执行
- 首页恢复

这些都不适合堆在事件分发层里。

所以这里单独拆出：

- `menu_service.c`
- `menu_service.h`

更利于以后继续扩展菜单模板。

## 当前菜单页有哪些

当前只做了 3 页：

### 1. `MENU_PAGE_HOME`

显示：

- 当前版本
- 当前配置来源
- 当前 Wi-Fi 状态

### 2. `MENU_PAGE_CONFIG`

显示：

- `SSID`
- `HTTP URL` 简写
- `OTA URL` 简写

### 3. `MENU_PAGE_ACTION`

显示：

- `RESET_CFG`
- `RELOAD_CFG`
- `REBOOT`

这 3 页足够建立菜单模板，但还不会把复杂功能做重。

## menu_service_ctx_t 在存什么

核心状态结构是：

```c
typedef struct {
    bool inited;
    bool visible;
    menu_page_t page;
    uint8_t action_index;
} menu_service_ctx_t;
```

这几个字段作用分别是：

- `inited`
  - 菜单服务是否完成初始化
- `visible`
  - 当前菜单是否显示中
- `page`
  - 当前在哪一页
- `action_index`
  - `ACTION` 页当前选中了哪一项

你可以把它理解成：

```text
当前菜单开没开
当前在哪个页面
当前动作页选中了哪个动作
```

## 整个模块的主入口是谁

真正的核心入口是：

- `menu_service_handle_button_event()`

因为菜单本质上就是一个“按键驱动状态机”。

它的输入是：

- `button_id`
- `button_event`

它的输出是：

- 更新菜单状态
- 更新屏幕显示
- 必要时执行动作
- 返回这个事件是否被菜单消费

## 为什么它要返回 bool

函数签名是：

```c
bool menu_service_handle_button_event(button_id_t button_id, button_event_t button_event)
```

这个返回值非常关键。

意思是：

- `true`
  - 说明这次按键事件已经被菜单处理了
  - 后面不要再继续走原来的 LED / BEEP 业务逻辑
- `false`
  - 说明这次事件不是菜单要处理的
  - 原业务逻辑继续走

所以在 `app_event_task.c` 里会先判断：

```c
if (menu_service_handle_button_event(button_id, button_event)) {
    return;
}
```

这就是“菜单优先接管按键”的关键点。

## 菜单是怎么进入的

菜单未显示时，只认一个入口：

- `KEY3` 长按

也就是：

```text
BTN_FUNC + BUTTON_EVENT_LONG
```

对应逻辑在：

- `menu_service_handle_button_event()`

当满足这个条件时，会调用：

- `menu_service_enter()`

## menu_service_enter() 做了什么

进入菜单时，它会做 3 件事：

1. `visible = true`
2. `page = MENU_PAGE_HOME`
3. `action_index = 0`

然后调用：

- `menu_service_render()`

也就是说，每次进入菜单都回到一个稳定起点：

```text
菜单显示
-> 首页
-> 动作选中项回到第一个
```

## 菜单是怎么退出的

菜单显示后，`KEY3` 短按负责：

- 返回
- 或退出

规则是：

- 如果当前就在 `HOME` 页
  - 直接退出菜单
- 如果当前在 `CONFIG / ACTION`
  - 先回到 `HOME`

对应函数是：

- `menu_service_exit()`

退出后它会调用：

- `display_service_hide_menu()`

这会让 LCD 从菜单覆盖页回到原来的首页状态刷新。

## KEY0 / KEY1 为什么在不同页面行为不同

这是这版菜单里最容易绕的一点。

### 在 `HOME / CONFIG` 页

- `KEY0`
  - 上一页
- `KEY1`
  - 下一页

也就是在页面层面移动。

### 在 `ACTION` 页

- `KEY0`
  - 上一个动作
- `KEY1`
  - 下一个动作

也就是在动作项里移动，不再切页。

为什么这么设计？

因为 `ACTION` 页里已经不是“看信息”，而是“选动作”了。  
所以这时候按键更应该服务于动作选择，而不是继续翻页。

## KEY2 为什么在三页里行为不同

`KEY2` 当前统一被当成：

- 确认键

但在不同页面里确认的对象不同：

### 在 `HOME`

确认后：

- 进入 `CONFIG`

### 在 `CONFIG`

确认后：

- 进入 `ACTION`

### 在 `ACTION`

确认后：

- 执行当前选中的动作

所以 `KEY2` 的语义始终是统一的：

```text
确认当前页面的“下一步”
```

## menu_service_render() 是做什么的

这是菜单显示层的桥梁函数。

它的任务不是直接操作底层 LCD，而是：

- 根据当前页
- 组织出标题和三行文本
- 再交给 `display_service_show_menu_page()`

也就是说它做的是：

```text
菜单状态
-> 转成显示内容
-> 交给 display_service
```

比如：

### `MENU_PAGE_HOME`

组织成：

- `VER ...`
- `CFG ...`
- `WIFI ...`

### `MENU_PAGE_CONFIG`

组织成：

- `SSID ...`
- `HTTP ...`
- `OTA ...`

### `MENU_PAGE_ACTION`

组织成：

- `RESET_CFG`
- `RELOAD_CFG`
- `REBOOT`

并把 `action_index` 作为高亮选中项传给显示层。

## 切到菜单后，真正是谁在画菜单

这是这块最容易搞混的一点。

菜单显示链路其实分成三层：

### 第 1 层：`menu_service_render()`

位置：

- `menu_service.c`

它负责：

- 根据当前菜单页
- 组织标题和三行文本
- 再调用：
  - `display_service_show_menu_page(...)`

也就是说，这一层只是把“菜单要显示什么”告诉显示层。

它**不直接画 LCD**。

### 第 2 层：`display_service_show_menu_page(...)`

位置：

- `display_service.c`

它负责：

- `menu_visible = true`
- `menu_dirty = true`
- 把：
  - `title`
  - `line1`
  - `line2`
  - `line3`
  - `selected_index`
  写进显示缓存

这一层本质上还是：

```text
更新显示缓存
```

它也**不是最终真正落到屏幕上的那一步**。

### 第 3 层：`display_service_process()`

真正执行菜单绘制的是这里。

当 `display_service_process()` 发现：

- `s_display.menu_visible == true`

就会优先走菜单绘制分支：

```text
if menu_visible
-> display_service_draw_menu_page()
-> return
```

这里的意思是：

- 菜单一旦显示中
- 就暂停首页普通区域的刷新逻辑
- 先专门画菜单页

### 最底层真正绘制函数：`display_service_draw_menu_page()`

这个函数才是最终直接调用 LCD 绘图接口的地方。

它会做：

1. 清屏
2. 画菜单标题
3. 画三行菜单项
4. 根据 `menu_selected_index` 做高亮
5. 画底部按键提示

所以你可以把整条链记成：

```text
menu_service_render()
-> display_service_show_menu_page()
-> display_service_process()
-> display_service_draw_menu_page()
-> LCD
```

一句话总结：

- `menu_service` 决定“菜单显示什么”
- `display_service` 决定“菜单怎么画出来”

## 为什么 URL 要截短

在 `CONFIG` 页里你会看到：

- `menu_service_copy_short_text()`

它的目的就是：

- 把很长的 URL 截成适合菜单页显示的长度

因为菜单页空间有限。  
这里当前只需要看：

- 地址是不是大体对

不需要完整显示一整条很长的 URL。

## menu_service_execute_action() 做了什么

当前动作页只有三个动作：

### 1. `RESET_CFG`

执行：

- `config_service_reset_to_default()`
- 再 `config_service_load()`

### 2. `RELOAD_CFG`

执行：

- `config_service_load()`

### 3. `REBOOT`

执行：

- `esp_restart()`

执行完配置相关动作后，还会调用：

- `menu_service_sync_home_summary()`

作用是把首页里的：

- `CFG : ...`

同步成最新来源摘要。

## 为什么菜单服务还要调 display_service

因为菜单服务只负责“菜单状态机”，不应该自己关心：

- LCD 坐标
- 字体颜色
- 清屏方式
- 屏幕布局

这些都属于显示层职责。

所以菜单服务只做：

```text
告诉显示层：
现在该显示什么
```

而具体怎么画，仍然由：

- `display_service`

负责。

## 菜单模式下为什么要屏蔽原来的业务按键逻辑

如果菜单显示时还让按键继续触发原来的：

- LED 切换
- 蜂鸣器控制

那你在菜单里翻页时，外面的业务状态也会乱动。  
这会非常混乱。

所以当前规则是：

- 菜单显示时，按键优先由菜单消费
- 原来的业务逻辑暂停响应

这就是为什么：

- `menu_service_handle_button_event()` 返回 `true`

时，`app_event_task` 就直接 `return` 了。

## 当前这版菜单的限制

这版是模板版，不是产品版，所以刻意做得很轻：

- 不做字符串编辑
- 不做输入 URL
- 不做输入 SSID
- 不做复杂多级菜单
- 不做动画
- 不做 LVGL

目的是先沉淀：

- 菜单状态机骨架
- 按键导航骨架
- 动作执行骨架

## 一句话总结

`menu_service` 的本质就是：

```text
把现有按键事件
转换成一个最小菜单状态机
并通过 display_service 把菜单页显示出来
```

如果你后面继续扩展，这个模块最自然的下一步就是：

- 增加更多页面
- 增加更多动作
- 增加配置项查看
- 再往后才考虑真正的字符串编辑
