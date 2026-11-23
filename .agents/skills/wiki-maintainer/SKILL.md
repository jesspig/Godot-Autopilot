---
name: wiki-maintainer
description: "维护项目知识库：按索引查询页面、按源码变更增量更新页面、校验格式与链接。何时用：查询 wiki、更新 wiki、同步文档、维护项目知识库；不用于从零建库与全量重写。"
---

# Wiki Maintainer

WIKI 根为 `docs/wiki`；本页路径均相对仓库根书写。

## 一、查询协议

- 读 docs/wiki/index.md，按链接加一句话描述定位候选页。
- 查候选页 frontmatter 的 type、title、description、tags 做二次过滤，只用 grep 结果定位。
- 按 title/description/tags 命中数降序、再按 timestamp 降序，打开 Top1 至 Top3 页面正文；其余页面不再打开。
- 答复只依据已打开页面内容，找不到时直说未知，不编造。
- 完成信号：已给出答案加引用页，或已明确答复未知。

## 二、更新协议

- 比对 `git diff <source-commit>..HEAD` 输出，列出受影响源码文件清单；`<source-commit>` 取 `docs/wiki/log.md` 最近构建记录的 commit，首次构建用初始提交，缺失时询问用户。
- 改受影响页面正文，不碰无关页面。
- 删过时页面或段落，不留已删除标记，不留空链接。
- 更新每页的 timestamp 为真实当前 ISO8601 时间。
- 写 docs/wiki/log.md 当天条目，记录本次构建的 commit，合并同一天变更，只保留最近 7 天。
- 写 docs/wiki/changelog 下新文件，标题用真实时间，格式为 `YYYY/MM/DD HH:mm`。
- 刷新 docs/wiki/index.md，保持链接加一句话描述与现有页面一致。
- 限触发时机为功能完成、交付指南变化、提交前三种情况。
- 完成信号：受影响页、timestamp、log、changelog、index 均已一致。

## 三、格式

- 写每页 frontmatter 时填 type 为自解释短语，必填。
- 写 title 与 description 各用一句话概括本页。
- 写 tags 为列表，收录本页关键词。
- 加源码对应页的 resource URI，抽象概念页不加。
- 保持 docs/wiki/index.md 为链接加一句话描述的派生目录，不加 frontmatter。
- 保持 docs/wiki/log.md 按天合并只留 7 天，不加 frontmatter。
- 写正文用目标仓库既有文档语言的纯 Markdown，链接以 `/` 开头指向知识库根。
- 禁止粘贴大段源码，只写结论加最小必要片段。
- 页内引用只做一层，禁止链式跳转。
- 完成信号：格式校验通过，无缺字段、无大段源码、无链式引用。

## 四、约束

- 依据实际源码写文档，禁止推测未实现行为；禁止全量重写 WIKI，只做增量变更。
- 重算引用的每个数值，不照抄旧文档数字。
- 标无法核实内容为 `> [!todo] 待补充` 块，不留空白断言。
- 保持每页至少含一个指向库内其他页的链接。
- 逐页检查链接可达；按至少 20% 且覆盖每个目录抽查 index 与页面对应、log 与 changelog 对齐。
- 禁止为单页编造不存在的源码依据；禁止保留过时内容并加注说明，直接删除或改写。
- 禁止打开超过 3 个页面做查询，禁止通读全库；禁止写营销式介绍，只写操作步骤与事实。
- 完成信号：审计无断链、无孤岛页、无时间戳过期（timestamp 早于对应源码最后一次变更），变更集最小可用。
