#include "parser.h"
#include <QQueue>

parser::parser() : currentToken(0) {
    initializeGrammar();       // 初始化语法规则
    LR0 = buildLR0ItemSets();  // 构建LR(0)的项集族 并 记录转移表
    // printItemSets(LR0);

    // 打印状态转移关系
    // for (const StateTransition& trans : transitions) {
    //     qDebug() << "State" << trans.fromState
    //              << "--" << trans.symbol << "--> State"
    //              << trans.toState;
    // }
    // qDebug() << "";

    kernels = extractKernelItems(LR0);  // 从LR(0)的项集族中提取kernel
    buildInitialKernels();  // 为增广文法添加向前看符号 $
    // qDebug () << "添加了 $ 的内核项";
    // printKernels(kernels);

    buildFirstSets();
    // printFirstSets();
    // qDebug() << "";

    // lookahead的闭包计算
    QVector<LRItem> temp = closureWithLookahead(kernels[0]);
    initializeKernelLookaheads(kernels, temp);  // 自动生成和传播 lookahead 的第一步

    initializeLookaheadWithTerminals();   // 查看有没有遗漏的lookahead

    // 调试闭包计算结果
    // for (const LRItem& item : temp) {
    //     const Production& prod = productions[item.productionId];
    //     QString rhs;
    //     for (int i = 0; i <= prod.right.size(); ++i) {
    //         if (i == item.dotPosition)
    //             rhs += "· ";
    //         if (i < prod.right.size())
    //             rhs += prod.right[i] + " ";
    //     }

    //     QString lookaheadStr = "{ ";
    //     for (const QString& la : item.lookahead)
    //         lookaheadStr += la + " ";
    //     lookaheadStr += "}";

    //     qDebug().noquote() << QString("%1 -> %2 , %3")
    //                               .arg(prod.left, -10)
    //                               .arg(rhs.trimmed(), -20)
    //                               .arg(lookaheadStr);
    // }
    // qDebug() << "";
    // qDebug() << "自动生成的向前看符号";
    // printKernels(kernels);

    // 传播向前看符号
    propagateLookaheads();
    // qDebug() << "内核项传播后的结果: ";
    // printKernels(kernels);

    // 构建完整的LALR(1)项集族
    LALR1 = buildCompleteLALR1ItemSets();
    // qDebug() << "完整的LALR1";
    // printKernels(LALR1);

    propagateLookaheadUntilStable(LALR1);

    // printKernels(LALR1);

    // 构建ACTION表和GOTO表
    buildTables();
    // printTables();
}

parser::~parser() {}

