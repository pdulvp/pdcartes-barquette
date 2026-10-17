#pragma once

#include <QDialog>

class QTextBrowser;

// Le mode d'emploi de l'atelier, affiché dans la fenêtre.
class HelpDialog : public QDialog {
    Q_OBJECT

public:
    explicit HelpDialog(QWidget *parent = nullptr);

private:
    QTextBrowser *m_pages = nullptr;
};
