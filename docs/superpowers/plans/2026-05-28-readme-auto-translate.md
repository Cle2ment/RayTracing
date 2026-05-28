# README 自动翻译功能 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建 GitHub Actions 工作流，在 README.md 变更时自动通过 DeepSeek V4 Flash 翻译为 `docs/README_zh-CN.md` 并提交。

**Architecture:** GitHub Actions workflow 触发 → Python 脚本调用 DeepSeek API（OpenAI 兼容）→ 翻译 Markdown → git commit + push。两个文件，零额外依赖。

**Tech Stack:** GitHub Actions, Python 3, `openai` SDK, DeepSeek API (deepseek-v4-flash)

**Prerequisites:** 用户需在 GitHub 仓库 Settings → Secrets and variables → Actions 中创建 `DEEPSEEK_API_KEY` secret。

---

### Task 1: 创建翻译脚本

**Files:**
- Create: `.github/scripts/translate_readme.py`

- [ ] **Step 1: 编写翻译脚本**

```python
#!/usr/bin/env python3
"""
README.md 自动翻译脚本。
调用 DeepSeek API (deepseek-v4-flash) 将 README.md 翻译为中文，
输出到 docs/README_zh-CN.md。
"""

import os
import sys
from pathlib import Path

from openai import OpenAI


def main():
    api_key = os.environ.get("DEEPSEEK_API_KEY")
    if not api_key:
        print("ERROR: DEEPSEEK_API_KEY environment variable not set", file=sys.stderr)
        sys.exit(1)

    repo_root = Path(__file__).resolve().parent.parent.parent
    readme_path = repo_root / "README.md"
    output_path = repo_root / "docs" / "README_zh-CN.md"

    if not readme_path.exists():
        print(f"ERROR: {readme_path} not found", file=sys.stderr)
        sys.exit(1)

    readme_content = readme_path.read_text(encoding="utf-8")

    client = OpenAI(
        api_key=api_key,
        base_url="https://api.deepseek.com/v1",
    )

    system_prompt = """\
You are a technical translator specializing in Markdown documentation.
Translate the following README.md from English to Simplified Chinese (zh-CN).

STRICT RULES:
1. Translate ONLY English prose into Chinese. Do NOT translate:
   - Markdown structure (headings, lists, tables, code fences)
   - Code blocks, inline code, commands, file paths
   - Badge URLs, image URLs, hyperlinks
   - ASCII art diagrams
   - Technical terms: CUDA, Vulkan, GPU, CPU, NVIDIA, SDK, API, etc.
   - Content that is ALREADY in Chinese (like the Troubleshooting table)
2. Preserve ALL Markdown formatting exactly — headings, bold, italic, links, tables, code blocks.
3. Keep ALL URLs, image paths, and badge markdown unchanged.
4. Output ONLY the translated Markdown content. No explanations, no preambles.
5. The output must be a valid, complete Markdown file."""

    user_prompt = f"Translate this README.md to Chinese:\n\n{readme_content}"

    try:
        response = client.chat.completions.create(
            model="deepseek-v4-flash",
            messages=[
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt},
            ],
            temperature=0.3,
            max_tokens=4096,
        )
    except Exception as e:
        print(f"ERROR: DeepSeek API call failed: {e}", file=sys.stderr)
        sys.exit(1)

    translated = response.choices[0].message.content

    # Ensure output directory exists
    output_path.parent.mkdir(parents=True, exist_ok=True)

    output_path.write_text(translated, encoding="utf-8")
    print(f"Translation written to {output_path}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: 本地验证脚本语法**

```powershell
python -c "import ast; ast.parse(open('.github/scripts/translate_readme.py', encoding='utf-8').read()); print('Syntax OK')"
```

Expected: `Syntax OK`

- [ ] **Step 3: 提交**

```powershell
git add .github/scripts/translate_readme.py
git commit -m "feat: add README auto-translate script"
```

---

### Task 2: 创建 GitHub Actions 工作流

**Files:**
- Create: `.github/workflows/translate-readme.yml`

- [ ] **Step 1: 编写工作流文件**

```yaml
name: Translate README

on:
  push:
    branches: [master]
    paths:
      - 'README.md'
  workflow_dispatch:

concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  translate:
    name: Translate README to Chinese
    runs-on: ubuntu-latest

    steps:
      - name: Checkout Repository
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Detect README.md Change
        id: detect
        shell: bash
        run: |
          if git diff --name-only ${{ github.event.before }} ${{ github.sha }} | grep -q "^README.md$"; then
            echo "changed=true" >> $GITHUB_OUTPUT
            echo "README.md changed — will translate"
          else
            echo "changed=false" >> $GITHUB_OUTPUT
            echo "README.md not changed — skipping"
          fi

      - name: Setup Python
        if: steps.detect.outputs.changed == 'true'
        uses: actions/setup-python@v5
        with:
          python-version: '3.x'

      - name: Install Dependencies
        if: steps.detect.outputs.changed == 'true'
        shell: bash
        run: pip install openai

      - name: Run Translation
        if: steps.detect.outputs.changed == 'true'
        shell: bash
        env:
          DEEPSEEK_API_KEY: ${{ secrets.DEEPSEEK_API_KEY }}
        run: python .github/scripts/translate_readme.py

      - name: Commit Translation
        if: steps.detect.outputs.changed == 'true'
        shell: bash
        run: |
          if git diff --quiet docs/README_zh-CN.md; then
            echo "No changes to commit"
            exit 0
          fi
          git config user.name "github-actions[bot]"
          git config user.email "github-actions[bot]@users.noreply.github.com"
          git add docs/README_zh-CN.md
          git commit -m "docs: auto-translate README to Chinese [skip ci]"
          git push
```

- [ ] **Step 2: 本地验证 YAML 语法**

```powershell
python -c "import yaml; yaml.safe_load(open('.github/workflows/translate-readme.yml', encoding='utf-8')); print('YAML OK')"
```

Expected: `YAML OK`

- [ ] **Step 3: 提交并推送**

```powershell
git add .github/workflows/translate-readme.yml
git commit -m "ci: add README auto-translate workflow"
git push
```

---

### Post-Implementation

- [ ] 用户在 GitHub Settings → Secrets → Actions 添加 `DEEPSEEK_API_KEY`
- [ ] 手动触发一次 `workflow_dispatch` 生成初始 `docs/README_zh-CN.md`
- [ ] 以后每次修改 `README.md` 并 push 到 master 即自动翻译
