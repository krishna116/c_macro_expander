#pragma once

#include <map>
#include <sstream>

#include "token.h"

struct Macro {
  std::string name;
  bool isFunctionLike = false;
  std::vector<std::string> params;
  std::vector<Token> replacement;
};

// =====================================================================
//                  宏展开器（核心：基于栈的非递归算法）
// =====================================================================

class MacroExpander {
public:
  explicit MacroExpander(const std::map<std::string, Macro> &table)
      : table_(table) {}

  /*
   * 主入口：对输入标记序列执行宏展开
   *
   * 算法核心思想：
   *   使用一个工作栈（workStack）模拟递归展开过程：
   *   1. 将输入标记逆序压入工作栈（使第一个标记位于栈顶）
   *   2. 循环弹出栈顶标记 cur：
   *      a. 若 cur 是宏名且未被扩展（宏名 ∉ cur.hideSet）：
   *         - 对象式宏：处理替换体，更新隐藏集，将替换体逆序压回栈
   *         - 函数式宏：向前查看 '('，收集实参，处理替换体，逆序压回栈
   *         - 扩展：将宏名加入替换体所有标记的隐藏集
   *      b. 否则（非宏 / 已被扩展）：直接输出到结果序列
   *   3. 栈空时，结果序列即为最终展开结果
   *
   *   逆序压回 + 从栈顶弹出 = 自然实现"替换后重新扫描"
   *   隐藏集 = 自然实现"扩展规则"防止无限递归
   */
  std::vector<Token> expand(const std::vector<Token> &input) {
    std::vector<Token> workStack; // 工作栈（vector 模拟，栈顶 = back()）
    std::vector<Token> output;    // 输出序列

    // 步骤1：将输入序列逆序压入工作栈(从尾部逆向推理，而不是从前向后推理)
    for (int i = (int)input.size() - 1; i >= 0; i--)
      workStack.push_back(input[i]);

    // 步骤2：主循环——不断弹出栈顶标记处理
    while (!workStack.empty()) {
      Token cur = workStack.back();
      workStack.pop_back();

      // 判断：是否为可展开的宏标识符
      bool isMacro = (table_.find(cur.text) != table_.end());
      bool bluePainted = cur.hideSet.count(cur.text) > 0;

      if (cur.isIdentifier() && isMacro && !bluePainted) {
        const Macro &macro = table_.at(cur.text);

        if (!macro.isFunctionLike) {
          // ---- 对象式宏 ----
          std::vector<Token> rep = processReplacement(macro, {});
          // 扩展：newHide = cur.hideSet ∪ {宏名}
          std::set<std::string> newHide = cur.hideSet;
          newHide.insert(cur.text);
          applyHideSet(rep, newHide);
          // 逆序压回栈（替换体的第一个标记在栈顶 → 下次先被弹出处理）
          pushReversed(workStack, rep);
        } else {
          // ---- 函数式宏 ----
          // 向前查看是否紧跟 '('（不弹出，仅 peek）
          if (!workStack.empty() && workStack.back().text == "(") {
            workStack.pop_back(); // 消费 '('
            // 收集实参（从栈中弹出直到匹配的 ')'）
            std::vector<std::vector<Token>> args =
                collectArgs(workStack, macro);
            // 处理替换体（字符串化 / 粘贴 / 参数替换）
            std::vector<Token> rep = processReplacement(macro, args);
            // 扩展
            std::set<std::string> newHide = cur.hideSet;
            newHide.insert(cur.text);
            applyHideSet(rep, newHide);
            // 逆序压回栈
            pushReversed(workStack, rep);
          } else {
            // 后面没有 '(' → 不是宏调用，作为普通标识符输出
            output.push_back(cur);
          }
        }
      } else {
        // 非宏 / 已被扩展 → 直接输出
        output.push_back(cur);
      }
    }
    return output;
  }

private:
  const std::map<std::string, Macro> &table_;

