#ifndef PARSER_H
#define PARSER_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStack>
#include <QDebug>
#include "lexer.h"

// 产生式规则
struct Production {
    QString left;           // 左部非终结符
    QVector<QString> right; // 右部符号序列
    int id;                 // 产生式编号

    Production(const QString& l, const QVector<QString>& r, int i = 0)
        : left(l), right(r), id(i) {}

    QString toString() const {
        QString result = left + " -> ";
        for (const QString &s : right) {
            result += s + " ";
        }
        return result.trimmed();
    }
};

// LR项目
struct LRItem {
    int productionId;       // 产生式编号
    int dotPosition;        // 点的位置
    QSet<QString> lookahead; // 向前看符号集合

    LRItem(int prodId = 0, int dotPos = 0)
        : productionId(prodId), dotPosition(dotPos) {}

    bool operator==(const LRItem& other) const {
        return productionId == other.productionId &&
               dotPosition == other.dotPosition &&
               lookahead == other.lookahead;
    }

    bool operator<(const LRItem& other) const {
        if (productionId != other.productionId)
            return productionId < other.productionId;
        if (dotPosition != other.dotPosition)
            return dotPosition < other.dotPosition;
        return lookahead.values() < other.lookahead.values();
    }

    QString toString() const {
        return QString("%1-%2").arg(productionId).arg(dotPosition);
    }
};

inline size_t qHash(const LRItem &key, size_t seed = 0) {
    size_t h = qHash(key.productionId, seed) ^ qHash(key.dotPosition, seed);
    for (const QString &s : key.lookahead) {
        h ^= qHash(s, seed);
    }
    return h;
}


// 状态转移结构体
struct StateTransition {
    int fromState;
    QString symbol;
    int toState;
};


// 动作类型
enum class ActionType {
    SHIFT,    // 移入
    REDUCE,   // 规约
    ACCEPT,   // 接受
    ERROR     // 错误
};

// 分析动作
struct Action {
    ActionType type;
    int target;  // 状态编号或产生式编号

    Action(ActionType t = ActionType::ERROR, int tgt = -1)
        : type(t), target(tgt) {}
};

// 添加用于存储分析步骤的结构
struct ParseStep {
    int stepNumber;    // 步骤编号
    QString stateStack;  // 状态栈的字符串表示
    QString symbolStack;  // 符号栈的字符串表示
    QString remainingInput;  // 剩余未处理的输入符号串
    QString action;  // 动作
    bool isError;  // 是否为错误步骤
    QString errorMessage;  // 错误信息描述

    ParseStep() : stepNumber(0), isError(false) {}

    ParseStep(int step, const QString& states, const QString& symbols,
              const QString& input, const QString& act, bool error = false,
              const QString& errMsg = "")
        : stepNumber(step), stateStack(states), symbolStack(symbols),
        remainingInput(input), action(act), isError(error), errorMessage(errMsg) {}
};

// 分析结果结构
struct ParseResult {
    bool success;  // 语法分析是否成功
    QVector<ParseStep> steps;  // 步骤数组
    QString errorMessage;  // 错误信息描述
    int errorLine;  // 错误行号
    int errorColumn;  // 错误列号

    ParseResult() : success(false), errorLine(-1), errorColumn(-1) {}
};



class parser : public QObject
{
    Q_OBJECT

public:
    parser();
    ~parser();

    // 初始化语法规则
    void initializeGrammar();

    // 主要接口
    ParseResult parse(const QVector<Token>& inputTokens);    // 语法分析

    // 错误处理
    void reportError(const QString& message);

    QVector<Token> tokens;                 // token流
    int currentToken;                      // 当前token位置


private:
    QVector<Production> productions;        // 产生式集合

    QMap<QString, QSet<QString>> firstSets; // FIRST集合

    // 分析表
    QMap<QPair<int, QString>, Action> actionTable;  // ACTION表  ACTION[state][terminal]
    QMap<QPair<int, QString>, int> gotoTable;       // GOTO表  GOTO[state][nonterminal]

    // 符号集合
    QSet<QString> terminals;               // 终结符
    QSet<QString> nonterminals;            // 非终结符
    QString startSymbol;                   // 开始符号

    QVector<QVector<LRItem>> LR0;          // LR(0)项集族
    QVector<QVector<LRItem>> kernels;      // 内核项

    QVector<StateTransition> transitions;  // 传播表

    QVector<QVector<LRItem>> LALR1;        // LALR(1)项集族

    // 核心算法
    void buildFirstSets();                 // 构造FIRST集合
    void printFirstSets() const;                 // 打印 firstSets
    QVector<LRItem> closure(const QVector<LRItem>& items); // 闭包运算
    QVector<LRItem> gotoFunction(const QVector<LRItem>& items, const QString& symbol); // GOTO函数
    void augmentGrammar();   // 添加增广文法
    QVector<QVector<LRItem>> buildLR0ItemSets();  // 构建LR0项集族
    void printItemSets(const QVector<QVector<LRItem>>& itemSets);  // 打印LR0项集族，用于调试
    bool isKernelItem(const LRItem& item);  // 判断是否是内核项
    QVector<QVector<LRItem>> extractKernelItems(const QVector<QVector<LRItem>>& LR0);  // 从LR(0)中提取内核项
    void buildInitialKernels();       // 给 S'-> S 添加 $
    void initializeLookaheadWithTerminals();    // 给内核项添加点后的 Terminal
    void printKernels(const QVector<QVector<LRItem>>& kernels);  // 打印内核项，用于调试
    QSet<QString> computeFirst(const QVector<QString>& beta);    // 计算 FIRST(β) 集合（β 是符号串）
    QVector<LRItem> closureWithLookahead(const QVector<LRItem>& items);  // 计算向前看符号的闭包函数
    void initializeKernelLookaheads(QVector<QVector<LRItem>>& kernels, const QVector<LRItem>& temp);
    void propagateLookaheads();     // 传播向前看符号
    QVector<QVector<LRItem>> buildCompleteLALR1ItemSets();  // 构建完整的LALR(1)项集族

    void propagateLookaheadUntilStable(QVector<QVector<LRItem>>& LALR1);

    void buildTables();  // 构建ACTION表和GOTO表
    void printTables();  // 打印ACTION表和GOTO表


    // 辅助函数
    QString tokenToSymbol(const Token& token);    // Token转换为文法符号
    bool isTerminal(const QString& symbol);       // 判断是否为终结符

    void propagateWithinState(QVector<LRItem>& stateItems, bool& changed);
    void propagateAcrossStates(QVector<QVector<LRItem>>& LALR1,
                                       int fromState, int toState,
                                       const QString& symbol, bool& changed);

    // ParseResult parseWithSteps(const QVector<Token>& inputTokens);
};

#endif // PARSER_H
