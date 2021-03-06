#include "veditutils.h"

#include <QTextDocument>
#include <QDebug>
#include <QTextEdit>
#include <QScrollBar>

#include "vutils.h"

void VEditUtils::removeBlock(QTextBlock &p_block, QString *p_text)
{
    QTextCursor cursor(p_block);
    removeBlock(cursor, p_text);
}

void VEditUtils::removeBlock(QTextCursor &p_cursor, QString *p_text)
{
    const QTextDocument *doc = p_cursor.document();
    int blockCount = doc->blockCount();
    int blockNum = p_cursor.block().blockNumber();

    p_cursor.select(QTextCursor::BlockUnderCursor);
    if (p_text) {
        *p_text = selectedText(p_cursor) + "\n";
    }

    p_cursor.deleteChar();

    // Deleting the first block will leave an empty block.
    // Deleting the last empty block will not work with deleteChar().
    if (blockCount == doc->blockCount()) {
        if (blockNum == blockCount - 1) {
            // The last block.
            p_cursor.deletePreviousChar();
        } else {
            p_cursor.deleteChar();
        }
    }

    if (p_cursor.block().blockNumber() < blockNum) {
        p_cursor.movePosition(QTextCursor::NextBlock);
    }

    p_cursor.movePosition(QTextCursor::StartOfBlock);
}

bool VEditUtils::insertBlockWithIndent(QTextCursor &p_cursor)
{
    V_ASSERT(!p_cursor.hasSelection());
    p_cursor.insertBlock();
    return indentBlockAsPreviousBlock(p_cursor);
}

bool VEditUtils::insertListMarkAsPreviousBlock(QTextCursor &p_cursor)
{
    bool ret = false;
    QTextBlock block = p_cursor.block();
    QTextBlock preBlock = block.previous();
    if (!preBlock.isValid()) {
        return false;
    }

    QString text = preBlock.text();
    QRegExp regExp("^\\s*(-|\\d+\\.)\\s");
    int regIdx = regExp.indexIn(text);
    if (regIdx != -1) {
        ret = true;
        V_ASSERT(regExp.captureCount() == 1);
        QString markText = regExp.capturedTexts()[1];
        if (markText == "-") {
            // Insert - in front.
            p_cursor.insertText("- ");
        } else {
            // markText is like "123.".
            V_ASSERT(markText.endsWith('.'));
            bool ok = false;
            int num = markText.left(markText.size() - 1).toInt(&ok, 10);
            V_ASSERT(ok);
            num++;
            p_cursor.insertText(QString::number(num, 10) + ". ");
        }
    }

    return ret;

}

bool VEditUtils::indentBlockAsPreviousBlock(QTextCursor &p_cursor)
{
    bool changed = false;
    QTextBlock block = p_cursor.block();
    if (block.blockNumber() == 0) {
        // The first block.
        return false;
    }

    QTextBlock preBlock = block.previous();
    QString text = preBlock.text();
    QRegExp regExp("(^\\s*)");
    regExp.indexIn(text);
    V_ASSERT(regExp.captureCount() == 1);
    QString leadingSpaces = regExp.capturedTexts()[1];

    moveCursorFirstNonSpaceCharacter(p_cursor, QTextCursor::MoveAnchor);
    if (!p_cursor.atBlockStart()) {
        p_cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
        p_cursor.removeSelectedText();
        changed = true;
    }

    if (!leadingSpaces.isEmpty()) {
        p_cursor.insertText(leadingSpaces);
        changed = true;
    }

    return changed;
}

void VEditUtils::moveCursorFirstNonSpaceCharacter(QTextCursor &p_cursor,
                                                  QTextCursor::MoveMode p_mode)
{
    QTextBlock block = p_cursor.block();
    QString text = block.text();
    int idx = 0;
    for (; idx < text.size(); ++idx) {
        if (text[idx].isSpace()) {
            continue;
        } else {
            break;
        }
    }

    p_cursor.setPosition(block.position() + idx, p_mode);
}

void VEditUtils::removeObjectReplacementCharacter(QString &p_text)
{
    QRegExp orcBlockExp(QString("[\\n|^][ |\\t]*\\xfffc[ |\\t]*(?=\\n)"));
    p_text.remove(orcBlockExp);
    p_text.remove(QChar::ObjectReplacementCharacter);
}

