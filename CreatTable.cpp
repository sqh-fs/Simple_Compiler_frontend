#include "parser.h"

// 构建ACTION表和GOTO表
void parser::buildTables() {
    actionTable.clear();
    gotoTable.clear();

    // 预构建转移状态查找表
    QHash<QPair<int, QString>, int> transitionMap;
    for (const StateTransition& trans : transitions) {
        transitionMap.insert(qMakePair(trans.fromState, trans.symbol), trans.toState);
    }

    for (int stateId = 0; stateId < LALR1.size(); ++stateId) {
        const QVector<LRItem>& itemSet = LALR1[stateId];

        for (const LRItem& item : itemSet) {
            const Production& prod = productions[item.productionId];

            // 情况1: 项目未完成 [A -> α·Xβ]
            if (item.dotPosition < prod.right.size()) {
                QString nextSymbol = prod.right[item.dotPosition];
                int nextState = transitionMap.value(qMakePair(stateId, nextSymbol), -1);

                // 点后有符号 X
                if (nextState != -1) {
                    if (isTerminal(nextSymbol)) {
                        // 终结符：移入动作
                        QPair<int, QString> key(stateId, nextSymbol);
                        // 不覆盖已有的移入动作
                        if (!actionTable.contains(key)) {
                            actionTable[key] = Action(ActionType::SHIFT, nextState);
                        }
                    } else {
                        // 非终结符：GOTO动作
                        QPair<int, QString> key(stateId, nextSymbol);
                        gotoTable[key] = nextState;
                    }
                }
            }
            // 情况2: 项目已完成 [A -> α·]
            else {
                // 特殊处理增广文法的接受项目 [S' -> S·, $]
                if (prod.left == startSymbol) {
                    QPair<int, QString> key(stateId, "$");
                    actionTable[key] = Action(ActionType::ACCEPT, -1);
                } else {
                    // 规约动作：[A -> α·, lookahead]
                    for (const QString& lookahead : item.lookahead) {
                        QPair<int, QString> key(stateId, lookahead);

                        // 检查是否已有动作
                        if (actionTable.contains(key)) {
                            Action existing = actionTable[key];
                            if (existing.type == ActionType::SHIFT) {
                                // 移入/规约冲突：保留移入动作
                                continue;
                            } else if (existing.type == ActionType::REDUCE &&
                                       existing.target != item.productionId) {
                                // 规约/规约冲突
                                reportError(QString("Reduce/Reduce conflict in state %1 on %2")
                                                .arg(stateId).arg(lookahead));
                            }
                        } else {
                            // 添加规约动作，产生式编号从0开始
                            actionTable[key] = Action(ActionType::REDUCE, item.productionId);
                        }
                    }
                }
            }
        }
    }
}

// 打印ACTION表和GOTO表
void parser::printTables(){
    qDebug() << "ACTION Table:";
    for (auto it = actionTable.begin(); it != actionTable.end(); ++it) {
        QString actionStr;
        switch (it.value().type) {
        case ActionType::SHIFT:
            actionStr = QString("s%1").arg(it.value().target);
            break;
        case ActionType::REDUCE:
            actionStr = QString("r%1").arg(it.value().target);
            break;
        case ActionType::ACCEPT:
            actionStr = "acc";
            break;
        case ActionType::ERROR:
            actionStr = "error";
            break;
        }
        qDebug() << QString("ACTION[%1][%2] = %3")
                        .arg(it.key().first).arg(it.key().second).arg(actionStr);
    }

    qDebug() << "GOTO Table:";
    for (auto it = gotoTable.begin(); it != gotoTable.end(); ++it) {
        qDebug() << QString("GOTO[%1][%2] = %3")
        .arg(it.key().first).arg(it.key().second).arg(it.value());
    }
}


void parser::reportError(const QString& message) {
    // 基本错误信息输出
    qDebug() << "Parser Error:" << message;

    // 如果正在解析过程中，提供更详细的上下文信息
    if (currentToken >= 0 && currentToken < tokens.size()) {
        const Token& token = tokens[currentToken];
        qDebug() << "Error occurred at token:" << token.value
                 << "(" << static_cast<int>(token.type) << ")"
                 << "at line:" << token.line
                 << "column:" << token.column;
    }
}

