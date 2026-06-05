#ifndef ANALYZER_H
#define ANALYZER_H

#include <QObject>
#include <QVector>
#include "lexer.h"
#include "lexer.h"
#include "parser.h"

class analyzer : public QObject
{
    Q_OBJECT
public:
    analyzer(QObject *parent = nullptr);

public slots:
    void analyzeCode(const QString &code);

signals:
    void tokensReady(const QVector<Token> &tokens);
    void parseFinish(const ParseResult &result);
    void analysisFinished();

private:
    LexicalAnalyzer *lexer;
    parser *parse;
};

#endif // ANALYZER_H
