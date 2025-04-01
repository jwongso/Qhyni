#include "HighlightTableWidget.h"
#include "hyni_merge.h"
#include <QHeaderView>
#include <QDebug>
#include <qevent.h>


HighlightTableWidget::HighlightTableWidget(QWidget* parent) : QTableWidget(parent) {
    // Set up a table with one column
    setColumnCount(1);
    setHorizontalHeaderLabels(QStringList() << "Content");

    // Make the table fill the entire control
    horizontalHeader()->setStretchLastSection(true);
    verticalHeader()->setVisible(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setFocusPolicy(Qt::NoFocus);

    // Enable word wrapping for the table
    setWordWrap(true);
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

void HighlightTableWidget::clearRow() {
    setRowCount(0);
}

void HighlightTableWidget::addText(const QString& text) {

    if (text.isEmpty()) return;

    const QString base = getLastRowString();

    if (!base.isEmpty()) {
        std::string mergedText = hyni::HyniMerge::mergeStrings(base.toStdString(), text.toStdString());
        QTableWidgetItem *obj = item(rowCount() - 1, 0);
        if (obj) {
            obj->setText(QString::fromStdString(mergedText));
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

QString HighlightTableWidget::getLastRowString() const {
    return getLastRowStringImpl();
}

QString HighlightTableWidget::getLastRowString()  {
    return getLastRowStringImpl();
}
