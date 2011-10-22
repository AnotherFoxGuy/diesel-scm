#include "Utils.h"
#include <QMessageBox>
#include <QDialogButtonBox>

///////////////////////////////////////////////////////////////////////////////
QMessageBox::StandardButton DialogQuery(QWidget *parent, const QString &title, const QString &query, QMessageBox::StandardButtons buttons)
{
	QMessageBox mb(QMessageBox::Question, title, query, buttons, parent, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::Sheet );
	mb.setDefaultButton(QMessageBox::No);
	mb.setWindowModality(Qt::WindowModal);
	mb.setModal(true);
	mb.exec();
	QMessageBox::StandardButton res = mb.standardButton(mb.clickedButton());
	return res;
}

//-----------------------------------------------------------------------------
#if 0 // Unused
#include <QInputDialog>

static bool DialogQueryText(QWidget *parent, const QString &title, const QString &query, QString &text, bool isPassword=false)
{
	QInputDialog dlg(parent, Qt::Sheet);
	dlg.setWindowTitle(title);
	dlg.setInputMode(QInputDialog::TextInput);
	dlg.setWindowModality(Qt::WindowModal);
	dlg.setModal(true);
	dlg.setLabelText(query);
	dlg.setTextValue(text);
	if(isPassword)
		dlg.setTextEchoMode(QLineEdit::Password);

	if(dlg.exec() == QDialog::Rejected)
		return false;

	text = dlg.textValue();
	return true;
}
#endif
