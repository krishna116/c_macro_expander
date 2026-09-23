#pragma once

#include <cctype>
#include <string>
#include <vector>
#include <set>

/*
 * Token —— 词法单元
 *
 * 每个标记携带一个 hideSet（隐藏集），用于实现 C 标准中的"扩展"规则：
 * 当宏 M 被展开时，其替换体中的所有标记都会将 M 加入各自的隐藏集。
 * 在后续重新扫描时，若发现某标记的隐藏集中包含其对应的宏名，则不再展开，
 * 从而避免无限递归。
 */
struct Token {
  std::string text;              // 标记文本
  std::set<std::string> hideSet; // 隐藏集：记录哪些宏名不应被再次展开

  Token() = default;
  explicit Token(std::string t) : text(std::move(t)) {}

  bool isIdentifier() const {
    if (text.empty())
      return false;
    if (!std::isalpha((unsigned char)text[0]) && text[0] != '_')
      return false;
    for (char c : text)
      if (!std::isalnum((unsigned char)c) && c != '_')
        return false;
    return true;
  }
};

std::vector<Token> tokenize(const std::string &src) {
  std::vector<Token> tokens;
  size_t i = 0, n = src.size();
  while (i < n) {
    char c = src[i];

    // 跳过空白
    if (std::isspace((unsigned char)c)) {
      i++;
      continue;
    }

    // 行注释 //
    if (c == '/' && i + 1 < n && src[i + 1] == '/') {
      i += 2;
      while (i < n && src[i] != '\n')
        i++;
      continue;
    }
    // 块注释 /* ... */
    if (c == '/' && i + 1 < n && src[i + 1] == '*') {
      i += 2;
      while (i < n && !(src[i] == '*' && i + 1 < n && src[i + 1] == '/'))
        i++;
      if (i < n)
        i += 2;
      continue;
    }
    // 标识符
    if (std::isalpha((unsigned char)c) || c == '_') {
      std::string s;
      while (i < n && (std::isalnum((unsigned char)src[i]) || src[i] == '_'))
        s += src[i++];
      tokens.push_back(Token(s));
      continue;
    }
    // 数字
    if (std::isdigit((unsigned char)c)) {
      std::string s;
      while (i < n && (std::isalnum((unsigned char)src[i]) || src[i] == '.'))
        s += src[i++];
      tokens.push_back(Token(s));
      continue;
    }
    // 字符串字面量
    if (c == '"') {
      std::string s = "\"";
      i++;
      while (i < n && src[i] != '"') {
        if (src[i] == '\\' && i + 1 < n)
          s += src[i++];
        s += src[i++];
      }
      if (i < n)
        s += src[i++];
      tokens.push_back(Token(s));
      continue;
    }
    // 字符字面量
    if (c == '\'') {
      std::string s = "'";
      i++;
      while (i < n && src[i] != '\'') {
        if (src[i] == '\\' && i + 1 < n)
          s += src[i++];
        s += src[i++];
      }
      if (i < n)
        s += src[i++];
      tokens.push_back(Token(s));
      continue;
    }
    // # 或 ##
    if (c == '#') {
      std::string s = "#";
      i++;
      if (i < n && src[i] == '#') {
        s += src[i++];
      }
      tokens.push_back(Token(s));
      continue;
    }
    // 其他单字符标点
    tokens.push_back(Token(std::string(1, c)));
    i++;
  }
  return tokens;
}

// =====================================================================
//                         辅助函数
// =====================================================================

// 字符串化：将参数标记序列转为字符串字面量
std::string stringize(const std::vector<Token> &tokens) {
  std::string result = "\"";
  for (size_t i = 0; i < tokens.size(); i++) {
    if (i > 0)
      result += " ";
    for (char c : tokens[i].text) {
      if (c == '\\' || c == '"')
        result += '\\';
      result += c;
    }
  }
  result += "\"";
  return result;
}

// 标记粘贴：合并两个标记的文本与隐藏集
Token paste(const Token &a, const Token &b) {
  Token result;
  result.text = a.text + b.text;
  result.hideSet = a.hideSet;
  for (const auto &h : b.hideSet)
    result.hideSet.insert(h);
  return result;
}

// 将标记序列格式化为字符串（空格分隔）
std::string tokensToString(const std::vector<Token> &tokens) {
  std::string result;
  for (size_t i = 0; i < tokens.size(); i++) {
    if (i > 0)
      result += " ";
    result += tokens[i].text;
  }
  return result;
}

