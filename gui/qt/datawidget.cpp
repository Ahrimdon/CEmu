#include "datawidget.h"

/* extraHighlights[0] = current line selection */

DataWidget::DataWidget(QWidget *p) : QPlainTextEdit(p) {
    cursorMoveable = false;
    setContextMenuPolicy(Qt::CustomContextMenu);
}

void DataWidget::clearAllHighlights() {
    disconnect(this, &QPlainTextEdit::cursorPositionChanged, this, &DataWidget::highlightCurrentLine);
    while (!extraHighlights.isEmpty()) {
        extraHighlights.removeFirst();
    }

    extraHighlights.clear();
    updateAllHighlights();
}

void DataWidget::updateAllHighlights() {
    setExtraSelections(extraHighlights);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &DataWidget::highlightCurrentLine);
}

QString DataWidget::getSelectedAddress() {
    if (!isEnabled()) {
        return QString("000000");
    }
    QTextCursor c = textCursor();
    c.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
    c.setPosition(c.position()+6, QTextCursor::KeepAnchor); // +6 == size of the address
                                                            // See MainWindow::drawNextDisassembleLine() for details
    return c.selectedText();
}

bool DataWidget::labelCheck() {
    if (!isEnabled()) {
        return false;
    }
    QTextCursor c = textCursor();
    c.movePosition(QTextCursor::EndOfLine, QTextCursor::MoveAnchor);
    c.setPosition(c.position()-1, QTextCursor::KeepAnchor);

    return c.selectedText().at(0) == ':';
}

void DataWidget::cursorState(bool moveable) {
    cursorMoveable = moveable;
    if (moveable) {
        addHighlight(QColor(Qt::yellow).lighter(160));
    }
}

void DataWidget::addHighlight(const QColor &color) {
    QTextEdit::ExtraSelection selection;

    selection.format.setBackground(color);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.movePosition(QTextCursor::StartOfLine);

    extraHighlights.append(selection);
}

void DataWidget::highlightCurrentLine() {
    if (cursorMoveable == true) {
        if (!extraHighlights.isEmpty()) {
            extraHighlights.removeLast();
        }
        addHighlight(QColor(Qt::yellow).lighter(160));
        setExtraSelections(extraHighlights);
    }
}
