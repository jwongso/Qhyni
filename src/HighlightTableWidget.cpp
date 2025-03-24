#include "HighlightTableWidget.h"
#include <QHeaderView>
#include <QDebug>
#include <qevent.h>


std::vector<int> computeKMPTable(const QString &pattern) {
    int m = pattern.length();
    std::vector<int> lps(m, 0);
    int j = 0;

    for (int i = 1; i < m; ++i) {
        while (j > 0 && pattern[i] != pattern[j]) {
            j = lps[j - 1];
        }
        if (pattern[i] == pattern[j]) {
            ++j;
            lps[i] = j;
        }
    }
    return lps;
}

// Find the longest suffix of A that matches a prefix of B
int findOverlapKMP(const QString &A, const QString &B) {
    QString combined = A + "#" + B;
    std::vector<int> lps = computeKMPTable(combined);
    return lps.back();
}

// Merge two strings properly
QString mergeStrings(const QString &A, const QString &B) {
    int overlapLen = findOverlapKMP(A, B);
    return A + B.mid(overlapLen);
}


HighlightTableWidget::HighlightTableWidget(QWidget* parent) : QTableWidget(parent) {
    // Set up a table with one column
    setColumnCount(1);
    setHorizontalHeaderLabels(QStringList() << "Content");

    // Make the table fill the entire control
    horizontalHeader()->setStretchLastSection(true); // Stretch the last column (only column)
    verticalHeader()->setVisible(false); // Hide the row numbers
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // Expand to fill available space

    setEditTriggers(QAbstractItemView::NoEditTriggers); // Disable editing
    setSelectionMode(QAbstractItemView::SingleSelection); // Allow single selection
    setSelectionBehavior(QAbstractItemView::SelectRows); // Select entire rows
    setFocusPolicy(Qt::StrongFocus); // Allow focus

    // Enable word wrapping for the table
    setWordWrap(true); // Enable word wrapping for the entire table
}

void HighlightTableWidget::highlightText(const QString& text) {
    QString highlightedText;

    // Iterate through all cells in the table
    for (int row = 0; row < rowCount(); ++row) {
        QTableWidgetItem* item = this->item(row, 0); // Only one column
        if (item && item->text().contains(text)) {
            // Highlight the cell
            item->setBackground(Qt::yellow);
            highlightedText = item->text(); // Store the highlighted text
            break; // Stop after finding the first match
        } else if (item) {
            // Remove highlighting
            item->setBackground(Qt::white);
        }
    }

    // Emit the highlighted text (single line)
    if (!highlightedText.isEmpty()) {
        emit textHighlighted(highlightedText);
    }
}

void HighlightTableWidget::keyPressEvent(QKeyEvent* event) {
    // Check if the 's' key is pressed
    if (event->key() == Qt::Key_S) {
        emit triggerSendText();
    } else if (event->key() == Qt::Key_C) {
        setRowCount(0);
    }

    // Call the base class implementation to handle other key events
    QTableWidget::keyPressEvent(event);
}

QString HighlightTableWidget::getHighlightedText() const {
    // Get the current selection
    QList<QTableWidgetItem*> selectedItems = this->selectedItems();

    // If there is a selection, return the text of the first selected item
    if (!selectedItems.isEmpty()) {
        return selectedItems.first()->text(); // Return the text of the first selected item
    }

    // If no text is selected, return an empty string
    return QString();
}

void HighlightTableWidget::addText(const QString& text) {

    if (text.isEmpty()) return;

    const QString base = getLastRowString();

    if (!base.isEmpty()) {
        QString mergedText = mergeStrings(base, text);
        QTableWidgetItem *obj = item(rowCount() - 1, 0);
        if (obj) {
            obj->setText(mergedText);
            resizeRowToContents(rowCount() - 1);
        }
        return;
    }

    // Add a new row to the table
    int row = rowCount();
    insertRow(row);

    // Add the text to the first column of the new row
    QTableWidgetItem* item = new QTableWidgetItem(text);

    // Enable text wrapping
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop); // Align text to the top-left
    item->setFlags(item->flags() & ~Qt::ItemIsEditable); // Ensure the item is not editable
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop); // Align text to the top-left
    item->setData(Qt::TextWordWrap, true); // Enable word wrapping

    setItem(row, 0, item);

    // Adjust row height to fit wrapped text
    resizeRowToContents(row);
}

QString HighlightTableWidget::getLastRowString() {
    if (rowCount() > 0) {
        QTableWidgetItem *obj = item(rowCount() - 1, 0);
        if (obj) {
            return obj->text();
        }
    }

    return QString();
}
