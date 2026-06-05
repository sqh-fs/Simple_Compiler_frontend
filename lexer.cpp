#include "lexer.h"
#include <chrono>

using namespace std::chrono;

LexicalAnalyzer::LexicalAnalyzer(QObject *parent)
    : QObject(parent), startState(0), currentState(0)
{
    // 初始化关键字集合
    keywords = {"if", "else", "while", "for",
                "void", "return", "break", "continue", "struct", "class"};



    auto startLexer = high_resolution_clock::now();
    initializeDFA();
    auto endLexer = high_resolution_clock::now();
    auto lexerDuration = duration_cast<milliseconds>(endLexer - startLexer).count();
    qDebug() << "构建原始DFA耗时: " << lexerDuration << " ms\n";

    auto startLexer1 = high_resolution_clock::now();
    minimizeDFA();
    auto endLexer1 = high_resolution_clock::now();
    auto lexerDuration1 = duration_cast<milliseconds>(endLexer1 - startLexer1).count();
    qDebug() << "构建最小状态DFA耗时: " << lexerDuration1 << " ms\n";

    // qDebug() << "初始 DFA 状态数为: " << states.size();
    // qDebug() << "最小化后 DFA 状态数为: " << minimizedStates.size();
}

// 初始化DFA
void LexicalAnalyzer::initializeDFA()
{
    // 构建DFA状态机
    // 状态0: 开始状态
    addState(0);

    // 状态1-2: 标识符和关键字
    addState(1, true, TokenType::IDENTIFIER);

    // 状态3-4: 数字
    addState(3, true, TokenType::INTEGER);  // 整数
    addState(4, true, TokenType::DECIMAL);  // 小数

    // 状态5-10: 单目操作符
    addState(5, true, TokenType::OPERATOR);  // +
    addState(6, true, TokenType::OPERATOR);  // -
    addState(7, true, TokenType::OPERATOR);  // * = !
    addState(8, true, TokenType::OPERATOR);  // /
    addState(9, false, TokenType::OPERATOR);  // &
    addState(10, false, TokenType::OPERATOR); // |
    addState(21, true, TokenType::OPERATOR);  // <
    addState(22, true, TokenType::OPERATOR);  // >



    // 状态11: 分隔符
    addState(11, true, TokenType::DELIMITER);

    // 状态12-19: 双目操作符
    addState(12, true, TokenType::OPERATOR);  // ++
    addState(13, true, TokenType::OPERATOR);  // +=
    addState(14, true, TokenType::OPERATOR);  // --
    addState(15, true, TokenType::OPERATOR);  // -=
    addState(16, true, TokenType::OPERATOR);  // *= == !=
    addState(17, true, TokenType::OPERATOR);  // /=
    addState(18, true, TokenType::OPERATOR);  // &&
    addState(19, true, TokenType::OPERATOR);  // ||
    addState(23, true, TokenType::OPERATOR);  // <=
    addState(24, true, TokenType::OPERATOR);  // >=

    // 状态20: 单行注释
    addState(20, true, TokenType::COMMENT);

    // 构建转换表
    // 从状态0开始
    // 字母 -> 标识符状态1
    for (char c = 'a'; c <= 'z'; ++c) {
        addTransition(0, QChar(c), 1);  // 26个
        alphabet.insert(QChar(c));
    }
    for (char c = 'A'; c <= 'Z'; ++c) {
        addTransition(0, QChar(c), 1);  // 26个
        alphabet.insert(QChar(c));
    }
    addTransition(0, '_', 1);
    alphabet.insert('_');

    // 数字 -> 数字状态3
    for (char c = '0'; c <= '9'; ++c) {
        addTransition(0, QChar(c), 3);  // 10个
        alphabet.insert(QChar(c));
    }

    // 数字状态3的转换
    for (char c = '0'; c <= '9'; ++c) {
        addTransition(3, QChar(c), 3);
    }
    addTransition(3, '.', 4); // 小数点
    alphabet.insert('.');

    // 小数状态4
    for (char c = '0'; c <= '9'; ++c) {
        addTransition(4, QChar(c), 4);
    }



    // 状态0 -> 单目操作符
    addTransition(0, '+', 5);  // +
    alphabet.insert('+');
    addTransition(0, '-', 6);  // -
    alphabet.insert('-');
    QString tmp = "*=!";
    for(const QChar& op : tmp){
        addTransition(0, op,7);
        alphabet.insert(op);
    }
    addTransition(0, '/', 8);  // /
    alphabet.insert('/');
    addTransition(0, '&', 9);  // &
    alphabet.insert('&');
    addTransition(0, '|', 10);  // |
    alphabet.insert('|');
    addTransition(0, '<', 21);  // <
    alphabet.insert('<');
    addTransition(0, '>', 22);  // >



    // 分隔符
    QString delimiters = "(){}[];,";
    for (const QChar& delim : delimiters) {
        addTransition(0, delim, 6);   // 8个
        alphabet.insert(delim);
    }

    // 标识符状态1的自循环
    for (char c = 'a'; c <= 'z'; ++c) {
        addTransition(1, QChar(c), 1);  // 26个
    }
    for (char c = 'A'; c <= 'Z'; ++c) {
        addTransition(1, QChar(c), 1);  // 26个
    }
    for (char c = '0'; c <= '9'; ++c) {
        addTransition(1, QChar(c), 1);  // 10个
    }
    addTransition(1, '_', 1);

    // 单目 -> 双目的转换
    addTransition(5, '+', 12);
    addTransition(5, '=', 13);
    addTransition(6, '-', 14);
    addTransition(6, '=', 15);
    addTransition(7, '=', 16);
    addTransition(8, '=', 17);
    addTransition(9, '&', 18);
    addTransition(10, '|', 19);
    addTransition(21, '=', 23);
    addTransition(22, '=', 24);

    // / -> 注释的转换
    addTransition(8, '/', 20);
    // 状态20自循环：任意字符（除 \n 外）都保持在状态20
    for (ushort uc = 1; uc < 65535; ++uc) {
        QChar ch(uc);
        if (ch != '\n') {
            addTransition(20, ch, 20);
            alphabet.insert(ch);
        }
    }
}