// 初始化文法
void parser::initializeGrammar() {
    startSymbol = "Program";

    // Production definitions
    productions = {
        // Program → StatementList
        Production("Program", {"StatementList"}, 0),

        // StatementList → Statement StatementList | ε
        Production("StatementList", {"Statement", "StatementList"}, 1),
        Production("StatementList", {"ε"}, 2),

        // Statement → Declaration | Assignment | IfStatement | WhileStatement
        Production("Statement", {"Declaration"}, 3),
        Production("Statement", {"Assignment"}, 4),
        Production("Statement", {"IfStatement"}, 5),
        Production("Statement", {"WhileStatement"}, 6),

        // Declaration → Type IDENTIFIER ArrayDecl InitPart;
        Production("Declaration", {"Type", "IDENTIFIER", "ArrayDecl", "InitPart", ";"}, 7),

        // ArrayDecl → [ NUMBER ] ArrayDecl | ε
        Production("ArrayDecl", {"[", "NUMBER", "]", "ArrayDecl"}, 47),
        Production("ArrayDecl", {"ε"}, 48),

        // InitPart → = Initial | ε
        Production("InitPart", {"=", "Initial"}, 8),
        Production("InitPart", {"ε"}, 9),

        // Initial -> Expression | { NUMBER NumberList}
        Production("Initial", {"Expression"}, 49),
        Production("Initial", {"{", "NUMBER", "NumberList", "}"}, 53),

        // NumberList → , NUMBER NumberList | ε
        Production("NumberList", {",", "NUMBER", "NumberList"}, 50),
        Production("NumberList", {"ε"},52),

        // Assignment → IDENTIFIER = Expression ;
        Production("Assignment", {"IDENTIFIER", "=", "Expression", ";"}, 10),

        // Type → INT | FLOAT | DOUBLE | BOOL
        Production("Type", {"INT"}, 11),
        Production("Type", {"FLOAT"}, 12),
        Production("Type", {"DOUBLE"}, 13),
        Production("Type", {"BOOL"}, 14),

        // IfStatement → if ( BoolExpression ) { StatementList } ElsePart
        Production("IfStatement", {"if", "(", "BoolExpression", ")", "{", "StatementList", "}", "ElsePart"}, 15),

        // ElsePart → else { StatementList }| ε
        Production("ElsePart", {"else", "{", "StatementList", "}"}, 16),
        Production("ElsePart", {"ε"}, 17),

        // WhileStatement → while ( BoolExpression )  { StatementList }
        Production("WhileStatement", {"while", "(", "BoolExpression", ")", "{", "StatementList", "}"}, 18),

        // Expression → Term ExpressionTail
        Production("Expression", {"Term", "ExpressionTail"}, 19),

        // ExpressionTail → + Term ExpressionTail | - Term ExpressionTail | ε
        Production("ExpressionTail", {"+", "Term", "ExpressionTail"}, 20),
        Production("ExpressionTail", {"-", "Term", "ExpressionTail"}, 21),
        Production("ExpressionTail", {"ε"}, 22),

        // Term → Factor TermTail
        Production("Term", {"Factor", "TermTail"}, 23),

        // TermTail → * Factor TermTail | / Factor TermTail | ε
        Production("TermTail", {"*", "Factor", "TermTail"}, 24),
        Production("TermTail", {"/", "Factor", "TermTail"}, 25),
        Production("TermTail", {"ε"}, 26),

        // Factor → ( Expression ) | IDENTIFIER | NUMBER | BOOL_LITERAL
        Production("Factor", {"(", "Expression", ")"}, 27),
        Production("Factor", {"IDENTIFIER"}, 28),
        Production("Factor", {"NUMBER"}, 29),
        Production("Factor", {"BOOL_LITERAL"}, 30),

        // BoolExpression → BoolTerm BoolExprTail
        Production("BoolExpression", {"BoolTerm", "BoolExprTail"}, 31),

        // BoolExprTail → || BoolTerm BoolExprTail | ε
        Production("BoolExprTail", {"||", "BoolTerm", "BoolExprTail"}, 32),
        Production("BoolExprTail", {"ε"}, 33),

        // BoolTerm → BoolFactor BoolTermTail
        Production("BoolTerm", {"BoolFactor", "BoolTermTail"}, 34),

        // BoolTermTail → && BoolFactor BoolTermTail | ε
        Production("BoolTermTail", {"&&", "BoolFactor", "BoolTermTail"}, 35),
        Production("BoolTermTail", {"ε"}, 36),

        // BoolFactor → ! BoolFactor | Relation
        Production("BoolFactor", {"!", "BoolFactor"}, 37),
        Production("BoolFactor", {"Relation"}, 38),

        // Relation → Expression RelOp Expression | ( BoolExpression )
        Production("Relation", {"Expression", "RelOp", "Expression"}, 39),
        Production("Relation", {"(", "BoolExpression", ")"}, 40),

        // RelOp → > | < | >= | <= | == | !=
        Production("RelOp", {">"}, 41),
        Production("RelOp", {"<"}, 42),
        Production("RelOp", {">="}, 43),
        Production("RelOp", {"<="}, 44),
        Production("RelOp", {"=="}, 45),
        Production("RelOp", {"!="}, 46),
    };

    // 终结符
    terminals = {
        // Types
        "INT", "FLOAT", "DOUBLE", "BOOL",
        // Keywords
        "if", "else", "while",
        // Identifiers and literals
        "IDENTIFIER", "NUMBER", "BOOL_LITERAL",
        // Operators
        "+", "-", "*", "/", "=", "!", "||", "&&",
        // Relational operators
        ">", "<", ">=", "<=", "==", "!=",
        // Delimiters
        "(", ")", ";", "{", "}", "[", "]", ",",
        // End marker and epsilon
        "$", "ε"
    };

    // 非终结符
    nonterminals = {
        "Program", "StatementList", "Statement",
        "Declaration", "Assignment", "IfStatement", "WhileStatement",
        "Type", "Expression", "ExpressionTail", "Term", "TermTail", "Factor",
        "BoolExpression", "BoolExprTail", "BoolTerm", "BoolTermTail",
        "BoolFactor", "Relation", "RelOp", "ElsePart", "InitPart",
        "ArrayDecl", "NumberList", "Initial"
    };
}