QString VEditUtils::selectedText(const QTextCursor &p_cursor)
{
    QString text = p_cursor.selectedText();
    text.replace(QChar::ParagraphSeparator, '\n');
    return text;
}

// Use another QTextCursor to remain the selection.
void VEditUtils::indentSelectedBlocks(const QTextDocument *p_doc,
                                      const QTextCursor &p_cursor,
                                      const QString &p_indentationText,
                                      bool p_isIndent)
{
    int nrBlocks = 1;
    int start = p_cursor.selectionStart();
    int end = p_cursor.selectionEnd();

    QTextBlock sBlock = p_doc->findBlock(start);
    if (start != end) {
        QTextBlock eBlock = p_doc->findBlock(end);
        nrBlocks = eBlock.blockNumber() - sBlock.blockNumber() + 1;
    }

    QTextCursor bCursor(sBlock);
    bCursor.beginEditBlock();
    for (int i = 0; i < nrBlocks; ++i) {
        if (p_isIndent) {
            indentBlock(bCursor, p_indentationText);
        } else {
            unindentBlock(bCursor, p_indentationText);
        }

        bCursor.movePosition(QTextCursor::NextBlock);
    }
    bCursor.endEditBlock();
}

void VEditUtils::indentBlock(QTextCursor &p_cursor,
                             const QString &p_indentationText)
{
    QTextBlock block = p_cursor.block();
    if (block.length() > 1) {
        p_cursor.movePosition(QTextCursor::StartOfBlock);
        p_cursor.insertText(p_indentationText);
    }
}

void VEditUtils::unindentBlock(QTextCursor &p_cursor,
                               const QString &p_indentationText)
{
    QTextBlock block = p_cursor.block();
    QString text = block.text();
    if (text.isEmpty()) {
        return;
    }

    p_cursor.movePosition(QTextCursor::StartOfBlock);
    if (text[0] == '\t') {
        p_cursor.deleteChar();
    } else if (text[0].isSpace()) {
        int width = p_indentationText.size();
        for (int i = 0; i < width; ++i) {
            if (text[i] == ' ') {
                p_cursor.deleteChar();
            } else {
                break;
            }
        }
    }
}

bool VEditUtils::findTargetWithinBlock(QTextCursor &p_cursor,
                                       QTextCursor::MoveMode p_mode,
                                       QChar p_target,
                                       bool p_forward,
                                       bool p_inclusive,
                                       int p_repeat)
{
    if (p_repeat < 1) {
        return false;
    }

    QTextBlock block = p_cursor.block();
    QString text = block.text();
    int pib = p_cursor.positionInBlock();
    int delta = p_forward ? 1 : -1;

    // The index to start searching.
    int idx = pib + (p_inclusive ? delta : 2 * delta);

    for (; idx < text.size() && idx >= 0; idx += delta) {
        if (text[idx] == p_target) {
            if (--p_repeat == 0) {
                break;
            }
        }
    }

    if (idx < 0 || idx >= text.size() || p_repeat > 0) {
        return false;
    }

    // text[idx] is the target character.
    if ((p_forward && p_inclusive && p_mode == QTextCursor::KeepAnchor)
        || (!p_forward && !p_inclusive)) {
        ++idx;
    } else if (p_forward && !p_inclusive && p_mode == QTextCursor::MoveAnchor) {
        --idx;
    }

    p_cursor.setPosition(block.position() + idx, p_mode);
    return true;
}

int VEditUtils::findTargetsWithinBlock(QTextCursor &p_cursor,
                                       QTextCursor::MoveMode p_mode,
                                       const QList<QChar> &p_targets,
                                       bool p_forward,
                                       bool p_inclusive)
{
    if (p_targets.isEmpty()) {
        return -1;
    }

    int targetIdx = -1;
    QTextBlock block = p_cursor.block();
    QString text = block.text();
    int pib = p_cursor.positionInBlock();
    int delta = p_forward ? 1 : -1;

    // The index to start searching.
    int idx = pib + (p_inclusive ? delta : 2 * delta);

    for (; idx < text.size() && idx >= 0; idx += delta) {
        int index = p_targets.indexOf(text[idx]);
        if (index != -1) {
            targetIdx = index;
            break;
        }
    }

    if (idx < 0 || idx >= text.size()) {
        return -1;
    }

    // text[idx] is the target character.
    if ((p_forward && p_inclusive && p_mode == QTextCursor::KeepAnchor)
        || (!p_forward && !p_inclusive)) {
        ++idx;
    } else if (p_forward && !p_inclusive && p_mode == QTextCursor::MoveAnchor) {
        --idx;
    }

    p_cursor.setPosition(block.position() + idx, p_mode);
    return targetIdx;
}


