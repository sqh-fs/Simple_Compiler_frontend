#include "parser.h"
#include <QQueue>

// 初始化lookahead（给增广文法加 $）
void parser::buildInitialKernels() {
    if (kernels.isEmpty()) return;

    // productions[0] 是 S' → S，是起始产生式
    for (LRItem& item : kernels[0]) {
        if (item.productionId == 0 && item.dotPosition == 0) {
            item.lookahead.insert("$");  // 只初始化这个起始项的 lookahead
            break;
        }
    }
}

// 先把项中点后是终结符的加入lookahead
void parser::initializeLookaheadWithTerminals() {
    for (int state = 0; state < kernels.size(); ++state) {
        QVector<LRItem>& kernelSet = kernels[state];
        for (LRItem& item : kernelSet) {
            const Production& prod = productions[item.productionId];

            int pos = item.dotPosition;
            // 判断点后是否还有符号
            if (pos < prod.right.size()) {
                QString nextSymbol = prod.right[pos];
                // 如果是终结符则加入lookahead
                if (terminals.contains(nextSymbol)) {
                    item.lookahead.insert(nextSymbol);
                }
                // 不是终结符则跳过，不做任何处理
            }
            // 点已经在产生式末尾，跳过
        }
    }
}



// 打印所有内核项
void parser::printKernels(const QVector<QVector<LRItem>>& kernels) {
    for (int i = 0; i < kernels.size(); ++i) {
        qDebug() << "State" << i << "Kernel:";
        for (const LRItem& item : kernels[i]) {
            const Production& prod = productions[item.productionId];
            QString itemStr = prod.left + " -> ";
            for (int j = 0; j < prod.right.size(); ++j) {
                if (j == item.dotPosition) itemStr += "·";
                itemStr += prod.right[j] + " ";
            }
            if (item.dotPosition == prod.right.size()) itemStr += "·";

            itemStr = itemStr.trimmed();

            // 打印 lookahead（如果非空）
            if (!item.lookahead.isEmpty()) {
                QList<QString> sortedLookahead = item.lookahead.values();
                std::sort(sortedLookahead.begin(), sortedLookahead.end());
                itemStr += " , { ";
                for (const QString& s : sortedLookahead) {
                    itemStr += s + " ";
                }
                itemStr += "}";
            }

            qDebug() << "  " << itemStr;
        }
        qDebug() << "";
    }
    qDebug() << "";
}

// 计算FIRST集
void parser::buildFirstSets(){
    // 变化标记
    bool changed = true;

    // 初始化终结符的FIRST集
    for (const QString& symbol : terminals) {
        firstSets[symbol].insert(symbol);
    }
    // 初始化非终结符的FIRST集
    for (const QString& symbol : nonterminals) {
        firstSets[symbol] = QSet<QString>();
    }

    while (changed) {
        changed = false;
        for (const Production& prod : productions) {
            QSet<QString> oldFirst = firstSets[prod.left];

            // 处理空产生式
            if (prod.right.isEmpty() || prod.right[0] == "ε") {
                firstSets[prod.left].insert("ε"); // 空产生式
            } else {

                for (int i = 0; i < prod.right.size(); ++i) {
                    QString symbol = prod.right[i];
                    QSet<QString> symbolFirst = firstSets[symbol];
                    symbolFirst.remove("ε");
                    firstSets[prod.left].unite(symbolFirst);

                    // 若 FIRST(Xi) 不包含 ε，则停止
                    if (!firstSets[symbol].contains("ε")) {
                        break;
                    }

                    // 如果所有 X1...Xn 都能推出 ε，则将 ε 加入 FIRST(A)
                    if (i == prod.right.size() - 1) {
                        firstSets[prod.left].insert("ε");
                    }
                }
            }

            if (firstSets[prod.left] != oldFirst) {
                changed = true;
            }
        }
    }
}

// 打印FIRST集
void parser::printFirstSets() const {
    qDebug() << "FIRST Sets:";
    for (auto it = firstSets.constBegin(); it != firstSets.constEnd(); ++it) {
        QString symbol = it.key();
        const QSet<QString>& firstSet = it.value();

        QStringList elements = QStringList(firstSet.begin(), firstSet.end());
        elements.sort();  // 可选：让输出更有序

        qDebug() << QString("FIRST(%1) = { %2 }")
                        .arg(symbol, elements.join(", "));
    }
}


