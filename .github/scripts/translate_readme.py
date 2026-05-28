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
