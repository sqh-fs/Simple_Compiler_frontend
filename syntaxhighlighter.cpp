#include "syntaxhighlighter.h"
#include <QMutexLocker>

syntaxhighlighter::syntaxhighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent)
{
    initializeFormats();
}

// 高亮规则
void syntaxhighlighter::initializeFormats()
{
    // 初始化各种token类型的格式
    // 关键字：深蓝色 + 加粗
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(Qt::darkBlue);
    keywordFormat.setFontWeight(QFont::Bold);

    m_formats[TokenType::KEYWORD] = keywordFormat;
    m_formats[TokenType::INT] = keywordFormat;
    m_formats[TokenType::DOUBLE] = keywordFormat;
    m_formats[TokenType::FLOAT] = keywordFormat;
    m_formats[TokenType::BOOL] = keywordFormat;
    m_formats[TokenType::IF] = keywordFormat;
    m_formats[TokenType::ELSE] = keywordFormat;
    m_formats[TokenType::WHILE] = keywordFormat;
    m_formats[TokenType::TRUE] = keywordFormat;
    m_formats[TokenType::FALSE] = keywordFormat;
    m_formats[TokenType::AND] = keywordFormat;
    m_formats[TokenType::OR] = keywordFormat;

    // 标识符：暗紫色 + 加粗
    QTextCharFormat typeFormat;
    typeFormat.setForeground(Qt::darkMagenta);
    typeFormat.setFontWeight(QFont::Bold);
    m_formats[TokenType::IDENTIFIER] = typeFormat;

    // 数字：深绿色 + 加粗
    QTextCharFormat numberFormat;
    numberFormat.setForeground(Qt::darkGreen);
    m_formats[TokenType::INTEGER] = numberFormat;
    m_formats[TokenType::DECIMAL] = numberFormat;

    // 运算符：红色 + 加粗
    QTextCharFormat operatorFormat;
    operatorFormat.setForeground(Qt::red);
    operatorFormat.setFontWeight(QFont::Bold);
    m_formats[TokenType::OPERATOR] = operatorFormat;

    // 分隔符：深灰色
    QTextCharFormat delimiterFormat;
    delimiterFormat.setForeground(Qt::darkGray);
    m_formats[TokenType::DELIMITER] = delimiterFormat;

    // 注释（可以不要）
    QTextCharFormat commentFormat;
    commentFormat.setForeground(Qt::gray);
    commentFormat.setFontItalic(true);
    m_formats[TokenType::COMMENT] = commentFormat;
}

// 更新文本框的高亮
void syntaxhighlighter::updateTokens(const QVector<Token> &tokens)
{
    QMutexLocker locker(&m_mutex);
    m_tokens = tokens;
    QMetaObject::invokeMethod(this, "rehighlight", Qt::QueuedConnection);
}

// 在文本中设置高亮
void syntaxhighlighter::highlightBlock(const QString &text)
{
    if (m_tokens.isEmpty()) {
        return;
    }

    // 获取当前块的行号
    const int currentLine = currentBlock().blockNumber() + 1;

    // 筛选当前行的token
    QVector<Token> lineTokens;
    for (const Token &token : m_tokens) {
        if (token.line == currentLine) {  // QTextBlock从0开始，Token行号从1开始
            lineTokens.append(token);
        }
    }

    // 应用高亮
    for (const Token &token : lineTokens) {
        if (m_formats.contains(token.type)) {
            // 计算token在行中的位置
            int start = token.column - 1;  // 列号从1开始
            int length = token.value.length();

            // 确保位置在文本范围内
            if (start >= 0 && start + length <= text.length()) {
                setFormat(start, length, m_formats[token.type]);
            }
        }
    }
}

void syntaxhighlighter::clear(){
    updateTokens(QVector<Token>());
}