QSet<QString> parser::computeFirst(const QVector<QString>& beta) {
    QSet<QString> firstSet;
    bool allNullable = true;

    for (const QString& symbol : beta) {
        // 如果是终结符，加入 FIRST 集并终止
        if (terminals.contains(symbol)) {
            firstSet.insert(symbol);
            allNullable = false;
            break;
        }
        // 如果是非终结符，合并其 FIRST 集
        else if (nonterminals.contains(symbol)) {
            const QSet<QString>& symbolFirst = firstSets[symbol];

            // 加入非 ε 的符号
            for (const QString& s : symbolFirst) {
                if (s != "ε") {
                    firstSet.insert(s);
                }
            }

            // 如果当前符号不可推导出 ε，终止
            if (!symbolFirst.contains("ε")) {
                allNullable = false;
                break;
            }
        }
        // 未知符号（错误情况）
        else {
            firstSet.clear();
            allNullable = false;
            break;
        }
    }

    // 如果所有符号都可推导出 ε，则加入 ε
    if (allNullable) {
        firstSet.insert("ε");
    }

    return firstSet;
}

// 带向前看的闭包运算
QVector<LRItem> parser::closureWithLookahead(const QVector<LRItem>& items) {
    QVector<LRItem> closureItems = items;
    QHash<QString, LRItem*> itemMap; // 用哈希表快速查找项

    // 初始化队列和哈希表
    QQueue<LRItem> queue;
    for (const LRItem& item : items) {
        queue.enqueue(item);
        itemMap.insert(item.toString(), &closureItems.last());
    }

    // 广度遍历
    while (!queue.isEmpty()) {
        LRItem current = queue.dequeue();
        const Production& prod = productions[current.productionId];

        // 如果点已到末尾，跳过
        if (current.dotPosition >= prod.right.size()) continue;

        // 找点后的符号 B
        QString B = prod.right[current.dotPosition];
        if (!nonterminals.contains(B)) continue;

        // 计算 FIRST(β)
        QVector<QString> beta;
        // B后面的符号串
        for (int i = current.dotPosition + 1; i < prod.right.size(); i++) {
            beta.append(prod.right[i]);
        }
        // 计算FIRST集
        QSet<QString> firstBeta = computeFirst(beta);

        // 合并 lookahead
        QSet<QString> newLookaheads = firstBeta;
        // 如果包含空，需要把当前项的lookahead传给新的项
        if (firstBeta.contains("ε")) {
            newLookaheads.unite(current.lookahead);
        }
        newLookaheads.remove("ε");

        // 添加 B 的所有产生式
        for (int i = 0; i < productions.size(); i++) {
            if (productions[i].left == B) {
                LRItem newItem(i, 0);
                newItem.lookahead = newLookaheads;
                QString newItemStr = newItem.toString();

                // 如果项已存在，合并 lookahead
                if (itemMap.contains(newItemStr)) {
                    LRItem* existing = itemMap[newItemStr];
                    int oldSize = existing->lookahead.size();
                    existing->lookahead.unite(newLookaheads);

                    // 如果 lookahead 有变化，重新加入队列以传播
                    if (existing->lookahead.size() > oldSize) {
                        queue.enqueue(*existing);
                    }
                }
                // 如果项不存在，添加新项
                else {
                    closureItems.append(newItem);
                    itemMap.insert(newItemStr, &closureItems.last());
                    queue.enqueue(newItem);
                }
            }
        }
    }
    return closureItems;
}

// 自动生成的lookahead
void parser::initializeKernelLookaheads(QVector<QVector<LRItem>>& kernels,
                                        const QVector<LRItem>& temp) {
    // 遍历所有内核状态
    for (auto& kernelState : kernels) {
        // 遍历当前状态中的所有内核项
        for (auto& kernelItem : kernelState) {
            // 跳过点位置为0的项(没有前驱)
            if (kernelItem.dotPosition == 0) {
                continue;
            }

            // 在闭包项中查找对应的前驱项
            // 前驱项应该具有相同的产生式ID，且点位置少1
            for (const auto& closureItem : temp) {
                if (closureItem.productionId == kernelItem.productionId &&
                    closureItem.dotPosition == kernelItem.dotPosition - 1) {
                    // 将闭包项的向前看符号添加到内核项
                    kernelItem.lookahead.unite(closureItem.lookahead);
                    break; // 找到一个匹配就可以停止搜索
                }
            }
        }
    }
}


