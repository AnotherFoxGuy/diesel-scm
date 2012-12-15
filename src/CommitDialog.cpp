#include "CommitDialog.h"
#include <QPushButton>
#include <QShortcut>
#include "ui_CommitDialog.h"
#include "MainWindow.h" // Ugly. I know.

CommitDialog::CommitDialog(QWidget *parent, QString title, QStringList &files, const QStringList *history, bool singleLineEntry, const QString *checkBoxText, bool *checkBoxValue) :
	QDialog(parent, Qt::Sheet),
	ui(new Ui::CommitDialog)
{
	ui->setupUi(this);
	ui->plainTextEdit->clear();
	ui->listView->setModel(&itemModel);

	setWindowTitle(title);

	// Activate the appropriate control based on mode
	ui->plainTextEdit->setVisible(!singleLineEntry);
	ui->lineEdit->setVisible(singleLineEntry);

	// Activate the checkbox if we have some text
	ui->checkBox->setVisible(checkBoxText!=0);
	if(checkBoxText && checkBoxValue)
	{
		ui->checkBox->setText(*checkBoxText);
		ui->checkBox->setCheckState(*checkBoxValue ? Qt::Checked : Qt::Unchecked);
	}

	// Activate the combo if we have history
	ui->comboBox->setVisible(history!=0);
	if(history)
	{
		// Generate the history combo
		foreach(const QString msg, *history)
		{
			QString trimmed = msg.trimmed();
			if(trimmed.isEmpty())
				continue;

			commitMessages.append(trimmed);
			QStringList lines = trimmed.split('\n');
			QString first_line;
			if(!lines.empty())
				first_line = lines[0] + "...";

			ui->comboBox->addItem(first_line);
		}
	}

	// Populate file list
	for(QStringList::const_iterator it=files.begin(); it!=files.end(); ++it)
	{
		QStandardItem *si = new QStandardItem(*it);
		si->setCheckable(true);
		si->setCheckState(Qt::Checked);
		itemModel.appendRow(si);
	}


	// Trigger commit with a Ctrl-Return from the comment box
	QAction* action = new QAction(ui->plainTextEdit);
	QShortcut* shortcut = new QShortcut(QKeySequence("Ctrl+Return"), ui->plainTextEdit);
	action->setAutoRepeat(false);
	connect(shortcut, SIGNAL(activated()), ui->buttonBox->button(QDialogButtonBox::Ok), SLOT(click()));

	// Abort commit with an Escape key from the comment box
	action = new QAction(ui->plainTextEdit);
	shortcut = new QShortcut(QKeySequence("Escape"), ui->plainTextEdit);
	action->setAutoRepeat(false);
	connect(shortcut, SIGNAL(activated()), ui->buttonBox->button(QDialogButtonBox::Cancel), SLOT(click()));
}

//------------------------------------------------------------------------------
CommitDialog::~CommitDialog()
{
	delete ui;
}

//------------------------------------------------------------------------------
bool CommitDialog::run(QWidget *parent, QString title, QStringList &files, QString &commitMsg, const QStringList *history, bool singleLineEntry, const QString *checkBoxText, bool *checkBoxValue)
{
	CommitDialog dlg(parent, title, files, history, singleLineEntry, checkBoxText, checkBoxValue);
	int res = dlg.exec();

	if(singleLineEntry)
		commitMsg = dlg.ui->lineEdit->text();
	else
		commitMsg = dlg.ui->plainTextEdit->toPlainText();


	if(res!=QDialog::Accepted)
		return false;

	files.clear();
	for(int i=0; i<dlg.itemModel.rowCount(); ++i)
	{
		QStandardItem *si = dlg.itemModel.item(i);
		if(si->checkState()!=Qt::Checked)
			continue;
		files.append(si->text());
	}

	if(checkBoxText)
	{
		Q_ASSERT(checkBoxValue);
		*checkBoxValue = dlg.ui->checkBox->checkState() == Qt::Checked;
	}

	return true;
}

//------------------------------------------------------------------------------
void CommitDialog::on_comboBox_activated(int index)
{
	Q_ASSERT(index < commitMessages.length());

	if(ui->plainTextEdit->isVisible())
		ui->plainTextEdit->setPlainText(commitMessages[index]);

	if(ui->lineEdit->isVisible())
		ui->lineEdit->setText(commitMessages[index]);
}

//------------------------------------------------------------------------------
void CommitDialog::on_listView_doubleClicked(const QModelIndex &index)
{
	QVariant data = itemModel.data(index);
	QString filename = data.toString();
	reinterpret_cast<MainWindow*>(parent())->diffFile(filename);
}

//------------------------------------------------------------------------------
void CommitDialog::on_listView_clicked(const QModelIndex &)
{
	int num_selected = 0;
	for(int i=0; i<itemModel.rowCount(); ++i)
	{
		QStandardItem *si = itemModel.item(i);
		if(si->checkState()==Qt::Checked)
			++num_selected;
	}

	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(num_selected>0);
}
