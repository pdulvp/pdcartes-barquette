#pragma once

#include <QDialog>

class QTextBrowser;

// Le mode d'emploi du programme, affiché dans une boîte de dialogue.
class HelpDialog : public QDialog {
    Q_OBJECT

public:
    explicit HelpDialog(QWidget *parent = nullptr);

private:
    QTextBrowser *m_pages = nullptr;
};
