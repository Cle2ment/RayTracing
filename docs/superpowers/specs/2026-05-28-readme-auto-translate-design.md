# README 自动翻译功能设计

**日期**: 2026-05-28
**状态**: Approved

## 目标

维护 `docs/README_zh-CN.md`，每次 `README.md` 变更时自动通过 DeepSeek API 翻译并提交。

## 架构

```
push to master (README.md changed)
  → translate-readme.yml (GitHub Actions)
    → translate_readme.py (Python)
      → DeepSeek API (deepseek-v4-flash)
    → git commit + push docs/README_zh-CN.md
```

## 新增文件

| 文件 | 用途 |
|------|------|
| `.github/workflows/translate-readme.yml` | CI 编排 |
| `.github/scripts/translate_readme.py` | 翻译脚本 |
| `docs/README_zh-CN.md` | 翻译输出 |

## 工作流设计

### 触发条件
- push 到 master 分支，且 `README.md` 在变更路径中
- 支持 `workflow_dispatch` 手动触发

### 步骤
1. Checkout 仓库（`actions/checkout@v4`）
2. Setup Python 3.x
3. 安装 `openai` 包
4. 运行 `translate_readme.py`
5. 检查 `docs/README_zh-CN.md` 是否有变更
6. 如有变更：`git add` → `git commit` → `git push`（使用 `GITHUB_TOKEN`）

### 安全
- `DEEPSEEK_API_KEY` 通过 GitHub Secrets 注入
- 提交使用 `github-actions[bot]` 身份
- Commit message 包含 `[skip ci]` 防止循环触发

## 翻译脚本设计

### API 配置
- Endpoint: `https://api.deepseek.com/v1`
- Model: `deepseek-v4-flash`
- SDK: `openai` Python 包（完全兼容）

### System Prompt 策略
- 翻译所有英文段落为中文
- 保留：Markdown 结构、代码块、ASCII 图、Badge URL、表格、HTML
- 不翻译：已有的中文内容（Troubleshooting 表格等）
- 不翻译：代码、命令、文件路径、技术术语

### 输入/输出
- 输入：根目录 `README.md`
- 输出：`docs/README_zh-CN.md`（自动创建目录）

## 边界条件

- `docs/` 目录不存在 → 脚本自动创建
- API 调用失败 → workflow 失败，不提交
- README.md 未变更 → 跳过，不提交空 commit
- 首次运行 → 生成初始翻译

## 非目标

- 不翻译 Walnut 子模块的 README
- 不支持多语言（仅中英）
- 不做增量翻译（每次全量翻译）
- 不做翻译质量审查（人工检查即可）