int VEditUtils::selectedBlockCount(const QTextCursor &p_cursor)
{
    if (!p_cursor.hasSelection()) {
        return 0;
    }

    QTextDocument *doc = p_cursor.document();
    int sbNum = doc->findBlock(p_cursor.selectionStart()).blockNumber();
    int ebNum = doc->findBlock(p_cursor.selectionEnd()).blockNumber();

    return ebNum - sbNum + 1;
}

void VEditUtils::scrollBlockInPage(QTextEdit *p_edit,
                                   int p_blockNum,
                                   int p_dest)
{
    QTextDocument *doc = p_edit->document();
    QTextCursor cursor = p_edit->textCursor();
    if (p_blockNum >= doc->blockCount()) {
        p_blockNum = doc->blockCount() - 1;
    }

    QTextBlock block = doc->findBlockByNumber(p_blockNum);

    int pib = cursor.positionInBlock();
    if (cursor.block().blockNumber() != p_blockNum) {
        // Move the cursor to the block.
        if (pib >= block.length()) {
            pib = block.length() - 1;
        }

        cursor.setPosition(block.position() + pib);
        p_edit->setTextCursor(cursor);
    }

    // Scroll to let current cursor locate in proper position.
    p_edit->ensureCursorVisible();
    QScrollBar *vsbar = p_edit->verticalScrollBar();

    if (!vsbar || !vsbar->isVisible()) {
        // No vertical scrollbar. No need to scrool.
        return;
    }

    QRect rect = p_edit->cursorRect();
    int height = p_edit->rect().height();
    QScrollBar *sbar = p_edit->horizontalScrollBar();
    if (sbar && sbar->isVisible()) {
        height -= sbar->height();
    }

    switch (p_dest) {
    case 0:
    {
        // Top.
        while (rect.y() > 0 && vsbar->value() < vsbar->maximum()) {
            vsbar->setValue(vsbar->value() + vsbar->singleStep());
            rect = p_edit->cursorRect();
        }

        break;
    }

    case 1:
    {
        // Center.
        height = qMax(height / 2, 1);
        if (rect.y() > height) {
            while (rect.y() > height && vsbar->value() < vsbar->maximum()) {
                vsbar->setValue(vsbar->value() + vsbar->singleStep());
                rect = p_edit->cursorRect();
            }
        } else if (rect.y() < height) {
            while (rect.y() < height && vsbar->value() > vsbar->minimum()) {
                vsbar->setValue(vsbar->value() - vsbar->singleStep());
                rect = p_edit->cursorRect();
            }
        }

        break;
    }

    case 2:
        // Bottom.
        while (rect.y() < height && vsbar->value() > vsbar->minimum()) {
            vsbar->setValue(vsbar->value() - vsbar->singleStep());
            rect = p_edit->cursorRect();
        }

        break;

    default:
        break;
    }

    p_edit->ensureCursorVisible();
}

bool VEditUtils::isListBlock(const QTextBlock &p_block, int *p_seq)
{
    QString text = p_block.text();
    QRegExp regExp("^\\s*(-|\\d+\\.)\\s");

    if (p_seq) {
        *p_seq = -1;
    }

    int regIdx = regExp.indexIn(text);
    if (regIdx == -1) {
        return false;
    }

    V_ASSERT(regExp.captureCount() == 1);
    QString markText = regExp.capturedTexts()[1];
    if (markText != "-") {
        V_ASSERT(markText.endsWith('.'));
        bool ok = false;
        int num = markText.left(markText.size() - 1).toInt(&ok, 10);
        V_ASSERT(ok);
        if (p_seq) {
            *p_seq = num;
        }
    }

    return true;
}

bool VEditUtils::isSpaceToBlockStart(const QTextBlock &p_block, int p_posInBlock)
{
    if (p_posInBlock <= 0) {
        return true;
    }

    QString text = p_block.text();
    V_ASSERT(text.size() >= p_posInBlock);
    return text.left(p_posInBlock).trimmed().isEmpty();
}