  // ---- 收集函数式宏的实参 ----
  // 从工作栈中弹出标记，以逗号分隔实参，直到匹配的 ')'
  std::vector<std::vector<Token>> collectArgs(std::vector<Token> &stack,
                                              const Macro &macro) {
    std::vector<std::vector<Token>> args;
    std::vector<Token> current;
    int depth = 0; // 括号嵌套深度

    while (!stack.empty()) {
      Token t = stack.back();
      stack.pop_back();

      if (t.text == "(") {
        depth++;
        current.push_back(t);
      } else if (t.text == ")") {
        if (depth == 0) {
          // 实参列表结束
          if (!(macro.params.empty() && current.empty()))
            args.push_back(current);
          break;
        } else {
          depth--;
          current.push_back(t);
        }
      } else if (t.text == "," && depth == 0) {
        // 顶层逗号：分隔实参
        args.push_back(current);
        current.clear();
      } else {
        current.push_back(t);
      }
    }
    // 实参数不足时补空
    while (args.size() < macro.params.size())
      args.push_back({});
    return args;
  }

  // ---- 处理替换体 ----
  // 依次处理 # 字符串化、## 标记粘贴、参数替换
  std::vector<Token>
  processReplacement(const Macro &macro,
                     const std::vector<std::vector<Token>> &args) {
    std::vector<Token> result;
    const auto &rep = macro.replacement;

    // 查找形参索引
    auto paramIndex = [&](const std::string &name) -> int {
      for (size_t k = 0; k < macro.params.size(); k++)
        if (macro.params[k] == name)
          return (int)k;
      return -1;
    };

    for (size_t i = 0; i < rep.size(); i++) {
      const Token &t = rep[i];

      // --- # 字符串化：# 后跟形参名 ---
      if (t.text == "#" && i + 1 < rep.size()) {
        int idx = paramIndex(rep[i + 1].text);
        if (idx >= 0) {
          const auto &arg =
              (idx < (int)args.size()) ? args[idx] : std::vector<Token>{};
          result.push_back(Token(stringize(arg)));
          i++; // 跳过形参名
          continue;
        }
      }

      // --- ## 标记粘贴 ---
      if (t.text == "##" && i + 1 < rep.size() && !result.empty()) {
        Token left = result.back();
        result.pop_back();
        const Token &rightTok = rep[i + 1];
        int ridx = paramIndex(rightTok.text);

        if (ridx >= 0) {
          // 右操作数是形参：使用原始（未展开）实参
          if (ridx < (int)args.size() && !args[ridx].empty()) {
            // 粘贴左操作数与实参第一个标记，其余标记追加
            result.push_back(paste(left, args[ridx][0]));
            for (size_t k = 1; k < args[ridx].size(); k++)
              result.push_back(args[ridx][k]);
          } else {
            // 空实参 → 占位，放回左操作数
            result.push_back(left);
          }
        } else {
          // 右操作数是普通标记
          result.push_back(paste(left, rightTok));
        }
        i++; // 跳过右操作数
        continue;
      }

      // --- 参数替换 ---
      int idx = paramIndex(t.text);
      if (idx >= 0) {
        // 将原始实参标记压入结果
        // （主循环重新扫描时会自动展开其中的宏，等效于标准中的 prescan）
        if (idx < (int)args.size()) {
          for (const auto &at : args[idx])
            result.push_back(at);
        }
      } else {
        // 普通标记，直接复制
        result.push_back(t);
      }
    }
    return result;
  }

  // 将隐藏集合并到标记序列中每个标记
  static void applyHideSet(std::vector<Token> &tokens,
                           const std::set<std::string> &hs) {
    for (auto &t : tokens)
      t.hideSet.insert(hs.begin(), hs.end());
  }

  // 逆序压栈（使 tokens[0] 位于栈顶）
  static void pushReversed(std::vector<Token> &stack,
                           const std::vector<Token> &tokens) {
    for (int i = (int)tokens.size() - 1; i >= 0; i--)
      stack.push_back(tokens[i]);
  }
};