// 添加状态  (状态编号 + 接收状态 + token类型)
void LexicalAnalyzer::addState(int id, bool isAccepting, TokenType type)
{
    states[id] = DFAState(id, isAccepting, type);
}

// 添加状态转移 （状态 -> 另一状态）
void LexicalAnalyzer::addTransition(int from, QChar input, int to)
{
    if (states.contains(from)) {
        states[from].transitions[input] = to;
    }
}

// 词法分析
QVector<Token> LexicalAnalyzer::analyze(const QString& input)
{
    // 存储token
    QVector<Token> tokens;

    // 初始化
    int pos = 0;
    int line = 1;
    int column = 1;

    // 遍历输入的源代码
    while (pos < input.length()) {
        // 跳过空白字符
        skipWhitespace(input, pos, line, column);

        if (pos >= input.length()) {
            break;
        }

        // 下一个token
        Token token = getNextToken(input, pos, line, column);
        if (token.type != TokenType::UNKNOWN || !token.value.isEmpty()) {
            tokens.append(token);
        }
    }

    // 最后加入 END_OF_INPUT
    tokens.append(Token(TokenType::END_OF_INPUT, "", line, column));
    return tokens;
}

// 获取下一个token
Token LexicalAnalyzer::getNextToken(const QString& input, int& pos, int& line, int& column)
{
    // 超出源代码的长度，标记结束
    if (pos >= input.length()) {
        return Token(TokenType::END_OF_INPUT, "", line, column);
    }

    // 当前行号和列号为起始
    int startPos = pos;
    int startColumn = column;

    // 分隔符
    QChar currentChar = input[pos];
    if (isDelimiter(currentChar)) {
        pos++;
        column++;
        return Token(TokenType::DELIMITER, QString(currentChar), line, startColumn);
    }


    // 初始化状态
    currentState = startState;
    QString tokenValue;
    TokenType lastAcceptingType = TokenType::UNKNOWN;
    int lastAcceptingPos = -1;

    // 使用最小化后的DFA
    const auto& dfaStates = minimizedStates.isEmpty() ? states : minimizedStates;

    // 状态机扫描循环，从当前位置开始尝试识别 token
    while (pos < input.length()) {
        QChar currentChar = input[pos];

        // 检查当前状态是否有对应字符的转换
        if (dfaStates.contains(currentState) &&
            dfaStates[currentState].transitions.contains(currentChar)) {

            // 根据 DFA 的转移函数更新当前状态
            currentState = dfaStates[currentState].transitions[currentChar];
            tokenValue += currentChar;
            pos++;

            column++;

            // 如果当前状态是接受状态，记录下来
            if (dfaStates.contains(currentState) && dfaStates[currentState].isAccepting) {
                lastAcceptingType = dfaStates[currentState].tokenType;
                lastAcceptingPos = pos;
            }
        } else {
            break;
        }
    }

    // 如果找到了接受状态
    if (lastAcceptingPos != -1) {
        // 回退到最后一个接受状态的位置
        int backtrack = pos - lastAcceptingPos;
        pos = lastAcceptingPos;  //
        column -= backtrack;
        tokenValue = tokenValue.left(tokenValue.length() - backtrack);

        // 检查是否为关键字
        if (lastAcceptingType == TokenType::IDENTIFIER && isKeyword(tokenValue)) {
            lastAcceptingType = TokenType::KEYWORD;
        }

        if (lastAcceptingType == TokenType::IDENTIFIER && tokenValue == "int") {
            lastAcceptingType = TokenType::INT;
        }

        if (lastAcceptingType == TokenType::IDENTIFIER && tokenValue == "char") {
            lastAcceptingType = TokenType::CHAR;
        }

        if (lastAcceptingType == TokenType::IDENTIFIER && tokenValue == "float") {
            lastAcceptingType = TokenType::FLOAT;
        }

        if (lastAcceptingType == TokenType::IDENTIFIER && tokenValue == "double") {
            lastAcceptingType = TokenType::DOUBLE;
        }

        if (lastAcceptingType == TokenType::IDENTIFIER && tokenValue == "bool") {
            lastAcceptingType = TokenType::BOOL;
        }

        if (lastAcceptingType == TokenType::IDENTIFIER && tokenValue == "true") {
            lastAcceptingType = TokenType::TRUE;
        }
        if (lastAcceptingType == TokenType::IDENTIFIER && tokenValue == "false") {
            lastAcceptingType = TokenType::FALSE;
        }

        if(lastAcceptingType == TokenType::OPERATOR && tokenValue == "&&") {
            lastAcceptingType = TokenType::AND;
        }

        if(lastAcceptingType == TokenType::OPERATOR && tokenValue == "||") {
            lastAcceptingType = TokenType::OR;
        }

        if(lastAcceptingType == TokenType::KEYWORD && tokenValue == "if"){
            lastAcceptingType = TokenType::IF;
        }

        if(lastAcceptingType == TokenType::KEYWORD && tokenValue == "else"){
            lastAcceptingType = TokenType::ELSE;
        }

        if(lastAcceptingType == TokenType::KEYWORD && tokenValue == "while"){
            lastAcceptingType = TokenType::WHILE;
        }

        // 返回token
        return Token(lastAcceptingType, tokenValue, line, startColumn);
    }

    // 如果没有找到有效的转换，处理单个字符
    if (pos == startPos) {
        QChar ch = input[pos];
        pos++;
        column++;
        return Token(TokenType::UNKNOWN, QString(ch), line, startColumn);
    }

    // 无法识别，返回UNKNOWN token
    return Token(TokenType::UNKNOWN, "", line, startColumn);
}

