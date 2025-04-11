#ifndef HIGHLIGHTTABLEWIDGET_H
#define HIGHLIGHTTABLEWIDGET_H

#include <QTableWidget>
#include <QTableWidgetItem>

class HighlightTableWidget : public QTableWidget {
    Q_OBJECT

public:
    explicit HighlightTableWidget(QWidget* parent = nullptr);
    QString getLastRowString() const;
    QString getLastRowString();

signals:
    void textHighlighted(const QString& text);

public slots:
    void highlightText(const QString& text);
    void addText(const QString& text);
    void clearRow();

private:
    QString getLastRowStringImpl() const {
        if (rowCount() > 0) {
            QTableWidgetItem *obj = item(rowCount() - 1, 0);
            if (obj) {
                return obj->text();
            }
        }
        return QString();
    }
};

#endif // HIGHLIGHTTABLEWIDGET_H