void VEditUtils::deleteIndentAndListMark(QTextCursor &p_cursor)
{
    V_ASSERT(!p_cursor.hasSelection());
    p_cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
    p_cursor.removeSelectedText();
}


bool VEditUtils::selectPairTargetAround(QTextCursor &p_cursor,
                                        QChar p_opening,
                                        QChar p_closing,
                                        bool p_inclusive,
                                        bool p_crossBlock,
                                        int p_repeat)
{
    Q_ASSERT(p_repeat >= 1);

    QTextDocument *doc = p_cursor.document();
    int pos = p_cursor.position();

    // Search range [start, end].
    int start = 0;
    int end = doc->characterCount() - 1;
    if (!p_crossBlock) {
        QTextBlock block = p_cursor.block();
        start = block.position();
        end = block.position() + block.length() - 1;
    }

    if (start == end || pos > end) {
        return false;
    }

    Q_ASSERT(!doc->characterAt(pos).isNull());

    bool found = false;

    // The number of un-paired symbols before we meet a target.
    // For example, when we are searching the `(`, nrPair is the number of
    // the un-paired `)` currently.
    int nrPair = 0;

    // The absolute position of the found target.
    // vnote|(vnote|)vnote
    int opening = pos;
    int closing = pos;

round:
    // "abc|"def", after `di"`, becomes "|"def"
    // So we need to try closing first.
    QChar ch = doc->characterAt(closing);
    Q_ASSERT(!ch.isNull());
    if (ch == p_closing) {
        // Try to find the opening.
        nrPair = 1;
        int i = opening;
        if (opening == closing) {
            --i;
        }

        for (; i >= start; --i) {
            ch = doc->characterAt(i);
            Q_ASSERT(!ch.isNull());
            if (ch == p_opening) {
                if (--nrPair == 0) {
                    break;
                }
            } else if (ch == p_closing) {
                ++nrPair;
            }
        }

        if (i >= start) {
            // Found the opening. Done.
            opening = i;
            found = true;
        }
    }

    ch = doc->characterAt(opening);
    Q_ASSERT(!ch.isNull());
    if (!found && ch == p_opening) {
        // Try to find the closing.
        nrPair = 1;
        int j = closing;
        if (opening == closing) {
            ++j;
        }

        for (; j <= end; ++j) {
            ch = doc->characterAt(j);
            Q_ASSERT(!ch.isNull());
            if (ch == p_closing) {
                if (--nrPair == 0) {
                    break;
                }
            } else if (ch == p_opening) {
                ++nrPair;
            }
        }

        if (j <= end) {
            // Foudnd the closing. Done.
            closing = j;
            found = true;
        }
    }

    if (!found
        && doc->characterAt(opening) != p_opening
        && doc->characterAt(closing) != p_closing) {
        // Need to find both the opening and closing.
        int i = opening - 1;
        int j = closing + 1;
        // Pretend that we have found one.
        nrPair = 1;
        for (; i >= start; --i) {
            ch = doc->characterAt(i);
            Q_ASSERT(!ch.isNull());
            if (ch == p_opening) {
                if (--nrPair == 0) {
                    break;
                }
            } else if (ch == p_closing) {
                ++nrPair;
            }
        }

        if (i >= start) {
            opening = i;
            // Continue to find the closing.
            nrPair = 1;
            for (; j <= end; ++j) {
                ch = doc->characterAt(j);
                Q_ASSERT(!ch.isNull());
                if (ch == p_closing) {
                    if (--nrPair == 0) {
                        break;
                    }
                } else if (ch == p_opening) {
                    ++nrPair;
                }
            }

            if (j <= end) {
                closing = j;
                found = true;
            }
        }
    }

    if (!found) {
        return false;
    } else if (--p_repeat) {
        // Need to find more.
        found = false;
        --opening;
        ++closing;

        if (opening < start && closing > end) {
            return false;
        }

        goto round;
    }

    if (p_inclusive) {
        ++closing;
    } else {
        ++opening;
    }

    p_cursor.setPosition(opening, QTextCursor::MoveAnchor);
    p_cursor.setPosition(closing, QTextCursor::KeepAnchor);
    return true;
}