// 跳过空格
void LexicalAnalyzer::skipWhitespace(const QString& input, int& pos, int& line, int& column)
{
    // 检查是否为空格
    while (pos < input.length() && input[pos].isSpace()) {
        // 如果是换行符，换行，行号 +1,列号重置为 1
        if (input[pos] == '\n') {
            line++;
            column = 1;
        } else {
            // 否则列号 +1
            column++;
        }

        // 下一个字符
        pos++;
    }
}

// 关键字判断
bool LexicalAnalyzer::isKeyword(const QString& word) const
{
    return keywords.contains(word);
}

// 是否是分隔符
bool LexicalAnalyzer::isDelimiter(QChar ch) const{
    static const QSet<QChar> delimiters = {
        '(', ')', '{', '}', '[', ']', ';', ','
    };
    return delimiters.contains(ch);
}


// DFA最小化实现
void LexicalAnalyzer::minimizeDFA()
{
    // qDebug() << "开始DFA最小化...";

    // 初始划分：接受状态和非接受状态
    QVector<StateGroup> partition = partitionStates();

    // 新的划分
    QVector<StateGroup> newPartition;

    do {
        // 重新划分
        newPartition = refinePartition(partition);

        // 如果新旧分组数量相同，检查每组是否完全一致
        if (newPartition.size() == partition.size()) {
            bool same = true;
            for (int i = 0; i < partition.size(); ++i) {
                if (partition[i].states != newPartition[i].states) {
                    same = false;
                    break;
                }
            }
            // 如果所有分组一致，则最小化完成
            if (same) break;
        }
        partition = newPartition;
    } while (true);

    // 划分完成
    // 构建最小化的DFA
    buildMinimizedDFA(partition);
}

// 初始状态划分
QVector<StateGroup> LexicalAnalyzer::partitionStates()
{
    QVector<StateGroup> partition;         // 最终的划分
    QMap<TokenType, StateGroup> groups;    // 按 token 类型分类的 map

    // 遍历 DFA，将状态按照类型分组
    for (auto it = states.begin(); it != states.end(); ++it) {
        const DFAState& state = it.value();

        // 接受状态，按 Token 类型分组
        TokenType type = state.isAccepting ? state.tokenType : TokenType::UNKNOWN;

        // 将状态 ID 插入对应类型的 group
        groups[type].states.insert(state.id);
        groups[type].tokenType = type;
        groups[type].isAccepting = state.isAccepting;
    }

    // 将groups 中的各个 StartGroup 提取到 partition 向量中
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        partition.append(it.value());
    }

    return partition;  // 返回初始划分结果
}