// 语法分析函数
ParseResult parser::parse(const QVector<Token>& inputTokens) {
    ParseResult result;

    // 初始化
    QStack<int> stateStack;        // 状态栈
    QStack<QString> symbolStack;   // 符号栈
    stateStack.push(0);            // 初始状态0入栈

    // 添加结束符到输入
    QVector<Token> tokens = inputTokens;
    // tokens.append(Token(TokenType::END_OF_INPUT, "$"));

    int index = 0; // 当前输入位置

    // 分析过程输出
    // qDebug() << "开始LALR(1)语法分析...";
    // qDebug() << "步骤\t状态栈\t符号栈\t\t输入\t\t动作";

    int step = 0;
    while (true) {
        step++;
        int currentState = stateStack.top();
        QString currentSymbol = tokenToSymbol(tokens[index]);

        // 构造当前状态信息
        QString stateStr = "";
        for (int i = 0; i < stateStack.size(); i++) {
            stateStr += QString::number(stateStack[i]) + " ";
        }
        stateStr = stateStr.trimmed();

        QString symbolStr = "";
        for (int i = 0; i < symbolStack.size(); i++) {
            symbolStr += symbolStack[i] + " ";
        }
        symbolStr = symbolStr.trimmed();

        QString inputStr = "";
        for (int i = index; i < tokens.size() && i < index + 5; i++) {
            inputStr += tokenToSymbol(tokens[i]) + " ";
        }
        inputStr = inputStr.trimmed();

        // 查找ACTION表
        QPair<int, QString> actionKey(currentState, currentSymbol);
        if (!actionTable.contains(actionKey)) {
            // 如果当前符号没有定义动作，检查是否存在 ε 转移
            QPair<int, QString> epsilonKey(currentState, "ε");
            if (actionTable.contains(epsilonKey)) {
                Action epsilonAction = actionTable[epsilonKey];
                if (epsilonAction.type == ActionType::SHIFT) {
                    // 执行 ε 转移：压入 ε 和转移状态
                    symbolStack.push("ε");
                    stateStack.push(epsilonAction.target);

                    QString actionStr = "ε转移至状态 " + QString::number(epsilonAction.target);

                    // 记录步骤
                    result.steps.append(ParseStep(step, stateStr, symbolStr, inputStr, actionStr));

                    // 调试输出
                    // qDebug() << step << "\t" << stateStr << "\t" << symbolStr
                    //          << "\t\t" << inputStr << "\t\t" << actionStr;
                    continue; // 继续循环，不消耗输入符号
                }
            }

            // 如果既没有当前符号的动作，也没有 ε 转移，则报错
            QString errorMsg = QString("在状态%1遇到符号%2时找不到对应的动作").arg(currentState).arg(currentSymbol);
            // qDebug() << "错误：" << errorMsg;

            // 记录错误步骤
            result.steps.append(ParseStep(step, stateStr, symbolStr, inputStr, "ERROR", true, errorMsg));
            result.success = false;
            result.errorMessage = errorMsg;
            if (index < tokens.size()) {
                result.errorLine = tokens[index].line;
                result.errorColumn = tokens[index].column;
            }
            return result;
        }

        Action action = actionTable[actionKey];
        QString actionStr = "";

        switch (action.type) {
        case ActionType::SHIFT: {
            // 移入动作
            actionStr = "SHIFT " + QString::number(action.target);

            // 记录步骤
            result.steps.append(ParseStep(step, stateStr, symbolStr, inputStr, actionStr));

            // qDebug() << step << "\t" << stateStr << "\t" << symbolStr
            //          << "\t\t" << inputStr << "\t\t" << actionStr;

            // 将当前符号和新状态压入栈
            symbolStack.push(currentSymbol);
            stateStack.push(action.target);
            index++; // 移动到下一个输入符号
            break;
        }

        case ActionType::REDUCE: {
            // 规约动作
            int productionId = action.target;
            if (productionId >= productions.size()) {
                QString errorMsg = QString("产生式编号%1超出范围").arg(productionId);
                // qDebug() << "错误：" << errorMsg;

                // 记录错误步骤
                result.steps.append(ParseStep(step, stateStr, symbolStr, inputStr, "ERROR", true, errorMsg));
                result.success = false;
                result.errorMessage = errorMsg;
                return result;
            }

            Production& production = productions[productionId];
            actionStr = "REDUCE by " + production.toString();

            // 记录步骤
            result.steps.append(ParseStep(step, stateStr, symbolStr, inputStr, actionStr));

            // qDebug() << step << "\t" << stateStr << "\t" << symbolStr
            //          << "\t\t" << inputStr << "\t\t" << actionStr;

            // 弹出右部符号个数的状态和符号
            int rightSize = production.right.size();
            for (int i = 0; i < rightSize; i++) {
                if (!stateStack.isEmpty()) stateStack.pop();
                if (!symbolStack.isEmpty()) symbolStack.pop();
            }

            // 将左部符号压入符号栈
            symbolStack.push(production.left);

            // 查找GOTO表，决定下一个状态
            if (stateStack.isEmpty()) {
                QString errorMsg = "状态栈为空";
                // qDebug() << "错误：" << errorMsg;

                // 记录错误步骤
                result.steps.append(ParseStep(step + 1, "", symbolStack.join(" "), inputStr, "ERROR", true, errorMsg));
                result.success = false;
                result.errorMessage = errorMsg;
                return result;
            }

            int topState = stateStack.top();
            QPair<int, QString> gotoKey(topState, production.left);

            if (!gotoTable.contains(gotoKey)) {
                QString errorMsg = QString("在状态%1处理非终结符%2时找不到GOTO项").arg(topState).arg(production.left);
                // qDebug() << "错误：" << errorMsg;

                // 记录错误步骤
                result.steps.append(ParseStep(step + 1, QString::number(topState), symbolStack.join(" "), inputStr, "ERROR", true, errorMsg));
                result.success = false;
                result.errorMessage = errorMsg;
                return result;
            }

            // GOTO下一个状态
            int nextState = gotoTable[gotoKey];
            stateStack.push(nextState);
            break;
        }

        case ActionType::ACCEPT: {
            // 接受动作
            actionStr = "ACCEPT";

            // 记录最终步骤
            result.steps.append(ParseStep(step, stateStr, symbolStr, inputStr, actionStr));

            // qDebug() << step << "\t" << stateStr << "\t" << symbolStr
            //          << "\t\t" << inputStr << "\t\t" << actionStr;
            // qDebug() << "语法分析成功！";

            result.success = true;
            return result;
        }

        case ActionType::ERROR:
        default: {
            // 错误处理
            QString errorMsg = QString("在状态%1遇到unexpected符号%2").arg(currentState).arg(currentSymbol);
            // qDebug() << "语法错误：" << errorMsg;
            // qDebug() << "错误位置：第" << tokens[index].line << "行，第" << tokens[index].column << "列";
            // qDebug() << "当前符号：" << tokens[index].value;

            // 记录错误步骤
            result.steps.append(ParseStep(step, stateStr, symbolStr, inputStr, "ERROR", true, errorMsg));
            result.success = false;
            result.errorMessage = errorMsg;
            result.errorLine = tokens[index].line;
            result.errorColumn = tokens[index].column;
            return result;
        }
        }

        // 防止无限循环
        if (step > 1000) {
            QString errorMsg = "分析步骤过多，可能存在无限循环";
            // qDebug() << "警告：" << errorMsg;

            // 记录错误步骤
            result.steps.append(ParseStep(step, stateStr, symbolStr, inputStr, "ERROR", true, errorMsg));
            result.success = false;
            result.errorMessage = errorMsg;
            return result;
        }
    }

    result.success = true;
    return result;
}
