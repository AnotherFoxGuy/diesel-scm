#ifndef CLONEDIALOG_H
#define CLONEDIALOG_H

#include <QDialog>

#define FOSSIL_CHECKOUT1	"_FOSSIL_"
#define FOSSIL_CHECKOUT2	".fslckout"
#define FOSSIL_EXT			"fossil"

namespace Ui {
class CloneDialog;
}

class CloneDialog : public QDialog
{
	Q_OBJECT

public:
	explicit CloneDialog(QWidget *parent = 0);
	~CloneDialog();

	static bool run(QWidget *parent, class QUrl &url, QString &repository);

private slots:
	void on_btnSelectRepository_clicked();

private:
	Ui::CloneDialog *ui;
};

#endif // CLONEDIALOG_H
