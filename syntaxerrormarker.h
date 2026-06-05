#ifndef SYNTAXERRORMARKER_H
#define SYNTAXERRORMARKER_H

#include <QTextEdit>
#include <QTextBlock>
#include <QList>

class SyntaxErrorMarker {
public:
    explicit SyntaxErrorMarker(QTextEdit *editor) : editor(editor) {}

    // 标记语法错误（基于行号、列号和错误信息）
    void markError(int line, int column, int length, const QString& message) {
        clearMarks();

        QTextCharFormat errorFormat;
        errorFormat.setUnderlineColor(Qt::red); // 红色波浪线
        errorFormat.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        errorFormat.setToolTip(message);

        addMark(line, column, length, errorFormat);
    }

    void clearMarks() {
        editor->setExtraSelections({});
    }

private:
    void addMark(int line, int column, int length, const QTextCharFormat& format) {
        QTextCursor cursor(editor->document());
        cursor.movePosition(QTextCursor::Start);

        // 移动到指定行
        for (int i = 1; i < line; ++i) {
            if (!cursor.movePosition(QTextCursor::Down)) break;
        }

        // 移动到指定列（防止列号超出范围）
        int maxColumn = cursor.block().length() - 1;
        int actualColumn = qMin(column - 1, maxColumn);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, actualColumn);

        // 计算实际长度（防止超出当前行）
        int actualLength = qMin(length, maxColumn - actualColumn + 1);
        if (actualLength > 0) {
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, actualLength);

            QTextEdit::ExtraSelection extra;
            extra.cursor = cursor;
            extra.format = format;
            editor->setExtraSelections({extra});
        }
    }

    QTextEdit *editor;
};

#endif // SYNTAXERRORMARKER_H
