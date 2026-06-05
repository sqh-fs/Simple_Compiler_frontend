#ifndef LEXER_H
#define LEXER_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <QString>
#include <QVector>
#include <QDebug>

// 记号类型枚举
enum class TokenType {
    IDENTIFIER,     // 标识符
    INTEGER,        // 整数
    DECIMAL,        // 小数
    KEYWORD,        // 关键字
    OPERATOR,       // 操作符
    DELIMITER,      // 分隔符
    COMMENT,        // 注释
    AND,            // 与
    OR,             // 或
    INT,            // int
    FLOAT,          // float
    DOUBLE,         // double
    CHAR,           // char
    BOOL,           // bool
    TRUE,           // true
    FALSE,          // false
    IF,             // if
    ELSE,           // else
    WHILE,          // while
    UNKNOWN,        // 未知
    END_OF_INPUT    // 输入结束
};

// 记号结构
struct Token {
    TokenType type;
    QString value;
    int line;
    int column;

    Token(TokenType t = TokenType::UNKNOWN, const QString& v = "", int l = 1, int c = 1)
        : type(t), value(v), line(l), column(c) {}
};

// DFA状态
struct DFAState {
    int id;
    bool isAccepting;
    TokenType tokenType;
    QMap<QChar, int> transitions;  // 字符 -> 目标状态ID

    DFAState(int stateId = -1, bool accepting = false, TokenType type = TokenType::UNKNOWN)
        : id(stateId), isAccepting(accepting), tokenType(type) {}
};

// DFA最小化相关结构
struct StateGroup {
    QSet<int> states;    // DFA 状态
    TokenType tokenType; // 类型
    bool isAccepting;    // 接受状态

    StateGroup() : tokenType(TokenType::UNKNOWN), isAccepting(false) {}
};

class LexicalAnalyzer : public QObject
{
    Q_OBJECT

public:
    explicit LexicalAnalyzer(QObject *parent = nullptr);

    // 初始化DFA
    void initializeDFA();

    // 词法分析主函数
    QVector<Token> analyze(const QString& input);

    // DFA最小化
    void minimizeDFA();

    // 调试函数
    void printDFA() const;
    void printMinimizedDFA() const;

private:
    // DFA相关
    QMap<int, DFAState> states;           // 状态集合
    QMap<int, DFAState> minimizedStates;  // 最小化后的状态集合
    int startState;                       // 开始状态
    int currentState;                     // 当前状态
    QSet<QChar> alphabet;                 // 字母表
    QSet<QString> keywords;               // 关键字集合

    // 辅助函数
    void addState(int id, bool isAccepting = false, TokenType type = TokenType::UNKNOWN);
    void addTransition(int from, QChar input, int to);
    bool isKeyword(const QString& word) const;
    // bool isLetter(QChar c) const;
    // bool isDigit(QChar c) const;
    // bool isOperator(QChar c) const;
    bool isDelimiter(QChar ch) const;

    // DFA最小化相关函数
    QVector<StateGroup> partitionStates();
    QVector<StateGroup> refinePartition(const QVector<StateGroup>& partition);
    int findGroupId(int state, const QVector<StateGroup>& partition) const;
    void buildMinimizedDFA(const QVector<StateGroup>& finalPartition);

    // 词法分析辅助函数
    Token getNextToken(const QString& input, int& pos, int& line, int& column);
    void skipWhitespace(const QString& input, int& pos, int& line, int& column);
};

#endif // LEXER_H
