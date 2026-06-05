#include "analyzer.h"
#include <QThread>
#include <chrono>
analyzer::analyzer(QObject *parent) : QObject(parent) {
    lexer = new LexicalAnalyzer(this);
    parse = new parser();
}

void analyzer::analyzeCode(const QString &code){
    // qDebug() << "当前线程：" << QThread::currentThread();
    // qDebug() << "开始分析";

    // 词法分析
    auto start = std::chrono::high_resolution_clock::now();
    QVector<Token> tokens = lexer->analyze(code);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration_ms = end - start;
    qDebug() << "词法分析耗时: " << duration_ms.count() << " ms\n";

    emit tokensReady(tokens);

    // 语法分析
    if(!tokens.isEmpty()){
        auto start = std::chrono::high_resolution_clock::now();
        ParseResult parseResult = parse->parse(tokens);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration_ms = end - start;
        qDebug() << "语法分析耗时: " << duration_ms.count() << " ms\n";

        emit parseFinish(parseResult);
    }

    emit analysisFinished();
}