// 划分函数
QVector<StateGroup> LexicalAnalyzer::refinePartition(const QVector<StateGroup>& partition)
{
    QVector<StateGroup> newPartition;

    // 遍历每一个状态组，尝试将其进一步划分为多个子组
    for (const StateGroup& group : partition) {
        QMap<QString, StateGroup> subGroups;  // 根据签名分类的子组集合

        // 遍历当前组内的每个状态
        for (int state : group.states) {
            QString signature;   // 用于唯一标识状态行为的 “签名”

            // 为每个状态生成签名（基于转换函数）
            for (const QChar& c : alphabet) {
                int targetState = -1;

                // 查找该状态下对输入字符 c 的目标状态
                if (states.contains(state) && states[state].transitions.contains(c)) {
                    targetState = states[state].transitions[c];
                }

                // 获取目标状态所属的组编号
                int targetGroupId = findGroupId(targetState, partition);

                // 将组 ID 加入签名字符串中，用逗号分隔
                signature += QString::number(targetGroupId) + ",";
            }
            // 使用签名将状态分类到子组中
            subGroups[signature].states.insert(state);
            subGroups[signature].tokenType = group.tokenType;
            subGroups[signature].isAccepting = group.isAccepting;
        }

        // 将所有细化后的子组加入到新的分组中
        for (auto it = subGroups.begin(); it != subGroups.end(); ++it) {
            newPartition.append(it.value());
        }
    }

    return newPartition;
}

// 查找某个状态属于哪个分组，并返回该组在分区中的索引
int LexicalAnalyzer::findGroupId(int state, const QVector<StateGroup>& partition) const
{
    for (int i = 0; i < partition.size(); ++i) {
        if (partition[i].states.contains(state)) {
            return i;  // 组ID
        }
    }
    return -1;
}

// 建立最小DFA
void LexicalAnalyzer::buildMinimizedDFA(const QVector<StateGroup>& finalPartition)
{
    minimizedStates.clear();

    // 为每个组创建一个新状态
    for (int i = 0; i < finalPartition.size(); ++i) {
        const StateGroup& group = finalPartition[i];

        // 创建新的最小状态
        DFAState newState(i, group.isAccepting, group.tokenType);

        // 选择组中的一个代表状态来构建转换
        int representative = *group.states.begin();

        if (states.contains(representative)) {
            // 遍历该代表状态的所有转换
            for (auto transIt = states[representative].transitions.begin();
                transIt != states[representative].transitions.end(); ++transIt) {

                QChar input = transIt.key();     // 输入字符
                int oldTarget = transIt.value(); // 旧 DFA 中的目标状态
                int newTarget = findGroupId(oldTarget, finalPartition);

                // 找到合法的新目标组，就添加到新状态的转换中
                if (newTarget != -1) {
                    newState.transitions[input] = newTarget;
                }
            }
        }

        minimizedStates[i] = newState;
    }

    // 更新开始状态
    startState = findGroupId(0, finalPartition);
}

// 打印DFA
void LexicalAnalyzer::printDFA() const
{
    qDebug() << "=== 原始DFA ===";
    for (auto it = states.begin(); it != states.end(); ++it) {
        const DFAState& state = it.value();
        QString stateInfo = QString("状态 %1:").arg(state.id);
        if (state.isAccepting) {
            stateInfo += QString(" [接受状态, 类型: %1]").arg(static_cast<int>(state.tokenType));
        }
        qDebug() << stateInfo;

        for (auto transIt = state.transitions.begin(); transIt != state.transitions.end(); ++transIt) {
            qDebug() << QString("  %1 -> 状态 %2").arg(transIt.key()).arg(transIt.value());
        }
    }
}

// 打印最小化DFA
void LexicalAnalyzer::printMinimizedDFA() const
{
    qDebug() << "=== 最小化DFA ===";
    for (auto it = minimizedStates.begin(); it != minimizedStates.end(); ++it) {
        const DFAState& state = it.value();
        QString stateInfo = QString("状态 %1:").arg(state.id);
        if (state.isAccepting) {
            stateInfo += QString(" [接受状态, 类型: %1]").arg(static_cast<int>(state.tokenType));
        }
        qDebug() << stateInfo;

        for (auto transIt = state.transitions.begin(); transIt != state.transitions.end(); ++transIt) {
            qDebug() << QString("  %1 -> 状态 %2").arg(transIt.key()).arg(transIt.value());
        }
    }
}