QString parser::tokenToSymbol(const Token& token) {
    // 将Token类型转换为文法符号
    switch (token.type) {
    case TokenType::IDENTIFIER: return "IDENTIFIER";
    case TokenType::INTEGER: return "NUMBER";
    case TokenType::DECIMAL: return "NUMBER";
    case TokenType::INT: return "INT";
    case TokenType::FLOAT: return "FLOAT";
    case TokenType::DOUBLE: return "DOUBLE";
    case TokenType::CHAR: return "CHAR";
    case TokenType::BOOL: return "BOOL";
    case TokenType::TRUE: return "BOOL_LITERAL";
    case TokenType::FALSE: return "BOOL_LITERAL";
    case TokenType::IF: return "if";
    case TokenType::ELSE: return "else";
    case TokenType::WHILE: return "while";
    case TokenType::AND: return "&&";
    case TokenType::OR: return "||";
    case TokenType::OPERATOR:
        if (token.value == "+") return "+";
        if (token.value == "-") return "-";
        if (token.value == "*") return "*";
        if (token.value == "/") return "/";
        if (token.value == "=") return "=";
        if (token.value == "!") return "!";
        if (token.value == "==") return "==";
        if (token.value == "!=") return "!=";
        if (token.value == "<") return "<";
        if (token.value == ">") return ">";
        if (token.value == "<=") return "<=";
        if (token.value == ">=") return ">=";
        break;
    case TokenType::DELIMITER:
        if (token.value == "(") return "(";
        if (token.value == ")") return ")";
        if (token.value == "{") return "{";
        if (token.value == "}") return "}";
        if (token.value == ";") return ";";
        if (token.value == ",") return ",";
        if (token.value == "[") return "[";
        if (token.value == "]") return "]";
        break;
    case TokenType::END_OF_INPUT: return "$";
    default: break;
    }
    return "UNKNOWN";
}

// 判断是否为终结符
bool parser::isTerminal(const QString& symbol) {
    return terminals.contains(symbol);
}

// LR(0)的闭包函数
QVector<LRItem> parser::closure(const QVector<LRItem>& items) {
    QVector<LRItem> closureItems = items;
    QSet<LRItem> processedItems;   // 存储已经处理的项

    QQueue<LRItem> queue;
    for (const LRItem& item : items) {
        queue.enqueue(item);
        processedItems.insert(item);
    }

    // 广度遍历
    while (!queue.isEmpty()) {
        LRItem current = queue.dequeue();
        const Production& prod = productions[current.productionId];

        // 若点的位置在产生式末尾，无法再展开，跳过
        if (current.dotPosition >= prod.right.size()) {
            continue;
        }

        // 点后的符号
        QString nextSymbol = prod.right[current.dotPosition];

        // 是非终结符，需要将它的所有产生式加入闭包
        if (nonterminals.contains(nextSymbol)) {
            for (int i = 0; i < productions.size(); ++i) {
                if (productions[i].left == nextSymbol) {
                    // 构造一个新的项，点在开头
                    LRItem newItem(i, 0);

                    // 若之前未处理，加入闭包并排入队列
                    if (!processedItems.contains(newItem)) {
                        closureItems.append(newItem);
                        queue.enqueue(newItem);
                        processedItems.insert(newItem);
                    }
                }
            }
        }
    }

    return closureItems;
}

// LR(0)的GOTO函数
QVector<LRItem> parser::gotoFunction(const QVector<LRItem>& items, const QString& symbol) {
    QVector<LRItem> gotoItems;

    for (const LRItem& item : items) {
        const Production& prod = productions[item.productionId];

        // 检查点后面的符号是否是我们要转移的符号
        if (item.dotPosition < prod.right.size() && prod.right[item.dotPosition] == symbol) {
            // 创建点位置 +1 的新项目
            gotoItems.append(LRItem(item.productionId, item.dotPosition + 1));
        }
    }

    // 返回新项目的闭包
    return closure(gotoItems);
}