// 传播算法
void parser::propagateLookaheads() {
    bool changed;
    do {
        changed = false;

        // 遍历所有状态转移
        for (const StateTransition& trans : transitions) {
            const int fromState = trans.fromState;
            const int toState = trans.toState;

            // 遍历源状态的所有内核项
            for (const LRItem& fromItem : kernels[fromState]) {

                // 遍历目标状态的所有内核项
                for (LRItem& toItem : kernels[toState]) {

                    // 记录传播前的 lookahead 大小
                    int beforeSize = toItem.lookahead.size();

                    // 无条件传播所有向前看符号
                    toItem.lookahead.unite(fromItem.lookahead);

                    // 如果 lookahead 增加了新符号，标记需要继续迭代
                    if (toItem.lookahead.size() > beforeSize) {
                        changed = true;
                    }
                }
            }
        }
    } while (changed);
}



// 构建完整的LALR(1)项集族
QVector<QVector<LRItem>> parser::buildCompleteLALR1ItemSets() {

    QVector<QVector<LRItem>> completeItemSets;

    // 为每个内核状态计算完整闭包
    for (const QVector<LRItem>& kernelItems : kernels) {
        // 复制内核项（保留lookahead）
        QVector<LRItem> items = kernelItems;

        // 计算闭包（携带lookahead传播）
        QVector<LRItem> closureItems = closureWithLookahead(items);

        // 合并内核项和闭包项（去重）
        QSet<LRItem> uniqueItems;
        for (const LRItem& item : items) {
            uniqueItems.insert(item);
        }
        for (const LRItem& item : closureItems) {
            uniqueItems.insert(item);
        }

        // 存入结果
        completeItemSets.append(uniqueItems.values().toVector());
    }

    return completeItemSets;
}

// 第二步传播
void parser::propagateLookaheadUntilStable(QVector<QVector<LRItem>>& LALR1) {
    bool changed;
    do {
        changed = false;

        // Step 1: 状态间的传播（通过转移）
        for (int fromState = 0; fromState < LALR1.size(); ++fromState) {
            for (int toState = 0; toState < LALR1.size(); ++toState) {
                // 检查是否存在转移
                for (const StateTransition& trans : transitions) {
                    if (trans.fromState == fromState && trans.toState == toState) {
                        // 传播匹配转移的项
                        propagateAcrossStates(LALR1, fromState, toState, trans.symbol, changed);
                    }
                }
            }
        }

        // Step 2: 状态内的传播（闭包操作）
        for (int state = 0; state < LALR1.size(); ++state) {
            propagateWithinState(LALR1[state], changed);
        }

    } while (changed);
}

// 辅助函数：状态间的传播
void parser::propagateAcrossStates(QVector<QVector<LRItem>>& LALR1,
                                   int fromState, int toState,
                                   const QString& symbol, bool& changed) {
    for (const LRItem& fromItem : LALR1[fromState]) {
        const Production& prod = productions[fromItem.productionId];

        // 检查是否匹配转移符号
        if (fromItem.dotPosition < prod.right.size() &&
            prod.right[fromItem.dotPosition] == symbol) {

            // 在目标状态查找对应的项
            for (LRItem& toItem : LALR1[toState]) {
                if (toItem.productionId == fromItem.productionId &&
                    toItem.dotPosition == fromItem.dotPosition + 1) {
                    // 传播 lookaheads
                    int before = toItem.lookahead.size();
                    toItem.lookahead.unite(fromItem.lookahead);
                    if (toItem.lookahead.size() > before) {
                        changed = true;
                    }
                }
            }
        }
    }
}

// 辅助函数：状态内的传播
void parser::propagateWithinState(QVector<LRItem>& stateItems, bool& changed) {
    for (const LRItem& item : stateItems) {
        const Production& prod = productions[item.productionId];

        if (item.dotPosition < prod.right.size()) {
            const QString& nextSymbol = prod.right[item.dotPosition];

            if (!isTerminal(nextSymbol)) {
                // 查找所有以 nextSymbol 开头的产生式
                for (LRItem& candidate : stateItems) {
                    if (candidate.dotPosition == 0 &&
                        productions[candidate.productionId].left == nextSymbol) {
                        // 传播 lookaheads
                        int before = candidate.lookahead.size();
                        candidate.lookahead.unite(item.lookahead);
                        if (candidate.lookahead.size() > before) {
                            changed = true;
                        }
                    }
                }
            }
        }
    }
}
