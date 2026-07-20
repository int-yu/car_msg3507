# 教程 HTML 片段约定

每个子代理只创建自己负责的 `tutorial/sections/NN-name.html`，不要修改源码、其他片段或最终页面。

片段最外层必须是：

```html
<section class="lesson-section" id="module-NN" data-title="中文标题" data-group="学习阶段">
  ...
</section>
```

正文要求：

- 使用 `h2` 作为模块标题，`h3` 作为固定栏目标题，必要时使用 `h4`。
- 开头放一个 `.module-map`，写清职责、输入、输出、依赖、调用者和总体位置。
- 专业术语使用 `<dfn data-term="英文或缩写">中文术语</dfn>`，紧邻通俗解释。
- 源码块使用 `<pre class="source-code"><code data-source="相对路径" data-lines="起止行">...</code></pre>`，必须进行 HTML 转义。
- 每个源码块后使用 `<ol class="line-notes">`；一项可以解释一行或紧密相关的 2–4 行，但必须明确写出行号，不能只概括整个函数。
- 难函数使用 `.deep-dive`，依次说明输入、过程、输出、边界条件、设计原因。
- 模块关系使用 `.relation-chain`，用文本箭头表示调用链。
- 常见误解使用 `.pitfall`，修改联动使用 `.ripple-effect`。
- 自测题放在 `<details class="quiz"><summary>问题</summary><p>答案</p></details>`。
- 末尾必须有 `.coverage-list`，逐文件列出覆盖范围和处理方式。
- 不插入 `<style>`、`<script>`、外部资源、Markdown 或待办占位符。

讲解深度：以第一次系统学习 C 语言与嵌入式项目的读者为准。解释 `static`、`volatile`、指针、位运算、枚举、结构体、回调、中断、状态机、环形缓冲区、PID 等术语第一次出现的含义；重复出现时说明本处的具体作用。