// 在初始化函数中添加增广文法
void parser::augmentGrammar() {
    // 创建新的开始符号 S'（确保它不会与现有符号冲突）
    QString newStart = startSymbol + "'";
    while (nonterminals.contains(newStart)) {
        newStart += "'";
    }

    // 添加产生式 S' -> S
    QVector<QString> right = {startSymbol};
    productions.prepend(Production(newStart, right, 0));

    // 更新其他产生式的ID（因为我们在前面插入了一个产生式）
    for (int i = 1; i < productions.size(); ++i) {
        productions[i].id = i;
    }

    // 更新符号集合
    nonterminals.insert(newStart);
    startSymbol = newStart;
}

// 构建LR(0)项集族
QVector<QVector<LRItem>> parser::buildLR0ItemSets() {
    transitions.clear();

    // 添加增广文法
    augmentGrammar();

    QVector<QVector<LRItem>> itemSets;
    QMap<QVector<LRItem>, int> itemSetToState;

    // 初始项目集：S' -> ·S
    QVector<LRItem> initialItems;
    int startProdIndex = -1;

    // 找到开始符号对应的产生式
    for (int i = 0; i < productions.size(); ++i) {
        if (productions[i].left == startSymbol) {
            startProdIndex = i;
            break;
        }
    }

    if (startProdIndex == -1) {
        qWarning() << "Start symbol production not found!";
        return itemSets;
    }

    // 将初始项目集加入，并计算闭包
    initialItems.append(LRItem(startProdIndex, 0));
    QVector<LRItem> initialClosure = closure(initialItems);

    itemSets.append(initialClosure);
    itemSetToState[initialClosure] = 0;

    QQueue<int> unprocessedStates;
    unprocessedStates.enqueue(0);

    // 广度遍历
    while (!unprocessedStates.isEmpty()) {
        int currentState = unprocessedStates.dequeue();

        // 收集所有可能的转移符号
        QSet<QString> symbols;
        for (const LRItem& item : itemSets[currentState]) {
            const Production& prod = productions[item.productionId];
            if (item.dotPosition < prod.right.size()) {
                symbols.insert(prod.right[item.dotPosition]);
            }
        }

        // 对每个符号计算GOTO
        for (const QString& symbol : symbols) {
            QVector<LRItem> newItemSet = gotoFunction(itemSets[currentState], symbol);

            if (newItemSet.isEmpty()) continue;

            // 检查是否是新项目集
            int targetState;
            if (!itemSetToState.contains(newItemSet)) {
                targetState = itemSets.size();
                itemSets.append(newItemSet);
                itemSetToState[newItemSet] = targetState;
                unprocessedStates.enqueue(targetState);
            } else {
                targetState = itemSetToState[newItemSet];
            }

            // 直接记录到 transitions
            transitions.append({currentState, symbol, targetState});
        }
    }

    return itemSets;
}

// 打印LR(0)
void parser::printItemSets(const QVector<QVector<LRItem>>& itemSets) {
    qDebug() << "LR(0) Item Sets:";
    for (int i = 0; i < itemSets.size(); ++i) {
        qDebug() << "State" << i << ":";
        for (const LRItem& item : itemSets[i]) {
            const Production& prod = productions[item.productionId];
            QString itemStr = prod.left + " -> ";
            for (int j = 0; j < prod.right.size(); ++j) {
                if (j == item.dotPosition) {
                    itemStr += "· ";
                }
                itemStr += prod.right[j] + " ";
            }
            if (item.dotPosition == prod.right.size()) {
                itemStr += "·";
            }
            qDebug() << "  " << itemStr;
        }
        qDebug() << "";
    }
}

// 判断一个项目是否属于内核项
bool parser::isKernelItem(const LRItem& item) {
    // 初始项目 S' -> ·S 总是内核项
    if (item.productionId == 0 && item.dotPosition == 0) {
        return true;
    }
    // 点不在最左端的项目是内核项
    return item.dotPosition > 0;
}

// 从LR(0)项集族中提取所有内核项
QVector<QVector<LRItem>> parser::extractKernelItems(const QVector<QVector<LRItem>>& LR0) {
    QVector<QVector<LRItem>> kernels;

    for (const QVector<LRItem>& state : LR0) {
        QVector<LRItem> kernel;

        for (const LRItem& item : state) {
            if (isKernelItem(item)) {
                kernel.append(item);
            }
        }

        kernels.append(kernel);
    }

    return kernels;
}
