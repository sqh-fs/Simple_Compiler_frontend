#ifndef SYNTAXHIGHLIGHTER_H
#define SYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QMutex>
#include "lexer.h"

class syntaxhighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    syntaxhighlighter(QTextDocument *parent = nullptr);
    void clear();

public slots:
    void updateTokens(const QVector<Token> &tokens);

protected:
    void highlightBlock(const QString& text) override;

private:
    QVector<Token> m_tokens;
    QMap<TokenType, QTextCharFormat> m_formats;
    QMutex m_mutex;

    void initializeFormats();
};

#endif // SYNTAXHIGHLIGHTER_H
