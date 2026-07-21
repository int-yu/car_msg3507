# 项目源码教程

最终网页是 `tutorial/index.html`，不需要服务器或网络，直接用浏览器打开即可。

讲解内容分散在 `tutorial/sections/`，这样每个项目模块可以独立核对；根目录执行以下命令会把它们合并为一个离线网页：

```powershell
node tutorial/build.mjs
node tests/test_tutorial.mjs
```

当源码发生变化时，应同步更新对应模块的代码行号、逐行说明、调用关系和覆盖清单，然后重新执行构建与检查。
