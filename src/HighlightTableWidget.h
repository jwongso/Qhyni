#ifndef HIGHLIGHTTABLEWIDGET_H
#define HIGHLIGHTTABLEWIDGET_H

#include <QTableWidget>
#include <QTableWidgetItem>

class HighlightTableWidget : public QTableWidget {
    Q_OBJECT

public:
    explicit HighlightTableWidget(QWidget* parent = nullptr);

signals:
    void textHighlighted(const QString& text);
    void triggerSendText();

protected:
    void keyPressEvent(QKeyEvent* event) override;

public slots:
    void highlightText(const QString& text); // Highlight text in the table
    QString getHighlightedText() const; // Retrieve highlighted text
    void addText(const QString& text); // Add text to the table

private:
    QString getLastRowString();
};

#endif // HIGHLIGHTTABLEWIDGET_H
